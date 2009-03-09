/*
 * e-attachment-dialog.c
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

#include "e-attachment-dialog.h"

#include <glib/gi18n.h>

#define E_ATTACHMENT_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_DIALOG, EAttachmentDialogPrivate))

struct _EAttachmentDialogPrivate {
	EAttachment *attachment;
	GtkWidget *filename_entry;
	GtkWidget *description_entry;
	GtkWidget *mime_type_label;
	GtkWidget *disposition_checkbox;
};

enum {
	PROP_0,
	PROP_ATTACHMENT
};

static gpointer parent_class;

static void
attachment_dialog_update (EAttachmentDialog *dialog)
{
	EAttachment *attachment;
	CamelMimePart *mime_part;
	GtkWidget *widget;
	gboolean sensitive;
	const gchar *text;
	gboolean active;

	/* XXX This is too complex.  I shouldn't have to use the
	 *     MIME part at all. */

	attachment = e_attachment_dialog_get_attachment (dialog);
	if (attachment != NULL)
		mime_part = e_attachment_get_mime_part (attachment);
	else
		mime_part = NULL;

	sensitive = (attachment != NULL);
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK, sensitive);

	text = NULL;
	if (attachment != NULL)
		text = e_attachment_get_filename (attachment);
	text = (text != NULL) ? text : "";
	widget = dialog->priv->filename_entry;
	gtk_widget_set_sensitive (widget, sensitive);
	gtk_entry_set_text (GTK_ENTRY (widget), text);

	text = NULL;
	if (attachment != NULL)
		text = e_attachment_get_description (attachment);
	text = (text != NULL) ? text : "";
	widget = dialog->priv->description_entry;
	gtk_widget_set_sensitive (widget, sensitive);
	gtk_entry_set_text (GTK_ENTRY (widget), text);

	text = NULL;
	if (attachment != NULL)
		text = e_attachment_get_mime_type (attachment);
	text = (text != NULL) ? text : "";
	widget = dialog->priv->mime_type_label;
	gtk_label_set_text (GTK_LABEL (widget), text);

	active = FALSE;
	if (mime_part != NULL) {
		const gchar *disposition;

		disposition = camel_mime_part_get_disposition (mime_part);
		active = (g_ascii_strcasecmp (disposition, "inline") == 0);
	} else if (attachment != NULL)
		active = e_attachment_is_inline (attachment);
	widget = dialog->priv->disposition_checkbox;
	gtk_widget_set_sensitive (widget, sensitive);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);
}

static void
attachment_dialog_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ATTACHMENT:
			e_attachment_dialog_set_attachment (
				E_ATTACHMENT_DIALOG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_dialog_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ATTACHMENT:
			g_value_set_object (
				value, e_attachment_dialog_get_attachment (
				E_ATTACHMENT_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_dialog_dispose (GObject *object)
{
	EAttachmentDialogPrivate *priv;

	priv = E_ATTACHMENT_DIALOG_GET_PRIVATE (object);

	if (priv->attachment != NULL) {
		g_object_unref (priv->attachment);
		priv->attachment = NULL;
	}

	if (priv->filename_entry != NULL) {
		g_object_unref (priv->filename_entry);
		priv->filename_entry = NULL;
	}

	if (priv->description_entry != NULL) {
		g_object_unref (priv->description_entry);
		priv->description_entry = NULL;
	}

	if (priv->mime_type_label != NULL) {
		g_object_unref (priv->mime_type_label);
		priv->mime_type_label = NULL;
	}

	if (priv->disposition_checkbox != NULL) {
		g_object_unref (priv->disposition_checkbox);
		priv->disposition_checkbox = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
attachment_dialog_map (GtkWidget *widget)
{
	GtkWidget *action_area;
	GtkWidget *content_area;

	/* Chain up to parent's map() method. */
	GTK_WIDGET_CLASS (parent_class)->map (widget);

	/* XXX Override GtkDialog's broken style property defaults. */
	action_area = gtk_dialog_get_action_area (GTK_DIALOG (widget));
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (widget));

	gtk_box_set_spacing (GTK_BOX (content_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 0);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);
}

static void
attachment_dialog_response (GtkDialog *dialog,
                            gint response_id)
{
	EAttachmentDialogPrivate *priv;
	EAttachment *attachment;
	GtkToggleButton *button;
	GtkEntry *entry;
	const gchar *text;
	gboolean active;

	if (response_id != GTK_RESPONSE_OK)
		return;

	priv = E_ATTACHMENT_DIALOG_GET_PRIVATE (dialog);
	g_return_if_fail (priv->attachment != NULL);
	attachment = priv->attachment;

	entry = GTK_ENTRY (priv->filename_entry);
	text = gtk_entry_get_text (entry);
	e_attachment_set_filename (attachment, text);

	entry = GTK_ENTRY (priv->description_entry);
	text = gtk_entry_get_text (entry);
	e_attachment_set_description (attachment, text);

	button = GTK_TOGGLE_BUTTON (priv->disposition_checkbox);
	active = gtk_toggle_button_get_active (button);
	text = active ? "inline" : "attachment";
	e_attachment_set_disposition (attachment, text);
}

static void
attachment_dialog_class_init (EAttachmentDialogClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkDialogClass *dialog_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAttachmentDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_dialog_set_property;
	object_class->get_property = attachment_dialog_get_property;
	object_class->dispose = attachment_dialog_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = attachment_dialog_map;

	dialog_class = GTK_DIALOG_CLASS (class);
	dialog_class->response = attachment_dialog_response;

	g_object_class_install_property (
		object_class,
		PROP_ATTACHMENT,
		g_param_spec_object (
			"attachment",
			"Attachment",
			NULL,
			E_TYPE_ATTACHMENT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
attachment_dialog_init (EAttachmentDialog *dialog)
{
	GtkWidget *container;
	GtkWidget *widget;

	dialog->priv = E_ATTACHMENT_DIALOG_GET_PRIVATE (dialog);

	gtk_dialog_add_button (
		GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (
		GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_window_set_icon_name (
		GTK_WINDOW (dialog), "mail-attachment");
	gtk_window_set_title (
		GTK_WINDOW (dialog), _("Attachment Properties"));

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	widget = gtk_table_new (4, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 6);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 6);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	dialog->priv->filename_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("_Filename:"));
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->filename_entry);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	widget = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	dialog->priv->description_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("_Description:"));
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->description_entry);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_selectable (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	dialog->priv->mime_type_label = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new (_("MIME Type:"));
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	widget = gtk_check_button_new_with_mnemonic (
		_("_Suggest automatic display of attachment"));
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 2, 3, 4, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	dialog->priv->disposition_checkbox = g_object_ref (widget);
	gtk_widget_show (widget);
}

GType
e_attachment_dialog_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EAttachmentDialogClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) attachment_dialog_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_init */
			sizeof (EAttachmentDialog),
			0,     /* n_preallocs */
			(GInstanceInitFunc) attachment_dialog_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_DIALOG, "EAttachmentDialog", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_attachment_dialog_new (GtkWindow *parent,
                         EAttachment *attachment)
{
	if (parent != NULL)
	 	g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
	if (attachment != NULL)
		g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	return g_object_new (
		E_TYPE_ATTACHMENT_DIALOG,
		"transient-for", parent, "attachment", attachment, NULL);
}

EAttachment *
e_attachment_dialog_get_attachment (EAttachmentDialog *dialog)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_DIALOG (dialog), NULL);

	return dialog->priv->attachment;
}

void
e_attachment_dialog_set_attachment (EAttachmentDialog *dialog,
                                    EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT_DIALOG (dialog));

	if (attachment != NULL) {
		g_return_if_fail (E_IS_ATTACHMENT (attachment));
		g_object_ref (attachment);
	}

	if (dialog->priv->attachment != NULL)
		g_object_unref (dialog->priv->attachment);

	dialog->priv->attachment = attachment;

	attachment_dialog_update (dialog);

	g_object_notify (G_OBJECT (dialog), "attachment");
}
