/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-folder.c : class for a imap folder */

/* 
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2002 Ximian, Inc. (www.ximian.com)
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

#include <errno.h>

#include "camel/camel-exception.h"
#include "camel/camel-stream-mem.h"
#include "camel/camel-stream-filter.h"
#include "camel/camel-mime-message.h"
#include "camel/camel-operation.h"
#include "camel/camel-data-cache.h"
#include "camel/camel-session.h"
#include "camel/camel-file-utils.h"

#include "camel-imapp-store.h"
#include "camel-imapp-folder.h"
#include "camel-imapp-summary.h"
#include "camel-imapp-exception.h"

#include <libedataserver/md5-utils.h>

#include <stdlib.h>
#include <string.h>

#define d(x)

#define CF_CLASS(o) (CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(o)))
static CamelFolderClass *parent_class;

static void imap_finalize (CamelObject *object);
static void imap_refresh_info (CamelFolder *folder, CamelException *ex);
static void imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
static CamelMimeMessage *imap_get_message (CamelFolder *folder, const char *uid, CamelException *ex);

static void
imap_folder_class_init (CamelIMAPPFolderClass *camel_imapp_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS(camel_imapp_folder_class);
	
	parent_class = CAMEL_FOLDER_CLASS(camel_folder_get_type());
	
	/* virtual method overload */
	camel_folder_class->refresh_info = imap_refresh_info;
	camel_folder_class->sync = imap_sync;
	
	camel_folder_class->get_message = imap_get_message;
}

static void
imap_folder_init(CamelObject *o, CamelObjectClass *klass)
{
	CamelFolder *folder = (CamelFolder *)o;
	CamelIMAPPFolder *ifolder = (CamelIMAPPFolder *)o;

	folder->folder_flags |= (CAMEL_FOLDER_HAS_SUMMARY_CAPABILITY |
				 CAMEL_FOLDER_HAS_SEARCH_CAPABILITY);
	
	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED |
		CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_DRAFT |
		CAMEL_MESSAGE_FLAGGED | CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_USER;

	/* FIXME: this is just a skeleton */

	ifolder->changes = camel_folder_change_info_new();
}

CamelType
camel_imapp_folder_get_type (void)
{
	static CamelType camel_imapp_folder_type = CAMEL_INVALID_TYPE;
	
	if (!camel_imapp_folder_type) {
		camel_imapp_folder_type = camel_type_register (CAMEL_FOLDER_TYPE, "CamelIMAPPFolder",
							      sizeof (CamelIMAPPFolder),
							      sizeof (CamelIMAPPFolderClass),
							      (CamelObjectClassInitFunc) imap_folder_class_init,
							      NULL,
							      imap_folder_init,
							      (CamelObjectFinalizeFunc) imap_finalize);
	}
	
	return camel_imapp_folder_type;
}

void
imap_finalize (CamelObject *object)
{
	CamelIMAPPFolder *folder = (CamelIMAPPFolder *)object;

	camel_folder_change_info_free(folder->changes);
}

CamelFolder *
camel_imapp_folder_new(CamelStore *store, const char *path)
{
	CamelFolder *folder;
	char *root;

	d(printf("opening imap folder '%s'\n", path));
	
	folder = CAMEL_FOLDER (camel_object_new (CAMEL_IMAPP_FOLDER_TYPE));
	camel_folder_construct(folder, store, path, path);

	((CamelIMAPPFolder *)folder)->raw_name = g_strdup(path);

	folder->summary = camel_imapp_summary_new();

	root = camel_session_get_storage_path(((CamelService *)store)->session, (CamelService *)store, NULL);
	if (root) {
		char *base = g_build_filename(root, path, NULL);
		char *file = g_build_filename(base, ".ev-summary", NULL);

		camel_mkdir(base, 0777);
		g_free(base);

		camel_folder_summary_set_filename(folder->summary, file);
		printf("loading summary from '%s' (root=%s)\n", file, root);
		g_free(file);
		camel_folder_summary_load(folder->summary);
		g_free(root);
	}

	return folder;
}

#if 0
/* experimental interfaces */
void
camel_imapp_folder_open(CamelIMAPPFolder *folder, CamelException *ex)
{
	/* */
}

void
camel_imapp_folder_delete(CamelIMAPPFolder *folder, CamelException *ex)
{
}

void
camel_imapp_folder_rename(CamelIMAPPFolder *folder, const char *new, CamelException *ex)
{
}

void
camel_imapp_folder_close(CamelIMAPPFolder *folder, CamelException *ex)
{
}
#endif

static void 
imap_refresh_info (CamelFolder *folder, CamelException *ex)
{
	printf("imapp refresh info?\n");
}

static void
imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	camel_imapp_driver_sync(((CamelIMAPPStore *)(folder->parent_store))->driver, expunge, (CamelIMAPPFolder *) folder);
}

static CamelMimeMessage *
imap_get_message (CamelFolder *folder, const char *uid, CamelException *ex)
{
	CamelMimeMessage * volatile msg = NULL;
	CamelStream * volatile stream = NULL;

	printf("get message '%s'\n", uid);

	CAMEL_TRY {
		/* simple implementation, just get whole message in 1 go */
		stream = camel_imapp_driver_fetch(((CamelIMAPPStore *)(folder->parent_store))->driver, (CamelIMAPPFolder *)folder, uid, "");
		camel_stream_reset(stream);
		msg = camel_mime_message_new();
		if (camel_data_wrapper_construct_from_stream((CamelDataWrapper *)msg, stream) != -1) {
			/* do we care? */
		}
	} CAMEL_CATCH(e) {
		if (msg)
			camel_object_unref(msg);
		msg = NULL;
		camel_exception_xfer(ex, e);
	} CAMEL_DONE;

	if (stream)
		camel_object_unref(stream);

	return msg;
}


/* Algorithm for selecting a folder:

  - If uidvalidity == old uidvalidity
    and exsists == old exists
    and recent == old recent
    and unseen == old unseen
    Assume our summary is correct
  for each summary item
    mark the summary item as 'old/not updated'
  rof
  fetch flags from 1:*
  for each fetch response
    info = summary[index]
    if info.uid != uid
      info = summary_by_uid[uid]
    fi
    if info == NULL
      create new info @ index
    fi
    if got.flags
      update flags
    fi
    if got.header
      update based on header
      mark as retrieved
    else if got.body
      update based on imap body
      mark as retrieved
    fi

  Async fetch response:
    info = summary[index]
    if info == null
       if uid == null
          force resync/select?
       info = empty @ index
    else if uid && info.uid != uid
       force a resync?
       return
    fi

    if got.flags {
      info.flags = flags
    }
    if got.header {
      info.init(header)
      info.empty = false
    }

info.state - 2 bit field in flags
   0 = empty, nothing set
   1 = uid & flags set
   2 = update required
   3 = up to date
*/

