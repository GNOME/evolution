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
 *		Diego Escalante Urrelo <diegoe@gnome.org>
 *		Bharath Acharya <abharath@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2008 - Diego Escalante Urrelo
 *
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>

#include <gconf/gconf-client.h>

#include <e-util/e-config.h>
#include <camel/camel-url.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream-mem.h>

#include <mail/em-composer-utils.h>
#include <mail/em-popup.h>
#include <mail/mail-session.h>
#include <mail/mail-component.h>
#include <mail/mail-ops.h>
#include <e-util/e-error.h>
#include <e-util/e-plugin.h>
#include <glade/glade.h>

#include <composer/e-msg-composer.h>

#define GCONF_KEY_TEMPLATE_PLACEHOLDERS "/apps/evolution/mail/template_placeholders"

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
	CLUE_VALUE_COLUMN,
	CLUE_N_COLUMNS
};

typedef struct {
    CamelMimeMessage *msg;
    EMPopupTargetSelect *t;
} UserData;

static gchar * get_content	(CamelMimeMessage *message);

static void reply_with_template	(EPopup *ep, EPopupItem *item, gpointer data);

static void popup_free		(EPopup *ep, GSList *l, gpointer data);

static GSList *fill_submenu	(CamelStore *store,
				CamelFolderInfo *info,
				GSList *list,
				EMPopupTargetSelect *t);

static GSList *append_to_menu	(CamelFolder *folder,
				GPtrArray *uids,
				GSList *list,
				EMPopupTargetSelect *t);

void org_gnome_templates_popup	(EPlugin *ep, EMPopupTargetSelect *t);

GtkWidget *e_plugin_lib_get_configure_widget	(EPlugin *epl);

gboolean e_plugin_ui_init	(GtkUIManager *ui_manager, EMsgComposer *composer);

/* Thanks to attachment reminder plugin for this*/
static void commit_changes (UIData *ui);

static void  key_cell_edited_callback (GtkCellRendererText *cell, gchar *path_string,
				   gchar *new_text,UIData *ui);

static void  value_cell_edited_callback (GtkCellRendererText *cell, gchar *path_string,
				   gchar *new_text,UIData *ui);

static gboolean clue_foreach_check_isempty (GtkTreeModel *model, GtkTreePath
					*path, GtkTreeIter *iter, UIData *ui);

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
		gchar *keyword, *value;
		gchar *key;

		gtk_tree_model_get (model, &iter, CLUE_KEYWORD_COLUMN, &keyword, -1);
		gtk_tree_model_get (model, &iter, CLUE_VALUE_COLUMN, &value, -1);

		/* Check if the keyword and value are not empty */
		if ((keyword) && (value) && (g_utf8_strlen(g_strstrip(keyword), -1) > 0)
			&& (g_utf8_strlen(g_strstrip(value), -1) > 0)) {
			key = g_strdup_printf("%s=%s", keyword, value);
			clue_list = g_slist_append (clue_list, key);
		}
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	gconf_client_set_list (ui->gconf, GCONF_KEY_TEMPLATE_PLACEHOLDERS, GCONF_VALUE_STRING, clue_list, NULL);

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
key_cell_edited_callback (GtkCellRendererText *cell,
		      gchar               *path_string,
		      gchar               *new_text,
		      UIData             *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *value;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));

	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	gtk_tree_model_get (model, &iter, CLUE_VALUE_COLUMN, &value, -1);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    CLUE_KEYWORD_COLUMN, new_text, CLUE_VALUE_COLUMN, value, -1);

	commit_changes (ui);
}

static void
value_cell_edited_callback (GtkCellRendererText *cell,
		      gchar               *path_string,
		      gchar               *new_text,
		      UIData             *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *keyword;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));

	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	gtk_tree_model_get (model, &iter, CLUE_KEYWORD_COLUMN, &keyword, -1);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    CLUE_KEYWORD_COLUMN, keyword, CLUE_VALUE_COLUMN, new_text, -1);

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
				    CLUE_KEYWORD_COLUMN, new_clue, CLUE_VALUE_COLUMN, new_clue, -1);

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

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	GtkCellRenderer *renderer_key, *renderer_value;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GConfClient *gconf = gconf_client_get_default();
	GtkWidget *hbox;
	GSList *clue_list = NULL, *list;
	GtkTreeModel *model;

	UIData *ui = g_new0 (UIData, 1);

	gchar *gladefile;

	gladefile = g_build_filename (EVOLUTION_PLUGINDIR,
			"templates.glade",
			NULL);
	ui->xml = glade_xml_new (gladefile, "templates_configuration_box", NULL);
	g_free (gladefile);

	ui->gconf = gconf_client_get_default ();

	ui->treeview = glade_xml_get_widget (ui->xml, "clue_treeview");

	ui->store = gtk_list_store_new (CLUE_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (ui->treeview), GTK_TREE_MODEL (ui->store));

	renderer_key = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ui->treeview), -1, _("Keywords"),
			renderer_key, "text", CLUE_KEYWORD_COLUMN, NULL);
	g_object_set (G_OBJECT (renderer_key), "editable", TRUE, NULL);
	g_signal_connect(renderer_key, "edited", (GCallback) key_cell_edited_callback, ui);

	renderer_value = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ui->treeview), -1, _("Values"),
			renderer_value, "text", CLUE_VALUE_COLUMN, NULL);
	g_object_set (G_OBJECT (renderer_value), "editable", TRUE, NULL);
	g_signal_connect(renderer_value, "edited", (GCallback) value_cell_edited_callback, ui);

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
	clue_list = gconf_client_get_list ( gconf, GCONF_KEY_TEMPLATE_PLACEHOLDERS, GCONF_VALUE_STRING, NULL );

	for (list = clue_list; list; list = g_slist_next (list)) {
		gchar **temp = g_strsplit (list->data, "=", 2);
		gtk_list_store_append (ui->store, &iter);
		gtk_list_store_set (ui->store, &iter, CLUE_KEYWORD_COLUMN, temp[0], CLUE_VALUE_COLUMN, temp[1], -1);
		g_strfreev(temp);
	}

	if (clue_list) {
		g_slist_foreach (clue_list, (GFunc) g_free, NULL);
		g_slist_free (clue_list);
	}

	/* Add the list here */

	hbox = gtk_vbox_new (FALSE, 0);

	gtk_box_pack_start (GTK_BOX (hbox), glade_xml_get_widget (ui->xml, "templates_configuration_box"), TRUE, TRUE, 0);

	/* to let free data properly on destroy of configuration widget */
	g_object_set_data_full (G_OBJECT (hbox), "myui-data", ui, destroy_ui_data);

	return hbox;
}

/* borrowed from plugins/mail-to-task/ */
static gchar *
get_content (CamelMimeMessage *message)
{
	CamelDataWrapper *content;
	CamelStream *mem;
	CamelContentType *type;
	CamelMimePart *mime_part = CAMEL_MIME_PART (message);
	gchar *str, *convert_str = NULL;
	gsize bytes_read, bytes_written;
	gint count = 2;

	content = camel_medium_get_content_object ((CamelMedium *) message);
	if (!content)
		return NULL;

	/* Get non-multipart content from multipart message. */
	while (CAMEL_IS_MULTIPART (content) && count > 0) {
		mime_part = camel_multipart_get_part (CAMEL_MULTIPART (content), 0);
		content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
		count--;
	}

	if (!mime_part)
		return NULL;

	type = camel_mime_part_get_content_type (mime_part);
	if (!camel_content_type_is (type, "text", "plain"))
		return NULL;

	mem = camel_stream_mem_new ();
	camel_data_wrapper_decode_to_stream (content, mem);

	str = g_strndup ((const gchar *)((CamelStreamMem *) mem)->buffer->data, ((CamelStreamMem *) mem)->buffer->len);
	camel_object_unref (mem);

	/* convert to UTF-8 string */
	if (str && content->mime_type->params && content->mime_type->params->value) {
		convert_str = g_convert (str, strlen (str),
				"UTF-8", content->mime_type->params->value,
				&bytes_read, &bytes_written, NULL);
	}

	if (convert_str) {
		g_free (str);
		return convert_str;
	}
	else
		return str;

}

static void
reply_with_template (EPopup *ep, EPopupItem *item, gpointer data)
{
	CamelMimeMessage *new, *template, *reply_to;
	CamelFolder *templates_folder;
	struct _camel_header_raw *header;
	UserData *userdata = item->user_data;
	gchar *cont;

	/* We get the templates folder and all the uids of the messages in there */
	templates_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_TEMPLATES);

	/* Get from the currently selected folder, the currently selected message */
	reply_to = camel_folder_get_message (userdata->t->folder,
			g_ptr_array_index (userdata->t->uids, 0),
			NULL);

	/* The message we'll be using has been stored when building the menu */
	template = userdata->msg;

	/* The new message we are creating */
	new = camel_mime_message_new();

	/* Add the headers from the message we are replying to, so CC and that
	 * stuff is preserved.
	 */
	header = ((CamelMimePart *)reply_to)->headers;
	while (header) {
		if (g_ascii_strncasecmp (header->name, "content-", 8) != 0) {
			camel_medium_add_header((CamelMedium *) new,
					header->name,
					header->value);
		}
		header = header->next;
	}

	camel_mime_part_set_encoding((CamelMimePart *) new, CAMEL_TRANSFER_ENCODING_8BIT);

	/* Get the template content. */
	cont = get_content (template);

	/* Set the To: field to the same To: field of the message we are replying to. */
	camel_mime_message_set_recipients (new, CAMEL_RECIPIENT_TYPE_TO,
			camel_mime_message_get_from (reply_to));

	/* Copy the CC and BCC from the template.*/
	camel_mime_message_set_recipients (new, CAMEL_RECIPIENT_TYPE_CC,
			camel_mime_message_get_recipients (template, CAMEL_RECIPIENT_TYPE_CC));

	camel_mime_message_set_recipients (new, CAMEL_RECIPIENT_TYPE_BCC,
			camel_mime_message_get_recipients (template, CAMEL_RECIPIENT_TYPE_BCC));

	camel_mime_part_set_content((CamelMimePart *)new,
			cont, (gint) g_utf8_strlen(cont, -1), "text");

	/* Create the composer */
	em_utils_edit_message (new, templates_folder);

	camel_object_unref(new);
}

static void
popup_free (EPopup *ep, GSList *l, gpointer data)
{
	g_slist_free (l);
}

static GSList
*append_to_menu (CamelFolder *folder, GPtrArray *uids, GSList *list, EMPopupTargetSelect *t)
{
	gint i;

	for (i = 0; i < uids->len; i++) {
		const gchar *subject;
		gchar *path;
		EPopupItem *item;
		CamelMimeMessage *message;
		UserData *user_data;
		const gchar *uid;

		uid = g_strdup (g_ptr_array_index (uids, i));

		/* Same as in fill_submenu */
		if (!g_str_has_suffix (folder->name, "Templates"))
			path = g_strdup_printf ("80.%s", folder->full_name);
		else
			path = g_strdup ("80.Templates");

		/* If this uid is trashed, ignore it */
		if (camel_folder_get_message_flags (folder, uid) & CAMEL_MESSAGE_DELETED)
			continue;

		/* Get the message for this uid */
		message = camel_folder_get_message (folder,
				uid,
				NULL);

		subject = camel_mime_message_get_subject (message);

		/* Create the menu item for it */
		item = g_slice_alloc0(sizeof(*item));
		item->type = E_POPUP_ITEM;
		item->path = g_strdup_printf ("%s/%02d", path, i);
		item->label = g_strdup ((strlen(subject) > 0) ? subject : _("No title"));
		item->visible = EM_POPUP_SELECT_MANY | EM_POPUP_SELECT_ONE;

		/* Make some info available to the callback */
		user_data = g_slice_new(UserData);
		user_data->msg = message;
		user_data->t = t;

		item->user_data = user_data;
		item->activate = reply_with_template;

		list = g_slist_prepend (list, item);
	}

	return list;
}

static GSList
*fill_submenu (CamelStore *store, CamelFolderInfo *info, GSList *list, EMPopupTargetSelect *t)
{
	while (info) {
		CamelFolder *folder;
		GPtrArray *uids;
		EPopupItem *item;

		folder = camel_store_get_folder (store, info->full_name, 0, NULL);

		item = g_slice_alloc0(sizeof(*item));
		item->type = E_POPUP_SUBMENU;
		item->label = folder->name;
		item->visible = EM_POPUP_SELECT_MANY | EM_POPUP_SELECT_ONE;

		/* To avoid having a Templates dir, we ignore the top level */
		if (!g_str_has_suffix (folder->name, "Templates"))
			item->path = g_strdup_printf ("80.%s", folder->full_name);
		else
			item->path = g_strdup ("80.Templates");

		list = g_slist_prepend (list, item);

		/* Get the uids for this folder and fill them in the menu */
		uids = camel_folder_get_uids (folder);
		list = append_to_menu (folder, uids, list, t);
		camel_folder_free_uids (folder, uids);

		/* If the folder has a child, call this function again */
		if (info->child) {
			list = fill_submenu (store, info->child, list, t);
		}

		info = info->next;
	}

	return list;
}

void
org_gnome_templates_popup (EPlugin *ep, EMPopupTargetSelect *t)
{
	CamelFolder *templates_folder;
	CamelFolderInfo *templates_info;
	CamelStore *store;

	GSList *list = NULL;

	/* We get the templates folder and all the uids of the messages in there */
	store = mail_component_peek_local_store (NULL);

	templates_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_TEMPLATES);

	templates_info = camel_store_get_folder_info (store,
			templates_folder->full_name,
			CAMEL_STORE_FOLDER_INFO_RECURSIVE | CAMEL_STORE_FOLDER_INFO_FAST,
			NULL);

	/* Get subfolders and fill it */
	list = fill_submenu (store, templates_info, list, t);

	e_popup_add_items (t->target.popup, list, NULL, popup_free, NULL);

	return;
}

static void
action_template_cb (GtkAction *action,
		EMsgComposer *composer)
{
	CamelMessageInfo *info;
	CamelMimeMessage *msg;
	CamelFolder *templates_folder;

	/* We get the templates folder and all the uids of the messages in there */
	templates_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_TEMPLATES);
	msg = e_msg_composer_get_message_draft (composer);
	info = camel_message_info_new (NULL);

	/* FIXME: what's the ~0 for? :) */
	camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DRAFT, ~0);

	mail_append_mail (templates_folder, msg, info, NULL, composer);

	return;
}

static GtkActionEntry entries[] = {

	{ "Template",
	  GTK_STOCK_SAVE,
	  N_("Save as _Template"),
	  "<Shift><Control>t",
	  N_("Save as Template"),
	  G_CALLBACK (action_template_cb) }
};

gboolean
e_plugin_ui_init (GtkUIManager *ui_manager,
                  EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	editor = GTKHTML_EDITOR (composer);

	/* Add actions to the "composer" action group. */
	gtk_action_group_add_actions (
		gtkhtml_editor_get_action_group (editor, "composer"),
		entries, G_N_ELEMENTS (entries), composer);

	return TRUE;
}
