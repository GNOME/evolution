/*
 *
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

#ifndef _E_TABLE_SELECTION_MODEL_H_
#define _E_TABLE_SELECTION_MODEL_H_

#include <glib-object.h>
#include <misc/e-selection-model-array.h>
#include <table/e-table-model.h>
#include <table/e-table-header.h>

G_BEGIN_DECLS

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
	gchar *cursor_id;
} ETableSelectionModel;

typedef struct {
	ESelectionModelArrayClass parent_class;
} ETableSelectionModelClass;

GType                 e_table_selection_model_get_type            (void);
ETableSelectionModel *e_table_selection_model_new                 (void);

G_END_DECLS

#endif /* _E_TABLE_SELECTION_MODEL_H_ */
