 /* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Author: Johnny Jacob <jjohnny@novell.com>
 *
 *  Copyright 2007 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

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
#include <mail/em-utils.h>

#include "widgets/misc/e-attachment-bar.h"
#include "composer/e-msg-composer.h"


#define GCONF_KEY_ATTACHMENT_REMINDER "/apps/evolution/mail/prompts/attachment_presend_check"
#define GCONF_KEY_ATTACH_REMINDER_CLUES "/apps/evolution/mail/attachment_reminder_clues"

typedef struct {
	GConfClient *gconf;
	GtkWidget   *treeview;
	GtkWidget   *clue_add;
	GtkWidget   *clue_edit;
	GtkWidget   *clue_remove;
	GtkWidget   *clue_container;
} UIData;


enum {
	CLUE_KEYWORD_COLUMN,
	CLUE_N_COLUMNS,
};

int e_plugin_lib_enable (EPluginLib *ep, int enable);
void org_gnome_evolution_attachment_reminder (EPlugin *ep, EMEventTargetComposer *t);
GtkWidget* org_gnome_attachment_reminder_config_option (struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data);

static gboolean ask_for_missing_attachment (GtkWindow *widget);
static gboolean check_for_attachment_clues (gchar *msg);
static gboolean check_for_attachment (EMsgComposer *composer);
static gchar* strip_text_msg (gchar *msg);
static void toggle_cb (GtkWidget *widget, UIData *ui);
static void commit_changes (UIData *ui);

static void  cell_edited_callback (GtkCellRendererText *cell, gchar *path_string,
				   gchar *new_text,UIData *ui);

static GtkListStore *store = NULL;

int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{
	return 0;
}

void org_gnome_evolution_attachment_reminder (EPlugin *ep, EMEventTargetComposer *t)
{
	GConfClient *gconf;
	char *rawstr = NULL, *filtered_str = NULL;

	gconf = gconf_client_get_default ();
	if (!gconf_client_get_bool (gconf, GCONF_KEY_ATTACHMENT_REMINDER, NULL))
	{
		g_object_unref (gconf);
		return;
	}
	else
		g_object_unref (gconf);

	rawstr = g_strdup (e_msg_composer_get_raw_message_text (t->composer));

	filtered_str = strip_text_msg (rawstr);

	g_free (rawstr);

	/* Set presend_check_status for the composer*/
	if (check_for_attachment_clues (filtered_str) && !check_for_attachment (t->composer))
		if (!ask_for_missing_attachment ((GtkWindow *)t->composer))
			g_object_set_data ((GObject *) t->composer, "presend_check_status", GINT_TO_POINTER(1));

	g_free (filtered_str);

	return ;
}

static gboolean ask_for_missing_attachment (GtkWindow *window)
{
	return em_utils_prompt_user(window, GCONF_KEY_ATTACHMENT_REMINDER ,"org.gnome.evolution.plugins.attachment_reminder:attachment-reminder", NULL);
}

/* check for the clues */
static gboolean check_for_attachment_clues (gchar *msg)
{
	//TODO : Add more strings. RegEx ???

	GConfClient *gconf;
	GSList *clue_list = NULL, *list;
	gboolean ret_val = FALSE;

	gconf = gconf_client_get_default ();

	//Get the list from gconf
	clue_list = gconf_client_get_list ( gconf, GCONF_KEY_ATTACH_REMINDER_CLUES, GCONF_VALUE_STRING, NULL );

	guint msg_length = strlen (msg);

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
static gboolean check_for_attachment (EMsgComposer *composer)
{
	EAttachmentBar* bar = (EAttachmentBar*)e_msg_composer_get_attachment_bar (composer);

	if (e_attachment_bar_get_num_attachments (bar))
		return TRUE;

	return FALSE;
}

static gchar* strip_text_msg (gchar *msg)
{
	gchar **lines = g_strsplit ( msg, "\n", -1);
	gchar *stripped_msg = g_strdup (" ");

	guint i=0;

	while (lines [i])
	{
		if (lines [i] != NULL && !g_str_has_prefix (g_strstrip(lines[i]), ">"))
		{
			gchar *temp = stripped_msg;
			stripped_msg = g_strconcat (" ", stripped_msg, lines[i], NULL);

			g_free (temp);
		}
		i++;
	}

	g_strfreev (lines);

	return g_utf8_strdown (stripped_msg, g_utf8_strlen (stripped_msg, -1));
}

static void
commit_changes (UIData *ui)
{
	GtkTreeModel *model = NULL;
	GSList *clue_list = NULL;
	GtkTreeIter iter;
	gboolean valid;
	GConfClient *client;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		char *keyword;

		gtk_tree_model_get (model, &iter, CLUE_KEYWORD_COLUMN, &keyword, -1);
		clue_list = g_slist_append (clue_list, keyword);
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	client = gconf_client_get_default ();
	gconf_client_set_list (client, GCONF_KEY_ATTACH_REMINDER_CLUES, GCONF_VALUE_STRING, clue_list, NULL);

	g_slist_foreach (clue_list, (GFunc) g_free, NULL);
	g_slist_free (clue_list);
}

static void  cell_edited_callback (GtkCellRendererText *cell,
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

	if (new_text == NULL)
		g_warning ("foobar : we hae a null string here");
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

	//TODO : Trim and check for blank strings
	new_clue = g_strdup ("");
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    CLUE_KEYWORD_COLUMN, new_clue, -1);

	focus_col = gtk_tree_view_get_column (GTK_TREE_VIEW (ui->treeview), CLUE_KEYWORD_COLUMN);	
	path = gtk_tree_model_get_path (model, &iter);

	if (path) {
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (ui->treeview), path, focus_col, TRUE);

	}

}

static void
clue_remove_clicked (GtkButton *button, UIData *ui)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint len;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	len = gtk_tree_model_iter_n_children (model, NULL);
	if (len > 0) {
		gtk_tree_selection_select_iter (selection, &iter);
	} else {
		gtk_widget_set_sensitive (ui->clue_edit, FALSE);
		gtk_widget_set_sensitive (ui->clue_remove, FALSE);
	}

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
toggle_cb (GtkWidget *widget, UIData *ui)
{

	gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	GConfClient *gconf = gconf_client_get_default();

   	gconf_client_set_bool (gconf, GCONF_KEY_ATTACHMENT_REMINDER, active, NULL);
	gtk_widget_set_sensitive (ui->clue_container, active);
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

/* Configuration in Mail Prefs Page goes here */

GtkWidget *
org_gnome_attachment_reminder_config_option (struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data)
{
	GtkVBox *parent_container = (GtkVBox *) (data->parent);
	GladeXML *xml;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
    	GConfClient *gconf = gconf_client_get_default();
	GtkWidget *hbox, *button;
	GSList *clue_list = NULL, *list;
	gboolean enable_ui;

	UIData *ui = g_new0 (UIData, 1);

	char *gladefile;

	gladefile = g_build_filename (EVOLUTION_PLUGINDIR,
				      "attachment-reminder.glade",
				      NULL);
	xml = glade_xml_new (gladefile, "reminder_configuration_box", NULL);
	g_free (gladefile);

	if (data->old)
                return data->old;
		
	
	ui->gconf = gconf_client_get_default ();
	enable_ui = gconf_client_get_bool (ui->gconf, GCONF_KEY_ATTACHMENT_REMINDER, NULL);

	ui->treeview = glade_xml_get_widget (xml, "clue_treeview");

	if (store == NULL)
		store = gtk_list_store_new (CLUE_N_COLUMNS, G_TYPE_STRING);
	else 
		gtk_list_store_clear (store);

	gtk_tree_view_set_model (GTK_TREE_VIEW (ui->treeview), GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ui->treeview), -1, _("Keywords"),
			                             renderer, "text", CLUE_KEYWORD_COLUMN, NULL);
	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
	g_signal_connect(renderer, "edited", (GCallback) cell_edited_callback, ui);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (selection_changed), ui);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ui->treeview), TRUE);

	ui->clue_add = glade_xml_get_widget (xml, "clue_add");
	g_signal_connect (G_OBJECT (ui->clue_add), "clicked", G_CALLBACK (clue_add_clicked), ui);

	ui->clue_remove = glade_xml_get_widget (xml, "clue_remove");
	g_signal_connect (G_OBJECT (ui->clue_remove), "clicked", G_CALLBACK (clue_remove_clicked), ui);
	gtk_widget_set_sensitive (ui->clue_remove, FALSE);

	ui->clue_edit = glade_xml_get_widget (xml, "clue_edit");
	g_signal_connect (G_OBJECT (ui->clue_edit), "clicked", G_CALLBACK (clue_edit_clicked), ui);
	gtk_widget_set_sensitive (ui->clue_edit, FALSE);

	/* Populate tree view with values from gconf */
	clue_list = gconf_client_get_list ( gconf, GCONF_KEY_ATTACH_REMINDER_CLUES, GCONF_VALUE_STRING, NULL );

	for (list = clue_list; list; list = g_slist_next (clue_list)) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    CLUE_KEYWORD_COLUMN, list->data, -1);
	}

	if (clue_list) {
		g_slist_foreach (clue_list, (GFunc) g_free, NULL);
		g_slist_free (clue_list);
	}

	/* Enable / Disable */
	gconf = gconf_client_get_default ();
	button = glade_xml_get_widget (xml, "reminder_enable_check");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button) , enable_ui);
	g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (toggle_cb), ui);

	/* Add the list here */
	ui->clue_container = glade_xml_get_widget (xml, "clue_container");
	gtk_widget_set_sensitive (ui->clue_container, enable_ui);

	hbox = glade_xml_get_widget (xml, "reminder_configuration_box");
	gtk_box_pack_start (GTK_BOX (parent_container), hbox, FALSE, FALSE, 0);
	gtk_widget_show_all (hbox);

	return (GtkWidget *)hbox;
}

