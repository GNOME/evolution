/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Chris Lahey     <clahey@ximian.com>
 *
 * Copyright (C) 2000 Ximian, Inc.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtksignal.h>
#include <gal/util/e-util.h>
#include <libgnome/gnome-i18n.h>
#include <libebook/e-contact.h>
#include "e-select-names-table-model.h"

/* Object argument IDs */
enum {
	PROP_0,
	PROP_SOURCE,
};

static void e_select_names_table_model_init       (ESelectNamesTableModel *model);
static void e_select_names_table_model_class_init (ESelectNamesTableModelClass *klass);

static void e_select_names_table_model_dispose      (GObject *object);
static void e_select_names_table_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_select_names_table_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_select_names_table_model_model_changed (ESelectNamesModel *source,
						      ESelectNamesTableModel *model);

static ETableModelClass *parent_class = NULL;

static void
e_select_names_table_model_add_source (ESelectNamesTableModel *model,
				       ESelectNamesModel     *source)
{
	model->source = source;
	if (model->source)
		g_object_ref(model->source);
	model->source_changed_id = g_signal_connect(model->source, "changed",
						    G_CALLBACK(e_select_names_table_model_model_changed),
						    model);
}

static void
e_select_names_table_model_drop_source (ESelectNamesTableModel *model)
{
	if (model->source_changed_id)
		g_signal_handler_disconnect(model->source, model->source_changed_id);
	if (model->source)
		g_object_unref(model->source);
	model->source = NULL;
	model->source_changed_id = 0;
}

/**
 * e_select_names_table_model_get_type:
 * @void: 
 * 
 * Registers the &ESelectNamesTableModel class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ESelectNamesTableModel class.
 **/
GType
e_select_names_table_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (ESelectNamesTableModelClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_select_names_table_model_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ESelectNamesTableModel),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_select_names_table_model_init,
		};

		type = g_type_register_static (e_table_model_get_type (), "ESelectNamesTableModel", &info, 0);
	}

	return type;
}

/**
 * e_select_names_table_model_new:
 * @VCard: a string in vCard format
 *
 * Returns: a new #ESelectNamesTableModel that wraps the @VCard.
 */
ETableModel *
e_select_names_table_model_new (ESelectNamesModel *source)
{
	ETableModel *model = g_object_new (E_TYPE_SELECT_NAMES_TABLE_MODEL, NULL);
	g_object_set(model,
		     "source", source,
		     NULL);
	return model;
}

static void
fill_in_info (ESelectNamesTableModel *model)
{
	if (model->source) {
		int count = e_select_names_model_count (model->source);
		gint i;

		model->count = count;
		model->data = g_new(ESelectNamesTableModelData, count);

		for (i = 0; i < count; ++i) {
			const EDestination *dest = e_select_names_model_get_destination (model->source, i);
			EContact *contact = dest ? e_destination_get_contact (dest) : NULL;

			if (contact) {
				model->data[i].name =  e_contact_get(contact, E_CONTACT_NAME_OR_ORG);
				if (model->data[i].name == 0)
					model->data[i].name = g_strdup("");
				model->data[i].email = e_contact_get(contact, E_CONTACT_EMAIL_1);
				if (model->data[i].email == 0)
					model->data[i].email = g_strdup("");
			} else {
				const gchar *name = e_destination_get_name (dest);
				const gchar *email = e_destination_get_email (dest);

				model->data[i].name =  g_strdup (name && *name ? name : email);
				model->data[i].email = g_strdup (email);
			}
		}
	} else {
		model->count = 0;
	}
}

static void
clear_info (ESelectNamesTableModel *model)
{
	if (model->data) {
		int i;
		for (i = 0; i < model->count; i++) {
			g_free(model->data[i].name);
			g_free(model->data[i].email);
		}
		g_free(model->data);
		model->data = NULL;
	}

	model->count = -1;
}

/*
 * ESelectNamesTableModel lifecycle management and vcard loading/saving.
 */

static void
e_select_names_table_model_dispose (GObject *object)
{
	ESelectNamesTableModel *model;
	
	model = E_SELECT_NAMES_TABLE_MODEL (object);

	e_select_names_table_model_drop_source (model);
	clear_info(model);

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}

/* This function returns the number of columns in our ETableModel. */
static int
e_select_names_table_model_col_count (ETableModel *etc)
{
	return 3;
}

/* This function returns the number of rows in our ETableModel. */
static int
e_select_names_table_model_row_count (ETableModel *etc)
{
	ESelectNamesTableModel *e_select_names_table_model = E_SELECT_NAMES_TABLE_MODEL(etc);
	if (e_select_names_table_model->count == -1) {
		if (e_select_names_table_model->source) {
			fill_in_info(e_select_names_table_model);
		} else {
			return 0;
		}
	}
	return e_select_names_table_model->count;
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
e_select_names_table_model_value_at (ETableModel *etc, int col, int row)
{
	ESelectNamesTableModel *e_select_names_table_model = E_SELECT_NAMES_TABLE_MODEL(etc);
	if (e_select_names_table_model->data == NULL) {
		fill_in_info(e_select_names_table_model);
	}
	switch (col) {
	case 0:
		if (e_select_names_table_model->data[row].name == NULL) {
			fill_in_info(e_select_names_table_model);
		}
		return e_select_names_table_model->data[row].name;
		break;
	case 1:
		if (e_select_names_table_model->data[row].email == NULL) {
			fill_in_info(e_select_names_table_model);
		}
		return e_select_names_table_model->data[row].email;
		break;
	case 2:
		/* underline column*/
		return (void*)TRUE;
		break;
	}
	return "";
}

/* This function sets the value at a particular point in our ETableModel. */
static void
e_select_names_table_model_set_value_at (ETableModel *etc, int col, int row, const void *val)
{
}

/* This function returns whether a particular cell is editable. */
static gboolean
e_select_names_table_model_is_cell_editable (ETableModel *etc, int col, int row)
{
	return FALSE;
}

/* This function duplicates the value passed to it. */
static void *
e_select_names_table_model_duplicate_value (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

/* This function frees the value passed to it. */
static void
e_select_names_table_model_free_value (ETableModel *etc, int col, void *value)
{
	g_free(value);
}

static void *
e_select_names_table_model_initialize_value (ETableModel *etc, int col)
{
	return g_strdup("");
}

static gboolean
e_select_names_table_model_value_is_empty (ETableModel *etc, int col, const void *value)
{
	return !(value && *(char *)value);
}

static char *
e_select_names_table_model_value_to_string (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

static void
e_select_names_table_model_model_changed (ESelectNamesModel     *source,
					  ESelectNamesTableModel *model)
{
	e_table_model_pre_change(E_TABLE_MODEL(model));
	clear_info(model);
	e_table_model_changed(E_TABLE_MODEL(model));
}

/* Set_arg handler for the model */
static void
e_select_names_table_model_set_property (GObject *object, guint prop_id,
					 const GValue *value, GParamSpec *pspec)
{
	ESelectNamesTableModel *model;
	
	model = E_SELECT_NAMES_TABLE_MODEL (object);

	switch (prop_id) {
	case PROP_SOURCE:
		e_select_names_table_model_drop_source (model);
		e_select_names_table_model_add_source (model, E_SELECT_NAMES_MODEL(g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* Get_arg handler for the model */
static void
e_select_names_table_model_get_property (GObject *object, guint prop_id,
					 GValue *value, GParamSpec *pspec)
{
	ESelectNamesTableModel *model;

	model = E_SELECT_NAMES_TABLE_MODEL (object);

	switch (prop_id) {
	case PROP_SOURCE:
		g_value_set_object (value, model->source);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * e_select_names_table_model_init:
 */
static void
e_select_names_table_model_init (ESelectNamesTableModel *model)
{
	model->source = NULL;
	model->source_changed_id = 0;

	model->count = -1;
	model->data = NULL;
}

static void
e_select_names_table_model_class_init (ESelectNamesTableModelClass *klass)
{
	GObjectClass *object_class;
	ETableModelClass *table_model_class;

	object_class = G_OBJECT_CLASS(klass);
	table_model_class = E_TABLE_MODEL_CLASS(klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = e_select_names_table_model_dispose;
	object_class->get_property = e_select_names_table_model_get_property;
	object_class->set_property = e_select_names_table_model_set_property;

	g_object_class_install_property (object_class, PROP_SOURCE, 
					 g_param_spec_object ("source",
							      _("Source"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_SELECT_NAMES_MODEL,
							      G_PARAM_READWRITE));

	table_model_class->column_count = e_select_names_table_model_col_count;
	table_model_class->row_count = e_select_names_table_model_row_count;
	table_model_class->value_at = e_select_names_table_model_value_at;
	table_model_class->set_value_at = e_select_names_table_model_set_value_at;
	table_model_class->is_cell_editable = e_select_names_table_model_is_cell_editable;
	table_model_class->duplicate_value = e_select_names_table_model_duplicate_value;
	table_model_class->free_value = e_select_names_table_model_free_value;
	table_model_class->initialize_value = e_select_names_table_model_initialize_value;
	table_model_class->value_is_empty = e_select_names_table_model_value_is_empty;
	table_model_class->value_to_string = e_select_names_table_model_value_to_string;
}
