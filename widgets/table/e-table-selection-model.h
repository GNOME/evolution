/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-selection-model.h
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

#ifndef _E_TABLE_SELECTION_MODEL_H_
#define _E_TABLE_SELECTION_MODEL_H_

#include <gtk/gtkobject.h>
#include <misc/e-selection-model-array.h>
#include <table/e-table-model.h>
#include <table/e-table-header.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TABLE_SELECTION_MODEL_TYPE        (e_table_selection_model_get_type ())
#define E_TABLE_SELECTION_MODEL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_SELECTION_MODEL_TYPE, ETableSelectionModel))
#define E_TABLE_SELECTION_MODEL_CLASS(k)    (G_TYPE-CHECK_CLASS_CAST((k), E_TABLE_SELECTION_MODEL_TYPE, ETableSelectionModelClass))
#define E_IS_TABLE_SELECTION_MODEL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_SELECTION_MODEL_TYPE))
#define E_IS_TABLE_SELECTION_MODEL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_SELECTION_MODEL_TYPE))

typedef struct {
	ESelectionModelArray base;

	ETableModel  *model;
	ETableHeader *eth;

	guint model_pre_change_id;
	guint model_changed_id;
	guint model_row_changed_id;
	guint model_cell_changed_id;
	guint model_rows_inserted_id;
	guint model_rows_deleted_id;

	guint model_changed_idle_id;

	guint selection_model_changed : 1;
	guint group_info_changed : 1;

	GHashTable *hash;
	char      *cursor_id;
} ETableSelectionModel;

typedef struct {
	ESelectionModelArrayClass parent_class;
} ETableSelectionModelClass;

GType                 e_table_selection_model_get_type            (void);
ETableSelectionModel *e_table_selection_model_new                 (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _E_TABLE_SELECTION_MODEL_H_ */
