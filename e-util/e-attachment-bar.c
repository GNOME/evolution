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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-attachment-bar.h"

#include <glib/gi18n.h>

#include "e-attachment-store.h"
#include "e-attachment-icon-view.h"
#include "e-attachment-tree-view.h"
#include "e-misc-utils.h"

#define E_ATTACHMENT_BAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_BAR, EAttachmentBarPrivate))

#define NUM_VIEWS 2

struct _EAttachmentBarPrivate {
	GtkTreeModel *model;
	GtkWidget *vbox;
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

	gint active_view;
	guint expanded : 1;
};

enum {
	PROP_0,
	PROP_ACTIVE_VIEW,
	PROP_DRAGGING,
	PROP_EDITABLE,
	PROP_EXPANDED,
	PROP_STORE
};

/* Forward Declarations */
static void	e_attachment_bar_interface_init
				(EAttachmentViewInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EAttachmentBar,
	e_attachment_bar,
	GTK_TYPE_BOX,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_ATTACHMENT_VIEW,
		e_attachment_bar_interface_init))

static void
attachment_bar_update_status (EAttachmentBar *bar)
{
	EAttachmentStore *store;
	GtkActivatable *activatable;
	GtkAction *action;
	GtkLabel *label;
	gint num_attachments;
	guint64 total_size;
	gchar *display_size;
	gchar *markup;

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

	activatable = GTK_ACTIVATABLE (bar->priv->save_all_button);
	action = gtk_activatable_get_related_action (activatable);
	gtk_action_set_visible (action, (num_attachments > 1));

	activatable = GTK_ACTIVATABLE (bar->priv->save_one_button);
	action = gtk_activatable_get_related_action (activatable);
	gtk_action_set_visible (action, (num_attachments == 1));

	g_free (display_size);
}

static void
attachment_bar_set_store (EAttachmentBar *bar,
                          EAttachmentStore *store)
{
	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	bar->priv->model = g_object_ref (store);

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
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_bar_dispose (GObject *object)
{
	EAttachmentBarPrivate *priv;

	priv = E_ATTACHMENT_BAR_GET_PRIVATE (object);

	if (priv->model != NULL) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	if (priv->vbox != NULL) {
		g_object_unref (priv->vbox);
		priv->vbox = NULL;
	}

	if (priv->expander != NULL) {
		g_object_unref (priv->expander);
		priv->expander = NULL;
	}

	if (priv->combo_box != NULL) {
		g_object_unref (priv->combo_box);
		priv->combo_box = NULL;
	}

	if (priv->icon_view != NULL) {
		g_object_unref (priv->icon_view);
		priv->icon_view = NULL;
	}

	if (priv->tree_view != NULL) {
		g_object_unref (priv->tree_view);
		priv->tree_view = NULL;
	}

	if (priv->icon_frame != NULL) {
		g_object_unref (priv->icon_frame);
		priv->icon_frame = NULL;
	}

	if (priv->tree_frame != NULL) {
		g_object_unref (priv->tree_frame);
		priv->tree_frame = NULL;
	}

	if (priv->status_icon != NULL) {
		g_object_unref (priv->status_icon);
		priv->status_icon = NULL;
	}

	if (priv->status_label != NULL) {
		g_object_unref (priv->status_label);
		priv->status_label = NULL;
	}

	if (priv->save_all_button != NULL) {
		g_object_unref (priv->save_all_button);
		priv->save_all_button = NULL;
	}

	if (priv->save_one_button != NULL) {
		g_object_unref (priv->save_one_button);
		priv->save_one_button = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_bar_parent_class)->dispose (object);
}

static void
attachment_bar_constructed (GObject *object)
{
	EAttachmentBarPrivate *priv;
	GSettings *settings;

	priv = E_ATTACHMENT_BAR_GET_PRIVATE (object);

	/* Set up property-to-property bindings. */

	e_binding_bind_property (
		object, "active-view",
		priv->combo_box, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "dragging",
		priv->icon_view, "dragging",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "dragging",
		priv->tree_view, "dragging",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "editable",
		priv->icon_view, "editable",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "editable",
		priv->tree_view, "editable",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "expanded",
		priv->expander, "expanded",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "expanded",
		priv->combo_box, "visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "expanded",
		priv->vbox, "visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

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

	g_type_class_add_private (class, sizeof (EAttachmentBarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_bar_set_property;
	object_class->get_property = attachment_bar_get_property;
	object_class->dispose = attachment_bar_dispose;
	object_class->constructed = attachment_bar_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = attachment_bar_button_press_event;
	widget_class->button_release_event = attachment_bar_button_release_event;
	widget_class->motion_notify_event = attachment_bar_motion_notify_event;

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
	EAttachmentView *view;
	GtkSizeGroup *size_group;
	GtkWidget *container;
	GtkWidget *widget;
	GtkAction *action;

	bar->priv = E_ATTACHMENT_BAR_GET_PRIVATE (bar);

	gtk_box_set_spacing (GTK_BOX (bar), 6);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (bar), GTK_ORIENTATION_VERTICAL);

	/* Keep the expander label and save button the same height. */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	/* Construct the Attachment Views */

	container = GTK_WIDGET (bar);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->vbox = g_object_ref (widget);
	gtk_widget_show (widget);

	container = bar->priv->vbox;

	widget = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	bar->priv->icon_frame = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_attachment_icon_view_new ();
	gtk_widget_set_can_focus (widget, TRUE);
	gtk_icon_view_set_model (GTK_ICON_VIEW (widget), bar->priv->model);
	gtk_container_add (GTK_CONTAINER (container), widget);
	bar->priv->icon_view = g_object_ref (widget);
	gtk_widget_show (widget);

	container = bar->priv->vbox;

	widget = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	bar->priv->tree_frame = g_object_ref (widget);
	gtk_widget_hide (widget);

	container = widget;

	widget = e_attachment_tree_view_new ();
	gtk_widget_set_can_focus (widget, TRUE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), bar->priv->model);
	gtk_container_add (GTK_CONTAINER (container), widget);
	bar->priv->tree_view = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Construct the Controls */

	container = GTK_WIDGET (bar);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_expander_new (NULL);
	gtk_expander_set_spacing (GTK_EXPANDER (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->expander = g_object_ref (widget);
	gtk_widget_show (widget);

	/* The "Save All" button proxies the "save-all" action from
	 * one of the two attachment views.  Doesn't matter which. */
	widget = gtk_button_new ();
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);
	action = e_attachment_view_get_action (view, "save-all");
	gtk_button_set_image (GTK_BUTTON (widget), gtk_image_new ());
	gtk_activatable_set_related_action (GTK_ACTIVATABLE (widget), action);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->save_all_button = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Same deal with the "Save" button. */
	widget = gtk_button_new ();
	view = E_ATTACHMENT_VIEW (bar->priv->icon_view);
	action = e_attachment_view_get_action (view, "save-one");
	gtk_button_set_image (GTK_BUTTON (widget), gtk_image_new ());
	gtk_activatable_set_related_action (GTK_ACTIVATABLE (widget), action);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	bar->priv->save_one_button = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_combo_box_text_new ();
	gtk_size_group_add_widget (size_group, widget);
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("Icon View"));
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("List View"));
	gtk_container_add (GTK_CONTAINER (container), widget);
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
}

GtkWidget *
e_attachment_bar_new (EAttachmentStore *store)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), NULL);

	return g_object_new (
		E_TYPE_ATTACHMENT_BAR,
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
