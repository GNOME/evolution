/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-specification.c: Implements a savable description of the inital state of a table.
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
#include "e-table-specification.h"

#define PARENT_TYPE (gtk_object_get_type())

static GtkObjectClass *etsp_parent_class;

static void
etsp_destroy (GtkObject *object)
{
	ETableSpecification *etsp = E_TABLE_SPECIFICATION (object);
	int i;

	if (etsp->columns) {
		for (i = 0; etsp->columns[i]; i++) {
			gtk_object_unref (GTK_OBJECT (etsp->columns[i]));
		}
		g_free(etsp->columns);
	}

	if (etsp->state)
		gtk_object_unref(GTK_OBJECT(etsp->state));
	g_free(etsp->click_to_add_message_);

	etsp->columns               = NULL;
	etsp->state                 = NULL;
	etsp->click_to_add_message_ = NULL;

	GTK_OBJECT_CLASS (etsp_parent_class)->destroy (object);
}

static void
etsp_class_init (GtkObjectClass *klass)
{
	etsp_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = etsp_destroy;
}

static void
etsp_init (ETableSpecification *etsp)
{
	etsp->columns               = NULL;
	etsp->state                 = NULL;

	etsp->no_headers            = FALSE;
	etsp->click_to_add          = FALSE;
	etsp->draw_grid             = FALSE;
	etsp->cursor_mode           = E_TABLE_CURSOR_SIMPLE;

	etsp->click_to_add_message_ = NULL;
}

E_MAKE_TYPE(e_table_specification, "ETableSpecification", ETableSpecification, etsp_class_init, etsp_init, PARENT_TYPE);

ETableSpecification *
e_table_specification_new (void)
{
	ETableSpecification *etsp = gtk_type_new (E_TABLE_SPECIFICATION_TYPE);

	return (ETableSpecification *) etsp;
}

gboolean
e_table_specification_load_from_file    (ETableSpecification *specification,
					 const char          *filename)
{
	xmlDoc *doc;
	doc = xmlParseFile (filename);
	if (doc) {
		xmlNode *node = xmlDocGetRootElement(doc);
		e_table_specification_load_from_node(specification, node);
		xmlFreeDoc(doc);
		return TRUE;
	}
	return FALSE;
}

void 
e_table_specification_load_from_string  (ETableSpecification *specification,
					 const char          *xml)
{
	xmlDoc *doc;
	doc = xmlParseMemory ((char *) xml, strlen(xml));
	if (doc) {
		xmlNode *node = xmlDocGetRootElement(doc);
		e_table_specification_load_from_node(specification, node);
		xmlFreeDoc(doc);
	}
}

void
e_table_specification_load_from_node    (ETableSpecification *specification,
					 const xmlNode       *node)
{
	char *temp;
	xmlNode *children;
	GList *list = NULL, *list2;
	int i;

	specification->no_headers = e_xml_get_bool_prop_by_name(node, "no-headers");
	specification->click_to_add = e_xml_get_bool_prop_by_name(node, "click-to-add");
	specification->draw_grid = e_xml_get_bool_prop_by_name(node, "draw-grid");

	specification->cursor_mode = E_TABLE_CURSOR_SIMPLE;
	temp = e_xml_get_string_prop_by_name(node, "cursor-mode");
	if (temp && !strcasecmp(temp, "line")) {
		specification->cursor_mode = E_TABLE_CURSOR_LINE;
	}
	g_free(temp);

	g_free(specification->click_to_add_message_);

	specification->click_to_add_message_ = e_xml_get_translated_string_prop_by_name(node, "_click-to-add-message");

	if (specification->state)
		gtk_object_unref(GTK_OBJECT(specification->state));
	specification->state = NULL;
	if (specification->columns) {
		for (i = 0; specification->columns[i]; i++) {
			gtk_object_unref(GTK_OBJECT(specification->columns[i]));
		}
		g_free(specification->columns);
	}
	specification->columns = NULL;

	for (children = node->xmlChildrenNode; children; children = children->next) {
		if (!strcmp(children->name, "ETableColumn")) {
			ETableColumnSpecification *col_spec = e_table_column_specification_new();

			e_table_column_specification_load_from_node(col_spec, children);
			list = g_list_append(list, col_spec);
		} else if (specification->state == NULL && !strcmp(children->name, "ETableState")) {
			specification->state = e_table_state_new();
			e_table_state_load_from_node(specification->state, children);
		}
	}

	specification->columns = g_new(ETableColumnSpecification *, g_list_length(list) + 1);
	for (list2 = list, i = 0; list2; list2 = g_list_next(list2), i++) {
		specification->columns[i] = list2->data;
	}
	specification->columns[i] = NULL;
	g_list_free(list);
}

#if 0
void
e_table_specification_save_to_file      (ETableSpecification *specification,
					 const char          *filename)
{
	xmlDoc *doc;
	doc = xmlNewDoc(NULL);
	xmlDocSetRootElement(doc, e_table_specification_save_to_node(specification, doc));
	xmlSaveFile(filename, doc);
}

char *
e_table_specification_save_to_string    (ETableSpecification *specification)
{
	char *ret_val;
	xmlChar *string;
	int length;
	xmlDoc *doc;

	doc = xmlNewDoc(NULL);
	xmlDocSetRootElement(doc, e_table_specification_save_to_node(specification, doc));
	xmlDocDumpMemory(doc, &string, &length);

	ret_val = g_strdup(string);
	xmlFree(string);
	return ret_val;
}

xmlNode *
e_table_specification_save_to_node      (ETableSpecification *specification,
					 xmlDoc              *doc)
{
	return NULL;
}
#endif
