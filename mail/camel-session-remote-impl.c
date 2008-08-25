/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <glib.h>
#include "mail-dbus.h"
#include <camel/camel-session.h>
#include <camel/camel-store.h>

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
	DBusMessage *return_val;

	CamelSession *session = NULL;
	CamelStore *store = NULL;

  	printf ("D-Bus message: obj_path = '%s' interface = '%s' method = '%s' destination = '%s'\n",
           dbus_message_get_path (message),
           dbus_message_get_interface (message),
           dbus_message_get_member (message),
           dbus_message_get_destination (message));
	
	
	return_val = dbus_message_new_method_return (message);

	if (strcmp(dbus_message_get_member (message), "camel_session_construct") == 0) {
		char *storage_path;
		char *session_str;
		gboolean ret;

		ret = dbus_message_get_args(message, NULL,
				DBUS_TYPE_STRING, &session_str,
				DBUS_TYPE_STRING, &storage_path,
				DBUS_TYPE_INVALID);
		if (!ret) {
			g_warning ("Unable to get args\n");
			return DBUS_HANDLER_RESULT_HANDLED;
		}

		camel_session_construct (session, storage_path);
		dbus_message_append_args (return_val, DBUS_TYPE_INVALID);
	
	} else if (strcmp(dbus_message_get_member (message), "camel_session_get_password") == 0) {
		char *session_str, *store_str, *domain, *prompt, *item, *err;
		const char *passwd;
		guint32 flags;
		gboolean ret;
		CamelException *ex;

		ret = dbus_message_get_args(message, NULL,
				DBUS_TYPE_STRING, &session_str,
				DBUS_TYPE_STRING, &store_str,
				DBUS_TYPE_STRING, &domain,
				DBUS_TYPE_STRING, &prompt,
				DBUS_TYPE_STRING, &item,
				DBUS_TYPE_UINT32, &flags,
				DBUS_TYPE_INVALID);

		camel_exception_init (ex);
		
		passwd = camel_session_get_password (session, store, domain, prompt, item, flags, ex);

		if (ex)
			err = g_strdup (camel_exception_get_description (ex));
		else
			err = g_strdup ("");
		camel_exception_free (ex);
			
		dbus_message_append_args (return_val, DBUS_TYPE_STRING, passwd, DBUS_TYPE_STRING, err, DBUS_TYPE_INVALID);
		g_free (err);
	}

	dbus_connection_send (connection, return_val, NULL);
	dbus_message_unref (return_val);
	dbus_connection_flush(connection);

	return DBUS_HANDLER_RESULT_HANDLED;

}

void
camel_session_remote_impl_init ()
{
	session_setup = TRUE;
	e_dbus_register_handler (CAMEL_SESSION_OBJECT_PATH, dbus_listener_message_handler, NULL);
}


