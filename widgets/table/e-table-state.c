/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-state.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkobject.h>
#include <parser.h>
#include <xmlmemory.h>
#include "gal/util/e-util.h"
#include "gal/util/e-xml-utils.h"
#include "e-table-state.h"


#define PARENT_TYPE (gtk_object_get_type())

#define STATE_VERSION 0.1

static GtkObjectClass *etst_parent_class;

static void
etst_destroy (GtkObject *object)
{
	ETableState *etst = E_TABLE_STATE (object);

	gtk_object_unref (GTK_OBJECT (etst->sort_info));
	if (etst->columns) {
		g_free (etst->columns);
		etst->columns = NULL;
	}

	if (etst->expansions) {
		g_free (etst->expansions);
		etst->expansions = NULL;
	}
	
	GTK_OBJECT_CLASS (etst_parent_class)->destroy (object);
}

static void
etst_class_init (GtkObjectClass *klass)
{
	etst_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = etst_destroy;
}

static void
etst_init (ETableState *state)
{
	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (state), GTK_FLOATING);

	state->columns = NULL;
	state->expansions = NULL;
	state->sort_info = e_table_sort_info_new();
}

E_MAKE_TYPE(e_table_state, "ETableState", ETableState, etst_class_init, etst_init, PARENT_TYPE);

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

typedef struct {
	int column;
	double expansion;
} int_and_double;

void
e_table_state_load_from_node (ETableState *state,
			      const xmlNode *node)
{
	xmlNode *children;
	GList *list = NULL, *iterator;
	gdouble state_version;
	int i;

	state_version = e_xml_get_double_prop_by_name_with_default (
		node, "state-version", STATE_VERSION);

	if (state->sort_info)
		gtk_object_unref (GTK_OBJECT(state->sort_info));

	state->sort_info = NULL;
	children = node->xmlChildrenNode;
	for (; children; children = children->next) {
		if (!strcmp (children->name, "column")) {
			int_and_double *column_info = g_new(int_and_double, 1);

			column_info->column = e_xml_get_integer_prop_by_name(
				children, "source");
			column_info->expansion =
				e_xml_get_double_prop_by_name_with_default(
					children, "expansion", 1);

			list = g_list_append (list, column_info);
		} else if (state->sort_info == NULL &&
			   !strcmp (children->name, "grouping")) {
			state->sort_info = e_table_sort_info_new();
			e_table_sort_info_load_from_node(
				state->sort_info, children, state_version);
		}
	}
	g_free(state->columns);
	g_free(state->expansions);
	state->col_count = g_list_length(list);
	state->columns = g_new(int, state->col_count);
	state->expansions = g_new(double, state->col_count);

	for (iterator = list, i = 0; iterator; i++) {
		int_and_double *column_info = iterator->data;
		
		state->columns [i] = column_info->column;
		state->expansions [i] = column_info->expansion;
		g_free (column_info);
		iterator = g_list_next (iterator);
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

	doc = xmlNewDoc("1.0");
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

	e_xml_set_double_prop_by_name(node, "state-version", STATE_VERSION);

	for (i = 0; i < state->col_count; i++) {
		int column = state->columns[i];
		double expansion = state->expansions[i];
		xmlNode *new_node;

		new_node = xmlNewChild(node, NULL, "column", NULL);
		e_xml_set_integer_prop_by_name (new_node, "source", column);
		if (expansion >= -1)
			e_xml_set_double_prop_by_name(new_node, "expansion", expansion);
	}


	e_table_sort_info_save_to_node(state->sort_info, node);

	return node;
}

/**
 * e_table_state_duplicate:
 * @state: The ETableState to duplicate
 *
 * This creates a copy of the %ETableState @state
 *
 * Returns: The duplicated %ETableState.
 */
ETableState *
e_table_state_duplicate (ETableState *state)
{
	ETableState *new_state;
	char *copy;
	
	g_return_val_if_fail (state != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_STATE (state), NULL);

	new_state = e_table_state_new ();
	copy = e_table_state_save_to_string (state);
	e_table_state_load_from_string (new_state, copy);
	g_free (copy);

	e_table_sort_info_set_can_group
		(new_state->sort_info,
		 e_table_sort_info_get_can_group (state->sort_info));

	return new_state;
}
