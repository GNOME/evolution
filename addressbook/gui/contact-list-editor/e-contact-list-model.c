/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include "e-contact-list-model.h"

#define PARENT_TYPE e_table_model_get_type()
ETableModelClass *parent_class;

#define COLS 1

/* This function returns the number of columns in our ETableModel. */
static int
contact_list_col_count (ETableModel *etc)
{
	return COLS;
}

/* This function returns the number of rows in our ETableModel. */
static int
contact_list_row_count (ETableModel *etc)
{
	EContactListModel *model = E_CONTACT_LIST_MODEL (etc);
	return model->data_count;
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
contact_list_value_at (ETableModel *etc, int col, int row)
{
	EContactListModel *model = E_CONTACT_LIST_MODEL (etc);

	return model->data[row]->string;
}

/* This function sets the value at a particular point in our ETableModel. */
static void
contact_list_set_value_at (ETableModel *etc, int col, int row, const void *val)
{
	/* nothing */
}

/* This function returns whether a particular cell is editable. */
static gboolean
contact_list_is_cell_editable (ETableModel *etc, int col, int row)
{
	return FALSE;
}

/* This function duplicates the value passed to it. */
static void *
contact_list_duplicate_value (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

/* This function frees the value passed to it. */
static void
contact_list_free_value (ETableModel *etc, int col, void *value)
{
	g_free(value);
}

static void *
contact_list_initialize_value (ETableModel *etc, int col)
{
	return g_strdup("");
}

static gboolean
contact_list_value_is_empty (ETableModel *etc, int col, const void *value)
{
	return !(value && *(char *)value);
}

static char *
contact_list_value_to_string (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

static void
contact_list_model_destroy (GtkObject *o)
{
	EContactListModel *model = E_CONTACT_LIST_MODEL (o);
	int i;

	for (i = 0; i < model->data_count; i ++) {
		g_free (model->data[i]->string);
		if (model->data[i]->simple)
			gtk_object_unref (GTK_OBJECT(model->data[i]->simple));
		g_free (model->data[i]);
	}
	g_free (model->data);

	model->data_count = 0;
	model->data_alloc = 0;
}

static void
e_contact_list_model_class_init (GtkObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = contact_list_model_destroy;

	model_class->column_count = contact_list_col_count;
	model_class->row_count = contact_list_row_count;
	model_class->value_at = contact_list_value_at;
	model_class->set_value_at = contact_list_set_value_at;
	model_class->is_cell_editable = contact_list_is_cell_editable;
	model_class->duplicate_value = contact_list_duplicate_value;
	model_class->free_value = contact_list_free_value;
	model_class->initialize_value = contact_list_initialize_value;
	model_class->value_is_empty = contact_list_value_is_empty;
	model_class->value_to_string = contact_list_value_to_string;
}

static void
e_contact_list_model_init (GtkObject *object)
{
	EContactListModel *model = E_CONTACT_LIST_MODEL(object);

	model->data_alloc = 10;
	model->data_count = 0;
	model->data = g_new (EContactListModelRow*, model->data_alloc);
}

GtkType
e_contact_list_model_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EContactListModel",
			sizeof (EContactListModel),
			sizeof (EContactListModelClass),
			(GtkClassInitFunc) e_contact_list_model_class_init,
			(GtkObjectInitFunc) e_contact_list_model_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

void
e_contact_list_model_construct (EContactListModel *model)
{
}

ETableModel *
e_contact_list_model_new ()
{
	EContactListModel *model;

	model = gtk_type_new (e_contact_list_model_get_type ());

	e_contact_list_model_construct (model);

	return E_TABLE_MODEL(model);
}

void
e_contact_list_model_add_email (EContactListModel *model,
				const char *email)
{
	EContactListModelRow *new_row;

	if (model->data_count + 1 >= model->data_alloc) {
		model->data_alloc *= 2;
		model->data = g_renew (EContactListModelRow*, model->data, model->data_alloc);
	}

	new_row = g_new (EContactListModelRow, 1);
	new_row->type = E_CONTACT_LIST_MODEL_ROW_EMAIL;
	new_row->simple = NULL;
	new_row->string = g_strdup (email);

	model->data[model->data_count ++] = new_row;

	e_table_model_changed (E_TABLE_MODEL (model));
}

void
e_contact_list_model_add_card (EContactListModel *model,
			       ECardSimple *simple)
{
	EContactListModelRow *new_row;
	char *email, *name;

	name = e_card_simple_get (simple, E_CARD_SIMPLE_FIELD_NAME_OR_ORG);
	email = e_card_simple_get (simple, E_CARD_SIMPLE_FIELD_EMAIL);

	if (! name && ! email) {
		/* what to do here? */
		return;
	}
	    

	if (model->data_count + 1 >= model->data_alloc) {
		model->data_alloc *= 2;
		model->data = g_renew (EContactListModelRow*, model->data, model->data_alloc);
	}

	new_row = g_new (EContactListModelRow, 1);

	new_row->type = E_CONTACT_LIST_MODEL_ROW_CARD;
	new_row->simple = simple;
	if (FALSE /* XXX e_card_simple_get (simple, E_CARD_SIMPLE_FIELD_EVOLUTION_LIST)*/)
		new_row->string = g_strconcat ("<", name, ">", NULL);
	else
		new_row->string = g_strconcat (name ? name : "",
					       email ? " <" : "", email ? email : "", email ? ">" : "",
					       NULL);

	model->data[model->data_count++] = new_row;

	gtk_object_ref (GTK_OBJECT (simple));

	e_table_model_changed (E_TABLE_MODEL (model));
}

void
e_contact_list_model_remove_row (EContactListModel *model, int row)
{
	g_free (model->data[row]->string);
	if (model->data[row]->simple)
		gtk_object_unref (GTK_OBJECT(model->data[row]->simple));
	g_free (model->data[row]);
	memmove (model->data + row, model->data + row + 1, sizeof (EContactListModelRow*) * (model->data_count - row - 1));
	model->data_count --;

	e_table_model_changed (E_TABLE_MODEL (model));
}

void
e_contact_list_model_remove_all (EContactListModel *model)
{
	int i;

	for (i = 0; i < model->data_count; i ++) {
		g_free (model->data[i]->string);
		if (model->data[i]->simple)
			gtk_object_unref (GTK_OBJECT(model->data[i]->simple));
		g_free (model->data[i]);
		model->data[i] = NULL;
	}

	model->data_count = 0;

	e_table_model_changed (E_TABLE_MODEL (model));
}


char*
e_contact_list_model_get_email (EContactListModel *model, int row)
{
	EContactListModelRow *data = model->data[row];

	if (data->type == E_CONTACT_LIST_MODEL_ROW_EMAIL)
		return g_strdup (data->string);
	else
		return g_strconcat (ECARD_UID_LINK_PREFIX, e_card_simple_get_id (data->simple), NULL);
}
