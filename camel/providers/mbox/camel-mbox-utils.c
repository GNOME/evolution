/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Various utilities for the mbox provider */

/* 
 * Authors : 
 *   Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright (C) 1999 Helix Code.
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


/* "xev" stands for x-evolution, which is the name of the
 * evolution specific header where are stored informations
 * like : 
 *   - mail status 
 *   - mail uid 
 *  ...
 *
 *
 * The evolution line ha10s the following format :
 *
 *   X-Evolution:XXXXX-X
 *               \___/ \/
 *          UID ---'    `- Status 
 * 
 * the UID is internally used as a 32 bits long integer, but only the first 24 bits are 
 * used. The UID is coded as a string on 4 characters. Each character is a 6 bits 
 * integer coded using the b64 alphabet. 
 *
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


#include <glib.h>
#include "camel-mbox-utils.h"
#include "camel-mbox-parser.h"
#include "camel-folder-summary.h"
#include "camel-mbox-summary.h"



static gchar b64_alphabet[64] = 
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";



static void 
uid_to_string (guint32 uid, gchar string[4])
{
	
	string [0] = b64_alphabet [(uid >> 18) & 0x3f];
	string [1] = b64_alphabet [(uid >> 12) & 0x3f];
	string [2] = b64_alphabet [(uid >>  6) & 0x3f];
	string [3] = b64_alphabet [(uid      ) & 0x3f];
}


static guint32
string_to_uid (gchar *string)
{
	guint32 i;
	
	i = 
		(((string [0] >= 97) ? ( string [0] - 71 ) :
		 ((string [0] >= 65) ? ( string [0] - 65 ) :
		  ((string [0] >= 48) ? ( string [0] + 4 ) :
		   ((string [0] == 43) ? 62 : 63 )))) << 18)		
		
		+ (((string [1] >= 97) ? ( string [1] - 71 ) :
		   ((string [1] >= 65) ? ( string [1] - 65 ) :
		    ((string [1] >= 48) ? ( string [1] + 4 ) :
		     ((string [1] == 43) ? 62 : 63 )))) << 12)
		
		
		+ ((((string [2] >= 97) ? ( string [2] - 71 ) :
		   ((string [2] >= 65) ? ( string [2] - 65 ) :
		    ((string [2] >= 48) ? ( string [2] + 4 ) :
		     ((string [2] == 43) ? 62 : 63 ))))) << 6)
		
		
		+ (((string [3] >= 97) ? ( string [3] - 71 ) :
		   ((string [3] >= 65) ? ( string [3] - 65 ) :
		    ((string [3] >= 48) ? ( string [3] + 4 ) :
		     ((string [3] == 43) ? 62 : 63 )))));
	
	return i;
	
}


static gchar
flag_to_string (guchar status)
{
	return b64_alphabet [status & 0x3f];
}


static guchar
string_to_flag (gchar string)
{	
	return (string >= 97) ? ( string - 71 ) :
		((string >= 65) ? ( string - 65 ) :
		 ((string >= 48) ? ( string + 4 ) :
		  ((string == 43) ? 62 : 63 )));
}





void 
camel_mbox_xev_parse_header_content (gchar header_content[6], 
				     guint32 *uid, 
				     guchar *status)
{
	
	/* we assume that the first 4 characters of the header content 
	   are actually the uid stuff. If somebody messed with it ...
	   toooo bad. 
	*/
	*uid = string_to_uid (header_content);
	*status = string_to_flag (header_content[5]);
}

void 
camel_mbox_xev_write_header_content (gchar header_content[6], 
				     guint32 uid, 
				     guchar status)
{
	uid_to_string (uid, header_content);
	header_content[5] = flag_to_string (status);
	header_content[4] = '-';
}






static void 
copy_file_chunk (gint fd_src,
		 gint fd_dest, 
		 glong nb_bytes, 
		 CamelException *ex)
{
	gchar buffer [1000];
	glong nb_to_read;
	glong nb_read, v;
	
	
	nb_to_read = nb_bytes;
	while (nb_to_read > 0) {
		
		do {
			nb_read = read (fd_src, buffer, MIN (1000, nb_to_read));
		} while (nb_read == -1 && errno == EINTR);
		
		if (nb_read == -1) {
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					     "could read from the mbox file");
			return;
		}


		nb_to_read -= nb_read;

		do {
			v = write (fd_dest, buffer, nb_read);
		} while (v == -1 && errno == EINTR);

		if (v == -1) {
			camel_exception_setv (ex, 
					     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					     "could not write to the mbox copy file\n"
					      "Full error is : %s\n",
					      strerror (errno));
			return;
		}

		
	}
	
	
	
}


glong
camel_mbox_write_xev (gchar *mbox_file_name,
		      GArray *summary_information, 
		      glong  next_uid, 
		      CamelException *ex)
{
	gint cur_msg;
	CamelMboxParserMessageInfo *cur_msg_info;
	gint fd1, fd2;
	guint bytes_to_copy = 0;
	glong cur_pos = 0;
	glong cur_offset = 0;
	glong end_of_last_message;
	glong next_free_uid;
	gchar xev_header[20] = "X-Evolution:XXXX-X\n";
	gchar *tmp_file_name;
	gchar *tmp_file_name_secure;
	gint rename_result;
	gint unlink_result;
	
	tmp_file_name = g_strdup_printf ("%s__.ev_tmp", mbox_file_name);
	tmp_file_name_secure = g_strdup_printf ("%s__.ev_tmp_secure", mbox_file_name);

	fd1 = open (mbox_file_name, O_RDONLY);
	fd2 = open (tmp_file_name, O_WRONLY | O_CREAT | O_TRUNC );
	if (fd2 == -1) {
			camel_exception_setv (ex, 
					     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					     "could not create the temporary mbox copy file\n"
					      "\t%s\n"
					      "Full error is : %s\n",
					      tmp_file_name,
					      strerror (errno));
			return next_uid;
		}
	
	next_free_uid = next_uid;
	for (cur_msg = 0; cur_msg < summary_information->len; cur_msg++) {
		
		cur_msg_info = (CamelMboxParserMessageInfo *)(summary_information->data) + cur_msg;
		end_of_last_message = cur_msg_info->message_position + cur_msg_info->size;

		if (cur_msg_info->uid == 0) {
			
			bytes_to_copy = cur_msg_info->message_position 
				+ cur_msg_info->end_of_headers_offset
				- cur_pos;

			cur_pos = cur_msg_info->message_position 
				+ cur_msg_info->end_of_headers_offset;
			
			copy_file_chunk (fd1, fd2, bytes_to_copy, ex);
			if (camel_exception_get_id (ex)) {
				close (fd1);
				close (fd2);
				goto end;
			}
			
			cur_msg_info->uid = next_free_uid;
			cur_msg_info->status = 0;
			camel_mbox_xev_write_header_content (xev_header + 12, next_free_uid, 0);
			next_free_uid++;
			write (fd2, xev_header, 19);
			cur_offset += 19;
			cur_msg_info->size += 19;
			cur_msg_info->x_evolution_offset = cur_msg_info->end_of_headers_offset;
			cur_msg_info->x_evolution = g_strdup_printf ("%.6s", xev_header + 12);
			cur_msg_info->end_of_headers_offset += 19;
		} 
		cur_msg_info->message_position += cur_offset;
	}
	
	
	bytes_to_copy = end_of_last_message - cur_pos;
		copy_file_chunk (fd1, fd2, bytes_to_copy, ex);


	/* close the original file as well as the 
	   newly created one */
	close (fd1);
	close (fd2);
	


	/* replace the mbox file with the temporary
	   file we just created */ 

	/* first rename the old mbox file to a temporary file */
	rename_result = rename (mbox_file_name, tmp_file_name_secure);
	if (rename_result == -1) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
				     "could not rename the mbox file to a temporary file");
		goto end;
	}
	
	/* then rename the newly created mbox file to the name 
	   of the original one */
	rename_result = rename (tmp_file_name, mbox_file_name);
	if (rename_result == -1) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
				     "could not rename the X-Evolution fed file to the mbox file");
		goto end;
	}

	/* finally, remove the old renamed mbox file */
	unlink_result = unlink (tmp_file_name_secure);
	if (unlink_result == -1) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
				     "could not remove the saved original mbox file");
		goto end;
	}


 end: /* free everything and return */
	
	g_free (tmp_file_name);
	g_free (tmp_file_name_secure);
	return next_free_uid;
}






GArray *
parsed_information_to_mbox_summary (GArray *parsed_information)
{
	guint cur_msg;
	CamelMboxParserMessageInfo *cur_msg_info;
	GArray *mbox_summary;
	CamelMboxSummaryInformation *cur_sum_info;

	mbox_summary = g_array_new (FALSE, FALSE, sizeof (CamelMboxSummaryInformation));
	mbox_summary =  g_array_set_size (mbox_summary, parsed_information->len);

	for (cur_msg = 0; cur_msg < parsed_information->len; cur_msg++) {
		
		cur_msg_info = (CamelMboxParserMessageInfo *)(parsed_information->data) + cur_msg;
		cur_sum_info = (CamelMboxSummaryInformation *)(mbox_summary->data) + cur_msg;

		cur_sum_info->position = cur_msg_info->message_position;

		cur_sum_info->x_evolution_offset = cur_msg_info->x_evolution_offset;

		cur_sum_info->uid = cur_msg_info->uid;

		cur_sum_info->status = cur_msg_info->status;

		cur_sum_info->subject = cur_msg_info->subject;
		cur_msg_info->subject = NULL;

		cur_sum_info->sender =  cur_msg_info->from;
		cur_msg_info->from = NULL;
	
		cur_sum_info->to =  cur_msg_info->to;
		cur_msg_info->to = NULL;		
		
	}
	
	return mbox_summary;
}
