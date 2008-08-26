/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <camel/camel.h>

typedef struct {
	char *object_id;
} CamelFolderRemote;

typedef struct {
	char *object_id;
} CamelStoreRemote;

CamelFolder *camel_store_get_folder_remote(CamelStoreRemote * store,
					   const char *folder_name,
					   guint32 flags);
CamelFolder *camel_store_get_inbox_remote(CamelStoreRemote * store);
CamelFolder *camel_store_get_trash_remote(CamelStoreRemote * store);
CamelFolder *camel_store_get_junk_remote(CamelStoreRemote * store);

void camel_store_delete_folder_remote(CamelStoreRemote * store,
				      const char *folder_name);
void camel_store_rename_folder_remote(CamelStoreRemote * store,
				      const char *old_folder_name,
				      const char *new_folder_name);
