/*
 * e-attachment-icon-view.c
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

#include "e-attachment-icon-view.h"

#include <glib/gi18n.h>
#include <libebackend/libebackend.h>

#include "e-attachment.h"
#include "e-attachment-store.h"
#include "e-attachment-view.h"

struct _EAttachmentIconViewPrivate {
	EAttachmentViewPrivate view_priv;
};

enum {
	PROP_0,
	PROP_DRAGGING,
	PROP_EDITABLE,
	PROP_ALLOW_URI
};

/* Forward Declarations */
static void	e_attachment_icon_view_interface_init
					(EAttachmentViewInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EAttachmentIconView, e_attachment_icon_view, GTK_TYPE_ICON_VIEW,
	G_ADD_PRIVATE (EAttachmentIconView)
	G_IMPLEMENT_INTERFACE (E_TYPE_ATTACHMENT_VIEW, e_attachment_icon_view_interface_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
attachment_icon_view_set_property (GObject *object,
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
attachment_icon_view_get_property (GObject *object,
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
attachment_icon_view_dispose (GObject *object)
{
	e_attachment_view_dispose (E_ATTACHMENT_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_icon_view_parent_class)->dispose (object);
}

static void
attachment_icon_view_finalize (GObject *object)
{
	e_attachment_view_finalize (E_ATTACHMENT_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_attachment_icon_view_parent_class)->finalize (object);
}

static void
attachment_icon_view_constructed (GObject *object)
{
	GtkCellLayout *cell_layout;
	GtkCellRenderer *renderer;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_attachment_icon_view_parent_class)->constructed (object);

	gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (object), GTK_SELECTION_MULTIPLE);
	gtk_icon_view_set_item_width (GTK_ICON_VIEW (object), 96);

	cell_layout = GTK_CELL_LAYOUT (object);

	/* This needs to happen after constructor properties are set
	 * so that GtkCellLayout.get_area() returns something valid. */

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer,
		"stock-size", GTK_ICON_SIZE_DIALOG,
		"xalign", 0.5,
		"yalign", 0.5,
		NULL);
	gtk_cell_layout_pack_start (cell_layout, renderer, FALSE);

	gtk_cell_layout_add_attribute (
		cell_layout, renderer, "gicon",
		E_ATTACHMENT_STORE_COLUMN_ICON);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		"alignment", PANGO_ALIGN_LEFT,
		"ellipsize", PANGO_ELLIPSIZE_END,
		"wrap-mode", PANGO_WRAP_WORD,
		"wrap-width", 96,
		"scale", 0.8,
		"xpad", 0,
		"xalign", 0.5,
		"yalign", 0.0,
		NULL);
	gtk_cell_layout_pack_start (cell_layout, renderer, FALSE);

	gtk_cell_layout_add_attribute (
		cell_layout, renderer, "text",
		E_ATTACHMENT_STORE_COLUMN_CAPTION);

	renderer = gtk_cell_renderer_progress_new ();
	g_object_set (renderer, "text", _("Loading"), NULL);
	gtk_cell_layout_pack_start (cell_layout, renderer, TRUE);

	gtk_cell_layout_add_attribute (
		cell_layout, renderer, "value",
		E_ATTACHMENT_STORE_COLUMN_PERCENT);

	gtk_cell_layout_add_attribute (
		cell_layout, renderer, "visible",
		E_ATTACHMENT_STORE_COLUMN_LOADING);

	renderer = gtk_cell_renderer_progress_new ();
	g_object_set (renderer, "text", _("Saving"), NULL);
	gtk_cell_layout_pack_start (cell_layout, renderer, TRUE);

	gtk_cell_layout_add_attribute (
		cell_layout, renderer, "value",
		E_ATTACHMENT_STORE_COLUMN_PERCENT);

	gtk_cell_layout_add_attribute (
		cell_layout, renderer, "visible",
		E_ATTACHMENT_STORE_COLUMN_SAVING);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static gboolean
attachment_icon_view_button_press_event (GtkWidget *widget,
                                         GdkEventButton *event)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	if (!e_attachment_view_button_press_event (view, event)) {
		/* Chain up to parent's button_press_event() method. */
		GTK_WIDGET_CLASS (e_attachment_icon_view_parent_class)->
			button_press_event (widget, event);
	}

	/* Never propagate the event to the parent */
	return TRUE;
}

static gboolean
attachment_icon_view_button_release_event (GtkWidget *widget,
                                           GdkEventButton *event)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	if (!e_attachment_view_button_release_event (view, event)) {
		/* Chain up to parent's button_release_event() method. */
		GTK_WIDGET_CLASS (e_attachment_icon_view_parent_class)->
			button_release_event (widget, event);
	}

	/* Never propagate the event to the parent */
	return TRUE;
}

static gboolean
attachment_icon_view_motion_notify_event (GtkWidget *widget,
                                          GdkEventMotion *event)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	if (!e_attachment_view_motion_notify_event (view, event)) {
		/* Chain up to parent's motion_notify_event() method. */
		GTK_WIDGET_CLASS (e_attachment_icon_view_parent_class)->
			motion_notify_event (widget, event);
	}

	/* Never propagate the event to the parent */
	return TRUE;
}

static gboolean
attachment_icon_view_key_press_event (GtkWidget *widget,
                                      GdkEventKey *event)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	if (e_attachment_view_key_press_event (view, event))
		return TRUE;

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_attachment_icon_view_parent_class)->
		key_press_event (widget, event);
}

static void
attachment_icon_view_drag_begin (GtkWidget *widget,
                                 GdkDragContext *context)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	/* Chain up to parent's drag_begin() method. */
	GTK_WIDGET_CLASS (e_attachment_icon_view_parent_class)->
		drag_begin (widget, context);

	e_attachment_view_drag_begin (view, context);
}

static void
attachment_icon_view_drag_end (GtkWidget *widget,
                               GdkDragContext *context)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	/* Chain up to parent's drag_end() method. */
	GTK_WIDGET_CLASS (e_attachment_icon_view_parent_class)->
		drag_end (widget, context);

	e_attachment_view_drag_end (view, context);
}

static void
attachment_icon_view_drag_data_get (GtkWidget *widget,
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
attachment_icon_view_drag_motion (GtkWidget *widget,
                                  GdkDragContext *context,
                                  gint x,
                                  gint y,
                                  guint time)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	return e_attachment_view_drag_motion (view, context, x, y, time);
}

static gboolean
attachment_icon_view_drag_drop (GtkWidget *widget,
                                GdkDragContext *context,
                                gint x,
                                gint y,
                                guint time)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);

	if (!e_attachment_view_drag_drop (view, context, x, y, time))
		return FALSE;

	/* Chain up to parent's drag_drop() method. */
	return GTK_WIDGET_CLASS (e_attachment_icon_view_parent_class)->
		drag_drop (widget, context, x, y, time);
}

static void
attachment_icon_view_drag_data_received (GtkWidget *widget,
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
attachment_icon_view_popup_menu (GtkWidget *widget)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (widget);
	GtkWidget *menu;

	e_attachment_view_update_actions (view);
	menu = e_attachment_view_get_popup_menu (view);
	gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);

	return TRUE;
}

static void
attachment_icon_view_item_activated (GtkIconView *icon_view,
                                     GtkTreePath *path)
{
	EAttachmentView *view = E_ATTACHMENT_VIEW (icon_view);

	e_attachment_view_open_path (view, path, NULL);
}

static EAttachmentViewPrivate *
attachment_icon_view_get_private (EAttachmentView *view)
{
	EAttachmentIconView *self = E_ATTACHMENT_ICON_VIEW (view);

	return &self->priv->view_priv;
}

static EAttachmentStore *
attachment_icon_view_get_store (EAttachmentView *view)
{
	GtkIconView *icon_view;
	GtkTreeModel *model;

	icon_view = GTK_ICON_VIEW (view);
	model = gtk_icon_view_get_model (icon_view);

	return E_ATTACHMENT_STORE (model);
}

static GtkTreePath *
attachment_icon_view_get_path_at_pos (EAttachmentView *view,
                                      gint x,
                                      gint y)
{
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (view);

	return gtk_icon_view_get_path_at_pos (icon_view, x, y);
}

static GList *
attachment_icon_view_get_selected_paths (EAttachmentView *view)
{
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (view);

	return gtk_icon_view_get_selected_items (icon_view);
}

static gboolean
attachment_icon_view_path_is_selected (EAttachmentView *view,
                                       GtkTreePath *path)
{
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (view);

	return gtk_icon_view_path_is_selected (icon_view, path);
}

static void
attachment_icon_view_select_path (EAttachmentView *view,
                                  GtkTreePath *path)
{
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (view);

	gtk_icon_view_select_path (icon_view, path);
}

static void
attachment_icon_view_unselect_path (EAttachmentView *view,
                                    GtkTreePath *path)
{
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (view);

	gtk_icon_view_unselect_path (icon_view, path);
}

static void
attachment_icon_view_select_all (EAttachmentView *view)
{
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (view);

	gtk_icon_view_select_all (icon_view);
}

static void
attachment_icon_view_unselect_all (EAttachmentView *view)
{
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (view);

	gtk_icon_view_unselect_all (icon_view);
}

static void
attachment_icon_view_drag_source_set (EAttachmentView *view,
                                      GdkModifierType start_button_mask,
                                      const GtkTargetEntry *targets,
                                      gint n_targets,
                                      GdkDragAction actions)
{
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (view);

	gtk_icon_view_enable_model_drag_source (
		icon_view, start_button_mask, targets, n_targets, actions);
}

static void
attachment_icon_view_drag_dest_set (EAttachmentView *view,
                                    const GtkTargetEntry *targets,
                                    gint n_targets,
                                    GdkDragAction actions)
{
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (view);

	gtk_icon_view_enable_model_drag_dest (
		icon_view, targets, n_targets, actions);
}

static void
attachment_icon_view_drag_source_unset (EAttachmentView *view)
{
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (view);

	gtk_icon_view_unset_model_drag_source (icon_view);
}

static void
attachment_icon_view_drag_dest_unset (EAttachmentView *view)
{
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (view);

	gtk_icon_view_unset_model_drag_dest (icon_view);
}

static void
e_attachment_icon_view_class_init (EAttachmentIconViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkIconViewClass *icon_view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_icon_view_set_property;
	object_class->get_property = attachment_icon_view_get_property;
	object_class->dispose = attachment_icon_view_dispose;
	object_class->finalize = attachment_icon_view_finalize;
	object_class->constructed = attachment_icon_view_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = attachment_icon_view_button_press_event;
	widget_class->button_release_event = attachment_icon_view_button_release_event;
	widget_class->motion_notify_event = attachment_icon_view_motion_notify_event;
	widget_class->key_press_event = attachment_icon_view_key_press_event;
	widget_class->drag_begin = attachment_icon_view_drag_begin;
	widget_class->drag_end = attachment_icon_view_drag_end;
	widget_class->drag_data_get = attachment_icon_view_drag_data_get;
	widget_class->drag_motion = attachment_icon_view_drag_motion;
	widget_class->drag_drop = attachment_icon_view_drag_drop;
	widget_class->drag_data_received = attachment_icon_view_drag_data_received;
	widget_class->popup_menu = attachment_icon_view_popup_menu;

	icon_view_class = GTK_ICON_VIEW_CLASS (class);
	icon_view_class->item_activated = attachment_icon_view_item_activated;

	g_object_class_override_property (
		object_class, PROP_DRAGGING, "dragging");

	g_object_class_override_property (
		object_class, PROP_EDITABLE, "editable");

	g_object_class_override_property (
		object_class, PROP_ALLOW_URI, "allow-uri");
}

static void
e_attachment_icon_view_init (EAttachmentIconView *icon_view)
{
	icon_view->priv = e_attachment_icon_view_get_instance_private (icon_view);

	e_attachment_view_init (E_ATTACHMENT_VIEW (icon_view));
}

static void
e_attachment_icon_view_interface_init (EAttachmentViewInterface *iface)
{
	iface->get_private = attachment_icon_view_get_private;
	iface->get_store = attachment_icon_view_get_store;

	iface->get_path_at_pos = attachment_icon_view_get_path_at_pos;
	iface->get_selected_paths = attachment_icon_view_get_selected_paths;
	iface->path_is_selected = attachment_icon_view_path_is_selected;
	iface->select_path = attachment_icon_view_select_path;
	iface->unselect_path = attachment_icon_view_unselect_path;
	iface->select_all = attachment_icon_view_select_all;
	iface->unselect_all = attachment_icon_view_unselect_all;

	iface->drag_source_set = attachment_icon_view_drag_source_set;
	iface->drag_dest_set = attachment_icon_view_drag_dest_set;
	iface->drag_source_unset = attachment_icon_view_drag_source_unset;
	iface->drag_dest_unset = attachment_icon_view_drag_dest_unset;
}

GtkWidget *
e_attachment_icon_view_new (void)
{
	return g_object_new (E_TYPE_ATTACHMENT_ICON_VIEW, NULL);
}
