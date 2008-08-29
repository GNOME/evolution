/*
 * 
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus.h>
#include "mail-dbus.h"
#include <camel/camel.h>
#include "camel-object-remote.h"
#include "camel-object-remote-impl.h"
#include "evo-dbus.h"

extern GHashTable *store_hash;
extern GHashTable *folder_hash;
extern CamelSession *session;

/* Session */
static void 
object_signal_cb (CamelObject *sess, gpointer ev_data, gpointer data)
{
	DBusError err;
	dbus_bool_t ret;
	dbus_error_init (&err);

	access ("before dbind context emit signal", 0);
	ret = dbind_context_emit_signal (e_dbus_peek_context(),
					 CAMEL_DBUS_NAME,
					 CAMEL_SESSION_OBJECT_PATH,
					 CAMEL_SESSION_INTERFACE,
					 "signal",
					 &err, "is", ev_data, data);
	access ("after dbind context emit signal", 0);

	if (!ret)
		g_warning ("error: %s\n", err.message);
}

DBusHandlerResult
camel_object_signal_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data,
				    CamelObjectRemoteImplType type)
{
	const char *method = dbus_message_get_member (message);
	DBusMessage *return_val;
	gboolean added = FALSE;
	CamelObject *object;

	printf("Handling session/co functions : %s\n", dbus_message_get_sender(message));
	return_val = dbus_message_new_method_return (message);

	if (strcmp(method, "camel_object_hook_event") == 0) {
		char *signal, *data, *object_hash;
		unsigned int ret_id;
		int ptr;

		dbus_message_get_args(message, NULL,
				DBUS_TYPE_STRING, &object_hash,
				DBUS_TYPE_STRING, &signal,
				DBUS_TYPE_INT32, &ptr,
				DBUS_TYPE_INVALID);
		data = g_strdup_printf ("%s:%s:%d", object_hash, signal, ptr);
		if (type == CAMEL_ROT_STORE)
			object = g_hash_table_lookup (store_hash, object_hash);
		else if (type == CAMEL_ROT_FOLDER)
				object = g_hash_table_lookup (folder_hash, object_hash);
		else 
			object = session;

		ret_id = camel_object_hook_event (object, signal, (CamelObjectEventHookFunc)object_signal_cb, data);
		dbus_message_append_args (return_val, DBUS_TYPE_UINT32, &ret_id, DBUS_TYPE_INVALID);
		added = TRUE;
	} else if (strcmp(method, "camel_object_unhook_event") == 0) {
		char *signal, *data, *object_hash;
		unsigned int ret_id;
		int ptr;

		dbus_message_get_args(message, NULL,
				DBUS_TYPE_STRING, &object_hash,
				DBUS_TYPE_STRING, &signal,
				DBUS_TYPE_INT32, &ptr,
				DBUS_TYPE_INVALID);
		data = g_strdup_printf ("%s:%s:%d", object_hash, signal, ptr);
		if (type == CAMEL_ROT_STORE)
			object = g_hash_table_lookup (store_hash, object_hash);
		else if (type == CAMEL_ROT_FOLDER)
				object = g_hash_table_lookup (folder_hash, object_hash);
		else 
			object = session;

		camel_object_unhook_event (object, signal, (CamelObjectEventHookFunc)object_signal_cb, data);
	} else if(strcmp (method, "camel_object_meta_set") == 0) {
		 char *name, *value, *object_hash;
		 gboolean ret;

		 dbus_message_get_args(message, NULL,
				DBUS_TYPE_STRING, &object_hash,
				DBUS_TYPE_STRING, &name,
				DBUS_TYPE_STRING, &value,
				DBUS_TYPE_INVALID);
		 if (type == CAMEL_ROT_STORE)
			  object = g_hash_table_lookup (store_hash, object_hash);
		 else if (type == CAMEL_ROT_FOLDER)
			  object = g_hash_table_lookup (folder_hash, object_hash);
		 else 
			  object = session;

		 ret = camel_object_meta_set (object, name, value);
		 dbus_message_append_args (return_val, DBUS_TYPE_INT32, &ret, DBUS_TYPE_INVALID);
		 added = TRUE;
	} else if(strcmp (method, "camel_object_state_write") == 0) {
		 char *object_hash;
		 int ret;

		 dbus_message_get_args(message, NULL,
				DBUS_TYPE_STRING, &object_hash,
				DBUS_TYPE_INVALID);
		 
		 if (type == CAMEL_ROT_STORE)
			  object = g_hash_table_lookup (store_hash, object_hash);
		 else if (type == CAMEL_ROT_FOLDER)
			  object = g_hash_table_lookup (folder_hash, object_hash);
		 else 
			  object = session;

		 ret = camel_object_state_write (object);
		 dbus_message_append_args (return_val, DBUS_TYPE_INT32, &ret, DBUS_TYPE_INVALID);
		 added = TRUE;
	} else if(strcmp (method, "camel_object_meta_get") == 0) {
		 char *name, *value, *object_hash;
		 gboolean ret;

		 dbus_message_get_args(message, NULL,
				DBUS_TYPE_STRING, &object_hash,
				DBUS_TYPE_STRING, &name,
				DBUS_TYPE_INVALID);
		 if (type == CAMEL_ROT_STORE)
			  object = g_hash_table_lookup (store_hash, object_hash);
		 else if (type == CAMEL_ROT_FOLDER)
			  object = g_hash_table_lookup (folder_hash, object_hash);
		 else 
			  object = session;

		 value = camel_object_meta_get (object, name);
		 if (!value)
			  value = "";
		 dbus_message_append_args (return_val, DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID);
		 added = TRUE;
	} else /* FIXME: Free memory and return */
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!added)
		dbus_message_append_args (return_val, DBUS_TYPE_INVALID);

	dbus_connection_send (connection, return_val, NULL);
	dbus_message_unref (return_val);
	dbus_connection_flush(connection);

	return DBUS_HANDLER_RESULT_HANDLED;

}

void
camel_object_remote_impl_init ()
{
	/* Later...  these comments no longer needed */
	//e_dbus_register_handler (CAMEL_SESSION_OBJECT_PATH, dbus_listener_session_handler, NULL);
	//e_dbus_register_handler (CAMEL_STORE_OBJECT_PATH, dbus_listener_store_handler, NULL);
	//e_dbus_register_handler (CAMEL_FOLDER_OBJECT_PATH, dbus_listener_folder_handler, NULL);
}

