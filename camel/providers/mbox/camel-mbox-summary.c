/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : Bertrand Guiheneuf <bertrand@helixcode.com> 
 *
 * Copyright (C) 1999 Helix Code .

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

#include "camel-log.h"
#include "camel-mbox-folder.h"
#include "camel-mbox-summary.h"
#include "md5-utils.h"


#include <sys/stat.h> 
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


void 
camel_mbox_save_summary (CamelMboxSummary *summary, const gchar *filename, CamelException *ex)
{
	CamelMboxSummaryInformation *msg_info;
	guint cur_msg;
	guint field_lgth;
	gint fd;

	CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::save_summary entering \n");

	fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd == -1) {
			camel_exception_setv (ex, 
					     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					     "could not create the mbox summary file\n"
					      "\t%s\n"
					      "Full error is : %s\n",
					      filename,
					      strerror (errno));
			return;
		}
	
	/* compute and write the mbox file md5 signature */
	//md5_get_digest_from_file (filename, summary->md5_digest);

	/* write the number of messages  + the md5 signatures */
	write (fd, summary, sizeof (guint) +  sizeof (guchar) * 16);
	       
	
	printf ("%d %d\n", summary->nb_message, summary->message_info->len);
	for (cur_msg=0; cur_msg < summary->nb_message; cur_msg++) {

		msg_info = (CamelMboxSummaryInformation *)(summary->message_info->data) + cur_msg;

		/* write message position  + x-evolution offset
		 + uid + status */
		write (fd, (gchar *)msg_info, 
		       sizeof (guint32) + sizeof (guint) + 
		       sizeof (guint32) + sizeof (guchar));
		
		//printf ("IN iewr subject = %s\n", msg_info->subject);
		/* write subject */
		field_lgth = msg_info->subject ? strlen (msg_info->subject) : 0;
		write (fd, &field_lgth, sizeof (guint));
		if (field_lgth)
			write (fd, msg_info->subject, field_lgth);

		/* write sender */
		field_lgth = msg_info->sender ? strlen (msg_info->sender) : 0;
		write (fd, &field_lgth, sizeof (gint));
		if (field_lgth)
			write (fd, msg_info->sender, field_lgth);

		/* write to */
		field_lgth = msg_info->to ? strlen (msg_info->to) : 0;
		write (fd, &field_lgth, sizeof (gint));
		if (field_lgth)
			write (fd, msg_info->to, field_lgth);

		/* write date */
		field_lgth = msg_info->date ? strlen (msg_info->date) : 0;
		write (fd, &field_lgth, sizeof (guint));
		if (field_lgth)
			write (fd, msg_info->date, field_lgth);


	}
			
	close (fd);

	CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::save_summary leaving \n");
}





CamelMboxSummary *
camel_mbox_load_summary (const gchar *filename, CamelException *ex)
{
	CamelMboxSummaryInformation *msg_info;
	guint cur_msg;
	guint field_lgth;
	gint fd;
	CamelMboxSummary *summary;


	CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::save_summary entering \n");

	fd = open (filename, O_RDONLY);
	if (fd == -1) {
			camel_exception_setv (ex, 
					     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					     "could not open the mbox summary file\n"
					      "\t%s\n"
					      "Full error is : %s\n",
					      filename,
					      strerror (errno));
			return NULL;
		}
	summary = g_new0 (CamelMboxSummary, 1);

	/* read the message number as well as the md5 signature */
	read (fd, summary, sizeof (guint) + sizeof (guchar) * 16);

	summary->message_info = g_array_new (FALSE, FALSE, sizeof (CamelMboxSummaryInformation));
	summary->message_info =  g_array_set_size (summary->message_info, summary->nb_message);

	
	for (cur_msg=0; cur_msg < summary->nb_message; cur_msg++)  {
		
		msg_info = (CamelMboxSummaryInformation *)(summary->message_info->data) + cur_msg;
		
		/* read message position  + x-evolution offset
		 + uid + status */
		read (fd, (gchar *)msg_info, 
		       sizeof (guint32) + sizeof (guint) + 
		       sizeof (guint32) + sizeof (guchar));
		

		/* read the subject */
		read (fd, &field_lgth, sizeof (gint));
		if (field_lgth > 0) {			
			msg_info->subject = g_new0 (gchar, field_lgth + 1);
			read (fd, msg_info->subject, field_lgth);
		} else 
			msg_info->subject = NULL;
		
		/* read the sender */
		read (fd, &field_lgth, sizeof (gint));
		if (field_lgth > 0) {			
			msg_info->sender = g_new0 (gchar, field_lgth + 1);
			read (fd, msg_info->sender, field_lgth);
		} else 
			msg_info->sender = NULL;
		
		/* read the "to" field */
		read (fd, &field_lgth, sizeof (gint));
		if (field_lgth > 0) {			
			msg_info->to = g_new0 (gchar, field_lgth + 1);
			read (fd, msg_info->to, field_lgth);
		} else 
			msg_info->to = NULL;
		
		/* read the "date" field */
		read (fd, &field_lgth, sizeof (gint));
		if (field_lgth > 0) {			
			msg_info->date = g_new0 (gchar, field_lgth + 1);
			read (fd, msg_info->date, field_lgth);
		} else 
			msg_info->date = NULL;
		

		
		
	}		
	
	close (fd);
	return summary;
}












