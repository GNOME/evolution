/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <glib.h>
#include <dbus/dbus.h>
#include "mail-dbus.h"
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
		if (!ret) {
			g_warning ("Unable to get args\n");
			return DBUS_HANDLER_RESULT_HANDLED;
		}

		d(printf("calling mail_session_init with: %s\n", path));
		mail_session_init (path);
	}

	dbus_message_append_args(reply, DBUS_TYPE_INVALID);
	dbus_connection_send (connection, reply, NULL);
	dbus_message_unref (reply);
	dbus_connection_flush  (connection);
	printf("reply send\n");
	return DBUS_HANDLER_RESULT_HANDLED;

}

void
mail_session_remote_impl_init ()
{
	ms_setup = TRUE;
	e_dbus_register_handler (MAIL_SESSION_OBJECT_PATH, dbus_listener_message_handler, NULL);
}


