/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMimeMessage.c : class for a mime_message */

/* 
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
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
#include "gmime-utils.h"
#include "hash-table-utils.h"

#define d(x)

typedef enum {
	HEADER_UNKNOWN,
	HEADER_FROM,
	HEADER_REPLY_TO,
	HEADER_SUBJECT,
	HEADER_TO,
	HEADER_CC,
	HEADER_BCC,
	HEADER_DATE
} CamelHeaderType;

static GHashTable *header_name_table;

static CamelMimePartClass *parent_class=NULL;

/* WTF are these for?? */
static gchar *received_date_str;
static gchar *sent_date_str;
static gchar *reply_to_str;
static gchar *subject_str;
static gchar *from_str;

static void _add_recipient (CamelMimeMessage *mime_message, const gchar *recipient_type, const gchar *recipient); 
static void _remove_recipient (CamelMimeMessage *mime_message, const gchar *recipient_type, const gchar *recipient);
static const GList *_get_recipients (CamelMimeMessage *mime_message, const gchar *recipient_type);
static void _set_flag (CamelMimeMessage *mime_message, const gchar *flag, gboolean value);
static gboolean _get_flag (CamelMimeMessage *mime_message, const gchar *flag);
static GList *_get_flag_list (CamelMimeMessage *mime_message);
static void _set_message_number (CamelMimeMessage *mime_message, guint number);
static guint _get_message_number (CamelMimeMessage *mime_message);
static void _write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static void _finalize (GtkObject *object);
static void add_header (CamelMedium *medium, const char *header_name, const void *header_value);
static void set_header (CamelMedium *medium, const char *header_name, const void *header_value);
static void remove_header (CamelMedium *medium, const char *header_name);
static void construct_from_parser (CamelMimePart *, CamelMimeParser *);

/* Returns the class for a CamelMimeMessage */
#define CMM_CLASS(so) CAMEL_MIME_MESSAGE_CLASS (GTK_OBJECT(so)->klass)
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (GTK_OBJECT(so)->klass)
#define CMD_CLASS(so) CAMEL_MEDIUM_CLASS (GTK_OBJECT(so)->klass)


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
	g_hash_table_insert (header_name_table, "Date", (gpointer)HEADER_DATE);
}

static void
camel_mime_message_class_init (CamelMimeMessageClass *camel_mime_message_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (camel_mime_message_class);
	CamelMimePartClass *camel_mime_part_class = CAMEL_MIME_PART_CLASS (camel_mime_message_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_mime_message_class);
	CamelMediumClass *camel_medium_class = CAMEL_MEDIUM_CLASS (camel_mime_message_class);
	
	parent_class = gtk_type_class (camel_mime_part_get_type ());
	_init_header_name_table();
	
	received_date_str = "";
	sent_date_str = "";
	reply_to_str = "Reply-To";
	subject_str = "Subject";
	from_str = "From";
	
	/* virtual method definition */
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

	camel_medium_class->add_header = add_header;
	camel_medium_class->set_header = set_header;
	camel_medium_class->remove_header = remove_header;
	
	camel_mime_part_class->construct_from_parser = construct_from_parser;

	gtk_object_class->finalize = _finalize;
}




static void
camel_mime_message_init (gpointer object, gpointer klass)
{
	CamelMimeMessage *camel_mime_message = CAMEL_MIME_MESSAGE (object);
	
	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (object),
					  "message/rfc822");

	camel_mime_message->recipients =  camel_recipient_table_new ();
	camel_mime_message->flags =
		g_hash_table_new (g_strcase_hash, g_strcase_equal);

	camel_mime_message->subject = NULL;
	camel_mime_message->reply_to = NULL;
	camel_mime_message->from = NULL;
	camel_mime_message->folder = NULL;
	camel_mime_message->date = CAMEL_MESSAGE_DATE_CURRENT;
	camel_mime_message->date_offset = 0;
	camel_mime_message->date_str = NULL;
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
	
	g_free (message->date_str);
	g_free (message->subject);
	g_free (message->reply_to);
	g_free (message->from);
	
	if (message->recipients) camel_recipient_table_unref (message->recipients);
	if (message->folder) gtk_object_unref (GTK_OBJECT (message->folder));
	
	if (message->flags)
		g_hash_table_foreach (message->flags, g_hash_table_generic_free, NULL);
	g_hash_table_destroy(message->flags);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}



CamelMimeMessage *
camel_mime_message_new (void) 
{
	CamelMimeMessage *mime_message;
	mime_message = gtk_type_new (CAMEL_MIME_MESSAGE_TYPE);
	
	return mime_message;
}


/* **** Date: */

void
camel_mime_message_set_date(CamelMimeMessage *message,  time_t date, int offset)
{
	g_assert(message);
	if (date == CAMEL_MESSAGE_DATE_CURRENT) {
		struct tm *local;
		int tz;

		date = time(0);
		local = localtime(&date);
		offset = 0;
#if defined(HAVE_TIMEZONE)
		tz = timezone;
#elif defined(HAVE_TM_GMTOFF)
		tz = local->tm_gmtoff;
#endif
		offset = ((tz/60/60) * 100) + (tz/60 % 60);
	}
	message->date = date;
	message->date_offset = offset;
	g_free(message->date_str);
	message->date_str = header_format_date(date, offset);

	CMD_CLASS(parent_class)->set_header((CamelMedium *)message, "Date", message->date_str);
}

void
camel_mime_message_get_date(CamelMimeMessage *message,  time_t *date, int *offset)
{
	if (message->date == CAMEL_MESSAGE_DATE_CURRENT)
		camel_mime_message_set_date(message, CAMEL_MESSAGE_DATE_CURRENT, 0);
	if (date)
		*date = message->date;
	if (offset)
		*offset = message->date_offset;
}

char *
camel_mime_message_get_date_string(CamelMimeMessage *message)
{
	if (message->date == CAMEL_MESSAGE_DATE_CURRENT)
		camel_mime_message_set_date(message, CAMEL_MESSAGE_DATE_CURRENT, 0);
	return message->date_str;
}

/* **** Reply-To: */

void
camel_mime_message_set_reply_to (CamelMimeMessage *mime_message, const gchar *reply_to)
{
	g_assert (mime_message);

	/* FIXME: check format of string, handle it nicer ... */

	g_free(mime_message->reply_to);
	mime_message->reply_to = g_strdup(reply_to);
	CMD_CLASS(parent_class)->set_header((CamelMedium *)mime_message, "Reply-To", reply_to);
}

const gchar *
camel_mime_message_get_reply_to (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);

	return mime_message->reply_to;
}

void
camel_mime_message_set_subject (CamelMimeMessage *mime_message,
				const gchar *subject)
{
	char *text;
	g_assert (mime_message);

	g_free(mime_message->subject);
	mime_message->subject = g_strdup(subject);
	text = header_encode_string(subject);
	CMD_CLASS(parent_class)->set_header((CamelMedium *)mime_message, "Subject", text);
	g_free(text);
}

const gchar *
camel_mime_message_get_subject (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);

	return mime_message->subject;
}

/* *** From: */
void
camel_mime_message_set_from (CamelMimeMessage *mime_message, const gchar *from)
{
	g_assert (mime_message);

	g_free(mime_message->from);
	mime_message->from = g_strdup(from);
	CMD_CLASS(parent_class)->set_header((CamelMedium *)mime_message, "From", from);
}

const gchar *
camel_mime_message_get_from (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);

	return mime_message->from;
}

/*  ****  */

static void
_add_recipient (CamelMimeMessage *mime_message, 
		const gchar *recipient_type, 
		const gchar *recipient) 
{
	camel_recipient_table_add (mime_message->recipients, recipient_type, recipient); 
}



void
camel_mime_message_add_recipient (CamelMimeMessage *mime_message, 
				  const gchar *recipient_type, 
				  const gchar *recipient) 
{
	g_assert (mime_message);
	g_return_if_fail (!mime_message->expunged);
	CMM_CLASS (mime_message)->add_recipient (mime_message, recipient_type, recipient);
}


static void
_remove_recipient (CamelMimeMessage *mime_message, 
		   const gchar *recipient_type, 
		   const gchar *recipient) 
{
	camel_recipient_table_remove (mime_message->recipients, recipient_type, recipient); 
}


void
camel_mime_message_remove_recipient (CamelMimeMessage *mime_message, 
				     const gchar *recipient_type, 
				     const gchar *recipient) 
{
	g_assert (mime_message);
	g_return_if_fail (!mime_message->expunged);
	CMM_CLASS (mime_message)->remove_recipient (mime_message, recipient_type, recipient);
}


static const GList *
_get_recipients (CamelMimeMessage *mime_message, 
		 const gchar *recipient_type)
{
	return camel_recipient_table_get (mime_message->recipients, recipient_type);
}


const GList *
camel_mime_message_get_recipients (CamelMimeMessage *mime_message, 
				   const gchar *recipient_type)
{
	g_assert (mime_message);
	g_return_val_if_fail (!mime_message->expunged, NULL);
	return CMM_CLASS (mime_message)->get_recipients (mime_message, recipient_type);
}



/*  ****  */



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
	g_return_if_fail (!mime_message->expunged);
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
	g_return_val_if_fail (!mime_message->expunged, FALSE);
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
	g_return_val_if_fail (!mime_message->expunged, NULL);
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

/* mime_message */
static void
construct_from_parser(CamelMimePart *dw, CamelMimeParser *mp)
{
	char *buf;
	int len;
	int state;

	d(printf("constructing mime-message\n"));

	d(printf("mime_message::construct_from_parser()\n"));

	/* let the mime-part construct the guts ... */
	((CamelMimePartClass *)parent_class)->construct_from_parser(dw, mp);

	/* ... then clean up the follow-on state */
	state = camel_mime_parser_step(mp, &buf, &len);
	if (!(state == HSCAN_MESSAGE_END || state == HSCAN_EOF)) {
		g_warning("Bad parser state: Expecing MESSAGE_END or EOF, got: %d", camel_mime_parser_state(mp));
		camel_mime_parser_unstep(mp);
	}

	d(printf("mime_message::construct_from_parser() leaving\n"));
}

#ifdef WHPT
#warning : WHPT is already defined !!!!!!
#endif
#define WHPT gmime_write_header_pair_to_stream

static void
_write_one_recipient_to_stream (gchar *recipient_type,
				GList *recipient_list,
				gpointer user_data)
{
	
	CamelStream *stream = (CamelStream *)user_data;
	if  (recipient_type)
		gmime_write_header_with_glist_to_stream (stream, recipient_type, recipient_list, ", ");
}

static void
_write_recipients_to_stream (CamelMimeMessage *mime_message, CamelStream *stream)
{
	camel_recipient_foreach_recipient_type (mime_message->recipients, 
						_write_one_recipient_to_stream, 
						(gpointer)stream);
}

static void
_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMimeMessage *mm = CAMEL_MIME_MESSAGE (data_wrapper);

	/* force mandatory headers ... */
	if (mm->from == NULL) {
		g_warning("No from set for message");
		camel_mime_message_set_from(mm, "");
	}
	if (mm->date_str == NULL) {
		g_warning("Application did not set date, using 'now'");
		camel_mime_message_set_date(mm, CAMEL_MESSAGE_DATE_CURRENT, 0);
	}
	if (mm->subject == NULL) {
		g_warning("Application did not set subject, creating one");
		camel_mime_message_set_subject(mm, "No Subject");
	}

	camel_medium_set_header((CamelMedium *)mm, "Mime-Version", "1.0");

#if 1
#warning need to store receipients lists to headers
	/* FIXME: remove this snot ... */
	_write_recipients_to_stream (mm, stream);
#endif

	CAMEL_DATA_WRAPPER_CLASS (parent_class)->write_to_stream (data_wrapper, stream);
}

/*******************************/
/* mime message header parsing */

/* FIXME: This is totally totally broken */
static void
_set_recipient_list_from_string (CamelMimeMessage *message, const char *recipient_type, const char *recipients_string)
{
	GList *recipients_list;

#warning need to parse receipient lists properly - <feddy>BROKEN!!!</feddy>
	recipients_list = string_split (
					recipients_string, ',', "\t ",
					STRING_TRIM_STRIP_TRAILING | STRING_TRIM_STRIP_LEADING);
	
	camel_recipient_table_add_list (message->recipients, recipient_type, recipients_list);
	
}

/* FIXME: check format of fields. */
static gboolean
process_header(CamelMedium *medium, const char *header_name, const char *header_value)
{
	CamelHeaderType header_type;
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (medium);

	header_type = (CamelHeaderType) g_hash_table_lookup (header_name_table, header_name);
	switch (header_type) {
	case HEADER_FROM:
		g_free(message->from); /* FIXME: parse the from line into something useful */
		message->from = g_strdup(header_value);
		break;
	case HEADER_REPLY_TO:
		g_free(message->reply_to); /* FIXME: parse the from line into something useful */
		message->reply_to = g_strdup(header_value);
		break;
	case HEADER_SUBJECT:
		g_free(message->subject);
		message->subject = header_decode_string(header_value);
		break;
	case HEADER_TO:
		if (header_value)
			_set_recipient_list_from_string (message, "To", header_value);
		else
			camel_recipient_table_remove_type (message->recipients, "To");
		break;
	case HEADER_CC:
		if (header_value)
			_set_recipient_list_from_string (message, "Cc", header_value);
		else
			camel_recipient_table_remove_type (message->recipients, "Cc");
		break;
	case HEADER_BCC:
		if (header_value)
			_set_recipient_list_from_string (message, "Bcc", header_value);
		else
			camel_recipient_table_remove_type (message->recipients, "Bcc");
		break;
	case HEADER_DATE:
		g_free(message->date_str);
		message->date_str = g_strdup(header_value);
		if (header_value) {
			message->date = header_decode_date(header_value, &message->date_offset);
		} else {
			message->date = CAMEL_MESSAGE_DATE_CURRENT;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

static void
set_header(CamelMedium *medium, const char *header_name, const void *header_value)
{
	process_header(medium, header_name, header_value);
	parent_class->parent_class.set_header (medium, header_name, header_value);
}

static void
add_header(CamelMedium *medium, const char *header_name, const void *header_value)
{
	/* if we process it, then it must be forced unique as well ... */
	if (process_header(medium, header_name, header_value))
		parent_class->parent_class.set_header (medium, header_name, header_value);
	else
		parent_class->parent_class.add_header (medium, header_name, header_value);
}

static void
remove_header(CamelMedium *medium, const char *header_name)
{
	process_header(medium, header_name, NULL);
	parent_class->parent_class.remove_header (medium, header_name);
}

