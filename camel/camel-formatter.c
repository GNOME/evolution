
/*--------------------------------*-C-*---------------------------------*
 *
 * Author :
 *  Matt Loper <matt@helixcode.com>
 *
 *  Copyright 2000, Helix Code, Inc. (http://www.helixcode.com) .
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 *----------------------------------------------------------------------*/

#include <config.h>
#include "camel-formatter.h"

static GtkObjectClass *parent_class=NULL;
static void _finalize (GtkObject *object);

struct _CamelFormatterPrivate {
	/* nothing here yet */
};

GHashTable *mime_function_table = NULL;

static void
write_field_to_stream (gchar* description, gchar* value, CamelStream *stream)
{
	gchar *s;
	g_assert (description && value);
	
	s = g_strdup_printf ("<b>%s: %s</b><br>\n",
			     description, value);
	camel_stream_write_string (stream, s);
	g_free (s);
}

static void
write_recipients_to_stream (const gchar *recipient_type,
			    const GList *recipients,
			    CamelStream* stream)
{
	gchar *s;
	g_assert (recipient_type && stream);
	
	/* Write "To:", "CC:", or "BCC:" to the stream */
	s = g_strdup_printf ("<b>%s:</b> ", recipient_type);
	camel_stream_write_string (stream, s);
	g_free (s);

	/* Write out each recipient of 'recipient_type' to the stream */
 	while (recipients) {
		camel_stream_write_string (stream, recipients->data);
		recipients = recipients->next;
		if (recipients)
			camel_stream_write_string (stream, "; ");
	}
	camel_stream_write_string (stream, "<br><br>\n");	
}


CamelFormatter*
camel_formatter_new ()
{
	return (gtk_type_new (CAMEL_FORMATTER_TYPE));
}


static void
write_header_info_to_stream (CamelMimeMessage* mime_message,
			     CamelStream* stream)
{
	gchar *s = NULL;
	const GList *recipients = NULL;

	g_assert (mime_message && stream);

	camel_stream_write_string (stream, "Content type: text/html\n");
	
	/* A few fields will probably be available from the mime_message;
	   for each one that's available, write it to the output stream
	   with a helper function, 'write_field_to_stream'. */
	if ((s = (gchar*)camel_mime_message_get_subject (mime_message))) {
		write_field_to_stream ("Subject: ", s, stream);
	}

	if ((s = (gchar*)camel_mime_message_get_from (mime_message))) {
		write_field_to_stream ("From: ", s, stream);
	}

	if ((s = (gchar*)camel_mime_message_get_received_date (mime_message))) {
		write_field_to_stream ("Received Date: ", s, stream);
	}

	if ((s = (gchar*)camel_mime_message_get_sent_date (mime_message))) {
		write_field_to_stream ("Sent Date: ", s, stream);
	}		
 
	/* Fill out the "To:" recipients line */
	recipients = camel_mime_message_get_recipients (
		mime_message, CAMEL_RECIPIENT_TYPE_TO);
	
	if (recipients)
		write_recipients_to_stream ("To:", recipients, stream);

	/* Fill out the "CC:" recipients line */
	recipients = camel_mime_message_get_recipients (
		mime_message, CAMEL_RECIPIENT_TYPE_CC);
	if (recipients)
		write_recipients_to_stream ("CC:", recipients, stream);	

	/* Fill out the "BCC:" recipients line */
	recipients = camel_mime_message_get_recipients (
		mime_message, CAMEL_RECIPIENT_TYPE_BCC);	
	if (recipients)
		write_recipients_to_stream ("BCC:", recipients, stream);
}

 
#define MIME_TYPE_WHOLE(a)  (gmime_content_field_get_mime_type ( \
                                      camel_mime_part_get_content_type (CAMEL_MIME_PART (a))))
#define MIME_TYPE_MAIN(a)  ((camel_mime_part_get_content_type (CAMEL_MIME_PART (a)))->type)
#define MIME_TYPE_SUB(a)   ((camel_mime_part_get_content_type (CAMEL_MIME_PART (a)))->subtype)

static gboolean
strmatch (gchar *a, gchar *b)
{
	return (g_strcasecmp (a,b) == 0);
}


static void
write_mimepart_to_stream (CamelMimePart* mime_part, CamelStream* stream)
{
	if (strmatch (MIME_TYPE_WHOLE(mime_part), "text/plain")) {
                /* print out the shit plain-style */
	}
	else if (strmatch (MIME_TYPE_WHOLE(mime_part), "text/html")) {
                /* print out the html */
	}
	else if (strmatch (MIME_TYPE_MAIN(mime_part), "image")) {
                /* print out <img src="foofuck"> */
	}
}


/* returns NULL if no text/html or text/plan msg is found */
static CamelMimePart*
find_preferred_displayable_body_part_in_multipart_related (
	CamelMultipart* multipart)
{
	int i, max_multiparts;
	CamelMimePart* html_part = NULL;
	CamelMimePart* plain_part = NULL;	

	/* find out out many parts are in it...*/
	max_multiparts = camel_multipart_get_number (multipart);

        /* TODO: DO LEAF-LOOKUP HERE FOR OTHER MIME-TYPES!!! */

        /* ...and write each one, as html, into the stream. */
	for (i = 0; i < max_multiparts; i++) {
		CamelMimeBodyPart* body_part = camel_multipart_get_part (multipart, i);
		if (strmatch (MIME_TYPE_SUB (body_part), "plain")) {
			plain_part = CAMEL_MIME_PART (body_part);
		}
		else if (strmatch (MIME_TYPE_SUB (body_part), "html")) {
			html_part = CAMEL_MIME_PART (body_part);
		}
	}

	if (html_part)
		return html_part;
	if (plain_part)
		return plain_part;
	return NULL;
}

/* Converts the contents of a CamelMimePart into HTML */
static void
mime_part_to_html (CamelFormatter* formatter, CamelMimePart* part,
			    CamelStream *stream)
{
	/* Get the mime-type of the mime message */
	gchar* mime_type_whole = /* ex. "text/plain" */
		MIME_TYPE_WHOLE (part);

	/* get the contents of the mime message */
	CamelDataWrapper* message_contents =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));

	/* if we're dealing with a multipart/related message... */
	if (strmatch (MIME_TYPE_WHOLE (part), "multipart/related")) {

		CamelMultipart *multipart = CAMEL_MULTIPART (
			message_contents);

		int i, max_multiparts;

		/* find out out many parts are in it...*/
		max_multiparts = camel_multipart_get_number (multipart);

		/* ...and write each one, as html, into the stream. */
		for (i = 0; i < max_multiparts; i++) {
			CamelMimeBodyPart* body_part =
				camel_multipart_get_part (multipart, i);

			/* TODO: insert html delimiters, probably
			 * <hr>'s, before and after the following
			 * call*/
			mime_part_to_html (
				formatter, CAMEL_MIME_PART (body_part),
				stream);
		}
	}

	/* okay, it's not multipart-related, so we have only one 'thing'
	 * to convert to html */
	else { 
		CamelMimePart* mime_part = NULL;

		/* if it's a multipart/alternate, track down one we can
		 * convert to html (if any) */
		if (strmatch (mime_type_whole, "multipart/alternate")) {
			mime_part =
				find_preferred_displayable_body_part_in_multipart_related (
					CAMEL_MULTIPART(message_contents));
		}

		else if (strmatch (mime_type_whole, "text/plain") ||
			 strmatch (mime_type_whole, "text/html")) {

			mime_part = CAMEL_MIME_PART (message_contents);
		}

		if (message_contents) {

		}
		else {
			gchar *error_string = g_strdup_printf (
				"Sorry, but I don't know how to display items of type %s\n",
				mime_type_whole);

			camel_stream_write_string (stream, error_string);
			g_free (error_string);
		}
	}	
	
}


/**
 * camel_formatter_mime_message_to_html: 
 * @formatter: the camel formatter object
 * @mime_message: the input mime message
 * @stream: byte stream where data will be written 
 *
 * Writes a CamelMimeMessage out, as html, into a stream passed in as
 * a parameter.
 **/
void
camel_formatter_mime_message_to_html (CamelFormatter* formatter,
				      CamelMimeMessage* mime_message,
				      CamelStream* stream)
{

}

typedef void (*mime_handler_fn) (CamelFormatter *formatter,
				 CamelDataWrapper *data_wrapper,
				 CamelStream *stream);

static void
handle_text_plain (CamelFormatter *formatter, CamelDataWrapper *wrapper,
		   CamelStream *stream)
{
	
}

static void
handle_html (CamelFormatter *formatter, CamelDataWrapper *wrapper,
	     CamelStream *stream)
{
	
}

static void
handle_multipart_related (CamelFormatter *formatter,
			  CamelDataWrapper *wrapper,
			  CamelStream *stream)
{
	
}

static void
handle_multipart_alternate (CamelFormatter *formatter,
			    CamelDataWrapper *wrapper,
			    CamelStream *stream)
{
	
}

static void
handle_unknown_type (CamelFormatter *formatter,
		     CamelDataWrapper *wrapper,
		     CamelStream *stream)
{
	
}

static void
call_handler_function (CamelFormatter* formatter,
		       CamelDataWrapper* wrapper, gchar* mimetype)
{
	mime_handler_fn handler_function;
	
	handler_function = g_hash_table_lookup (
		mime_function_table, mimetype);

	if (!handler_function)
		handler_function = handle_unknown_type;
}


static void
handle_mime_message (CamelFormatter *formatter,
		     CamelDataWrapper *wrapper,
		     CamelStream *stream)
{
	CamelMimeMessage* mime_message =
		CAMEL_MIME_MESSAGE (wrapper);
	
	CamelDataWrapper* message_contents =
		camel_medium_get_content_object (CAMEL_MEDIUM (mime_message));

	/* write the subj:, to:, from: etc. fields out as html */
	write_header_info_to_stream (mime_message, stream);

	/* dispatch the correct handler function for the mime type */
	call_handler_function (formatter, message_contents, MIME_TYPE_WHOLE (mime_message));
	
	/* close up the table opened by 'write_header_info_to_stream' */
	camel_stream_write_string (stream, "</td></tr></table>");		
}

static void
camel_formatter_class_init (CamelFormatterClass *camel_formatter_class)
{
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_formatter_class);

	parent_class = gtk_type_class (gtk_object_get_type ());

	mime_function_table = g_hash_table_new (g_str_hash, g_str_equal);

#define ADD_HANDLER(a,b) g_hash_table_insert (mime_function_table, a, b)

	/* hook up mime types to functions that handle them */
	ADD_HANDLER ("text/plain", handle_text_plain);
	ADD_HANDLER ("text/html", handle_html);
	ADD_HANDLER ("multipart/alternate", handle_multipart_alternate);
	ADD_HANDLER ("multipart/related", handle_multipart_related);
	ADD_HANDLER ("message/rfc822", handle_mime_message);	

        /* virtual method overload */
	gtk_object_class->finalize = _finalize;
}

static void
camel_formatter_init (gpointer object, gpointer klass) 
{
	CamelFormatter* cmf = CAMEL_FORMATTER (object);
	cmf->priv = g_new (CamelFormatterPrivate, 1);
}

 
GtkType
camel_formatter_get_type (void)
{
	static GtkType camel_formatter_type = 0;
	
	if (!camel_formatter_type)	{
		GtkTypeInfo camel_formatter_info =	
		{
			"CamelFormatter",
			sizeof (CamelFormatter),
			sizeof (CamelFormatterClass),
			(GtkClassInitFunc) camel_formatter_class_init,
			(GtkObjectInitFunc) camel_formatter_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_formatter_type = gtk_type_unique (
			gtk_object_get_type (),
			&camel_formatter_info);
	}
	
	return camel_formatter_type;
}


static void           
_finalize (GtkObject *object)
{
	CamelFormatter *formatter = CAMEL_FORMATTER (object);

	g_free (formatter->priv);
	
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}


