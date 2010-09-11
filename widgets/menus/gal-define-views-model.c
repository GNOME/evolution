/*
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

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"

#include "gal-define-views-model.h"

G_DEFINE_TYPE (GalDefineViewsModel, gal_define_views_model, E_TABLE_MODEL_TYPE)

enum {
	PROP_0,
	PROP_EDITABLE,
	PROP_COLLECTION
};

static void
gal_define_views_model_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
	GalDefineViewsModel *model;

	model = GAL_DEFINE_VIEWS_MODEL (object);

	switch (property_id) {
		case PROP_EDITABLE:
			model->editable = g_value_get_boolean (value);
			return;

		case PROP_COLLECTION:
			e_table_model_pre_change (E_TABLE_MODEL (object));
			if (g_value_get_object (value))
				model->collection = GAL_VIEW_COLLECTION (
					g_value_get_object (value));
			else
				model->collection = NULL;
			e_table_model_changed (E_TABLE_MODEL (object));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gal_define_views_model_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
	GalDefineViewsModel *model;

	model = GAL_DEFINE_VIEWS_MODEL (object);

	switch (property_id) {
		case PROP_EDITABLE:
			g_value_set_boolean (value, model->editable);
			return;

		case PROP_COLLECTION:
			g_value_set_object (value, model->collection);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gdvm_dispose (GObject *object)
{
	GalDefineViewsModel *model = GAL_DEFINE_VIEWS_MODEL (object);

	if (model->collection)
		g_object_unref (model->collection);
	model->collection = NULL;

	if (G_OBJECT_CLASS (gal_define_views_model_parent_class)->dispose)
		(* G_OBJECT_CLASS (gal_define_views_model_parent_class)->dispose) (object);
}

/* This function returns the number of columns in our ETableModel. */
static gint
gdvm_col_count (ETableModel *etc)
{
	return 1;
}

/* This function returns the number of rows in our ETableModel. */
static gint
gdvm_row_count (ETableModel *etc)
{
	GalDefineViewsModel *views = GAL_DEFINE_VIEWS_MODEL (etc);
	if (views->collection)
		return gal_view_collection_get_count (views->collection);
	else
		return 0;
}

/* This function returns the value at a particular point in our ETableModel. */
static gpointer
gdvm_value_at (ETableModel *etc, gint col, gint row)
{
	GalDefineViewsModel *views = GAL_DEFINE_VIEWS_MODEL (etc);
	const gchar *value;

	value = gal_view_get_title (gal_view_collection_get_view (views->collection, row));

	return (gpointer)(value ? value : "");
}

/* This function sets the value at a particular point in our ETableModel. */
static void
gdvm_set_value_at (ETableModel *etc, gint col, gint row, gconstpointer val)
{
	GalDefineViewsModel *views = GAL_DEFINE_VIEWS_MODEL (etc);
	if (views->editable) {
		e_table_model_pre_change (etc);
		gal_view_set_title (gal_view_collection_get_view (views->collection, row), val);
		e_table_model_cell_changed (etc, col, row);
	}
}

/* This function returns whether a particular cell is editable. */
static gboolean
gdvm_is_cell_editable (ETableModel *etc, gint col, gint row)
{
	return GAL_DEFINE_VIEWS_MODEL (etc)->editable;
}

static void
gdvm_append_row (ETableModel *etm, ETableModel *source, gint row)
{
}

/* This function duplicates the value passed to it. */
static gpointer
gdvm_duplicate_value (ETableModel *etc, gint col, gconstpointer value)
{
	return g_strdup (value);
}

/* This function frees the value passed to it. */
static void
gdvm_free_value (ETableModel *etc, gint col, gpointer value)
{
	g_free (value);
}

static gpointer
gdvm_initialize_value (ETableModel *etc, gint col)
{
	return g_strdup("");
}

static gboolean
gdvm_value_is_empty (ETableModel *etc, gint col, gconstpointer value)
{
	return !(value && *(gchar *)value);
}

static gchar *
gdvm_value_to_string (ETableModel *etc, gint col, gconstpointer value)
{
	return g_strdup (value);
}

/**
 * gal_define_views_model_append
 * @model: The model to add to.
 * @view: The view to add.
 *
 * Adds the given view to the gal define views model.
 */
void
gal_define_views_model_append (GalDefineViewsModel *model,
			       GalView             *view)
{
	ETableModel *etm = E_TABLE_MODEL (model);

	e_table_model_pre_change (etm);
	gal_view_collection_append (model->collection, view);
	e_table_model_row_inserted (etm, gal_view_collection_get_count (model->collection) - 1);
}

static void
gal_define_views_model_class_init (GalDefineViewsModelClass *klass)
{
	ETableModelClass *model_class = E_TABLE_MODEL_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose        = gdvm_dispose;
	object_class->set_property   = gal_define_views_model_set_property;
	object_class->get_property   = gal_define_views_model_get_property;

	g_object_class_install_property (object_class, PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       "Editable",
							       NULL,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_COLLECTION,
					 g_param_spec_object ("collection",
							      "Collection",
							      NULL,
							      GAL_VIEW_COLLECTION_TYPE,
							      G_PARAM_READWRITE));

	model_class->column_count     = gdvm_col_count;
	model_class->row_count        = gdvm_row_count;
	model_class->value_at         = gdvm_value_at;
	model_class->set_value_at     = gdvm_set_value_at;
	model_class->is_cell_editable = gdvm_is_cell_editable;
	model_class->append_row       = gdvm_append_row;
	model_class->duplicate_value  = gdvm_duplicate_value;
	model_class->free_value       = gdvm_free_value;
	model_class->initialize_value = gdvm_initialize_value;
	model_class->value_is_empty   = gdvm_value_is_empty;
	model_class->value_to_string  = gdvm_value_to_string;
}

static void
gal_define_views_model_init (GalDefineViewsModel *model)
{
	model->collection = NULL;
}

/**
 * gal_define_views_model_new
 *
 * Returns a new define views model.  This is a list of views as an
 * ETable for use in the GalDefineViewsDialog.
 *
 * Returns: The new GalDefineViewsModel.
 */
ETableModel *
gal_define_views_model_new (void)
{
	GalDefineViewsModel *et;

	et = g_object_new (GAL_DEFINE_VIEWS_MODEL_TYPE, NULL);

	return E_TABLE_MODEL (et);
}

/**
 * gal_define_views_model_get_view:
 * @model: The GalDefineViewsModel.
 * @n: Which view to get.
 *
 * Gets the nth view.
 *
 * Returns: The view.
 */
GalView *
gal_define_views_model_get_view (GalDefineViewsModel *model,
				 gint n)
{
	return gal_view_collection_get_view (model->collection, n);
}

/**
 * gal_define_views_model_delete_view:
 * @model: The GalDefineViewsModel.
 * @n: Which view to delete.
 *
 * Deletes the nth view.
 */
void
gal_define_views_model_delete_view (GalDefineViewsModel *model,
				    gint n)
{
	e_table_model_pre_change (E_TABLE_MODEL (model));
	gal_view_collection_delete_view (model->collection, n);
	e_table_model_row_deleted (E_TABLE_MODEL (model), n);
}

/**
 * gal_define_views_model_copy_view:
 * @model: The GalDefineViewsModel.
 * @n: Which view to copy.
 *
 * Copys the nth view.
 */
void
gal_define_views_model_copy_view (GalDefineViewsModel *model,
				  gint n)
{
	ETableModel *etm = E_TABLE_MODEL (model);
	e_table_model_pre_change (etm);
	gal_view_collection_copy_view (model->collection, n);
	e_table_model_row_inserted (etm, gal_view_collection_get_count (model->collection) - 1);
}
