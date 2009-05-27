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

#include <config.h>

#include <stdlib.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"

#include "e-table-column-specification.h"

/* workaround for avoiding API breakage */
#define etcs_get_type e_table_column_specification_get_type
G_DEFINE_TYPE (ETableColumnSpecification, etcs, G_TYPE_OBJECT)

static void
free_strings (ETableColumnSpecification *etcs)
{
	g_free(etcs->title);
	etcs->title = NULL;
	g_free(etcs->pixbuf);
	etcs->pixbuf = NULL;
	g_free(etcs->cell);
	etcs->cell = NULL;
	g_free(etcs->compare);
	etcs->compare = NULL;
	g_free(etcs->search);
	etcs->search = NULL;
	g_free (etcs->sortable);
	etcs->sortable = NULL;
}

static void
etcs_finalize (GObject *object)
{
	ETableColumnSpecification *etcs = E_TABLE_COLUMN_SPECIFICATION (object);

	free_strings(etcs);

	G_OBJECT_CLASS (etcs_parent_class)->finalize (object);
}

static void
etcs_class_init (ETableColumnSpecificationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = etcs_finalize;
}

static void
etcs_init (ETableColumnSpecification *specification)
{
	specification->model_col     = 0;
	specification->compare_col   = 0;
	specification->title         = g_strdup("");
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
	ETableColumnSpecification *etcs = g_object_new (E_TABLE_COLUMN_SPECIFICATION_TYPE, NULL);

	return (ETableColumnSpecification *) etcs;
}

void
e_table_column_specification_load_from_node (ETableColumnSpecification *etcs,
					     const xmlNode       *node)
{
	free_strings(etcs);

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
	etcs->sortable	    = e_xml_get_string_prop_by_name (node, (const guchar *)"sortable");
	etcs->priority      = e_xml_get_integer_prop_by_name_with_default (node, (const guchar *)"priority", 0);

	if (etcs->title == NULL)
		etcs->title = g_strdup("");
}

xmlNode *
e_table_column_specification_save_to_node (ETableColumnSpecification *specification,
					   xmlNode                   *parent)
{
	xmlNode *node;
	if (parent)
		node = xmlNewChild(parent, NULL, (const guchar *)"ETableColumn", NULL);
	else
		node = xmlNewNode(NULL, (const guchar *)"ETableColumn");

	e_xml_set_integer_prop_by_name(node, (const guchar *)"model_col", specification->model_col);
	if (specification->compare_col != specification->model_col)
		e_xml_set_integer_prop_by_name(node, (const guchar *)"compare_col", specification->compare_col);
	e_xml_set_string_prop_by_name(node, (const guchar *)"_title", specification->title);
	e_xml_set_string_prop_by_name(node, (const guchar *)"pixbuf", specification->pixbuf);

	e_xml_set_double_prop_by_name(node, (const guchar *)"expansion", specification->expansion);
	e_xml_set_integer_prop_by_name(node, (const guchar *)"minimum_width", specification->minimum_width);
	e_xml_set_bool_prop_by_name(node, (const guchar *)"resizable", specification->resizable);
	e_xml_set_bool_prop_by_name(node, (const guchar *)"disabled", specification->disabled);

	e_xml_set_string_prop_by_name(node, (const guchar *)"cell", specification->cell);
	e_xml_set_string_prop_by_name(node, (const guchar *)"compare", specification->compare);
	e_xml_set_string_prop_by_name(node, (const guchar *)"search", specification->search);
	if (specification->priority != 0)
		e_xml_set_integer_prop_by_name (node, (const guchar *)"priority", specification->priority);

	return node;
}

