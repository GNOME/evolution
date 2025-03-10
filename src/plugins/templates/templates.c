/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2008 - Diego Escalante Urrelo
 * Copyright (C) 2018 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *		Diego Escalante Urrelo <diegoe@gnome.org>
 *		Bharath Acharya <abharath@novell.com>
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include "e-util/e-util.h"

#include "shell/e-shell-view.h"

#include "mail/e-mail-reader.h"
#include "mail/e-mail-reader-utils.h"
#include "mail/e-mail-ui-session.h"
#include "mail/e-mail-view.h"
#include "mail/e-mail-templates.h"
#include "mail/e-mail-templates-store.h"
#include "mail/em-composer-utils.h"
#include "mail/em-utils.h"
#include "mail/message-list.h"

#include "composer/e-msg-composer.h"

#define CONF_KEY_TEMPLATE_PLACEHOLDERS "template-placeholders"

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EActivity *activity;
	EMailReader *reader;
	CamelMimeMessage *source_message;
	CamelMimeMessage *new_message;
	CamelFolder *template_folder;
	CamelFolder *source_folder;
	gchar *source_folder_uri;
	gchar *source_message_uid;
	gchar *orig_source_message_uid;
	gchar *template_message_uid;
	gboolean selection_is_html;
	EMailPartValidityFlags validity_pgp_sum;
	EMailPartValidityFlags validity_smime_sum;
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
gboolean	init_composer_actions		(EUIManager *ui_manager,
						 EMsgComposer *composer);
gboolean	init_mail_actions		(EUIManager *ui_manager,
						 EShellView *shell_view);
gboolean	init_mail_browser_actions	(EUIManager *ui_manager,
						 EMailBrowser *mail_browser);
gint		e_plugin_lib_enable		(EPlugin *plugin,
						 gboolean enabled);

#define TEMPLATES_DATA_KEY "templates::data"

typedef struct _TemplatesData {
	GWeakRef mail_reader_weakref;
	EMailTemplatesStore *templates_store;
	GMenu *reply_template_menu;
	gulong changed_handler_id;
	guint update_menu_id;
	gboolean changed;
	gboolean update_immediately;
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

		if (td->update_menu_id) {
			g_source_remove (td->update_menu_id);
			td->update_menu_id = 0;
		}

		g_clear_object (&td->templates_store);
		g_weak_ref_clear (&td->mail_reader_weakref);
		g_clear_object (&td->reply_template_menu);
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
	g_clear_object (&context->source_message);
	g_clear_object (&context->new_message);
	g_clear_object (&context->source_folder);
	g_clear_object (&context->template_folder);

	g_free (context->source_folder_uri);
	g_free (context->source_message_uid);
	g_free (context->orig_source_message_uid);
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

static void
create_new_message_composer_created_cb (GObject *source_object,
					GAsyncResult *result,
					gpointer user_data)
{
	AsyncContext *context = user_data;
	EAlertSink *alert_sink;
	EMsgComposer *composer;
	GError *error = NULL;

	g_return_if_fail (context != NULL);

	alert_sink = e_activity_get_alert_sink (context->activity);

	composer = e_msg_composer_new_finish (result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	/* Create the composer */
	em_utils_edit_message (composer, context->template_folder, context->new_message, context->source_message_uid, TRUE, FALSE);

	em_composer_utils_update_security (composer, context->validity_pgp_sum, context->validity_smime_sum);

	if (context->source_folder_uri && context->source_message_uid)
		e_msg_composer_set_source_headers (
			composer, context->source_folder_uri,
			context->source_message_uid, CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_SEEN);

	async_context_free (context);
}

static void
templates_template_applied_cb (GObject *source_object,
			       GAsyncResult *result,
			       gpointer user_data)
{
	AsyncContext *context = user_data;
	EAlertSink *alert_sink;
	EMailBackend *backend;
	EShell *shell;
	GError *error = NULL;

	g_return_if_fail (context != NULL);

	alert_sink = e_activity_get_alert_sink (context->activity);

	context->new_message = e_mail_templates_apply_finish (source_object, result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		g_warn_if_fail (context->new_message == NULL);
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (context->new_message == NULL);
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	g_warn_if_fail (context->new_message != NULL);

	backend = e_mail_reader_get_backend (context->reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

	e_msg_composer_new (shell, create_new_message_composer_created_cb, context);
}

static void
template_got_message_cb (GObject *source_object,
			 GAsyncResult *result,
			 gpointer user_data)
{
	AsyncContext *context = user_data;
	EAlertSink *alert_sink;
	CamelFolder *folder = NULL;
	CamelMimeMessage *message;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (context->activity);

	message = e_mail_reader_utils_get_selection_or_message_finish (E_MAIL_READER (source_object), result,
			NULL, &folder, NULL, NULL, &context->validity_pgp_sum, &context->validity_smime_sum, &error);

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

	context->source_message = message;

	e_mail_templates_apply (context->source_message, folder, context->orig_source_message_uid,
		context->template_folder, context->template_message_uid,
		e_activity_get_cancellable (context->activity), templates_template_applied_cb, context);
}

static void
action_reply_with_template_cb (EMailTemplatesStore *templates_store,
			       CamelFolder *template_folder,
			       const gchar *template_message_uid,
			       gpointer user_data)
{
	EMailReader *reader = user_data;
	EActivity *activity;
	AsyncContext *context;
	GCancellable *cancellable;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *message_uid;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL && uids->len == 1);
	message_uid = g_ptr_array_index (uids, 0);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	context = g_slice_new0 (AsyncContext);
	context->activity = activity;
	context->reader = g_object_ref (reader);
	context->orig_source_message_uid = g_strdup (message_uid);
	context->template_folder = g_object_ref (template_folder);
	context->template_message_uid = g_strdup (template_message_uid);

	folder = e_mail_reader_ref_folder (reader);

	em_utils_get_real_folder_uri_and_message_uid (
		folder, message_uid,
		&context->source_folder_uri,
		&context->source_message_uid);

	if (context->source_message_uid == NULL)
		context->source_message_uid = g_strdup (message_uid);

	e_mail_reader_utils_get_selection_or_message (reader, NULL, cancellable,
		template_got_message_cb, context);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static gchar *
get_account_templates_folder_uri (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	ESource *source;
	gchar *identity_uid;
	gchar *templates_folder_uri = NULL;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	table = e_msg_composer_get_header_table (composer);
	identity_uid = e_composer_header_table_dup_identity_uid (table, NULL, NULL);
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

	g_free (identity_uid);

	return templates_folder_uri;
}

typedef struct _SaveTemplateAsyncData {
	EMsgComposer *composer;
	EMailSession *session;
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	gchar *templates_folder_uri;
	gchar *delete_message_uid;
	gchar *new_message_uid;
} SaveTemplateAsyncData;

static void
save_template_async_data_free (gpointer ptr)
{
	SaveTemplateAsyncData *sta = ptr;

	if (sta) {
		if (sta->templates_folder_uri && sta->new_message_uid) {
			EHTMLEditor *editor;
			EUIAction *action;

			e_msg_composer_set_header (sta->composer, "X-Evolution-Templates-Folder", sta->templates_folder_uri);
			e_msg_composer_set_header (sta->composer, "X-Evolution-Templates-Message", sta->new_message_uid);

			editor = e_msg_composer_get_editor (sta->composer);
			action = e_html_editor_get_action (editor, "template-replace");
			if (action) {
				e_ui_action_set_visible (action, TRUE);
				e_ui_action_set_sensitive (action, TRUE);
			}
		}

		g_clear_object (&sta->composer);
		g_clear_object (&sta->session);
		g_clear_object (&sta->message);
		g_clear_object (&sta->info);
		g_free (sta->templates_folder_uri);
		g_free (sta->delete_message_uid);
		g_free (sta->new_message_uid);
		g_slice_free (SaveTemplateAsyncData, sta);
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
	gboolean success;

	if (sta->templates_folder_uri && *sta->templates_folder_uri) {
		templates_folder = e_mail_session_uri_to_folder_sync (sta->session,
			sta->templates_folder_uri, 0, cancellable, error);
		if (!templates_folder)
			return;
	}

	if (!templates_folder) {
		g_clear_pointer (&sta->templates_folder_uri, g_free);
		sta->templates_folder_uri = g_strdup (e_mail_session_get_local_folder_uri (sta->session, E_MAIL_LOCAL_FOLDER_TEMPLATES));

		success = e_mail_session_append_to_local_folder_sync (
			sta->session, E_MAIL_LOCAL_FOLDER_TEMPLATES,
			sta->message, sta->info,
			&sta->new_message_uid, cancellable, error);
	} else {
		success = e_mail_folder_append_message_sync (
			templates_folder, sta->message, sta->info,
			&sta->new_message_uid, cancellable, error);
	}

	if (success && sta->delete_message_uid && templates_folder)
		camel_folder_set_message_flags (templates_folder, sta->delete_message_uid, CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);

	g_clear_object (&templates_folder);
}

static void
got_message_draft_cb (GObject *source_object,
                      GAsyncResult *result,
		      gpointer user_data)
{
	EMsgComposer *composer = E_MSG_COMPOSER (source_object);
	gboolean replace_template = GPOINTER_TO_INT (user_data) == 1;
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

	sta = g_slice_new0 (SaveTemplateAsyncData);
	sta->composer = g_object_ref (composer);
	sta->session = g_object_ref (session);
	sta->message = message;
	sta->info = info;

	if (replace_template) {
		const gchar *existing_folder_uri;
		const gchar *existing_message_uid;

		existing_folder_uri = e_msg_composer_get_header (composer, "X-Evolution-Templates-Folder", 0);
		existing_message_uid = e_msg_composer_get_header (composer, "X-Evolution-Templates-Message", 0);

		if (existing_folder_uri && *existing_folder_uri && existing_message_uid && *existing_message_uid) {
			sta->templates_folder_uri = g_strdup (existing_folder_uri);
			sta->delete_message_uid = g_strdup (existing_message_uid);
		}
	}

	if (!sta->templates_folder_uri)
		sta->templates_folder_uri = get_account_templates_folder_uri (composer);

	html_editor = e_msg_composer_get_editor (composer);

	activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (html_editor),
			_("Saving message template"),
			"mail-composer:failed-save-template",
			NULL, save_template_thread, sta, save_template_async_data_free);

	g_clear_object (&activity);
}

static void
action_template_replace_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMsgComposer *composer = user_data;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	/* XXX Pass a GCancellable */
	e_msg_composer_get_message_draft (
		composer, G_PRIORITY_DEFAULT, NULL,
		got_message_draft_cb, GINT_TO_POINTER (1));
}

static void
action_template_save_new_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMsgComposer *composer = user_data;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	/* XXX Pass a GCancellable */
	e_msg_composer_get_message_draft (
		composer, G_PRIORITY_DEFAULT, NULL,
		got_message_draft_cb, GINT_TO_POINTER (0));
}

static void
templates_composer_realize_cb (EMsgComposer *composer,
			       gpointer user_data)
{
	EHTMLEditor *editor;
	EUIAction *action;
	const gchar *existing_folder_uri;
	const gchar *existing_message_uid;

	editor = e_msg_composer_get_editor (composer);
	action = e_html_editor_get_action (editor, "template-replace");
	if (!action)
		return;

	existing_folder_uri = e_msg_composer_get_header (composer, "X-Evolution-Templates-Folder", 0);
	existing_message_uid = e_msg_composer_get_header (composer, "X-Evolution-Templates-Message", 0);

	e_ui_action_set_visible (action, existing_folder_uri && *existing_folder_uri && existing_message_uid && *existing_message_uid);
	e_ui_action_set_sensitive (action, e_ui_action_get_visible (action));
}

static void
templates_update_menu (TemplatesData *td)
{
	EMailReader *mail_reader;

	g_return_if_fail (td != NULL);

	td->changed = FALSE;

	mail_reader = g_weak_ref_get (&td->mail_reader_weakref);

	if (mail_reader) {
		e_mail_templates_store_update_menu (td->templates_store, td->reply_template_menu, e_mail_reader_get_ui_manager (mail_reader),
			action_reply_with_template_cb, mail_reader);

		g_clear_object (&mail_reader);
	}
}

static void
templates_mail_reader_update_actions_cb (EMailReader *reader,
					 guint state,
					 gpointer user_data)
{
	TemplatesData *td;
	gboolean sensitive;

	if (!plugin_enabled)
		return;

	td = g_object_get_data (G_OBJECT (reader), TEMPLATES_DATA_KEY);
	if (td && td->changed)
		templates_update_menu (td);

	sensitive = (state & E_MAIL_READER_SELECTION_SINGLE) != 0;

	e_ui_action_set_sensitive (e_mail_reader_get_action (reader, "EPluginTemplates::mail-reply-template"), sensitive);
	e_ui_action_set_sensitive (e_mail_reader_get_action (reader, "template-use-this"), sensitive);
}

gboolean
init_composer_actions (EUIManager *ui_manager,
                       EMsgComposer *composer)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<placeholder id='pre-edit-menu'>"
		      "<submenu action='file-menu'>"
			"<placeholder id='template-holder'>"
			  "<item action='template-replace'/>"
			  "<item action='template-save-new'/>"
			"</placeholder>"
                      "</submenu>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {
		{ "template-replace",
		  "document-save",
		  N_("Save _Template"),
		  "<Shift><Control>t",
		  N_("Replace opened Template message"),
		  action_template_replace_cb, NULL, NULL, NULL },

		{ "template-save-new",
		  "document-save",
		  N_("Save as _New Template"),
		  NULL,
		  N_("Save as Template"),
		  action_template_save_new_cb, NULL, NULL, NULL }
	};

	e_ui_manager_add_actions_with_eui_data (ui_manager, "composer", GETTEXT_PACKAGE,
		entries, G_N_ELEMENTS (entries), composer, eui);

	g_signal_connect (composer, "realize",
		G_CALLBACK (templates_composer_realize_cb), NULL);

	return TRUE;
}

static gboolean
templates_update_menu_timeout_cb (gpointer user_data)
{
	TemplatesData *td = user_data;

	td->update_menu_id = 0;

	templates_update_menu (td);

	return G_SOURCE_REMOVE;
}

static void
templates_store_changed_cb (EMailTemplatesStore *templates_store,
			    gpointer user_data)
{
	TemplatesData *td = user_data;

	g_return_if_fail (td != NULL);

	td->changed = TRUE;

	if (td->update_immediately && !td->update_menu_id)
		td->update_menu_id = g_timeout_add (100, templates_update_menu_timeout_cb, td);
}

static gboolean
templates_ui_manager_create_item_cb (EUIManager *ui_manager,
				     EUIElement *elem,
				     EUIAction *action,
				     EUIElementKind for_kind,
				     GObject **out_item,
				     gpointer user_data)
{
	GMenuModel *reply_template_menu = user_data;
	const gchar *name;

	g_return_val_if_fail (G_IS_MENU (reply_template_menu), FALSE);

	name = g_action_get_name (G_ACTION (action));

	if (!g_str_has_prefix (name, "EPluginTemplates::"))
		return FALSE;

	#define is_action(_nm) (g_strcmp0 (name, (_nm)) == 0)

	if (is_action ("EPluginTemplates::mail-reply-template")) {
		*out_item = e_ui_manager_create_item_from_menu_model (ui_manager, elem, action, for_kind, reply_template_menu);
	} else if (for_kind == E_UI_ELEMENT_KIND_MENU) {
		g_warning ("%s: Unhandled menu action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		g_warning ("%s: Unhandled toolbar action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		g_warning ("%s: Unhandled headerbar action '%s'", G_STRFUNC, name);
	} else {
		g_warning ("%s: Unhandled element kind '%d' for action '%s'", G_STRFUNC, (gint) for_kind, name);
	}

	#undef is_action

	return TRUE;
}

static void
init_actions_for_mail_backend (EMailBackend *mail_backend,
			       EUIManager *ui_manager,
			       EMailReader *mail_reader,
			       gboolean update_immediately)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<placeholder id='custom-menus'>"
		      "<submenu action='mail-message-menu'>"
			"<placeholder id='mail-reply-template'>"
			  "<item action='EPluginTemplates::mail-reply-template'/>"
			"</placeholder>"
                      "</submenu>"
		    "</placeholder>"
		  "</menu>"
		  "<menu id='mail-message-popup' is-popup='true'>"
		    "<placeholder id='mail-message-popup-common-actions'>"
		      "<placeholder id='mail-reply-template'>"
			"<item action='EPluginTemplates::mail-reply-template'/>"
		      "</placeholder>"
		    "</placeholder>"
		  "</menu>"
		  "<menu id='mail-preview-popup' is-popup='true'>"
		    "<placeholder id='mail-reply-template'>"
		      "<item action='EPluginTemplates::mail-reply-template'/>"
		    "</placeholder>"
		  "</menu>"
		  "<menu id='mail-reply-group-menu'>"
		    "<placeholder id='mail-reply-template'>"
		      "<item action='EPluginTemplates::mail-reply-template'/>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {
		{ "EPluginTemplates::mail-reply-template", NULL, N_("Repl_y with Template"), NULL, NULL, NULL, NULL, NULL, NULL }
	};

	EMailSession *session;
	TemplatesData *td;

	session = e_mail_backend_get_session (mail_backend);

	td = g_new0 (TemplatesData, 1);
	td->templates_store = e_mail_templates_store_ref_default (e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session)));
	g_weak_ref_init (&td->mail_reader_weakref, mail_reader);
	td->reply_template_menu = g_menu_new ();
	td->changed_handler_id = g_signal_connect (td->templates_store, "changed", G_CALLBACK (templates_store_changed_cb), td);
	td->changed = TRUE;
	td->update_immediately = update_immediately;

	g_object_set_data_full (G_OBJECT (mail_reader), TEMPLATES_DATA_KEY, td, templates_data_free);

	g_signal_connect_data (ui_manager, "create-item",
		G_CALLBACK (templates_ui_manager_create_item_cb), g_object_ref (td->reply_template_menu),
		(GClosureNotify) g_object_unref, 0);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "templates", NULL,
		entries, G_N_ELEMENTS (entries), td, eui);

	templates_update_menu (td);
}

gboolean
init_mail_actions (EUIManager *ui_manager,
		   EShellView *shell_view)
{
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EMailView *mail_view = NULL;

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	g_object_get (shell_content, "mail-view", &mail_view, NULL);
	if (mail_view) {
		init_actions_for_mail_backend (E_MAIL_BACKEND (shell_backend), ui_manager, E_MAIL_READER (mail_view), FALSE);

		g_signal_connect (
			mail_view, "update-actions",
			G_CALLBACK (templates_mail_reader_update_actions_cb), NULL);

		g_clear_object (&mail_view);
	}

	return TRUE;
}

gboolean
init_mail_browser_actions (EUIManager *ui_manager,
			   EMailBrowser *mail_browser)
{
	EMailReader *reader = E_MAIL_READER (mail_browser);

	init_actions_for_mail_backend (e_mail_reader_get_backend (reader), ui_manager, reader, TRUE);

	return TRUE;
}

gint
e_plugin_lib_enable (EPlugin *plugin,
                     gboolean enabled)
{
	plugin_enabled = enabled;

	return 0;
}
