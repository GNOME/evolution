/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mbox-parser.c : mbox folder parser */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <bertrand@helixcode.com> .
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
#include "camel-mbox-parser.h"
#include "camel-log.h"
#include "camel-exception.h"

#include <sys/types.h>
#include <unistd.h>




GList * 
camel_mbox_find_message_positions (int fd, gint first_position, CamelException *ex)
{
#define MBOX_PARSER_BUF_SIZE 1000

	off_t seek_res;
	GList *message_positions = NULL;
	char buffer[MBOX_PARSER_BUF_SIZE]; 
	ssize_t buf_nb_read;


	/* set the initial position */
	seek_res = lseek (fd, first_position, SEEK_SET);
	if (seek_res == (off_t)-1) goto io_error;

	/* populate the buffer and initialize the search proc */
	buf_nb_read = read (fd, buffer, MBOX_PARSER_BUF_SIZE);
	
	while (buf_nb_read>0) {
		current_pos = 0;
		
		
		

		/* read the next chunk of data in the folder file */
		buf_nb_read = read (fd, buffer, MBOX_PARSER_BUF_SIZE);	
	}
	
	
	
		

	
		/* io exception handling */
	io_error : 

		switch errno { 
		case EACCES :
			
			camel_exception_setv (ex, 
					      CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					      "Unable to list the directory. Full Error text is : %s ", 
					      strerror (errno));
			break;
			
		case ENOENT :
		case ENOTDIR :
			camel_exception_setv (ex, 
					      CAMEL_EXCEPTION_FOLDER_INVALID_PATH,
					      "Invalid mbox folder path. Full Error text is : %s ", 
					      strerror (errno));
			break;
			
		default :
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_SYSTEM,
					     "Unable to delete the mbox folder.");
			
		}

}
