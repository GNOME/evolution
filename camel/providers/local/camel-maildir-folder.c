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

#include "camel-maildir-folder.h"
#include "camel-maildir-store.h"
#include "string-utils.h"
#include "camel-stream-fs.h"
#include "camel-maildir-summary.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-exception.h"

#define d(x) (printf("%s(%d): ", __FILE__, __LINE__),(x))

static CamelFolderClass *parent_class = NULL;

/* Returns the class for a CamelMaildirFolder */
#define CMAILDIRF_CLASS(so) CAMEL_MAILDIR_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CMAILDIRS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static CamelLocalSummary *maildir_create_summary(const char *path, const char *folder, ibex *index);

static void maildir_append_message(CamelFolder * folder, CamelMimeMessage * message, const CamelMessageInfo *info, CamelException * ex);
static CamelMimeMessage *maildir_get_message(CamelFolder * folder, const gchar * uid, CamelException * ex);

static void maildir_finalize(CamelObject * object);

static void camel_maildir_folder_class_init(CamelObjectClass * camel_maildir_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS(camel_maildir_folder_class);
	CamelLocalFolderClass *lclass = (CamelLocalFolderClass *)camel_maildir_folder_class;

	parent_class = CAMEL_FOLDER_CLASS (camel_type_get_global_classfuncs(camel_folder_get_type()));

	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->append_message = maildir_append_message;
	camel_folder_class->get_message = maildir_get_message;

	lclass->create_summary = maildir_create_summary;
}

static void maildir_init(gpointer object, gpointer klass)
{
	/*CamelFolder *folder = object;
	  CamelMaildirFolder *maildir_folder = object;*/
}

static void maildir_finalize(CamelObject * object)
{
	/*CamelMaildirFolder *maildir_folder = CAMEL_MAILDIR_FOLDER(object);*/
}

CamelType camel_maildir_folder_get_type(void)
{
	static CamelType camel_maildir_folder_type = CAMEL_INVALID_TYPE;

	if (camel_maildir_folder_type == CAMEL_INVALID_TYPE) {
		camel_maildir_folder_type = camel_type_register(CAMEL_LOCAL_FOLDER_TYPE, "CamelMaildirFolder",
							   sizeof(CamelMaildirFolder),
							   sizeof(CamelMaildirFolderClass),
							   (CamelObjectClassInitFunc) camel_maildir_folder_class_init,
							   NULL,
							   (CamelObjectInitFunc) maildir_init,
							   (CamelObjectFinalizeFunc) maildir_finalize);
	}
 
	return camel_maildir_folder_type;
}

CamelFolder *
camel_maildir_folder_new(CamelStore *parent_store, const char *full_name, guint32 flags, CamelException *ex)
{
	CamelFolder *folder;

	d(printf("Creating maildir folder: %s\n", full_name));

	folder = (CamelFolder *)camel_object_new(CAMEL_MAILDIR_FOLDER_TYPE);
	folder = (CamelFolder *)camel_local_folder_construct((CamelLocalFolder *)folder,
							     parent_store, full_name, flags, ex);

	return folder;
}

static CamelLocalSummary *maildir_create_summary(const char *path, const char *folder, ibex *index)
{
	return (CamelLocalSummary *)camel_maildir_summary_new(path, folder, index);
}

static void maildir_append_message(CamelFolder * folder, CamelMimeMessage * message, const CamelMessageInfo *info, CamelException * ex)
{
	CamelMaildirFolder *maildir_folder = (CamelMaildirFolder *)folder;
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	CamelStream *output_stream;
	CamelMessageInfo *mi;
	CamelMaildirMessageInfo *mdi;
	char *name, *dest;

	d(printf("Appending message\n"));

	/* add it to the summary/assign the uid, etc */
	mi = camel_local_summary_add(lf->summary, message, info, lf->changes, ex);
	if (camel_exception_is_set(ex)) {
		return;
	}

	mdi = (CamelMaildirMessageInfo *)mi;

	g_assert(mdi->filename);

	d(printf("Appending message: uid is %s filename is %s\n", mi->uid, mdi->filename));

	/* write it out to tmp, use the uid we got from the summary */
	name = g_strdup_printf("%s/tmp/%s", lf->folder_path, mi->uid);
	output_stream = camel_stream_fs_new_with_name(name, O_WRONLY|O_CREAT, 0600);
	if (output_stream == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot append message to maildir folder: %s: %s"), name, g_strerror(errno));
		g_free(name);
		return;
	}

	if (camel_data_wrapper_write_to_stream((CamelDataWrapper *)message, output_stream) == -1
	    || camel_stream_close(output_stream) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot append message to maildir folder: %s: %s"), name, g_strerror(errno));
		camel_object_unref((CamelObject *)output_stream);
		unlink(name);
		g_free(name);
		return;
	}

	/* now move from tmp to cur (bypass new, does it matter?) */
	dest = g_strdup_printf("%s/cur/%s", lf->folder_path, mdi->filename);
	if (rename(name, dest) == 1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot append message to maildir folder: %s: %s"), name, g_strerror(errno));
		camel_object_unref((CamelObject *)output_stream);	
		unlink(name);
		g_free(name);
		g_free(dest);
		return;
	}

	g_free(dest);
	g_free(name);

	camel_object_trigger_event((CamelObject *)folder, "folder_changed", ((CamelLocalFolder *)maildir_folder)->changes);
	camel_folder_change_info_clear(((CamelLocalFolder *)maildir_folder)->changes);
}

static CamelMimeMessage *maildir_get_message(CamelFolder * folder, const gchar * uid, CamelException * ex)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	CamelStream *message_stream = NULL;
	CamelMimeMessage *message = NULL;
	CamelMessageInfo *info;
	char *name;
	CamelMaildirMessageInfo *mdi;

	d(printf("getting message: %s\n", uid));

	/* get the message summary info */
	if ((info = camel_folder_summary_uid((CamelFolderSummary *)lf->summary, uid)) == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID, _("Cannot get message: %s\n  %s"), uid, _("No such message"));
		return NULL;
	}

	mdi = (CamelMaildirMessageInfo *)info;

	/* what do we do if the message flags (and :info data) changes?  filename mismatch - need to recheck I guess */
	name = g_strdup_printf("%s/cur/%s", lf->folder_path, mdi->filename);
	if ((message_stream = camel_stream_fs_new_with_name(name, O_RDONLY, 0)) == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID, _("Cannot get message: %s\n  %s"),
				     name, g_strerror(errno));
		g_free(name);
		return NULL;
	}

	message = camel_mime_message_new();
	if (camel_data_wrapper_construct_from_stream((CamelDataWrapper *)message, message_stream) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID, _("Cannot get message: %s\n  %s"),
				     name, _("Invalid message contents"));
		g_free(name);
		camel_object_unref((CamelObject *)message_stream);
		camel_object_unref((CamelObject *)message);
		return NULL;

	}
	camel_object_unref((CamelObject *)message_stream);
	g_free(name);

	return message;
}
