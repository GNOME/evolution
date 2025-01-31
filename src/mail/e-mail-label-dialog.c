/*
 * e-mail-label-dialog.c
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

#include "e-util/e-util.h"
#include "e-mail-label-dialog.h"

#include <glib/gi18n.h>

struct _EMailLabelDialogPrivate {
	GtkWidget *entry;
	GtkWidget *colorsel;
};

enum {
	PROP_0,
	PROP_LABEL_COLOR,
	PROP_LABEL_NAME
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailLabelDialog, e_mail_label_dialog, GTK_TYPE_DIALOG)

static void
mail_label_dialog_entry_changed_cb (EMailLabelDialog *dialog)
{
	const gchar *text;
	gboolean sensitive;

	text = gtk_entry_get_text (GTK_ENTRY (dialog->priv->entry));
	sensitive = (text != NULL && *text != '\0');

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK, sensitive);
}

static void
mail_label_dialog_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_LABEL_COLOR:
			e_mail_label_dialog_set_label_color (
				E_MAIL_LABEL_DIALOG (object),
				g_value_get_boxed (value));
			return;

		case PROP_LABEL_NAME:
			e_mail_label_dialog_set_label_name (
				E_MAIL_LABEL_DIALOG (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_label_dialog_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	GdkRGBA color;

	switch (property_id) {
		case PROP_LABEL_COLOR:
			e_mail_label_dialog_get_label_color (
				E_MAIL_LABEL_DIALOG (object), &color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_LABEL_NAME:
			g_value_set_string (
				value, e_mail_label_dialog_get_label_name (
				E_MAIL_LABEL_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_label_dialog_dispose (GObject *object)
{
	EMailLabelDialog *self = E_MAIL_LABEL_DIALOG (object);

	g_clear_object (&self->priv->entry);
	g_clear_object (&self->priv->colorsel);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_label_dialog_parent_class)->dispose (object);
}

static void
mail_label_dialog_constructed (GObject *object)
{
	GtkWidget *action_area;
	GtkWidget *content_area;

	/* XXX Override GTK's style property defaults for GtkDialog.
	 *     Hopefully GTK+ 3.0 will fix the broken defaults. */

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (object));
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (object));

	gtk_box_set_spacing (GTK_BOX (content_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (object), 12);
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 0);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_label_dialog_parent_class)->constructed (object);
}

static void
e_mail_label_dialog_class_init (EMailLabelDialogClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_label_dialog_set_property;
	object_class->get_property = mail_label_dialog_get_property;
	object_class->dispose = mail_label_dialog_dispose;
	object_class->constructed = mail_label_dialog_constructed;

	g_object_class_install_property (
		object_class,
		PROP_LABEL_COLOR,
		g_param_spec_boxed (
			"label-color",
			"Label Color",
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_LABEL_NAME,
		g_param_spec_string (
			"label-name",
			"Label Name",
			NULL,
			NULL,
			G_PARAM_READWRITE));
}

static void
e_mail_label_dialog_init (EMailLabelDialog *dialog)
{
	GtkWidget *content_area;
	GtkWidget *container;
	GtkWidget *widget;

	dialog->priv = e_mail_label_dialog_get_instance_private (dialog);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	gtk_dialog_add_button (
		GTK_DIALOG (dialog),
		_("_Cancel"), GTK_RESPONSE_CANCEL);

	gtk_dialog_add_button (
		GTK_DIALOG (dialog),
		_("OK"), GTK_RESPONSE_OK);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	container = content_area;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	gtk_box_pack_end (GTK_BOX (container), widget, TRUE, TRUE, 0);
	dialog->priv->entry = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (mail_label_dialog_entry_changed_cb), dialog);

	mail_label_dialog_entry_changed_cb (dialog);

	widget = gtk_label_new_with_mnemonic (_("_Label name:"));
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->entry);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = content_area;

	widget = gtk_color_selection_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	dialog->priv->colorsel = g_object_ref (widget);
	gtk_widget_show (widget);
}

GtkWidget *
e_mail_label_dialog_new (GtkWindow *parent)
{
	return g_object_new (
		E_TYPE_MAIL_LABEL_DIALOG,
		"transient-for", parent,
		"use-header-bar", e_util_get_use_header_bar (),
		 NULL);
}

const gchar *
e_mail_label_dialog_get_label_name (EMailLabelDialog *dialog)
{
	GtkEntry *entry;

	g_return_val_if_fail (E_IS_MAIL_LABEL_DIALOG (dialog), NULL);

	entry = GTK_ENTRY (dialog->priv->entry);

	return gtk_entry_get_text (entry);
}

void
e_mail_label_dialog_set_label_name (EMailLabelDialog *dialog,
                                    const gchar *label_name)
{
	GtkEntry *entry;

	g_return_if_fail (E_IS_MAIL_LABEL_DIALOG (dialog));

	entry = GTK_ENTRY (dialog->priv->entry);

	if (g_strcmp0 (gtk_entry_get_text (entry), label_name) == 0)
		return;

	gtk_entry_set_text (entry, label_name);

	g_object_notify (G_OBJECT (dialog), "label-name");
}

void
e_mail_label_dialog_get_label_color (EMailLabelDialog *dialog,
                                     GdkRGBA *label_color)
{
	GtkColorSelection *colorsel;

	g_return_if_fail (E_IS_MAIL_LABEL_DIALOG (dialog));
	g_return_if_fail (label_color != NULL);

	colorsel = GTK_COLOR_SELECTION (dialog->priv->colorsel);

	gtk_color_selection_get_current_rgba (colorsel, label_color);
}

void
e_mail_label_dialog_set_label_color (EMailLabelDialog *dialog,
                                     const GdkRGBA *label_color)
{
	GtkColorSelection *colorsel;

	g_return_if_fail (E_IS_MAIL_LABEL_DIALOG (dialog));
	g_return_if_fail (label_color != NULL);

	colorsel = GTK_COLOR_SELECTION (dialog->priv->colorsel);

	gtk_color_selection_set_current_rgba (colorsel, label_color);

	g_object_notify (G_OBJECT (dialog), "label-color");
}
