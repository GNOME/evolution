/*
 * e-attachment-dialog.c
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

#include "e-attachment-dialog.h"

#include <glib/gi18n.h>

#define E_ATTACHMENT_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_DIALOG, EAttachmentDialogPrivate))

struct _EAttachmentDialogPrivate {
	EAttachment *attachment;
	GtkWidget *display_name_entry;
	GtkWidget *description_entry;
	GtkWidget *content_type_label;
	GtkWidget *disposition_checkbox;
};

enum {
	PROP_0,
	PROP_ATTACHMENT
};

G_DEFINE_TYPE (
	EAttachmentDialog,
	e_attachment_dialog,
	GTK_TYPE_DIALOG)

static void
attachment_dialog_update (EAttachmentDialog *dialog)
{
	EAttachment *attachment;
	GFileInfo *file_info;
	GtkWidget *widget;
	const gchar *content_type;
	const gchar *display_name;
	gchar *description;
	gchar *disposition;
	gchar *type_description = NULL;
	gboolean sensitive;
	gboolean active;

	attachment = e_attachment_dialog_get_attachment (dialog);

	if (attachment != NULL) {
		file_info = e_attachment_ref_file_info (attachment);
		description = e_attachment_dup_description (attachment);
		disposition = e_attachment_dup_disposition (attachment);
	} else {
		file_info = NULL;
		description = NULL;
		disposition = NULL;
	}

	if (file_info != NULL) {
		content_type = g_file_info_get_content_type (file_info);
		display_name = g_file_info_get_display_name (file_info);
	} else {
		content_type = NULL;
		display_name = NULL;
	}

	if (content_type != NULL) {
		gchar *comment;
		gchar *mime_type;

		comment = g_content_type_get_description (content_type);
		mime_type = g_content_type_get_mime_type (content_type);

		type_description =
			g_strdup_printf ("%s (%s)", comment, mime_type);

		g_free (comment);
		g_free (mime_type);
	}

	sensitive = G_IS_FILE_INFO (file_info);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK, sensitive);

	widget = dialog->priv->display_name_entry;
	gtk_widget_set_sensitive (widget, sensitive);
	if (display_name != NULL)
		gtk_entry_set_text (GTK_ENTRY (widget), display_name);

	widget = dialog->priv->description_entry;
	gtk_widget_set_sensitive (widget, sensitive);
	if (description != NULL)
		gtk_entry_set_text (GTK_ENTRY (widget), description);

	widget = dialog->priv->content_type_label;
	gtk_label_set_text (GTK_LABEL (widget), type_description);

	active = (g_strcmp0 (disposition, "inline") == 0);
	widget = dialog->priv->disposition_checkbox;
	gtk_widget_set_sensitive (widget, sensitive);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);

	g_free (description);
	g_free (disposition);
	g_free (type_description);

	g_clear_object (&file_info);
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

	if (priv->display_name_entry != NULL) {
		g_object_unref (priv->display_name_entry);
		priv->display_name_entry = NULL;
	}

	if (priv->description_entry != NULL) {
		g_object_unref (priv->description_entry);
		priv->description_entry = NULL;
	}

	if (priv->content_type_label != NULL) {
		g_object_unref (priv->content_type_label);
		priv->content_type_label = NULL;
	}

	if (priv->disposition_checkbox != NULL) {
		g_object_unref (priv->disposition_checkbox);
		priv->disposition_checkbox = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_dialog_parent_class)->dispose (object);
}

static void
attachment_dialog_map (GtkWidget *widget)
{
	GtkWidget *action_area;
	GtkWidget *content_area;

	/* Chain up to parent's map() method. */
	GTK_WIDGET_CLASS (e_attachment_dialog_parent_class)->map (widget);

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
	GFileInfo *file_info;
	CamelMimePart *mime_part;
	const gchar *attribute;
	const gchar *text;
	gboolean active;

	if (response_id != GTK_RESPONSE_OK)
		return;

	priv = E_ATTACHMENT_DIALOG_GET_PRIVATE (dialog);
	g_return_if_fail (E_IS_ATTACHMENT (priv->attachment));
	attachment = priv->attachment;

	file_info = e_attachment_ref_file_info (attachment);
	g_return_if_fail (G_IS_FILE_INFO (file_info));

	mime_part = e_attachment_ref_mime_part (attachment);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME;
	text = gtk_entry_get_text (GTK_ENTRY (priv->display_name_entry));
	g_file_info_set_attribute_string (file_info, attribute, text);

	if (mime_part != NULL)
		camel_mime_part_set_filename (mime_part, text);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION;
	text = gtk_entry_get_text (GTK_ENTRY (priv->description_entry));
	g_file_info_set_attribute_string (file_info, attribute, text);

	if (mime_part != NULL)
		camel_mime_part_set_description (mime_part, text);

	button = GTK_TOGGLE_BUTTON (priv->disposition_checkbox);
	active = gtk_toggle_button_get_active (button);
	text = active ? "inline" : "attachment";
	e_attachment_set_disposition (attachment, text);

	if (mime_part != NULL)
		camel_mime_part_set_disposition (mime_part, text);

	g_clear_object (&file_info);
	g_clear_object (&mime_part);

	g_object_notify (G_OBJECT (attachment), "file-info");
}

static void
e_attachment_dialog_class_init (EAttachmentDialogClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkDialogClass *dialog_class;

	g_type_class_add_private (class, sizeof (EAttachmentDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_dialog_set_property;
	object_class->get_property = attachment_dialog_get_property;
	object_class->dispose = attachment_dialog_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = attachment_dialog_map;

	#if GTK_CHECK_VERSION (3, 20, 0)
	gtk_widget_class_set_css_name (widget_class, G_OBJECT_CLASS_NAME (class));
	#endif

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
e_attachment_dialog_init (EAttachmentDialog *dialog)
{
	GtkWidget *container;
	GtkWidget *widget;

	dialog->priv = E_ATTACHMENT_DIALOG_GET_PRIVATE (dialog);

	gtk_dialog_add_button (
		GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (
		GTK_DIALOG (dialog), _("_OK"), GTK_RESPONSE_OK);
	gtk_window_set_icon_name (
		GTK_WINDOW (dialog), "mail-attachment");
	gtk_window_set_title (
		GTK_WINDOW (dialog), _("Attachment Properties"));

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
	dialog->priv->display_name_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("F_ilename:"));
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->display_name_entry);
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
	dialog->priv->content_type_label = g_object_ref (widget);
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
