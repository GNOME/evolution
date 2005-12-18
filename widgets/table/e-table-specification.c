/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-specification.c
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

#include <glib.h>
#include <glib/gstdio.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <libedataserver/e-xml-utils.h>

#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"

#include "e-table-specification.h"

static GObjectClass *etsp_parent_class;

static void
etsp_finalize (GObject *object)
{
	ETableSpecification *etsp = E_TABLE_SPECIFICATION (object);
	int i;

	if (etsp->columns) {
		for (i = 0; etsp->columns[i]; i++) {
			g_object_unref (etsp->columns[i]);
		}
		g_free (etsp->columns);
		etsp->columns = NULL;
	}

	if (etsp->state)
		g_object_unref (etsp->state);
	etsp->state                = NULL;

	g_free (etsp->click_to_add_message);
	etsp->click_to_add_message = NULL;

	g_free (etsp->domain);
	etsp->domain		   = NULL;

	etsp_parent_class->finalize (object);
}

static void
etsp_class_init (GObjectClass *klass)
{
	etsp_parent_class = g_type_class_peek_parent (klass);
	
	klass->finalize = etsp_finalize;
}

static void
etsp_init (ETableSpecification *etsp)
{
	etsp->columns                = NULL;
	etsp->state                  = NULL;

	etsp->alternating_row_colors = TRUE;
	etsp->no_headers             = FALSE;
	etsp->click_to_add           = FALSE;
	etsp->click_to_add_end       = FALSE;
	etsp->horizontal_draw_grid   = FALSE;
	etsp->vertical_draw_grid     = FALSE;
	etsp->draw_focus             = TRUE;
	etsp->horizontal_scrolling   = FALSE;
	etsp->horizontal_resize      = FALSE;
	etsp->allow_grouping         = TRUE;

	etsp->cursor_mode            = E_CURSOR_SIMPLE;
	etsp->selection_mode         = GTK_SELECTION_MULTIPLE;

	etsp->click_to_add_message   = NULL;
	etsp->domain                 = NULL;
}

E_MAKE_TYPE (e_table_specification, "ETableSpecification", ETableSpecification, etsp_class_init, etsp_init, G_TYPE_OBJECT)

/**
 * e_table_specification_new:
 *
 * Creates a new %ETableSpecification object.   This object is used to hold the
 * information about the rendering information for ETable.
 * 
 * Returns: a newly created %ETableSpecification object.
 */
ETableSpecification *
e_table_specification_new (void)
{
	ETableSpecification *etsp = g_object_new (E_TABLE_SPECIFICATION_TYPE, NULL);

	return (ETableSpecification *) etsp;
}

/**
 * e_table_specification_load_from_file:
 * @specification: An ETableSpecification that you want to modify
 * @filename: a filename that contains an ETableSpecification
 *
 * This routine modifies @specification to reflect the state described
 * by the file @filename.  
 *
 * Returns: TRUE on success, FALSE on failure.
 */
gboolean
e_table_specification_load_from_file (ETableSpecification *specification,
				      const char          *filename)
{
	xmlDoc *doc;

	doc = e_xml_parse_file (filename);
	if (doc) {
		xmlNode *node = xmlDocGetRootElement (doc);
		e_table_specification_load_from_node (specification, node);
		xmlFreeDoc (doc);
		return TRUE;
	}
	return FALSE;
}

/**
 * e_table_specification_load_from_string:
 * @specification: An ETableSpecification that you want to modify
 * @xml: a stringified representation of an ETableSpecification description.
 *
 * This routine modifies @specification to reflect the state described
 * by @xml.  @xml is typically returned by e_table_specification_save_to_string
 * or it can be embedded in your source code.
 *
 * Returns: TRUE on success, FALSE on failure.
 */
gboolean
e_table_specification_load_from_string (ETableSpecification *specification,
					const char          *xml)
{
	xmlDoc *doc;
	doc = xmlParseMemory ( (char *) xml, strlen (xml));
	if (doc) {
		xmlNode *node = xmlDocGetRootElement (doc);
		e_table_specification_load_from_node (specification, node);
		xmlFreeDoc (doc);
		return TRUE;
	}

	return FALSE;
}

/**
 * e_table_specification_load_from_node:
 * @specification: An ETableSpecification that you want to modify
 * @node: an xmlNode with an XML ETableSpecification description.
 *
 * This routine modifies @specification to reflect the state described
 * by @node.
 */
void
e_table_specification_load_from_node (ETableSpecification *specification,
				      const xmlNode       *node)
{
	char *temp;
	xmlNode *children;
	GList *list = NULL, *list2;
	int i;

	specification->no_headers = e_xml_get_bool_prop_by_name (node, "no-headers");
	specification->click_to_add = e_xml_get_bool_prop_by_name (node, "click-to-add");
	specification->click_to_add_end = e_xml_get_bool_prop_by_name (node, "click-to-add-end") && specification->click_to_add;
	specification->alternating_row_colors = e_xml_get_bool_prop_by_name_with_default (node, "alternating-row-colors", TRUE);
	specification->horizontal_draw_grid = e_xml_get_bool_prop_by_name (node, "horizontal-draw-grid");
	specification->vertical_draw_grid = e_xml_get_bool_prop_by_name (node, "vertical-draw-grid");
	if (e_xml_get_bool_prop_by_name_with_default(node, "draw-grid", TRUE) ==
	    e_xml_get_bool_prop_by_name_with_default(node, "draw-grid", FALSE)) {
		specification->horizontal_draw_grid =
			specification->vertical_draw_grid = e_xml_get_bool_prop_by_name (node, "draw-grid");
	}
	specification->draw_focus = e_xml_get_bool_prop_by_name_with_default (node, "draw-focus", TRUE);
	specification->horizontal_scrolling = e_xml_get_bool_prop_by_name_with_default (node, "horizontal-scrolling", FALSE);
	specification->horizontal_resize = e_xml_get_bool_prop_by_name_with_default (node, "horizontal-resize", FALSE);
	specification->allow_grouping = e_xml_get_bool_prop_by_name_with_default (node, "allow-grouping", TRUE);

	specification->selection_mode = GTK_SELECTION_MULTIPLE;
	temp = e_xml_get_string_prop_by_name (node, "selection-mode");
	if (temp && !g_ascii_strcasecmp (temp, "single")) {
		specification->selection_mode = GTK_SELECTION_SINGLE;
	} else if (temp && !g_ascii_strcasecmp (temp, "browse")) {
		specification->selection_mode = GTK_SELECTION_BROWSE;
	} else if (temp && !g_ascii_strcasecmp (temp, "extended")) {
		specification->selection_mode = GTK_SELECTION_EXTENDED;
	}
	g_free (temp);

	specification->cursor_mode = E_CURSOR_SIMPLE;
	temp = e_xml_get_string_prop_by_name (node, "cursor-mode");
	if (temp && !g_ascii_strcasecmp (temp, "line")) {
		specification->cursor_mode = E_CURSOR_LINE;
	} else 	if (temp && !g_ascii_strcasecmp (temp, "spreadsheet")) {
		specification->cursor_mode = E_CURSOR_SPREADSHEET;
	}
	g_free (temp);

	g_free (specification->click_to_add_message);
	specification->click_to_add_message =
		e_xml_get_string_prop_by_name (
			node, "_click-to-add-message");

	g_free (specification->domain);
	specification->domain =
		e_xml_get_string_prop_by_name (
			node, "gettext-domain");
	if (specification->domain && !*specification->domain) {
		g_free (specification->domain);
		specification->domain = NULL;
	}

	if (specification->state)
		g_object_unref (specification->state);
	specification->state = NULL;
	if (specification->columns) {
		for (i = 0; specification->columns[i]; i++) {
			g_object_unref (specification->columns[i]);
		}
		g_free (specification->columns);
	}
	specification->columns = NULL;

	for (children = node->xmlChildrenNode; children; children = children->next) {
		if (!strcmp (children->name, "ETableColumn")) {
			ETableColumnSpecification *col_spec = e_table_column_specification_new ();

			e_table_column_specification_load_from_node (col_spec, children);
			list = g_list_append (list, col_spec);
		} else if (specification->state == NULL && !strcmp (children->name, "ETableState")) {
			specification->state = e_table_state_new ();
			e_table_state_load_from_node (specification->state, children);
			e_table_sort_info_set_can_group (specification->state->sort_info, specification->allow_grouping);
		}
	}

	if (specification->state == NULL) {
		/* Make the default state.  */
		specification->state = e_table_state_vanilla (g_list_length (list));
	}

	specification->columns = g_new (ETableColumnSpecification *, g_list_length (list) + 1);
	for (list2 = list, i = 0; list2; list2 = g_list_next (list2), i++) {
		specification->columns[i] = list2->data;
	}
	specification->columns[i] = NULL;
	g_list_free (list);
}

/**
 * e_table_specification_save_to_file:
 * @specification: An %ETableSpecification that you want to save
 * @filename: a file name to store the specification.
 *
 * This routine stores the @specification into @filename.  
 *
 * Returns: 0 on success or -1 on error.
 */
int
e_table_specification_save_to_file (ETableSpecification *specification,
				    const char          *filename)
{
	xmlDoc *doc;
	int ret;
	
	g_return_val_if_fail (specification != NULL, -1);
	g_return_val_if_fail (filename != NULL, -1);
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), -1);
	
	if ((doc = xmlNewDoc ("1.0")) == NULL)
		return -1;
	
	xmlDocSetRootElement (doc, e_table_specification_save_to_node (specification, doc));
	
	ret = e_xml_save_file (filename, doc);
	
	xmlFreeDoc (doc);
	
	return ret;
}

/**
 * e_table_specification_save_to_string:
 * @specification: An %ETableSpecification that you want to stringify
 *
 * Saves the state of @specification to a string.
 *
 * Returns: an g_alloc() allocated string containing the stringified 
 * representation of @specification.  This stringified representation
 * uses XML as a convenience.
 */
char *
e_table_specification_save_to_string (ETableSpecification *specification)
{
	char *ret_val;
	xmlChar *string;
	int length;
	xmlDoc *doc;

	g_return_val_if_fail (specification != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);
	
	doc = xmlNewDoc ("1.0");
	xmlDocSetRootElement (doc, e_table_specification_save_to_node (specification, doc));
	xmlDocDumpMemory (doc, &string, &length);

	ret_val = g_strdup (string);
	xmlFree (string);
	return ret_val;
}

/**
 * e_table_specification_save_to_node:
 * @specification: An ETableSpecification that you want to store.
 * @doc: Node where the specification is saved
 *
 * This routine saves the %ETableSpecification state in the object @specification
 * into the xmlDoc represented by @doc.  
 *
 * Returns: The node that has been attached to @doc with the contents
 * of the ETableSpecification.
 */
xmlNode *
e_table_specification_save_to_node (ETableSpecification *specification,
				    xmlDoc              *doc)
{
	xmlNode *node;
	char *s;

	g_return_val_if_fail (doc != NULL, NULL);
	g_return_val_if_fail (specification != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);

	node = xmlNewNode (NULL, "ETableSpecification");
	e_xml_set_bool_prop_by_name (node, "no-headers", specification->no_headers);
	e_xml_set_bool_prop_by_name (node, "click-to-add", specification->click_to_add);
	e_xml_set_bool_prop_by_name (node, "click-to-add-end", specification->click_to_add_end && specification->click_to_add);
	e_xml_set_bool_prop_by_name (node, "alternating-row-colors", specification->alternating_row_colors);
	e_xml_set_bool_prop_by_name (node, "horizontal-draw-grid", specification->horizontal_draw_grid);
	e_xml_set_bool_prop_by_name (node, "vertical-draw-grid", specification->vertical_draw_grid);
	e_xml_set_bool_prop_by_name (node, "draw-focus", specification->draw_focus);
	e_xml_set_bool_prop_by_name (node, "horizontal-scrolling", specification->horizontal_scrolling);
	e_xml_set_bool_prop_by_name (node, "horizontal-resize", specification->horizontal_resize);
	e_xml_set_bool_prop_by_name (node, "allow-grouping", specification->allow_grouping);

	switch (specification->selection_mode){
	case GTK_SELECTION_SINGLE:
		s = "single";
		break;
	case GTK_SELECTION_BROWSE:
		s = "browse";
		break;
	default:
	case GTK_SELECTION_EXTENDED:
		s = "extended";
	}
	xmlSetProp (node, "selection-mode", s);
	if (specification->cursor_mode == E_CURSOR_LINE)
		s = "line";
	else
		s = "cell";
	xmlSetProp (node, "cursor-mode", s);

	xmlSetProp (node, "_click-to-add-message", specification->click_to_add_message);
	xmlSetProp (node, "gettext-domain", specification->domain);

	if (specification->columns){
		int i;
		
		for (i = 0; specification->columns [i]; i++)
			e_table_column_specification_save_to_node (
				specification->columns [i],
				node);
	}

	if (specification->state)
		e_table_state_save_to_node (specification->state, node);

	return node;
}

/**
 * e_table_specification_duplicate:
 * @spec: specification to duplicate
 *
 * This creates a copy of the %ETableSpecification @spec
 *
 * Returns: The duplicated %ETableSpecification.
 */
ETableSpecification *
e_table_specification_duplicate (ETableSpecification *spec)
{
	ETableSpecification *new_spec;
	char *spec_str;

	g_return_val_if_fail (spec != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (spec), NULL);
	
	new_spec = e_table_specification_new ();
	spec_str = e_table_specification_save_to_string (spec);
	e_table_specification_load_from_string (new_spec, spec_str);
	g_free (spec_str);
	
	return new_spec;
}
