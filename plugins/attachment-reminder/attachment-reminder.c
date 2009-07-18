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

#include <glade/glade-xml.h>
#include <gconf/gconf-client.h>

#include <e-util/e-config.h>
#include <mail/em-config.h>
#include <mail/em-event.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-medium.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-multipart.h>

#include <e-util/e-error.h>
#include <e-util/e-plugin.h>

#include <mail/em-utils.h>

#include "composer/e-msg-composer.h"
#include "composer/e-composer-actions.h"
#include "widgets/misc/e-attachment-view.h"
#include "widgets/misc/e-attachment-store.h"

#define GCONF_KEY_ATTACH_REMINDER_CLUES "/apps/evolution/mail/attachment_reminder_clues"
#define SIGNATURE "-- "

typedef struct {
	GladeXML *xml;
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

gint e_plugin_lib_enable (EPluginLib *ep, gint enable);
GtkWidget *e_plugin_lib_get_configure_widget (EPlugin *epl);

void org_gnome_evolution_attachment_reminder (EPlugin *ep, EMEventTargetComposer *t);
GtkWidget* org_gnome_attachment_reminder_config_option (struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data);

static gboolean ask_for_missing_attachment (EPlugin *ep, GtkWindow *widget);
static gboolean check_for_attachment_clues (gchar *msg);
static gboolean check_for_attachment (EMsgComposer *composer);
static gchar * strip_text_msg (gchar *msg);
static void commit_changes (UIData *ui);

static void  cell_edited_callback (GtkCellRendererText *cell, gchar *path_string,
				   gchar *new_text,UIData *ui);
static gboolean clue_foreach_check_isempty (GtkTreeModel *model, GtkTreePath
					*path, GtkTreeIter *iter, UIData *ui);

gint
e_plugin_lib_enable (EPluginLib *ep, gint enable)
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
	gint response;

	dialog = (GtkDialog*)e_error_new(window, "org.gnome.evolution.plugins.attachment_reminder:attachment-reminder", NULL);

	/*Check buttons*/
	check = gtk_check_button_new_with_mnemonic (_("_Do not show this message again."));
	gtk_container_set_border_width((GtkContainer *)check, 12);
	gtk_box_pack_start ((GtkBox *)dialog->vbox, check, TRUE, TRUE, 0);
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
	while (lines [i] && g_strcmp0 (lines[i], SIGNATURE)) {
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

		gtk_tree_model_get (model, &iter, CLUE_KEYWORD_COLUMN, &keyword, -1);
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
clue_check_isempty (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, UIData *ui)
{
	GtkTreeSelection *selection;
	gchar *keyword = NULL;
	gboolean valid;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	/* move to the previous node */
	valid = gtk_tree_path_prev (path);

	gtk_tree_model_get (model, iter, CLUE_KEYWORD_COLUMN, &keyword, -1);
	if ((keyword) && !(g_utf8_strlen (g_strstrip (keyword), -1) > 0))
		gtk_list_store_remove (ui->store, iter);

	/* Check if we have a valid row to select. If not, then select
	 * the previous row */
	if (gtk_list_store_iter_is_valid (GTK_LIST_STORE (model), iter)) {
		gtk_tree_selection_select_iter (selection, iter);
	} else {
		if (path && valid) {
			gtk_tree_model_get_iter (model, iter, path);
			gtk_tree_selection_select_iter (selection, iter);
		}
	}

	gtk_widget_grab_focus (ui->treeview);
	g_free (keyword);
}

static gboolean
clue_foreach_check_isempty (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, UIData *ui)
{
	gboolean valid;

	valid = gtk_tree_model_get_iter_first (model, iter);
	while (valid && gtk_list_store_iter_is_valid (ui->store, iter)) {
		gchar *keyword = NULL;
		gtk_tree_model_get (model, iter, CLUE_KEYWORD_COLUMN, &keyword, -1);
		/* Check if the keyword is not empty and then emit the row-changed
		signal (if we delete the row, then the iter gets corrupted) */
		if ((keyword) && !(g_utf8_strlen (g_strstrip (keyword), -1) > 0))
			gtk_tree_model_row_changed (model, path, iter);

		g_free (keyword);
		valid = gtk_tree_model_iter_next (model, iter);
	}

	return FALSE;
}

static void
cell_edited_callback (GtkCellRendererText *cell,
		      gchar               *path_string,
		      gchar               *new_text,
		      UIData             *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));

	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    CLUE_KEYWORD_COLUMN, new_text, -1);

	commit_changes (ui);
}

static void
clue_add_clicked (GtkButton *button, UIData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *new_clue = NULL;
	GtkTreeViewColumn *focus_col;
	GtkTreePath *path;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc) clue_foreach_check_isempty, ui);

	/* Disconnect from signal so that we can create an empty row */
	g_signal_handlers_disconnect_matched(G_OBJECT(model), G_SIGNAL_MATCH_FUNC, 0, 0, NULL, clue_check_isempty, ui);

	/* TODO : Trim and check for blank strings */
	new_clue = g_strdup ("");
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    CLUE_KEYWORD_COLUMN, new_clue, -1);

	focus_col = gtk_tree_view_get_column (GTK_TREE_VIEW (ui->treeview), CLUE_KEYWORD_COLUMN);
	path = gtk_tree_model_get_path (model, &iter);

	if (path) {
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (ui->treeview), path, focus_col, TRUE);
		gtk_tree_view_row_activated(GTK_TREE_VIEW(ui->treeview), path, focus_col);
		gtk_tree_path_free (path);
	}

	/* We have done our job, connect back to the signal */
	g_signal_connect(G_OBJECT(model), "row-changed", G_CALLBACK(clue_check_isempty), ui);
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

	g_object_unref (ui->xml);
	g_object_unref (ui->gconf);
	g_free (ui);
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GConfClient *gconf = gconf_client_get_default();
	GtkWidget *hbox;
	GSList *clue_list = NULL, *list;
	GtkTreeModel *model;

	UIData *ui = g_new0 (UIData, 1);

	gchar *gladefile;

	gladefile = g_build_filename (EVOLUTION_PLUGINDIR,
			"attachment-reminder.glade",
			NULL);
	ui->xml = glade_xml_new (gladefile, "reminder_configuration_box", NULL);
	g_free (gladefile);

	ui->gconf = gconf_client_get_default ();

	ui->treeview = glade_xml_get_widget (ui->xml, "clue_treeview");

	ui->store = gtk_list_store_new (CLUE_N_COLUMNS, G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (ui->treeview), GTK_TREE_MODEL (ui->store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ui->treeview), -1, _("Keywords"),
			renderer, "text", CLUE_KEYWORD_COLUMN, NULL);
	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
	g_signal_connect(renderer, "edited", (GCallback) cell_edited_callback, ui);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (selection_changed), ui);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ui->treeview), TRUE);

	ui->clue_add = glade_xml_get_widget (ui->xml, "clue_add");
	g_signal_connect (G_OBJECT (ui->clue_add), "clicked", G_CALLBACK (clue_add_clicked), ui);

	ui->clue_remove = glade_xml_get_widget (ui->xml, "clue_remove");
	g_signal_connect (G_OBJECT (ui->clue_remove), "clicked", G_CALLBACK (clue_remove_clicked), ui);
	gtk_widget_set_sensitive (ui->clue_remove, FALSE);

	ui->clue_edit = glade_xml_get_widget (ui->xml, "clue_edit");
	g_signal_connect (G_OBJECT (ui->clue_edit), "clicked", G_CALLBACK (clue_edit_clicked), ui);
	gtk_widget_set_sensitive (ui->clue_edit, FALSE);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	g_signal_connect(G_OBJECT(model), "row-changed", G_CALLBACK(clue_check_isempty), ui);

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

	gtk_box_pack_start (GTK_BOX (hbox), glade_xml_get_widget (ui->xml, "reminder_configuration_box"), TRUE, TRUE, 0);

	/* to let free data properly on destroy of configuration widget */
	g_object_set_data_full (G_OBJECT (hbox), "myui-data", ui, destroy_ui_data);

	return hbox;
}

/* Configuration in Mail Prefs Page goes here */

GtkWidget *
org_gnome_attachment_reminder_config_option (struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data)
{
	/* This function and the hook needs to be removed,
	once the configure code is thoroughly tested */

	return NULL;

}

