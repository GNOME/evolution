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
#include "gstring-util.h"



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

static void _add_recipient (CamelMimeMessage *mime_message, GString *recipient_type, GString *recipient); 
static void _remove_recipient (CamelMimeMessage *mime_message, GString *recipient_type, GString *recipient);
static GList *_get_recipients (CamelMimeMessage *mime_message, GString *recipient_type);

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
	camel_mime_message_class->add_recipient = _add_recipient; 
	camel_mime_message_class->remove_recipient = _remove_recipient;
	camel_mime_message_class->get_recipients = _get_recipients;
	
	/* virtual method overload */
}




static void
camel_mime_message_init (gpointer   object,  gpointer   klass)
{
	CamelMimeMessage *camel_mime_message = CAMEL_MIME_MESSAGE (object);

	camel_mime_message->recipients =  g_hash_table_new(g_string_hash, g_string_equal_for_hash);

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
			(GtkObjectInitFunc) camel_mime_message_init,
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






static void
_add_recipient (CamelMimeMessage *mime_message, GString *recipient_type, GString *recipient) 
{
	GList *recipients_list;
	GList *existent_list;

	/* see if there is already a list for this recipient type */
	existent_list = (GList *)g_hash_table_lookup (mime_message->recipients, recipient_type);

	/* if the recipient is already in this list, do nothing */
	if ( existent_list && g_list_find_custom (existent_list, (gpointer)recipient, g_string_equal_for_hash) )
		return;

	/* append the new recipient to the recipient list
	   if the existent_list is NULL, then a new GList is
	   automagically created */	
	recipients_list = g_list_append (existent_list, (gpointer)recipient);

	if (!existent_list) /* if there was no recipient of this type create the section */
		g_hash_table_insert (mime_message->recipients, recipient_type, recipients_list);
}


/**
 * _remove_recipient: remove a recipient from the list of recipients
 * @mime_message: the message
 * @recipient_type: recipient type from which the recipient should be removed
 * @recipient: recipient to remove
 * 
 * Be careful, recipient and recipient_type are not freed. 
 * calling programns must free them themselves. They can free
 * them just after remove_recipient returns.
 **/
static void
_remove_recipient (CamelMimeMessage *mime_message, GString *recipient_type, GString *recipient) 
{
	GList *recipients_list;
	GList *new_recipients_list;
	GList *old_element;
	GString *old_recipient_type;
	
	/* if the recipient type section does not exist, do nothing */
	if (! g_hash_table_lookup_extended (mime_message->recipients, 
					    recipient_type, 
					    (gpointer)&(old_recipient_type),
					    (gpointer)&(recipients_list)) 
	    ) return;
	
	/* look for the recipient to remoce */
	old_element = g_list_find_custom (recipients_list, recipient, g_string_equal_for_hash);
	if (old_element) {
		/* if recipient exists, remove it */
		new_recipients_list =  g_list_remove_link (recipients_list, old_element);

		/* if glist head has changed, fix up hash table */
		if (new_recipients_list != recipients_list)
			g_hash_table_insert (mime_message->recipients, old_recipient_type, new_recipients_list);

		g_string_free( (GString *)(old_element->data), TRUE);
		g_list_free_1(old_element);
	}
}





static GList *
_get_recipients (CamelMimeMessage *mime_message, GString *recipient_type)
{
	return (GList *)g_hash_table_lookup (mime_message->recipients, recipient_type);
}
