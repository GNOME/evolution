/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-mime-message.c : class for a mime_message */

/* 
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@helixcode.com>
 *          Jeffrey Stedfast <fejj@helixcode.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include "camel-mime-message.h"
#include "camel-multipart.h"
#include "camel-stream-mem.h"
#include "string-utils.h"
#include "hash-table-utils.h"

#include "camel-stream-filter.h"
#include "camel-stream-null.h"
#include "camel-mime-filter-charset.h"
#include "camel-mime-filter-bestenc.h"

#define d(x)

/* these 2 below should be kept in sync */
typedef enum {
	HEADER_UNKNOWN,
	HEADER_FROM,
	HEADER_REPLY_TO,
	HEADER_SUBJECT,
	HEADER_TO,
	HEADER_CC,
	HEADER_BCC,
	HEADER_DATE,
	HEADER_MESSAGE_ID
} CamelHeaderType;

static char *header_names[] = {
	/* dont include HEADER_UNKNOWN string */
	"From", "Reply-To", "Subject", "To", "Cc", "Bcc", "Date", "Message-Id", NULL
};

static GHashTable *header_name_table;

static CamelMimePartClass *parent_class=NULL;

static char *recipient_names[] = {
	"To", "Cc", "Bcc", NULL
};

static int write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static void add_header (CamelMedium *medium, const char *header_name, const void *header_value);
static void set_header (CamelMedium *medium, const char *header_name, const void *header_value);
static void remove_header (CamelMedium *medium, const char *header_name);
static int construct_from_parser (CamelMimePart *, CamelMimeParser *);
static void unref_recipient (gpointer key, gpointer value, gpointer user_data);

/* Returns the class for a CamelMimeMessage */
#define CMM_CLASS(so) CAMEL_MIME_MESSAGE_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CMD_CLASS(so) CAMEL_MEDIUM_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static void
camel_mime_message_class_init (CamelMimeMessageClass *camel_mime_message_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (camel_mime_message_class);
	CamelMimePartClass *camel_mime_part_class = CAMEL_MIME_PART_CLASS (camel_mime_message_class);
	CamelMediumClass *camel_medium_class = CAMEL_MEDIUM_CLASS (camel_mime_message_class);
	int i;
	
	parent_class = CAMEL_MIME_PART_CLASS(camel_type_get_global_classfuncs (camel_mime_part_get_type ()));

	header_name_table = g_hash_table_new (g_strcase_hash, g_strcase_equal);
	for (i=0;header_names[i];i++)
		g_hash_table_insert (header_name_table, header_names[i], (gpointer)i+1);

	/* virtual method overload */
	camel_data_wrapper_class->write_to_stream = write_to_stream;

	camel_medium_class->add_header = add_header;
	camel_medium_class->set_header = set_header;
	camel_medium_class->remove_header = remove_header;
	
	camel_mime_part_class->construct_from_parser = construct_from_parser;
}


static void
camel_mime_message_init (gpointer object, gpointer klass)
{
	CamelMimeMessage *mime_message = (CamelMimeMessage *)object;
	int i;
	
	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (object), "message/rfc822");

	mime_message->recipients =  g_hash_table_new(g_strcase_hash, g_strcase_equal);
	for (i=0;recipient_names[i];i++) {
		g_hash_table_insert(mime_message->recipients, recipient_names[i], camel_internet_address_new());
	}

	mime_message->subject = NULL;
	mime_message->reply_to = NULL;
	mime_message->from = NULL;
	mime_message->date = CAMEL_MESSAGE_DATE_CURRENT;
	mime_message->date_offset = 0;
	mime_message->date_received = CAMEL_MESSAGE_DATE_CURRENT;
	mime_message->date_received_offset = 0;
	mime_message->message_id = NULL;
}

static void           
camel_mime_message_finalize (CamelObject *object)
{
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (object);
	
	g_free(message->subject);

	g_free(message->message_id);
	
	if (message->reply_to)
		camel_object_unref((CamelObject *)message->reply_to);

	if (message->from)
		camel_object_unref((CamelObject *)message->from);

	g_hash_table_foreach(message->recipients, unref_recipient, NULL);
	g_hash_table_destroy(message->recipients);
}


CamelType
camel_mime_message_get_type (void)
{
	static CamelType camel_mime_message_type = CAMEL_INVALID_TYPE;
	
	if (camel_mime_message_type == CAMEL_INVALID_TYPE)	{
		camel_mime_message_type = camel_type_register (camel_mime_part_get_type(), "CamelMimeMessage",
							       sizeof (CamelMimeMessage),
							       sizeof (CamelMimeMessageClass),
							       (CamelObjectClassInitFunc) camel_mime_message_class_init,
							       NULL,
							       (CamelObjectInitFunc) camel_mime_message_init,
							       (CamelObjectFinalizeFunc) camel_mime_message_finalize);
	}
	
	return camel_mime_message_type;
}

static void unref_recipient (gpointer key, gpointer value, gpointer user_data)
{
	camel_object_unref (CAMEL_OBJECT (value));
}

CamelMimeMessage *
camel_mime_message_new (void) 
{
	CamelMimeMessage *mime_message;
	mime_message = CAMEL_MIME_MESSAGE(camel_object_new (CAMEL_MIME_MESSAGE_TYPE));
	
	return mime_message;
}

/* **** Date: */

void
camel_mime_message_set_date(CamelMimeMessage *message,  time_t date, int offset)
{
	char *datestr;

	g_assert(message);
	if (date == CAMEL_MESSAGE_DATE_CURRENT) {
		struct tm *local;
		int tz;

		date = time(0);
		local = localtime(&date);
#if defined(HAVE_TIMEZONE)
		tz = timezone;
#elif defined(HAVE_TM_GMTOFF)
		tz = local->tm_gmtoff;
#endif
		offset = -(((tz/60/60) * 100) + (tz/60 % 60));
		if (local->tm_isdst>0)
			offset += 100;
	}
	message->date = date;
	message->date_offset = offset;

	datestr = header_format_date(date, offset);
	CAMEL_MEDIUM_CLASS(parent_class)->set_header((CamelMedium *)message, "Date", datestr);
	g_free(datestr);
}

time_t
camel_mime_message_get_date(CamelMimeMessage *msg, int *offset)
{
	if (offset)
		*offset = msg->date_offset;

	return msg->date;
}

time_t
camel_mime_message_get_date_received(CamelMimeMessage *msg, int *offset)
{
	if (msg->date_received == CAMEL_MESSAGE_DATE_CURRENT) {
		const char *received;

		received = camel_medium_get_header((CamelMedium *)msg, "received");
		if (received)
			received = strrchr(received, ';');
		if (received)
			msg->date_received = header_decode_date(received + 1, &msg->date_received_offset);
	}

	if (offset)
		*offset = msg->date_received_offset;

	return msg->date_received;
}

/* **** Message-Id: */

void
camel_mime_message_set_message_id (CamelMimeMessage *mime_message, const char *message_id)
{
	char *id;
	
	g_assert (mime_message);
	
	g_free (mime_message->message_id);
	
	if (message_id) {
		id = g_strstrip (g_strdup (message_id));
	} else {
		id = header_msgid_generate ();
	}
	
	mime_message->message_id = id;
	id = g_strdup_printf ("<%s>", mime_message->message_id);
	CAMEL_MEDIUM_CLASS (parent_class)->set_header (CAMEL_MEDIUM (mime_message), "Message-Id", id);
	g_free (id);
}

const char *
camel_mime_message_get_message_id (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);
	
	return mime_message->message_id;
}

/* **** Reply-To: */

void
camel_mime_message_set_reply_to (CamelMimeMessage *msg, const CamelInternetAddress *reply_to)
{
	char *addr;
	
	g_assert(msg);

	if (msg->reply_to) {
		camel_object_unref((CamelObject *)msg->reply_to);
		msg->reply_to = NULL;
	}

	if (reply_to == NULL) {
		CAMEL_MEDIUM_CLASS(parent_class)->remove_header(CAMEL_MEDIUM(msg), "Reply-To");
		return;
	}

	msg->reply_to = (CamelInternetAddress *)camel_address_new_clone((CamelAddress *)reply_to);
	addr = camel_address_encode((CamelAddress *)msg->reply_to);
	CAMEL_MEDIUM_CLASS(parent_class)->set_header(CAMEL_MEDIUM(msg), "Reply-To", addr);
	g_free(addr);
}

const CamelInternetAddress *
camel_mime_message_get_reply_to(CamelMimeMessage *mime_message)
{
	g_assert (mime_message);

	/* TODO: ref for threading? */

	return mime_message->reply_to;
}

/* **** Subject: */

void
camel_mime_message_set_subject(CamelMimeMessage *mime_message, const char *subject)
{
	char *text;
	
	g_assert(mime_message);
	
	g_free(mime_message->subject);
	mime_message->subject = g_strstrip (g_strdup (subject));
	text = header_encode_string((unsigned char *)mime_message->subject);
	CAMEL_MEDIUM_CLASS(parent_class)->set_header(CAMEL_MEDIUM (mime_message), "Subject", text);
	g_free (text);
}

const char *
camel_mime_message_get_subject(CamelMimeMessage *mime_message)
{
	g_assert(mime_message);

	return mime_message->subject;
}

/* *** From: */

/* Thought: Since get_from/set_from are so rarely called, it is probably not useful
   to cache the from (and reply_to) addresses as InternetAddresses internally, we
   could just get it from the headers and reprocess every time. */
void
camel_mime_message_set_from(CamelMimeMessage *msg, const CamelInternetAddress *from)
{
	char *addr;
	
	g_assert(msg);

	if (msg->from) {
		camel_object_unref((CamelObject *)msg->from);
		msg->from = NULL;
	}

	if (from == NULL || camel_address_length((CamelAddress *)from) == 0) {
		CAMEL_MEDIUM_CLASS(parent_class)->remove_header(CAMEL_MEDIUM(msg), "From");
		return;
	}

	msg->from = (CamelInternetAddress *)camel_address_new_clone((CamelAddress *)from);
	addr = camel_address_encode((CamelAddress *)msg->from);
	CAMEL_MEDIUM_CLASS (parent_class)->set_header(CAMEL_MEDIUM(msg), "From", addr);
	g_free(addr);
}

const CamelInternetAddress *
camel_mime_message_get_from(CamelMimeMessage *mime_message)
{
	g_assert (mime_message);

	/* TODO: we should really ref this for multi-threading to work */

	return mime_message->from;
}

/*  **** To: Cc: Bcc: */

void
camel_mime_message_set_recipients(CamelMimeMessage *mime_message, const char *type, const CamelInternetAddress *r)
{
	char *text;
	CamelInternetAddress *addr;

	g_assert(mime_message);

	addr = g_hash_table_lookup(mime_message->recipients, type);
	if (addr == NULL) {
		g_warning("trying to set a non-valid receipient type: %s", type);
		return;
	}

	if (r == NULL || camel_address_length((CamelAddress *)r) == 0) {
		camel_address_remove((CamelAddress *)addr, -1);
		CAMEL_MEDIUM_CLASS(parent_class)->remove_header(CAMEL_MEDIUM(mime_message), type);
		return;
	}

	/* note this does copy, and not append (cat) */
	camel_address_copy((CamelAddress *)addr, (const CamelAddress *)r);

	/* and sync our headers */
	text = camel_address_encode(CAMEL_ADDRESS(addr));
	CAMEL_MEDIUM_CLASS(parent_class)->set_header(CAMEL_MEDIUM(mime_message), type, text);
	g_free(text);
}

void
camel_mime_message_set_identity(CamelMimeMessage *mime_message, const char *identity)
{
	g_assert (mime_message);
	camel_medium_add_header (CAMEL_MEDIUM (mime_message), "X-Evolution-Identity", identity);
}

const CamelInternetAddress *
camel_mime_message_get_recipients(CamelMimeMessage *mime_message, const char *type)
{
	g_assert(mime_message);
	
	return g_hash_table_lookup(mime_message->recipients, type);
}

/* mime_message */
static int
construct_from_parser(CamelMimePart *dw, CamelMimeParser *mp)
{
	char *buf;
	int len;
	int state;
	int ret;

	d(printf("constructing mime-message\n"));

	d(printf("mime_message::construct_from_parser()\n"));

	/* let the mime-part construct the guts ... */
	ret = ((CamelMimePartClass *)parent_class)->construct_from_parser(dw, mp);

	if (ret == -1)
		return -1;

	/* ... then clean up the follow-on state */
	state = camel_mime_parser_step(mp, &buf, &len);
	switch (state) {
	case HSCAN_EOF: case HSCAN_FROM_END: /* these doesn't belong to us */
		camel_mime_parser_unstep(mp);
	case HSCAN_MESSAGE_END:
		break;
	default:
		g_error("Bad parser state: Expecing MESSAGE_END or EOF or EOM, got: %d", camel_mime_parser_state(mp));
		camel_mime_parser_unstep(mp);
		return -1;
	}

	d(printf("mime_message::construct_from_parser() leaving\n"));
#ifndef NO_WARNINGS
#warning "return a real error code"
#endif
	return 0;
}

static int
write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMimeMessage *mm = CAMEL_MIME_MESSAGE (data_wrapper);

	/* force mandatory headers ... */
	if (mm->from == NULL) {
		/* FIXME: should we just abort?  Should we make one up? */
		g_warning("No from set for message");
		camel_medium_set_header((CamelMedium *)mm, "From", "");
	}
	if (!camel_medium_get_header((CamelMedium *)mm, "Date"))
		camel_mime_message_set_date(mm, CAMEL_MESSAGE_DATE_CURRENT, 0);
	
	if (mm->subject == NULL)
		camel_mime_message_set_subject(mm, "No Subject");
	
	if (mm->message_id == NULL)
		camel_mime_message_set_message_id (mm, NULL);
	
	/* FIXME: "To" header needs to be set explicitly as well ... */

	if (!camel_medium_get_header ((CamelMedium *)mm, "Mime-Version"))
		camel_medium_set_header((CamelMedium *)mm, "Mime-Version", "1.0");

	return CAMEL_DATA_WRAPPER_CLASS (parent_class)->write_to_stream (data_wrapper, stream);
}

/* FIXME: check format of fields. */
static gboolean
process_header (CamelMedium *medium, const char *header_name, const char *header_value)
{
	CamelHeaderType header_type;
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (medium);
	CamelInternetAddress *addr;

	header_type = (CamelHeaderType)g_hash_table_lookup(header_name_table, header_name);
	switch (header_type) {
	case HEADER_FROM:
		if (message->from)
			camel_object_unref((CamelObject *)message->from);
		message->from = camel_internet_address_new();
		camel_address_decode((CamelAddress *)message->from, header_value);
		break;
	case HEADER_REPLY_TO:
		if (message->reply_to)
			camel_object_unref((CamelObject *)message->reply_to);
		message->reply_to = camel_internet_address_new();
		camel_address_decode((CamelAddress *)message->reply_to, header_value);
		break;
	case HEADER_SUBJECT:
		g_free(message->subject);
		message->subject = g_strstrip(header_decode_string(header_value));
		break;
	case HEADER_TO:
	case HEADER_CC:
	case HEADER_BCC:
		addr = g_hash_table_lookup (message->recipients, header_name);
		if (header_value)
			camel_address_decode(CAMEL_ADDRESS (addr), header_value);
		else
			camel_address_remove(CAMEL_ADDRESS (addr), -1);
		break;
	case HEADER_DATE:
		if (header_value) {
			message->date = header_decode_date(header_value, &message->date_offset);
		} else {
			message->date = CAMEL_MESSAGE_DATE_CURRENT;
			message->date_offset = 0;
		}
		break;
	case HEADER_MESSAGE_ID:
		g_free (message->message_id);
		if (header_value)
			message->message_id = header_msgid_decode (header_value);
		else
			message->message_id = NULL;
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

typedef gboolean (*CamelPartFunc)(CamelMimeMessage *, CamelMimePart *, void *data);

static gboolean
message_foreach_part_rec(CamelMimeMessage *msg, CamelMimePart *part, CamelPartFunc callback, void *data)
{
	CamelDataWrapper *containee;
	int parts, i;
	int go = TRUE;

	if (callback(msg, part, data) == FALSE)
		return FALSE;

	containee = camel_medium_get_content_object(CAMEL_MEDIUM(part));

	if (containee == NULL)
		return go;

	/* using the object types is more accurate than using the mime/types */
	if (CAMEL_IS_MULTIPART(containee)) {
		parts = camel_multipart_get_number(CAMEL_MULTIPART(containee));
		for (i=0;go && i<parts;i++) {
			CamelMimePart *part = camel_multipart_get_part(CAMEL_MULTIPART(containee), i);

			go = message_foreach_part_rec(msg, part, callback, data);
		}
	} else if (CAMEL_IS_MIME_MESSAGE(containee)) {
		go = message_foreach_part_rec(msg, (CamelMimePart *)containee, callback, data);
	}

	return go;
}

/* dont make this public yet, it might need some more thinking ... */
/* MPZ */
static void
camel_mime_message_foreach_part(CamelMimeMessage *msg, CamelPartFunc callback, void *data)
{
	message_foreach_part_rec(msg, (CamelMimePart *)msg, callback, data);
}

static gboolean
check_8bit(CamelMimeMessage *msg, CamelMimePart *part, void *data)
{
	int *has8bit = data;

	/* check this part, and stop as soon as we are done */
	*has8bit = camel_mime_part_get_encoding(part) == CAMEL_MIME_PART_ENCODING_8BIT;
	return !(*has8bit);
}

gboolean
camel_mime_message_has_8bit_parts(CamelMimeMessage *msg)
{
	int has8bit = FALSE;

	camel_mime_message_foreach_part(msg, check_8bit, &has8bit);

	return has8bit;
}

/* finds the best charset and transfer encoding for a given part */
static CamelMimePartEncodingType
find_best_encoding(CamelMimePart *part, CamelBestencRequired required, CamelBestencEncoding enctype, char **charsetp)
{
	const char *charsetin = NULL;
	char *charset = NULL;
	CamelStream *null;
	CamelStreamFilter *filter;
	CamelMimeFilterCharset *charenc = NULL;
	CamelMimeFilterBestenc *bestenc;
	int idb, idc = -1;
	gboolean istext;
	unsigned int flags, callerflags;
	CamelMimePartEncodingType encoding;
	CamelDataWrapper *content;

	/* we use all these weird stream things so we can do it with streams, and
	   not have to read the whole lot into memory - although i have a feeling
	   it would make things a fair bit simpler to do so ... */

	d(printf("starting to check part\n"));

	content = camel_medium_get_content_object((CamelMedium *)part);
	if (content == NULL) {
		/* charset might not be right here, but it'll get the right stuff
		   if it is ever set */
		*charsetp = NULL;
		return CAMEL_MIME_PART_ENCODING_DEFAULT;
	}

	istext = header_content_type_is(part->content_type, "text", "*");
	if (istext) {
		flags = CAMEL_BESTENC_GET_CHARSET|CAMEL_BESTENC_GET_ENCODING;
	} else {
		flags = CAMEL_BESTENC_GET_ENCODING;
	}

	/* when building the message, any encoded parts are translated already */
	flags |= CAMEL_BESTENC_LF_IS_CRLF;
	/* and get any flags the caller passed in */
	callerflags = (required & CAMEL_BESTENC_NO_FROM);
	flags |= callerflags;

	/* first a null stream, so any filtering is thrown away; we only want the sideeffects */
	null = (CamelStream *)camel_stream_null_new();
	filter = camel_stream_filter_new_with_stream(null);

	/* if we're not looking for the best charset, then use the one we have */
	if (istext && (required & CAMEL_BESTENC_GET_CHARSET) == 0
	    && (charsetin = header_content_type_param(part->content_type, "charset"))) {
		/* if libunicode doesn't support it, we dont really have utf8 anyway, so
		   we dont need a converter */
		charenc = camel_mime_filter_charset_new_convert("UTF-8", charsetin);
		if (charenc != NULL)
			idc = camel_stream_filter_add(filter, (CamelMimeFilter *)charenc);
		charsetin = NULL;
	}

	bestenc = camel_mime_filter_bestenc_new(flags);
	idb = camel_stream_filter_add(filter, (CamelMimeFilter *)bestenc);
	d(printf("writing to checking stream\n"));
	camel_data_wrapper_write_to_stream(content, (CamelStream *)filter);
	camel_stream_filter_remove(filter, idb);
	if (idc != -1) {
		camel_stream_filter_remove(filter, idc);
		camel_object_unref((CamelObject *)charenc);
		charenc = NULL;
	}

	if (istext)
		charsetin = camel_mime_filter_bestenc_get_best_charset(bestenc);

	d(printf("charsetin = %s\n", charsetin));

	/* if we have US-ASCII, or we're not doing text, we dont need to bother with the rest */
	if (charsetin != NULL && (required & CAMEL_BESTENC_GET_CHARSET) != 0) {
		charset = g_strdup(charsetin);

		d(printf("have charset, trying conversion/etc\n"));

		/* now the 'bestenc' can has told us what the best encoding is, we can use that to create
		   a charset conversion filter as well, and then re-add the bestenc to filter the
		   result to find the best encoding to use as well */
		
		charenc = camel_mime_filter_charset_new_convert("UTF-8", charset);

		/* eek, libunicode doesn't undertand this charset anyway, then the 'utf8' we
		   thought we had is really the native format, in which case, we just treat
		   it as binary data (and take the result we have so far) */
		   
		if (charenc != NULL) {

			/* otherwise, try another pass, converting to the real charset */

			camel_mime_filter_reset((CamelMimeFilter *)bestenc);
			camel_mime_filter_bestenc_set_flags(bestenc, CAMEL_BESTENC_GET_ENCODING|CAMEL_BESTENC_LF_IS_CRLF|callerflags);

			camel_stream_filter_add(filter, (CamelMimeFilter *)charenc);
			camel_stream_filter_add(filter, (CamelMimeFilter *)bestenc);

			/* and write it to the new stream */
			camel_data_wrapper_write_to_stream(content, (CamelStream *)filter);

			camel_object_unref((CamelObject *)charenc);
		}
	}
	
	encoding = camel_mime_filter_bestenc_get_best_encoding(bestenc, enctype);

	camel_object_unref((CamelObject *)filter);
	camel_object_unref((CamelObject *)bestenc);
	camel_object_unref((CamelObject *)null);

	d(printf("done, best encoding = %d\n", encoding));

	if (charsetp)
		*charsetp = charset;
	else
		g_free(charset);

	return encoding;
}

struct _enc_data {
	CamelBestencRequired required;
	CamelBestencEncoding enctype;
};

static gboolean
best_encoding(CamelMimeMessage *msg, CamelMimePart *part, void *datap)
{
	struct _enc_data *data = datap;
	char *charset;
	CamelMimePartEncodingType encoding;

	/* we only care about actual content objects */
	if (!CAMEL_IS_MULTIPART(part) && !CAMEL_IS_MIME_MESSAGE(part)) {

		encoding = find_best_encoding(part, data->required, data->enctype, &charset);
		/* we always set the encoding, if we got this far.  GET_CHARSET implies
		   also GET_ENCODING */
		camel_mime_part_set_encoding(part, encoding);

		if ((data->required & CAMEL_BESTENC_GET_CHARSET) != 0) {
			if (header_content_type_is(part->content_type, "text", "*")) {
				char *newct;

				/* FIXME: ick, the part content_type interface needs fixing bigtime */
				header_content_type_set_param(part->content_type, "charset", charset?charset:"us-ascii");
				newct = header_content_type_format(part->content_type);
				if (newct) {
					d(printf("Setting content-type to %s\n", newct));

					camel_mime_part_set_content_type(part, newct);
					g_free(newct);
				}
			}
		}
	}

	return TRUE;
}

void
camel_mime_message_set_best_encoding(CamelMimeMessage *msg, CamelBestencRequired required, CamelBestencEncoding enctype)
{
	struct _enc_data data;

	if ((required & (CAMEL_BESTENC_GET_ENCODING|CAMEL_BESTENC_GET_CHARSET)) == 0)
		return;

	data.required = required;
	data.enctype = enctype;

	camel_mime_message_foreach_part(msg, best_encoding, &data);
}

void
camel_mime_message_encode_8bit_parts (CamelMimeMessage *mime_message)
{
	camel_mime_message_set_best_encoding(mime_message, CAMEL_BESTENC_GET_ENCODING, CAMEL_BESTENC_7BIT);
}

