/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-mh-folder.c : Abstract class for an email folder */

/* 
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

#include "camel-mh-folder.h"
#include "camel-mh-store.h"
#include "string-utils.h"
#include "camel-stream-fs.h"
#include "camel-mh-summary.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-exception.h"

#define d(x)

static CamelFolderClass *parent_class = NULL;

/* Returns the class for a CamelMhFolder */
#define CMHF_CLASS(so) CAMEL_MH_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CMHS_CLASS(so) CAMEL_STORE_CLASS (GTK_OBJECT(so)->klass)

static void mh_init(CamelFolder * folder, CamelStore * parent_store,
		      CamelFolder * parent_folder, const gchar * name,
		      gchar * separator, gboolean path_begins_with_sep, CamelException * ex);

static void mh_sync(CamelFolder * folder, gboolean expunge, CamelException * ex);
static gint mh_get_message_count(CamelFolder * folder);
static gint mh_get_unread_message_count(CamelFolder * folder);
static void mh_append_message(CamelFolder * folder, CamelMimeMessage * message, guint32 flags, CamelException * ex);
static GPtrArray *mh_get_uids(CamelFolder * folder);
static GPtrArray *mh_get_subfolder_names(CamelFolder * folder);
static GPtrArray *mh_get_summary(CamelFolder * folder);
static CamelMimeMessage *mh_get_message(CamelFolder * folder, const gchar * uid, CamelException * ex);

static void mh_expunge(CamelFolder * folder, CamelException * ex);

static const CamelMessageInfo *mh_get_message_info(CamelFolder * folder, const char *uid);

static GPtrArray *mh_search_by_expression(CamelFolder * folder, const char *expression, CamelException * ex);

static guint32 mh_get_message_flags(CamelFolder * folder, const char *uid);
static void mh_set_message_flags(CamelFolder * folder, const char *uid, guint32 flags, guint32 set);
static gboolean mh_get_message_user_flag(CamelFolder * folder, const char *uid, const char *name);
static void mh_set_message_user_flag(CamelFolder * folder, const char *uid, const char *name, gboolean value);

static void mh_finalize(GtkObject * object);

static void camel_mh_folder_class_init(CamelMhFolderClass * camel_mh_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS(camel_mh_folder_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS(camel_folder_class);

	parent_class = gtk_type_class(camel_folder_get_type());

	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->init = mh_init;
	camel_folder_class->sync = mh_sync;
	camel_folder_class->get_message_count = mh_get_message_count;
	camel_folder_class->get_unread_message_count = mh_get_unread_message_count;
	camel_folder_class->append_message = mh_append_message;
	camel_folder_class->get_uids = mh_get_uids;
	camel_folder_class->free_uids = camel_folder_free_deep;
	camel_folder_class->get_subfolder_names = mh_get_subfolder_names;
	camel_folder_class->free_subfolder_names = camel_folder_free_deep;
	camel_folder_class->get_summary = mh_get_summary;
	camel_folder_class->free_summary = camel_folder_free_nop;
	camel_folder_class->expunge = mh_expunge;

	camel_folder_class->get_message = mh_get_message;

	camel_folder_class->search_by_expression = mh_search_by_expression;

	camel_folder_class->get_message_info = mh_get_message_info;

	camel_folder_class->get_message_flags = mh_get_message_flags;
	camel_folder_class->set_message_flags = mh_set_message_flags;
	camel_folder_class->get_message_user_flag = mh_get_message_user_flag;
	camel_folder_class->set_message_user_flag = mh_set_message_user_flag;

	gtk_object_class->finalize = mh_finalize;
}

static void mh_finalize(GtkObject * object)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(object);

	g_free(mh_folder->folder_file_path);
	g_free(mh_folder->summary_file_path);
	g_free(mh_folder->folder_dir_path);
	g_free(mh_folder->index_file_path);

	GTK_OBJECT_CLASS(parent_class)->finalize(object);
}

GtkType camel_mh_folder_get_type(void)
{
	static GtkType camel_mh_folder_type = 0;

	if (!camel_mh_folder_type) {
		GtkTypeInfo camel_mh_folder_info = {
			"CamelMhFolder",
			sizeof(CamelMhFolder),
			sizeof(CamelMhFolderClass),
			(GtkClassInitFunc) camel_mh_folder_class_init,
			(GtkObjectInitFunc) NULL,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_mh_folder_type = gtk_type_unique(CAMEL_FOLDER_TYPE, &camel_mh_folder_info);
	}

	return camel_mh_folder_type;
}

static void
mh_init(CamelFolder * folder, CamelStore * parent_store,
	  CamelFolder * parent_folder, const gchar * name, gchar * separator,
	  gboolean path_begins_with_sep, CamelException * ex)
{
	CamelMhFolder *mh_folder = (CamelMhFolder *) folder;
	const gchar *root_dir_path;
	gchar *real_name;
	int forceindex;
	struct stat st;

	/* call parent method */
	parent_class->init(folder, parent_store, parent_folder, name, separator, path_begins_with_sep, ex);
	if (camel_exception_get_id(ex))
		return;

	/* we assume that the parent init method checks for the existance of @folder */
	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = TRUE;
	folder->has_summary_capability = TRUE;
	folder->has_search_capability = TRUE;

	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED |
	    CAMEL_MESSAGE_DELETED |
	    CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_FLAGGED | CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_USER;

	mh_folder->summary = NULL;
	mh_folder->search = NULL;

	/* now set the name info */
	g_free(mh_folder->folder_file_path);
	g_free(mh_folder->folder_dir_path);
	g_free(mh_folder->index_file_path);

	root_dir_path = camel_mh_store_get_toplevel_dir(CAMEL_MH_STORE(folder->parent_store));

	real_name = g_basename(folder->full_name);
	mh_folder->folder_file_path = g_strdup_printf("%s/%s", root_dir_path, real_name);
	mh_folder->summary_file_path = g_strdup_printf("%s/%s/ev-summary", root_dir_path, real_name);
	mh_folder->folder_dir_path = g_strdup_printf("%s/%s", root_dir_path, real_name);
	mh_folder->index_file_path = g_strdup_printf("%s/%s/ev-index.ibex", root_dir_path, real_name);

	/* if we have no index file, force it */
	forceindex = stat(mh_folder->index_file_path, &st) == -1;

	mh_folder->index = ibex_open(mh_folder->index_file_path, O_CREAT | O_RDWR, 0600);
	if (mh_folder->index == NULL) {
		/* yes, this isn't fatal at all */
		g_warning("Could not open/create index file: %s: indexing not performed", strerror(errno));
	}

	/* no summary (disk or memory), and we're proverbially screwed */
	mh_folder->summary = camel_mh_summary_new(mh_folder->summary_file_path,
						  mh_folder->folder_file_path,
						  mh_folder->index);

	if (camel_mh_summary_load(mh_folder->summary, forceindex) == -1) {
		camel_exception_set(ex, CAMEL_EXCEPTION_FOLDER_INVALID,	/* FIXME: right error code */
				    "Could not load or create summary");
		return;
	}
}

static void mh_sync(CamelFolder * folder, gboolean expunge, CamelException * ex)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);

	if (expunge)
		mh_expunge(folder, ex);
	else
		camel_mh_summary_sync(mh_folder->summary, FALSE, ex);

	/* save index */
	if (mh_folder->index)
		ibex_save(mh_folder->index);
	if (mh_folder->summary)
		camel_folder_summary_save(CAMEL_FOLDER_SUMMARY(mh_folder->summary));
}

static void mh_expunge(CamelFolder * folder, CamelException * ex)
{
	CamelMhFolder *mh = CAMEL_MH_FOLDER(folder);

	camel_mh_summary_sync(mh->summary, TRUE, ex);

	/* TODO: check it actually changed */
	gtk_signal_emit_by_name(GTK_OBJECT(folder), "folder_changed", 0);
}

static gint mh_get_message_count(CamelFolder * folder)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);

	g_return_val_if_fail(mh_folder->summary != NULL, -1);

	return camel_folder_summary_count(CAMEL_FOLDER_SUMMARY(mh_folder->summary));
}

static gint mh_get_unread_message_count(CamelFolder * folder)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	CamelMessageInfo *info;
	GPtrArray *infolist;
	gint i, count = 0;

	g_return_val_if_fail(mh_folder->summary != NULL, -1);

	infolist = mh_get_summary(folder);

	for (i = 0; i < infolist->len; i++) {
		info = (CamelMessageInfo *) g_ptr_array_index(infolist, i);
		if (!(info->flags & CAMEL_MESSAGE_SEEN))
			count++;
	}

	return count;
}

/* FIXME: this may need some tweaking for performance? */
static void mh_append_message(CamelFolder * folder, CamelMimeMessage * message, guint32 flags, CamelException * ex)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	CamelStream *output_stream = NULL;
	char *name = NULL;
	char *uid = NULL;

	/* FIXME: probably needs additional locking */

	/* keep trying uid's until we find one thats ok */
	do {
		g_free(uid);
		g_free(name);
		uid = camel_folder_summary_next_uid_string((CamelFolderSummary *)mh_folder->summary);
		name = g_strdup_printf("%s/%s", mh_folder->folder_file_path, uid);
		output_stream = camel_stream_fs_new_with_name(name, O_WRONLY|O_CREAT|O_EXCL, 0600);
	} while (output_stream == NULL || errno != EEXIST);

	if (output_stream == NULL)
		goto fail;

	/* write the message */
	if (camel_data_wrapper_write_to_stream(CAMEL_DATA_WRAPPER(message), output_stream) == -1)
		goto fail;

	if (camel_stream_close(output_stream) == -1)
		goto fail;

	/* index/summarise the message.  Yes this re-reads it, its just simpler */
	camel_mh_summary_add(mh_folder->summary, uid, TRUE);
	gtk_signal_emit_by_name(GTK_OBJECT(folder), "folder_changed", 0);
	g_free(name);
	g_free(uid);
	return;

fail:
	if (camel_exception_is_set(ex)) {
		camel_exception_setv(ex, camel_exception_get_id(ex),
				     "Cannot append message to mh file: %s", camel_exception_get_description(ex));
	} else {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     "Cannot append message to mh file: %s", g_strerror(errno));
	}
	if (output_stream)
		gtk_object_unref(GTK_OBJECT(output_stream));
	if (name) {
		unlink(name);
		g_free(name);
	}
	g_free(uid);
}

static GPtrArray *mh_get_uids(CamelFolder * folder)
{
	GPtrArray *array;
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	int i, count;

	count = camel_folder_summary_count(CAMEL_FOLDER_SUMMARY(mh_folder->summary));
	array = g_ptr_array_new();
	g_ptr_array_set_size(array, count);
	for (i = 0; i < count; i++) {
		CamelMessageInfo *info =
			camel_folder_summary_index(CAMEL_FOLDER_SUMMARY(mh_folder->summary), i);

		array->pdata[i] = g_strdup(info->uid);
	}

	return array;
}

static GPtrArray *mh_get_subfolder_names(CamelFolder * folder)
{
	/* FIXME: scan for sub-folders */
	/* No subfolders. */
	return g_ptr_array_new();
}

static CamelMimeMessage *mh_get_message(CamelFolder * folder, const gchar * uid, CamelException * ex)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	CamelStream *message_stream = NULL;
	CamelMimeMessage *message = NULL;
	CamelMessageInfo *info;
	char *name;

	name = g_strdup_printf("%s/%s", mh_folder->folder_file_path, uid);

	/* get the message summary info */
	info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mh_folder->summary), uid);

	if (info == NULL) {
		errno = ENOENT;
		goto fail;
	}

	message_stream = camel_stream_fs_new_with_name(name, O_RDONLY, 0);

	/* where we read from */
	if (message_stream == NULL)
		goto fail;

	message = camel_mime_message_new();
	if (camel_data_wrapper_construct_from_stream(CAMEL_DATA_WRAPPER(message), message_stream) == -1) {
		g_warning("Construction failed");
		errno = EINVAL;
		goto fail;
	}
	gtk_object_unref(GTK_OBJECT(message_stream));
	g_free(name);

	return message;

fail:
	camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID, "Cannot get message: %s\n  %s",
			     name, g_strerror(errno));

	if (message_stream)
		gtk_object_unref(GTK_OBJECT(message_stream));

	if (message)
		gtk_object_unref(GTK_OBJECT(message));

	g_free(name);

	return NULL;
}

GPtrArray *mh_get_summary(CamelFolder * folder)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);

	return CAMEL_FOLDER_SUMMARY(mh_folder->summary)->messages;
}

/* get a single message info, by uid */
static const CamelMessageInfo *mh_get_message_info(CamelFolder * folder, const char *uid)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);

	return camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mh_folder->summary), uid);
}

static GPtrArray *mh_search_by_expression(CamelFolder * folder, const char *expression, CamelException * ex)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);

	if (mh_folder->search == NULL) {
		mh_folder->search = camel_folder_search_new();
	}

	camel_folder_search_set_folder(mh_folder->search, folder);
	if (mh_folder->summary) {
		/* FIXME: dont access summary array directly? */
		camel_folder_search_set_summary(mh_folder->search,
						CAMEL_FOLDER_SUMMARY(mh_folder->summary)->messages);
	}

	camel_folder_search_set_body_index(mh_folder->search, mh_folder->index);

	return camel_folder_search_execute_expression(mh_folder->search, expression, ex);
}

static guint32 mh_get_message_flags(CamelFolder * folder, const char *uid)
{
	CamelMessageInfo *info;
	CamelMhFolder *mf = CAMEL_MH_FOLDER(folder);

	info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mf->summary), uid);
	g_return_val_if_fail(info != NULL, 0);

	return info->flags;
}

static void mh_set_message_flags(CamelFolder * folder, const char *uid, guint32 flags, guint32 set)
{
	CamelMessageInfo *info;
	CamelMhFolder *mf = CAMEL_MH_FOLDER(folder);

	info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mf->summary), uid);
	g_return_if_fail(info != NULL);

	info->flags = (info->flags & ~flags) | (set & flags) | CAMEL_MESSAGE_FOLDER_FLAGGED;
	camel_folder_summary_touch(CAMEL_FOLDER_SUMMARY(mf->summary));

	gtk_signal_emit_by_name(GTK_OBJECT(folder), "message_changed", uid);
}

static gboolean mh_get_message_user_flag(CamelFolder * folder, const char *uid, const char *name)
{
	CamelMessageInfo *info;
	CamelMhFolder *mf = CAMEL_MH_FOLDER(folder);

	info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mf->summary), uid);
	g_return_val_if_fail(info != NULL, FALSE);

	return camel_flag_get(&info->user_flags, name);
}

static void mh_set_message_user_flag(CamelFolder * folder, const char *uid, const char *name, gboolean value)
{
	CamelMessageInfo *info;
	CamelMhFolder *mf = CAMEL_MH_FOLDER(folder);

	info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY(mf->summary), uid);
	g_return_if_fail(info != NULL);

	camel_flag_set(&info->user_flags, name, value);
	info->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED;
	camel_folder_summary_touch(CAMEL_FOLDER_SUMMARY(mf->summary));
	gtk_signal_emit_by_name(GTK_OBJECT(folder), "message_changed", uid);
}
