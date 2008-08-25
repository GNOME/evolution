/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <mail-dbus.h>
#include <dbind.h>
#include <camel/camel-folder.h>
#include "camel-store-remote.h"
extern GHashTable *folder_hash;

#define CAMEL_DBUS_NAME "org.gnome.evolution.camel"
#define CAMEL_STORE_INTERFACE "org.gnome.evolution.camel.store"
#define CAMEL_STORE_OBJECT_PATH "/org/gnome/evolution/camel/store"


CamelFolder * 
camel_store_remote_get_folder (CamelStoreRemote *store, const char *folder_name, guint32 flags)
{
		gboolean ret;
		DBusError error;
		char *err;
		char *shash;

		dbus_error_init (&error);
		/* Invoke the appropriate dbind call to CamelStoreGetFolder */
		ret = dbind_context_method_call (evolution_dbus_peek_context(), 
						CAMEL_DBUS_NAME,
						CAMEL_STORE_OBJECT_PATH,
						CAMEL_STORE_INTERFACE,
						"camel_store_get_folder",
						&error, 
						"ssu=>ss", store->object_id, folder_name, flags, &shash, &err); 


		if (ret != DBUS_HANDLER_RESULT_HANDLED) {
				return NULL;
		} else {
			CamelFolder *folder = g_hash_table_lookup (folder_hash, shash);
			return folder;
		}
}
