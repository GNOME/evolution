/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <glib.h>
#include <dbus/dbus.h>
#include "mail-dbus.h"
/* Lets load the actual session. */
#define MAIL_SESSION_REMOTE_H
#include "mail-session.h"

#define MAIL_SESSION_OBJECT_PATH "/org/gnome/evolution/camel/session/mail"

#define d(x) x

static gboolean ms_setup = FALSE;

static DBusHandlerResult
dbus_listener_message_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data);



static DBusHandlerResult
dbus_listener_message_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data)
{
	const char *method = dbus_message_get_member (message);
	DBusMessage *reply;
	gboolean added = FALSE;

	reply = dbus_message_new_method_return (message);

  	d(printf ("D-Bus message: obj_path = '%s' interface = '%s' method = '%s' destination = '%s'\n",
           dbus_message_get_path (message),
           dbus_message_get_interface (message),
           dbus_message_get_member (message),
           dbus_message_get_destination (message)));
	if (strcmp(method, "mail_session_init") == 0) {
		char *path;
		gboolean ret;

		ret = dbus_message_get_args(message, NULL, 
				DBUS_TYPE_STRING, &path,
				DBUS_TYPE_INVALID);
		d(printf("calling mail_session_init with: %s\n", path));
		mail_session_init (path);
	} else if (strcmp(method, "mail_session_get_interactive") == 0) {
		gboolean interactive = mail_session_get_interactive();
		dbus_message_append_args (reply, DBUS_TYPE_BOOLEAN, interactive, DBUS_TYPE_INVALID);
		d(printf("%s: %d\n", method, interactive));
		added = TRUE;
	} else if (strcmp(method, "mail_session_set_interactive") == 0) {
		gboolean interactive;
		gboolean ret;

		ret = dbus_message_get_args(message, NULL, 
				DBUS_TYPE_BOOLEAN, &interactive,
				DBUS_TYPE_INVALID);
		d(printf("mail_session_set_interactive: %d\n", interactive));
		mail_session_set_interactive (interactive);
	} else if (strcmp(method, "mail_session_get_password") == 0) {
		gboolean ret;
		char *key = NULL, *pass;

		ret = dbus_message_get_args(message, NULL, 
				DBUS_TYPE_STRING, &key,
				DBUS_TYPE_INVALID);
		d(printf("%s: %s\n", method, key));
		pass = mail_session_get_password (key);
		added = TRUE;
		dbus_message_append_args (reply, DBUS_TYPE_STRING, pass, DBUS_TYPE_INVALID);
	} else if (strcmp(method, "mail_session_add_password") == 0) {
		gboolean ret;
		char *key = NULL, *pass = NULL;

		ret = dbus_message_get_args(message, NULL, 
				DBUS_TYPE_STRING, &key,
				DBUS_TYPE_STRING, &pass,
				DBUS_TYPE_INVALID);
		d(printf("%s: %s\n", method, key));
		mail_session_add_password (key, pass);
	} else if (strcmp(method, "mail_session_remember password") == 0) {
		gboolean ret;
		char *key = NULL;

		ret = dbus_message_get_args(message, NULL, 
				DBUS_TYPE_STRING, &key,
				DBUS_TYPE_INVALID);
		d(printf("%s: %s\n", method, key));
		mail_session_remember_password (key);
	} else if (strcmp(method, "mail_session_forget_password") == 0) {
		gboolean ret;
		char *key = NULL;

		ret = dbus_message_get_args(message, NULL, 
				DBUS_TYPE_STRING, &key,
				DBUS_TYPE_INVALID);
		d(printf("%s: %s\n", method, key));
		mail_session_forget_password (key);
	} else if (strcmp(method, "mail_session_flush_filter_log") == 0) {
		gboolean ret;

		d(printf("%s\n", method));
		mail_session_flush_filter_log ();
	} else if (strcmp(method, "mail_session_shutdown") == 0) {
		gboolean ret;

		d(printf("%s\n", method));
		mail_session_shutdown ();
	} 


	if (!added)
		dbus_message_append_args(reply, DBUS_TYPE_INVALID);

	dbus_connection_send (connection, reply, NULL);
	dbus_message_unref (reply);
	dbus_connection_flush  (connection);
	d(printf("reply send\n"));

	return DBUS_HANDLER_RESULT_HANDLED;

}

void
mail_session_remote_impl_init ()
{
	ms_setup = TRUE;
	e_dbus_register_handler (MAIL_SESSION_OBJECT_PATH, dbus_listener_message_handler, NULL);
}


