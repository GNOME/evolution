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

#include "camel-nntp-folder.h"
#include "camel-nntp-store.h"
#include "camel-nntp-summary.h"
#include "camel-nntp-utils.h"
#include "camel-stream-buffer.h"
#include "camel-stream-mem.h"
#include "gmime-utils.h"

#include <stdlib.h>
#include <string.h>

static GArray*
get_XOVER_headers(CamelNNTPStore *nntp_store, CamelFolder *folder,
		  int first_message, int last_message)
{
	int status;

	status = camel_nntp_command (nntp_store, NULL,
				     "XOVER %d-%d",
				     first_message,
				     last_message);
		
	if (status == CAMEL_NNTP_OK) {
		CamelStream *nntp_istream = nntp_store->istream;
		GArray *array;
		gboolean done = FALSE;

		array = g_array_new(FALSE, FALSE, sizeof(CamelNNTPSummaryInformation));

		while (!done) {
			char *line;

			line = camel_stream_buffer_read_line ( 
					      CAMEL_STREAM_BUFFER ( nntp_istream ));

			if (*line == '.') {
				done = TRUE;
			}
			else {
				CamelNNTPSummaryInformation new_info;
				char **split_line = g_strsplit (line, "\t", 7);

				memset (&new_info, 0, sizeof(new_info));
					
				new_info.headers.subject = g_strdup(split_line[1]);
				new_info.headers.sender = g_strdup(split_line[2]);
				new_info.headers.to = g_strdup(folder->name);
				new_info.headers.sent_date = g_strdup(split_line[3]);
				/* XXX do we need to fill in both dates? */
				new_info.headers.received_date = g_strdup(split_line[3]);
				new_info.headers.size = atoi(split_line[5]);
				new_info.headers.uid = g_strdup(split_line[4]);
				g_strfreev (split_line);

				g_array_append_val(array, new_info);
			}
			g_free (line);
		}

		return array;
	}

	return NULL;
}

static GArray*
get_HEAD_headers(CamelNNTPStore *nntp_store, CamelFolder *folder,
		 int first_message, int last_message)
{
	int i;
	int status;
	GArray *array;
	CamelNNTPSummaryInformation info;

	array = g_array_new(FALSE, FALSE, sizeof(CamelNNTPSummaryInformation));

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
				if (!strcasecmp(header->name, "From"))
					info.headers.sender = g_strdup(header->value);
				else if (!strcasecmp(header->name, "To"))
					info.headers.to = g_strdup(header->value);
				else if (!strcasecmp(header->name, "Subject"))
					info.headers.subject = g_strdup(header->value);
				else if (!strcasecmp(header->name, "Message-ID"))
					info.headers.uid = g_strdup(header->value);
				else if (!strcasecmp(header->name, "Date")) {
					info.headers.sent_date = g_strdup(header->value);
					info.headers.received_date = g_strdup(header->value);
				}
			}
			g_array_append_val(array, info);
		}
		else if (status == CAMEL_NNTP_FAIL) {
			/* nasty things are afoot */
			g_warning ("failure doing HEAD\n");
			break;
		}
	}
	return array;
}

GArray *
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
				     "GROUP %s", folder->name);

	sscanf (ret, "%d %d %d", &nb_message, &first_message, &last_message);
	g_free (ret);

	if (status != CAMEL_NNTP_OK)
		return NULL;

	if (TRUE /* nntp_store->extensions & CAMEL_NNTP_EXT_XOVER */) {
		return get_XOVER_headers (nntp_store, folder, first_message, last_message);
	}
	else {
		return get_HEAD_headers (nntp_store, folder, first_message, last_message);
	}
}

