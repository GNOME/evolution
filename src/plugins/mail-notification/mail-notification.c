/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Miguel Angel Lopez Hernandez <miguel@gulev.org.mx>
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#ifndef G_OS_WIN32
#include <gio/gdesktopappinfo.h>
#endif

#ifdef HAVE_CANBERRA
#include <canberra-gtk.h>
#endif

#include <time.h>

#include "mail/e-mail-backend.h"
#include "mail/e-mail-reader.h"
#include "mail/e-mail-view.h"
#include "mail/em-event.h"
#include "mail/em-folder-tree.h"
#include "mail/message-list.h"
#include "shell/e-shell-view.h"

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#ifdef HAVE_LIBUNITY
#include <unity.h>
#endif

#define CONF_KEY_NOTIFY_ONLY_INBOX	"notify-only-inbox"
#define CONF_KEY_ENABLED_STATUS	        "notify-status-enabled"
#define CONF_KEY_STATUS_NOTIFICATION	"notify-status-notification"
#define CONF_KEY_ENABLED_SOUND		"notify-sound-enabled"

static GMutex mlock;
static gulong not_accounts_handler_id = 0;
static GHashTable *not_accounts = NULL; /* gchar * ~> NULL; UIDs of accounts which have disabled notifications */
static gpointer em_event_handle = NULL;

/**
 * each part should "implement" its own "public" functions:
 * a) void new_notify_... (EMEventTargetFolder *t)
 *    when new_notify message is sent by Evolution
 *
 * b) void read_notify_... (EMEventTargetMessage *t)
 *    it is called when read_notify message is sent by Evolution
 *
 * c) void enable_... (gint enable)
 *    when plugin itself or the part is enabled/disabled
 *
 * It also should have its own GSettings key for enabled state. In each particular
 * function it should do its work as expected. enable_... will be called always
 * when disabling plugin, but only when enabling/disabling part itself.
 **/

/* -------------------------------------------------------------------  */
/*                       Helper functions                               */
/* -------------------------------------------------------------------  */

static gboolean
is_part_enabled (const gchar *key)
{
	/* the part is enabled by default */
	gboolean res = TRUE;
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.mail-notification");

	res = g_settings_get_boolean (settings, key);

	g_object_unref (settings);

	return res;
}

static gboolean
can_notify_account (CamelStore *store)
{
	gboolean can_notify;
	const gchar *uid;

	if (!store)
		return TRUE;

	g_mutex_lock (&mlock);

	uid = camel_service_get_uid (CAMEL_SERVICE (store));
	can_notify = !uid || !not_accounts || !g_hash_table_contains (not_accounts, uid);

	g_mutex_unlock (&mlock);

	return can_notify;
}

static void
mail_notify_not_accounts_changed_locked (GSettings *settings)
{
	gchar **uids;

	g_return_if_fail (G_IS_SETTINGS (settings));

	uids = g_settings_get_strv (settings, "notify-not-accounts");

	if (uids && *uids) {
		gint ii;

		if (!not_accounts)
			not_accounts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

		g_hash_table_remove_all (not_accounts);

		for (ii = 0; uids[ii]; ii++) {
			g_hash_table_insert (not_accounts, g_strdup (uids[ii]), NULL);
		}
	} else {
		g_clear_pointer (&not_accounts, g_hash_table_destroy);
	}

	g_strfreev (uids);
}

static void
mail_notify_not_accounts_changed_cb (GSettings *settings,
				     const gchar *key,
				     gpointer user_data)
{
	g_return_if_fail (G_IS_SETTINGS (settings));

	g_mutex_lock (&mlock);
	mail_notify_not_accounts_changed_locked (settings);
	g_mutex_unlock (&mlock);
}

/* -------------------------------------------------------------------  */
/*                           DBUS part                                  */
/* -------------------------------------------------------------------  */

#define DBUS_PATH		"/org/gnome/evolution/mail/newmail"
#define DBUS_INTERFACE		"org.gnome.evolution.mail.dbus.Signal"

static GDBusConnection *connection = NULL;

static gboolean init_gdbus (void);

static void
send_dbus_message (const gchar *name,
                   const gchar *display_name,
                   guint new_count,
                   const gchar *msg_uid,
                   const gchar *msg_sender,
                   const gchar *msg_subject)
{
	GDBusMessage *message;
	GVariantBuilder *builder;
	GError *error = NULL;

	g_return_if_fail (name != NULL);
	g_return_if_fail (display_name != NULL);
	g_return_if_fail (g_utf8_validate (name, -1, NULL));
	g_return_if_fail (g_utf8_validate (display_name, -1, NULL));
	g_return_if_fail (msg_uid == NULL || g_utf8_validate (msg_uid, -1, NULL));
	g_return_if_fail (msg_sender == NULL || g_utf8_validate (msg_sender, -1, NULL));
	g_return_if_fail (msg_subject == NULL || g_utf8_validate (msg_subject, -1, NULL));

	/* Create a new message on the DBUS_INTERFACE */
	if (!(message = g_dbus_message_new_signal (DBUS_PATH, DBUS_INTERFACE, name)))
		return;

	builder = g_variant_builder_new (G_VARIANT_TYPE_TUPLE);

	/* Appends the data as an argument to the message */
	g_variant_builder_add (builder, "(s)", display_name);

	if (new_count) {
		g_variant_builder_add (builder, "(s)", display_name);
		g_variant_builder_add (builder, "(u)", new_count);
	}

	#define add_named_param(name, value) \
		if (value) { \
			gchar *val; \
			val = g_strconcat (name, ":", value, NULL); \
			g_variant_builder_add (builder, "(s)", val); \
			g_free (val); \
		}

	add_named_param ("msg_uid", msg_uid);
	add_named_param ("msg_sender", msg_sender);
	add_named_param ("msg_subject", msg_subject);

	#undef add_named_param

	g_dbus_message_set_body (message, g_variant_builder_end (builder));
	g_variant_builder_unref (builder);

	/* Sends the message */
	g_dbus_connection_send_message (
		connection, message,
		G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL, &error);

	/* Frees the message */
	g_object_unref (message);

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

static gboolean
reinit_gdbus (gpointer user_data)
{
	if (init_gdbus ())
		return FALSE;

	/* keep trying to re-establish dbus connection */
	return TRUE;
}

static void
connection_closed_cb (GDBusConnection *pconnection,
                      gboolean remote_peer_vanished,
                      GError *error,
                      gpointer user_data)
{
	g_return_if_fail (connection != pconnection);

	g_object_unref (connection);
	connection = NULL;

	e_named_timeout_add (3000, reinit_gdbus, NULL);
}

static gboolean
init_gdbus (void)
{
	GError *error = NULL;

	if (connection != NULL)
		return TRUE;

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (error != NULL) {
		g_warning ("could not get system bus: %s\n", error->message);
		g_error_free (error);

		return FALSE;
	}

	g_dbus_connection_set_exit_on_close (connection, FALSE);

	g_signal_connect (
		connection, "closed",
		G_CALLBACK (connection_closed_cb), NULL);

	return TRUE;
}

/* -------------------------------------------------------------------  */

static void
new_notify_dbus (EMEventTargetFolder *t)
{
	if (connection != NULL)
		send_dbus_message (
			"Newmail", t->display_name, t->new,
			t->msg_uid, t->msg_sender, t->msg_subject);
}

static void
read_notify_dbus (EMEventTargetMessage *t)
{
	if (connection != NULL)
		send_dbus_message (
			"MessageReading",
			camel_folder_get_display_name (t->folder),
			0, NULL, NULL, NULL);
}

static void
enable_dbus (gint enable)
{
	if (enable) {
		/* we ignore errors here */
		init_gdbus ();
	} else {
		g_clear_object (&connection);
	}
}

/* -------------------------------------------------------------------  */
/*                     Notification area part                           */
/* -------------------------------------------------------------------  */

#ifdef HAVE_LIBNOTIFY

static guint status_count = 0;
static GHashTable *unread_messages_by_folder = NULL;

#ifdef HAVE_LIBUNITY
static guint unread_message_count = 0;

static void
update_unity_launcher_entry (void)
{
	UnityLauncherEntry *entry = unity_launcher_entry_get_for_desktop_id ("org.gnome.Evolution.desktop");

	if (entry) {
		unity_launcher_entry_set_count (entry, unread_message_count);
		unity_launcher_entry_set_count_visible (entry, unread_message_count != 0);
	}
}
#endif

static NotifyNotification *notify = NULL;

static void
remove_notification (void)
{
	if (notify)
		notify_notification_close (notify, NULL);

	notify = NULL;

	status_count = 0;

#ifdef HAVE_LIBUNITY
	unread_message_count = 0;
	update_unity_launcher_entry ();
#endif
}

static gboolean
notification_callback (NotifyNotification *notification)
{
	GError *error = NULL;

	notify_notification_show (notification, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	return FALSE;
}

/* -------------------------------------------------------------------  */

typedef struct _NotifyDefaultActionData NotifyDefaultActionData;

struct _NotifyDefaultActionData {
	gchar *folder_uri;
	gchar *msg_uid;
};

static NotifyDefaultActionData *
notify_default_action_data_new (const gchar *folder_uri,
                                const gchar *msg_uid)
{
	NotifyDefaultActionData *result;
	result = g_slice_new (NotifyDefaultActionData);
	result->folder_uri = g_strdup (folder_uri);
	result->msg_uid = g_strdup (msg_uid);
	return result;
}

static void
notify_default_action_free_cb (gpointer user_data)
{
	NotifyDefaultActionData *data = user_data;
	g_free (data->folder_uri);
	g_free (data->msg_uid);
	g_slice_free (NotifyDefaultActionData, data);
}

static void
notify_default_action_cb (NotifyNotification *notification,
                          gchar *label,
                          gpointer user_data)
{
	NotifyDefaultActionData *data = user_data;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree;
	EUIAction *action;
	GtkApplication *application;
	GList *list, *fallback = NULL;

	shell = e_shell_get_default ();
	application = GTK_APPLICATION (shell);
	list = gtk_application_get_windows (application);

	/* Find the first EShellWindow in the list. */
	while (list != NULL) {
		if (E_IS_SHELL_WINDOW (list->data)) {
			if (!fallback)
				fallback = list;

			if (g_strcmp0 (e_shell_window_get_active_view (list->data), "mail") == 0)
				break;
		}

		list = g_list_next (list);
	}

	if (!list)
		list = fallback;

	if (!list) {
		g_warn_if_reached ();
		return;
	}

	/* Present the shell window. */
	shell_window = E_SHELL_WINDOW (list->data);
	gtk_window_present (GTK_WINDOW (shell_window));

	/* Switch to the mail view. */
	shell_view = e_shell_window_get_shell_view (shell_window, "mail");
	action = e_shell_view_get_switcher_action (shell_view);
	e_ui_action_set_active (action, TRUE);

	/* Select the latest folder with new mail. */
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
	em_folder_tree_set_selected (folder_tree, data->folder_uri, FALSE);

	if (data->msg_uid) {
		EMailView *mail_view = NULL;

		g_object_get (e_shell_view_get_shell_content (shell_view), "mail-view", &mail_view, NULL);
		if (mail_view) {
			MessageList *message_list;

			/* Select the message. */
			message_list = MESSAGE_LIST (e_mail_reader_get_message_list (E_MAIL_READER (mail_view)));
			message_list_select_uid (message_list, data->msg_uid, TRUE);

			g_clear_object (&mail_view);
		}
	}

	remove_notification ();
}

static void
uri_to_folder_cb (GObject *source_object,
		  GAsyncResult *result,
		  gpointer user_data)
{
	NotifyDefaultActionData *data = user_data;
	EMailSession *session = E_MAIL_SESSION (source_object);
	CamelFolder *folder;
	GError *error = NULL;

	folder = e_mail_session_uri_to_folder_finish (session, result, &error);

	if (!folder) {
		if (error) {
			g_warning ("Failed to get folder: %s", error->message);
			g_clear_error (&error);
		}
		notify_default_action_free_cb (data);
	        return;
	}

	camel_folder_set_message_flags (folder, data->msg_uid, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);

	g_object_unref (folder);

	notify_default_action_free_cb (data);
}

static void
notify_mark_as_read_action_cb (NotifyNotification *notification,
                               gchar *action,
                               gpointer user_data)
{
	NotifyDefaultActionData *data = user_data;
	NotifyDefaultActionData *async_data;
	EShell *shell;
	EShellBackend *backend;
	EMailBackend *mail_backend;
	EMailSession *session;

	if (!data || !data->folder_uri || !data->msg_uid)
		return;

	shell = e_shell_get_default ();
	backend = e_shell_get_backend_by_name (shell, "mail");
	if (!backend)
		return;

	mail_backend = E_MAIL_BACKEND (backend);
	session = e_mail_backend_get_session (mail_backend);
	async_data = notify_default_action_data_new (data->folder_uri, data->msg_uid);
	e_mail_session_uri_to_folder (
		session,
		async_data->folder_uri,
		0,
		G_PRIORITY_DEFAULT,
		NULL,
		uri_to_folder_cb,
		async_data);

	remove_notification ();
}

/* Check if actions are supported by the notification daemon.
 * This is really only for Ubuntu's Notify OSD, which does not
 * support actions.  Pretty much all other implementations do. */
static gboolean
can_support_actions (void)
{
	static gboolean supports_actions = FALSE;
	static gboolean have_checked = FALSE;

	if (!have_checked) {
		GList *caps = NULL;
		GList *match;

		have_checked = TRUE;

		caps = notify_get_server_caps ();

		match = g_list_find_custom (
			caps, "actions", (GCompareFunc) strcmp);
		supports_actions = (match != NULL);

		g_list_foreach (caps, (GFunc) g_free, NULL);
		g_list_free (caps);
	}

	return supports_actions;
}

static void
new_notify_status (EMEventTargetFolder *t)
{
	gchar *escaped_text;
	GString *text;
	const gchar *summary;
	const gchar *icon_name;

	status_count += t->new;

	text = g_string_sized_new (256);

	g_string_append_printf (text, ngettext (
		/* Translators: '%d' is the count of mails received. */
		"You have received %d new message.",
		"You have received %d new messages.",
		status_count), status_count);

	if (t->msg_sender) {
		g_string_append_c (text, '\n');

		/* Translators: "From:" is preceding a new mail
		 * sender address, like "From: user@example.com" */
		g_string_append_printf (text, _("From: %s"), t->msg_sender);
	}

	if (t->msg_subject) {
		g_string_append_c (text, '\n');

		/* Translators: "Subject:" is preceding a new mail
		 * subject, like "Subject: It happened again" */
		g_string_append_printf (text, _("Subject: %s"), t->msg_subject);
	}

	if (t->full_display_name) {
		g_string_append_c (text, '\n');

		/* Translators: The '%s' is replaced by the folder name, where a new
		 * mail message arrived. Example: "Folder: On This Computer : Inbox" */
		g_string_append_printf (text, _("Folder: %s"), t->full_display_name);
	}

	if (status_count > 1 && (t->msg_sender || t->msg_subject)) {
		guint additional_messages = status_count - 1;

		g_string_append_c (text, '\n');

		g_string_append_printf (text, ngettext (
			/* Translators: %d is the count of mails received in addition
			 * to the one displayed in this notification. */
			"(and %d more)",
			"(and %d more)",
			additional_messages), additional_messages);
	}

	if (e_util_is_running_flatpak ())
		icon_name = "org.gnome.Evolution";
	else
		icon_name = "evolution";

	summary = _("New email in Evolution");
	escaped_text = g_markup_escape_text (text->str, -1);

	if (notify) {
		notify_notification_update (
			notify, summary, escaped_text, icon_name);
	} else {
		if (!notify_init ("evolution-mail-notification"))
			fprintf (stderr,"notify init error");

		notify = notify_notification_new (
			summary, escaped_text, icon_name);

		notify_notification_set_urgency (
			notify, NOTIFY_URGENCY_NORMAL);

		notify_notification_set_timeout (
			notify, NOTIFY_EXPIRES_DEFAULT);

		notify_notification_set_hint (
			notify, "desktop-entry",
			g_variant_new_string ("org.gnome.Evolution"));

		if (e_util_is_running_gnome ()) {
			notify_notification_set_hint (
				notify, "sound-name",
				g_variant_new_string ("message-new-email"));
		}
	}

	/* Check if actions are supported */
	if (can_support_actions ()) {
		gchar *label;
		NotifyDefaultActionData *data;

		/* NotifyAction takes ownership. */
		data = notify_default_action_data_new (t->folder_name, t->msg_uid);

		label = g_strdup_printf (
			/* Translators: The '%s' is a mail
			 * folder name.  (e.g. "Show Inbox") */
			_("Show %s"), t->display_name);

		notify_notification_clear_actions (notify);
		notify_notification_add_action (
			notify, "default", label,
			notify_default_action_cb,
			data,
			notify_default_action_free_cb);

		if (t->msg_uid && t->folder_name && status_count == 1) {
			NotifyDefaultActionData *mark_data;

			mark_data = notify_default_action_data_new (t->folder_name, t->msg_uid);
			notify_notification_add_action (
				notify,
				"mark-read",
				_("Mark as read"),
				notify_mark_as_read_action_cb,
				mark_data,
				notify_default_action_free_cb);
		}
		g_free (label);
	}

	g_idle_add_full (
		G_PRIORITY_DEFAULT_IDLE,
		(GSourceFunc) notification_callback,
		g_object_ref (notify),
		(GDestroyNotify) g_object_unref);

	g_free (escaped_text);
	g_string_free (text, TRUE);
}

static void
unread_notify_status (EMEventTargetFolderUnread *t)
{
	gpointer lookup;
	guint old_unread;

	if (unread_messages_by_folder == NULL)
		unread_messages_by_folder = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	lookup = g_hash_table_lookup (unread_messages_by_folder, t->folder_uri);
	old_unread = lookup == NULL ? 0 : GPOINTER_TO_UINT (lookup);

	/* Infer that a message has been read by a decrease in the number of unread
	 * messages in a folder. */
	if (t->unread < old_unread)
		remove_notification ();
#ifdef HAVE_LIBUNITY
	else if (t->is_inbox) {
		unread_message_count += t->unread - old_unread;
		update_unity_launcher_entry ();
	}
#endif

	if (t->unread != old_unread) {
		if (t->unread) {
			g_hash_table_insert (unread_messages_by_folder, g_strdup (t->folder_uri), GUINT_TO_POINTER (t->unread));
		} else {
			g_hash_table_remove (unread_messages_by_folder, t->folder_uri);
		}
	}
}

static void
read_notify_status (EMEventTargetMessage *t)
{
	remove_notification ();
}
#endif

/* -------------------------------------------------------------------  */
/*                         Sound part                                   */
/* -------------------------------------------------------------------  */

/* min no. seconds between newmail notifications */
#define NOTIFY_THROTTLE 30

#define CONF_KEY_SOUND_BEEP		"notify-sound-beep"
#define CONF_KEY_SOUND_FILE		"notify-sound-file"
#define CONF_KEY_SOUND_PLAY_FILE	"notify-sound-play-file"
#define CONF_KEY_SOUND_USE_THEME        "notify-sound-use-theme"

#ifdef HAVE_CANBERRA
static ca_context *mailnotification = NULL;
#endif

static gint eca_debug = -1;

static void
do_play_sound (gboolean beep,
               gboolean use_theme,
               const gchar *file)
{
	if (eca_debug == -1)
		eca_debug = g_strcmp0 (g_getenv ("ECA_DEBUG"), "1") == 0 ? 1 : 0;

	if (!beep) {
#ifdef HAVE_CANBERRA
		gint err;

		if (!use_theme && file && *file)
			err = ca_context_play (
				mailnotification, 0,
				CA_PROP_MEDIA_FILENAME, file,
				NULL);
		else
			err = ca_context_play (
				mailnotification, 0,
				CA_PROP_EVENT_ID, "message-new-email",
				NULL);

		if (eca_debug) {
			if (err != 0 && file && *file)
				e_util_debug_print ("ECA", "Mail Notification: Failed to play '%s': %s\n", file, ca_strerror (err));
			else if (err != 0)
				e_util_debug_print ("ECA", "Mail Notification: Failed to play 'message-new-email' sound: %s\n", ca_strerror (err));
			else if (file && *file)
				e_util_debug_print ("ECA", "Mail Notification: Played file '%s'\n", file);
			else
				e_util_debug_print ("ECA", "Mail Notification: Played 'message-new-email' sound\n");
		}
#else
		if (eca_debug)
			e_util_debug_print ("ECA", "Mail Notification: Cannot play sound, not compiled with libcanberra\n");
#endif
	} else
		gdk_display_beep (gdk_display_get_default ());
}

struct _SoundNotifyData {
	time_t last_notify;
	guint notify_idle_id;
};

static gboolean
sound_notify_idle_cb (gpointer user_data)
{
	gchar *file;
	GSettings *settings;
	struct _SoundNotifyData *data = (struct _SoundNotifyData *) user_data;

	g_return_val_if_fail (data != NULL, FALSE);

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.mail-notification");
	file = g_settings_get_string (settings, CONF_KEY_SOUND_FILE);

	do_play_sound (
		is_part_enabled (CONF_KEY_SOUND_BEEP),
		is_part_enabled (CONF_KEY_SOUND_USE_THEME),
		file);

	g_object_unref (settings);
	g_free (file);

	time (&data->last_notify);

	data->notify_idle_id = 0;

	return FALSE;
}

/* -------------------------------------------------------------------  */

static void
new_notify_sound (EMEventTargetFolder *t)
{
	time_t last_newmail;
	static struct _SoundNotifyData data = {0, 0};

	time (&last_newmail);

	/* just put it to the idle queue, if not under GNOME, where everything is
	   handled by the libnotify */
	if (data.notify_idle_id == 0 &&
	    (last_newmail - data.last_notify >= NOTIFY_THROTTLE) &&
	    !e_util_is_running_gnome ())
		data.notify_idle_id = g_idle_add_full (
			G_PRIORITY_LOW, sound_notify_idle_cb, &data, NULL);
}

static void
read_notify_sound (EMEventTargetMessage *t)
{
	/* we do nothing here */
}

static void
enable_sound (gint enable)
{
#ifdef HAVE_CANBERRA
	if (enable) {
		ca_context_create (&mailnotification);
		ca_context_change_props (
			mailnotification,
			CA_PROP_APPLICATION_NAME,
			"mailnotification Plugin",
			NULL);
	} else {
		ca_context_destroy (mailnotification);
		mailnotification = NULL;
	}
#endif
}

/* -------------------------------------------------------------------  */
/*                     Module event handlers                            */
/* -------------------------------------------------------------------  */

static void
mail_notify_new_notify (EEvent *ee,
                        EEventItem *item,
                        gpointer data)
{
	EMEventTargetFolder *t = (EMEventTargetFolder *) ee->target;

	g_return_if_fail (t != NULL);

	if ((!t->is_inbox && is_part_enabled (CONF_KEY_NOTIFY_ONLY_INBOX)) ||
	    !can_notify_account (t->store))
		return;

	g_mutex_lock (&mlock);

	new_notify_dbus (t);

#ifdef HAVE_LIBNOTIFY
	if (is_part_enabled (CONF_KEY_ENABLED_STATUS) || e_util_is_running_gnome ())
		new_notify_status (t);
#endif

	if (is_part_enabled (CONF_KEY_ENABLED_SOUND))
		new_notify_sound (t);

	g_mutex_unlock (&mlock);
}

static void
mail_notify_unread_notify (EEvent *ee,
                           EEventItem *item,
                           gpointer data)
{
#ifdef HAVE_LIBNOTIFY
	EMEventTargetFolderUnread *t = (EMEventTargetFolderUnread *) ee->target;

	g_return_if_fail (t != NULL);

	if ((!t->is_inbox && is_part_enabled (CONF_KEY_NOTIFY_ONLY_INBOX)) ||
	    !can_notify_account (t->store))
		return;

	g_mutex_lock (&mlock);

	if (is_part_enabled (CONF_KEY_ENABLED_STATUS) || e_util_is_running_gnome ())
		unread_notify_status (t);

	g_mutex_unlock (&mlock);
#endif
}

static void
mail_notify_read_notify (EEvent *ee,
                         EEventItem *item,
                         gpointer data)
{
	EMEventTargetMessage *t = (EMEventTargetMessage *) ee->target;

	g_return_if_fail (t != NULL);

	if (!can_notify_account (camel_folder_get_parent_store (t->folder)))
		return;

	g_mutex_lock (&mlock);

	read_notify_dbus (t);

#ifdef HAVE_LIBNOTIFY
	if (is_part_enabled (CONF_KEY_ENABLED_STATUS) || e_util_is_running_gnome ())
		read_notify_status (t);
#endif

	if (is_part_enabled (CONF_KEY_ENABLED_SOUND))
		read_notify_sound (t);

	g_mutex_unlock (&mlock);
}

static EEventItem mail_notify_event_items[] = {
	{ E_EVENT_PASS, 0, "folder.changed", EM_EVENT_TARGET_FOLDER,
	  mail_notify_new_notify, NULL, EM_EVENT_FOLDER_NEWMAIL },
	{ E_EVENT_PASS, 0, "folder.unread-updated", EM_EVENT_TARGET_FOLDER_UNREAD,
	  mail_notify_unread_notify, NULL, 0 },
	{ E_EVENT_PASS, 0, "message.reading", EM_EVENT_TARGET_MESSAGE,
	  mail_notify_read_notify, NULL, 0 },
};

static void
mail_notify_free_event_items (EEvent *event,
                              GSList *items,
                              gpointer data)
{
	g_slist_free (items);
}

void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	GSettings *settings;
	EMEvent *event;
	GSList *items = NULL;
	gint ii;

	enable_dbus (1);

	if (is_part_enabled (CONF_KEY_ENABLED_SOUND))
		enable_sound (1);

	g_mutex_lock (&mlock);

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.mail-notification");
	mail_notify_not_accounts_changed_locked (settings);
	not_accounts_handler_id = g_signal_connect (
		settings, "changed::notify-not-accounts",
		G_CALLBACK (mail_notify_not_accounts_changed_cb), NULL);
	g_object_unref (settings);

	g_mutex_unlock (&mlock);

	event = em_event_peek ();
	for (ii = G_N_ELEMENTS (mail_notify_event_items) - 1; ii >= 0; ii--)
		items = g_slist_prepend (items, &mail_notify_event_items[ii]);
	em_event_handle = e_event_add_items (
		(EEvent *) event, items,
		mail_notify_free_event_items, NULL);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
	if (em_event_handle) {
		EMEvent *event = em_event_peek ();
		e_event_remove_items ((EEvent *) event, em_event_handle);
		em_event_handle = NULL;
	}

	enable_dbus (0);
	enable_sound (0);

	g_mutex_lock (&mlock);

	if (not_accounts_handler_id) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.plugin.mail-notification");
		g_signal_handler_disconnect (settings, not_accounts_handler_id);
		g_object_unref (settings);

		not_accounts_handler_id = 0;
		g_clear_pointer (&not_accounts, g_hash_table_destroy);
	}

	g_mutex_unlock (&mlock);
}
