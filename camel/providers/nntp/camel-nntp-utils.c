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

#include <stdlib.h>

GArray *
camel_nntp_get_headers (CamelStore *store,
			CamelNNTPFolder *nntp_folder,
			CamelException *ex)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);
	CamelFolder *folder = CAMEL_FOLDER (nntp_folder);

	if (TRUE /* nntp_store->extensions & CAMEL_NNTP_EXT_XOVER */) {
		int status;
		char *ret;
		int first_message, nb_message, last_message;

		status = camel_nntp_command (nntp_store, &ret,
					     "GROUP %s", folder->name);

		if (status != CAMEL_NNTP_OK)
			return NULL;

		sscanf (ret, "%d %d %d", &nb_message, &first_message, &last_message);
		g_free (ret);

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
					
					new_info.headers.subject = g_strdup(split_line[1]);
					new_info.headers.sender = g_strdup(split_line[2]);
					new_info.headers.to = g_strdup(folder->name);
					new_info.headers.sent_date = g_strdup(split_line[3]);
					/* XXX do we need to fill in both dates? */
					new_info.headers.received_date = g_strdup(split_line[3]);
					new_info.headers.size = atoi(split_line[5]);
					new_info.headers.uid = g_strdup(split_line[4]);
					g_strfreev (split_line);

					printf ("%s\t%s\t%s\n", new_info.headers.subject,
						new_info.headers.sender,
						new_info.headers.uid);
					g_array_append_val(array, new_info);
				}
				g_free (line);
			}

			return array;
		}
	}
	else {
		/* do HEAD stuff for the range of articles */
	}

	return NULL;
}

