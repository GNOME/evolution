/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-imap-folder.c: Abstract class for an imap folder */

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


#include <config.h> 

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include <gal/util/e-util.h>

#include "camel-imap-folder.h"
#include "camel-imap-command.h"
#include "camel-imap-store.h"
#include "camel-imap-stream.h"
#include "camel-imap-utils.h"
#include "string-utils.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel-stream-mem.h"
#include "camel-stream-buffer.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-mime-filter-crlf.h"
#include "camel-exception.h"
#include "camel-mime-utils.h"

#define d(x) x

#define CF_CLASS(o) (CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(o)))

static CamelFolderClass *parent_class = NULL;

static void imap_finalize (CamelObject *object);
static void imap_refresh_info (CamelFolder *folder, CamelException *ex);
static void imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
static void imap_expunge (CamelFolder *folder, CamelException *ex);

/* message counts */
static gint imap_get_message_count_internal (CamelFolder *folder, CamelException *ex);
static gint imap_get_message_count (CamelFolder *folder);
static gint imap_get_unread_message_count (CamelFolder *folder);

/* message manipulation */
static CamelMimeMessage *imap_get_message (CamelFolder *folder, const gchar *uid,
					   CamelException *ex);
static void imap_append_message (CamelFolder *folder, CamelMimeMessage *message,
				 const CamelMessageInfo *info, CamelException *ex);
static void imap_copy_message_to (CamelFolder *source, const char *uid,
				  CamelFolder *destination, CamelException *ex);
static void imap_move_message_to (CamelFolder *source, const char *uid,
				  CamelFolder *destination, CamelException *ex);

/* summary info */
static GPtrArray *imap_get_uids (CamelFolder *folder);
static GPtrArray *imap_get_summary_internal (CamelFolder *folder, CamelException *ex);
static GPtrArray *imap_get_summary (CamelFolder *folder);
static const CamelMessageInfo *imap_get_message_info (CamelFolder *folder, const char *uid);

/* searching */
static GPtrArray *imap_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex);

/* flag methods */
static guint32  imap_get_message_flags     (CamelFolder *folder, const char *uid);
static void     imap_set_message_flags     (CamelFolder *folder, const char *uid, guint32 flags, guint32 set);
static gboolean imap_get_message_user_flag (CamelFolder *folder, const char *uid, const char *name);
static void     imap_set_message_user_flag (CamelFolder *folder, const char *uid, const char *name,
					    gboolean value);


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
	
	camel_folder_class->get_uids = imap_get_uids;
	camel_folder_class->free_uids = camel_folder_free_nop;
	
	camel_folder_class->get_message_count = imap_get_message_count;
	camel_folder_class->get_unread_message_count = imap_get_unread_message_count;
	camel_folder_class->get_message = imap_get_message;
	camel_folder_class->append_message = imap_append_message;
	camel_folder_class->copy_message_to = imap_copy_message_to;
	camel_folder_class->move_message_to = imap_move_message_to;
	
	camel_folder_class->get_summary = imap_get_summary;
	camel_folder_class->get_message_info = imap_get_message_info;
	camel_folder_class->free_summary = camel_folder_free_nop;
	
	camel_folder_class->search_by_expression = imap_search_by_expression;
	
	camel_folder_class->get_message_flags = imap_get_message_flags;
	camel_folder_class->set_message_flags = imap_set_message_flags;
	camel_folder_class->get_message_user_flag = imap_get_message_user_flag;
	camel_folder_class->set_message_user_flag = imap_set_message_user_flag;
}

static void
camel_imap_folder_init (gpointer object, gpointer klass)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (object);
	CamelFolder *folder = CAMEL_FOLDER (object);
	
	folder->has_summary_capability = TRUE;
	folder->has_search_capability = TRUE;
	
	imap_folder->summary = NULL;
	imap_folder->summary_hash = NULL;

        /* some IMAP daemons support user-flags              *
	 * I would not, however, rely on this feature as     *
	 * most IMAP daemons do not support all the features */
	folder->permanent_flags = CAMEL_MESSAGE_SEEN |
		CAMEL_MESSAGE_ANSWERED |
		CAMEL_MESSAGE_FLAGGED |
		CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT |
		CAMEL_MESSAGE_USER;
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
camel_imap_folder_new (CamelStore *parent, const char *folder_name)
{
	CamelFolder *folder = CAMEL_FOLDER (camel_object_new (camel_imap_folder_get_type ()));
	const char *dir_sep, *short_name;
	
	dir_sep = CAMEL_IMAP_STORE (parent)->dir_sep;
	short_name = strrchr (folder_name, *dir_sep);
	if (short_name)
		short_name++;
	else
		short_name = folder_name;
	camel_folder_construct (folder, parent, folder_name, short_name);
	
	return folder;
}

static void
imap_summary_free (GPtrArray **summary)
{
	GPtrArray *array = *summary;
	gint i;
	
	if (array) {
		for (i = 0; i < array->len; i++)
			camel_message_info_free (array->pdata[i]);
		
		g_ptr_array_free (array, TRUE);
		*summary = NULL;
	}
}

static void
imap_folder_summary_free (CamelImapFolder *imap_folder)
{
	if (imap_folder->summary_hash) {
		g_hash_table_destroy (imap_folder->summary_hash);
		imap_folder->summary_hash = NULL;
	}
	
	imap_summary_free (&imap_folder->summary);
}

static void           
imap_finalize (CamelObject *object)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (object);

	imap_folder_summary_free (imap_folder);
}

static void
imap_refresh_info (CamelFolder *folder, CamelException *ex)
{
	imap_get_summary_internal (folder, ex);
}

static void
imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelImapResponse *response;
	gint i, max;
	
	if (expunge) {
		imap_expunge (folder, ex);
		return;
	}
	
	/* Set the flags on any messages that have changed this session */
	if (imap_folder->summary) {
		max = imap_folder->summary->len;
		for (i = 0; i < max; i++) {
			CamelMessageInfo *info;
			
			info = g_ptr_array_index (imap_folder->summary, i);
			if (info->flags & CAMEL_MESSAGE_FOLDER_FLAGGED) {
				char *flags;
				
				flags = imap_create_flag_list (info->flags);
				if (flags) {
					response = camel_imap_command (
						store, folder, ex,
						"UID STORE %s FLAGS.SILENT %s",
						info->uid, flags);
					g_free (flags);
					if (!response)
						return;
					camel_imap_response_free (response);
				}
				info->flags &= ~CAMEL_MESSAGE_FOLDER_FLAGGED;
			}
		}
	}
}

static void
imap_expunge (CamelFolder *folder, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelImapResponse *response;

	imap_sync (folder, FALSE, ex);
	response = camel_imap_command (store, folder, ex, "EXPUNGE");
	camel_imap_response_free (response);
}

static gint
imap_get_message_count_internal (CamelFolder *folder, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	char *result, *msg_count, *folder_path;
	CamelImapResponse *response;
	int count = 0;
	
	folder_path = camel_imap_store_folder_path (store, folder->full_name);
	if (store->has_status_capability)
		response = camel_imap_command (store, folder, ex,
					       "STATUS \"%s\" (MESSAGES)",
					       folder_path);
	else
		response = camel_imap_command (store, folder, ex,
					       "EXAMINE \"%s\"", folder_path);
	g_free (folder_path);
	if (!response)
		return 0;
	
	/* parse out the message count */
	if (store->has_status_capability) {
		/* should come in the form: "* STATUS <folder> (MESSAGES <count>)" */
		result = camel_imap_response_extract (response, "STATUS", NULL);
		if (result) {
			if ((msg_count = strstr (result, "MESSAGES")) != NULL) {
				msg_count = imap_next_word (msg_count);
			
				/* we should now be pointing to the message count */
				count = atoi (msg_count);
			}
			g_free (result);
		}
	} else {
		/* should come in the form: "* <count> EXISTS" */
		result = camel_imap_response_extract (response, "EXISTS", NULL);
		if (result) {
			if ((msg_count = strstr (result, "EXISTS")) != NULL) {
				for ( ; msg_count > result && *msg_count != '*'; msg_count--);
				
				msg_count = imap_next_word (msg_count);
				
				/* we should now be pointing to the message count */
				count = atoi (msg_count);
			}
			g_free (result);
		}
	}
	
	return count;
}

static gint
imap_get_message_count (CamelFolder *folder)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	
	if (imap_folder->summary)
		return imap_folder->summary->len;
	else
		return 0;
}

static gint
imap_get_unread_message_count (CamelFolder *folder)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelMessageInfo *info;
	GPtrArray *infolist;
	gint i, count = 0;
	
	g_return_val_if_fail (folder != NULL, 0);
	
	/* If we don't have a message count, return 0 */
	if (!imap_folder->summary)
		return 0;
	
	infolist = imap_folder->summary;
	
	for (i = 0; i < infolist->len; i++) {
		info = (CamelMessageInfo *) g_ptr_array_index (infolist, i);
		if (!(info->flags & CAMEL_MESSAGE_SEEN))
			count++;
	}
	
	return count;
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
	char *folder_path, *flagstr, *result;
	
	folder_path = camel_imap_store_folder_path (store, folder->full_name);
	
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

	response = camel_imap_command (store, NULL, ex, "APPEND %s%s%s {%d}",
				       folder_path, flagstr ? " " : "",
				       flagstr ? flagstr : "", ba->len);
	g_free (folder_path);
	g_free (flagstr);
	
	if (!response) {
		g_byte_array_free (ba, TRUE);
		return;
	}
	result = camel_imap_response_extract_continuation (response, ex);
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
	camel_imap_response_free (response);
}

static void
imap_copy_message_to (CamelFolder *source, const char *uid,
		      CamelFolder *destination, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (source->parent_store);
	CamelImapResponse *response;
	char *folder_path;
	
	folder_path = camel_imap_store_folder_path (store, destination->full_name);
	response = camel_imap_command (store, source, ex, "UID COPY %s \"%s\"",
				       uid, folder_path);
	camel_imap_response_free (response);
	g_free (folder_path);
}

/* FIXME: Duplication of code! */
static void
imap_move_message_to (CamelFolder *source, const char *uid,
		      CamelFolder *destination, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (source->parent_store);
	CamelImapResponse *response;
	char *folder_path;

	folder_path = camel_imap_store_folder_path (store, destination->full_name);	
	response = camel_imap_command (store, source, ex, "UID COPY %s \"%s\"",
				       uid, folder_path);
	camel_imap_response_free (response);
	g_free (folder_path);

	if (camel_exception_is_set (ex))
		return;

	camel_folder_delete_message (source, uid);
}

static GPtrArray *
imap_get_uids (CamelFolder *folder) 
{
	const CamelMessageInfo *info;
	GPtrArray *array, *infolist;
	gint i, count;
	
	infolist = imap_get_summary (folder);
	
	count = infolist ? infolist->len : 0;
	
	array = g_ptr_array_new ();
	g_ptr_array_set_size (array, count);
	
	for (i = 0; i < count; i++) {
		info = g_ptr_array_index (infolist, i);
		array->pdata[i] = g_strdup (info->uid);
	}
	
	return array;
}

static CamelMimeMessage *
imap_get_message (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelImapResponse *response;
	CamelStream *msgstream;
	CamelMimeMessage *msg;
	char *result, *mesg, *p;
	int len;

	response = camel_imap_command (store, folder, ex,
				       "UID FETCH %s RFC822", uid);
	if (!response)
		return NULL;
	result = camel_imap_response_extract (response, "FETCH", ex);
	if (!result)
		return NULL;

	p = strstr (result, "RFC822");
	if (p) {
		p += 7;
		mesg = imap_parse_nstring (&p, &len);
	}
	if (!p) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not find message body in FETCH "
				      "response.");
		g_free (result);
		return NULL;
	}
	g_free (result);

	msgstream = camel_stream_mem_new_with_buffer (mesg, len);
	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg),
						  msgstream);
	camel_object_unref (CAMEL_OBJECT (msgstream));
	g_free (mesg);

	return msg;
}

/* This probably shouldn't go here...but it will for now */
static gchar *
get_header_field (gchar *header, gchar *field)
{
	gchar *part, *index, *p, *q;
	
	index = (char *) e_strstrcase (header, field);
	if (index == NULL)
		return NULL;
	
	p = index + strlen (field) + 1;
	for (q = p; *q; q++)
		if (*q == '\n' && (*(q + 1) != ' ' && *(q + 1) != '\t'))
			break;
	
	part = g_strndup (p, (gint)(q - p));
	
	/* it may be wrapped on multiple lines, so lets strip out \n's */
	for (p = part; *p; ) {
		if (*p == '\n')
			memmove (p, p + 1, strlen (p));
		else
			p++;
	}
	
	return part;
}

static char *header_fields[] = { "subject", "from", "to", "cc", "date",
				 "received", "message-id", "references",
				 "in-reply-to", "" };
/**
 * imap_protocol_get_summary_specifier
 *
 * Make a data item specifier for the header lines we need,
 * appropriate to the server level.
 *
 * IMAP4rev1:  UID FLAGS BODY.PEEK[HEADER.FIELDS (SUBJECT FROM .. IN-REPLY-TO)]
 * IMAP4:      UID FLAGS RFC822.HEADER.LINES (SUBJECT FROM .. IN-REPLY-TO)
 **/
static char *
imap_protocol_get_summary_specifier (CamelImapStore *store)
{
	char *sect_begin, *sect_end;
	char *headers_wanted = "SUBJECT FROM TO CC DATE MESSAGE-ID REFERENCES IN-REPLY-TO";
	
	if (store->server_level >= IMAP_LEVEL_IMAP4REV1) {
		sect_begin = "BODY.PEEK[HEADER.FIELDS";
		sect_end = "]";
	} else {
		sect_begin = "RFC822.HEADER.LINES";
		sect_end   = "";
	}
	
	return g_strdup_printf ("UID FLAGS %s (%s)%s", sect_begin,
				headers_wanted, sect_end);
}

static GPtrArray *
imap_get_summary_internal (CamelFolder *folder, CamelException *ex)
{
	/* This ALWAYS updates the summary except on fail */
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelImapResponse *response;
	GPtrArray *summary = NULL, *headers = NULL;
	GHashTable *hash = NULL;
	int num, i, j;
	char *q;
	const char *received;
	char *summary_specifier;
	struct _header_raw *h = NULL, *tail = NULL;
	
	num = imap_get_message_count_internal (folder, ex);
	
	/* sync any previously set/changed message flags */
	imap_sync (folder, FALSE, ex);
	
	if (num == 0) {
		/* clean up any previous summary data */
		imap_folder_summary_free (imap_folder);
		
		imap_folder->summary = g_ptr_array_new ();
		imap_folder->summary_hash = g_hash_table_new (g_str_hash, g_str_equal);
		
		return imap_folder->summary;
	}
	
	summary_specifier = imap_protocol_get_summary_specifier (store);
	if (num == 1) {
		response = camel_imap_command (store, folder, ex,
					       "FETCH 1 (%s)",
					       summary_specifier);
	} else {
		response = camel_imap_command (store, folder, ex,
					       "FETCH 1:%d (%s)", num,
					       summary_specifier);
	}
	g_free (summary_specifier);
	
	if (!response) {
		if (!imap_folder->summary) {
			imap_folder->summary = g_ptr_array_new ();
			imap_folder->summary_hash = g_hash_table_new (g_str_hash, g_str_equal);
		}
		
		return imap_folder->summary;
	}
	headers = response->untagged;

	/* initialize our new summary-to-be */
	summary = g_ptr_array_new ();
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	
	for (i = 0; i < headers->len; i++) {
		CamelMessageInfo *info;
		char *uid, *flags, *header;
		
		info = g_malloc0 (sizeof (CamelMessageInfo));
		
		/* lets grab the UID... */
		if (!(uid = strstr (headers->pdata[i], "UID "))) {
			d(fprintf (stderr, "Cannot get a uid for %d\n\n%s\n\n", i+1, (char *) headers->pdata[i]));
			g_free (info);
			break;
		}
		
		for (uid += 4; *uid && (*uid < '0' || *uid > '9'); uid++); /* advance to <uid> */
		for (q = uid; *q && *q >= '0' && *q <= '9'; q++); /* find the end of the <uid> */
		info->uid = g_strndup (uid, (gint)(q - uid));
		/*d(fprintf (stderr, "*** info->uid = %s\n", info->uid));*/
		
		/* now lets grab the FLAGS */
		if (!(flags = strstr (headers->pdata[i], "FLAGS "))) {
			d(fprintf (stderr, "We didn't seem to get any flags for %d...\n", i));
			g_free (info->uid);
			g_free (info);
			break;
		}
		
		for (flags += 6; *flags && *flags != '('; flags++); /* advance to <flags> */
		info->flags = imap_parse_flag_list (flags);
		
		/* construct the header list */
		/* fast-forward to beginning of header info... */
		for (header = headers->pdata[i]; *header && *header != '\n'; header++);
		h = NULL;
		for (j = 0; *header_fields[j]; j++) {
			struct _header_raw *raw;
			char *field, *value;
			
			field = g_strdup_printf ("\n%s:", header_fields[j]);
			value = get_header_field (header, field);
			g_free (field);
			if (!value)
				continue;
			
			raw = g_malloc0 (sizeof (struct _header_raw));
			raw->next = NULL;
			raw->name = g_strdup (header_fields[j]);
			raw->value = value;
			raw->offset = -1;
			
			if (!h) {
				h = raw;
				tail = h;
			} else {
				tail->next = raw;
				tail = raw;
			}
		}
		
		/* construct the CamelMessageInfo */
		info->subject = camel_folder_summary_format_string (h, "subject");
		info->from = camel_folder_summary_format_address (h, "from");
		info->to = camel_folder_summary_format_address (h, "to");
		info->cc = camel_folder_summary_format_address (h, "cc");
		info->user_flags = NULL;
		info->date_sent = header_decode_date (header_raw_find (&h, "date", NULL), NULL);
		received = header_raw_find (&h, "received", NULL);
		if (received)
			received = strrchr (received, ';');
		if (received)
			info->date_received = header_decode_date (received + 1, NULL);
		else
			info->date_received = 0;
		info->message_id = header_msgid_decode (header_raw_find (&h, "message-id", NULL));
		/* if we have a references, use that, otherwise, see if we have an in-reply-to
		   header, with parsable content, otherwise *shrug* */
		info->references = header_references_decode (header_raw_find (&h, "references", NULL));
		if (info->references == NULL)
			info->references = header_references_decode (header_raw_find (&h, "in-reply-to", NULL));
		
		header_raw_clear (&h);
		
		g_ptr_array_add (summary, info);
		g_hash_table_insert (hash, info->uid, info);
	}
	camel_imap_response_free (response);
	
	/* clean up any previous summary data */
	imap_folder_summary_free (imap_folder);
	
	imap_folder->summary = summary;
	imap_folder->summary_hash = hash;
	
	return imap_folder->summary;
}

static GPtrArray *
imap_get_summary (CamelFolder *folder)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	
	return imap_folder->summary;
}

/* get a single message info from the server */
static CamelMessageInfo *
imap_get_message_info_internal (CamelFolder *folder, guint id, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (folder->parent_store);
	CamelImapResponse *response;
	CamelMessageInfo *info = NULL;
	struct _header_raw *h, *tail = NULL;
	const char *received;
	char *result, *uid, *flags, *header, *q;
	char *summary_specifier;
	int j;
	
	/* we don't have a cached copy, so fetch it */
	summary_specifier = imap_protocol_get_summary_specifier (store);
	response = camel_imap_command (store, folder, ex,
				     "FETCH %d (%s)", id, summary_specifier);
	g_free (summary_specifier);

	if (!response)
		return NULL;
	result = camel_imap_response_extract (response, "FETCH", ex);
	if (!result)
		return NULL;
	
	/* lets grab the UID... */
	if (!(uid = (char *) e_strstrcase (result, "UID "))) {
		d(fprintf (stderr, "Cannot get a uid for %d\n\n%s\n\n", id, result));
		g_free (result);
		return NULL;
	}
		
	for (uid += 4; *uid && (*uid < '0' || *uid > '9'); uid++); /* advance to <uid> */
	for (q = uid; *q && *q >= '0' && *q <= '9'; q++); /* find the end of the <uid> */
	uid = g_strndup (uid, (gint)(q - uid));
	
	info = g_malloc0 (sizeof (CamelMessageInfo));
	info->uid = uid;
	d(fprintf (stderr, "*** info->uid = %s\n", info->uid));
	
	/* now lets grab the FLAGS */
	if (!(flags = strstr (q, "FLAGS "))) {
		d(fprintf (stderr, "We didn't seem to get any flags for %s...\n", uid));
		g_free (info->uid);
		g_free (info);
		g_free (result);
		return NULL;
	}
	
	for (flags += 6; *flags && *flags != '('; flags++); /* advance to <flags> */
	info->flags = imap_parse_flag_list (flags);
	
	/* construct the header list */
	/* fast-forward to beginning of header info... */
	for (header = q; *header && *header != '\n'; header++);
	h = NULL;
	for (j = 0; *header_fields[j]; j++) {
		struct _header_raw *raw;
		char *field, *value;
		
		field = g_strdup_printf ("\n%s:", header_fields[j]);
		value = get_header_field (header, field);
		g_free (field);
		if (!value)
			continue;
		
		raw = g_malloc0 (sizeof (struct _header_raw));
		raw->next = NULL;
		raw->name = g_strdup (header_fields[j]);
		raw->value = value;
		raw->offset = -1;
		
		if (!h) {
			h = raw;
			tail = h;
		} else {
			tail->next = raw;
			tail = raw;
		}
	}
	
	/* construct the CamelMessageInfo */
	info->subject = camel_folder_summary_format_string (h, "subject");
	info->from = camel_folder_summary_format_address (h, "from");
	info->to = camel_folder_summary_format_address (h, "to");
	info->cc = camel_folder_summary_format_address (h, "cc");
	info->user_flags = NULL;
	info->date_sent = header_decode_date (header_raw_find (&h, "date", NULL), NULL);
	received = header_raw_find (&h, "received", NULL);
	if (received)
		received = strrchr (received, ';');
	if (received)
		info->date_received = header_decode_date (received + 1, NULL);
	else
		info->date_received = 0;
	info->message_id = header_msgid_decode (header_raw_find (&h, "message-id", NULL));
	/* if we have a references, use that, otherwise, see if we have an in-reply-to
	   header, with parsable content, otherwise *shrug* */
	info->references = header_references_decode (header_raw_find (&h, "references", NULL));
	if (info->references == NULL)
		info->references = header_references_decode (header_raw_find (&h, "in-reply-to", NULL));
	
	header_raw_clear (&h);
	g_free (result);
	
	return info;
}

/* get a single message info, by uid */
static const CamelMessageInfo *
imap_get_message_info (CamelFolder *folder, const char *uid)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	
	if (imap_folder->summary)
		return g_hash_table_lookup (imap_folder->summary_hash, uid);
	
	return NULL;
}

static GPtrArray *
imap_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex)
{
	CamelImapResponse *response;
	GPtrArray *uids = NULL;
	char *result, *sexp, *p;
	
	d(fprintf (stderr, "camel sexp: '%s'\n", expression));
	sexp = imap_translate_sexp (expression);
	d(fprintf (stderr, "imap sexp: '%s'\n", sexp));
	
	uids = g_ptr_array_new ();
	
	if (!folder->has_search_capability) {
		g_free (sexp);
		return uids;
	}
	
	response = camel_imap_command (CAMEL_IMAP_STORE (folder->parent_store),
				       folder, NULL, "UID SEARCH %s", sexp);
	g_free (sexp);
	if (!response)
		return uids;

	result = camel_imap_response_extract (response, "SEARCH", NULL);
	if (!result)
		return uids;
	
	if ((p = strstr (result, "* SEARCH"))) {
		char *word;
		
		word = imap_next_word (p); /* word now points to SEARCH */
		
		for (word = imap_next_word (word); *word && *word != '*'; word = imap_next_word (word)) {
			gboolean word_is_numeric = TRUE;
			char *ep;
			
			/* find the end of this word and make sure it's a numeric uid */
			for (ep = word; *ep && *ep != ' ' && *ep != '\n'; ep++)
				if (*ep < '0' || *ep > '9')
					word_is_numeric = FALSE;
			
			if (word_is_numeric)
				g_ptr_array_add (uids, g_strndup (word, (gint)(ep - word)));
		}
	}
	
	g_free (result);
	
	return uids;
}

static guint32
imap_get_message_flags (CamelFolder *folder, const char *uid)
{
	const CamelMessageInfo *info;
	
	info = imap_get_message_info (folder, uid);
	g_return_val_if_fail (info != NULL, 0);
	
	return info->flags;
}

static void
imap_set_message_flags (CamelFolder *folder, const char *uid, guint32 flags, guint32 set)
{
	CamelMessageInfo *info;
	
	info = (CamelMessageInfo*)imap_get_message_info (folder, uid);
	g_return_if_fail (info != NULL);
	
	info->flags = (info->flags & ~flags) | (set & flags) | CAMEL_MESSAGE_FOLDER_FLAGGED;
	
	camel_object_trigger_event (CAMEL_OBJECT (folder), "message_changed", (gpointer *) uid);
}

static gboolean
imap_get_message_user_flag (CamelFolder *folder, const char *uid, const char *name)
{
	return FALSE;
}

static void
imap_set_message_user_flag (CamelFolder *folder, const char *uid, const char *name, gboolean value)
{
	camel_object_trigger_event (CAMEL_OBJECT (folder), "message_changed", (gpointer *) uid);
}

void
camel_imap_folder_changed (CamelFolder *folder, gint recent, GArray *expunged, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	
	if (expunged) {
		gint i, id;
		
		for (i = 0; i < expunged->len; i++) {
			id = g_array_index (expunged, int, i);
			d(fprintf (stderr, "Expunging message %d from the summary (i = %d)\n", id + i, i));
			
			if (id <= imap_folder->summary->len) {
				CamelMessageInfo *info;
				
				info = (CamelMessageInfo *) imap_folder->summary->pdata[id - 1];
				
				/* remove from the lookup table and summary */
				g_hash_table_remove (imap_folder->summary_hash, info->uid);
				g_ptr_array_remove_index (imap_folder->summary, id - 1);
				
				camel_message_info_free (info);
			} else {
				/* Hopefully this should never happen */
				d(fprintf (stderr, "imap expunge-error: message %d is out of range\n", id));
			}
		}
	}
	
	if (recent > 0) {
		CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
		CamelMessageInfo *info;
		gint i, j, last, slast;
		
		if (!imap_folder->summary) {
			imap_folder->summary = g_ptr_array_new ();
			imap_folder->summary_hash = g_hash_table_new (g_str_hash, g_str_equal);
		}
		
		last = imap_folder->summary->len + 1;
		slast = imap_get_message_count_internal (folder, ex);
		fprintf (stderr, "calculated next message is: %d\n", last);
		fprintf (stderr, "server says %d mesgs total\n", slast);
		slast -= (recent - 1);
		fprintf (stderr, "based on total, new guess is: %d\n", slast);
		
		for (i = slast, j = 0; j < recent; i++, j++) {
			info = imap_get_message_info_internal (folder, i, ex);
			if (info) {
				if (!imap_get_message_info (folder, info->uid)) {
					/* add to our summary */
					g_ptr_array_add (imap_folder->summary, info);
					g_hash_table_insert (imap_folder->summary_hash, info->uid, info);
				} else {
					/* we already have a record of it */
					camel_message_info_free (info);
					d(fprintf (stderr, "we already had message %d!!\n", i));
				}
			} else {
				/* our hack failed so now we need to do it the old fashioned way */
				/*imap_get_summary_internal (folder, ex);*/
				d(fprintf (stderr, "*** we tried to get message %d but failed\n", i));
				break;
			}
		}
	}
	
	camel_object_trigger_event (CAMEL_OBJECT (folder), "folder_changed", GINT_TO_POINTER (0));
}
