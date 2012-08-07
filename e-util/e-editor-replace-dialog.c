/*
 * e-editor-replace-dialog.h
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

#include "e-editor-replace-dialog.h"

#include <glib/gi18n-lib.h>

G_DEFINE_TYPE (
	EEditorReplaceDialog,
	e_editor_replace_dialog,
	E_TYPE_EDITOR_DIALOG);

struct _EEditorReplaceDialogPrivate {
	GtkWidget *search_entry;
	GtkWidget *replace_entry;

	GtkWidget *case_sensitive;
	GtkWidget *backwards;
	GtkWidget *wrap;

	GtkWidget *result_label;

	GtkWidget *close_button;
	GtkWidget *skip_button;
	GtkWidget *replace_button;
	GtkWidget *replace_all_button;
};

static gboolean
jump (EEditorReplaceDialog *dialog)
{
	EEditor *editor;
	WebKitWebView *webview;
	gboolean found;

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	webview = WEBKIT_WEB_VIEW (
			e_editor_get_editor_widget (editor));

	found = webkit_web_view_search_text (
		webview,
		gtk_entry_get_text (GTK_ENTRY (dialog->priv->search_entry)),
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->case_sensitive)),
		!gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->backwards)),
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->wrap)));

	return found;
}

static void
editor_replace_dialog_skip_cb (EEditorReplaceDialog *dialog)
{
	if (!jump (dialog)) {
		gtk_label_set_label (
			GTK_LABEL (dialog->priv->result_label),
			N_("No match found"));
		gtk_widget_show (dialog->priv->result_label);
	} else {
		gtk_widget_hide (dialog->priv->result_label);
	}
}

static void
editor_replace_dialog_replace_cb (EEditorReplaceDialog *dialog)
{
	EEditor *editor;
	EEditorWidget *editor_widget;
	EEditorSelection *selection;

	/* Jump to next matching word */
	if (!jump (dialog)) {
		gtk_label_set_label (
			GTK_LABEL (dialog->priv->result_label),
			N_("No match found"));
		gtk_widget_show (dialog->priv->result_label);
		return;
	} else {
		gtk_widget_hide (dialog->priv->result_label);
	}

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	editor_widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (editor_widget);

	e_editor_selection_replace (
		selection,
		gtk_entry_get_text (GTK_ENTRY (dialog->priv->replace_entry)));
}

static void
editor_replace_dialog_replace_all_cb (EEditorReplaceDialog *dialog)
{
	gint i = 0;
	gchar *result;
	EEditor *editor;
	EEditorWidget *widget;
	EEditorSelection *selection;
	const gchar *replacement;

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (widget);
	replacement = gtk_entry_get_text (GTK_ENTRY (dialog->priv->replace_entry));

	while (jump (dialog)) {
		e_editor_selection_replace (selection, replacement);
		i++;
	}

	result = g_strdup_printf (_("%d occurences replaced"), i);
	gtk_label_set_label (GTK_LABEL (dialog->priv->result_label), result);
	gtk_widget_show (dialog->priv->result_label);
	g_free (result);
}

static void
editor_replace_dialog_close_cb (EEditorReplaceDialog *dialog)
{
	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
editor_replace_dialog_entry_changed (EEditorReplaceDialog *dialog)
{
	gboolean ready;
	ready = ((gtk_entry_get_text_length (
			GTK_ENTRY (dialog->priv->search_entry)) != 0) &&
		 (gtk_entry_get_text_length (
			 GTK_ENTRY (dialog->priv->replace_entry)) != 0));

	gtk_widget_set_sensitive (dialog->priv->skip_button, ready);
	gtk_widget_set_sensitive (dialog->priv->replace_button, ready);
	gtk_widget_set_sensitive (dialog->priv->replace_all_button, ready);
}

static void
editor_replace_dialog_show (GtkWidget *widget)
{
	EEditorReplaceDialog *dialog = E_EDITOR_REPLACE_DIALOG (widget);

	gtk_widget_grab_focus (dialog->priv->search_entry);
	gtk_widget_hide (dialog->priv->result_label);

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_editor_replace_dialog_parent_class)->show (widget);
}

static void
e_editor_replace_dialog_class_init (EEditorReplaceDialogClass *klass)
{
	GtkWidgetClass *widget_class;

	e_editor_replace_dialog_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorReplaceDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show = editor_replace_dialog_show;
}

static void
e_editor_replace_dialog_init (EEditorReplaceDialog *dialog)
{
	GtkGrid *main_layout;
	GtkWidget *widget, *layout;

	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (
				dialog, E_TYPE_EDITOR_REPLACE_DIALOG,
				EEditorReplaceDialogPrivate);

	main_layout = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (main_layout, 10);
	gtk_grid_set_column_spacing (main_layout, 10);
	gtk_container_add (GTK_CONTAINER (dialog), GTK_WIDGET (main_layout));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);

	widget = gtk_entry_new ();
	gtk_grid_attach (main_layout, widget, 1, 0, 2, 1);
	dialog->priv->search_entry = widget;
	g_signal_connect_swapped (
		widget, "notify::text-length",
		G_CALLBACK (editor_replace_dialog_entry_changed), dialog);

	widget = gtk_label_new_with_mnemonic (_("R_eplace:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->search_entry);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	widget = gtk_entry_new ();
	gtk_grid_attach (main_layout, widget, 1, 1, 2, 1);
	dialog->priv->replace_entry = widget;
	g_signal_connect_swapped (
		widget, "notify::text-length",
		G_CALLBACK (editor_replace_dialog_entry_changed), dialog);

	widget = gtk_label_new_with_mnemonic (_("_With:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->replace_entry);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_grid_attach (main_layout, widget, 0, 1, 1, 1);

	layout = gtk_hbox_new (FALSE, 5);
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

	layout = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_set_spacing (GTK_BOX (layout), 5);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (layout), GTK_BUTTONBOX_START);
	gtk_grid_attach (main_layout, layout, 2, 3, 1, 1);

	widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_box_pack_start (GTK_BOX (layout), widget, FALSE, FALSE, 5);
	dialog->priv->close_button = widget;
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_replace_dialog_close_cb), dialog);

	widget = gtk_button_new_with_mnemonic (_("_Skip"));
	gtk_box_pack_start (GTK_BOX (layout), widget, FALSE, FALSE, 5);
	gtk_widget_set_sensitive (widget, FALSE);
	dialog->priv->skip_button = widget;
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_replace_dialog_skip_cb), dialog);

	widget = gtk_button_new_with_mnemonic (_("_Replace"));
	gtk_box_pack_start (GTK_BOX (layout), widget, FALSE, FALSE, 5);
	gtk_widget_set_sensitive (widget, FALSE);
	dialog->priv->replace_button = widget;
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_replace_dialog_replace_cb), dialog);

	widget = gtk_button_new_with_mnemonic (_("Replace _All"));
	gtk_box_pack_start (GTK_BOX (layout), widget, FALSE, FALSE, 5);
	gtk_widget_set_sensitive (widget, FALSE);
	dialog->priv->replace_all_button = widget;
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_replace_dialog_replace_all_cb), dialog);

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_editor_replace_dialog_new (EEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_EDITOR_REPLACE_DIALOG,
			"editor", editor,
			"icon-name", GTK_STOCK_FIND_AND_REPLACE,
			"title", N_("Replace"),
			NULL));
}
