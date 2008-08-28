#ifndef CAMEL_SESSION_REMOTE_H
#define CAMEL_SESSION_REMOTE_H

#include "camel-object-remote.h"
#include "camel-store-remote.h"

void camel_folder_remote_construct (CamelFolderRemote *folder,
				CamelStoreRemote *parent_store,
				const char *full_name,
				const char *name);

void camel_folder_remote_thaw (CamelFolderRemote *folder);

void camel_folder_remote_freeze (CamelFolderRemote *folder);

int camel_folder_remote_get_message_count (CamelFolderRemote *folder);

int camel_folder_remote_get_deleted_message_count (CamelFolderRemote *folder);

const char *camel_folder_remote_get_name (CamelFolder *folder);

#endif
