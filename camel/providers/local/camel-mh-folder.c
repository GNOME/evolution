/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999, 2003 Ximian Inc.
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
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "camel-mh-folder.h"
#include "camel-mh-store.h"
#include "camel-stream-fs.h"
#include "camel-mh-summary.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-exception.h"
#include "camel-i18n.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

static CamelLocalFolderClass *parent_class = NULL;

/* Returns the class for a CamelMhFolder */
#define CMHF_CLASS(so) CAMEL_MH_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CMHS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static CamelLocalSummary *mh_create_summary(const char *path, const char *folder, CamelIndex *index);

static void mh_append_message(CamelFolder * folder, CamelMimeMessage * message, const CamelMessageInfo *info, char **appended_uid, CamelException * ex);
static CamelMimeMessage *mh_get_message(CamelFolder * folder, const gchar * uid, CamelException * ex);

static void mh_finalize(CamelObject * object);

static void camel_mh_folder_class_init(CamelObjectClass * camel_mh_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS(camel_mh_folder_class);
	CamelLocalFolderClass *lclass = (CamelLocalFolderClass *)camel_mh_folder_class;

	parent_class = CAMEL_LOCAL_FOLDER_CLASS (camel_type_get_global_classfuncs(camel_local_folder_get_type()));

	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->append_message = mh_append_message;
	camel_folder_class->get_message = mh_get_message;

	lclass->create_summary = mh_create_summary;
}

static void mh_init(gpointer object, gpointer klass)
{
	/*CamelFolder *folder = object;
	  CamelMhFolder *mh_folder = object;*/
}

static void mh_finalize(CamelObject * object)
{
	/*CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(object);*/
}

CamelType camel_mh_folder_get_type(void)
{
	static CamelType camel_mh_folder_type = CAMEL_INVALID_TYPE;

	if (camel_mh_folder_type == CAMEL_INVALID_TYPE) {
		camel_mh_folder_type = camel_type_register(CAMEL_LOCAL_FOLDER_TYPE, "CamelMhFolder",
							   sizeof(CamelMhFolder),
							   sizeof(CamelMhFolderClass),
							   (CamelObjectClassInitFunc) camel_mh_folder_class_init,
							   NULL,
							   (CamelObjectInitFunc) mh_init,
							   (CamelObjectFinalizeFunc) mh_finalize);
	}

	return camel_mh_folder_type;
}

CamelFolder *
camel_mh_folder_new(CamelStore *parent_store, const char *full_name, guint32 flags, CamelException *ex)
{
	CamelFolder *folder;

	d(printf("Creating mh folder: %s\n", full_name));

	folder = (CamelFolder *)camel_object_new(CAMEL_MH_FOLDER_TYPE);
	folder = (CamelFolder *)camel_local_folder_construct((CamelLocalFolder *)folder,
							     parent_store, full_name, flags, ex);

	return folder;
}

static CamelLocalSummary *mh_create_summary(const char *path, const char *folder, CamelIndex *index)
{
	return (CamelLocalSummary *)camel_mh_summary_new(path, folder, index);
}

static void
mh_append_message (CamelFolder *folder, CamelMimeMessage *message, const CamelMessageInfo *info, char **appended_uid, CamelException *ex)
{
	CamelMhFolder *mh_folder = (CamelMhFolder *)folder;
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	CamelStream *output_stream;
	CamelMessageInfo *mi;
	char *name;

	/* FIXME: probably needs additional locking (although mh doesn't appear do do it) */

	d(printf("Appending message\n"));

	/* add it to the summary/assign the uid, etc */
	mi = camel_local_summary_add((CamelLocalSummary *)folder->summary, message, info, lf->changes, ex);
	if (camel_exception_is_set (ex))
		return;
	
	d(printf("Appending message: uid is %s\n", camel_message_info_uid(mi)));
	
	/* write it out, use the uid we got from the summary */
	name = g_strdup_printf("%s/%s", lf->folder_path, camel_message_info_uid(mi));
	output_stream = camel_stream_fs_new_with_name(name, O_WRONLY|O_CREAT, 0600);
	if (output_stream == NULL)
		goto fail_write;
	
	if (camel_data_wrapper_write_to_stream ((CamelDataWrapper *)message, output_stream) == -1
	    || camel_stream_close (output_stream) == -1)
		goto fail_write;
	
	/* close this? */
	camel_object_unref (CAMEL_OBJECT (output_stream));

	g_free(name);
	
	camel_object_trigger_event (CAMEL_OBJECT (folder), "folder_changed",
				    ((CamelLocalFolder *)mh_folder)->changes);
	camel_folder_change_info_clear (((CamelLocalFolder *)mh_folder)->changes);
	
	if (appended_uid)
		*appended_uid = g_strdup(camel_message_info_uid(mi));

	return;
	
 fail_write:
	
	/* remove the summary info so we are not out-of-sync with the mh folder */
	camel_folder_summary_remove_uid (CAMEL_FOLDER_SUMMARY (folder->summary),
					 camel_message_info_uid (mi));
	
	if (errno == EINTR)
		camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
				     _("MH append message cancelled"));
	else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot append message to mh folder: %s: %s"),
				      name, g_strerror (errno));
	
	if (output_stream) {
		camel_object_unref (CAMEL_OBJECT (output_stream));
		unlink (name);
	}
	
	g_free (name);
}

static CamelMimeMessage *mh_get_message(CamelFolder * folder, const gchar * uid, CamelException * ex)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	CamelStream *message_stream = NULL;
	CamelMimeMessage *message = NULL;
	CamelMessageInfo *info;
	char *name;

	d(printf("getting message: %s\n", uid));

	/* get the message summary info */
	if ((info = camel_folder_summary_uid(folder->summary, uid)) == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     _("Cannot get message: %s from folder %s\n  %s"), uid, lf->folder_path,
				     _("No such message"));
		return NULL;
	}

	/* we only need it to check the message exists */
	camel_folder_summary_info_free(folder->summary, info);

	name = g_strdup_printf("%s/%s", lf->folder_path, uid);
	if ((message_stream = camel_stream_fs_new_with_name(name, O_RDONLY, 0)) == NULL) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot get message: %s from folder %s\n  %s"), name, lf->folder_path,
				      g_strerror (errno));
		g_free(name);
		return NULL;
	}

	message = camel_mime_message_new();
	if (camel_data_wrapper_construct_from_stream((CamelDataWrapper *)message, message_stream) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot get message: %s from folder %s\n  %s"), name, lf->folder_path,
				      _("Message construction failed."));
		g_free(name);
		camel_object_unref((CamelObject *)message_stream);
		camel_object_unref((CamelObject *)message);
		return NULL;

	}
	camel_object_unref((CamelObject *)message_stream);
	g_free(name);

	return message;
}
