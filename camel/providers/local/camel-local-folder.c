/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 * 
 * Authors: Michael Zucchi <notzed@helixcode.com>
 *
 * Copyright (C) 1999, 2000 Helix Code Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "camel-local-folder.h"
#include "camel-local-store.h"
#include "string-utils.h"
#include "camel-stream-fs.h"
#include "camel-local-summary.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-exception.h"

#define d(x) (printf("%s(%d): ", __FILE__, __LINE__),(x))

static CamelFolderClass *parent_class = NULL;

/* Returns the class for a CamelLocalFolder */
#define CLOCALF_CLASS(so) CAMEL_LOCAL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CLOCALS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static int local_lock(CamelLocalFolder *lf, CamelLockType type, CamelException *ex);
static void local_unlock(CamelLocalFolder *lf);

static void local_sync(CamelFolder *folder, gboolean expunge, CamelException *ex);
static gint local_get_message_count(CamelFolder *folder);
static gint local_get_unread_message_count(CamelFolder *folder);

static GPtrArray *local_get_uids(CamelFolder *folder);
static GPtrArray *local_get_summary(CamelFolder *folder);
#if 0
static void local_append_message(CamelFolder *folder, CamelMimeMessage * message, const CamelMessageInfo * info, CamelException *ex);
static CamelMimeMessage *local_get_message(CamelFolder *folder, const gchar * uid, CamelException *ex);
#endif
static void local_expunge(CamelFolder *folder, CamelException *ex);

static const CamelMessageInfo *local_get_message_info(CamelFolder *folder, const char *uid);

static GPtrArray *local_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex);
static void local_search_free(CamelFolder *folder, GPtrArray * result);

static guint32 local_get_message_flags(CamelFolder *folder, const char *uid);
static void local_set_message_flags(CamelFolder *folder, const char *uid, guint32 flags, guint32 set);
static gboolean local_get_message_user_flag(CamelFolder *folder, const char *uid, const char *name);
static void local_set_message_user_flag(CamelFolder *folder, const char *uid, const char *name, gboolean value);
static const char *local_get_message_user_tag(CamelFolder *folder, const char *uid, const char *name);
static void local_set_message_user_tag(CamelFolder *folder, const char *uid, const char *name, const char *value);


static void local_finalize(CamelObject * object);

static void
camel_local_folder_class_init(CamelLocalFolderClass * camel_local_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS(camel_local_folder_class);

	parent_class = CAMEL_FOLDER_CLASS(camel_type_get_global_classfuncs(camel_folder_get_type()));

	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->sync = local_sync;
	camel_folder_class->get_message_count = local_get_message_count;
	camel_folder_class->get_unread_message_count = local_get_unread_message_count;
	camel_folder_class->get_uids = local_get_uids;
	camel_folder_class->free_uids = camel_folder_free_deep;
	camel_folder_class->get_summary = local_get_summary;
	camel_folder_class->free_summary = camel_folder_free_nop;
	camel_folder_class->expunge = local_expunge;

	camel_folder_class->search_by_expression = local_search_by_expression;
	camel_folder_class->search_free = local_search_free;

	camel_folder_class->get_message_info = local_get_message_info;

	camel_folder_class->get_message_flags = local_get_message_flags;
	camel_folder_class->set_message_flags = local_set_message_flags;
	camel_folder_class->get_message_user_flag = local_get_message_user_flag;
	camel_folder_class->set_message_user_flag = local_set_message_user_flag;
	camel_folder_class->get_message_user_tag = local_get_message_user_tag;
	camel_folder_class->set_message_user_tag = local_set_message_user_tag;

	camel_local_folder_class->lock = local_lock;
	camel_local_folder_class->unlock = local_unlock;
}

static void
local_init(gpointer object, gpointer klass)
{
	CamelFolder *folder = object;
	CamelLocalFolder *local_folder = object;

	folder->has_summary_capability = TRUE;
	folder->has_search_capability = TRUE;

	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED |
	    CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_DRAFT |
	    CAMEL_MESSAGE_FLAGGED | CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_USER;

	local_folder->summary = NULL;
	local_folder->search = NULL;
}

static void
local_finalize(CamelObject * object)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(object);

	if (local_folder->index)
		ibex_close(local_folder->index);

	g_free(local_folder->base_path);
	g_free(local_folder->folder_path);
	g_free(local_folder->summary_path);
	g_free(local_folder->index_path);

	camel_folder_change_info_free(local_folder->changes);
}

CamelType camel_local_folder_get_type(void)
{
	static CamelType camel_local_folder_type = CAMEL_INVALID_TYPE;

	if (camel_local_folder_type == CAMEL_INVALID_TYPE) {
		camel_local_folder_type = camel_type_register(CAMEL_FOLDER_TYPE, "CamelLocalFolder",
							     sizeof(CamelLocalFolder),
							     sizeof(CamelLocalFolderClass),
							     (CamelObjectClassInitFunc) camel_local_folder_class_init,
							     NULL,
							     (CamelObjectInitFunc) local_init,
							     (CamelObjectFinalizeFunc) local_finalize);
	}

	return camel_local_folder_type;
}

CamelLocalFolder *
camel_local_folder_construct(CamelLocalFolder *lf, CamelStore *parent_store, const char *full_name, guint32 flags, CamelException *ex)
{
	CamelFolder *folder;
	const char *root_dir_path, *name;
	struct stat st;
	int forceindex;

	folder = (CamelFolder *)lf;

	name = strrchr(full_name, '/');
	if (name)
		name++;
	else
		name = full_name;

	camel_folder_construct(folder, parent_store, full_name, name);

	root_dir_path = camel_local_store_get_toplevel_dir(CAMEL_LOCAL_STORE(folder->parent_store));

	lf->base_path = g_strdup(root_dir_path);
	lf->folder_path = g_strdup_printf("%s/%s", root_dir_path, full_name);
	lf->summary_path = g_strdup_printf("%s/%s.ev-summary", root_dir_path, full_name);
	lf->index_path = g_strdup_printf("%s/%s.ibex", root_dir_path, full_name);

	lf->changes = camel_folder_change_info_new();

	/* if we have no index file, force it */
	forceindex = stat(lf->index_path, &st) == -1;
	if (flags & CAMEL_STORE_FOLDER_BODY_INDEX) {

		lf->index = ibex_open(lf->index_path, O_CREAT | O_RDWR, 0600);
		if (lf->index == NULL) {
			/* yes, this isn't fatal at all */
			g_warning("Could not open/create index file: %s: indexing not performed", strerror(errno));
			forceindex = FALSE;
			/* record that we dont have an index afterall */
			flags &= ~CAMEL_STORE_FOLDER_BODY_INDEX;
		}
	} else {
		/* if we do have an index file, remove it */
		if (forceindex == FALSE) {
			unlink(lf->index_path);
		}
		forceindex = FALSE;
	}

	lf->flags = flags;

	lf->summary = CLOCALF_CLASS(lf)->create_summary(lf->summary_path, lf->folder_path, lf->index);
	if (camel_local_summary_load(lf->summary, forceindex, ex) == -1) {
		camel_object_unref (CAMEL_OBJECT (folder));
		return NULL;
	}

	return lf;
}

/* lock the folder, may be called repeatedly (with matching unlock calls),
   with type the same or less than the first call */
int camel_local_folder_lock(CamelLocalFolder *lf, CamelLockType type, CamelException *ex)
{
	if (lf->locked > 0) {
		/* lets be anal here - its important the code knows what its doing */
		g_assert(lf->locktype == type || lf->locktype == CAMEL_LOCK_WRITE);
	} else {
		if (CLOCALF_CLASS(lf)->lock(lf, type, ex) == -1)
			return -1;
		lf->locktype = type;
	}

	lf->locked++;

	return 0;
}

/* unlock folder */
int camel_local_folder_unlock(CamelLocalFolder *lf)
{
	g_assert(lf->locked>0);
	lf->locked--;
	if (lf->locked == 0)
		CLOCALF_CLASS(lf)->unlock(lf);

	return 0;
}

static int
local_lock(CamelLocalFolder *lf, CamelLockType type, CamelException *ex)
{
	return 0;
}

static void
local_unlock(CamelLocalFolder *lf)
{
	/* nothing */
}

static void
local_sync(CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelLocalFolder *lf = CAMEL_LOCAL_FOLDER(folder);

	d(printf("local sync, expunge=%s\n", expunge?"true":"false"));

	if (camel_local_folder_lock(lf, CAMEL_LOCK_WRITE, ex) == -1)
		return;

	/* if sync fails, we'll pass it up on exit through ex */
	camel_local_summary_sync(lf->summary, expunge, lf->changes, ex);
	camel_local_folder_unlock(lf);

	if (camel_folder_change_info_changed(lf->changes)) {
		camel_object_trigger_event(CAMEL_OBJECT(folder), "folder_changed", lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}

	/* force save of metadata */
	if (lf->index)
		ibex_save(lf->index);
	if (lf->summary)
		camel_folder_summary_save(CAMEL_FOLDER_SUMMARY(lf->summary));
}

static void
local_expunge(CamelFolder *folder, CamelException *ex)
{
	d(printf("expunge\n"));

	/* Just do a sync with expunge, serves the same purpose */
	camel_folder_sync(folder, TRUE, ex);
}

/*
  The following functions all work off the summary, so the default operations provided
  in camel-local-folder will suffice for all subclasses.  They may want to
  snoop various operations to ensure the status remains synced, or just wait
  for the sync operation
*/
static gint
local_get_message_count(CamelFolder *folder)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);

	g_return_val_if_fail(local_folder->summary != NULL, -1);

	return camel_folder_summary_count(CAMEL_FOLDER_SUMMARY(local_folder->summary));
}

static gint
local_get_unread_message_count(CamelFolder *folder)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);
	CamelMessageInfo *info;
	GPtrArray *infolist;
	gint i, max, count = 0;

	g_return_val_if_fail(local_folder->summary != NULL, -1);

	max = camel_folder_summary_count(CAMEL_FOLDER_SUMMARY(local_folder->summary));
	if (max == -1)
		return -1;

	infolist = local_get_summary(folder);

	for (i = 0; i < infolist->len; i++) {
		info = (CamelMessageInfo *) g_ptr_array_index(infolist, i);
		if (!(info->flags & CAMEL_MESSAGE_SEEN))
			count++;
	}

	return count;
}

static GPtrArray *
local_get_uids(CamelFolder *folder)
{
	GPtrArray *array;
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);
	int i, count;

	count = camel_folder_summary_count(CAMEL_FOLDER_SUMMARY(local_folder->summary));
	array = g_ptr_array_new();
	g_ptr_array_set_size(array, count);
	for (i = 0; i < count; i++) {
		CamelMessageInfo *info = camel_folder_summary_index(CAMEL_FOLDER_SUMMARY(local_folder->summary), i);

		array->pdata[i] = g_strdup(info->uid);
	}

	return array;
}

GPtrArray *
local_get_summary(CamelFolder *folder)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);

	return CAMEL_FOLDER_SUMMARY(local_folder->summary)->messages;
}

/* get a single message info, by uid */
static const CamelMessageInfo *
local_get_message_info(CamelFolder *folder, const char *uid)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);

	return camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(local_folder->summary), uid);
}

static GPtrArray *
local_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);

	if (local_folder->search == NULL) {
		local_folder->search = camel_folder_search_new();
	}

	camel_folder_search_set_folder(local_folder->search, folder);
	if (local_folder->summary) {
		/* FIXME: dont access summary array directly? */
		camel_folder_search_set_summary(local_folder->search,
						CAMEL_FOLDER_SUMMARY(local_folder->summary)->messages);
	}

	camel_folder_search_set_body_index(local_folder->search, local_folder->index);

	return camel_folder_search_execute_expression(local_folder->search, expression, ex);
}

static void
local_search_free(CamelFolder *folder, GPtrArray * result)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);

	camel_folder_search_free_result(local_folder->search, result);
}

static guint32
local_get_message_flags(CamelFolder *folder, const char *uid)
{
	CamelMessageInfo *info;
	CamelLocalFolder *mf = CAMEL_LOCAL_FOLDER(folder);

	info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mf->summary), uid);
	g_return_val_if_fail(info != NULL, 0);

	return info->flags;
}

static void
local_set_message_flags(CamelFolder *folder, const char *uid, guint32 flags, guint32 set)
{
	CamelMessageInfo *info;
	CamelLocalFolder *mf = CAMEL_LOCAL_FOLDER(folder);

	info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mf->summary), uid);
	g_return_if_fail(info != NULL);

	info->flags = (info->flags & ~flags) | (set & flags) | CAMEL_MESSAGE_FOLDER_FLAGGED;
	camel_folder_summary_touch(CAMEL_FOLDER_SUMMARY(mf->summary));

	camel_object_trigger_event(CAMEL_OBJECT(folder), "message_changed", (char *) uid);
}

static gboolean
local_get_message_user_flag(CamelFolder *folder, const char *uid, const char *name)
{
	CamelMessageInfo *info;
	CamelLocalFolder *mf = CAMEL_LOCAL_FOLDER(folder);

	info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mf->summary), uid);
	g_return_val_if_fail(info != NULL, FALSE);

	return camel_flag_get(&info->user_flags, name);
}

static void
local_set_message_user_flag(CamelFolder *folder, const char *uid, const char *name, gboolean value)
{
	CamelMessageInfo *info;
	CamelLocalFolder *mf = CAMEL_LOCAL_FOLDER(folder);

	info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mf->summary), uid);
	g_return_if_fail(info != NULL);

	camel_flag_set(&info->user_flags, name, value);
	info->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED;
	camel_folder_summary_touch(CAMEL_FOLDER_SUMMARY(mf->summary));
	camel_object_trigger_event(CAMEL_OBJECT(folder), "message_changed", (char *) uid);
}

static const char *
local_get_message_user_tag(CamelFolder *folder, const char *uid, const char *name)
{
	CamelMessageInfo *info;
	CamelLocalFolder *mf = CAMEL_LOCAL_FOLDER(folder);

	info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mf->summary), uid);
	g_return_val_if_fail(info != NULL, FALSE);

	return camel_tag_get(&info->user_tags, name);
}

static void
local_set_message_user_tag(CamelFolder *folder, const char *uid, const char *name, const char *value)
{
	CamelMessageInfo *info;
	CamelLocalFolder *mf = CAMEL_LOCAL_FOLDER(folder);

	info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mf->summary), uid);
	g_return_if_fail(info != NULL);

	camel_tag_set(&info->user_tags, name, value);
	info->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED;
	camel_folder_summary_touch(CAMEL_FOLDER_SUMMARY(mf->summary));
	camel_object_trigger_event(CAMEL_OBJECT(folder), "message_changed", (char *) uid);
}


