/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
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

#include "mh-uid.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel-stream-buffered-fs.h"
#include "gmime-utils.h"
#include "md5-utils.h"

guchar *
mh_uid_get_for_file (gchar *filename)
{
	CamelStream *message_stream;
	GArray *header_array;
	Rfc822Header *cur_header;
	int i;
	MD5Context ctx;
	guchar *md5_digest_uid;


	message_stream = camel_stream_buffered_fs_new_with_name (filename, 
								 CAMEL_STREAM_BUFFERED_FS_READ);
	header_array = get_header_array_from_stream (message_stream);
	gtk_object_unref (GTK_OBJECT (message_stream));
	
	md5_init (&ctx);
	for (i=0; i<header_array->len; i++) {
		cur_header = (Rfc822Header *)header_array->data + i;
		if (!g_strcasecmp (cur_header->name, "subject")) {
			md5_update (&ctx, cur_header->value, strlen (cur_header->value));
		} else if (!g_strcasecmp (cur_header->name, "sender")) {
			md5_update (&ctx, cur_header->value, strlen (cur_header->value));
		} else if (!g_strcasecmp (cur_header->name, "date")) {
			md5_update (&ctx, cur_header->value, strlen (cur_header->value));			
		}
		
		g_free (cur_header->name);
		g_free (cur_header->value);
		
	}
	
	g_array_free (header_array, TRUE);
	
	md5_digest_uid = g_new0 (guchar, 16);
	md5_final (md5_digest_uid, &ctx);

	return md5_digest_uid;
}


