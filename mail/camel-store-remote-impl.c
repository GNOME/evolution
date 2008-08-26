/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <glib.h>
#include <glib/gi18n.h>
#include "mail-dbus.h"
#include <camel/camel.h>

#define CAMEL_STORE_OBJECT_PATH "/org/gnome/evolution/camel/store"
#define CAMEL_STORE_INTERFACE "org.gnome.evolution.camel.store"

static gboolean store_setup = FALSE;

GHashTable *store_hash = NULL;
GHashTable *folder_hash = NULL;

static DBusHandlerResult
dbus_listener_message_handler(DBusConnection * connection,
			      DBusMessage * message, void *user_data);

void camel_store_remote_impl_init(void);
int camel_store_get_specific_folder_remote(DBusConnection * connection,
					   DBusMessage * message,
					   const char *method,
					   DBusMessage * reply);

int camel_store_get_specific_folder_remote(DBusConnection * connection,
					   DBusMessage * message,
					   const char *method,
					   DBusMessage * reply)
{
	CamelException *ex;
	CamelFolder *folder = NULL;
	CamelStore *store;
	char *err, *folder_hash_key = NULL, *store_hash_key;

	int ret = dbus_message_get_args(message,
					NULL,
					DBUS_TYPE_STRING, &store_hash_key,
					DBUS_TYPE_INVALID);

	if (ret)
		return ret;

	store = g_hash_table_lookup(store_hash, store_hash_key);
	if (!store) {
		dbus_message_append_args(reply, DBUS_TYPE_STRING, "",
					 DBUS_TYPE_STRING, _("Store not found"),
					 DBUS_TYPE_INVALID);
		return -1;
	}

	ex = camel_exception_new ();

	if (g_str_has_suffix(method, "inbox"))
		folder = camel_store_get_inbox(store, ex);
	else if (g_str_has_suffix(method, "trash"))
		folder = camel_store_get_trash(store, ex);
	else if (g_str_has_suffix(method, "junk"))
		folder = camel_store_get_junk(store, ex);

	if (!folder) {
		err = g_strdup(camel_exception_get_description(ex));
	} else {
		err = g_strdup("");
		/* FIXME: Free all */
		folder_hash_key =
		    e_dbus_get_folder_hash(camel_service_get_url
					   ((CamelService *) folder->
					    parent_store), folder->full_name);
		g_hash_table_insert(folder_hash, folder_hash_key, folder);
		g_free(folder_hash_key);
	}
	camel_exception_free(ex);

	dbus_message_append_args(reply, DBUS_TYPE_STRING, folder_hash_key,
				 DBUS_TYPE_STRING, err, DBUS_TYPE_INVALID);
	g_free(folder_hash_key);
	g_free(err);
	return 0;
}

static DBusHandlerResult
dbus_listener_message_handler(DBusConnection * connection,
			      DBusMessage * message, void *user_data)
{
	const char *method = dbus_message_get_member(message);
	DBusMessage *reply;

	printf
	    ("D-Bus message: obj_path = '%s' interface = '%s' method = '%s' destination = '%s'\n",
	     dbus_message_get_path(message),
	     dbus_message_get_interface(message),
	     dbus_message_get_member(message),
	     dbus_message_get_destination(message));

	reply = dbus_message_new_method_return(message);

	if (g_strcmp0(method, "camel_store_get_folder")) {
		guint32 flags;
		CamelException *ex;
		CamelFolder *folder;
		CamelStore *store;
		char *err, *folder_hash_key = NULL, *store_hash_key, *folder_name;

		int ret = dbus_message_get_args(message,
						NULL,
						DBUS_TYPE_STRING,
						&store_hash_key,
						DBUS_TYPE_STRING, &folder_name,
						DBUS_TYPE_UINT32, &flags,
						DBUS_TYPE_INVALID);

		if (ret)
			goto fail;

		store = g_hash_table_lookup(store_hash, store_hash_key);
		if (!store) {
			dbus_message_append_args(reply, DBUS_TYPE_STRING, "",
						 DBUS_TYPE_STRING,
						 _("Store not found"),
						 DBUS_TYPE_INVALID);
			goto fail;
		}

		ex = camel_exception_new ();
		folder = camel_store_get_folder(store, folder_name, flags, ex);
		if (!folder) {
			err = g_strdup(camel_exception_get_description(ex));
		} else {
			err = g_strdup("");
			/* FIXME: Free all */
			folder_hash_key =
			    e_dbus_get_folder_hash(camel_service_get_url
						   ((CamelService *) folder->
						    parent_store),
						   folder->full_name);
			g_hash_table_insert(folder_hash, folder_hash_key,
					    folder);
			g_free(folder_hash_key);
		}
		camel_exception_free(ex);

		dbus_message_append_args(reply, DBUS_TYPE_STRING,
					 folder_hash_key, DBUS_TYPE_STRING, err,
					 DBUS_TYPE_INVALID);
		g_free(folder_hash_key);
		g_free(err);

	} else if (g_strcmp0(method, "camel_store_get_inbox") ||
		   g_strcmp0(method, "camel_store_get_trash") ||
		   g_strcmp0(method, "camel_store_get_junk")) {
		camel_store_get_specific_folder_remote(connection, message,
						       method, reply);
	} else if (g_strcmp0(method, "camel_store_delete_folder")) {
		char *folder_name, *store_hash_key;
		CamelStore *store;
		CamelException *ex;
		int ret = dbus_message_get_args(message,
						NULL,
						DBUS_TYPE_STRING,
						&store_hash_key,
						DBUS_TYPE_STRING, &folder_name,
						DBUS_TYPE_INVALID);

		if (ret) {
			dbus_message_append_args(reply, DBUS_TYPE_STRING,
						 _("Arguments not parseable"),
						 DBUS_TYPE_INVALID);
			goto fail;
		}

		store = g_hash_table_lookup(store_hash, store_hash_key);
		if (!store) {
			dbus_message_append_args(reply, DBUS_TYPE_STRING,
						 _("Store not found"),
						 DBUS_TYPE_INVALID);
			goto fail;
		}

		camel_exception_init(ex);
		/* FIXME: camel_store_delete_folder should have sane return values and the exception should be used properly */
		camel_store_delete_folder(store, folder_name, ex);
		camel_exception_free(ex);
	} else if (g_strcmp0(method, "camel_store_rename_folder")) {
		char *old_folder_name, *new_folder_name, *store_hash_key;
		CamelStore *store;
		CamelException *ex;

		int ret = dbus_message_get_args(message,
						NULL,
						DBUS_TYPE_STRING,
						&store_hash_key,
						DBUS_TYPE_STRING,
						&old_folder_name,
						DBUS_TYPE_STRING,
						&new_folder_name,
						DBUS_TYPE_INVALID);

		if (ret) {
			dbus_message_append_args(reply, DBUS_TYPE_STRING,
						 _("Arguments not parseable"),
						 DBUS_TYPE_INVALID);
			goto fail;
		}

		store = g_hash_table_lookup(store_hash, store_hash_key);
		if (!store) {
			dbus_message_append_args(reply, DBUS_TYPE_STRING,
						 _("Store not found"),
						 DBUS_TYPE_INVALID);
			goto fail;
		}

		camel_exception_init(ex);
		/* FIXME: camel_store_delete_folder should have sane return values and the exception should be used properly */
		camel_store_rename_folder(store, old_folder_name,
					  new_folder_name, ex);
		camel_exception_free(ex);
	}
      fail:
	dbus_connection_send(connection, reply, NULL);
	dbus_message_unref(reply);

	return DBUS_HANDLER_RESULT_HANDLED;

}

void camel_store_remote_impl_init()
{
	store_setup = TRUE;
	store_hash = g_hash_table_new(g_str_hash, g_str_equal);
	folder_hash = g_hash_table_new(g_str_hash, g_str_equal);
	e_dbus_register_handler(CAMEL_STORE_OBJECT_PATH,
				dbus_listener_message_handler, NULL);
}
