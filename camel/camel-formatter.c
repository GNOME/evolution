 
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

#include "camel-log.h"
#include <libgnome/libgnome.h>
#include <ctype.h> // for isprint
#include <string.h> // for strstr

/*
 * The CamelFormatter takes a mime message, and produces html from it,
 * through the single function camel_formatter_mime_message_to_html().
 * The flow of execution goes something like this:
 *
 *     camel_formatter_mime_message_to_html()
 *                 |
 *                 V
 *         handle_mime_message()
 *                 |
 *                 V
 *        call_handler_function()
 *
 * Then, 'call_handler_function' acts as a dispatcher, using a
 * hashtable to match a mime type to one of the following functions;
 * note that the below functions sometimes then use
 * 'call_handler_function()' to continue the process recursively.
 */

static void handle_text_plain           (CamelFormatter *formatter,
			                 CamelDataWrapper *wrapper);
static void handle_text_html            (CamelFormatter *formatter,
			                 CamelDataWrapper *wrapper);
static void handle_image                (CamelFormatter *formatter,
				         CamelDataWrapper *wrapper);
static void handle_vcard                (CamelFormatter *formatter,
				         CamelDataWrapper *wrapper);
static void handle_mime_message         (CamelFormatter *formatter,
			                 CamelDataWrapper *wrapper);
static void handle_multipart_mixed      (CamelFormatter *formatter,
				         CamelDataWrapper *wrapper);
static void handle_multipart_related    (CamelFormatter *formatter,
				         CamelDataWrapper *wrapper);
static void handle_multipart_alternative(CamelFormatter *formatter,
				         CamelDataWrapper *wrapper);
static void handle_unknown_type         (CamelFormatter *formatter,
				         CamelDataWrapper *wrapper);

/* encodes some characters into their 'escaped' version;
 * so '<' turns into '&lt;', and '"' turns into '&quot;' */
static gchar* text_to_html (const guchar *input,
			    guint len,
			    guint *encoded_len_return);

/* compares strings case-insensitively */
static gint strcase_equal (gconstpointer v, gconstpointer v2);
static void str_tolower (gchar* str);

/* writes the header info for a mime message into a stream */
static void write_header_info_to_stream (CamelMimeMessage* mime_message,
					 CamelStream* stream);

static GtkObjectClass *parent_class = NULL;

struct _CamelFormatterPrivate {
	CamelMimeMessage *current_root;
	CamelStream *stream;
	GHashTable *attachments;
};


static void
debug (const gchar *format, ...)
{
	va_list args;
	gchar *string;
	
	g_return_if_fail (format != NULL);
	
	va_start (args, format);
	string = g_strdup_vprintf (format, args);
	va_end (args);
	
	fputs (string, stdout);
	fflush (stdout);
	
	g_free (string);
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
				      CamelStream* header_stream,
				      CamelStream* body_stream)
{
	CamelFormatterPrivate* fmt = formatter->priv;

	g_print ("camel_formatter_mime_message_to_html: entered\n");
	g_assert (mime_message);
	
	/* initialize members of our formatter */
	fmt->current_root = mime_message;
	fmt->stream = body_stream;
	if (fmt->attachments)
		g_hash_table_destroy (fmt->attachments);
	fmt->attachments = g_hash_table_new (g_str_hash, strcase_equal);
	
	/* write the subj:, to:, from: etc. fields out as html to the
           header stream */
	if (header_stream)
		write_header_info_to_stream (mime_message,
					     header_stream);

	/* write everything to the stream */
	if (body_stream) {
		
		camel_stream_write_string (fmt->stream, "<html><body>\n");
		
		handle_mime_message (
			formatter,
			CAMEL_DATA_WRAPPER (mime_message));
		
		camel_stream_write_string (fmt->stream,
					   "\n</body></html>\n");
	}
	else {
		g_print ("camel-formatter.c: ");
	        g_print ("camel_formatter_mime_message_to_html: ");
		g_print ("you don't want the body??\n");
	}
}

/* we're maintaining a hashtable of mimetypes -> functions;
 * those functions have the following signature...*/
typedef void (*mime_handler_fn) (CamelFormatter *formatter,
				 CamelDataWrapper *data_wrapper);

static gchar*
lookup_unique_id (CamelMimeMessage* root, CamelDataWrapper* child)
{
	/* TODO: assert our return value != NULL */

	return "NYI";
}

static GHashTable* mime_function_table;

/* This tries to create a tag, given a mimetype and the child of a
 * mime message. It can return NULL if it can't match the mimetype to
 * a bonobo object.  */
static gchar*
get_bonobo_tag_for_object (CamelFormatter* formatter,
			   CamelDataWrapper* wrapper,
			   gchar* mimetype)
{
	
	CamelMimeMessage* root = formatter->priv->current_root;
	char* uid = lookup_unique_id (root, wrapper);
	const char* goad_id = gnome_mime_get_value (
		mimetype, "bonobo-goad_id");
	
	g_assert (root);
	
	if (goad_id) {
		char* tag = g_strdup_printf (
			"<object classid=\"%s\" uid=\"camel://%s\">",
			goad_id, uid);
		return tag;
	}
	else
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
call_handler_function (CamelFormatter* formatter,
		       CamelDataWrapper* wrapper,
		       gchar* mimetype_whole, /* ex. "image/jpeg" */
		       gchar* mimetype_main)  /* ex. "image" */
{
	mime_handler_fn handler_function = NULL;

	g_assert (formatter);
	g_assert (mimetype_whole || mimetype_main);
	g_assert (wrapper);
	
/*
 * Try to find a handler function in our own lookup table
 */
	str_tolower (mimetype_whole);
	str_tolower (mimetype_main);	
	
	if (mimetype_whole)
		handler_function = g_hash_table_lookup (
			mime_function_table, mimetype_whole);

	if (mimetype_main && !handler_function)
		handler_function = g_hash_table_lookup (
			mime_function_table, mimetype_main);
/*
 * Upon failure, try to find a bonobo object to show the object
 */
	if (!handler_function) {

		gchar* bonobo_tag = NULL;

		if (mimetype_whole)
			bonobo_tag = get_bonobo_tag_for_object (
				formatter, wrapper, mimetype_whole);

		if (mimetype_main && !bonobo_tag)
			bonobo_tag = get_bonobo_tag_for_object (
				formatter, wrapper, mimetype_main);			

		if (bonobo_tag) {

			/* we can print a tag, and return! */
			camel_stream_write_string (
				formatter->priv->stream, bonobo_tag);
			g_free (bonobo_tag);

			return; 
		}
	}
/*
 * Use either a handler function we've found, or a default handler
 */
	if (handler_function)
		(*handler_function)(formatter, wrapper);
	else {
		handle_unknown_type (formatter, wrapper);
		debug ("no function or bonobo object found for mimetype \"%s\"\n",
		       mimetype_whole?mimetype_whole:mimetype_main);
	}
}


/*----------------------------------------------------------------------*
 *     Header (ex. "subj:", "from:") helper functions for mime msgs
 *----------------------------------------------------------------------*/

/* This routine was originally written by Daniel Velliard, (C) 1998
 * World Wide Web Consortium.
 * - It will (for example) turn the input 'ab <c>' into 'ab &lt;c&gt;'
 * - It has also been altered to turn '\n' into <br>. */
static gchar *
text_to_html (const guchar *input,
		 guint len,
		 guint *encoded_len_return)
{
	const guchar *cur = input;
	guchar *buffer = NULL;
	guchar *out = NULL;
	gint buffer_size = 0;
	guint count;

	/* Allocate a translation buffer.  */
	buffer_size = 1000;
	buffer = g_malloc (buffer_size);

	out = buffer;
	count = 0;

	while (count < len) {
		if (out - buffer > buffer_size - 100) {
			gint index = out - buffer;

			buffer_size *= 2;
			buffer = g_realloc (buffer, buffer_size);
			out = &buffer[index];
		}

		/* By default one has to encode at least '<', '>', '"' and '&'.  */
		if (*cur == '<') {
			*out++ = '&';
			*out++ = 'l';
			*out++ = 't';
			*out++ = ';';
		} else if (*cur == '>') {
			*out++ = '&';
			*out++ = 'g';
			*out++ = 't';
			*out++ = ';';
		} else if (*cur == '&') {
			*out++ = '&';
			*out++ = 'a';
			*out++ = 'm';
			*out++ = 'p';
			*out++ = ';';
		} else if (*cur == '"') {
			*out++ = '&';
			*out++ = 'q';
			*out++ = 'u';
			*out++ = 'o';
			*out++ = 't';
			*out++ = ';';
		} else if (((*cur >= 0x20) && (*cur < 0x80))
			   || (*cur == '\n') || (*cur == '\r') || (*cur == '\t')) {
			/* Default case, just copy. */
			*out++ = *cur;
		} else {
			char buf[10], *ptr;

			g_snprintf(buf, 9, "&#%d;", *cur);

			ptr = buf;
			while (*ptr != 0)
				*out++ = *ptr++;
		}

		/* turn newlines into <br> */
		if (*cur == '\n') {
			*out++ = '<';
			*out++ = 'b';
			*out++ = 'r';
			*out++ = '>';				
		}
	

		cur++;
		count++;
	}

	*out = 0;
	*encoded_len_return = out - buffer;

	return buffer;
}


static void
write_field_to_stream (const gchar* description, const gchar* value,
		       CamelStream *stream)
{
	gchar *s;
	guint ev_length;
	gchar* encoded_value = value?text_to_html (
		value, strlen(value), &ev_length):"";
	int i;
	for (i = 0; i < strlen (value); i++)
		if (!isprint(encoded_value[i]))
			encoded_value[i] = 'Z';
	
	g_assert (description && value);
	
	s = g_strdup_printf ("<b>%s</b>: %s<br>\n",
			      description, encoded_value);

	camel_stream_write_string (stream, s);
	g_free (encoded_value);
	g_free (s);
}


static void
write_recipients_to_stream (const gchar* recipient_type,
			    const GList* recipients,
			    CamelStream* stream)
{
	/* list of recipients, like "elvis@graceland; bart@springfield" */
	gchar *recipients_string = NULL;
	g_assert (recipient_type && stream);
	
	/* Write out each recipient of 'recipient_type' to the stream */
 	while (recipients) {
		gchar *old_string = recipients_string;
		recipients_string = g_strdup_printf (
			"%s%s%s",
			old_string?old_string:"",
			old_string?"; ":"",
			(gchar*)recipients->data);

		g_free (old_string);
		
		recipients = recipients->next;
	}
	write_field_to_stream (recipient_type, recipients_string, stream);

	g_free (recipients_string);
	camel_stream_write_string (stream, "<br><br>\n");	
}



static void
write_header_info_to_stream (CamelMimeMessage* mime_message,
			     CamelStream* stream)
{
	gchar *s = NULL;
	const GList *recipients = NULL;

	g_assert (mime_message && stream);

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

/* case-insensitive string comparison */
static gint
strcase_equal (gconstpointer v, gconstpointer v2)
{
	return g_strcasecmp ((const gchar*) v, (const gchar*)v2) == 0;
}

static void
str_tolower (gchar* str)
{
	int i;
	int len = strlen (str);
	
	for (i = 0; i < len; i++) {
		str[i] = tolower (str[i]);
	}
}

 
#define MIME_TYPE_WHOLE(a)  (gmime_content_field_get_mime_type ( \
                                      camel_mime_part_get_content_type (CAMEL_MIME_PART (a))))
#define MIME_TYPE_MAIN(a)  ((camel_mime_part_get_content_type (CAMEL_MIME_PART (a)))->type)
#define MIME_TYPE_SUB(a)   ((camel_mime_part_get_content_type (CAMEL_MIME_PART (a)))->subtype)


/*----------------------------------------------------------------------*
 *                     Mime handling functions
 *----------------------------------------------------------------------*/

static void
handle_text_plain (CamelFormatter *formatter, CamelDataWrapper *wrapper)
{
	CamelSimpleDataWrapper* simple_data_wrapper;
	gchar* text;

	debug ("handle_text_plain: entered\n");

	/* Plain text is embodied in a CamelSimpleDataWrapper */
	g_assert (CAMEL_IS_SIMPLE_DATA_WRAPPER (wrapper));
	simple_data_wrapper = CAMEL_SIMPLE_DATA_WRAPPER (wrapper);

	camel_stream_write_string (formatter->priv->stream,
				   "\n<!-- text/plain below -->\n");

	/* If there's any text, write it to the stream */
	if (simple_data_wrapper->byte_array->len != 0) {

		int returned_strlen;

		g_assert (simple_data_wrapper->byte_array->data);

		/* replace '<' with '&lt;', etc. */
		text = text_to_html (simple_data_wrapper->byte_array->data,
				     simple_data_wrapper->byte_array->len,
				     &returned_strlen);
					
		camel_stream_write_string (formatter->priv->stream, text);
		g_free (text);
	}
	else 
		debug ("Warning: handle_text_plain: length of byte array is zero!\n");

	debug ("handle_text_plain: exiting\n");
}

static void
handle_text_html (CamelFormatter *formatter, CamelDataWrapper *wrapper)
{
	CamelSimpleDataWrapper* simple_data_wrapper;
	gchar* text;
	
	debug ("handle_text_html: entered\n");

        /* text is embodied in a CamelSimpleDataWrapper */
	g_assert (CAMEL_IS_SIMPLE_DATA_WRAPPER (wrapper));
	simple_data_wrapper = CAMEL_SIMPLE_DATA_WRAPPER (wrapper);	

	/* If there's any text, write it to the stream */
	if (simple_data_wrapper->byte_array->len != 0) {

		int returned_strlen;

		g_assert (simple_data_wrapper->byte_array->data);

		/* replace '<' with '&lt;', etc. */
		text = g_strndup (simple_data_wrapper->byte_array->data,
				  simple_data_wrapper->byte_array->len);

		camel_stream_write_string (formatter->priv->stream,
					   "\n<!-- text/html below -->\n");
		camel_stream_write_string (formatter->priv->stream, text);

		g_free (text);
	}
	else 
		debug ("Warning: handle_text_html: length of byte array is zero!\n");

	debug ("handle_text_html: exiting\n");		
}

static void
handle_image (CamelFormatter *formatter, CamelDataWrapper *wrapper)
{
	gchar* uuid;
	gchar* tag;
	
	debug ("handle_image: entered\n");

	uuid = lookup_unique_id (formatter->priv->current_root, wrapper);
	
	tag = g_strdup_printf ("<img src=\"%s\">\n", uuid);
	camel_stream_write_string (formatter->priv->stream, tag);

	g_free (uuid);
	g_free (tag);		
		
	debug ("handle_image: exiting\n");	
}

static void
handle_vcard (CamelFormatter *formatter, CamelDataWrapper *wrapper)
{
	gchar* vcard;
	debug ("handle_vcard: entered\n");

	camel_stream_write_string (formatter->priv->stream,
				   "\n<!-- image below -->\n");	
//	camel_stream_write_string (formatter->priv->stream, vcard);
//	g_free (vcard);
		
	debug ("handle_vcard: exiting\n");	
}

static void
handle_mime_message (CamelFormatter *formatter,
		     CamelDataWrapper *wrapper)
{
	CamelMimeMessage* mime_message; 
	CamelDataWrapper* message_contents; 

	g_assert (formatter);
	g_assert (wrapper);
	g_assert (CAMEL_IS_MIME_MESSAGE (wrapper));
	
	mime_message = CAMEL_MIME_MESSAGE (wrapper);
	message_contents =
		camel_medium_get_content_object (CAMEL_MEDIUM (mime_message));
	
	g_assert (message_contents);
	
	debug ("handle_mime_message: entered\n");
	camel_stream_write_string (formatter->priv->stream,
				   "\n<!-- mime message below -->\n");
	
//	camel_stream_write_string (formatter->priv->stream,
//				   "<table width=95% border=1><tr><td>\n\n");		

	/* dispatch the correct handler function for the mime type */
	call_handler_function (formatter, message_contents,
			       MIME_TYPE_WHOLE (mime_message),
			       MIME_TYPE_MAIN (mime_message));
	
	/* close up the table we opened */
//	camel_stream_write_string (formatter->priv->stream,
//				   "\n\n</td></tr></table>\n\n");
	
	debug ("handle_mime_message: exiting\n");
}


/*
 * multipart-alternative helper function --
 * returns NULL if no text/html or text/plan msg is found
 */
static CamelMimePart*
find_preferred_displayable_body_part_in_multipart_alternative (
	CamelMultipart* multipart)
{
	int i, max_multiparts;
	CamelMimePart* html_part = NULL;
	CamelMimePart* plain_part = NULL;	

	/* find out out many parts are in it...*/
	max_multiparts = camel_multipart_get_number (multipart);

        /* TODO: DO LEAF-LOOKUP HERE FOR OTHER MIME-TYPES!!! */

	for (i = 0; i < max_multiparts; i++) {
		CamelMimeBodyPart* body_part = camel_multipart_get_part (multipart, i);

		if (!strcase_equal (MIME_TYPE_MAIN (body_part), "text"))
			continue;
		
		if (strcase_equal (MIME_TYPE_SUB (body_part), "plain")) {
			plain_part = CAMEL_MIME_PART (body_part);
		}
		else if (strcase_equal (MIME_TYPE_SUB (body_part), "html")) {
			html_part = CAMEL_MIME_PART (body_part);
		}
	}

	if (html_part)
		return html_part;
	if (plain_part)
		return plain_part;
	return NULL;
}


/* called for each body part in a multipart/mixed */
static void
print_camel_body_part (CamelMimeBodyPart* body_part,
		       CamelFormatter* formatter,
		       gboolean* text_printed_yet)
{
	CamelDataWrapper* contents =
		camel_medium_get_content_object (CAMEL_MEDIUM (body_part));
	gboolean is_text =
		strcase_equal (MIME_TYPE_MAIN (body_part), "text");

	if (is_text && *text_printed_yet)
		return;

	call_handler_function (formatter, contents, MIME_TYPE_WHOLE (body_part),
			       MIME_TYPE_MAIN (body_part));
	camel_stream_write_string (formatter->priv->stream, "\n<hr>\n");
}



/* Our policy here is this:
   (1) print text/(plain|html) parts found
   (2) print vcards and images inline
   (3) treat all other parts as attachments */
static void
handle_multipart_mixed (CamelFormatter *formatter,
			CamelDataWrapper *wrapper)
{
	CamelMultipart* mp;
	gboolean text_printed_yet = FALSE;

	g_assert (formatter);
	g_assert (wrapper);
	g_assert (CAMEL_IS_MULTIPART (wrapper));
	
	mp = CAMEL_MULTIPART (wrapper);
	g_assert (mp);
	
//	debug ("handle_multipart_mixed: entered\n");


	{
		int i, max_multiparts;

		max_multiparts = camel_multipart_get_number (mp);	
		for (i = 0; i < max_multiparts; i++) {
			CamelMimeBodyPart* body_part =
				camel_multipart_get_part (mp, i);

			print_camel_body_part (body_part, formatter, &text_printed_yet);
		}
	}


//	debug ("handle_multipart_mixed: exiting\n");	
}

static void
handle_multipart_related (CamelFormatter *formatter,
			  CamelDataWrapper *wrapper)
{
	CamelMultipart* mp = CAMEL_MULTIPART (wrapper);
	debug ("handle_multipart_related: entered\n");

	debug ("handle_multipart_related: NYI!!\n");
	
	/* TODO: read RFC, in terms of how a one message
	         may refer to another object */	

	debug ("handle_multipart_related: exiting\n");	
}

/*
   The current policy for multipart/alternative is this: 

   if (we find a text/html body part)
       we print it
   else if (we find a text/plain body part)
       we print it
   else
       we print nothing
*/
static void
handle_multipart_alternative (CamelFormatter *formatter,
			    CamelDataWrapper *wrapper)
{
	CamelMultipart* multipart = CAMEL_MULTIPART (wrapper);
	CamelMimePart* mime_part;
	
	debug ("handle_multipart_alternative: entered\n");

	mime_part = find_preferred_displayable_body_part_in_multipart_alternative(
		multipart);	
	if (mime_part) {

		CamelDataWrapper* contents =
			camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

		call_handler_function (formatter, contents,
				       MIME_TYPE_WHOLE (mime_part),
				       MIME_TYPE_MAIN (mime_part));
	}
	
	debug ("handle_multipart_alternative: exiting\n");		
}

static void
handle_unknown_type (CamelFormatter *formatter,
		     CamelDataWrapper *wrapper)
{
	gchar* tag;
	CamelMimeMessage* root = formatter->priv->current_root;
	char* uid = lookup_unique_id (root, wrapper);	

	debug ("handle_unknown_type: entered\n");
	
	tag = g_strdup_printf ("<a href=\"camel://%s\">click-me-to-save</a>\n",
			       uid);

	camel_stream_write_string (formatter->priv->stream, tag);

	debug ("handle_unknown_type: exiting\n");
}

/*----------------------------------------------------------------------*
 *                   Standard Gtk+ class functions
 *----------------------------------------------------------------------*/

CamelFormatter*
camel_formatter_new ()
{
	return (gtk_type_new (CAMEL_FORMATTER_TYPE));
}


static void           
_finalize (GtkObject* object)
{
	CamelFormatter *formatter = CAMEL_FORMATTER (object);

	if (formatter->priv->attachments)
		g_hash_table_destroy (formatter->priv->attachments);
	
	g_free (formatter->priv);
	
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
camel_formatter_class_init (CamelFormatterClass *camel_formatter_class)
{
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_formatter_class);

	parent_class = gtk_type_class (gtk_object_get_type ());

	mime_function_table =
		g_hash_table_new (g_str_hash, strcase_equal);

#define ADD_HANDLER(a,b) g_hash_table_insert (mime_function_table, a, b)

	/* hook up mime types to functions that handle them */
	ADD_HANDLER ("text/plain", handle_text_plain);
	ADD_HANDLER ("text/html", handle_text_html);
	ADD_HANDLER ("multipart/alternative", handle_multipart_alternative);
	ADD_HANDLER ("multipart/related", handle_multipart_related);
	ADD_HANDLER ("multipart/mixed", handle_multipart_mixed);	
	ADD_HANDLER ("message/rfc822", handle_mime_message);
	ADD_HANDLER ("image/", handle_image);
	ADD_HANDLER ("vcard/", handle_vcard);			

        /* virtual method overload */
	gtk_object_class->finalize = _finalize;
}
 

static void
camel_formatter_init (gpointer object, gpointer klass) 
{
	CamelFormatter* cmf = CAMEL_FORMATTER (object);
	cmf->priv = g_new (CamelFormatterPrivate, 1);
	cmf->priv->attachments = NULL;
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






