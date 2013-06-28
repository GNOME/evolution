/*
 * e-table-column-specification.c - Savable specification of a column.
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
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-table-column-specification.h"

#include <stdlib.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-xml-utils.h"

G_DEFINE_TYPE (
	ETableColumnSpecification,
	e_table_column_specification,
	G_TYPE_OBJECT)

static void
free_strings (ETableColumnSpecification *etcs)
{
	g_free (etcs->title);
	etcs->title = NULL;
	g_free (etcs->pixbuf);
	etcs->pixbuf = NULL;
	g_free (etcs->cell);
	etcs->cell = NULL;
	g_free (etcs->compare);
	etcs->compare = NULL;
	g_free (etcs->search);
	etcs->search = NULL;
}

static void
etcs_finalize (GObject *object)
{
	ETableColumnSpecification *etcs = E_TABLE_COLUMN_SPECIFICATION (object);

	free_strings (etcs);

	G_OBJECT_CLASS (e_table_column_specification_parent_class)->finalize (object);
}

static void
e_table_column_specification_class_init (ETableColumnSpecificationClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = etcs_finalize;
}

static void
e_table_column_specification_init (ETableColumnSpecification *specification)
{
	specification->model_col     = 0;
	specification->compare_col   = 0;
	specification->title         = g_strdup ("");
	specification->pixbuf        = NULL;

	specification->expansion     = 0;
	specification->minimum_width = 0;
	specification->resizable     = FALSE;
	specification->disabled      = FALSE;

	specification->cell          = NULL;
	specification->compare       = NULL;
	specification->search        = NULL;
	specification->priority      = 0;
}

ETableColumnSpecification *
e_table_column_specification_new (void)
{
	return g_object_new (E_TYPE_TABLE_COLUMN_SPECIFICATION, NULL);
}

/**
 * e_table_column_specification_equal:
 * @spec_a: an #ETableColumnSpecification
 * @spec_b: another #ETableColumnSpecification
 *
 * Convenience function compares @spec_a and @spec_b for equality, which
 * simply means they share the same model column number.
 *
 * <note>
 *   <para>
 *     We should strive to get rid of this function by ensuring only one
 *     #ETableSpecification instance exists per table specification file.
 *     Then we could compare for equality by simply comparing pointers.
 *   </para>
 * </note>
 *
 * Returns: %TRUE if @spec_a and @spec_b describe the same column
 **/
gboolean
e_table_column_specification_equal (ETableColumnSpecification *spec_a,
                                    ETableColumnSpecification *spec_b)
{
	g_return_val_if_fail (E_IS_TABLE_COLUMN_SPECIFICATION (spec_a), FALSE);
	g_return_val_if_fail (E_IS_TABLE_COLUMN_SPECIFICATION (spec_b), FALSE);

	return (spec_a->model_col == spec_b->model_col);
}

void
e_table_column_specification_load_from_node (ETableColumnSpecification *etcs,
                                             const xmlNode *node)
{
	free_strings (etcs);

	etcs->model_col     = e_xml_get_integer_prop_by_name (node, (const guchar *)"model_col");
	etcs->compare_col   = e_xml_get_integer_prop_by_name_with_default (node, (const guchar *)"compare_col", etcs->model_col);
	etcs->title         = e_xml_get_string_prop_by_name (node, (const guchar *)"_title");
	etcs->pixbuf        = e_xml_get_string_prop_by_name (node, (const guchar *)"pixbuf");

	etcs->expansion     = e_xml_get_double_prop_by_name (node, (const guchar *)"expansion");
	etcs->minimum_width = e_xml_get_integer_prop_by_name (node, (const guchar *)"minimum_width");
	etcs->resizable     = e_xml_get_bool_prop_by_name (node, (const guchar *)"resizable");
	etcs->disabled      = e_xml_get_bool_prop_by_name (node, (const guchar *)"disabled");

	etcs->cell          = e_xml_get_string_prop_by_name (node, (const guchar *)"cell");
	etcs->compare       = e_xml_get_string_prop_by_name (node, (const guchar *)"compare");
	etcs->search        = e_xml_get_string_prop_by_name (node, (const guchar *)"search");
	etcs->sortable      = e_xml_get_bool_prop_by_name_with_default (node, (const guchar *)"sortable", TRUE);
	etcs->priority      = e_xml_get_integer_prop_by_name_with_default (node, (const guchar *)"priority", 0);

	if (etcs->title == NULL)
		etcs->title = g_strdup ("");
}

