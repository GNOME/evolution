/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-menus.c: Deploy a GalViewCollection in the menus.
 *
 * Author:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#include <config.h>

#include "gal-view-menus.h"

#include <stdlib.h>
#include <gtk/gtksignal.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include <gal/util/e-xml-utils.h>
#include <gal/menus/gal-define-views-dialog.h>
#include <gal/widgets/e-unicode.h>
#include <e-util/e-list.h>

struct _GalViewMenusPrivate {
	GalViewCollection *collection;
	int collection_changed_id;
	BonoboUIVerb *verbs;
	BonoboUIComponent *component;
	EList *listenerClosures;
};

typedef struct {
	GalViewCollection *collection;
	GalView *view;
	const char *id;
} ListenerClosure;

#define PARENT_TYPE (gtk_object_get_type())

static GtkObjectClass *gvm_parent_class;
static void collection_changed (GalViewCollection *collection,
				GalViewMenus *gvm);

#define d(x)

static void
free_verbs (GalViewMenus *gvm)
{
	if (gvm->priv->verbs) {
		g_free(gvm->priv->verbs->cname);
		g_free(gvm->priv->verbs);
	}
	gvm->priv->verbs = NULL;
}

static void
closure_free (void *data, void *user_data)
{
	ListenerClosure *closure = data;
	GalViewMenus *gvm = user_data;

	gtk_object_ref(GTK_OBJECT(closure->view));
	gtk_object_ref(GTK_OBJECT(closure->collection));

	bonobo_ui_component_remove_listener (gvm->priv->component, closure->id);

	g_free (closure);
}

static void
remove_listeners (GalViewMenus *gvm)
{
	if (gvm->priv->listenerClosures) {
		gtk_object_unref (GTK_OBJECT(gvm->priv->listenerClosures));
	}
	gvm->priv->listenerClosures = NULL;
}

static void
remove_xml (GalViewMenus *gvm)
{
}

static void
gvm_destroy (GtkObject *object)
{
	GalViewMenus *gvm = GAL_VIEW_MENUS (object);

	if (gvm->priv->collection && gvm->priv->collection_changed_id != 0) {
		gtk_signal_disconnect(GTK_OBJECT(gvm->priv->collection), gvm->priv->collection_changed_id);
		gvm->priv->collection_changed_id = 0;
	}

	if (gvm->priv->collection)
		gtk_object_unref(GTK_OBJECT(gvm->priv->collection));

	free_verbs(gvm);
	remove_xml(gvm);
	remove_listeners(gvm);
	g_free(gvm->priv);
	gvm->priv = NULL;

	GTK_OBJECT_CLASS (gvm_parent_class)->destroy (object);
}

static void
gvm_class_init (GtkObjectClass *klass)
{
	gvm_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = gvm_destroy;
}

static void
gvm_init (GalViewMenus *gvm)
{
	gvm->priv = g_new(GalViewMenusPrivate, 1);
	gvm->priv->collection = NULL;
	gvm->priv->collection_changed_id = 0;
	gvm->priv->verbs = NULL;
	gvm->priv->component = NULL;
	gvm->priv->listenerClosures = NULL;
}

E_MAKE_TYPE(gal_view_menus, "GalViewMenus", GalViewMenus, gvm_class_init, gvm_init, PARENT_TYPE);

GalViewMenus *
gal_view_menus_new (GalViewCollection *collection)
{
	GalViewMenus *gvm;

	g_return_val_if_fail (collection != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);

	gvm = gtk_type_new (GAL_VIEW_MENUS_TYPE);
	gal_view_menus_construct(gvm, collection);

	return gvm;
}

GalViewMenus *
gal_view_menus_construct (GalViewMenus      *gvm,
			  GalViewCollection *collection)
{
	g_return_val_if_fail (gvm != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_MENUS (gvm), NULL);
	g_return_val_if_fail (collection != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);

	gtk_object_ref(GTK_OBJECT(collection));
	gvm->priv->collection = collection;
	gvm->priv->collection_changed_id = gtk_signal_connect (
		GTK_OBJECT(collection), "changed",
		GTK_SIGNAL_FUNC(collection_changed), gvm);

	return gvm;
}

static void
dialog_clicked(GtkWidget *dialog, int button, GalViewMenus *menus)
{
	if (button == 0) {
		gal_view_collection_save(menus->priv->collection);
	}
	gnome_dialog_close(GNOME_DIALOG(dialog));
}

static void
define_views(BonoboUIComponent *component,
	     GalViewMenus      *menus,
	     char              *cname)
{
	GtkWidget *dialog = gal_define_views_dialog_new(menus->priv->collection);
	gtk_signal_connect(GTK_OBJECT(dialog), "clicked",
			   GTK_SIGNAL_FUNC(dialog_clicked), menus);
	gtk_widget_show(dialog);
}

static void
toggled_cb (BonoboUIComponent *component,
	    const char *path,
	    Bonobo_UIComponent_EventType type,
	    const char *state,
	    gpointer user_data)
{
	ListenerClosure *closure = user_data;

	/* do nothing on state change to untoggled */
	if (!strcmp (state, "0"))
		return;

	gal_view_collection_display_view(closure->collection, closure->view);
}

static char *
build_menus(GalViewMenus *menus)
{
	BonoboUINode *root, *menu, *submenu, *place, *menuitem;
	char *xml;
	xmlChar *string;
	int length;
	int i;
	GalViewCollection *collection = menus->priv->collection;

	root = bonobo_ui_node_new("Root");
	menu = bonobo_ui_node_new_child(root, "menu");

	submenu = bonobo_ui_node_new_child(menu, "submenu");
	bonobo_ui_node_set_attr(submenu, "name", "View");

	place = bonobo_ui_node_new_child(submenu, "placeholder");
	bonobo_ui_node_set_attr(place, "name", "ViewBegin");
	
	submenu = bonobo_ui_node_new_child(place, "submenu");
	bonobo_ui_node_set_attr(submenu, "name", "CurrentView");
	bonobo_ui_node_set_attr(submenu, "_label", N_("_Current View"));

	length = gal_view_collection_get_count(collection);

	menus->priv->listenerClosures = e_list_new (NULL, closure_free, menus);

	for (i = 0; i < length; i++) {
		char *label;
		GalViewCollectionItem *item = gal_view_collection_get_view_item(collection, i);
		ListenerClosure *closure;

		menuitem = bonobo_ui_node_new_child(submenu, "menuitem");
		bonobo_ui_node_set_attr(menuitem, "name", item->id);
		bonobo_ui_node_set_attr(menuitem, "id", item->id);
		bonobo_ui_node_set_attr(menuitem, "group", "GalViewMenus");
		bonobo_ui_node_set_attr(menuitem, "type", "radio");

		/* bonobo displays this string so it must be in locale */
		label = e_utf8_to_locale_string(item->title);
		/* All labels are bonobo_ui_util_decode_str()ed,
		 * so even translated label must be set with _label */
		bonobo_ui_node_set_attr(menuitem, "_label", label);
		g_free(label);

		closure = g_new (ListenerClosure, 1);
		closure->collection   = collection;
		closure->view         = item->view;
		closure->id           = item->id;

		gtk_object_ref(GTK_OBJECT(closure->view));
		gtk_object_ref(GTK_OBJECT(closure->collection));

		bonobo_ui_component_add_listener (menus->priv->component, item->id, toggled_cb, closure);

		e_list_append (menus->priv->listenerClosures, closure);
	}

	menuitem = bonobo_ui_node_new_child(submenu, "separator");

	menuitem = bonobo_ui_node_new_child(submenu, "menuitem");
	bonobo_ui_node_set_attr(menuitem, "name", "DefineViews");
	bonobo_ui_node_set_attr(menuitem, "_label", N_("Define Views"));
	bonobo_ui_node_set_attr(menuitem, "verb", "DefineViews");

	string = bonobo_ui_node_to_string(root, TRUE);
	xml = g_strdup(string);
	bonobo_ui_node_free_string(string);

	bonobo_ui_node_free(root);

	d(g_print (xml));

	return xml;
}

static BonoboUIVerb *
build_verbs (GalViewMenus *menus)
{
	BonoboUIVerb *verbs = g_new(BonoboUIVerb, 2);
	BonoboUIVerb *verb;
	
	verb            = verbs;
	verb->cname     = g_strdup("DefineViews");
	verb->cb        = (BonoboUIVerbFn) define_views;
	verb->user_data = menus;
	verb->dummy     = NULL;
	verb ++;

	verb->cname     = NULL;
	verb->cb        = NULL;
	verb->user_data = NULL;
	verb->dummy     = NULL;
	verb++;

	return verbs;
}

static void
build_stuff (GalViewMenus      *gvm,
	     CORBA_Environment *ev)
{
	char *xml;

	remove_xml(gvm);
	remove_listeners(gvm);
	xml = build_menus(gvm);
	bonobo_ui_component_set_translate(gvm->priv->component, "/", xml, ev);
	g_free(xml);

	free_verbs(gvm);
	gvm->priv->verbs = build_verbs(gvm);
	bonobo_ui_component_add_verb_list(gvm->priv->component, gvm->priv->verbs);
}

void
gal_view_menus_apply     (GalViewMenus      *gvm,
			  BonoboUIComponent *component,
			  CORBA_Environment *ev)
{
	gvm->priv->component = component;

	build_stuff (gvm, ev);
}

static void
collection_changed (GalViewCollection *collection,
		    GalViewMenus *gvm)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	build_stuff(gvm, &ev);
	CORBA_exception_free (&ev);
}
