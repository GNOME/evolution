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
#include <gtk/gtksignal.h>
#include <parser.h>
#include <xmlmemory.h>
#include "gal/util/e-xml-utils.h"
#include "gal/util/e-util.h"
#include "e-table-column-specification.h"

#define PARENT_TYPE (gtk_object_get_type())

static GtkObjectClass *etcs_parent_class;

static void
free_strings (ETableColumnSpecification *etcs)
{
	g_free(etcs->title);
	g_free(etcs->pixbuf);
	g_free(etcs->cell);
	g_free(etcs->compare);
}

static void
etcs_destroy (GtkObject *object)
{
	ETableColumnSpecification *etcs = E_TABLE_COLUMN_SPECIFICATION (object);

	free_strings(etcs);

	GTK_OBJECT_CLASS (etcs_parent_class)->destroy (object);
}

static void
etcs_class_init (GtkObjectClass *klass)
{
	etcs_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = etcs_destroy;
}

static void
etcs_init (ETableColumnSpecification *specification)
{
	specification->model_col     = 0;
	specification->title         = g_strdup("");
	specification->pixbuf        = NULL;
	
	specification->expansion     = 0;
	specification->minimum_width = 0;
	specification->resizable     = FALSE;
	specification->disabled      = FALSE;
	
	specification->cell          = NULL;
	specification->compare       = NULL;
	specification->priority      = 0;
}

E_MAKE_TYPE(e_table_column_specification, "ETableColumnSpecification", ETableColumnSpecification, etcs_class_init, etcs_init, PARENT_TYPE);

ETableColumnSpecification *
e_table_column_specification_new (void)
{
	ETableColumnSpecification *etcs = gtk_type_new (E_TABLE_COLUMN_SPECIFICATION_TYPE);

	return (ETableColumnSpecification *) etcs;
}

void
e_table_column_specification_load_from_node (ETableColumnSpecification *etcs,
					     const xmlNode       *node)
{
	free_strings(etcs);

	etcs->model_col     = e_xml_get_integer_prop_by_name (node, "model_col");
	etcs->title         = e_xml_get_string_prop_by_name (node, "_title");
	etcs->pixbuf        = e_xml_get_string_prop_by_name (node, "pixbuf");

	etcs->expansion     = e_xml_get_double_prop_by_name (node, "expansion");
	etcs->minimum_width = e_xml_get_integer_prop_by_name (node, "minimum_width");
	etcs->resizable     = e_xml_get_bool_prop_by_name (node, "resizable");
	etcs->disabled      = e_xml_get_bool_prop_by_name (node, "disabled");

	etcs->cell          = e_xml_get_string_prop_by_name (node, "cell");
	etcs->compare       = e_xml_get_string_prop_by_name (node, "compare");
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
	e_xml_set_string_prop_by_name(node, "_title", specification->title);
	e_xml_set_string_prop_by_name(node, "pixbuf", specification->pixbuf);

	e_xml_set_double_prop_by_name(node, "expansion", specification->expansion);
	e_xml_set_integer_prop_by_name(node, "minimum_width", specification->minimum_width);
	e_xml_set_bool_prop_by_name(node, "resizable", specification->resizable);
	e_xml_set_bool_prop_by_name(node, "disabled", specification->disabled);

	e_xml_set_string_prop_by_name(node, "cell", specification->cell);
	e_xml_set_string_prop_by_name(node, "compare", specification->compare);
	if (specification->priority != 0)
		e_xml_set_integer_prop_by_name (node, "priority", specification->priority);

	return node;
}

