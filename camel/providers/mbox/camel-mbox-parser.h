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

#include <glib.h>
#include "camel-log.h"
#include "camel-exception.h"


typedef struct {

	guint message_position;
	gchar *from;
	gchar *date;
	gchar *subject;
	gchar *status;
	gchar *priority;
	gchar *references;
	gchar *body_summary;

} CamelMboxParserMessageInfo;


GArray * camel_mbox_find_message_positions (int fd, 
					    const gchar *message_delimiter,
					    gint first_position, 
					    CamelException *ex);
