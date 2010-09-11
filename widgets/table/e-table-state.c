/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <libedataserver/e-xml-utils.h>

#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"

#include "e-table-state.h"

#define STATE_VERSION 0.1

G_DEFINE_TYPE (ETableState, e_table_state, G_TYPE_OBJECT)

static void
etst_dispose (GObject *object)
{
	ETableState *etst = E_TABLE_STATE (object);

	if (etst->sort_info) {
		g_object_unref (etst->sort_info);
		etst->sort_info = NULL;
	}

	G_OBJECT_CLASS (e_table_state_parent_class)->dispose (object);
}

static void
etst_finalize (GObject *object)
{
	ETableState *etst = E_TABLE_STATE (object);

	if (etst->columns) {
		g_free (etst->columns);
		etst->columns = NULL;
	}

	if (etst->expansions) {
		g_free (etst->expansions);
		etst->expansions = NULL;
	}

	G_OBJECT_CLASS (e_table_state_parent_class)->finalize (object);
}

static void
e_table_state_class_init (ETableStateClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = etst_dispose;
	object_class->finalize = etst_finalize;
}

static void
e_table_state_init (ETableState *state)
{
	state->columns = NULL;
	state->expansions = NULL;
	state->sort_info = e_table_sort_info_new ();
}

ETableState *
e_table_state_new (void)
{
	return (ETableState *) g_object_new (E_TABLE_STATE_TYPE, NULL);
}

ETableState *
e_table_state_vanilla (gint col_count)
{
	GString *str;
	gint i;
	ETableState *res;

	str = g_string_new ("<ETableState>\n");
	for (i = 0; i < col_count; i++)
		g_string_append_printf (str, "  <column source=\"%d\"/>\n", i);
	g_string_append (str, "  <grouping></grouping>\n");
	g_string_append (str, "</ETableState>\n");

	res = e_table_state_new ();
	e_table_state_load_from_string (res, str->str);

	g_string_free (str, TRUE);
	return res;
}

gboolean
e_table_state_load_from_file    (ETableState *state,
				 const gchar          *filename)
{
	xmlDoc *doc;

	g_return_val_if_fail (E_IS_TABLE_STATE (state), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	doc = e_xml_parse_file (filename);
	if (doc) {
		xmlNode *node = xmlDocGetRootElement (doc);
		e_table_state_load_from_node (state, node);
		xmlFreeDoc (doc);
		return TRUE;
	}
	return FALSE;
}

void
e_table_state_load_from_string  (ETableState *state,
				 const gchar          *xml)
{
	xmlDoc *doc;

	g_return_if_fail (E_IS_TABLE_STATE (state));
	g_return_if_fail (xml != NULL);

	doc = xmlParseMemory ((gchar *) xml, strlen (xml));
	if (doc) {
		xmlNode *node = xmlDocGetRootElement (doc);
		e_table_state_load_from_node (state, node);
		xmlFreeDoc (doc);
	}
}

typedef struct {
	gint column;
	gdouble expansion;
} int_and_double;

void
e_table_state_load_from_node (ETableState *state,
			      const xmlNode *node)
{
	xmlNode *children;
	GList *list = NULL, *iterator;
	gdouble state_version;
	gint i;

	g_return_if_fail (E_IS_TABLE_STATE (state));
	g_return_if_fail (node != NULL);

	state_version = e_xml_get_double_prop_by_name_with_default (
		node, (const guchar *)"state-version", STATE_VERSION);

	if (state->sort_info)
		g_object_unref (state->sort_info);

	state->sort_info = NULL;
	children = node->xmlChildrenNode;
	for (; children; children = children->next) {
		if (!strcmp ((gchar *)children->name, "column")) {
			int_and_double *column_info = g_new (int_and_double, 1);

			column_info->column = e_xml_get_integer_prop_by_name (
				children, (const guchar *)"source");
			column_info->expansion =
				e_xml_get_double_prop_by_name_with_default (
					children, (const guchar *)"expansion", 1);

			list = g_list_append (list, column_info);
		} else if (state->sort_info == NULL &&
			   !strcmp ((gchar *)children->name, "grouping")) {
			state->sort_info = e_table_sort_info_new ();
			e_table_sort_info_load_from_node (
				state->sort_info, children, state_version);
		}
	}
	g_free (state->columns);
	g_free (state->expansions);
	state->col_count = g_list_length (list);
	state->columns = g_new (int, state->col_count);
	state->expansions = g_new (double, state->col_count);

	if (!state->sort_info)
		state->sort_info = e_table_sort_info_new ();

	for (iterator = list, i = 0; iterator; i++) {
		int_and_double *column_info = iterator->data;

		state->columns[i] = column_info->column;
		state->expansions[i] = column_info->expansion;
		g_free (column_info);
		iterator = g_list_next (iterator);
	}
	g_list_free (list);
}

void
e_table_state_save_to_file      (ETableState *state,
				 const gchar          *filename)
{
	xmlDoc *doc;

	if ((doc = xmlNewDoc ((const guchar *)"1.0")) == NULL)
		return;

	xmlDocSetRootElement (doc, e_table_state_save_to_node (state, NULL));

	e_xml_save_file (filename, doc);

	xmlFreeDoc (doc);
}

gchar *
e_table_state_save_to_string    (ETableState *state)
{
	gchar *ret_val;
	xmlChar *string;
	gint length;
	xmlDoc *doc;

	g_return_val_if_fail (E_IS_TABLE_STATE (state), NULL);

	doc = xmlNewDoc((const guchar *)"1.0");
	xmlDocSetRootElement (doc, e_table_state_save_to_node (state, NULL));
	xmlDocDumpMemory (doc, &string, &length);
	xmlFreeDoc (doc);

	ret_val = g_strdup ((gchar *)string);
	xmlFree (string);
	return ret_val;
}

xmlNode *
e_table_state_save_to_node      (ETableState *state,
				 xmlNode     *parent)
{
	gint i;
	xmlNode *node;

	g_return_val_if_fail (E_IS_TABLE_STATE (state), NULL);

	if (parent)
		node = xmlNewChild (
			parent, NULL, (const guchar *) "ETableState", NULL);
	else
		node = xmlNewNode (NULL, (const guchar *) "ETableState");

	e_xml_set_double_prop_by_name (
		node, (const guchar *)"state-version", STATE_VERSION);

	for (i = 0; i < state->col_count; i++) {
		gint column = state->columns[i];
		gdouble expansion = state->expansions[i];
		xmlNode *new_node;

		new_node = xmlNewChild (
			node, NULL, (const guchar *) "column", NULL);
		e_xml_set_integer_prop_by_name (
			new_node, (const guchar *) "source", column);
		if (expansion >= -1)
			e_xml_set_double_prop_by_name (
				new_node, (const guchar *)
				"expansion", expansion);
	}

	e_table_sort_info_save_to_node (state->sort_info, node);

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
	gchar *copy;

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
