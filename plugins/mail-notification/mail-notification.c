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
 *		Miguel Angel Lopez Hernandez <miguel@gulev.org.mx>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#ifdef HAVE_CANBERRA
#include <canberra-gtk.h>
#endif

#include <time.h>

#include <mail/em-utils.h>
#include <mail/em-event.h>
#include <mail/em-folder-tree.h>
#include <shell/e-shell-view.h>

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#define CONF_KEY_NOTIFY_ONLY_INBOX	"notify-only-inbox"
#define CONF_KEY_ENABLED_STATUS	        "notify-status-enabled"
#define CONF_KEY_STATUS_NOTIFICATION	"notify-status-notification"
#define CONF_KEY_ENABLED_SOUND		"notify-sound-enabled"

static gboolean enabled = FALSE;
static GtkWidget *get_cfg_widget (void);
static GMutex mlock;

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
 * d) GtkWidget *get_config_widget_...(void)
 *    to obtain config widget for the particular part
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
	if (!enabled || init_gdbus ())
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
	} else if (connection != NULL) {
		g_object_unref (connection);
		connection = NULL;
	}
}

/* -------------------------------------------------------------------  */
/*                     Notification area part                           */
/* -------------------------------------------------------------------  */

#ifdef HAVE_LIBNOTIFY

static guint status_count = 0;

static NotifyNotification *notify = NULL;

static void
remove_notification (void)
{
	if (notify)
		notify_notification_close (notify, NULL);

	notify = NULL;

	status_count = 0;
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

static void
notify_default_action_cb (NotifyNotification *notification,
                          const gchar *label,
                          const gchar *folder_uri)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree;
	GtkApplication *application;
	GtkAction *action;
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

	g_return_if_fail (list != NULL);

	/* Present the shell window. */
	shell_window = E_SHELL_WINDOW (list->data);
	gtk_window_present (GTK_WINDOW (shell_window));

	/* Switch to the mail view. */
	shell_view = e_shell_window_get_shell_view (shell_window, "mail");
	action = e_shell_view_get_action (shell_view);
	gtk_action_activate (action);

	/* Select the latest folder with new mail. */
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
	em_folder_tree_set_selected (folder_tree, folder_uri, FALSE);

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
	gchar *text;
	const gchar *summary;
	const gchar *icon_name;

	if (!status_count) {
		CamelService *service;
		const gchar *store_name;
		gchar *folder_name;

		service = CAMEL_SERVICE (t->store);
		store_name = camel_service_get_display_name (service);

		folder_name = g_strdup_printf (
			"%s/%s", store_name, t->folder_name);

		status_count = t->new;

		/* Translators: '%d' is the count of mails received. */
		text = g_strdup_printf (ngettext (
			"You have received %d new message.",
			"You have received %d new messages.",
			status_count), status_count);

		g_free (folder_name);

		if (t->msg_sender) {
			gchar *tmp, *str;

			/* Translators: "From:" is preceding a new mail
			 * sender address, like "From: user@example.com" */
			str = g_strdup_printf (_("From: %s"), t->msg_sender);
			tmp = g_strconcat (text, "\n", str, NULL);

			g_free (text);
			g_free (str);

			text = tmp;
		}

		if (t->msg_subject) {
			gchar *tmp, *str;

			/* Translators: "Subject:" is preceding a new mail
			 * subject, like "Subject: It happened again" */
			str = g_strdup_printf (_("Subject: %s"), t->msg_subject);
			tmp = g_strconcat (text, "\n", str, NULL);

			g_free (text);
			g_free (str);

			text = tmp;
		}
	} else {
		status_count += t->new;
		text = g_strdup_printf (ngettext (
			"You have received %d new message.",
			"You have received %d new messages.",
			status_count), status_count);
	}

	icon_name = "evolution";
	summary = _("New email in Evolution");
	escaped_text = g_markup_escape_text (text, strlen (text));

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
			g_variant_new_string (PACKAGE));

		/* Check if actions are supported */
		if (can_support_actions ()) {
			gchar *label;
			gchar *folder_uri;

			/* NotifyAction takes ownership. */
			folder_uri = e_mail_folder_uri_build (
				t->store, t->folder_name);

			label = g_strdup_printf (
				/* Translators: The '%s' is a mail
				 * folder name.  (e.g. "Show Inbox") */
				_("Show %s"), t->display_name);

			notify_notification_add_action (
				notify, "default", label,
				(NotifyActionCallback)
				notify_default_action_cb,
				folder_uri,
				(GFreeFunc) g_free);

			g_free (label);
		}
	}

	g_idle_add_full (
		G_PRIORITY_DEFAULT_IDLE,
		(GSourceFunc) notification_callback,
		g_object_ref (notify),
		(GDestroyNotify) g_object_unref);

	g_free (escaped_text);
	g_free (text);
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

static void
do_play_sound (gboolean beep,
               gboolean use_theme,
               const gchar *file)
{
	if (!beep) {
#ifdef HAVE_CANBERRA
		if (!use_theme && file && *file)
			ca_context_play (
				mailnotification, 0,
				CA_PROP_MEDIA_FILENAME, file,
				NULL);
		else
			ca_context_play (
				mailnotification, 0,
				CA_PROP_EVENT_ID,"message-new-email",
				NULL);
#endif
	} else
		gdk_beep ();
}

struct _SoundConfigureWidgets {
	GtkToggleButton *enable;
	GtkToggleButton *beep;
	GtkToggleButton *use_theme;
	GtkFileChooser *filechooser;
};

static void
sound_file_set_cb (GtkFileChooser *file_chooser,
                   gpointer data)
{
	gchar *file;
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.mail-notification");
	file = gtk_file_chooser_get_filename (file_chooser);

	g_settings_set_string (settings, CONF_KEY_SOUND_FILE, (file != NULL) ? file : "");

	g_object_unref (settings);
	g_free (file);
}

static void
sound_play_cb (GtkWidget *widget,
               struct _SoundConfigureWidgets *scw)
{
	gchar *file;

	if (!gtk_toggle_button_get_active (scw->enable))
		return;

	file = gtk_file_chooser_get_filename (scw->filechooser);

	do_play_sound (
		gtk_toggle_button_get_active (scw->beep),
		gtk_toggle_button_get_active (scw->use_theme),
		file);

	g_free (file);
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

	/* just put it to the idle queue */
	if (data.notify_idle_id == 0 &&
		(last_newmail - data.last_notify >= NOTIFY_THROTTLE))
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
	}
	else
		ca_context_destroy (mailnotification);
#endif
}

static GtkWidget *
get_config_widget_sound (void)
{
	GtkWidget *vbox;
	GtkWidget *container;
	GtkWidget *master;
	GtkWidget *widget;
	gchar *file;
	GSettings *settings;
	GSList *group = NULL;
	struct _SoundConfigureWidgets *scw;
	const gchar *text;

	scw = g_malloc0 (sizeof (struct _SoundConfigureWidgets));

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (vbox);

	container = vbox;

	text = _("_Play sound when a new message arrives");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.mail-notification");

	g_settings_bind (
		settings, CONF_KEY_ENABLED_SOUND,
		widget, "active", G_SETTINGS_BIND_DEFAULT);

	master = widget;
	scw->enable = GTK_TOGGLE_BUTTON (widget);

	widget = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (widget), 0, 0, 12, 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		master, "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	text = _("_Beep");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_settings_bind (
		settings, CONF_KEY_SOUND_BEEP,
		widget, "active", G_SETTINGS_BIND_DEFAULT);

	scw->beep = GTK_TOGGLE_BUTTON (widget);

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	text = _("Use sound _theme");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_settings_bind (
		settings, CONF_KEY_SOUND_USE_THEME,
		widget, "active", G_SETTINGS_BIND_DEFAULT);

	scw->use_theme = GTK_TOGGLE_BUTTON (widget);

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Play _file:");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_settings_bind (
		settings, CONF_KEY_SOUND_PLAY_FILE,
		widget, "active", G_SETTINGS_BIND_DEFAULT);

	text = _("Select sound file");
	widget = gtk_file_chooser_button_new (
		text, GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	scw->filechooser = GTK_FILE_CHOOSER (widget);

	widget = gtk_button_new ();
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_icon_name (
		"media-playback-start", GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (sound_play_cb), scw);

	file = g_settings_get_string (settings, CONF_KEY_SOUND_FILE);

	if (file && *file)
		gtk_file_chooser_set_filename (scw->filechooser, file);

	g_object_unref (settings);
	g_free (file);

	g_signal_connect (
		scw->filechooser, "file-set",
		G_CALLBACK (sound_file_set_cb), scw);

	/* to let structure free properly */
	g_object_set_data_full (G_OBJECT (vbox), "scw-data", scw, g_free);

	return vbox;
}

/* -------------------------------------------------------------------  */
/*                     Plugin itself part                               */
/* -------------------------------------------------------------------  */

static GtkWidget *
get_cfg_widget (void)
{
	GtkWidget *container;
	GtkWidget *widget;
	GSettings *settings;
	const gchar *text;

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.mail-notification");

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_show (widget);

	container = widget;

	text = _("Notify new messages for _Inbox only");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_settings_bind (
		settings, CONF_KEY_NOTIFY_ONLY_INBOX,
		widget, "active", G_SETTINGS_BIND_DEFAULT);

#ifdef HAVE_LIBNOTIFY
	text = _("Show _notification when a new message arrives");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_settings_bind (
		settings, CONF_KEY_ENABLED_STATUS,
		widget, "active", G_SETTINGS_BIND_DEFAULT);
#endif

	widget = get_config_widget_sound ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	g_object_unref (settings);

	return container;
}

void org_gnome_mail_new_notify (EPlugin *ep, EMEventTargetFolder *t);
void org_gnome_mail_read_notify (EPlugin *ep, EMEventTargetMessage *t);

gint e_plugin_lib_enable (EPlugin *ep, gint enable);
GtkWidget *e_plugin_lib_get_configure_widget (EPlugin *epl);

void
org_gnome_mail_new_notify (EPlugin *ep,
                           EMEventTargetFolder *t)
{
	g_return_if_fail (t != NULL);

	if (!enabled || !t->new || (!t->is_inbox &&
		is_part_enabled (CONF_KEY_NOTIFY_ONLY_INBOX)))
		return;

	g_mutex_lock (&mlock);

	new_notify_dbus (t);

#ifdef HAVE_LIBNOTIFY
	if (is_part_enabled (CONF_KEY_ENABLED_STATUS))
		new_notify_status (t);
#endif

	if (is_part_enabled (CONF_KEY_ENABLED_SOUND))
		new_notify_sound (t);

	g_mutex_unlock (&mlock);
}

void
org_gnome_mail_read_notify (EPlugin *ep,
                            EMEventTargetMessage *t)
{
	g_return_if_fail (t != NULL);

	if (!enabled)
		return;

	g_mutex_lock (&mlock);

	read_notify_dbus (t);

#ifdef HAVE_LIBNOTIFY
	if (is_part_enabled (CONF_KEY_ENABLED_STATUS))
		read_notify_status (t);
#endif

	if (is_part_enabled (CONF_KEY_ENABLED_SOUND))
		read_notify_sound (t);

	g_mutex_unlock (&mlock);
}

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	if (enable) {
		enable_dbus (enable);

		if (is_part_enabled (CONF_KEY_ENABLED_SOUND))
			enable_sound (enable);

		enabled = TRUE;
	} else {
		enable_dbus (enable);
		enable_sound (enable);

		enabled = FALSE;
	}

	return 0;
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	return get_cfg_widget ();
}
