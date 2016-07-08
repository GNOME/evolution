/*
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include <e-util/e-util.h>

#include <shell/e-shell-view.h>

#include <mail/e-mail-reader.h>
#include <mail/e-mail-ui-session.h>
#include <mail/em-composer-utils.h>
#include <mail/em-utils.h>
#include <mail/message-list.h>

#include <composer/e-msg-composer.h>

#include "e-templates-store.h"

#define CONF_KEY_TEMPLATE_PLACEHOLDERS "template-placeholders"

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EActivity *activity;
	EMailReader *reader;
	CamelMimeMessage *message;
	CamelMimeMessage *template;
	CamelFolder *template_folder;
	gchar *source_folder_uri;
	gchar *message_uid;
	gchar *template_message_uid;
};

typedef struct {
	GSettings   *settings;
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

GtkWidget *	e_plugin_lib_get_configure_widget
						(EPlugin *plugin);
gboolean	init_composer_actions		(GtkUIManager *ui_manager,
						 EMsgComposer *composer);
gboolean	init_shell_actions		(GtkUIManager *ui_manager,
						 EShellWindow *shell_window);
gint		e_plugin_lib_enable		(EPlugin *plugin,
						 gboolean enabled);

#define TEMPLATES_DATA_KEY "templates::data"

typedef struct _TemplatesData {
	ETemplatesStore *templates_store;
	gulong changed_handler_id;
	gboolean changed;
	guint merge_id;
} TemplatesData;

static void
templates_data_free (gpointer ptr)
{
	TemplatesData *td = ptr;

	if (td) {
		if (td->templates_store && td->changed_handler_id) {
			g_signal_handler_disconnect (td->templates_store, td->changed_handler_id);
			td->changed_handler_id = 0;
		}

		g_clear_object (&td->templates_store);
		g_free (td);
	}
}

/* Thanks to attachment reminder plugin for this*/
static void commit_changes (UIData *ui);

static void  key_cell_edited_callback (GtkCellRendererText *cell, gchar *path_string,
				   gchar *new_text,UIData *ui);

static void  value_cell_edited_callback (GtkCellRendererText *cell, gchar *path_string,
				   gchar *new_text,UIData *ui);

static gboolean clue_foreach_check_isempty (GtkTreeModel *model, GtkTreePath
					*path, GtkTreeIter *iter, UIData *ui);

static gboolean plugin_enabled;

static void
async_context_free (AsyncContext *context)
{
	g_clear_object (&context->activity);
	g_clear_object (&context->reader);
	g_clear_object (&context->message);
	g_clear_object (&context->template);
	g_clear_object (&context->template_folder);

	g_free (context->source_folder_uri);
	g_free (context->message_uid);
	g_free (context->template_message_uid);

	g_slice_free (AsyncContext, context);
}

static void
selection_changed (GtkTreeSelection *selection,
                   UIData *ui)
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

	g_object_unref (ui->settings);
	g_free (ui);
}

static void
commit_changes (UIData *ui)
{
	GtkTreeModel *model = NULL;
	GVariantBuilder b;
	GtkTreeIter iter;
	gboolean valid;
	GVariant *v;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	g_variant_builder_init (&b, G_VARIANT_TYPE ("as"));
	while (valid) {
		gchar *keyword, *value;
		gchar *key;

		gtk_tree_model_get (
			model, &iter,
			CLUE_KEYWORD_COLUMN, &keyword,
			CLUE_VALUE_COLUMN, &value,
			-1);

		/* Check if the keyword and value are not empty */
		if ((keyword) && (value) && (g_utf8_strlen (g_strstrip (keyword), -1) > 0)
			&& (g_utf8_strlen (g_strstrip (value), -1) > 0)) {
			key = g_strdup_printf ("%s=%s", keyword, value);
			g_variant_builder_add (&b, "s", key);
		}

		g_free (keyword);
		g_free (value);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	/* A floating GVariant is returned, which is consumed by the g_settings_set_value() */
	v = g_variant_builder_end (&b);
	g_settings_set_value (ui->settings, CONF_KEY_TEMPLATE_PLACEHOLDERS, v);
}

static void
clue_check_isempty (GtkTreeModel *model,
                    GtkTreePath *path,
                    GtkTreeIter *iter,
                    UIData *ui)
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
clue_foreach_check_isempty (GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            UIData *ui)
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
                          gchar *path_string,
                          gchar *new_text,
                          UIData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *value;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));

	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	gtk_tree_model_get (model, &iter, CLUE_VALUE_COLUMN, &value, -1);
	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		CLUE_KEYWORD_COLUMN, new_text, CLUE_VALUE_COLUMN, value, -1);
	g_free (value);

	commit_changes (ui);
}

static void
value_cell_edited_callback (GtkCellRendererText *cell,
                            gchar *path_string,
                            gchar *new_text,
                            UIData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *keyword;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));

	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	gtk_tree_model_get (model, &iter, CLUE_KEYWORD_COLUMN, &keyword, -1);

	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		CLUE_KEYWORD_COLUMN, keyword, CLUE_VALUE_COLUMN, new_text, -1);
	g_free (keyword);

	commit_changes (ui);
}

static void
clue_add_clicked (GtkButton *button,
                  UIData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *new_clue = NULL;
	GtkTreeViewColumn *focus_col;
	GtkTreePath *path;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc) clue_foreach_check_isempty, ui);

	/* Disconnect from signal so that we can create an empty row */
	g_signal_handlers_disconnect_matched (
		model, G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, clue_check_isempty, ui);

	/* TODO : Trim and check for blank strings */
	new_clue = g_strdup ("");
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		CLUE_KEYWORD_COLUMN, new_clue, CLUE_VALUE_COLUMN, new_clue, -1);

	focus_col = gtk_tree_view_get_column (GTK_TREE_VIEW (ui->treeview), CLUE_KEYWORD_COLUMN);
	path = gtk_tree_model_get_path (model, &iter);

	if (path) {
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (ui->treeview), path, focus_col, TRUE);
		gtk_tree_view_row_activated (GTK_TREE_VIEW (ui->treeview), path, focus_col);
		gtk_tree_path_free (path);
	}

	/* We have done our job, connect back to the signal */
	g_signal_connect (
		model, "row-changed",
		G_CALLBACK (clue_check_isempty), ui);
}

static void
clue_remove_clicked (GtkButton *button,
                     UIData *ui)
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
		valid = gtk_tree_path_prev (path);

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	len = gtk_tree_model_iter_n_children (model, NULL);
	if (len > 0) {
		if (gtk_list_store_iter_is_valid (GTK_LIST_STORE (model), &iter)) {
			gtk_tree_selection_select_iter (selection, &iter);
		} else {
			if (path && valid) {
				gtk_tree_model_get_iter (model, &iter, path);
				gtk_tree_selection_select_iter (selection, &iter);
			}
		}
	} else {
		gtk_widget_set_sensitive (ui->clue_edit, FALSE);
		gtk_widget_set_sensitive (ui->clue_remove, FALSE);
	}

	gtk_widget_grab_focus (ui->treeview);
	gtk_tree_path_free (path);

	commit_changes (ui);
}

static void
clue_edit_clicked (GtkButton *button,
                   UIData *ui)
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
	GtkWidget *hbox;
	gchar **clue_list;
	gint i;
	GtkTreeModel *model;
	GtkWidget *templates_configuration_box;
	GtkWidget *clue_container;
	GtkWidget *scrolledwindow1;
	GtkWidget *clue_treeview;
	GtkWidget *vbuttonbox2;
	GtkWidget *clue_add;
	GtkWidget *clue_edit;
	GtkWidget *clue_remove;

	UIData *ui = g_new0 (UIData, 1);

	templates_configuration_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (templates_configuration_box);
	gtk_widget_set_size_request (templates_configuration_box, 385, 189);

	clue_container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_show (clue_container);
	gtk_box_pack_start (GTK_BOX (templates_configuration_box), clue_container, TRUE, TRUE, 0);

	scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow1);
	gtk_box_pack_start (GTK_BOX (clue_container), scrolledwindow1, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	clue_treeview = gtk_tree_view_new ();
	gtk_widget_show (clue_treeview);
	gtk_container_add (GTK_CONTAINER (scrolledwindow1), clue_treeview);
	gtk_container_set_border_width (GTK_CONTAINER (clue_treeview), 1);

	vbuttonbox2 = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (vbuttonbox2);
	gtk_box_pack_start (GTK_BOX (clue_container), vbuttonbox2, FALSE, TRUE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox2), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (vbuttonbox2), 6);

	clue_add = e_dialog_button_new_with_icon ("list-add", _("_Add"));
	gtk_widget_show (clue_add);
	gtk_container_add (GTK_CONTAINER (vbuttonbox2), clue_add);
	gtk_widget_set_can_default (clue_add, TRUE);

	clue_edit = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_widget_show (clue_edit);
	gtk_container_add (GTK_CONTAINER (vbuttonbox2), clue_edit);
	gtk_widget_set_can_default (clue_edit, TRUE);

	clue_remove = e_dialog_button_new_with_icon ("list-remove", _("_Remove"));
	gtk_widget_show (clue_remove);
	gtk_container_add (GTK_CONTAINER (vbuttonbox2), clue_remove);
	gtk_widget_set_can_default (clue_remove, TRUE);

	ui->settings = e_util_ref_settings ("org.gnome.evolution.plugin.templates");

	ui->treeview = clue_treeview;

	ui->store = gtk_list_store_new (CLUE_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (ui->treeview), GTK_TREE_MODEL (ui->store));

	renderer_key = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (ui->treeview), -1, _("Keywords"),
		renderer_key, "text", CLUE_KEYWORD_COLUMN, NULL);
	g_object_set (renderer_key, "editable", TRUE, NULL);
	g_signal_connect (
		renderer_key, "edited",
		(GCallback) key_cell_edited_callback, ui);

	renderer_value = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (ui->treeview), -1, _("Values"),
		renderer_value, "text", CLUE_VALUE_COLUMN, NULL);
	g_object_set (renderer_value, "editable", TRUE, NULL);
	g_signal_connect (
		renderer_value, "edited",
		(GCallback) value_cell_edited_callback, ui);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (selection_changed), ui);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ui->treeview), TRUE);

	ui->clue_add = clue_add;
	g_signal_connect (
		ui->clue_add, "clicked",
		G_CALLBACK (clue_add_clicked), ui);

	ui->clue_remove = clue_remove;
	g_signal_connect (
		ui->clue_remove, "clicked",
		G_CALLBACK (clue_remove_clicked), ui);
	gtk_widget_set_sensitive (ui->clue_remove, FALSE);

	ui->clue_edit = clue_edit;
	g_signal_connect (
		ui->clue_edit, "clicked",
		G_CALLBACK (clue_edit_clicked), ui);
	gtk_widget_set_sensitive (ui->clue_edit, FALSE);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	g_signal_connect (
		model, "row-changed",
		G_CALLBACK (clue_check_isempty), ui);

	/* Populate tree view with values from GSettings */
	clue_list = g_settings_get_strv (ui->settings, CONF_KEY_TEMPLATE_PLACEHOLDERS);

	for (i = 0; clue_list[i] != NULL; i++) {
		gchar **temp = g_strsplit (clue_list[i], "=", 2);
		gtk_list_store_append (ui->store, &iter);
		gtk_list_store_set (ui->store, &iter, CLUE_KEYWORD_COLUMN, temp[0], CLUE_VALUE_COLUMN, temp[1], -1);
		g_strfreev (temp);
	}

	g_strfreev (clue_list);

	/* Add the list here */

	hbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	gtk_box_pack_start (GTK_BOX (hbox), templates_configuration_box, TRUE, TRUE, 0);

	/* to let free data properly on destroy of configuration widget */
	g_object_set_data_full (G_OBJECT (hbox), "myui-data", ui, destroy_ui_data);

	return hbox;
}

/* Case insensitive version of strstr */
static gchar *
strstr_nocase (const gchar *haystack,
               const gchar *needle)
{
/* When _GNU_SOURCE is available, use the nonstandard extension of libc */
#ifdef _GNU_SOURCE
	g_return_val_if_fail (haystack, NULL);
	g_return_Val_if_fail (needle, NULL);

	return strcasestr (haystack, needle)
#else
/* Otherwise convert both, haystack and needle to lowercase and use good old strstr */
	gchar *l_haystack;
	gchar *l_needle;
	gchar *pos;

	g_return_val_if_fail (haystack, NULL);
	g_return_val_if_fail (needle, NULL);

	l_haystack = g_ascii_strdown (haystack, -1);
	l_needle = g_ascii_strdown (needle, -1);
	pos = strstr (l_haystack, l_needle);

	/* Get actual position of the needle in the haystack instead of l_haystack or
	 * leave it NULL */
	if (pos)
		pos = (gchar *)(haystack + (pos - l_haystack));

	g_free (l_haystack);
	g_free (l_needle);

	return pos;
#endif
}

/* Replaces $ORIG[variable] in given template by given replacement from the original message */
static void
replace_template_variable (GString *text,
                           const gchar *variable,
                           const gchar *replacement)
{
	const gchar *p, *next;
	GString *str;
	gint find_len;
	gchar *find;

	g_return_if_fail (text != NULL);
	g_return_if_fail (variable != NULL);
	g_return_if_fail (*variable);

	find = g_strconcat ("$ORIG[", variable, "]", NULL);

	find_len = strlen (find);
	str = g_string_new ("");
	p = text->str;
	while (next = strstr_nocase (p, find), next) {
		if (p < next)
			g_string_append_len (str, p, next - p);
		if (replacement && *replacement)
			g_string_append (str, replacement);
		p = next + find_len;
	}
	g_string_append (str, p);

	g_string_assign (text, str->str);

	g_string_free (str, TRUE);
	g_free (find);
}

static void
replace_email_addresses (GString *template,
                         CamelInternetAddress *internet_address,
                         const gchar *variable)
{
	gint address_index = 0;
	GString *emails = g_string_new ("");
	const gchar *address_name, *address_email;

	g_return_if_fail (template);
	g_return_if_fail (internet_address);
	g_return_if_fail (variable);

	while (camel_internet_address_get (internet_address, address_index, &address_name, &address_email)) {
		gchar *address = camel_internet_address_format_address (address_name, address_email);

		if (address_index > 0)
			g_string_append_printf (emails, ", %s", address);
		else
			g_string_append_printf (emails, "%s", address);

		address_index++;
		g_free (address);
	}
	replace_template_variable (template, variable, emails->str);
	g_string_free (emails, TRUE);
}

static CamelMimePart *
fill_template (CamelMimeMessage *message,
               CamelMimePart *template)
{
	struct _camel_header_raw *header;
	CamelContentType *ct;
	CamelStream *stream;
	CamelMimePart *return_part;
	CamelMimePart *message_part = NULL;
	CamelDataWrapper *dw;
	CamelInternetAddress *internet_address;
	GString *template_body;
	GByteArray *byte_array;
	gint i;
	gboolean message_html, template_html;

	ct = camel_mime_part_get_content_type (template);
	template_html = ct && camel_content_type_is (ct, "text", "html");

	message_html = FALSE;
	/* When template is html, then prefer HTML part of the original message. Otherwise go for plaintext */
	dw = camel_medium_get_content (CAMEL_MEDIUM (message));
	if (CAMEL_IS_MULTIPART (dw)) {
		CamelMultipart *multipart = CAMEL_MULTIPART (dw);

		for (i = 0; i < camel_multipart_get_number (multipart); i++) {
			CamelMimePart *part = camel_multipart_get_part (multipart, i);
			CamelContentType *ct = camel_mime_part_get_content_type (part);

			if (!ct)
				continue;

			if (camel_content_type_is (ct, "text", "html") && template_html) {
				message_part = camel_multipart_get_part (multipart, i);
				message_html = TRUE;
				break;
			} else if (camel_content_type_is (ct, "text", "plain") && message_html == FALSE) {
				message_part = camel_multipart_get_part (multipart, i);
			}
		}
	 } else
		message_part = CAMEL_MIME_PART (message);

	/* Get content of the template */
	stream = camel_stream_mem_new ();
	camel_data_wrapper_decode_to_stream_sync (camel_medium_get_content (CAMEL_MEDIUM (template)), stream, NULL, NULL);
	camel_stream_flush (stream, NULL, NULL);
	byte_array = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (stream));
	template_body = g_string_new_len ((gchar *) byte_array->data, byte_array->len);
	g_object_unref (stream);

	/* Replace all $ORIG[header_name] by respective values */
	header = CAMEL_MIME_PART (message)->headers;
	while (header) {
		if (g_ascii_strncasecmp (header->name, "content-", 8) != 0 &&
		    g_ascii_strncasecmp (header->name, "to", 2) != 0 &&
		    g_ascii_strncasecmp (header->name, "cc", 2) != 0 &&
		    g_ascii_strncasecmp (header->name, "bcc", 3) != 0 &&
		    g_ascii_strncasecmp (header->name, "from", 4) != 0 &&
		    g_ascii_strncasecmp (header->name, "subject", 7) != 0)
			replace_template_variable (template_body, header->name, header->value);

		header = header->next;
	}

	/* Now manually replace the *subject* header. The header->value for subject header could be
	 * base64 encoded, so let camel_mime_message to decode it for us if needed */
	replace_template_variable (template_body, "subject", camel_mime_message_get_subject (message));

	/* Replace TO and FROM modifiers. */
	internet_address = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	replace_email_addresses (template_body, internet_address, "to");

	internet_address = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	replace_email_addresses (template_body, internet_address, "cc");

	internet_address = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_BCC);
	replace_email_addresses (template_body, internet_address, "bcc");

	internet_address = camel_mime_message_get_from (message);
	replace_email_addresses (template_body, internet_address, "from");

	/* Now extract body of the original message and replace the $ORIG[body] modifier in template */
	if (message_part && strstr_nocase (template_body->str, "$ORIG[body]")) {
		GString *message_body;
		CamelStream *mem_stream;

		stream = camel_stream_mem_new ();
		mem_stream = stream;

		ct = camel_mime_part_get_content_type (message_part);
		if (ct) {
			const gchar *charset = camel_content_type_param (ct, "charset");
			if (charset && *charset) {
				CamelMimeFilter *filter = camel_mime_filter_charset_new (charset, "UTF-8");
				if (filter) {
					CamelStream *filtered = camel_stream_filter_new (stream);

					if (filtered) {
						camel_stream_filter_add (CAMEL_STREAM_FILTER (filtered), filter);
						g_object_unref (stream);
						stream = filtered;
					}

					g_object_unref (filter);
				}
			}
		}

		camel_data_wrapper_decode_to_stream_sync (camel_medium_get_content (CAMEL_MEDIUM (message_part)), stream, NULL, NULL);
		camel_stream_flush (stream, NULL, NULL);
		byte_array = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (mem_stream));
		message_body = g_string_new_len ((gchar *) byte_array->data, byte_array->len);
		g_object_unref (stream);

		if (template_html && !message_html) {
			gchar *html = camel_text_to_html (
				message_body->str,
				CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
				CAMEL_MIME_FILTER_TOHTML_MARK_CITATION |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
			g_string_assign (message_body, html);
			g_free (html);
		} else if (!template_html && message_html) {
			g_string_prepend (message_body, "<pre>");
			g_string_append (message_body, "</pre>");
		} /* Other cases should not occur. And even if they happen to do, there's nothing we can really do about it */

		replace_template_variable (template_body, "body", message_body->str);
		g_string_free (message_body, TRUE);
	} else {
		replace_template_variable (template_body, "body", "");
	}

	return_part = camel_mime_part_new ();

	if (template_html)
		camel_mime_part_set_content (return_part, template_body->str, template_body->len, "text/html");
	else
		camel_mime_part_set_content (return_part, template_body->str, template_body->len, "text/plain");

	g_string_free (template_body, TRUE);

	return return_part;
}

static CamelMimePart *
find_template_part_in_multipart (CamelMultipart *multipart,
				 CamelMultipart *new_multipart)
{
	CamelMimePart *template_part = NULL;
	gint ii;

	for (ii = 0; ii < camel_multipart_get_number (multipart); ii++) {
		CamelMimePart *part = camel_multipart_get_part (multipart, ii);
		CamelContentType *ct = camel_mime_part_get_content_type (part);

		if (!template_part && ct && camel_content_type_is (ct, "multipart", "*")) {
			CamelDataWrapper *dw;

			dw = camel_medium_get_content (CAMEL_MEDIUM (part));
			template_part = (dw && CAMEL_IS_MULTIPART (dw)) ?
				find_template_part_in_multipart (CAMEL_MULTIPART (dw), new_multipart) : NULL;

			if (!template_part) {
				/* Copy any other parts (attachments...) to the output message */
				camel_mime_part_set_disposition (part, "attachment");
				camel_multipart_add_part (new_multipart, part);
			}
		} else if (ct && camel_content_type_is (ct, "text", "html")) {
			template_part = part;
		} else if (ct && camel_content_type_is (ct, "text", "plain") && !template_part) {
			template_part = part;
		} else {
			/* Copy any other parts (attachments...) to the output message */
			camel_mime_part_set_disposition (part, "attachment");
			camel_multipart_add_part (new_multipart, part);
		}
	}

	return template_part;
}

static void
create_new_message_composer_created_cb (GObject *source_object,
					GAsyncResult *result,
					gpointer user_data)
{
	AsyncContext *context = user_data;
	EAlertSink *alert_sink;
	CamelMimeMessage *new;
	CamelMimeMessage *message;
	CamelMimeMessage *template;
	CamelMultipart *new_multipart;
	CamelDataWrapper *dw;
	struct _camel_header_raw *header;
	EMailBackend *backend;
	EMailSession *session;
	const gchar *message_uid;
	EMsgComposer *composer;
	GError *error = NULL;
	CamelMimePart *template_part = NULL;
	CamelFolder *folder;

	g_return_if_fail (context != NULL);

	alert_sink = e_activity_get_alert_sink (context->activity);

	composer = e_msg_composer_new_finish (result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		g_warn_if_fail (context->template == NULL);
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (context->template == NULL);
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	message = context->message;
	message_uid = context->message_uid;
	template = context->template;

	backend = e_mail_reader_get_backend (context->reader);
	session = e_mail_backend_get_session (backend);

	folder = e_mail_session_get_local_folder (session, E_MAIL_LOCAL_FOLDER_TEMPLATES);

	new = camel_mime_message_new ();
	new_multipart = camel_multipart_new ();
	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (new_multipart), "multipart/alternative");
	camel_multipart_set_boundary (new_multipart, NULL);

	dw = camel_medium_get_content (CAMEL_MEDIUM (template));
	/* If template is a multipart, then try to use HTML. When no HTML part is available, use plaintext. Every other
	 * add as an attachment */
	if (CAMEL_IS_MULTIPART (dw)) {
		template_part = find_template_part_in_multipart (CAMEL_MULTIPART (dw), new_multipart);
	} else {
		CamelContentType *ct = camel_mime_part_get_content_type (CAMEL_MIME_PART (template));

		if (ct && (camel_content_type_is (ct, "text", "html") ||
		    camel_content_type_is (ct, "text", "plain"))) {
			template_part = CAMEL_MIME_PART (template);
		}
	}

	g_warn_if_fail (template_part != NULL);

	if (template_part) {
		CamelMimePart *out_part = NULL;

		/* Here replace all the modifiers in template body by values
		   from message and return the newly created part */
		out_part = fill_template (message, template_part);

		/* Assigning part directly to mime_message causes problem with
		   "Content-type" header displaying in the HTML message (camel parsing bug?) */
		camel_multipart_add_part (new_multipart, out_part);
		g_object_unref (out_part);
	}

	camel_medium_set_content (CAMEL_MEDIUM (new), CAMEL_DATA_WRAPPER (new_multipart));

	/* Add the headers from the message we are replying to, so CC and that
	 * stuff is preserved. Also replace any $ORIG[header-name] modifiers ignoring
	 * 'content-*' headers */
	header = CAMEL_MIME_PART (message)->headers;
	while (header) {
		if (g_ascii_strncasecmp (header->name, "content-", 8) != 0 &&
		    g_ascii_strcasecmp (header->name, "from") != 0) {

			/* Some special handling of the 'subject' header */
			if (g_ascii_strncasecmp (header->name, "subject", 7) == 0) {
				GString *subject = g_string_new (camel_mime_message_get_subject (template));

				/* Now replace all possible $ORIG[]s in the subject line by values from original message */
				struct _camel_header_raw *m_header = CAMEL_MIME_PART (message)->headers;
				while (m_header) {
					if (g_ascii_strncasecmp (m_header->name, "content-", 8) != 0 &&
					    g_ascii_strncasecmp (m_header->name, "subject", 7) !=0)
						replace_template_variable (subject, m_header->name, m_header->value);

					m_header = m_header->next;
				}
				/* Now replace $ORIG[subject] variable, handling possible base64 encryption */
				replace_template_variable (
					subject, "subject",
					camel_mime_message_get_subject (message));
				header->value = g_strdup (subject->str);
				g_string_free (subject, TRUE);
			}

			camel_medium_add_header (
				CAMEL_MEDIUM (new),
				header->name,
				header->value);
		}

		header = header->next;
	}

	/* Set the To: field to the same To: field of the message we are replying to. */
	camel_mime_message_set_recipients (
		new, CAMEL_RECIPIENT_TYPE_TO,
		camel_mime_message_get_reply_to (message) ? camel_mime_message_get_reply_to (message) :
		camel_mime_message_get_from (message));

	/* Copy the CC and BCC from the template.*/
	camel_mime_message_set_recipients (
		new, CAMEL_RECIPIENT_TYPE_CC,
		camel_mime_message_get_recipients (
			template, CAMEL_RECIPIENT_TYPE_CC));

	camel_mime_message_set_recipients (
		new, CAMEL_RECIPIENT_TYPE_BCC,
		camel_mime_message_get_recipients (
			template, CAMEL_RECIPIENT_TYPE_BCC));

	/* Create the composer */
	em_utils_edit_message (composer, folder, new, message_uid, TRUE);
	if (composer && context->source_folder_uri && context->message_uid)
		e_msg_composer_set_source_headers (
			composer, context->source_folder_uri,
			context->message_uid, CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_SEEN);

	g_object_unref (new_multipart);
	g_object_unref (new);

	async_context_free (context);
}

static void
create_new_message (GObject *source_object,
                    GAsyncResult *result,
                    gpointer user_data)
{
	AsyncContext *context = user_data;
	EAlertSink *alert_sink;
	EMailBackend *backend;
	EShell *shell;
	CamelFolder *folder;
	GError *error = NULL;

	g_return_if_fail (CAMEL_IS_FOLDER (source_object));
	g_return_if_fail (context != NULL);

	folder = CAMEL_FOLDER (source_object);

	alert_sink = e_activity_get_alert_sink (context->activity);

	context->template = camel_folder_get_message_finish (folder, result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		g_warn_if_fail (context->template == NULL);
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (context->template == NULL);
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (context->template));

	backend = e_mail_reader_get_backend (context->reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

	e_msg_composer_new (shell, create_new_message_composer_created_cb, context);
}

static void
template_got_source_message (CamelFolder *folder,
                             GAsyncResult *result,
                             AsyncContext *context)
{
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	CamelMimeMessage *message;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (context->activity);
	cancellable = e_activity_get_cancellable (context->activity);

	message = camel_folder_get_message_finish (folder, result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		g_warn_if_fail (message == NULL);
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (message == NULL);
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	context->message = message;

	/* Now fetch the template message. */

	camel_folder_get_message (
		context->template_folder,
		context->template_message_uid,
		G_PRIORITY_DEFAULT, cancellable,
		create_new_message, context);
}

static void
action_reply_with_template_cb (ETemplatesStore *templates_store,
			       CamelFolder *template_folder,
			       const gchar *template_message_uid,
			       gpointer user_data)
{
	EActivity *activity;
	AsyncContext *context;
	GCancellable *cancellable;
	CamelFolder *folder;
	EShellView *shell_view = user_data;
	EShellContent *shell_content;
	EMailReader *reader;
	GPtrArray *uids;
	const gchar *message_uid;

	shell_content = e_shell_view_get_shell_content (shell_view);
	reader = E_MAIL_READER (shell_content);

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL && uids->len == 1);
	message_uid = g_ptr_array_index (uids, 0);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	context = g_slice_new0 (AsyncContext);
	context->activity = activity;
	context->reader = g_object_ref (reader);
	context->template_folder = g_object_ref (template_folder);
	context->template_message_uid = g_strdup (template_message_uid);

	folder = e_mail_reader_ref_folder (reader);

	em_utils_get_real_folder_uri_and_message_uid (
		folder, message_uid,
		&context->source_folder_uri,
		&context->message_uid);

	if (context->message_uid == NULL)
		context->message_uid = g_strdup (message_uid);

	camel_folder_get_message (
		folder, message_uid, G_PRIORITY_DEFAULT,
		cancellable, (GAsyncReadyCallback)
		template_got_source_message, context);

	g_clear_object (&folder);

	g_ptr_array_unref (uids);
}

static gchar *
get_account_templates_folder_uri (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	ESource *source;
	const gchar *identity_uid;
	gchar *templates_folder_uri = NULL;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	table = e_msg_composer_get_header_table (composer);
	identity_uid = e_composer_header_table_get_identity_uid (table);
	source = e_composer_header_table_ref_source (table, identity_uid);

	/* Get the selected identity's preferred Templates folder. */
	if (source != NULL) {
		ESourceMailComposition *extension;
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
		extension = e_source_get_extension (source, extension_name);
		templates_folder_uri = e_source_mail_composition_dup_templates_folder (extension);

		g_object_unref (source);
	}

	return templates_folder_uri;
}

typedef struct _SaveTemplateAsyncData {
	EMsgComposer *composer;
	EMailSession *session;
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	gchar *templates_folder_uri;
} SaveTemplateAsyncData;

static void
save_template_async_data_free (gpointer ptr)
{
	SaveTemplateAsyncData *sta = ptr;

	if (sta) {
		g_clear_object (&sta->composer);
		g_clear_object (&sta->session);
		g_clear_object (&sta->message);
		camel_message_info_unref (sta->info);
		g_free (sta->templates_folder_uri);
		g_free (sta);
	}
}

static void
save_template_thread (EAlertSinkThreadJobData *job_data,
		      gpointer user_data,
		      GCancellable *cancellable,
		      GError **error)
{
	SaveTemplateAsyncData *sta = user_data;
	CamelFolder *templates_folder = NULL;

	if (sta->templates_folder_uri && *sta->templates_folder_uri) {
		templates_folder = e_mail_session_uri_to_folder_sync (sta->session,
			sta->templates_folder_uri, 0, cancellable, error);
		if (!templates_folder)
			return;
	}

	if (!templates_folder) {
		e_mail_session_append_to_local_folder_sync (
			sta->session, E_MAIL_LOCAL_FOLDER_TEMPLATES,
			sta->message, sta->info,
			NULL, cancellable, error);
	} else {
		e_mail_folder_append_message_sync (
			templates_folder, sta->message, sta->info,
			NULL, cancellable, error);
	}

	g_clear_object (&templates_folder);
}

static void
got_message_draft_cb (EMsgComposer *composer,
                      GAsyncResult *result)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	EHTMLEditor *html_editor;
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	SaveTemplateAsyncData *sta;
	EActivity *activity;
	GError *error = NULL;

	message = e_msg_composer_get_message_draft_finish (
		composer, result, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (message == NULL);
		g_error_free (error);
		return;
	}

	if (error != NULL) {
		g_warn_if_fail (message == NULL);
		e_alert_run_dialog_for_args (
			GTK_WINDOW (composer),
			"mail-composer:no-build-message",
			error->message, NULL);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	info = camel_message_info_new (NULL);

	/* The last argument is a bit mask which tells the function
	 * which flags to modify.  In this case, ~0 means all flags.
	 * So it clears all the flags and then sets SEEN and DRAFT. */
	camel_message_info_set_flags (
		info, CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DRAFT |
		(camel_mime_message_has_attachment (message) ? CAMEL_MESSAGE_ATTACHMENTS : 0), ~0);

	sta = g_new0 (SaveTemplateAsyncData, 1);
	sta->composer = g_object_ref (composer);
	sta->session = g_object_ref (session);
	sta->message = message;
	sta->info = info;
	sta->templates_folder_uri = get_account_templates_folder_uri (composer);

	html_editor = e_msg_composer_get_editor (composer);

	activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (html_editor),
			_("Saving message template"),
			"mail-composer:failed-save-template",
			NULL, save_template_thread, sta, save_template_async_data_free);

	g_clear_object (&activity);
}

static void
action_template_cb (GtkAction *action,
                    EMsgComposer *composer)
{
	/* XXX Pass a GCancellable */
	e_msg_composer_get_message_draft (
		composer, G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) got_message_draft_cb, NULL);
}

static GtkActionEntry composer_entries[] = {

	{ "template",
	  "document-save",
	  N_("Save as _Template"),
	  "<Shift><Control>t",
	  N_("Save as Template"),
	  G_CALLBACK (action_template_cb) }
};

static void
templates_update_actions_cb (EShellView *shell_view,
			     GtkActionGroup *action_group)
{
	TemplatesData *td;

	if (!plugin_enabled)
		return;

	td = g_object_get_data (G_OBJECT (shell_view), TEMPLATES_DATA_KEY);
	if (td) {
		if (td->changed) {
			EShellWindow *shell_window;
			GtkUIManager *ui_manager;

			td->changed = FALSE;

			shell_window = e_shell_view_get_shell_window (shell_view);
			ui_manager = e_shell_window_get_ui_manager (shell_window);

			e_templates_store_build_menu (td->templates_store, shell_view, ui_manager, action_group,
				"/mail-message-popup/mail-message-templates", td->merge_id,
				action_reply_with_template_cb, shell_view);
		}
	}

	gtk_action_group_set_sensitive (action_group, TRUE);
	gtk_action_group_set_visible (action_group, TRUE);
}

gboolean
init_composer_actions (GtkUIManager *ui_manager,
                       EMsgComposer *composer)
{
	EHTMLEditor *editor;

	editor = e_msg_composer_get_editor (composer);

	/* Add actions to the "composer" action group. */
	gtk_action_group_add_actions (
		e_html_editor_get_action_group (editor, "composer"),
		composer_entries, G_N_ELEMENTS (composer_entries), composer);

	return TRUE;
}

static void
templates_store_changed_cb (ETemplatesStore *templates_store,
			    gpointer user_data)
{
	TemplatesData *td = user_data;

	g_return_if_fail (td != NULL);

	td->changed = TRUE;
}

static void
mail_shell_view_created_cb (EShellWindow *shell_window,
                            EShellView *shell_view)
{
	EMailBackend *backend;
	EMailSession *session;
	EShellBackend *shell_backend;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	TemplatesData *td;

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	e_shell_window_add_action_group (shell_window, "templates");
	action_group = e_lookup_action_group (ui_manager, "templates");

	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	td = g_new0 (TemplatesData, 1);
	td->templates_store = e_templates_store_ref_default (e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session)));
	td->changed_handler_id = g_signal_connect (td->templates_store, "changed", G_CALLBACK (templates_store_changed_cb), td);
	td->merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	td->changed = TRUE;

	g_object_set_data_full (G_OBJECT (shell_view), TEMPLATES_DATA_KEY, td, templates_data_free);

	g_signal_connect (
		shell_view, "update-actions",
		G_CALLBACK (templates_update_actions_cb), action_group);
}

gboolean
init_shell_actions (GtkUIManager *ui_manager,
                    EShellWindow *shell_window)
{
	EShellView *shell_view;

	/* Be careful not to instantiate the mail view ourselves. */
	shell_view = e_shell_window_peek_shell_view (shell_window, "mail");
	if (shell_view != NULL)
		mail_shell_view_created_cb (shell_window, shell_view);
	else
		g_signal_connect (
			shell_window, "shell-view-created::mail",
			G_CALLBACK (mail_shell_view_created_cb), NULL);

	return TRUE;
}

gint
e_plugin_lib_enable (EPlugin *plugin,
                     gboolean enabled)
{
	plugin_enabled = enabled;

	return 0;
}
