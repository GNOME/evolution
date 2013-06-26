/*
 * e-table-specification.c
 *
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
 */

#include "e-table-specification.h"

#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <libedataserver/libedataserver.h>

#include "e-xml-utils.h"

#define E_TABLE_SPECIFICATION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TABLE_SPECIFICATION, ETableSpecificationPrivate))

struct _ETableSpecificationPrivate {
	gint placeholder;
};

G_DEFINE_TYPE (
	ETableSpecification,
	e_table_specification,
	G_TYPE_OBJECT)

static void
table_specification_dispose (GObject *object)
{
	ETableSpecification *specification;
	gint ii;

	specification = E_TABLE_SPECIFICATION (object);

	if (specification->columns != NULL) {
		for (ii = 0; specification->columns[ii] != NULL; ii++)
			g_object_unref (specification->columns[ii]);
		g_free (specification->columns);
		specification->columns = NULL;
	}

	g_clear_object (&specification->state);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_specification_parent_class)->dispose (object);
}

static void
table_specification_finalize (GObject *object)
{
	ETableSpecification *specification;

	specification = E_TABLE_SPECIFICATION (object);

	g_free (specification->click_to_add_message);
	g_free (specification->domain);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_table_specification_parent_class)->finalize (object);
}

static void
e_table_specification_class_init (ETableSpecificationClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ETableSpecificationPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = table_specification_dispose;
	object_class->finalize = table_specification_finalize;
}

static void
e_table_specification_init (ETableSpecification *specification)
{
	specification->priv =
		E_TABLE_SPECIFICATION_GET_PRIVATE (specification);

	specification->alternating_row_colors = TRUE;
	specification->no_headers             = FALSE;
	specification->click_to_add           = FALSE;
	specification->click_to_add_end       = FALSE;
	specification->horizontal_draw_grid   = FALSE;
	specification->vertical_draw_grid     = FALSE;
	specification->draw_focus             = TRUE;
	specification->horizontal_scrolling   = FALSE;
	specification->horizontal_resize      = FALSE;
	specification->allow_grouping         = TRUE;

	specification->cursor_mode            = E_CURSOR_SIMPLE;
	specification->selection_mode         = GTK_SELECTION_MULTIPLE;
}

/**
 * e_table_specification_new:
 *
 * Creates a new #ETableSpecification.  This holds the rendering information
 * for an #ETable.
 *
 * Returns: an #ETableSpecification
 */
ETableSpecification *
e_table_specification_new (void)
{
	return g_object_new (E_TYPE_TABLE_SPECIFICATION, NULL);
}

/**
 * e_table_specification_ref_columns:
 * @specification: an #ETableSpecification
 *
 * Returns a #GPtrArray containing #ETableColumnSpecification instances for
 * all columns defined by @specification.  The array contents are owned by
 * the @specification and should not be modified.  Unreference the array
 * with g_ptr_array_unref() when finished with it.
 *
 * Returns: a #GPtrArray of #ETableColumnSpecification instances
 **/
GPtrArray *
e_table_specification_ref_columns (ETableSpecification *specification)
{
	GPtrArray *array;
	guint ii;

	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);
	g_return_val_if_fail (specification->columns != NULL, NULL);

	array = g_ptr_array_new ();

	for (ii = 0; specification->columns[ii] != NULL; ii++)
		g_ptr_array_add (array, specification->columns[ii]);

	return array;
}

/**
 * e_table_specification_load_from_file:
 * @specification: an #ETableSpecification
 * @filename: the name of a file containing an #ETable specification
 *
 * Parses the contents of @filename and configures @specification.
 *
 * Returns: TRUE on success, FALSE on failure.
 */
gboolean
e_table_specification_load_from_file (ETableSpecification *specification,
                                      const gchar *filename)
{
	xmlDoc *doc;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	doc = e_xml_parse_file (filename);
	if (doc != NULL) {
		xmlNode *node = xmlDocGetRootElement (doc);
		e_table_specification_load_from_node (specification, node);
		xmlFreeDoc (doc);
		success = TRUE;
	}

	return success;
}

/**
 * e_table_specification_load_from_string:
 * @specification: an #ETableSpecification
 * @xml: a string containing an #ETable specification
 *
 * Parses the contents of @xml and configures @specification.
 *
 * @xml is typically returned by e_table_specification_save_to_string()
 * or it can be embedded in your source code.
 *
 * Returns: %TRUE on success, %FALSE on failure
 */
gboolean
e_table_specification_load_from_string (ETableSpecification *specification,
                                        const gchar *xml)
{
	xmlDoc *doc;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), FALSE);
	g_return_val_if_fail (xml != NULL, FALSE);

	doc = xmlParseMemory ((gchar *) xml, strlen (xml));
	if (doc != NULL) {
		xmlNode *node = xmlDocGetRootElement (doc);
		e_table_specification_load_from_node (specification, node);
		xmlFreeDoc (doc);
		success = TRUE;
	}

	return success;
}

/**
 * e_table_specification_load_from_node:
 * @specification: an #ETableSpecification
 * @node: an #xmlNode containing an #ETable specification
 *
 * Parses the contents of @node and configures @specification.
 */
void
e_table_specification_load_from_node (ETableSpecification *specification,
                                      const xmlNode *node)
{
	gchar *temp;
	xmlNode *children;
	GQueue columns = G_QUEUE_INIT;
	guint ii = 0;

	specification->no_headers = e_xml_get_bool_prop_by_name (node, (const guchar *)"no-headers");
	specification->click_to_add = e_xml_get_bool_prop_by_name (node, (const guchar *)"click-to-add");
	specification->click_to_add_end = e_xml_get_bool_prop_by_name (node, (const guchar *)"click-to-add-end") && specification->click_to_add;
	specification->alternating_row_colors = e_xml_get_bool_prop_by_name_with_default (node, (const guchar *)"alternating-row-colors", TRUE);
	specification->horizontal_draw_grid = e_xml_get_bool_prop_by_name (node, (const guchar *)"horizontal-draw-grid");
	specification->vertical_draw_grid = e_xml_get_bool_prop_by_name (node, (const guchar *)"vertical-draw-grid");
	if (e_xml_get_bool_prop_by_name_with_default (node, (const guchar *)"draw-grid", TRUE) ==
	    e_xml_get_bool_prop_by_name_with_default (node, (const guchar *)"draw-grid", FALSE)) {
		specification->horizontal_draw_grid =
			specification->vertical_draw_grid = e_xml_get_bool_prop_by_name (node, (const guchar *)"draw-grid");
	}
	specification->draw_focus = e_xml_get_bool_prop_by_name_with_default (node, (const guchar *)"draw-focus", TRUE);
	specification->horizontal_scrolling = e_xml_get_bool_prop_by_name_with_default (node, (const guchar *)"horizontal-scrolling", FALSE);
	specification->horizontal_resize = e_xml_get_bool_prop_by_name_with_default (node, (const guchar *)"horizontal-resize", FALSE);
	specification->allow_grouping = e_xml_get_bool_prop_by_name_with_default (node, (const guchar *)"allow-grouping", TRUE);

	specification->selection_mode = GTK_SELECTION_MULTIPLE;
	temp = e_xml_get_string_prop_by_name (node, (const guchar *)"selection-mode");
	if (temp && !g_ascii_strcasecmp (temp, "single")) {
		specification->selection_mode = GTK_SELECTION_SINGLE;
	} else if (temp && !g_ascii_strcasecmp (temp, "browse")) {
		specification->selection_mode = GTK_SELECTION_BROWSE;
	} else if (temp && !g_ascii_strcasecmp (temp, "extended")) {
		specification->selection_mode = GTK_SELECTION_MULTIPLE;
	}
	g_free (temp);

	specification->cursor_mode = E_CURSOR_SIMPLE;
	temp = e_xml_get_string_prop_by_name (node, (const guchar *)"cursor-mode");
	if (temp && !g_ascii_strcasecmp (temp, "line")) {
		specification->cursor_mode = E_CURSOR_LINE;
	} else	if (temp && !g_ascii_strcasecmp (temp, "spreadsheet")) {
		specification->cursor_mode = E_CURSOR_SPREADSHEET;
	}
	g_free (temp);

	g_free (specification->click_to_add_message);
	specification->click_to_add_message =
		e_xml_get_string_prop_by_name (
			node, (const guchar *)"_click-to-add-message");

	g_free (specification->domain);
	specification->domain =
		e_xml_get_string_prop_by_name (
			node, (const guchar *)"gettext-domain");
	if (specification->domain && !*specification->domain) {
		g_free (specification->domain);
		specification->domain = NULL;
	}

	if (specification->state)
		g_object_unref (specification->state);
	specification->state = NULL;
	if (specification->columns) {
		for (ii = 0; specification->columns[ii] != NULL; ii++) {
			g_object_unref (specification->columns[ii]);
		}
		g_free (specification->columns);
	}
	specification->columns = NULL;

	for (children = node->xmlChildrenNode; children; children = children->next) {
		if (!strcmp ((gchar *) children->name, "ETableColumn")) {
			ETableColumnSpecification *col_spec = e_table_column_specification_new ();

			e_table_column_specification_load_from_node (col_spec, children);
			g_queue_push_tail (&columns, col_spec);
		} else if (specification->state == NULL && !strcmp ((gchar *) children->name, "ETableState")) {
			specification->state = e_table_state_new (specification);
			e_table_state_load_from_node (specification->state, children);
			e_table_sort_info_set_can_group (specification->state->sort_info, specification->allow_grouping);
		}
	}

	ii = 0;
	specification->columns = g_new0 (
		ETableColumnSpecification *,
		g_queue_get_length (&columns) + 1);
	while (!g_queue_is_empty (&columns))
		specification->columns[ii++] = g_queue_pop_head (&columns);

	/* e_table_state_vanilla() uses the columns array we just created. */
	if (specification->state == NULL)
		specification->state = e_table_state_vanilla (specification);
}

