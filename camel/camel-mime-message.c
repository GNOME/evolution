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
#include "camel-log.h"
#include "gmime-utils.h"

typedef enum {
	HEADER_UNKNOWN,
	HEADER_FROM,
	HEADER_REPLY_TO,
	HEADER_SUBJECT,
	HEADER_TO,
	HEADER_CC,
	HEADER_BCC
} CamelHeaderType;

static GHashTable *header_name_table;



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

static void _set_flag (CamelMimeMessage *mime_message, GString *flag, gboolean value);
static gboolean _get_flag (CamelMimeMessage *mime_message, GString *flag);

static void _set_message_number (CamelMimeMessage *mime_message, guint number);
static guint _get_message_number (CamelMimeMessage *mime_message);

static void _write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static gboolean _parse_header_pair (CamelMimePart *mime_part, GString *header_name, GString *header_value);

/* Returns the class for a CamelMimeMessage */
#define CMM_CLASS(so) CAMEL_MIME_MESSAGE_CLASS (GTK_OBJECT(so)->klass)
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (GTK_OBJECT(so)->klass)


static void
_init_header_name_table()
{
	header_name_table = g_hash_table_new (g_string_hash, g_string_equal_for_hash);
	g_hash_table_insert (header_name_table, g_string_new ("From"), (gpointer)HEADER_FROM);
	g_hash_table_insert (header_name_table, g_string_new ("Reply-To"), (gpointer)HEADER_REPLY_TO);
	g_hash_table_insert (header_name_table, g_string_new ("Subject"), (gpointer)HEADER_SUBJECT);
	g_hash_table_insert (header_name_table, g_string_new ("To"), (gpointer)HEADER_TO);
	g_hash_table_insert (header_name_table, g_string_new ("Cc"), (gpointer)HEADER_CC);
	g_hash_table_insert (header_name_table, g_string_new ("Bcc"), (gpointer)HEADER_BCC);

}

static void
camel_mime_message_class_init (CamelMimeMessageClass *camel_mime_message_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (camel_mime_message_class);
	CamelMimePartClass *camel_mime_part_class = CAMEL_MIME_PART_CLASS (camel_mime_message_class);

	parent_class = gtk_type_class (camel_mime_part_get_type ());
	_init_header_name_table();

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
	camel_mime_message_class->set_flag = _set_flag;
	camel_mime_message_class->get_flag = _get_flag;
	camel_mime_message_class->set_message_number = _set_message_number;
	camel_mime_message_class->get_message_number = _get_message_number;
	
	/* virtual method overload */
	camel_data_wrapper_class->write_to_stream = _write_to_stream;
	camel_mime_part_class->parse_header_pair = _parse_header_pair;

}




static void
camel_mime_message_init (gpointer   object,  gpointer   klass)
{
	CamelMimeMessage *camel_mime_message = CAMEL_MIME_MESSAGE (object);

	camel_mime_message->recipients =  g_hash_table_new(g_string_hash, g_string_equal_for_hash);
	camel_mime_message->flags = g_hash_table_new(g_string_hash, g_string_equal_for_hash);
	
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



CamelMimeMessage *
camel_mime_message_new_with_session (CamelSession *session) 
{
	CamelMimeMessage *mime_message;
	mime_message = gtk_type_new (CAMEL_MIME_MESSAGE_TYPE);
	mime_message->session = session;
	return mime_message;
}




/* two utils func */

static void
_set_field (CamelMimeMessage *mime_message, GString *name, GString *value, GString **variable)
{
	if (variable) {
		if (*variable) g_string_free (*variable, FALSE);
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
camel_mime_message_set_received_date (CamelMimeMessage *mime_message, GString *received_date)
{
	 CMM_CLASS (mime_message)->set_received_date (mime_message, received_date);
}


static GString *
_get_received_date (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, received_date_str, mime_message->received_date);
}

GString *
camel_mime_message_get_received_date (CamelMimeMessage *mime_message)
{
	 return CMM_CLASS (mime_message)->get_received_date (mime_message);
}






static GString *
_get_sent_date (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, sent_date_str, mime_message->sent_date);
}

GString *
camel_mime_message_get_sent_date (CamelMimeMessage *mime_message)
{
	 return CMM_CLASS (mime_message)->get_sent_date (mime_message);
}





static void
_set_reply_to (CamelMimeMessage *mime_message, GString *reply_to)
{
	_set_field (mime_message, reply_to_str, reply_to, &(mime_message->reply_to));
}

void
camel_mime_message_set_reply_to (CamelMimeMessage *mime_message, GString *reply_to)
{
	 CMM_CLASS (mime_message)->set_reply_to (mime_message, reply_to);
}


static GString *
_get_reply_to (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, reply_to_str, mime_message->reply_to);
}

GString *
camel_mime_message_get_reply_to (CamelMimeMessage *mime_message)
{
	 return CMM_CLASS (mime_message)->get_reply_to (mime_message);
}




static void
_set_subject (CamelMimeMessage *mime_message, GString *subject)
{
	_set_field (mime_message, subject_str, subject, &(mime_message->subject));
}

void
camel_mime_message_set_subject (CamelMimeMessage *mime_message, GString *subject)
{
	 CMM_CLASS (mime_message)->set_subject (mime_message, subject);
}


static GString *
_get_subject (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, subject_str, mime_message->subject);
}

GString *
camel_mime_message_get_subject (CamelMimeMessage *mime_message)
{
	 return CMM_CLASS (mime_message)->get_subject (mime_message);
}




static void
_set_from (CamelMimeMessage *mime_message, GString *from)
{
	_set_field (mime_message, from_str, from, &(mime_message->from));
}

void
camel_mime_message_set_from (CamelMimeMessage *mime_message, GString *from)
{
	 CMM_CLASS (mime_message)->set_from (mime_message, from);
}


static GString *
_get_from (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, from_str, mime_message->from);
}

GString *
camel_mime_message_get_from (CamelMimeMessage *mime_message)
{
	 return CMM_CLASS (mime_message)->get_from (mime_message);
}






static void
_add_recipient (CamelMimeMessage *mime_message, GString *recipient_type, GString *recipient) 
{
	/* be careful, recipient_type and recipient may be freed within this func */
	GList *recipients_list;
	GList *existent_list;

	/* see if there is already a list for this recipient type */
	existent_list = (GList *)g_hash_table_lookup (mime_message->recipients, recipient_type);

	/* if the recipient is already in this list, do nothing */
	if ( existent_list && g_list_find_custom (existent_list, (gpointer)recipient, g_string_equal_for_glist) ) {
		g_string_free (recipient_type, FALSE);
		g_string_free (recipient, FALSE);
		return;
	}
	/* append the new recipient to the recipient list
	   if the existent_list is NULL, then a new GList is
	   automagically created */	
	recipients_list = g_list_append (existent_list, (gpointer)recipient);

	if (!existent_list) /* if there was no recipient of this type create the section */
		g_hash_table_insert (mime_message->recipients, recipient_type, recipients_list);
	else 
		g_string_free (recipient_type, FALSE);
}


/**
 * add_recipient:
 * @mime_message: 
 * @recipient_type: 
 * @recipient: 
 * 
 * Have to write the doc. IMPORTANT : @recipient_type and 
 * @recipient may be freed within this func
 **/
void
camel_mime_message_add_recipient (CamelMimeMessage *mime_message, GString *recipient_type, GString *recipient) 
{
	 CMM_CLASS (mime_message)->add_recipient (mime_message, recipient_type, recipient);
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
	
	/* look for the recipient to remove */
	old_element = g_list_find_custom (recipients_list, recipient, g_string_equal_for_hash);
	if (old_element) {
		/* if recipient exists, remove it */
		new_recipients_list =  g_list_remove_link (recipients_list, old_element);

		/* if glist head has changed, fix up hash table */
		if (new_recipients_list != recipients_list)
			g_hash_table_insert (mime_message->recipients, old_recipient_type, new_recipients_list);

		g_string_free( (GString *)(old_element->data), FALSE);
		g_list_free_1(old_element);
	}
}


void
camel_mime_message_remove_recipient (CamelMimeMessage *mime_message, GString *recipient_type, GString *recipient) 
{
	 CMM_CLASS (mime_message)->remove_recipient (mime_message, recipient_type, recipient);
}




static GList *
_get_recipients (CamelMimeMessage *mime_message, GString *recipient_type)
{
	return (GList *)g_hash_table_lookup (mime_message->recipients, recipient_type);
}

GList *
camel_mime_message_get_recipients (CamelMimeMessage *mime_message, GString *recipient_type)
{
	return CMM_CLASS (mime_message)->get_recipients (mime_message, recipient_type);
}




static void
_set_flag (CamelMimeMessage *mime_message, GString *flag, gboolean value)
{
	GString old_flags;
	gboolean *ptr_value;
	if (! g_hash_table_lookup_extended (mime_message->flags, 
					    flag, 
					    (gpointer)&(old_flags),
					    (gpointer)&(ptr_value)) ) {
		
		ptr_value = g_new (gboolean, 1);
		g_hash_table_insert (mime_message->flags, flag, ptr_value);
	} else {
		g_string_free (flag, FALSE);
	}
	*ptr_value = value;
		
}

void
camel_mime_message_set_flag (CamelMimeMessage *mime_message, GString *flag, gboolean value)
{
	CMM_CLASS (mime_message)->set_flag (mime_message, flag, value);
}



static gboolean 
_get_flag (CamelMimeMessage *mime_message, GString *flag)
{
	gboolean *value;
	value = (gboolean *)g_hash_table_lookup (mime_message->flags, flag);
	return ( (value) && (*value));
}

gboolean 
camel_mime_message_get_flag (CamelMimeMessage *mime_message, GString *flag)
{
	return CMM_CLASS (mime_message)->get_flag (mime_message, flag);
}




static void 
_set_message_number (CamelMimeMessage *mime_message, guint number)
{
	mime_message->message_number = number;
}

static guint 
_get_message_number (CamelMimeMessage *mime_message)
{
	return mime_message->message_number;
}



guint
camel_mime_message_get_message_number (CamelMimeMessage *mime_message)
{
	return CMM_CLASS (mime_message)->get_message_number (mime_message);
}




#ifdef WHPT
#warning : WHPT is already defined !!!!!!
#endif
#define WHPT gmime_write_header_pair_to_stream

static void
_write_one_recipient_to_stream (gpointer key, gpointer value, gpointer user_data)
{
	GString *recipient_type = (GString *)key;
	GList *recipients = (GList *)value;
	//	GString *current;
	CamelStream *stream = (CamelStream *)user_data;
	if ( (recipient_type) && (recipient_type->str) )
	     write_header_with_glist_to_stream (stream, recipient_type->str, recipients, ", ");
}

static void
_write_recipients_to_stream (CamelMimeMessage *mime_message, CamelStream *stream)
{
	g_hash_table_foreach (mime_message->recipients, _write_one_recipient_to_stream, (gpointer)stream);
}

static void
_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMimeMessage *mm = CAMEL_MIME_MESSAGE (data_wrapper);
	CAMEL_LOG (FULL_DEBUG, "CamelMimeMessage::write_to_stream\n");
	CAMEL_LOG (FULL_DEBUG, "Writing \"From\"\n");
	WHPT (stream, "From", mm->from);
	CAMEL_LOG (FULL_DEBUG, "Writing \"Reply-To\"\n");
	WHPT (stream, "Reply-To", mm->reply_to);
	CAMEL_LOG (FULL_DEBUG, "Writing recipients\n");
	_write_recipients_to_stream (mm, stream);
	CAMEL_LOG (FULL_DEBUG, "Writing \"Date\"\n");
	WHPT (stream, "Date", mm->received_date);
	CAMEL_LOG (FULL_DEBUG, "Writing \"Subject\"\n");
	WHPT (stream, "Subject", mm->subject);
	CAMEL_DATA_WRAPPER_CLASS (parent_class)->write_to_stream (data_wrapper, stream);
	
}

/*******************************/
/* mime message header parsing */

static void
_set_recipient_list_from_string (CamelMimeMessage *message, GString *recipient_type, GString *recipients_string)
{
	GList *recipients_list;
	CAMEL_LOG (FULL_DEBUG,"CamelMimeMessage::_set_recipient_list_from_string parsing ##%s##\n", recipients_string->str);
	recipients_list = g_string_split (recipients_string, ',', "\t ", TRIM_STRIP_TRAILING | TRIM_STRIP_LEADING);
	g_hash_table_insert (message->recipients, recipient_type, recipients_list);

}

static gboolean
_parse_header_pair (CamelMimePart *mime_part, GString *header_name, GString *header_value)
{
	CamelHeaderType header_type;
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (mime_part);
	gboolean header_handled = FALSE;
	
	
	header_type = (CamelHeaderType) g_hash_table_lookup (header_name_table, header_name);
	switch (header_type) {
	
	case HEADER_FROM:
		CAMEL_LOG (FULL_DEBUG,
			  "CamelMimeMessage::parse_header_pair found HEADER_FROM : %s\n",
			  header_value->str );

		camel_mime_message_set_from (message, header_value);
		header_handled = TRUE;
		break;

	case HEADER_REPLY_TO:
		CAMEL_LOG (FULL_DEBUG,
			  "CamelMimeMessage::parse_header_pair found HEADER_REPLY_YO : %s\n",
			  header_value->str );

		camel_mime_message_set_reply_to (message, header_value);
		header_handled = TRUE;
		break;

	case HEADER_SUBJECT:
		CAMEL_LOG (FULL_DEBUG,
			  "CamelMimeMessage::parse_header_pair found HEADER_SUBJECT : %s\n",
			  header_value->str );

		camel_mime_message_set_subject (message, header_value);
		header_handled = TRUE;
		break;

	case HEADER_TO:
		CAMEL_LOG (FULL_DEBUG,
			  "CamelMimeMessage::parse_header_pair found HEADER_TO : %s\n",
			  header_value->str );

		_set_recipient_list_from_string (message, g_string_new ("To"), header_value);
		g_string_free (header_value, TRUE);
		header_handled = TRUE;
		break;
	
	case HEADER_CC:
		CAMEL_LOG (FULL_DEBUG,
			  "CamelMimeMessage::parse_header_pair found HEADER_CC : %s\n",
			  header_value->str );
		
		_set_recipient_list_from_string (message, g_string_new ("Cc"), header_value);
		g_string_free (header_value, TRUE);
		header_handled = TRUE;
		break;
	
	case HEADER_BCC:
		CAMEL_LOG (FULL_DEBUG,
			  "CamelMimeMessage::parse_header_pair found HEADER_BCC : %s\n",
			  header_value->str );
		
		_set_recipient_list_from_string (message, g_string_new ("Bcc"), header_value);
		g_string_free (header_value, TRUE);
		header_handled = TRUE;
		break;
	

	}
	if (header_handled) {
		g_string_free (header_name, TRUE);
		return TRUE;
	} else
		return parent_class->parse_header_pair (mime_part, header_name, header_value);
	
}
