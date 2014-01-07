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

#ifndef E_SELECTION_MODEL_ARRAY_H
#define E_SELECTION_MODEL_ARRAY_H

#include <e-util/e-bit-array.h>
#include <e-util/e-selection-model.h>

/* Standard GObject macros */
#define E_TYPE_SELECTION_MODEL_ARRAY \
	(e_selection_model_array_get_type ())
#define E_SELECTION_MODEL_ARRAY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SELECTION_MODEL_ARRAY, ESelectionModelArray))
#define E_SELECTION_MODEL_ARRAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SELECTION_MODEL_ARRAY, ESelectionModelArrayClass))
#define E_IS_SELECTION_MODEL_ARRAY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SELECTION_MODEL_ARRAY))
#define E_IS_SELECTION_MODEL_ARRAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SELECTION_MODEL_ARRAY))
#define E_SELECTION_MODEL_ARRAY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SELECTION_MODEL_ARRAY, ESelectionModelArrayClass))

G_BEGIN_DECLS

typedef struct _ESelectionModelArray ESelectionModelArray;
typedef struct _ESelectionModelArrayClass ESelectionModelArrayClass;

struct _ESelectionModelArray {
	ESelectionModel parent;

	EBitArray *eba;

	gint cursor_row;
	gint cursor_col;
	gint selection_start_row;
	gint cursor_row_sorted; /* cursor_row passed through base::sorter if necessary */

	guint model_changed_id;
	guint model_row_inserted_id, model_row_deleted_id;

	/* Anything other than -1 means that the selection is a single
	 * row.  This being -1 does not impart any information. */
	gint selected_row;

	/* Anything other than -1 means that the selection is a all
	 * rows between selection_start_path and cursor_path where
	 * selected_range_end is the rwo number of cursor_path.  This
	 * being -1 does not impart any information. */
	gint selected_range_end;

	guint frozen : 1;
	guint selection_model_changed : 1;
	guint group_info_changed : 1;
};

struct _ESelectionModelArrayClass {
	ESelectionModelClass parent_class;

	gint		(*get_row_count)
					(ESelectionModelArray *selection);
};

GType		e_selection_model_array_get_type
					(void) G_GNUC_CONST;
void		e_selection_model_array_insert_rows
					(ESelectionModelArray *selection,
					 gint row,
					 gint count);
void		e_selection_model_array_delete_rows
					(ESelectionModelArray *selection,
					 gint row,
					 gint count);
void		e_selection_model_array_move_row
					(ESelectionModelArray *selection,
					 gint old_row,
					 gint new_row);
void		e_selection_model_array_confirm_row_count
					(ESelectionModelArray *selection);
gint		e_selection_model_array_get_row_count
					(ESelectionModelArray *selection);

G_END_DECLS

#endif /* E_SELECTION_MODEL_ARRAY_H */
