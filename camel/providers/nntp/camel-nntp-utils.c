/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-utils.c : utilities used by the nntp code. */

/* 
 * Author : Chris Toshok <toshok@helixcode.com> 
 *
 * Copyright (C) 2000 Helix Code .
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

#include "camel-folder-summary.h"
#include "camel-nntp-folder.h"
#include "camel-nntp-store.h"
#include "camel-nntp-utils.h"
#include "camel-stream-buffer.h"
#include "camel-stream-mem.h"

#include <stdlib.h>
#include <string.h>

static void
get_XOVER_headers(CamelNNTPStore *nntp_store, CamelFolder *folder,
		  int first_message, int last_message, CamelException *ex)
{
	int status;
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);

	status = camel_nntp_command (nntp_store, NULL,
				     "XOVER %d-%d",
				     first_message,
				     last_message);
		
	if (status == CAMEL_NNTP_OK) {
		CamelStream *nntp_istream = nntp_store->istream;
		gboolean done = FALSE;

		while (!done) {
			char *line;

			line = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER ( nntp_istream ));

			if (*line == '.') {
				done = TRUE;
			}
			else {
				CamelMessageInfo *new_info = g_new0(CamelMessageInfo, 1);
				char **split_line = g_strsplit (line, "\t", 7);

				new_info->subject = g_strdup(split_line[1]);
				new_info->from = g_strdup(split_line[2]);
				new_info->to = g_strdup(nntp_folder->group_name);
				new_info->date_sent = header_decode_date(split_line[3], NULL);
#if 0
				/* XXX do we need to fill in both dates? */
				new_info->headers.date_received = g_strdup(split_line[3]);
#endif
				new_info->size = atoi(split_line[5]);
				new_info->uid = g_strdup(split_line[4]);
				g_strfreev (split_line);

				camel_folder_summary_add (nntp_folder->summary, new_info);
			}
			g_free (line);
		}
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
		status = camel_nntp_command (nntp_store, NULL,
					     "HEAD %d", i);

		if (status == CAMEL_NNTP_OK) {
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
			buf = malloc(buf_alloc);
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
						buf = realloc (buf, buf_alloc);
					}
					strcat(buf, line);
					strcat(buf, "\n");
					buf_len += strlen(line);
					g_free (line);
				}
			}

			/* create a stream from which to parse the headers */
			header_stream = camel_stream_mem_new_with_buffer(buf,
								 buf_len,
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
				else if (!g_strcasecmp(header->name, "Message-ID"))
					new_info->uid = g_strdup(header->value);
				else if (!g_strcasecmp(header->name, "Date")) {
					new_info->date_sent = header_decode_date (header->value);
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

void
camel_nntp_get_headers (CamelStore *store,
			CamelNNTPFolder *nntp_folder,
			CamelException *ex)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);
	CamelFolder *folder = CAMEL_FOLDER (nntp_folder);
	char *ret;
	int first_message, nb_message, last_message;
	int status;

	status = camel_nntp_command (nntp_store, &ret,
				     "GROUP %s", CAMEL_NNTP_FOLDER (folder)->group_name);

	sscanf (ret, "%d %d %d", &nb_message, &first_message, &last_message);
	g_free (ret);

	if (status != CAMEL_NNTP_OK) {
		/* XXX throw invalid group exception */
		printf ("invalid group\n");
		return;
	}

#if 0
	if (nntp_store->extensions & CAMEL_NNTP_EXT_XOVER) {
#endif
		get_XOVER_headers (nntp_store, folder, first_message, last_message, ex);
#if 0
	}
	else {
		get_HEAD_headers (nntp_store, folder, first_message, last_message, ex);
	}
#endif
}

