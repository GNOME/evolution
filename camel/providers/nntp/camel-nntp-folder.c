/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-folder.c : Class for a news folder
 *
 * Authors : Chris Toshok <toshok@ximian.com> 
 *           Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2001-2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "camel/camel-file-utils.h"
#include "camel/camel-stream-mem.h"
#include "camel/camel-data-wrapper.h"
#include "camel/camel-mime-message.h"
#include "camel/camel-folder-search.h"
#include "camel/camel-exception.h"
#include "camel/camel-session.h"
#include "camel/camel-data-cache.h"

#include "camel-nntp-summary.h"
#include "camel-nntp-store.h"
#include "camel-nntp-folder.h"
#include "camel-nntp-store.h"
#include "camel-nntp-private.h"

static CamelFolderClass *parent_class = NULL;

/* Returns the class for a CamelNNTPFolder */
#define CNNTPF_CLASS(so) CAMEL_NNTP_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CNNTPS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static void
nntp_folder_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelNNTPStore *nntp_store;
	CamelFolderChangeInfo *changes = NULL;
	CamelNNTPFolder *nntp_folder;

	nntp_store = (CamelNNTPStore *)folder->parent_store;
	nntp_folder = (CamelNNTPFolder *)folder;

	CAMEL_NNTP_STORE_LOCK(nntp_store, command_lock);

	if (camel_nntp_summary_check((CamelNNTPSummary *)folder->summary, nntp_folder->changes, ex) != -1)
		camel_folder_summary_save (folder->summary);

	if (camel_folder_change_info_changed(nntp_folder->changes)) {
		changes = nntp_folder->changes;
		nntp_folder->changes = camel_folder_change_info_new();
	}

	CAMEL_NNTP_STORE_UNLOCK(nntp_store, command_lock);

	if (changes) {
		camel_object_trigger_event((CamelObject *)folder, "folder_changed", changes);
		camel_folder_change_info_free(changes);
	}
}

static void
nntp_folder_set_message_flags(CamelFolder *folder, const char *uid, guint32 flags, guint32 set)
{
        ((CamelFolderClass *)parent_class)->set_message_flags(folder, uid, flags, set);
}

static CamelMimeMessage *
nntp_folder_get_message (CamelFolder *folder, const char *uid, CamelException *ex)
{
	CamelMimeMessage *message = NULL;
	CamelNNTPStore *nntp_store;
	CamelFolderChangeInfo *changes;
	CamelNNTPFolder *nntp_folder;
	CamelStream *stream = NULL;
	int ret;
	char *line;
	const char *msgid;

	nntp_store = (CamelNNTPStore *)folder->parent_store;
	nntp_folder = (CamelNNTPFolder *)folder;

	CAMEL_NNTP_STORE_LOCK(nntp_store, command_lock);

	msgid = strchr(uid, ',');
	if (msgid == 0) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Internal error: uid in invalid format: %s"), uid);
		goto fail;
	}
	msgid++;

	/* Lookup in cache, NEWS is global messageid's so use a global cache path */
	stream = camel_data_cache_get(nntp_store->cache, "cache", msgid, NULL);
	if (stream == NULL) {
		/* Not in cache, retrieve and put in cache */
		if (camel_nntp_store_set_folder(nntp_store, folder, nntp_folder->changes, ex) == -1)
			goto fail;

		ret = camel_nntp_command(nntp_store, &line, "article %s", msgid);
		if (ret == -1)
			goto error;

		if (ret == 220) {
			stream = camel_data_cache_add(nntp_store->cache, "cache", msgid, NULL);
			if (stream) {
				if (camel_stream_write_to_stream((CamelStream *)nntp_store->stream, stream) == -1)
					goto error;
				if (camel_stream_reset(stream) == -1)
					goto error;
			} else {
				stream = (CamelStream *)nntp_store->stream;
				camel_object_ref((CamelObject *)stream);
			}
		}
	}

	if (stream) {
		message = camel_mime_message_new();
		if (camel_data_wrapper_construct_from_stream((CamelDataWrapper *)message, stream) == -1)
			goto error;

		CAMEL_NNTP_STORE_UNLOCK(nntp_store, command_lock);

		camel_object_unref((CamelObject *)stream);
		return message;
	}

	camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot get message %s: %s"), uid, line);

error:
	if (errno == EINTR)
		camel_exception_setv(ex, CAMEL_EXCEPTION_USER_CANCEL, _("User cancelled"));
	else
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot get message %s: %s"), uid, strerror(errno));

fail:
	if (message)
		camel_object_unref((CamelObject *)message);

	if (stream)
		camel_object_unref((CamelObject *)stream);

	if (camel_folder_change_info_changed(nntp_folder->changes)) {
		changes = nntp_folder->changes;
		nntp_folder->changes = camel_folder_change_info_new();
	} else {
		changes = NULL;
	}

	CAMEL_NNTP_STORE_UNLOCK(nntp_store, command_lock);

	if (changes) {
		camel_object_trigger_event((CamelObject *)folder, "folder_changed", changes);
		camel_folder_change_info_free(changes);
	}

	return NULL;
}

static GPtrArray*
nntp_folder_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	GPtrArray *matches, *summary;

	CAMEL_NNTP_FOLDER_LOCK(nntp_folder, search_lock);

	if(nntp_folder->search == NULL)
		nntp_folder->search = camel_folder_search_new();
	
	camel_folder_search_set_folder(nntp_folder->search, folder);
	summary = camel_folder_get_summary(folder);
	camel_folder_search_set_summary(nntp_folder->search, summary);

	matches = camel_folder_search_execute_expression(nntp_folder->search, expression, ex);

	CAMEL_NNTP_FOLDER_UNLOCK(nntp_folder, search_lock);

	camel_folder_free_summary(folder, summary);

	return matches;
}

static GPtrArray *
nntp_folder_search_by_uids(CamelFolder *folder, const char *expression, GPtrArray *uids, CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER(folder);
	GPtrArray *summary, *matches;
	int i;

	/* NOTE: could get away without the search lock by creating a new
	   search object each time */

	summary = g_ptr_array_new();
	for (i=0;i<uids->len;i++) {
		CamelMessageInfo *info;

		info = camel_folder_get_message_info(folder, uids->pdata[i]);
		if (info)
			g_ptr_array_add(summary, info);
	}

	if (summary->len == 0)
		return summary;

	CAMEL_NNTP_FOLDER_LOCK(folder, search_lock);

	if (nntp_folder->search == NULL)
		nntp_folder->search = camel_folder_search_new();

	camel_folder_search_set_folder(nntp_folder->search, folder);
	camel_folder_search_set_summary(nntp_folder->search, summary);

	matches = camel_folder_search_execute_expression(nntp_folder->search, expression, ex);

	CAMEL_NNTP_FOLDER_UNLOCK(folder, search_lock);

	for (i=0;i<summary->len;i++)
		camel_folder_free_message_info(folder, summary->pdata[i]);
	g_ptr_array_free(summary, TRUE);

	return matches;
}

static void
nntp_folder_search_free(CamelFolder *folder, GPtrArray *result)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);

	camel_folder_search_free_result(nntp_folder->search, result);

}

static void           
nntp_folder_init(CamelNNTPFolder *nntp_folder, CamelNNTPFolderClass *klass)
{
	struct _CamelNNTPFolderPrivate *p;

	nntp_folder->changes = camel_folder_change_info_new();
	p = nntp_folder->priv = g_malloc0(sizeof(*nntp_folder->priv));
	p->search_lock = g_mutex_new();
	p->cache_lock = g_mutex_new();
}

static void           
nntp_folder_finalise (CamelNNTPFolder *nntp_folder)
{
	struct _CamelNNTPFolderPrivate *p;

	g_free(nntp_folder->storage_path);
	
	p = nntp_folder->priv;
	g_mutex_free(p->search_lock);
	g_mutex_free(p->cache_lock);
	g_free(p);
}

static void
nntp_folder_class_init (CamelNNTPFolderClass *camel_nntp_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_nntp_folder_class);

	parent_class = CAMEL_FOLDER_CLASS (camel_type_get_global_classfuncs (camel_folder_get_type ()));
		
	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->sync = nntp_folder_sync;
	camel_folder_class->set_message_flags = nntp_folder_set_message_flags;
	camel_folder_class->get_message = nntp_folder_get_message;
	camel_folder_class->search_by_expression = nntp_folder_search_by_expression;
	camel_folder_class->search_by_uids = nntp_folder_search_by_uids;
	camel_folder_class->search_free = nntp_folder_search_free;
}

CamelType
camel_nntp_folder_get_type (void)
{
	static CamelType camel_nntp_folder_type = CAMEL_INVALID_TYPE;
	
	if (camel_nntp_folder_type == CAMEL_INVALID_TYPE)	{
		camel_nntp_folder_type = camel_type_register (CAMEL_FOLDER_TYPE, "CamelNNTPFolder",
							      sizeof (CamelNNTPFolder),
							      sizeof (CamelNNTPFolderClass),
							      (CamelObjectClassInitFunc) nntp_folder_class_init,
							      NULL,
							      (CamelObjectInitFunc) nntp_folder_init,
							      (CamelObjectFinalizeFunc) nntp_folder_finalise);
	}
	
	return camel_nntp_folder_type;
}


/* not yet */
/* Idea is we update in stages, but this requires a different xover command, etc */
#ifdef ASYNC_SUMMARY
struct _folder_check_msg {
	CamelSessionThreadMsg msg;
	CamelNNTPFolder *folder;
};

static void
folder_check(CamelSession *session, CamelSessionThreadMsg *msg)
{
	struct _folder_check_msg *m = (struct _folder_check_msg *)msg;
	CamelException *ex;
	CamelNNTPStore *nntp_store;

	nntp_store = (CamelNNTPStore *)m->folder->parent.parent_store;

	CAMEL_NNTP_STORE_LOCK(nntp_store, command_lock);

	ex = camel_exception_new();
	camel_nntp_summary_check((CamelNNTPSummary *)m->folder->parent.summary, m->folder->changes, ex);
	camel_exception_free(ex);

	CAMEL_NNTP_STORE_UNLOCK(nntp_store, command_lock);
}

static void
folder_check_free(CamelSession *session, CamelSessionThreadMsg *msg)
{
	struct _folder_check_msg *m = (struct _folder_check_msg *)msg;

	camel_object_unref((CamelObject *)m->folder);
}

static CamelSessionThreadOps folder_check_ops = {
	folder_check,
	folder_check_free,
};
#endif

CamelFolder *
camel_nntp_folder_new (CamelStore *parent, const char *folder_name, CamelException *ex)
{
	CamelFolder *folder;
	CamelNNTPFolder *nntp_folder;
	char *root;
	CamelService *service;
#ifdef ASYNC_SUMMARY
	struct _folder_check_msg *m;
#endif

	service = (CamelService *)parent;
	root = camel_session_get_storage_path(service->session, service, ex);
	if (root == NULL)
		return NULL;

	/* If this doesn't work, stuff wont save, but let it continue anyway */
	camel_mkdir (root, 0777);

	folder = (CamelFolder *) camel_object_new (CAMEL_NNTP_FOLDER_TYPE);
	nntp_folder = (CamelNNTPFolder *)folder;

	camel_folder_construct (folder, parent, folder_name, folder_name);
	folder->folder_flags |= CAMEL_FOLDER_HAS_SUMMARY_CAPABILITY|CAMEL_FOLDER_HAS_SEARCH_CAPABILITY;

	nntp_folder->storage_path = g_strdup_printf("%s/%s", root, folder->full_name);
	g_free(root);

	folder->summary = (CamelFolderSummary *)camel_nntp_summary_new(nntp_folder);
	camel_folder_summary_load (folder->summary);
#ifdef ASYNC_SUMMARY
	m = camel_session_thread_msg_new(service->session, &folder_check_ops, sizeof(*m));
	m->folder = nntp_folder;
	camel_object_ref((CamelObject *)folder);
	camel_session_thread_queue(service->session, &m->msg, 0);
#else
	if (camel_nntp_summary_check((CamelNNTPSummary *)folder->summary, nntp_folder->changes, ex) == -1) {
		camel_object_unref((CamelObject *)folder);
		folder = NULL;
	}
#endif

	return folder;
}
