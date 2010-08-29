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

#ifndef E_SELECTION_MODEL_H
#define E_SELECTION_MODEL_H

#include <gtk/gtk.h>
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

#ifndef _E_FOREACH_FUNC_H_
#define _E_FOREACH_FUNC_H_
typedef void (*EForeachFunc) (gint model_row,
			      gpointer closure);
#endif

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
	gboolean	(*is_row_selected)	(ESelectionModel *esm,
						 gint row);
	void		(*foreach)		(ESelectionModel *esm,
						 EForeachFunc callback,
						 gpointer closure);
	void		(*clear)		(ESelectionModel *esm);
	gint		(*selected_count)	(ESelectionModel *esm);
	void		(*select_all)		(ESelectionModel *esm);
	void		(*invert_selection)	(ESelectionModel *esm);
	gint		(*row_count)		(ESelectionModel *esm);

	/* Protected virtual methods. */
	void		(*change_one_row)	(ESelectionModel *esm,
						 gint row,
						 gboolean on);
	void		(*change_cursor)	(ESelectionModel *esm,
						 gint row,
						 gint col);
	gint		(*cursor_row)		(ESelectionModel *esm);
	gint		(*cursor_col)		(ESelectionModel *esm);

	void		(*select_single_row)	(ESelectionModel *selection,
						 gint row);
	void		(*toggle_single_row)	(ESelectionModel *selection,
						 gint row);
	void		(*move_selection_end)	(ESelectionModel *selection,
						 gint row);
	void		(*set_selection_end)	(ESelectionModel *selection,
						 gint row);

	/* Signals */
	void		(*cursor_changed)	(ESelectionModel *esm,
						 gint row,
						 gint col);
	void		(*cursor_activated)	(ESelectionModel *esm,
						 gint row,
						 gint col);
	void		(*selection_row_changed)(ESelectionModel *esm,
						 gint row);
	void		(*selection_changed)	(ESelectionModel *esm);
};

GType		e_selection_model_get_type	(void);
void		e_selection_model_do_something	(ESelectionModel *esm,
						 guint row,
						 guint col,
						 GdkModifierType state);
gboolean	e_selection_model_maybe_do_something
						(ESelectionModel *esm,
						 guint row,
						 guint col,
						 GdkModifierType state);
void		e_selection_model_right_click_down
						(ESelectionModel *selection,
						 guint row,
						 guint col,
						 GdkModifierType state);
void		e_selection_model_right_click_up
						(ESelectionModel *selection);
gboolean	e_selection_model_key_press	(ESelectionModel *esm,
						 GdkEventKey *key);
void		e_selection_model_select_as_key_press
						(ESelectionModel *esm,
						 guint row,
						 guint col,
						 GdkModifierType state);

/* Virtual functions */
gboolean	e_selection_model_is_row_selected
						(ESelectionModel *esm,
						 gint             n);
void		e_selection_model_foreach	(ESelectionModel *esm,
						 EForeachFunc     callback,
						 gpointer         closure);
void		e_selection_model_clear		(ESelectionModel *esm);
gint		e_selection_model_selected_count
						(ESelectionModel *esm);
void		e_selection_model_select_all	(ESelectionModel *esm);
void		e_selection_model_invert_selection
						(ESelectionModel *esm);
gint		e_selection_model_row_count	(ESelectionModel *esm);

/* Private virtual Functions */
void		e_selection_model_change_one_row
						(ESelectionModel *esm,
						 gint row,
						 gboolean on);
void		e_selection_model_change_cursor	(ESelectionModel *esm,
						 gint row,
						 gint col);
gint		e_selection_model_cursor_row	(ESelectionModel *esm);
gint		e_selection_model_cursor_col	(ESelectionModel *esm);
void		e_selection_model_select_single_row
						(ESelectionModel *selection,
						 gint row);
void		e_selection_model_toggle_single_row
						(ESelectionModel *selection,
						 gint row);
void		e_selection_model_move_selection_end
						(ESelectionModel *selection,
						 gint row);
void		e_selection_model_set_selection_end
						(ESelectionModel *selection,
						 gint row);

/* Signals */
void		e_selection_model_cursor_changed
						(ESelectionModel *selection,
						 gint row,
						 gint col);
void		e_selection_model_cursor_activated
						(ESelectionModel *selection,
						 gint row,
						 gint col);
void		e_selection_model_selection_row_changed
						(ESelectionModel *selection,
						 gint row);
void		e_selection_model_selection_changed
						(ESelectionModel *selection);

G_END_DECLS

#endif /* E_SELECTION_MODEL_H */

