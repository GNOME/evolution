/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2003 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-digest-folder.h"
#include "camel-digest-summary.h"

#include "camel-exception.h"
#include "camel-multipart.h"
#include "camel-mime-message.h"
#include "camel-folder-search.h"

#define d(x)

#define _PRIVATE(o) (((CamelDigestFolder *)(o))->priv)

struct _CamelDigestFolderPrivate {
	CamelMimeMessage *message;
	CamelFolderSearch *search;
	GMutex *search_lock;
};

#define CAMEL_DIGEST_FOLDER_LOCK(f, l) (g_mutex_lock(((CamelDigestFolder *)f)->priv->l))
#define CAMEL_DIGEST_FOLDER_UNLOCK(f, l) (g_mutex_unlock(((CamelDigestFolder *)f)->priv->l))

static CamelFolderClass *parent_class = NULL;

static void digest_refresh_info (CamelFolder *folder, CamelException *ex);
static void digest_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
static const char *digest_get_full_name (CamelFolder *folder);
static void digest_expunge (CamelFolder *folder, CamelException *ex);

/* message manipulation */
static CamelMimeMessage *digest_get_message (CamelFolder *folder, const gchar *uid,
					     CamelException *ex);
static void digest_append_message (CamelFolder *folder, CamelMimeMessage *message,
				   const CamelMessageInfo *info, char **appended_uid, CamelException *ex);
static void digest_transfer_messages_to (CamelFolder *source, GPtrArray *uids,
					 CamelFolder *dest, GPtrArray **transferred_uids,
					 gboolean delete_originals, CamelException *ex);

static GPtrArray *digest_search_by_expression (CamelFolder *folder, const char *expression,
					       CamelException *ex);

static GPtrArray *digest_search_by_uids (CamelFolder *folder, const char *expression,
					 GPtrArray *uids, CamelException *ex);

static void digest_search_free (CamelFolder *folder, GPtrArray *result);

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
	
	camel_folder_class->get_message = digest_get_message;
	camel_folder_class->append_message = digest_append_message;
	camel_folder_class->transfer_messages_to = digest_transfer_messages_to;
	
	camel_folder_class->search_by_expression = digest_search_by_expression;
	camel_folder_class->search_by_uids = digest_search_by_uids;
	camel_folder_class->search_free = digest_search_free;
}

static void
camel_digest_folder_init (gpointer object, gpointer klass)
{
	CamelDigestFolder *digest_folder = CAMEL_DIGEST_FOLDER (object);
	CamelFolder *folder = CAMEL_FOLDER (object);
	
	folder->folder_flags |= CAMEL_FOLDER_HAS_SUMMARY_CAPABILITY | CAMEL_FOLDER_HAS_SEARCH_CAPABILITY;
	
	folder->summary = camel_digest_summary_new ();
	
	digest_folder->priv = g_new (struct _CamelDigestFolderPrivate, 1);
	digest_folder->priv->message = NULL;
	digest_folder->priv->search = NULL;
	digest_folder->priv->search_lock = g_mutex_new ();
}

static void
digest_finalize (CamelObject *object)
{
	CamelDigestFolder *digest_folder = CAMEL_DIGEST_FOLDER (object);
	CamelFolder *folder = CAMEL_FOLDER (object);
	
	if (folder->summary) {
		camel_object_unref (folder->summary);
		folder->summary = NULL;
	}
	
	camel_object_unref (digest_folder->priv->message);
	
	if (digest_folder->priv->search)
		camel_object_unref (digest_folder->priv->search);
	
	g_mutex_free (digest_folder->priv->search_lock);
	
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

static gboolean
multipart_contains_message_parts (CamelMultipart *multipart)
{
	gboolean has_message_parts = FALSE;
	CamelDataWrapper *wrapper;
	CamelMimePart *part;
	int i, parts;
	
	parts = camel_multipart_get_number (multipart);
	for (i = 0; i < parts && !has_message_parts; i++) {
		part = camel_multipart_get_part (multipart, i);
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
		if (CAMEL_IS_MULTIPART (wrapper)) {
			has_message_parts = multipart_contains_message_parts (CAMEL_MULTIPART (wrapper));
		} else if (CAMEL_IS_MIME_MESSAGE (wrapper)) {
			has_message_parts = TRUE;
		}
	}
	
	return has_message_parts;
}

static void
digest_add_multipart (CamelFolder *folder, CamelMultipart *multipart, const char *preuid)
{
	CamelDataWrapper *wrapper;
	CamelMessageInfo *info;
	CamelMimePart *part;
	int parts, i;
	char *uid;
	
	parts = camel_multipart_get_number (multipart);
	for (i = 0; i < parts; i++) {
		part = camel_multipart_get_part (multipart, i);
		
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
		
		if (CAMEL_IS_MULTIPART (wrapper)) {
			uid = g_strdup_printf ("%s%d.", preuid, i);
			digest_add_multipart (folder, CAMEL_MULTIPART (wrapper), uid);
			g_free (uid);
			continue;
		} else if (!CAMEL_IS_MIME_MESSAGE (wrapper)) {
			continue;
		}
		
		info = camel_folder_summary_info_new_from_message (folder->summary, CAMEL_MIME_MESSAGE (wrapper));
		info->uid = g_strdup_printf ("%s%d", preuid, i);
		camel_folder_summary_add (folder->summary, info);
	}
}

static void
construct_summary (CamelFolder *folder, CamelMultipart *multipart)
{
	digest_add_multipart (folder, multipart, "");
}

CamelFolder *
camel_digest_folder_new (CamelStore *parent_store, CamelMimeMessage *message)
{
	CamelDigestFolder *digest_folder;
	CamelDataWrapper *wrapper;
	CamelFolder *folder;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	if (!wrapper || !CAMEL_IS_MULTIPART (wrapper))
		return NULL;
	
	/* Make sure we have a multipart/digest subpart or at least some message/rfc822 attachments... */
	if (!camel_content_type_is (CAMEL_DATA_WRAPPER (message)->mime_type, "multipart", "digest")) {
		if (!multipart_contains_message_parts (CAMEL_MULTIPART (wrapper)))
			return NULL;
	}
	
	folder = CAMEL_FOLDER (camel_object_new (camel_digest_folder_get_type ()));
	digest_folder = CAMEL_DIGEST_FOLDER (folder);
	
	camel_folder_construct (folder, parent_store, "folder_name", "short_name");
	
	camel_object_ref (message);
	digest_folder->priv->message = message;
	
	construct_summary (folder, CAMEL_MULTIPART (wrapper));
	
	return folder;
}

static void
digest_refresh_info (CamelFolder *folder, CamelException *ex)
{
	
}

static void
digest_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	/* no-op */
}

static void
digest_expunge (CamelFolder *folder, CamelException *ex)
{
	/* no-op */
}

static const char *
digest_get_full_name (CamelFolder *folder)
{
	return folder->full_name;
}

static void
digest_append_message (CamelFolder *folder, CamelMimeMessage *message,
		       const CamelMessageInfo *info, char **appended_uid,
		       CamelException *ex)
{
	/* no-op */
	if (appended_uid)
		*appended_uid = NULL;
}

static void
digest_transfer_messages_to (CamelFolder *source, GPtrArray *uids,
			     CamelFolder *dest, GPtrArray **transferred_uids,
			     gboolean delete_originals, CamelException *ex)
{
	/* no-op */
	if (transferred_uids)
		*transferred_uids = NULL;
}

static CamelMimeMessage *
digest_get_message (CamelFolder *folder, const char *uid, CamelException *ex)
{
	CamelDigestFolder *digest = CAMEL_DIGEST_FOLDER (folder);
	CamelDataWrapper *wrapper;
	CamelMimeMessage *message;
	CamelMimePart *part;
	char *subuid;
	int id;
	
	part = CAMEL_MIME_PART (digest->priv->message);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	
	do {
		id = strtoul (uid, &subuid, 10);
		if (!CAMEL_IS_MULTIPART (wrapper))
			return NULL;
		
		part = camel_multipart_get_part (CAMEL_MULTIPART (wrapper), id);
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
		uid = subuid + 1;
	} while (*subuid == '.');
	
	if (!CAMEL_IS_MIME_MESSAGE (wrapper))
		return NULL;
	
	message = CAMEL_MIME_MESSAGE (wrapper);
	camel_object_ref (message);
	
	return message;
}

static GPtrArray *
digest_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex)
{
	CamelDigestFolder *df = (CamelDigestFolder *) folder;
	GPtrArray *matches;
	
	CAMEL_DIGEST_FOLDER_LOCK (folder, search_lock);
	
	if (!df->priv->search)
		df->priv->search = camel_folder_search_new ();
	
	camel_folder_search_set_folder (df->priv->search, folder);
	matches = camel_folder_search_search(df->priv->search, expression, NULL, ex);
	
	CAMEL_DIGEST_FOLDER_UNLOCK (folder, search_lock);
	
	return matches;
}

static GPtrArray *
digest_search_by_uids (CamelFolder *folder, const char *expression, GPtrArray *uids, CamelException *ex)
{
	CamelDigestFolder *df = (CamelDigestFolder *) folder;
	GPtrArray *matches;

	if (uids->len == 0)
		return g_ptr_array_new();

	CAMEL_DIGEST_FOLDER_LOCK (folder, search_lock);
	
	if (!df->priv->search)
		df->priv->search = camel_folder_search_new ();
	
	camel_folder_search_set_folder (df->priv->search, folder);
	matches = camel_folder_search_search(df->priv->search, expression, NULL, ex);
	
	CAMEL_DIGEST_FOLDER_UNLOCK (folder, search_lock);
	
	return matches;
}

static void
digest_search_free (CamelFolder *folder, GPtrArray *result)
{
	CamelDigestFolder *digest_folder = CAMEL_DIGEST_FOLDER (folder);
	
	CAMEL_DIGEST_FOLDER_LOCK (folder, search_lock);
	
	camel_folder_search_free_result (digest_folder->priv->search, result);
	
	CAMEL_DIGEST_FOLDER_UNLOCK (folder, search_lock);
}
