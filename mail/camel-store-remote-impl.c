/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <glib.h>
#include "mail-dbus.h"
#include <camel/camel.h>

#define CAMEL_STORE_OBJECT_PATH "/org/gnome/evolution/camel/store"
#define CAMEL_STORE_INTERFACE "org.gnome.evolution.camel.store"

static gboolean store_setup = FALSE;

GHashTable *store_hash = NULL;
GHashTable *folder_hash = NULL;

static DBusHandlerResult
dbus_listener_message_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data);

static DBusHandlerResult
dbus_listener_message_handler (DBusConnection *connection, DBusMessage *message, void *user_data)
{
		const char *method = dbus_message_get_member (message);
		DBusMessage *reply;

		printf ("D-Bus message: obj_path = '%s' interface = '%s' method = '%s' destination = '%s'\n",
						dbus_message_get_path (message),
						dbus_message_get_interface (message),
						dbus_message_get_member (message),
						dbus_message_get_destination (message));

		reply = dbus_message_new_method_return (message);

		if (strcmp (method, "camel_store_get_folder")) {
			guint32 flags;
			CamelException *ex;
			CamelFolder *folder;
			CamelStore *store;
			char *err, *folder_hash_key, *store_hash_key, *folder_name;

			int ret = dbus_message_get_args (message, 
					NULL,
					DBUS_TYPE_STRING, &store_hash_key,
					DBUS_TYPE_STRING, &folder_name,
					DBUS_TYPE_UINT32, &flags,
					DBUS_TYPE_INVALID);

			store = g_hash_table_lookup (store_hash, store_hash_key);
			if (!store) {
				dbus_message_append_args (reply, DBUS_TYPE_STRING, "", DBUS_TYPE_STRING, _("Store not found"), DBUS_TYPE_INVALID);
				goto fail;
			}

			camel_exception_init (ex);
			folder = camel_store_get_folder (store, folder_name, flags, ex);
			if (!folder) {
				err = g_strdup (camel_exception_get_description (ex));
			} else {
				err = g_strdup ("");
				/* FIXME: Free all */
				folder_hash_key = e_dbus_get_folder_hash (camel_service_get_url((CamelService *)folder->parent_store), folder->full_name); 
				g_hash_table_insert (folder_hash, folder_hash_key, folder);
				g_free (folder_hash_key);
			}
			camel_exception_free (ex);

			dbus_message_append_args (reply, DBUS_TYPE_STRING, folder_hash_key, DBUS_TYPE_STRING, err, DBUS_TYPE_INVALID);
			g_free (folder_hash_key);
			g_free (err);

		}
fail:
	dbus_connection_send (connection, reply, NULL);
	dbus_message_unref (reply);

	return DBUS_HANDLER_RESULT_HANDLED;


}

void
camel_store_remote_impl_init ()
{
	store_setup = TRUE;
	store_hash = g_hash_table_new (g_str_hash, g_str_equal);
	folder_hash = g_hash_table_new (g_str_hash, g_str_equal);
	e_dbus_register_handler (CAMEL_STORE_OBJECT_PATH, dbus_listener_message_handler, NULL);
}


