/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-digest-folder.h"

#include "camel-exception.h"
#include "camel-multipart.h"
#include "camel-mime-message.h"
#include "camel-folder-summary.h"

#define d(x)

#define _PRIVATE(o) (((CamelDigestFolder *)(o))->priv)

struct _CamelDigestFolderPrivate {
	CamelMimeMessage *message;
	GHashTable *info_hash;
	GPtrArray *summary;
	GPtrArray *uids;
};

static CamelFolderClass *parent_class = NULL;

static void digest_refresh_info (CamelFolder *folder, CamelException *ex);
static void digest_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
static const char *digest_get_full_name (CamelFolder *folder);
static void digest_expunge (CamelFolder *folder, CamelException *ex);

static GPtrArray *digest_get_uids (CamelFolder *folder);
static void digest_free_uids (CamelFolder *folder, GPtrArray *uids);
static CamelMessageInfo *digest_get_message_info (CamelFolder *folder, const char *uid);

/* message manipulation */
static CamelMimeMessage *digest_get_message (CamelFolder *folder, const gchar *uid,
					   CamelException *ex);
static void digest_append_message (CamelFolder *folder, CamelMimeMessage *message,
				 const CamelMessageInfo *info, CamelException *ex);
static void digest_copy_messages_to (CamelFolder *source, GPtrArray *uids,
				   CamelFolder *destination, CamelException *ex);
static void digest_move_messages_to (CamelFolder *source, GPtrArray *uids,
				   CamelFolder *destination, CamelException *ex);


static void
camel_digest_folder_class_init (CamelDigestFolderClass *camel_digest_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_digest_folder_class);
	
	parent_class = CAMEL_FOLDER_CLASS (camel_type_get_global_classfuncs (camel_folder_get_type ()));
	
	/* virtual method definition */
	
	/* virtual method overload */
	camel_folder_class->refresh_info = digest_refresh_info;
	camel_folder_class->sync = digest_sync;
	camel_folder_class->expunge = digest_expunge;
	camel_folder_class->get_full_name = digest_get_full_name;
	
	camel_folder_class->get_uids = digest_get_uids;
	camel_folder_class->free_uids = digest_free_uids;
	camel_folder_class->get_message_info = digest_get_message_info;
	
	camel_folder_class->get_message = digest_get_message;
	camel_folder_class->append_message = digest_append_message;
	camel_folder_class->copy_messages_to = digest_copy_messages_to;
	camel_folder_class->move_messages_to = digest_move_messages_to;
}

static void
camel_digest_folder_init (gpointer object, gpointer klass)
{
	CamelDigestFolder *digest_folder = CAMEL_DIGEST_FOLDER (object);
	CamelFolder *folder = CAMEL_FOLDER (object);
	
	folder->has_summary_capability = TRUE;
	folder->has_search_capability = FALSE;
	
	digest_folder->priv = g_new0 (struct _CamelDigestFolderPrivate, 1);
	digest_folder->priv->info_hash = g_hash_table_new (g_str_hash, g_str_equal);
}

static void           
digest_finalize (CamelObject *object)
{
	CamelDigestFolder *digest_folder = CAMEL_DIGEST_FOLDER (object);
	GPtrArray *summary;
	
	camel_object_unref (CAMEL_OBJECT (digest_folder->priv->message));
	
	g_hash_table_destroy (digest_folder->priv->info_hash);
	
	summary = digest_folder->priv->summary;
	if (summary) {
		int i;
		
		for (i = 0; i < summary->len; i++)
			camel_message_info_free (summary->pdata[i]);
		
		g_ptr_array_free (summary, TRUE);
	}
	
	if (digest_folder->priv->uids)
		g_ptr_array_free (digest_folder->priv->uids, TRUE);
	
	g_free (digest_folder->priv);
}

CamelType
camel_digest_folder_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (CAMEL_FOLDER_TYPE,
					    "CamelDigestFolder",
					    sizeof (CamelDigestFolder),
					    sizeof (CamelDigestFolderClass),
					    (CamelObjectClassInitFunc) camel_digest_folder_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_digest_folder_init,
					    (CamelObjectFinalizeFunc) digest_finalize);
	}
	
	return type;
}

CamelFolder *
camel_digest_folder_new (CamelMimeMessage *message)
{
	CamelDigestFolder *digest_folder;
	CamelDataWrapper *wrapper;
	CamelFolder *folder;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	if (!wrapper || !CAMEL_IS_MULTIPART (wrapper))
		return NULL;
	
	if (!header_content_type_is (CAMEL_MIME_PART (message)->content_type, "multipart", "digest")) {
		int i, parts;
		
		/* Make sure we have a multipart of message/rfc822 attachments... */
		parts = camel_multipart_get_number (CAMEL_MULTIPART (wrapper));
		for (i = 0; i < parts; i++) {
			CamelMimePart *part = camel_multipart_get_part (CAMEL_MULTIPART (wrapper), i);
			
			if (!header_content_type_is (part->content_type, "message", "rfc822"))
				return NULL;
		}
	}
	
	folder = CAMEL_FOLDER (camel_object_new (camel_digest_folder_get_type ()));
	digest_folder = CAMEL_DIGEST_FOLDER (folder);
	
	camel_folder_construct (folder, NULL, "folder_name", "short_name");
	
	camel_object_ref (CAMEL_OBJECT (message));
	digest_folder->priv->message = message;
	
	return folder;
}

static void
digest_refresh_info (CamelFolder *folder, CamelException *ex)
{
	
}

static void
digest_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	
}

static void
digest_expunge (CamelFolder *folder, CamelException *ex)
{
	
}

static GPtrArray *
digest_get_uids (CamelFolder *folder)
{
	CamelDigestFolder *digest_folder = CAMEL_DIGEST_FOLDER (folder);
	CamelDataWrapper *wrapper;
	GHashTable *info_hash;
	GPtrArray *summary;
	GPtrArray *uids;
	int parts, i;
	
	if (digest_folder->priv->uids)
		return digest_folder->priv->uids;
	
	uids = g_ptr_array_new ();
	summary = g_ptr_array_new ();
	info_hash = digest_folder->priv->info_hash;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (digest_folder->priv->message));
	parts = camel_multipart_get_number (CAMEL_MULTIPART (wrapper));
	for (i = 0; i < parts; i++) {
		CamelMimeMessage *message;
		CamelMessageInfo *info;
		CamelMimePart *part;
		char *uid;
		
		uid = g_strdup_printf ("%d", i + 1);
		
		part = camel_multipart_get_part (CAMEL_MULTIPART (wrapper), i);
		message = CAMEL_MIME_MESSAGE (part);
		
		info = camel_message_info_new_from_header (CAMEL_MIME_PART (message)->headers);
		camel_message_info_set_uid (info, uid);
		
		g_ptr_array_add (uids, uid);
		g_ptr_array_add (summary, info);
		g_hash_table_insert (info_hash, uid, info);
	}
	
	digest_folder->priv->uids = uids;
	digest_folder->priv->summary = summary;
	
	return uids;
}

static void
digest_free_uids (CamelFolder *folder, GPtrArray *uids)
{
	/* no-op */
}

static CamelMessageInfo *
digest_get_message_info (CamelFolder *folder, const char *uid)
{
	CamelDigestFolder *digest = CAMEL_DIGEST_FOLDER (folder);
	
	return g_hash_table_lookup (digest->priv->info_hash, uid);
}

static const char *
digest_get_full_name (CamelFolder *folder)
{
	return folder->full_name;
}

static void
digest_append_message (CamelFolder *folder, CamelMimeMessage *message,
		       const CamelMessageInfo *info, CamelException *ex)
{
	/* no-op */
}

static void
digest_copy_messages_to (CamelFolder *source, GPtrArray *uids,
		       CamelFolder *destination, CamelException *ex)
{
	/* no-op */
}

static void
digest_move_messages_to (CamelFolder *source, GPtrArray *uids,
			 CamelFolder *destination, CamelException *ex)
{
	/* no-op */
}

static CamelMimeMessage *
digest_get_message (CamelFolder *folder, const char *uid, CamelException *ex)
{
	CamelDigestFolder *digest = CAMEL_DIGEST_FOLDER (folder);
	CamelDataWrapper *wrapper;
	CamelMimeMessage *message;
	CamelMimePart *part;
	int id;
	
	id = atoi (uid) - 1;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (digest->priv->message));
	part = camel_multipart_get_part (CAMEL_MULTIPART (wrapper), id);
	message = CAMEL_MIME_MESSAGE (part);
	camel_object_ref (CAMEL_OBJECT (message));
	
	return message;
}
