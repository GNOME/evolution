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

#ifndef E_SELECTION_MODEL_SIMPLE_H
#define E_SELECTION_MODEL_SIMPLE_H

#include <e-util/e-selection-model-array.h>

/* Standard GObject macros */
#define E_TYPE_SELECTION_MODEL_SIMPLE \
	(e_selection_model_simple_get_type ())
#define E_SELECTION_MODEL_SIMPLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SELECTION_MODEL_SIMPLE, ESelectionModelSimple))
#define E_SELECTION_MODEL_SIMPLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SELECTION_MODEL_SIMPLE, ESelectionModelSimpleClass))
#define E_IS_SELECTION_MODEL_SIMPLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SELECTION_MODEL_SIMPLE))
#define E_IS_SELECTION_MODEL_SIMPLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SELECTION_MODEL_SIMPLE))
#define E_SELECTION_MODEL_SIMPLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SELECTION_MODEL_SIMPLE, ESelectionModelSimpleClass))

G_BEGIN_DECLS

typedef struct _ESelectionModelSimple ESelectionModelSimple;
typedef struct _ESelectionModelSimpleClass ESelectionModelSimpleClass;

struct _ESelectionModelSimple {
	ESelectionModelArray parent;

	gint row_count;
};

struct _ESelectionModelSimpleClass {
	ESelectionModelArrayClass parent_class;
};

GType		e_selection_model_simple_get_type
					(void) G_GNUC_CONST;
ESelectionModelSimple *
		e_selection_model_simple_new
					(void);
void		e_selection_model_simple_insert_rows
					(ESelectionModelSimple *selection,
					 gint row,
					 gint count);
void		e_selection_model_simple_delete_rows
					(ESelectionModelSimple *selection,
					 gint row,
					 gint count);
void		e_selection_model_simple_move_row
					(ESelectionModelSimple *selection,
					 gint old_row,
					 gint new_row);
void		e_selection_model_simple_set_row_count
					(ESelectionModelSimple *selection,
					 gint row_count);

G_END_DECLS

#endif /* E_SELECTION_MODEL_SIMPLE_H */

