/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-state.c: Savable state of a table.
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
#include "gal/util/e-util.h"
#include "gal/util/e-xml-utils.h"
#include "e-table-state.h"

#define PARENT_TYPE (gtk_object_get_type())

static GtkObjectClass *etst_parent_class;

static void
etst_destroy (GtkObject *object)
{
	ETableState *etst = E_TABLE_STATE (object);

	if (etst->columns)
		g_free (etst->columns);

	GTK_OBJECT_CLASS (etst_parent_class)->destroy (object);
}

static void
etst_class_init (GtkObjectClass *klass)
{
	etst_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = etst_destroy;
}

E_MAKE_TYPE(e_table_state, "ETableState", ETableState, etst_class_init, NULL, PARENT_TYPE);

ETableState *
e_table_state_new (void)
{
	ETableState *etst = gtk_type_new (E_TABLE_STATE_TYPE);

	return (ETableState *) etst;
}

gboolean
e_table_state_load_from_file    (ETableState *state,
				 const char          *filename)
{
	xmlDoc *doc;
	doc = xmlParseFile (filename);
	if (doc) {
		xmlNode *node = xmlDocGetRootElement(doc);
		e_table_state_load_from_node(state, node);
		xmlFreeDoc(doc);
		return TRUE;
	}
	return FALSE;
}

void 
e_table_state_load_from_string  (ETableState *state,
				 const char          *xml)
{
	xmlDoc *doc;
	doc = xmlParseMemory ((char *) xml, strlen(xml));
	if (doc) {
		xmlNode *node = xmlDocGetRootElement(doc);
		e_table_state_load_from_node(state, node);
		xmlFreeDoc(doc);
	}
}

void
e_table_state_load_from_node    (ETableState *state,
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
e_table_state_save_to_file      (ETableState *state,
				 const char          *filename)
{
	xmlDoc *doc;
	doc = xmlNewDoc("1.0");
	xmlDocSetRootElement(doc, e_table_state_save_to_node(state, NULL));
	xmlSaveFile(filename, doc);
	xmlFreeDoc(doc);
}

char *
e_table_state_save_to_string    (ETableState *state)
{
	char *ret_val;
	xmlChar *string;
	int length;
	xmlDoc *doc;

	doc = xmlNewDoc(NULL);
	xmlDocSetRootElement(doc, e_table_state_save_to_node(state, NULL));
	xmlDocDumpMemory(doc, &string, &length);
	xmlFreeDoc(doc);

	ret_val = g_strdup(string);
	xmlFree(string);
	return ret_val;
}

xmlNode *
e_table_state_save_to_node      (ETableState *state,
				 xmlNode     *parent)
{
	int i;
	xmlNode *node;

	if (parent)
		node = xmlNewChild (parent, NULL, "ETableState", NULL);
	else
		node = xmlNewNode (NULL, "ETableState");

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
