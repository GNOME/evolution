/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2003 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <pthread.h>

#include "camel-exception.h"
#include "camel-vee-folder.h"
#include "camel-store.h"
#include "camel-folder-summary.h"
#include "camel-mime-message.h"
#include "camel-folder-search.h"

#include "camel-session.h"
#include "camel-vee-store.h"	/* for open flags */
#include "camel-private.h"
#include "camel-debug.h"

#include "e-util/md5-utils.h"

#if defined (DOEPOOLV) || defined (DOESTRV)
#include "e-util/e-memory.h"
#endif

#define d(x) 
#define dd(x) (camel_debug("vfolder")?(x):0)

#define _PRIVATE(o) (((CamelVeeFolder *)(o))->priv)

static void vee_refresh_info(CamelFolder *folder, CamelException *ex);

static void vee_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
static void vee_expunge (CamelFolder *folder, CamelException *ex);

static void vee_freeze(CamelFolder *folder);
static void vee_thaw(CamelFolder *folder);

static CamelMimeMessage *vee_get_message (CamelFolder *folder, const gchar *uid, CamelException *ex);
static void vee_append_message(CamelFolder *folder, CamelMimeMessage *message, const CamelMessageInfo *info, char **appended_uid, CamelException *ex);
static void vee_transfer_messages_to(CamelFolder *source, GPtrArray *uids, CamelFolder *dest, GPtrArray **transferred_uids, gboolean delete_originals, CamelException *ex);

static GPtrArray *vee_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex);
static GPtrArray *vee_search_by_uids(CamelFolder *folder, const char *expression, GPtrArray *uids, CamelException *ex);

static gboolean vee_set_message_flags (CamelFolder *folder, const char *uid, guint32 flags, guint32 set);
static void vee_set_message_user_flag (CamelFolder *folder, const char *uid, const char *name, gboolean value);
static void vee_set_message_user_tag(CamelFolder *folder, const char *uid, const char *name, const char *value);
static void vee_rename(CamelFolder *folder, const char *new);

static void camel_vee_folder_class_init (CamelVeeFolderClass *klass);
static void camel_vee_folder_init       (CamelVeeFolder *obj);
static void camel_vee_folder_finalise   (CamelObject *obj);

static int vee_folder_build_folder(CamelVeeFolder *vf, CamelFolder *source, CamelException *ex);
static void vee_folder_remove_folder(CamelVeeFolder *vf, CamelFolder *source, int killun);

static void folder_changed(CamelFolder *sub, CamelFolderChangeInfo *changes, CamelVeeFolder *vf);
static void subfolder_deleted(CamelFolder *f, void *event_data, CamelVeeFolder *vf);
static void subfolder_renamed(CamelFolder *f, void *event_data, CamelVeeFolder *vf);

static void folder_changed_remove_uid(CamelFolder *sub, const char *uid, const char hash[8], int keep, CamelVeeFolder *vf);

static CamelFolderClass *camel_vee_folder_parent;

/* a vfolder for unmatched messages */
/* use folder_unmatched->summary_lock for access to unmatched_uids or appropriate internals, for consistency */
static CamelVeeFolder *folder_unmatched;
static GHashTable *unmatched_uids; /* a refcount of uid's that are matched by any rules */
static pthread_mutex_t unmatched_lock = PTHREAD_MUTEX_INITIALIZER;
/* only used to initialise folder_unmatched */
#define UNMATCHED_LOCK() pthread_mutex_lock(&unmatched_lock)
#define UNMATCHED_UNLOCK() pthread_mutex_unlock(&unmatched_lock)


CamelType
camel_vee_folder_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_folder_get_type (), "CamelVeeFolder",
					    sizeof (CamelVeeFolder),
					    sizeof (CamelVeeFolderClass),
					    (CamelObjectClassInitFunc) camel_vee_folder_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_vee_folder_init,
					    (CamelObjectFinalizeFunc) camel_vee_folder_finalise);
	}
	
	return type;
}

static void
camel_vee_folder_class_init (CamelVeeFolderClass *klass)
{
	CamelFolderClass *folder_class = (CamelFolderClass *) klass;

	camel_vee_folder_parent = CAMEL_FOLDER_CLASS(camel_type_get_global_classfuncs (camel_folder_get_type ()));

	folder_class->refresh_info = vee_refresh_info;
	folder_class->sync = vee_sync;
	folder_class->expunge = vee_expunge;

	folder_class->get_message = vee_get_message;
	folder_class->append_message = vee_append_message;
	folder_class->transfer_messages_to = vee_transfer_messages_to;

	folder_class->search_by_expression = vee_search_by_expression;
	folder_class->search_by_uids = vee_search_by_uids;

	folder_class->set_message_flags = vee_set_message_flags;
	folder_class->set_message_user_flag = vee_set_message_user_flag;
	folder_class->set_message_user_tag = vee_set_message_user_tag;

	folder_class->rename = vee_rename;

	folder_class->freeze = vee_freeze;
	folder_class->thaw = vee_thaw;
}

static void
camel_vee_folder_init (CamelVeeFolder *obj)
{
	struct _CamelVeeFolderPrivate *p;
	CamelFolder *folder = (CamelFolder *)obj;

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));

	folder->folder_flags |= (CAMEL_FOLDER_HAS_SUMMARY_CAPABILITY |
				 CAMEL_FOLDER_HAS_SEARCH_CAPABILITY);

	/* FIXME: what to do about user flags if the subfolder doesn't support them? */
	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED |
		CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT |
		CAMEL_MESSAGE_FLAGGED |
		CAMEL_MESSAGE_SEEN;

	obj->changes = camel_folder_change_info_new();
	obj->search = camel_folder_search_new();
	
	p->summary_lock = g_mutex_new();
	p->subfolder_lock = g_mutex_new();
	p->changed_lock = g_mutex_new();
}

static void
camel_vee_folder_finalise (CamelObject *obj)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)obj;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	/* FIXME: check leaks */
	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;

		if (vf != folder_unmatched) {
			camel_object_unhook_event((CamelObject *)f, "folder_changed", (CamelObjectEventHookFunc) folder_changed, vf);
			camel_object_unhook_event((CamelObject *)f, "deleted", (CamelObjectEventHookFunc) subfolder_deleted, vf);
			camel_object_unhook_event((CamelObject *)f, "renamed", (CamelObjectEventHookFunc) subfolder_renamed, vf);
			/* this updates the vfolder */
			if ((vf->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0)
				vee_folder_remove_folder(vf, f, FALSE);
		}
		camel_object_unref((CamelObject *)f);

		node = g_list_next(node);
	}

	g_free(vf->expression);
	g_free(vf->vname);
	
	g_list_free(p->folders);
	g_list_free(p->folders_changed);

	camel_folder_change_info_free(vf->changes);
	camel_object_unref((CamelObject *)vf->search);
	
	g_mutex_free(p->summary_lock);
	g_mutex_free(p->subfolder_lock);
	g_mutex_free(p->changed_lock);
	
	g_free(p);
}

static void
vee_folder_construct (CamelVeeFolder *vf, CamelStore *parent_store, const char *name, guint32 flags)
{
	CamelFolder *folder = (CamelFolder *)vf;
	char *tmp;

	vf->flags = flags;
	vf->vname = g_strdup(name);
	tmp = strrchr(vf->vname, '/');
	if (tmp)
		tmp++;
	else
		tmp = vf->vname;
	camel_folder_construct(folder, parent_store, vf->vname, tmp);

	/* should CamelVeeMessageInfo be subclassable ..? */
	folder->summary = camel_folder_summary_new();
	folder->summary->message_info_size = sizeof(CamelVeeMessageInfo);
}

void
camel_vee_folder_construct(CamelVeeFolder *vf, CamelStore *parent_store, const char *name, guint32 flags)
{
	UNMATCHED_LOCK();
	
	/* setup unmatched folder if we haven't yet */
	if (folder_unmatched == NULL) {
		unmatched_uids = g_hash_table_new (g_str_hash, g_str_equal);
		folder_unmatched = (CamelVeeFolder *)camel_object_new (camel_vee_folder_get_type ());
		d(printf("created foldeer unmatched %p\n", folder_unmatched));
		
		vee_folder_construct (folder_unmatched, parent_store, CAMEL_UNMATCHED_NAME, CAMEL_STORE_FOLDER_PRIVATE);
	}
	
	UNMATCHED_UNLOCK();
	
	vee_folder_construct (vf, parent_store, name, flags);
}

/**
 * camel_vee_folder_new:
 * @parent_store: the parent CamelVeeStore
 * @name: the vfolder name
 * @ex: a CamelException
 *
 * Create a new CamelVeeFolder object.
 *
 * Return value: A new CamelVeeFolder widget.
 **/
CamelFolder *
camel_vee_folder_new(CamelStore *parent_store, const char *name, guint32 flags)
{
	CamelVeeFolder *vf;
	char *tmp;

	UNMATCHED_LOCK();

	/* setup unmatched folder if we haven't yet */
	if (folder_unmatched == NULL) {
		unmatched_uids = g_hash_table_new(g_str_hash, g_str_equal);
		folder_unmatched = vf = (CamelVeeFolder *)camel_object_new(camel_vee_folder_get_type());
		d(printf("created foldeer unmatched %p\n", folder_unmatched));
		vee_folder_construct (vf, parent_store, CAMEL_UNMATCHED_NAME, CAMEL_STORE_FOLDER_PRIVATE);
	}

	UNMATCHED_UNLOCK();

	if (strcmp(name, CAMEL_UNMATCHED_NAME) == 0) {
		vf = folder_unmatched;
		camel_object_ref(vf);
	} else {
		vf = (CamelVeeFolder *)camel_object_new(camel_vee_folder_get_type());
		vee_folder_construct(vf, parent_store, name, flags);
	}

	d(printf("returning folder %s %p, count = %d\n", name, vf, camel_folder_get_message_count((CamelFolder *)vf)));

	tmp = g_strdup_printf("%s/%s.cmeta", ((CamelService *)parent_store)->url->path, name);
	camel_object_set(vf, NULL, CAMEL_OBJECT_STATE_FILE, tmp, NULL);
	g_free(tmp);
	if (camel_object_state_read(vf) == -1) {
		/* setup defaults: we have none currently */
	}

	return (CamelFolder *)vf;
}

void
camel_vee_folder_set_expression(CamelVeeFolder *vf, const char *query)
{
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);

	/* no change, do nothing */
	if ((vf->expression && query && strcmp(vf->expression, query) == 0)
	    || (vf->expression == NULL && query == NULL)) {
		CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);
		return;
	}

	g_free(vf->expression);
	if (query)
		vf->expression = g_strdup(query);

	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;

		if (vee_folder_build_folder(vf, f, NULL) == -1)
			break;

		node = node->next;
	}

	CAMEL_VEE_FOLDER_LOCK(vf, changed_lock);
	g_list_free(p->folders_changed);
	p->folders_changed = NULL;
	CAMEL_VEE_FOLDER_UNLOCK(vf, changed_lock);

	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);
}

/**
 * camel_vee_folder_add_folder:
 * @vf: Virtual Folder object
 * @sub: source CamelFolder to add to @vf
 *
 * Adds @sub as a source folder to @vf.
 **/
void
camel_vee_folder_add_folder(CamelVeeFolder *vf, CamelFolder *sub)
{
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf), *up = _PRIVATE(folder_unmatched);
	int i;
	
	if (vf == (CamelVeeFolder *)sub) {
		g_warning("Adding a virtual folder to itself as source, ignored");
		return;
	}

	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);

	/* for normal vfolders we want only unique ones, for unmatched we want them all recorded */
	if (g_list_find(p->folders, sub) == NULL) {
		camel_object_ref((CamelObject *)sub);
		p->folders = g_list_append(p->folders, sub);
		
		CAMEL_FOLDER_LOCK(vf, change_lock);

		/* update the freeze state of 'sub' to match our freeze state */
		for (i = 0; i < ((CamelFolder *)vf)->priv->frozen; i++)
			camel_folder_freeze(sub);

		CAMEL_FOLDER_UNLOCK(vf, change_lock);
	}
	if ((vf->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0 && !CAMEL_IS_VEE_FOLDER(sub)) {
		camel_object_ref((CamelObject *)sub);
		up->folders = g_list_append(up->folders, sub);

		CAMEL_FOLDER_LOCK(folder_unmatched, change_lock);

		/* update the freeze state of 'sub' to match Unmatched's freeze state */
		for (i = 0; i < ((CamelFolder *)folder_unmatched)->priv->frozen; i++)
			camel_folder_freeze(sub);

		CAMEL_FOLDER_UNLOCK(folder_unmatched, change_lock);
	}

	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);

	d(printf("camel_vee_folder_add_folde(%p, %p)\n", vf, sub));

	camel_object_hook_event((CamelObject *)sub, "folder_changed", (CamelObjectEventHookFunc)folder_changed, vf);
	camel_object_hook_event((CamelObject *)sub, "deleted", (CamelObjectEventHookFunc)subfolder_deleted, vf);
	camel_object_hook_event((CamelObject *)sub, "renamed", (CamelObjectEventHookFunc)subfolder_renamed, vf);

	vee_folder_build_folder(vf, sub, NULL);
}

/**
 * camel_vee_folder_remove_folder:
 * @vf: Virtual Folder object
 * @sub: source CamelFolder to remove from @vf
 *
 * Removed the source folder, @sub, from the virtual folder, @vf.
 **/
void
camel_vee_folder_remove_folder(CamelVeeFolder *vf, CamelFolder *sub)
{
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf), *up = _PRIVATE(folder_unmatched);
	int killun = FALSE;
	int i;
	
	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);

	CAMEL_VEE_FOLDER_LOCK(vf, changed_lock);
	p->folders_changed = g_list_remove(p->folders_changed, sub);
	CAMEL_VEE_FOLDER_UNLOCK(vf, changed_lock);

	if (g_list_find(p->folders, sub) == NULL) {
		CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);
		return;
	}
	
	camel_object_unhook_event((CamelObject *)sub, "folder_changed", (CamelObjectEventHookFunc) folder_changed, vf);
	camel_object_unhook_event((CamelObject *)sub, "deleted", (CamelObjectEventHookFunc) subfolder_deleted, vf);
	camel_object_unhook_event((CamelObject *)sub, "renamed", (CamelObjectEventHookFunc) subfolder_renamed, vf);

	p->folders = g_list_remove(p->folders, sub);
	
	/* undo the freeze state that we have imposed on this source folder */
	CAMEL_FOLDER_LOCK(vf, change_lock);
	for (i = 0; i < ((CamelFolder *)vf)->priv->frozen; i++)
		camel_folder_thaw(sub);
	CAMEL_FOLDER_UNLOCK(vf, change_lock);
	
	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);

	CAMEL_VEE_FOLDER_LOCK(folder_unmatched, subfolder_lock);
	/* if folder deleted, then blow it away from unmatched always, and remove all refs to it */
	if (sub->folder_flags & CAMEL_FOLDER_HAS_BEEN_DELETED) {
		while (g_list_find(up->folders, sub)) {
			killun = TRUE;
			up->folders = g_list_remove(up->folders, sub);
			camel_object_unref((CamelObject *)sub);
			
			/* undo the freeze state that Unmatched has imposed on this source folder */
			CAMEL_FOLDER_LOCK(folder_unmatched, change_lock);
			for (i = 0; i < ((CamelFolder *)folder_unmatched)->priv->frozen; i++)
				camel_folder_thaw(sub);
			CAMEL_FOLDER_UNLOCK(folder_unmatched, change_lock);
		}
	} else if ((vf->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0) {
		if (g_list_find(up->folders, sub) != NULL) {
			up->folders = g_list_remove(up->folders, sub);
			camel_object_unref((CamelObject *)sub);
			
			/* undo the freeze state that Unmatched has imposed on this source folder */
			CAMEL_FOLDER_LOCK(folder_unmatched, change_lock);
			for (i = 0; i < ((CamelFolder *)folder_unmatched)->priv->frozen; i++)
				camel_folder_thaw(sub);
			CAMEL_FOLDER_UNLOCK(folder_unmatched, change_lock);
		}
		if (g_list_find(up->folders, sub) == NULL) {
			killun = TRUE;
		}
	}
	CAMEL_VEE_FOLDER_UNLOCK(folder_unmatched, subfolder_lock);

	vee_folder_remove_folder(vf, sub, killun);

	camel_object_unref((CamelObject *)sub);
}

static void
remove_folders(CamelFolder *folder, CamelFolder *foldercopy, CamelVeeFolder *vf)
{
	camel_vee_folder_remove_folder(vf, folder);
	camel_object_unref((CamelObject *)folder);
}

/**
 * camel_vee_folder_set_folders:
 * @vf: 
 * @folders: 
 * 
 * Set the whole list of folder sources on a vee folder.
 **/
void
camel_vee_folder_set_folders(CamelVeeFolder *vf, GList *folders)
{
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GHashTable *remove = g_hash_table_new(NULL, NULL);
	GList *l;
	CamelFolder *folder;
	int changed;

	/* setup a table of all folders we have currently */
	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);
	l = p->folders;
	while (l) {
		g_hash_table_insert(remove, l->data, l->data);
		camel_object_ref((CamelObject *)l->data);
		l = l->next;
	}
	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);

	/* if we already have the folder, ignore it, otherwise add it */
	l = folders;
	while (l) {
		if ((folder = g_hash_table_lookup(remove, l->data))) {
			g_hash_table_remove(remove, folder);
			camel_object_unref((CamelObject *)folder);

			/* if this was a changed folder, re-update it while we're here */
			CAMEL_VEE_FOLDER_LOCK(vf, changed_lock);
			changed = g_list_find(p->folders_changed, folder) != NULL;
			if (changed)
				p->folders_changed = g_list_remove(p->folders_changed, folder);
			CAMEL_VEE_FOLDER_UNLOCK(vf, changed_lock);
			if (changed)
				vee_folder_build_folder(vf, folder, NULL);
		} else {
			camel_vee_folder_add_folder(vf, l->data);
		}
		l = l->next;
	}

	/* then remove any we still have */
	g_hash_table_foreach(remove, (GHFunc)remove_folders, vf);
	g_hash_table_destroy(remove);
}

/**
 * camel_vee_folder_hash_folder:
 * @folder: 
 * @: 
 * 
 * Create a hash string representing the folder name, which should be
 * unique, and remain static for a given folder.
 **/
void
camel_vee_folder_hash_folder(CamelFolder *folder, char buffer[8])
{
	MD5Context ctx;
	unsigned char digest[16];
	unsigned int state = 0, save = 0;
	char *tmp;
	int i;

	md5_init(&ctx);
	tmp = camel_service_get_url((CamelService *)folder->parent_store);
	md5_update(&ctx, tmp, strlen(tmp));
	g_free(tmp);
	md5_update(&ctx, folder->full_name, strlen(folder->full_name));
	md5_final(&ctx, digest);
	camel_base64_encode_close(digest, 6, FALSE, buffer, &state, &save);

	for (i=0;i<8;i++) {
		if (buffer[i] == '+')
			buffer[i] = '.';
		if (buffer[i] == '/')
			buffer[i] = '_';
	}
}

/**
 * camel_vee_folder_get_location:
 * @vf: 
 * @vinfo: 
 * @realuid: if not NULL, set to the uid of the real message, must be
 * g_free'd by caller.
 * 
 * Find the real folder (and uid)
 * 
 * Return value: 
 **/
CamelFolder *
camel_vee_folder_get_location(CamelVeeFolder *vf, const CamelVeeMessageInfo *vinfo, char **realuid)
{
	/* locking?  yes?  no?  although the vfolderinfo is valid when obtained
	   the folder in it might not necessarily be so ...? */
	if (CAMEL_IS_VEE_FOLDER(vinfo->folder)) {
		CamelFolder *folder;
		const CamelVeeMessageInfo *vfinfo;

		vfinfo = (CamelVeeMessageInfo *)camel_folder_get_message_info(vinfo->folder, camel_message_info_uid(vinfo)+8);
		folder = camel_vee_folder_get_location((CamelVeeFolder *)vinfo->folder, vfinfo, realuid);
		camel_folder_free_message_info(vinfo->folder, (CamelMessageInfo *)vfinfo);
		return folder;
	} else {
		if (realuid)
			*realuid = g_strdup(camel_message_info_uid(vinfo)+8);

		return vinfo->folder;
	}
}

static void vee_refresh_info(CamelFolder *folder, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node, *list;

	CAMEL_VEE_FOLDER_LOCK(vf, changed_lock);
	list = p->folders_changed;
	p->folders_changed = NULL;
	CAMEL_VEE_FOLDER_UNLOCK(vf, changed_lock);

	node = list;
	while (node) {
		CamelFolder *f = node->data;

		if (vee_folder_build_folder(vf, f, ex) == -1)
			break;

		node = node->next;
	}

	g_list_free(list);
}

static void
vee_sync(CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);

	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;

		camel_folder_sync(f, expunge, ex);
		if (camel_exception_is_set(ex)) {
			char *desc;

			camel_object_get(f, NULL, CAMEL_OBJECT_DESCRIPTION, &desc, NULL);
			camel_exception_setv(ex, ex->id, _("Error storing `%s': %s"), desc, ex->desc);
			break;
		}

		if (vee_folder_build_folder(vf, f, ex) == -1)
			break;

		node = node->next;
	}

	if (node == NULL) {
		CAMEL_VEE_FOLDER_LOCK(vf, changed_lock);
		g_list_free(p->folders_changed);
		p->folders_changed = NULL;
		CAMEL_VEE_FOLDER_UNLOCK(vf, changed_lock);
	}

	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);

	camel_object_state_write(vf);
}

static void
vee_expunge (CamelFolder *folder, CamelException *ex)
{
	((CamelFolderClass *)((CamelObject *)folder)->klass)->sync(folder, TRUE, ex);
}

static CamelMimeMessage *
vee_get_message(CamelFolder *folder, const char *uid, CamelException *ex)
{
	CamelVeeMessageInfo *mi;
	CamelMimeMessage *msg = NULL;

	mi = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, uid);
	if (mi) {
		msg =  camel_folder_get_message(mi->folder, camel_message_info_uid(mi)+8, ex);
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);
	} else {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     _("No such message %s in %s"), uid,
				     folder->name);
	}

	return msg;
}

static GPtrArray *
vee_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex)
{
	GList *node;
	GPtrArray *matches, *result = g_ptr_array_new ();
	char *expr;
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GHashTable *searched = g_hash_table_new(NULL, NULL);
	
	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);
	
	if (vf != folder_unmatched)
		expr = g_strdup_printf ("(and %s %s)", vf->expression, expression);
	else
		expr = g_strdup (expression);
	
	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;
		int i;
		char hash[8];
		
		/* make sure we only search each folder once - for unmatched folder to work right */
		if (g_hash_table_lookup(searched, f) == NULL) {
			camel_vee_folder_hash_folder(f, hash);
			/* FIXME: shouldn't ignore search exception */
			matches = camel_folder_search_by_expression(f, expression, NULL);
			if (matches) {
				for (i = 0; i < matches->len; i++) {
					char *uid = matches->pdata[i], *vuid;

					vuid = g_malloc(strlen(uid)+9);
					memcpy(vuid, hash, 8);
					strcpy(vuid+8, uid);
					g_ptr_array_add(result, vuid);
				}
				camel_folder_search_free(f, matches);
			}
			g_hash_table_insert(searched, f, f);
		}
		node = g_list_next(node);
	}

	g_free(expr);
	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);

	g_hash_table_destroy(searched);

	return result;
}

static GPtrArray *
vee_search_by_uids(CamelFolder *folder, const char *expression, GPtrArray *uids, CamelException *ex)
{
	GList *node;
	GPtrArray *matches, *result = g_ptr_array_new ();
	GPtrArray *folder_uids = g_ptr_array_new();
	char *expr;
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GHashTable *searched = g_hash_table_new(NULL, NULL);

	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);

	expr = g_strdup_printf("(and %s %s)", vf->expression, expression);
	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;
		int i;
		char hash[8];
		
		/* make sure we only search each folder once - for unmatched folder to work right */
		if (g_hash_table_lookup(searched, f) == NULL) {
			camel_vee_folder_hash_folder(f, hash);

			/* map the vfolder uid's to the source folder uid's first */
			g_ptr_array_set_size(folder_uids, 0);
			for (i=0;i<uids->len;i++) {
				char *uid = uids->pdata[i];

				if (strlen(uid) >= 8 && strncmp(uid, hash, 8) == 0)
					g_ptr_array_add(folder_uids, uid+8);
			}
			if (folder_uids->len > 0) {
				matches = camel_folder_search_by_uids(f, expression, folder_uids, ex);
				if (matches) {
					for (i = 0; i < matches->len; i++) {
						char *uid = matches->pdata[i], *vuid;
				
						vuid = g_malloc(strlen(uid)+9);
						memcpy(vuid, hash, 8);
						strcpy(vuid+8, uid);
						g_ptr_array_add(result, vuid);
					}
					camel_folder_search_free(f, matches);
				} else {
					g_warning("Search failed: %s", camel_exception_get_description(ex));
				}
			}
			g_hash_table_insert(searched, f, f);
		}
		node = g_list_next(node);
	}

	g_free(expr);
	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);

	g_hash_table_destroy(searched);
	g_ptr_array_free(folder_uids, 0);

	return result;
}

static gboolean
vee_set_message_flags(CamelFolder *folder, const char *uid, guint32 flags, guint32 set)
{
	CamelVeeMessageInfo *mi;
	int res = FALSE;

	mi = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, uid);
	if (mi) {
		res = camel_folder_set_message_flags(mi->folder, camel_message_info_uid(mi) + 8, flags, set);
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);
		res = res || ((CamelFolderClass *)camel_vee_folder_parent)->set_message_flags(folder, uid, flags, set);
	}

	return res;
}

static void
vee_set_message_user_flag(CamelFolder *folder, const char *uid, const char *name, gboolean value)
{
	CamelVeeMessageInfo *mi;

	mi = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, uid);
	if (mi) {
		camel_folder_set_message_user_flag(mi->folder, camel_message_info_uid(mi) + 8, name, value);
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);
		((CamelFolderClass *)camel_vee_folder_parent)->set_message_user_flag(folder, uid, name, value);
	}
}

static void
vee_set_message_user_tag(CamelFolder *folder, const char *uid, const char *name, const char *value)
{
	CamelVeeMessageInfo *mi;

	mi = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, uid);
	if (mi) {
		camel_folder_set_message_user_tag(mi->folder, camel_message_info_uid(mi) + 8, name, value);
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);
		((CamelFolderClass *)camel_vee_folder_parent)->set_message_user_tag(folder, uid, name, value);
	}
}

static void
vee_append_message(CamelFolder *folder, CamelMimeMessage *message, const CamelMessageInfo *info, char **appended_uid, CamelException *ex)
{
	camel_exception_set(ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot copy or move messages into a Virtual Folder"));
}

static void
vee_transfer_messages_to (CamelFolder *folder, GPtrArray *uids, CamelFolder *dest, GPtrArray **transferred_uids, gboolean delete_originals, CamelException *ex)
{
	camel_exception_set(ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot copy or move messages into a Virtual Folder"));
}

static void vee_rename(CamelFolder *folder, const char *new)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;

	g_free(vf->vname);
	vf->vname = g_strdup(new);

	((CamelFolderClass *)camel_vee_folder_parent)->rename(folder, new);
}

/* ********************************************************************** *
   utility functions */

/* must be called with summary_lock held */
static CamelVeeMessageInfo *
vee_folder_add_info(CamelVeeFolder *vf, CamelFolder *f, CamelMessageInfo *info, const char hash[8])
{
	CamelVeeMessageInfo *mi;
	char *vuid;
	const char *uid;
	CamelFolder *folder = (CamelFolder *)vf;
	CamelMessageInfo *dinfo;

	uid = camel_message_info_uid(info);
	vuid = alloca(strlen(uid)+9);
	memcpy(vuid, hash, 8);
	strcpy(vuid+8, uid);
	dinfo = camel_folder_summary_uid(folder->summary, vuid);
	if (dinfo) {
		d(printf("w:clash, we already have '%s' in summary\n", vuid));
		camel_folder_summary_info_free(folder->summary, dinfo);
		return NULL;
	}

	d(printf("adding vuid %s to %s\n", vuid, vf->vname));

	mi = (CamelVeeMessageInfo *)camel_folder_summary_info_new(folder->summary);
	camel_message_info_dup_to(info, (CamelMessageInfo *)mi);
#ifdef DOEPOOLV
	mi->info.strings = e_poolv_set(mi->info.strings, CAMEL_MESSAGE_INFO_UID, vuid, FALSE);
#elif defined (DOESTRV)
	mi->info.strings = e_strv_set_ref(mi->info.strings, CAMEL_MESSAGE_INFO_UID, vuid);
	mi->info.strings = e_strv_pack(mi->info.strings);
#else	
	g_free(mi->info.uid);
	mi->info.uid = g_strdup(vuid);
#endif
	mi->folder = f;
	camel_folder_summary_add(folder->summary, (CamelMessageInfo *)mi);

	return mi;
}

/* must be called with summary_lock held */
static CamelVeeMessageInfo *
vee_folder_add_uid(CamelVeeFolder *vf, CamelFolder *f, const char *inuid, const char hash[8])
{
	CamelMessageInfo *info;
	CamelVeeMessageInfo *mi = NULL;

	info = camel_folder_get_message_info(f, inuid);
	if (info) {
		mi = vee_folder_add_info(vf, f, info, hash);
		camel_folder_free_message_info(f, info);
	}
	return mi;
}

static void
vee_folder_remove_folder(CamelVeeFolder *vf, CamelFolder *source, int killun)
{
	int i, count, n, still, start, last;
	char *oldkey;
	CamelFolder *folder = (CamelFolder *)vf;
	char hash[8];
	/*struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);*/
	CamelFolderChangeInfo *vf_changes = NULL, *unmatched_changes = NULL;
	void *oldval;
	
	if (vf == folder_unmatched)
		return;

	/* check if this folder is still to be part of unmatched */
	if ((vf->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0 && !killun) {
		CAMEL_VEE_FOLDER_LOCK(folder_unmatched, subfolder_lock);
		still = g_list_find(_PRIVATE(folder_unmatched)->folders, source) != NULL;
		CAMEL_VEE_FOLDER_UNLOCK(folder_unmatched, subfolder_lock);
		camel_vee_folder_hash_folder(source, hash);
	} else {
		still = FALSE;
	}

	CAMEL_VEE_FOLDER_LOCK(vf, summary_lock);
	CAMEL_VEE_FOLDER_LOCK(folder_unmatched, summary_lock);

	/* See if we just blow all uid's from this folder away from unmatched, regardless */
	if (killun) {
		start = -1;
		last = -1;
		count = camel_folder_summary_count(((CamelFolder *)folder_unmatched)->summary);
		for (i=0;i<count;i++) {
			CamelVeeMessageInfo *mi = (CamelVeeMessageInfo *)camel_folder_summary_index(((CamelFolder *)folder_unmatched)->summary, i);
			
			if (mi) {
				if (mi->folder == source) {
					camel_folder_change_info_remove_uid(folder_unmatched->changes, camel_message_info_uid(mi));
					if (last == -1) {
						last = start = i;
					} else if (last+1 == i) {
						last = i;
					} else {
						camel_folder_summary_remove_range(((CamelFolder *)folder_unmatched)->summary, start, last);
						i -= (last-start)+1;
						start = last = i;
					}
				}
				camel_folder_summary_info_free(((CamelFolder *)folder_unmatched)->summary, (CamelMessageInfo *)mi);
			}
		}
		if (last != -1)
			camel_folder_summary_remove_range(((CamelFolder *)folder_unmatched)->summary, start, last);
	}

	start = -1;
	last = -1;
	count = camel_folder_summary_count(folder->summary);
	for (i=0;i<count;i++) {
		CamelVeeMessageInfo *mi = (CamelVeeMessageInfo *)camel_folder_summary_index(folder->summary, i);
		if (mi) {
			if (mi->folder == source) {
				const char *uid = camel_message_info_uid(mi);

				camel_folder_change_info_remove_uid(vf->changes, uid);

				if (last == -1) {
					last = start = i;
				} else if (last+1 == i) {
					last = i;
				} else {
					camel_folder_summary_remove_range(folder->summary, start, last);
					i -= (last-start)+1;
					start = last = i;
				}
				if ((vf->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0) {
					if (still) {
						if (g_hash_table_lookup_extended(unmatched_uids, uid, (void **)&oldkey, &oldval)) {
							n = GPOINTER_TO_INT (oldval);
							if (n == 1) {
								g_hash_table_remove(unmatched_uids, oldkey);
								if (vee_folder_add_uid(folder_unmatched, source, oldkey+8, hash))
									camel_folder_change_info_add_uid(folder_unmatched->changes, oldkey);
								g_free(oldkey);
							} else {
								g_hash_table_insert(unmatched_uids, oldkey, GINT_TO_POINTER(n-1));
							}
						}
					} else {
						if (g_hash_table_lookup_extended(unmatched_uids, camel_message_info_uid(mi), (void **)&oldkey, &oldval)) {
							g_hash_table_remove(unmatched_uids, oldkey);
							g_free(oldkey);
						}
					}
				}
			}
			camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);
		}
	}

	if (last != -1)
		camel_folder_summary_remove_range(folder->summary, start, last);

	if (camel_folder_change_info_changed(folder_unmatched->changes)) {
		unmatched_changes = folder_unmatched->changes;
		folder_unmatched->changes = camel_folder_change_info_new();
	}

	if (camel_folder_change_info_changed(vf->changes)) {
		vf_changes = vf->changes;
		vf->changes = camel_folder_change_info_new();
	}

	CAMEL_VEE_FOLDER_UNLOCK(folder_unmatched, summary_lock);
	CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);

	if (unmatched_changes) {
		camel_object_trigger_event((CamelObject *)folder_unmatched, "folder_changed", unmatched_changes);
		camel_folder_change_info_free(unmatched_changes);
	}

	if (vf_changes) {
		camel_object_trigger_event((CamelObject *)vf, "folder_changed", vf_changes);
		camel_folder_change_info_free(vf_changes);
	}
}

struct _update_data {
	CamelFolder *source;
	CamelVeeFolder *vf;
	char hash[8];
};

static void
unmatched_check_uid(char *uidin, void *value, struct _update_data *u)
{
	char *uid;
	int n;

	uid = alloca(strlen(uidin)+9);
	memcpy(uid, u->hash, 8);
	strcpy(uid+8, uidin);
	n = GPOINTER_TO_INT(g_hash_table_lookup(unmatched_uids, uid));
	if (n == 0) {
		if (vee_folder_add_uid(folder_unmatched, u->source, uidin, u->hash))
			camel_folder_change_info_add_uid(folder_unmatched->changes, uid);
	} else {
		CamelVeeMessageInfo *mi = (CamelVeeMessageInfo *)camel_folder_summary_uid(((CamelFolder *)folder_unmatched)->summary, uid);
		if (mi) {
			camel_folder_summary_remove(((CamelFolder *)folder_unmatched)->summary, (CamelMessageInfo *)mi);
			camel_folder_change_info_remove_uid(folder_unmatched->changes, uid);
			camel_folder_summary_info_free(((CamelFolder *)folder_unmatched)->summary, (CamelMessageInfo *)mi);
		}
	}
}

static void
folder_added_uid(char *uidin, void *value, struct _update_data *u)
{
	CamelVeeMessageInfo *mi;
	char *oldkey;
	void *oldval;
	int n;

	if ( (mi = vee_folder_add_uid(u->vf, u->source, uidin, u->hash)) ) {
		camel_folder_change_info_add_uid(u->vf->changes, camel_message_info_uid(mi));

		if (!CAMEL_IS_VEE_FOLDER(u->source)) {
			if (g_hash_table_lookup_extended(unmatched_uids, camel_message_info_uid(mi), (void **)&oldkey, &oldval)) {
				n = GPOINTER_TO_INT (oldval);
				g_hash_table_insert(unmatched_uids, oldkey, GINT_TO_POINTER(n+1));
			} else {
				g_hash_table_insert(unmatched_uids, g_strdup(camel_message_info_uid(mi)), GINT_TO_POINTER(1));
			}
		}
	}
}

/* build query contents for a single folder */
static int
vee_folder_build_folder(CamelVeeFolder *vf, CamelFolder *source, CamelException *ex)
{
	GPtrArray *match, *all;
	GHashTable *allhash, *matchhash;
	CamelFolder *f = source;
	CamelFolder *folder = (CamelFolder *)vf;
	int i, n, count, start, last;
	struct _update_data u;
	CamelFolderChangeInfo *vf_changes = NULL, *unmatched_changes = NULL;

	if (vf == folder_unmatched)
		return 0;

	/* if we have no expression, or its been cleared, then act as if no matches */
	if (vf->expression == NULL) {
		match = g_ptr_array_new();
	} else {
		match = camel_folder_search_by_expression(f, vf->expression, ex);
		if (match == NULL)
			return -1;
	}

	u.source = source;
	u.vf = vf;
	camel_vee_folder_hash_folder(source, u.hash);

	CAMEL_VEE_FOLDER_LOCK(vf, summary_lock);

	/* we build 2 hash tables, one for all uid's not matched, the other for all matched uid's,
	   we just ref the real memory */
	matchhash = g_hash_table_new(g_str_hash, g_str_equal);
	for (i=0;i<match->len;i++)
		g_hash_table_insert(matchhash, match->pdata[i], GINT_TO_POINTER (1));

	allhash = g_hash_table_new(g_str_hash, g_str_equal);
	all = camel_folder_get_uids(f);
	for (i=0;i<all->len;i++)
		if (g_hash_table_lookup(matchhash, all->pdata[i]) == NULL)
			g_hash_table_insert(allhash, all->pdata[i], GINT_TO_POINTER (1));

	CAMEL_VEE_FOLDER_LOCK(folder_unmatched, summary_lock);

	/* scan, looking for "old" uid's to be removed */
	start = -1;
	last = -1;
	count = camel_folder_summary_count(folder->summary);
	for (i=0;i<count;i++) {
		CamelVeeMessageInfo *mi = (CamelVeeMessageInfo *)camel_folder_summary_index(folder->summary, i);

		if (mi) {
			if (mi->folder == source) {
				char *uid = (char *)camel_message_info_uid(mi), *oldkey;
				void *oldval;
				
				if (g_hash_table_lookup(matchhash, uid+8) == NULL) {
					if (last == -1) {
						last = start = i;
					} else if (last+1 == i) {
						last = i;
					} else {
						camel_folder_summary_remove_range(folder->summary, start, last);
						i -= (last-start)+1;
						start = last = i;
					}
					camel_folder_change_info_remove_uid(vf->changes, camel_message_info_uid(mi));
					if (!CAMEL_IS_VEE_FOLDER(source)
					    && g_hash_table_lookup_extended(unmatched_uids, uid, (void **)&oldkey, &oldval)) {
						n = GPOINTER_TO_INT (oldval);
						if (n == 1) {
							g_hash_table_remove(unmatched_uids, oldkey);
							g_free(oldkey);
						} else {
							g_hash_table_insert(unmatched_uids, oldkey, GINT_TO_POINTER(n-1));
						}
					}
				} else {
					g_hash_table_remove(matchhash, uid+8);
				}
			}
			camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);
		}
	}
	if (last != -1)
		camel_folder_summary_remove_range(folder->summary, start, last);

	/* now matchhash contains any new uid's, add them, etc */
	g_hash_table_foreach(matchhash, (GHFunc)folder_added_uid, &u);

	/* scan unmatched, remove any that have vanished, etc */
	count = camel_folder_summary_count(((CamelFolder *)folder_unmatched)->summary);
	for (i=0;i<count;i++) {
		CamelVeeMessageInfo *mi = (CamelVeeMessageInfo *)camel_folder_summary_index(((CamelFolder *)folder_unmatched)->summary, i);

		if (mi) {
			if (mi->folder == source) {
				char *uid = (char *)camel_message_info_uid(mi);

				if (g_hash_table_lookup(allhash, uid+8) == NULL) {
					/* no longer exists at all, just remove it entirely */
					camel_folder_summary_remove_index(((CamelFolder *)folder_unmatched)->summary, i);
					camel_folder_change_info_remove_uid(folder_unmatched->changes, camel_message_info_uid(mi));
					i--;
				} else {
					g_hash_table_remove(allhash, uid+8);
				}
			}
			camel_folder_summary_info_free(((CamelFolder *)folder_unmatched)->summary, (CamelMessageInfo *)mi);
		}
	}

	/* now allhash contains all potentially new uid's for the unmatched folder, process */
	if (!CAMEL_IS_VEE_FOLDER(source))
		g_hash_table_foreach(allhash, (GHFunc)unmatched_check_uid, &u);

	/* copy any changes so we can raise them outside the lock */
	if (camel_folder_change_info_changed(folder_unmatched->changes)) {
		unmatched_changes = folder_unmatched->changes;
		folder_unmatched->changes = camel_folder_change_info_new();
	}

	if (camel_folder_change_info_changed(vf->changes)) {
		vf_changes = vf->changes;
		vf->changes = camel_folder_change_info_new();
	}

	CAMEL_VEE_FOLDER_UNLOCK(folder_unmatched, summary_lock);
	CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);

	g_hash_table_destroy(matchhash);
	g_hash_table_destroy(allhash);
	/* if expression not set, we only had a null list */
	if (vf->expression == NULL)
		g_ptr_array_free(match, TRUE);
	else
		camel_folder_search_free(f, match);
	camel_folder_free_uids(f, all);

	if (unmatched_changes) {
		camel_object_trigger_event((CamelObject *)folder_unmatched, "folder_changed", unmatched_changes);
		camel_folder_change_info_free(unmatched_changes);
	}

	if (vf_changes) {
		camel_object_trigger_event((CamelObject *)vf, "folder_changed", vf_changes);
		camel_folder_change_info_free(vf_changes);
	}

	return 0;
}

/*

  (match-folder "folder1" "folder2")

 */


/* Hold all these with summary lock and unmatched summary lock held */
static void
folder_changed_add_uid(CamelFolder *sub, const char *uid, const char hash[8], CamelVeeFolder *vf)
{
	CamelVeeMessageInfo *vinfo;
	const char *vuid;
	char *oldkey;
	void *oldval;
	int n;

	vinfo = vee_folder_add_uid(vf, sub, uid, hash);
	if (vinfo == NULL)
		return;
	
	vuid = camel_message_info_uid(vinfo);
	camel_folder_change_info_add_uid(vf->changes,  vuid);

	if ((vf->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0 && !CAMEL_IS_VEE_FOLDER(sub)) {
		if (g_hash_table_lookup_extended(unmatched_uids, vuid, (void **)&oldkey, &oldval)) {
			n = GPOINTER_TO_INT (oldval);
			g_hash_table_insert(unmatched_uids, oldkey, GINT_TO_POINTER(n+1));
		} else {
			g_hash_table_insert(unmatched_uids, g_strdup(vuid), GINT_TO_POINTER (1));
		}
		vinfo = (CamelVeeMessageInfo *)camel_folder_get_message_info((CamelFolder *)folder_unmatched, vuid);
		if (vinfo) {
			camel_folder_change_info_remove_uid(folder_unmatched->changes, vuid);
			camel_folder_summary_remove(((CamelFolder *)folder_unmatched)->summary, (CamelMessageInfo *)vinfo);
			camel_folder_free_message_info((CamelFolder *)folder_unmatched, (CamelMessageInfo *)vinfo);
		}
	}
}

static void
folder_changed_remove_uid(CamelFolder *sub, const char *uid, const char hash[8], int keep, CamelVeeFolder *vf)
{
	CamelFolder *folder = (CamelFolder *)vf;
	char *vuid, *oldkey;
	void *oldval;
	int n;
	CamelVeeMessageInfo *vinfo;

	vuid = alloca(strlen(uid)+9);
	memcpy(vuid, hash, 8);
	strcpy(vuid+8, uid);

	vinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, vuid);
	if (vinfo) {
		camel_folder_change_info_remove_uid(vf->changes, vuid);
		camel_folder_summary_remove(folder->summary, (CamelMessageInfo *)vinfo);
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)vinfo);
	}

	if ((vf->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0 && !CAMEL_IS_VEE_FOLDER(sub)) {
		if (keep) {
			if (g_hash_table_lookup_extended(unmatched_uids, vuid, (void **)&oldkey, &oldval)) {
				n = GPOINTER_TO_INT (oldval);
				if (n == 1) {
					g_hash_table_remove(unmatched_uids, oldkey);
					if (vee_folder_add_uid(folder_unmatched, sub, uid, hash))
						camel_folder_change_info_add_uid(folder_unmatched->changes, oldkey);
					g_free(oldkey);
				} else {
					g_hash_table_insert(unmatched_uids, oldkey, GINT_TO_POINTER(n-1));
				}
			} else {
				if (vee_folder_add_uid(folder_unmatched, sub, uid, hash))
					camel_folder_change_info_add_uid(folder_unmatched->changes, oldkey);
			}
		} else {
			if (g_hash_table_lookup_extended(unmatched_uids, vuid, (void **)&oldkey, &oldval)) {
				g_hash_table_remove(unmatched_uids, oldkey);
				g_free(oldkey);
			}

			vinfo = (CamelVeeMessageInfo *)camel_folder_get_message_info((CamelFolder *)folder_unmatched, vuid);
			if (vinfo) {
				camel_folder_change_info_remove_uid(folder_unmatched->changes, vuid);
				camel_folder_summary_remove_uid(((CamelFolder *)folder_unmatched)->summary, vuid);
				camel_folder_free_message_info((CamelFolder *)folder_unmatched, (CamelMessageInfo *)vinfo);
			}
		}
	}
}

static void
folder_changed_change_uid(CamelFolder *sub, const char *uid, const char hash[8], CamelVeeFolder *vf)
{
	char *vuid;
	CamelVeeMessageInfo *vinfo, *uinfo;
	CamelMessageInfo *info;
	CamelFolder *folder = (CamelFolder *)vf;

	vuid = alloca(strlen(uid)+9);
	memcpy(vuid, hash, 8);
	strcpy(vuid+8, uid);

	vinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, vuid);
	uinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(((CamelFolder *)folder_unmatched)->summary, vuid);
	if (vinfo || uinfo) {
		info = camel_folder_get_message_info(sub, uid);
		if (info) {
			if (vinfo) {
				int changed = FALSE;

				if (vinfo->info.flags != info->flags){
					vinfo->info.flags = info->flags;
					changed = TRUE;
				}
			
				changed |= camel_flag_list_copy(&vinfo->info.user_flags, &info->user_flags);
				changed |= camel_tag_list_copy(&vinfo->info.user_tags, &info->user_tags);
				if (changed)
					camel_folder_change_info_change_uid(vf->changes, vuid);

				camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)vinfo);
			}

			if (uinfo) {
				int changed = FALSE;

				if (uinfo->info.flags != info->flags){
					uinfo->info.flags = info->flags;
					changed = TRUE;
				}
			
				changed |= camel_flag_list_copy(&uinfo->info.user_flags, &info->user_flags);
				changed |= camel_tag_list_copy(&uinfo->info.user_tags, &info->user_tags);
				if (changed)
					camel_folder_change_info_change_uid(folder_unmatched->changes, vuid);

				camel_folder_summary_info_free(((CamelFolder *)folder_unmatched)->summary, (CamelMessageInfo *)uinfo);
			}

			camel_folder_free_message_info(sub, info);
		} else {
			if (vinfo) {
				folder_changed_remove_uid(sub, uid, hash, FALSE, vf);
				camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)vinfo);
			}
			if (uinfo)
				camel_folder_summary_info_free(((CamelFolder *)folder_unmatched)->summary, (CamelMessageInfo *)uinfo);
		}
	}
}

struct _folder_changed_msg {
	CamelSessionThreadMsg msg;
	CamelFolderChangeInfo *changes;
	CamelFolder *sub;
	CamelVeeFolder *vf;
};

static void
folder_changed_change(CamelSession *session, CamelSessionThreadMsg *msg)
{
	struct _folder_changed_msg *m = (struct _folder_changed_msg *)msg;
	CamelFolder *sub = m->sub;
	CamelFolder *folder = (CamelFolder *)m->vf;
	CamelVeeFolder *vf = m->vf;
	CamelFolderChangeInfo *changes = m->changes;
	char *vuid = NULL, hash[8];
	const char *uid;
	CamelVeeMessageInfo *vinfo;
	int i, vuidlen = 0;
	CamelFolderChangeInfo *vf_changes = NULL, *unmatched_changes = NULL;
	GPtrArray *matches_added = NULL, /* newly added, that match */
		*matches_changed = NULL, /* newly changed, that now match */
		*newchanged = NULL,
		*changed;
	GPtrArray *always_changed = NULL;
	GHashTable *matches_hash;

	/* Check the folder hasn't beem removed while we weren't watching */
	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);
	if (g_list_find(_PRIVATE(vf)->folders, sub) == NULL) {
		CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);
		return;
	}

	camel_vee_folder_hash_folder(sub, hash);

	/* Lookup anything before we lock anything, to avoid deadlock with build_folder */

	/* Find newly added that match */
	if (changes->uid_added->len > 0) {
		dd(printf(" Searching for added matches '%s'\n", vf->expression));
		matches_added = camel_folder_search_by_uids(sub, vf->expression, changes->uid_added, NULL);
	}

	/* TODO:
	   In this code around here, we can work out if the search will affect the changes
	   we had, and only re-search against them if they might have */

	/* Search for changed items that newly match, but only if we dont have them */
	changed = changes->uid_changed;
	if (changed->len > 0) {
		dd(printf(" Searching for changed matches '%s'\n", vf->expression));

		if ((vf->flags & CAMEL_STORE_VEE_FOLDER_AUTO) == 0) {
			newchanged = g_ptr_array_new();
			always_changed = g_ptr_array_new();
			for (i=0;i<changed->len;i++) {
				uid = changed->pdata[i];
				if (strlen(uid)+9 > vuidlen) {
					vuidlen = strlen(uid)+64;
					vuid = g_realloc(vuid, vuidlen);
				}
				memcpy(vuid, hash, 8);
				strcpy(vuid+8, uid);
				vinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, vuid);
				if (vinfo == NULL) {
					g_ptr_array_add(newchanged, (char *)uid);
				} else {
					g_ptr_array_add(always_changed, (char *)uid);
					camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)vinfo);
				}
			}
			changed = newchanged;
		}

		if (changed->len)
			matches_changed = camel_folder_search_by_uids(sub, vf->expression, changed, NULL);
	}

	CAMEL_VEE_FOLDER_LOCK(vf, summary_lock);
	CAMEL_VEE_FOLDER_LOCK(folder_unmatched, summary_lock);

	dd(printf("Vfolder '%s' subfolder changed '%s'\n", folder->full_name, sub->full_name));
	dd(printf(" changed %d added %d removed %d\n", changes->uid_changed->len, changes->uid_added->len, changes->uid_removed->len));

	/* Always remove removed uid's, in any case */
	for (i=0;i<changes->uid_removed->len;i++) {
		dd(printf("  removing uid '%s'\n", (char *)changes->uid_removed->pdata[i]));
		folder_changed_remove_uid(sub, changes->uid_removed->pdata[i], hash, FALSE, vf);
	}

	/* Add any newly matched or to unmatched folder if they dont */
	if (matches_added) {
		matches_hash = g_hash_table_new(g_str_hash, g_str_equal);
		for (i=0;i<matches_added->len;i++) {
			dd(printf(" %s", (char *)matches_added->pdata[i]));
			g_hash_table_insert(matches_hash, matches_added->pdata[i], matches_added->pdata[i]);
		}
		for (i=0;i<changes->uid_added->len;i++) {
			uid = changes->uid_added->pdata[i];
			if (g_hash_table_lookup(matches_hash, uid)) {
				dd(printf("  adding uid '%s' [newly matched]\n", (char *)uid));
				folder_changed_add_uid(sub, uid, hash, vf);
			} else if ((vf->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0) {
				if (strlen(uid)+9 > vuidlen) {
					vuidlen = strlen(uid)+64;
					vuid = g_realloc(vuid, vuidlen);
				}
				memcpy(vuid, hash, 8);
				strcpy(vuid+8, uid);
				
				if (!CAMEL_IS_VEE_FOLDER(sub) && g_hash_table_lookup(unmatched_uids, vuid) == NULL) {
					dd(printf("  adding uid '%s' to Unmatched [newly unmatched]\n", (char *)uid));
					vinfo = (CamelVeeMessageInfo *)camel_folder_get_message_info((CamelFolder *)folder_unmatched, vuid);
					if (vinfo == NULL) {
						if (vee_folder_add_uid(folder_unmatched, sub, uid, hash))
							camel_folder_change_info_add_uid(folder_unmatched->changes, vuid);
					} else {
						camel_folder_free_message_info((CamelFolder *)folder_unmatched, (CamelMessageInfo *)vinfo);
					}
				}
			}
		}
		g_hash_table_destroy(matches_hash);
	}

	/* Change any newly changed */
	if (always_changed) {
		for (i=0;i<always_changed->len;i++)
			folder_changed_change_uid(sub, always_changed->pdata[i], hash, vf);
		g_ptr_array_free(always_changed, TRUE);
	}

	/* Change/add/remove any changed */
	if (matches_changed) {
		/* If we are auto-updating, then re-check changed uids still match */
		dd(printf(" Vfolder %supdate\nuids match:", (vf->flags & CAMEL_STORE_VEE_FOLDER_AUTO)?"auto-":""));
		matches_hash = g_hash_table_new(g_str_hash, g_str_equal);
		for (i=0;i<matches_changed->len;i++) {
			dd(printf(" %s", (char *)matches_changed->pdata[i]));
			g_hash_table_insert(matches_hash, matches_changed->pdata[i], matches_changed->pdata[i]);
		}
		dd(printf("\n"));
		for (i=0;i<changed->len;i++) {
			uid = changed->pdata[i];
			if (strlen(uid)+9 > vuidlen) {
				vuidlen = strlen(uid)+64;
				vuid = g_realloc(vuid, vuidlen);
			}
			memcpy(vuid, hash, 8);
			strcpy(vuid+8, uid);
			vinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, vuid);
			if (vinfo == NULL) {
				if (g_hash_table_lookup(matches_hash, uid)) {
					/* A uid we dont have, but now it matches, add it */
					dd(printf("  adding uid '%s' [newly matched]\n", uid));
					folder_changed_add_uid(sub, uid, hash, vf);
				} else {
					/* A uid we still don't have, just change it (for unmatched) */
					folder_changed_change_uid(sub, uid, hash, vf);
				}
			} else {
				if ((vf->flags & CAMEL_STORE_VEE_FOLDER_AUTO) == 0
				    || g_hash_table_lookup(matches_hash, uid)) {
					/* still match, or we're not auto-updating, change event, (if it changed) */
					dd(printf("  changing uid '%s' [still matches]\n", uid));
					folder_changed_change_uid(sub, uid, hash, vf);
				} else {
					/* No longer matches, remove it, but keep it in unmatched (potentially) */
					dd(printf("  removing uid '%s' [did match]\n", uid));
					folder_changed_remove_uid(sub, uid, hash, TRUE, vf);
				}
				camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)vinfo);
			}
		}
		g_hash_table_destroy(matches_hash);
	} else {
		/* stuff didn't match but it changed - check unmatched folder for changes */
		for (i=0;i<changed->len;i++)
			folder_changed_change_uid(sub, changed->pdata[i], hash, vf);
	}

	if (camel_folder_change_info_changed(folder_unmatched->changes)) {
		unmatched_changes = folder_unmatched->changes;
		folder_unmatched->changes = camel_folder_change_info_new();
	}
		
	if (camel_folder_change_info_changed(vf->changes)) {
		vf_changes = vf->changes;
		vf->changes = camel_folder_change_info_new();
	}

	CAMEL_VEE_FOLDER_UNLOCK(folder_unmatched, summary_lock);
	CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);

	/* Cleanup stuff on our folder */
	if (matches_added)
		camel_folder_search_free(sub, matches_added);

	if (matches_changed)
		camel_folder_search_free(sub, matches_changed);

	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);

	/* cleanup the rest */
	if (newchanged)
		g_ptr_array_free(newchanged, TRUE);

	g_free(vuid);

	if (unmatched_changes) {
		camel_object_trigger_event((CamelObject *)folder_unmatched, "folder_changed", unmatched_changes);
		camel_folder_change_info_free(unmatched_changes);
	}
	
	if (vf_changes) {
		/* If not auto-updating, keep track of changed folders for later re-sync */
		if ((vf->flags & CAMEL_STORE_VEE_FOLDER_AUTO) == 0) {
			CAMEL_VEE_FOLDER_LOCK(vf, changed_lock);
			if (g_list_find(vf->priv->folders_changed, sub) != NULL)
				vf->priv->folders_changed = g_list_prepend(vf->priv->folders_changed, sub);
			CAMEL_VEE_FOLDER_UNLOCK(vf, changed_lock);
		}

		camel_object_trigger_event((CamelObject *)vf, "folder_changed", vf_changes);
		camel_folder_change_info_free(vf_changes);
	}
}

static void
folder_changed_free(CamelSession *session, CamelSessionThreadMsg *msg)
{
	struct _folder_changed_msg *m = (struct _folder_changed_msg *)msg;

	camel_folder_change_info_free(m->changes);
	camel_object_unref((CamelObject *)m->vf);
	camel_object_unref((CamelObject *)m->sub);
}

static CamelSessionThreadOps folder_changed_ops = {
	folder_changed_change,
	folder_changed_free,
};

static void
folder_changed(CamelFolder *sub, CamelFolderChangeInfo *changes, CamelVeeFolder *vf)
{
	struct _folder_changed_msg *m;
	CamelSession *session = ((CamelService *)((CamelFolder *)vf)->parent_store)->session;
	
	m = camel_session_thread_msg_new(session, &folder_changed_ops, sizeof(*m));
	m->changes = camel_folder_change_info_new();
	camel_folder_change_info_cat(m->changes, changes);
	m->sub = sub;
	camel_object_ref((CamelObject *)sub);
	m->vf = vf;
	camel_object_ref((CamelObject *)vf);
	camel_session_thread_queue(session, &m->msg, 0);
}

/* track vanishing folders */
static void
subfolder_deleted(CamelFolder *f, void *event_data, CamelVeeFolder *vf)
{
	camel_vee_folder_remove_folder(vf, f);
}

static void
subfolder_renamed_update(CamelVeeFolder *vf, CamelFolder *sub, char hash[8])
{
	int count, i;
	CamelFolderChangeInfo *changes = NULL;

	CAMEL_VEE_FOLDER_LOCK(vf, summary_lock);

	count = camel_folder_summary_count(((CamelFolder *)vf)->summary);
	for (i=0;i<count;i++) {
		CamelVeeMessageInfo *mi = (CamelVeeMessageInfo *)camel_folder_summary_index(((CamelFolder *)vf)->summary, i);
		CamelVeeMessageInfo *vinfo;

		if (mi == NULL)
			continue;

		if (mi->folder == sub) {
			char *uid = (char *)camel_message_info_uid(mi);
			char *oldkey;
			void *oldval;

			camel_folder_change_info_remove_uid(vf->changes, uid);
			camel_folder_summary_remove(((CamelFolder *)vf)->summary, (CamelMessageInfo *)mi);

			/* works since we always append on the end */
			i--;
			count--;

			vinfo = vee_folder_add_uid(vf, sub, uid+8, hash);
			if (vinfo)
				camel_folder_change_info_add_uid(vf->changes, camel_message_info_uid(vinfo));

			/* check unmatched uid's table for any matches */
			if (vf == folder_unmatched
			    && g_hash_table_lookup_extended(unmatched_uids, uid, (void **)&oldkey, &oldval)) {
				g_hash_table_remove(unmatched_uids, oldkey);
				g_hash_table_insert(unmatched_uids, g_strdup(camel_message_info_uid(vinfo)), oldval);
				g_free(oldkey);
			}
		}

		camel_folder_summary_info_free(((CamelFolder *)vf)->summary, (CamelMessageInfo *)mi);
	}

	if (camel_folder_change_info_changed(vf->changes)) {
		changes = vf->changes;
		vf->changes = camel_folder_change_info_new();
	}

	CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);

	if (changes) {
		camel_object_trigger_event((CamelObject *)vf, "folder_changed", changes);
		camel_folder_change_info_free(changes);
	}
}

static void
subfolder_renamed(CamelFolder *f, void *event_data, CamelVeeFolder *vf)
{
	char hash[8];

	/* TODO: This could probably be done in another thread, tho it is pretty quick/memory bound */

	/* Life just got that little bit harder, if the folder is renamed, it means it breaks all of our uid's.
	   We need to remove the old uid's, fix them up, then release the new uid's, for the uid's that match this folder */

	camel_vee_folder_hash_folder(f, hash);

	subfolder_renamed_update(vf, f, hash);
	subfolder_renamed_update(folder_unmatched, f, hash);
}

static void
vee_freeze (CamelFolder *folder)
{
	CamelVeeFolder *vfolder = (CamelVeeFolder *)folder;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vfolder);
	GList *node;
	
	CAMEL_VEE_FOLDER_LOCK(vfolder, subfolder_lock);
	
	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;
		
		camel_folder_freeze(f);
		node = node->next;
	}
	
	CAMEL_VEE_FOLDER_UNLOCK(vfolder, subfolder_lock);
	
	/* call parent implementation */
	CAMEL_FOLDER_CLASS (camel_vee_folder_parent)->freeze(folder);
}

static void
vee_thaw(CamelFolder *folder)
{
	CamelVeeFolder *vfolder = (CamelVeeFolder *)folder;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vfolder);
	GList *node;
	
	CAMEL_VEE_FOLDER_LOCK(vfolder, subfolder_lock);
	
	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;
		
		camel_folder_thaw(f);
		node = node->next;
	}
	
	CAMEL_VEE_FOLDER_UNLOCK(vfolder, subfolder_lock);
	
	/* call parent implementation */
	CAMEL_FOLDER_CLASS (camel_vee_folder_parent)->thaw(folder);
}
