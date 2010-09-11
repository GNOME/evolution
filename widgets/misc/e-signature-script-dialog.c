/*
 * e-signature-script-dialog.c
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

#include "e-signature-script-dialog.h"

#include <glib/gi18n.h>
#include "e-util/e-binding.h"

#define E_SIGNATURE_SCRIPT_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SIGNATURE_SCRIPT_DIALOG, ESignatureScriptDialogPrivate))

struct _ESignatureScriptDialogPrivate {
	GtkWidget *entry;
	GtkWidget *file_chooser;
	GtkWidget *alert;
};

enum {
	PROP_0,
	PROP_SCRIPT_FILE,
	PROP_SCRIPT_NAME
};

G_DEFINE_TYPE (
	ESignatureScriptDialog,
	e_signature_script_dialog,
	GTK_TYPE_DIALOG)

static gboolean
signature_script_dialog_filter_cb (const GtkFileFilterInfo *filter_info)
{
	const gchar *filename = filter_info->filename;

	return g_file_test (filename, G_FILE_TEST_IS_EXECUTABLE);
}

static void
signature_script_dialog_update_status (ESignatureScriptDialog *dialog)
{
	GFile *script_file;
	const gchar *script_name;
	gboolean show_alert;
	gboolean sensitive;

	script_file = e_signature_script_dialog_get_script_file (dialog);
	script_name = e_signature_script_dialog_get_script_name (dialog);

	sensitive = (script_name != NULL && *script_name != '\0');

	if (script_file != NULL) {
		gboolean executable;
		gchar *filename;

		filename = g_file_get_path (script_file);
		executable = g_file_test (filename, G_FILE_TEST_IS_EXECUTABLE);
		g_free (filename);

		show_alert = !executable;
		sensitive &= executable;

		g_object_unref (script_file);
	} else {
		sensitive = FALSE;
		show_alert = FALSE;
	}

	if (show_alert)
		gtk_widget_show (dialog->priv->alert);
	else
		gtk_widget_hide (dialog->priv->alert);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK, sensitive);
}

static void
signature_script_dialog_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SCRIPT_FILE:
			e_signature_script_dialog_set_script_file (
				E_SIGNATURE_SCRIPT_DIALOG (object),
				g_value_get_object (value));
			return;

		case PROP_SCRIPT_NAME:
			e_signature_script_dialog_set_script_name (
				E_SIGNATURE_SCRIPT_DIALOG (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_script_dialog_get_property (GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SCRIPT_FILE:
			g_value_set_object (
				value,
				e_signature_script_dialog_get_script_file (
				E_SIGNATURE_SCRIPT_DIALOG (object)));
			return;

		case PROP_SCRIPT_NAME:
			g_value_set_string (
				value,
				e_signature_script_dialog_get_script_name (
				E_SIGNATURE_SCRIPT_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_script_dialog_dispose (GObject *object)
{
	ESignatureScriptDialogPrivate *priv;

	priv = E_SIGNATURE_SCRIPT_DIALOG_GET_PRIVATE (object);

	if (priv->entry != NULL) {
		g_object_unref (priv->entry);
		priv->entry = NULL;
	}

	if (priv->file_chooser != NULL) {
		g_object_unref (priv->file_chooser);
		priv->file_chooser = NULL;
	}

	if (priv->alert != NULL) {
		g_object_unref (priv->alert);
		priv->alert = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_signature_script_dialog_parent_class)->dispose (object);
}

static void
signature_script_dialog_map (GtkWidget *widget)
{
	GtkWidget *action_area;
	GtkWidget *content_area;

	/* Chain up to parent's map() method. */
	GTK_WIDGET_CLASS (e_signature_script_dialog_parent_class)->map (widget);

	/* XXX Override GtkDialog's broken style property defaults. */
	action_area = gtk_dialog_get_action_area (GTK_DIALOG (widget));
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (widget));

	gtk_box_set_spacing (GTK_BOX (content_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 0);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);
}

static void
e_signature_script_dialog_class_init (ESignatureScriptDialogClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (ESignatureScriptDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = signature_script_dialog_set_property;
	object_class->get_property = signature_script_dialog_get_property;
	object_class->dispose = signature_script_dialog_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = signature_script_dialog_map;

	g_object_class_install_property (
		object_class,
		PROP_SCRIPT_FILE,
		g_param_spec_object (
			"script-file",
			"Script File",
			NULL,
			G_TYPE_FILE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SCRIPT_NAME,
		g_param_spec_string (
			"script-name",
			"Script Name",
			NULL,
			_("Unnamed"),
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
e_signature_script_dialog_init (ESignatureScriptDialog *dialog)
{
	GtkFileFilter *filter;
	GtkWidget *content_area;
	GtkWidget *container;
	GtkWidget *widget;
	gchar *markup;

	dialog->priv = E_SIGNATURE_SCRIPT_DIALOG_GET_PRIVATE (dialog);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	gtk_dialog_add_button (
		GTK_DIALOG (dialog),
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	gtk_dialog_add_button (
		GTK_DIALOG (dialog),
		GTK_STOCK_SAVE, GTK_RESPONSE_OK);

#if !GTK_CHECK_VERSION(2,90,7)
	g_object_set (dialog, "has-separator", FALSE, NULL);
#endif
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	container = content_area;

	widget = gtk_table_new (4, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 6);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 6);
	gtk_table_set_row_spacing (GTK_TABLE (widget), 0, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_stock (
		GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 0, 1, 0, 0, 0, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (_(
		"The output of this script will be used as your\n"
		"signature. The name you specify will be used\n"
		"for display purposes only."));
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show (widget);

	widget = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	dialog->priv->entry = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("_Name:"));
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->entry);
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	widget = gtk_file_chooser_button_new (
		NULL, GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	dialog->priv->file_chooser = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Restrict file selection to executable files. */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_custom (
		filter, GTK_FILE_FILTER_FILENAME,
		(GtkFileFilterFunc) signature_script_dialog_filter_cb,
		NULL, NULL);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (widget), filter);

	/* XXX ESignature stores a filename instead of a URI,
	 *     so we have to restrict it to local files only. */
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (widget), TRUE);

	widget = gtk_label_new_with_mnemonic (_("S_cript:"));
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->file_chooser);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	/* This is just a placeholder. */
	widget = gtk_label_new (NULL);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	widget = gtk_hbox_new (FALSE, 6);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 3, 4, 0, 0, 0, 0);
	dialog->priv->alert = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_stock (
		GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	markup = g_markup_printf_escaped (
		"<small>%s</small>",
		_("Script file must be executable."));
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	g_signal_connect (
		dialog, "notify::script-file",
		G_CALLBACK (signature_script_dialog_update_status), NULL);

	g_signal_connect (
		dialog, "notify::script-name",
		G_CALLBACK (signature_script_dialog_update_status), NULL);

	g_signal_connect_swapped (
		dialog->priv->entry, "changed",
		G_CALLBACK (signature_script_dialog_update_status), dialog);

	g_signal_connect_swapped (
		dialog->priv->file_chooser, "file-set",
		G_CALLBACK (signature_script_dialog_update_status), dialog);

	signature_script_dialog_update_status (dialog);
}

GtkWidget *
e_signature_script_dialog_new (GtkWindow *parent)
{
	return g_object_new (
		E_TYPE_SIGNATURE_SCRIPT_DIALOG,
		"transient-for", parent, NULL);
}

GFile *
e_signature_script_dialog_get_script_file (ESignatureScriptDialog *dialog)
{
	GtkFileChooser *file_chooser;

	g_return_val_if_fail (E_IS_SIGNATURE_SCRIPT_DIALOG (dialog), NULL);

	file_chooser = GTK_FILE_CHOOSER (dialog->priv->file_chooser);

	return gtk_file_chooser_get_file (file_chooser);
}

void
e_signature_script_dialog_set_script_file (ESignatureScriptDialog *dialog,
                                           GFile *script_file)
{
	GtkFileChooser *file_chooser;
	GError *error = NULL;

	g_return_if_fail (E_IS_SIGNATURE_SCRIPT_DIALOG (dialog));
	g_return_if_fail (G_IS_FILE (script_file));

	file_chooser = GTK_FILE_CHOOSER (dialog->priv->file_chooser);

	if (gtk_file_chooser_set_file (file_chooser, script_file, &error))
		g_object_notify (G_OBJECT (dialog), "script-file");
	else {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

const gchar *
e_signature_script_dialog_get_script_name (ESignatureScriptDialog *dialog)
{
	GtkEntry *entry;

	g_return_val_if_fail (E_IS_SIGNATURE_SCRIPT_DIALOG (dialog), NULL);

	entry = GTK_ENTRY (dialog->priv->entry);

	return gtk_entry_get_text (entry);
}

void
e_signature_script_dialog_set_script_name (ESignatureScriptDialog *dialog,
                                           const gchar *script_name)
{
	GtkEntry *entry;

	g_return_if_fail (E_IS_SIGNATURE_SCRIPT_DIALOG (dialog));

	if (script_name == NULL)
		script_name = "";

	entry = GTK_ENTRY (dialog->priv->entry);
	gtk_entry_set_text (entry, script_name);

	g_object_notify (G_OBJECT (dialog), "script-name");
}
