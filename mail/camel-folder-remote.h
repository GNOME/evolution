#ifndef CAMEL_SESSION_REMOTE_H
#define CAMEL_SESSION_REMOTE_H

#include "camel-object-remote.h"
#include "camel-store-remote.h"

void camel_folder_construct_remote (CamelFolderRemote *folder,
				CamelStoreRemote *parent_store,
				const char *full_name,
				const char *name);

void camel_folder_thaw_remote (CamelFolderRemote *folder);

#endif
