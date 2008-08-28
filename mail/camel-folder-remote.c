#include <mail-dbus.h>
#include <evo-dbus.h>
#include <dbind.h>
#include <camel/camel-folder.h>
#include "camel-folder-remote.h"


#define d(x) x

extern GHashTable *folder_hash;

void
camel_folder_construct_remote (CamelFolderRemote *folder,
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
camel_folder_thaw_remote (CamelFolderRemote *folder)
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


