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
 *		Miguel Angel Lopez Hernandez <miguel@gulev.org.mx>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <gio/gio.h>

#ifdef HAVE_CANBERRA
#include <canberra-gtk.h>
#endif

#include <time.h>

#include <e-util/e-binding.h>
#include <e-util/e-config.h>
#include <e-util/gconf-bridge.h>
#include <mail/em-utils.h>
#include <mail/em-event.h>
#include <mail/em-folder-tree.h>
#include <shell/e-shell-view.h>

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#define GCONF_KEY_ROOT			"/apps/evolution/eplugin/mail-notification/"
#define GCONF_KEY_NOTIFY_ONLY_INBOX	GCONF_KEY_ROOT "notify-only-inbox"
#define GCONF_KEY_ENABLED_DBUS		GCONF_KEY_ROOT "dbus-enabled"
#define GCONF_KEY_ENABLED_STATUS	GCONF_KEY_ROOT "status-enabled"
#define GCONF_KEY_ENABLED_SOUND		GCONF_KEY_ROOT "sound-enabled"

static gboolean enabled = FALSE;
static GtkWidget *get_cfg_widget (void);
static GStaticMutex mlock = G_STATIC_MUTEX_INIT;

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
 * It also should have its own gconf key for enabled state. In each particular
 * function it should do its work as expected. enable_... will be called always
 * when disabling plugin, but only when enabling/disabling part itself.
 **/

/* -------------------------------------------------------------------  */
/*                       Helper functions                               */
/* -------------------------------------------------------------------  */

static gboolean
is_part_enabled (const gchar *gconf_key)
{
	/* the part is enabled by default */
	gboolean res = TRUE;
	GConfClient *client;
	GConfValue  *is_key;

	client = gconf_client_get_default ();

	is_key = gconf_client_get (client, gconf_key, NULL);
	if (is_key) {
		res = gconf_client_get_bool (client, gconf_key, NULL);
		gconf_value_free (is_key);
	}

	g_object_unref (client);

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
                   const gchar *data,
                   guint new_count,
                   const gchar *msg_uid,
                   const gchar *msg_sender,
                   const gchar *msg_subject)
{
	GDBusMessage *message;
	GVariantBuilder *builder;
	GError *error = NULL;

	/* Create a new message on the DBUS_INTERFACE */
	if (!(message = g_dbus_message_new_signal (DBUS_PATH, DBUS_INTERFACE, name)))
		return;

	builder = g_variant_builder_new (G_VARIANT_TYPE_TUPLE);

	/* Appends the data as an argument to the message */
	g_variant_builder_add (builder, "(s)", data);

	if (new_count) {
		gchar *display_name = em_utils_folder_name_from_uri (data);

		g_variant_builder_add (builder, "(s)", display_name);
		g_variant_builder_add (builder, "(u)", new_count);

		g_free (display_name);
	}

	#define add_named_param(name, value)				\
		if (value) {						\
			gchar *val;					\
			val = g_strconcat (name, ":", value, NULL);	\
			g_variant_builder_add (builder, "(s)", val);	\
			g_free (val);					\
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

	if (error) {
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

	g_timeout_add (3000, reinit_gdbus, NULL);
}

static gboolean
init_gdbus (void)
{
	GError *error = NULL;

	if (connection != NULL)
		return TRUE;

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (error) {
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
			"Newmail", t->uri, t->new, t->msg_uid,
			t->msg_sender, t->msg_subject);
}

static void
read_notify_dbus (EMEventTargetMessage *t)
{
	if (connection != NULL)
		send_dbus_message (
			"MessageReading",
			camel_folder_get_name (t->folder),
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

#define GCONF_KEY_STATUS_NOTIFICATION	GCONF_KEY_ROOT "status-notification"

static GtkStatusIcon *status_icon = NULL;
static guint status_count = 0;

#ifdef HAVE_LIBNOTIFY
static NotifyNotification *notify = NULL;
#endif

static void
remove_notification (void)
{
#ifdef HAVE_LIBNOTIFY
	if (notify)
		notify_notification_close (notify, NULL);

	notify = NULL;
#endif

	gtk_status_icon_set_visible (status_icon, FALSE);
	g_object_unref (status_icon);

	status_icon = NULL;
	status_count = 0;
}

static void
status_icon_activate_cb (void)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree;
	GtkAction *action;
	GList *list;
	const gchar *uri;

	shell = e_shell_get_default ();
	list = e_shell_get_watched_windows (shell);

	/* Find the first EShellWindow in the list. */
	while (list != NULL && !E_IS_SHELL_WINDOW (list->data))
		list = g_list_next (list);

	g_return_if_fail (list != NULL);

	/* Present the shell window. */
	shell_window = E_SHELL_WINDOW (list->data);
	gtk_window_present (GTK_WINDOW (shell_window));

	/* Switch to the mail view. */
	shell_view = e_shell_window_get_shell_view (shell_window, "mail");
	action = e_shell_view_get_action (shell_view);
	gtk_action_activate (action);

	/* Select the latest folder with new mail. */
	uri = g_object_get_data (G_OBJECT (status_icon), "uri");
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
	em_folder_tree_set_selected (folder_tree, uri, FALSE);

	remove_notification ();
}

#ifdef HAVE_LIBNOTIFY
static gboolean
notification_callback (gpointer notify)
{
	return (!notify_notification_show (notify, NULL));
}
#endif

/* -------------------------------------------------------------------  */

static void
do_properties (GtkMenuItem *item, gpointer user_data)
{
	GtkWidget *cfg, *dialog, *vbox, *label, *hbox;
	GtkWidget *content_area;
	gchar *text;

	cfg = get_cfg_widget ();
	if (!cfg)
		return;

	text = g_markup_printf_escaped (
		"<span size=\"x-large\">%s</span>",
		_("Evolution's Mail Notification"));

	vbox = gtk_vbox_new (FALSE, 10);
	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_markup (GTK_LABEL (label), text);
	g_free (text);

	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	gtk_widget_show (vbox);

	hbox = gtk_hbox_new (FALSE, 10);
	label = gtk_label_new ("   ");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show_all (hbox);

	gtk_box_pack_start (GTK_BOX (hbox), cfg, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	dialog = gtk_dialog_new_with_buttons (
		_("Mail Notification Properties"),
		NULL,
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		NULL);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

#if !GTK_CHECK_VERSION(2,90,7)
	g_object_set (dialog, "has-separator", FALSE, NULL);
#endif
	gtk_container_add (GTK_CONTAINER (content_area), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
	gtk_widget_set_size_request (dialog, 400, -1);
	g_signal_connect_swapped (
		dialog, "response",
		G_CALLBACK (gtk_widget_destroy), dialog);
	gtk_widget_show (dialog);
}

static void
popup_menu_status (GtkStatusIcon *status_icon,
                   guint button,
                   guint activate_time,
                   gpointer user_data)
{
	GtkMenu *menu;
	GtkWidget *item;

	menu = GTK_MENU (gtk_menu_new ());

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLOSE, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	g_signal_connect (
		item, "activate",
		G_CALLBACK (remove_notification), NULL);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PROPERTIES, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	g_signal_connect (
		item, "activate",
		G_CALLBACK (do_properties), NULL);

	g_object_ref_sink (menu);
	gtk_menu_popup (menu, NULL, NULL, NULL, NULL, button, activate_time);
	g_object_unref (menu);
}

#ifdef HAVE_LIBNOTIFY
static void
notifyActionCallback (NotifyNotification *n, gchar *label, gpointer a)
{
	g_static_mutex_lock (&mlock);

	gtk_status_icon_set_visible (status_icon, FALSE);
	g_object_unref (status_icon);

	status_icon = NULL;
	status_count = 0;
	g_static_mutex_unlock (&mlock);
}

/* Function to check if actions are supported by the notification daemon */
static gboolean
can_support_actions (void)
{
	static gboolean supports_actions = FALSE;
	static gboolean have_checked = FALSE;

	if (!have_checked) {
		GList *caps = NULL;
		GList *c;

		have_checked = TRUE;

		caps = notify_get_server_caps ();
		if (caps != NULL) {
			for (c = caps; c != NULL; c = c->next) {
				if (strcmp ((gchar *)c->data, "actions") == 0) {
					supports_actions = TRUE;
					break;
				}
			}
		}

		g_list_foreach (caps, (GFunc)g_free, NULL);
		g_list_free (caps);
	}

	return supports_actions;
}
#endif

static void
new_notify_status (EMEventTargetFolder *t)
{
	gchar *msg;
	gboolean new_icon = !status_icon;

	if (new_icon) {
		status_icon = gtk_status_icon_new ();
		gtk_status_icon_set_from_icon_name (status_icon, "mail-unread");
	}

	g_object_set_data_full (
		G_OBJECT (status_icon), "uri",
		g_strdup (t->uri), (GDestroyNotify) g_free);

	if (!status_count) {
		EAccount *account;
		gchar *name = t->name;

		account = mail_config_get_account_by_source_url (t->uri);

		if (account != NULL) {
			name = g_strdup_printf (
				"%s/%s", e_account_get_string (
				account, E_ACCOUNT_NAME), name);
		}

		status_count = t->new;

		/* Translators: '%d' is the count of mails received
		 * and '%s' is the name of the folder*/
		msg = g_strdup_printf (ngettext (
			"You have received %d new message\nin %s.",
			"You have received %d new messages\nin %s.",
			status_count), status_count, name);

		if (name != t->name)
			g_free (name);

		if (t->msg_sender) {
			gchar *tmp, *str;

			/* Translators: "From:" is preceding a new mail
			 * sender address, like "From: user@example.com" */
			str = g_strdup_printf (_("From: %s"), t->msg_sender);
			tmp = g_strconcat (msg, "\n", str, NULL);

			g_free (msg);
			g_free (str);
			msg = tmp;
		}

		if (t->msg_subject) {
			gchar *tmp, *str;

			/* Translators: "Subject:" is preceding a new mail
			 * subject, like "Subject: It happened again" */
			str = g_strdup_printf (_("Subject: %s"), t->msg_subject);
			tmp = g_strconcat (msg, "\n", str, NULL);

			g_free (msg);
			g_free (str);
			msg = tmp;
		}
	} else {
		status_count += t->new;
		msg = g_strdup_printf (ngettext (
			"You have received %d new message.",
			"You have received %d new messages.",
			status_count), status_count);
	}

	gtk_status_icon_set_tooltip_text (status_icon, msg);

	gtk_status_icon_set_visible (status_icon, TRUE);

#ifdef HAVE_LIBNOTIFY
	/* Now check whether we're supposed to send notifications */
	if (is_part_enabled (GCONF_KEY_STATUS_NOTIFICATION)) {
		gchar *safetext;

		safetext = g_markup_escape_text(msg, strlen(msg));
		if (notify) {
			notify_notification_update (
				notify, _("New email"),
				safetext, "mail-unread");
		} else {
			if (!notify_init ("evolution-mail-notification"))
				fprintf (stderr,"notify init error");

			notify  = notify_notification_new (
				_("New email"), safetext,
				"mail-unread", NULL);
			notify_notification_attach_to_status_icon (
				notify, status_icon);

			/* Check if actions are supported */
			if (can_support_actions ()) {
				notify_notification_set_urgency (
					notify, NOTIFY_URGENCY_NORMAL);
				notify_notification_set_timeout (
					notify, NOTIFY_EXPIRES_DEFAULT);
				notify_notification_add_action (
					notify, "default", "Default",
					notifyActionCallback, NULL, NULL);
				g_timeout_add (
					500, notification_callback, notify);
			}
		}
		g_free(safetext);
	}
#endif

	g_free (msg);

	if (new_icon) {
		g_signal_connect (
			status_icon, "activate",
			G_CALLBACK (status_icon_activate_cb), NULL);

		g_signal_connect (
			status_icon, "popup-menu",
			G_CALLBACK (popup_menu_status), NULL);
	}
}

static void
read_notify_status (EMEventTargetMessage *t)
{
	if (!status_icon)
		return;

	remove_notification ();
}

static GtkWidget *
get_config_widget_status (void)
{
	GtkWidget *vbox;
	GtkWidget *master;
	GtkWidget *container;
	GtkWidget *widget;
	GConfBridge *bridge;
	const gchar *text;

	bridge = gconf_bridge_get ();

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	container = vbox;

	text = _("Show icon in _notification area");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_ENABLED_STATUS,
		G_OBJECT (widget), "active");

	master = widget;

	widget = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (widget), 0, 0, 12, 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_new (
		master, "active",
		widget, "sensitive");

	container = widget;

	widget = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

#ifdef HAVE_LIBNOTIFY
	text = _("Popup _message together with the icon");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_STATUS_NOTIFICATION,
		G_OBJECT (widget), "active");
#endif

	return vbox;
}

/* -------------------------------------------------------------------  */
/*                         Sound part                                   */
/* -------------------------------------------------------------------  */

/* min no. seconds between newmail notifications */
#define NOTIFY_THROTTLE 30

#define GCONF_KEY_SOUND_BEEP		GCONF_KEY_ROOT "sound-beep"
#define GCONF_KEY_SOUND_FILE		GCONF_KEY_ROOT "sound-file"
#define GCONF_KEY_SOUND_PLAY_FILE	GCONF_KEY_ROOT "sound-play-file"
#define GCONF_KEY_SOUND_USE_THEME       GCONF_KEY_ROOT "sound-use-theme"

#ifdef HAVE_CANBERRA
static ca_context *mailnotification = NULL;
#endif

static void
do_play_sound (gboolean beep, gboolean use_theme, const gchar *file)
{
	if (!beep) {
#ifdef HAVE_CANBERRA
		if (!use_theme && file && *file)
			ca_context_play(mailnotification, 0,
			CA_PROP_MEDIA_FILENAME, file,
			NULL);
		else
			ca_context_play(mailnotification, 0,
			CA_PROP_EVENT_ID,"message-new-email",
			NULL);
#endif
	}
	else
		gdk_beep();
}

struct _SoundConfigureWidgets
{
	GtkWidget *enable;
	GtkWidget *beep;
	GtkWidget *use_theme;
	GtkWidget *file;
	GtkWidget *filechooser;
	GtkWidget *play;
};

static void
sound_file_set_cb (GtkWidget *widget, gpointer data)
{
	gchar *file;
	GConfClient *client;

	g_return_if_fail (widget != NULL);

	client = gconf_client_get_default ();
	file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

	gconf_client_set_string (client, GCONF_KEY_SOUND_FILE, file ? file : "", NULL);

	g_object_unref (client);
	g_free (file);
}

static void
sound_play_cb (GtkWidget *widget, gpointer data)
{
	struct _SoundConfigureWidgets *scw = (struct _SoundConfigureWidgets *) data;
	gchar *file;

	g_return_if_fail (data != NULL);

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scw->enable)))
		return;

	file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (scw->filechooser));
	do_play_sound (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (scw->beep)),
		       gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (scw->use_theme)),
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
	GConfClient *client;
	struct _SoundNotifyData *data = (struct _SoundNotifyData *) user_data;

	g_return_val_if_fail (data != NULL, FALSE);

	client = gconf_client_get_default ();
	file = gconf_client_get_string (client, GCONF_KEY_SOUND_FILE, NULL);

	do_play_sound (is_part_enabled (GCONF_KEY_SOUND_BEEP),
		       is_part_enabled (GCONF_KEY_SOUND_USE_THEME),
		       file);

	g_object_unref (client);
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
		ca_context_create(&mailnotification);
		ca_context_change_props(
			mailnotification,
			CA_PROP_APPLICATION_NAME,
			"mailnotification Plugin",
			NULL);
	}
	else
		ca_context_destroy(mailnotification);
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
	GConfBridge *bridge;
	GConfClient *client;
	GSList *group = NULL;
	struct _SoundConfigureWidgets *scw;
	const gchar *text;

	bridge = gconf_bridge_get ();

	scw = g_malloc0 (sizeof (struct _SoundConfigureWidgets));

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	container = vbox;

	text = _("_Play sound when new messages arrive");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_ENABLED_SOUND,
		G_OBJECT (widget), "active");

	master = widget;
	scw->enable = widget;

	widget = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (widget), 0, 0, 12, 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_new (
		master, "active",
		widget, "sensitive");

	container = widget;

	widget = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	text = _("_Beep");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_SOUND_BEEP,
		G_OBJECT (widget), "active");

	scw->beep = widget;

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	text = _("Use sound _theme");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_SOUND_USE_THEME,
		G_OBJECT (widget), "active");

	scw->use_theme = widget;

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	widget = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Play _file:");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_SOUND_PLAY_FILE,
		G_OBJECT (widget), "active");

	scw->file = widget;

	text = _("Select sound file");
	widget = gtk_file_chooser_button_new (
		text, GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	scw->filechooser = widget;

	widget = gtk_button_new ();
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_icon_name (
		"media-playback-start", GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	scw->play = widget;

	client = gconf_client_get_default ();
	file = gconf_client_get_string (client, GCONF_KEY_SOUND_FILE, NULL);

	if (file && *file)
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (scw->filechooser), file);

	g_object_unref (client);
	g_free (file);

	g_signal_connect (
		scw->filechooser, "file-set",
		G_CALLBACK (sound_file_set_cb), scw);
	g_signal_connect (
		scw->play, "clicked",
		G_CALLBACK (sound_play_cb), scw);

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
	GConfBridge *bridge;
	const gchar *text;

	bridge = gconf_bridge_get ();

	widget = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (widget);

	container = widget;

	text = _("Notify new messages for _Inbox only");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_NOTIFY_ONLY_INBOX,
		G_OBJECT (widget), "active");

	text = _("Generate a _D-Bus message");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_ENABLED_DBUS,
		G_OBJECT (widget), "active");

	widget = get_config_widget_status ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	widget = get_config_widget_sound ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	return container;
}

void org_gnome_mail_new_notify (EPlugin *ep, EMEventTargetFolder *t);
void org_gnome_mail_read_notify (EPlugin *ep, EMEventTargetMessage *t);

gint e_plugin_lib_enable (EPlugin *ep, gint enable);
GtkWidget *e_plugin_lib_get_configure_widget (EPlugin *epl);

void
org_gnome_mail_new_notify (EPlugin *ep, EMEventTargetFolder *t)
{
	g_return_if_fail (t != NULL);

	if (!enabled || !t->new || (!t->is_inbox &&
		is_part_enabled (GCONF_KEY_NOTIFY_ONLY_INBOX)))
		return;

	g_static_mutex_lock (&mlock);

	if (is_part_enabled (GCONF_KEY_ENABLED_DBUS))
		new_notify_dbus (t);

	if (is_part_enabled (GCONF_KEY_ENABLED_STATUS))
		new_notify_status (t);

	if (is_part_enabled (GCONF_KEY_ENABLED_SOUND))
		new_notify_sound (t);

	g_static_mutex_unlock (&mlock);
}

void
org_gnome_mail_read_notify (EPlugin *ep, EMEventTargetMessage *t)
{
	g_return_if_fail (t != NULL);

	if (!enabled)
		return;

	g_static_mutex_lock (&mlock);

	if (is_part_enabled (GCONF_KEY_ENABLED_DBUS))
		read_notify_dbus (t);

	if (is_part_enabled (GCONF_KEY_ENABLED_STATUS))
		read_notify_status (t);

	if (is_part_enabled (GCONF_KEY_ENABLED_SOUND))
		read_notify_sound (t);

	g_static_mutex_unlock (&mlock);
}

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	if (enable) {
		if (is_part_enabled (GCONF_KEY_ENABLED_DBUS))
			enable_dbus (enable);

		if (is_part_enabled (GCONF_KEY_ENABLED_SOUND))
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
