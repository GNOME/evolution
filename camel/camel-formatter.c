
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
			    CamelStream* stream_out)
{
	gchar *s;
	g_assert (recipient_type && stream_out);
	
	/* Write "To:", "CC:", or "BCC:" to the stream */
	s = g_strdup_printf ("<b>%s:</b> ", recipient_type);
	camel_stream_write_string (stream_out, s);
	g_free (s);

	/* Write out each recipient of 'recipient_type' to the stream */
 	while (recipients) {
		camel_stream_write_string (stream_out, recipients->data);
		recipients = recipients->next;
		if (recipients)
			camel_stream_write_string (stream_out, "; ");
	}
	camel_stream_write_string (stream_out, "<br><br>\n");	
}


CamelFormatter*
camel_formatter_new ()
{
	return (gtk_type_new (CAMEL_FORMATTER_TYPE));
}


static void
write_header_info_to_stream (CamelMimeMessage* mime_message,
			     CamelStream* stream_out)
{
	gchar *s = NULL;
	const GList *recipients = NULL;

	g_assert (mime_message && stream_out);

	camel_stream_write_string (stream_out, "Content type: text/html\n");
	
	/* A few fields will probably be available from the mime_message;
	   for each one that's available, write it to the output stream
	   with a helper function, 'write_field_to_stream'. */
	if ((s = (gchar*)camel_mime_message_get_subject (mime_message))) {
		write_field_to_stream ("Subject: ", s, stream_out);
	}

	if ((s = (gchar*)camel_mime_message_get_from (mime_message))) {
		write_field_to_stream ("From: ", s, stream_out);
	}

	if ((s = (gchar*)camel_mime_message_get_received_date (mime_message))) {
		write_field_to_stream ("Received Date: ", s, stream_out);
	}

	if ((s = (gchar*)camel_mime_message_get_sent_date (mime_message))) {
		write_field_to_stream ("Sent Date: ", s, stream_out);
	}		
 
	/* Fill out the "To:" recipients line */
	recipients = camel_mime_message_get_recipients (
		mime_message, CAMEL_RECIPIENT_TYPE_TO);
	
	if (recipients)
		write_recipients_to_stream ("To:", recipients, stream_out);

	/* Fill out the "CC:" recipients line */
	recipients = camel_mime_message_get_recipients (
		mime_message, CAMEL_RECIPIENT_TYPE_CC);
	if (recipients)
		write_recipients_to_stream ("CC:", recipients, stream_out);	

	/* Fill out the "BCC:" recipients line */
	recipients = camel_mime_message_get_recipients (
		mime_message, CAMEL_RECIPIENT_TYPE_BCC);	
	if (recipients)
		write_recipients_to_stream ("BCC:", recipients, stream_out);
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
find_text_body_part_in_multipart_related (CamelMultipart* multipart)
{
	int i, max_multiparts;
	CamelMimePart* html_part = NULL;
	CamelMimePart* plain_part = NULL;	

	/* find out out many parts are in it...*/
	max_multiparts = camel_multipart_get_number (multipart);
	
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


/**
 * camel_formatter_make_html: 
 * @formatter: the camel formatter object
 * @stream_out: byte stream where data will be written 
 *
 * Writes a CamelMimeMessage out, as html, into a stream passed in as
 * a parameter.
 **/
void
camel_formatter_make_html (CamelFormatter* formatter,
			   CamelMimeMessage* mime_message,
			   CamelStream* stream_out)
{
	/* Get the mime-type of the mime message */
	gchar* mime_type_whole = /* ex. "text/plain" */
		MIME_TYPE_WHOLE (mime_message);

	/* get the contents of the mime message */
	CamelDataWrapper* message_contents = camel_medium_get_content_object (
		CAMEL_MEDIUM (mime_message));
	
	/* write the 'subject:', 'to:', 'from:', etc. into the output stream */
	write_header_info_to_stream (mime_message, stream_out);

	/* if we're dealing with a multipart/related message... */
	if (strmatch (MIME_TYPE_WHOLE (mime_message), "multipart/related")) {

		CamelMultipart *multipart = CAMEL_MULTIPART (
			message_contents);

		int i, max_multiparts;

		/* find out out many parts are in it...*/
		max_multiparts = camel_multipart_get_number (multipart);

		/* ...and write each one, as html, into the stream. */
		for (i = 0; i < max_multiparts; i++) {
			CamelMimeBodyPart* body_part =
				camel_multipart_get_part (multipart, i);

			write_mimepart_to_stream (CAMEL_MIME_PART (body_part), stream_out);
		}
	}
	else { /* okay, it's not multipart-related */

		CamelMimePart* mime_part = NULL;

		if (strmatch (mime_type_whole, "multipart/alternate")) {
			mime_part =
				find_text_body_part_in_multipart_related (
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

			camel_stream_write_string (stream_out, error_string);
			g_free (error_string);
		}
	}
}


static void
camel_formatter_class_init (CamelFormatterClass *camel_formatter_class)
{
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_formatter_class);

	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method overload */
	gtk_object_class->finalize = _finalize;
}

void
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


