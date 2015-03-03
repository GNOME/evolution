/*
 * e-html-editor-find-dialog.h
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

#include "e-html-editor-find-dialog.h"
#include "e-dialog-widgets.h"

#include <webkit2/webkit2.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#define E_HTML_EDITOR_FIND_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_FIND_DIALOG, EHTMLEditorFindDialogPrivate))

struct _EHTMLEditorFindDialogPrivate {
	GtkWidget *entry;
	GtkWidget *backwards;
	GtkWidget *case_sensitive;
	GtkWidget *wrap_search;

	GtkWidget *find_button;

	GtkWidget *result_label;

	WebKitFindController *find_controller;
	gulong found_text_handler_id;
	gulong failed_to_find_text_handler_id;
};

G_DEFINE_TYPE (
	EHTMLEditorFindDialog,
	e_html_editor_find_dialog,
	E_TYPE_HTML_EDITOR_DIALOG);

static void
reset_dialog (EHTMLEditorFindDialog *dialog)
{
	gtk_widget_set_sensitive (dialog->priv->find_button, TRUE);
	gtk_widget_hide (dialog->priv->result_label);
}

static void
html_editor_find_dialog_show (GtkWidget *widget)
{
	EHTMLEditorFindDialog *dialog = E_HTML_EDITOR_FIND_DIALOG (widget);

	reset_dialog (dialog);
	gtk_widget_grab_focus (dialog->priv->entry);

	/* Chain up to parent's implementation */
	GTK_WIDGET_CLASS (e_html_editor_find_dialog_parent_class)->show (widget);
}

static void
webkit_find_controller_found_text_cb (WebKitFindController *find_controller,
                                      guint match_count,
                                      EHTMLEditorFindDialog *dialog)
{
	gtk_widget_set_sensitive (dialog->priv->find_button, TRUE);

	/* We give focus to WebKit so that the selection is highlited.
	 * Without focus selection is not visible (at least with my default
	 * color scheme). The focus in fact is not given to WebKit, because
	 * this dialog is modal, but it satisfies it in a way that it paints
	 * the selection :) */
	/* FIXME WK2 - still needed ?
	gtk_widget_grab_focus (GTK_WIDGET (view)); */
}

static void
webkit_find_controller_failed_to_found_text_cb (WebKitFindController *find_controller,
                                                EHTMLEditorFindDialog *dialog)
{
	gtk_label_set_label (
		GTK_LABEL (dialog->priv->result_label), N_("No match found"));
	gtk_widget_show (dialog->priv->result_label);
}

static void
html_editor_find_dialog_find_cb (EHTMLEditorFindDialog *dialog)
{
	guint32 flags = 0;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->case_sensitive)))
		flags |= WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->backwards)))
		flags |= WEBKIT_FIND_OPTIONS_BACKWARDS;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->wrap_search)))
		flags |= WEBKIT_FIND_OPTIONS_WRAP_AROUND;

	webkit_find_controller_search (
		dialog->priv->find_controller,
		gtk_entry_get_text (GTK_ENTRY (dialog->priv->entry)),
		flags,
		G_MAXUINT);
}

static gboolean
entry_key_release_event (GtkWidget *widget,
                         GdkEvent *event,
                         gpointer user_data)
{
	GdkEventKey *key = &event->key;
	EHTMLEditorFindDialog *dialog = user_data;

	if (key->keyval == GDK_KEY_Return) {
		html_editor_find_dialog_find_cb (dialog);
		return TRUE;
	}

	reset_dialog (dialog);
	return FALSE;
}

static void
html_editor_find_dialog_dispose (GObject *object)
{
	EHTMLEditorFindDialogPrivate *priv;

	priv = E_HTML_EDITOR_FIND_DIALOG_GET_PRIVATE (object);

	if (priv->found_text_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->find_controller,
			priv->found_text_handler_id);
		priv->found_text_handler_id = 0;
	}

	if (priv->failed_to_find_text_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->find_controller,
			priv->failed_to_find_text_handler_id);
		priv->failed_to_find_text_handler_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_html_editor_find_dialog_parent_class)->dispose (object);
}

static void
e_html_editor_find_dialog_class_init (EHTMLEditorFindDialogClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorFindDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = html_editor_find_dialog_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_find_dialog_show;
}

static void
e_html_editor_find_dialog_init (EHTMLEditorFindDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GtkGrid *main_layout;
	GtkBox *box;
	GtkWidget *widget;
	WebKitFindController *find_controller;

	dialog->priv = E_HTML_EDITOR_FIND_DIALOG_GET_PRIVATE (dialog);

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	find_controller =
		webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (view));

	dialog->priv->found_text_handler_id = g_signal_connect (
		find_controller, "found-text",
		G_CALLBACK (webkit_find_controller_found_text_cb), dialog);

	dialog->priv->failed_to_find_text_handler_id = g_signal_connect (
		find_controller, "failed-to-find-text",
		G_CALLBACK (webkit_find_controller_failed_to_found_text_cb), dialog);

	dialog->priv->find_controller = find_controller;

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);
	dialog->priv->entry = widget;
	g_signal_connect (
		widget, "key-release-event",
		G_CALLBACK (entry_key_release_event), dialog);

	box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5));
	gtk_grid_attach (main_layout, GTK_WIDGET (box), 0, 1, 1, 1);

	widget = gtk_check_button_new_with_mnemonic (N_("Search _backwards"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	dialog->priv->backwards = widget;
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (reset_dialog), dialog);

	widget = gtk_check_button_new_with_mnemonic (N_("Case _Sensitive"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	dialog->priv->case_sensitive = widget;
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (reset_dialog), dialog);

	widget = gtk_check_button_new_with_mnemonic (N_("_Wrap Search"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	dialog->priv->wrap_search = widget;
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (reset_dialog), dialog);

	box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5));
	gtk_grid_attach (main_layout, GTK_WIDGET (box), 0, 2, 1, 1);

	widget = gtk_label_new ("");
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	dialog->priv->result_label = widget;

	widget = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_set_spacing (GTK_BOX (widget), 5);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_END);
	gtk_box_pack_end (box, widget, TRUE, TRUE, 0);
	box = GTK_BOX (widget);

	box = e_html_editor_dialog_get_button_box (E_HTML_EDITOR_DIALOG (dialog));
	widget = e_dialog_button_new_with_icon ("edit-find", _("_Find"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 5);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_find_dialog_find_cb), dialog);
	dialog->priv->find_button = widget;

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_html_editor_find_dialog_new (EHTMLEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_HTML_EDITOR_FIND_DIALOG,
			"editor", editor,
			"icon-name", "edit-find",
			"title", _("Find"),
			NULL));
}

void
e_html_editor_find_dialog_find_next (EHTMLEditorFindDialog *dialog)
{
	if (gtk_entry_get_text_length (GTK_ENTRY (dialog->priv->entry)) == 0)
		return;

	html_editor_find_dialog_find_cb (dialog);
}
