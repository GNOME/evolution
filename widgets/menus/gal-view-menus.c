/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-menus.c: Savable state of a table.
 *
 * Author:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <stdlib.h>
#include <gtk/gtksignal.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <gal/util/e-util.h>
#include <gal/util/e-xml-utils.h>
#include "gal-view-menus.h"
#include <gal/menus/gal-define-views-dialog.h>

#define PARENT_TYPE (gtk_object_get_type())

static GtkObjectClass *gvm_parent_class;

static void
gvm_destroy (GtkObject *object)
{
#if 0
	GalViewMenus *gvm = GAL_VIEW_MENUS (object);
#endif

	GTK_OBJECT_CLASS (gvm_parent_class)->destroy (object);
}

static void
gvm_class_init (GtkObjectClass *klass)
{
	gvm_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = gvm_destroy;
}

E_MAKE_TYPE(gal_view_menus, "GalViewMenus", GalViewMenus, gvm_class_init, NULL, PARENT_TYPE);

GalViewMenus *
gal_view_menus_new (void)
{
	GalViewMenus *gvm = gtk_type_new (GAL_VIEW_MENUS_TYPE);

	return (GalViewMenus *) gvm;
}

static void
dialog_clicked(GtkWidget *dialog, int button, GalViewMenus *menus)
{
	gnome_dialog_close(GNOME_DIALOG(dialog));
}

static void
define_views(BonoboUIComponent *component,
	     GalViewMenus      *menus,
	     char              *cname)
{
	GtkWidget *dialog = gal_define_views_dialog_new();
	gtk_signal_connect(GTK_OBJECT(dialog), "clicked",
			   GTK_SIGNAL_FUNC(dialog_clicked), menus);
	gtk_widget_show(dialog);
}

static BonoboUIVerb verbs[] = {
	{"DefineViews", (BonoboUIVerbFn) define_views, NULL, NULL},
};

void          gal_view_menus_apply     (GalViewMenus      *menus,
					BonoboUIComponent *component,
					CORBA_Environment *ev)
{
	bonobo_ui_component_set_translate(component, "/", "<Root> <menu> <submenu name=\"View\"> <menuitem name=\"DefineViews\" _label=\"Define Views\" verb=\"DefineViews\"/> </submenu></menu></Root>", ev);
	bonobo_ui_component_add_verb_list_with_data(component, verbs, menus);
}

#if 0
gboolean
gal_view_menus_load_from_file    (GalViewMenus *state,
				 const char          *filename)
{
	xmlDoc *doc;
	doc = xmlParseFile (filename);
	if (doc) {
		xmlNode *node = xmlDocGetRootElement(doc);
		gal_view_menus_load_from_node(state, node);
		xmlFreeDoc(doc);
		return TRUE;
	}
	return FALSE;
}

void 
gal_view_menus_load_from_string  (GalViewMenus *state,
				 const char          *xml)
{
	xmlDoc *doc;
	doc = xmlParseMemory ((char *) xml, strlen(xml));
	if (doc) {
		xmlNode *node = xmlDocGetRootElement(doc);
		gal_view_menus_load_from_node(state, node);
		xmlFreeDoc(doc);
	}
}

void
gal_view_menus_load_from_node    (GalViewMenus *state,
				 const xmlNode       *node)
{
	xmlNode *children;
	GList *list = NULL, *iterator;
	int i;

	if (state->sort_info)
		gtk_object_unref(GTK_OBJECT(state->sort_info));
	state->sort_info = NULL;
	for (children = node->xmlChildrenNode; children; children = children->next) {
		if (!strcmp(children->name, "column")) {
			int *column = g_new(int, 1);

			*column = e_xml_get_integer_prop_by_name(children, "source");

			list = g_list_append(list, column);
		} else if (state->sort_info == NULL && !strcmp(children->name, "grouping")) {
			state->sort_info = e_table_sort_info_new();
			e_table_sort_info_load_from_node(state->sort_info, children);
		}
	}
	g_free(state->columns);
	state->col_count = g_list_length(list);
	state->columns = g_new(int, state->col_count);
	for (iterator = list, i = 0; iterator; iterator = g_list_next(iterator), i++) {
		state->columns[i] = *(int *)iterator->data;
		g_free(iterator->data);
	}
	g_list_free(list);
}

void
gal_view_menus_save_to_file      (GalViewMenus *state,
				 const char          *filename)
{
	xmlDoc *doc;
	doc = xmlNewDoc("1.0");
	xmlDocSetRootElement(doc, gal_view_menus_save_to_node(state, NULL));
	xmlSaveFile(filename, doc);
}

char *
gal_view_menus_save_to_string    (GalViewMenus *state)
{
	char *ret_val;
	xmlChar *string;
	int length;
	xmlDoc *doc;

	doc = xmlNewDoc(NULL);
	xmlDocSetRootElement(doc, gal_view_menus_save_to_node(state, NULL));
	xmlDocDumpMemory(doc, &string, &length);

	ret_val = g_strdup(string);
	xmlFree(string);
	return ret_val;
}

xmlNode *
gal_view_menus_save_to_node      (GalViewMenus *state,
				 xmlNode     *parent)
{
	int i;
	xmlNode *node;

	if (parent)
		node = xmlNewChild (parent, NULL, "GalViewMenus", NULL);
	else
		node = xmlNewNode (NULL, "GalViewMenus");

	e_xml_set_double_prop_by_name(node, "state-version", 0.0);

	for (i = 0; i < state->col_count; i++) {
		int column = state->columns[i];
		xmlNode *new_node;

		new_node = xmlNewChild(node, NULL, "column", NULL);
		e_xml_set_integer_prop_by_name (new_node, "source", column);
	}


	e_table_sort_info_save_to_node(state->sort_info, node);

	return node;
}
#endif
