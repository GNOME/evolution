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
#ifdef DYNAMIC
#include "camel-folder-search.h"
#endif

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

struct _CamelVeeFolderPrivate {
	GList *folders;
};

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

static void folder_changed(CamelFolder *sub, gpointer type, CamelVeeFolder *vf);
static void message_changed(CamelFolder *f, const char *uid, CamelVeeFolder *mf);

static void vee_folder_build(CamelVeeFolder *vf, CamelException *ex);
static void vee_folder_build_folder(CamelVeeFolder *vf, CamelFolder *source, CamelException *ex);

static CamelFolderClass *camel_vee_folder_parent;

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
#ifdef DYNAMIC
	obj->search = camel_folder_search_new();
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
#ifdef DYNAMIC
	camel_object_unref((CamelObject *)vf->search);
#endif
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
camel_vee_folder_new (CamelStore *parent_store, const char *name, CamelException *ex)
{
	CamelFolderInfo *fi;
	CamelFolder *folder;
	CamelVeeFolder *vf;
	char *namepart, *searchpart;

	folder =  CAMEL_FOLDER (camel_object_new (camel_vee_folder_get_type()));
	vf = (CamelVeeFolder *)folder;

	namepart = g_strdup(name);
	searchpart = strchr(namepart, '?');
	if (searchpart == NULL) {
		/* no search, no result! */
		searchpart = "(body-contains \"=some-invalid_string-sequence=xx\")";
	} else {
		*searchpart++ = 0;
	}

	camel_folder_construct (folder, parent_store, namepart, namepart);

	folder->summary = camel_folder_summary_new();
	folder->summary->message_info_size = sizeof(CamelVeeMessageInfo);

	vf->expression = g_strdup(searchpart);
	vf->vname = namepart;

	vee_folder_build(vf, ex);
	if (camel_exception_is_set (ex)) {
		camel_object_unref (CAMEL_OBJECT (folder));
		return NULL;
	}
	
	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = g_strdup (name);
	fi->name = g_strdup (name);
	fi->url = g_strdup_printf ("vfolder:%s?%s", vf->vname, vf->expression);
	fi->unread_message_count = -1;
	
	camel_object_trigger_event (CAMEL_OBJECT (parent_store),
				    "folder_created", fi);
	
	camel_folder_info_free (fi);
	
	return folder;
}

static CamelVeeMessageInfo *
vee_folder_add(CamelVeeFolder *vf, CamelFolder *f, CamelMessageInfo *info)
{
	CamelVeeMessageInfo *mi;
	char *uid;
	CamelFolder *folder = (CamelFolder *)vf;

	mi = (CamelVeeMessageInfo *)camel_folder_summary_info_new(folder->summary);
	camel_message_info_dup_to(info, (CamelMessageInfo *)mi);
	uid = g_strdup_printf("%p:%s", f, camel_message_info_uid(info));
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

static CamelVeeMessageInfo *
vee_folder_add_uid(CamelVeeFolder *vf, CamelFolder *f, const char *inuid)
{
	CamelMessageInfo *info;
	CamelVeeMessageInfo *mi = NULL;

	info = camel_folder_get_message_info(f, inuid);
	if (info) {
		mi = vee_folder_add(vf, f, info);
		camel_folder_free_message_info(f, info);
	}
	return mi;
}

#ifdef DYNAMIC
static void
vfolder_remove_match(CamelVeeFolder *vf, CamelVeeMessageInfo *vinfo)
{
	const char *uid = camel_message_info_uid(vinfo);

	printf("removing match %s\n", uid);

	camel_folder_summary_remove(((CamelFolder *)vf)->summary, (CamelMessageInfo *)vinfo);
	camel_folder_change_info_remove_uid(vf->changes, uid);
}

static CamelVeeMessageInfo *
vee_folder_add_change(CamelVeeFolder *vf, CamelFolder *f, CamelMessageInfo *info)
{
	CamelVeeMessageInfo *mi = NULL;

	mi = vee_folder_add(vf, f, info);
	camel_folder_change_info_add_uid(vf->changes, camel_message_info_uid(mi));
	
	return mi;
}

#endif

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

/* FIXME: This code is a big race, as it is never called locked ... */

static void
folder_changed(CamelFolder *sub, gpointer type, CamelVeeFolder *vf)
{
	CamelException *ex;

#ifdef DYNAMIC
	CamelFolderChangeInfo *changes = type;
	CamelFolder *folder = (CamelFolder *)vf;

	/* assume its faster to search a long list in whole, than by part */
	if (changes && (changes->uid_added->len + changes->uid_changed->len) < 500) {
		int i;
		char *vuid;
		CamelVeeMessageInfo *vinfo;
		gboolean match;
		CamelMessageInfo *info;

		ex = camel_exception_new();

		/* FIXME: We dont search body contents with this search, so, it isn't as
		   useful as it might be.
		   We shold probably just perform a whole search if we need to, i.e. there
		   are added items.  Changed items we are unlikely to want to remove immediately
		   anyway, although I guess it might be useful.
		   Removed items can always just be removed.
		*/

		/* see if added ones now match us */
		for (i=0;i<changes->uid_added->len;i++) {
			info = camel_folder_get_message_info(sub, changes->uid_added->pdata[i]);
			if (info) {
				camel_folder_search_set_folder(vf->search, sub);
				match = camel_folder_search_match_expression(vf->search, vf->expression, info, ex);
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
#if 0
				match = camel_folder_search_match_expression(vf->search, vf->expression, info, ex);
#endif
				if (vinfo) {
#if 0
					if (!match)
						vfolder_remove_match(vf, vinfo);
					else
#endif
						vfolder_change_match(vf, vinfo, info);
				}
#if 0
				else if (match)
					vee_folder_add_change(vf, sub, info);
#endif
				camel_folder_free_message_info(sub, info);
			} else if (vinfo)
				vfolder_remove_match(vf, vinfo);

			if (vinfo)
				camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)vinfo);

			g_free(vuid);
		}

		camel_exception_free(ex);

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
#endif
		ex = camel_exception_new();
		vee_folder_build_folder(vf, sub, ex);
		camel_exception_free(ex);
#ifdef DYNAMIC
	}
#endif

	/* cascade up, if we need to */
	if (camel_folder_change_info_changed(vf->changes)) {
		camel_object_trigger_event( CAMEL_OBJECT(vf), "folder_changed", vf->changes);
		camel_folder_change_info_clear(vf->changes);
	}
}

/* FIXME: This code is a race, as it is never called locked */

/* track flag changes in the summary */
static void
message_changed(CamelFolder *f, const char *uid, CamelVeeFolder *mf)
{
	CamelMessageInfo *info;
	CamelVeeMessageInfo *vinfo;
	char *vuid;
	CamelFolder *folder = (CamelFolder *)mf;
#ifdef DYNAMIC
	/*gboolean match;*/
	CamelException *ex;
#endif

	info = camel_folder_get_message_info(f, uid);
	vuid = g_strdup_printf("%p:%s", f, uid);
	vinfo = (CamelVeeMessageInfo *)camel_folder_summary_uid(folder->summary, vuid);

	/* see if this message now matches/doesn't match anymore */

	/* Hmm, this might not work if the folder uses some weird search thing,
	   and/or can be slow since it wont use any index index, hmmm. */

#ifdef DYNAMIC
	camel_folder_search_set_folder(mf->search, f);
	ex = camel_exception_new();
#if 0
	match = camel_folder_search_match_expression(mf->search, mf->expression, info, ex);
#endif
	camel_exception_free(ex);
	if (info) {
		if (vinfo) {
#if 0
			if (!match)
				vfolder_remove_match(mf, vinfo);
			else
#endif
				vfolder_change_match(mf, vinfo, info);
		}
#if 0
		else if (match)
			vee_folder_add_change(mf, f, info);
#endif
	} else if (vinfo)
		vfolder_remove_match(mf, vinfo);
#else
	if (info && vinfo)
		vfolder_change_match(mf, vinfo, info);
#endif

	if (info)
		camel_folder_free_message_info(f, info);
	if (vinfo)
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)vinfo);

	/* cascade up, if required.  This could probably be delayed,
	   but doesn't matter really, that is what freeze is for. */
	if (camel_folder_change_info_changed(mf->changes)) {
		camel_object_trigger_event( CAMEL_OBJECT(mf), "folder_changed", mf->changes);
		camel_folder_change_info_clear(mf->changes);
	}

	g_free(vuid);
}

void
camel_vee_folder_add_folder(CamelVeeFolder *vf, CamelFolder *sub)
{
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	CamelException *ex;

	camel_object_ref((CamelObject *)sub);
	p->folders = g_list_append(p->folders, sub);

	camel_object_hook_event ((CamelObject *)sub, "folder_changed", (CamelObjectEventHookFunc) folder_changed, vf);
	camel_object_hook_event ((CamelObject *)sub, "message_changed", (CamelObjectEventHookFunc) message_changed, vf);

	ex = camel_exception_new();
	vee_folder_build_folder(vf, sub, ex);
	camel_exception_free(ex);

	/* we'll assume the caller is going to update the whole list after they do this
	   this may or may not be the right thing to do, but it should be close enough */
#if 0	
	if (camel_folder_change_info_changed(vf->changes)) {
		camel_object_trigger_event( CAMEL_OBJECT(vf), "folder_changed", vf->changes);
		camel_folder_change_info_clear(vf->changes);
	}
#else
	camel_folder_change_info_clear(vf->changes);
#endif
	
}

static void
vee_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;

		camel_folder_sync(f, expunge, ex);
		node = node->next;
	}
}

static void
vee_expunge (CamelFolder *folder, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;

		camel_folder_expunge(f, ex);
		node = node->next;
	}
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
static void
vee_folder_build(CamelVeeFolder *vf, CamelException *ex)
{
	CamelFolder *folder = (CamelFolder *)vf;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	camel_folder_summary_clear(folder->summary);

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
}


/* build query contents for a single folder */
static void
vee_folder_build_folder(CamelVeeFolder *vf, CamelFolder *source, CamelException *ex)
{
	GPtrArray *matches;
	CamelFolder *f = source;
	CamelVeeMessageInfo *mi;
	CamelFolder *folder = (CamelFolder *)vf;
	int i;
	int count;

	count = camel_folder_summary_count(folder->summary);
	for (i=0;i<count;i++) {
		CamelVeeMessageInfo *mi = (CamelVeeMessageInfo *)camel_folder_summary_index(folder->summary, i);
		if (mi) {
			if (mi->folder == source) {
				const char *uid = camel_message_info_uid(mi);
				camel_folder_change_info_add_source(vf->changes, uid);
				camel_folder_summary_remove(folder->summary, (CamelMessageInfo *)mi);
				i--;
			}
			camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)mi);
		}
	}

	matches = camel_folder_search_by_expression(f, vf->expression, ex);
	for (i = 0; i < matches->len; i++) {
		mi = vee_folder_add_uid(vf, f, matches->pdata[i]);
		if (mi)
			camel_folder_change_info_add_update(vf->changes, camel_message_info_uid(mi));
	}
	camel_folder_search_free(f, matches);

	camel_folder_change_info_build_diff(vf->changes);
}


/*

  (match-folder "folder1" "folder2")

 */
