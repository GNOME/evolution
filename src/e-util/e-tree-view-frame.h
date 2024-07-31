/*
 * e-tree-view-frame.h
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TREE_VIEW_FRAME_H
#define E_TREE_VIEW_FRAME_H

#include <gtk/gtk.h>

#include <e-util/e-ui-action.h>

/* Standard GObject macros */
#define E_TYPE_TREE_VIEW_FRAME \
	(e_tree_view_frame_get_type ())
#define E_TREE_VIEW_FRAME(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TREE_VIEW_FRAME, ETreeViewFrame))
#define E_TREE_VIEW_FRAME_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TREE_VIEW_FRAME, ETreeViewFrameClass))
#define E_IS_TREE_VIEW_FRAME(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TREE_VIEW_FRAME))
#define E_IS_TREE_VIEW_FRAME_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TREE_VIEW_FRAME))
#define E_TREE_VIEW_FRAME_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TREE_VIEW_FRAME, ETreeViewFrameClass))

#define E_TREE_VIEW_FRAME_ACTION_ADD		"e-tree-view-frame-add"
#define E_TREE_VIEW_FRAME_ACTION_REMOVE		"e-tree-view-frame-remove"
#define E_TREE_VIEW_FRAME_ACTION_MOVE_TOP	"e-tree-view-frame-move-top"
#define E_TREE_VIEW_FRAME_ACTION_MOVE_UP	"e-tree-view-frame-move-up"
#define E_TREE_VIEW_FRAME_ACTION_MOVE_DOWN	"e-tree-view-frame-move-down"
#define E_TREE_VIEW_FRAME_ACTION_MOVE_BOTTOM	"e-tree-view-frame-move-bottom"
#define E_TREE_VIEW_FRAME_ACTION_SELECT_ALL	"e-tree-view-frame-select-all"

G_BEGIN_DECLS

typedef struct _ETreeViewFrame ETreeViewFrame;
typedef struct _ETreeViewFrameClass ETreeViewFrameClass;
typedef struct _ETreeViewFramePrivate ETreeViewFramePrivate;

/**
 * ETreeViewFrame:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _ETreeViewFrame {
	GtkBox parent;
	ETreeViewFramePrivate *priv;
};

struct _ETreeViewFrameClass {
	GtkBoxClass parent_class;

	/* Signals */
	gboolean	(*toolbar_action_activate)
					(ETreeViewFrame *tree_view_frame,
					 EUIAction *action);
	void		(*update_toolbar_actions)
					(ETreeViewFrame *tree_view_frame);
};

GType		e_tree_view_frame_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_tree_view_frame_new	(void);
GtkTreeView *	e_tree_view_frame_get_tree_view
					(ETreeViewFrame *tree_view_frame);
void		e_tree_view_frame_set_tree_view
					(ETreeViewFrame *tree_view_frame,
					 GtkTreeView *tree_view);
gboolean	e_tree_view_frame_get_toolbar_visible
					(ETreeViewFrame *tree_view_frame);
void		e_tree_view_frame_set_toolbar_visible
					(ETreeViewFrame *tree_view_frame,
					 gboolean toolbar_visible);
GtkPolicyType	e_tree_view_frame_get_hscrollbar_policy
					(ETreeViewFrame *tree_view_frame);
void		e_tree_view_frame_set_hscrollbar_policy
					(ETreeViewFrame *tree_view_frame,
					 GtkPolicyType hscrollbar_policy);
GtkPolicyType	e_tree_view_frame_get_vscrollbar_policy
					(ETreeViewFrame *tree_view_frame);
void		e_tree_view_frame_set_vscrollbar_policy
					(ETreeViewFrame *tree_view_frame,
					 GtkPolicyType vscrollbar_policy);
void		e_tree_view_frame_insert_toolbar_action
					(ETreeViewFrame *tree_view_frame,
					 EUIAction *action,
					 gint position);
EUIAction *	e_tree_view_frame_lookup_toolbar_action
					(ETreeViewFrame *tree_view_frame,
					 const gchar *action_name);
void		e_tree_view_frame_update_toolbar_actions
					(ETreeViewFrame *tree_view_frame);

G_END_DECLS

#endif /* E_TREE_VIEW_FRAME_H */
