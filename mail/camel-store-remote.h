/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#ifndef CAMEL_STORE_REMOTE_H
#define CAMEL_STORE_REMOTE_H

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

gboolean camel_store_supports_subscriptions_remote(CamelStoreRemote * store);

void camel_store_sync_remote(CamelStoreRemote * store, int expunge);
gboolean camel_store_folder_subscribed_remote(CamelStoreRemote * store,
					      const char *folder_name);
void camel_store_subscribe_folder_remote(CamelStoreRemote * store,
					 const char *folder_name);
void camel_store_unsubscribe_folder_remote(CamelStoreRemote * store,
					   const char *folder_name);
void camel_store_noop_remote(CamelStoreRemote * store);
int camel_store_folder_uri_equal_remote(CamelStoreRemote * store,
					const char *uri0, const char *uri1);
void camel_isubscribe_subscribe_remote(CamelStoreRemote * store,
				       const char *folder_name);
void camel_isubscribe_unsubscribe_remote(CamelStoreRemote * store,
					 const char *folder_name);
guint32 camel_store_get_mode_remote(CamelStoreRemote * store);
guint32 camel_store_get_flags_remote(CamelStoreRemote * store);
void camel_store_set_mode_remote(CamelStoreRemote * store, guint32 mode);
void camel_store_set_flags_remote(CamelStoreRemote * store, guint32 mode);
#endif
