/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-tree.h
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

#ifndef _E_TREE_H_
#define _E_TREE_H_

#include <gtk/gtkdnd.h>
#include <gtk/gtktable.h>
#include <libxml/tree.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <misc/e-printable.h>

#include <table/e-table-extras.h>
#include <table/e-table-specification.h>
#include <table/e-table-state.h>
#include <table/e-tree-model.h>
#include <table/e-tree-table-adapter.h>
#include <table/e-table-item.h>

#define E_TREE_USE_TREE_SELECTION

#ifdef E_TREE_USE_TREE_SELECTION
#include <table/e-tree-selection-model.h>
#endif

G_BEGIN_DECLS

#define E_TREE_TYPE        (e_tree_get_type ())
#define E_TREE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TREE_TYPE, ETree))
#define E_TREE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TREE_TYPE, ETreeClass))
#define E_IS_TREE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TREE_TYPE))
#define E_IS_TREE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TREE_TYPE))
typedef struct _ETreeDragSourceSite ETreeDragSourceSite;
typedef struct ETreePriv ETreePriv;

typedef struct {
	GtkTable parent;

	ETreePriv *priv;
} ETree;

typedef struct {
	GtkTableClass parent_class;

	void        (*cursor_change)      (ETree *et, int row, ETreePath path);
	void        (*cursor_activated)   (ETree *et, int row, ETreePath path);
	void        (*selection_change)   (ETree *et);
	void        (*double_click)       (ETree *et, int row, ETreePath path, int col, GdkEvent *event);
	gint        (*right_click)        (ETree *et, int row, ETreePath path, int col, GdkEvent *event);
	gint        (*click)              (ETree *et, int row, ETreePath path, int col, GdkEvent *event);
	gint        (*key_press)          (ETree *et, int row, ETreePath path, int col, GdkEvent *event);
	gint        (*start_drag)         (ETree *et, int row, ETreePath path, int col, GdkEvent *event);
	gint        (*state_change)       (ETree *et);
	gint        (*white_space_event)  (ETree *et, GdkEvent *event);

	void  (*set_scroll_adjustments)   (ETree	 *tree,
					   GtkAdjustment *hadjustment,
					   GtkAdjustment *vadjustment);

	/* Source side drag signals */
	void (* tree_drag_begin)	           (ETree	       *tree,
						    int                 row,
						    ETreePath           path,
						    int                 col,
						    GdkDragContext     *context);
	void (* tree_drag_end)	           (ETree	       *tree,
					    int                 row,
					    ETreePath           path,
					    int                 col,
					    GdkDragContext     *context);
	void (* tree_drag_data_get)             (ETree             *tree,
						 int                 row,
						 ETreePath           path,
						 int                 col,
						 GdkDragContext     *context,
						 GtkSelectionData   *selection_data,
						 guint               info,
						 guint               time);
	void (* tree_drag_data_delete)          (ETree	       *tree,
						 int                 row,
						 ETreePath           path,
						 int                 col,
						 GdkDragContext     *context);
	
	/* Target side drag signals */	   
	void (* tree_drag_leave)	           (ETree	       *tree,
						    int                 row,
						    ETreePath           path,
						    int                 col,
						    GdkDragContext     *context,
						    guint               time);
	gboolean (* tree_drag_motion)           (ETree	       *tree,
						 int                 row,
						 ETreePath           path,
						 int                 col,
						 GdkDragContext     *context,
						 gint                x,
						 gint                y,
						 guint               time);
	gboolean (* tree_drag_drop)             (ETree	       *tree,
						 int                 row,
						 ETreePath           path,
						 int                 col,
						 GdkDragContext     *context,
						 gint                x,
						 gint                y,
						 guint               time);
	void (* tree_drag_data_received)        (ETree             *tree,
						 int                 row,
						 ETreePath           path,
						 int                 col,
						 GdkDragContext     *context,
						 gint                x,
						 gint                y,
						 GtkSelectionData   *selection_data,
						 guint               info,
						 guint               time);
} ETreeClass;

GType           e_tree_get_type                   (void);
ETree          *e_tree_construct                  (ETree                *e_tree,
						   ETreeModel           *etm,
						   ETableExtras         *ete,
						   const char           *spec,
						   const char           *state);
GtkWidget      *e_tree_new                        (ETreeModel           *etm,
						   ETableExtras         *ete,
						   const char           *spec,
						   const char           *state);

/* Create an ETree using files. */
ETree          *e_tree_construct_from_spec_file   (ETree                *e_tree,
						   ETreeModel           *etm,
						   ETableExtras         *ete,
						   const char           *spec_fn,
						   const char           *state_fn);
GtkWidget      *e_tree_new_from_spec_file         (ETreeModel           *etm,
						   ETableExtras         *ete,
						   const char           *spec_fn,
						   const char           *state_fn);

/* To save the state */
gchar          *e_tree_get_state                  (ETree                *e_tree);
void            e_tree_save_state                 (ETree                *e_tree,
						   const gchar          *filename);
ETableState    *e_tree_get_state_object           (ETree                *e_tree);
ETableSpecification    *e_tree_get_spec           (ETree                *e_tree);

/* note that it is more efficient to provide the state at creation time */
void            e_tree_set_search_column          (ETree *e_tree,
						   gint  col);
void            e_tree_set_state                  (ETree                *e_tree,
						   const gchar          *state);
void            e_tree_set_state_object           (ETree                *e_tree,
						   ETableState          *state);
void            e_tree_load_state                 (ETree                *e_tree,
						   const gchar          *filename);
void            e_tree_set_cursor                 (ETree                *e_tree,
						   ETreePath             path);

/* NULL means we don't have the cursor. */
ETreePath       e_tree_get_cursor                 (ETree                *e_tree);
void            e_tree_selected_row_foreach       (ETree                *e_tree,
						   EForeachFunc          callback,
						   gpointer              closure);
#ifdef E_TREE_USE_TREE_SELECTION
void            e_tree_selected_path_foreach      (ETree                *e_tree,
						   ETreeForeachFunc      callback,
						   gpointer              closure);
void            e_tree_path_foreach               (ETree                *e_tree,
						   ETreeForeachFunc      callback,
						   gpointer              closure);
#endif
gint            e_tree_selected_count             (ETree                *e_tree);
EPrintable     *e_tree_get_printable              (ETree                *e_tree);
gint            e_tree_get_next_row               (ETree                *e_tree,
						   gint                  model_row);
gint            e_tree_get_prev_row               (ETree                *e_tree,
						   gint                  model_row);
gint            e_tree_model_to_view_row          (ETree                *e_tree,
						   gint                  model_row);
gint            e_tree_view_to_model_row          (ETree                *e_tree,
						   gint                  view_row);
void            e_tree_get_cell_at                (ETree                *tree,
						   int                   x,
						   int                   y,
						   int                  *row_return,
						   int                  *col_return);
void            e_tree_get_cell_geometry          (ETree                *tree,
						   int                   row,
						   int                   col,
						   int                  *x_return,
						   int                  *y_return,
						   int                  *width_return,
						   int                  *height_return);

/* Useful accessors */
ETreeModel *    e_tree_get_model                  (ETree *et);
ESelectionModel *e_tree_get_selection_model       (ETree *et);
ETreeTableAdapter *e_tree_get_table_adapter       (ETree *et);

/* Drag & drop stuff. */
/* Target */
void            e_tree_drag_get_data              (ETree                *tree,
						   int                   row,
						   int                   col,
						   GdkDragContext       *context,
						   GdkAtom               target,
						   guint32               time);
void            e_tree_drag_highlight             (ETree                *tree,
						   int                   row,
						   int                   col); /* col == -1 to highlight entire row. */
void            e_tree_drag_unhighlight           (ETree                *tree);
void            e_tree_drag_dest_set              (ETree                *tree,
						   GtkDestDefaults       flags,
						   const GtkTargetEntry *targets,
						   gint                  n_targets,
						   GdkDragAction         actions);
void            e_tree_drag_dest_set_proxy        (ETree                *tree,
						   GdkWindow            *proxy_window,
						   GdkDragProtocol       protocol,
						   gboolean              use_coordinates);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */
void            e_tree_drag_dest_unset            (GtkWidget            *widget);

/* Source side */
void            e_tree_drag_source_set            (ETree                *tree,
						   GdkModifierType       start_button_mask,
						   const GtkTargetEntry *targets,
						   gint                  n_targets,
						   GdkDragAction         actions);
void            e_tree_drag_source_unset          (ETree                *tree);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */
GdkDragContext *e_tree_drag_begin                 (ETree                *tree,
						   int                   row,
						   int                   col,
						   GtkTargetList        *targets,
						   GdkDragAction         actions,
						   gint                  button,
                                                   GdkEvent             *event);

/* Adapter functions */
gboolean        e_tree_node_is_expanded           (ETree                *et,
						   ETreePath             path);
void            e_tree_node_set_expanded          (ETree                *et,
						   ETreePath             path,
						   gboolean              expanded);
void            e_tree_node_set_expanded_recurse  (ETree                *et,
						   ETreePath             path,
						   gboolean              expanded);
void            e_tree_root_node_set_visible      (ETree                *et,
						   gboolean              visible);
ETreePath       e_tree_node_at_row                (ETree                *et,
						   int                   row);
int             e_tree_row_of_node                (ETree                *et,
						   ETreePath             path);
gboolean        e_tree_root_node_is_visible       (ETree                *et);
void            e_tree_show_node                  (ETree                *et,
						   ETreePath             path);
void            e_tree_save_expanded_state        (ETree                *et,
						   char                 *filename);
void            e_tree_load_expanded_state        (ETree                *et,
						   char                 *filename);
int             e_tree_row_count                  (ETree                *et);
GtkWidget      *e_tree_get_tooltip                (ETree                *et);

typedef enum {
	E_TREE_FIND_NEXT_BACKWARD = 0,
	E_TREE_FIND_NEXT_FORWARD  = 1 << 0,
	E_TREE_FIND_NEXT_WRAP     = 1 << 1
} ETreeFindNextParams;

gboolean        e_tree_find_next                  (ETree                *et,
						   ETreeFindNextParams   params,
						   ETreePathFunc         func,
						   gpointer              data);

/* This function is only needed in single_selection_mode. */
void            e_tree_right_click_up             (ETree                *et);

ETableItem *	e_tree_get_item(ETree * et);

GnomeCanvasItem * e_tree_get_header_item(ETree * et);

G_END_DECLS

#endif /* _E_TREE_H_ */

