/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mbox-parser.c : mbox folder parser */

/* 
 *
 * Author : Bertrand Guiheneuf <bertrand@helixcode.com> 
 *
 * Copyright (C) 1999 Helix Code .
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

#include <glib.h>
#include "camel-log.h"
#include "camel-exception.h"


typedef struct {

	glong message_position;
	glong size;
	
	gchar *from;
	gchar *to;
	gchar *date;
	gchar *subject;
	gchar *priority;
	gchar *references;
	gchar *body_summary;
	gshort end_of_headers_offset;

	gchar *x_evolution;
	gshort x_evolution_offset;
	guint32 uid;
	guchar status;

} CamelMboxParserMessageInfo;


typedef void camel_mbox_preparser_status_callback (double percentage_done, gpointer user_data);


GArray *
camel_mbox_parse_file (int fd, 
		       const gchar *message_delimiter,
		       glong start_position,
		       guint32 *file_size,
		       guint32 *next_uid,
		       gboolean get_message_summary,
		       camel_mbox_preparser_status_callback *status_callback,
		       double status_interval,
		       gpointer user_data);

