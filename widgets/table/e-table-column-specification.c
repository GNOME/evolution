/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-column-specification.c - Savable specification of a column.
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

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"

#include "e-table-column-specification.h"

static GObjectClass *etcs_parent_class;

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
}

static void
etcs_finalize (GObject *object)
{
	ETableColumnSpecification *etcs = E_TABLE_COLUMN_SPECIFICATION (object);

	free_strings(etcs);

	etcs_parent_class->finalize (object);
}

static void
etcs_class_init (GObjectClass *klass)
{
	etcs_parent_class = g_type_class_peek_parent (klass);
	
	klass->finalize = etcs_finalize;
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

E_MAKE_TYPE(e_table_column_specification, "ETableColumnSpecification", ETableColumnSpecification, etcs_class_init, etcs_init, G_TYPE_OBJECT)

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

	etcs->model_col     = e_xml_get_integer_prop_by_name (node, "model_col");
	etcs->compare_col   = e_xml_get_integer_prop_by_name_with_default (node, "compare_col", etcs->model_col);
	etcs->title         = e_xml_get_string_prop_by_name (node, "_title");
	etcs->pixbuf        = e_xml_get_string_prop_by_name (node, "pixbuf");

	etcs->expansion     = e_xml_get_double_prop_by_name (node, "expansion");
	etcs->minimum_width = e_xml_get_integer_prop_by_name (node, "minimum_width");
	etcs->resizable     = e_xml_get_bool_prop_by_name (node, "resizable");
	etcs->disabled      = e_xml_get_bool_prop_by_name (node, "disabled");

	etcs->cell          = e_xml_get_string_prop_by_name (node, "cell");
	etcs->compare       = e_xml_get_string_prop_by_name (node, "compare");
	etcs->search        = e_xml_get_string_prop_by_name (node, "search");
	etcs->priority      = e_xml_get_integer_prop_by_name_with_default (node, "priority", 0);

	if (etcs->title == NULL)
		etcs->title = g_strdup("");
}

xmlNode *
e_table_column_specification_save_to_node (ETableColumnSpecification *specification,
					   xmlNode                   *parent)
{
	xmlNode *node;
	if (parent)
		node = xmlNewChild(parent, NULL, "ETableColumn", NULL);
	else
		node = xmlNewNode(NULL, "ETableColumn");

	e_xml_set_integer_prop_by_name(node, "model_col", specification->model_col);
	if (specification->compare_col != specification->model_col)
		e_xml_set_integer_prop_by_name(node, "compare_col", specification->compare_col);
	e_xml_set_string_prop_by_name(node, "_title", specification->title);
	e_xml_set_string_prop_by_name(node, "pixbuf", specification->pixbuf);

	e_xml_set_double_prop_by_name(node, "expansion", specification->expansion);
	e_xml_set_integer_prop_by_name(node, "minimum_width", specification->minimum_width);
	e_xml_set_bool_prop_by_name(node, "resizable", specification->resizable);
	e_xml_set_bool_prop_by_name(node, "disabled", specification->disabled);

	e_xml_set_string_prop_by_name(node, "cell", specification->cell);
	e_xml_set_string_prop_by_name(node, "compare", specification->compare);
	e_xml_set_string_prop_by_name(node, "search", specification->search);
	if (specification->priority != 0)
		e_xml_set_integer_prop_by_name (node, "priority", specification->priority);

	return node;
}

