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

struct _GalViewMenusPrivate {
	GalViewCollection *collection;
	int collection_changed_id;
	BonoboUIVerb *verbs;
	BonoboUIComponent *component;
};

#define PARENT_TYPE (gtk_object_get_type())

static GtkObjectClass *gvm_parent_class;
static void collection_changed (GalViewCollection *collection,
				GalViewMenus *gvm);

#define d(x)

typedef struct {
	GalViewCollection *collection;
	GalView *view;
} CollectionAndView;

static void
free_verbs (GalViewMenus *gvm)
{
	BonoboUIVerb *verbs;
	if (gvm->priv->verbs) {
		for (verbs = gvm->priv->verbs + 1; verbs->cname; verbs++) {
			CollectionAndView *cnv;

			if (gvm->priv->component)
				bonobo_ui_component_remove_verb(gvm->priv->component, verbs->cname);

			cnv = verbs->user_data;
			g_free(verbs->cname);

			gtk_object_unref(GTK_OBJECT(cnv->collection));
			gtk_object_unref(GTK_OBJECT(cnv->view));
			g_free(cnv);
		}
		g_free(gvm->priv->verbs);
	}
	gvm->priv->verbs = NULL;
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
	}

	if (gvm->priv->collection)
		gtk_object_unref(GTK_OBJECT(gvm->priv->collection));
	free_verbs(gvm);
	remove_xml(gvm);
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
}

E_MAKE_TYPE(gal_view_menus, "GalViewMenus", GalViewMenus, gvm_class_init, gvm_init, PARENT_TYPE);

GalViewMenus *
gal_view_menus_new (GalViewCollection *collection)
{
	GalViewMenus *gvm = gtk_type_new (GAL_VIEW_MENUS_TYPE);

	gal_view_menus_construct(gvm, collection);

	return gvm;
}

GalViewMenus *
gal_view_menus_construct (GalViewMenus      *gvm,
			  GalViewCollection *collection)
{
	if (collection)
		gtk_object_ref(GTK_OBJECT(collection));
	gvm->priv->collection = collection;

	gtk_signal_connect(GTK_OBJECT(collection), "changed",
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
	bonobo_ui_node_set_attr(submenu, "_label", _("_Current View"));

	length = gal_view_collection_get_count(collection);
	for (i = 0; i < length; i++) {
		char *verb;
		GalViewCollectionItem *item = gal_view_collection_get_view_item(collection, i);
		menuitem = bonobo_ui_node_new_child(submenu, "menuitem");
		bonobo_ui_node_set_attr(menuitem, "name", item->id);
		bonobo_ui_node_set_attr(menuitem, "_label", item->title);

		verb = g_strdup_printf("DefineViews:%s", item->id);
		bonobo_ui_node_set_attr(menuitem, "verb", verb);
		g_free(verb);
	}

	menuitem = bonobo_ui_node_new_child(submenu, "separator");

	menuitem = bonobo_ui_node_new_child(submenu, "menuitem");
	bonobo_ui_node_set_attr(menuitem, "name", "DefineViews");
	bonobo_ui_node_set_attr(menuitem, "_label",
				e_utf8_from_locale_string(_("Define Views")));
	bonobo_ui_node_set_attr(menuitem, "verb", "DefineViews");

	string = bonobo_ui_node_to_string(root, TRUE);
	xml = g_strdup(string);
	bonobo_ui_node_free_string(string);

	bonobo_ui_node_free(root);

	d(g_print (xml));

	return xml;
}

static void
show_view(BonoboUIComponent *component,
	  gpointer           user_data,
	  const char        *cname)
{
	CollectionAndView *cnv = user_data;
	gal_view_collection_display_view(cnv->collection, cnv->view);
}

static BonoboUIVerb *
build_verbs (GalViewMenus *menus)
{
	GalViewCollection *collection = menus->priv->collection;
	int count = gal_view_collection_get_count(collection);
	BonoboUIVerb *verbs = g_new(BonoboUIVerb, count + 2);
	BonoboUIVerb *verb;
	int i;
	
	verb            = verbs;
	verb->cname     = g_strdup("DefineViews");
	verb->cb        = (BonoboUIVerbFn) define_views;
	verb->user_data = menus;
	verb->dummy     = NULL;
	verb ++;
	for (i = 0; i < count; i++) {
		CollectionAndView *cnv;
		GalViewCollectionItem *item = gal_view_collection_get_view_item(collection, i);

		cnv             = g_new(CollectionAndView, 1);
		cnv->view       = item->view;
		cnv->collection = collection;

		gtk_object_ref(GTK_OBJECT(cnv->view));
		gtk_object_ref(GTK_OBJECT(cnv->collection));

		verb->cname     = g_strdup_printf("DefineViews:%s", item->id);
		verb->cb        = show_view;
		verb->user_data = cnv;
		verb->dummy     = NULL;
		verb++;
	}

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
	xml = build_menus(gvm);
	bonobo_ui_component_set_translate(gvm->priv->component, "/", xml, ev);
	g_free(xml);

	free_verbs(gvm);
	gvm->priv->verbs = build_verbs(gvm);
	bonobo_ui_component_add_verb_list(gvm->priv->component, gvm->priv->verbs);
}

void          gal_view_menus_apply     (GalViewMenus      *gvm,
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
