/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "camel-test.h"
#include "messages.h"

#include <camel/camel-multipart.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-mem.h>


#if 0
static void
dump_mime_struct (CamelMimePart *mime_part, int depth)
{
	CamelDataWrapper *content;
	char *mime_type;
	int i = 0;
	
	while (i < depth) {
		printf ("   ");
		i++;
	}
	
	content = camel_medium_get_content_object ((CamelMedium *) mime_part);
	
	mime_type = camel_data_wrapper_get_mime_type (content);
	printf ("Content-Type: %s\n", mime_type);
	g_free (mime_type);
	
	if (CAMEL_IS_MULTIPART (content)) {
		guint num, index = 0;
		
		num = camel_multipart_get_number ((CamelMultipart *) content);
		while (index < num) {
			mime_part = camel_multipart_get_part ((CamelMultipart *) content, index);
			dump_mime_struct (mime_part, depth + 1);
			index++;
		}
	} else if (CAMEL_IS_MIME_MESSAGE (content)) {
		dump_mime_struct ((CamelMimePart *) content, depth + 1);
	}
}
#endif

int main (int argc, char **argv)
{
	struct dirent *dent;
	DIR *dir;
	int fd;
	
	camel_test_init (argc, argv);
	
	camel_test_start ("Message Test Suite");
	
	if (!(dir = opendir ("../data/messages")))
		return 77;
	
	while ((dent = readdir (dir)) != NULL) {
		CamelMimeMessage *message;
		CamelStream *stream;
		char *filename;
		struct stat st;
		
		if (dent->d_name[0] == '.')
			continue;
		
		filename = g_strdup_printf ("../data/messages/%s", dent->d_name);
		if (stat (filename, &st) == -1 || !S_ISREG (st.st_mode)) {
			g_free (filename);
			continue;
		}
		
		if ((fd = open (filename, O_RDONLY)) == -1) {
			g_free (filename);
			continue;
		}
		
		push ("testing message `%s`", filename);
		g_free (filename);
		
		stream = camel_stream_fs_new_with_fd (fd);
		message = camel_mime_message_new ();
		camel_data_wrapper_construct_from_stream ((CamelDataWrapper *) message, stream);
		camel_stream_reset (stream);
		
		/*dump_mime_struct ((CamelMimePart *) message, 0);*/
		test_message_compare (message);
		
		camel_object_unref (message);
		camel_object_unref (stream);
		
		pull ();
	}
	
	closedir (dir);
	
	camel_test_end ();
	
	return 0;
}
