/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TREE_H_
#define _E_TREE_H_

#include <gtk/gtktable.h>
#include <gnome-xml/tree.h>

#include <gal/widgets/e-printable.h>

#include <gal/e-table/e-table-extras.h>
#include <gal/e-table/e-table-specification.h>
#include <gal/e-table/e-table-state.h>
#include <gal/e-table/e-tree-model.h>
#include <gal/e-table/e-tree-selection-model.h>

BEGIN_GNOME_DECLS

#define E_TREE_TYPE        (e_tree_get_type ())
#define E_TREE(o)          (GTK_CHECK_CAST ((o), E_TREE_TYPE, ETree))
#define E_TREE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_TYPE, ETreeClass))
#define E_IS_TREE(o)       (GTK_CHECK_TYPE ((o), E_TREE_TYPE))
#define E_IS_TREE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_TYPE))

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

GtkType         e_tree_get_type                  (void);

ETree         *e_tree_construct                 (ETree               *e_tree,
						 ETreeModel          *etm,
						 ETableExtras         *ete,
						 const char           *spec,
						 const char           *state);
GtkWidget      *e_tree_new                       (ETreeModel          *etm,
						  ETableExtras         *ete,
						  const char           *spec,
						  const char           *state);

/* Create an ETree using files. */
ETree         *e_tree_construct_from_spec_file  (ETree               *e_tree,
						 ETreeModel          *etm,
						 ETableExtras         *ete,
						 const char           *spec_fn,
						 const char           *state_fn);
GtkWidget      *e_tree_new_from_spec_file        (ETreeModel          *etm,
						  ETableExtras         *ete,
						  const char           *spec_fn,
						  const char           *state_fn);

/* To save the state */
gchar          *e_tree_get_state                 (ETree               *e_tree);
void            e_tree_save_state                (ETree               *e_tree,
						  const gchar          *filename);
ETableState    *e_tree_get_state_object          (ETree               *e_tree);

/* note that it is more efficient to provide the state at creation time */
void            e_tree_set_state                 (ETree               *e_tree,
						  const gchar          *state);
void            e_tree_set_state_object          (ETree               *e_tree,
						  ETableState          *state);
void            e_tree_load_state                (ETree               *e_tree,
						  const gchar          *filename);

void            e_tree_set_cursor                (ETree               *e_tree,
						  ETreePath            path);

/* NULL means we don't have the cursor. */
ETreePath       e_tree_get_cursor                (ETree               *e_tree);
void            e_tree_selected_row_foreach      (ETree               *e_tree,
						  EForeachFunc         callback,
						  gpointer             closure);
void            e_tree_selected_path_foreach     (ETree               *e_tree,
						  ETreeForeachFunc     callback,
						  gpointer             closure);
gint            e_tree_selected_count            (ETree               *e_tree);
EPrintable     *e_tree_get_printable             (ETree               *e_tree);

gint            e_tree_get_next_row              (ETree               *e_tree,
						  gint                  model_row);
gint            e_tree_get_prev_row              (ETree               *e_tree,
						  gint                  model_row);

gint            e_tree_model_to_view_row         (ETree               *e_tree,
						  gint                  model_row);
gint            e_tree_view_to_model_row         (ETree               *e_tree,
						  gint                  view_row);
void            e_tree_get_cell_at               (ETree *tree,
						  int x, int y,
						  int *row_return, int *col_return);

/* Drag & drop stuff. */
/* Target */
void            e_tree_drag_get_data             (ETree               *tree,
						  int                   row,
						  int                   col,
						  GdkDragContext       *context,
						  GdkAtom               target,
						  guint32               time);
void            e_tree_drag_highlight            (ETree               *tree,
						  int                   row,
						  int                   col); /* col == -1 to highlight entire row. */
void            e_tree_drag_unhighlight          (ETree               *tree);
void            e_tree_drag_dest_set             (ETree               *tree,
						  GtkDestDefaults       flags,
						  const GtkTargetEntry *targets,
						  gint                  n_targets,
						  GdkDragAction         actions);
void            e_tree_drag_dest_set_proxy       (ETree               *tree,
						  GdkWindow            *proxy_window,
						  GdkDragProtocol       protocol,
						  gboolean              use_coordinates);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */
void            e_tree_drag_dest_unset           (GtkWidget            *widget);

/* Source side */
void            e_tree_drag_source_set           (ETree               *tree,
						  GdkModifierType       start_button_mask,
						  const GtkTargetEntry *targets,
						  gint                  n_targets,
						  GdkDragAction         actions);
void            e_tree_drag_source_unset         (ETree               *tree);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */
GdkDragContext *e_tree_drag_begin                (ETree                *tree,
						  int                   row,
						  int                   col,
						  GtkTargetList        *targets,
						  GdkDragAction         actions,
						  gint                  button,
						  GdkEvent             *event);

/* selection stuff */
void            e_tree_select_all                (ETree               *tree);
void            e_tree_invert_selection          (ETree               *tree);


/* Adapter functions */

gboolean     e_tree_node_is_expanded (ETree *et, ETreePath path);
void         e_tree_node_set_expanded (ETree *et, ETreePath path, gboolean expanded);
void         e_tree_node_set_expanded_recurse (ETree *et, ETreePath path, gboolean expanded);
void         e_tree_root_node_set_visible (ETree *et, gboolean visible);
ETreePath    e_tree_node_at_row (ETree *et, int row);
int          e_tree_row_of_node (ETree *et, ETreePath path);
gboolean     e_tree_root_node_is_visible(ETree *et);

void         e_tree_show_node (ETree *et, ETreePath path);

void   	     e_tree_save_expanded_state (ETree *et, char *filename);
void   	     e_tree_load_expanded_state (ETree *et, char *filename);

int          e_tree_row_count           (ETree *et);

END_GNOME_DECLS

#endif /* _E_TREE_H_ */

