/*
 * e-attachment-tree-view.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-attachment-tree-view.h"

#include <glib/gi18n.h>
#include <libebackend/libebackend.h>

#include "e-attachment.h"
#include "e-attachment-store.h"
#include "e-attachment-view.h"

struct _EAttachmentTreeViewPrivate {
	EAttachmentViewPrivate view_priv;
};

enum {
	PROP_0,
	PROP_DRAGGING,
	PROP_EDITABLE,
	PROP_ALLOW_URI
};

/* Forward Declarations */
static void	e_attachment_tree_view_interface_init
					(EAttachmentViewInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EAttachmentTreeView, e_attachment_tree_view, GTK_TYPE_TREE_VIEW,
	G_ADD_PRIVATE (EAttachmentTreeView)
	G_IMPLEMENT_INTERFACE (E_TYPE_ATTACHMENT_VIEW, e_attachment_tree_view_interface_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
attachment_tree_view_render_size (GtkTreeViewColumn *column,
                                  GtkCellRenderer *renderer,
                                  GtkTreeModel *model,
                                  GtkTreeIter *iter)
{
	gchar *display_size = NULL;
	gint column_id;
	guint64 size;

	column_id = E_ATTACHMENT_STORE_COLUMN_SIZE;
	gtk_tree_model_get (model, iter, column_id, &size, -1);

	if (size > 0)
		display_size = g_format_size ((goffset) size);

	g_object_set (renderer, "text", display_size, NULL);

	g_free (display_size);
}

static void
attachment_tree_view_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DRAGGING:
			e_attachment_view_set_dragging (
				E_ATTACHMENT_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_EDITABLE:
			e_attachment_view_set_editable (
				E_ATTACHMENT_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_ALLOW_URI:
			e_attachment_view_set_allow_uri (
				E_ATTACHMENT_VIEW (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_tree_view_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DRAGGING:
			g_value_set_boolean (
				value, e_attachment_view_get_dragging (
				E_ATTACHMENT_VIEW (object)));
			return;

		case PROP_EDITABLE:
			g_value_set_boolean (
				value, e_attachment_view_get_editable (
				E_ATTACHMENT_VIEW (object)));
			return;

		case PROP_ALLOW_URI:
			g_value_set_boolean (
				value, e_attachment_view_get_allow_uri (
				E_ATTACHMENT_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_tree_view_constructed (GObject *object)
{
	EAttachmentTreeView *tree_view = E_ATTACHMENT_TREE_VIEW (object);
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_attachment_tree_view_parent_class)->constructed (object);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	/* Name Column */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_spacing (column, 3);
	gtk_tree_view_column_set_title (column, _("Description"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);

	gtk_tree_view_column_add_attribute (
		column, renderer, "gicon",
		E_ATTACHMENT_STORE_COLUMN_ICON);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	gtk_tree_view_column_add_attribute (
		column, renderer, "text",
		E_ATTACHMENT_STORE_COLUMN_DESCRIPTION);

	renderer = gtk_cell_renderer_progress_new ();
	g_object_set (renderer, "text", _("Loading"), NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	gtk_tree_view_column_add_attribute (
		column, renderer, "value",
		E_ATTACHMENT_STORE_COLUMN_PERCENT);

	gtk_tree_view_column_add_attribute (
		column, renderer, "visible",
		E_ATTACHMENT_STORE_COLUMN_LOADING);

	renderer = gtk_cell_renderer_progress_new ();
	g_object_set (renderer, "text", _("Saving"), NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	gtk_tree_view_column_add_attribute (
		column, renderer, "value",
		E_ATTACHMENT_STORE_COLUMN_PERCENT);

	gtk_tree_view_column_add_attribute (
		column, renderer, "visible",
		E_ATTACHMENT_STORE_COLUMN_SAVING);

	/* Size Column */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Size"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	gtk_tree_view_column_set_cell_data_func (
		column, renderer, (GtkTreeCellDataFunc)
		attachment_tree_view_render_size, NULL, NULL);

	/* Type Column */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Type"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	gtk_tree_view_column_add_attribute (
		column, renderer, "text",
		E_ATTACHMENT_STORE_COLUMN_CONTENT_TYPE);

	e_extensible_load_extensions (E_EXTENSIBLE (tree_view));
}

static void
attachment_tree_view_dispose (GObject *object)
{
	e_attachment_view_dispose (E_ATTACHMENT_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_tree_view_parent_class)->dispose (object);
}

static void
attachment_tree_view_finalize (GObject *object)
{
	e_attachment_view_finalize (E_ATTACHMENT_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_attachment_tree_view_parent_class)->finalize (object);
}

static gboolean
attachment_tree_view_button_press_event (GtkWidget *widget,
                                         GdkEventButton *event)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	if (!e_attachment_view_button_press_event (view, event)) {
		/* Chain up to parent's button_press_event() method. */
		GTK_WIDGET_CLASS (e_attachment_tree_view_parent_class)->
			button_press_event (widget, event);
	}

	/* Never propagate the event to the parent */
	return TRUE;
}

static gboolean
attachment_tree_view_button_release_event (GtkWidget *widget,
                                           GdkEventButton *event)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	if (!e_attachment_view_button_release_event (view, event)) {
		/* Chain up to parent's button_release_event() method. */
		GTK_WIDGET_CLASS (e_attachment_tree_view_parent_class)->
			button_release_event (widget, event);
	}

	/* Never propagate the event to the parent */
	return TRUE;
}

static gboolean
attachment_tree_view_motion_notify_event (GtkWidget *widget,
                                          GdkEventMotion *event)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	if (!e_attachment_view_motion_notify_event (view, event)) {
		/* Chain up to parent's motion_notify_event() method. */
		GTK_WIDGET_CLASS (e_attachment_tree_view_parent_class)->
			motion_notify_event (widget, event);
	}

	/* Never propagate the event to the parent */
	return TRUE;
}

static gboolean
attachment_tree_view_key_press_event (GtkWidget *widget,
                                      GdkEventKey *event)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	if (e_attachment_view_key_press_event (view, event))
		return TRUE;

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_attachment_tree_view_parent_class)->
		key_press_event (widget, event);
}

static void
attachment_tree_view_drag_begin (GtkWidget *widget,
                                 GdkDragContext *context)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	/* Chain up to parent's drag_begin() method. */
	GTK_WIDGET_CLASS (e_attachment_tree_view_parent_class)->
		drag_begin (widget, context);

	e_attachment_view_drag_begin (view, context);
}

static void
attachment_tree_view_drag_end (GtkWidget *widget,
                               GdkDragContext *context)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	/* Chain up to parent's drag_end() method. */
	GTK_WIDGET_CLASS (e_attachment_tree_view_parent_class)->
		drag_end (widget, context);

	e_attachment_view_drag_end (view, context);
}

static void
attachment_tree_view_drag_data_get (GtkWidget *widget,
                                    GdkDragContext *context,
                                    GtkSelectionData *selection,
                                    guint info,
                                    guint time)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	e_attachment_view_drag_data_get (
		view, context, selection, info, time);
}

static gboolean
attachment_tree_view_drag_motion (GtkWidget *widget,
                                  GdkDragContext *context,
                                  gint x,
                                  gint y,
                                  guint time)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	return e_attachment_view_drag_motion (view, context, x, y, time);
}

static gboolean
attachment_tree_view_drag_drop (GtkWidget *widget,
                                GdkDragContext *context,
                                gint x,
                                gint y,
                                guint time)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	if (!e_attachment_view_drag_drop (view, context, x, y, time))
		return FALSE;

	/* Chain up to parent's drag_drop() method. */
	return GTK_WIDGET_CLASS (e_attachment_tree_view_parent_class)->
		drag_drop (widget, context, x, y, time);
}

static void
attachment_tree_view_drag_data_received (GtkWidget *widget,
                                         GdkDragContext *context,
                                         gint x,
                                         gint y,
                                         GtkSelectionData *selection,
                                         guint info,
                                         guint time)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	e_attachment_view_drag_data_received (
		view, context, x, y, selection, info, time);
}

static gboolean
attachment_tree_view_popup_menu (GtkWidget *widget)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);
	GtkWidget *menu;

	e_attachment_view_update_actions (view);
	menu = e_attachment_view_get_popup_menu (view);
	gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);

	return TRUE;
}

static void
attachment_tree_view_row_activated (GtkTreeView *tree_view,
                                    GtkTreePath *path,
                                    GtkTreeViewColumn *column)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (tree_view);

	e_attachment_view_open_path (view, path, NULL);
}

static EAttachmentViewPrivate *
attachment_tree_view_get_private (EAttachmentView *view)
{
	EAttachmentTreeView *self = E_ATTACHMENT_TREE_VIEW (view);

	return &self->priv->view_priv;
}

static EAttachmentStore *
attachment_tree_view_get_store (EAttachmentView *view)
{
	GtkTreeView *tree_view;
	GtkTreeModel *model;

	tree_view = GTK_TREE_VIEW (view);
	model = gtk_tree_view_get_model (tree_view);

	return E_ATTACHMENT_STORE (model);
}

static GtkTreePath *
attachment_tree_view_get_path_at_pos (EAttachmentView *view,
                                      gint x,
                                      gint y)
{
	GtkTreeView *tree_view;
	GtkTreePath *path;
	gboolean row_exists;

	tree_view = GTK_TREE_VIEW (view);

	row_exists = gtk_tree_view_get_path_at_pos (
		tree_view, x, y, &path, NULL, NULL, NULL);

	return row_exists ? path : NULL;
}

static GList *
attachment_tree_view_get_selected_paths (EAttachmentView *view)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;

	tree_view = GTK_TREE_VIEW (view);
	selection = gtk_tree_view_get_selection (tree_view);

	return gtk_tree_selection_get_selected_rows (selection, NULL);
}

static gboolean
attachment_tree_view_path_is_selected (EAttachmentView *view,
                                       GtkTreePath *path)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;

	tree_view = GTK_TREE_VIEW (view);
	selection = gtk_tree_view_get_selection (tree_view);

	return gtk_tree_selection_path_is_selected (selection, path);
}

static void
attachment_tree_view_select_path (EAttachmentView *view,
                                  GtkTreePath *path)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;

	tree_view = GTK_TREE_VIEW (view);
	selection = gtk_tree_view_get_selection (tree_view);

	gtk_tree_selection_select_path (selection, path);
}

static void
attachment_tree_view_unselect_path (EAttachmentView *view,
                                    GtkTreePath *path)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;

	tree_view = GTK_TREE_VIEW (view);
	selection = gtk_tree_view_get_selection (tree_view);

	gtk_tree_selection_unselect_path (selection, path);
}

static void
attachment_tree_view_select_all (EAttachmentView *view)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;

	tree_view = GTK_TREE_VIEW (view);
	selection = gtk_tree_view_get_selection (tree_view);

	gtk_tree_selection_select_all (selection);
}

static void
attachment_tree_view_unselect_all (EAttachmentView *view)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;

	tree_view = GTK_TREE_VIEW (view);
	selection = gtk_tree_view_get_selection (tree_view);

	gtk_tree_selection_unselect_all (selection);
}

static void
attachment_tree_view_drag_source_set (EAttachmentView *view,
                                      GdkModifierType start_button_mask,
                                      const GtkTargetEntry *targets,
                                      gint n_targets,
                                      GdkDragAction actions)
{
	GtkTreeView *tree_view;

	tree_view = GTK_TREE_VIEW (view);

	gtk_tree_view_enable_model_drag_source (
		tree_view, start_button_mask, targets, n_targets, actions);
}

static void
attachment_tree_view_drag_dest_set (EAttachmentView *view,
                                    const GtkTargetEntry *targets,
                                    gint n_targets,
                                    GdkDragAction actions)
{
	GtkTreeView *tree_view;

	tree_view = GTK_TREE_VIEW (view);

	gtk_tree_view_enable_model_drag_dest (
		tree_view, targets, n_targets, actions);
}

static void
attachment_tree_view_drag_source_unset (EAttachmentView *view)
{
	GtkTreeView *tree_view;

	tree_view = GTK_TREE_VIEW (view);

	gtk_tree_view_unset_rows_drag_source (tree_view);
}

static void
attachment_tree_view_drag_dest_unset (EAttachmentView *view)
{
	GtkTreeView *tree_view;

	tree_view = GTK_TREE_VIEW (view);

	gtk_tree_view_unset_rows_drag_dest (tree_view);
}

static void
e_attachment_tree_view_class_init (EAttachmentTreeViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkTreeViewClass *tree_view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_tree_view_set_property;
	object_class->get_property = attachment_tree_view_get_property;
	object_class->constructed = attachment_tree_view_constructed;
	object_class->dispose = attachment_tree_view_dispose;
	object_class->finalize = attachment_tree_view_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = attachment_tree_view_button_press_event;
	widget_class->button_release_event = attachment_tree_view_button_release_event;
	widget_class->motion_notify_event = attachment_tree_view_motion_notify_event;
	widget_class->key_press_event = attachment_tree_view_key_press_event;
	widget_class->drag_begin = attachment_tree_view_drag_begin;
	widget_class->drag_end = attachment_tree_view_drag_end;
	widget_class->drag_data_get = attachment_tree_view_drag_data_get;
	widget_class->drag_motion = attachment_tree_view_drag_motion;
	widget_class->drag_drop = attachment_tree_view_drag_drop;
	widget_class->drag_data_received = attachment_tree_view_drag_data_received;
	widget_class->popup_menu = attachment_tree_view_popup_menu;

	tree_view_class = GTK_TREE_VIEW_CLASS (class);
	tree_view_class->row_activated = attachment_tree_view_row_activated;

	g_object_class_override_property (
		object_class, PROP_DRAGGING, "dragging");

	g_object_class_override_property (
		object_class, PROP_EDITABLE, "editable");

	g_object_class_override_property (
		object_class, PROP_ALLOW_URI, "allow-uri");
}

static void
e_attachment_tree_view_init (EAttachmentTreeView *tree_view)
{
	tree_view->priv = e_attachment_tree_view_get_instance_private (tree_view);

	e_attachment_view_init (E_ATTACHMENT_VIEW (tree_view));
}

static void
e_attachment_tree_view_interface_init (EAttachmentViewInterface *iface)
{
	iface->get_private = attachment_tree_view_get_private;
	iface->get_store = attachment_tree_view_get_store;

	iface->get_path_at_pos = attachment_tree_view_get_path_at_pos;
	iface->get_selected_paths = attachment_tree_view_get_selected_paths;
	iface->path_is_selected = attachment_tree_view_path_is_selected;
	iface->select_path = attachment_tree_view_select_path;
	iface->unselect_path = attachment_tree_view_unselect_path;
	iface->select_all = attachment_tree_view_select_all;
	iface->unselect_all = attachment_tree_view_unselect_all;

	iface->drag_source_set = attachment_tree_view_drag_source_set;
	iface->drag_dest_set = attachment_tree_view_drag_dest_set;
	iface->drag_source_unset = attachment_tree_view_drag_source_unset;
	iface->drag_dest_unset = attachment_tree_view_drag_dest_unset;
}

GtkWidget *
e_attachment_tree_view_new (void)
{
	return g_object_new (E_TYPE_ATTACHMENT_TREE_VIEW, NULL);
}
