/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMimeMessage.c : class for a mime_message */


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
#include <config.h>
#include "camel-mime-message.h"
#include <stdio.h>
#include "gmime-content-field.h"
#include "string-utils.h"
#include "camel-log.h"
#include "gmime-utils.h"
#include "hash-table-utils.h"

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

static gchar *received_date_str;
static gchar *sent_date_str;
static gchar *reply_to_str;
static gchar *subject_str;
static gchar *from_str;

static void _set_received_date (CamelMimeMessage *mime_message, gchar *received_date);
static const gchar *_get_received_date (CamelMimeMessage *mime_message);
static const gchar *_get_sent_date (CamelMimeMessage *mime_message);
static void _set_reply_to (CamelMimeMessage *mime_message, gchar *reply_to);
static const gchar *_get_reply_to (CamelMimeMessage *mime_message);
static void _set_subject (CamelMimeMessage *mime_message, gchar *subject);
static const gchar *_get_subject (CamelMimeMessage *mime_message);
static void _set_from (CamelMimeMessage *mime_message, gchar *from);
static const gchar *_get_from (CamelMimeMessage *mime_message);
static void _add_recipient (CamelMimeMessage *mime_message, gchar *recipient_type, gchar *recipient); 
static void _remove_recipient (CamelMimeMessage *mime_message, const gchar *recipient_type, const gchar *recipient);
static const GList *_get_recipients (CamelMimeMessage *mime_message, const gchar *recipient_type);
static void _set_flag (CamelMimeMessage *mime_message, const gchar *flag, gboolean value);
static gboolean _get_flag (CamelMimeMessage *mime_message, const gchar *flag);
static GList *_get_flag_list (CamelMimeMessage *mime_message);
static void _set_message_number (CamelMimeMessage *mime_message, guint number);
static guint _get_message_number (CamelMimeMessage *mime_message);
static void _write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static gboolean _parse_header_pair (CamelMimePart *mime_part, gchar *header_name, gchar *header_value);
static void _finalize (GtkObject *object);

/* Returns the class for a CamelMimeMessage */
#define CMM_CLASS(so) CAMEL_MIME_MESSAGE_CLASS (GTK_OBJECT(so)->klass)
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (GTK_OBJECT(so)->klass)


static void
_init_header_name_table()
{
	header_name_table = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (header_name_table, "From", (gpointer)HEADER_FROM);
	g_hash_table_insert (header_name_table, "Reply-To", (gpointer)HEADER_REPLY_TO);
	g_hash_table_insert (header_name_table, "Subject", (gpointer)HEADER_SUBJECT);
	g_hash_table_insert (header_name_table, "To", (gpointer)HEADER_TO);
	g_hash_table_insert (header_name_table, "Cc", (gpointer)HEADER_CC);
	g_hash_table_insert (header_name_table, "Bcc", (gpointer)HEADER_BCC);
	
}

static void
camel_mime_message_class_init (CamelMimeMessageClass *camel_mime_message_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (camel_mime_message_class);
	CamelMimePartClass *camel_mime_part_class = CAMEL_MIME_PART_CLASS (camel_mime_message_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_mime_message_class);
	
	parent_class = gtk_type_class (camel_mime_part_get_type ());
	_init_header_name_table();
	
	received_date_str = "";
	sent_date_str = "";
	reply_to_str = "Reply-To";
	subject_str = "Subject";
	from_str = "From";
	
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
	camel_mime_message_class->get_flag_list = _get_flag_list;
	camel_mime_message_class->set_message_number = _set_message_number;
	camel_mime_message_class->get_message_number = _get_message_number;
	
	/* virtual method overload */
	camel_data_wrapper_class->write_to_stream = _write_to_stream;
	camel_mime_part_class->parse_header_pair = _parse_header_pair;
	
	gtk_object_class->finalize = _finalize;
}




static void
camel_mime_message_init (gpointer object, gpointer klass)
{
	CamelMimeMessage *camel_mime_message = CAMEL_MIME_MESSAGE (object);
	
	camel_mime_message->recipients =  g_hash_table_new (g_strcase_hash, g_strcase_equal);
	camel_mime_message->flags = g_hash_table_new (g_strcase_hash, g_strcase_equal);
	
	camel_mime_message->received_date = NULL;
	camel_mime_message->sent_date = NULL;
	camel_mime_message->subject = NULL;
	camel_mime_message->reply_to = NULL;
	camel_mime_message->from = NULL;
	camel_mime_message->folder = NULL;
	camel_mime_message->session = NULL;
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


static void           
_finalize (GtkObject *object)
{
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (object);
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelMimeMessage::finalize\n");
	g_free (message->received_date);
	g_free (message->sent_date);
	g_free (message->subject);
	g_free (message->reply_to);
	g_free (message->from);
	
#warning free recipients.
	if (message->folder) gtk_object_unref (GTK_OBJECT (message->folder));
	if (message->session) gtk_object_unref (GTK_OBJECT (message->session));
	
	if (message->flags)
		g_hash_table_foreach (message->flags, g_hash_table_generic_free, NULL);
	
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMimeMessage::finalize\n");
}



CamelMimeMessage *
camel_mime_message_new_with_session (CamelSession *session) 
{
	CamelMimeMessage *mime_message;
	mime_message = gtk_type_new (CAMEL_MIME_MESSAGE_TYPE);
	mime_message->session = session;
	if (session) gtk_object_ref (GTK_OBJECT (session));
	
	return mime_message;
}


/* some utils func */

static void
_set_field (CamelMimeMessage *mime_message, gchar *name, gchar *value, gchar **variable)
{
	if (variable) {
		g_free (*variable);
		*variable = value;
	}
}

/* for future use */
/* for the moment, only @variable is used */
static gchar *
_get_field (CamelMimeMessage *mime_message, gchar *name, gchar *variable)
{
	return variable;
}

static gboolean
_check_not_expunged (CamelMimeMessage *mime_message)
{
	if (mime_message->expunged) {
		CAMEL_LOG_WARNING ("CamelMimeMessage:: An invalid operation has been tempted on an expunged message\n");
	}
	return (!mime_message->expunged);
}

/* * */


static void
_set_received_date (CamelMimeMessage *mime_message, gchar *received_date)
{
	_set_field (mime_message, received_date_str, received_date, &(mime_message->received_date));
}

void
camel_mime_message_set_received_date (CamelMimeMessage *mime_message, gchar *received_date)
{
	g_assert (mime_message);
	g_return_if_fail (_check_not_expunged (mime_message));
	CMM_CLASS (mime_message)->set_received_date (mime_message, received_date);
}


static const gchar *
_get_received_date (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, received_date_str, mime_message->received_date);
}

const gchar *
camel_mime_message_get_received_date (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);
	g_return_val_if_fail (_check_not_expunged (mime_message), NULL);
	return CMM_CLASS (mime_message)->get_received_date (mime_message);
}


static const gchar *
_get_sent_date (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, sent_date_str, mime_message->sent_date);
}

const gchar *
camel_mime_message_get_sent_date (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);
	g_return_val_if_fail (_check_not_expunged (mime_message), NULL);
	return CMM_CLASS (mime_message)->get_sent_date (mime_message);
}


static void
_set_reply_to (CamelMimeMessage *mime_message, gchar *reply_to)
{
	_set_field (mime_message, reply_to_str, reply_to, &(mime_message->reply_to));
}

void
camel_mime_message_set_reply_to (CamelMimeMessage *mime_message, gchar *reply_to)
{
	g_assert (mime_message);
	g_return_if_fail (_check_not_expunged (mime_message));
	CMM_CLASS (mime_message)->set_reply_to (mime_message, reply_to);
}


static const gchar *
_get_reply_to (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, reply_to_str, mime_message->reply_to);
}

const gchar *
camel_mime_message_get_reply_to (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);
	g_return_val_if_fail (_check_not_expunged (mime_message), NULL);
	return CMM_CLASS (mime_message)->get_reply_to (mime_message);
}




static void
_set_subject (CamelMimeMessage *mime_message, gchar *subject)
{
	_set_field (mime_message, subject_str, subject, &(mime_message->subject));
}

void
camel_mime_message_set_subject (CamelMimeMessage *mime_message, gchar *subject)
{
	g_assert (mime_message);
	g_return_if_fail (_check_not_expunged (mime_message));
	CMM_CLASS (mime_message)->set_subject (mime_message, subject);
}


static const gchar *
_get_subject (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, subject_str, mime_message->subject);
}

const gchar *
camel_mime_message_get_subject (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);
	g_return_val_if_fail (_check_not_expunged (mime_message), NULL);
	return CMM_CLASS (mime_message)->get_subject (mime_message);
}




static void
_set_from (CamelMimeMessage *mime_message, gchar *from)
{
	_set_field (mime_message, from_str, from, &(mime_message->from));
}

void
camel_mime_message_set_from (CamelMimeMessage *mime_message, gchar *from)
{
	g_assert (mime_message);
	g_return_if_fail (_check_not_expunged (mime_message));
	CMM_CLASS (mime_message)->set_from (mime_message, from);
}


static const gchar *
_get_from (CamelMimeMessage *mime_message)
{
	return _get_field (mime_message, from_str, mime_message->from);
}

const gchar *
camel_mime_message_get_from (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);
	g_return_val_if_fail (_check_not_expunged (mime_message), NULL);
	return CMM_CLASS (mime_message)->get_from (mime_message);
}






static void
_add_recipient (CamelMimeMessage *mime_message, gchar *recipient_type, gchar *recipient) 
{
	/* be careful, recipient_type and recipient may be freed within this func */
	GList *recipients_list;
	GList *existent_list;
	
	/* see if there is already a list for this recipient type */
	existent_list = (GList *)g_hash_table_lookup (mime_message->recipients, recipient_type);
	
	/* if the recipient is already in this list, do nothing */
	if ( existent_list && g_list_find_custom (existent_list, (gpointer)recipient, string_equal_for_glist) ) {
		g_free (recipient_type);
		g_free (recipient);
		return;
	}
	/* append the new recipient to the recipient list
	   if the existent_list is NULL, then a new GList is
	   automagically created */	
	recipients_list = g_list_append (existent_list, (gpointer)recipient);
	
	if (!existent_list) /* if there was no recipient of this type create the section */
		g_hash_table_insert (mime_message->recipients, recipient_type, recipients_list);
	else 
		g_free (recipient_type);
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
camel_mime_message_add_recipient (CamelMimeMessage *mime_message, gchar *recipient_type, gchar *recipient) 
{
	g_assert (mime_message);
	g_return_if_fail (_check_not_expunged (mime_message));
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
_remove_recipient (CamelMimeMessage *mime_message, const gchar *recipient_type, const gchar *recipient) 
{
	GList *recipients_list;
	GList *new_recipients_list;
	GList *old_element;
	gchar *old_recipient_type;
	
	/* if the recipient type section does not exist, do nothing */
	if (! g_hash_table_lookup_extended (mime_message->recipients, 
					    recipient_type, 
					    (gpointer)&(old_recipient_type),
					    (gpointer)&(recipients_list)) 
	    ) return;
	
	/* look for the recipient to remove */
	/* g_list_find_custom does use "const" for recipient, is it a mistake ? */
	old_element = g_list_find_custom (recipients_list, recipient, g_str_equal);
	if (old_element) {
		/* if recipient exists, remove it */
		new_recipients_list =  g_list_remove_link (recipients_list, old_element);
		
		/* if glist head has changed, fix up hash table */
		if (new_recipients_list != recipients_list)
			g_hash_table_insert (mime_message->recipients, old_recipient_type, new_recipients_list);
		
		g_free( (gchar *)(old_element->data));
		g_list_free_1 (old_element);
	}
}


void
camel_mime_message_remove_recipient (CamelMimeMessage *mime_message, const gchar *recipient_type, const gchar *recipient) 
{
	g_assert (mime_message);
	g_return_if_fail (_check_not_expunged (mime_message));
	CMM_CLASS (mime_message)->remove_recipient (mime_message, recipient_type, recipient);
}


static const GList *
_get_recipients (CamelMimeMessage *mime_message, const gchar *recipient_type)
{
	return (GList *)g_hash_table_lookup (mime_message->recipients, recipient_type);
}

const GList *
camel_mime_message_get_recipients (CamelMimeMessage *mime_message, const gchar *recipient_type)
{
	g_assert (mime_message);
	g_return_val_if_fail (_check_not_expunged (mime_message), NULL);
	return CMM_CLASS (mime_message)->get_recipients (mime_message, recipient_type);
}


static void
_set_flag (CamelMimeMessage *mime_message, const gchar *flag, gboolean value)
{
	gchar *old_flags;
	gboolean ptr_value;

	if (! g_hash_table_lookup_extended (mime_message->flags, 
					    flag, 
					    (gpointer)&(old_flags),
					    (gpointer)&(ptr_value)) ) {
		
		g_hash_table_insert (mime_message->flags, g_strdup (flag), GINT_TO_POINTER (value));
	} else 
		g_hash_table_insert (mime_message->flags, old_flags, GINT_TO_POINTER (value));
	
}

void
camel_mime_message_set_flag (CamelMimeMessage *mime_message, const gchar *flag, gboolean value)
{
	g_assert (mime_message);
	g_return_if_fail (_check_not_expunged (mime_message));
	CMM_CLASS (mime_message)->set_flag (mime_message, flag, value);
}



static gboolean 
_get_flag (CamelMimeMessage *mime_message, const gchar *flag)
{
	return GPOINTER_TO_INT (g_hash_table_lookup (mime_message->flags, flag));
}

gboolean 
camel_mime_message_get_flag (CamelMimeMessage *mime_message, const gchar *flag)
{
	g_assert (mime_message);
	g_return_val_if_fail (_check_not_expunged (mime_message), FALSE);
	return CMM_CLASS (mime_message)->get_flag (mime_message, flag);
}



static void
_add_flag_to_list (gpointer key, gpointer value, gpointer user_data)
{
	GList **flag_list = (GList **)user_data;
	gchar *flag_name = (gchar *)key;
	
	if ((flag_name) && (flag_name[0] != '\0'))
		*flag_list = g_list_append (*flag_list, flag_name);
}

static GList *
_get_flag_list (CamelMimeMessage *mime_message)
{
	GList *flag_list = NULL;
	
	if (mime_message->flags)
		g_hash_table_foreach (mime_message->flags, _add_flag_to_list, &flag_list);
	return flag_list;
}


GList *
camel_mime_message_get_flag_list (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);
	g_return_val_if_fail (_check_not_expunged (mime_message), NULL);
	return CMM_CLASS (mime_message)->get_flag_list (mime_message);
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
	gchar *recipient_type = (gchar *)key;
	GList *recipients = (GList *)value;
	//	gchar *current;
	CamelStream *stream = (CamelStream *)user_data;
	if  (recipient_type)
		write_header_with_glist_to_stream (stream, recipient_type, recipients, ", ");
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
	CAMEL_LOG_FULL_DEBUG ( "CamelMimeMessage::write_to_stream\n");
	CAMEL_LOG_FULL_DEBUG ( "CamelMimeMessage:: Writing \"From\"\n");
	WHPT (stream, "From", mm->from);
	CAMEL_LOG_FULL_DEBUG ( "CamelMimeMessage:: Writing \"Reply-To\"\n");
	WHPT (stream, "Reply-To", mm->reply_to);
	CAMEL_LOG_FULL_DEBUG ( "CamelMimeMessage:: Writing recipients\n");
	_write_recipients_to_stream (mm, stream);
	CAMEL_LOG_FULL_DEBUG ( "CamelMimeMessage:: Writing \"Date\"\n");
	WHPT (stream, "Date", mm->received_date);
	CAMEL_LOG_FULL_DEBUG ( "CamelMimeMessage:: Writing \"Subject\"\n");
	WHPT (stream, "Subject", mm->subject);
	CAMEL_DATA_WRAPPER_CLASS (parent_class)->write_to_stream (data_wrapper, stream);
	
}

/*******************************/
/* mime message header parsing */

static void
_set_recipient_list_from_string (CamelMimeMessage *message, gchar *recipient_type, gchar *recipients_string)
{
	GList *recipients_list;
	CAMEL_LOG_FULL_DEBUG ("CamelMimeMessage::_set_recipient_list_from_string parsing ##%s##\n", recipients_string);
	recipients_list = string_split (
					recipients_string, ',', "\t ",
					STRING_TRIM_STRIP_TRAILING | STRING_TRIM_STRIP_LEADING);
	g_hash_table_insert (message->recipients, recipient_type, recipients_list);
	
}

static gboolean
_parse_header_pair (CamelMimePart *mime_part, gchar *header_name, gchar *header_value)
{
	CamelHeaderType header_type;
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (mime_part);
	gboolean header_handled = FALSE;
	
	
	header_type = (CamelHeaderType) g_hash_table_lookup (header_name_table, header_name);
	switch (header_type) {
		
	case HEADER_FROM:
		CAMEL_LOG_FULL_DEBUG (
				      "CamelMimeMessage::parse_header_pair found HEADER_FROM : %s\n",
				      header_value );
		
		camel_mime_message_set_from (message, header_value);
		header_handled = TRUE;
		break;
		
	case HEADER_REPLY_TO:
		CAMEL_LOG_FULL_DEBUG (
				      "CamelMimeMessage::parse_header_pair found HEADER_REPLY_YO : %s\n",
				      header_value );
		
		camel_mime_message_set_reply_to (message, header_value);
		header_handled = TRUE;
		break;
		
	case HEADER_SUBJECT:
		CAMEL_LOG_FULL_DEBUG (
				      "CamelMimeMessage::parse_header_pair found HEADER_SUBJECT : %s\n",
				      header_value );
		
		camel_mime_message_set_subject (message, header_value);
		header_handled = TRUE;
		break;
		
	case HEADER_TO:
		CAMEL_LOG_FULL_DEBUG (
				      "CamelMimeMessage::parse_header_pair found HEADER_TO : %s\n",
				      header_value );
		
		_set_recipient_list_from_string (message, "To", header_value);
		g_free (header_value);
		header_handled = TRUE;
		break;
		
	case HEADER_CC:
		CAMEL_LOG_FULL_DEBUG (
				      "CamelMimeMessage::parse_header_pair found HEADER_CC : %s\n",
				      header_value );
		
		_set_recipient_list_from_string (message, "Cc", header_value);
		g_free (header_value);
		header_handled = TRUE;
		break;
		
	case HEADER_BCC:
		CAMEL_LOG_FULL_DEBUG (
				      "CamelMimeMessage::parse_header_pair found HEADER_BCC : %s\n",
				      header_value );
		
		_set_recipient_list_from_string (message, "Bcc", header_value);
		g_free (header_value);
		header_handled = TRUE;
		break;
		
		
	}
	if (header_handled) {
		g_free (header_name);
		return TRUE;
	} else
		return parent_class->parse_header_pair (mime_part, header_name, header_value);
	
	
}

