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
#include "mail-display.h"
#include "camel/hash-table-utils.h"

#include <libgnome/libgnome.h>
#include <bonobo.h>
#include <libgnorba/gnorba.h>
#include <bonobo/bonobo-stream-memory.h>

#include <ctype.h>    /* for isprint */
#include <string.h>   /* for strstr  */
#include <fcntl.h>

static void handle_text_plain            (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);
static void handle_text_enriched         (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);
static void handle_text_html             (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);
static void handle_image                 (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);
static void handle_multipart_mixed       (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);
static void handle_multipart_related     (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);
static void handle_multipart_alternative (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);
static void handle_multipart_appledouble (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);
static void handle_audio                 (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);
static void handle_message_rfc822        (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);

static void handle_unknown_type          (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);
static void handle_via_bonobo            (CamelMimePart *part,
					  CamelMimeMessage *root,
					  GtkBox *box);

/* encodes some characters into their 'escaped' version;
 * so '<' turns into '&lt;', and '"' turns into '&quot;' */
static gchar *text_to_html (const guchar *input, guint len,
			    guint *encoded_len_return, gboolean add_pre,
			    gboolean convert_newlines_to_br);

/* writes the header info for a mime message into an html stream */
static void write_headers (CamelMimeMessage *mime_message, GtkBox *box);

/* dispatch html printing via mimetype */
static void call_handler_function (CamelMimePart *part,
				   CamelMimeMessage *root,
				   GtkBox *box);



/**
 * mail_format_mime_message: 
 * @mime_message: the input mime message
 * @box: GtkBox to stack elements into.
 *
 * Writes a CamelMimeMessage out, as a series of GtkHTML objects,
 * into the provided box.
 **/
void
mail_format_mime_message (CamelMimeMessage *mime_message, GtkBox *box)
{
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (mime_message));
	g_return_if_fail (GTK_IS_BOX (box));

	write_headers (mime_message, box);
	call_handler_function (CAMEL_MIME_PART (mime_message),
			       mime_message, box);
}

static char *
get_cid (CamelMimePart *part, CamelMimeMessage *root)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	char *cid;

	/* If we have a real Content-ID, use it. If we don't,
	 * make a (syntactically invalid) fake one.
	 */
	if (camel_mime_part_get_content_id (part))
		cid = g_strdup (camel_mime_part_get_content_id (part));
	else
		cid = g_strdup_printf ("@@@%p", wrapper);

	gtk_object_set_data (GTK_OBJECT (root), cid, wrapper);
	return cid;
}


/* We're maintaining a hashtable of mimetypes -> functions;
 * Those functions have the following signature...
 */
typedef void (*mime_handler_fn) (CamelMimePart *part, CamelMimeMessage *root, GtkBox *box);

static GHashTable *mime_function_table, *mime_fallback_table;
static GHashTable *mime_icon_table;

static void
setup_function_table (void)
{
	mime_function_table = g_hash_table_new (g_str_hash, g_str_equal);
	mime_fallback_table = g_hash_table_new (g_str_hash, g_str_equal);

	g_hash_table_insert (mime_function_table, "text/plain",
			     handle_text_plain);
	g_hash_table_insert (mime_function_table, "text/richtext",
			     handle_text_enriched);
	g_hash_table_insert (mime_function_table, "text/enriched",
			     handle_text_enriched);
	g_hash_table_insert (mime_function_table, "text/html",
			     handle_text_html);

	g_hash_table_insert (mime_function_table, "image/*",
			     handle_image);

	g_hash_table_insert (mime_function_table, "audio/*",
			     handle_audio);

	g_hash_table_insert (mime_function_table, "message/rfc822",
			     handle_message_rfc822);

	g_hash_table_insert (mime_function_table, "multipart/alternative",
			     handle_multipart_alternative);
	g_hash_table_insert (mime_function_table, "multipart/related",
			     handle_multipart_related);
	g_hash_table_insert (mime_function_table, "multipart/mixed",
			     handle_multipart_mixed);
	g_hash_table_insert (mime_function_table, "multipart/appledouble",
			     handle_multipart_appledouble);

	/* RFC 2046 says unrecognized text subtypes can be treated
	 * as text/plain (as long as you recognize the character set),
	 * and unrecognized multipart subtypes as multipart/mixed.
	 */
	g_hash_table_insert (mime_fallback_table, "text/*",
			     handle_text_plain);
	g_hash_table_insert (mime_function_table, "multipart/*",
			     handle_multipart_mixed);
}

static CamelStream *
icon_stream (char *path)
{
	GByteArray *ba;
	char buf[8192];
	int fd, nread;

	fd = open (path, O_RDONLY);
	if (fd == -1)
		return NULL;

	ba = g_byte_array_new ();
	while (1) {
		nread = read (fd, buf, sizeof (buf));
		if (nread < 1)
			break;
		g_byte_array_append (ba, buf, nread);
	}
	close (fd);
	return camel_stream_mem_new_with_byte_array (ba, CAMEL_STREAM_MEM_READ);
}

static void
setup_icon_table (void)
{
	char *path;

	mime_icon_table = g_hash_table_new (g_strcase_hash,
					    g_strcase_equal);

	path = gnome_pixmap_file ("gnome-audio2.png");
	if (path) {
		g_hash_table_insert (mime_icon_table, "audio",
				     icon_stream (path));
		g_free (path);
	}
	path = gnome_pixmap_file ("gnome-graphics.png");
	if (path) {
		g_hash_table_insert (mime_icon_table, "image",
				     icon_stream (path));
		g_free (path);
	}
	path = gnome_pixmap_file ("gnome-qeye.png");
	if (path) {
		g_hash_table_insert (mime_icon_table, "video",
				     icon_stream (path));
		g_free (path);
	}
	path = gnome_pixmap_file ("gnome-question.png");
	if (path) {
		g_hash_table_insert (mime_icon_table, "unknown",
				     icon_stream (path));
		g_free (path);
	}
}

static mime_handler_fn
lookup_handler (CamelMimePart *part, gboolean *generic)
{
	CamelDataWrapper *wrapper;
	mime_handler_fn handler_function;
	const char *whole_goad_id, *generic_goad_id;
	char *mimetype_whole = NULL;
	char *mimetype_main = NULL;

	g_return_val_if_fail (CAMEL_IS_MIME_PART (part), NULL);

	if (mime_function_table == NULL) {
		setup_function_table ();
		setup_icon_table ();
	}

	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));

	/* OK. There are 6 possibilities, which we try in this order:
	 *   1) full match in the main table
	 *   2) partial match in the main table
	 *   3) full match in bonobo
	 *   4) full match in the fallback table
	 *   5) partial match in the fallback table
	 *   6) partial match in bonobo
	 *
	 * Of these, 1-4 are considered exact matches, and 5 and 6 are
	 * considered generic.
	 */

	/* Check for full match in mime_function_table. */
	mimetype_whole = camel_data_wrapper_get_mime_type (wrapper);
	g_strdown (mimetype_whole);
	mimetype_main = g_strdup_printf ("%s/*", wrapper->mime_type->type);
	g_strdown (mimetype_main);

	handler_function = g_hash_table_lookup (mime_function_table,
						mimetype_whole);
	if (!handler_function) {
		handler_function = g_hash_table_lookup (mime_function_table,
							mimetype_main);
		if (handler_function) {
			/* Optimize this for the next time through. */
			g_hash_table_insert (mime_function_table,
					     g_strdup (mimetype_whole),
					     handler_function);
		}
	}

	if (handler_function) {
		g_free (mimetype_whole);
		g_free (mimetype_main);
		*generic = FALSE;
		return handler_function;
	}

	whole_goad_id = gnome_mime_get_value (mimetype_whole,
					      "bonobo-goad-id");
	generic_goad_id = gnome_mime_get_value (mimetype_main,
						"bonobo-goad-id");

	if (whole_goad_id && (!generic_goad_id ||
			      strcmp (whole_goad_id, generic_goad_id) != 0)) {
		/* Optimize this for the next time through. */
		g_hash_table_insert (mime_function_table,
				     mimetype_whole,
				     handle_via_bonobo);
		g_free (mimetype_main);
		*generic = FALSE;
		return handle_via_bonobo;
	}

	handler_function = g_hash_table_lookup (mime_fallback_table,
						mimetype_whole);
	g_free (mimetype_whole);
	if (handler_function)
		*generic = FALSE;
	else {
		handler_function = g_hash_table_lookup (mime_fallback_table,
							mimetype_main);
		if (!handler_function && generic_goad_id)
			handler_function = handle_via_bonobo;
		*generic = TRUE;
	}
	g_free (mimetype_main);
	return handler_function;
}

static void
call_handler_function (CamelMimePart *part, CamelMimeMessage *root,
		       GtkBox *box)
{
	CamelDataWrapper *wrapper;
	mime_handler_fn handler_function = NULL;
	gboolean generic;

	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));

	handler_function = lookup_handler (part, &generic);

	if (handler_function)
		(*handler_function) (part, root, box);
	else
		handle_unknown_type (part, root, box);
}


/* Convert plain text into equivalent-looking valid HTML. */
static gchar *
text_to_html (const guchar *input, guint len,
	      guint *encoded_len_return, gboolean add_pre,
	      gboolean convert_newlines_to_br)
{
	const guchar *cur = input;
	guchar *buffer = NULL;
	guchar *out = NULL;
	gint buffer_size = 0;

	/* Allocate a translation buffer.  */
	buffer_size = len * 2 + 5;
	buffer = g_malloc (buffer_size);

	out = buffer;
	if (add_pre)
		out += sprintf (out, "<PRE>\n");

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

	if (add_pre)
		strcpy (out, "</PRE>");
	else
		*out = '\0';
	if (encoded_len_return)
		*encoded_len_return = out - buffer;

	return buffer;
}

static void
write_field_to_stream (const char *description, const char *value,
		       gboolean bold, GtkHTML *html,
		       GtkHTMLStreamHandle *stream)
{
	char *encoded_value;

	if (value) {
		unsigned char *p;

		encoded_value = text_to_html (value, strlen(value),
					      NULL, FALSE, TRUE);
		for (p = (unsigned char *)encoded_value; *p; p++) {
			if (!isprint (*p))
				*p = '?';
		}
	} else
		encoded_value = "";

	mail_html_write (html, stream,
			 "<tr valign=top><%s align=right>%s</%s>"
			 "<td>%s</td></tr>", bold ? "th" : "td",
			 description, bold ? "th" : "td", encoded_value);
	if (value)
		g_free (encoded_value);
}

static void
write_recipients_to_stream (const gchar *recipient_type,
			    const GList *recipients, gboolean bold,
			    GtkHTML *html,
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

	write_field_to_stream (recipient_type, recipients_string,
			       bold, html, stream);
	g_free (recipients_string);
}



static void
write_headers (CamelMimeMessage *mime_message, GtkBox *box)
{
	const GList *recipients;
	GtkHTML *html;
	GtkHTMLStreamHandle *stream;

	mail_html_new (&html, &stream, mime_message, FALSE);
	mail_html_write (html, stream, "%s%s", HTML_HEADER,
			 "<BODY TEXT=\"#000000\" BGCOLOR=\"#EEEEEE\">\n");

	mail_html_write (html, stream, "<table>");

	/* A few fields will probably be available from the mime_message;
	 * for each one that's available, write it to the output stream
	 * with a helper function, 'write_field_to_stream'.
	 */

	write_field_to_stream ("From:",
			       camel_mime_message_get_from (mime_message),
			       TRUE, html, stream);

	if (camel_mime_message_get_reply_to (mime_message)) {
		write_field_to_stream ("Reply-To:",
				       camel_mime_message_get_reply_to (mime_message),
				       FALSE, html, stream);
	}

	write_recipients_to_stream ("To:",
				    camel_mime_message_get_recipients (mime_message, CAMEL_RECIPIENT_TYPE_TO),
				    TRUE, html, stream);

	recipients = camel_mime_message_get_recipients (mime_message, CAMEL_RECIPIENT_TYPE_CC);
	if (recipients)
		write_recipients_to_stream ("Cc:", recipients, TRUE, html, stream);
	write_field_to_stream ("Subject:",
			       camel_mime_message_get_subject (mime_message),
			       TRUE, html, stream);

	mail_html_write (html, stream, "</table>");

	mail_html_end (html, stream, TRUE, box);
}

#define MIME_TYPE_WHOLE(a)  (gmime_content_field_get_mime_type ( \
                                      camel_mime_part_get_content_type (CAMEL_MIME_PART (a))))
#define MIME_TYPE_MAIN(a)  ((camel_mime_part_get_content_type (CAMEL_MIME_PART (a)))->type)
#define MIME_TYPE_SUB(a)   ((camel_mime_part_get_content_type (CAMEL_MIME_PART (a)))->subtype)


/*----------------------------------------------------------------------*
 *                     Mime handling functions
 *----------------------------------------------------------------------*/

static void
handle_text_plain (CamelMimePart *part, CamelMimeMessage *root, GtkBox *box)
{
	GtkHTML *html;
	GtkHTMLStreamHandle *stream;
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	char *text;
	CamelStream *wrapper_output_stream;
	gchar tmp_buffer[4096];
	gint nb_bytes_read;
	gboolean empty_text = TRUE;

	mail_html_new (&html, &stream, root, TRUE);
	mail_html_write (html, stream, "\n<!-- text/plain -->\n<pre>\n");

	/* Get the output stream of the data wrapper. */
	wrapper_output_stream = camel_data_wrapper_get_output_stream (wrapper);
	camel_stream_reset (wrapper_output_stream);

	do {
		/* Read next chunk of text. */
		nb_bytes_read = camel_stream_read (wrapper_output_stream,
						   tmp_buffer,
						   sizeof (tmp_buffer));

		/* If there's any text, write it to the stream. */
		if (nb_bytes_read > 0) {
			int returned_strlen;

			empty_text = FALSE;

			/* replace '<' with '&lt;', etc. */
			text = text_to_html (tmp_buffer,
					     nb_bytes_read,
					     &returned_strlen,
					     FALSE, FALSE);
			gtk_html_write (html, stream, text,
					returned_strlen);
			g_free (text);
		}
	} while (!camel_stream_eos (wrapper_output_stream));

	if (empty_text)
		mail_html_write (html, stream, "<b>(empty)</b>");

	mail_html_write (html, stream, "</pre>\n");
	mail_html_end (html, stream, TRUE, box);
}

/* text/enriched (RFC 1896) or text/richtext (included in RFC 1341) */
static void
handle_text_enriched (CamelMimePart *part, CamelMimeMessage *root, GtkBox *box)
{
	static GHashTable *translations = NULL;
	GtkHTML *html;
	GtkHTMLStreamHandle *stream;
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelStream *memstream;
	GByteArray *ba;
	char *p;
	int len, nofill = 0;

	if (!translations) {
		translations = g_hash_table_new (g_strcase_hash,
						 g_strcase_equal);
		g_hash_table_insert (translations, "bold", "<b>");
		g_hash_table_insert (translations, "/bold", "</b>");
		g_hash_table_insert (translations, "italic", "<i>");
		g_hash_table_insert (translations, "/italic", "</i>");
		g_hash_table_insert (translations, "fixed", "<tt>");
		g_hash_table_insert (translations, "/fixed", "</tt>");
		g_hash_table_insert (translations, "smaller", "<font size=-1>");
		g_hash_table_insert (translations, "/smaller", "</font>");
		g_hash_table_insert (translations, "bigger", "<font size=+1>");
		g_hash_table_insert (translations, "/bigger", "</font>");
		g_hash_table_insert (translations, "underline", "<u>");
		g_hash_table_insert (translations, "/underline", "</u>");
		g_hash_table_insert (translations, "center", "<p align=center>");
		g_hash_table_insert (translations, "/center", "</p>");
		g_hash_table_insert (translations, "flushleft", "<p align=left>");
		g_hash_table_insert (translations, "/flushleft", "</p>");
		g_hash_table_insert (translations, "flushright", "<p align=right>");
		g_hash_table_insert (translations, "/flushright", "</p>");
		g_hash_table_insert (translations, "excerpt", "<blockquote>");
		g_hash_table_insert (translations, "/excerpt", "</blockquote>");
		g_hash_table_insert (translations, "paragraph", "<p>");
		g_hash_table_insert (translations, "signature", "<address>");
		g_hash_table_insert (translations, "/signature", "</address>");
		g_hash_table_insert (translations, "comment", "<!-- ");
		g_hash_table_insert (translations, "/comment", " -->");
		g_hash_table_insert (translations, "param", "<!-- ");
		g_hash_table_insert (translations, "/param", " -->");
		g_hash_table_insert (translations, "nl", "<br>");
		g_hash_table_insert (translations, "np", "<hr>");
	}

	mail_html_new (&html, &stream, root, TRUE);
	mail_html_write (html, stream, "\n<!-- text/enriched -->\n");

	ba = g_byte_array_new ();
	memstream = camel_stream_mem_new_with_byte_array (ba, CAMEL_STREAM_MEM_WRITE);
	camel_data_wrapper_write_to_stream (wrapper, memstream);
	g_byte_array_append (ba, "", 1);

	p = ba->data;

	while (p) {
		len = strcspn (p, " <>&\n");
		if (len)
			gtk_html_write (html, stream, p, len);

		p += len;
		if (!*p)
			break;

		switch (*p++) {
		case ' ':
			while (*p == ' ') {
				mail_html_write (html, stream, "&nbsp;");
				p++;
			}
			mail_html_write (html, stream, " ");
			break;

		case '\n':
			mail_html_write (html, stream, " ");
			if (nofill <= 0) {
				while (*p == '\n') {
					mail_html_write (html, stream, "<br>");
					p++;
				}
			}
			break;

		case '>':
			mail_html_write (html, stream, "&gt;");
			break;

		case '&':
			mail_html_write (html, stream, "&amp;");
			break;

		case '<':
			if (*p == '<') {
				mail_html_write (html, stream, "&lt;");
				break;
			}

			if (strncmp (p, "lt>", 3) == 0)
				mail_html_write (html, stream, "&lt;");
			else if (strncmp (p, "nofill>", 7) == 0) {
				nofill++;
				mail_html_write (html, stream, "<pre>");
			} else if (strncmp (p, "/nofill>", 8) == 0) {
				nofill--;
				mail_html_write (html, stream, "</pre>");
			} else {
				char *copy, *match;

				len = strcspn (p, ">");
				copy = g_strndup (p, len);
				match = g_hash_table_lookup (translations,
							     copy);
				g_free (copy);
				if (match)
					mail_html_write (html, stream, match);
			}

			p = strchr (p, '>');
			if (p)
				p++;
		}
	}

	mail_html_end (html, stream, TRUE, box);
}

static void
handle_text_html (CamelMimePart *part, CamelMimeMessage *root, GtkBox *box)
{
	GtkHTML *html;
	GtkHTMLStreamHandle *stream;
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelStream *wrapper_output_stream;
	gchar tmp_buffer[4096];
	gint nb_bytes_read;
	gboolean empty_text = TRUE;

	mail_html_new (&html, &stream, root, FALSE);
	mail_html_write (html, stream, "\n<!-- text/html -->\n");

	/* Get the output stream of the data wrapper. */
	wrapper_output_stream = camel_data_wrapper_get_output_stream (wrapper);
	camel_stream_reset (wrapper_output_stream);

	do {
		/* Read next chunk of text. */
		nb_bytes_read = camel_stream_read (wrapper_output_stream,
						   tmp_buffer, 4096);

		/* If there's any text, write it to the stream */
		if (nb_bytes_read > 0) {
			empty_text = FALSE;
			
			/* Write the buffer to the html stream */
			gtk_html_write (html, stream, tmp_buffer,
					nb_bytes_read);
		}
	} while (!camel_stream_eos (wrapper_output_stream));

	if (empty_text)
		mail_html_write (html, stream, "<b>(empty)</b>");

	mail_html_end (html, stream, FALSE, box);
}

static void
handle_image (CamelMimePart *part, CamelMimeMessage *root, GtkBox *box)
{
	GtkHTML *html;
	GtkHTMLStreamHandle *stream;
	char *cid;

	cid = get_cid (part, root);
	mail_html_new (&html, &stream, root, TRUE);
	mail_html_write (html, stream, "<img src=\"cid:%s\">", cid);
	mail_html_end (html, stream, TRUE, box);
	g_free (cid);
}

static void
handle_multipart_mixed (CamelMimePart *part, CamelMimeMessage *root, GtkBox *box)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *mp;
	int i, nparts;

	g_return_if_fail (CAMEL_IS_MULTIPART (wrapper));
	mp = CAMEL_MULTIPART (wrapper);

	nparts = camel_multipart_get_number (mp);	
	for (i = 0; i < nparts; i++) {
		CamelMimeBodyPart *body_part =
			camel_multipart_get_part (mp, i);

		call_handler_function (CAMEL_MIME_PART (body_part), root, box);
	}
}

/* As seen in RFC 2387! */
static void
handle_multipart_related (CamelMimePart *part, CamelMimeMessage *root, GtkBox *box)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *mp;
	CamelMimeBodyPart *body_part, *display_part = NULL;
	GMimeContentField *content_type;
	const char *start;
	int i, nparts;

	g_return_if_fail (CAMEL_IS_MULTIPART (wrapper));
	mp = CAMEL_MULTIPART (wrapper);
	nparts = camel_multipart_get_number (mp);	

	content_type = camel_mime_part_get_content_type (part);
	start = gmime_content_field_get_parameter (content_type, "start");
	if (start) {
		int len;

		/* The "start" parameter includes <>s, which Content-Id
		 * does not.
		 */
		len = strlen (start) - 2;

		for (i = 0; i < nparts; i++) {
			const char *cid;

			body_part = camel_multipart_get_part (mp, i);
			cid = camel_mime_part_get_content_id (
				CAMEL_MIME_PART (body_part));

			if (!strncmp (cid, start + 1, len) &&
			    strlen (cid) == len) {
				display_part = body_part;
				break;
			}
		}

		if (!display_part) {
			/* Oops. Hrmph. */
			handle_multipart_mixed (part, root, box);
		}
	} else {
		/* No start parameter, so it defaults to the first part. */
		display_part = camel_multipart_get_part (mp, 0);
	}

	/* Record the Content-IDs of any non-displayed parts. */
	for (i = 0; i < nparts; i++) {
		char *cid;

		body_part = camel_multipart_get_part (mp, i);
		if (body_part == display_part)
			continue;

		part = CAMEL_MIME_PART (body_part);
		cid = get_cid (part, root);
		g_free (cid);
	}

	/* Now, display the displayed part. */
	call_handler_function (CAMEL_MIME_PART (display_part), root, box);
}

/* RFC 2046 says "display the last part that you are able to display". */
static CamelMimePart *
find_preferred_alternative (CamelMultipart *multipart)
{
	int i, nparts;
	CamelMimePart *preferred_part = NULL;
	gboolean generic;

	nparts = camel_multipart_get_number (multipart);
	for (i = 0; i < nparts; i++) {
		CamelMimePart *part = CAMEL_MIME_PART (
			camel_multipart_get_part (multipart, i));

		if (lookup_handler (part, &generic) &&
		    (!preferred_part || !generic))
			preferred_part = part;
	}

	return preferred_part;
}

static void
handle_multipart_alternative (CamelMimePart *part, CamelMimeMessage *root,
			      GtkBox *box)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *multipart;
	CamelMimePart *mime_part;

	g_return_if_fail (CAMEL_IS_MULTIPART (wrapper));
	multipart = CAMEL_MULTIPART (wrapper);

	mime_part = find_preferred_alternative (multipart);	
	if (mime_part)
		call_handler_function (mime_part, root, box);
	else
		handle_unknown_type (part, root, box);
}

/* RFC 1740 */
static void
handle_multipart_appledouble (CamelMimePart *part, CamelMimeMessage *root,
			      GtkBox *box)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *multipart;

	g_return_if_fail (CAMEL_IS_MULTIPART (wrapper));
	multipart = CAMEL_MULTIPART (wrapper);

	/* The first part is application/applefile and is not useful
	 * to us. The second part _may_ be displayable data. Most
	 * likely it's application/octet-stream though.
	 */
	part = CAMEL_MIME_PART (camel_multipart_get_part (multipart, 1));
	call_handler_function (part, root, box);
}

static void
handle_mystery (CamelMimePart *part, CamelMimeMessage *root, GtkBox *box,
		char *icon_cid, char *id, char *action)
{
	GtkHTML *html;
	GtkHTMLStreamHandle *stream;
	char *cid;

	cid = get_cid (part, root);
	mail_html_new (&html, &stream, root, TRUE);
	mail_html_write (html, stream, "<table><tr><td><a href=\"camel:%p\">"
			 "<img src=\"camel:%p\"></a></td>"
			 "<td>%s<br><br>Click on the icon to %s."
			 "</td></tr></table>", get_cid (part, root),
			 icon_cid, id, action);
	mail_html_end (html, stream, TRUE, box);
}

static void
handle_audio (CamelMimePart *part, CamelMimeMessage *root, GtkBox *box)
{
	char *cid, *id;

	cid = g_hash_table_lookup (mime_icon_table, "audio");
	id = g_strdup_printf ("Audio data in \"%s\" format.",
			      camel_mime_part_get_content_type (part)->subtype);
	handle_mystery (part, root, box, cid, id, "play it");
	g_free (id);
}

static void
handle_message_rfc822 (CamelMimePart *part, CamelMimeMessage *root,
		       GtkBox *box)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	GtkWidget *subbox, *frame;

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (wrapper));

	subbox = gtk_vbox_new (FALSE, 2);
	mail_format_mime_message (CAMEL_MIME_MESSAGE (wrapper),
				  GTK_BOX (subbox));

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 8);
	gtk_box_pack_start (box, frame, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (frame), subbox);
	gtk_widget_show_all (frame);
}

static void
handle_unknown_type (CamelMimePart *part, CamelMimeMessage *root, GtkBox *box)
{
	char *cid, *id;

	cid = g_hash_table_lookup (mime_icon_table, "unknown");
	id = g_strdup_printf ("Unknown data of type \"%s/%s\".",
			      camel_mime_part_get_content_type (part)->type,
			      camel_mime_part_get_content_type (part)->subtype);
	handle_mystery (part, root, box, cid, id, "save it to disk");
	g_free (id);
}

static void 
embeddable_destroy_cb (GtkObject *obj, gpointer user_data)
{
	BonoboWidget *be;      /* bonobo embeddable */
	BonoboViewFrame *vf;   /* the embeddable view frame */
	BonoboObjectClient* server;
	CORBA_Environment ev;

	be = BONOBO_WIDGET (obj);
	server = bonobo_widget_get_server (be);

	vf = bonobo_widget_get_view_frame (be);
	bonobo_control_frame_control_deactivate (
		BONOBO_CONTROL_FRAME (vf));
	/* w = bonobo_control_frame_get_widget (BONOBO_CONTROL_FRAME (vf)); */
	
	/* gtk_widget_destroy (w); */
	
	CORBA_exception_init (&ev);
	Bonobo_Unknown_unref (
		bonobo_object_corba_objref (BONOBO_OBJECT(server)), &ev);
	CORBA_Object_release (
		bonobo_object_corba_objref (BONOBO_OBJECT(server)), &ev);

	CORBA_exception_free (&ev);
	bonobo_object_destroy (BONOBO_OBJECT (vf));
	/* gtk_object_unref (obj); */
}

static void
handle_via_bonobo (CamelMimePart *part, CamelMimeMessage *root, GtkBox *box)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	GMimeContentField *type;
	char *mimetype;
	const char *goad_id;
	GtkWidget *embedded;
	BonoboObjectClient *server;
	Bonobo_PersistStream persist;	
	CORBA_Environment ev;
	GByteArray *ba;
	CamelStream *cstream;
	BonoboStream *bstream;

	type = camel_data_wrapper_get_mime_type_field (
		camel_medium_get_content_object (CAMEL_MEDIUM (part)));
	mimetype = g_strdup_printf ("%s/%s", type->type, type->subtype);
	goad_id = gnome_mime_get_value (mimetype, "bonobo-goad-id");
	g_free (mimetype);

	if (!goad_id)
		goad_id = gnome_mime_get_value (type->type, "bonobo-goad-id");
	if (!goad_id)
		return;

	embedded = bonobo_widget_new_subdoc (goad_id, NULL);
	if (!embedded)
		return;
	server = bonobo_widget_get_server (BONOBO_WIDGET (embedded));
	if (!server) {
		bonobo_object_destroy (BONOBO_OBJECT (embedded));
		return;
	}

	persist = (Bonobo_PersistStream) bonobo_object_client_query_interface (
		server, "IDL:Bonobo/PersistStream:1.0", NULL);
	if (persist == CORBA_OBJECT_NIL) {
		bonobo_object_destroy (BONOBO_OBJECT (embedded));
		return;
	}

	/* Write the data to a CamelStreamMem... */
	ba = g_byte_array_new ();
	cstream = camel_stream_mem_new_with_byte_array (
		ba, CAMEL_STREAM_MEM_WRITE);
	camel_data_wrapper_write_to_stream (wrapper, cstream);

	/* ...convert the CamelStreamMem to a BonoboStreamMem... */
	bstream = bonobo_stream_mem_create (ba->data, ba->len, TRUE, FALSE);
	camel_stream_close (cstream);

	/* ...and hydrate the PersistStream from the BonoboStream. */
	CORBA_exception_init (&ev);
	Bonobo_PersistStream_load (persist,
				   bonobo_object_corba_objref (
					   BONOBO_OBJECT (bstream)),
				   &ev);
	bonobo_object_unref (BONOBO_OBJECT (bstream));
	Bonobo_Unknown_unref (persist, &ev);
	CORBA_Object_release (persist, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		bonobo_object_unref (BONOBO_OBJECT (embedded));
		CORBA_exception_free (&ev);				
		return;
	}
	CORBA_exception_free (&ev);				

	/* Embed the widget. */
	gtk_widget_show (embedded);
	gtk_box_pack_start (box, embedded, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (embedded), "destroy",
			    embeddable_destroy_cb, NULL);
}


static char *
get_data_wrapper_text (CamelDataWrapper *data)
{
	CamelStream *memstream;
	GByteArray *ba;
	char *text;

	ba = g_byte_array_new ();
	memstream = camel_stream_mem_new_with_byte_array (ba, CAMEL_STREAM_MEM_WRITE);

	camel_data_wrapper_write_to_stream (data, memstream);
	text = g_malloc (ba->len + 1);
	memcpy (text, ba->data, ba->len);
	text[ba->len] = '\0';

	camel_stream_close (memstream);
	return text;
}

static char *
reply_body (CamelDataWrapper *data, gboolean *html)
{
	CamelMultipart *mp;
	CamelMimePart *subpart;
	int i, nparts;
	char *subtext, *old;
	const char *boundary, *disp;
	char *text = NULL;
	GMimeContentField *mime_type;

	/* We only include text, message, and multipart bodies. */
	mime_type = camel_data_wrapper_get_mime_type_field (data);

	/* FIXME: This is wrong. We don't want to include large
	 * images. But if we don't do it this way, we don't get
	 * the headers...
	 */
	if (strcasecmp (mime_type->type, "message") == 0) {
		*html = FALSE;
		return get_data_wrapper_text (data);
	}

	if (strcasecmp (mime_type->type, "text") == 0) {
		*html = !strcasecmp (mime_type->subtype, "html");
		return get_data_wrapper_text (data);
	}

	/* If it's not message and it's not text, and it's not
	 * multipart, we don't want to deal with it.
	 */
	if (strcasecmp (mime_type->type, "multipart") != 0)
		return NULL;

	mp = CAMEL_MULTIPART (data);

	if (strcasecmp (mime_type->subtype, "alternative") == 0) {
		/* Pick our favorite alternative and reply to it. */

		subpart = find_preferred_alternative (mp);
		if (!subpart)
			return NULL;

		data = camel_medium_get_content_object (
			CAMEL_MEDIUM (subpart));
		return reply_body (data, html);
	}

	nparts = camel_multipart_get_number (mp);

	/* Otherwise, concatenate all the parts that we can. If we find
	 * an HTML part in there though, return just that: We don't want
	 * to deal with merging HTML and non-HTML parts.
	 */
	boundary = camel_multipart_get_boundary (mp);
	for (i = 0; i < nparts; i++) {
		subpart = CAMEL_MIME_PART (camel_multipart_get_part (mp, i));

		disp = camel_mime_part_get_disposition (subpart);
		if (disp && strcasecmp (disp, "inline") != 0)
			continue;

		data = camel_medium_get_content_object (
			CAMEL_MEDIUM (subpart));
		subtext = reply_body (data, html);
		if (!subtext)
			continue;
		if (*html) {
			g_free (text);
			return subtext;
		}

		if (text) {
			old = text;
			text = g_strdup_printf ("%s\n--%s\n%s", text,
						boundary, subtext);
			g_free (subtext);
			g_free (old);
		} else
			text = subtext;
	}

	if (!text)
		return NULL;

	return text;
}

EMsgComposer *
mail_generate_reply (CamelMimeMessage *message, gboolean to_all)
{
	CamelDataWrapper *contents;
	char *text, *subject;
	EMsgComposer *composer;
	gboolean html;
	const char *repl_to, *message_id, *references;
	GList *to, *cc;

	contents = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	text = reply_body (contents, &html);

	composer = E_MSG_COMPOSER (e_msg_composer_new ());

	/* Set the quoted reply text. */
	if (text) {
		char *repl_text;

		if (html) {
			repl_text = g_strdup_printf ("<blockquote><i>\n%s\n"
						     "</i></blockquote>\n",
						     text);
		} else {
			char *s, *d, *quoted_text;
			int lines, len;

			/* Count the number of lines in the body. If
			 * the text ends with a \n, this will be one
			 * too high, but that's ok. Allocate enough
			 * space for the text and the "> "s.
			 */
			for (s = text, lines = 0; s; s = strchr (s + 1, '\n'))
				lines++;
			quoted_text = g_malloc (strlen (text) + lines * 2);

			s = text;
			d = quoted_text;

			/* Copy text to quoted_text line by line,
			 * prepending "> ".
			 */
			while (1) {
				len = strcspn (s, "\n");
				if (len == 0 && !*s)
					break;
				sprintf (d, "> %.*s\n", len, s);
				s += len;
				if (!*s++)
					break;
				d += len + 3;
			}

			/* Now convert that to HTML. */
			repl_text = text_to_html (quoted_text,
						  strlen (quoted_text),
						  &len, TRUE, FALSE);
			g_free (quoted_text);
		}
		e_msg_composer_set_body_text (composer, repl_text);
		g_free (repl_text);
		g_free (text);
	}

	/* Set the recipients */
	repl_to = camel_mime_message_get_reply_to (message);
	if (!repl_to)
		repl_to = camel_mime_message_get_from (message);
	to = g_list_append (NULL, (gpointer)repl_to);

	if (to_all) {
		const GList *recip;

		recip = camel_mime_message_get_recipients (message, 
			CAMEL_RECIPIENT_TYPE_TO);
		cc = g_list_copy ((GList*)recip);

		recip = camel_mime_message_get_recipients (message,
			CAMEL_RECIPIENT_TYPE_CC);
		while (recip) {
			cc = g_list_append (cc, recip->data);
			recip = recip->next;
		}
	} else
		cc = NULL;

	/* Set the subject of the new message. */
	subject = (char *)camel_mime_message_get_subject (message);
	if (!subject)
		subject = g_strdup ("");
	else if (!strncasecmp (subject, "Re: ", 4))
		subject = g_strdup (subject);
	else
		subject = g_strdup_printf ("Re: %s", subject);

	e_msg_composer_set_headers (composer, to, cc, NULL, subject);
	g_list_free (to);
	g_list_free (cc);
	g_free (subject);

	/* Add In-Reply-To and References. */
	message_id = camel_medium_get_header (CAMEL_MEDIUM (message),
					      "Message-Id");
	references = camel_medium_get_header (CAMEL_MEDIUM (message),
					      "References");
	if (message_id) {
		e_msg_composer_add_header (composer, "In-Reply-To",
					   message_id);
		if (references) {
			char *reply_refs;
			reply_refs = g_strdup_printf ("%s %s", references,
						      message_id);
			e_msg_composer_add_header (composer, "References",
						   reply_refs);
			g_free (reply_refs);
		}
	} else if (references)
		e_msg_composer_add_header (composer, "References", references);

	return composer;
}

/* This is part of the temporary kludge below. */
#ifndef HAVE_MKSTEMP
#include <fcntl.h>
#include <sys/stat.h>
#endif

EMsgComposer *
mail_generate_forward (CamelMimeMessage *mime_message,
		       gboolean forward_as_attachment,
		       gboolean keep_attachments)
{
	EMsgComposer *composer;
	char *tmpfile;
	int fd;
	CamelStream *stream;

	if (!forward_as_attachment)
		g_warning ("Forward as non-attachment not implemented.");
	if (!keep_attachments)
		g_warning ("Forwarding without attachments not implemented.");

	/* For now, we kludge by writing out a temp file. Later,
	 * EMsgComposer will support attaching CamelMimeParts directly,
	 * or something. FIXME.
	 */
	tmpfile = g_strdup ("/tmp/evolution-kludge-XXXX");
#ifdef HAVE_MKSTEMP
	fd = mkstemp (tmpfile);
#else
	if (mktemp (tmpfile)) {
		fd = open (tmpfile, O_RDWR | O_CREAT | O_EXCL,
			   S_IRUSR | S_IWUSR);
	} else
		fd = -1;
#endif
	if (fd == -1) {
		g_warning ("Couldn't create temp file for forwarding");
		g_free (tmpfile);
		return NULL;
	}

	stream = camel_stream_fs_new_with_fd (fd);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (mime_message),
					    stream);
	camel_stream_close (stream);

	composer = E_MSG_COMPOSER (e_msg_composer_new ());
	e_msg_composer_attachment_bar_attach (E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar), tmpfile);
	g_free (tmpfile);

	/* FIXME: should we default a subject? */

	return composer;
}
