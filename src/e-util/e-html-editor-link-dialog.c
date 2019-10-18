/*
 * e-html-editor-link-dialog.h
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
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
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-misc-utils.h"
#include "e-url-entry.h"

#include "e-html-editor-link-dialog.h"

#define E_HTML_EDITOR_LINK_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_LINK_DIALOG, EHTMLEditorLinkDialogPrivate))

G_DEFINE_TYPE (
	EHTMLEditorLinkDialog,
	e_html_editor_link_dialog,
	E_TYPE_HTML_EDITOR_DIALOG);

struct _EHTMLEditorLinkDialogPrivate {
	GtkWidget *url_edit;
	GtkWidget *label_edit;

	GtkWidget *remove_link_button;
	GtkWidget *ok_button;

	gboolean label_autofill;
};

static void
html_editor_link_dialog_url_changed (EHTMLEditorLinkDialog *dialog)
{
	if (dialog->priv->label_autofill &&
	    gtk_widget_is_sensitive (dialog->priv->label_edit)) {
		const gchar *text;

		text = gtk_entry_get_text (
			GTK_ENTRY (dialog->priv->url_edit));
		gtk_entry_set_text (
			GTK_ENTRY (dialog->priv->label_edit), text);
	}
}

static gboolean
html_editor_link_dialog_description_changed (EHTMLEditorLinkDialog *dialog)
{
	const gchar *text;

	text = gtk_entry_get_text (GTK_ENTRY (dialog->priv->label_edit));
	dialog->priv->label_autofill = (*text == '\0');

	return FALSE;
}

static void
html_editor_link_dialog_remove_link (EHTMLEditorLinkDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_selection_unlink (cnt_editor);

	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
html_editor_link_dialog_ok (EHTMLEditorLinkDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_link_set_properties (
		cnt_editor,
		gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit)),
		gtk_entry_get_text (GTK_ENTRY (dialog->priv->label_edit)));

	gtk_widget_hide (GTK_WIDGET (dialog));
}

static gboolean
html_editor_link_dialog_entry_key_pressed (EHTMLEditorLinkDialog *dialog,
                                           GdkEventKey *event)
{
	/* We can't do things in key_released, because then you could not open
	 * this dialog from main menu by pressing enter on Insert->Link action */
	if (event->keyval == GDK_KEY_Return) {
		html_editor_link_dialog_ok (dialog);
		return TRUE;
	}

	return FALSE;
}

static void
html_editor_link_dialog_hide (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorLinkDialog *dialog;
	EContentEditor *cnt_editor;

	dialog = E_HTML_EDITOR_LINK_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_on_dialog_close (cnt_editor, E_CONTENT_EDITOR_DIALOG_LINK);

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_link_dialog_parent_class)->hide (widget);
}

static void
html_editor_link_dialog_show (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorLinkDialog *dialog;
	EContentEditor *cnt_editor;
	gchar *href = NULL, *text = NULL;

	dialog = E_HTML_EDITOR_LINK_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	/* Reset to default values */
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->url_edit), "http://");
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->label_edit), "");
	gtk_widget_set_sensitive (dialog->priv->label_edit, TRUE);
	gtk_widget_set_sensitive (dialog->priv->remove_link_button, TRUE);

	dialog->priv->label_autofill = TRUE;

	e_content_editor_on_dialog_open (cnt_editor, E_CONTENT_EDITOR_DIALOG_LINK);

	e_content_editor_link_get_properties (cnt_editor, &href, &text);
	if (href && *href)
		gtk_entry_set_text (GTK_ENTRY (dialog->priv->url_edit), href);
	else
		gtk_widget_set_sensitive (dialog->priv->remove_link_button, FALSE);

	g_free (href);

	if (text && *text) {
		gtk_entry_set_text (GTK_ENTRY (dialog->priv->label_edit), text);
		dialog->priv->label_autofill = FALSE;
	}
	g_free (text);

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_link_dialog_parent_class)->show (widget);
}

static void
e_html_editor_link_dialog_class_init (EHTMLEditorLinkDialogClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorLinkDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_link_dialog_show;
	widget_class->hide = html_editor_link_dialog_hide;
}

static void
e_html_editor_link_dialog_init (EHTMLEditorLinkDialog *dialog)
{
	GtkGrid *main_layout;
	GtkBox *button_box;
	GtkWidget *widget;

	dialog->priv = E_HTML_EDITOR_LINK_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	widget = e_url_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (main_layout, widget, 1, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "notify::text",
		G_CALLBACK (html_editor_link_dialog_url_changed), dialog);
	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (html_editor_link_dialog_entry_key_pressed), dialog);
	dialog->priv->url_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_URL:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->url_edit);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (main_layout, widget, 1, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "key-release-event",
		G_CALLBACK (html_editor_link_dialog_description_changed), dialog);
	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (html_editor_link_dialog_entry_key_pressed), dialog);
	dialog->priv->label_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_Description:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->label_edit);
	gtk_grid_attach (main_layout, widget, 0, 1, 1, 1);

	button_box = e_html_editor_dialog_get_button_box (E_HTML_EDITOR_DIALOG (dialog));

	widget = gtk_button_new_with_mnemonic (_("_Remove Link"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_link_dialog_remove_link), dialog);
	gtk_box_pack_start (button_box, widget, FALSE, FALSE, 5);
	dialog->priv->remove_link_button = widget;

	widget = gtk_button_new_with_mnemonic (_("_OK"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_link_dialog_ok), dialog);
	gtk_box_pack_end (button_box, widget, FALSE, FALSE, 5);
	dialog->priv->ok_button = widget;

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_html_editor_link_dialog_new (EHTMLEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_HTML_EDITOR_LINK_DIALOG,
			"editor", editor,
			"icon-name", "insert-link",
			"title", _("Link Properties"),
			NULL));
}
