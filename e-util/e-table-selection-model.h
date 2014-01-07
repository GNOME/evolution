/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_SELECTION_MODEL_H_
#define _E_TABLE_SELECTION_MODEL_H_

#include <e-util/e-selection-model-array.h>
#include <e-util/e-table-header.h>
#include <e-util/e-table-model.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_SELECTION_MODEL \
	(e_table_selection_model_get_type ())
#define E_TABLE_SELECTION_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_SELECTION_MODEL, ETableSelectionModel))
#define E_TABLE_SELECTION_MODEL_CLASS(cls) \
	(G_TYPE - CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_SELECTION_MODEL, ETableSelectionModelClass))
#define E_IS_TABLE_SELECTION_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_SELECTION_MODEL))
#define E_IS_TABLE_SELECTION_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_SELECTION_MODEL))
#define E_TABLE_SELECTION_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_SELECTION_MODEL, ETableSelectionModelClass))

G_BEGIN_DECLS

typedef struct _ETableSelectionModel ETableSelectionModel;
typedef struct _ETableSelectionModelClass ETableSelectionModelClass;

struct _ETableSelectionModel {
	ESelectionModelArray parent;

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
};

struct _ETableSelectionModelClass {
	ESelectionModelArrayClass parent_class;
};

GType		e_table_selection_model_get_type	(void) G_GNUC_CONST;
ETableSelectionModel *
		e_table_selection_model_new		(void);

G_END_DECLS

#endif /* _E_TABLE_SELECTION_MODEL_H_ */
