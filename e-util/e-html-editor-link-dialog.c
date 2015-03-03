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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-html-editor-link-dialog.h"
#include "e-html-editor-selection.h"
#include "e-html-editor-view.h"

#include <glib/gi18n-lib.h>

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
	GtkWidget *test_button;

	GtkWidget *remove_link_button;
	GtkWidget *ok_button;

	gboolean label_autofill;
};

static void
html_editor_link_dialog_test_link (EHTMLEditorLinkDialog *dialog)
{
	gtk_show_uri (
		gtk_window_get_screen (GTK_WINDOW (dialog)),
		gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit)),
		GDK_CURRENT_TIME,
		NULL);
}

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
	EHTMLEditorView *view;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	e_html_editor_view_call_simple_extension_function (
		view, "EHTMLEditorSelectionDOMUnlink");

	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
html_editor_link_dialog_ok (EHTMLEditorLinkDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GDBusProxy *web_extension;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"EHTMLEditorLinkDialogOk",
		g_variant_new (
			"(tss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			gtk_entry_get_text (GTK_ENTRY (dialog->priv->url_edit)),
			gtk_entry_get_text (GTK_ENTRY (dialog->priv->label_edit))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	gtk_widget_hide (GTK_WIDGET (dialog));
}

static gboolean
html_editor_link_dialog_entry_key_pressed (EHTMLEditorLinkDialog *dialog,
                                           GdkEventKey *event)
{
	/* We can't do thins in key_released, because then you could not open
	 * this dialog from main menu by pressing enter on Insert->Link action */
	if (event->keyval == GDK_KEY_Return) {
		html_editor_link_dialog_ok (dialog);
		return TRUE;
	}

	return FALSE;
}

static void
html_editor_link_dialog_show (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorLinkDialog *dialog;
	EHTMLEditorView *view;
	GDBusProxy *web_extension;
	GVariant *result;

	dialog = E_HTML_EDITOR_LINK_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	/* Reset to default values */
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->url_edit), "http://");
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->label_edit), "");
	gtk_widget_set_sensitive (dialog->priv->label_edit, TRUE);
	gtk_widget_set_sensitive (dialog->priv->remove_link_button, TRUE);

	dialog->priv->label_autofill = TRUE;
	result = g_dbus_proxy_call_sync (
		web_extension,
		"EHTMLEditorLinkDialogShow",
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		const gchar *href, *inner_text;

		g_variant_get (result, "(&s&s)", &href, &inner_text);

		if (href && *href) {
			gtk_entry_set_text (
				GTK_ENTRY (dialog->priv->url_edit), href);
		}

		if (inner_text && *inner_text) {
			gtk_widget_set_sensitive (
				dialog->priv->label_edit, TRUE);
			if (!href || !*href) {
				gtk_widget_set_sensitive (
					dialog->priv->label_edit, FALSE);
				gtk_widget_set_sensitive (
					dialog->priv->remove_link_button, FALSE);
			}
		}
		g_variant_unref (result);
	}

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
}

static void
e_html_editor_link_dialog_init (EHTMLEditorLinkDialog *dialog)
{
	GtkGrid *main_layout;
	GtkBox *button_box;
	GtkWidget *widget;

	dialog->priv = E_HTML_EDITOR_LINK_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	widget = gtk_entry_new ();
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

	widget = gtk_button_new_with_mnemonic (_("_Test URL..."));
	gtk_grid_attach (main_layout, widget, 2, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_link_dialog_test_link), dialog);
	dialog->priv->test_button = widget;

	widget = gtk_entry_new ();
	gtk_grid_attach (main_layout, widget, 1, 1, 2, 1);
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
