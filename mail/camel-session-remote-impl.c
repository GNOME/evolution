/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <glib.h>
#include "mail-dbus.h"

#define CAMEL_SESSION_OBJECT_PATH "/org/gnome/evolution/camel/session"

static gboolean session_setup = FALSE;

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

  	printf ("D-Bus message: obj_path = '%s' interface = '%s' method = '%s' destination = '%s'\n",
           dbus_message_get_path (message),
           dbus_message_get_interface (message),
           dbus_message_get_member (message),
           dbus_message_get_destination (message));

	return DBUS_HANDLER_RESULT_HANDLED;

}

void
camel_session_remote_impl_init ()
{
	session_setup = TRUE;
	e_dbus_register_handler (CAMEL_SESSION_OBJECT_PATH, dbus_listener_message_handler, NULL);
}


