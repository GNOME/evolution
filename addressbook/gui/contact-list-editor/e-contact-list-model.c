/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <string.h>
#include "e-contact-list-model.h"
#include "widgets/misc/e-error.h"

#include <gtk/gtkmessagedialog.h>
#define PARENT_TYPE e_table_model_get_type()
static ETableModelClass *parent_class;

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

	return (void *) e_destination_get_textrep (model->data[row], TRUE);
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
contact_list_model_dispose (GObject *o)
{
	EContactListModel *model = E_CONTACT_LIST_MODEL (o);
	int i;

	if (model->data != NULL) {
		for (i = 0; i < model->data_count; i ++) {
			g_object_unref (model->data[i]);
		}
		g_free (model->data);
		model->data = NULL;
	}

	model->data_count = 0;
	model->data_alloc = 0;

	(* G_OBJECT_CLASS (parent_class)->dispose) (o);
}

static void
e_contact_list_model_class_init (GObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = contact_list_model_dispose;

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
e_contact_list_model_init (GObject *object)
{
	EContactListModel *model = E_CONTACT_LIST_MODEL(object);

	model->data_alloc = 10;
	model->data_count = 0;
	model->data = g_new (EDestination*, model->data_alloc);
}

GType
e_contact_list_model_get_type (void)
{
	static GType cle_type = 0;

	if (!cle_type) {
		static const GTypeInfo cle_info =  {
			sizeof (EContactListModelClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_contact_list_model_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EContactListModel),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_contact_list_model_init,
		};

		cle_type = g_type_register_static (E_TABLE_MODEL_TYPE, "EContactListModel", &cle_info, 0);
	}

	return cle_type;
}

void
e_contact_list_model_construct (EContactListModel *model)
{
}

ETableModel *
e_contact_list_model_new ()
{
	EContactListModel *model;

	model = g_object_new (E_TYPE_CONTACT_LIST_MODEL, NULL);

	e_contact_list_model_construct (model);

	return E_TABLE_MODEL(model);
}

void
e_contact_list_model_add_destination (EContactListModel *model, EDestination *dest)
{
	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));
	g_return_if_fail (E_IS_DESTINATION (dest));

	e_table_model_pre_change (E_TABLE_MODEL (model));

	if (model->data_count + 1 >= model->data_alloc) {
		model->data_alloc *= 2;
		model->data = g_renew (EDestination*, model->data, model->data_alloc);
	}

	model->data[model->data_count ++] = dest;
	g_object_ref (dest);

	e_table_model_row_inserted (E_TABLE_MODEL (model), model->data_count - 1);
}

void
e_contact_list_model_add_email (EContactListModel *model,
				const char *email)
{
	EDestination *new_dest;
	char *list_email;
	int row;
	int row_count;

	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));
	g_return_if_fail (email != NULL);

	row_count = e_table_model_row_count (E_TABLE_MODEL (model));

	for (row = 0; row < row_count; row++) {
		list_email = (char *) e_table_model_value_at (E_TABLE_MODEL (model), 1, row);
		
		if (strcmp (list_email, email) == 0) {
			if (e_error_run (NULL, "addressbook:ask-list-add-exists", 
					 email) != GTK_RESPONSE_YES)
				return;
			break;
		}
	}

	new_dest = e_destination_new ();
	e_destination_set_email (new_dest, email);

	e_contact_list_model_add_destination (model, new_dest);
}

void
e_contact_list_model_add_contact (EContactListModel *model,
				  EContact *contact,
				  int email_num)
{
	EDestination *new_dest;

	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));
	g_return_if_fail (E_IS_CONTACT (contact));

	new_dest = e_destination_new ();
	e_destination_set_contact (new_dest, contact, email_num);

	e_contact_list_model_add_destination (model, new_dest);
}

void
e_contact_list_model_remove_row (EContactListModel *model, int row)
{
	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));
	g_return_if_fail (0 <= row && row < model->data_count);

	e_table_model_pre_change (E_TABLE_MODEL (model));

	g_object_unref (model->data[row]);
	memmove (model->data + row, model->data + row + 1, sizeof (EDestination*) * (model->data_count - row - 1));
	model->data_count --;

	e_table_model_row_deleted (E_TABLE_MODEL (model), row);
}

void
e_contact_list_model_remove_all (EContactListModel *model)
{
	int i;

	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));

	e_table_model_pre_change (E_TABLE_MODEL (model));

	for (i = 0; i < model->data_count; i ++) {
		g_object_unref (model->data[i]);
		model->data[i] = NULL;
	}

	model->data_count = 0;

	e_table_model_changed (E_TABLE_MODEL (model));
}


const EDestination *
e_contact_list_model_get_destination (EContactListModel *model, int row)
{
	g_return_val_if_fail (E_IS_CONTACT_LIST_MODEL (model), NULL);
	g_return_val_if_fail (0 <= row && row < model->data_count, NULL);

	return model->data[row];
}
