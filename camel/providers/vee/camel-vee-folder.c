/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *           Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

#include "camel-exception.h"
#include "camel-vee-folder.h"
#include "camel-store.h"
#include "camel-folder-summary.h"
#include "camel-mime-message.h"
#include "camel-folder-search.h"

#include "camel-vee-store.h"	/* for open flags */
#include "camel-vee-private.h"

#ifdef DOESTRV
#include "e-util/e-memory.h"
#endif

#include <string.h>

#define d(x)

/* our message info includes the parent folder */
typedef struct _CamelVeeMessageInfo {
	CamelMessageInfo info;
	CamelFolder *folder;
} CamelVeeMessageInfo;

#define _PRIVATE(o) (((CamelVeeFolder *)(o))->priv)

static void vee_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
static void vee_expunge (CamelFolder *folder, CamelException *ex);

static CamelMimeMessage *vee_get_message (CamelFolder *folder, const gchar *uid, CamelException *ex);
static void vee_move_message_to(CamelFolder *source, const char *uid, CamelFolder *dest, CamelException *ex);

static GPtrArray *vee_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex);

static void vee_set_message_flags (CamelFolder *folder, const char *uid, guint32 flags, guint32 set);
static void vee_set_message_user_flag (CamelFolder *folder, const char *uid, const char *name, gboolean value);

static void camel_vee_folder_class_init (CamelVeeFolderClass *klass);
static void camel_vee_folder_init       (CamelVeeFolder *obj);
static void camel_vee_folder_finalise   (CamelObject *obj);

static void unmatched_finalise(CamelFolder *sub, gpointer type, CamelVeeFolder *vf);

static void folder_changed(CamelFolder *sub, gpointer type, CamelVeeFolder *vf);
static void message_changed(CamelFolder *f, const char *uid, CamelVeeFolder *vf);

static void vee_folder_build(CamelVeeFolder *vf, CamelException *ex);
static void vee_folder_build_folder(CamelVeeFolder *vf, CamelFolder *source, CamelException *ex);

static CamelFolderClass *camel_vee_folder_parent;

/* a vfolder for unmatched messages */
static CamelVeeFolder *folder_unmatched;
static GHashTable *unmatched_uids;
#ifdef ENABLE_THREADS
#include <pthread.h>
static pthread_mutex_t unmatched_lock = PTHREAD_MUTEX_INITIALIZER;
#define UNMATCHED_LOCK() pthread_mutex_lock(&unmatched_lock)
#define UNMATCHED_UNLOCK() pthread_mutex_unlock(&unmatched_lock)
#else
#define UNMATCHED_LOCK()
#define UNMATCHED_UNLOCK()
#endif

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

	folder_class->sync = vee_sync;
	folder_class->expunge = vee_expunge;

	folder_class->get_message = vee_get_message;
	folder_class->move_message_to = vee_move_message_to;

	folder_class->search_by_expression = vee_search_by_expression;

	folder_class->set_message_flags = vee_set_message_flags;
	folder_class->set_message_user_flag = vee_set_message_user_flag;
}

static void
camel_vee_folder_init (CamelVeeFolder *obj)
{
	struct _CamelVeeFolderPrivate *p;
	CamelFolder *folder = (CamelFolder *)obj;

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));

	folder->has_summary_capability = TRUE;
	folder->has_search_capability = TRUE;

	/* FIXME: what to do about user flags if the subfolder doesn't support them? */
	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED |
		CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT |
		CAMEL_MESSAGE_FLAGGED |
		CAMEL_MESSAGE_SEEN;

	obj->changes = camel_folder_change_info_new();
	obj->search = camel_folder_search_new();

#ifdef ENABLE_THREADS
	p->summary_lock = g_mutex_new();
	p->subfolder_lock = g_mutex_new();
#endif

}

static void
camel_vee_folder_finalise (CamelObject *obj)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)obj;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	/* FIXME: some leaks here, summary etc */

	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;
		camel_object_unhook_event ((CamelObject *)f, "folder_changed", (CamelObjectEventHookFunc) folder_changed, vf);
		camel_object_unhook_event ((CamelObject *)f, "message_changed", (CamelObjectEventHookFunc) message_changed, vf);
		camel_object_unref((CamelObject *)f);
		node = g_list_next(node);
	}

	g_free(vf->expression);
	g_free(vf->vname);

	camel_folder_change_info_free(vf->changes);
	camel_object_unref((CamelObject *)vf->search);

#ifdef ENABLE_THREADS
	g_mutex_free(p->summary_lock);
	g_mutex_free(p->subfolder_lock);
#endif
	g_free(p);
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
camel_vee_folder_new(CamelStore *parent_store, const char *name, guint32 flags, CamelException *ex)
{
	CamelFolderInfo *fi;
	CamelFolder *folder;
	CamelVeeFolder *vf;
	char *namepart, *searchpart;

	namepart = g_strdup(name);
	searchpart = strchr(namepart, '?');
	if (searchpart == NULL) {
		/* no search, no result! */
		searchpart = "(body-contains \"=some-invalid_string-sequence=xx\")";
	} else {
		*searchpart++ = 0;
	}

	UNMATCHED_LOCK();

	if (folder_unmatched == NULL) {
		printf("setting up unmatched folder\n");
		unmatched_uids = g_hash_table_new(g_str_hash, g_str_equal);

		folder = (CamelFolder *)camel_object_new(camel_vee_folder_get_type());
		folder_unmatched = vf = (CamelVeeFolder *)folder;
		camel_folder_construct(folder, parent_store, "UNMATCHED", "UNMATCHED");
		folder->summary = camel_folder_summary_new();
		folder->summary->message_info_size = sizeof(CamelVeeMessageInfo);

		vf->expression = g_strdup("(header-contains \"subject\" \"--= in =-=-=+ valid , ., .l\")");
		vf->vname = g_strdup("UNMATCHED");
	}

	UNMATCHED_UNLOCK();

	printf("opening vee folder %s\n", name);
	if (strcmp(namepart, "UNMATCHED") == 0) {
		camel_object_ref((CamelObject *)folder_unmatched);
		g_free(namepart);
		printf("opened UNMATCHED folder %p %s with %d messages\n", folder_unmatched, name, camel_folder_get_message_count((CamelFolder *)folder_unmatched));
		return (CamelFolder *)folder_unmatched;
	}


	folder =  CAMEL_FOLDER (camel_object_new (camel_vee_folder_get_type()));
	vf = (CamelVeeFolder *)folder;
	vf->flags = flags;

	/* remove folders as they vanish */
	camel_object_hook_event((CamelObject *)vf, "finalize", (CamelObjectEventHookFunc)unmatched_finalise, folder_unmatched);

	camel_folder_construct (folder, parent_store, namepart, namepart);

	folder->summary = camel_folder_summary_new();
	folder->summary->message_info_size = sizeof(CamelVeeMessageInfo);

	vf->expression = g_strdup(searchpart);
	vf->vname = namepart;

	vee_folder_build(vf, ex);
	if (camel_exception_is_set(ex)) {
		printf("opening folder failed\n");
		camel_object_unref((CamelObject *)folder);
		return NULL;
	}
	
	printf("opened normal folder folder %p %s with %d messages\n", folder, name, camel_folder_get_message_count(folder));

	/* FIXME: should be moved to store */
	fi = g_new0(CamelFolderInfo, 1);
	fi->full_name = g_strdup(name);
	fi->name = g_strdup(name);
	fi->url = g_strdup_printf("vfolder:%s?%s", vf->vname, vf->expression);
	fi->unread_message_count = -1;
	
	camel_object_trigger_event(CAMEL_OBJECT(parent_store), "folder_created", fi);
	camel_folder_info_free (fi);

	return folder;
}

static CamelVeeMessageInfo * vee_folder_add_uid(CamelVeeFolder *vf, CamelFolder *f, const char *inuid);

/* must be called with summary_lock held */
static void
unmatched_uid_remove(const char *uidin, CamelFolder *source)
{
	char *oldkey, *uid;
	int n;

	uid = g_strdup_printf("%p:%s", source, uidin);
	
	/*printf("checking unmatched uid (remove from source) %s\n", uid);*/

	UNMATCHED_LOCK();

	if (g_hash_table_lookup_extended(unmatched_uids, uid, (void **)&oldkey, (void **)&n)) {
		if (n == 1) {
			/*printf("lost all matches, adding uid to unmatched\n");*/
			if (vee_folder_add_uid(folder_unmatched, source, oldkey))
				camel_folder_change_info_add_uid(folder_unmatched->changes, uid);
			g_hash_table_remove(unmatched_uids, oldkey);
			g_free(oldkey);
		} else
			g_hash_table_insert(unmatched_uids, oldkey, (void *)n-1);
	} else {
		/*printf("unknown uid, adding to unmatched\n");*/
		/* FIXME: lookup to see if we already have it first, to save doing it later */
		if (vee_folder_add_uid(folder_unmatched, source, uidin))
			camel_folder_change_info_add_uid(folder_unmatched->changes, uid);
	}

	UNMATCHED_UNLOCK();

	g_free(uid);
}

/* add a uid to the unmatched folder if it is unmatched everywhere else */
static void
unmatched_uid_check(const char *uidin, CamelFolder *source)
{
	char *oldkey, *uid;
	int n;

	uid = g_strdup_printf("%p:%s", source, uidin);
	
	/*printf("checking unmatched uid (remove from source) %s\n", uid);*/

	UNMATCHED_LOCK();

	if (!g_hash_table_lookup_extended(unmatched_uids, uid, (void **)&oldkey, (void **)&n)) {
		/*printf("unknown uid, adding to unmatched\n");*/
		/* FIXME: lookup to see if we already have it first, to save doing it later */
		if (vee_folder_add_uid(folder_unmatched, source, uidin))
			camel_folder_change_info_add_uid(folder_unmatched->changes, uid);
	}

	UNMATCHED_UNLOCK();

	g_free(uid);
}

/* must be called with summary_lock held */
static void
unmatched_uid_add(const char *uidin, CamelFolder *source)
{
	char *oldkey, *uid;
	int n;
	CamelMessageInfo *info;

	uid = g_strdup_printf("%p:%s", source, uidin);

	/*printf("checking unmatched uid (added to source) %s\n", uid);*/

	UNMATCHED_LOCK();

	info = camel_folder_summary_uid(((CamelFolder *)folder_unmatched)->summary, uid);
	if (info) {
		/*printf("we have it, lets remove it\n");*/
		camel_folder_summary_remove_uid(((CamelFolder *)folder_unmatched)->summary, uid);
		camel_folder_change_info_remove_uid(folder_unmatched->changes, uid);
		camel_folder_summary_info_free(((CamelFolder *)folder_unmatched)->summary, info);
	}

	if (g_hash_table_lookup_extended(unmatched_uids, uid, (void **)&oldkey, (void **)&n)) {
		g_hash_table_insert(unmatched_uids, oldkey, (void **)n+1);
		g_free(uid);
	} else
		g_hash_table_insert(unmatched_uids, uid, (void **)1);

	UNMATCHED_UNLOCK();
}

/* must be called with summary_lock held */
static CamelVeeMessageInfo *
vee_folder_add(CamelVeeFolder *vf, CamelFolder *f, CamelMessageInfo *info)
{
	CamelVeeMessageInfo *mi;
	char *uid;
	CamelFolder *folder = (CamelFolder *)vf;
	CamelMessageInfo *dinfo;

	uid = g_strdup_printf("%p:%s", f, camel_message_info_uid(info));
	/* FIXME: Has races */
	dinfo = camel_folder_summary_uid(folder->summary, uid);
	if (dinfo) {
		g_free(uid);
		camel_folder_summary_info_free(folder->summary, dinfo);
		return NULL;
	}

	mi = (CamelVeeMessageInfo *)camel_folder_summary_info_new(folder->summary);
	camel_message_info_dup_to(info, (CamelMessageInfo *)mi);
#ifdef DOESTRV
	mi->info.strings = e_strv_set_ref_free(mi->info.strings, CAMEL_MESSAGE_INFO_UID, uid);
	mi->info.strings = e_strv_pack(mi->info.strings);
#else	
	g_free(mi->info.uid);
	mi->info.uid = uid;
#endif		
	mi->folder = f;
	camel_folder_summary_add(folder->summary, (CamelMessageInfo *)mi);

	return mi;
}

/* must be called with summary_lock held */
static CamelVeeMessageInfo *
vee_folder_add_uid(CamelVeeFolder *vf, CamelFolder *f, const char *inuid)
{
	CamelMessageInfo *info;
	CamelVeeMessageInfo *mi = NULL;

	info = camel_folder_get_message_info(f, inuid);
	if (info) {
		if ((mi = vee_folder_add(vf, f, info)))
			if (vf != folder_unmatched)
				unmatched_uid_add(inuid, f);

		camel_folder_free_message_info(f, info);
	}
	return mi;
}

/* must be called with summary_lock held */
static void
vfolder_remove_match(CamelVeeFolder *vf, CamelVeeMessageInfo *vinfo)
{
	const char *uid = camel_message_info_uid(vinfo);

	printf("removing match %s\n", uid);

	unmatched_uid_remove(strchr(uid, ':'), vinfo->folder);

	camel_folder_change_info_remove_uid(vf->changes, uid);
	camel_folder_summary_remove(((CamelFolder *)vf)->summary, (CamelMessageInfo *)vinfo);
}

/* must be called with summary_lock held */
static CamelVeeMessageInfo *
vee_folder_add_change(CamelVeeFolder *vf, CamelFolder *f, CamelMessageInfo *info)
{
	CamelVeeMessageInfo *mi = NULL;

	mi = vee_folder_add(vf, f, info);
	if (mi) {
		unmatched_uid_add(camel_message_info_uid(info), f);

		camel_folder_change_info_add_uid(vf->changes, camel_message_info_uid(mi));
	}
	
	return mi;
}

/* must be called with summary_lock held */
static void
vfolder_change_match(CamelVeeFolder *vf, CamelVeeMessageInfo *vinfo, const CamelMessageInfo *info)
{
	CamelFlag *flag;
	CamelTag *tag;

	d(printf("changing match %s\n", camel_message_info_uid(vinfo)));

	vinfo->info.flags = info->flags;
	camel_flag_list_free(&vinfo->info.user_flags);
	flag = info->user_flags;
	while (flag) {
		camel_flag_set(&vinfo->info.user_flags, flag->name, TRUE);
		flag = flag->next;
	}
	camel_tag_list_free(&vinfo->info.user_tags);
	tag = info->user_tags;
	while (tag) {
		camel_tag_set(&vinfo->info.user_tags, tag->name, tag->value);
		tag = tag->next;
	}
	camel_folder_change_info_change_uid(vf->changes, camel_message_info_uid(vinfo));
}

/* track changes to the unmatched folders */
static void
unmatched_finalise(CamelFolder *sub, gpointer type, CamelVeeFolder *vf)
{
	int count, i;

	CAMEL_VEE_FOLDER_LOCK(vf, summary_lock);
	UNMATCHED_LOCK();

	count = camel_folder_summary_count(((CamelFolder *)folder_unmatched)->summary);
	for (i=0;i<count;i++) {
		CamelVeeMessageInfo *mi = (CamelVeeMessageInfo *)camel_folder_summary_index(((CamelFolder *)folder_unmatched)->summary, i);
		const char *uid;
		char *oldkey;
		int n;

		if (mi) {
			uid = camel_message_info_uid(mi);
			if (mi->folder == sub) {
				if (g_hash_table_lookup_extended(unmatched_uids, uid, (void **)&oldkey, (void **)&n)) {
					if (n == 1)
						g_hash_table_remove(unmatched_uids, oldkey);
					else
						camel_folder_change_info_remove_uid(folder_unmatched->changes, uid);
				}
				camel_folder_summary_remove(((CamelFolder *)folder_unmatched)->summary, (CamelMessageInfo *)mi);
				i--;
			}
			camel_folder_summary_info_free(((CamelFolder *)folder_unmatched)->summary, (CamelMessageInfo *)mi);
		}
	}
	
	if (camel_folder_change_info_changed(folder_unmatched->changes)) {
		camel_object_trigger_event((CamelObject *)folder_unmatched, "folder_changed", folder_unmatched->changes);
		camel_folder_change_info_clear(folder_unmatched->changes);
	}

	UNMATCHED_UNLOCK();
	CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);
}

/* FIXME: This code is a big race, as it is never called locked ... */

static void
folder_changed(CamelFolder *sub, gpointer type, CamelVeeFolder *vf)
{
	CamelFolderChangeInfo *changes = type;
	CamelFolder *folder = (CamelFolder *)vf;
	char *vuid;
	CamelVeeMessageInfo *vinfo;
	int i;
	CamelMessageInfo *info;

	printf("folder_changed(%p, %p) (for %s)\n", sub, vf, vf->expression);

	/* if not auto-updating, only propagate changed events, not added/removed items */
	if ((vf->flags & CAMEL_STORE_VEE_FOLDER_AUTO) == 0) {
		CAMEL_VEE_FOLDER_LOCK(vf, summary_lock);

		for (i=0;i<changes->uid_changed->len;i++) {
			info = camel_folder_get_message_info(sub, changes->uid_changed->pdata[i]);
			vuid = g_strdup_printf("%p:%s", sub, (char *)changes->uid_changed->pdata[i]);
			vinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, vuid);
			if (vinfo && info)
				vfolder_change_match(vf, vinfo, info);
			
			g_free(vuid);
			
			if (info)
				camel_folder_free_message_info(sub, info);
			if (vinfo)
				camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)vinfo);
		}

		if (camel_folder_change_info_changed(vf->changes)) {
			camel_object_trigger_event((CamelObject *)vf, "folder_changed", vf->changes);
			camel_folder_change_info_clear(vf->changes);
		}

		CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);

		return;
	}

	/* if we are autoupdating, then do the magic */

	/* assume its faster to search a long list in whole, than by part */
	if (changes && (changes->uid_added->len + changes->uid_changed->len) < 500) {
		gboolean match;

		/* FIXME: We dont search body contents with this search, so, it isn't as
		   useful as it might be.
		   We shold probably just perform a whole search if we need to, i.e. there
		   are added items.  Changed items we are unlikely to want to remove immediately
		   anyway, although I guess it might be useful.
		   Removed items can always just be removed.
		*/

		/* see if added ones now match us */
		for (i=0;i<changes->uid_added->len;i++) {
			printf("checking new uid: %s\n", (char *)changes->uid_added->pdata[i]);
			info = camel_folder_get_message_info(sub, changes->uid_added->pdata[i]);
			if (info) {
				printf("uid ok, subject: %s\n", camel_message_info_subject(info));
				camel_folder_search_set_folder(vf->search, sub);
				match = camel_folder_search_match_expression(vf->search, vf->expression, info, NULL);
				if (match)
					vinfo = vee_folder_add_change(vf, sub, info);
				camel_folder_free_message_info(sub, info);
			}
		}

		/* check if changed ones still match */
		for (i=0;i<changes->uid_changed->len;i++) {
			info = camel_folder_get_message_info(sub, changes->uid_changed->pdata[i]);
			vuid = g_strdup_printf("%p:%s", sub, (char *)changes->uid_changed->pdata[i]);
			vinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, vuid);
			if (info) {
				camel_folder_search_set_folder(vf->search, sub);
				match = camel_folder_search_match_expression(vf->search, vf->expression, info, NULL);
				if (vinfo) {
					if (!match)
						vfolder_remove_match(vf, vinfo);
					else
						vfolder_change_match(vf, vinfo, info);
				} else if (match)
					vee_folder_add_change(vf, sub, info);
				camel_folder_free_message_info(sub, info);
			} else if (vinfo)
				vfolder_remove_match(vf, vinfo);

			if (vinfo)
				camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)vinfo);

			g_free(vuid);
		}

		/* mirror removes directly, if they used to match */
		for (i=0;i<changes->uid_removed->len;i++) {
			vuid = g_strdup_printf("%p:%s", sub, (char *)changes->uid_removed->pdata[i]);
			vinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, vuid);
			if (vinfo) {
				vfolder_remove_match(vf, vinfo);
				camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)vinfo);
			}
			g_free(vuid);
		}
	} else {
		vee_folder_build_folder(vf, sub, NULL);
	}

	/* cascade up, if we need to */
	if (camel_folder_change_info_changed(vf->changes)) {
		printf("got folder changes\n");
		camel_object_trigger_event( CAMEL_OBJECT(vf), "folder_changed", vf->changes);
		camel_folder_change_info_clear(vf->changes);
	} else
		printf("no, we didn't really get any changes\n");

	CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);

	UNMATCHED_LOCK();

	if (camel_folder_change_info_changed(folder_unmatched->changes)) {
		camel_object_trigger_event((CamelObject *)folder_unmatched, "folder_changed", folder_unmatched->changes);
		camel_folder_change_info_clear(folder_unmatched->changes);
	}

	UNMATCHED_UNLOCK();
}

/* FIXME: This code is a race, as it is never called locked */

/* track flag changes in the summary */
static void
message_changed(CamelFolder *f, const char *uid, CamelVeeFolder *vf)
{
	CamelMessageInfo *info;
	CamelVeeMessageInfo *vinfo;
	char *vuid;
	CamelFolder *folder = (CamelFolder *)vf;
	gboolean match;

	CAMEL_VEE_FOLDER_LOCK(vf, summary_lock);

	info = camel_folder_get_message_info(f, uid);
	vuid = g_strdup_printf("%p:%s", f, uid);
	vinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, vuid);

	/* see if this message now matches/doesn't match anymore */

	/* Hmm, this might not work if the folder uses some weird search thing,
	   and/or can be slow since it wont use any index index, hmmm. */

	if (vf->flags & CAMEL_STORE_VEE_FOLDER_AUTO) {
		camel_folder_search_set_folder(vf->search, f);
		
		match = camel_folder_search_match_expression(vf->search, vf->expression, info, NULL);

		if (info) {
			if (vinfo) {
				if (!match)
					vfolder_remove_match(vf, vinfo);
				else
					vfolder_change_match(vf, vinfo, info);
			}
			else if (match)
				vee_folder_add_change(vf, f, info);
		} else if (vinfo)
			vfolder_remove_match(vf, vinfo);
	} else {
		if (info && vinfo)
			vfolder_change_match(vf, vinfo, info);
	}

	if (info)
		camel_folder_free_message_info(f, info);
	if (vinfo)
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)vinfo);

	/* cascade up, if required.  This could probably be delayed,
	   but doesn't matter really, that is what freeze is for. */
	if (camel_folder_change_info_changed(vf->changes)) {
		camel_object_trigger_event( CAMEL_OBJECT(vf), "folder_changed", vf->changes);
		camel_folder_change_info_clear(vf->changes);
	}

	CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);

	UNMATCHED_LOCK();

	if (camel_folder_change_info_changed(folder_unmatched->changes)) {
		camel_object_trigger_event((CamelObject *)folder_unmatched, "folder_changed", folder_unmatched->changes);
		camel_folder_change_info_clear(folder_unmatched->changes);
	}

	UNMATCHED_UNLOCK();

	g_free(vuid);
}

void
camel_vee_folder_add_folder(CamelVeeFolder *vf, CamelFolder *sub)
{
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf), *up = _PRIVATE(folder_unmatched);

	camel_object_ref((CamelObject *)sub);

	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);

	/* the reference is shared with both the real vfolder and the unmatched vfolder */
	p->folders = g_list_append(p->folders, sub);
	up->folders = g_list_append(up->folders, sub);

	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);

	printf("camel_vee_folder_add_folde(%p, %p)\n", vf, sub);

	camel_object_hook_event((CamelObject *)sub, "folder_changed", (CamelObjectEventHookFunc)folder_changed, vf);
	camel_object_hook_event((CamelObject *)sub, "message_changed", (CamelObjectEventHookFunc)message_changed, vf);

	CAMEL_VEE_FOLDER_LOCK(vf, summary_lock);

	vee_folder_build_folder(vf, sub, NULL);

	/* we'll assume the caller is going to update the whole list after they do this
	   this may or may not be the right thing to do, but it should be close enough */
	camel_folder_change_info_clear(vf->changes);

	CAMEL_VEE_FOLDER_UNLOCK(vf, summary_lock);
}

static void
vee_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);

	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;

		camel_folder_sync(f, expunge, ex);
		if (camel_exception_is_set(ex))
			break;

		node = node->next;
	}

	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);
}

static void
vee_expunge (CamelFolder *folder, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);

	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;

		camel_folder_expunge(f, ex);
		if (camel_exception_is_set(ex))
			break;
		vee_folder_build_folder(vf, f, ex);
		if (camel_exception_is_set(ex))
			break;

		node = node->next;
	}

	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);
}

static CamelMimeMessage *vee_get_message(CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelVeeMessageInfo *mi;
	CamelMimeMessage *msg = NULL;

	mi = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, uid);
	if (mi == NULL)
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     "No such message %s in %s", uid,
				     folder->name);
	else
		msg =  camel_folder_get_message(mi->folder, strchr(camel_message_info_uid(mi), ':') + 1, ex);
	camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);

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

	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);

	expr = g_strdup_printf("(and %s %s)", vf->expression, expression);
	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;
		int i;

		matches = camel_folder_search_by_expression(f, expression, ex);
		for (i = 0; i < matches->len; i++) {
			char *uid = matches->pdata[i];
			g_ptr_array_add(result, g_strdup_printf("%p:%s", f, uid));
		}
		camel_folder_search_free(f, matches);
		node = g_list_next(node);
	}

	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);

	return result;
}

static void
vee_set_message_flags(CamelFolder *folder, const char *uid, guint32 flags, guint32 set)
{
	CamelVeeMessageInfo *mi;

	mi = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, uid);
	if (mi) {
		((CamelFolderClass *)camel_vee_folder_parent)->set_message_flags(folder, uid, flags, set);
		camel_folder_set_message_flags(mi->folder, strchr(camel_message_info_uid(mi), ':') + 1, flags, set);
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);
	}
}

static void
vee_set_message_user_flag(CamelFolder *folder, const char *uid, const char *name, gboolean value)
{
	CamelVeeMessageInfo *mi;

	mi = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, uid);
	if (mi) {
		((CamelFolderClass *)camel_vee_folder_parent)->set_message_user_flag(folder, uid, name, value);
		camel_folder_set_message_user_flag(mi->folder, strchr(camel_message_info_uid(mi), ':') + 1, name, value);
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);
	}
}

static void
vee_move_message_to(CamelFolder *folder, const char *uid, CamelFolder *dest, CamelException *ex)
{
	CamelVeeMessageInfo *mi;

	mi = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, uid);
	if (mi) {
		/* noop if it we're moving from the same vfolder (uh, which should't happen but who knows) */
		if (folder != mi->folder) {
			camel_folder_move_message_to(mi->folder, strchr(camel_message_info_uid(mi), ':')+1, dest, ex);
		}
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);
	} else {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID, _("No such message: %s"), uid);
	}
}

/*
  need incremental update, based on folder.
  Need to watch folders for changes and update accordingly.
*/

/* this does most of the vfolder magic */
/* must have summary_lock held when calling */
static void
vee_folder_build(CamelVeeFolder *vf, CamelException *ex)
{
	CamelFolder *folder = (CamelFolder *)vf;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	camel_folder_summary_clear(folder->summary);

	CAMEL_VEE_FOLDER_LOCK(vf, subfolder_lock);

	node = p->folders;
	while (node) {
		GPtrArray *matches;
		CamelFolder *f = node->data;
		int i;

		matches = camel_folder_search_by_expression(f, vf->expression, ex);
		for (i = 0; i < matches->len; i++)
			vee_folder_add_uid(vf, f, matches->pdata[i]);

		camel_folder_search_free(f, matches);
		node = g_list_next(node);
	}

	CAMEL_VEE_FOLDER_UNLOCK(vf, subfolder_lock);
}

static void
removed_uid(void *key, void *value, void *data)
{
	unmatched_uid_check(key, data);
}

/* build query contents for a single folder */
/* must have summary_lock held when calling */
static void
vee_folder_build_folder(CamelVeeFolder *vf, CamelFolder *source, CamelException *ex)
{
	GPtrArray *matches, *all;
	GHashTable *left;
	CamelFolder *f = source;
	CamelVeeMessageInfo *mi;
	CamelFolder *folder = (CamelFolder *)vf;
	int i;
	int count;

	left = g_hash_table_new(g_str_hash, g_str_equal);
	all = camel_folder_get_uids(f);
	for (i=0;i<all->len;i++)
		g_hash_table_insert(left, all->pdata[i], (void *)1);

	count = camel_folder_summary_count(folder->summary);
	for (i=0;i<count;i++) {
		CamelVeeMessageInfo *mi = (CamelVeeMessageInfo *)camel_folder_summary_index(folder->summary, i);
		if (mi) {
			if (mi->folder == source) {
				camel_folder_change_info_add_source(vf->changes, camel_message_info_uid(mi));
				camel_folder_summary_remove(folder->summary, (CamelMessageInfo *)mi);
				unmatched_uid_remove(camel_message_info_uid(mi), source);
				i--;
			}
			camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);
		}
	}

	matches = camel_folder_search_by_expression(f, vf->expression, ex);
	for (i = 0; i < matches->len; i++) {
		g_hash_table_remove(left, matches->pdata[i]);
		mi = vee_folder_add_uid(vf, f, matches->pdata[i]);
		if (mi)
			camel_folder_change_info_add_update(vf->changes, camel_message_info_uid(mi));
	}

	/* check if we have a match for these in another vfolder, else add them to the UNMATCHED folder */
	g_hash_table_foreach(left, removed_uid, source);
	g_hash_table_destroy(left);
	camel_folder_search_free(f, matches);
	camel_folder_free_uids(f, all);

	camel_folder_change_info_build_diff(vf->changes);
	camel_folder_change_info_build_diff(folder_unmatched->changes);

	if (camel_folder_change_info_changed(folder_unmatched->changes)) {
		camel_object_trigger_event((CamelObject *)folder_unmatched, "folder_changed", folder_unmatched->changes);
		camel_folder_change_info_clear(folder_unmatched->changes);
	}
}

/*

  (match-folder "folder1" "folder2")

 */
