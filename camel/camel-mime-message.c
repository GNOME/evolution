/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMimeMessage.c : class for a mime_message */


/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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

#include "camel-mime-message.h"
#include <stdio.h>
#include "gmime-content-field.h"



static CamelMimePartClass *parent_class=NULL;

static GString *received_date_str;
static GString *sent_date_str;
static GString *reply_to_str;
static GString *subject_str;
static GString *from_str;

static void _set_received_date (CamelMimeMessage *mime_message, GString *received_date);
static GString *_get_received_date (CamelMimeMessage *mime_message);
static GString *_get_sent_date (CamelMimeMessage *mime_message);
static void _set_reply_to (CamelMimeMessage *mime_message, GString *reply_to);
static GString *_get_reply_to (CamelMimeMessage *mime_message);
static void _set_subject (CamelMimeMessage *mime_message, GString *subject);
static GString *_get_subject (CamelMimeMessage *mime_message);
static void _set_from (CamelMimeMessage *mime_message, GString *from);
static GString *_get_from (CamelMimeMessage *mime_message);

/* Returns the class for a CamelMimeMessage */
#define CMM_CLASS(so) CAMEL_MIME_MESSAGE_CLASS (GTK_OBJECT(so)->klass)



static void
camel_mime_message_class_init (CamelMimeMessageClass *camel_mime_message_class)
{
	parent_class = gtk_type_class (camel_mime_part_get_type ());

	received_date_str = g_string_new("");
	sent_date_str = g_string_new("");
	reply_to_str = g_string_new("Reply-To");
	subject_str = g_string_new("Subject");
	from_str = g_string_new("From");
	
	/* virtual method definition */
	camel_mime_message_class->set_received_date = _set_received_date;
	camel_mime_message_class->get_received_date = _get_received_date;
	camel_mime_message_class->get_sent_date = _get_sent_date;
	camel_mime_message_class->set_reply_to = _set_reply_to;
	camel_mime_message_class->get_reply_to = _get_reply_to;
	camel_mime_message_class->set_subject = _set_subject;
	camel_mime_message_class->get_subject = _get_subject;
	camel_mime_message_class->set_from = _set_from;
	camel_mime_message_class->get_from = _get_from;
	
	/* virtual method overload */
}







GtkType
camel_mime_message_get_type (void)
{
	static GtkType camel_mime_message_type = 0;
	
	if (!camel_mime_message_type)	{
		GtkTypeInfo camel_mime_message_info =	
		{
			"CamelMimeMessage",
			sizeof (CamelMimeMessage),
			sizeof (CamelMimeMessageClass),
			(GtkClassInitFunc) camel_mime_message_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_mime_message_type = gtk_type_unique (camel_mime_part_get_type (), &camel_mime_message_info);
	}
	
	return camel_mime_message_type;
}




/* two utils func */

static void
_set_field (CamelMimeMessage *mime_message, GString *name, GString *value, GString **variable)
{
	if (variable) {
		if (*variable) G_string_free (*variable, TRUE);
		*variable = value;
	}
}

static GString *
_get_field (CamelMimeMessage *mime_message, GString *name, GString *variable)
{
	return variable;
}





static void
_set_received_date (CamelMimeMessage *mime_message, GString *received_date)
{
	_set_field (mime_message, received_date_str, received_date, &(mime_message->received_date));
}

void
set_received_date (CamelMimeMessage *mime_message, GString *received_date)
{
	 CMM_CLASS (mime_message)->set_received_date (mime_message, received_date);
}


static GString *
_get_received_date (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, received_date_str, mime_message->received_date);
}

GString *
get_received_date (CamelMimeMessage *mime_message)
{
	 return CMM_CLASS (mime_message)->get_received_date (mime_message);
}






static GString *
_get_sent_date (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, sent_date_str, mime_message->sent_date);
}

GString *
get_sent_date (CamelMimeMessage *mime_message)
{
	 return CMM_CLASS (mime_message)->get_sent_date (mime_message);
}





static void
_set_reply_to (CamelMimeMessage *mime_message, GString *reply_to)
{
	_set_field (mime_message, reply_to_str, reply_to, &(mime_message->reply_to));
}

void
set_reply_to (CamelMimeMessage *mime_message, GString *reply_to)
{
	 CMM_CLASS (mime_message)->set_reply_to (mime_message, reply_to);
}


static GString *
_get_reply_to (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, reply_to_str, mime_message->reply_to);
}

GString *
get_reply_to (CamelMimeMessage *mime_message)
{
	 return CMM_CLASS (mime_message)->get_reply_to (mime_message);
}




static void
_set_subject (CamelMimeMessage *mime_message, GString *subject)
{
	_set_field (mime_message, subject_str, subject, &(mime_message->subject));
}

void
set_subject (CamelMimeMessage *mime_message, GString *subject)
{
	 CMM_CLASS (mime_message)->set_subject (mime_message, subject);
}


static GString *
_get_subject (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, subject_str, mime_message->subject);
}

GString *
get_subject (CamelMimeMessage *mime_message)
{
	 return CMM_CLASS (mime_message)->get_subject (mime_message);
}




static void
_set_from (CamelMimeMessage *mime_message, GString *from)
{
	_set_field (mime_message, from_str, from, &(mime_message->from));
}

void
set_from (CamelMimeMessage *mime_message, GString *from)
{
	 CMM_CLASS (mime_message)->set_from (mime_message, from);
}


static GString *
_get_from (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, from_str, mime_message->from);
}

GString *
get_from (CamelMimeMessage *mime_message)
{
	 return CMM_CLASS (mime_message)->get_from (mime_message);
}

