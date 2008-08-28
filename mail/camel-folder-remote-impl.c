#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include "mail-dbus.h"
#include <camel/camel.h>
#include "camel-object-remote-impl.h"

#define CAMEL_FOLDER_OBJECT_PATH "/org/gnome/evolution/camel/folder"
#define CAMEL_FOLDER_INTERFACE "org.gnome.evolution.camel.folder"

extern GHashTable *store_hash;
extern GHashTable *folder_hash;

static DBusHandlerResult
dbus_listener_message_handler(DBusConnection * connection,
			      DBusMessage * message, void *user_data);

void camel_folder_remote_impl_init(void);

static DBusHandlerResult
dbus_listener_message_handler(DBusConnection * connection,
			      DBusMessage * message, void *user_data)
{
	const char *method = dbus_message_get_member (message);
	DBusMessage *return_val;
	char *folder_hash_key, *store_hash_key;
	CamelStore *store;
	CamelFolder *folder;	

	printf
	    ("D-Bus message: obj_path = '%s' interface = '%s' method = '%s' destination = '%s'\n",
	     dbus_message_get_path (message),
	     dbus_message_get_interface (message),
	     dbus_message_get_member (message),
	     dbus_message_get_destination (message));

	return_val = dbus_message_new_method_return (message);

	if (strcmp(method, "camel_folder_construct") == 0) {
		char *full_name, *name;
		gboolean ret;

		ret = dbus_message_get_args (message, NULL,
				DBUS_TYPE_STRING, &folder_hash_key,
				DBUS_TYPE_STRING, &store_hash_key,
				DBUS_TYPE_STRING, &full_name,
				DBUS_TYPE_STRING, &name,
				DBUS_TYPE_INVALID);
		store = g_hash_table_lookup (store_hash, store_hash_key);
		folder = g_hash_table_lookup (folder_hash, folder_hash_key);

		camel_folder_construct (folder, store, full_name, name);
		dbus_message_append_args (return_val, DBUS_TYPE_INVALID);
	} else if (strcmp(method, "camel_folder_thaw") == 0) {
		gboolean ret;

		ret = dbus_message_get_args (message, NULL,
				DBUS_TYPE_STRING, &folder_hash_key,
				DBUS_TYPE_INVALID);
		folder = g_hash_table_lookup (folder_hash, folder_hash_key);

		camel_folder_thaw (folder);
		dbus_message_append_args (return_val, DBUS_TYPE_INVALID);
	} else if (strncmp (method, "camel_object", 12) == 0) {
		return camel_object_folder_signal_handler (connection, message, user_data);
	} else
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void camel_folder_remote_impl_init()
{
	e_dbus_register_handler(CAMEL_FOLDER_OBJECT_PATH,
				dbus_listener_message_handler, NULL);
}
