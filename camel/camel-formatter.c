
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

static void handle_text_plain         (CamelFormatter *formatter,
			               CamelDataWrapper *wrapper,
			               CamelStream *stream);
static void handle_text_html          (CamelFormatter *formatter,
			               CamelDataWrapper *wrapper,
			               CamelStream *stream);
static void handle_image              (CamelFormatter *formatter,
				       CamelDataWrapper *wrapper,
				       CamelStream *stream);
static void handle_mime_message       (CamelFormatter *formatter,
			               CamelDataWrapper *wrapper,
			               CamelStream *stream);
static void handle_multipart_mixed  (CamelFormatter *formatter,
				       CamelDataWrapper *wrapper,
				       CamelStream *stream);
static void handle_multipart_related  (CamelFormatter *formatter,
				       CamelDataWrapper *wrapper,
				       CamelStream *stream);
static void handle_multipart_alternate(CamelFormatter *formatter,
				       CamelDataWrapper *wrapper,
				       CamelStream *stream);
static void handle_unknown_type       (CamelFormatter *formatter,
				       CamelDataWrapper *wrapper,
				       CamelStream *stream);



/* encodes some characters into their 'escaped' version;
 * so '<' turns into '&lt;', and '"' turns into '&quot;' */
static gchar* encode_entities (const guchar *input,
				guint len,
				guint *encoded_len_return);


static GtkObjectClass *parent_class = NULL;

static void _finalize (GtkObject *object);

struct _CamelFormatterPrivate {
	CamelMimeMessage *current_root;
};

static GHashTable *mime_function_table = NULL;

void
debug (const gchar *format,
	 ...)
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
				      CamelStream* stream)
{
	camel_stream_write_string (stream, "<html><body>\n");

	formatter->priv->current_root = mime_message;
	
	handle_mime_message (
		formatter,
		CAMEL_DATA_WRAPPER (mime_message),
		stream);

	camel_stream_write_string (stream, "\n</body></html>\n");	
}

/* we're maintaining a hashtable of mimetypes -> functions;
 * those functions have the following signature...*/
typedef void (*mime_handler_fn) (CamelFormatter *formatter,
				 CamelDataWrapper *data_wrapper,
				 CamelStream *stream);

static gchar*
lookup_unique_id (CamelMimeMessage* root, CamelDataWrapper* child)
{
	return "NYI";
}


/* takes a mimetype, calls a function to handle it */
static void
call_handler_function (CamelFormatter* formatter,
		       CamelDataWrapper* wrapper,
		       gchar* mimetype, CamelStream* stream)
{
	mime_handler_fn handler_function;

	/* try to find a handler function in our own lookup table */
	handler_function = g_hash_table_lookup (
		mime_function_table, mimetype);

	/* If there's no such handler function, try to find bonobo
	 * object to show the object */
	if (!handler_function)
	{
		CamelMimeMessage* root = formatter->priv->current_root;
		char* uid = lookup_unique_id (root, wrapper);
		const char* goad_id = gnome_mime_get_value (
			mimetype, "bonobo-goad_id");

		g_assert (root && uid);

		if (goad_id) {
			char* tag = g_strdup_printf (
				"<object classid=\"%s\" uid=\"%s\">",
				goad_id, uid);
			
			camel_stream_write_string (stream, tag);
		}
		/* we don't know how to show something of this
                   mimetype; punt */
		else {
			debug ("no function or bonobo object found for mimetype \"%s\"\n",
			       mimetype);
			handler_function = handle_unknown_type;
		}
	}
	else {
		(*handler_function)(formatter, wrapper, stream);
	}
}


/*----------------------------------------------------------------------*
 *     Header (ex. "subj:", "from:") helper functions for mime msgs
 *----------------------------------------------------------------------*/

/* This routine was originally written by Daniel Velliard, (C) 1998
   World Wide Web Consortium.

   It will (for example) turn the input 'ab <c>' into 'ab &lt;c&gt;' */
static gchar *
encode_entities (const guchar *input,
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
	gchar* encoded_value = value?encode_entities (
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
handle_text_plain (CamelFormatter *formatter, CamelDataWrapper *wrapper,
		   CamelStream *stream)
{
	CamelSimpleDataWrapper* simple_data_wrapper;
	gchar* text;

	debug ("handle_text_plain: entered\n");
	
	g_assert (CAMEL_IS_SIMPLE_DATA_WRAPPER (wrapper));
	simple_data_wrapper = CAMEL_SIMPLE_DATA_WRAPPER (wrapper);

//	camel_simple_data_wrapper_set_text (
//		simple_data_wrapper, "hello world");
	if (simple_data_wrapper->byte_array->len != 0) {
		debug ("yay, simple_data_wrapper->byte_array->len != 0\n");
		g_assert (simple_data_wrapper->byte_array->data);

		text = g_strndup (simple_data_wrapper->byte_array->data,
				  simple_data_wrapper->byte_array->len);
		camel_stream_write_string (stream, text);
		g_free (text);
	}
	else {
		debug ("boo, simple_data_wrapper->byte_array->len == 0\n");
	}

	debug ("handle_text_plain: exiting\n");
}

static void
handle_text_html (CamelFormatter *formatter, CamelDataWrapper *wrapper,
		  CamelStream *stream)
{
	debug ("handle_text_html: entered\n");

	/* TODO: replace 'body' tag with 'table' tag; delete
           everything prior to the 'body' tag */

	debug ("handle_text_html: exiting\n");		
}

static void
handle_image (CamelFormatter *formatter, CamelDataWrapper *wrapper,
	      CamelStream *stream)
{
	debug ("handle_image: entered\n");

	
	
	debug ("handle_image: exiting\n");	
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

	debug ("handle_mime_message: entered\n");	
	camel_stream_write_string (stream, "<table width=95% border=1><tr><td>\n\n");		

	/* write the subj:, to:, from: etc. fields out as html */
	write_header_info_to_stream (mime_message, stream);

	/* dispatch the correct handler function for the mime type */
	call_handler_function (formatter, message_contents,
			       MIME_TYPE_WHOLE (mime_message), stream);
	
	/* close up the table we opened */
	camel_stream_write_string (stream, "\n\n</td></tr></table>\n\n");
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

        /* ...and write each one, as html, into the stream. */
	for (i = 0; i < max_multiparts; i++) {
		CamelMimeBodyPart* body_part = camel_multipart_get_part (multipart, i);
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


static void
handle_multipart_mixed (CamelFormatter *formatter,
			CamelDataWrapper *wrapper,
			CamelStream *stream)
{
	CamelMultipart* mp = CAMEL_MULTIPART (wrapper);
	debug ("handle_multipart_mixed: entered\n");
	debug ("handle_multipart_mixed: exiting\n");	
}

static void
handle_multipart_related (CamelFormatter *formatter,
			  CamelDataWrapper *wrapper,
			  CamelStream *stream)
{
	CamelMultipart* mp = CAMEL_MULTIPART (wrapper);
	debug ("handle_multipart_related: entered\n");
	debug ("handle_multipart_related: exiting\n");	
}

static void
handle_multipart_alternate (CamelFormatter *formatter,
			    CamelDataWrapper *wrapper,
			    CamelStream *stream)
{
	CamelMultipart* mp = CAMEL_MULTIPART (wrapper);
	debug ("handle_multipart_alternate: entered\n");
	debug ("handle_multipart_alternate: exiting\n");		
}

static void
handle_unknown_type (CamelFormatter *formatter,
		     CamelDataWrapper *wrapper,
		     CamelStream *stream)
{
	debug ("handle_unknown_type: entered\n");
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
camel_formatter_class_init (CamelFormatterClass *camel_formatter_class)
{
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_formatter_class);

	parent_class = gtk_type_class (gtk_object_get_type ());

	mime_function_table = g_hash_table_new (g_str_hash, strcase_equal);

#define ADD_HANDLER(a,b) g_hash_table_insert (mime_function_table, a, b)

	/* hook up mime types to functions that handle them */
	ADD_HANDLER ("text/plain", handle_text_plain);
	ADD_HANDLER ("text/html", handle_text_html);
	ADD_HANDLER ("multipart/alternate", handle_multipart_alternate);
	ADD_HANDLER ("multipart/related", handle_multipart_related);
	ADD_HANDLER ("multipart/related", handle_multipart_mixed);	
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
_finalize (GtkObject* object)
{
	CamelFormatter *formatter = CAMEL_FORMATTER (object);

	g_free (formatter->priv);
	
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}


/* GARBAGE GARBAGE GARBAGE GARBAGE GARBAGE GARBAGE */
/* GARBAGE GARBAGE GARBAGE GARBAGE GARBAGE GARBAGE */
/* GARBAGE GARBAGE GARBAGE GARBAGE GARBAGE GARBAGE */

/* Converts the contents of a CamelMimePart into HTML */
static void
mime_part_to_html (CamelFormatter* formatter, CamelMimePart* part,
		   CamelStream* stream)
{
	/* Get the mime-type of the mime message */
	gchar* mime_type_whole = /* ex. "text/plain" */
		MIME_TYPE_WHOLE (part);

	/* get the contents of the mime message */
	CamelDataWrapper* message_contents =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));

	/* if we're dealing with a multipart/related message... */
	if (strcase_equal (MIME_TYPE_WHOLE (part), "multipart/related")) {

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
		if (strcase_equal (mime_type_whole, "multipart/alternate")) {
			mime_part =
				find_preferred_displayable_body_part_in_multipart_alternative (
					CAMEL_MULTIPART(message_contents));
		}

		else if (strcase_equal (mime_type_whole, "text/plain") ||
			 strcase_equal (mime_type_whole, "text/html")) {

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
