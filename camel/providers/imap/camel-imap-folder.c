/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-folder.c: class for an imap folder */

/* 
 * Authors: Jeffrey Stedfast <fejj@helixcode.com> 
 *
 * Copyright (C) 2000 Helix Code, Inc.
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
#include <ctype.h>

#include "camel-imap-folder.h"
#include "camel-imap-command.h"
#include "camel-imap-message-cache.h"
#include "camel-imap-search.h"
#include "camel-imap-store.h"
#include "camel-imap-summary.h"
#include "camel-imap-utils.h"
#include "camel-imap-wrapper.h"
#include "string-utils.h"
#include "camel-session.h"
#include "camel-stream.h"
#include "camel-stream-mem.h"
#include "camel-stream-buffer.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-mime-filter-crlf.h"
#include "camel-exception.h"
#include "camel-mime-utils.h"
#include "camel-imap-private.h"
#include "camel-multipart.h"
#include "camel-operation.h"

#define d(x) x

#define CF_CLASS(o) (CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(o)))

static CamelFolderClass *parent_class = NULL;

static void imap_finalize (CamelObject *object);
static void imap_rescan (CamelFolder *folder, int exists, CamelException *ex);
static void imap_refresh_info (CamelFolder *folder, CamelException *ex);
static void imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
static const char *imap_get_full_name (CamelFolder *folder);
static void imap_expunge (CamelFolder *folder, CamelException *ex);

/* message manipulation */
static CamelMimeMessage *imap_get_message (CamelFolder *folder, const gchar *uid,
					   CamelException *ex);
static void imap_append_message (CamelFolder *folder, CamelMimeMessage *message,
				 const CamelMessageInfo *info, CamelException *ex);
static void imap_copy_messages_to (CamelFolder *source, GPtrArray *uids,
				   CamelFolder *destination, CamelException *ex);
static void imap_move_messages_to (CamelFolder *source, GPtrArray *uids,
				   CamelFolder *destination, CamelException *ex);

/* searching */
static GPtrArray *imap_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex);
static void       imap_search_free          (CamelFolder *folder, GPtrArray *uids);

GData *parse_fetch_response (CamelImapFolder *imap_folder, char *msg_att);

static void
camel_imap_folder_class_init (CamelImapFolderClass *camel_imap_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_imap_folder_class);

	parent_class = CAMEL_FOLDER_CLASS(camel_type_get_global_classfuncs (camel_folder_get_type ()));
	
	/* virtual method definition */
	
	/* virtual method overload */
	camel_folder_class->refresh_info = imap_refresh_info;
	camel_folder_class->sync = imap_sync;
	camel_folder_class->expunge = imap_expunge;
	camel_folder_class->get_full_name = imap_get_full_name;
	
	camel_folder_class->get_message = imap_get_message;
	camel_folder_class->append_message = imap_append_message;
	camel_folder_class->copy_messages_to = imap_copy_messages_to;
	camel_folder_class->move_messages_to = imap_move_messages_to;
	
	camel_folder_class->search_by_expression = imap_search_by_expression;
	camel_folder_class->search_free = imap_search_free;
}

static void
camel_imap_folder_init (gpointer object, gpointer klass)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (object);
	CamelFolder *folder = CAMEL_FOLDER (object);
	
	folder->has_summary_capability = TRUE;
	folder->has_search_capability = TRUE;
	
	imap_folder->priv = g_malloc0(sizeof(*imap_folder->priv));
#ifdef ENABLE_THREADS
	imap_folder->priv->search_lock = e_mutex_new(E_MUTEX_SIMPLE);
	imap_folder->priv->cache_lock = e_mutex_new(E_MUTEX_REC);
#endif
}

CamelType
camel_imap_folder_get_type (void)
{
	static CamelType camel_imap_folder_type = CAMEL_INVALID_TYPE;
	
	if (camel_imap_folder_type == CAMEL_INVALID_TYPE) {
		camel_imap_folder_type =
			camel_type_register (CAMEL_FOLDER_TYPE, "CamelImapFolder",
					     sizeof (CamelImapFolder),
					     sizeof (CamelImapFolderClass),
					     (CamelObjectClassInitFunc) camel_imap_folder_class_init,
					     NULL,
					     (CamelObjectInitFunc) camel_imap_folder_init,
					     (CamelObjectFinalizeFunc) imap_finalize);
	}
	
	return camel_imap_folder_type;
}

CamelFolder *
camel_imap_folder_new (CamelStore *parent, const char *folder_name,
		       const char *short_name, const char *folder_dir,
		       CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (parent);
	CamelFolder *folder = CAMEL_FOLDER (camel_object_new (camel_imap_folder_get_type ()));
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelImapResponse *response;
	char *summary_file;

	camel_folder_construct (folder, parent, folder_name, short_name);

	summary_file = g_strdup_printf ("%s/summary", folder_dir);
	folder->summary = camel_imap_summary_new (summary_file);
	g_free (summary_file);
	if (!folder->summary) {
		camel_object_unref (CAMEL_OBJECT (folder));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not load summary for %s"),
				      folder_name);
		return NULL;
	}

	imap_folder->cache = camel_imap_message_cache_new (folder_dir, folder->summary, ex);
	if (!imap_folder->cache) {
		camel_object_unref (CAMEL_OBJECT (folder));
		return NULL;
	}

	if (camel_imap_store_check_online (imap_store, NULL)) {
		response = camel_imap_command (imap_store, folder, ex, NULL);
		if (response)
			camel_imap_response_free (imap_store, response);
		else {
			camel_object_unref (CAMEL_OBJECT (folder));
			return NULL;
		}
	}

	return folder;
}

/* Called with the store's command_lock locked */
void
camel_imap_folder_selected (CamelFolder *folder, CamelImapResponse *response,
			    CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelImapSummary *imap_summary = CAMEL_IMAP_SUMMARY (folder->summary);
	unsigned long exists = 0, validity = 0, val, uid;
	CamelMessageInfo *info;
	GData *fetch_data;
	int i, count;
	char *resp;

	CAMEL_IMAP_STORE_ASSERT_LOCKED (folder->parent_store, command_lock);

	count = camel_folder_summary_count (folder->summary);

	for (i = 0; i < response->untagged->len; i++) {
		resp = response->untagged->pdata[i] + 2;
		if (!g_strncasecmp (resp, "FLAGS ", 6) &&
		    !folder->permanent_flags) {
			resp += 6;
			folder->permanent_flags = imap_parse_flag_list (&resp);
		} else if (!g_strncasecmp (resp, "OK [PERMANENTFLAGS ", 19)) {
			resp += 19;
			folder->permanent_flags = imap_parse_flag_list (&resp);
		} else if (!g_strncasecmp (resp, "OK [UIDVALIDITY ", 16)) {
			validity = strtoul (resp + 16, NULL, 10);
		} else if (isdigit ((unsigned char)*resp)) {
			unsigned long num = strtoul (resp, &resp, 10);

			if (!g_strncasecmp (resp, " EXISTS", 7)) {
				exists = num;
				/* Remove from the response so nothing
				 * else tries to interpret it.
				 */
				g_free (response->untagged->pdata[i]);
				g_ptr_array_remove_index (response->untagged, i--);
			}
		}
	}

	if (!imap_summary->validity)
		imap_summary->validity = validity;
	else if (validity != imap_summary->validity) {
		imap_summary->validity = validity;
		camel_folder_summary_clear (folder->summary);
		camel_imap_message_cache_clear (imap_folder->cache);
		camel_imap_folder_changed (folder, exists, NULL, ex);
		return;
	}

	/* If we've lost messages, we have to rescan everything */
	if (exists < count) {
		imap_rescan (folder, exists, ex);
		return;
	}

	if (count != 0) {
		CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);

		/* Similarly, if the UID of the highest message we
		 * know about has changed, then that indicates that
		 * messages have been both added and removed, so we
		 * have to rescan to find the removed ones. (We pass
		 * NULL for the folder since we know that this folder
		 * is selected, and we don't want camel_imap_command
		 * to worry about it.)
		 */
		response = camel_imap_command (store, NULL, ex, "FETCH %d UID", count);
		if (!response)
			return;
		uid = 0;
		for (i = 0; i < response->untagged->len; i++) {
			resp = response->untagged->pdata[i];
			val = strtoul (resp + 2, &resp, 10);
			if (val == 0)
				continue;
			if (!g_strcasecmp (resp, " EXISTS")) {
				/* Another one?? */
				exists = val;
				continue;
			}
			if (uid != 0 || val != count || g_strncasecmp (resp, " FETCH (", 8) != 0)
				continue;

			fetch_data = parse_fetch_response (imap_folder, resp + 7);
			uid = strtoul (g_datalist_get_data (&fetch_data, "UID"), NULL, 10);
			g_datalist_clear (&fetch_data);
		}
		camel_imap_response_free_without_processing (store, response);

		info = camel_folder_summary_index (folder->summary, count - 1);
		val = strtoul (camel_message_info_uid (info), NULL, 10);
		camel_folder_summary_info_free (folder->summary, info);
		if (uid == 0 || uid != val) {
			imap_rescan (folder, exists, ex);
			return;
		}
	}

	/* OK. So now we know that no messages have been expunged. Whew.
	 * Now see if messages have been added.
	 */
	if (exists > count)
		camel_imap_folder_changed (folder, exists, NULL, ex);

	/* And we're done. */
}	

static void           
imap_finalize (CamelObject *object)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (object);

	if (imap_folder->search)
		camel_object_unref (CAMEL_OBJECT (imap_folder->search));
	if (imap_folder->cache)
		camel_object_unref (CAMEL_OBJECT (imap_folder->cache));

#ifdef ENABLE_THREADS
	e_mutex_destroy(imap_folder->priv->search_lock);
	e_mutex_destroy(imap_folder->priv->cache_lock);
#endif
	g_free(imap_folder->priv);
}

static void
imap_refresh_info (CamelFolder *folder, CamelException *ex)
{
	if (!camel_imap_store_check_online (CAMEL_IMAP_STORE (folder->parent_store), NULL))
		return;

	CAMEL_IMAP_STORE_LOCK (folder->parent_store, command_lock);
	imap_rescan (folder, camel_folder_summary_count (folder->summary), ex);
	CAMEL_IMAP_STORE_UNLOCK (folder->parent_store, command_lock);
}

/* Called with the store's command_lock locked */
static void
imap_rescan (CamelFolder *folder, int exists, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelImapResponse *response;
	struct {
		char *uid;
		guint32 flags;
	} *new = NULL;
	char *resp;
	int i, j, seq, summary_len;
	CamelMessageInfo *info;
	CamelImapMessageInfo *iinfo;
	GArray *removed;
	GData *fetch_data;

	CAMEL_IMAP_STORE_ASSERT_LOCKED (store, command_lock);

	camel_operation_start(NULL, _("Scanning IMAP folder"));

	/* Get UIDs and flags of all messages. */
	if (exists > 0) {
		response = camel_imap_command (store, folder, ex,
					       "FETCH 1:%d (UID FLAGS)",
					       exists);
		if (!response)
			return;

		new = g_malloc0 (exists * sizeof (*new));
		for (i = 0; i < response->untagged->len; i++) {
			resp = response->untagged->pdata[i];

			seq = strtoul (resp + 2, &resp, 10);
			if (g_strncasecmp (resp, " FETCH (", 8) != 0)
				continue;

			fetch_data = parse_fetch_response (imap_folder, resp + 7);
			new[seq - 1].uid = g_strdup (g_datalist_get_data (&fetch_data, "UID"));
			new[seq - 1].flags = GPOINTER_TO_UINT (g_datalist_get_data (&fetch_data, "FLAGS"));
			g_datalist_clear (&fetch_data);
			g_ptr_array_remove_index_fast (response->untagged, i--);
		}
		camel_imap_response_free_without_processing (store, response);
	}

	/* If we find a UID in the summary that doesn't correspond to
	 * the UID in the folder, that it means the message was
	 * deleted on the server, so we remove it from the summary.
	 */
	removed = g_array_new (FALSE, FALSE, sizeof (int));
	summary_len = camel_folder_summary_count (folder->summary);
	for (i = 0; i < summary_len && i < exists; i++) {
		int pc = (i*100)/MIN(summary_len, exists);

		camel_operation_progress(NULL, pc);

		/* Shouldn't happen, but... */
		if (!new[i].uid)
			continue;

		info = camel_folder_summary_index (folder->summary, i);
		iinfo = (CamelImapMessageInfo *)info;

		if (strcmp (camel_message_info_uid (info), new[i].uid) != 0) {
			seq = i + 1;
			g_array_append_val (removed, seq);
			i--;
			summary_len--;
			continue;
		}

		/* Update summary flags */
		if (new[i].flags != iinfo->server_flags) {
			guint32 server_set, server_cleared;

			server_set = new[i].flags & ~iinfo->server_flags;
			server_cleared = iinfo->server_flags & ~new[i].flags;

			info->flags = (info->flags | server_set) & ~server_cleared;
			iinfo->server_flags = new[i].flags;

			camel_object_trigger_event (CAMEL_OBJECT (folder),
						    "message_changed",
						    g_strdup (new[i].uid));
		}

		camel_folder_summary_info_free(folder->summary, info);

		g_free (new[i].uid);
	}

	/* Remove any leftover cached summary messages. */
	for (j = i + 1; j < summary_len; j++) {
		seq = j - removed->len;
		g_array_append_val (removed, seq);
	}

	/* Free remaining memory. */
	while (i < exists)
		g_free (new[i++].uid);
	g_free (new);

	/* And finally update the summary. */
	camel_imap_folder_changed (folder, exists, removed, ex);
	g_array_free (removed, TRUE);

	camel_operation_end(NULL);
}

/* Find all messages in @folder with flags matching @flags and @mask.
 * If no messages match, returns %NULL. Otherwise, returns an array of
 * CamelMessageInfo and sets *@set to a message set corresponding the
 * UIDs of the matched messages. The caller must free the infos, the
 * array, and the set string.
 */
static GPtrArray *
get_matching (CamelFolder *folder, guint32 flags, guint32 mask, char **set)
{
	GPtrArray *matches;
	CamelMessageInfo *info;
	int i, max, range;
	GString *gset;

	matches = g_ptr_array_new ();
	gset = g_string_new ("");
	max = camel_folder_summary_count (folder->summary);
	range = -1;
	for (i = 0; i < max; i++) {
		info = camel_folder_summary_index (folder->summary, i);
		if (!info)
			continue;
		if ((info->flags & mask) != flags) {
			camel_folder_summary_info_free (folder->summary, info);
			if (range != -1) {
				if (range != i - 1) {
					info = matches->pdata[matches->len - 1];
					g_string_sprintfa (gset, ":%s", camel_message_info_uid (info));
				}
				range = -1;
			}
			continue;
		}

		g_ptr_array_add (matches, info);
		if (range != -1)
			continue;
		range = i;
		if (gset->len)
			g_string_append_c (gset, ',');
		g_string_sprintfa (gset, "%s", camel_message_info_uid (info));
	}
	if (range != -1 && range != max - 1) {
		info = matches->pdata[matches->len - 1];
		g_string_sprintfa (gset, ":%s", camel_message_info_uid (info));
	}

	if (matches->len) {
		*set = gset->str;
		g_string_free (gset, FALSE);
		return matches;
	} else {
		g_string_free (gset, TRUE);
		g_ptr_array_free (matches, TRUE);
		return NULL;
	}
}

static void
imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelImapResponse *response = NULL;
	CamelMessageInfo *info;
	GPtrArray *matches;
	char *set, *flaglist;
	int i, j, max;

	if (!camel_imap_store_check_online (store, NULL))
		return;

	max = camel_folder_summary_count (folder->summary);

	CAMEL_IMAP_STORE_LOCK (store, command_lock);

	/* If we're expunging then we don't need to be precise about the
	 * flags of deleted messages. Just add \Deleted to anything that
	 * should have it.
	 */
	if (expunge && (matches = get_matching (folder, CAMEL_MESSAGE_DELETED,
						CAMEL_MESSAGE_DELETED, &set))) {
		for (i = 0; i < matches->len; i++) {
			info = matches->pdata[i];
			info->flags &= ~CAMEL_MESSAGE_FOLDER_FLAGGED;
			camel_folder_summary_info_free (folder->summary, info);
		}
		g_ptr_array_free (matches, TRUE);
		camel_folder_summary_touch (folder->summary);

		response = camel_imap_command (store, folder, ex,
					       "UID STORE %s +FLAGS.SILENT \\Deleted",
					       set);
		g_free (set);
		if (response)
			camel_imap_response_free (store, response);
		if (camel_exception_is_set (ex)) {
			CAMEL_IMAP_STORE_UNLOCK (store, command_lock);
			return;
		}
	}

	/* OK, now, find a message with changed flags, find all of the
	 * other messages like it, sync them as a group, mark them as
	 * updated, and continue.
	 */
	for (i = 0; i < max; i++) {
		info = camel_folder_summary_index (folder->summary, i);
		if (!info)
			continue;
		if (!(info->flags & CAMEL_MESSAGE_FOLDER_FLAGGED)) {
			camel_folder_summary_info_free (folder->summary, info);
			continue;
		}

		flaglist = imap_create_flag_list (info->flags);
		matches = get_matching (folder, info->flags & (CAMEL_IMAP_SERVER_FLAGS | CAMEL_MESSAGE_FOLDER_FLAGGED),
					CAMEL_IMAP_SERVER_FLAGS | CAMEL_MESSAGE_FOLDER_FLAGGED, &set);
		camel_folder_summary_info_free (folder->summary, info);

		response = camel_imap_command (store, folder, ex,
					       "UID STORE %s FLAGS.SILENT %s",
					       set, flaglist);
		g_free (set);
		g_free (flaglist);
		if (response)
			camel_imap_response_free (store, response);
		if (!camel_exception_is_set (ex)) {
			for (j = 0; j < matches->len; j++) {
				info = matches->pdata[j];
				info->flags &= ~CAMEL_MESSAGE_FOLDER_FLAGGED;
				((CamelImapMessageInfo*)info)->server_flags =
					info->flags & CAMEL_IMAP_SERVER_FLAGS;
			}
			camel_folder_summary_touch (folder->summary);
		}
		for (j = 0; j < matches->len; j++) {
			info = matches->pdata[j];
			camel_folder_summary_info_free (folder->summary, info);
		}
		g_ptr_array_free (matches, TRUE);

		if (camel_exception_is_set (ex)) {
			CAMEL_IMAP_STORE_UNLOCK (store, command_lock);
			return;
		}
	}

	if (expunge) {
		response = camel_imap_command (store, folder, ex, "EXPUNGE");
		camel_imap_response_free (store, response);
	}

	if (!response) {
		/* We didn't sync or expunge anything... Do a noop so
		 * the server gets a chance to tell us any news it has.
		 */
		response = camel_imap_command (store, folder, ex, "NOOP");
		camel_imap_response_free (store, response);
	}		

	camel_folder_summary_save (folder->summary);
	CAMEL_IMAP_STORE_UNLOCK (store, command_lock);

	camel_operation_end(NULL);
}

static void
imap_expunge (CamelFolder *folder, CamelException *ex)
{
	if (!camel_imap_store_check_online (CAMEL_IMAP_STORE (folder->parent_store), ex))
		return;

	imap_sync (folder, TRUE, ex);
}

static const char *
imap_get_full_name (CamelFolder *folder)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	char *name;
	int len;

	name = folder->full_name;
	if (store->namespace && *store->namespace) {
		len = strlen (store->namespace);
		if (!strncmp (store->namespace, folder->full_name, len) &&
		    strlen (folder->full_name) > len)
			name += len;
		if (*name == store->dir_sep)
			name++;
	}
	return name;
}	

static void
imap_append_message (CamelFolder *folder, CamelMimeMessage *message,
		     const CamelMessageInfo *info, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelImapResponse *response;
	CamelStream *memstream;
	CamelMimeFilter *crlf_filter;
	CamelStreamFilter *streamfilter;
	GByteArray *ba;
	char *flagstr, *result, *uid;

	if (!camel_imap_store_check_online (store, ex))
		return;

	/* create flag string param */
	if (info && info->flags)
		flagstr = imap_create_flag_list (info->flags);
	else
		flagstr = NULL;

	/* FIXME: We could avoid this if we knew how big the message was. */
	memstream = camel_stream_mem_new ();
	ba = g_byte_array_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (memstream), ba);

	streamfilter = camel_stream_filter_new_with_stream (memstream);
	crlf_filter = camel_mime_filter_crlf_new (
		CAMEL_MIME_FILTER_CRLF_ENCODE,
		CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	camel_stream_filter_add (streamfilter, crlf_filter);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message),
					    CAMEL_STREAM (streamfilter));
	camel_object_unref (CAMEL_OBJECT (streamfilter));
	camel_object_unref (CAMEL_OBJECT (crlf_filter));
	camel_object_unref (CAMEL_OBJECT (memstream));

	response = camel_imap_command (store, NULL, ex, "APPEND %S%s%s {%d}",
				       folder->full_name, flagstr ? " " : "",
				       flagstr ? flagstr : "", ba->len);
	g_free (flagstr);
	
	if (!response) {
		g_byte_array_free (ba, TRUE);
		return;
	}
	result = camel_imap_response_extract_continuation (store, response, ex);
	if (!result) {
		g_byte_array_free (ba, TRUE);
		return;
	}
	g_free (result);

	/* send the rest of our data - the mime message */
	g_byte_array_append (ba, "\0", 3);
	response = camel_imap_command_continuation (store, ex, ba->data);
	g_byte_array_free (ba, TRUE);
	if (!response)
		return;

	if (store->capabilities & IMAP_CAPABILITY_UIDPLUS) {
		uid = strstrcase (response->status, "[APPENDUID ");
		if (uid)
			uid = strchr (uid + 11, ' ');
		if (uid)
			uid = g_strndup (uid + 1, strcspn (uid + 1, "]"));
		if (uid) {
			/* Make sure it's a number */
			if (strtoul (uid, &result, 10) != 0 && !*result) {
				/* OK. Cache the data. */
				camel_imap_message_cache_insert_wrapper (
					CAMEL_IMAP_FOLDER (folder)->cache,
					uid, "", CAMEL_DATA_WRAPPER (message));
			}
			g_free (uid);
		}
	}

	camel_imap_response_free (store, response);
}

static void
handle_copyuid (CamelImapResponse *response, CamelFolder *source,
		CamelFolder *destination)
{
	CamelImapMessageCache *scache = CAMEL_IMAP_FOLDER (source)->cache;
	CamelImapMessageCache *dcache = CAMEL_IMAP_FOLDER (destination)->cache;
	char *validity, *srcset, *destset;
	GPtrArray *src, *dest;
	int i;

	validity = strstrcase (response->status, "[COPYUID ");
	if (!validity)
		return;
	validity += 9;
	if (strtoul (validity, NULL, 10) !=
	    CAMEL_IMAP_SUMMARY (destination->summary)->validity)
		return;

	srcset = strchr (validity, ' ');
	if (!srcset++)
		goto lose;
	destset = strchr (srcset, ' ');
	if (!destset++)
		goto lose;

	src = imap_uid_set_to_array (source->summary, srcset);
	dest = imap_uid_set_to_array (destination->summary, destset);

	if (src && dest && src->len == dest->len) {
		/* We don't have to worry about deadlocking on the
		 * cache locks here, because we've got the store's
		 * command lock too, so no one else could be here.
		 */
		CAMEL_IMAP_FOLDER_LOCK (source, cache_lock);
		CAMEL_IMAP_FOLDER_LOCK (destination, cache_lock);
		for (i = 0; i < src->len; i++) {
			camel_imap_message_cache_copy (scache, src->pdata[i],
						       dcache, dest->pdata[i]);
		}
		CAMEL_IMAP_FOLDER_UNLOCK (source, cache_lock);
		CAMEL_IMAP_FOLDER_UNLOCK (destination, cache_lock);

		imap_uid_array_free (src);
		imap_uid_array_free (dest);
		return;
	}

	imap_uid_array_free (src);
	imap_uid_array_free (dest);
 lose:
	g_warning ("Bad COPYUID response from server");
}

static void
imap_copy_messages_to (CamelFolder *source, GPtrArray *uids,
		       CamelFolder *destination, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (source->parent_store);
	CamelImapResponse *response;
	char *set;
	
	if (!camel_imap_store_check_online (store, ex))
		return;
	
	/* Sync message flags if needed. */
	imap_sync (source, FALSE, ex);
	if (camel_exception_is_set (ex))
		return;
	
	/* Now copy the messages */
	set = imap_uid_array_to_set (source->summary, uids);
	response = camel_imap_command (store, source, ex, "UID COPY %s %S",
				       set, destination->full_name);
	if (response && (store->capabilities & IMAP_CAPABILITY_UIDPLUS))
		handle_copyuid (response, source, destination);

	camel_imap_response_free (store, response);
	g_free (set);
	
	if (camel_exception_is_set (ex))
		return;
	
	/* Force the destination folder to notice its new messages. */
	response = camel_imap_command (store, destination, NULL, "NOOP");
	camel_imap_response_free (store, response);
}

static void
imap_move_messages_to (CamelFolder *source, GPtrArray *uids,
		       CamelFolder *destination, CamelException *ex)
{
	int i;
	
	imap_copy_messages_to (source, uids, destination, ex);
	if (camel_exception_is_set (ex))
		return;
	
	for (i = 0; i < uids->len; i++)
		camel_folder_delete_message (source, uids->pdata[i]);
}

static GPtrArray *
imap_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	GPtrArray *matches, *summary;

	if (!camel_imap_store_check_online (CAMEL_IMAP_STORE (folder->parent_store), ex))
		return NULL;

	/* we could get around this by creating a new search object each time,
	   but i doubt its worth it since any long operation would lock the
	   command channel too */
	CAMEL_IMAP_FOLDER_LOCK(folder, search_lock);

	if (!imap_folder->search)
		imap_folder->search = camel_imap_search_new ();

	camel_folder_search_set_folder (imap_folder->search, folder);
	summary = camel_folder_get_summary(folder);
	camel_folder_search_set_summary(imap_folder->search, summary);
	matches = camel_folder_search_execute_expression (imap_folder->search, expression, ex);

	CAMEL_IMAP_FOLDER_UNLOCK(folder, search_lock);

	camel_folder_free_summary(folder, summary);

	return matches;
}

static void
imap_search_free (CamelFolder *folder, GPtrArray *uids)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);

	g_return_if_fail (imap_folder->search);

	CAMEL_IMAP_FOLDER_LOCK(folder, search_lock);

	camel_folder_search_free_result (imap_folder->search, uids);

	CAMEL_IMAP_FOLDER_UNLOCK(folder, search_lock);
}

static CamelMimeMessage *get_message (CamelImapFolder *imap_folder,
				      const char *uid,
				      const char *part_specifier,
				      CamelMessageContentInfo *ci,
				      CamelException *ex);

/* Fetch the contents of the MIME part indicated by @ci, which is part
 * of message @uid in @folder.
 */
static CamelDataWrapper *
get_content (CamelImapFolder *imap_folder, const char *uid,
	     const char *part_spec, CamelMimePart *part,
	     CamelMessageContentInfo *ci, CamelException *ex)
{
	CamelDataWrapper *content;
	CamelStream *stream;
	char *child_spec;

	/* There are three cases: multipart, message/rfc822, and "other" */

	if (header_content_type_is (ci->type, "multipart", "*")) {
		CamelMultipart *body_mp;
		int speclen, num;

		body_mp = camel_multipart_new ();
		camel_data_wrapper_set_mime_type_field (
			CAMEL_DATA_WRAPPER (body_mp), ci->type);
		camel_multipart_set_boundary (body_mp, NULL);

		speclen = strlen (part_spec);
		child_spec = g_malloc (speclen + 15);
		memcpy (child_spec, part_spec, speclen);
		if (speclen > 0)
			child_spec[speclen++] = '.';

		ci = ci->childs;
		num = 1;
		while (ci) {
			sprintf (child_spec + speclen, "%d.MIME", num++);
			stream = camel_imap_folder_fetch_data (imap_folder, uid, child_spec, FALSE, ex);
			if (stream) {
				part = camel_mime_part_new ();
				camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (part), stream);
				camel_object_unref (CAMEL_OBJECT (stream));
				*(strchr (child_spec + speclen, '.')) = '\0';
				content = get_content (imap_folder, uid, child_spec, part, ci, ex);
			}
			if (!stream || !content) {
				g_free (child_spec);
				camel_object_unref (CAMEL_OBJECT (body_mp));
				return NULL;
			}

			camel_medium_set_content_object (CAMEL_MEDIUM (part), content);
			camel_object_unref (CAMEL_OBJECT (content));
			camel_multipart_add_part (body_mp, part);
			camel_object_unref (CAMEL_OBJECT (part));

			ci = ci->next;
		}
		g_free (child_spec);

		return (CamelDataWrapper *)body_mp;
	} else if (header_content_type_is (ci->type, "message", "rfc822")) {
		return (CamelDataWrapper *)
			get_message (imap_folder, uid, part_spec, ci->childs, ex);
	} else {
		if (!ci->parent || header_content_type_is (ci->parent->type, "message", "rfc822"))
			child_spec = g_strdup_printf ("%s%s1", part_spec, *part_spec ? "." : "");
		else
			child_spec = g_strdup (part_spec);

		content = camel_imap_wrapper_new (imap_folder, ci->type, uid, child_spec, part);
		g_free (child_spec);
		return content;
	}
}

static CamelMimeMessage *
get_message (CamelImapFolder *imap_folder, const char *uid,
	     const char *part_spec, CamelMessageContentInfo *ci,
	     CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (CAMEL_FOLDER (imap_folder)->parent_store);
	CamelDataWrapper *content;
	CamelMimeMessage *msg;
	CamelStream *stream;
	char *section_text;

	section_text = g_strdup_printf ("%s%s%s", part_spec, *part_spec ? "." : "",
					store->server_level >= IMAP_LEVEL_IMAP4REV1 ? "HEADER" : "0");
	stream = camel_imap_folder_fetch_data (imap_folder, uid, section_text, FALSE, ex);
	g_free (section_text);
	if (!stream)
		return NULL;

	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), stream);
	camel_object_unref (CAMEL_OBJECT (stream));

	content = get_content (imap_folder, uid, part_spec, CAMEL_MIME_PART (msg), ci, ex);
	if (!content) {
		camel_object_unref (CAMEL_OBJECT (msg));
		return NULL;
	}

	camel_medium_set_content_object (CAMEL_MEDIUM (msg), content);
	camel_object_unref (CAMEL_OBJECT (content));

	return msg;
}

/* FIXME: I pulled this number out of my butt. */
#define IMAP_SMALL_BODY_SIZE 5120

static CamelMimeMessage *
imap_get_message (CamelFolder *folder, const char *uid, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelMessageInfo *mi;
	CamelMimeMessage *msg;
	CamelStream *stream = NULL;

	mi = camel_folder_summary_uid (folder->summary, uid);
	g_return_val_if_fail (mi != NULL, NULL);

	/* If the message is small, or the server doesn't support
	 * IMAP4rev1, or we already have the whole thing cached,
	 * fetch it in one piece.
	 */
	if (mi->size < IMAP_SMALL_BODY_SIZE ||
	    store->server_level < IMAP_LEVEL_IMAP4REV1 ||
	    (stream = camel_imap_folder_fetch_data (imap_folder, uid, "", TRUE, NULL))) {
		camel_folder_summary_info_free (folder->summary, mi);
		if (!stream)
			stream = camel_imap_folder_fetch_data (imap_folder, uid, "", FALSE, ex);
		if (!stream)
			return NULL;
		msg = camel_mime_message_new ();
		camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), stream);
		camel_object_unref (CAMEL_OBJECT (stream));
		return msg;
	}

	/* For larger messages, fetch the structure and build a message
	 * with offline parts. (We check mi->content->type rather than
	 * mi->content because camel_folder_summary_info_new always creates
	 * an empty content struct.)
	 */
	if (!mi->content->type) {
		CamelImapResponse *response;
		GData *fetch_data;
		char *body, *found_uid;
		int i;

		if (!camel_imap_store_check_online (store, NULL)) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					     _("This message is not currently available"));
			return NULL;
		}

		response = camel_imap_command (store, folder, ex,
					       "UID FETCH %s BODY", uid);
		if (!response) {
			camel_folder_summary_info_free (folder->summary, mi);
			return NULL;
		}

		for (i = 0, body = NULL; i < response->untagged->len; i++) {
			fetch_data = parse_fetch_response (imap_folder, response->untagged->pdata[i]);
			found_uid = g_datalist_get_data (&fetch_data, "UID");
			body = g_datalist_get_data (&fetch_data, "BODY");
			if (found_uid && body && !strcmp (found_uid, uid))
				break;
			g_datalist_clear (&fetch_data);
			body = NULL;
		}

		if (body)
			imap_parse_body (&body, folder, mi->content);
		g_datalist_clear (&fetch_data);
		camel_imap_response_free (store, response);

		if (!mi->content->type) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      _("Could not find message body in FETCH response."));
			camel_folder_summary_info_free (folder->summary, mi);
			return NULL;
		}
	}

	msg = get_message (imap_folder, uid, "", mi->content, ex);
	camel_folder_summary_info_free (folder->summary, mi);

	return msg;
}

static void
imap_update_summary (CamelFolder *folder,
		     CamelFolderChangeInfo *changes,
		     CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelImapResponse *response;
	GPtrArray *lines, *messages;
	char *p, *uid;
	int i, seq, first, exists = 0;
	CamelMimeMessage *msg;
	CamelMessageInfo *mi;
	GData *fetch_data;
	CamelStream *stream;

	CAMEL_IMAP_STORE_ASSERT_LOCKED (store, command_lock);

	first = camel_folder_summary_count (folder->summary) + 1;

	response = camel_imap_command (store, folder, ex, "FETCH %d:* (UID FLAGS RFC822.SIZE)", first);
	if (!response)
		return;

	/* Walk through the responses, looking for UIDs, and make sure
	 * we have those headers cached.
	 */
	messages = g_ptr_array_new ();
	lines = response->untagged;
	for (i = 0; i < lines->len; i++) {
		p = lines->pdata[i];
		if (*p++ != '*' || *p++ != ' ') {
			g_ptr_array_remove_index_fast (lines, i--);
			continue;
		}
		seq = strtoul (p, &p, 10);
		if (!g_strcasecmp (p, " EXISTS")) {
			exists = seq;
			g_ptr_array_remove_index_fast (lines, i--);
			continue;
		}
		if (!seq || seq < first || g_strncasecmp (p, " FETCH (", 8) != 0) {
			g_ptr_array_remove_index_fast (lines, i--);
			continue;
		}

		if (seq - first >= messages->len)
			g_ptr_array_set_size (messages, seq - first + 1);

		fetch_data = parse_fetch_response (imap_folder, p + 7);
		uid = g_datalist_get_data (&fetch_data, "UID");
		if (uid) {
			stream = camel_imap_folder_fetch_data (
				imap_folder, uid,
				store->server_level >= IMAP_LEVEL_IMAP4REV1 ?
				"HEADER" : "0", FALSE, ex);
			if (!stream) {
				camel_imap_response_free_without_processing (store, response);
				/* XXX messages */
				return;
			}

			msg = camel_mime_message_new ();
			camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), stream);
			camel_object_unref (CAMEL_OBJECT (stream));
			mi = camel_folder_summary_info_new_from_message (folder->summary, msg);
			camel_object_unref (CAMEL_OBJECT (msg));

			messages->pdata[seq - first] = mi;
		}
		g_datalist_clear (&fetch_data);
	}

	/* Now go back through and create summary items */
	lines = response->untagged;
	for (i = 0; i < lines->len; i++) {
		p = lines->pdata[i];
		seq = strtoul (p + 2, &p, 10);
		p = strchr (p, '(');

		mi = messages->pdata[seq - first];
		if (!mi) /* ? */
			continue;
		fetch_data = parse_fetch_response (imap_folder, p);

		if (g_datalist_get_data (&fetch_data, "UID"))
			camel_message_info_set_uid (mi, g_strdup (g_datalist_get_data (&fetch_data, "UID")));
		if (g_datalist_get_data (&fetch_data, "FLAGS")) {
			guint32 flags = GPOINTER_TO_INT (g_datalist_get_data (&fetch_data, "FLAGS"));

			((CamelImapMessageInfo *)mi)->server_flags = flags;
			/* "or" them in with the existing flags that may
			 * have been set by summary_info_new_from_message.
			 */
			mi->flags |= flags;
		}
		if (g_datalist_get_data (&fetch_data, "RFC822.SIZE"))
			mi->size = GPOINTER_TO_INT (g_datalist_get_data (&fetch_data, "RFC822.SIZE"));

		g_datalist_clear (&fetch_data);
	}
	camel_imap_response_free_without_processing (store, response);

	for (i = 0; i < messages->len; i++) {
		mi = messages->pdata[i];
		if (!mi) {
			g_warning ("No information for message %d", i + first);
			continue;
		}
		camel_folder_summary_add (folder->summary, mi);
		camel_folder_change_info_add_uid (changes, camel_message_info_uid (mi));
	}
	g_ptr_array_free (messages, TRUE);

	/* Did more mail arrive while we were doing this? */
	if (exists && exists > camel_folder_summary_count (folder->summary))
		imap_update_summary (folder, changes, ex);
}

/* Called with the store's command_lock locked */
void
camel_imap_folder_changed (CamelFolder *folder, int exists,
			   GArray *expunged, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelFolderChangeInfo *changes;
	CamelMessageInfo *info;
	int len;

	CAMEL_IMAP_STORE_ASSERT_LOCKED (folder->parent_store, command_lock);

	changes = camel_folder_change_info_new ();
	if (expunged) {
		int i, id;

		for (i = 0; i < expunged->len; i++) {
			id = g_array_index (expunged, int, i);
			info = camel_folder_summary_index (folder->summary, id - 1);
			camel_folder_change_info_remove_uid (changes, camel_message_info_uid (info));
			/* It's safe to not lock around this. */
			camel_imap_message_cache_remove (imap_folder->cache, camel_message_info_uid (info));
			camel_folder_summary_remove (folder->summary, info);
			camel_folder_summary_info_free(folder->summary, info);
		}
	}

	len = camel_folder_summary_count (folder->summary);
	if (exists > len)
		imap_update_summary (folder, changes, ex);

	if (camel_folder_change_info_changed (changes)) {
		camel_object_trigger_event (CAMEL_OBJECT (folder),
					    "folder_changed", changes);
	}
	camel_folder_change_info_free (changes);
}


CamelStream *
camel_imap_folder_fetch_data (CamelImapFolder *imap_folder, const char *uid,
			      const char *section_text, gboolean cache_only,
			      CamelException *ex)
{
	CamelFolder *folder = CAMEL_FOLDER (imap_folder);
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelImapResponse *response;
	CamelStream *stream;
	GData *fetch_data;
	char *found_uid;
	int i;

	CAMEL_IMAP_FOLDER_LOCK (imap_folder, cache_lock);
	stream = camel_imap_message_cache_get (imap_folder->cache, uid, section_text);
	if (stream || cache_only) {
		CAMEL_IMAP_FOLDER_UNLOCK (imap_folder, cache_lock);
		return stream;
	}

	if (!camel_imap_store_check_online (store, NULL)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				     _("This message is not currently available"));
		return NULL;
	}

	if (store->server_level < IMAP_LEVEL_IMAP4REV1 && !*section_text) {
		response = camel_imap_command (store, folder, ex,
					       "UID FETCH %s RFC822.PEEK",
					       uid);
	} else {
		response = camel_imap_command (store, folder, ex,
					       "UID FETCH %s BODY.PEEK[%s]",
					       uid, section_text);
	}
	if (!response) {
		CAMEL_IMAP_FOLDER_UNLOCK (imap_folder, cache_lock);
		return NULL;
	}

	for (i = 0; i < response->untagged->len; i++) {
		fetch_data = parse_fetch_response (imap_folder, response->untagged->pdata[i]);
		found_uid = g_datalist_get_data (&fetch_data, "UID");
		stream = g_datalist_get_data (&fetch_data, "BODY_PART_STREAM");
		if (found_uid && stream && !strcmp (uid, found_uid))
			break;

		g_datalist_clear (&fetch_data);
		stream = NULL;
	}
	camel_imap_response_free (store, response);
	CAMEL_IMAP_FOLDER_UNLOCK (imap_folder, cache_lock);
	if (!stream) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Could not find message body in FETCH "
					"response."));
	} else {
		camel_object_ref (CAMEL_OBJECT (stream));
		g_datalist_clear (&fetch_data);
	}

	return stream;
}

GData *
parse_fetch_response (CamelImapFolder *imap_folder, char *response)
{
	GData *data = NULL;
	char *start, *part_spec = NULL, *body = NULL, *uid = NULL;
	int body_len = 0;

	if (*response != '(') {
		long seq;

		if (*response != '*' || *(response + 1) != ' ')
			return NULL;
		seq = strtol (response + 2, &response, 10);
		if (seq == 0)
			return NULL;
		if (g_strncasecmp (response, " FETCH (", 8) != 0)
			return NULL;
		response += 7;
	}

	do {
		/* Skip the initial '(' or the ' ' between elements */
		response++;

		if (!g_strncasecmp (response, "FLAGS ", 6)) {
			guint32 flags;

			response += 6;
			/* FIXME user flags */
			flags = imap_parse_flag_list (&response);

			g_datalist_set_data (&data, "FLAGS", GUINT_TO_POINTER (flags));
		} else if (!g_strncasecmp (response, "RFC822.SIZE ", 12)) {
			unsigned long size;

			response += 12;
			size = strtoul (response, &response, 10);
			g_datalist_set_data (&data, "RFC822.SIZE", GUINT_TO_POINTER (size));
		} else if (!g_strncasecmp (response, "BODY[", 5) ||
			   !g_strncasecmp (response, "RFC822 ", 7)) {
			char *p;

			if (*response == 'B') {
				response += 5;
				p = strchr (response, ']');
				if (!p || *(p + 1) != ' ')
					break;
				part_spec = g_strndup (response, p - response);
				response = p + 2;
			} else {
				part_spec = g_strdup ("");
				response += 7;
			}

			body = imap_parse_nstring (&response, &body_len);
			if (!response) {
				g_free (part_spec);
				break;
			}

			if (!body)
				body = g_strdup ("");
			g_datalist_set_data_full (&data, "BODY_PART_SPEC", part_spec, g_free);
			g_datalist_set_data_full (&data, "BODY_PART_DATA", body, g_free);
			g_datalist_set_data (&data, "BODY_PART_LEN", GINT_TO_POINTER (body_len));
		} else if (!g_strncasecmp (response, "BODY ", 5) ||
			   !g_strncasecmp (response, "BODYSTRUCTURE ", 14)) {
			response = strchr (response, ' ') + 1;
			start = response;
			imap_skip_list (&response);
			g_datalist_set_data_full (&data, "BODY", g_strndup (start, response - start), g_free);
		} else if (!g_strncasecmp (response, "UID ", 4)) {
			int len;

			len = strcspn (response + 4, " )");
			uid = g_strndup (response + 4, len);
			g_datalist_set_data_full (&data, "UID", uid, g_free);
			response += 4 + len;
		} else {
			g_warning ("Unexpected FETCH response from server: "
				   "(%s", response);
			break;
		}
	} while (response && *response != ')');

	if (!response || *response != ')') {
		g_datalist_clear (&data);
		return NULL;
	}

	if (uid && body) {
		CamelStream *stream;

		CAMEL_IMAP_FOLDER_LOCK (imap_folder, cache_lock);
		stream = camel_imap_message_cache_insert (imap_folder->cache,
							  uid, part_spec,
							  body, body_len);
		CAMEL_IMAP_FOLDER_UNLOCK (imap_folder, cache_lock);
		g_datalist_set_data_full (&data, "BODY_PART_STREAM", stream,
					  (GDestroyNotify)camel_object_unref);
	}

	return data;
}

