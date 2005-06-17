/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-define-views-model.c
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

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-util/e-i18n.h"
#include "e-util/e-util.h"

#include "gal-define-views-model.h"

#define PARENT_TYPE E_TABLE_MODEL_TYPE
static ETableModelClass *parent_class;

/*
 * GalDefineViewsModel callbacks
 * These are the callbacks that define the behavior of our custom model.
 */
static void gal_define_views_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gal_define_views_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

enum {
	PROP_0,
	PROP_EDITABLE,
	PROP_COLLECTION
};

static void
gdvm_dispose(GObject *object)
{
	GalDefineViewsModel *model = GAL_DEFINE_VIEWS_MODEL(object);

	if (model->collection)
		g_object_unref(model->collection);
	model->collection = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

/* This function returns the number of columns in our ETableModel. */
static int
gdvm_col_count (ETableModel *etc)
{
	return 1;
}

/* This function returns the number of rows in our ETableModel. */
static int
gdvm_row_count (ETableModel *etc)
{
	GalDefineViewsModel *views = GAL_DEFINE_VIEWS_MODEL(etc);
	if (views->collection)
		return gal_view_collection_get_count(views->collection);
	else
		return 0;
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
gdvm_value_at (ETableModel *etc, int col, int row)
{
	GalDefineViewsModel *views = GAL_DEFINE_VIEWS_MODEL(etc);
	const char *value;

	value = gal_view_get_title (gal_view_collection_get_view(views->collection, row));

	return (void *)(value ? value : "");
}

/* This function sets the value at a particular point in our ETableModel. */
static void
gdvm_set_value_at (ETableModel *etc, int col, int row, const void *val)
{
	GalDefineViewsModel *views = GAL_DEFINE_VIEWS_MODEL(etc);
	if (views->editable) {
		e_table_model_pre_change(etc);
		gal_view_set_title(gal_view_collection_get_view(views->collection, row), val);
		e_table_model_cell_changed(etc, col, row);
	}
}

/* This function returns whether a particular cell is editable. */
static gboolean
gdvm_is_cell_editable (ETableModel *etc, int col, int row)
{
	return GAL_DEFINE_VIEWS_MODEL(etc)->editable;
}

static void
gdvm_append_row (ETableModel *etm, ETableModel *source, gint row)
{
}

/* This function duplicates the value passed to it. */
static void *
gdvm_duplicate_value (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

/* This function frees the value passed to it. */
static void
gdvm_free_value (ETableModel *etc, int col, void *value)
{
	g_free(value);
}

static void *
gdvm_initialize_value (ETableModel *etc, int col)
{
	return g_strdup("");
}

static gboolean
gdvm_value_is_empty (ETableModel *etc, int col, const void *value)
{
	return !(value && *(char *)value);
}

static char *
gdvm_value_to_string (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
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
	ETableModel *etm = E_TABLE_MODEL(model);

	e_table_model_pre_change(etm);
	gal_view_collection_append(model->collection, view);
	e_table_model_row_inserted(etm, gal_view_collection_get_count(model->collection) - 1);
}

static void
gal_define_views_model_class_init (GObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose        = gdvm_dispose;
	object_class->set_property   = gal_define_views_model_set_property;
	object_class->get_property   = gal_define_views_model_get_property;

	g_object_class_install_property (object_class, PROP_EDITABLE, 
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_COLLECTION, 
					 g_param_spec_object ("collection",
							      _("Collection"),
							      /*_( */"XXX blurb" /*)*/,
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
gal_define_views_model_init (GObject *object)
{
	GalDefineViewsModel *model = GAL_DEFINE_VIEWS_MODEL(object);

	model->collection = NULL;
}

static void
gal_define_views_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GalDefineViewsModel *model;

	model = GAL_DEFINE_VIEWS_MODEL (object);

	switch (prop_id){
	case PROP_EDITABLE:
		model->editable = g_value_get_boolean (value);
		break;

	case PROP_COLLECTION:
		e_table_model_pre_change(E_TABLE_MODEL(object));
		if (g_value_get_object (value))
			model->collection = GAL_VIEW_COLLECTION(g_value_get_object (value));
		else
			model->collection = NULL;
		e_table_model_changed(E_TABLE_MODEL(object));
		break;
	}
}

static void
gal_define_views_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GalDefineViewsModel *model;

	model = GAL_DEFINE_VIEWS_MODEL (object);

	switch (prop_id) {
	case PROP_EDITABLE:
		g_value_set_boolean (value, model->editable);
		break;

	case PROP_COLLECTION:
		g_value_set_object (value, model->collection);

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

E_MAKE_TYPE(gal_define_views_model, "GalDefineViewsModel", GalDefineViewsModel, gal_define_views_model_class_init, gal_define_views_model_init, PARENT_TYPE)

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

	return E_TABLE_MODEL(et);
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
				 int n)
{
	return gal_view_collection_get_view(model->collection, n);
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
				    int n)
{
	e_table_model_pre_change(E_TABLE_MODEL(model));
	gal_view_collection_delete_view(model->collection, n);
	e_table_model_row_deleted(E_TABLE_MODEL(model), n);
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
				  int n)
{
	ETableModel *etm = E_TABLE_MODEL(model);
	e_table_model_pre_change(etm);
	gal_view_collection_copy_view(model->collection, n);
	e_table_model_row_inserted(etm, gal_view_collection_get_count(model->collection) - 1);
}
