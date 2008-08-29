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

CamelObjectRemote *camel_folder_remote_get_parent_store (CamelFolderRemote *folder);

const char *camel_folder_remote_get_name (CamelFolderRemote *folder);

const char *camel_folder_remote_get_full_name (CamelFolderRemote *folder);

void camel_folder_remote_sync (CamelFolderRemote *folder, gboolean expunge, CamelException *ex);

gboolean camel_folder_remote_set_message_flags (CamelFolderRemote *folder, const char *uid, guint32 flags, guint32 set);

guint32 camel_folder_remote_get_folder_flags (CamelFolderRemote *folder);

gboolean camel_folder_remote_get_message_user_flag (CamelFolderRemote *folder, const char *uid,
				    		const char *name);

void camel_folder_remote_set_message_user_flag (CamelFolderRemote *folder, const char *uid,
				    		const char *name, gboolean value);

const char *camel_folder_remote_get_message_user_tag (CamelFolderRemote *folder, const char *uid,  const char *name);

void camel_folder_remote_set_message_user_tag (CamelFolderRemote *folder, const char *uid, const char *name, const char *value);

void camel_folder_remote_expunge (CamelFolderRemote *folder, CamelException *ex);

gboolean camel_folder_remote_has_search_capability (CamelFolderRemote *folder);

guint32 camel_folder_remote_get_message_flags (CamelFolderRemote *folder, const char *uid);

#define camel_folder_remote_delete_message(folder, uid) \
	camel_folder_remote_set_message_flags (folder, uid, CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_SEEN)

void camel_folder_remote_set_vee_folder_expression (CamelFolderRemote *folder, const char *query);

#endif
