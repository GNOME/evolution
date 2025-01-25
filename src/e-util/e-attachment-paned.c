/*
 * e-attachment-paned.c
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

#include "e-attachment-paned.h"

#include <glib/gi18n.h>

#include "e-misc-utils.h"
#include "e-attachment-view.h"
#include "e-attachment-store.h"
#include "e-attachment-icon-view.h"
#include "e-attachment-tree-view.h"

#define NUM_VIEWS 2

/* Initial height of the lower pane. */
static gint initial_height = 150;

struct _EAttachmentPanedPrivate {
	GtkTreeModel *model;
	GtkWidget *expander;
	GtkWidget *notebook;
	GtkWidget *combo_box;
	GtkWidget *controls_container;
	GtkWidget *icon_view;
	GtkWidget *tree_view;
	GtkWidget *show_hide_label;
	GtkWidget *status_icon;
	GtkWidget *status_label;
	GtkWidget *content_area;

	gint active_view;
	gint vpaned_handle_size;
	gboolean expanded;
	gboolean resize_toplevel;
};

enum {
	PROP_0,
	PROP_ACTIVE_VIEW,
	PROP_DRAGGING,
	PROP_EDITABLE,
	PROP_ALLOW_URI,
	PROP_EXPANDED,
	PROP_RESIZE_TOPLEVEL
};

/* Forward Declarations */
static void	e_attachment_paned_interface_init
					(EAttachmentViewInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EAttachmentPaned, e_attachment_paned, GTK_TYPE_PANED,
	G_ADD_PRIVATE (EAttachmentPaned)
	G_IMPLEMENT_INTERFACE (E_TYPE_ATTACHMENT_VIEW, e_attachment_paned_interface_init))

void
e_attachment_paned_set_default_height (gint height)
{
	initial_height = height;
}

static void
attachment_paned_notify_cb (EAttachmentPaned *paned,
                            GParamSpec *pspec,
                            GtkExpander *expander)
{
	GtkAllocation toplevel_allocation;
	GtkWidget *toplevel;
	GtkWidget *child;
	GtkLabel *label;
	const gchar *text;

	label = GTK_LABEL (paned->priv->show_hide_label);

	/* Update the expander label. */
	if (gtk_expander_get_expanded (expander))
		text = _("Hide Attachment _Bar");
	else
		text = _("Show Attachment _Bar");

	gtk_label_set_text_with_mnemonic (label, text);

	/* Resize the top-level window if required conditions are met.
	 * This is based on gtk_expander_resize_toplevel(), but adapted
	 * to the fact our GtkExpander has no direct child widget. */

	if (!e_attachment_paned_get_resize_toplevel (paned))
		return;

	if (!gtk_widget_get_realized (GTK_WIDGET (paned)))
		return;

	child = gtk_paned_get_child2 (GTK_PANED (paned));
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (paned));

	if (toplevel == NULL)
		return;

	if (!gtk_widget_get_realized (GTK_WIDGET (toplevel)))
		return;

	gtk_widget_get_allocation (toplevel, &toplevel_allocation);

	if (gtk_expander_get_expanded (expander)) {
		GtkRequisition child_requisition;

		gtk_widget_get_preferred_size (
			child, &child_requisition, NULL);

		toplevel_allocation.height += child_requisition.height;
	} else {
		GtkAllocation child_allocation;

		gtk_widget_get_allocation (child, &child_allocation);

		toplevel_allocation.height -= child_allocation.height;
	}

	gtk_window_resize (
		GTK_WINDOW (toplevel),
		toplevel_allocation.width,
		toplevel_allocation.height);
}

static void
attachment_paned_update_status (EAttachmentPaned *paned)
{
	EAttachmentView *view;
	EAttachmentStore *store;
	GtkExpander *expander;
	GtkLabel *label;
	guint num_attachments;
	guint64 total_size;
	gchar *display_size;
	gchar *markup;

	view = E_ATTACHMENT_VIEW (paned);
	store = e_attachment_view_get_store (view);
	expander = GTK_EXPANDER (paned->priv->expander);
	label = GTK_LABEL (paned->priv->status_label);

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

	g_free (display_size);

	if (num_attachments > 0) {
		gtk_widget_show (paned->priv->status_icon);
		gtk_widget_show (paned->priv->status_label);
		gtk_expander_set_expanded (expander, TRUE);
	} else {
		gtk_widget_hide (paned->priv->status_icon);
		gtk_widget_hide (paned->priv->status_label);
		gtk_expander_set_expanded (expander, FALSE);
	}
}

static void
attachment_paned_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_VIEW:
			e_attachment_paned_set_active_view (
				E_ATTACHMENT_PANED (object),
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

		case PROP_ALLOW_URI:
			e_attachment_view_set_allow_uri (
				E_ATTACHMENT_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_EXPANDED:
			e_attachment_paned_set_expanded (
				E_ATTACHMENT_PANED (object),
				g_value_get_boolean (value));
			return;

		case PROP_RESIZE_TOPLEVEL:
			e_attachment_paned_set_resize_toplevel (
				E_ATTACHMENT_PANED (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_paned_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_VIEW:
			g_value_set_int (
				value,
				e_attachment_paned_get_active_view (
				E_ATTACHMENT_PANED (object)));
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
				e_attachment_paned_get_expanded (
				E_ATTACHMENT_PANED (object)));
			return;

		case PROP_RESIZE_TOPLEVEL:
			g_value_set_boolean (
				value,
				e_attachment_paned_get_resize_toplevel (
				E_ATTACHMENT_PANED (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_paned_dispose (GObject *object)
{
	EAttachmentPaned *self = E_ATTACHMENT_PANED (object);

	if (self->priv->model != NULL) {
		e_attachment_store_remove_all (E_ATTACHMENT_STORE (self->priv->model));
		g_clear_object (&self->priv->model);
	}

	g_clear_object (&self->priv->expander);
	g_clear_object (&self->priv->notebook);
	g_clear_object (&self->priv->combo_box);
	g_clear_object (&self->priv->icon_view);
	g_clear_object (&self->priv->tree_view);
	g_clear_object (&self->priv->show_hide_label);
	g_clear_object (&self->priv->status_icon);
	g_clear_object (&self->priv->status_label);
	g_clear_object (&self->priv->content_area);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_paned_parent_class)->dispose (object);
}

static void
attachment_paned_constructed (GObject *object)
{
	EAttachmentPaned *self;
	GSettings *settings;

	self = E_ATTACHMENT_PANED (object);

	settings = e_util_ref_settings ("org.gnome.evolution.shell");

	/* Set up property-to-property bindings. */

	e_binding_bind_property (
		object, "active-view",
		self->priv->combo_box, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "active-view",
		self->priv->notebook, "page",
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
		self->priv->combo_box, "sensitive",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		object, "expanded",
		self->priv->notebook, "visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* Set up property-to-GSettings bindings. */
	g_settings_bind (
		settings, "attachment-view",
		object, "active-view",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_attachment_paned_parent_class)->constructed (object);
}

static EAttachmentViewPrivate *
attachment_paned_get_private (EAttachmentView *view)
{
	EAttachmentPaned *self = E_ATTACHMENT_PANED (view);

	return e_attachment_view_get_private (E_ATTACHMENT_VIEW (self->priv->icon_view));
}

static EAttachmentStore *
attachment_paned_get_store (EAttachmentView *view)
{
	EAttachmentPaned *self = E_ATTACHMENT_PANED (view);

	return e_attachment_view_get_store (E_ATTACHMENT_VIEW (self->priv->icon_view));
}

static GtkTreePath *
attachment_paned_get_path_at_pos (EAttachmentView *view,
                                  gint x,
                                  gint y)
{
	EAttachmentPaned *self = E_ATTACHMENT_PANED (view);

	return e_attachment_view_get_path_at_pos (E_ATTACHMENT_VIEW (self->priv->icon_view), x, y);
}

static GList *
attachment_paned_get_selected_paths (EAttachmentView *view)
{
	EAttachmentPaned *self = E_ATTACHMENT_PANED (view);

	return e_attachment_view_get_selected_paths (E_ATTACHMENT_VIEW (self->priv->icon_view));
}

static gboolean
attachment_paned_path_is_selected (EAttachmentView *view,
                                   GtkTreePath *path)
{
	EAttachmentPaned *self = E_ATTACHMENT_PANED (view);

	return e_attachment_view_path_is_selected (E_ATTACHMENT_VIEW (self->priv->icon_view), path);
}

static void
attachment_paned_select_path (EAttachmentView *view,
                              GtkTreePath *path)
{
	EAttachmentPaned *self = E_ATTACHMENT_PANED (view);

	e_attachment_view_select_path (E_ATTACHMENT_VIEW (self->priv->icon_view), path);
}

static void
attachment_paned_unselect_path (EAttachmentView *view,
                                GtkTreePath *path)
{
	EAttachmentPaned *self = E_ATTACHMENT_PANED (view);

	e_attachment_view_unselect_path (E_ATTACHMENT_VIEW (self->priv->icon_view), path);
}

static void
attachment_paned_select_all (EAttachmentView *view)
{
	EAttachmentPaned *self = E_ATTACHMENT_PANED (view);

	e_attachment_view_select_all (E_ATTACHMENT_VIEW (self->priv->icon_view));
}

static void
attachment_paned_unselect_all (EAttachmentView *view)
{
	EAttachmentPaned *self = E_ATTACHMENT_PANED (view);

	e_attachment_view_unselect_all (E_ATTACHMENT_VIEW (self->priv->icon_view));
}

static void
attachment_paned_update_actions (EAttachmentView *view)
{
	EAttachmentPaned *self = E_ATTACHMENT_PANED (view);

	e_attachment_view_update_actions (E_ATTACHMENT_VIEW (self->priv->icon_view));
}

static void
e_attachment_paned_class_init (EAttachmentPanedClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_paned_set_property;
	object_class->get_property = attachment_paned_get_property;
	object_class->dispose = attachment_paned_dispose;
	object_class->constructed = attachment_paned_constructed;

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
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_override_property (
		object_class, PROP_DRAGGING, "dragging");

	g_object_class_override_property (
		object_class, PROP_EDITABLE, "editable");

	g_object_class_override_property (
		object_class, PROP_ALLOW_URI, "allow-uri");

	g_object_class_install_property (
		object_class,
		PROP_EXPANDED,
		g_param_spec_boolean (
			"expanded",
			"Expanded",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_RESIZE_TOPLEVEL,
		g_param_spec_boolean (
			"resize-toplevel",
			"Resize-Toplevel",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
attachment_paned_style_updated_cb (EAttachmentPaned *paned)
{
	g_return_if_fail (E_IS_ATTACHMENT_PANED (paned));

	gtk_widget_style_get (
		GTK_WIDGET (paned), "handle-size",
		&paned->priv->vpaned_handle_size, NULL);

	if (paned->priv->vpaned_handle_size < 0)
		paned->priv->vpaned_handle_size = 0;
}

static void
e_attachment_paned_init (EAttachmentPaned *paned)
{
	EAttachmentView *view;
	GtkSizeGroup *size_group;
	GtkWidget *container;
	GtkWidget *widget;
	EUIAction *action;
	EUIManager *ui_manager;

	paned->priv = e_attachment_paned_get_instance_private (paned);
	paned->priv->model = e_attachment_store_new ();

	gtk_orientable_set_orientation (GTK_ORIENTABLE (paned), GTK_ORIENTATION_VERTICAL);

	/* Keep the expander label and combo box the same height. */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	/* Construct the Attachment Views */

	container = GTK_WIDGET (paned);

	widget = gtk_notebook_new ();
	gtk_widget_set_size_request (widget, -1, initial_height);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, FALSE);
	paned->priv->notebook = g_object_ref (widget);
	gtk_widget_hide (widget);

	container = paned->priv->notebook;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_notebook_append_page (GTK_NOTEBOOK (container), widget, NULL);
	gtk_widget_show (widget);

	container = widget;

	widget = e_attachment_icon_view_new ();
	gtk_widget_set_can_focus (widget, TRUE);
	gtk_icon_view_set_model (GTK_ICON_VIEW (widget), paned->priv->model);
	gtk_container_add (GTK_CONTAINER (container), widget);
	paned->priv->icon_view = g_object_ref (widget);
	gtk_widget_show (widget);

	container = paned->priv->notebook;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_notebook_append_page (GTK_NOTEBOOK (container), widget, NULL);
	gtk_widget_show (widget);

	container = widget;

	widget = e_attachment_tree_view_new ();
	gtk_widget_set_can_focus (widget, TRUE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), paned->priv->model);
	gtk_container_add (GTK_CONTAINER (container), widget);
	paned->priv->tree_view = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Construct the Controls */

	container = GTK_WIDGET (paned);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	paned->priv->content_area = g_object_ref (widget);
	gtk_widget_show (widget);

	paned->priv->vpaned_handle_size = 5;
	attachment_paned_style_updated_cb (paned);

	g_signal_connect (
		GTK_PANED (paned), "style-updated",
		G_CALLBACK (attachment_paned_style_updated_cb), NULL);

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_end (widget, 6);
	gtk_widget_set_margin_start (widget, 6);
	gtk_widget_set_margin_bottom (widget, 6);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	paned->priv->controls_container = widget;
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_expander_new (NULL);
	gtk_expander_set_spacing (GTK_EXPANDER (widget), 0);
	gtk_expander_set_label_fill (GTK_EXPANDER (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	paned->priv->expander = g_object_ref (widget);
	gtk_widget_show (widget);

	/* The "Add Attachment" button proxies the "add" action from
	 * one of the two attachment views.  Doesn't matter which. */
	view = E_ATTACHMENT_VIEW (paned->priv->icon_view);
	action = e_attachment_view_get_action (view, "add");

	widget = gtk_button_new_with_mnemonic (e_ui_action_get_label (action));
	if (e_ui_action_get_icon_name (action))
		gtk_button_set_image (GTK_BUTTON (widget), gtk_image_new_from_icon_name (e_ui_action_get_icon_name (action), GTK_ICON_SIZE_BUTTON));
	e_ui_action_util_assign_to_widget (action, widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* needed to be able to click the button */
	ui_manager = e_attachment_view_get_ui_manager (view);
	e_ui_manager_add_action_groups_to_widget (ui_manager, container);

	widget = gtk_combo_box_text_new ();
	gtk_size_group_add_widget (size_group, widget);
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("Icon View"));
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("List View"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	paned->priv->combo_box = g_object_ref (widget);
	gtk_widget_show (widget);

	container = paned->priv->expander;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_size_group_add_widget (size_group, widget);
	gtk_expander_set_label_widget (GTK_EXPANDER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("Show Attachment _Bar"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	paned->priv->show_hide_label = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_icon_name (
		"mail-attachment", GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	paned->priv->status_icon = g_object_ref (widget);
	gtk_widget_hide (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	paned->priv->status_label = g_object_ref (widget);
	gtk_widget_hide (widget);

	e_signal_connect_notify_swapped (
		paned->priv->expander, "notify::expanded",
		G_CALLBACK (attachment_paned_notify_cb), paned);

	e_signal_connect_notify_swapped (
		paned->priv->model, "notify::num-attachments",
		G_CALLBACK (attachment_paned_update_status), paned);

	e_signal_connect_notify_swapped (
		paned->priv->model, "notify::total-size",
		G_CALLBACK (attachment_paned_update_status), paned);

	g_object_unref (size_group);

	attachment_paned_notify_cb (paned, NULL, GTK_EXPANDER (paned->priv->expander));
}

static void
e_attachment_paned_interface_init (EAttachmentViewInterface *iface)
{
	iface->get_private = attachment_paned_get_private;
	iface->get_store = attachment_paned_get_store;
	iface->get_path_at_pos = attachment_paned_get_path_at_pos;
	iface->get_selected_paths = attachment_paned_get_selected_paths;
	iface->path_is_selected = attachment_paned_path_is_selected;
	iface->select_path = attachment_paned_select_path;
	iface->unselect_path = attachment_paned_unselect_path;
	iface->select_all = attachment_paned_select_all;
	iface->unselect_all = attachment_paned_unselect_all;
	iface->update_actions = attachment_paned_update_actions;
}

GtkWidget *
e_attachment_paned_new (void)
{
	return g_object_new (E_TYPE_ATTACHMENT_PANED, NULL);
}

GtkWidget *
e_attachment_paned_get_content_area (EAttachmentPaned *paned)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_PANED (paned), NULL);

	return paned->priv->content_area;
}

gint
e_attachment_paned_get_active_view (EAttachmentPaned *paned)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_PANED (paned), 0);

	return paned->priv->active_view;
}

void
e_attachment_paned_set_active_view (EAttachmentPaned *paned,
                                    gint active_view)
{
	EAttachmentView *source;
	EAttachmentView *target;

	g_return_if_fail (E_IS_ATTACHMENT_PANED (paned));
	g_return_if_fail (active_view >= 0 && active_view < NUM_VIEWS);

	if (active_view == paned->priv->active_view)
		return;

	paned->priv->active_view = active_view;

	/* Synchronize the item selection of the view we're
	 * switching TO with the view we're switching FROM. */
	if (active_view == 0) {
		/* from tree view to icon view */
		source = E_ATTACHMENT_VIEW (paned->priv->tree_view);
		target = E_ATTACHMENT_VIEW (paned->priv->icon_view);
	} else {
		/* from icon view to tree view */
		source = E_ATTACHMENT_VIEW (paned->priv->icon_view);
		target = E_ATTACHMENT_VIEW (paned->priv->tree_view);
	}

	e_attachment_view_sync_selection (source, target);

	g_object_notify (G_OBJECT (paned), "active-view");
}

gboolean
e_attachment_paned_get_expanded (EAttachmentPaned *paned)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_PANED (paned), FALSE);

	return paned->priv->expanded;
}

void
e_attachment_paned_set_expanded (EAttachmentPaned *paned,
                                 gboolean expanded)
{
	g_return_if_fail (E_IS_ATTACHMENT_PANED (paned));

	if (paned->priv->expanded == expanded)
		return;

	paned->priv->expanded = expanded;

	g_object_notify (G_OBJECT (paned), "expanded");
}

gboolean
e_attachment_paned_get_resize_toplevel (EAttachmentPaned *paned)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_PANED (paned), FALSE);

	return paned->priv->resize_toplevel;
}

void
e_attachment_paned_set_resize_toplevel (EAttachmentPaned *paned,
                                        gboolean resize_toplevel)
{
	g_return_if_fail (E_IS_ATTACHMENT_PANED (paned));

	if (paned->priv->resize_toplevel == resize_toplevel)
		return;

	paned->priv->resize_toplevel = resize_toplevel;

	g_object_notify (G_OBJECT (paned), "resize-toplevel");
}

void
e_attachment_paned_drag_data_received (EAttachmentPaned *paned,
                                       GdkDragContext *context,
                                       gint x,
                                       gint y,
                                       GtkSelectionData *selection,
                                       guint info,
                                       guint time)
{
	g_return_if_fail (E_IS_ATTACHMENT_PANED (paned));

	/* XXX Dirty hack for forwarding drop events. */
	g_signal_emit_by_name (
		paned->priv->icon_view, "drag-data-received",
		context, x, y, selection, info, time);
}

GtkWidget *
e_attachment_paned_get_controls_container (EAttachmentPaned *paned)
{
	return paned->priv->controls_container;
}

GtkWidget *
e_attachment_paned_get_view_combo (EAttachmentPaned *paned)
{
	return paned->priv->combo_box;
}

