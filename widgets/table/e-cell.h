/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-cell.h
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
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

#ifndef _E_CELL_H_
#define _E_CELL_H_

#include <gdk/gdktypes.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-font.h>
#include <table/e-table-model.h>
#include <table/e-table-tooltip.h>

G_BEGIN_DECLS

#define E_CELL_TYPE         (e_cell_get_type ())
#define E_CELL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_TYPE, ECell))
#define E_CELL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_TYPE, ECellClass))
#define E_CELL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_CELL_TYPE, ECellClass))
#define E_IS_CELL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_TYPE))
#define E_IS_CELL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_TYPE))

typedef gboolean (*ETableSearchFunc) (gconstpointer haystack,
				      const char *needle);

typedef enum {
	E_CELL_SELECTED       = 1 << 0,

	E_CELL_JUSTIFICATION  = 3 << 1,
	E_CELL_JUSTIFY_CENTER = 0 << 1,
	E_CELL_JUSTIFY_LEFT   = 1 << 1,
	E_CELL_JUSTIFY_RIGHT  = 2 << 1,
	E_CELL_JUSTIFY_FILL   = 3 << 1,

	E_CELL_ALIGN_LEFT     = 1 << 1,
	E_CELL_ALIGN_RIGHT    = 1 << 2,

	E_CELL_FOCUSED        = 1 << 3,

	E_CELL_EDITING        = 1 << 4,

	E_CELL_CURSOR         = 1 << 5,

	E_CELL_PREEDIT        = 1 << 6
} ECellFlags;

typedef enum {
	E_CELL_GRAB           = 1 << 0,
	E_CELL_UNGRAB         = 1 << 1
} ECellActions;

typedef struct {
	GtkObject       object;
} ECell;

typedef struct {
	ECell *ecell;
	ETableModel *e_table_model;
	void        *e_table_item_view;
	
	gint   focus_x1, focus_y1, focus_x2, focus_y2;
	gint   focus_col, focus_row;
} ECellView;

#define E_CELL_IS_FOCUSED(ecell_view) (ecell_view->focus_x1 != -1)

typedef struct {
	GtkObjectClass parent_class;
	
	ECellView *(*new_view)         (ECell *ecell, ETableModel *table_model, void *e_table_item_view);
	void       (*kill_view)        (ECellView *ecell_view);

	void       (*realize)          (ECellView *ecell_view);
	void       (*unrealize)        (ECellView *ecell_view);

	void   	   (*draw)             (ECellView *ecell_view, GdkDrawable *drawable,
	       			       	int model_col, int view_col, int row,
				       	ECellFlags flags, int x1, int y1, int x2, int y2);
	gint   	   (*event)            (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row, ECellFlags flags, ECellActions *actions);
	void   	   (*focus)            (ECellView *ecell_view, int model_col, int view_col,
				       	int row, int x1, int y1, int x2, int y2);
	void   	   (*unfocus)          (ECellView *ecell_view);
	int        (*height)           (ECellView *ecell_view, int model_col, int view_col, int row);

	void      *(*enter_edit)       (ECellView *ecell_view, int model_col, int view_col, int row);
	void       (*leave_edit)       (ECellView *ecell_view, int model_col, int view_col, int row, void *context);
	void      *(*save_state)       (ECellView *ecell_view, int model_col, int view_col, int row, void *context);
	void       (*load_state)       (ECellView *ecell_view, int model_col, int view_col, int row, void *context, void *save_state);
	void       (*free_state)       (ECellView *ecell_view, int model_col, int view_col, int row, void *save_state);
	void       (*print)            (ECellView *ecell_view, GnomePrintContext *context,
				       	int model_col, int view_col, int row,
				       	gdouble width, gdouble height);
	gdouble    (*print_height)     (ECellView *ecell_view, GnomePrintContext *context,
				       	int model_col, int view_col, int row, gdouble width);
	int        (*max_width)        (ECellView *ecell_view, int model_col, int view_col);
	int        (*max_width_by_row) (ECellView *ecell_view, int model_col, int view_col, int row);
	void       (*show_tooltip)     (ECellView *ecell_view, int model_col, int view_col, int row, int col_width, ETableTooltip *tooltip);
	gchar     *(*get_bg_color)     (ECellView *ecell_view, int row);

	void       (*style_set)        (ECellView *ecell_view, GtkStyle *previous_style);
} ECellClass;

GType      e_cell_get_type                      (void);

/* View creation methods. */
ECellView *e_cell_new_view                      (ECell             *ecell,
						 ETableModel       *table_model,
						 void              *e_table_item_view);
void       e_cell_kill_view                     (ECellView         *ecell_view);

/* Cell View methods. */
gint       e_cell_event                         (ECellView         *ecell_view,
						 GdkEvent          *event,
						 int                model_col,
						 int                view_col,
						 int                row,
						 ECellFlags         flags,
						 ECellActions      *actions);
void       e_cell_realize                       (ECellView         *ecell_view);
void       e_cell_unrealize                     (ECellView         *ecell_view);
void       e_cell_draw                          (ECellView         *ecell_view,
						 GdkDrawable       *drawable,
						 int                model_col,
						 int                view_col,
						 int                row,
						 ECellFlags         flags,
						 int                x1,
						 int                y1,
						 int                x2,
						 int                y2);
void       e_cell_print                         (ECellView         *ecell_view,
						 GnomePrintContext *context,
						 int                model_col,
						 int                view_col,
						 int                row,
						 double             width,
						 double             height);
gdouble    e_cell_print_height                  (ECellView         *ecell_view,
						 GnomePrintContext *context,
						 int                model_col,
						 int                view_col,
						 int                row,
						 gdouble            width);
int        e_cell_max_width                     (ECellView         *ecell_view,
						 int                model_col,
						 int                view_col);
int        e_cell_max_width_by_row              (ECellView         *ecell_view,
						 int                model_col,
						 int                view_col,
						 int                row);
gboolean   e_cell_max_width_by_row_implemented  (ECellView         *ecell_view);
void       e_cell_show_tooltip                  (ECellView         *ecell_view,
						 int                model_col,
						 int                view_col,
						 int                row,
						 int                col_width,
						 ETableTooltip     *tooltip);
gchar     *e_cell_get_bg_color                  (ECellView         *ecell_view,
						 int                row);
void       e_cell_style_set                     (ECellView         *ecell_view,
						 GtkStyle          *previous_style);

void       e_cell_focus                         (ECellView         *ecell_view,
						 int                model_col,
						 int                view_col,
						 int                row,
						 int                x1,
						 int                y1,
						 int                x2,
						 int                y2);
void       e_cell_unfocus                       (ECellView         *ecell_view);
int        e_cell_height                        (ECellView         *ecell_view,
						 int                model_col,
						 int                view_col,
						 int                row);
void      *e_cell_enter_edit                    (ECellView         *ecell_view,
						 int                model_col,
						 int                view_col,
						 int                row);
void       e_cell_leave_edit                    (ECellView         *ecell_view,
						 int                model_col,
						 int                view_col,
						 int                row,
						 void              *edit_context);
void      *e_cell_save_state                    (ECellView         *ecell_view,
						 int                model_col,
						 int                view_col,
						 int                row,
						 void              *edit_context);
void       e_cell_load_state                    (ECellView         *ecell_view,
						 int                model_col,
						 int                view_col,
						 int                row,
						 void              *edit_context,
						 void              *state);
void       e_cell_free_state                    (ECellView         *ecell_view,
						 int                model_col,
						 int                view_col,
						 int                row,
						 void              *state);

G_END_DECLS

#endif /* _E_CELL_H_ */
