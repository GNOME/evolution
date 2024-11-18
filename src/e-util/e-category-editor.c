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
#include "e-misc-utils.h"

struct _ECategoryEditorPrivate {
	GtkWidget *category_name;
	GtkWidget *category_icon;
};

G_DEFINE_TYPE_WITH_PRIVATE (ECategoryEditor, e_category_editor, GTK_TYPE_DIALOG)

static void
update_preview (GtkFileChooser *chooser,
                gpointer user_data)
{
	GtkImage *image;
	gchar *filename;

	g_return_if_fail (chooser != NULL);

	image = GTK_IMAGE (gtk_file_chooser_get_preview_widget (chooser));
	if (!image)
		return;

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
unset_icon_clicked_cb (GtkWidget *button,
		       gpointer user_data)
{
	GtkFileChooser *file_chooser = user_data;

	g_return_if_fail (GTK_IS_FILE_CHOOSER (file_chooser));

	gtk_file_chooser_unselect_all (file_chooser);
	gtk_widget_set_sensitive (button, FALSE);
}

static void
chooser_button_file_set_cb (GtkFileChooser *chooser_button,
			    gpointer user_data)
{
	GtkWidget *unset_button = user_data;
	GSList *uris;

	g_return_if_fail (GTK_IS_WIDGET (unset_button));

	uris = gtk_file_chooser_get_uris (chooser_button);

	gtk_widget_set_sensitive (unset_button, uris != NULL);

	g_slist_free_full (uris, g_free);
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
				g_string_append_c (str, *p);
		}
		p++;
	}

	return g_strstrip (g_string_free (str, FALSE));
}

static void
e_category_editor_class_init (ECategoryEditorClass *class)
{
}

static void
e_category_editor_init (ECategoryEditor *editor)
{
	GtkWidget *dialog_content;
	GtkGrid *grid_category_properties;
	GtkWidget *label_name;
	GtkWidget *label_icon;
	GtkWidget *category_name;
	GtkWidget *chooser_button;
	GtkWidget *no_image_button;
	GtkWidget *chooser_dialog = NULL;
	GtkWidget *preview;

	editor->priv = e_category_editor_get_instance_private (editor);

	gtk_window_set_resizable (GTK_WINDOW (editor), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (editor), 6);

	if (!e_util_is_running_flatpak ()) {
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
	}

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
	gtk_label_set_xalign (GTK_LABEL (label_name), 0);
	gtk_grid_attach (grid_category_properties, label_name, 0, 0, 1, 1);

	category_name = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (category_name), TRUE);
	gtk_widget_set_hexpand (category_name, TRUE);
	gtk_widget_set_halign (category_name, GTK_ALIGN_FILL);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label_name), category_name);
	gtk_grid_attach (grid_category_properties, category_name, 1, 0, 1, 1);
	editor->priv->category_name = category_name;

	label_icon = gtk_label_new_with_mnemonic (_("Category _Icon"));
	gtk_widget_set_halign (label_icon, GTK_ALIGN_FILL);
	gtk_label_set_xalign (GTK_LABEL (label_icon), 0);
	gtk_grid_attach (grid_category_properties, label_icon, 0, 1, 1, 1);

	if (chooser_dialog) {
		chooser_button = gtk_file_chooser_button_new_with_dialog (chooser_dialog);

		g_signal_connect (
			chooser_dialog, "response",
			G_CALLBACK (file_chooser_response), chooser_button);
	} else {
		GtkWidget *unset_button;

		chooser_button = gtk_file_chooser_button_new (_("Category Icon"), GTK_FILE_CHOOSER_ACTION_OPEN);

		unset_button = gtk_button_new_with_mnemonic (_("_Unset icon"));
		gtk_widget_set_sensitive (unset_button, FALSE);
		gtk_grid_attach (grid_category_properties, unset_button, 1, 2, 1, 1);

		g_signal_connect (unset_button, "clicked",
			G_CALLBACK (unset_icon_clicked_cb), chooser_button);

		g_signal_connect (chooser_button, "file-set",
			G_CALLBACK (chooser_button_file_set_cb), unset_button);
	}

	gtk_widget_set_hexpand (chooser_button, TRUE);
	gtk_widget_set_halign (chooser_button, GTK_ALIGN_FILL);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label_icon), chooser_button);
	gtk_grid_attach (grid_category_properties, chooser_button, 1, 1, 1, 1);
	editor->priv->category_icon = chooser_button;

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
e_category_editor_new (void)
{
	return g_object_new (E_TYPE_CATEGORY_EDITOR,
		"use-header-bar", e_util_get_use_header_bar (),
		NULL);
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
				_("There is already a category “%s” in the "
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

		if (e_util_is_running_flatpak ())
			g_signal_emit_by_name (file_chooser, "file-set", NULL);
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
