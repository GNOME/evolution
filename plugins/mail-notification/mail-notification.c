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
#include <libgnome/gnome-sound.h>

#ifdef HAVE_DBUS
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#endif

#include <time.h>

#include "e-util/e-config.h"
#include "mail/em-utils.h"
#include "mail/em-event.h"
#include "mail/em-folder-tree-model.h"
#include "camel/camel-folder.h"

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
	/* the part is enabled by defaul */
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

static void
set_part_enabled (const gchar *gconf_key, gboolean enable)
{
	GConfClient *client;

	client = gconf_client_get_default ();

	gconf_client_set_bool (client, gconf_key, enable, NULL);
	g_object_unref (client);
}

/* -------------------------------------------------------------------  */
/*                           DBUS part                                  */
/* -------------------------------------------------------------------  */

#ifdef HAVE_DBUS

#define DBUS_PATH		"/org/gnome/evolution/mail/newmail"
#define DBUS_INTERFACE		"org.gnome.evolution.mail.dbus.Signal"

static DBusConnection *bus = NULL;

static gboolean init_dbus (void);

static void
send_dbus_message (const gchar *name, const gchar *data, guint new)
{
	DBusMessage *message;

	/* Create a new message on the DBUS_INTERFACE */
	if (!(message = dbus_message_new_signal (DBUS_PATH, DBUS_INTERFACE, name)))
		return;

	/* Appends the data as an argument to the message */
	dbus_message_append_args (message, DBUS_TYPE_STRING, &data, DBUS_TYPE_INVALID);

	if (new) {
		gchar * display_name = em_utils_folder_name_from_uri (data);
		dbus_message_append_args (message,
					  DBUS_TYPE_STRING, &display_name, DBUS_TYPE_UINT32, &new,
					  DBUS_TYPE_INVALID);
	}

	/* Sends the message */
	dbus_connection_send (bus, message, NULL);

	/* Frees the message */
	dbus_message_unref (message);
}

static gboolean
reinit_dbus (gpointer user_data)
{
	if (!enabled || init_dbus ())
		return FALSE;

	/* keep trying to re-establish dbus connection */
	return TRUE;
}

static DBusHandlerResult
filter_function (DBusConnection *connection, DBusMessage *message, gpointer user_data)
{
	if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected") &&
	    strcmp (dbus_message_get_path (message), DBUS_PATH_LOCAL) == 0) {
		dbus_connection_unref (bus);
		bus = NULL;

		g_timeout_add (3000, reinit_dbus, NULL);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static gboolean
init_dbus (void)
{
	DBusError error;

	if (bus != NULL)
		return TRUE;

	dbus_error_init (&error);
	if (!(bus = dbus_bus_get (DBUS_BUS_SESSION, &error))) {
		g_warning ("could not get system bus: %s\n", error.message);
		dbus_error_free (&error);
		return FALSE;
	}

	dbus_connection_setup_with_g_main (bus, NULL);
	dbus_connection_set_exit_on_disconnect (bus, FALSE);

	dbus_connection_add_filter (bus, filter_function, NULL, NULL);

	return TRUE;
}

static void
toggled_dbus_cb (GtkWidget *widget, gpointer data)
{
	g_return_if_fail (widget != NULL);

	set_part_enabled (GCONF_KEY_ENABLED_DBUS, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

/* -------------------------------------------------------------------  */

static void
new_notify_dbus (EMEventTargetFolder *t)
{
	if (bus != NULL)
		send_dbus_message ("Newmail", t->uri, t->new);
}

static void
read_notify_dbus (EMEventTargetMessage *t)
{
	if (bus != NULL)
		send_dbus_message ("MessageReading", t->folder->name, 0);
}

static void
enable_dbus (gint enable)
{
	if (enable) {
		/* we ignore errors here */
		init_dbus ();
	} else if (bus != NULL) {
		dbus_connection_unref (bus);
		bus = NULL;
	}
}

static GtkWidget *
get_config_widget_dbus (void)
{
	GtkWidget *w;

	w = gtk_check_button_new_with_mnemonic (_("Generate a _D-Bus message"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), is_part_enabled (GCONF_KEY_ENABLED_DBUS));
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (toggled_dbus_cb), NULL);
	gtk_widget_show (w);

	return w;
}

#endif

/* -------------------------------------------------------------------  */
/*                     Notification area part                           */
/* -------------------------------------------------------------------  */

#define GCONF_KEY_STATUS_BLINK		GCONF_KEY_ROOT "status-blink-icon"
#define GCONF_KEY_STATUS_NOTIFICATION	GCONF_KEY_ROOT "status-notification"

static GtkStatusIcon *status_icon = NULL;
static guint blink_timeout_id = 0;
static guint status_count = 0;

#ifdef HAVE_LIBNOTIFY
static gboolean notification_callback (gpointer notify);
static NotifyNotification *notify = NULL;
#endif

static void
#ifdef HAVE_LIBNOTIFY
icon_activated (GtkStatusIcon *icon, NotifyNotification *pnotify)
#else
icon_activated (GtkStatusIcon *icon, gpointer pnotify)
#endif
{
	#ifdef HAVE_LIBNOTIFY
	if (notify)
		notify_notification_close (notify, NULL);

	notify = NULL;
	#endif

	gtk_status_icon_set_visible (status_icon, FALSE);
	g_object_unref (status_icon);

	if (blink_timeout_id) {
		g_source_remove (blink_timeout_id);
		blink_timeout_id = 0;
	}

	status_icon = NULL;
	status_count = 0;
}

#ifdef HAVE_LIBNOTIFY
static gboolean
notification_callback (gpointer notify)
{
	return (!notify_notification_show (notify, NULL));
}
#endif

static gboolean
stop_blinking_cb (gpointer data)
{
	blink_timeout_id = 0;

	if (status_icon)
		gtk_status_icon_set_blinking (status_icon, FALSE);

	return FALSE;
}

struct _StatusConfigureWidgets
{
	GtkWidget *enable;
	GtkWidget *blink;
	#ifdef HAVE_LIBNOTIFY
	GtkWidget *message;
	#endif
};

static void
toggled_status_cb (GtkWidget *widget, gpointer data)
{
	struct _StatusConfigureWidgets *scw = (struct _StatusConfigureWidgets *) data;
	gboolean enabl;

	g_return_if_fail (scw != NULL);
	g_return_if_fail (scw->enable != NULL);
	g_return_if_fail (scw->blink != NULL);
	#ifdef HAVE_LIBNOTIFY
	g_return_if_fail (scw->message != NULL);
	#endif

	enabl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scw->enable));
	if (widget == scw->enable)
		set_part_enabled (GCONF_KEY_ENABLED_STATUS, enabl);

	#define work_widget(w, key) \
		gtk_widget_set_sensitive (w, enabl); \
		if (widget == w) \
			set_part_enabled (key, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)));

	work_widget (scw->blink, GCONF_KEY_STATUS_BLINK);
	#ifdef HAVE_LIBNOTIFY
	work_widget (scw->message, GCONF_KEY_STATUS_NOTIFICATION);
	#endif

	#undef work_widget
}

/* -------------------------------------------------------------------  */

static void
do_properties (GtkMenuItem *item, gpointer user_data)
{
	GtkWidget *cfg, *dialog, *vbox, *label, *hbox;
	gchar *text;

	cfg = get_cfg_widget ();
	if (!cfg)
		return;

	text = g_strconcat ("<span size=\"x-large\">", _("Evolution's Mail Notification"), "</span>", NULL);

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

	dialog = gtk_dialog_new_with_buttons (_("Mail Notification Properties"),
						NULL,
						GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
						NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
	gtk_widget_set_size_request (dialog, 400, -1);
	g_signal_connect_swapped (dialog, "response",  G_CALLBACK (gtk_widget_destroy), dialog);
	gtk_widget_show (dialog);
}

static void
popup_menu_status (GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer user_data)
{
	GtkMenu *menu;
	GtkWidget *item;

	menu = GTK_MENU (gtk_menu_new ());

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLOSE, NULL);
	#ifdef HAVE_LIBNOTIFY
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (icon_activated), notify);
	#else
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (icon_activated), NULL);
	#endif
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_separator_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PROPERTIES, NULL);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (do_properties), NULL);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

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

	if (blink_timeout_id) {
		g_source_remove (blink_timeout_id);
		blink_timeout_id = 0;
	}

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
	gchar *msg, *safetext;
	gboolean new_icon = !status_icon;

	if (new_icon) {
		status_icon = gtk_status_icon_new ();
		gtk_status_icon_set_from_icon_name (status_icon, "mail-unread");
	}

	if (!status_count) {
		status_count = t->new;
		/* To translators: '%d' is the number of mails recieved and '%s' is the name of the folder*/
		msg = g_strdup_printf (ngettext ("You have received %d new message\nin %s.",
						 "You have received %d new messages\nin %s.",
						 status_count),status_count, t->name);
	} else {
		status_count += t->new;
		msg = g_strdup_printf (ngettext ("You have received %d new message.",
						 "You have received %d new messages.", status_count), status_count);
	}

	gtk_status_icon_set_tooltip_text (status_icon, msg);

	if (new_icon && is_part_enabled (GCONF_KEY_STATUS_BLINK)) {
		gtk_status_icon_set_blinking (status_icon, TRUE);
		blink_timeout_id = g_timeout_add_seconds (15, stop_blinking_cb, NULL);
	}

	gtk_status_icon_set_visible (status_icon, TRUE);

	#ifdef HAVE_LIBNOTIFY
	/* Now check whether we're supposed to send notifications */
	if (is_part_enabled (GCONF_KEY_STATUS_NOTIFICATION)) {
		safetext = g_markup_escape_text(msg, strlen(msg));
		if (notify) {
			notify_notification_update (notify, _("New email"), safetext, "mail-unread");
		} else {
			if (!notify_init ("evolution-mail-notification"))
				fprintf (stderr,"notify init error");

			notify  = notify_notification_new (_("New email"), safetext, "mail-unread", NULL);
			notify_notification_attach_to_status_icon (notify, status_icon);

			/* Check if actions are supported */
			if (can_support_actions ()) {
				notify_notification_set_urgency (notify, NOTIFY_URGENCY_NORMAL);
				notify_notification_set_timeout (notify, NOTIFY_EXPIRES_DEFAULT);
				notify_notification_add_action(notify, "default", "Default", notifyActionCallback, NULL, NULL);
				g_timeout_add (500, notification_callback, notify);
			}
		}
		g_free(safetext);
	}
	#endif

	g_free (msg);

	if (new_icon) {
		#ifdef HAVE_LIBNOTIFY
		g_signal_connect (G_OBJECT (status_icon), "activate", G_CALLBACK (icon_activated), notify);
		#else
		g_signal_connect (G_OBJECT (status_icon), "activate", G_CALLBACK (icon_activated), NULL);
		#endif

		g_signal_connect (G_OBJECT (status_icon), "popup-menu", G_CALLBACK (popup_menu_status), NULL);
	}
}

static void
read_notify_status (EMEventTargetMessage *t)
{
	if (!status_icon)
		return;

	#ifdef HAVE_LIBNOTIFY
	icon_activated (status_icon, notify);
	#else
	icon_activated (status_icon, NULL);
	#endif
}

static void
enable_status (gint enable)
{
	/* this does nothing on enable, it's here just to be
	   consistent with other parts of this plugin */
}

static GtkWidget *
get_config_widget_status (void)
{
	GtkWidget *vbox, *parent, *alignment;
	struct _StatusConfigureWidgets *scw;

	vbox = gtk_vbox_new (FALSE, 0);
	scw = g_malloc0 (sizeof (struct _StatusConfigureWidgets));

	#define create_check(c, key, desc)							\
		c = gtk_check_button_new_with_mnemonic (desc);					\
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (c), is_part_enabled (key));	\
		gtk_box_pack_start (GTK_BOX (parent), c, FALSE, FALSE, 0);			\
		g_signal_connect (G_OBJECT (c), "toggled", G_CALLBACK (toggled_status_cb), scw);

	parent = vbox;
	create_check (scw->enable,     GCONF_KEY_ENABLED_STATUS,      _("Show icon in _notification area"));

	parent = gtk_vbox_new (FALSE, 0);
	create_check (scw->blink,      GCONF_KEY_STATUS_BLINK,        _("B_link icon in notification area"));
	#ifdef HAVE_LIBNOTIFY
	create_check (scw->message,    GCONF_KEY_STATUS_NOTIFICATION, _("Popup _message together with the icon"));
	#endif

	alignment = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 20, 0);
	gtk_container_add (GTK_CONTAINER (alignment), parent);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	#undef create_check

	/* to let structure free properly */
	g_object_set_data_full (G_OBJECT (vbox), "scw-data", scw, g_free);

	toggled_status_cb (NULL, scw);
	gtk_widget_show_all (vbox);

	return vbox;
}

/* -------------------------------------------------------------------  */
/*                         Sound part                                   */
/* -------------------------------------------------------------------  */

/* min no. seconds between newmail notifications */
#define NOTIFY_THROTTLE 30

#define GCONF_KEY_SOUND_BEEP		GCONF_KEY_ROOT "sound-beep"
#define GCONF_KEY_SOUND_FILE		GCONF_KEY_ROOT "sound-file"

static void
do_play_sound (gboolean beep, const gchar *file)
{
	if (beep)
		gdk_beep ();
	else if (!file || !*file)
		g_warning ("No file to play!");
	else
		gnome_sound_play (file);
}

struct _SoundConfigureWidgets
{
	GtkWidget *enable;
	GtkWidget *beep;
	GtkWidget *file;
	GtkWidget *label;
	GtkWidget *filechooser;
	GtkWidget *play;
};

static void
toggled_sound_cb (GtkWidget *widget, gpointer data)
{
	struct _SoundConfigureWidgets *scw = (struct _SoundConfigureWidgets *) data;
	gboolean enabl;

	g_return_if_fail (data != NULL);
	g_return_if_fail (scw->enable != NULL);
	g_return_if_fail (scw->beep != NULL);
	g_return_if_fail (scw->file != NULL);
	g_return_if_fail (scw->label != NULL);
	g_return_if_fail (scw->filechooser != NULL);
	g_return_if_fail (scw->play != NULL);

	enabl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scw->enable));
	if (widget == scw->enable)
		set_part_enabled (GCONF_KEY_ENABLED_SOUND, enabl);

	gtk_widget_set_sensitive (scw->beep, enabl);
	gtk_widget_set_sensitive (scw->file, enabl);
	gtk_widget_set_sensitive (scw->label, enabl);
	gtk_widget_set_sensitive (scw->filechooser, enabl && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scw->file)));
	gtk_widget_set_sensitive (scw->play, enabl);

	if (widget == scw->beep || widget == scw->file)
		set_part_enabled (GCONF_KEY_SOUND_BEEP, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scw->beep)));
}

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
	do_play_sound (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scw->beep)), file);
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

	do_play_sound (is_part_enabled (GCONF_KEY_SOUND_BEEP), file);

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
	if (data.notify_idle_id == 0 && (last_newmail - data.last_notify >= NOTIFY_THROTTLE))
		data.notify_idle_id = g_idle_add_full (G_PRIORITY_LOW, sound_notify_idle_cb, &data, NULL);
}

static void
read_notify_sound (EMEventTargetMessage *t)
{
	/* we do nothing here */
}

static void
enable_sound (gint enable)
{
	if (enable)
		gnome_sound_init ("localhost");
	else
		gnome_sound_shutdown ();
}

static GtkWidget *
get_config_widget_sound (void)
{
	GtkWidget *vbox, *alignment, *parent, *hbox;
	gchar *file;
	GConfClient *client;
	struct _SoundConfigureWidgets *scw;

	vbox = gtk_vbox_new (FALSE, 0);
	scw = g_malloc0 (sizeof (struct _SoundConfigureWidgets));

	scw->enable = gtk_check_button_new_with_mnemonic (_("_Play sound when new messages arrive"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scw->enable), is_part_enabled (GCONF_KEY_ENABLED_SOUND));
	gtk_box_pack_start (GTK_BOX (vbox), scw->enable, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (scw->enable), "toggled", G_CALLBACK (toggled_sound_cb), scw);

	parent = gtk_vbox_new (FALSE, 0);
	scw->beep = gtk_radio_button_new_with_mnemonic (NULL, _("_Beep"));
	scw->file = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (scw->beep), _("Play _sound file"));

	if (is_part_enabled (GCONF_KEY_SOUND_BEEP))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scw->beep), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scw->file), TRUE);

	g_signal_connect (G_OBJECT (scw->beep), "toggled", G_CALLBACK (toggled_sound_cb), scw);
	g_signal_connect (G_OBJECT (scw->file), "toggled", G_CALLBACK (toggled_sound_cb), scw);

	hbox = gtk_hbox_new (FALSE, 0);
	scw->label = gtk_label_new_with_mnemonic (_("Specify _filename:"));
	scw->filechooser = gtk_file_chooser_button_new (_("Select sound file"), GTK_FILE_CHOOSER_ACTION_OPEN);
	scw->play = gtk_button_new_with_mnemonic (_("Pl_ay"));

	gtk_label_set_mnemonic_widget (GTK_LABEL (scw->label), scw->filechooser);
	gtk_button_set_image (GTK_BUTTON (scw->play), gtk_image_new_from_icon_name ("media-playback-start", GTK_ICON_SIZE_BUTTON));

	client = gconf_client_get_default ();
	file = gconf_client_get_string (client, GCONF_KEY_SOUND_FILE, NULL);

	if (file && *file)
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (scw->filechooser), file);

	g_object_unref (client);
	g_free (file);

	g_signal_connect (G_OBJECT (scw->filechooser), "file-set", G_CALLBACK (sound_file_set_cb), scw);
	g_signal_connect (G_OBJECT (scw->play), "clicked", G_CALLBACK (sound_play_cb), scw);

	gtk_box_pack_start (GTK_BOX (hbox), scw->label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), scw->filechooser, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), scw->play, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (parent), scw->beep, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (parent), scw->file, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (parent), hbox, FALSE, FALSE, 0);

	alignment = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 20, 0);
	gtk_container_add (GTK_CONTAINER (alignment), parent);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	/* to let structure free properly */
	g_object_set_data_full (G_OBJECT (vbox), "scw-data", scw, g_free);

	toggled_sound_cb (NULL, scw);
	gtk_widget_show_all (vbox);

	return vbox;
}

/* -------------------------------------------------------------------  */
/*                     Plugin itself part                               */
/* -------------------------------------------------------------------  */

static void
toggled_only_inbox_cb (GtkWidget *widget, gpointer data)
{
	g_return_if_fail (widget != NULL);

	set_part_enabled (GCONF_KEY_NOTIFY_ONLY_INBOX, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static GtkWidget *
get_cfg_widget (void)
{
	GtkWidget *cfg, *vbox, *check;

	vbox = gtk_vbox_new (FALSE, 6);
	check = gtk_check_button_new_with_mnemonic (_("Notify new messages for _Inbox only"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), is_part_enabled (GCONF_KEY_NOTIFY_ONLY_INBOX));
	g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (toggled_only_inbox_cb), NULL);
	gtk_widget_show (check);
	gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, FALSE, 0);

#ifdef HAVE_DBUS
	cfg = get_config_widget_dbus ();
	if (cfg)
		gtk_box_pack_start (GTK_BOX (vbox), cfg, FALSE, FALSE, 0);
#endif
	cfg = get_config_widget_status ();
	if (cfg)
		gtk_box_pack_start (GTK_BOX (vbox), cfg, FALSE, FALSE, 0);

	cfg = get_config_widget_sound ();
	if (cfg)
		gtk_box_pack_start (GTK_BOX (vbox), cfg, FALSE, FALSE, 0);

	gtk_widget_show (vbox);

	return vbox;
}

void org_gnome_mail_new_notify (EPlugin *ep, EMEventTargetFolder *t);
void org_gnome_mail_read_notify (EPlugin *ep, EMEventTargetMessage *t);

gint e_plugin_lib_enable (EPluginLib *ep, gint enable);
GtkWidget *e_plugin_lib_get_configure_widget (EPlugin *epl);

void
org_gnome_mail_new_notify (EPlugin *ep, EMEventTargetFolder *t)
{
	g_return_if_fail (t != NULL);

	if (!enabled || !t->new || (!t->is_inbox && is_part_enabled (GCONF_KEY_NOTIFY_ONLY_INBOX)))
		return;

	g_static_mutex_lock (&mlock);

#ifdef HAVE_DBUS
	if (is_part_enabled (GCONF_KEY_ENABLED_DBUS))
		new_notify_dbus (t);
#endif
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

#ifdef HAVE_DBUS
	if (is_part_enabled (GCONF_KEY_ENABLED_DBUS))
		read_notify_dbus (t);
#endif
	if (is_part_enabled (GCONF_KEY_ENABLED_STATUS))
		read_notify_status (t);

	if (is_part_enabled (GCONF_KEY_ENABLED_SOUND))
		read_notify_sound (t);

	g_static_mutex_unlock (&mlock);
}

gint
e_plugin_lib_enable (EPluginLib *ep, gint enable)
{
	if (enable) {
#ifdef HAVE_DBUS
		if (is_part_enabled (GCONF_KEY_ENABLED_DBUS))
			enable_dbus (enable);
#endif
		if (is_part_enabled (GCONF_KEY_ENABLED_STATUS))
			enable_status (enable);

		if (is_part_enabled (GCONF_KEY_ENABLED_SOUND))
			enable_sound (enable);

		enabled = TRUE;
	} else {
#ifdef HAVE_DBUS
		enable_dbus (enable);
#endif
		enable_status (enable);
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
