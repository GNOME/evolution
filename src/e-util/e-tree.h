/*
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

#ifndef E_TREE_H
#define E_TREE_H

#include <gtk/gtk.h>
#include <libxml/tree.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-printable.h>
#include <e-util/e-table-extras.h>
#include <e-util/e-table-item.h>
#include <e-util/e-table-specification.h>
#include <e-util/e-table-state.h>
#include <e-util/e-tree-model.h>
#include <e-util/e-tree-table-adapter.h>
#include <e-util/e-tree-selection-model.h>

/* Standard GObject macros */
#define E_TYPE_TREE \
	(e_tree_get_type ())
#define E_TREE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TREE, ETree))
#define E_TREE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TREE, ETreeClass))
#define E_IS_TREE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TREE))
#define E_IS_TREE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TREE))
#define E_TREE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TREE))

G_BEGIN_DECLS

typedef struct _ETree ETree;
typedef struct _ETreeClass ETreeClass;
typedef struct _ETreePrivate ETreePrivate;

struct _ETree {
	GtkGrid parent;
	ETreePrivate *priv;
};

struct _ETreeClass {
	GtkGridClass parent_class;

	void		(*cursor_change)	(ETree *tree,
						 gint row,
						 ETreePath path);
	void		(*cursor_activated)	(ETree *tree,
						 gint row,
						 ETreePath path);
	void		(*selection_change)	(ETree *tree);
	void		(*double_click)		(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkEvent *event);
	gboolean	(*right_click)		(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkEvent *event);
	gboolean	(*click)		(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkEvent *event);
	gboolean	(*key_press)		(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkEvent *event);
	gboolean	(*start_drag)		(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkEvent *event);
	void		(*state_change)		(ETree *tree);
	gboolean	(*white_space_event)	(ETree *tree,
						 GdkEvent *event);

	/* Source side drag signals */
	void		(*tree_drag_begin)	(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkDragContext *context);
	void		(*tree_drag_end)	(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkDragContext *context);
	void		(*tree_drag_data_get)	(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkDragContext *context,
						 GtkSelectionData *selection_data,
						 guint info,
						 guint time);
	void		(*tree_drag_data_delete)
						(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkDragContext *context);

	/* Target side drag signals */
	void		(*tree_drag_leave)	(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkDragContext *context,
						 guint time);
	gboolean	(*tree_drag_motion)	(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkDragContext *context,
						 gint x,
						 gint y,
						 guint time);
	gboolean	(*tree_drag_drop)	(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkDragContext *context,
						 gint x,
						 gint y,
						 guint time);
	void		(*tree_drag_data_received)
						(ETree *tree,
						 gint row,
						 ETreePath path,
						 gint col,
						 GdkDragContext *context,
						 gint x,
						 gint y,
						 GtkSelectionData *selection_data,
						 guint info,
						 guint time);
};

GType		e_tree_get_type			(void) G_GNUC_CONST;
gboolean	e_tree_construct		(ETree *tree,
						 ETreeModel *etm,
						 ETableExtras *ete,
						 ETableSpecification *specification);
GtkWidget *	e_tree_new			(ETreeModel *etm,
						 ETableExtras *ete,
						 ETableSpecification *specification);

/* To save the state */
ETableState *	e_tree_get_state_object		(ETree *tree);
ETableSpecification *
		e_tree_get_spec			(ETree *tree);

/* note that it is more efficient to provide the state at creation time */
void		e_tree_set_state_object		(ETree *tree,
						 ETableState *state);
void		e_tree_show_cursor_after_reflow	(ETree *tree);

void		e_tree_set_cursor		(ETree *tree,
						 ETreePath path);

/* NULL means we don't have the cursor. */
ETreePath	e_tree_get_cursor		(ETree *tree);
void		e_tree_path_foreach		(ETree *tree,
						 ETreeForeachFunc callback,
						 gpointer closure);
void		e_tree_get_cell_at		(ETree *tree,
						 gint x,
						 gint y,
						 gint *row_return,
						 gint *col_return);
void		e_tree_get_cell_geometry	(ETree *tree,
						 gint row,
						 gint col,
						 gint *x_return,
						 gint *y_return,
						 gint *width_return,
						 gint *height_return);

/* Useful accessors */
ETreeModel *	e_tree_get_model		(ETree *tree);
ESelectionModel *
		e_tree_get_selection_model	(ETree *tree);
ETreeTableAdapter *
		e_tree_get_table_adapter	(ETree *tree);

/* Drag & drop stuff. */

/* Source side */
void		e_tree_drag_source_set		(ETree *tree,
						 GdkModifierType start_button_mask,
						 const GtkTargetEntry *targets,
						 gint n_targets,
						 GdkDragAction actions);
void		e_tree_drag_source_unset	(ETree *tree);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */
GdkDragContext *e_tree_drag_begin		(ETree *tree,
						 gint row,
						 gint col,
						 GtkTargetList *targets,
						 GdkDragAction actions,
						 gint button,
						 GdkEvent *event);

gboolean	e_tree_is_dragging		(ETree *tree);

ETableItem *	e_tree_get_item			(ETree *tree);

GnomeCanvasItem *
		e_tree_get_header_item		(ETree *tree);

void		e_tree_set_info_message		(ETree *tree,
						 const gchar *info_message);

void		e_tree_freeze_state_change	(ETree *tree);
void		e_tree_thaw_state_change	(ETree *tree);

gboolean	e_tree_is_editing		(ETree *tree);

gboolean	e_tree_get_grouped_view		(ETree *tree);
void		e_tree_set_grouped_view		(ETree *tree,
						 gboolean grouped_view);
gboolean	e_tree_get_sort_children_ascending
						(ETree *tree);
void		e_tree_set_sort_children_ascending
						(ETree *tree,
						 gboolean sort_children_ascending);
void		e_tree_customize_view		(ETree *tree);

G_END_DECLS

#endif /* E_TREE_H */

