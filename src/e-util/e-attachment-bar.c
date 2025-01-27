/*
 * e-attachment-bar.c
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

#include "e-attachment-bar.h"

#include <glib/gi18n.h>

#include "e-attachment-store.h"
#include "e-attachment-icon-view.h"
#include "e-attachment-tree-view.h"
#include "e-misc-utils.h"

#define NUM_VIEWS 2

struct _EAttachmentBarPrivate {
	GPtrArray *possible_attachments; /* EAttachment *; these are not part of the attachment store, but can be added */
	GtkTreeModel *model;
	GtkWidget *content_area;
	GtkWidget *attachments_area;
	GtkWidget *info_vbox;
	GtkWidget *expander;
	GtkWidget *combo_box;
	GtkWidget *icon_view;
	GtkWidget *tree_view;
	GtkWidget *icon_frame;
	GtkWidget *tree_frame;
	GtkWidget *status_icon;
	GtkWidget *status_label;
	GtkWidget *save_all_button;
	GtkWidget *save_one_button;
	GtkWidget *icon_scrolled_window; /* not referenced */
	GtkWidget *tree_scrolled_window; /* not referenced */
	GtkWidget *menu_button;
	EUIAction *show_possible_action;
	EUIAction *hide_possible_action;

	gint active_view;
	guint expanded : 1;
};

enum {
	PROP_0,
	PROP_ACTIVE_VIEW,
	PROP_ATTACHMENTS_VISIBLE,
	PROP_DRAGGING,
	PROP_EDITABLE,
	PROP_ALLOW_URI,
	PROP_EXPANDED,
	PROP_STORE
};

/* Forward Declarations */
static void	e_attachment_bar_interface_init
				(EAttachmentViewInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EAttachmentBar, e_attachment_bar, GTK_TYPE_PANED,
	G_ADD_PRIVATE (EAttachmentBar)
	G_IMPLEMENT_INTERFACE (E_TYPE_ATTACHMENT_VIEW, e_attachment_bar_interface_init))

static void
attachment_bar_show_hide_possible (EAttachmentBar *self,
				   gboolean show)
{
	EAttachmentStore *store;
	guint ii;

	if (!self->priv->possible_attachments || !self->priv->possible_attachments->len) {
		e_ui_action_set_visible (self->priv->show_possible_action, FALSE);
		e_ui_action_set_visible (self->priv->hide_possible_action, FALSE);
		return;
	}

	e_ui_action_set_visible (self->priv->show_possible_action, !show);
	e_ui_action_set_visible (self->priv->hide_possible_action, show);

	store = e_attachment_bar_get_store (self);

	for (ii = 0; ii < self->priv->possible_attachments->len; ii++) {
		EAttachment *attach = g_ptr_array_index (self->priv->possible_attachments, ii);

		if (show)
			e_attachment_store_add_attachment (store, attach);
		else
			e_attachment_store_remove_attachment (store, attach);
	}
}

static void
action_show_possible_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EAttachmentBar *self = user_data;

	attachment_bar_show_hide_possible (self, TRUE);
}

static void
action_hide_possible_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EAttachmentBar *self = user_data;

	attachment_bar_show_hide_possible (self, FALSE);
}

static void
attachment_bar_update_status (EAttachmentBar *bar)
{
	EAttachmentStore *store;
	EUIAction *action;
	GtkLabel *label;
	gint num_attachments;
	guint64 total_size;
	gchar *display_size;
	gchar *markup;

	/* dispose was called */
	if (!bar->priv->model)
		return;

	store = E_ATTACHMENT_STORE (bar->priv->model);
	label = GTK_LABEL (bar->priv->status_label);

	num_attachments = e_attachment_store_get_num_attachments (store);
	total_size = e_attachment_store_get_total_size (store);
	display_size = g_format_size (total_size);

	if (total_size > 0)
		markup = g_strdup_printf (
			"<b>%d</b> %s (%s)", num_attachments, ngettext (
			"Attachment", "Attachments", num_attachments),
			display_size);
	else
		markup = g_strdup_printf (
			"<b>%d</b> %s", num_attachments, ngettext (
			"Attachment", "Attachments", num_attachments));
	gtk_label_set_markup (label, markup);
	g_free (markup);

	action = e_attachment_view_get_action (E_ATTACHMENT_VIEW (bar), "save-all");
	e_ui_action_set_visible (action, num_attachments > 1);

	action = e_attachment_view_get_action (E_ATTACHMENT_VIEW (bar), "save-one");
	e_ui_action_set_visible (action, (num_attachments == 1));

	g_free (display_size);
}

static void
attachment_bar_notify_vadjustment_upper_cb (GObject *object,
					    GParamSpec *param,
					    gpointer user_data)
{
	EAttachmentBar *bar = user_data;
	GtkAdjustment *adjustment;
	gint max_upper, max_content_height = -2;
	gint request_height = -1;

	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));

	adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (bar->priv->icon_scrolled_window));
	max_upper = gtk_adjustment_get_upper (adjustment);

	adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (bar->priv->tree_scrolled_window));
	max_upper = MAX (max_upper, gtk_adjustment_get_upper (adjustment));

	gtk_widget_style_get (GTK_WIDGET (bar), "max-content-height", &max_content_height, NULL);

	if ((max_content_height >= 0 && max_content_height < 50) || max_content_height <= -2)
		max_content_height = 50;

	if (max_content_height == -1) {
		request_height = max_upper;
	} else if (max_content_height < max_upper) {
		request_height = max_content_height;
	} else {
		request_height = max_upper;
	}

	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (bar->priv->icon_scrolled_window),
		request_height);
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (bar->priv->tree_scrolled_window),
		request_height);
}

static gboolean
attachment_bar_expanded_to_attachments_area_visible_boolean_cb (GBinding *binding,
								const GValue *from_value,
								GValue *to_value,
								gpointer user_data)
{
	EAttachmentBar *bar = user_data;

	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), FALSE);

	if (e_attachment_bar_get_attachments_visible (bar))
		g_value_set_boolean (to_value, e_attachment_bar_get_expanded (bar));
	else
		g_value_set_boolean (to_value, FALSE);

	return TRUE;
}

static void
attachment_bar_set_store (EAttachmentBar *bar,
                          EAttachmentStore *store)
{
	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	bar->priv->model = GTK_TREE_MODEL (g_object_ref (store));

	gtk_icon_view_set_model (
		GTK_ICON_VIEW (bar->priv->icon_view),
		bar->priv->model);
	gtk_tree_view_set_model (
		GTK_TREE_VIEW (bar->priv->tree_view),
		bar->priv->model);

	e_signal_connect_notify_object (
		bar->priv->model, "notify::num-attachments",
		G_CALLBACK (attachment_bar_update_status), bar,
		G_CONNECT_SWAPPED);

	e_signal_connect_notify_object (
		bar->priv->model, "notify::total-size",
		G_CALLBACK (attachment_bar_update_status), bar,
		G_CONNECT_SWAPPED);

	/* Initialize */
	attachment_bar_update_status (bar);
}

static void
attachment_bar_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_VIEW:
				e_attachment_bar_set_active_view (
				E_ATTACHMENT_BAR (object),
				g_value_get_int (value));
			return;

		case PROP_ATTACHMENTS_VISIBLE:
			e_attachment_bar_set_attachments_visible (
				E_ATTACHMENT_BAR (object),
				g_value_get_boolean (value));
			return;

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

		case PROP_EXPANDED:
			e_attachment_bar_set_expanded (
				E_ATTACHMENT_BAR (object),
				g_value_get_boolean (value));
			return;

		case PROP_STORE:
			attachment_bar_set_store (
				E_ATTACHMENT_BAR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_bar_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_VIEW:
			g_value_set_int (
				value,
				e_attachment_bar_get_active_view (
				E_ATTACHMENT_BAR (object)));
			return;

		case PROP_ATTACHMENTS_VISIBLE:
			g_value_set_boolean (
				value,
				e_attachment_bar_get_attachments_visible (
				E_ATTACHMENT_BAR (object)));
			return;

		case PROP_DRAGGING:
			g_value_set_boolean (
				value,
				e_attachment_view_get_dragging (
				E_ATTACHMENT_VIEW (object)));
			return;

		case PROP_EDITABLE:
			g_value_set_boolean (
				value,
				e_attachment_view_get_editable (
				E_ATTACHMENT_VIEW (object)));
			return;

		case PROP_ALLOW_URI:
			g_value_set_boolean (
				value,
				e_attachment_view_get_allow_uri (
				E_ATTACHMENT_VIEW (object)));
			return;

		case PROP_EXPANDED:
			g_value_set_boolean (
				value,
				e_attachment_bar_get_expanded (
				E_ATTACHMENT_BAR (object)));
			return;

		case PROP_STORE:
			g_value_set_object (
				value,
				e_attachment_bar_get_store (
				E_ATTACHMENT_BAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_bar_dispose (GObject *object)
{
	EAttachmentBar *self = E_ATTACHMENT_BAR (object);

	g_clear_pointer (&self->priv->possible_attachments, g_ptr_array_unref);
	g_clear_object (&self->priv->model);
	g_clear_object (&self->priv->content_area);
	g_clear_object (&self->priv->attachments_area);
	g_clear_object (&self->priv->info_vbox);
	g_clear_object (&self->priv->expander);
	g_clear_object (&self->priv->combo_box);
	g_clear_object (&self->priv->icon_view);
	g_clear_object (&self->priv->tree_view);
	g_clear_object (&self->priv->icon_frame);
	g_clear_object (&self->priv->tree_frame);
	g_clear_object (&self->priv->status_icon);
	g_clear_object (&self->priv->status_label);
	g_clear_object (&self->priv->save_all_button);
	g_clear_object (&self->priv->save_one_button);
	g_clear_object (&self->priv->menu_button);
	g_clear_object (&self->priv->show_possible_action);
	g_clear_object (&self->priv->hide_possible_action);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_bar_parent_class)->dispose (object);
}

static void
attachment_bar_constructed (GObject *object)
{
	EAttachmentBar *self;
	GSettings *settings;

	self = E_ATTACHMENT_BAR (object);

	/* Set up property-to-property bindings. */

	e_binding_bind_property (
		object, "active-view",
		self->priv->combo_box, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "dragging",
		self->priv->icon_view, "dragging",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "dragging",
		self->priv->tree_view, "dragging",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "editable",
		self->priv->icon_view, "editable",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "editable",
		self->priv->tree_view, "editable",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "expanded",
		self->priv->expander, "expanded",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "expanded",
		self->priv->combo_box, "visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property_full (
		object, "expanded",
		self->priv->attachments_area, "visible",
		G_BINDING_SYNC_CREATE,
		attachment_bar_expanded_to_attachments_area_visible_boolean_cb,
		NULL, object, NULL);

	e_binding_bind_property_full (
		object, "attachments-visible",
		self->priv->attachments_area, "visible",
		G_BINDING_SYNC_CREATE,
		attachment_bar_expanded_to_attachments_area_visible_boolean_cb,
		NULL, object, NULL);

	/* Set up property-to-GSettings bindings. */
	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	g_settings_bind (
		settings, "attachment-view",
		object, "active-view",
		G_SETTINGS_BIND_DEFAULT);
	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_attachment_bar_parent_class)->constructed (object);
}

static EAttachmentViewPrivate *
attachment_bar_get_private (EAttachmentView *view)
{
	EAttachmentBar *bar;

	bar = E_ATTACHMENT_BAR (view);
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);

	return e_attachment_view_get_private (view);
}

static GtkTreePath *
attachment_bar_get_path_at_pos (EAttachmentView *view,
                                gint x,
                                gint y)
{
	EAttachmentBar *bar;

	bar = E_ATTACHMENT_BAR (view);
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);

	return e_attachment_view_get_path_at_pos (view, x, y);
}

static EAttachmentStore *
attachment_bar_get_store (EAttachmentView *view)
{
	return e_attachment_bar_get_store (E_ATTACHMENT_BAR (view));
}

static GList *
attachment_bar_get_selected_paths (EAttachmentView *view)
{
	EAttachmentBar *bar;

	bar = E_ATTACHMENT_BAR (view);
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);

	return e_attachment_view_get_selected_paths (view);
}

static gboolean
attachment_bar_path_is_selected (EAttachmentView *view,
                                 GtkTreePath *path)
{
	EAttachmentBar *bar;

	bar = E_ATTACHMENT_BAR (view);
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);

	return e_attachment_view_path_is_selected (view, path);
}

static void
attachment_bar_select_path (EAttachmentView *view,
                            GtkTreePath *path)
{
	EAttachmentBar *bar;

	bar = E_ATTACHMENT_BAR (view);
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);

	e_attachment_view_select_path (view, path);
}

static void
attachment_bar_unselect_path (EAttachmentView *view,
                              GtkTreePath *path)
{
	EAttachmentBar *bar;

	bar = E_ATTACHMENT_BAR (view);
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);

	e_attachment_view_unselect_path (view, path);
}

static void
attachment_bar_select_all (EAttachmentView *view)
{
	EAttachmentBar *bar;

	bar = E_ATTACHMENT_BAR (view);
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);

	e_attachment_view_select_all (view);
}

static void
attachment_bar_unselect_all (EAttachmentView *view)
{
	EAttachmentBar *bar;

	bar = E_ATTACHMENT_BAR (view);
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);

	e_attachment_view_unselect_all (view);
}

static void
attachment_bar_update_actions (EAttachmentView *view)
{
	EAttachmentBar *bar;

	bar = E_ATTACHMENT_BAR (view);
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);

	e_attachment_view_update_actions (view);
}

static gboolean
attachment_bar_button_press_event (GtkWidget *widget,
				   GdkEventButton *event)
{
	/* Chain up to parent's button_press_event() method. */
	GTK_WIDGET_CLASS (e_attachment_bar_parent_class)->button_press_event (widget, event);

	/* Never propagate the event to the parent */
	return TRUE;
}

static gboolean
attachment_bar_button_release_event (GtkWidget *widget,
				     GdkEventButton *event)
{
	/* Chain up to parent's button_release_event() method. */
	GTK_WIDGET_CLASS (e_attachment_bar_parent_class)->button_release_event (widget, event);

	/* Never propagate the event to the parent */
	return TRUE;
}

static gboolean
attachment_bar_motion_notify_event (GtkWidget *widget,
				    GdkEventMotion *event)
{
	/* Chain up to parent's motion_notify_event() method. */
	GTK_WIDGET_CLASS (e_attachment_bar_parent_class)->motion_notify_event (widget, event);

	/* Never propagate the event to the parent */
	return TRUE;
}

static void
e_attachment_bar_class_init (EAttachmentBarClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_bar_set_property;
	object_class->get_property = attachment_bar_get_property;
	object_class->dispose = attachment_bar_dispose;
	object_class->constructed = attachment_bar_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = attachment_bar_button_press_event;
	widget_class->button_release_event = attachment_bar_button_release_event;
	widget_class->motion_notify_event = attachment_bar_motion_notify_event;

	/* Do not set the CSS class name, it breaks styling of the resize handle */
	/* gtk_widget_class_set_css_name (widget_class, G_OBJECT_CLASS_NAME (class)); */

	g_object_class_install_property (
		object_class,
		PROP_ACTIVE_VIEW,
		g_param_spec_int (
			"active-view",
			"Active View",
			NULL,
			0,
			NUM_VIEWS,
			0,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_ATTACHMENTS_VISIBLE,
		g_param_spec_boolean (
			"attachments-visible",
			"Attachments Visible",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_EXPANDED,
		g_param_spec_boolean (
			"expanded",
			"Expanded",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_STORE,
		g_param_spec_object (
			"store",
			"Attachment Store",
			NULL,
			E_TYPE_ATTACHMENT_STORE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_override_property (
		object_class, PROP_DRAGGING, "dragging");

	g_object_class_override_property (
		object_class, PROP_EDITABLE, "editable");

	g_object_class_override_property (
		object_class, PROP_ALLOW_URI, "allow-uri");

	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_int (
			"max-content-height",
			"Max Content Height",
			NULL,
			-1, G_MAXINT, 150,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
e_attachment_bar_interface_init (EAttachmentViewInterface *iface)
{
	iface->get_private = attachment_bar_get_private;
	iface->get_store = attachment_bar_get_store;
	iface->get_path_at_pos = attachment_bar_get_path_at_pos;
	iface->get_selected_paths = attachment_bar_get_selected_paths;
	iface->path_is_selected = attachment_bar_path_is_selected;
	iface->select_path = attachment_bar_select_path;
	iface->unselect_path = attachment_bar_unselect_path;
	iface->select_all = attachment_bar_select_all;
	iface->unselect_all = attachment_bar_unselect_all;
	iface->update_actions = attachment_bar_update_actions;
}

static void
e_attachment_bar_init (EAttachmentBar *bar)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='attach-bar-menu' is-popup='true'>"
		    "<item action='attach-bar-show-possible'/>"
		    "<item action='attach-bar-hide-possible'/>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {
		{ "attach-bar-show-possible",
		  NULL,
		  N_("_Show Inline Parts in Attachments"),
		  NULL,
		  NULL,
		  action_show_possible_cb, NULL, NULL, NULL },

		{ "attach-bar-hide-possible",
		  NULL,
		  N_("_Hide Inline Parts in Attachments"),
		  NULL,
		  NULL,
		  action_hide_possible_cb, NULL, NULL, NULL }
	};

	EAttachmentView *view;
	GtkSizeGroup *size_group;
	GtkWidget *container;
	GtkWidget *widget;
	EUIAction *action;
	EUIManager *ui_manager;
	GObject *ui_object;
	GtkAdjustment *adjustment;

	gtk_widget_set_name (GTK_WIDGET (bar), "e-attachment-bar");

	bar->priv = e_attachment_bar_get_instance_private (bar);
	bar->priv->possible_attachments = g_ptr_array_new_with_free_func (g_object_unref);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (bar), GTK_ORIENTATION_VERTICAL);

	/* Keep the expander label and save button the same height. */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	/* Construct the Attachment Views */

	container = GTK_WIDGET (bar);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, FALSE);
	bar->priv->attachments_area = g_object_ref (widget);
	gtk_widget_show (widget);

	container = bar->priv->attachments_area;

	widget = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	bar->priv->icon_frame = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (container), widget);
	bar->priv->icon_scrolled_window = widget;
	gtk_widget_show (widget);

	widget = e_attachment_icon_view_new ();
	gtk_widget_set_can_focus (widget, TRUE);
	gtk_icon_view_set_model (GTK_ICON_VIEW (widget), bar->priv->model);
	gtk_container_add (GTK_CONTAINER (bar->priv->icon_scrolled_window), widget);
	bar->priv->icon_view = g_object_ref (widget);
	gtk_widget_show (widget);

	container = bar->priv->attachments_area;

	widget = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	bar->priv->tree_frame = g_object_ref (widget);
	gtk_widget_hide (widget);

	container = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (container), widget);
	bar->priv->tree_scrolled_window = widget;
	gtk_widget_show (widget);

	widget = e_attachment_tree_view_new ();
	gtk_widget_set_can_focus (widget, TRUE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), bar->priv->model);
	gtk_container_add (GTK_CONTAINER (bar->priv->tree_scrolled_window), widget);
	bar->priv->tree_view = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Construct the Controls */

	container = GTK_WIDGET (bar);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	bar->priv->content_area = g_object_ref (widget);
	gtk_widget_show (widget);

	container = bar->priv->content_area;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->info_vbox = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_expander_new (NULL);
	gtk_expander_set_spacing (GTK_EXPANDER (widget), 0);
	gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->expander = g_object_ref (widget);
	gtk_widget_show (widget);

	/* The "Save All" button proxies the "save-all" action from
	 * one of the two attachment views.  Doesn't matter which. */
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);
	action = e_attachment_view_get_action (view, "save-all");

	widget = gtk_button_new_with_mnemonic (e_ui_action_get_label (action));
	if (e_ui_action_get_icon_name (action))
		gtk_button_set_image (GTK_BUTTON (widget), gtk_image_new_from_icon_name (e_ui_action_get_icon_name (action), GTK_ICON_SIZE_BUTTON));
	e_ui_action_util_assign_to_widget (action, widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->save_all_button = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Same deal with the "Save" button. */
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);
	action = e_attachment_view_get_action (view, "save-one");

	widget = gtk_button_new_with_mnemonic (e_ui_action_get_label (action));
	if (e_ui_action_get_icon_name (action))
		gtk_button_set_image (GTK_BUTTON (widget), gtk_image_new_from_icon_name (e_ui_action_get_icon_name (action), GTK_ICON_SIZE_BUTTON));
	e_ui_action_util_assign_to_widget (action, widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->save_one_button = g_object_ref (widget);
	gtk_widget_show (widget);

	/* needed to be able to click the buttons */
	ui_manager = e_attachment_view_get_ui_manager (view);
	e_ui_manager_add_actions_with_eui_data (ui_manager, "attach-bar", NULL,
		entries, G_N_ELEMENTS (entries), bar, eui);
	e_ui_manager_add_action_groups_to_widget (ui_manager, container);

	bar->priv->show_possible_action = g_object_ref (e_ui_manager_get_action (ui_manager, "attach-bar-show-possible"));
	bar->priv->hide_possible_action = g_object_ref (e_ui_manager_get_action (ui_manager, "attach-bar-hide-possible"));

	widget = gtk_menu_button_new ();
	gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (widget), FALSE);
	gtk_menu_button_set_direction (GTK_MENU_BUTTON (widget), GTK_ARROW_NONE);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->menu_button = g_object_ref (widget);
	/* no possible attachments after create */
	gtk_widget_set_visible (widget, FALSE);
	e_ui_action_set_visible (bar->priv->show_possible_action, FALSE);
	e_ui_action_set_visible (bar->priv->hide_possible_action, FALSE);

	ui_object = e_ui_manager_create_item (ui_manager, "attach-bar-menu");
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (bar->priv->menu_button), G_MENU_MODEL (ui_object));
	g_clear_object (&ui_object);

	widget = gtk_combo_box_text_new ();
	gtk_size_group_add_widget (size_group, widget);
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("Icon View"));
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("List View"));
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->combo_box = g_object_ref (widget);
	gtk_widget_show (widget);

	container = bar->priv->expander;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_size_group_add_widget (size_group, widget);
	gtk_expander_set_label_widget (GTK_EXPANDER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_icon_name (
		"mail-attachment", GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->status_icon = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->status_label = g_object_ref (widget);
	gtk_widget_show (widget);

	g_object_unref (size_group);

	adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (bar->priv->icon_scrolled_window));
	e_signal_connect_notify (adjustment, "notify::upper",
		G_CALLBACK (attachment_bar_notify_vadjustment_upper_cb), bar);

	adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (bar->priv->tree_scrolled_window));
	e_signal_connect_notify (adjustment, "notify::upper",
		G_CALLBACK (attachment_bar_notify_vadjustment_upper_cb), bar);
}

GtkWidget *
e_attachment_bar_new (EAttachmentStore *store)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), NULL);

	return g_object_new (
		E_TYPE_ATTACHMENT_BAR,
		"orientation", GTK_ORIENTATION_VERTICAL,
		"editable", FALSE,
		"store", store, NULL);
}

gint
e_attachment_bar_get_active_view (EAttachmentBar *bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), 0);

	return bar->priv->active_view;
}

void
e_attachment_bar_set_active_view (EAttachmentBar *bar,
                                  gint active_view)
{
	EAttachmentView *source;
	EAttachmentView *target;

	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	g_return_if_fail (active_view >= 0 && active_view < NUM_VIEWS);

	if (active_view == bar->priv->active_view)
		return;

	bar->priv->active_view = active_view;

	if (active_view == 0) {
		gtk_widget_show (bar->priv->icon_frame);
		gtk_widget_hide (bar->priv->tree_frame);
	} else {
		gtk_widget_hide (bar->priv->icon_frame);
		gtk_widget_show (bar->priv->tree_frame);
	}

	/* Synchronize the item selection of the view we're
	 * switching TO with the view we're switching FROM. */
	if (active_view == 0) {
		/* from tree view to icon view */
		source = E_ATTACHMENT_VIEW (bar->priv->tree_view);
		target = E_ATTACHMENT_VIEW (bar->priv->icon_view);
	} else {
		/* from icon view to tree view */
		source = E_ATTACHMENT_VIEW (bar->priv->icon_view);
		target = E_ATTACHMENT_VIEW (bar->priv->tree_view);
	}

	e_attachment_view_sync_selection (source, target);

	g_object_notify (G_OBJECT (bar), "active-view");
}

gboolean
e_attachment_bar_get_expanded (EAttachmentBar *bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), FALSE);

	return bar->priv->expanded;
}

void
e_attachment_bar_set_expanded (EAttachmentBar *bar,
                                    gboolean expanded)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));

	if (bar->priv->expanded == expanded)
		return;

	bar->priv->expanded = expanded;

	g_object_notify (G_OBJECT (bar), "expanded");
}

EAttachmentStore *
e_attachment_bar_get_store (EAttachmentBar *bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);

	return E_ATTACHMENT_STORE (bar->priv->model);
}

GtkWidget *
e_attachment_bar_get_content_area (EAttachmentBar *bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);

	return bar->priv->content_area;
}

void
e_attachment_bar_set_attachments_visible (EAttachmentBar *bar,
					  gboolean value)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));

	if (!bar->priv->info_vbox || (e_attachment_bar_get_attachments_visible (bar) ? 1 : 0) == (value ? 1 : 0))
		return;

	gtk_widget_set_visible (bar->priv->info_vbox, value);

	g_object_notify (G_OBJECT (bar), "attachments-visible");
}

gboolean
e_attachment_bar_get_attachments_visible (EAttachmentBar *bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), FALSE);

	return bar->priv->info_vbox && gtk_widget_get_visible (bar->priv->info_vbox);
}

void
e_attachment_bar_add_possible_attachment (EAttachmentBar *self,
					  EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (self));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	if (self->priv->possible_attachments && !g_ptr_array_find (self->priv->possible_attachments, attachment, NULL)) {
		g_ptr_array_add (self->priv->possible_attachments, g_object_ref (attachment));
		if (self->priv->possible_attachments->len == 1) {
			EAttachmentStore *store;

			gtk_widget_set_visible (self->priv->menu_button, TRUE);
			e_ui_action_set_visible (self->priv->show_possible_action, TRUE);
			e_ui_action_set_visible (self->priv->hide_possible_action, FALSE);

			/* this can change visibility of the attachment bar, which is tracked through the store */
			store = e_attachment_bar_get_store (self);
			if (store)
				g_object_notify (G_OBJECT (store), "num-attachments");
		}
	}
}

void
e_attachment_bar_clear_possible_attachments (EAttachmentBar *self)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (self));

	if (self->priv->possible_attachments && self->priv->possible_attachments->len > 0) {
		EAttachmentStore *store;

		g_ptr_array_set_size (self->priv->possible_attachments, 0);
		gtk_widget_set_visible (self->priv->menu_button, FALSE);
		e_ui_action_set_visible (self->priv->show_possible_action, FALSE);
		e_ui_action_set_visible (self->priv->hide_possible_action, FALSE);

		/* this can change visibility of the attachment bar, which is tracked through the store */
		store = e_attachment_bar_get_store (self);
		if (store)
			g_object_notify (G_OBJECT (store), "num-attachments");
	}
}

guint
e_attachment_bar_get_n_possible_attachments (EAttachmentBar *self)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (self), 0);

	return self->priv->possible_attachments ? self->priv->possible_attachments->len : 0;
}
