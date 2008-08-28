#include <mail-dbus.h>
#include <evo-dbus.h>
#include <dbind.h>
#include <camel/camel-folder.h>
#include "camel-folder-remote.h"


#define d(x) x

extern GHashTable *folder_hash;
extern GHashTable *store_rhash;

void
camel_folder_remote_construct (CamelFolderRemote *folder,
			CamelStoreRemote *parent_store,
			const char *full_name,
			const char *name)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_construct",
			&error, 
			"ssss", folder->object_id, parent_store->object_id, full_name, name); 

	if (!ret) {
		g_warning ("Error: Constructing camel folder: %s\n", error.message);
		return;
	}

	d(printf("Camel folder constructed remotely\n"));
}

void 
camel_folder_remote_thaw (CamelFolderRemote *folder)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_thaw",
			&error, 
			"s", folder->object_id); 

	if (!ret) {
		g_warning ("Error: Camel folder thaw: %s\n", error.message);
		return;
	}

	d(printf("Camel folder thaw remotely\n"));
}

void
camel_folder_remote_freeze (CamelFolderRemote *folder)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_freeze",
			&error, 
			"s", folder->object_id); 

	if (!ret) {
		g_warning ("Error: Camel folder freeze: %s\n", error.message);
		return;
	}

	d(printf("Camel folder freeze remotely\n"));
}

int
camel_folder_remote_get_message_count (CamelFolderRemote *folder)
{
	gboolean ret;
	DBusError error;
	int message_count;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_get_message_count",
			&error, 
			"s=>i", folder->object_id, &message_count); 

	if (!ret) {
		g_warning ("Error: Camel folder get message count: %s\n", error.message);
		return 0;
	}

	d(printf("Camel folder get message count remotely\n"));
	
	return message_count;
}

int
camel_folder_remote_get_deleted_message_count (CamelFolderRemote *folder)
{
	gboolean ret;
	DBusError error;
	int message_count;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_get_deleted_message_count",
			&error, 
			"s=>i", folder->object_id, &message_count); 

	if (!ret) {
		g_warning ("Error: Camel folder get deleted message count: %s\n", error.message);
		return 0;
	}

	d(printf("Camel folder get deleted message count remotely\n"));
	
	return message_count;
}

CamelObjectRemote *
camel_folder_remote_get_parent_store (CamelFolderRemote *folder)
{
	gboolean ret;
	DBusError error;
	char *store_hash_key;
	CamelObjectRemote *rstore;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_get_parent_store",
			&error, 
			"s=>s", folder->object_id, &store_hash_key); 

	if (!ret) {
		g_warning ("Error: Get parent store from camel folder remote: %s\n", error.message);
		return NULL;
	}

	rstore = (CamelObjectRemote *) g_hash_table_lookup (store_rhash, store_hash_key);
	
	d(printf("Got parent store from camel folder remotely\n"));
	return rstore;
}

guint32 camel_folder_remote_get_folder_flags (CamelFolderRemote *folder)
{
	abort ();
	return 0;
}

const char *camel_folder_remote_get_name (CamelFolderRemote *folder)
{
	return "";
}
