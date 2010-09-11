/*
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
 * Authors:
 *		Johnny Jacob <jjohnny@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include <gconf/gconf-client.h>

#include <e-util/e-util.h>
#include <e-util/e-config.h>
#include <mail/em-config.h>
#include <mail/em-event.h>

#include <e-util/e-alert-dialog.h>
#include <e-util/e-plugin.h>

#include <mail/em-utils.h>

#include "composer/e-msg-composer.h"
#include "composer/e-composer-actions.h"
#include "widgets/misc/e-attachment-view.h"
#include "widgets/misc/e-attachment-store.h"

#define GCONF_KEY_ATTACH_REMINDER_CLUES "/apps/evolution/mail/attachment_reminder_clues"
#define SIGNATURE "-- "

typedef struct {
	GConfClient *gconf;
	GtkWidget   *treeview;
	GtkWidget   *clue_add;
	GtkWidget   *clue_edit;
	GtkWidget   *clue_remove;
	GtkListStore *store;
} UIData;

enum {
	CLUE_KEYWORD_COLUMN,
	CLUE_N_COLUMNS
};

gint e_plugin_lib_enable (EPlugin *ep, gint enable);
GtkWidget *e_plugin_lib_get_configure_widget (EPlugin *plugin);

void org_gnome_evolution_attachment_reminder (EPlugin *ep, EMEventTargetComposer *t);
GtkWidget* org_gnome_attachment_reminder_config_option (EPlugin *plugin, struct _EConfigHookItemFactoryData *data);

static gboolean ask_for_missing_attachment (EPlugin *ep, GtkWindow *widget);
static gboolean check_for_attachment_clues (gchar *msg);
static gboolean check_for_attachment (EMsgComposer *composer);
static gchar * strip_text_msg (gchar *msg);
static void commit_changes (UIData *ui);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	return 0;
}

void
org_gnome_evolution_attachment_reminder (EPlugin *ep, EMEventTargetComposer *t)
{
	GByteArray *raw_msg_barray;

	gchar *filtered_str = NULL;

	raw_msg_barray = e_msg_composer_get_raw_message_text (t->composer);

	if (!raw_msg_barray)
		return;

	raw_msg_barray = g_byte_array_append (raw_msg_barray, (const guint8 *)"", 1);

	filtered_str = strip_text_msg ((gchar *) raw_msg_barray->data);

	g_byte_array_free (raw_msg_barray, TRUE);

	/* Set presend_check_status for the composer*/
	if (check_for_attachment_clues (filtered_str) && !check_for_attachment (t->composer))
		if (!ask_for_missing_attachment (ep, (GtkWindow *)t->composer))
			g_object_set_data ((GObject *) t->composer, "presend_check_status", GINT_TO_POINTER(1));

	g_free (filtered_str);
}

static gboolean
ask_for_missing_attachment (EPlugin *ep, GtkWindow *window)
{
	GtkWidget *check = NULL;
	GtkDialog *dialog = NULL;
	GtkWidget *content_area;
	gint response;

	dialog = (GtkDialog*) e_alert_dialog_new_for_args (
		window, "org.gnome.evolution.plugins.attachment_reminder:attachment-reminder", NULL);

	/*Check buttons*/
	check = gtk_check_button_new_with_mnemonic (_("_Do not show this message again."));
	gtk_container_set_border_width((GtkContainer *)check, 12);
	content_area = gtk_dialog_get_content_area (dialog);
	gtk_box_pack_start (GTK_BOX (content_area), check, TRUE, TRUE, 0);
	gtk_widget_show (check);

	response = gtk_dialog_run ((GtkDialog *) dialog);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check)))
		e_plugin_enable (ep, FALSE);

	gtk_widget_destroy((GtkWidget *)dialog);

	if (response == GTK_RESPONSE_OK)
		gtk_action_activate (E_COMPOSER_ACTION_ATTACH (window));

	return response == GTK_RESPONSE_YES;
}

/* check for the clues */
static gboolean
check_for_attachment_clues (gchar *msg)
{
	/* TODO : Add more strings. RegEx ??? */
	GConfClient *gconf;
	GSList *clue_list = NULL, *list;
	gboolean ret_val = FALSE;
	guint msg_length;

	gconf = gconf_client_get_default ();

	/* Get the list from gconf */
	clue_list = gconf_client_get_list ( gconf, GCONF_KEY_ATTACH_REMINDER_CLUES, GCONF_VALUE_STRING, NULL );

	g_object_unref (gconf);

	msg_length = strlen (msg);
	for (list = clue_list;list && !ret_val;list=g_slist_next(list)) {
		gchar *needle = g_utf8_strdown (list->data, -1);
		if (g_strstr_len (msg, msg_length, needle)) {
			ret_val = TRUE;
		}
		g_free (needle);
	}

	if (clue_list) {
		g_slist_foreach (clue_list, (GFunc) g_free, NULL);
		g_slist_free (clue_list);
	}

	return ret_val;
}

/* check for the any attachment */
static gboolean
check_for_attachment (EMsgComposer *composer)
{
	EAttachmentView *view;
	EAttachmentStore *store;

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	return (e_attachment_store_get_num_attachments (store) > 0);
}

static gchar *
strip_text_msg (gchar *msg)
{
	gchar **lines = g_strsplit ( msg, "\n", -1);
	gchar *stripped_msg = g_strdup (" ");
	guint i=0;
	gchar *temp;

	/* Note : HTML Signatures won't work. Depends on Bug #522784 */
	while (lines[i] && g_strcmp0 (lines[i], SIGNATURE)) {
		if (!g_str_has_prefix (g_strstrip(lines[i]), ">")) {
			temp = stripped_msg;

			stripped_msg = g_strconcat (" ", stripped_msg, lines[i], NULL);

			g_free (temp);
		}
		i++;
	}

	g_strfreev (lines);

	temp = g_utf8_strdown (stripped_msg, -1);
	g_free (stripped_msg);

	return temp;
}

static void
commit_changes (UIData *ui)
{
	GtkTreeModel *model = NULL;
	GSList *clue_list = NULL;
	GtkTreeIter iter;
	gboolean valid;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gchar *keyword;

		gtk_tree_model_get (
			model, &iter, CLUE_KEYWORD_COLUMN, &keyword, -1);

		/* Check if the keyword is not empty */
		if ((keyword) && (g_utf8_strlen(g_strstrip(keyword), -1) > 0))
			clue_list = g_slist_append (clue_list, keyword);
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	gconf_client_set_list (ui->gconf, GCONF_KEY_ATTACH_REMINDER_CLUES, GCONF_VALUE_STRING, clue_list, NULL);

	g_slist_foreach (clue_list, (GFunc) g_free, NULL);
	g_slist_free (clue_list);
}

static void
cell_edited_cb (GtkCellRendererText *cell,
                gchar *path_string,
                gchar *new_text,
                UIData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	if (new_text == NULL || *g_strstrip (new_text) == '\0')
		gtk_button_clicked (GTK_BUTTON (ui->clue_remove));
	else {
		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			CLUE_KEYWORD_COLUMN, new_text, -1);
		commit_changes (ui);
	}
}

static void
cell_editing_canceled_cb (GtkCellRenderer *cell,
                          UIData *ui)
{
	gtk_button_clicked (GTK_BUTTON (ui->clue_remove));
}

static void
clue_add_clicked (GtkButton *button, UIData *ui)
{
	GtkTreeModel *model;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	GtkTreeIter iter;

	tree_view = GTK_TREE_VIEW (ui->treeview);
	model = gtk_tree_view_get_model (tree_view);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	path = gtk_tree_model_get_path (model, &iter);
	column = gtk_tree_view_get_column (tree_view, CLUE_KEYWORD_COLUMN);
	gtk_tree_view_set_cursor (tree_view, path, column, TRUE);
	gtk_tree_view_row_activated (tree_view, path, column);
	gtk_tree_path_free (path);
}

static void
clue_remove_clicked (GtkButton *button, UIData *ui)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean valid;
	gint len;

	valid = FALSE;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	/* Get the path and move to the previous node :) */
	path = gtk_tree_model_get_path (model, &iter);
	if (path)
		valid = gtk_tree_path_prev(path);

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	len = gtk_tree_model_iter_n_children (model, NULL);
	if (len > 0) {
		if (gtk_list_store_iter_is_valid (GTK_LIST_STORE(model), &iter)) {
			gtk_tree_selection_select_iter (selection, &iter);
		} else {
			if (path && valid) {
				gtk_tree_model_get_iter(model, &iter, path);
				gtk_tree_selection_select_iter (selection, &iter);
			}
		}
	} else {
		gtk_widget_set_sensitive (ui->clue_edit, FALSE);
		gtk_widget_set_sensitive (ui->clue_remove, FALSE);
	}

	gtk_widget_grab_focus(ui->treeview);
	gtk_tree_path_free (path);

	commit_changes (ui);
}

static void
clue_edit_clicked (GtkButton *button, UIData *ui)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeViewColumn *focus_col;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	focus_col = gtk_tree_view_get_column (GTK_TREE_VIEW (ui->treeview), CLUE_KEYWORD_COLUMN);
	path = gtk_tree_model_get_path (model, &iter);

	if (path) {
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (ui->treeview), path, focus_col, TRUE);
		gtk_tree_path_free (path);
	}
}

static void
selection_changed (GtkTreeSelection *selection, UIData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_widget_set_sensitive (ui->clue_edit, TRUE);
		gtk_widget_set_sensitive (ui->clue_remove, TRUE);
	} else {
		gtk_widget_set_sensitive (ui->clue_edit, FALSE);
		gtk_widget_set_sensitive (ui->clue_remove, FALSE);
	}
}

static void
destroy_ui_data (gpointer data)
{
	UIData *ui = (UIData *) data;

	if (!ui)
		return;

	g_object_unref (ui->gconf);
	g_free (ui);
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *plugin)
{
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GConfClient *gconf = gconf_client_get_default();
	GtkWidget *hbox;
	GSList *clue_list = NULL, *list;

	GtkWidget *reminder_configuration_box;
	GtkWidget *clue_container;
	GtkWidget *scrolledwindow1;
	GtkWidget *clue_treeview;
	GtkWidget *vbuttonbox2;
	GtkWidget *clue_add;
	GtkWidget *clue_edit;
	GtkWidget *clue_remove;

	UIData *ui = g_new0 (UIData, 1);

	reminder_configuration_box = gtk_vbox_new (FALSE, 5);
	gtk_widget_show (reminder_configuration_box);
	gtk_widget_set_size_request (reminder_configuration_box, 385, 189);

	clue_container = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (clue_container);
	gtk_box_pack_start (GTK_BOX (reminder_configuration_box), clue_container, TRUE, TRUE, 0);

	scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow1);
	gtk_box_pack_start (GTK_BOX (clue_container), scrolledwindow1, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	clue_treeview = gtk_tree_view_new ();
	gtk_widget_show (clue_treeview);
	gtk_container_add (GTK_CONTAINER (scrolledwindow1), clue_treeview);
	gtk_container_set_border_width (GTK_CONTAINER (clue_treeview), 1);

	vbuttonbox2 = gtk_vbutton_box_new ();
	gtk_widget_show (vbuttonbox2);
	gtk_box_pack_start (GTK_BOX (clue_container), vbuttonbox2, FALSE, TRUE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox2), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (vbuttonbox2), 6);

	clue_add = gtk_button_new_from_stock ("gtk-add");
	gtk_widget_show (clue_add);
	gtk_container_add (GTK_CONTAINER (vbuttonbox2), clue_add);
	gtk_widget_set_can_default (clue_add, TRUE);

	clue_edit = gtk_button_new_from_stock ("gtk-edit");
	gtk_widget_show (clue_edit);
	gtk_container_add (GTK_CONTAINER (vbuttonbox2), clue_edit);
	gtk_widget_set_can_default (clue_edit, TRUE);

	clue_remove = gtk_button_new_from_stock ("gtk-remove");
	gtk_widget_show (clue_remove);
	gtk_container_add (GTK_CONTAINER (vbuttonbox2), clue_remove);
	gtk_widget_set_can_default (clue_remove, TRUE);

	ui->gconf = gconf_client_get_default ();

	ui->treeview = clue_treeview;

	ui->store = gtk_list_store_new (CLUE_N_COLUMNS, G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (ui->treeview), GTK_TREE_MODEL (ui->store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ui->treeview), -1, _("Keywords"),
			renderer, "text", CLUE_KEYWORD_COLUMN, NULL);
	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (cell_edited_cb), ui);
	g_signal_connect (
		renderer, "editing-canceled",
		G_CALLBACK (cell_editing_canceled_cb), ui);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (selection_changed), ui);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ui->treeview), TRUE);

	ui->clue_add = clue_add;
	g_signal_connect (G_OBJECT (ui->clue_add), "clicked", G_CALLBACK (clue_add_clicked), ui);

	ui->clue_remove = clue_remove;
	g_signal_connect (G_OBJECT (ui->clue_remove), "clicked", G_CALLBACK (clue_remove_clicked), ui);
	gtk_widget_set_sensitive (ui->clue_remove, FALSE);

	ui->clue_edit = clue_edit;
	g_signal_connect (G_OBJECT (ui->clue_edit), "clicked", G_CALLBACK (clue_edit_clicked), ui);
	gtk_widget_set_sensitive (ui->clue_edit, FALSE);

	/* Populate tree view with values from gconf */
	clue_list = gconf_client_get_list ( gconf, GCONF_KEY_ATTACH_REMINDER_CLUES, GCONF_VALUE_STRING, NULL );

	for (list = clue_list; list; list = g_slist_next (list)) {
		gtk_list_store_append (ui->store, &iter);
		gtk_list_store_set (ui->store, &iter, CLUE_KEYWORD_COLUMN, list->data, -1);
	}

	if (clue_list) {
		g_slist_foreach (clue_list, (GFunc) g_free, NULL);
		g_slist_free (clue_list);
	}

	/* Add the list here */

	hbox = gtk_vbox_new (FALSE, 0);

	gtk_box_pack_start (GTK_BOX (hbox), reminder_configuration_box, TRUE, TRUE, 0);

	/* to let free data properly on destroy of configuration widget */
	g_object_set_data_full (G_OBJECT (hbox), "myui-data", ui, destroy_ui_data);

	return hbox;
}

/* Configuration in Mail Prefs Page goes here */

GtkWidget *
org_gnome_attachment_reminder_config_option (EPlugin *plugin,
                                             struct _EConfigHookItemFactoryData *data)
{
	/* This function and the hook needs to be removed,
	once the configure code is thoroughly tested */

	return NULL;

}

