/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */

#include <config.h>
#include "e-test-model.h"
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <gnome.h>

#define PARENT_TYPE e_table_model_get_type()
/*
 * ETestModel callbacks
 * These are the callbacks that define the behavior of our custom model.
 */

static void
test_destroy(GtkObject *object)
{
	ETestModel *model = E_TEST_MODEL(object);
	int i;
	for ( i = 0; i < model->data_count; i++ ) {
		g_free(model->data[i]->email);
		g_free(model->data[i]->full_name);
		g_free(model->data[i]->street);
		g_free(model->data[i]->phone);
		g_free(model->data[i]);
	}
	g_free(model->data);
	g_free(model->filename);
}

/* This function returns the number of columns in our ETableModel. */
static int
test_col_count (ETableModel *etc)
{
	return LAST_COL;
}

/* This function returns the number of rows in our ETableModel. */
static int
test_row_count (ETableModel *etc)
{
	ETestModel *test = E_TEST_MODEL(etc);
	return test->data_count;
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
test_value_at (ETableModel *etc, int col, int row)
{
	ETestModel *test = E_TEST_MODEL(etc);
	if ( col >= LAST_COL || row >= test->data_count )
		return NULL;
	switch (col) {
	case EMAIL:
		return test->data[row]->email;
	case FULL_NAME:
		return test->data[row]->full_name;
	case STREET:
		return test->data[row]->street;
	case PHONE:
		return test->data[row]->phone;
	default:
		return NULL;
	}
}

/* This function sets the value at a particular point in our ETableModel. */
static void
test_set_value_at (ETableModel *etc, int col, int row, const void *val)
{
	ETestModel *test = E_TEST_MODEL(etc);
	if ( col >= LAST_COL || row >= test->data_count )
		return;
	switch (col) {
	case EMAIL:
		g_free (test->data[row]->email);
		test->data[row]->email = g_strdup (val);	
		break;
	case FULL_NAME:
		g_free (test->data[row]->full_name);
		test->data[row]->full_name = g_strdup (val);	
		break;
	case STREET:
		g_free (test->data[row]->street);
		test->data[row]->street = g_strdup (val);	
		break;
	case PHONE:
		g_free (test->data[row]->phone);
		test->data[row]->phone = g_strdup (val);	
		break;
	default:
		return;
	}
	if ( !etc->frozen )
		e_table_model_cell_changed(etc, col, row);
}

/* This function returns whether a particular cell is editable. */
static gboolean
test_is_cell_editable (ETableModel *etc, int col, int row)
{
	return TRUE;
}

/* This function duplicates the value passed to it. */
static void *
test_duplicate_value (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

/* This function frees the value passed to it. */
static void
test_free_value (ETableModel *etc, int col, void *value)
{
	g_free(value);
}

/* This function is for when the model is unfrozen.  This can mostly
   be ignored for simple models.  */
static void
test_thaw (ETableModel *etc)
{
	e_table_model_changed(etc);
}

static void
e_test_model_class_init (GtkObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;
	
	object_class->destroy = test_destroy;

	model_class->column_count = test_col_count;
	model_class->row_count = test_row_count;
	model_class->value_at = test_value_at;
	model_class->set_value_at = test_set_value_at;
	model_class->is_cell_editable = test_is_cell_editable;
	model_class->duplicate_value = test_duplicate_value;
	model_class->free_value = test_free_value;
	model_class->thaw = test_thaw;
}

static void
e_test_model_init (GtkObject *object)
{
	ETestModel *model = E_TEST_MODEL(object);
	model->data = NULL;
	model->data_count = 0;
	model->idle = 0;
}

GtkType
e_test_model_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETestModel",
			sizeof (ETestModel),
			sizeof (ETestModelClass),
			(GtkClassInitFunc) e_test_model_class_init,
			(GtkObjectInitFunc) e_test_model_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

static gboolean
save(gpointer data)
{
	int i;
	xmlDoc *document = xmlNewDoc("1.0");
	xmlNode *root;
	ETestModel *model = data;

	root = xmlNewDocNode(document, NULL, "address-book", NULL);
	xmlDocSetRootElement(document, root);
	for ( i = 0; i < model->data_count; i++ ) {
		xmlNode *xml_address = xmlNewChild(root, NULL, "address", NULL);
		if ( model->data[i]->email && *model->data[i]->email )
			xmlSetProp(xml_address, "email", model->data[i]->email);
		if ( model->data[i]->email && *model->data[i]->street )
			xmlSetProp(xml_address, "street", model->data[i]->street);
		if ( model->data[i]->email && *model->data[i]->full_name )
			xmlSetProp(xml_address, "full-name", model->data[i]->full_name);
		if ( model->data[i]->email && *model->data[i]->phone )
			xmlSetProp(xml_address, "phone", model->data[i]->phone);
	}
	xmlSaveFile (model->filename, document);
	model->idle = 0;
	gtk_object_unref(GTK_OBJECT(model));
	/*	e_table_save_specification(E_TABLE(e_table), "spec"); */
	return FALSE;
}

void
e_test_model_queue_save(ETestModel *model)
{
	if ( !model->idle ) {
		gtk_object_ref(GTK_OBJECT(model));
		model->idle = g_idle_add(save, model);
	}
}

void
e_test_model_add_column (ETestModel *model, Address *newadd)
{
	model->data = g_realloc(model->data, (++model->data_count) * sizeof(Address *));
	model->data[model->data_count - 1] = newadd;
	e_test_model_queue_save(model);
	if ( model && !E_TABLE_MODEL(model)->frozen )
		e_table_model_changed(E_TABLE_MODEL(model));
}


ETableModel *
e_test_model_new (gchar *filename)
{
	ETestModel *et;
	xmlDoc *document;
	xmlNode *xml_addressbook;
	xmlNode *xml_address;

	et = gtk_type_new (e_test_model_get_type ());

	/* First we fill in the simple data. */
	if ( g_file_exists(filename) ) {
		e_table_model_freeze(E_TABLE_MODEL(et));
		document = xmlParseFile(filename);
		xml_addressbook = xmlDocGetRootElement(document);
		for (xml_address = xml_addressbook->childs; xml_address; xml_address = xml_address->next) {
			char *datum;
			Address *newadd;

			newadd = g_new(Address, 1);

			datum = xmlGetProp(xml_address, "email");
			if ( datum ) {
				newadd->email = g_strdup(datum);
				xmlFree(datum);
			} else
				newadd->email = g_strdup("");

			datum = xmlGetProp(xml_address, "street");
			if ( datum ) {
				newadd->street = g_strdup(datum);
				xmlFree(datum);
			} else
				newadd->street = g_strdup("");

			datum = xmlGetProp(xml_address, "full-name");
			if ( datum ) {
				newadd->full_name = g_strdup(datum);
				xmlFree(datum);
			} else
				newadd->full_name = g_strdup("");

			datum = xmlGetProp(xml_address, "phone");
			if ( datum ) {
				newadd->phone = g_strdup(datum);
				xmlFree(datum);
			} else
				newadd->phone = g_strdup("");
			e_test_model_add_column (et, newadd);
		}
		xmlFreeDoc(document);
		e_table_model_thaw(E_TABLE_MODEL(et));
	}

	et->filename = g_strdup(filename);
	
	
	gtk_signal_connect(GTK_OBJECT(et), "model_changed",
			   GTK_SIGNAL_FUNC(e_test_model_queue_save), NULL);
	gtk_signal_connect(GTK_OBJECT(et), "model_row_changed",
			   GTK_SIGNAL_FUNC(e_test_model_queue_save), NULL);
	gtk_signal_connect(GTK_OBJECT(et), "model_cell_changed",
			   GTK_SIGNAL_FUNC(e_test_model_queue_save), NULL);

	return E_TABLE_MODEL(et);
}
