/*
 * e-attachment-paned.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-attachment-paned.h"

#include <glib/gi18n.h>

#include "e-util/e-binding.h"
#include "e-util/gconf-bridge.h"

#include "e-attachment-view.h"
#include "e-attachment-store.h"
#include "e-attachment-icon-view.h"
#include "e-attachment-tree-view.h"

#define E_ATTACHMENT_PANED_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_PANED, EAttachmentPanedPrivate))

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
	guint expanded : 1;
};

enum {
	PROP_0,
	PROP_ACTIVE_VIEW,
	PROP_DRAGGING,
	PROP_EDITABLE,
	PROP_EXPANDED
};

/* Forward Declarations */
static void	e_attachment_paned_interface_init
					(EAttachmentViewInterface *interface);

G_DEFINE_TYPE_WITH_CODE (
	EAttachmentPaned,
	e_attachment_paned,
	GTK_TYPE_VPANED,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_ATTACHMENT_VIEW,
		e_attachment_paned_interface_init))

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
	GtkLabel *label;
	const gchar *text;

	label = GTK_LABEL (paned->priv->show_hide_label);

	/* Update the expander label. */
	if (gtk_expander_get_expanded (expander))
		text = _("Hide Attachment _Bar");
	else
		text = _("Show Attachment _Bar");

	gtk_label_set_text_with_mnemonic (label, text);
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
	display_size = g_format_size_for_display (total_size);

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

		case PROP_EXPANDED:
			e_attachment_paned_set_expanded (
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
				value, e_attachment_paned_get_active_view (
				E_ATTACHMENT_PANED (object)));
			return;

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

		case PROP_EXPANDED:
			g_value_set_boolean (
				value, e_attachment_paned_get_expanded (
				E_ATTACHMENT_PANED (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_paned_dispose (GObject *object)
{
	EAttachmentPanedPrivate *priv;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (object);

	if (priv->model != NULL) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	if (priv->expander != NULL) {
		g_object_unref (priv->expander);
		priv->expander = NULL;
	}

	if (priv->notebook != NULL) {
		g_object_unref (priv->notebook);
		priv->notebook = NULL;
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

	if (priv->show_hide_label != NULL) {
		g_object_unref (priv->show_hide_label);
		priv->show_hide_label = NULL;
	}

	if (priv->status_icon != NULL) {
		g_object_unref (priv->status_icon);
		priv->status_icon = NULL;
	}

	if (priv->status_label != NULL) {
		g_object_unref (priv->status_label);
		priv->status_label = NULL;
	}

	if (priv->content_area != NULL) {
		g_object_unref (priv->content_area);
		priv->content_area = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_paned_parent_class)->dispose (object);
}

static void
attachment_paned_constructed (GObject *object)
{
	EAttachmentPanedPrivate *priv;
	GConfBridge *bridge;
	const gchar *key;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (object);

	bridge = gconf_bridge_get ();

	/* Set up property-to-property bindings. */

	e_mutual_binding_new (
		object, "active-view",
		priv->combo_box, "active");

	e_mutual_binding_new (
		object, "active-view",
		priv->notebook, "page");

	e_mutual_binding_new (
		object, "dragging",
		priv->icon_view, "dragging");

	e_mutual_binding_new (
		object, "dragging",
		priv->tree_view, "dragging");

	e_mutual_binding_new (
		object, "editable",
		priv->icon_view, "editable");

	e_mutual_binding_new (
		object, "editable",
		priv->tree_view, "editable");

	e_mutual_binding_new (
		object, "expanded",
		priv->expander, "expanded");

	e_mutual_binding_new (
		object, "expanded",
		priv->combo_box, "sensitive");

	e_mutual_binding_new (
		object, "expanded",
		priv->notebook, "visible");

	/* Set up property-to-GConf bindings. */

	key = "/apps/evolution/shell/attachment_view";
	gconf_bridge_bind_property (bridge, key, object, "active-view");
}

static EAttachmentViewPrivate *
attachment_paned_get_private (EAttachmentView *view)
{
	EAttachmentPanedPrivate *priv;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (view);
	view = E_ATTACHMENT_VIEW (priv->icon_view);

	return e_attachment_view_get_private (view);
}

static EAttachmentStore *
attachment_paned_get_store (EAttachmentView *view)
{
	EAttachmentPanedPrivate *priv;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (view);
	view = E_ATTACHMENT_VIEW (priv->icon_view);

	return e_attachment_view_get_store (view);
}

static GtkTreePath *
attachment_paned_get_path_at_pos (EAttachmentView *view,
                                  gint x,
                                  gint y)
{
	EAttachmentPanedPrivate *priv;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (view);
	view = E_ATTACHMENT_VIEW (priv->icon_view);

	return e_attachment_view_get_path_at_pos (view, x, y);
}

static GList *
attachment_paned_get_selected_paths (EAttachmentView *view)
{
	EAttachmentPanedPrivate *priv;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (view);
	view = E_ATTACHMENT_VIEW (priv->icon_view);

	return e_attachment_view_get_selected_paths (view);
}

static gboolean
attachment_paned_path_is_selected (EAttachmentView *view,
                                   GtkTreePath *path)
{
	EAttachmentPanedPrivate *priv;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (view);
	view = E_ATTACHMENT_VIEW (priv->icon_view);

	return e_attachment_view_path_is_selected (view, path);
}

static void
attachment_paned_select_path (EAttachmentView *view,
                              GtkTreePath *path)
{
	EAttachmentPanedPrivate *priv;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (view);
	view = E_ATTACHMENT_VIEW (priv->icon_view);

	e_attachment_view_select_path (view, path);
}

static void
attachment_paned_unselect_path (EAttachmentView *view,
                                GtkTreePath *path)
{
	EAttachmentPanedPrivate *priv;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (view);
	view = E_ATTACHMENT_VIEW (priv->icon_view);

	e_attachment_view_unselect_path (view, path);
}

static void
attachment_paned_select_all (EAttachmentView *view)
{
	EAttachmentPanedPrivate *priv;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (view);
	view = E_ATTACHMENT_VIEW (priv->icon_view);

	e_attachment_view_select_all (view);
}

static void
attachment_paned_unselect_all (EAttachmentView *view)
{
	EAttachmentPanedPrivate *priv;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (view);
	view = E_ATTACHMENT_VIEW (priv->icon_view);

	e_attachment_view_unselect_all (view);
}

static void
attachment_paned_update_actions (EAttachmentView *view)
{
	EAttachmentPanedPrivate *priv;

	priv = E_ATTACHMENT_PANED_GET_PRIVATE (view);
	view = E_ATTACHMENT_VIEW (priv->icon_view);

	e_attachment_view_update_actions (view);
}

static void
e_attachment_paned_class_init (EAttachmentPanedClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EAttachmentPanedPrivate));

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

	g_object_class_override_property (
		object_class, PROP_DRAGGING, "dragging");

	g_object_class_override_property (
		object_class, PROP_EDITABLE, "editable");
}

static void
e_attachment_paned_init (EAttachmentPaned *paned)
{
	EAttachmentView *view;
	GtkSizeGroup *size_group;
	GtkWidget *container;
	GtkWidget *widget;
	GtkAction *action;

	paned->priv = E_ATTACHMENT_PANED_GET_PRIVATE (paned);
	paned->priv->model = e_attachment_store_new ();

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
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
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

	widget = gtk_vbox_new (FALSE, 6);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	paned->priv->content_area = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	paned->priv->controls_container = widget;
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_expander_new (NULL);
	gtk_expander_set_spacing (GTK_EXPANDER (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	paned->priv->expander = g_object_ref (widget);
	gtk_widget_show (widget);

	/* The "Add Attachment" button proxies the "add" action from
	 * one of the two attachment views.  Doesn't matter which. */
	widget = gtk_button_new ();
	view = E_ATTACHMENT_VIEW (paned->priv->icon_view);
	action = e_attachment_view_get_action (view, "add");
	gtk_button_set_image (GTK_BUTTON (widget), gtk_image_new ());
	gtk_activatable_set_related_action (GTK_ACTIVATABLE (widget), action);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_combo_box_new_text ();
	gtk_size_group_add_widget (size_group, widget);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Icon View"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("List View"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	paned->priv->combo_box = g_object_ref (widget);
	gtk_widget_show (widget);

	container = paned->priv->expander;

	/* Request the width to be as large as possible, and let the
	 * GtkExpander allocate what space there is.  This effectively
	 * packs the widget to expand.
	 *
	 * XXX This hack causes nasty side-effects in RTL locales,
	 *     so check the reading direction before applying it.
	 *     See GNOME bug #614049 for details + screenshot.
	 *
	 * XXX It also trips a height-for-width assertion GTK+ 3.
	 *     The same GNOME bug has a patch to add a "label-fill"
	 *     boolean property to GtkExpander to allow me to do
	 *     what I'm trying to do here properly.
	 */
	widget = gtk_hbox_new (FALSE, 6);
	gtk_size_group_add_widget (size_group, widget);
#if !GTK_CHECK_VERSION(2,90,0)
	if (gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL)
		gtk_widget_set_size_request (widget, G_MAXINT, -1);
#endif
	gtk_expander_set_label_widget (GTK_EXPANDER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("Show Attachment _Bar"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	paned->priv->show_hide_label = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_alignment_new (0.5, 0.5, 0.0, 1.0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_hbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (container), widget);
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

	g_signal_connect_swapped (
		paned->priv->expander, "notify::expanded",
		G_CALLBACK (attachment_paned_notify_cb), paned);

	g_signal_connect_swapped (
		paned->priv->model, "notify::num-attachments",
		G_CALLBACK (attachment_paned_update_status), paned);

	g_signal_connect_swapped (
		paned->priv->model, "notify::total-size",
		G_CALLBACK (attachment_paned_update_status), paned);

	g_object_unref (size_group);
}

static void
e_attachment_paned_interface_init (EAttachmentViewInterface *interface)
{
	interface->get_private = attachment_paned_get_private;
	interface->get_store = attachment_paned_get_store;
	interface->get_path_at_pos = attachment_paned_get_path_at_pos;
	interface->get_selected_paths = attachment_paned_get_selected_paths;
	interface->path_is_selected = attachment_paned_path_is_selected;
	interface->select_path = attachment_paned_select_path;
	interface->unselect_path = attachment_paned_unselect_path;
	interface->select_all = attachment_paned_select_all;
	interface->unselect_all = attachment_paned_unselect_all;
	interface->update_actions = attachment_paned_update_actions;
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

	paned->priv->expanded = expanded;

	g_object_notify (G_OBJECT (paned), "expanded");
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

