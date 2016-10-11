/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 */

#include "evolution-config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include "e-category-editor.h"
#include "e-dialog-widgets.h"

#define E_CATEGORY_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CATEGORY_EDITOR, ECategoryEditorPrivate))

struct _ECategoryEditorPrivate {
	GtkWidget *category_name;
	GtkWidget *category_icon;
};

G_DEFINE_TYPE (ECategoryEditor, e_category_editor, GTK_TYPE_DIALOG)

static void
update_preview (GtkFileChooser *chooser,
                gpointer user_data)
{
	GtkImage *image;
	gchar *filename;

	g_return_if_fail (chooser != NULL);

	image = GTK_IMAGE (gtk_file_chooser_get_preview_widget (chooser));
	g_return_if_fail (image != NULL);

	filename = gtk_file_chooser_get_preview_filename (chooser);

	gtk_image_set_from_file (image, filename);
	gtk_file_chooser_set_preview_widget_active (chooser, filename != NULL);

	g_free (filename);
}

static void
file_chooser_response (GtkDialog *dialog,
                       gint response_id,
                       GtkFileChooser *button)
{
	g_return_if_fail (button != NULL);

	if (response_id == GTK_RESPONSE_NO)
		gtk_file_chooser_unselect_all (button);
}

static void
category_editor_category_name_changed (GtkEntry *category_name_entry,
                                       ECategoryEditor *editor)
{
	gchar *name;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (category_name_entry != NULL);

	name = g_strdup (gtk_entry_get_text (category_name_entry));
	if (name != NULL)
		name = g_strstrip (name);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (editor), GTK_RESPONSE_OK, name && *name);

	g_free (name);
}

static gchar *
check_category_name (const gchar *name)
{
	GString *str = NULL;
	gchar *p = (gchar *) name;

	str = g_string_new ("");
	while (*p) {
		switch (*p) {
			case ',':
				break;
			default:
				str = g_string_append_c (str, *p);
		}
		p++;
	}

	p = g_strstrip (g_string_free (str, FALSE));

	return p;
}

static void
e_category_editor_class_init (ECategoryEditorClass *class)
{
	g_type_class_add_private (class, sizeof (ECategoryEditorPrivate));
}

static void
e_category_editor_init (ECategoryEditor *editor)
{
	GtkWidget *dialog_content;
	GtkWidget *dialog_action_area;
	GtkGrid *grid_category_properties;
	GtkWidget *label_name;
	GtkWidget *label_icon;
	GtkWidget *category_name;
	GtkWidget *chooser_button;
	GtkWidget *no_image_button;
	GtkWidget *chooser_dialog;
	GtkWidget *preview;

	editor->priv = E_CATEGORY_EDITOR_GET_PRIVATE (editor);

	chooser_dialog = gtk_file_chooser_dialog_new (
		_("Category Icon"),
		NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Cancel"), GTK_RESPONSE_CANCEL, NULL);

	no_image_button = e_dialog_button_new_with_icon ("window-close", _("_No Image"));
	gtk_dialog_add_action_widget (
		GTK_DIALOG (chooser_dialog),
		no_image_button, GTK_RESPONSE_NO);
	gtk_dialog_add_button (
		GTK_DIALOG (chooser_dialog),
		_("_Open"), GTK_RESPONSE_ACCEPT);
	gtk_file_chooser_set_local_only (
		GTK_FILE_CHOOSER (chooser_dialog), TRUE);
	gtk_widget_show (no_image_button);

	g_signal_connect (
		chooser_dialog, "update-preview",
		G_CALLBACK (update_preview), NULL);

	preview = gtk_image_new ();
	gtk_file_chooser_set_preview_widget (
		GTK_FILE_CHOOSER (chooser_dialog), preview);
	gtk_file_chooser_set_preview_widget_active (
		GTK_FILE_CHOOSER (chooser_dialog), TRUE);
	gtk_widget_show_all (preview);

	dialog_content = gtk_dialog_get_content_area (GTK_DIALOG (editor));

	grid_category_properties = GTK_GRID (gtk_grid_new ());
	gtk_box_pack_start (
		GTK_BOX (dialog_content),
		GTK_WIDGET (grid_category_properties), TRUE, TRUE, 0);
	gtk_container_set_border_width (
		GTK_CONTAINER (grid_category_properties), 12);
	gtk_grid_set_row_spacing (grid_category_properties, 6);
	gtk_grid_set_column_spacing (grid_category_properties, 6);

	label_name = gtk_label_new_with_mnemonic (_("Category _Name"));
	gtk_widget_set_halign (label_name, GTK_ALIGN_FILL);
	gtk_misc_set_alignment (GTK_MISC (label_name), 0, 0.5);
	gtk_grid_attach (grid_category_properties, label_name, 0, 0, 1, 1);

	category_name = gtk_entry_new ();
	gtk_widget_set_hexpand (category_name, TRUE);
	gtk_widget_set_halign (category_name, GTK_ALIGN_FILL);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label_name), category_name);
	gtk_grid_attach (grid_category_properties, category_name, 1, 0, 1, 1);
	editor->priv->category_name = category_name;

	label_icon = gtk_label_new_with_mnemonic (_("Category _Icon"));
	gtk_widget_set_halign (label_icon, GTK_ALIGN_FILL);
	gtk_misc_set_alignment (GTK_MISC (label_icon), 0, 0.5);
	gtk_grid_attach (grid_category_properties, label_icon, 0, 1, 1, 1);

	chooser_button = GTK_WIDGET (
		gtk_file_chooser_button_new_with_dialog (chooser_dialog));
	gtk_widget_set_hexpand (chooser_button, TRUE);
	gtk_widget_set_halign (chooser_button, GTK_ALIGN_FILL);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label_icon), chooser_button);
	gtk_grid_attach (grid_category_properties, chooser_button, 1, 1, 1, 1);
	editor->priv->category_icon = chooser_button;

	g_signal_connect (
		chooser_dialog, "response",
		G_CALLBACK (file_chooser_response), chooser_button);

	dialog_action_area = gtk_dialog_get_action_area (GTK_DIALOG (editor));
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

	gtk_dialog_add_buttons (
		GTK_DIALOG (editor),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (editor), GTK_RESPONSE_OK);
	gtk_window_set_title (GTK_WINDOW (editor), _("Category Properties"));
	gtk_window_set_type_hint (
		GTK_WINDOW (editor), GDK_WINDOW_TYPE_HINT_DIALOG);

	gtk_widget_show_all (dialog_content);

	g_signal_connect (
		category_name, "changed",
		G_CALLBACK (category_editor_category_name_changed), editor);

	category_editor_category_name_changed (
		GTK_ENTRY (category_name), editor);
}

/**
 * e_categort_editor_new:
 *
 * Creates a new #ECategoryEditor widget.
 *
 * Returns: a new #ECategoryEditor
 *
 * Since: 3.2
 **/
ECategoryEditor *
e_category_editor_new ()
{
	return g_object_new (E_TYPE_CATEGORY_EDITOR, NULL);
}

/**
 * e_category_editor_create_category:
 *
 * Since: 3.2
 **/
const gchar *
e_category_editor_create_category (ECategoryEditor *editor)
{
	GtkEntry *entry;
	GtkFileChooser *file_chooser;

	g_return_val_if_fail (E_IS_CATEGORY_EDITOR (editor), NULL);

	entry = GTK_ENTRY (editor->priv->category_name);
	file_chooser = GTK_FILE_CHOOSER (editor->priv->category_icon);

	do {
		const gchar *category_name;
		const gchar *correct_category_name;

		if (gtk_dialog_run (GTK_DIALOG (editor)) != GTK_RESPONSE_OK)
			return NULL;

		category_name = gtk_entry_get_text (entry);
		correct_category_name = check_category_name (category_name);

		if (e_categories_exist (correct_category_name)) {
			GtkWidget *error_dialog;

			error_dialog = gtk_message_dialog_new (
				GTK_WINDOW (editor),
				0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				_("There is already a category '%s' in the "
				"configuration. Please use another name"),
				category_name);

			gtk_dialog_run (GTK_DIALOG (error_dialog));
			gtk_widget_destroy (error_dialog);

			/* Now we loop and run the dialog again. */

		} else {
			gchar *category_icon;

			category_icon =
				gtk_file_chooser_get_filename (file_chooser);
			e_categories_add (
				correct_category_name, NULL,
				category_icon, TRUE);
			g_free (category_icon);

			return correct_category_name;
		}

	} while (TRUE);
}

/**
 * e_category_editor_edit_category:
 *
 * Since: 3.2
 **/
gboolean
e_category_editor_edit_category (ECategoryEditor *editor,
                                 const gchar *category)
{
	GtkFileChooser *file_chooser;
	gchar *icon_file;

	g_return_val_if_fail (E_IS_CATEGORY_EDITOR (editor), FALSE);
	g_return_val_if_fail (category != NULL, FALSE);

	file_chooser = GTK_FILE_CHOOSER (editor->priv->category_icon);

	gtk_entry_set_text (GTK_ENTRY (editor->priv->category_name), category);
	gtk_widget_set_sensitive (editor->priv->category_name, FALSE);

	icon_file = e_categories_dup_icon_file_for (category);
	if (icon_file) {
		gtk_file_chooser_set_filename (file_chooser, icon_file);
		update_preview (file_chooser, NULL);
	}
	g_free (icon_file);

	if (gtk_dialog_run (GTK_DIALOG (editor)) == GTK_RESPONSE_OK) {
		gchar *category_icon;

		category_icon = gtk_file_chooser_get_filename (file_chooser);
		e_categories_set_icon_file_for (category, category_icon);

		gtk_dialog_set_response_sensitive (
			GTK_DIALOG (editor), GTK_RESPONSE_OK, TRUE);

		g_free (category_icon);

		return TRUE;
	}

	return FALSE;
}
