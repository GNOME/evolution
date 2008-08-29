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
	CamelObjectRemote *rstore=NULL;

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
	
	d(printf("Got parent store from camel folder remotely:%s %p in %p\n", store_hash_key, rstore, store_rhash));
	return rstore;
}

const char *
camel_folder_remote_get_name (CamelFolderRemote *folder)
{
	gboolean ret;
	DBusError error;
	const char *name;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_get_name",
			&error, 
			"s=>s", folder->object_id, &name); 

	if (!ret) {
		g_warning ("Error: Camel folder get name: %s\n", error.message);
		return NULL;
	}

	d(printf("Camel folder get name remotely\n"));
	return name;
}

const char *
camel_folder_remote_get_full_name (CamelFolderRemote *folder)
{
	gboolean ret;
	DBusError error;
	const char *full_name;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_get_full_name",
			&error, 
			"s=>s", folder->object_id, &full_name); 

	if (!ret) {
		g_warning ("Error: Camel folder get full name: %s\n", error.message);
		return NULL;
	}

	d(printf("Camel folder get full name remotely\n"));
	return full_name;
}

void 
camel_folder_remote_sync (CamelFolderRemote *folder, gboolean expunge, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_sync",
			&error, 
			"si=>s", folder->object_id, expunge, &err); 

	if (!ret) {
		g_warning ("Error: Camel folder sync: %s\n", error.message);
		return;
	}

	d(printf("Camel folder sync remotely\n"));
}

gboolean 
camel_folder_remote_set_message_flags (CamelFolderRemote *folder, const char *uid, guint32 flags, guint32 set)
{
	gboolean ret, is_set;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_set_message_flags",
			&error, 
			"ssii=>i", folder->object_id, uid, flags, set, &is_set); 

	if (!ret) {
		g_warning ("Error: Camel folder sync: %s\n", error.message);
		return 0;
	}

	d(printf("Camel folder sync remotely\n"));
	return is_set;
}


guint32 camel_folder_remote_get_folder_flags (CamelFolderRemote *folder)
{
	return 0;
}

