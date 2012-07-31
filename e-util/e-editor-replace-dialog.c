/*
 * e-editor-replace-dialog.h
 *
<<<<<<< HEAD
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
=======
>>>>>>> Port Replace dialog and it's functionality
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

<<<<<<< HEAD
#define E_EDITOR_REPLACE_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_EDITOR_REPLACE_DIALOG, EEditorReplaceDialogPrivate))

G_DEFINE_TYPE (
	EEditorReplaceDialog,
	e_editor_replace_dialog,
	E_TYPE_EDITOR_DIALOG);
=======
G_DEFINE_TYPE (
	EEditorReplaceDialog,
	e_editor_replace_dialog,
	GTK_TYPE_WINDOW);
>>>>>>> Port Replace dialog and it's functionality

struct _EEditorReplaceDialogPrivate {
	GtkWidget *search_entry;
	GtkWidget *replace_entry;

	GtkWidget *case_sensitive;
	GtkWidget *backwards;
	GtkWidget *wrap;

	GtkWidget *result_label;

<<<<<<< HEAD
	GtkWidget *skip_button;
	GtkWidget *replace_button;
	GtkWidget *replace_all_button;
=======
	GtkWidget *close_button;
	GtkWidget *skip_button;
	GtkWidget *replace_button;
	GtkWidget *replace_all_button;

	EEditor *editor;
};

enum {
	PROP_0,
	PROP_EDITOR
>>>>>>> Port Replace dialog and it's functionality
};

static gboolean
jump (EEditorReplaceDialog *dialog)
{
<<<<<<< HEAD
	EEditor *editor;
	WebKitWebView *webview;
	gboolean found;

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	webview = WEBKIT_WEB_VIEW (
			e_editor_get_editor_widget (editor));
=======
	WebKitWebView *webview;
	gboolean found;

	webview = WEBKIT_WEB_VIEW (
			e_editor_get_editor_widget (dialog->priv->editor));
>>>>>>> Port Replace dialog and it's functionality

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
<<<<<<< HEAD
	EEditor *editor;
=======
>>>>>>> Port Replace dialog and it's functionality
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

<<<<<<< HEAD
	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	editor_widget = e_editor_get_editor_widget (editor);
=======
	editor_widget = e_editor_get_editor_widget (dialog->priv->editor);
>>>>>>> Port Replace dialog and it's functionality
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
<<<<<<< HEAD
	EEditor *editor;
=======
>>>>>>> Port Replace dialog and it's functionality
	EEditorWidget *widget;
	EEditorSelection *selection;
	const gchar *replacement;

<<<<<<< HEAD
	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);
=======
	widget = e_editor_get_editor_widget (dialog->priv->editor);
>>>>>>> Port Replace dialog and it's functionality
	selection = e_editor_widget_get_selection (widget);
	replacement = gtk_entry_get_text (GTK_ENTRY (dialog->priv->replace_entry));

	while (jump (dialog)) {
		e_editor_selection_replace (selection, replacement);
		i++;
<<<<<<< HEAD

		/* Jump behind the word */
		e_editor_selection_move (selection, TRUE, E_EDITOR_SELECTION_GRANULARITY_WORD);
=======
>>>>>>> Port Replace dialog and it's functionality
	}

	result = g_strdup_printf (_("%d occurences replaced"), i);
	gtk_label_set_label (GTK_LABEL (dialog->priv->result_label), result);
	gtk_widget_show (dialog->priv->result_label);
	g_free (result);
}

static void
<<<<<<< HEAD
=======
editor_replace_dialog_close_cb (EEditorReplaceDialog *dialog)
{
	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
>>>>>>> Port Replace dialog and it's functionality
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
<<<<<<< HEAD
e_editor_replace_dialog_class_init (EEditorReplaceDialogClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EEditorReplaceDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = editor_replace_dialog_show;
=======
editor_replace_dialog_set_property (GObject *object,
				    guint property_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			E_EDITOR_REPLACE_DIALOG (object)->priv->editor =
				g_object_ref (g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_replace_dialog_finalize (GObject *object)
{
	EEditorReplaceDialogPrivate *priv = E_EDITOR_REPLACE_DIALOG (object)->priv;

	g_clear_object (&priv->editor);

	/* Chain up to parent implementation */
	G_OBJECT_CLASS (e_editor_replace_dialog_parent_class)->finalize (object);
}

static void
e_editor_replace_dialog_class_init (EEditorReplaceDialogClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	e_editor_replace_dialog_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorReplaceDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show = editor_replace_dialog_show;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = editor_replace_dialog_set_property;
	object_class->finalize = editor_replace_dialog_finalize;

	g_object_class_install_property (
		object_class,
		PROP_EDITOR,
		g_param_spec_object (
			"editor",
		        NULL,
		        NULL,
		        E_TYPE_EDITOR,
		        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
>>>>>>> Port Replace dialog and it's functionality
}

static void
e_editor_replace_dialog_init (EEditorReplaceDialog *dialog)
{
	GtkGrid *main_layout;
	GtkWidget *widget, *layout;
<<<<<<< HEAD
	GtkBox *button_box;

	dialog->priv = E_EDITOR_REPLACE_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_editor_dialog_get_container (E_EDITOR_DIALOG (dialog));
=======

	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (
				dialog, E_TYPE_EDITOR_REPLACE_DIALOG,
				EEditorReplaceDialogPrivate);

	main_layout = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (main_layout, 10);
	gtk_grid_set_column_spacing (main_layout, 10);
	gtk_container_add (GTK_CONTAINER (dialog), GTK_WIDGET (main_layout));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
>>>>>>> Port Replace dialog and it's functionality

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

<<<<<<< HEAD
	layout = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
=======
	layout = gtk_hbox_new (FALSE, 5);
>>>>>>> Port Replace dialog and it's functionality
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

<<<<<<< HEAD
	button_box = e_editor_dialog_get_button_box (E_EDITOR_DIALOG (dialog));

	widget = gtk_button_new_with_mnemonic (_("_Skip"));
	gtk_box_pack_start (button_box, widget, FALSE, FALSE, 5);
=======
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
>>>>>>> Port Replace dialog and it's functionality
	gtk_widget_set_sensitive (widget, FALSE);
	dialog->priv->skip_button = widget;
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_replace_dialog_skip_cb), dialog);

	widget = gtk_button_new_with_mnemonic (_("_Replace"));
<<<<<<< HEAD
	gtk_box_pack_start (button_box, widget, FALSE, FALSE, 5);
=======
	gtk_box_pack_start (GTK_BOX (layout), widget, FALSE, FALSE, 5);
>>>>>>> Port Replace dialog and it's functionality
	gtk_widget_set_sensitive (widget, FALSE);
	dialog->priv->replace_button = widget;
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_replace_dialog_replace_cb), dialog);

	widget = gtk_button_new_with_mnemonic (_("Replace _All"));
<<<<<<< HEAD
	gtk_box_pack_start (button_box, widget, FALSE, FALSE, 5);
=======
	gtk_box_pack_start (GTK_BOX (layout), widget, FALSE, FALSE, 5);
>>>>>>> Port Replace dialog and it's functionality
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
<<<<<<< HEAD
			"editor", editor,
			"icon-name", GTK_STOCK_FIND_AND_REPLACE,
			"title", N_("Replace"),
=======
			"destroy-with-parent", TRUE,
			"events", GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK,
			"editor", editor,
			"icon-name", GTK_STOCK_FIND_AND_REPLACE,
			"resizable", FALSE,
			"title", N_("Replace"),
			"transient-for", gtk_widget_get_toplevel (GTK_WIDGET (editor)),
			"type", GTK_WINDOW_TOPLEVEL,
			"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
>>>>>>> Port Replace dialog and it's functionality
			NULL));
}
