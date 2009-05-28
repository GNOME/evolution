/*
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
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_TABLE_H_
#define _E_TABLE_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>
#include <table/e-table-model.h>
#include <table/e-table-header.h>
#include <table/e-table-group.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-item.h>
#include <table/e-table-selection-model.h>
#include <table/e-table-extras.h>
#include <table/e-table-specification.h>
#include <misc/e-printable.h>
#include <table/e-table-state.h>
#include <table/e-table-sorter.h>
#include <table/e-table-search.h>

G_BEGIN_DECLS

#define E_TABLE_TYPE        (e_table_get_type ())
#define E_TABLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_TYPE, ETable))
#define E_TABLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_TYPE, ETableClass))
#define E_IS_TABLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_TYPE))
#define E_IS_TABLE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_TYPE))

typedef struct _ETableDragSourceSite ETableDragSourceSite;

typedef enum {
	E_TABLE_CURSOR_LOC_NONE = 0,
	E_TABLE_CURSOR_LOC_ETCTA = 1 << 0,
	E_TABLE_CURSOR_LOC_TABLE = 1 << 1
} ETableCursorLoc;

typedef struct {
	GtkTable parent;

	ETableModel *model;

	ETableHeader *full_header, *header;

	GnomeCanvasItem *canvas_vbox;
	ETableGroup  *group;

	ETableSortInfo *sort_info;
	ETableSorter   *sorter;

	ETableSelectionModel *selection;
	ETableCursorLoc cursor_loc;
	ETableSpecification *spec;

	ETableSearch     *search;

	ETableCol        *current_search_col;

	guint	  search_search_id;
	guint	  search_accept_id;

	gint table_model_change_id;
	gint table_row_change_id;
	gint table_cell_change_id;
	gint table_rows_inserted_id;
	gint table_rows_deleted_id;

	gint group_info_change_id;
	gint sort_info_change_id;

	gint structure_change_id;
	gint expansion_change_id;
	gint dimension_change_id;

	gint reflow_idle_id;
	gint scroll_idle_id;

	GnomeCanvas *header_canvas, *table_canvas;

	GnomeCanvasItem *header_item, *root;

	GnomeCanvasItem *white_item;

	gint length_threshold;

	gint rebuild_idle_id;
	guint need_rebuild:1;

	/*
	 * Configuration settings
	 */
	guint alternating_row_colors : 1;
	guint horizontal_draw_grid : 1;
	guint vertical_draw_grid : 1;
	guint draw_focus : 1;
	guint row_selection_active : 1;

	guint horizontal_scrolling : 1;
	guint horizontal_resize : 1;

	guint is_grouped : 1;

	guint scroll_direction : 4;

	guint do_drag : 1;

	guint uniform_row_height : 1;
	guint allow_grouping : 1;

	guint always_search : 1;
	guint search_col_set : 1;

	gchar *click_to_add_message;
	GnomeCanvasItem *click_to_add;
	gboolean use_click_to_add;
	gboolean use_click_to_add_end;

	ECursorMode cursor_mode;

	gint drop_row;
	gint drop_col;
	GnomeCanvasItem *drop_highlight;
	gint last_drop_x;
	gint last_drop_y;
	gint last_drop_time;
	GdkDragContext *last_drop_context;

	gint drag_row;
	gint drag_col;
	ETableDragSourceSite *site;

	gint header_width;

	gchar *domain;
} ETable;

typedef struct {
	GtkTableClass parent_class;

	void        (*cursor_change)      (ETable *et, gint row);
	void        (*cursor_activated)   (ETable *et, gint row);
	void        (*selection_change)   (ETable *et);
	void        (*double_click)       (ETable *et, gint row, gint col, GdkEvent *event);
	gint        (*right_click)        (ETable *et, gint row, gint col, GdkEvent *event);
	gint        (*click)              (ETable *et, gint row, gint col, GdkEvent *event);
	gint        (*key_press)          (ETable *et, gint row, gint col, GdkEvent *event);
	gint        (*start_drag)         (ETable *et, gint row, gint col, GdkEvent *event);
	void        (*state_change)       (ETable *et);
	gint        (*white_space_event)  (ETable *et, GdkEvent *event);

	void  (*set_scroll_adjustments)   (ETable	 *table,
					   GtkAdjustment *hadjustment,
					   GtkAdjustment *vadjustment);

	/* Source side drag signals */
	void (* table_drag_begin)		   (ETable	       *table,
						    gint                 row,
						    gint                 col,
						    GdkDragContext     *context);
	void (* table_drag_end)		   (ETable	       *table,
					    gint                 row,
					    gint                 col,
					    GdkDragContext     *context);
	void (* table_drag_data_get)             (ETable             *table,
						  gint                 row,
						  gint                 col,
						  GdkDragContext     *context,
						  GtkSelectionData   *selection_data,
						  guint               info,
						  guint               time);
	void (* table_drag_data_delete)          (ETable	       *table,
						  gint                 row,
						  gint                 col,
						  GdkDragContext     *context);

	/* Target side drag signals */
	void (* table_drag_leave)		   (ETable	       *table,
						    gint                 row,
						    gint                 col,
						    GdkDragContext     *context,
						    guint               time);
	gboolean (* table_drag_motion)           (ETable	       *table,
						  gint                 row,
						  gint                 col,
						  GdkDragContext     *context,
						  gint                x,
						  gint                y,
						  guint               time);
	gboolean (* table_drag_drop)             (ETable	       *table,
						  gint                 row,
						  gint                 col,
						  GdkDragContext     *context,
						  gint                x,
						  gint                y,
						  guint               time);
	void (* table_drag_data_received)        (ETable             *table,
						  gint                 row,
						  gint                 col,
						  GdkDragContext     *context,
						  gint                x,
						  gint                y,
						  GtkSelectionData   *selection_data,
						  guint               info,
						  guint               time);
} ETableClass;
GType            e_table_get_type                  (void);
ETable          *e_table_construct                 (ETable               *e_table,
						    ETableModel          *etm,
						    ETableExtras         *ete,
						    const gchar           *spec,
						    const gchar           *state);
GtkWidget       *e_table_new                       (ETableModel          *etm,
						    ETableExtras         *ete,
						    const gchar           *spec,
						    const gchar           *state);

/* Create an ETable using files. */
ETable          *e_table_construct_from_spec_file  (ETable               *e_table,
						    ETableModel          *etm,
						    ETableExtras         *ete,
						    const gchar           *spec_fn,
						    const gchar           *state_fn);
GtkWidget       *e_table_new_from_spec_file        (ETableModel          *etm,
						    ETableExtras         *ete,
						    const gchar           *spec_fn,
						    const gchar           *state_fn);

/* To save the state */
gchar           *e_table_get_state                 (ETable               *e_table);
void             e_table_save_state                (ETable               *e_table,
						    const gchar          *filename);
ETableState     *e_table_get_state_object          (ETable               *e_table);

/* note that it is more efficient to provide the state at creation time */
void             e_table_set_state                 (ETable               *e_table,
						    const gchar          *state);
void             e_table_set_state_object          (ETable               *e_table,
						    ETableState          *state);
void             e_table_load_state                (ETable               *e_table,
						    const gchar          *filename);
void             e_table_set_cursor_row            (ETable               *e_table,
						    gint                   row);

/* -1 means we don't have the cursor.  This is in model rows. */
gint              e_table_get_cursor_row            (ETable               *e_table);
void             e_table_selected_row_foreach      (ETable               *e_table,
						    EForeachFunc          callback,
						    gpointer              closure);
gint             e_table_selected_count            (ETable               *e_table);
EPrintable      *e_table_get_printable             (ETable               *e_table);
gint             e_table_get_next_row              (ETable               *e_table,
						    gint                  model_row);
gint             e_table_get_prev_row              (ETable               *e_table,
						    gint                  model_row);
gint             e_table_model_to_view_row         (ETable               *e_table,
						    gint                  model_row);
gint             e_table_view_to_model_row         (ETable               *e_table,
						    gint                  view_row);
void             e_table_get_cell_at               (ETable               *table,
						    gint                   x,
						    gint                   y,
						    gint                  *row_return,
						    gint                  *col_return);
void e_table_get_mouse_over_cell (ETable *table, gint *row, gint *col);
void             e_table_get_cell_geometry         (ETable               *table,
						    gint                   row,
						    gint                   col,
						    gint                  *x_return,
						    gint                  *y_return,
						    gint                  *width_return,
						    gint                  *height_return);

/* Useful accessor functions. */
ESelectionModel *e_table_get_selection_model       (ETable               *table);

/* Drag & drop stuff. */
/* Target */
void             e_table_drag_get_data             (ETable               *table,
						    gint                   row,
						    gint                   col,
						    GdkDragContext       *context,
						    GdkAtom               target,
						    guint32               time);
void             e_table_drag_highlight            (ETable               *table,
						    gint                   row,
						    gint                   col); /* col == -1 to highlight entire row. */
void             e_table_drag_unhighlight          (ETable               *table);
void             e_table_drag_dest_set             (ETable               *table,
						    GtkDestDefaults       flags,
						    const GtkTargetEntry *targets,
						    gint                  n_targets,
						    GdkDragAction         actions);
void             e_table_drag_dest_set_proxy       (ETable               *table,
						    GdkWindow            *proxy_window,
						    GdkDragProtocol       protocol,
						    gboolean              use_coordinates);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */
void             e_table_drag_dest_unset           (GtkWidget            *widget);

/* Source side */
void             e_table_drag_source_set           (ETable               *table,
						    GdkModifierType       start_button_mask,
						    const GtkTargetEntry *targets,
						    gint                  n_targets,
						    GdkDragAction         actions);
void             e_table_drag_source_unset         (ETable               *table);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */
GdkDragContext  *e_table_drag_begin                (ETable               *table,
						    gint                   row,
						    gint                   col,
						    GtkTargetList        *targets,
						    GdkDragAction         actions,
						    gint                  button,
						    GdkEvent             *event);

/* selection stuff */
void             e_table_select_all                (ETable               *table);
void             e_table_invert_selection          (ETable               *table);

/* This function is only needed in single_selection_mode. */
void             e_table_right_click_up            (ETable               *table);

void             e_table_commit_click_to_add       (ETable               *table);

G_END_DECLS

#endif /* _E_TABLE_H_ */

