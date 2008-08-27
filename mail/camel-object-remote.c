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

static char *obj_path[] = {CAMEL_SESSION_OBJECT_PATH, MAIL_SESSION_OBJECT_PATH, CAMEL_STORE_OBJECT_PATH, CAMEL_FOLDER_OBJECT_PATH};
static char *obj_if[] = {CAMEL_SESSION_INTERFACE, MAIL_SESSION_INTERFACE, CAMEL_STORE_INTERFACE, CAMEL_FOLDER_INTERFACE};
static int signal_inited = FALSE;
static GHashTable *objects;


extern GHashTable *store_rhash;
extern GHashTable *store_hash;

static DBusHandlerResult
dbus_listener_store_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data);
static DBusHandlerResult
dbus_listener_session_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data);

static DBusHandlerResult
dbus_listener_folder_handler (DBusConnection *connection,
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

	rule = g_strconcat ("type='signal',path='", object_path, "'", NULL);
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
	CamelObjectRemote obj = {"session", CAMEL_RO_SESSION, NULL};


	if (object == NULL) {
		object = &obj;
	}

	if (!signal_inited) {
		DBindContext *ctx = evolution_dbus_peek_context ();
		signal_inited = TRUE;
		objects = g_hash_table_new (g_str_hash, g_str_equal);
		register_handler (CAMEL_SESSION_OBJECT_PATH, dbus_listener_session_handler, NULL);
		register_handler (CAMEL_STORE_OBJECT_PATH, dbus_listener_store_handler, NULL);
		register_handler (CAMEL_FOLDER_OBJECT_PATH, dbus_listener_folder_handler, NULL);
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
	d(printf("Mail session initated remotely\n"));
	if(!g_hash_table_lookup(objects, object->object_id))
		g_hash_table_insert (objects, object->object_id, object);

	return hook->remote_id;
}


static DBusHandlerResult
dbus_listener_store_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data)
{
  	printf ("D-Bus message: obj_path = '%s' interface = '%s' method = '%s' destination = '%s'\n",
           dbus_message_get_path (message),
           dbus_message_get_interface (message),
           dbus_message_get_member (message),
           dbus_message_get_destination (message));

	return DBUS_HANDLER_RESULT_HANDLED;
}
static DBusHandlerResult
dbus_listener_session_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data)
{
	printf ("EVOD-Bus message: obj_path = '%s' interface = '%s' method = '%s' destination = '%s'\n",
           dbus_message_get_path (message),
           dbus_message_get_interface (message),
           dbus_message_get_member (message),
           dbus_message_get_destination (message));

	return DBUS_HANDLER_RESULT_HANDLED;

}
static DBusHandlerResult
dbus_listener_folder_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data)
{
	printf ("D-Bus message: obj_path = '%s' interface = '%s' method = '%s' destination = '%s'\n",
           dbus_message_get_path (message),
           dbus_message_get_interface (message),
           dbus_message_get_member (message),
           dbus_message_get_destination (message));

	return DBUS_HANDLER_RESULT_HANDLED;
}

CamelObjectRemote *
camel_object_remote_from_camel_store (CamelStore *store)
{
	CamelObjectRemote *obj;

	obj = (CamelObjectRemote *) g_hash_table_lookup (store_rhash, camel_service_get_url((CamelService *)store));
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
