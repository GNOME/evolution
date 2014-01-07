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

#ifndef E_SELECTION_MODEL_H
#define E_SELECTION_MODEL_H

#include <gtk/gtk.h>
#include <e-util/e-misc-utils.h>
#include <e-util/e-sorter.h>

/* Standard GObject macros */
#define E_TYPE_SELECTION_MODEL \
	(e_selection_model_get_type ())
#define E_SELECTION_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SELECTION_MODEL, ESelectionModel))
#define E_SELECTION_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SELECTION_MODEL, ESelectionModelClass))
#define E_IS_SELECTION_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SELECTION_MODEL))
#define E_IS_SELECTION_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SELECTION_MODEL))
#define E_SELECTION_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SELECTION_MODEL, ESelectionModelClass))

G_BEGIN_DECLS

typedef struct _ESelectionModel ESelectionModel;
typedef struct _ESelectionModelClass ESelectionModelClass;

/* list selection modes */
typedef enum {
	E_CURSOR_LINE,
	E_CURSOR_SIMPLE,
	E_CURSOR_SPREADSHEET
} ECursorMode;

struct _ESelectionModel {
	GObject parent;

	ESorter *sorter;

	GtkSelectionMode mode;
	ECursorMode cursor_mode;

	gint old_selection;
};

struct _ESelectionModelClass {
	GObjectClass parent_class;

	/* Virtual methods */
	gboolean	(*is_row_selected)	(ESelectionModel *model,
						 gint row);
	void		(*foreach)		(ESelectionModel *model,
						 EForeachFunc callback,
						 gpointer closure);
	void		(*clear)		(ESelectionModel *model);
	gint		(*selected_count)	(ESelectionModel *model);
	void		(*select_all)		(ESelectionModel *model);
	gint		(*row_count)		(ESelectionModel *model);

	/* Protected virtual methods. */
	void		(*change_one_row)	(ESelectionModel *model,
						 gint row,
						 gboolean on);
	void		(*change_cursor)	(ESelectionModel *model,
						 gint row,
						 gint col);
	gint		(*cursor_row)		(ESelectionModel *model);
	gint		(*cursor_col)		(ESelectionModel *model);

	void		(*select_single_row)	(ESelectionModel *model,
						 gint row);
	void		(*toggle_single_row)	(ESelectionModel *model,
						 gint row);
	void		(*move_selection_end)	(ESelectionModel *model,
						 gint row);
	void		(*set_selection_end)	(ESelectionModel *model,
						 gint row);

	/* Signals */
	void		(*cursor_changed)	(ESelectionModel *model,
						 gint row,
						 gint col);
	void		(*cursor_activated)	(ESelectionModel *model,
						 gint row,
						 gint col);
	void		(*selection_row_changed)(ESelectionModel *model,
						 gint row);
	void		(*selection_changed)	(ESelectionModel *model);
};

GType		e_selection_model_get_type	(void) G_GNUC_CONST;
void		e_selection_model_do_something	(ESelectionModel *model,
						 guint row,
						 guint col,
						 GdkModifierType state);
gboolean	e_selection_model_maybe_do_something
						(ESelectionModel *model,
						 guint row,
						 guint col,
						 GdkModifierType state);
void		e_selection_model_right_click_down
						(ESelectionModel *model,
						 guint row,
						 guint col,
						 GdkModifierType state);
void		e_selection_model_right_click_up
						(ESelectionModel *model);
gboolean	e_selection_model_key_press	(ESelectionModel *model,
						 GdkEventKey *key);
void		e_selection_model_select_as_key_press
						(ESelectionModel *model,
						 guint row,
						 guint col,
						 GdkModifierType state);

/* Virtual functions */
gboolean	e_selection_model_is_row_selected
						(ESelectionModel *model,
						 gint n);
void		e_selection_model_foreach	(ESelectionModel *model,
						 EForeachFunc callback,
						 gpointer closure);
void		e_selection_model_clear		(ESelectionModel *model);
gint		e_selection_model_selected_count
						(ESelectionModel *model);
void		e_selection_model_select_all	(ESelectionModel *model);
gint		e_selection_model_row_count	(ESelectionModel *model);

/* Private virtual Functions */
void		e_selection_model_change_one_row
						(ESelectionModel *model,
						 gint row,
						 gboolean on);
void		e_selection_model_change_cursor	(ESelectionModel *model,
						 gint row,
						 gint col);
gint		e_selection_model_cursor_row	(ESelectionModel *model);
gint		e_selection_model_cursor_col	(ESelectionModel *model);
void		e_selection_model_select_single_row
						(ESelectionModel *model,
						 gint row);
void		e_selection_model_toggle_single_row
						(ESelectionModel *model,
						 gint row);
void		e_selection_model_move_selection_end
						(ESelectionModel *model,
						 gint row);
void		e_selection_model_set_selection_end
						(ESelectionModel *model,
						 gint row);

/* Signals */
void		e_selection_model_cursor_changed
						(ESelectionModel *model,
						 gint row,
						 gint col);
void		e_selection_model_cursor_activated
						(ESelectionModel *model,
						 gint row,
						 gint col);
void		e_selection_model_selection_row_changed
						(ESelectionModel *model,
						 gint row);
void		e_selection_model_selection_changed
						(ESelectionModel *model);

G_END_DECLS

#endif /* E_SELECTION_MODEL_H */

