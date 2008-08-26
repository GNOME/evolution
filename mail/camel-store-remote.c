/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <mail-dbus.h>
#include <evo-dbus.h>
#include <dbind.h>
#include <camel/camel-folder.h>
#include "camel-store-remote.h"
extern GHashTable *folder_hash;

#define CAMEL_DBUS_NAME "org.gnome.evolution.camel"
#define CAMEL_STORE_INTERFACE "org.gnome.evolution.camel.store"
#define CAMEL_STORE_OBJECT_PATH "/org/gnome/evolution/camel/store"

CamelFolder *camel_store_get_folder_remote(CamelStoreRemote * store,
					   const char *folder_name,
					   guint32 flags)
{
	DBusError error;
	char *err;
	char *shash;

	dbus_error_init(&error);
	/* Invoke the appropriate dbind call to CamelStoreGetFolder */
	dbind_context_method_call(evolution_dbus_peek_context(),
				  CAMEL_DBUS_NAME,
				  CAMEL_STORE_OBJECT_PATH,
				  CAMEL_STORE_INTERFACE,
				  "camel_store_get_folder",
				  &error,
				  "ssu=>ss", store->object_id, folder_name,
				  flags, &shash, &err);

	CamelFolder *folder = g_hash_table_lookup(folder_hash, shash);
	return folder;
}

static CamelFolder *camel_store_get_specific_folder_remote(CamelStoreRemote *
							   store,
							   const char *method)
{
	gboolean ret;
	DBusError error;
	char *err;
	char *fhash;

	dbus_error_init(&error);
	/* Invoke the appropriate dbind call to CamelStoreGetFolder */
	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					method,
					&error,
					"s=>ss", store->object_id, &fhash,
					&err);

	if (ret != DBUS_HANDLER_RESULT_HANDLED) {
		return NULL;
	} else {
		CamelFolder *folder = g_hash_table_lookup(folder_hash, fhash);
		return folder;
	}
}

CamelFolder *camel_store_get_inbox_remote(CamelStoreRemote * store)
{
	return (camel_store_get_specific_folder_remote
		(store, "camel_store_get_inbox"));
}

CamelFolder *camel_store_get_trash_remote(CamelStoreRemote * store)
{
	return (camel_store_get_specific_folder_remote
		(store, "camel_store_get_trash"));
}

CamelFolder *camel_store_get_junk_remote(CamelStoreRemote * store)
{
	return (camel_store_get_specific_folder_remote
		(store, "camel_store_get_junk"));
}

void
camel_store_delete_folder_remote(CamelStoreRemote * store,
				 const char *folder_name)
{
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_create_folder",
					&error,
					"ss=>s", store->object_id, folder_name,
					&err);
}

void
camel_store_rename_folder_remote(CamelStoreRemote * store,
				 const char *old_folder_name,
				 const char *new_folder_name)
{
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_rename_folder",
					&error,
					"sss=>s", store->object_id,
					old_folder_name, new_folder_name, &err);
}
