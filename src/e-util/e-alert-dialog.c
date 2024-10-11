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
 *   Michael Zucchi <notzed@ximian.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include "e-alert-dialog.h"

struct _EAlertDialogPrivate {
	GtkWidget *content_area;  /* not referenced */
	EAlert *alert;
};

enum {
	PROP_0,
	PROP_ALERT
};

G_DEFINE_TYPE_WITH_PRIVATE (EAlertDialog, e_alert_dialog, GTK_TYPE_DIALOG)

static void
alert_dialog_set_alert (EAlertDialog *dialog,
                        EAlert *alert)
{
	g_return_if_fail (E_IS_ALERT (alert));
	g_return_if_fail (dialog->priv->alert == NULL);

	dialog->priv->alert = g_object_ref (alert);
}

static void
alert_dialog_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALERT:
			alert_dialog_set_alert (
				E_ALERT_DIALOG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
alert_dialog_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALERT:
			g_value_set_object (
				value, e_alert_dialog_get_alert (
				E_ALERT_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
alert_dialog_dispose (GObject *object)
{
	EAlertDialog *self = E_ALERT_DIALOG (object);

	if (self->priv->alert) {
		g_signal_handlers_disconnect_matched (
			self->priv->alert, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_clear_object (&self->priv->alert);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_alert_dialog_parent_class)->dispose (object);
}

static void
alert_dialog_constructed (GObject *object)
{
	EAlert *alert;
	EAlertDialog *dialog;
	GtkWidget *action_area;
	GtkWidget *content_area;
	GtkWidget *container;
	GtkWidget *widget;
	PangoAttribute *attr;
	PangoAttrList *list;
	GList *link;
	const gchar *primary, *secondary;
	gint default_response;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_alert_dialog_parent_class)->constructed (object);

	dialog = E_ALERT_DIALOG (object);
	alert = e_alert_dialog_get_alert (dialog);

	default_response = e_alert_get_default_response (alert);

	gtk_window_set_title (GTK_WINDOW (dialog), " ");

	/* XXX Making the window non-resizable is the only way at
	 *     present for GTK+ to pick a reasonable default size.
	 *     See https://bugzilla.gnome.org/681937 for details. */
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	gtk_container_set_border_width (GTK_CONTAINER (action_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

	/* Forward EAlert::response signals to GtkDialog::response. */
	g_signal_connect_swapped (
		alert, "response",
		G_CALLBACK (gtk_dialog_response), dialog);

	/* Add buttons from actions. */
	link = e_alert_peek_actions (alert);
	if (!link && !e_alert_peek_widgets (alert)) {
		EUIAction *action;

		/* Make sure there is at least one action,
		 * thus the dialog can be closed. */
		action = e_ui_action_new ("alert-dialog-map", "alert-response-0", NULL);
		e_ui_action_set_label (action, _("_Dismiss"));
		e_alert_add_action (alert, action, GTK_RESPONSE_CLOSE, FALSE);
		g_object_unref (action);

		link = e_alert_peek_actions (alert);
	}

	while (link != NULL) {
		GtkWidget *button;
		EUIAction *action = E_UI_ACTION (link->data);
		gpointer data;

		/* These actions are already wired to trigger an
		 * EAlert::response signal when activated, which
		 * will in turn call to gtk_dialog_response(),
		 * so we can add buttons directly to the action
		 * area without knowing their response IDs.
		 * (XXX Well, kind of.  See below.) */

		button = e_alert_create_button_for_action (action);

		gtk_widget_set_can_default (button, TRUE);
		gtk_box_pack_end (GTK_BOX (action_area), button, FALSE, FALSE, 0);

		/* This is set in e_alert_add_action(). */
		data = g_object_get_data (G_OBJECT (action), "e-alert-response-id");

		/* Normally GtkDialog sets the initial focus widget to
		 * the button corresponding to the default response, but
		 * because the buttons are not directly tied to response
		 * IDs, we have set both the default widget and the
		 * initial focus widget ourselves. */
		if (GPOINTER_TO_INT (data) == default_response) {
			gtk_widget_grab_default (button);
			gtk_widget_grab_focus (button);
		}

		link = g_list_next (link);
	}

	link = e_alert_peek_widgets (alert);
	while (link != NULL) {
		widget = link->data;

		gtk_box_pack_end (GTK_BOX (action_area), widget, FALSE, FALSE, 0);
		link = g_list_next (link);
	}

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
	gtk_box_pack_start (GTK_BOX (content_area), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_alert_create_image (alert, GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	dialog->priv->content_area = widget;
	gtk_widget_show (widget);

	container = widget;

	primary = e_alert_get_primary_text (alert);
	secondary = e_alert_get_secondary_text (alert);

	list = pango_attr_list_new ();
	attr = pango_attr_scale_new (PANGO_SCALE_LARGE);
	pango_attr_list_insert (list, attr);
	attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
	pango_attr_list_insert (list, attr);

	widget = gtk_label_new (primary);
	gtk_label_set_attributes (GTK_LABEL (widget), list);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_selectable (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 40);
	gtk_label_set_max_width_chars (GTK_LABEL (widget), 60);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_yalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_set_can_focus (widget, FALSE);
	gtk_widget_show (widget);

	widget = gtk_label_new (secondary);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_selectable (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 60);
	gtk_label_set_max_width_chars (GTK_LABEL (widget), 80);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_yalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_set_can_focus (widget, FALSE);
	gtk_widget_show (widget);

	pango_attr_list_unref (list);
}

static void
e_alert_dialog_class_init (EAlertDialogClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = alert_dialog_set_property;
	object_class->get_property = alert_dialog_get_property;
	object_class->dispose = alert_dialog_dispose;
	object_class->constructed = alert_dialog_constructed;

	g_object_class_install_property (
		object_class,
		PROP_ALERT,
		g_param_spec_object (
			"alert",
			"Alert",
			"Alert to be displayed",
			E_TYPE_ALERT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_alert_dialog_init (EAlertDialog *dialog)
{
	dialog->priv = e_alert_dialog_get_instance_private (dialog);
}

GtkWidget *
e_alert_dialog_new (GtkWindow *parent,
                    EAlert *alert)
{
	g_return_val_if_fail (E_IS_ALERT (alert), NULL);

	return g_object_new (
		E_TYPE_ALERT_DIALOG,
		"alert", alert, "transient-for", parent, NULL);
}

GtkWidget *
e_alert_dialog_new_for_args (GtkWindow *parent,
                             const gchar *tag,
                             ...)
{
	GtkWidget *dialog;
	EAlert *alert;
	va_list ap;

	g_return_val_if_fail (tag != NULL, NULL);

	va_start (ap, tag);
	alert = e_alert_new_valist (tag, ap);
	va_end (ap);

	dialog = e_alert_dialog_new (parent, alert);

	g_object_unref (alert);

	return dialog;
}

static gboolean
dialog_focus_in_event_cb (GtkWindow *dialog,
                          GdkEvent *event,
                          GtkWindow *parent)
{
	gtk_window_set_urgency_hint (parent, FALSE);

	return FALSE;
}

gint
e_alert_run_dialog (GtkWindow *parent,
                    EAlert *alert)
{
	GtkWidget *dialog;
	gint response;
	gulong signal_id = 0;
	gulong parent_destroyed_signal_id = 0;

	g_return_val_if_fail (E_IS_ALERT (alert), 0);

	dialog = e_alert_dialog_new (parent, alert);

	if (parent != NULL) {
		/* Since we'll be in a nested main loop, the widgets may be destroyed
		 * before we get back from gtk_dialog_run(). In practice, this can happen
		 * if Evolution exits while the dialog is up. Make sure we don't try
		 * to access destroyed widgets. */
		parent_destroyed_signal_id = g_signal_connect (parent, "destroy", G_CALLBACK (gtk_widget_destroyed), &parent);

		gtk_window_set_urgency_hint (parent, TRUE);
		signal_id = g_signal_connect (
			dialog, "focus-in-event",
			G_CALLBACK (dialog_focus_in_event_cb), parent);
	} else {
		gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
	}

	g_signal_connect (dialog, "destroy", G_CALLBACK (gtk_widget_destroyed), &dialog);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (signal_id > 0) {
		if (parent)
			gtk_window_set_urgency_hint (parent, FALSE);
		if (dialog)
			g_signal_handler_disconnect (dialog, signal_id);
	}

	if (dialog)
		gtk_widget_destroy (dialog);
	if (parent && parent_destroyed_signal_id)
		g_signal_handler_disconnect (parent, parent_destroyed_signal_id);

	return response;
}

gint
e_alert_run_dialog_for_args (GtkWindow *parent,
                             const gchar *tag,
                             ...)
{
	EAlert *alert;
	gint response;
	va_list ap;

	g_return_val_if_fail (tag != NULL, 0);

	va_start (ap, tag);
	alert = e_alert_new_valist (tag, ap);
	va_end (ap);

	response = e_alert_run_dialog (parent, alert);

	g_object_unref (alert);

	return response;
}

/**
 * e_alert_dialog_get_alert:
 * @dialog: an #EAlertDialog
 *
 * Returns the #EAlert associated with @dialog.
 *
 * Returns: the #EAlert associated with @dialog
 **/
EAlert *
e_alert_dialog_get_alert (EAlertDialog *dialog)
{
	g_return_val_if_fail (E_IS_ALERT_DIALOG (dialog), NULL);

	return dialog->priv->alert;
}

/**
 * e_alert_dialog_get_content_area:
 * @dialog: an #EAlertDialog
 *
 * Returns the vertical box containing the primary and secondary labels.
 * Use this to pack additional widgets into the dialog with the proper
 * horizontal alignment (maintaining the left margin below the image).
 *
 * Returns: the content area #GtkBox
 **/
GtkWidget *
e_alert_dialog_get_content_area (EAlertDialog *dialog)
{
	g_return_val_if_fail (E_IS_ALERT_DIALOG (dialog), NULL);

	return dialog->priv->content_area;
}
