/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Matt Loper <matt@helixcode.com>
 *
 *  Copyright 2000, Helix Code, Inc. (http://www.helixcode.com)
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
 */

#include <config.h>
#include "mail-format.h"
#include "camel/hash-table-utils.h"

#include <libgnome/libgnome.h>
#include <ctype.h>    /* for isprint */
#include <string.h>   /* for strstr  */

/* We shouldn't be doing this, but I don't feel like fixing it right
 * now. (It's for gtk_html_stream_write.) When gtkhtml has nicer
 * interfaces, we can fix it.
 */
#include <gtkhtml/gtkhtml-private.h>

static void handle_text_plain           (CamelDataWrapper *wrapper,
			                 GtkHTMLStreamHandle *stream,
					 CamelDataWrapper *root);
static void handle_text_html            (CamelDataWrapper *wrapper,
			                 GtkHTMLStreamHandle *stream,
					 CamelDataWrapper *root);
static void handle_image                (CamelDataWrapper *wrapper,
				         GtkHTMLStreamHandle *stream,
					 CamelDataWrapper *root);
static void handle_vcard                (CamelDataWrapper *wrapper,
				         GtkHTMLStreamHandle *stream,
					 CamelDataWrapper *root);
static void handle_mime_part            (CamelDataWrapper *wrapper,
			                 GtkHTMLStreamHandle *stream,
					 CamelDataWrapper *root);
static void handle_multipart_mixed      (CamelDataWrapper *wrapper,
				         GtkHTMLStreamHandle *stream,
					 CamelDataWrapper *root);
static void handle_multipart_related    (CamelDataWrapper *wrapper,
				         GtkHTMLStreamHandle *stream,
					 CamelDataWrapper *root);
static void handle_multipart_alternative(CamelDataWrapper *wrapper,
				         GtkHTMLStreamHandle *stream,
					 CamelDataWrapper *root);
static void handle_unknown_type         (CamelDataWrapper *wrapper,
				         GtkHTMLStreamHandle *stream,
					 CamelDataWrapper *root);

/* encodes some characters into their 'escaped' version;
 * so '<' turns into '&lt;', and '"' turns into '&quot;' */
static gchar *text_to_html (const guchar *input,
			    guint len,
			    guint *encoded_len_return,
			    gboolean convert_newlines_to_br);

/* writes the header info for a mime message into an html stream */
static void write_header_info_to_stream (CamelMimeMessage* mime_message,
					 GtkHTMLStreamHandle *stream);

/* dispatch html printing via mimetype */
static void call_handler_function (CamelDataWrapper *wrapper,
				   gchar *mimetype_whole, 
				   gchar *mimetype_main,
				   GtkHTMLStreamHandle *stream,
				   CamelDataWrapper *root);

#if 0
/**
 * camel_formatter_wrapper_to_html: 
 * @formatter: the camel formatter object
 * @data_wrapper: the data wrapper
 * @stream: byte stream where data will be written 
 *
 * Writes a CamelDataWrapper out, as html, into a stream passed in as
 * a parameter.
 **/
void camel_formatter_wrapper_to_html (CamelFormatter* formatter,
				      CamelDataWrapper* data_wrapper,
				      CamelStream* stream_out)
{
	CamelFormatterPrivate* fmt = formatter->priv;
	gchar *mimetype_whole =
		g_strdup_printf ("%s/%s",
				 data_wrapper->mime_type->type,
				 data_wrapper->mime_type->subtype);

	debug ("camel_formatter_wrapper_to_html: entered\n");
	g_assert (formatter && data_wrapper && stream_out);

	/* give the root CamelDataWrapper and the stream to the formatter */
	initialize_camel_formatter (formatter, data_wrapper, stream_out);
	
	if (stream_out) {
		
		/* write everything to the stream */
		camel_stream_write_string (
			fmt->stream, "<html><body bgcolor=\"white\">\n");
		call_handler_function (
			formatter,
			data_wrapper,
			mimetype_whole,
			data_wrapper->mime_type->type);
		
		camel_stream_write_string (fmt->stream, "\n</body></html>\n");
	}
	

	g_free (mimetype_whole);
}
#endif

/**
 * mail_format_mime_message: 
 * @mime_message: the input mime message
 * @header_stream: HTML stream to write headers to
 * @body_stream: HTML stream to write data to
 *
 * Writes a CamelMimeMessage out, as html, into streams passed in as
 * a parameter. Either stream may be #NULL.
 **/
void
mail_format_mime_message (CamelMimeMessage *mime_message,
			  GtkHTMLStreamHandle *header_stream,
			  GtkHTMLStreamHandle *body_stream)
{
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (mime_message));

	/* Write the headers fields out as HTML to the header stream. */
	if (header_stream)
		write_header_info_to_stream (mime_message, header_stream);

	/* Write the contents of the MIME message to the body stream. */
	if (body_stream) {
		mail_write_html (body_stream, "<html><body>\n");
		call_handler_function (CAMEL_DATA_WRAPPER (mime_message),
				       "message/rfc822",
				       "message",
				       body_stream,
				       CAMEL_DATA_WRAPPER (mime_message));
		mail_write_html (body_stream, "\n</body></html>\n");
	}
}

/* We're maintaining a hashtable of mimetypes -> functions;
 * Those functions have the following signature...
 */
typedef void (*mime_handler_fn) (CamelDataWrapper *wrapper,
				 GtkHTMLStreamHandle *stream,
				 CamelDataWrapper *root);

static gchar*
lookup_unique_id (CamelDataWrapper *root, CamelDataWrapper *child)
{
	/* ** FIXME : replace this with a string representing
	   the location of the objetc in the tree */
	/* TODO: assert our return value != NULL */

	gchar *temp_hack_uid;

	temp_hack_uid = g_strdup_printf ("%p", camel_data_wrapper_get_output_stream (child));

	return temp_hack_uid;
}

static GHashTable *mime_function_table;

/* This tries to create a tag, given a mimetype and the child of a
 * mime message. It can return NULL if it can't match the mimetype to
 * a bonobo object.
 */
static gchar *
get_bonobo_tag_for_object (CamelDataWrapper *wrapper,
			   gchar *mimetype, CamelDataWrapper *root)
{
	char *uid = lookup_unique_id (root, wrapper);
	const char *goad_id = gnome_mime_get_value (mimetype,
						    "bonobo-goad-id");

	if (goad_id) {
		return g_strdup_printf ("<object classid=\"%s\"> "
					"<param name=\"uid\" "
					"value=\"camel://%s\"> </object>",
					goad_id, uid);
	} else
		return NULL;
}


/*
 * This takes a mimetype, and tries to map that mimetype to a function
 * or a bonobo object.
 *
 * - If it's mapped to a bonobo object, this function prints a tag
 *   into the stream, designating the bonobo object and a place that
 *   the bonobo object can find data to hydrate from
 *
 * - otherwise, the mimetype is mapped to another function, which can
 *   print into the stream
 */
static void
call_handler_function (CamelDataWrapper *wrapper,
		       gchar *mimetype_whole_in, /* ex. "image/jpeg" */
		       gchar *mimetype_main_in, /* ex. "image" */
		       GtkHTMLStreamHandle *stream,
		       CamelDataWrapper *root)
{
	mime_handler_fn handler_function = NULL;
	gchar *mimetype_whole = NULL;
	gchar *mimetype_main = NULL;

	g_return_if_fail (mimetype_whole_in || mimetype_main_in);
	g_return_if_fail (CAMEL_IS_DATA_WRAPPER (wrapper));
	g_return_if_fail (CAMEL_IS_DATA_WRAPPER (root));

	if (mime_function_table == NULL) {
		mime_function_table = g_hash_table_new (g_strcase_hash,
							g_strcase_equal);

		/* hook up mime types to functions that handle them */
		g_hash_table_insert (mime_function_table, "text/plain",
				     handle_text_plain);
		g_hash_table_insert (mime_function_table, "text/richtext",
				     handle_text_plain);
		g_hash_table_insert (mime_function_table, "text/html",
				     handle_text_html);
		g_hash_table_insert (mime_function_table, "multipart/alternative",
				     handle_multipart_alternative);
		g_hash_table_insert (mime_function_table, "multipart/related",
				     handle_multipart_related);
		g_hash_table_insert (mime_function_table, "multipart/mixed",
				     handle_multipart_mixed);
		g_hash_table_insert (mime_function_table, "message/rfc822",
				     handle_mime_part);
		g_hash_table_insert (mime_function_table, "image",
				     handle_image);
		g_hash_table_insert (mime_function_table, "vcard",
				     handle_vcard);

		/* RFC 2046 says unrecognized multipart subtypes should
		 * be treated like multipart/mixed.
		 */
		g_hash_table_insert (mime_function_table, "multipart",
				     handle_multipart_mixed);

		/* Body parts don't have mime parts per se, so Camel
		 * sticks on the following one.
		 */
		g_hash_table_insert (mime_function_table, "mime/body-part",
				     handle_mime_part);
	}

	/* Try to find a handler function in our own lookup table */
	if (mimetype_whole_in) {
		mimetype_whole = g_strdup (mimetype_whole_in);
		g_strdown (mimetype_whole);

		handler_function = g_hash_table_lookup (mime_function_table,
							mimetype_whole);
	}

	if (mimetype_main_in && !handler_function) {
		mimetype_main = g_strdup (mimetype_main_in);
		g_strdown (mimetype_main);

		handler_function = g_hash_table_lookup (mime_function_table,
							mimetype_main);
	}

	/* Upon failure, try to find a bonobo object to show the object */
	if (!handler_function) {
		gchar *bonobo_tag = NULL;

		if (mimetype_whole)
			bonobo_tag = get_bonobo_tag_for_object (
				wrapper, mimetype_whole, root);

		if (mimetype_main && !bonobo_tag)
			bonobo_tag = get_bonobo_tag_for_object (
				wrapper, mimetype_main, root);

		if (bonobo_tag) {
			/* We can print a tag, and return! */

			mail_write_html (stream, bonobo_tag);
			g_free (bonobo_tag);
			if (mimetype_whole)
				g_free (mimetype_whole);
			if (mimetype_main)
				g_free (mimetype_main);

			return; 
		}
	}

	/* Use either a handler function we've found, or a default handler. */
	if (handler_function)
		(*handler_function) (wrapper, stream, root);
	else
		handle_unknown_type (wrapper, stream, root);
	if (mimetype_whole)
		g_free (mimetype_whole);
	if (mimetype_main)
		g_free (mimetype_main);
}


/* Convert plain text in equivalent-looking valid HTML. */
static gchar *
text_to_html (const guchar *input, guint len,
	      guint *encoded_len_return,
	      gboolean convert_newlines_to_br)
{
	const guchar *cur = input;
	guchar *buffer = NULL;
	guchar *out = NULL;
	gint buffer_size = 0;
	guint count;

	/* Allocate a translation buffer.  */
	buffer_size = len * 2;
	buffer = g_malloc (buffer_size);

	out = buffer;
	count = 0;

	while (len--) {
		if (out - buffer > buffer_size - 100) {
			gint index = out - buffer;

			buffer_size *= 2;
			buffer = g_realloc (buffer, buffer_size);
			out = buffer + index;
		}

		switch (*cur) {
		case '<':
			strcpy (out, "&lt;");
			out += 4;
			break;

		case '>':
			strcpy (out, "&gt;");
			out += 4;
			break;

		case '&':
			strcpy (out, "&amp;");
			out += 5;
			break;

		case '"':
			strcpy (out, "&quot;");
			out += 6;
			break;

		case '\n':
			*out++ = *cur;
			if (convert_newlines_to_br) {
				strcpy (out, "<br>");
				out += 4;
			}
			break;

		default:
			if ((*cur >= 0x20 && *cur < 0x80) ||
			    (*cur == '\r' || *cur == '\t')) {
				/* Default case, just copy. */
				*out++ = *cur;
			} else
				out += g_snprintf(out, 9, "&#%d;", *cur);
			break;
		}

		cur++;
	}

	*out = '\0';
	if (encoded_len_return)
		*encoded_len_return = out - buffer;

	return buffer;
}


static void
write_field_to_stream (const gchar *description, const gchar *value,
		       GtkHTMLStreamHandle *stream)
{
	gchar *s;
	gchar *encoded_value;

	if (value) {
		unsigned char *p;

		encoded_value = text_to_html (value, strlen(value),
					      NULL, TRUE);
		for (p = (unsigned char *)encoded_value; *p; p++) {
			if (!isprint (*p))
				*p = '?';
		}
	} else
		encoded_value = g_strdup ("");

	s = g_strdup_printf ("<tr valign=top><th align=right>%s</th>"
			     "<td>%s</td></tr>", description, encoded_value);
	mail_write_html (stream, s);
	g_free (encoded_value);
	g_free (s);
}

static void
write_recipients_to_stream (const gchar *recipient_type,
			    const GList *recipients,
			    GtkHTMLStreamHandle *stream)
{
	gchar *recipients_string = NULL;

 	while (recipients) {
		gchar *old_string = recipients_string;
		recipients_string =
			g_strdup_printf ("%s%s%s",
					 old_string ? old_string : "",
					 old_string ? "; " : "",
					 (gchar *)recipients->data);
		g_free (old_string);

		recipients = recipients->next;
	}

	write_field_to_stream (recipient_type, recipients_string, stream);
	g_free (recipients_string);
}



static void
write_header_info_to_stream (CamelMimeMessage *mime_message,
			     GtkHTMLStreamHandle *stream)
{
	GList *recipients;

	mail_write_html (stream, "<table>");

	/* A few fields will probably be available from the mime_message;
	 * for each one that's available, write it to the output stream
	 * with a helper function, 'write_field_to_stream'.
	 */

	write_field_to_stream ("From:",
			       camel_mime_message_get_from (mime_message),
			       stream);

	write_recipients_to_stream ("To:",
				    camel_mime_message_get_recipients (mime_message, CAMEL_RECIPIENT_TYPE_TO),
				    stream);

	recipients = camel_mime_message_get_recipients (mime_message, CAMEL_RECIPIENT_TYPE_CC);
	if (recipients)
		write_recipients_to_stream ("Cc:", recipients, stream);
	write_field_to_stream ("Subject:",
			       camel_mime_message_get_subject (mime_message),
			       stream);

	mail_write_html (stream, "</table>");	
}

/* case-insensitive string comparison */
static gint
strcase_equal (gconstpointer v, gconstpointer v2)
{
	return g_strcasecmp ((const gchar*) v, (const gchar*)v2) == 0;
}

#define MIME_TYPE_WHOLE(a)  (gmime_content_field_get_mime_type ( \
                                      camel_mime_part_get_content_type (CAMEL_MIME_PART (a))))
#define MIME_TYPE_MAIN(a)  ((camel_mime_part_get_content_type (CAMEL_MIME_PART (a)))->type)
#define MIME_TYPE_SUB(a)   ((camel_mime_part_get_content_type (CAMEL_MIME_PART (a)))->subtype)


/*----------------------------------------------------------------------*
 *                     Mime handling functions
 *----------------------------------------------------------------------*/

static void
handle_text_plain (CamelDataWrapper *wrapper, GtkHTMLStreamHandle *stream,
		   CamelDataWrapper *root)
{
	gchar *text;
	CamelStream *wrapper_output_stream;
	gchar tmp_buffer[4096];
	gint nb_bytes_read;
	gboolean empty_text = TRUE;

	
	mail_write_html (stream, "\n<!-- text/plain below -->\n");
	mail_write_html (stream, "<pre>\n");

	/* FIXME: text/richtext is not difficult to translate into HTML */
	if (strcmp (wrapper->mime_type->subtype, "richtext") == 0) {
		mail_write_html (stream, "<center><b>"
				 "<table bgcolor=\"b0b0ff\" cellpadding=3>"
				 "<tr><td>Warning: the following "
				 "richtext may not be formatted correctly. "
				 "</b></td></tr></table></center><br>");
	}

	/* Get the output stream of the data wrapper. */
	wrapper_output_stream = camel_data_wrapper_get_output_stream (wrapper);
	camel_stream_reset (wrapper_output_stream);

	do {
		/* Read next chunk of text. */
		nb_bytes_read = camel_stream_read (wrapper_output_stream,
						   tmp_buffer, 4096);

		/* If there's any text, write it to the stream. */
		if (nb_bytes_read > 0) {
			int returned_strlen;

			empty_text = FALSE;

			/* replace '<' with '&lt;', etc. */
			text = text_to_html (tmp_buffer,
					     nb_bytes_read,
					     &returned_strlen,
					     FALSE);
			mail_write_html (stream, text);
			g_free (text);
		}
	} while (!camel_stream_eos (wrapper_output_stream));

	if (empty_text)
		mail_write_html (stream, "<b>(empty)</b>");

	mail_write_html (stream, "</pre>\n");	
}

static void
handle_text_html (CamelDataWrapper *wrapper, GtkHTMLStreamHandle *stream,
		  CamelDataWrapper *root)
{
	CamelStream *wrapper_output_stream;
	gchar tmp_buffer[4096];
	gint nb_bytes_read;
	gboolean empty_text = TRUE;

	/* Get the output stream of the data wrapper. */
	wrapper_output_stream = camel_data_wrapper_get_output_stream (wrapper);
	camel_stream_reset (wrapper_output_stream);

	/* Write the header. */
	mail_write_html (stream, "\n<!-- text/html below -->\n");

	do {
		/* Read next chunk of text. */
		nb_bytes_read = camel_stream_read (wrapper_output_stream,
						   tmp_buffer, 4096);

		/* If there's any text, write it to the stream */
		if (nb_bytes_read > 0) {
			empty_text = FALSE;
			
			/* Write the buffer to the html stream */
			gtk_html_stream_write (stream, tmp_buffer,
					       nb_bytes_read);
		}
	} while (!camel_stream_eos (wrapper_output_stream));

	if (empty_text)
		mail_write_html (stream, "<b>(empty)</b>");
}

static void
handle_image (CamelDataWrapper *wrapper, GtkHTMLStreamHandle *stream,
	      CamelDataWrapper *root)
{
	gchar *uuid;
	gchar *tag;

	uuid = lookup_unique_id (root, wrapper);

	mail_write_html (stream, "\n<!-- image below -->\n");
	tag = g_strdup_printf ("<img src=\"camel://%s\">\n", uuid);
	mail_write_html (stream, tag);
	g_free (uuid);
	g_free (tag);		
}

static void
handle_vcard (CamelDataWrapper *wrapper, GtkHTMLStreamHandle *stream,
	      CamelDataWrapper *root)
{
	mail_write_html (stream, "\n<!-- vcard below -->\n");

	/* FIXME: do something here. */
}

static void
handle_mime_part (CamelDataWrapper *wrapper, GtkHTMLStreamHandle *stream,
		  CamelDataWrapper *root)
{
	CamelMimePart *mime_part; 
	CamelDataWrapper *message_contents; 
	gchar *whole_mime_type;

	g_return_if_fail (CAMEL_IS_MIME_PART (wrapper));

	mime_part = CAMEL_MIME_PART (wrapper);
	message_contents =
		camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

	g_assert (message_contents);

	mail_write_html (stream, "\n<!-- mime message below -->\n");

//	mail_write_html (stream,
//				   "<table width=95% border=1><tr><td>\n\n");		

	/* dispatch the correct handler function for the mime type */
	whole_mime_type = MIME_TYPE_WHOLE (mime_part);
	call_handler_function (message_contents,
			       whole_mime_type,
			       MIME_TYPE_MAIN (mime_part),
			       stream, root);
	g_free (whole_mime_type);

	/* close up the table we opened */
//	mail_write_html (stream,
//				   "\n\n</td></tr></table>\n\n");
}


/* called for each body part in a multipart/mixed */
static void
display_camel_body_part (CamelMimeBodyPart *body_part,
			 GtkHTMLStreamHandle *stream,
			 CamelDataWrapper *root)
{
	gchar *whole_mime_type;

	CamelDataWrapper* contents =
		camel_medium_get_content_object (CAMEL_MEDIUM (body_part));

	whole_mime_type = MIME_TYPE_WHOLE (body_part);
	call_handler_function (contents, whole_mime_type,
			       MIME_TYPE_MAIN (body_part),
			       stream, root);
	g_free (whole_mime_type);

	mail_write_html (stream, "\n<hr>\n");
}


/* Our policy here is this:
   (1) print text/(plain|html) parts found
   (2) print vcards and images inline
   (3) treat all other parts as attachments */
static void
handle_multipart_mixed (CamelDataWrapper *wrapper,
			GtkHTMLStreamHandle *stream, CamelDataWrapper *root)
{
	CamelMultipart *mp;
	int i, nparts;

	g_return_if_fail (CAMEL_IS_MULTIPART (wrapper));

	mp = CAMEL_MULTIPART (wrapper);

	nparts = camel_multipart_get_number (mp);	
	for (i = 0; i < nparts; i++) {
		CamelMimeBodyPart *body_part =
			camel_multipart_get_part (mp, i);

		display_camel_body_part (body_part, stream, root);
	}
}

static void
handle_multipart_related (CamelDataWrapper *wrapper,
			  GtkHTMLStreamHandle *stream,
			  CamelDataWrapper *root)
{
//	CamelMultipart* mp = CAMEL_MULTIPART (wrapper);

	/* FIXME: read RFC, in terms of how a one message
	   may refer to another object */	
}

/* multipart/alternative helper function --
 * Returns NULL if no displayable msg is found
 */
static CamelMimePart *
find_preferred_alternative (CamelMultipart* multipart)
{
	int i, nparts;
	CamelMimePart* html_part = NULL;
	CamelMimePart* plain_part = NULL;	

	/* Find out out many parts are in it. */
	nparts = camel_multipart_get_number (multipart);

	/* FIXME: DO LEAF-LOOKUP HERE FOR OTHER MIME-TYPES!!! */

	for (i = 0; i < nparts; i++) {
		CamelMimeBodyPart *body_part =
			camel_multipart_get_part (multipart, i);

		if (strcasecmp (MIME_TYPE_MAIN (body_part), "text") != 0)
			continue;

		if (strcasecmp (MIME_TYPE_SUB (body_part), "plain") == 0)
			plain_part = CAMEL_MIME_PART (body_part);
		else if (strcasecmp (MIME_TYPE_SUB (body_part), "html") == 0)
			html_part = CAMEL_MIME_PART (body_part);
	}

	if (html_part)
		return html_part;
	if (plain_part)
		return plain_part;
	return NULL;
}

/* The current policy for multipart/alternative is this: 
 *
 * if (we find a text/html body part)
 *     we print it
 * else if (we find a text/plain body part)
 *     we print it
 * else
 *     we print nothing
 */
static void
handle_multipart_alternative (CamelDataWrapper *wrapper,
			      GtkHTMLStreamHandle *stream,
			      CamelDataWrapper *root)
{
	CamelMultipart *multipart = CAMEL_MULTIPART (wrapper);
	CamelMimePart *mime_part;
	gchar *whole_mime_type;

	mime_part = find_preferred_alternative (multipart);	
	if (mime_part) {
		CamelDataWrapper *contents =
			camel_medium_get_content_object (
				CAMEL_MEDIUM (mime_part));

		whole_mime_type = MIME_TYPE_WHOLE (mime_part);
		call_handler_function (contents, whole_mime_type,
				       MIME_TYPE_MAIN (mime_part),
				       stream, root);
		g_free (whole_mime_type);
	}
}

static void
handle_unknown_type (CamelDataWrapper *wrapper,
		     GtkHTMLStreamHandle *stream,
		     CamelDataWrapper *root)
{
	gchar *tag;
	char *uid = lookup_unique_id (root, wrapper);	

	tag = g_strdup_printf ("<a href=\"camel://%s\">click-me-to-save</a>\n",
			       uid);

	mail_write_html (stream, tag);
}


void
mail_write_html (GtkHTMLStreamHandle *stream, const char *data)
{
	gtk_html_stream_write (stream, data, strlen (data));
}
