/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-menus.c: Deploy a GalViewCollection in the menus.
 *
 * Author:
 *   Chris Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gal-view-menus.h"

#include <stdlib.h>
#include <string.h>
#include <gtk/gtksignal.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-ui-util.h>
#include <gal/util/e-util.h>
#include <gal/util/e-xml-utils.h>
#include <gal/menus/gal-define-views-dialog.h>
#include <bonobo/bonobo-ui-util.h>
#include <e-util/e-list.h>

struct _GalViewMenusPrivate {
	GalViewInstance *instance;
	int collection_changed_id;
	int instance_changed_id;
	BonoboUIComponent *component;
	EList *listenerClosures;
	GtkWidget *define_views_dialog;

	guint show_define_views : 1;
};

typedef struct {
	GalViewInstance *instance;
	char *id;
	int ref_count;
} ListenerClosure;

static void collection_changed (GalViewCollection *collection,
				GalViewMenus *gvm);
static void instance_changed (GalViewInstance *instance,
			      GalViewMenus *gvm);

#define d(x)
#define CURRENT_VIEW_PATH "/menu/View/ViewBegin/CurrentView"

G_DEFINE_TYPE(GalViewMenus, gal_view_menus, G_TYPE_OBJECT);

static void
closure_free (void *data, void *user_data)
{
	ListenerClosure *closure = data;
	GalViewMenus *gvm = user_data;

	closure->ref_count --;
	if (closure->ref_count == 0) {
		g_object_unref (closure->instance);

		bonobo_ui_component_remove_listener (gvm->priv->component, closure->id);
		g_free (closure);
	}
}

static void *
closure_copy (const void *data, void *user_data)
{
	ListenerClosure *closure = (void *) data;

	closure->ref_count ++;
	return closure;
}

static void
remove_listeners (GalViewMenus *gvm)
{
	if (gvm->priv->listenerClosures)
		g_object_unref (gvm->priv->listenerClosures);

	gvm->priv->listenerClosures = NULL;
}

static void
remove_xml (GalViewMenus *gvm)
{
}

static void
remove_instance (GalViewMenus *gvm)
{
	if (gvm->priv->instance) {
		if (gvm->priv->instance_changed_id != 0)
			g_signal_handler_disconnect (gvm->priv->instance, gvm->priv->instance_changed_id);
		
		if (gvm->priv->instance->collection && gvm->priv->collection_changed_id != 0)
			g_signal_handler_disconnect (gvm->priv->instance->collection, gvm->priv->collection_changed_id);
	}

	gvm->priv->instance_changed_id = 0;
	gvm->priv->collection_changed_id = 0;

	if (gvm->priv->instance) {
		g_object_unref (gvm->priv->instance);
		gvm->priv->instance = NULL;
	}

        remove_listeners(gvm);
	remove_xml(gvm);
}

static void
add_instance (GalViewMenus *gvm,
	      GalViewInstance *instance)
{
	g_object_ref (instance);

	if (gvm->priv->instance != NULL)
		remove_instance (gvm);

	gvm->priv->instance = instance;

	gal_view_instance_load (gvm->priv->instance);

	gvm->priv->instance_changed_id = g_signal_connect (instance, "changed",
							   G_CALLBACK (instance_changed), gvm);
	gvm->priv->collection_changed_id = g_signal_connect (instance->collection, "changed",
							     G_CALLBACK (collection_changed), gvm);
}

static void
clear_define_views_dialog (gpointer data,
			   GObject *where_the_object_was)
{
	GalViewMenus *gvm = GAL_VIEW_MENUS (data);
	gvm->priv->define_views_dialog = NULL;
}

static void
gal_view_menus_finalize (GObject *object)
{
	GalViewMenus *gvm = GAL_VIEW_MENUS (object);

	remove_instance (gvm);

	gal_view_menus_unmerge (gvm, NULL);

	if (gvm->priv->component)
		bonobo_object_unref (gvm->priv->component);
	
	if (gvm->priv->define_views_dialog)
		g_object_weak_unref (G_OBJECT (gvm->priv->define_views_dialog), clear_define_views_dialog, gvm);

	g_free(gvm->priv);

	(* G_OBJECT_CLASS (gal_view_menus_parent_class)->finalize) (object);
}

static void
gal_view_menus_class_init (GalViewMenusClass *gvm_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (gvm_class);

	object_class->finalize = gal_view_menus_finalize;
}

static void
gal_view_menus_init (GalViewMenus *gvm)
{
	gvm->priv                        = g_new(GalViewMenusPrivate, 1);
	gvm->priv->instance              = NULL;
	gvm->priv->collection_changed_id = 0;
	gvm->priv->instance_changed_id   = 0;
	gvm->priv->component             = NULL;
	gvm->priv->listenerClosures      = NULL;
	gvm->priv->define_views_dialog   = NULL;
	gvm->priv->show_define_views     = TRUE;
}

GalViewMenus *
gal_view_menus_new (GalViewInstance *instance)
{
	GalViewMenus *gvm;

	g_return_val_if_fail (instance != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_INSTANCE (instance), NULL);

	gvm = g_object_new (GAL_VIEW_MENUS_TYPE, NULL);
	gal_view_menus_construct(gvm, instance);

	return gvm;
}

GalViewMenus *
gal_view_menus_construct (GalViewMenus      *gvm,
			  GalViewInstance *instance)
{
	g_return_val_if_fail (gvm != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_MENUS (gvm), NULL);
	g_return_val_if_fail (instance != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_INSTANCE (instance), NULL);

	add_instance (gvm, instance);

	return gvm;
}

static void
dialog_response(GtkWidget *dialog, int id, GalViewMenus *menus)
{
	if (id == GTK_RESPONSE_OK) {
		gal_view_collection_save(menus->priv->instance->collection);
	}
	gtk_widget_destroy(dialog);
}

static void
define_views(BonoboUIComponent *component,
	     GalViewMenus      *menus,
	     char              *cname)
{
	if (menus->priv->define_views_dialog) {
		gdk_window_raise (menus->priv->define_views_dialog->window);
	} else {
		GtkWidget *dialog = gal_define_views_dialog_new(menus->priv->instance->collection);

		g_signal_connect (dialog, "response", G_CALLBACK (dialog_response), menus);
		menus->priv->define_views_dialog = dialog;
		g_object_weak_ref (G_OBJECT (dialog), clear_define_views_dialog, menus);
		gtk_widget_show(dialog);
	}
}

static void
save_current_view(BonoboUIComponent *component,
		  GalViewMenus      *menus,
		  char              *cname)
{
	gal_view_instance_save_as (menus->priv->instance);
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

	g_print ("%s\n", path);

	gal_view_instance_set_current_view_id (closure->instance, closure->id);
}

static char *
build_menus(GalViewMenus *menus)
{
	BonoboUINode *root, *menu, *submenu, *place, *menuitem, *commands, *command;
	char *xml;
	xmlChar *string;
	int length;
	int i;
	GalViewInstance *instance = menus->priv->instance;
	GalViewCollection *collection = instance->collection;
	char *id;
	gboolean found = FALSE;

	root = bonobo_ui_node_new("Root");
	menu = bonobo_ui_node_new_child(root, "menu");
	commands = bonobo_ui_node_new_child (root, "commands");

	submenu = bonobo_ui_node_new_child(menu, "submenu");
	bonobo_ui_node_set_attr(submenu, "name", "View");

	place = bonobo_ui_node_new_child(submenu, "placeholder");
	bonobo_ui_node_set_attr(place, "name", "ViewBegin");
	
	submenu = bonobo_ui_node_new_child(place, "submenu");
	bonobo_ui_node_set_attr(submenu, "name", "CurrentView");
	bonobo_ui_node_set_attr(submenu, "_label", N_("_Current View"));

	id = gal_view_instance_get_current_view_id (instance);


	length = gal_view_collection_get_count(collection);

        menus->priv->listenerClosures = e_list_new (closure_copy, closure_free, menus);

	for (i = 0; i < length; i++) {
		GalViewCollectionItem *item = gal_view_collection_get_view_item(collection, i);
		ListenerClosure *closure;
		char *label;
		
		menuitem = bonobo_ui_node_new_child(submenu, "menuitem");
		bonobo_ui_node_set_attr(menuitem, "name", item->id);
		bonobo_ui_node_set_attr(menuitem, "id", item->id);
                bonobo_ui_node_set_attr(menuitem, "group", "GalViewMenus");
                bonobo_ui_node_set_attr(menuitem, "type", "radio");

		command = bonobo_ui_node_new_child (commands, "cmd");
		bonobo_ui_node_set_attr(command, "name", item->id);
                bonobo_ui_node_set_attr(command, "group", "GalViewMenus");

		label = bonobo_ui_util_encode_str (item->title);
		bonobo_ui_node_set_attr(menuitem, "label", label);
		g_free (label);

		closure            = g_new (ListenerClosure, 1);
		closure->instance  = instance;
		closure->id        = item->id;
		closure->ref_count = 1;

		if (!found && id && !strcmp (item->id, id)) {
			found = TRUE;
		}

		g_object_ref (closure->instance);

                bonobo_ui_component_add_listener (menus->priv->component, item->id, toggled_cb, closure);
                e_list_append (menus->priv->listenerClosures, closure);

		closure_free (closure, menus);
	}

	if (menus->priv->show_define_views) {
		if (!found) {

			menuitem = bonobo_ui_node_new_child(submenu, "separator");
			bonobo_ui_node_set_attr(menuitem, "name", "GalView:first_sep");
			bonobo_ui_node_set_attr(menuitem, "f", "");
		

			menuitem = bonobo_ui_node_new_child(submenu, "menuitem");
			bonobo_ui_node_set_attr(menuitem, "name", "custom_view");
			bonobo_ui_node_set_attr(menuitem, "id", "custom_view");
			bonobo_ui_node_set_attr(menuitem, "group", "GalViewMenus");
			bonobo_ui_node_set_attr(menuitem, "type", "radio");
			/* bonobo displays this string so it must be in locale */
			bonobo_ui_node_set_attr(menuitem, "_label", N_("Custom View"));

			command = bonobo_ui_node_new_child (commands, "cmd");
			bonobo_ui_node_set_attr(command, "name", "custom_view");
			bonobo_ui_node_set_attr(command, "group", "GalViewMenus");


			menuitem = bonobo_ui_node_new_child(submenu, "menuitem");
			bonobo_ui_node_set_attr(menuitem, "name", "SaveCurrentView");
			bonobo_ui_node_set_attr(menuitem, "_label", N_("Save Custom View..."));
			bonobo_ui_node_set_attr(menuitem, "verb", "");

			command = bonobo_ui_node_new_child(commands, "cmd");
			bonobo_ui_node_set_attr(command, "name", "SaveCurrentView");
		}

		menuitem = bonobo_ui_node_new_child(submenu, "separator");
		bonobo_ui_node_set_attr(menuitem, "name", "GalView:second_sep");
		bonobo_ui_node_set_attr(menuitem, "f", "");

		menuitem = bonobo_ui_node_new_child(submenu, "menuitem");
		bonobo_ui_node_set_attr(menuitem, "name", "DefineViews");
		bonobo_ui_node_set_attr(menuitem, "_label", N_("Define Views..."));
		bonobo_ui_node_set_attr(menuitem, "verb", "");

		command = bonobo_ui_node_new_child(commands, "cmd");
		bonobo_ui_node_set_attr(command, "name", "DefineViews");
	}

	string = bonobo_ui_node_to_string(root, TRUE);
	xml = g_strdup(string);
	bonobo_ui_node_free_string(string);

	bonobo_ui_node_free(root);

	g_free (id);

	/*	d(g_print (xml));*/

	return xml;
}

static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("DefineViews", define_views),
	BONOBO_UI_UNSAFE_VERB ("SaveCurrentView", save_current_view),
	BONOBO_UI_VERB_END
};

static void
set_state (GalViewMenus *gvm, char *path, CORBA_Environment *ev)
{
	char *full_path = g_strdup_printf ("/commands/%s", path);

	bonobo_ui_component_set_prop (gvm->priv->component, full_path, "state", "1", ev);
	g_free (full_path);
}

static void
set_radio (GalViewMenus *gvm,
	   CORBA_Environment *ev)
{
	char *id;

	id = gal_view_instance_get_current_view_id (gvm->priv->instance);

	if (id) {
		set_state (gvm, id, ev);
	} else {
		set_state (gvm, "custom_view", ev);
	}
	g_free (id);
}

static void
build_stuff (GalViewMenus      *gvm,
	     CORBA_Environment *ev)
{
	char *xml;

	g_object_ref (gvm);

	gal_view_menus_unmerge (gvm, ev);

	remove_listeners(gvm);
	remove_xml(gvm);
	xml = build_menus(gvm);
	bonobo_ui_component_set_translate(gvm->priv->component, "/", xml, ev);
	g_free(xml);

	bonobo_ui_component_add_verb_list_with_data(gvm->priv->component, verbs, gvm);

	set_radio (gvm, ev);

	g_object_unref (gvm);
}

void
gal_view_menus_set_show_define_views (GalViewMenus *gvm,
				      gboolean show_define_views)
{
	if (gvm->priv->show_define_views == show_define_views)
		return;

	gvm->priv->show_define_views = show_define_views;

	if (gvm->priv->component) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		build_stuff(gvm, &ev);
		CORBA_exception_free (&ev);
	}
}

void
gal_view_menus_apply     (GalViewMenus      *gvm,
			  BonoboUIComponent *component,
			  CORBA_Environment *opt_ev)
{
	if (gvm->priv == NULL)
		return;

	if (component != gvm->priv->component) {
		if (component)
			bonobo_object_ref (BONOBO_OBJECT (component));

		if (gvm->priv->component)
			bonobo_object_unref (BONOBO_OBJECT (gvm->priv->component));
	}

	gvm->priv->component = component;

	build_stuff (gvm, opt_ev);
}

void
gal_view_menus_unmerge   (GalViewMenus      *gvm,
			  CORBA_Environment *opt_ev)
{
	d(g_print ("%s:\n", G_GNUC_FUNCTION));
	if (bonobo_ui_component_get_container (gvm->priv->component) != NULL
	    && bonobo_ui_component_path_exists  (gvm->priv->component, CURRENT_VIEW_PATH, opt_ev)) {
		d(g_print ("%s: Removing path\n", G_GNUC_FUNCTION));
		bonobo_ui_component_rm (gvm->priv->component, CURRENT_VIEW_PATH, opt_ev);
	}
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

static void
instance_changed (GalViewInstance *instance,
		  GalViewMenus *gvm)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	build_stuff(gvm, &ev);
	CORBA_exception_free (&ev);
}

void
gal_view_menus_set_instance (GalViewMenus *gvm,
			     GalViewInstance *instance)
{
	remove_instance (gvm);
	add_instance (gvm, instance);
	instance_changed (instance, gvm);
}
