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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <calendar/gui/e-cal-config.h>

#include <shell/e-shell.h>
#include <shell/e-shell-view.h>

#include "url-editor-dialog.h"
#include "publish-format-fb.h"
#include "publish-format-ical.h"

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

static GtkListStore *store = NULL;
static GHashTable *uri_timeouts = NULL;
static GSList *publish_uris = NULL;
static GSList *queued_publishes = NULL;
static gint online = 0;

static GSList *error_queue = NULL;
static GMutex error_queue_lock;
static guint error_queue_show_idle_id = 0;
static void  error_queue_add (gchar *descriptions, GError *error);

gint          e_plugin_lib_enable (EPlugin *ep, gint enable);
GtkWidget   *publish_calendar_locations (EPlugin *epl, EConfigHookItemFactoryData *data);
static void  update_timestamp (EPublishUri *uri);
static void publish (EPublishUri *uri, gboolean can_report_success);

static GtkStatusIcon *status_icon = NULL;
static guint status_icon_timeout_id = 0;
#ifdef HAVE_LIBNOTIFY
static NotifyNotification *notify = NULL;

static gboolean
show_notify_cb (gpointer data)
{
	return notify && !notify_notification_show (notify, NULL);
}
#endif

static gboolean
remove_notification (gpointer data)
{
	if (status_icon_timeout_id)
		g_source_remove (status_icon_timeout_id);
	status_icon_timeout_id = 0;

#ifdef HAVE_LIBNOTIFY
	if (notify)
		notify_notification_close (notify, NULL);
	notify = NULL;
#endif

	gtk_status_icon_set_visible (status_icon, FALSE);
	g_object_unref (status_icon);
	status_icon = NULL;

	return FALSE;
}

static void
update_publish_notification (GtkMessageType msg_type,
                             const gchar *msg_text)
{
	static GString *actual_msg = NULL;
	#ifdef HAVE_LIBNOTIFY
	static gboolean can_notify = TRUE;
	#endif
	gboolean new_icon = !status_icon;
	const gchar *icon_name;

	g_return_if_fail (msg_text != NULL);

	if (new_icon) {
		status_icon = gtk_status_icon_new ();
		if (actual_msg) {
			g_string_free (actual_msg, TRUE);
			actual_msg = NULL;
		}
	} else if (status_icon_timeout_id) {
		g_source_remove (status_icon_timeout_id);
	}

	switch (msg_type) {
	case GTK_MESSAGE_WARNING:
		icon_name = "dialog-warning";
		break;
	case GTK_MESSAGE_ERROR:
		icon_name = "dialog-error";
		break;
	default:
		icon_name = "dialog-information";
		break;
	}

	if (!actual_msg) {
		actual_msg = g_string_new (msg_text);
	} else {
		g_string_append_c (actual_msg, '\n');
		g_string_append (actual_msg, msg_text);
	}

	gtk_status_icon_set_from_icon_name (status_icon, icon_name);
	gtk_status_icon_set_tooltip_text (status_icon, actual_msg->str);

#ifdef HAVE_LIBNOTIFY
	if (can_notify) {
		if (notify) {
			notify_notification_update (notify, _("Calendar Publishing"), actual_msg->str, icon_name);
		} else {
			if (!notify_init ("evolution-publish-calendar")) {
				can_notify = FALSE;
				return;
			}

			notify = notify_notification_new (_("Calendar Publishing"), actual_msg->str, icon_name);
			notify_notification_set_urgency (notify, NOTIFY_URGENCY_NORMAL);
			notify_notification_set_timeout (notify, NOTIFY_EXPIRES_DEFAULT);
			notify_notification_set_hint (
				notify, "desktop-entry",
				g_variant_new_string ("org.gnome.Evolution"));
			e_named_timeout_add (500, show_notify_cb, NULL);

			g_signal_connect (
				notify, "closed",
				G_CALLBACK (remove_notification), NULL);
		}
	}
#endif

	status_icon_timeout_id =
		e_named_timeout_add_seconds (15, remove_notification, NULL);

	if (new_icon) {
		g_signal_connect (
			status_icon, "activate",
			G_CALLBACK (remove_notification), NULL);
	}
}

static void
publish_no_succ_info (EPublishUri *uri)
{
	publish (uri, FALSE);
}

static void
publish_uri_async (EPublishUri *uri)
{
	GThread *thread = NULL;
	GError *error = NULL;

	thread = g_thread_try_new (
		NULL, (GThreadFunc) publish_no_succ_info, uri, &error);
	if (error != NULL) {
		g_warning (G_STRLOC ": %s", error->message);
		g_error_free (error);
	} else {
		g_thread_unref (thread);
	}
}

static void
publish_online (EPublishUri *uri,
                GFile *file,
                GError **perror,
                gboolean can_report_success)
{
	GOutputStream *stream;
	GError *error = NULL;

	stream = G_OUTPUT_STREAM (g_file_replace (
		file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));

	/* Sanity check. */
	g_return_if_fail (
		((stream != NULL) && (error == NULL)) ||
		((stream == NULL) && (error != NULL)));

	if (error != NULL) {
		if (perror != NULL) {
			*perror = error;
		} else {
			error_queue_add (
				g_strdup_printf (
					_("Could not open %s:"),
					uri->location),
				error);
		}
		return;
	}

	switch (uri->publish_format) {
		case URI_PUBLISH_AS_ICAL:
			publish_calendar_as_ical (stream, uri, &error);
			break;
		case URI_PUBLISH_AS_FB:
		case URI_PUBLISH_AS_FB_WITH_DETAILS:
			publish_calendar_as_fb (stream, uri, &error);
			break;
	}

	if (error != NULL)
		error_queue_add (
			g_strdup_printf (
				_("There was an error while publishing to %s:"),
				uri->location),
			error);
	else if (can_report_success)
		error_queue_add (
			g_strdup_printf (
				_("Publishing to %s finished successfully"),
				uri->location),
			NULL);

	update_timestamp (uri);

	g_output_stream_close (stream, NULL, NULL);
	g_object_unref (stream);
}

static void
unmount_done_cb (GObject *source_object,
                 GAsyncResult *result,
                 gpointer user_data)
{
	GError *error = NULL;

	g_mount_unmount_with_operation_finish (G_MOUNT (source_object), result, &error);

	if (error != NULL) {
		g_warning ("Unmount failed: %s", error->message);
		g_error_free (error);
	}

	g_object_unref (source_object);
}

struct mnt_struct {
	EPublishUri *uri;
	GFile *file;
	GMountOperation *mount_op;
	gboolean can_report_success;
};

static void
mount_ready_cb (GObject *source_object,
                GAsyncResult *result,
                gpointer user_data)
{
	struct mnt_struct *ms = (struct mnt_struct *) user_data;
	GError *error = NULL;
	GMount *mount;

	g_return_if_fail (ms != NULL);

	g_file_mount_enclosing_volume_finish (G_FILE (source_object), result, &error);

	if (error != NULL) {
		error_queue_add (
			g_strdup_printf (_("Mount of %s failed:"), ms->uri->location),
			error);
	} else {
		publish_online (ms->uri, ms->file, NULL, ms->can_report_success);

		mount = g_file_find_enclosing_mount (G_FILE (source_object), NULL, NULL);
		if (mount)
			g_mount_unmount_with_operation (mount, G_MOUNT_UNMOUNT_NONE, NULL, NULL, unmount_done_cb, NULL);
	}

	g_clear_object (&ms->file);
	g_clear_object (&ms->mount_op);
	g_free (ms);
}

static void
ask_password (GMountOperation *op,
              const gchar *message,
              const gchar *default_user,
              const gchar *default_domain,
              GAskPasswordFlags flags,
              gpointer user_data)
{
	struct mnt_struct *ms = (struct mnt_struct *) user_data;
	const gchar *username;
	gchar *password;
	gboolean req_pass = FALSE;
	GUri *guri;

	g_return_if_fail (ms != NULL);

	/* we can ask only for a password */
	if ((flags & G_ASK_PASSWORD_NEED_PASSWORD) == 0)
		return;

	guri = g_uri_parse (ms->uri->location, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	g_return_if_fail (guri != NULL);

	username = g_uri_get_user (guri);
	password = e_passwords_get_password (ms->uri->location);
	req_pass =
		((username && *username) &&
		!(ms->uri->service_type == TYPE_ANON_FTP &&
		!strcmp (username, "anonymous")));

	if (!password && req_pass) {
		gboolean remember = FALSE;

		password = e_passwords_ask_password (
			_("Enter password"),
			ms->uri->location, message,
			E_PASSWORDS_REMEMBER_FOREVER |
			E_PASSWORDS_SECRET |
			E_PASSWORDS_ONLINE,
			&remember, NULL);

		if (!password) {
			/* user canceled password dialog */
			g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
			g_uri_unref (guri);

			return;
		}
	}

	if (!req_pass)
		g_mount_operation_set_anonymous (op, TRUE);
	else {
		g_mount_operation_set_anonymous (op, FALSE);
		g_mount_operation_set_username  (op, username);
		g_mount_operation_set_password  (op, password);
	}

	g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);

	g_uri_unref (guri);
}

static void
ask_question (GMountOperation *op,
              const gchar *message,
              const gchar *choices[])
{
	/* this has been stolen from file-chooser */
	GtkWidget *dialog;
	gint cnt, len;
	gchar *primary;
	const gchar *secondary = NULL;
	gint res;

	primary = strstr (message, "\n");
	if (primary) {
		secondary = primary + 1;
		primary = g_strndup (message, strlen (message) - strlen (primary));
	}

	dialog = gtk_message_dialog_new (
		NULL,
		0, GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_NONE, "%s", primary);
	g_free (primary);

	if (secondary) {
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  "%s", secondary);
	}

	if (choices) {
		/* First count the items in the list then
		 * add the buttons in reverse order */
		for (len = 0; choices[len] != NULL; len++) {
			;
		}

		for (cnt = len - 1; cnt >= 0; cnt--) {
			gtk_dialog_add_button (GTK_DIALOG (dialog), choices[cnt], cnt);
		}
	}

	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res >= 0) {
		g_mount_operation_set_choice (op, res);
		g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
	} else {
		g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
mount_first (EPublishUri *uri,
             GFile *file,
             gboolean can_report_success)
{
	struct mnt_struct *ms = g_malloc (sizeof (struct mnt_struct));
	GFile *path;

	ms->uri = uri;
	ms->file = g_object_ref (file);
	ms->mount_op = g_mount_operation_new ();
	ms->can_report_success = can_report_success;

	g_signal_connect (
		ms->mount_op, "ask-password",
		G_CALLBACK (ask_password), ms);
	g_signal_connect (
		ms->mount_op, "ask-question",
		G_CALLBACK (ask_question), ms);

	path = g_file_get_parent (file);
	/* this should not happen, because the 'file' is a file,
	   but to get UI failure, not on the terminal... */
	if (!path)
		path = g_object_ref (file);

	/* mount the path, not the file to be saved into that path */
	g_file_mount_enclosing_volume (path, G_MOUNT_MOUNT_NONE, ms->mount_op, NULL, mount_ready_cb, ms);

	g_clear_object (&path);
}

static void
publish (EPublishUri *uri,
         gboolean can_report_success)
{
	if (online) {
		GError *error = NULL;
		GFile *file;

		if (g_slist_find (queued_publishes, uri))
			queued_publishes = g_slist_remove (queued_publishes, uri);

		if (!uri->enabled)
			return;

		file = g_file_new_for_uri (uri->location);

		g_return_if_fail (file != NULL);

		publish_online (uri, file, &error, can_report_success);

		if (error && error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_MOUNTED) {
			g_error_free (error);
			error = NULL;

			mount_first (uri, file, can_report_success);
		}

		if (error != NULL)
			error_queue_add (
				g_strdup_printf (
					_("Could not open %s:"),
					uri->location),
				error);

		g_object_unref (file);
	} else {
		if (g_slist_find (queued_publishes, uri) == NULL)
			queued_publishes = g_slist_prepend (queued_publishes, uri);
	}
}

typedef struct {
	GSettings *settings;
	GtkWidget *treeview;
	GtkWidget *url_add;
	GtkWidget *url_edit;
	GtkWidget *url_remove;
	GtkWidget *url_enable;
} PublishUIData;

static void
add_timeout (EPublishUri *uri)
{
	guint id;

	/* Set the timeout for now+frequency */
	switch (uri->publish_frequency) {
	case URI_PUBLISH_DAILY:
		id = e_named_timeout_add_seconds (
			24 * 60 * 60, (GSourceFunc) publish, uri);
		g_hash_table_insert (uri_timeouts, uri, GUINT_TO_POINTER (id));
		break;
	case URI_PUBLISH_WEEKLY:
		id = e_named_timeout_add_seconds (
			7 * 24 * 60 * 60, (GSourceFunc) publish, uri);
		g_hash_table_insert (uri_timeouts, uri, GUINT_TO_POINTER (id));
		break;
	}
}

static void
update_timestamp (EPublishUri *uri)
{
	GSettings *settings;
	gchar **set_uris;
	GPtrArray *uris_array;
	gboolean found = FALSE;
	gchar *xml;
	gint ii;
	guint id;

	/* Remove timeout if we have one */
	id = GPOINTER_TO_UINT (g_hash_table_lookup (uri_timeouts, uri));
	if (id) {
		g_source_remove (id);
		add_timeout (uri);
	}

	/* Update timestamp in settings */
	xml = e_publish_uri_to_xml (uri);

	g_free (uri->last_pub_time);
	uri->last_pub_time = g_strdup_printf ("%d", (gint) time (NULL));

	uris_array = g_ptr_array_new_full (3, g_free);
	settings = e_util_ref_settings (PC_SETTINGS_ID);
	set_uris = g_settings_get_strv (settings, PC_SETTINGS_URIS);

	for (ii = 0; set_uris && set_uris[ii]; ii++) {
		const gchar *d = set_uris[ii];

		if (!found && g_str_equal (d, xml)) {
			found = TRUE;
			g_ptr_array_add (uris_array, e_publish_uri_to_xml (uri));
		} else {
			g_ptr_array_add (uris_array, g_strdup (d));
		}
	}

	g_strfreev (set_uris);
	g_free (xml);

	/* this should not happen, right? */
	if (!found)
		g_ptr_array_add (uris_array, e_publish_uri_to_xml (uri));
	g_ptr_array_add (uris_array, NULL);

	g_settings_set_strv (settings, PC_SETTINGS_URIS, (const gchar * const *) uris_array->pdata);

	g_object_unref (settings);
	g_ptr_array_free (uris_array, TRUE);
}

static void
add_offset_timeout (EPublishUri *uri)
{
	guint id;
	time_t offset = atoi (uri->last_pub_time);
	time_t current = time (NULL);
	gint elapsed = current - offset;

	switch (uri->publish_frequency) {
	case URI_PUBLISH_DAILY:
		if (elapsed > 24 * 60 * 60) {
			publish (uri, FALSE);
			add_timeout (uri);
		} else {
			id = e_named_timeout_add_seconds (
				24 * 60 * 60 - elapsed,
				(GSourceFunc) publish, uri);
			g_hash_table_insert (uri_timeouts, uri, GUINT_TO_POINTER (id));
			break;
		}
		break;
	case URI_PUBLISH_WEEKLY:
		if (elapsed > 7 * 24 * 60 * 60) {
			publish (uri, FALSE);
			add_timeout (uri);
		} else {
			id = e_named_timeout_add_seconds (
				7 * 24 * 60 * 60 - elapsed,
				(GSourceFunc) publish, uri);
			g_hash_table_insert (uri_timeouts, uri, GUINT_TO_POINTER (id));
			break;
		}
		break;
	}
}

static void
url_list_changed (PublishUIData *ui)
{
	GtkTreeModel *model = NULL;
	GPtrArray *uris;
	GtkTreeIter iter;
	gboolean valid;
	GSettings *settings;

	uris = g_ptr_array_new_full (3, g_free);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid) {
		EPublishUri *url;
		gchar *xml;

		gtk_tree_model_get (model, &iter, URL_LIST_URL_COLUMN, &url, -1);

		if ((xml = e_publish_uri_to_xml (url)) != NULL)
			g_ptr_array_add (uris, xml);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	g_ptr_array_add (uris, NULL);

	settings = e_util_ref_settings (PC_SETTINGS_ID);
	g_settings_set_strv (settings, PC_SETTINGS_URIS, (const gchar * const *) uris->pdata);
	g_object_unref (settings);

	g_ptr_array_free (uris, TRUE);
}

static void
url_list_enable_toggled (GtkCellRendererToggle *renderer,
                         const gchar *path_string,
                         PublishUIData *ui)
{
	EPublishUri *url = NULL;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	path = gtk_tree_path_new_from_string (path_string);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_model_get (model, &iter, URL_LIST_URL_COLUMN, &url, -1);

		url->enabled = !url->enabled;

		gtk_list_store_set (GTK_LIST_STORE (model), &iter, URL_LIST_ENABLED_COLUMN, url->enabled, -1);

		url_list_changed (ui);
	}

	gtk_tree_path_free (path);
}

static void
selection_changed (GtkTreeSelection *selection,
                   PublishUIData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	EPublishUri *url = NULL;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, URL_LIST_URL_COLUMN, &url, -1);
		gtk_widget_set_sensitive (ui->url_edit, TRUE);
		gtk_widget_set_sensitive (ui->url_remove, TRUE);
	} else {
		gtk_widget_set_sensitive (ui->url_edit, FALSE);
		gtk_widget_set_sensitive (ui->url_remove, FALSE);
	}
}

static void
url_add_clicked (GtkButton *button,
                 PublishUIData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *url_editor;
	EPublishUri *uri;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	url_editor = url_editor_dialog_new (model, NULL);

	if (url_editor_dialog_run ((UrlEditorDialog *) url_editor)) {
		uri = URL_EDITOR_DIALOG (url_editor)->uri;
		if (uri->location) {
			gtk_list_store_append (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (
				GTK_LIST_STORE (model), &iter,
				URL_LIST_ENABLED_COLUMN, uri->enabled,
				URL_LIST_LOCATION_COLUMN, uri->location,
				URL_LIST_URL_COLUMN, uri, -1);
			url_list_changed (ui);
			publish_uris = g_slist_prepend (publish_uris, uri);
			add_timeout (uri);
			publish_uri_async (uri);
		} else {
			g_free (uri);
		}
	}
	gtk_widget_destroy (url_editor);
}

static void
url_edit_clicked (GtkButton *button,
                  PublishUIData *ui)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		EPublishUri *uri;
		GtkWidget *url_editor;
		guint id;

		gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 2, &uri, -1);
		url_editor = url_editor_dialog_new (model, uri);

		if (url_editor_dialog_run ((UrlEditorDialog *) url_editor)) {
			gtk_list_store_set (
				GTK_LIST_STORE (model), &iter,
				URL_LIST_ENABLED_COLUMN, uri->enabled,
				URL_LIST_LOCATION_COLUMN, uri->location,
				URL_LIST_URL_COLUMN, uri, -1);

			id = GPOINTER_TO_UINT (g_hash_table_lookup (uri_timeouts, uri));
			if (id)
				g_source_remove (id);
			add_timeout (uri);
			url_list_changed (ui);
			publish_uri_async (uri);
		}

		gtk_widget_destroy (url_editor);
	}
}

static void
url_list_double_click (GtkTreeView *treeview,
                       GtkTreePath *path,
                       GtkTreeViewColumn *column,
                       PublishUIData *ui)
{
	url_edit_clicked (NULL, ui);
}

static void
url_remove_clicked (GtkButton *button,
                    PublishUIData *ui)
{
	EPublishUri *url = NULL;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *confirm;
	gint response;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, URL_LIST_URL_COLUMN, &url, -1);

	confirm = gtk_message_dialog_new (
		NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
		_("Are you sure you want to remove this location?"));
	gtk_dialog_add_button (GTK_DIALOG (confirm), _("_Cancel"), GTK_RESPONSE_NO);
	gtk_dialog_add_button (GTK_DIALOG (confirm), _("_Remove"), GTK_RESPONSE_YES);
	gtk_dialog_set_default_response (GTK_DIALOG (confirm), GTK_RESPONSE_CANCEL);

	response = gtk_dialog_run (GTK_DIALOG (confirm));
	gtk_widget_destroy (confirm);

	if (response == GTK_RESPONSE_YES) {
		gint len;
		guint id;
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

		len = gtk_tree_model_iter_n_children (model, NULL);
		if (len > 0) {
			gtk_tree_selection_select_iter (selection, &iter);
		} else {
			gtk_widget_set_sensitive (ui->url_edit, FALSE);
			gtk_widget_set_sensitive (ui->url_remove, FALSE);
		}

		publish_uris = g_slist_remove (publish_uris, url);
		id = GPOINTER_TO_UINT (g_hash_table_lookup (uri_timeouts, url));
		if (id)
			g_source_remove (id);

		g_free (url);
		url_list_changed (ui);
	}
}

static void
online_state_changed (EShell *shell)
{
	online = e_shell_get_online (shell);
	if (online)
		while (queued_publishes)
			publish (queued_publishes->data, FALSE);
}

GtkWidget *
publish_calendar_locations (EPlugin *epl,
                            EConfigHookItemFactoryData *data)
{
	GtkBuilder *builder;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkWidget *toplevel;
	PublishUIData *ui = g_new0 (PublishUIData, 1);
	GSList *l;
	GtkTreeIter iter;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "publish-calendar.ui");

	ui->treeview = e_builder_get_widget (builder, "url list");
	if (store == NULL)
		store = gtk_list_store_new (URL_LIST_N_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
	else
		gtk_list_store_clear (store);

	gtk_tree_view_set_model (GTK_TREE_VIEW (ui->treeview), GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (renderer, "activatable", TRUE, NULL);
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (ui->treeview), -1, _("Enabled"),
		renderer, "active", URL_LIST_ENABLED_COLUMN, NULL);
	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (url_list_enable_toggled), ui);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (ui->treeview), -1, _("Location"),
		renderer, "text", URL_LIST_LOCATION_COLUMN, NULL);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (selection_changed), ui);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ui->treeview), TRUE);
	g_signal_connect (
		ui->treeview, "row-activated",
		G_CALLBACK (url_list_double_click), ui);

	ui->url_add = e_builder_get_widget (builder, "url add");
	ui->url_edit = e_builder_get_widget (builder, "url edit");
	ui->url_remove = e_builder_get_widget (builder, "url remove");
	g_signal_connect (
		ui->url_add, "clicked",
		G_CALLBACK (url_add_clicked), ui);
	g_signal_connect (
		ui->url_edit, "clicked",
		G_CALLBACK (url_edit_clicked), ui);
	g_signal_connect (
		ui->url_remove, "clicked",
		G_CALLBACK (url_remove_clicked), ui);

	gtk_widget_set_sensitive (GTK_WIDGET (ui->url_edit), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (ui->url_remove), FALSE);

	l = publish_uris;
	while (l) {
		EPublishUri *url = (EPublishUri *) l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			URL_LIST_ENABLED_COLUMN, url->enabled,
			URL_LIST_LOCATION_COLUMN, url->location,
			URL_LIST_URL_COLUMN, url, -1);

		l = g_slist_next (l);
	}
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
		gtk_tree_selection_select_iter (selection, &iter);

	toplevel = e_builder_get_widget (builder, "toplevel");
	gtk_widget_show_all (toplevel);
	gtk_box_pack_start (GTK_BOX (data->parent), toplevel, FALSE, TRUE, 0);

	g_object_unref (builder);

	g_object_set_data_full (G_OBJECT (toplevel), "publish-calendar-ui-data", ui, g_free);

	return toplevel;
}

static gpointer
publish_urls (gpointer data)
{
	GSList *l;

	for (l = publish_uris; l; l = g_slist_next (l)) {
		EPublishUri *uri = l->data;
		publish (uri, TRUE);
	}

	return GINT_TO_POINTER (0);
}

static gpointer
publish_uris_set_timeout (gchar **uris)
{
	gint ii;

	uri_timeouts = g_hash_table_new (g_direct_hash, g_direct_equal);

	for (ii = 0; uris && uris[ii]; ii++) {
		const gchar *xml = uris[ii];
		EPublishUri *uri = e_publish_uri_from_xml (xml);

		if (!uri->location) {
			g_free (uri);
			continue;
		}

		publish_uris = g_slist_prepend (publish_uris, uri);

		/* Add a timeout based on the last publish time */
		add_offset_timeout (uri);
	}

	g_strfreev (uris);

	return NULL;
}

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	EShell *shell = e_shell_get_default ();

	if (shell) {
		static gulong notify_online_id = 0;

		e_signal_disconnect_notify_handler (shell, &notify_online_id);
		if (enable) {
			online = e_shell_get_online (shell);
			notify_online_id = e_signal_connect_notify (
				shell, "notify::online",
				G_CALLBACK (online_state_changed), NULL);
		}
	}

	if (enable) {
		GSettings *settings;
		gchar **uris;
		GThread *thread = NULL;
		GError *error = NULL;

		settings = e_util_ref_settings (PC_SETTINGS_ID);
		uris = g_settings_get_strv (settings, PC_SETTINGS_URIS);
		g_object_unref (settings);

		thread = g_thread_try_new (
			NULL, (GThreadFunc)
			publish_uris_set_timeout, uris, &error);
		if (error != NULL) {
			g_warning (
				"Could create thread to set timeout "
				"for publishing uris : %s", error->message);
			g_error_free (error);
		} else {
			g_thread_unref (thread);
		}
	}

	return 0;
}

struct eq_data {
	gchar *description;
	GError *error;
};

static gboolean
error_queue_show_idle (gpointer user_data)
{
	GString *info = NULL;
	GSList *l;
	gboolean has_error = FALSE, has_info = FALSE;

	g_mutex_lock (&error_queue_lock);

	for (l = error_queue; l; l = l->next) {
		struct eq_data *data = l->data;

		if (data) {
			if (data->description) {
				if (!info) {
					info = g_string_new (data->description);
				} else {
					g_string_append (info, "\n\n");
					g_string_append (info, data->description);
				}

				g_free (data->description);
			}

			if (data->error) {
				has_error = TRUE;
				if (!info) {
					info = g_string_new (data->error->message);
				} else if (data->description) {
					g_string_append_c (info, ' ');
					g_string_append (info, data->error->message);
				} else {
					g_string_append (info, "\n\n");
					g_string_append (info, data->error->message);
				}

				g_error_free (data->error);
			} else if (data->description) {
				has_info = TRUE;
			}

			g_slice_free (struct eq_data, data);
		}
	}

	g_slist_free (error_queue);

	error_queue = NULL;
	error_queue_show_idle_id = 0;

	g_mutex_unlock (&error_queue_lock);

	if (info) {
		update_publish_notification (has_error && has_info ? GTK_MESSAGE_WARNING : has_error ? GTK_MESSAGE_ERROR : GTK_MESSAGE_INFO, info->str);

		g_string_free (info, TRUE);
	}

	return FALSE;
}

void
error_queue_add (gchar *description,
                 GError *error)
{
	struct eq_data *data;

	if (!error && !description)
		return;

	data = g_slice_new0 (struct eq_data);
	data->description = description;
	data->error = error;

	g_mutex_lock (&error_queue_lock);
	error_queue = g_slist_append (error_queue, data);
	if (error_queue_show_idle_id == 0)
		error_queue_show_idle_id = g_idle_add (error_queue_show_idle, NULL);
	g_mutex_unlock (&error_queue_lock);
}

static void
action_calendar_publish_cb (EUIAction *action,
			    GVariant *parameter,
                            gpointer user_data)
{
	/* EShellView *shell_view = user_data; */
	GThread *thread = NULL;
	GError *error = NULL;

	thread = g_thread_try_new (NULL, (GThreadFunc) publish_urls, NULL, &error);
	if (!thread) {
		/* To Translators: This is shown to a user when creation of a new thread,
		 * where the publishing should be done, fails. Basically, this shouldn't
		 * ever happen, and if so, then something is really wrong. */
		error_queue_add (g_strdup (_("Could not create publish thread.")), error);
	} else {
		g_thread_unref (thread);
	}
}

gboolean e_plugin_ui_init (EUIManager *ui_manager, EShellView *shell_view);

gboolean
e_plugin_ui_init (EUIManager *ui_manager,
                  EShellView *shell_view)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<placeholder id='custom-menus'>"
		      "<submenu action='calendar-actions-menu'>"
			"<item action='calendar-publish'/>"
		      "</submenu>"
		    "</placeholder>"
		   "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {

		{ "calendar-publish",
		  NULL,
		  N_("_Publish Calendar Information"),
		  NULL,
		  NULL,
		  action_calendar_publish_cb, NULL, NULL, NULL }
	};

	e_ui_manager_add_actions_with_eui_data (ui_manager, "calendar", NULL,
		entries, G_N_ELEMENTS (entries), shell_view, eui);

	return TRUE;
}
