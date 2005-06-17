/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table.h - A graphical view of a Table.
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Miguel de Icaza <miguel@ximian.com>
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

#ifndef _E_TABLE_H_
#define _E_TABLE_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtktable.h>
#include <libxml/tree.h>
#include <table/e-table-model.h>
#include <table/e-table-header.h>
#include <table/e-table-group.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-item.h>
#include <table/e-table-selection-model.h>
#include <table/e-table-extras.h>
#include <table/e-table-specification.h>
#include <widgets/misc/e-printable.h>
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

	guint   	  search_search_id;
	guint   	  search_accept_id;

	int table_model_change_id;
	int table_row_change_id;
	int table_cell_change_id;
	int table_rows_inserted_id;
	int table_rows_deleted_id;

	int group_info_change_id;
	int sort_info_change_id;

	int structure_change_id;
	int expansion_change_id;
	int dimension_change_id;

	int reflow_idle_id;
	int scroll_idle_id;

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

	char *click_to_add_message;
	GnomeCanvasItem *click_to_add;
	gboolean use_click_to_add;
	gboolean use_click_to_add_end;

	ECursorMode cursor_mode;

	int drop_row;
	int drop_col;
	GnomeCanvasItem *drop_highlight;
	int last_drop_x;
	int last_drop_y;
	int last_drop_time;
	GdkDragContext *last_drop_context;

	int drag_row;
	int drag_col;
	ETableDragSourceSite *site;

	int header_width;

	char *domain;
} ETable;

typedef struct {
	GtkTableClass parent_class;

	void        (*cursor_change)      (ETable *et, int row);
	void        (*cursor_activated)   (ETable *et, int row);
	void        (*selection_change)   (ETable *et);
	void        (*double_click)       (ETable *et, int row, int col, GdkEvent *event);
	gint        (*right_click)        (ETable *et, int row, int col, GdkEvent *event);
	gint        (*click)              (ETable *et, int row, int col, GdkEvent *event);
	gint        (*key_press)          (ETable *et, int row, int col, GdkEvent *event);
	gint        (*start_drag)         (ETable *et, int row, int col, GdkEvent *event);
	void        (*state_change)       (ETable *et);
	gint        (*white_space_event)  (ETable *et, GdkEvent *event);

	void  (*set_scroll_adjustments)   (ETable	 *table,
					   GtkAdjustment *hadjustment,
					   GtkAdjustment *vadjustment);

	/* Source side drag signals */
	void (* table_drag_begin)	           (ETable	       *table,
						    int                 row,
						    int                 col,
						    GdkDragContext     *context);
	void (* table_drag_end)	           (ETable	       *table,
					    int                 row,
					    int                 col,
					    GdkDragContext     *context);
	void (* table_drag_data_get)             (ETable             *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context,
						  GtkSelectionData   *selection_data,
						  guint               info,
						  guint               time);
	void (* table_drag_data_delete)          (ETable	       *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context);
	
	/* Target side drag signals */	   
	void (* table_drag_leave)	           (ETable	       *table,
						    int                 row,
						    int                 col,
						    GdkDragContext     *context,
						    guint               time);
	gboolean (* table_drag_motion)           (ETable	       *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context,
						  gint                x,
						  gint                y,
						  guint               time);
	gboolean (* table_drag_drop)             (ETable	       *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context,
						  gint                x,
						  gint                y,
						  guint               time);
	void (* table_drag_data_received)        (ETable             *table,
						  int                 row,
						  int                 col,
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
						    const char           *spec,
						    const char           *state);
GtkWidget       *e_table_new                       (ETableModel          *etm,
						    ETableExtras         *ete,
						    const char           *spec,
						    const char           *state);

/* Create an ETable using files. */
ETable          *e_table_construct_from_spec_file  (ETable               *e_table,
						    ETableModel          *etm,
						    ETableExtras         *ete,
						    const char           *spec_fn,
						    const char           *state_fn);
GtkWidget       *e_table_new_from_spec_file        (ETableModel          *etm,
						    ETableExtras         *ete,
						    const char           *spec_fn,
						    const char           *state_fn);

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
						    int                   row);

/* -1 means we don't have the cursor.  This is in model rows. */
int              e_table_get_cursor_row            (ETable               *e_table);
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
						    int                   x,
						    int                   y,
						    int                  *row_return,
						    int                  *col_return);
void             e_table_get_cell_geometry         (ETable               *table,
						    int                   row,
						    int                   col,
						    int                  *x_return,
						    int                  *y_return,
						    int                  *width_return,
						    int                  *height_return);

/* Useful accessor functions. */
ESelectionModel *e_table_get_selection_model       (ETable               *table);

/* Drag & drop stuff. */
/* Target */
void             e_table_drag_get_data             (ETable               *table,
						    int                   row,
						    int                   col,
						    GdkDragContext       *context,
						    GdkAtom               target,
						    guint32               time);
void             e_table_drag_highlight            (ETable               *table,
						    int                   row,
						    int                   col); /* col == -1 to highlight entire row. */
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
						    int                   row,
						    int                   col,
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

void             e_table_commit_click_to_add       (ETable               *table);

G_END_DECLS

#endif /* _E_TABLE_H_ */

