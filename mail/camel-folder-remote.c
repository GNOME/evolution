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
			"ssuu=>i", folder->object_id, uid, flags, set, &is_set); 

	if (!ret) {
		g_warning ("Error: Camel folder set message flags: %s\n", error.message);
		return 0;
	}

	d(printf("Camel folder set message flags remotely\n"));
	return is_set;
}


guint32 
camel_folder_remote_get_folder_flags (CamelFolderRemote *folder)
{
	gboolean ret;
	DBusError error;
	guint32 folder_flags;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_get_folder_flags",
			&error, 
			"s=>u", folder->object_id, &folder_flags); 

	if (!ret) {
		g_warning ("Error: Camel folder get folder flags: %s\n", error.message);
		return 0;
	}

	d(printf("Camel folder get folder flags remotely\n"));
	
	return folder_flags;
}

gboolean 
camel_folder_remote_get_message_user_flag (CamelFolderRemote *folder, const char *uid,
				    	const char *name)
{
	gboolean ret, user_flag;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_get_message_user_flag",
			&error, 
			"sss=>i", folder->object_id, uid, name, &user_flag); 

	if (!ret) {
		g_warning ("Error: Camel folder get message user flag: %s\n", error.message);
		return 0;
	}

	d(printf("Camel folder get message user flag remotely\n"));
	return user_flag;
}

void 
camel_folder_remote_set_message_user_flag (CamelFolderRemote *folder, const char *uid,
				    		const char *name, gboolean value)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_set_message_user_flag",
			&error, 
			"sssi", folder->object_id, uid, name, value); 

	if (!ret) {
		g_warning ("Error: Camel folder set message user flag: %s\n", error.message);
		return;
	}

	d(printf("Camel folder set message user flag remotely\n"));
}

const char *
camel_folder_remote_get_message_user_tag (CamelFolderRemote *folder, const char *uid,  const char *name)
{
	gboolean ret;
	const char *user_tag;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_get_message_user_flag",
			&error, 
			"sss=>s", folder->object_id, uid, name, &user_tag); 

	if (!ret) {
		g_warning ("Error: Camel folder get message user tag: %s\n", error.message);
		return NULL;
	}

	d(printf("Camel folder get message user tag remotely\n"));
	return user_tag;
}

void 
camel_folder_remote_set_message_user_tag (CamelFolderRemote *folder, const char *uid, const char *name, const char *value)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_FOLDER_OBJECT_PATH,
			CAMEL_FOLDER_INTERFACE,
			"camel_folder_set_message_user_tag",
			&error, 
			"ssss", folder->object_id, uid, name, value); 

	if (!ret) {
		g_warning ("Error: Camel folder set message user tag: %s\n", error.message);
		return;
	}

	d(printf("Camel folder set message user tag remotely\n"));
}


