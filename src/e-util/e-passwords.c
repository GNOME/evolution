/*
 * e-passwords.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

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
 */

/*
 * This looks a lot more complicated than it is, and than you'd think
 * it would need to be.  There is however, method to the madness.
 *
 * The code must cope with being called from any thread at any time,
 * recursively from the main thread, and then serialising every
 * request so that sane and correct values are always returned, and
 * duplicate requests are never made.
 *
 * To this end, every call is marshalled and queued and a dispatch
 * method invoked until that request is satisfied.  If mainloop
 * recursion occurs, then the sub-call will necessarily return out of
 * order, but will not be processed out of order.
 */

#include "evolution-config.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include <libsoup/soup.h>

/* XXX Yeah, yeah... */
#define SECRET_API_SUBJECT_TO_CHANGE

#include <libsecret/secret.h>

#include <libedataserver/libedataserver.h>

#include "e-passwords.h"

#define d(x)

typedef struct _EPassMsg EPassMsg;

struct _EPassMsg {
	void (*dispatch) (EPassMsg *);
	EFlag *done;

	/* input */
	GtkWindow *parent;
	const gchar *key;
	const gchar *title;
	const gchar *prompt;
	const gchar *oldpass;
	guint32 flags;

	/* output */
	gboolean *remember;
	gchar *password;
	GError *error;

	/* work variables */
	GtkWidget *entry;
	GtkWidget *check;
	guint ismain : 1;
	guint noreply:1;	/* supress replies; when calling
				 * dispatch functions from others */
};

/* XXX probably want to share this with evalution-source-registry-migrate-sources.c */
static const SecretSchema e_passwords_schema = {
	"org.gnome.Evolution.Password",
	SECRET_SCHEMA_DONT_MATCH_NAME,
	{
		{ "application", SECRET_SCHEMA_ATTRIBUTE_STRING, },
		{ "user", SECRET_SCHEMA_ATTRIBUTE_STRING, },
		{ "server", SECRET_SCHEMA_ATTRIBUTE_STRING, },
		{ "protocol", SECRET_SCHEMA_ATTRIBUTE_STRING, },
	}
};

G_LOCK_DEFINE_STATIC (passwords);
static GThread *main_thread = NULL;
static GHashTable *password_cache = NULL;
static GtkDialog *password_dialog = NULL;
static GQueue message_queue = G_QUEUE_INIT;
static gint idle_id;
static gint ep_online_state = TRUE;

static GUri *
ep_keyring_uri_new (const gchar *string,
                    GError **error)
{
	GUri *uri;

	uri = g_uri_parse (string, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	/* LDAP URIs do not have usernames, so use the URI as the username. */
	if (!g_uri_get_user (uri) &&
	    (g_strcmp0 (g_uri_get_scheme (uri), "ldap") == 0 ||
	     g_strcmp0 (g_uri_get_scheme (uri), "google") == 0)) {
		gchar *user = g_strdelimit (g_strdup (string), "/=", '_');

		e_util_change_uri_component (&uri, SOUP_URI_USER, user);

		g_free (user);
	}

	/* Make sure the URI has the required components. */
	if (!g_uri_get_user (uri) && !g_uri_get_host (uri)) {
		g_set_error_literal (
			error, G_IO_ERROR,
			G_IO_ERROR_INVALID_ARGUMENT,
			_("Keyring key is unusable: no user or host name"));
		g_uri_unref (uri);
		uri = NULL;
	}

	return uri;
}

static gboolean
ep_idle_dispatch (gpointer data)
{
	EPassMsg *msg;

	/* As soon as a password window is up we stop; it will
	 * re - invoke us when it has been closed down */
	G_LOCK (passwords);
	while (password_dialog == NULL && (msg = g_queue_pop_head (&message_queue)) != NULL) {
		G_UNLOCK (passwords);

		msg->dispatch (msg);

		G_LOCK (passwords);
	}

	idle_id = 0;
	G_UNLOCK (passwords);

	return FALSE;
}

static EPassMsg *
ep_msg_new (void (*dispatch) (EPassMsg *))
{
	EPassMsg *msg;

	e_passwords_init ();

	msg = g_malloc0 (sizeof (*msg));
	msg->dispatch = dispatch;
	msg->done = e_flag_new ();
	msg->ismain = (g_thread_self () == main_thread);

	return msg;
}

static void
ep_msg_free (EPassMsg *msg)
{
	/* XXX We really should be passing this back to the caller, but
	 *     doing so will require breaking the password API. */
	if (msg->error != NULL) {
		g_warning ("%s", msg->error->message);
		g_error_free (msg->error);
	}

	e_flag_free (msg->done);
	g_free (msg->password);
	g_free (msg);
}

static void
ep_msg_send (EPassMsg *msg)
{
	gint needidle = 0;

	G_LOCK (passwords);
	g_queue_push_tail (&message_queue, msg);
	if (!idle_id) {
		if (!msg->ismain)
			idle_id = g_idle_add (ep_idle_dispatch, NULL);
		else
			needidle = 1;
	}
	G_UNLOCK (passwords);

	if (msg->ismain) {
		if (needidle)
			ep_idle_dispatch (NULL);
		while (!e_flag_is_set (msg->done))
			g_main_context_iteration (NULL, TRUE);
	} else
		e_flag_wait (msg->done);
}

/* the functions that actually do the work */

static void
ep_remember_password (EPassMsg *msg)
{
	gchar *password;
	GUri *uri;
	GError *error = NULL;

	password = g_hash_table_lookup (password_cache, msg->key);
	if (password == NULL) {
		g_warning ("Password for key \"%s\" not found", msg->key);
		goto exit;
	}

	uri = ep_keyring_uri_new (msg->key, &msg->error);
	if (uri == NULL)
		goto exit;

	secret_password_store_sync (
		&e_passwords_schema,
		SECRET_COLLECTION_DEFAULT,
		msg->key, password,
		NULL, &error,
		"application", "Evolution",
		"user", g_uri_get_user (uri),
		"server", g_uri_get_host (uri),
		"protocol", g_uri_get_scheme (uri),
		NULL);

	/* Only remove the password from the session hash
	 * if the keyring insertion was successful. */
	if (error == NULL)
		g_hash_table_remove (password_cache, msg->key);
	else
		g_propagate_error (&msg->error, error);

	g_uri_unref (uri);

exit:
	if (!msg->noreply)
		e_flag_set (msg->done);
}

static void
ep_forget_password (EPassMsg *msg)
{
	GUri *uri;
	GError *error = NULL;

	g_hash_table_remove (password_cache, msg->key);

	uri = ep_keyring_uri_new (msg->key, &msg->error);
	if (uri == NULL)
		goto exit;

	/* Find all Evolution passwords matching the URI and delete them.
	 *
	 * XXX We didn't always store protocols in the keyring, so for
	 *     backward-compatibility we need to lookup passwords by user
	 *     and host only (no protocol).  But we do send the protocol
	 *     to ep_keyring_delete_passwords(), which also knows about
	 *     the backward-compatibility issue and will filter the list
	 *     appropriately. */
	secret_password_clear_sync (
		&e_passwords_schema, NULL, &error,
		"application", "Evolution",
		"user", g_uri_get_user (uri),
		"server", g_uri_get_host (uri),
		NULL);

	if (error != NULL)
		g_propagate_error (&msg->error, error);

	g_uri_unref (uri);

exit:
	if (!msg->noreply)
		e_flag_set (msg->done);
}

static void
ep_get_password (EPassMsg *msg)
{
	GUri *uri;
	gchar *password;
	GError *error = NULL;

	/* Check the in-memory cache first. */
	password = g_hash_table_lookup (password_cache, msg->key);
	if (password != NULL) {
		msg->password = g_strdup (password);
		goto exit;
	}

	uri = ep_keyring_uri_new (msg->key, &msg->error);
	if (uri == NULL)
		goto exit;

	msg->password = secret_password_lookup_sync (
		&e_passwords_schema, NULL, &error,
		"application", "Evolution",
		"user", g_uri_get_user (uri),
		"server", g_uri_get_host (uri),
		"protocol", g_uri_get_scheme (uri),
		NULL);

	if (msg->password != NULL)
		goto done;

	/* Clear the previous error, if there was one.
	 * It's likely to occur again. */
	if (error != NULL)
		g_clear_error (&error);

	/* XXX We didn't always store protocols in the keyring, so for
	 *     backward-compatibility we also need to lookup passwords
	 *     by user and host only (no protocol). */
	msg->password = secret_password_lookup_sync (
		&e_passwords_schema, NULL, &error,
		"application", "Evolution",
		"user", g_uri_get_user (uri),
		"server", g_uri_get_host (uri),
		NULL);

done:
	if (error != NULL)
		g_propagate_error (&msg->error, error);

	g_uri_unref (uri);

exit:
	if (!msg->noreply)
		e_flag_set (msg->done);
}

static void
ep_add_password (EPassMsg *msg)
{
	g_hash_table_insert (
		password_cache, g_strdup (msg->key),
		g_strdup (msg->oldpass));

	if (!msg->noreply)
		e_flag_set (msg->done);
}

static void ep_ask_password (EPassMsg *msg);

static void
pass_response (GtkDialog *dialog,
               gint response,
               gpointer data)
{
	EPassMsg *msg = data;
	gint type = msg->flags & E_PASSWORDS_REMEMBER_MASK;
	GList *iter, *trash = NULL;

	if (response == GTK_RESPONSE_OK) {
		msg->password = g_strdup (gtk_entry_get_text ((GtkEntry *) msg->entry));

		if (type != E_PASSWORDS_REMEMBER_NEVER) {
			gint noreply = msg->noreply;

			*msg->remember = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (msg->check));

			msg->noreply = 1;

			if (*msg->remember || type == E_PASSWORDS_REMEMBER_FOREVER) {
				msg->oldpass = msg->password;
				ep_add_password (msg);
			}
			if (*msg->remember && type == E_PASSWORDS_REMEMBER_FOREVER)
				ep_remember_password (msg);

			msg->noreply = noreply;
		}
	}

	gtk_widget_destroy ((GtkWidget *) dialog);
	password_dialog = NULL;

	/* ok, here things get interesting, we suck up any pending
	 * operations on this specific password, and return the same
	 * result or ignore other operations */

	G_LOCK (passwords);
	for (iter = g_queue_peek_head_link (&message_queue); iter != NULL; iter = iter->next) {
		EPassMsg *pending = iter->data;

		if ((pending->dispatch == ep_forget_password
		     || pending->dispatch == ep_get_password
		     || pending->dispatch == ep_ask_password)
			&& strcmp (pending->key, msg->key) == 0) {

			/* Satisfy the pending operation. */
			pending->password = g_strdup (msg->password);
			e_flag_set (pending->done);

			/* Mark the queue node for deletion. */
			trash = g_list_prepend (trash, iter);
		}
	}

	/* Expunge the message queue. */
	for (iter = trash; iter != NULL; iter = iter->next)
		g_queue_delete_link (&message_queue, iter->data);
	g_list_free (trash);

	G_UNLOCK (passwords);

	if (!msg->noreply)
		e_flag_set (msg->done);

	ep_idle_dispatch (NULL);
}

static gboolean
update_capslock_state (GtkDialog *dialog,
                       GdkEvent *event,
                       GtkWidget *label)
{
	GdkModifierType mask = 0;
	GdkWindow *window;
	gchar *markup = NULL;
	GdkDeviceManager *device_manager;
	GdkDevice *device;

	device_manager = gdk_display_get_device_manager (gtk_widget_get_display (label));
	device = gdk_device_manager_get_client_pointer (device_manager);
	window = gtk_widget_get_window (GTK_WIDGET (dialog));
	gdk_window_get_device_position (window, device, NULL, NULL, &mask);

	/* The space acts as a vertical placeholder. */
	markup = g_markup_printf_escaped (
		"<small>%s</small>", (mask & GDK_LOCK_MASK) ?
		_("You have the Caps Lock key on.") : " ");
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);

	return FALSE;
}

static void
ep_ask_password (EPassMsg *msg)
{
	GtkWidget *widget;
	GtkWidget *container;
	GtkWidget *action_area;
	GtkWidget *content_area;
	GtkWindow *parent;
	gint type = msg->flags & E_PASSWORDS_REMEMBER_MASK;
	guint noreply = msg->noreply;
	gboolean visible;
	AtkObject *a11y;

	msg->noreply = 1;

	parent = msg->parent;
	if (!parent) {
		GApplication *app = g_application_get_default ();

		/* use the active window, even it can be a wrong one, but better
		   than using the default desktop, instead of the desktop where
		   the app window is */
		if (app && GTK_IS_APPLICATION (app))
			parent = gtk_application_get_active_window (GTK_APPLICATION (app));
	}

	widget = gtk_dialog_new_with_buttons (
		msg->title, parent, 0,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);
	gtk_dialog_set_default_response (
		GTK_DIALOG (widget), GTK_RESPONSE_OK);
	gtk_window_set_resizable (GTK_WINDOW (widget), FALSE);
	gtk_window_set_transient_for (GTK_WINDOW (widget), parent);
	gtk_window_set_position (GTK_WINDOW (widget), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
	password_dialog = GTK_DIALOG (widget);

	action_area = gtk_dialog_get_action_area (password_dialog);
	content_area = gtk_dialog_get_content_area (password_dialog);

	/* Override GtkDialog defaults */
	gtk_box_set_spacing (GTK_BOX (action_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 0);
	gtk_box_set_spacing (GTK_BOX (content_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

	/* Grid */
	container = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (container), 12);
	gtk_grid_set_row_spacing (GTK_GRID (container), 6);
	gtk_widget_show (container);

	gtk_box_pack_start (
		GTK_BOX (content_area), container, FALSE, TRUE, 0);

	/* Password Image */
	widget = gtk_image_new_from_icon_name (
		"dialog-password", GTK_ICON_SIZE_DIALOG);
	g_object_set (
		G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_widget_show (widget);

	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 3);

	/* Password Label */
	widget = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 20);
	gtk_label_set_markup (GTK_LABEL (widget), msg->prompt);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	g_object_set (
		G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);

	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);

	/* Password Entry */
	widget = gtk_entry_new ();
	a11y = gtk_widget_get_accessible (widget);
	visible = !(msg->flags & E_PASSWORDS_SECRET);
	atk_object_set_description (a11y, msg->prompt);
	gtk_entry_set_visibility (GTK_ENTRY (widget), visible);
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	gtk_widget_grab_focus (widget);
	g_object_set (
		G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);
	msg->entry = widget;

	if ((msg->flags & E_PASSWORDS_REPROMPT)) {
		ep_get_password (msg);
		if (msg->password != NULL) {
			gtk_entry_set_text (GTK_ENTRY (widget), msg->password);
			g_free (msg->password);
			msg->password = NULL;
		}
	}

	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);

	/* Caps Lock Label */
	widget = gtk_label_new (NULL);
	g_object_set (
		G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);

	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 1, 1);

	g_signal_connect (
		password_dialog, "key-release-event",
		G_CALLBACK (update_capslock_state), widget);
	g_signal_connect (
		password_dialog, "focus-in-event",
		G_CALLBACK (update_capslock_state), widget);

	/* static password, shouldn't be remembered between sessions,
	 * but will be remembered within the session beyond our control */
	if (type != E_PASSWORDS_REMEMBER_NEVER) {
		if (msg->flags & E_PASSWORDS_PASSPHRASE) {
			widget = gtk_check_button_new_with_mnemonic (
				(type == E_PASSWORDS_REMEMBER_FOREVER)
				? _("_Remember this passphrase")
				: _("_Remember this passphrase for"
				" the remainder of this session"));
		} else {
			widget = gtk_check_button_new_with_mnemonic (
				(type == E_PASSWORDS_REMEMBER_FOREVER)
				? _("_Remember this password")
				: _("_Remember this password for"
				" the remainder of this session"));
		}

		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (widget), *msg->remember);
		if (msg->flags & E_PASSWORDS_DISABLE_REMEMBER)
			gtk_widget_set_sensitive (widget, FALSE);
		g_object_set (
			G_OBJECT (widget),
			"hexpand", TRUE,
			"halign", GTK_ALIGN_FILL,
			"valign", GTK_ALIGN_FILL,
			NULL);
		gtk_widget_show (widget);
		msg->check = widget;

		gtk_grid_attach (GTK_GRID (container), widget, 1, 3, 1, 1);
	}

	msg->noreply = noreply;

	g_signal_connect (
		password_dialog, "response",
		G_CALLBACK (pass_response), msg);

	if (parent) {
		gtk_dialog_run (GTK_DIALOG (password_dialog));
	} else {
		gtk_window_present (GTK_WINDOW (password_dialog));
		/* workaround GTK+ bug (see Gnome's bugzilla bug #624229) */
		gtk_grab_add (GTK_WIDGET (password_dialog));
	}
}

/**
 * e_passwords_init:
 *
 * Initializes the e_passwords routines. Must be called before any other
 * e_passwords_* function.
 **/
void
e_passwords_init (void)
{
	G_LOCK (passwords);

	if (password_cache == NULL) {
		password_cache = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) g_free);
		main_thread = g_thread_self ();
	}

	G_UNLOCK (passwords);
}

/**
 * e_passwords_set_online:
 * @state:
 *
 * Set the offline-state of the application.  This is a work-around
 * for having the backends fully offline aware, and returns a
 * cancellation response instead of prompting for passwords.
 *
 * FIXME: This is not a permanent api, review post 2.0.
 **/
void
e_passwords_set_online (gint state)
{
	ep_online_state = state;
	/* TODO: we could check that a request is open and close it, or maybe who cares */
}

/**
 * e_passwords_remember_password:
 * @key: the key
 *
 * Saves the password associated with @key to disk.
 **/
void
e_passwords_remember_password (const gchar *key)
{
	EPassMsg *msg;

	g_return_if_fail (key != NULL);

	msg = ep_msg_new (ep_remember_password);
	msg->key = key;

	ep_msg_send (msg);
	ep_msg_free (msg);
}

/**
 * e_passwords_forget_password:
 * @key: the key
 *
 * Forgets the password associated with @key, in memory and on disk.
 **/
void
e_passwords_forget_password (const gchar *key)
{
	EPassMsg *msg;

	g_return_if_fail (key != NULL);

	msg = ep_msg_new (ep_forget_password);
	msg->key = key;

	ep_msg_send (msg);
	ep_msg_free (msg);
}

/**
 * e_passwords_get_password:
 * @key: the key
 *
 * Returns: the password associated with @key, or %NULL.  Caller
 * must free the returned password.
 **/
gchar *
e_passwords_get_password (const gchar *key)
{
	EPassMsg *msg;
	gchar *passwd;

	g_return_val_if_fail (key != NULL, NULL);

	msg = ep_msg_new (ep_get_password);
	msg->key = key;

	ep_msg_send (msg);

	passwd = msg->password;
	msg->password = NULL;
	ep_msg_free (msg);

	return passwd;
}

/**
 * e_passwords_add_password:
 * @key: a key
 * @passwd: the password for @key
 *
 * This stores the @key/@passwd pair in the current session's password
 * hash.
 **/
void
e_passwords_add_password (const gchar *key,
                          const gchar *passwd)
{
	EPassMsg *msg;

	g_return_if_fail (key != NULL);
	g_return_if_fail (passwd != NULL);

	msg = ep_msg_new (ep_add_password);
	msg->key = key;
	msg->oldpass = passwd;

	ep_msg_send (msg);
	ep_msg_free (msg);
}

/**
 * e_passwords_ask_password:
 * @title: title for the password dialog
 * @key: key to store the password under
 * @prompt: prompt string
 * @remember_type: whether or not to offer to remember the password,
 * and for how long.
 * @remember: on input, the default state of the remember checkbox.
 * on output, the state of the checkbox when the dialog was closed.
 * @parent: parent window of the dialog, or %NULL
 *
 * Asks the user for a password.
 *
 * Returns: the password, which the caller must free, or %NULL if
 * the user cancelled the operation. *@remember will be set if the
 * return value is non-%NULL and @remember_type is not
 * E_PASSWORDS_DO_NOT_REMEMBER.
 **/
gchar *
e_passwords_ask_password (const gchar *title,
                          const gchar *key,
                          const gchar *prompt,
                          EPasswordsRememberType remember_type,
                          gboolean *remember,
                          GtkWindow *parent)
{
	gchar *passwd;
	EPassMsg *msg;

	g_return_val_if_fail (key != NULL, NULL);

	if ((remember_type & E_PASSWORDS_ONLINE) && !ep_online_state)
		return NULL;

	msg = ep_msg_new (ep_ask_password);
	msg->title = title;
	msg->key = key;
	msg->prompt = prompt;
	msg->flags = remember_type;
	msg->remember = remember;
	msg->parent = parent;

	ep_msg_send (msg);
	passwd = msg->password;
	msg->password = NULL;
	ep_msg_free (msg);

	return passwd;
}
