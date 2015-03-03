/*
 * e-html-editor-replace-dialog.h
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

#include "e-html-editor-replace-dialog.h"

#include <webkit2/webkit2.h>
#include <glib/gi18n-lib.h>

#define E_HTML_EDITOR_REPLACE_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_REPLACE_DIALOG, EHTMLEditorReplaceDialogPrivate))

G_DEFINE_TYPE (
	EHTMLEditorReplaceDialog,
	e_html_editor_replace_dialog,
	E_TYPE_HTML_EDITOR_DIALOG);

struct _EHTMLEditorReplaceDialogPrivate {
	GtkWidget *search_entry;
	GtkWidget *replace_entry;

	GtkWidget *case_sensitive;
	GtkWidget *backwards;
	GtkWidget *wrap;

	GtkWidget *result_label;

	GtkWidget *skip_button;
	GtkWidget *replace_button;
	GtkWidget *replace_all_button;

	EHTMLEditor *editor;

	gboolean skip;

	WebKitFindController *find_controller;
	gulong found_text_handler_id;
	gulong failed_to_find_text_handler_id;
	gulong counted_matches_handler_id;
};

enum {
	PROP_0,
	PROP_EDITOR
};

static void
webkit_find_controller_found_text_cb (WebKitFindController *find_controller,
                                      guint match_count,
                                      EHTMLEditorReplaceDialog *dialog)
{
	EHTMLEditorSelection *selection;

	selection = e_html_editor_view_get_selection (
		E_HTML_EDITOR_VIEW (webkit_find_controller_get_web_view (find_controller)));

	gtk_widget_hide (dialog->priv->result_label);

	if (!dialog->priv->skip) {
		e_html_editor_selection_replace (
			selection,
			gtk_entry_get_text (GTK_ENTRY (dialog->priv->replace_entry)));
	}

	dialog->priv->skip = FALSE;
}

static void
webkit_find_controller_failed_to_found_text_cb (WebKitFindController *find_controller,
                                                EHTMLEditorReplaceDialog *dialog)
{
	gtk_label_set_label (
		GTK_LABEL (dialog->priv->result_label), N_("No match found"));
	gtk_widget_show (dialog->priv->result_label);
}

static void
webkit_find_controller_counted_matches_cb (WebKitFindController *find_controller,
                                           guint match_count,
                                           EHTMLEditorReplaceDialog *dialog)
{
	gchar *result;
	guint ii = 0;

	for (ii = 0; ii < match_count; ii++) {
		webkit_find_controller_search_next (dialog->priv->find_controller);
		/* Jump behind the word */
		/* FIXME WK2 is it needed ?
		e_html_editor_selection_move (
			selection, TRUE, E_HTML_EDITOR_SELECTION_GRANULARITY_WORD);*/
	}

	result = g_strdup_printf (ngettext("%d occurence replaced",
					   "%d occurences replaced",
					   match_count),
				 match_count);
	gtk_label_set_label (GTK_LABEL (dialog->priv->result_label), result);
	gtk_widget_show (dialog->priv->result_label);
	g_free (result);

}

static void
search (EHTMLEditorReplaceDialog *dialog)
{
	guint32 flags = 0;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->case_sensitive)))
		flags |= WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->backwards)))
		flags |= WEBKIT_FIND_OPTIONS_BACKWARDS;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->wrap)))
		flags |= WEBKIT_FIND_OPTIONS_WRAP_AROUND;

	webkit_find_controller_search (
		dialog->priv->find_controller,
		gtk_entry_get_text (GTK_ENTRY (dialog->priv->search_entry)),
		flags,
		G_MAXUINT);
}

static void
html_editor_replace_dialog_skip_cb (EHTMLEditorReplaceDialog *dialog)
{
	dialog->priv->skip = TRUE;
	webkit_find_controller_search_next (dialog->priv->find_controller);
}

static void
html_editor_replace_dialog_replace_cb (EHTMLEditorReplaceDialog *dialog)
{
	dialog->priv->skip = FALSE;
	/* Jump to next matching word */
	webkit_find_controller_search_next (dialog->priv->find_controller);
}

static void
html_editor_replace_dialog_replace_all_cb (EHTMLEditorReplaceDialog *dialog)
{
	guint32 flags = 0;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->case_sensitive)))
		flags |= WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->backwards)))
		flags |= WEBKIT_FIND_OPTIONS_BACKWARDS;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->wrap)))
		flags |= WEBKIT_FIND_OPTIONS_WRAP_AROUND;

	webkit_find_controller_count_matches (
		dialog->priv->find_controller,
		gtk_entry_get_text (GTK_ENTRY (dialog->priv->search_entry)),
		flags,
		G_MAXUINT);
}

static void
html_editor_replace_dialog_entry_changed (EHTMLEditorReplaceDialog *dialog)
{
	gboolean ready;
	ready = ((gtk_entry_get_text_length (
			GTK_ENTRY (dialog->priv->search_entry)) != 0) &&
		 (gtk_entry_get_text_length (
			 GTK_ENTRY (dialog->priv->replace_entry)) != 0));

	gtk_widget_set_sensitive (dialog->priv->skip_button, ready);
	gtk_widget_set_sensitive (dialog->priv->replace_button, ready);
	gtk_widget_set_sensitive (dialog->priv->replace_all_button, ready);

	if (ready)
		search (dialog);
}

static void
html_editor_replace_dialog_show (GtkWidget *widget)
{
	EHTMLEditorReplaceDialog *dialog = E_HTML_EDITOR_REPLACE_DIALOG (widget);

	gtk_widget_grab_focus (dialog->priv->search_entry);
	gtk_widget_hide (dialog->priv->result_label);

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_replace_dialog_parent_class)->show (widget);
}

static void
html_editor_replace_dialog_hide (GtkWidget *widget)
{
	EHTMLEditorReplaceDialog *dialog = E_HTML_EDITOR_REPLACE_DIALOG (widget);

	webkit_find_controller_search_finish (dialog->priv->find_controller);

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_replace_dialog_parent_class)->hide (widget);
}

static void
html_editor_replace_dialog_dispose (GObject *object)
{
	EHTMLEditorReplaceDialogPrivate *priv;

	priv = E_HTML_EDITOR_REPLACE_DIALOG_GET_PRIVATE (object);

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

	if (priv->counted_matches_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->find_controller,
			priv->counted_matches_handler_id);
		priv->counted_matches_handler_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_html_editor_replace_dialog_parent_class)->dispose (object);
}

static void
e_html_editor_replace_dialog_class_init (EHTMLEditorReplaceDialogClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorReplaceDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = html_editor_replace_dialog_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_replace_dialog_show;
	widget_class->hide = html_editor_replace_dialog_hide;
}

static void
e_html_editor_replace_dialog_init (EHTMLEditorReplaceDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GtkGrid *main_layout;
	GtkWidget *widget, *layout;
	GtkBox *button_box;
	WebKitFindController *find_controller;

	dialog->priv = E_HTML_EDITOR_REPLACE_DIALOG_GET_PRIVATE (dialog);

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

	dialog->priv->counted_matches_handler_id = g_signal_connect (
		find_controller, "counted-matches",
		G_CALLBACK (webkit_find_controller_counted_matches_cb), dialog);

	dialog->priv->find_controller = find_controller;

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	widget = gtk_entry_new ();
	gtk_grid_attach (main_layout, widget, 1, 0, 2, 1);
	dialog->priv->search_entry = widget;
	g_signal_connect_swapped (
		widget, "notify::text-length",
		G_CALLBACK (html_editor_replace_dialog_entry_changed), dialog);

	widget = gtk_label_new_with_mnemonic (_("R_eplace:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->search_entry);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	widget = gtk_entry_new ();
	gtk_grid_attach (main_layout, widget, 1, 1, 2, 1);
	dialog->priv->replace_entry = widget;
	g_signal_connect_swapped (
		widget, "notify::text-length",
		G_CALLBACK (html_editor_replace_dialog_entry_changed), dialog);

	widget = gtk_label_new_with_mnemonic (_("_With:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->replace_entry);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_grid_attach (main_layout, widget, 0, 1, 1, 1);

	layout = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_grid_attach (main_layout, layout, 1, 2, 2, 1);

	widget = gtk_check_button_new_with_mnemonic (_("Search _backwards"));
	gtk_box_pack_start (GTK_BOX (layout), widget, FALSE, FALSE, 0);
	dialog->priv->backwards = widget;

	widget = gtk_check_button_new_with_mnemonic (_("_Case sensitive"));
	gtk_box_pack_start (GTK_BOX (layout), widget, FALSE, FALSE, 0);
	dialog->priv->case_sensitive = widget;

	widget = gtk_check_button_new_with_mnemonic (_("Wra_p search"));
	gtk_box_pack_start (GTK_BOX (layout), widget, FALSE, FALSE, 0);
	dialog->priv->wrap = widget;

	widget = gtk_label_new ("");
	gtk_grid_attach (main_layout, widget, 0, 3, 2, 1);
	dialog->priv->result_label = widget;

	button_box = e_html_editor_dialog_get_button_box (E_HTML_EDITOR_DIALOG (dialog));

	widget = gtk_button_new_with_mnemonic (_("_Skip"));
	gtk_box_pack_start (button_box, widget, FALSE, FALSE, 5);
	gtk_widget_set_sensitive (widget, FALSE);
	dialog->priv->skip_button = widget;
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_replace_dialog_skip_cb), dialog);

	widget = gtk_button_new_with_mnemonic (_("_Replace"));
	gtk_box_pack_start (button_box, widget, FALSE, FALSE, 5);
	gtk_widget_set_sensitive (widget, FALSE);
	dialog->priv->replace_button = widget;
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_replace_dialog_replace_cb), dialog);

	widget = gtk_button_new_with_mnemonic (_("Replace _All"));
	gtk_box_pack_start (button_box, widget, FALSE, FALSE, 5);
	gtk_widget_set_sensitive (widget, FALSE);
	dialog->priv->replace_all_button = widget;
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_replace_dialog_replace_all_cb), dialog);

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_html_editor_replace_dialog_new (EHTMLEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_HTML_EDITOR_REPLACE_DIALOG,
			"editor", editor,
			"icon-name", "edit-find-replace",
			"resizable", FALSE,
			"title", C_("dialog-title", "Replace"),
			"transient-for", gtk_widget_get_toplevel (GTK_WIDGET (editor)),
			"type", GTK_WINDOW_TOPLEVEL,
			"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
			NULL));
}
