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
#include <tree.h>
#include <parser.h>
#include <xmlmemory.h>
#include "gal-define-views-model.h"

#define PARENT_TYPE e_table_model_get_type()
static ETableModelClass *parent_class;

/*
 * GalDefineViewsModel callbacks
 * These are the callbacks that define the behavior of our custom model.
 */
static void gal_define_views_model_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void gal_define_views_model_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);


enum {
	ARG_0,
	ARG_EDITABLE,
	ARG_COLLECTION
};

static void
gdvm_destroy(GtkObject *object)
{
	GalDefineViewsModel *model = GAL_DEFINE_VIEWS_MODEL(object);

	gtk_object_unref(GTK_OBJECT(model->collection));

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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
gal_define_views_model_class_init (GtkObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = gdvm_destroy;
	object_class->set_arg   = gal_define_views_model_set_arg;
	object_class->get_arg   = gal_define_views_model_get_arg;

	gtk_object_add_arg_type ("GalDefineViewsModel::editable", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_EDITABLE);
	gtk_object_add_arg_type ("GalDefineViewsModel::collection", GAL_VIEW_COLLECTION_TYPE,
				 GTK_ARG_READWRITE, ARG_COLLECTION);

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
gal_define_views_model_init (GtkObject *object)
{
	GalDefineViewsModel *model = GAL_DEFINE_VIEWS_MODEL(object);

	model->collection = NULL;
}

static void
gal_define_views_model_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GalDefineViewsModel *model;

	model = GAL_DEFINE_VIEWS_MODEL (o);

	switch (arg_id){
	case ARG_EDITABLE:
		model->editable = GTK_VALUE_BOOL (*arg);
		break;

	case ARG_COLLECTION:
		e_table_model_pre_change(E_TABLE_MODEL(o));
		if (GTK_VALUE_OBJECT (*arg))
			model->collection = GAL_VIEW_COLLECTION(GTK_VALUE_OBJECT (*arg));
		else
			model->collection = NULL;
		e_table_model_changed(E_TABLE_MODEL(o));
		break;
	}
}

static void
gal_define_views_model_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GalDefineViewsModel *model;

	model = GAL_DEFINE_VIEWS_MODEL (object);

	switch (arg_id) {
	case ARG_EDITABLE:
		GTK_VALUE_BOOL (*arg) = model->editable;
		break;

	case ARG_COLLECTION:
		if (model->collection)
			GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(model->collection);
		else
			GTK_VALUE_OBJECT (*arg) = NULL;
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

GtkType
gal_define_views_model_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"GalDefineViewsModel",
			sizeof (GalDefineViewsModel),
			sizeof (GalDefineViewsModelClass),
			(GtkClassInitFunc) gal_define_views_model_class_init,
			(GtkObjectInitFunc) gal_define_views_model_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
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

	et = gtk_type_new (gal_define_views_model_get_type ());

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
