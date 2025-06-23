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
 *		Michael Zucchi <NotZed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdio.h>
#include <string.h>
#include <glib/gi18n.h>

#include <shell/e-shell.h>
#include <shell/e-shell-content.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>
#include <e-util/e-util.h>

#include "e-mail-account-store.h"
#include "e-mail-reader-utils.h"
#include "e-mail-ui-session.h"
#include "e-mail-view.h"
#include "em-event.h"
#include "em-filter-rule.h"
#include "em-utils.h"

#include "mail-send-recv.h"

#define d(x)

/* ms between status updates to the gui */
#define STATUS_TIMEOUT (250)

/* pseudo-uri to key the send task on */
#define SEND_URI_KEY "send-task:"

#define SEND_RECV_ICON_SIZE GTK_ICON_SIZE_LARGE_TOOLBAR

/* send/receive email */

/* ********************************************************************** */
/*  This stuff below is independent of the stuff above */

/* this stuff is used to keep track of which folders filters have accessed, and
 * what not. the thaw/refreeze thing doesn't really seem to work though */
struct _folder_info {
	gchar *uri;
	CamelFolder *folder;
	time_t update;

	/* How many times updated, to slow it
	 * down as we go, if we have lots. */
	gint count;
};

struct _send_data {
	GList *infos;

	GtkDialog *gd;
	gint cancelled;

	/* Since we're never asked to update
	 * this one, do it ourselves. */
	CamelFolder *inbox;
	time_t inbox_update;

	GMutex lock;
	GHashTable *folders;

	GHashTable *active;	/* send_info's by uri */
};

typedef enum {
	SEND_RECEIVE,		/* receiver */
	SEND_SEND,		/* sender */
	SEND_UPDATE,		/* imap-like 'just update folder info' */
	SEND_INVALID
} send_info_t;

typedef enum {
	SEND_ACTIVE,
	SEND_CANCELLED,
	SEND_COMPLETE
} send_state_t;

struct _send_info {
	send_info_t type;		/* 0 = fetch, 1 = send */
	GCancellable *cancellable;
	CamelSession *session;
	CamelService *service;
	send_state_t state;
	GtkWidget *progress_bar;
	GtkWidget *cancel_button;

	gint again;		/* need to run send again */

	gint timeout_id;
	gchar *what;
	gint pc;

	GtkWidget *send_account_label;
	gchar *send_url;

	/*time_t update;*/
	struct _send_data *data;
};

static CamelFolder *
		receive_get_folder		(CamelFilterDriver *d,
						 const gchar *uri,
						 gpointer data,
						 GError **error);
static gboolean	send_done (gpointer data, const GError *error, const GPtrArray *failed_uids);

static struct _send_data *send_data = NULL;
static GtkWidget *send_recv_dialog = NULL;
static GSList *glob_ongoing_downsyncs = NULL; /* CamelService *, those where downsync for offline is ongoing */

static void
free_folder_info (struct _folder_info *info)
{
	/*camel_folder_thaw (info->folder);	*/
	mail_sync_folder (info->folder, FALSE, NULL, NULL);
	g_object_unref (info->folder);
	g_free (info->uri);
	g_free (info);
}

static void
free_send_info (struct _send_info *info)
{
	if (info->cancellable != NULL)
		g_object_unref (info->cancellable);
	if (info->session != NULL)
		g_object_unref (info->session);
	if (info->service != NULL)
		g_object_unref (info->service);
	if (info->timeout_id != 0)
		g_source_remove (info->timeout_id);
	g_free (info->what);
	g_free (info->send_url);
	g_free (info);
}

static struct _send_data *
setup_send_data (EMailSession *session)
{
	struct _send_data *data;

	if (send_data == NULL) {
		send_data = data = g_malloc0 (sizeof (*data));
		g_mutex_init (&data->lock);
		data->folders = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) NULL,
			(GDestroyNotify) free_folder_info);
		data->inbox =
			e_mail_session_get_local_folder (
			session, E_MAIL_LOCAL_FOLDER_LOCAL_INBOX);
		g_object_ref (data->inbox);
		data->active = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) free_send_info);
	}

	return send_data;
}

static void
receive_cancel (GtkButton *button,
                struct _send_info *info)
{
	if (info->state == SEND_ACTIVE) {
		g_cancellable_cancel (info->cancellable);
		if (info->progress_bar != NULL)
			gtk_progress_bar_set_text (
				GTK_PROGRESS_BAR (info->progress_bar),
				_("Canceling…"));
		info->state = SEND_CANCELLED;
	}
	if (info->cancel_button)
		gtk_widget_set_sensitive (info->cancel_button, FALSE);
}

static void
free_send_data (void)
{
	struct _send_data *data = send_data;

	if (!data)
		return;

	g_return_if_fail (g_hash_table_size (data->active) == 0);

	if (data->inbox) {
		mail_sync_folder (data->inbox, FALSE, NULL, NULL);
		/*camel_folder_thaw (data->inbox);		*/
		g_object_unref (data->inbox);
	}

	g_list_free (data->infos);
	g_hash_table_destroy (data->active);
	g_hash_table_destroy (data->folders);
	g_mutex_clear (&data->lock);
	g_free (data);
	send_data = NULL;
}

static void
cancel_send_info (gpointer key,
                  struct _send_info *info,
                  gpointer data)
{
	receive_cancel (GTK_BUTTON (info->cancel_button), info);
}

static void
hide_send_info (gpointer key,
                struct _send_info *info,
                gpointer data)
{
	info->cancel_button = NULL;
	info->progress_bar = NULL;

	if (info->timeout_id != 0) {
		g_source_remove (info->timeout_id);
		info->timeout_id = 0;
	}
}

static void
dialog_destroy_cb (struct _send_data *data,
                   GObject *deadbeef)
{
	g_hash_table_foreach (data->active, (GHFunc) hide_send_info, NULL);
	data->gd = NULL;
	send_recv_dialog = NULL;
}

static void
dialog_response (GtkDialog *gd,
                 gint button,
                 struct _send_data *data)
{
	switch (button) {
	case GTK_RESPONSE_CANCEL:
		d (printf ("cancelled whole thing\n"));
		if (!data->cancelled) {
			data->cancelled = TRUE;
			g_hash_table_foreach (data->active, (GHFunc) cancel_send_info, NULL);
		}
		gtk_dialog_set_response_sensitive (gd, GTK_RESPONSE_CANCEL, FALSE);
		break;
	default:
		d (printf ("hiding dialog\n"));
		g_hash_table_foreach (data->active, (GHFunc) hide_send_info, NULL);
		data->gd = NULL;
		/*gtk_widget_destroy((GtkWidget *)gd);*/
		break;
	}
}

static GMutex status_lock;
static gchar *format_service_name (CamelService *service);

static gint
operation_status_timeout (gpointer data)
{
	struct _send_info *info = data;

	if (info->progress_bar) {
		GtkProgressBar *progress_bar;

		g_mutex_lock (&status_lock);

		progress_bar = GTK_PROGRESS_BAR (info->progress_bar);

		gtk_progress_bar_set_fraction (progress_bar, info->pc / 100.0);
		if (info->what != NULL)
			gtk_progress_bar_set_text (progress_bar, info->what);
		if (info->service != NULL && info->send_account_label) {
			gchar *tmp = format_service_name (info->service);

			gtk_label_set_markup (
				GTK_LABEL (info->send_account_label), tmp);

			g_free (tmp);
		}

		g_mutex_unlock (&status_lock);

		return TRUE;
	}

	return FALSE;
}

static void
set_send_status (struct _send_info *info,
                 const gchar *desc,
                 gint pc)
{
	g_mutex_lock (&status_lock);

	g_free (info->what);
	info->what = g_strdup (desc);
	info->pc = pc;

	g_mutex_unlock (&status_lock);
}

static void
set_transport_service (struct _send_info *info,
                       const gchar *transport_uid)
{
	CamelService *service;

	g_mutex_lock (&status_lock);

	service = camel_session_ref_service (info->session, transport_uid);

	if (CAMEL_IS_TRANSPORT (service)) {
		if (info->service != NULL)
			g_object_unref (info->service);
		info->service = g_object_ref (service);
	}

	if (service != NULL)
		g_object_unref (service);

	g_mutex_unlock (&status_lock);
}

/* for camel operation status */
static void
operation_status (CamelOperation *op,
                  const gchar *what,
                  gint pc,
                  struct _send_info *info)
{
	set_send_status (info, what, pc);
}

static gchar *
format_service_name (CamelService *service)
{
	CamelProvider *provider;
	CamelSettings *settings;
	gchar *service_name = NULL;
	const gchar *display_name;
	gchar *pretty_url = NULL;
	gchar *host = NULL;
	gchar *path = NULL;
	gchar *user = NULL;
	gchar *cp;
	gboolean have_host = FALSE;
	gboolean have_path = FALSE;
	gboolean have_user = FALSE;

	provider = camel_service_get_provider (service);
	display_name = camel_service_get_display_name (service);

	settings = camel_service_ref_settings (service);

	if (CAMEL_IS_NETWORK_SETTINGS (settings)) {
		host = camel_network_settings_dup_host (
			CAMEL_NETWORK_SETTINGS (settings));
		have_host = (host != NULL) && (*host != '\0');

		user = camel_network_settings_dup_user (
			CAMEL_NETWORK_SETTINGS (settings));
		have_user = (user != NULL) && (*user != '\0');
	}

	if (CAMEL_IS_LOCAL_SETTINGS (settings)) {
		path = camel_local_settings_dup_path (
			CAMEL_LOCAL_SETTINGS (settings));
		have_path = (path != NULL) && (*path != '\0');
	}

	g_object_unref (settings);

	/* Shorten user names with '@', since multiple '@' in a
	 * 'user@host' label look weird.  This is just supposed
	 * to be a hint anyway so it doesn't matter if it's not
	 * strictly correct. */
	if (have_user && (cp = strchr (user, '@')) != NULL)
		*cp = '\0';

	g_return_val_if_fail (provider != NULL, NULL);

	/* This should never happen, but if the service has no
	 * display name, fall back to the generic service name. */
	if (display_name == NULL || *display_name == '\0') {
		service_name = camel_service_get_name (service, TRUE);
		display_name = service_name;
	}

	if (have_host && have_user) {
		pretty_url = g_markup_printf_escaped (
			"<b>%s</b> <small>(%s@%s)</small>",
			display_name, user, host);
	} else if (have_host) {
		pretty_url = g_markup_printf_escaped (
			"<b>%s</b> <small>(%s)</small>",
			display_name, host);
	} else if (have_path) {
		pretty_url = g_markup_printf_escaped (
			"<b>%s</b> <small>(%s)</small>",
			display_name, path);
	} else {
		pretty_url = g_markup_printf_escaped (
			"<b>%s</b>", display_name);
	}

	g_free (service_name);
	g_free (host);
	g_free (path);
	g_free (user);

	return pretty_url;
}

static EShellView *
mail_send_receive_get_mail_shell_view (void)
{
	EShellView *shell_view = NULL;

	if (send_recv_dialog) {
		GtkWidget *parent;

		parent = gtk_widget_get_parent (send_recv_dialog);
		if (parent && E_IS_SHELL_WINDOW (parent)) {
			EShellWindow *shell_window = E_SHELL_WINDOW (parent);

			shell_view = e_shell_window_get_shell_view (shell_window, "mail");
		}
	}

	if (!shell_view) {
		EShell *shell;
		GtkWindow *active_window;

		shell = e_shell_get_default ();
		active_window = e_shell_get_active_window (shell);

		if (E_IS_SHELL_WINDOW (active_window)) {
			EShellWindow *shell_window = E_SHELL_WINDOW (active_window);

			shell_view = e_shell_window_get_shell_view (shell_window, "mail");
		} else {
			GList *windows, *link;
			EShellView *adept_mail_view = NULL;
			EShellWindow *first_shell_window = NULL;

			windows = gtk_application_get_windows (GTK_APPLICATION (shell));
			for (link = windows; link; link = g_list_next (link)) {
				GtkWindow *window = link->data;

				if (E_IS_SHELL_WINDOW (window)) {
					EShellWindow *shell_window = E_SHELL_WINDOW (window);

					if (!first_shell_window)
						first_shell_window = shell_window;

					if (g_strcmp0 (e_shell_window_get_active_view (shell_window), "mail") == 0) {
						shell_view = e_shell_window_get_shell_view (shell_window, "mail");
						break;
					} else if (!adept_mail_view) {
						adept_mail_view = e_shell_window_peek_shell_view (shell_window, "mail");
					}
				}
			}

			if (!shell_view)
				shell_view = adept_mail_view;

			if (!shell_view && first_shell_window)
				shell_view = e_shell_window_get_shell_view (first_shell_window, "mail");
		}
	}

	return shell_view;
}

static void
mail_send_recv_send_fail_alert_response_cb (EAlert *alert,
					    gint response_id,
					    gpointer user_data)
{
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree = NULL;
	EMailSession *session;
	EMailReader *reader;
	EMailView *mail_view = NULL;
	CamelFolder *outbox;
	GPtrArray *uids;

	if (response_id != GTK_RESPONSE_APPLY && response_id != GTK_RESPONSE_REJECT)
		return;

	shell_view = mail_send_receive_get_mail_shell_view ();
	if (!shell_view)
		return;

	g_object_get (e_shell_view_get_shell_content (shell_view), "mail-view", &mail_view, NULL);
	g_return_if_fail (mail_view != NULL);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	g_object_get (G_OBJECT (shell_sidebar), "folder-tree", &folder_tree, NULL);
	g_return_if_fail (folder_tree != NULL);

	reader = E_MAIL_READER (mail_view);
	session = em_folder_tree_get_session (folder_tree);
	outbox = e_mail_session_get_local_folder (session, E_MAIL_LOCAL_FOLDER_OUTBOX);

	uids = g_object_get_data (G_OBJECT (alert), "message-uids");

	if (uids && response_id == GTK_RESPONSE_APPLY) {
		e_mail_reader_edit_messages (reader, outbox, uids, TRUE, TRUE);
	} else if (folder_tree) {
		gchar *folder_uri;

		folder_uri = e_mail_folder_uri_from_folder (outbox);
		g_warn_if_fail (folder_uri != NULL);

		if (folder_uri) {
			CamelFolder *selected_folder;

			em_folder_tree_set_selected (folder_tree, folder_uri, FALSE);

			selected_folder = e_mail_reader_ref_folder (reader);

			/* This makes sure the Outbox folder content is shown even
			   when the On This Computer account is disabled */
			if (selected_folder != outbox) {
				GtkTreeSelection *selection;

				selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));
				gtk_tree_selection_unselect_all (selection);

				em_folder_tree_set_selected (folder_tree, folder_uri, FALSE);
				e_mail_reader_set_folder (reader, outbox);
			}

			g_clear_object (&selected_folder);
		}

		g_free (folder_uri);
	}

	g_clear_object (&folder_tree);
	g_clear_object (&mail_view);
}

struct ReportErrorToUIData
{
	gchar *display_name;
	gchar *error_ident;
	GError *error;
	GPtrArray *send_failed_uids;
};

static gboolean
report_error_to_ui_cb (gpointer user_data)
{
	struct ReportErrorToUIData *data = user_data;
	EShellView *shell_view;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (data->display_name != NULL, FALSE);
	g_return_val_if_fail (data->error_ident != NULL, FALSE);
	g_return_val_if_fail (data->error != NULL, FALSE);

	shell_view = mail_send_receive_get_mail_shell_view ();

	if (shell_view) {
		EShellContent *shell_content;
		EAlertSink *alert_sink;
		EAlert *alert;

		shell_content = e_shell_view_get_shell_content (shell_view);
		alert_sink = E_ALERT_SINK (shell_content);

		alert = e_alert_new (data->error_ident, data->display_name,
			data->error->message ? data->error->message : _("Unknown error"), NULL);

		if (data->send_failed_uids) {
			EUIAction *action;

			if (data->send_failed_uids->len == 1) {
				g_object_set_data_full (G_OBJECT (alert), "message-uids",
					g_ptr_array_ref (data->send_failed_uids),
					(GDestroyNotify) g_ptr_array_unref);
			}

			if (data->send_failed_uids->len == 1) {
				action = e_ui_action_new ("mail-send-recv-map", "send-failed-edit-action", NULL);
				e_ui_action_set_label (action, _("Edit Message"));
				e_alert_add_action (alert, action, GTK_RESPONSE_APPLY, FALSE);
				g_object_unref (action);
			}

			action = e_ui_action_new ("mail-send-recv-map", "send-failed-outbox-action", NULL);
			e_ui_action_set_label (action, _("Open Outbox Folder"));
			e_alert_add_action (alert, action, GTK_RESPONSE_REJECT, FALSE);
			g_object_unref (action);

			g_signal_connect (alert, "response",
				G_CALLBACK (mail_send_recv_send_fail_alert_response_cb), NULL);
		}

		e_alert_sink_submit_alert (alert_sink, alert);

		g_object_unref (alert);
	} else {
		/* This may not happen, but just in case... */
		g_warning ("%s: %s '%s': %s\n", G_STRFUNC, data->error_ident, data->display_name, data->error->message);
	}

	g_free (data->display_name);
	g_free (data->error_ident);
	g_error_free (data->error);
	if (data->send_failed_uids)
		g_ptr_array_unref (data->send_failed_uids);
	g_slice_free (struct ReportErrorToUIData, data);

	return FALSE;
}

static void
report_error_to_ui (CamelService *service,
		    const gchar *folder_name,
		    const GError *error,
		    const GPtrArray *send_failed_uids)
{
	gchar *tmp = NULL;
	const gchar *display_name, *ident;
	struct ReportErrorToUIData *data;

	g_return_if_fail (CAMEL_IS_SERVICE (service));
	g_return_if_fail (error != NULL);

	/* Ignore 'offline' errors */
	if (g_error_matches (error, CAMEL_SERVICE_ERROR, CAMEL_SERVICE_ERROR_UNAVAILABLE))
		return;

	if (folder_name) {
		tmp = g_strdup_printf ("%s : %s",
			camel_service_get_display_name (service),
			folder_name);
		display_name = tmp;
		ident = "mail:no-refresh-folder";
	} else if (send_failed_uids) {
		display_name = _("Sending message");
		ident = "mail:async-error";
	} else {
		display_name = camel_service_get_display_name (service);
		ident = "mail:failed-connect";
	}

	data = g_slice_new0 (struct ReportErrorToUIData);
	data->display_name = g_strdup (display_name);
	data->error_ident = g_strdup (ident);
	data->error = g_error_copy (error);

	if (send_failed_uids) {
		gint ii;

		data->send_failed_uids = g_ptr_array_new_full (send_failed_uids->len + 1, (GDestroyNotify) camel_pstring_free);

		for (ii = 0; ii < send_failed_uids->len; ii++) {
			g_ptr_array_add (data->send_failed_uids, (gpointer) camel_pstring_strdup (g_ptr_array_index (send_failed_uids, ii)));
		}
	} else {
		data->send_failed_uids = NULL;
	}

	g_idle_add_full (G_PRIORITY_DEFAULT, report_error_to_ui_cb, data, NULL);

	g_free (tmp);
}

static send_info_t
get_receive_type (CamelService *service)
{
	CamelProvider *provider;
	const gchar *uid;

	/* Disregard CamelNullStores. */
	if (CAMEL_IS_NULL_STORE (service))
		return SEND_INVALID;

	/* mbox pointing to a file is a 'Local delivery'
	 * source which requires special processing. */
	if (em_utils_is_local_delivery_mbox_file (service))
		return SEND_RECEIVE;

	provider = camel_service_get_provider (service);

	if (provider == NULL)
		return SEND_INVALID;

	/* skip some well-known services */
	uid = camel_service_get_uid (service);
	if (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0)
		return SEND_INVALID;
	if (g_strcmp0 (uid, E_MAIL_SESSION_VFOLDER_UID) == 0)
		return SEND_INVALID;

	if (provider->object_types[CAMEL_PROVIDER_STORE]) {
		if (provider->flags & CAMEL_PROVIDER_IS_STORAGE)
			return SEND_UPDATE;
		else
			return SEND_RECEIVE;
	}

	if (provider->object_types[CAMEL_PROVIDER_TRANSPORT])
		return SEND_SEND;

	return SEND_INVALID;
}

static void
build_dialog (GtkWindow *parent,
              EMailSession *session,
              CamelFolder *outbox,
              CamelService *transport,
              gboolean allow_send)
{
	GtkDialog *gd;
	GtkWidget *wgrid;
	GtkGrid *grid;
	gint row;
	GList *list = NULL;
	struct _send_data *data;
	GtkWidget *container;
	GtkWidget *send_icon;
	GtkWidget *recv_icon;
	GtkWidget *scrolled_window;
	GtkWidget *label;
	GtkWidget *progress_bar;
	GtkWidget *cancel_button;
	EMailAccountStore *account_store;
	struct _send_info *info;
	gchar *pretty_url;
	EMEventTargetSendReceive *target;
	GQueue queue = G_QUEUE_INIT;

	account_store = e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session));

	send_recv_dialog = gtk_dialog_new ();

	gd = GTK_DIALOG (send_recv_dialog);
	gtk_window_set_modal (GTK_WINDOW (send_recv_dialog), FALSE);
	gtk_window_set_icon_name (GTK_WINDOW (gd), "mail-send-receive");
	gtk_window_set_default_size (GTK_WINDOW (gd), 600, 200);
	gtk_window_set_title (GTK_WINDOW (gd), _("Send & Receive Mail"));
	gtk_window_set_transient_for (GTK_WINDOW (gd), parent);

	e_restore_window (
		GTK_WINDOW (gd),
		"/org/gnome/evolution/mail/send-recv-window/",
		E_RESTORE_WINDOW_SIZE);

	container = gtk_dialog_get_action_area (gd);
	gtk_container_set_border_width (GTK_CONTAINER (container), 6);

	container = gtk_dialog_get_content_area (gd);
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);

	cancel_button = e_dialog_button_new_with_icon ("process-stop", _("Cancel _All"));
	gtk_widget_show (cancel_button);
	gtk_dialog_add_action_widget (gd, cancel_button, GTK_RESPONSE_CANCEL);

	wgrid = gtk_grid_new ();
	grid = GTK_GRID (wgrid);
	gtk_container_set_border_width (GTK_CONTAINER (grid), 6);
	gtk_grid_set_column_spacing (grid, 6);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_set_border_width (
		GTK_CONTAINER (scrolled_window), 6);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (scrolled_window, 50, 50);

	container = gtk_dialog_get_content_area (gd);
	gtk_container_add (GTK_CONTAINER (scrolled_window), wgrid);
	gtk_box_pack_start (
		GTK_BOX (container), scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (scrolled_window);

	/* must bet setup after send_recv_dialog as it may re-trigger send-recv button */
	data = setup_send_data (session);

	row = 0;
	e_mail_account_store_queue_enabled_services (account_store, &queue);
	while (!g_queue_is_empty (&queue)) {
		CamelService *service;
		const gchar *uid;

		service = g_queue_pop_head (&queue);
		uid = camel_service_get_uid (service);

		/* see if we have an outstanding download active */
		info = g_hash_table_lookup (data->active, uid);
		if (info == NULL) {
			send_info_t type = SEND_INVALID;

			type = get_receive_type (service);

			if (type == SEND_INVALID || type == SEND_SEND)
				continue;

			info = g_malloc0 (sizeof (*info));
			info->type = type;
			info->session = CAMEL_SESSION (g_object_ref (session));
			info->service = g_object_ref (service);
			info->cancellable = camel_operation_new ();
			info->state = allow_send ? SEND_ACTIVE : SEND_COMPLETE;
			info->timeout_id = e_named_timeout_add (
				STATUS_TIMEOUT, operation_status_timeout, info);

			g_signal_connect (
				info->cancellable, "status",
				G_CALLBACK (operation_status), info);

			g_hash_table_insert (
				data->active, g_strdup (uid), info);
			list = g_list_prepend (list, info);

		} else if (info->progress_bar != NULL) {
			/* incase we get the same source pop up again */
			continue;

		} else if (info->timeout_id == 0) {
			info->timeout_id = e_named_timeout_add (
				STATUS_TIMEOUT, operation_status_timeout, info);
		}

		recv_icon = gtk_image_new_from_icon_name (
			"mail-inbox", SEND_RECV_ICON_SIZE);
		gtk_widget_set_valign (recv_icon, GTK_ALIGN_START);

		pretty_url = format_service_name (service);
		label = gtk_label_new (NULL);
		gtk_label_set_ellipsize (
			GTK_LABEL (label), PANGO_ELLIPSIZE_END);
		gtk_label_set_markup (GTK_LABEL (label), pretty_url);
		g_free (pretty_url);

		progress_bar = gtk_progress_bar_new ();
		gtk_progress_bar_set_show_text (
			GTK_PROGRESS_BAR (progress_bar), TRUE);
		gtk_progress_bar_set_text (
			GTK_PROGRESS_BAR (progress_bar),
			(info->type == SEND_UPDATE) ?
			_("Updating…") : _("Waiting…"));
		gtk_widget_set_margin_bottom (progress_bar, 12);

		cancel_button = e_dialog_button_new_with_icon ("process-stop", _("_Cancel"));
		gtk_widget_set_valign (cancel_button, GTK_ALIGN_END);
		gtk_widget_set_margin_bottom (cancel_button, 12);

		/* g_object_set(data->label, "bold", TRUE, NULL); */
		gtk_label_set_xalign (GTK_LABEL (label), 0);

		gtk_widget_set_hexpand (label, TRUE);
		gtk_widget_set_halign (label, GTK_ALIGN_FILL);

		gtk_grid_attach (grid, recv_icon, 0, row, 1, 2);
		gtk_grid_attach (grid, label, 1, row, 1, 1);
		gtk_grid_attach (grid, progress_bar, 1, row + 1, 1, 1);
		gtk_grid_attach (grid, cancel_button, 2, row, 1, 2);

		info->progress_bar = progress_bar;
		info->cancel_button = cancel_button;
		info->data = data;

		g_signal_connect (
			cancel_button, "clicked",
			G_CALLBACK (receive_cancel), info);

		row = row + 2;
	}

	/* we also need gd during emition to be able to catch Cancel All */
	data->gd = gd;
	target = em_event_target_new_send_receive (
		em_event_peek (), wgrid, data, row, EM_EVENT_SEND_RECEIVE);
	e_event_emit (
		(EEvent *) em_event_peek (), "mail.sendreceive",
		(EEventTarget *) target);

	/* Skip displaying the SMTP row if we've got no outbox,
	 * outgoing account or unsent mails. */
	if (allow_send && outbox && CAMEL_IS_TRANSPORT (transport)
	 && (camel_folder_get_message_count (outbox) -
		camel_folder_summary_get_deleted_count (camel_folder_get_folder_summary (outbox))) != 0) {

		info = g_hash_table_lookup (data->active, SEND_URI_KEY);
		if (info == NULL) {
			info = g_malloc0 (sizeof (*info));
			info->type = SEND_SEND;
			info->session = CAMEL_SESSION (g_object_ref (session));
			info->service = g_object_ref (transport);
			info->cancellable = camel_operation_new ();
			info->state = SEND_ACTIVE;
			info->timeout_id = e_named_timeout_add (
				STATUS_TIMEOUT, operation_status_timeout, info);

			g_signal_connect (
				info->cancellable, "status",
				G_CALLBACK (operation_status), info);

			g_hash_table_insert (
				data->active, g_strdup (SEND_URI_KEY), info);
			list = g_list_prepend (list, info);
		} else if (info->timeout_id == 0) {
			info->timeout_id = e_named_timeout_add (
				STATUS_TIMEOUT, operation_status_timeout, info);
		}

		send_icon = gtk_image_new_from_icon_name (
			"mail-outbox", SEND_RECV_ICON_SIZE);
		gtk_widget_set_valign (send_icon, GTK_ALIGN_START);

		pretty_url = format_service_name (transport);
		label = gtk_label_new (NULL);
		gtk_label_set_ellipsize (
			GTK_LABEL (label), PANGO_ELLIPSIZE_END);
		gtk_label_set_markup (GTK_LABEL (label), pretty_url);
		g_free (pretty_url);

		progress_bar = gtk_progress_bar_new ();
		gtk_progress_bar_set_show_text (
			GTK_PROGRESS_BAR (progress_bar), TRUE);
		gtk_progress_bar_set_text (
			GTK_PROGRESS_BAR (progress_bar), _("Waiting…"));
		gtk_widget_set_margin_bottom (progress_bar, 12);

		cancel_button = e_dialog_button_new_with_icon ("process-stop", _("_Cancel"));
		gtk_widget_set_valign (cancel_button, GTK_ALIGN_END);

		gtk_label_set_xalign (GTK_LABEL (label), 0);

		gtk_widget_set_hexpand (label, TRUE);
		gtk_widget_set_halign (label, GTK_ALIGN_FILL);

		gtk_grid_attach (grid, send_icon, 0, row, 1, 2);
		gtk_grid_attach (grid, label, 1, row, 1, 1);
		gtk_grid_attach (grid, progress_bar, 1, row + 1, 1, 1);
		gtk_grid_attach (grid, cancel_button, 2, row, 1, 2);

		info->progress_bar = progress_bar;
		info->cancel_button = cancel_button;
		info->data = data;
		info->send_account_label = label;

		g_signal_connect (
			cancel_button, "clicked",
			G_CALLBACK (receive_cancel), info);
	}

	gtk_widget_show_all (wgrid);

	g_signal_connect (
		gd, "response",
		G_CALLBACK (dialog_response), data);

	g_object_weak_ref ((GObject *) gd, (GWeakNotify) dialog_destroy_cb, data);

	data->infos = list;
}

static void
update_folders (gchar *uri,
                struct _folder_info *info,
                gpointer data)
{
	time_t now = *((time_t *) data);

	d (printf ("checking update for folder: %s\n", info->uri));

	/* let it flow through to the folders every 10 seconds */
	/* we back off slowly as we progress */
	if (now > info->update + 10 + info->count *5) {
		d (printf ("upating a folder: %s\n", info->uri));
		/*camel_folder_thaw(info->folder);
		  camel_folder_freeze (info->folder);*/
		info->update = now;
		info->count++;
	}
}

static void
receive_status (CamelFilterDriver *driver,
                enum camel_filter_status_t status,
                gint pc,
                const gchar *desc,
                gpointer data)
{
	struct _send_info *info = data;
	time_t now = time (NULL);

	/* let it flow through to the folder, every now and then too? */
	g_hash_table_foreach (info->data->folders, (GHFunc) update_folders, &now);

	if (info->data->inbox && now > info->data->inbox_update + 20) {
		d (printf ("updating inbox too\n"));
		/* this doesn't seem to work right :( */
		/*camel_folder_thaw(info->data->inbox);
		  camel_folder_freeze (info->data->inbox);*/
		info->data->inbox_update = now;
	}

	/* we just pile them onto the port, assuming it can handle it.
	 * We could also have a receiver port and see if they've been processed
	 * yet, so if this is necessary its not too hard to add */
	/* the mail_gui_port receiver will free everything for us */
	switch (status) {
	case CAMEL_FILTER_STATUS_START:
	case CAMEL_FILTER_STATUS_END:
		set_send_status (info, desc, pc);
		break;
	case CAMEL_FILTER_STATUS_ACTION:
		set_transport_service (info, desc);
		break;
	default:
		break;
	}
}

static void
free_downsync_for_store_data (gpointer ptr)
{
	CamelService *service = ptr;

	if (!service)
		return;

	glob_ongoing_downsyncs = g_slist_remove (glob_ongoing_downsyncs, service);

	g_clear_object (&service);
}

static void
downsync_for_store_thread (EAlertSinkThreadJobData *job_data,
			   gpointer user_data,
			   GCancellable *cancellable,
			   GError **error)
{
	CamelOfflineStore *offline_store = user_data;

	g_return_if_fail (CAMEL_IS_OFFLINE_STORE (offline_store));

	camel_offline_store_prepare_for_offline_sync (offline_store, cancellable, error);
}

static void
run_downsync_for_store (CamelService *service)
{
	EShellView *shell_view;
	EActivity *activity;
	gchar *description;

	shell_view = mail_send_receive_get_mail_shell_view ();
	if (!shell_view)
		return;

	glob_ongoing_downsyncs = g_slist_prepend (glob_ongoing_downsyncs, service);

	description = g_strdup_printf (_("Preparing account “%s” for offline"), camel_service_get_display_name (service));

	activity = e_shell_view_submit_thread_job (shell_view, description,
		"mail:prepare-for-offline",
		camel_service_get_display_name (service),
		downsync_for_store_thread, g_object_ref (service),
		free_downsync_for_store_data);

	if (!activity)
		glob_ongoing_downsyncs = g_slist_remove (glob_ongoing_downsyncs, service);

	g_clear_object (&activity);
	g_free (description);
}

/* when receive/send is complete */
static void
receive_done (gpointer data)
{
	struct _send_info *info = data;
	const gchar *uid;

	uid = camel_service_get_uid (info->service);
	g_return_if_fail (uid != NULL);

	/* if we've been called to run again - run again */
	if (info->type == SEND_SEND && info->state == SEND_ACTIVE && info->again) {
		CamelFolder *local_outbox;

		local_outbox =
			e_mail_session_get_local_folder (
			E_MAIL_SESSION (info->session),
			E_MAIL_LOCAL_FOLDER_OUTBOX);

		g_return_if_fail (CAMEL_IS_TRANSPORT (info->service));

		info->again = 0;
		mail_send_queue (
			E_MAIL_SESSION (info->session),
			local_outbox,
			CAMEL_TRANSPORT (info->service),
			E_FILTER_SOURCE_OUTGOING,
			TRUE,
			info->cancellable,
			receive_get_folder, info,
			receive_status, info,
			send_done, info);
		return;
	}

	if (info->progress_bar) {
		const gchar *text;

		gtk_progress_bar_set_fraction (
			GTK_PROGRESS_BAR (info->progress_bar), 1.0);

		if (info->state == SEND_CANCELLED)
			text = _("Cancelled");
		else {
			text = _("Complete");
			info->state = SEND_COMPLETE;
		}

		gtk_progress_bar_set_text (
			GTK_PROGRESS_BAR (info->progress_bar), text);
	}

	if (info->cancel_button)
		gtk_widget_set_sensitive (info->cancel_button, FALSE);

	/* remove/free this active download */
	d (printf ("%s: freeing info %p\n", G_STRFUNC, info));
	if (info->type == SEND_SEND) {
		gpointer key = NULL, value = NULL;
		if (!g_hash_table_lookup_extended (info->data->active, SEND_URI_KEY, &key, &value))
			key = NULL;

		g_hash_table_steal (info->data->active, SEND_URI_KEY);
		g_free (key);
	} else {
		gpointer key = NULL, value = NULL;
		if (!g_hash_table_lookup_extended (info->data->active, uid, &key, &value))
			key = NULL;

		g_hash_table_steal (info->data->active, uid);
		g_free (key);
	}
	info->data->infos = g_list_remove (info->data->infos, info);

	if (g_hash_table_size (info->data->active) == 0) {
		if (info->data->gd)
			gtk_widget_destroy ((GtkWidget *) info->data->gd);
		free_send_data ();
	}

	if (info->state != SEND_CANCELLED &&
	    CAMEL_IS_OFFLINE_STORE (info->service) &&
	    camel_offline_store_get_online (CAMEL_OFFLINE_STORE (info->service)) &&
	    !g_slist_find (glob_ongoing_downsyncs, info->service)) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		if (g_settings_get_boolean (settings, "send-receive-downloads-for-offline") &&
		    camel_offline_store_requires_downsync (CAMEL_OFFLINE_STORE (info->service))) {
			run_downsync_for_store (info->service);
		}
		g_object_unref (settings);
	}

	free_send_info (info);
}

static gboolean
send_done (gpointer data,
	   const GError *error,
	   const GPtrArray *failed_uids)
{
	gboolean res = FALSE;

	if (error && failed_uids) {
		struct _send_info *info = data;

		res = TRUE;

		report_error_to_ui (info->service, NULL, error, failed_uids);
	}

	receive_done (data);

	return res;
}
/* although we don't do anything smart here yet, there is no need for this interface to
 * be available to anyone else.
 * This can also be used to hook into which folders are being updated, and occasionally
 * let them refresh */
static CamelFolder *
receive_get_folder (CamelFilterDriver *d,
                    const gchar *uri,
                    gpointer data,
                    GError **error)
{
	struct _send_info *info = data;
	CamelFolder *folder;
	struct _folder_info *oldinfo;
	gpointer oldkey, oldinfoptr;

	g_mutex_lock (&info->data->lock);
	oldinfo = g_hash_table_lookup (info->data->folders, uri);
	g_mutex_unlock (&info->data->lock);

	if (oldinfo) {
		g_object_ref (oldinfo->folder);
		return oldinfo->folder;
	}

	/* FIXME Not passing a GCancellable here. */
	folder = e_mail_session_uri_to_folder_sync (
		E_MAIL_SESSION (info->session), uri, 0, NULL, error);
	if (!folder)
		return NULL;

	/* we recheck that the folder hasn't snuck in while we were loading it... */
	/* and we assume the newer one is the same, but unref the old one anyway */
	g_mutex_lock (&info->data->lock);

	if (g_hash_table_lookup_extended (
			info->data->folders, uri, &oldkey, &oldinfoptr)) {
		oldinfo = (struct _folder_info *) oldinfoptr;
		g_object_unref (oldinfo->folder);
		oldinfo->folder = folder;
	} else {
		oldinfo = g_malloc0 (sizeof (*oldinfo));
		oldinfo->folder = folder;
		oldinfo->uri = g_strdup (uri);
		g_hash_table_insert (info->data->folders, oldinfo->uri, oldinfo);
	}

	g_object_ref (folder);

	g_mutex_unlock (&info->data->lock);

	return folder;
}

/* ********************************************************************** */

static gboolean
delete_junk_sync (CamelStore *store,
		  GCancellable *cancellable,
		  GError **error)
{
	CamelFolder *folder;
	GPtrArray *uids;
	guint32 flags;
	guint32 mask;
	guint ii;

	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);

	folder = camel_store_get_junk_folder_sync (store, cancellable, error);
	if (folder == NULL)
		return FALSE;

	uids = camel_folder_dup_uids (folder);
	flags = mask = CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN;

	camel_folder_freeze (folder);

	for (ii = 0; ii < uids->len; ii++) {
		const gchar *uid = uids->pdata[ii];
		camel_folder_set_message_flags (folder, uid, flags, mask);
	}

	camel_folder_thaw (folder);

	g_ptr_array_unref (uids);
	g_object_unref (folder);

	return TRUE;
}

struct TestShouldData
{
	gint64 last_delete_junk;
	gint64 last_expunge;
};

static void
test_should_delete_junk_or_expunge (CamelStore *store,
				    gboolean *should_delete_junk,
				    gboolean *should_expunge)
{
	static GMutex mutex;
	static GHashTable *last_expunge = NULL;

	GSettings *settings;
	const gchar *uid;
	gint64 trash_empty_date = 0, junk_empty_date = 0;
	gint trash_empty_days = 0, junk_empty_days = 0;
	gint64 now;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (should_delete_junk != NULL);
	g_return_if_fail (should_expunge != NULL);

	*should_delete_junk = FALSE;
	*should_expunge = FALSE;

	uid = camel_service_get_uid (CAMEL_SERVICE (store));
	g_return_if_fail (uid != NULL);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	now = time (NULL) / 60 / 60 / 24;

	*should_delete_junk = g_settings_get_boolean (settings, "junk-empty-on-exit");
	*should_expunge = g_settings_get_boolean (settings, "trash-empty-on-exit");

	if (*should_delete_junk || *should_expunge) {
		junk_empty_days = g_settings_get_int (settings, "junk-empty-on-exit-days");
		junk_empty_date = g_settings_get_int (settings, "junk-empty-date");

		trash_empty_days = g_settings_get_int (settings, "trash-empty-on-exit-days");
		trash_empty_date = g_settings_get_int (settings, "trash-empty-date");

		g_mutex_lock (&mutex);
		if (!last_expunge) {
			last_expunge = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
		} else {
			struct TestShouldData *tsd;

			tsd = g_hash_table_lookup (last_expunge, uid);
			if (tsd) {
				junk_empty_date = tsd->last_delete_junk;
				trash_empty_date = tsd->last_expunge;
			}
		}
		g_mutex_unlock (&mutex);
	}

	*should_delete_junk = *should_delete_junk && junk_empty_days > 0 && junk_empty_date + junk_empty_days <= now;
	*should_expunge = *should_expunge && trash_empty_days > 0 && trash_empty_date + trash_empty_days <= now;

	if (*should_delete_junk || *should_expunge) {
		struct TestShouldData *tsd;

		if (*should_delete_junk)
			junk_empty_date = now;
		if (*should_expunge)
			trash_empty_date = now;

		g_mutex_lock (&mutex);
		tsd = g_hash_table_lookup (last_expunge, uid);
		if (!tsd) {
			tsd = g_new0 (struct TestShouldData, 1);
			g_hash_table_insert (last_expunge, g_strdup (uid), tsd);
		}

		tsd->last_delete_junk = junk_empty_date;
		tsd->last_expunge = trash_empty_date;
		g_mutex_unlock (&mutex);
	}

	g_object_unref (settings);
}

static void
get_folders (CamelStore *store,
             GPtrArray *folders,
             CamelFolderInfo *info)
{
	while (info) {
		if (camel_store_can_refresh_folder (store, info, NULL)) {
			if ((info->flags & CAMEL_FOLDER_NOSELECT) == 0) {
				gchar *folder_uri;

				folder_uri = e_mail_folder_uri_build (
					store, info->full_name);
				g_ptr_array_add (folders, folder_uri);
			}
		}

		get_folders (store, folders, info->child);
		info = info->next;
	}
}

static void
main_op_cancelled_cb (GCancellable *main_op,
                      GCancellable *refresh_op)
{
	g_cancellable_cancel (refresh_op);
}

struct _refresh_folders_msg {
	MailMsg base;

	struct _send_info *info;
	GPtrArray *folders;
	CamelStore *store;
	CamelFolderInfo *finfo;
};

static gchar *
refresh_folders_desc (struct _refresh_folders_msg *m)
{
	return g_strdup_printf (
		_("Checking for new mail at “%s”"),
		camel_service_get_display_name (CAMEL_SERVICE (m->store)));
}

static void
refresh_folders_exec (struct _refresh_folders_msg *m,
                      GCancellable *cancellable,
                      GError **error)
{
	CamelFolder *folder;
	gint i;
	gboolean success;
	gboolean delete_junk = FALSE, expunge = FALSE;
	GHashTable *known_errors;
	EMailBackend *mail_backend;
	GError *local_error = NULL;
	gulong handler_id = 0;

	if (cancellable)
		handler_id = g_signal_connect (
			m->info->cancellable, "cancelled",
			G_CALLBACK (main_op_cancelled_cb), cancellable);

	success = camel_service_connect_sync (CAMEL_SERVICE (m->store), cancellable, &local_error);
	if (!success) {
		if (g_error_matches (local_error, CAMEL_SERVICE_ERROR, CAMEL_SERVICE_ERROR_UNAVAILABLE))
			g_clear_error (&local_error);
		else
			g_propagate_error (error, local_error);
		goto exit;
	}

	get_folders (m->store, m->folders, m->finfo);

	camel_operation_push_message (m->info->cancellable, _("Updating…"));

	test_should_delete_junk_or_expunge (m->store, &delete_junk, &expunge);

	if (delete_junk && !delete_junk_sync (m->store, cancellable, error)) {
		camel_operation_pop_message (m->info->cancellable);
		goto exit;
	}

	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (e_shell_get_default (), "mail"));

	known_errors = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (i = 0; i < m->folders->len; i++) {
		folder = e_mail_session_uri_to_folder_sync (
			E_MAIL_SESSION (m->info->session),
			m->folders->pdata[i], 0,
			cancellable, &local_error);
		if (folder && camel_folder_synchronize_sync (folder, expunge, cancellable, &local_error))
			camel_folder_refresh_info_sync (folder, cancellable, &local_error);

		if (folder && !local_error && mail_backend) {
			em_utils_process_autoarchive_sync (mail_backend, folder, m->folders->pdata[i], cancellable, &local_error);
		}

		if (local_error != NULL) {
			const gchar *error_message = local_error->message ? local_error->message : _("Unknown error");

			if (g_hash_table_contains (known_errors, error_message)) {
				/* Received the same error message multiple times; there can be some
				   connection issue probably, thus skip the rest folder updates for now */
				g_clear_object (&folder);
				g_clear_error (&local_error);
				break;
			} else if (!g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
				CamelStore *store;
				const gchar *full_name;

				if (folder) {
					store = camel_folder_get_parent_store (folder);
					full_name = camel_folder_get_full_display_name (folder);
				} else {
					store = m->store;
					full_name = (const gchar *) m->folders->pdata[i];
				}

				report_error_to_ui (CAMEL_SERVICE (store), full_name, local_error, NULL);

				/* To not report one error for multiple folders multiple times */
				g_hash_table_insert (known_errors, g_strdup (error_message), GINT_TO_POINTER (1));
			}

			g_clear_error (&local_error);
		}

		if (folder)
			g_object_unref (folder);

		if (g_cancellable_is_cancelled (m->info->cancellable) ||
		    g_cancellable_is_cancelled (cancellable))
			break;

		if (m->info->state != SEND_CANCELLED)
			camel_operation_progress (
				m->info->cancellable, 100 * i / m->folders->len);
	}

	camel_operation_pop_message (m->info->cancellable);
	g_hash_table_destroy (known_errors);

exit:
	if (handler_id > 0)
		g_signal_handler_disconnect (m->info->cancellable, handler_id);
}

static void
refresh_folders_done (struct _refresh_folders_msg *m)
{
	receive_done (m->info);
}

static void
refresh_folders_free (struct _refresh_folders_msg *m)
{
	gint i;

	for (i = 0; i < m->folders->len; i++)
		g_free (m->folders->pdata[i]);
	g_ptr_array_free (m->folders, TRUE);

	camel_folder_info_free (m->finfo);
	g_object_unref (m->store);
}

static MailMsgInfo refresh_folders_info = {
	sizeof (struct _refresh_folders_msg),
	(MailMsgDescFunc) refresh_folders_desc,
	(MailMsgExecFunc) refresh_folders_exec,
	(MailMsgDoneFunc) refresh_folders_done,
	(MailMsgFreeFunc) refresh_folders_free
};

static void
receive_update_got_folderinfo (GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
	CamelFolderInfo *info = NULL;
	struct _send_info *send_info = user_data;
	GError *local_error = NULL;

	mail_folder_cache_note_store_finish (
		MAIL_FOLDER_CACHE (source_object),
		result, &info, &local_error);

	/* Ignore cancellations. */
	if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (info == NULL);
		g_error_free (local_error);

		receive_done (send_info);

	/* XXX Need to hand this off to an EAlertSink. */
	} else if (local_error != NULL) {
		g_warn_if_fail (info == NULL);
		report_error_to_ui (send_info->service, NULL, local_error, NULL);
		g_error_free (local_error);

		receive_done (send_info);

	/* CamelFolderInfo may be NULL even if no error occurred. */
	} else if (info != NULL) {
		GPtrArray *folders = g_ptr_array_new ();
		struct _refresh_folders_msg *m;

		m = mail_msg_new (&refresh_folders_info);
		m->store = CAMEL_STORE (g_object_ref (send_info->service));
		m->folders = folders;
		m->info = send_info;
		m->finfo = info;  /* takes ownership */

		mail_msg_unordered_push (m);

	} else {
		receive_done (send_info);
	}
}

static void
receive_update_got_store (CamelStore *store,
                          struct _send_info *info)
{
	MailFolderCache *folder_cache;

	folder_cache = e_mail_session_get_folder_cache (
		E_MAIL_SESSION (info->session));

	if (store != NULL) {
		CamelProvider *provider;

		/* do not update remote stores in offline */
		provider = camel_service_get_provider (CAMEL_SERVICE (store));
		if (provider && (provider->flags & CAMEL_PROVIDER_IS_REMOTE) != 0 &&
		    !camel_session_get_online (info->session))
			store = NULL;
	}

	if (store != NULL) {
		mail_folder_cache_note_store (
			folder_cache, store, info->cancellable,
			receive_update_got_folderinfo, info);
	} else {
		receive_done (info);
	}
}


struct _refresh_local_store_msg {
	MailMsg base;

	CamelStore *store;
	gboolean delete_junk;
	gboolean expunge_trash;
};

static gchar *
refresh_local_store_desc (struct _refresh_local_store_msg *m)
{
	const gchar *display_name;

	display_name = camel_service_get_display_name (CAMEL_SERVICE (m->store));

	if (m->delete_junk && m->expunge_trash)
		return g_strdup_printf (_("Deleting junk and expunging trash at “%s”"), display_name);
	else if (m->delete_junk)
		return g_strdup_printf (_("Deleting junk at “%s”"), display_name);
	else
		return g_strdup_printf (_("Expunging trash at “%s”"), display_name);
}

static void
refresh_local_store_exec (struct _refresh_local_store_msg *m,
			  GCancellable *cancellable,
			  GError **error)
{
	if (m->delete_junk && !delete_junk_sync (m->store, cancellable, error))
		return;

	if (m->expunge_trash) {
		CamelFolder *trash;

		trash = camel_store_get_trash_folder_sync (m->store, cancellable, error);

		if (trash != NULL) {
			e_mail_folder_expunge_sync (trash, cancellable, error);
			g_object_unref (trash);
		}
	}
}

static void
refresh_local_store_free (struct _refresh_local_store_msg *m)
{
	g_object_unref (m->store);
}

static MailMsgInfo refresh_local_store_info = {
	sizeof (struct _refresh_local_store_msg),
	(MailMsgDescFunc) refresh_local_store_desc,
	(MailMsgExecFunc) refresh_local_store_exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) refresh_local_store_free
};

static void
maybe_delete_junk_or_expunge_local_store (EMailSession *session)
{
	CamelStore *store;
	gboolean delete_junk = FALSE, expunge_trash = FALSE;
	struct _refresh_local_store_msg *m;

	store = e_mail_session_get_local_store (session);
	test_should_delete_junk_or_expunge (store, &delete_junk, &expunge_trash);

	if (!delete_junk && !expunge_trash)
		return;

	m = mail_msg_new (&refresh_local_store_info);
	m->store = g_object_ref (store);
	m->delete_junk = delete_junk;
	m->expunge_trash = expunge_trash;

	mail_msg_unordered_push (m);
}

static CamelService *
ref_default_transport (EMailSession *session)
{
	ESource *source;
	ESourceRegistry *registry;
	CamelService *service;
	const gchar *extension_name;
	const gchar *uid;

	registry = e_mail_session_get_registry (session);
	source = e_source_registry_ref_default_mail_identity (registry);

	if (source == NULL)
		return NULL;

	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
	if (e_source_has_extension (source, extension_name)) {
		ESourceMailSubmission *extension;
		gchar *tr_uid;

		extension = e_source_get_extension (source, extension_name);
		tr_uid = e_source_mail_submission_dup_transport_uid (extension);

		g_object_unref (source);
		source = e_source_registry_ref_source (registry, tr_uid);

		g_free (tr_uid);
	} else {
		g_object_unref (source);
		source = NULL;
	}

	if (source == NULL)
		return NULL;

	uid = e_source_get_uid (source);
	service = camel_session_ref_service (CAMEL_SESSION (session), uid);

	g_object_unref (source);

	return service;
}

static GtkWidget *
send_receive (GtkWindow *parent,
              EMailSession *session,
              gboolean allow_send)
{
	CamelFolder *local_outbox;
	CamelService *transport;
	GList *scan, *siter;

	if (send_recv_dialog != NULL) {
		if (parent)
			gtk_window_present (GTK_WINDOW (send_recv_dialog));

		return send_recv_dialog;
	}

	transport = ref_default_transport (session);

	local_outbox =
		e_mail_session_get_local_folder (
		session, E_MAIL_LOCAL_FOLDER_OUTBOX);

	build_dialog (parent, session, local_outbox, transport, allow_send);

	if (transport != NULL)
		g_object_unref (transport);

	maybe_delete_junk_or_expunge_local_store (session);

	if (!send_data)
		return NULL;

	scan = g_list_copy (send_data->infos);

	for (siter = scan; siter != NULL; siter = siter->next) {
		struct _send_info *info = siter->data;

		if (!CAMEL_IS_SERVICE (info->service))
			continue;

		switch (info->type) {
		case SEND_RECEIVE:
			mail_fetch_mail (
				CAMEL_STORE (info->service),
				E_FILTER_SOURCE_INCOMING,
				NULL, NULL, NULL,
				info->cancellable,
				receive_get_folder, info,
				receive_status, info,
				receive_done, info);
			break;
		case SEND_SEND:
			/* todo, store the folder in info? */
			mail_send_queue (
				session, local_outbox,
				CAMEL_TRANSPORT (info->service),
				E_FILTER_SOURCE_OUTGOING,
				TRUE,
				info->cancellable,
				receive_get_folder, info,
				receive_status, info,
				send_done, info);
			break;
		case SEND_UPDATE:
			receive_update_got_store (CAMEL_STORE (info->service), info);
			break;
		default:
			break;
		}
	}

	g_list_free (scan);

	if (send_data && g_hash_table_size (send_data->active) == 0) {
		if (send_data->gd)
			gtk_widget_destroy ((GtkWidget *) send_data->gd);
		free_send_data ();
	}

	return send_recv_dialog;
}

GtkWidget *
mail_send_receive (GtkWindow *parent,
                   EMailSession *session)
{
	return send_receive (parent, session, TRUE);
}

GtkWidget *
mail_receive (GtkWindow *parent,
              EMailSession *session)
{
	return send_receive (parent, session, FALSE);
}

/* We setup the download info's in a hashtable, if we later
 * need to build the gui, we insert them in to add them. */
void
mail_receive_service (CamelService *service)
{
	struct _send_info *info;
	struct _send_data *data;
	CamelSession *session;
	CamelFolder *local_outbox;
	const gchar *uid;
	send_info_t type = SEND_INVALID;

	g_return_if_fail (CAMEL_IS_SERVICE (service));

	uid = camel_service_get_uid (service);
	session = camel_service_ref_session (service);

	data = setup_send_data (E_MAIL_SESSION (session));
	info = g_hash_table_lookup (data->active, uid);

	if (info != NULL)
		goto exit;

	type = get_receive_type (service);

	if (type == SEND_INVALID || type == SEND_SEND)
		goto exit;

	info = g_malloc0 (sizeof (*info));
	info->type = type;
	info->progress_bar = NULL;
	info->session = g_object_ref (session);
	info->service = g_object_ref (service);
	info->cancellable = camel_operation_new ();
	info->cancel_button = NULL;
	info->data = data;
	info->state = SEND_ACTIVE;
	info->timeout_id = 0;

	g_signal_connect (
		info->cancellable, "status",
		G_CALLBACK (operation_status), info);

	d (printf ("Adding new info %p\n", info));

	g_hash_table_insert (data->active, g_strdup (uid), info);

	switch (info->type) {
	case SEND_RECEIVE:
		mail_fetch_mail (
			CAMEL_STORE (service),
			E_FILTER_SOURCE_INCOMING,
			NULL, NULL, NULL,
			info->cancellable,
			receive_get_folder, info,
			receive_status, info,
			receive_done, info);
		break;
	case SEND_SEND:
		/* todo, store the folder in info? */
		local_outbox =
			e_mail_session_get_local_folder (
			E_MAIL_SESSION (session),
			E_MAIL_LOCAL_FOLDER_OUTBOX);
		mail_send_queue (
			E_MAIL_SESSION (session),
			local_outbox,
			CAMEL_TRANSPORT (service),
			E_FILTER_SOURCE_OUTGOING,
			FALSE,
			info->cancellable,
			receive_get_folder, info,
			receive_status, info,
			send_done, info);
		break;
	case SEND_UPDATE:
		receive_update_got_store (CAMEL_STORE (service), info);
		break;
	default:
		g_return_if_reached ();
	}

exit:
	g_object_unref (session);
}

static void
do_mail_send (EMailSession *session,
	      gboolean immediately)
{
	CamelFolder *local_outbox;
	CamelService *service;
	struct _send_info *info;
	struct _send_data *data;
	send_info_t type = SEND_INVALID;

	g_return_if_fail (E_IS_MAIL_SESSION (session));

	service = ref_default_transport (session);
	if (service == NULL)
		return;

	data = setup_send_data (session);
	info = g_hash_table_lookup (data->active, SEND_URI_KEY);
	if (info != NULL) {
		info->again++;
		d (printf ("send of %s still in progress\n", transport->url));
		g_object_unref (service);
		return;
	}

	d (printf ("starting non-interactive send of '%s'\n", transport->url));

	type = get_receive_type (service);
	if (type == SEND_INVALID) {
		g_object_unref (service);
		return;
	}

	info = g_malloc0 (sizeof (*info));
	info->type = SEND_SEND;
	info->progress_bar = NULL;
	info->session = CAMEL_SESSION (g_object_ref (session));
	info->service = g_object_ref (service);
	info->cancellable = NULL;
	info->cancel_button = NULL;
	info->data = data;
	info->state = SEND_ACTIVE;
	info->timeout_id = 0;

	d (printf ("Adding new info %p\n", info));

	g_hash_table_insert (data->active, g_strdup (SEND_URI_KEY), info);

	/* todo, store the folder in info? */
	local_outbox =
		e_mail_session_get_local_folder (
		session, E_MAIL_LOCAL_FOLDER_OUTBOX);

	mail_send_queue (
		session, local_outbox,
		CAMEL_TRANSPORT (service),
		E_FILTER_SOURCE_OUTGOING,
		immediately,
		info->cancellable,
		receive_get_folder, info,
		receive_status, info,
		send_done, info);

	g_object_unref (service);
}

void
mail_send (EMailSession *session)
{
	do_mail_send (session, FALSE);
}

void
mail_send_immediately (EMailSession *session)
{
	do_mail_send (session, TRUE);
}
