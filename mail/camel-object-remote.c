/*
 * 
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */


#include "camel-object-remote.h"
#include <dbus/dbus.h>
#include <dbind.h>
#include <evo-dbus.h>

#define d(x) x
extern CamelObjectRemote *rsession;

static char *obj_path[] = {CAMEL_SESSION_OBJECT_PATH, MAIL_SESSION_OBJECT_PATH, CAMEL_STORE_OBJECT_PATH, CAMEL_FOLDER_OBJECT_PATH};
static char *obj_if[] = {CAMEL_SESSION_INTERFACE, MAIL_SESSION_INTERFACE, CAMEL_STORE_INTERFACE, CAMEL_FOLDER_INTERFACE};
static int signal_inited = FALSE;
static GHashTable *objects;


extern GHashTable *store_rhash;
extern GHashTable *store_hash;

static DBusHandlerResult
dbus_listener_object_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data);


/* FIXME: needs to move into evo-dbus.[ch] */
static int 
register_handler (const char *object_path, DBusObjectPathMessageFunction reg, DBusObjectPathUnregisterFunction unreg)
{
	DBusObjectPathVTable *dbus_listener_vtable;
	DBindContext *ctx = evolution_dbus_peek_context ();
	DBusError err; int ret;
	char *rule;

	dbus_error_init (&err);
	dbus_listener_vtable = g_new0 (DBusObjectPathVTable, 1);
	dbus_listener_vtable->message_function = reg;
	dbus_listener_vtable->unregister_function = unreg;

	rule = g_strconcat ("type='signal',sender='", CAMEL_DBUS_NAME, "',path='", object_path, "'", NULL);
	d(printf("EVODBUS: add match '%s'\n", rule)); 
	dbus_bus_add_match (ctx->cnx, rule, NULL);
	g_free (rule);

	if (!dbus_connection_register_object_path (ctx->cnx,
						   object_path,
						   dbus_listener_vtable,
						   NULL)) {
		g_warning (("Out of memory registering object path '%s'"), object_path);
		return -1;
	}

	d(printf("EVODBUS: successfully inited signal handlers for %s\n", object_path));
	return 0;
}

unsigned int
camel_object_remote_hook_event (CamelObjectRemote *object, char *signal, CamelObjectEventHookFunc func, gpointer data)
{
	CamelHookRemote *hook;
	gboolean ret;
	DBusError error;
	char *hash;

	if (object == NULL) {
		object = rsession;
	}

	if (!signal_inited) {
		DBindContext *ctx = evolution_dbus_peek_context ();
		signal_inited = TRUE;
		objects = g_hash_table_new (g_str_hash, g_str_equal);
		register_handler (CAMEL_SESSION_OBJECT_PATH, dbus_listener_object_handler, NULL);
		register_handler (CAMEL_STORE_OBJECT_PATH, dbus_listener_object_handler, NULL);
		register_handler (CAMEL_FOLDER_OBJECT_PATH, dbus_listener_object_handler, NULL);
	}

	hook = g_new (CamelHookRemote, 1);
	hook->object_id = g_strdup (object->object_id);
	hook->signal = g_strdup (signal);
	hook->func = func;
	hook->data = data;
	hook->remote_id = 0;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			obj_path[object->type],
			obj_if[object->type],
			"camel_object_hook_event",
			&error, 
			"ssi=>u", object->object_id, signal, data, &hook->remote_id); /* Just string of base dir */

	if (!ret) {
		g_warning ("Error: Initializing mail session: %s\n", error.message);
		return 0;
	}
	
	/* We purposefully append */
	object->hooks = g_list_append (object->hooks, hook);
	if(!g_hash_table_lookup(objects, object->object_id))
		g_hash_table_insert (objects, object->object_id, object);

	return hook->remote_id;
}

gboolean
camel_object_remote_meta_set (CamelObjectRemote *object, char *name, char *value)
{
	 gboolean ret, ret_val;
	 DBusError error;

	 dbus_error_init (&error);
	 /* Invoke the appropriate dbind call to the camel object  */
	 ret = dbind_context_method_call (evolution_dbus_peek_context(), 
									  CAMEL_DBUS_NAME,
									  obj_path[object->type],
									  obj_if[object->type],
									  "camel_object_meta_set",
									  &error, 
									  "sss=>i", object->object_id, name, value, &ret_val);

	 if (!ret) {
		  g_warning ("Error: camel_object_remote_meta_set : %s\n", error.message);
		  return FALSE;
	 }


	 return ret_val;
}

int
camel_object_remote_state_write (CamelObjectRemote *object)
{
	 gboolean ret;
	 int ret_val;
	 DBusError error;

	 dbus_error_init (&error);
	 /* Invoke the appropriate dbind call to the camel object  */
	 ret = dbind_context_method_call (evolution_dbus_peek_context(), 
									  CAMEL_DBUS_NAME,
									  obj_path[object->type],
									  obj_if[object->type],
									  "camel_object_state_write",
									  &error, 
									  "s=>i", object->object_id, &ret_val);

	 if (!ret) {
		  g_warning ("Error: camel_object_remote_state_write : %s\n", error.message);
		  return -1;
	 }


	 return ret_val;
}

char *
camel_object_remote_meta_get (CamelObjectRemote *object, char *name)
{
	 gboolean ret;
	 DBusError error;
	 char *value;

	 dbus_error_init (&error);
	 /* Invoke the appropriate dbind call to the camel object  */
	 ret = dbind_context_method_call (evolution_dbus_peek_context(), 
									  CAMEL_DBUS_NAME,
									  obj_path[object->type],
									  obj_if[object->type],
									  "camel_object_meta_get",
									  &error, 
									  "ss=>s", object->object_id, name, &value);

	 if (!ret) {
		  g_warning ("Error: camel_object_remote_meta_get : %s\n", error.message);
		  return NULL;
	 }


	 return value;
}

static DBusHandlerResult
dbus_listener_object_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data)
{
	gpointer *ev_data;
	char *data;
	int ptr;
	char **tokens;
	CamelObjectRemote *object;

	printf ("EVOD-Bus message: obj_path = '%s' interface = '%s' method = '%s' destination = '%s'\n",
           dbus_message_get_path (message),
           dbus_message_get_interface (message),
           dbus_message_get_member (message),
           dbus_message_get_destination (message));

	if (strcmp(dbus_message_get_member (message), "signal") != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_get_args(message, NULL,
				DBUS_TYPE_INT32, &ptr,
				DBUS_TYPE_STRING, &data,
				DBUS_TYPE_INVALID);
	printf("dbus_listener_session_handler: %s %d\n", data, ptr);
	tokens = g_strsplit(data, ":", 0);
	object = g_hash_table_lookup(objects, tokens[0]);
	if (object) {
		GList *tmp = object->hooks;
		printf("Whoo, we have a object");
		while (tmp) {
			CamelHookRemote *hook = tmp->data;
			if (strcmp(hook->signal, tokens[1]) == 0) {
				printf("INVOKING CALLBACK: \n");
				hook->func (object, (gpointer)ptr, hook->data);
			}
			tmp = tmp->next;
		}
	}
	g_strfreev(tokens);
	{
		DBusMessage *return_val = dbus_message_new_method_return (message);
		dbus_message_append_args (return_val, DBUS_TYPE_INVALID);
		dbus_connection_send (connection, return_val, NULL);
		dbus_message_unref (return_val);
		dbus_connection_flush(connection);


	}
	return DBUS_HANDLER_RESULT_HANDLED;

}

CamelObjectRemote *
camel_object_remote_from_camel_store (CamelStore *store)
{
	CamelObjectRemote *obj;
	char *store_hash_key;

	store_hash_key = e_dbus_get_store_hash (camel_service_get_url((CamelService *)store));

	obj = (CamelObjectRemote *) g_hash_table_lookup (store_rhash, store_hash_key);
	return obj;
}


CamelStore *
camel_object_remote_get_camel_store (CamelObjectRemote *obj)
{
	CamelStore *store;

	if (obj->type != CAMEL_RO_STORE)
		return NULL;

	store = (CamelStore *) g_hash_table_lookup (store_hash, obj->object_id);
	return store;
}
