/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *	     Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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
 *
 */

#include <config.h>

#include "camel-exception.h"
#include "camel-vtrash-folder.h"
#include "camel-store.h"
#include "camel-vee-store.h"
#include "camel-mime-message.h"
#include "camel-i18n.h"
#include "camel-private.h"

#include <string.h>

/* Returns the class for a CamelFolder */
#define CF_CLASS(so) ((CamelFolderClass *)((CamelObject *)(so))->klass)

static struct {
	const char *name;
	const char *expr;
	guint32 bit;
	guint32 flags;
	const char *error_copy;
} vdata[] = {
	{ CAMEL_VTRASH_NAME, "(match-all (system-flag \"Deleted\"))", CAMEL_MESSAGE_DELETED, CAMEL_FOLDER_IS_TRASH,
	  N_("Cannot copy messages to the Trash folder") },
	{ CAMEL_VJUNK_NAME, "(match-all (system-flag \"Junk\"))", CAMEL_MESSAGE_JUNK, CAMEL_FOLDER_IS_JUNK,
	  N_("Cannot copy messages to the Junk folder") },
};

static CamelVeeFolderClass *camel_vtrash_folder_parent;

static void camel_vtrash_folder_class_init (CamelVTrashFolderClass *klass);

static void
camel_vtrash_folder_init (CamelVTrashFolder *vtrash)
{
	/*CamelFolder *folder = CAMEL_FOLDER (vtrash);*/
}

CamelType
camel_vtrash_folder_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_vee_folder_get_type (),
					    "CamelVTrashFolder",
					    sizeof (CamelVTrashFolder),
					    sizeof (CamelVTrashFolderClass),
					    (CamelObjectClassInitFunc) camel_vtrash_folder_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_vtrash_folder_init,
					    NULL);
	}
	
	return type;
}

/**
 * camel_vtrash_folder_new:
 * @parent_store: the parent CamelVeeStore
 * @type: type of vfolder, CAMEL_VTRASH_FOLDER_TRASH or CAMEL_VTRASH_FOLDER_JUNK currently.
 * @ex: a CamelException
 *
 * Create a new CamelVeeFolder object.
 *
 * Return value: A new CamelVeeFolder widget.
 **/
CamelFolder *
camel_vtrash_folder_new (CamelStore *parent_store, enum _camel_vtrash_folder_t type)
{
	CamelVTrashFolder *vtrash;
	
	g_assert(type < CAMEL_VTRASH_FOLDER_LAST);

	vtrash = (CamelVTrashFolder *)camel_object_new(camel_vtrash_folder_get_type());
	camel_vee_folder_construct(CAMEL_VEE_FOLDER (vtrash), parent_store, vdata[type].name,
				   CAMEL_STORE_FOLDER_PRIVATE|CAMEL_STORE_FOLDER_CREATE|CAMEL_STORE_VEE_FOLDER_AUTO);

	((CamelFolder *)vtrash)->folder_flags |= vdata[type].flags;
	camel_vee_folder_set_expression((CamelVeeFolder *)vtrash, vdata[type].expr);
	vtrash->bit = vdata[type].bit;
	vtrash->type = type;

	return (CamelFolder *)vtrash;
}

static int
vtrash_getv(CamelObject *object, CamelException *ex, CamelArgGetV *args)
{
	CamelFolder *folder = (CamelFolder *)object;
	int i;
	guint32 tag;
	int unread = -1, deleted = 0, junked = 0, visible = 0, count = -1;

	for (i=0;i<args->argc;i++) {
		CamelArgGet *arg = &args->argv[i];

		tag = arg->tag;

		/* NB: this is a copy of camel-folder.c with the unread count logic altered.
		   makes sure its still atomically calculated */
		switch (tag & CAMEL_ARG_TAG) {
		case CAMEL_FOLDER_ARG_UNREAD:
		case CAMEL_FOLDER_ARG_DELETED:
		case CAMEL_FOLDER_ARG_JUNKED:
		case CAMEL_FOLDER_ARG_VISIBLE:
			/* This is so we can get the values atomically, and also so we can calculate them only once */
			if (unread == -1) {
				int j;
				CamelMessageInfo *info;

				unread = 0;
				count = camel_folder_summary_count(folder->summary);
				for (j=0; j<count; j++) {
					if ((info = camel_folder_summary_index(folder->summary, j))) {
						guint32 flags = camel_message_info_flags(info);

						if ((flags & (CAMEL_MESSAGE_SEEN)) == 0)
							unread++;
						if (flags & CAMEL_MESSAGE_DELETED)
							deleted++;
						if (flags & CAMEL_MESSAGE_JUNK)
							junked++;
						if ((flags & (CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_JUNK)) == 0)
							visible++;
						camel_message_info_free(info);
					}
				}
			}

			switch (tag & CAMEL_ARG_TAG) {
			case CAMEL_FOLDER_ARG_UNREAD:
				count = unread;
				break;
			case CAMEL_FOLDER_ARG_DELETED:
				count = deleted;
				break;
			case CAMEL_FOLDER_ARG_JUNKED:
				count = junked;
				break;
			case CAMEL_FOLDER_ARG_VISIBLE:
				count = visible;
				break;
			}

			*arg->ca_int = count;
			break;
		default:
			continue;
		}

		arg->tag = (tag & CAMEL_ARG_TYPE) | CAMEL_ARG_IGNORE;
	}

	return ((CamelObjectClass *)camel_vtrash_folder_parent)->getv(object, ex, args);
}

static void
vtrash_append_message (CamelFolder *folder, CamelMimeMessage *message,
		       const CamelMessageInfo *info, char **appended_uid,
		       CamelException *ex)
{
	camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, 
			     _(vdata[((CamelVTrashFolder *)folder)->type].error_copy));
}

struct _transfer_data {
	CamelFolder *folder;
	CamelFolder *dest;
	GPtrArray *uids;
	gboolean delete;
};

static void
transfer_messages(CamelFolder *folder, struct _transfer_data *md, CamelException *ex)
{
	int i;

	if (!camel_exception_is_set (ex))
		camel_folder_transfer_messages_to(md->folder, md->uids, md->dest, NULL, md->delete, ex);

	for (i=0;i<md->uids->len;i++)
		g_free(md->uids->pdata[i]);
	g_ptr_array_free(md->uids, TRUE);
	camel_object_unref((CamelObject *)md->folder);
	g_free(md);
}

static void
vtrash_transfer_messages_to (CamelFolder *source, GPtrArray *uids,
			     CamelFolder *dest, GPtrArray **transferred_uids,
			     gboolean delete_originals, CamelException *ex)
{
	CamelVeeMessageInfo *mi;
	int i;
	GHashTable *batch = NULL;
	const char *tuid;
	struct _transfer_data *md;
	guint32 sbit = ((CamelVTrashFolder *)source)->bit;

	/* This is a special case of transfer_messages_to: Either the
	 * source or the destination is a vtrash folder (but not both
	 * since a store should never have more than one).
	 */

	if (transferred_uids)
		*transferred_uids = NULL;

	if (CAMEL_IS_VTRASH_FOLDER (dest)) {
		/* Copy to trash is meaningless. */
		if (!delete_originals) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, 
					     _(vdata[((CamelVTrashFolder *)dest)->type].error_copy));
			return;
		}

		/* Move to trash is the same as setting the message flag */
		for (i = 0; i < uids->len; i++)
			camel_folder_set_message_flags(source, uids->pdata[i], ((CamelVTrashFolder *)dest)->bit, ~0);
		return;
	}

	/* Moving/Copying from the trash to the original folder = undelete.
	 * Moving/Copying from the trash to a different folder = move/copy.
	 *
	 * Need to check this uid by uid, but we batch up the copies.
	 */

	for (i = 0; i < uids->len; i++) {
		mi = (CamelVeeMessageInfo *)camel_folder_get_message_info (source, uids->pdata[i]);
		if (mi == NULL) {
			g_warning ("Cannot find uid %s in source folder during transfer", (char *) uids->pdata[i]);
			continue;
		}
		
		if (dest == mi->real->summary->folder) {
			/* Just unset the flag on the original message */
			camel_folder_set_message_flags (source, uids->pdata[i], sbit, 0);
		} else {
			if (batch == NULL)
				batch = g_hash_table_new(NULL, NULL);
			md = g_hash_table_lookup(batch, mi->real->summary->folder);
			if (md == NULL) {
				md = g_malloc0(sizeof(*md));
				md->folder = mi->real->summary->folder;
				camel_object_ref((CamelObject *)md->folder);
				md->uids = g_ptr_array_new();
				md->dest = dest;
				g_hash_table_insert(batch, mi->real->summary->folder, md);
			}

			tuid = uids->pdata[i];
			if (strlen(tuid)>8)
				tuid += 8;
			g_ptr_array_add(md->uids, g_strdup(tuid));
		}
		camel_folder_free_message_info (source, (CamelMessageInfo *)mi);
	}

	if (batch) {
		g_hash_table_foreach(batch, (GHFunc)transfer_messages, ex);
		g_hash_table_destroy(batch);
	}
}

static GPtrArray *
vtrash_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex)
{
	GList *node;
	GPtrArray *matches, *result = g_ptr_array_new(), *uids = g_ptr_array_new();
	struct _CamelVeeFolderPrivate *p = ((CamelVeeFolder *)folder)->priv;

	/* we optimise the search by only searching for messages which we have anyway */
	CAMEL_VEE_FOLDER_LOCK(folder, subfolder_lock);
	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;
		int i;
		char hash[8];
		GPtrArray *infos = camel_folder_get_summary(f);

		camel_vee_folder_hash_folder(f, hash);

		for (i=0;i<infos->len;i++) {
			CamelMessageInfo *mi = infos->pdata[i];

			if (camel_message_info_flags(mi) & ((CamelVTrashFolder *)folder)->bit)
				g_ptr_array_add(uids, (void *)camel_message_info_uid(mi));
		}

		if (uids->len > 0
		    && (matches = camel_folder_search_by_uids(f, expression, uids, NULL))) {
			for (i = 0; i < matches->len; i++) {
				char *uid = matches->pdata[i], *vuid;

				vuid = g_malloc(strlen(uid)+9);
				memcpy(vuid, hash, 8);
				strcpy(vuid+8, uid);
				g_ptr_array_add(result, vuid);
			}
			camel_folder_search_free(f, matches);
		}
		g_ptr_array_set_size(uids, 0);
		camel_folder_free_summary(f, infos);

		node = g_list_next(node);
	}
	CAMEL_VEE_FOLDER_UNLOCK(folder, subfolder_lock);

	g_ptr_array_free(uids, TRUE);

	return result;
}

static GPtrArray *
vtrash_search_by_uids(CamelFolder *folder, const char *expression, GPtrArray *uids, CamelException *ex)
{
	GList *node;
	GPtrArray *matches, *result = g_ptr_array_new(), *folder_uids = g_ptr_array_new();
	struct _CamelVeeFolderPrivate *p = ((CamelVeeFolder *)folder)->priv;
	
	CAMEL_VEE_FOLDER_LOCK(folder, subfolder_lock);

	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;
		int i;
		char hash[8];
		
		camel_vee_folder_hash_folder(f, hash);

		/* map the vfolder uid's to the source folder uid's first */
		g_ptr_array_set_size(uids, 0);
		for (i=0;i<uids->len;i++) {
			char *uid = uids->pdata[i];
			
			if (strlen(uid) >= 8 && strncmp(uid, hash, 8) == 0) {
				CamelMessageInfo *mi;

				mi = camel_folder_get_message_info(f, uid+8);
				if (mi) {
					if(camel_message_info_flags(mi) & ((CamelVTrashFolder *)folder)->bit)
						g_ptr_array_add(folder_uids, uid+8);
					camel_folder_free_message_info(f, mi);
				}
			}
		}

		if (folder_uids->len > 0
		    && (matches = camel_folder_search_by_uids(f, expression, folder_uids, ex))) {
			for (i = 0; i < matches->len; i++) {
				char *uid = matches->pdata[i], *vuid;
				
				vuid = g_malloc(strlen(uid)+9);
				memcpy(vuid, hash, 8);
				strcpy(vuid+8, uid);
				g_ptr_array_add(result, vuid);
			}
			camel_folder_search_free(f, matches);
		}
		node = g_list_next(node);
	}

	CAMEL_VEE_FOLDER_UNLOCK(folder, subfolder_lock);

	g_ptr_array_free(folder_uids, TRUE);

	return result;
}

static void
vtrash_uid_removed(CamelVTrashFolder *vf, const char *uid, char hash[8])
{
	char *vuid;
	CamelVeeMessageInfo *vinfo;

	vuid = g_alloca(strlen(uid)+9);
	memcpy(vuid, hash, 8);
	strcpy(vuid+8, uid);
	vinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(((CamelFolder *)vf)->summary, vuid);
	if (vinfo) {
		camel_folder_change_info_remove_uid(((CamelVeeFolder *)vf)->changes, vuid);
		camel_folder_summary_remove(((CamelFolder *)vf)->summary, (CamelMessageInfo *)vinfo);
		camel_message_info_free(vinfo);
	}
}

static void
vtrash_uid_added(CamelVTrashFolder *vf, const char *uid, CamelMessageInfo *info, char hash[8])
{
	char *vuid;
	CamelVeeMessageInfo *vinfo;

	vuid = g_alloca(strlen(uid)+9);
	memcpy(vuid, hash, 8);
	strcpy(vuid+8, uid);
	vinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(((CamelFolder *)vf)->summary, vuid);
	if (vinfo == NULL) {
		camel_vee_summary_add((CamelVeeSummary *)((CamelFolder *)vf)->summary, info, hash);
		camel_folder_change_info_add_uid(((CamelVeeFolder *)vf)->changes, vuid);
	} else {
		camel_folder_change_info_change_uid(((CamelVeeFolder *)vf)->changes, vuid);
		camel_message_info_free(vinfo);
	}
}

static void
vtrash_folder_changed(CamelVeeFolder *vf, CamelFolder *sub, CamelFolderChangeInfo *changes)
{
	CamelMessageInfo *info;
	char hash[8];
	CamelFolderChangeInfo *vf_changes = NULL;
	int i;

	camel_vee_folder_hash_folder(sub, hash);

	CAMEL_VEE_FOLDER_LOCK(vf, summary_lock);

	/* remove any removed that we also have */
	for (i=0;i<changes->uid_removed->len;i++)
		vtrash_uid_removed((CamelVTrashFolder *)vf, (const char *)changes->uid_removed->pdata[i], hash);

	/* check any changed still deleted/junked */
	for (i=0;i<changes->uid_changed->len;i++) {
		const char *uid = changes->uid_changed->pdata[i];

		info = camel_folder_get_message_info(sub, uid);
		if (info == NULL)
			continue;

		if ((camel_message_info_flags(info) & ((CamelVTrashFolder *)vf)->bit) == 0)
			vtrash_uid_removed((CamelVTrashFolder *)vf, uid, hash);
		else
			vtrash_uid_added((CamelVTrashFolder *)vf, uid, info, hash);

		camel_message_info_free(info);
	}

	/* add any new ones which are already matching */
	for (i=0;i<changes->uid_added->len;i++) {
		const char *uid = changes->uid_added->pdata[i];

		info = camel_folder_get_message_info(sub, uid);
		if (info == NULL)
			continue;

		if ((camel_message_info_flags(info) & ((CamelVTrashFolder *)vf)->bit) != 0)
			vtrash_uid_added((CamelVTrashFolder *)vf, uid, info, hash);

		camel_message_info_free(info);
	}

	if (camel_folder_change_info_changed(((CamelVeeFolder *)vf)->changes)) {
		vf_changes = ((CamelVeeFolder *)vf)->changes;
		((CamelVeeFolder *)vf)->changes = camel_folder_change_info_new();
	}

	CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);
	
	if (vf_changes) {
		camel_object_trigger_event(vf, "folder_changed", vf_changes);
		camel_folder_change_info_free(vf_changes);
	}
}

static void
vtrash_add_folder(CamelVeeFolder *vf, CamelFolder *sub)
{
	GPtrArray *infos;
	int i;
	char hash[8];
	CamelFolderChangeInfo *vf_changes = NULL;

	camel_vee_folder_hash_folder(sub, hash);

	CAMEL_VEE_FOLDER_LOCK(vf, summary_lock);

	infos = camel_folder_get_summary(sub);
	for (i=0;i<infos->len;i++) {
		CamelMessageInfo *info = infos->pdata[i];

		if ((camel_message_info_flags(info) & ((CamelVTrashFolder *)vf)->bit))
			vtrash_uid_added((CamelVTrashFolder *)vf, camel_message_info_uid(info), info, hash);
	}
	camel_folder_free_summary(sub, infos);

	if (camel_folder_change_info_changed(vf->changes)) {
		vf_changes = vf->changes;
		vf->changes = camel_folder_change_info_new();
	}

	CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);

	if (vf_changes) {
		camel_object_trigger_event(vf, "folder_changed", vf_changes);
		camel_folder_change_info_free(vf_changes);
	}
}

static void
vtrash_remove_folder(CamelVeeFolder *vf, CamelFolder *sub)
{
	GPtrArray *infos;
	int i;
	char hash[8];
	CamelFolderChangeInfo *vf_changes = NULL;
	CamelFolderSummary *ssummary = sub->summary;
	int start, last;

	camel_vee_folder_hash_folder(sub, hash);

	CAMEL_VEE_FOLDER_LOCK(vf, summary_lock);

	start = -1;
	last = -1;
	infos = camel_folder_get_summary(sub);
	for (i=0;i<infos->len;i++) {
		CamelVeeMessageInfo *mi = infos->pdata[i];

		if (mi == NULL)
			continue;

		if (mi->real->summary == ssummary) {
			const char *uid = camel_message_info_uid(mi);

			camel_folder_change_info_remove_uid(vf->changes, uid);

			if (last == -1) {
				last = start = i;
			} else if (last+1 == i) {
				last = i;
			} else {
				camel_folder_summary_remove_range(((CamelFolder *)vf)->summary, start, last);
				i -= (last-start)+1;
				start = last = i;
			}
		}
	}
	camel_folder_free_summary(sub, infos);

	if (last != -1)
		camel_folder_summary_remove_range(((CamelFolder *)vf)->summary, start, last);

	if (camel_folder_change_info_changed(vf->changes)) {
		vf_changes = vf->changes;
		vf->changes = camel_folder_change_info_new();
	}

	CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);

	if (vf_changes) {
		camel_object_trigger_event(vf, "folder_changed", vf_changes);
		camel_folder_change_info_free(vf_changes);
	}
}

static int
vtrash_rebuild_folder(CamelVeeFolder *vf, CamelFolder *source, CamelException *ex)
{
	/* we should always be in sync */
	return 0;
}

static void
camel_vtrash_folder_class_init (CamelVTrashFolderClass *klass)
{
	CamelFolderClass *folder_class = (CamelFolderClass *) klass;
	
	camel_vtrash_folder_parent = CAMEL_VEE_FOLDER_CLASS(camel_vee_folder_get_type());

	((CamelObjectClass *)klass)->getv = vtrash_getv;
	
	folder_class->append_message = vtrash_append_message;
	folder_class->transfer_messages_to = vtrash_transfer_messages_to;
	folder_class->search_by_expression = vtrash_search_by_expression;
	folder_class->search_by_uids = vtrash_search_by_uids;

	((CamelVeeFolderClass *)klass)->add_folder = vtrash_add_folder;
	((CamelVeeFolderClass *)klass)->remove_folder = vtrash_remove_folder;
	((CamelVeeFolderClass *)klass)->rebuild_folder = vtrash_rebuild_folder;

	((CamelVeeFolderClass *)klass)->folder_changed = vtrash_folder_changed;
}
