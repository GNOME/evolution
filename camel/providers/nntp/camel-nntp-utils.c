/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-utils.c : utilities used by the nntp code. */

/* 
 * Author : Chris Toshok <toshok@ximian.com> 
 *
 * Copyright (C) 2000 Ximian .
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

#include "camel-folder-summary.h"
#include "camel-nntp-resp-codes.h"
#include "camel-nntp-folder.h"
#include "camel-nntp-store.h"
#include "camel-nntp-utils.h"
#include "camel-stream-mem.h"
#include "camel-exception.h"

#include "libedataserver/md5-utils.h"

#include <stdlib.h>
#include <string.h>

static void
get_XOVER_headers(CamelNNTPStore *nntp_store, CamelFolder *folder,
		  int first_message, int last_message, CamelException *ex)
{
	int status;
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	char digest[16];

	status = camel_nntp_command (nntp_store, ex, NULL,
				     "XOVER %d-%d",
				     first_message,
				     last_message);

	if (status == NNTP_DATA_FOLLOWS) {
		gboolean done = FALSE;

		while (!done) {
			char *line;

			if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (nntp_store), &line, ex) < 0) {
				g_warning ("failed to recv_line while building OVER header list\n");
				break;
			}

			if (*line == '.') {
				done = TRUE;
				g_print ("done\n");
			}
			else {
				CamelMessageInfo *new_info = camel_folder_summary_info_new(folder->summary);
				char **split_line = g_strsplit (line, "\t", 7);
				char *subject, *from, *date, *message_id, *bytes;
				char *uid;

				subject = split_line [nntp_store->overview_field [CAMEL_NNTP_OVER_SUBJECT].index];
				from = split_line [nntp_store->overview_field [CAMEL_NNTP_OVER_FROM].index];
				date = split_line [nntp_store->overview_field [CAMEL_NNTP_OVER_DATE].index];
				message_id = split_line [nntp_store->overview_field [CAMEL_NNTP_OVER_MESSAGE_ID].index];
				bytes = split_line [nntp_store->overview_field [CAMEL_NNTP_OVER_BYTES].index];
				
				/* if the overview format flagged this
                                   field as "full", skip over the
                                   preceding field name and colon */
				if (nntp_store->overview_field [ CAMEL_NNTP_OVER_SUBJECT ].full)
					subject += strlen ("Subject:");
				if (nntp_store->overview_field [ CAMEL_NNTP_OVER_FROM ].full)
					from += strlen ("From:");
				if (nntp_store->overview_field [ CAMEL_NNTP_OVER_DATE ].full)
					date += strlen ("Date:");
				if (nntp_store->overview_field [ CAMEL_NNTP_OVER_MESSAGE_ID ].full)
					message_id += strlen ("Message-ID:");
				if (nntp_store->overview_field [ CAMEL_NNTP_OVER_BYTES ].full)
					bytes += strlen ("Bytes:");

				uid = g_strdup_printf ("%s,%s", split_line[0], message_id);
				camel_message_info_set_subject(new_info, g_strdup(subject));
				camel_message_info_set_from(new_info, g_strdup(from));
				camel_message_info_set_to(new_info, g_strdup(folder->name));
				camel_message_info_set_uid(new_info, uid);

				new_info->date_sent = camel_header_decode_date(date, NULL);
#if 0
				/* XXX do we need to fill in both dates? */
				new_info->headers.date_received = g_strdup(date);
#endif
				new_info->size = atoi(bytes);
				md5_get_digest(message_id, strlen(message_id), digest);
				memcpy(new_info->message_id.id.hash, digest, sizeof(new_info->message_id.id.hash));

				if (camel_nntp_newsrc_article_is_read (nntp_store->newsrc,
								       folder->name,
								       atoi (split_line[0])))
				    new_info->flags |= CAMEL_MESSAGE_SEEN;

				camel_folder_summary_add (folder->summary, new_info);
				g_strfreev (split_line);
			}
			g_free (line);
		}
	}
	else {
		/* XXX */
		g_warning ("weird nntp response for XOVER: %d\n", status);
	}
}

#if 0
static GArray*
get_HEAD_headers(CamelNNTPStore *nntp_store, CamelFolder *folder,
		 int first_message, int last_message, CamelException *ex)
{
	int i;
	int status;

	for (i = first_message; i < last_message; i ++) {
		status = camel_nntp_command (nntp_store, ex, NULL,
					     "HEAD %d", i);

		if (status == NNTP_HEAD_FOLLOWS) {
			gboolean done = FALSE;
			char *buf;
			int buf_len;
			int buf_alloc;
			int h;
			CamelStream *header_stream;
			GArray *header_array;
			CamelStream *nntp_istream;
			CamelMessageInfo *new_info = g_new0(CamelMessageInfo, 1);

			buf_alloc = 2048;
			buf_len = 0;
			buf = g_malloc(buf_alloc);
			done = FALSE;

			buf[0] = 0;

			nntp_istream = nntp_store->istream;

			while (!done) {
				char *line;
				int line_length;

				line = camel_stream_buffer_read_line ( 
						      CAMEL_STREAM_BUFFER ( nntp_istream ));
				line_length = strlen ( line );

				if (*line == '.') {
					done = TRUE;
				}
				else {
					if (buf_len + line_length > buf_alloc) {
						buf_alloc *= 2;
						buf = g_realloc (buf, buf_alloc);
					}
					strcat(buf, line);
					strcat(buf, "\n");
					buf_len += strlen(line);
					g_free (line);
				}
			}

			/* create a stream from which to parse the headers */
			header_stream = camel_stream_mem_new_with_buffer (buf, buf_len,
									  CAMEL_STREAM_MEM_READ);

			header_array = get_header_array_from_stream (header_stream);

			memset (&info, 0, sizeof(info));

			for (h = 0; h < header_array->len; h ++) {
				Rfc822Header *header = &((Rfc822Header*)header_array->data)[h];
				if (!g_strcasecmp(header->name, "From"))
					new_info->from = g_strdup(header->value);
				else if (!g_strcasecmp(header->name, "To"))
					new_info->to = g_strdup(header->value);
				else if (!g_strcasecmp(header->name, "Subject"))
					new_info->subject = g_strdup(header->value);
				else if (!g_strcasecmp(header->name, "Message-ID")) {
					new_info->uid = g_strdup_printf("%d,%s", i, header->value);
					new_info->message_id = g_strdup(header->value);
				}
				else if (!g_strcasecmp(header->name, "Date")) {
					new_info->date_sent = camel_header_decode_date (header->value);
#if 0
					new_info->date_sent = g_strdup(header->value);
					new_info->date_received = g_strdup(header->value);
#endif
				}
			}

			camel_folder_summary_add (nntp_folder->summary, new_info);
		}
		else if (status == CAMEL_NNTP_FAIL) {
			/* nasty things are afoot */
			g_warning ("failure doing HEAD\n");
			break;
		}
	}
}
#endif

static inline int
uid_num (CamelFolderSummary *summary, int index)
{
	char *tmp;
	char *brk;
	CamelMessageInfo *minfo;
	int ret;
	
	minfo = camel_folder_summary_index(summary, index);
	if(minfo == NULL)
		return 0;

	tmp = g_strdup(camel_message_info_uid(minfo));
	camel_message_info_free(minfo);
	
	if((brk = strchr(tmp, ',')) == NULL)
		ret = 0;
	else {
		*brk = 0;
		ret = atoi(tmp);
	}
	
	g_free(tmp);
	
	return ret;
}

void
camel_nntp_get_headers (CamelStore *store,
			CamelNNTPFolder *nntp_folder,
			CamelException *ex)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);
	CamelFolder *folder = CAMEL_FOLDER (nntp_folder);
	char *ret;
	int first_message, nb_message, last_message, last_summary;
	int status;
	int i;

	status = camel_nntp_command (nntp_store, ex, &ret,
				     "GROUP %s", folder->name);
	sscanf (ret, "%d %d %d", &nb_message, &first_message, &last_message);
	g_free (ret);

	i = camel_folder_summary_count(folder->summary);
	if(i != 0) {
		last_summary = uid_num(folder->summary, i-1);

		if(last_summary < first_message)
			camel_folder_summary_clear(folder->summary);
		else {
			while(uid_num(folder->summary, 0) < first_message) 
				camel_folder_summary_remove_index(folder->summary, 0);
			
			if(last_summary >= last_message)
				return;
			
			first_message = last_summary;
		}
	}
			
	if (status == NNTP_NO_SUCH_GROUP) {
		/* XXX throw invalid group exception */
		camel_exception_setv (ex, 
				      CAMEL_EXCEPTION_FOLDER_INVALID,
				      "group %s not found on server",
				      folder->name);
		return;
	}


	if (nntp_store->extensions & CAMEL_NNTP_EXT_OVER) {
		get_XOVER_headers (nntp_store, folder, first_message, last_message, ex);
	}
	else {
		g_warning ("need to fix get_HEAD_headers\n");
#if 0
		get_HEAD_headers (nntp_store, folder, first_message, last_message, ex);
#endif
	}		

}
