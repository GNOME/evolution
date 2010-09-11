/*
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
 * Authors:
 *   Michael Zucchi <notzed@ximian.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 */

#include "e-alert-dialog.h"
#include "e-util.h"

G_DEFINE_TYPE (
	EAlertDialog,
	e_alert_dialog,
	GTK_TYPE_DIALOG)

#define ALERT_DIALOG_PRIVATE(o) \
		(G_TYPE_INSTANCE_GET_PRIVATE ((o), E_TYPE_ALERT_DIALOG, EAlertDialogPrivate))

struct _EAlertDialogPrivate
{
	GtkWindow *parent;
	EAlert *alert;
};

enum
{
	PROP_0,
	PROP_PARENT,
	PROP_ALERT
};

static void
e_alert_dialog_set_property (GObject *object, guint property_id,
			     const GValue *value, GParamSpec *pspec)
{
	EAlertDialog *dialog = (EAlertDialog*) object;

	switch (property_id)
	{
		case PROP_PARENT:
			dialog->priv->parent = g_value_dup_object (value);
			break;
		case PROP_ALERT:
			dialog->priv->alert = g_value_dup_object (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
e_alert_dialog_get_property (GObject *object, guint property_id,
			     GValue *value, GParamSpec *pspec)
{
	EAlertDialog *dialog = (EAlertDialog*) object;

	switch (property_id)
	{
		case PROP_PARENT:
			g_value_set_object (value, dialog->priv->parent);
			break;
		case PROP_ALERT:
			g_value_set_object (value, dialog->priv->alert);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
e_alert_dialog_dispose (GObject *object)
{
	EAlertDialog *dialog = (EAlertDialog*) object;

	if (dialog->priv->parent) {
		g_object_unref (dialog->priv->parent);
		dialog->priv->parent = NULL;
	}

	if (dialog->priv->alert) {
		g_object_unref (dialog->priv->alert);
		dialog->priv->alert = NULL;
	}

	G_OBJECT_CLASS (e_alert_dialog_parent_class)->dispose (object);
}

static void
dialog_response_cb (GtkWidget *w, guint button, gpointer user_data)
{
	EAlertDialogPrivate *priv = ALERT_DIALOG_PRIVATE (w);

	if (button == GTK_RESPONSE_HELP) {
		g_signal_stop_emission_by_name(w, "response");
		e_display_help (GTK_WINDOW (w), e_alert_peek_help_uri (priv->alert));
	}
}

static void
e_alert_dialog_init (EAlertDialog *self)
{
	self->priv = ALERT_DIALOG_PRIVATE (self);
}

static void
e_alert_dialog_constructed (GObject *obj)
{
	EAlertDialog *self = (EAlertDialog*) obj;
	EAlert *alert;
	struct _e_alert_button *b;
	GtkWidget *hbox, *w, *scroll=NULL;
	GtkWidget *action_area;
	GtkWidget *content_area;
	GString *out;
	gchar *title, *primary, *secondary;

	g_return_if_fail (self != NULL);

	self->priv = ALERT_DIALOG_PRIVATE (self);
	alert = self->priv->alert;

	action_area = gtk_dialog_get_action_area ((GtkDialog*) self);
	content_area = gtk_dialog_get_content_area ((GtkDialog*) self);

#if !GTK_CHECK_VERSION(2,90,7)
	g_object_set (self, "has-separator", FALSE, NULL);
#endif

	gtk_widget_ensure_style ((GtkWidget *)self);
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

	if (self->priv->parent)
		gtk_window_set_transient_for ((GtkWindow *)self, self->priv->parent);
	else
		g_warning (
			"Something called %s() with a NULL parent window.  "
			"This is no longer legal, please fix it.", G_STRFUNC);

	if (e_alert_get_flags (alert) & GTK_DIALOG_MODAL)
		gtk_window_set_modal ((GtkWindow *)self, TRUE);
	gtk_window_set_destroy_with_parent ((GtkWindow *)self, TRUE);

	if (e_alert_peek_help_uri (alert)) {
		gtk_dialog_add_button ((GtkDialog*) self, GTK_STOCK_HELP, GTK_RESPONSE_HELP);
		g_signal_connect(self, "response", G_CALLBACK(dialog_response_cb), NULL);
	}

	b = e_alert_peek_buttons (alert);
	if (b == NULL) {
		gtk_dialog_add_button ((GtkDialog*) self, GTK_STOCK_OK, GTK_RESPONSE_OK);
	} else {
		for (; b; b=b->next) {
			if (b->stock) {
				if (b->label) {
#if 0
					/* FIXME: So although this looks like it will work, it wont.
					   Need to do it the hard way ... it also breaks the
					   default_response stuff */
					w = gtk_button_new_from_stock (b->stock);
					gtk_button_set_label ((GtkButton *)w, b->label);
					gtk_widget_show (w);
					gtk_dialog_add_action_widget (self, w, b->response);
#endif
					gtk_dialog_add_button ((GtkDialog*) self, b->label, b->response);
				} else
					gtk_dialog_add_button ((GtkDialog*) self, b->stock, b->response);
			} else
				gtk_dialog_add_button ((GtkDialog*) self, b->label, b->response);
		}
	}

	if (e_alert_get_default_response (alert))
		gtk_dialog_set_default_response ((GtkDialog*) self,
						e_alert_get_default_response (alert));

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_set_border_width ((GtkContainer *)hbox, 12);

	w = gtk_image_new_from_stock
		(e_alert_peek_stock_image (alert), GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment ((GtkMisc *)w, 0.0, 0.0);
	gtk_box_pack_start ((GtkBox *)hbox, w, FALSE, FALSE, 12);

	title = e_alert_get_title (alert, FALSE);
	gtk_window_set_title ((GtkWindow *)self, title);

	out = g_string_new ("");
	primary = e_alert_get_primary_text (alert, TRUE);
	if (primary) {
		g_string_append_printf (out,
					"<span weight=\"bold\" size=\"larger\">%s</span>\n\n",
					primary);
	}

	secondary = e_alert_get_secondary_text (alert, TRUE);
	if (secondary) {
		g_string_append (out, secondary);
	}

	g_free (secondary);
	g_free (title);
	g_free (primary);

	if (e_alert_get_scroll (alert)) {
		scroll = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (
			GTK_SCROLLED_WINDOW (scroll),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	}
	w = gtk_label_new (NULL);
	gtk_label_set_selectable ((GtkLabel *)w, TRUE);
	gtk_label_set_line_wrap ((GtkLabel *)w, TRUE);
	gtk_label_set_markup ((GtkLabel *)w, out->str);
	gtk_widget_set_can_focus (w, FALSE);
	g_string_free (out, TRUE);
	if (e_alert_get_scroll (alert)) {
		gtk_scrolled_window_add_with_viewport ((GtkScrolledWindow *)scroll, w);
		gtk_box_pack_start ((GtkBox *)hbox, scroll, FALSE, FALSE, 0);
		gtk_window_set_default_size ((GtkWindow *)self, 360, 180);
	} else
		gtk_box_pack_start ((GtkBox *)hbox, w, TRUE, TRUE, 0);

	gtk_widget_show_all (hbox);

	gtk_box_pack_start (GTK_BOX (content_area), hbox, TRUE, TRUE, 0);
}

static void
e_alert_dialog_class_init (EAlertDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (EAlertDialogPrivate));

	object_class->dispose = e_alert_dialog_dispose;
	object_class->get_property = e_alert_dialog_get_property;
	object_class->set_property = e_alert_dialog_set_property;
	object_class->constructed = e_alert_dialog_constructed;

	g_object_class_install_property (object_class,
					 PROP_PARENT,
					 g_param_spec_object ("parent",
							      "parent window",
							      "A parent window to be transient for",
							      GTK_TYPE_WINDOW,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_ALERT,
					 g_param_spec_object ("alert",
							     "alert",
							     "EAlert to be displayed",
							     E_TYPE_ALERT,
							      G_PARAM_READWRITE |
							     G_PARAM_CONSTRUCT_ONLY |
							     G_PARAM_STATIC_STRINGS));
}

GtkWidget*
e_alert_dialog_new (GtkWindow *parent,
                    EAlert *alert)
{
	return g_object_new (
		E_TYPE_ALERT_DIALOG,
		"parent", parent, "alert", alert, NULL);
}

GtkWidget*
e_alert_dialog_new_for_args (GtkWindow *parent, const gchar *tag, ...)
{
	GtkWidget *d;
	EAlert *e;
	va_list ap;

	va_start (ap, tag);
	e = e_alert_new_valist (tag, ap);
	va_end (ap);

	d = e_alert_dialog_new (parent, e);
	g_object_unref (e);

	return d;
}

gint
e_alert_run_dialog (GtkWindow *parent, EAlert *alert)
{
	GtkWidget *dialog;
	gint res;

	dialog = e_alert_dialog_new (parent, alert);

	res = gtk_dialog_run ((GtkDialog *)dialog);
	gtk_widget_destroy (dialog);

	return res;
}

gint
e_alert_run_dialog_for_args (GtkWindow *parent, const gchar *tag, ...)
{
	EAlert *e;
	va_list ap;
	gint response;

	va_start (ap, tag);
	e = e_alert_new_valist (tag, ap);
	va_end (ap);

	response = e_alert_run_dialog (parent, e);
	g_object_unref (e);

	return response;
}

/**
 * e_alert_dialog_count_buttons:
 * @dialog: a #EAlertDialog
 *
 * Counts the number of buttons in @dialog's action area.
 *
 * Returns: number of action area buttons
 **/
guint
e_alert_dialog_count_buttons (EAlertDialog *dialog)
{
	GtkWidget *container;
	GList *children, *iter;
	guint n_buttons = 0;

	g_return_val_if_fail (E_IS_ALERT_DIALOG (dialog), 0);

	container = gtk_dialog_get_action_area ((GtkDialog*) dialog);
	children = gtk_container_get_children (GTK_CONTAINER (container));

	/* Iterate over the children looking for buttons. */
	for (iter = children; iter != NULL; iter = iter->next)
		if (GTK_IS_BUTTON (iter->data))
			n_buttons++;

	g_list_free (children);

	return n_buttons;
}

/**
 * e_alert_dialog_get_alert:
 * @dialog: a #EAlertDialog
 *
 * Convenience API for getting the #EAlert associated with @dialog
 *
 * Return value: the #EAlert associated with @dialog.  The alert should be
 * unreffed when no longer needed.
 */
EAlert *
e_alert_dialog_get_alert (EAlertDialog *dialog)
{
	EAlert *alert = NULL;

	g_return_val_if_fail (dialog != NULL, NULL);

	g_object_get (dialog, "alert", &alert,
		      NULL);
	return alert;
}
