/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors:
 *  Matt Loper <matt@helixcode.com>
 *  Dan Winship <danw@helixcode.com>
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
#include "mail-display.h"
#include "mail.h"
#include "e-util/e-html-utils.h"

#include <libgnome/libgnome.h>

#include <ctype.h>    /* for isprint */
#include <string.h>   /* for strstr  */
#include <fcntl.h>

struct mail_format_data {
	CamelMimeMessage *root;
	GHashTable *urls;
	GtkHTML *html;
	GtkHTMLStream *stream;
};

static void handle_text_plain            (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_text_plain_flowed     (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_text_enriched         (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_text_html             (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_image                 (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_multipart_mixed       (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_multipart_related     (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_multipart_alternative (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_multipart_appledouble (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_audio                 (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_message_rfc822        (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_message_external_body (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);

static void handle_unknown_type          (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);
static void handle_via_bonobo            (CamelMimePart *part,
					  const char *mime_type,
					  struct mail_format_data *mfd);

/* writes the header info for a mime message into an html stream */
static void write_headers (struct mail_format_data *mfd);

/* dispatch html printing via mimetype */
static void call_handler_function (CamelMimePart *part,
				   struct mail_format_data *mfd);

static void free_urls (gpointer data);


/**
 * mail_format_mime_message: 
 * @mime_message: the input mime message
 * @html: a GtkHTML
 * @stream: a stream on @html
 * @root: the root message being displayed (may be the same as @mime_message)
 *
 * Writes a CamelMimeMessage out into a GtkHTML
 **/
void
mail_format_mime_message (CamelMimeMessage *mime_message,
			  GtkHTML *html, GtkHTMLStream *stream,
			  CamelMimeMessage *root)
{
	struct mail_format_data mfd;

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (mime_message));

	mfd.html = html;
	mfd.stream = stream;
	mfd.root = root;
	mfd.urls = gtk_object_get_data (GTK_OBJECT (root), "urls");
	if (!mfd.urls) {
		mfd.urls = g_hash_table_new (g_str_hash, g_str_equal);
		gtk_object_set_data_full (GTK_OBJECT (root), "urls",
					  mfd.urls, free_urls);
	}

	write_headers (&mfd);
	call_handler_function (CAMEL_MIME_PART (mime_message), &mfd);
}

static void
free_url (gpointer key, gpointer value, gpointer data)
{
	g_free (key);
}

static void
free_urls (gpointer data)
{
	GHashTable *urls = data;

	g_hash_table_foreach (urls, free_url, NULL);
	g_hash_table_destroy (urls);
}

static const char *
get_cid (CamelMimePart *part, struct mail_format_data *mfd)
{
	char *cid;
	gpointer orig_name, value;

	/* If we have a real Content-ID, use it. If we don't,
	 * make a (syntactically invalid) fake one.
	 */
	if (camel_mime_part_get_content_id (part)) {
		cid = g_strdup_printf ("cid:%s",
				       camel_mime_part_get_content_id (part));
	} else
		cid = g_strdup_printf ("cid:@@@%p", part);

	if (g_hash_table_lookup_extended (mfd->urls, cid, &orig_name, &value)) {
		g_free (cid);
		return orig_name;
	} else
		g_hash_table_insert (mfd->urls, cid, part);

	return cid;
}



/* We're maintaining a hashtable of mimetypes -> functions;
 * Those functions have the following signature...
 */
typedef void (*mime_handler_fn) (CamelMimePart *part, const char *mime_type,
				 struct mail_format_data *mfd);

static GHashTable *mime_function_table, *mime_fallback_table;

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
	g_hash_table_insert (mime_function_table, "message/external-body",
			     handle_message_external_body);

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

static mime_handler_fn
lookup_handler (const char *mime_type, gboolean *generic)
{
	mime_handler_fn handler_function;
	const char *whole_goad_id, *generic_goad_id;
	char *mime_type_main;

	if (mime_function_table == NULL)
		setup_function_table ();

	mime_type_main = g_strdup_printf ("%.*s/*",
					  (int)strcspn (mime_type, "/"),
					  mime_type);

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
	handler_function = g_hash_table_lookup (mime_function_table,
						mime_type);
	if (!handler_function) {
		handler_function = g_hash_table_lookup (mime_function_table,
							mime_type_main);
		if (handler_function) {
			/* Optimize this for the next time through. */
			g_hash_table_insert (mime_function_table,
					     g_strdup (mime_type),
					     handler_function);
		}
	}

	if (handler_function) {
		g_free (mime_type_main);
		*generic = FALSE;
		return handler_function;
	}

	whole_goad_id = gnome_mime_get_value (mime_type, "bonobo-goad-id");
	generic_goad_id = gnome_mime_get_value (mime_type_main,
						"bonobo-goad-id");

	if (whole_goad_id && (!generic_goad_id ||
			      strcmp (whole_goad_id, generic_goad_id) != 0)) {
		/* Optimize this for the next time through. */
		g_hash_table_insert (mime_function_table,
				     g_strdup (mime_type),
				     handle_via_bonobo);
		g_free (mime_type_main);
		*generic = FALSE;
		return handle_via_bonobo;
	}

	handler_function = g_hash_table_lookup (mime_fallback_table,
						mime_type);
	if (handler_function)
		*generic = FALSE;
	else {
		handler_function = g_hash_table_lookup (mime_fallback_table,
							mime_type_main);
		if (!handler_function && generic_goad_id)
			handler_function = handle_via_bonobo;
		*generic = TRUE;
	}

	g_free (mime_type_main);
	return handler_function;
}

static void
call_handler_function (CamelMimePart *part, struct mail_format_data *mfd)
{
	CamelDataWrapper *wrapper;
	char *mime_type;
	mime_handler_fn handler_function = NULL;
	gboolean generic;

	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	mime_type = camel_data_wrapper_get_mime_type (wrapper);
	g_strdown (mime_type);

	handler_function = lookup_handler (mime_type, &generic);
	if (handler_function)
		(*handler_function) (part, mime_type, mfd);
	else
		handle_unknown_type (part, mime_type, mfd);

	g_free (mime_type);
}

static void
write_field_to_stream (const char *description, const char *value,
		       gboolean bold, GtkHTML *html,
		       GtkHTMLStream *stream)
{
	char *encoded_value;

	if (value) {
		unsigned char *p;

		encoded_value = e_text_to_html (value,
						E_TEXT_TO_HTML_CONVERT_NL |
						E_TEXT_TO_HTML_CONVERT_URLS);
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
			    const CamelInternetAddress *recipients,
			    gboolean optional, gboolean bold,
			    GtkHTML *html, GtkHTMLStream *stream)
{
	int i;
	char *recipients_string = NULL;
	const char *name, *addr;

	i = 0;
	while (camel_internet_address_get (recipients, i++, &name, &addr)) {
		char *old_string = recipients_string;

		if (*name) {
			recipients_string = g_strdup_printf (
				"%s%s\"%s\" <%s>",
				old_string ? old_string : "",
				old_string ? ", " : "",
				name, addr);
		} else {
			recipients_string = g_strdup_printf (
				"%s%s%s", old_string ? old_string : "",
				old_string ? ", " : "", addr);
		}
		g_free (old_string);
	}

	if (recipients_string || !optional) {
		write_field_to_stream (recipient_type, recipients_string,
				       bold, html, stream);
	}
	g_free (recipients_string);
}



static void
write_headers (struct mail_format_data *mfd)
{
	const CamelInternetAddress *recipients;

	mail_html_write (mfd->html, mfd->stream,
			 "<table bgcolor=\"#EEEEEE\" width=\"100%%\" "
			 "cellspacing=0 border=1>"
			 "<tr><td><table>\n");

	write_field_to_stream ("From:",
			       camel_mime_message_get_from (mfd->root),
			       TRUE, mfd->html, mfd->stream);

	if (camel_mime_message_get_reply_to (mfd->root)) {
		write_field_to_stream ("Reply-To:",
				       camel_mime_message_get_reply_to (mfd->root),
				       FALSE, mfd->html, mfd->stream);
	}

	write_recipients_to_stream ("To:",
				    camel_mime_message_get_recipients (mfd->root, CAMEL_RECIPIENT_TYPE_TO),
				    FALSE, TRUE, mfd->html, mfd->stream);

	recipients = camel_mime_message_get_recipients (mfd->root, CAMEL_RECIPIENT_TYPE_CC);
	write_recipients_to_stream ("Cc:", recipients, TRUE, TRUE,
				    mfd->html, mfd->stream);
	write_field_to_stream ("Subject:",
			       camel_mime_message_get_subject (mfd->root),
			       TRUE, mfd->html, mfd->stream);

	mail_html_write (mfd->html, mfd->stream, "</table></td></tr></table></center>");
}



static char *
get_data_wrapper_text (CamelDataWrapper *data)
{
	CamelStream *memstream;
	GByteArray *ba;
	char *text;

	ba = g_byte_array_new ();
	memstream = camel_stream_mem_new_with_byte_array (ba);

	camel_data_wrapper_write_to_stream (data, memstream);
	text = g_malloc (ba->len + 1);
	memcpy (text, ba->data, ba->len);
	text[ba->len] = '\0';

	gtk_object_unref (GTK_OBJECT (memstream));
	return text;
}

/*----------------------------------------------------------------------*
 *                     Mime handling functions
 *----------------------------------------------------------------------*/

static void
handle_text_plain (CamelMimePart *part, const char *mime_type,
		   struct mail_format_data *mfd)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	char *text, *htmltext;
	GMimeContentField *type;
	const char *format;

	/* Check for RFC 2646 flowed text. */
	type = camel_mime_part_get_content_type (part);
	format = gmime_content_field_get_parameter (type, "format");
	if (format && !g_strcasecmp (format, "flowed")) {
		handle_text_plain_flowed (part, mime_type, mfd);
		return;
	}

	mail_html_write (mfd->html, mfd->stream,
			 "\n<!-- text/plain -->\n<pre>\n");

	text = get_data_wrapper_text (wrapper);
	if (text && *text) {
		htmltext = e_text_to_html (text, E_TEXT_TO_HTML_CONVERT_URLS);
		mail_html_write (mfd->html, mfd->stream, "%s", htmltext);
		g_free (htmltext);
	}
	g_free (text);

	mail_html_write (mfd->html, mfd->stream, "</pre>\n");
}

static void
handle_text_plain_flowed (CamelMimePart *part, const char *mime_type,
			  struct mail_format_data *mfd)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	char *buf, *text, *line, *eol, *p;
	int prevquoting = 0, quoting, len;
	gboolean br_pending = FALSE;

	mail_html_write (mfd->html, mfd->stream,
			 "\n<!-- text/plain, flowed -->\n<tt>\n");

	buf = get_data_wrapper_text (wrapper);
	for (line = buf; *line; line = eol + 1) {
		/* Process next line */
		eol = strchr (line, '\n');
		if (eol)
			*eol = '\0';

		quoting = 0;
		for (p = line; *p == '>'; p++)
			quoting++;
		if (quoting != prevquoting) {
			mail_html_write (mfd->html, mfd->stream, "%s\n",
					 prevquoting == 0 ? "<i>\n" : "");
			while (quoting > prevquoting) {
				mail_html_write (mfd->html, mfd->stream,
						 "<blockquote>");
				prevquoting++;
			}
			while (quoting < prevquoting) {
				mail_html_write (mfd->html, mfd->stream,
						 "</blockquote>");
				prevquoting--;
			}
			mail_html_write (mfd->html, mfd->stream, "%s\n",
					 prevquoting == 0 ? "</i>\n" : "");
		} else if (br_pending) {
			mail_html_write (mfd->html, mfd->stream, "<br>\n");
			br_pending = FALSE;
		}

		if (*p == ' ')
			p++;

		/* replace '<' with '&lt;', etc. */
		text = e_text_to_html (p, E_TEXT_TO_HTML_CONVERT_SPACES |
				       E_TEXT_TO_HTML_CONVERT_URLS);
		if (text && *text)
			mail_html_write (mfd->html, mfd->stream, "%s", text);
		g_free (text);

		len = strlen (p);
		if (len == 0 || p[len - 1] != ' ' || !strcmp (p, "-- "))
			br_pending = TRUE;

		if (!eol)
			break;
	}
	g_free (buf);

	mail_html_write (mfd->html, mfd->stream, "</tt>\n");
}

static void
free_byte_array (GtkWidget *widget, gpointer user_data)
{
	g_byte_array_free (user_data, TRUE);
}

/* text/enriched (RFC 1896) or text/richtext (included in RFC 1341) */
static void
handle_text_enriched (CamelMimePart *part, const char *mime_type,
		      struct mail_format_data *mfd)
{
	static GHashTable *translations = NULL;
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	GString *string;
	CamelStream *memstream;
	GByteArray *ba;
	char *p, *xed;
	int len, nofill = 0;
	gboolean enriched;

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
		g_hash_table_insert (translations, "np", "<hr>");
	}

	if (!g_strcasecmp (mime_type, "text/richtext")) {
		enriched = FALSE;
		mail_html_write (mfd->html, mfd->stream,
				 "\n<!-- text/richtext -->\n");
	} else {
		enriched = TRUE;
		mail_html_write (mfd->html, mfd->stream,
				 "\n<!-- text/enriched -->\n");
	}

	/* This is not great code, but I don't feel like fixing it right
	 * now. I mean, it's just text/enriched...
	 */

	ba = g_byte_array_new ();
	memstream = camel_stream_mem_new_with_byte_array (ba);
	camel_data_wrapper_write_to_stream (wrapper, memstream);
	g_byte_array_append (ba, "", 1);

	p = ba->data;
	string = g_string_sized_new (2 * strlen (p));

	while (p) {
		len = strcspn (p, " <>&\n");
		if (len)
			g_string_sprintfa (string, "%.*s", len, p);

		p += len;
		if (!*p)
			break;

		switch (*p++) {
		case ' ':
			while (*p == ' ') {
				g_string_append (string, "&nbsp;");
				p++;
			}
			g_string_append (string, " ");
			break;

		case '\n':
			g_string_append (string, " ");
			if (enriched && nofill <= 0) {
				while (*p == '\n') {
					g_string_append (string, "<br>");
					p++;
				}
			}
			break;

		case '>':
			g_string_append (string, "&gt;");
			break;

		case '&':
			g_string_append (string, "&amp;");
			break;

		case '<':
			if (enriched) {
				if (*p == '<') {
					g_string_append (string, "&lt;");
					p++;
					break;
				}
			} else {
				if (strncmp (p, "lt>", 3) == 0) {
					g_string_append (string, "&lt;");
					p += 3;
					break;
				} else if (strncmp (p, "nl>", 3) == 0) {
					g_string_append (string, "<br>");
					p += 3;
					break;
				}
			}

			if (strncmp (p, "nofill>", 7) == 0) {
				nofill++;
				g_string_append (string, "<pre>");
			} else if (strncmp (p, "/nofill>", 8) == 0) {
				nofill--;
				g_string_append (string, "</pre>");
			} else {
				char *copy, *match;

				len = strcspn (p, ">");
				copy = g_strndup (p, len);
				match = g_hash_table_lookup (translations,
							     copy);
				g_free (copy);
				if (match)
					g_string_append (string, match);
			}

			p = strchr (p, '>');
			if (p)
				p++;
		}
	}
	gtk_object_unref (GTK_OBJECT (memstream));

	ba = g_byte_array_new ();
	g_byte_array_append (ba, (const guint8 *)string->str,
			     strlen (string->str));
	g_string_free (string, TRUE);

	xed = g_strdup_printf ("x-evolution-data:%p", part);
	g_hash_table_insert (mfd->urls, xed, ba);
	gtk_signal_connect (GTK_OBJECT (mfd->root), "destroy",
			    GTK_SIGNAL_FUNC (free_byte_array), ba);
	mail_html_write (mfd->html, mfd->stream,
			 "<iframe src=\"%s\" frameborder=0 scrolling=no>"
			 "</iframe>", xed);
}

static void
handle_text_html (CamelMimePart *part, const char *mime_type,
		  struct mail_format_data *mfd)
{
	mail_html_write (mfd->html, mfd->stream, "\n<!-- text/html -->\n");
	mail_html_write (mfd->html, mfd->stream,
			 "<iframe src=\"%s\" frameborder=0 scrolling=no>"
			 "</iframe>", get_cid (part, mfd));
}

static void
handle_image (CamelMimePart *part, const char *mime_type,
	      struct mail_format_data *mfd)
{
	mail_html_write (mfd->html, mfd->stream, "<img src=\"%s\">",
			 get_cid (part, mfd));
}

static void
handle_multipart_mixed (CamelMimePart *part, const char *mime_type,
			struct mail_format_data *mfd)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *mp;
	int i, nparts;

	g_return_if_fail (CAMEL_IS_MULTIPART (wrapper));
	mp = CAMEL_MULTIPART (wrapper);

	nparts = camel_multipart_get_number (mp);	
	for (i = 0; i < nparts; i++) {
		if (i != 0)
			mail_html_write (mfd->html, mfd->stream, "<hr>\n");

		part = camel_multipart_get_part (mp, i);

		call_handler_function (part, mfd);
	}
}

/* As seen in RFC 2387! */
static void
handle_multipart_related (CamelMimePart *part, const char *mime_type,
			  struct mail_format_data *mfd)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *mp;
	CamelMimePart *body_part, *display_part = NULL;
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
			cid = camel_mime_part_get_content_id (body_part);

			if (!strncmp (cid, start + 1, len) &&
			    strlen (cid) == len) {
				display_part = body_part;
				break;
			}
		}

		if (!display_part) {
			/* Oops. Hrmph. */
			handle_multipart_mixed (part, mime_type, mfd);
		}
	} else {
		/* No start parameter, so it defaults to the first part. */
		display_part = camel_multipart_get_part (mp, 0);
	}

	/* Record the Content-IDs of any non-displayed parts. */
	for (i = 0; i < nparts; i++) {
		body_part = camel_multipart_get_part (mp, i);
		if (body_part == display_part)
			continue;

		get_cid (body_part, mfd);
	}

	/* Now, display the displayed part. */
	call_handler_function (display_part, mfd);
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
		CamelMimePart *part = camel_multipart_get_part (multipart, i);
		char *mime_type = gmime_content_field_get_mime_type (
			camel_mime_part_get_content_type (part));

		g_strdown (mime_type);
		if (lookup_handler (mime_type, &generic) &&
		    (!preferred_part || !generic))
			preferred_part = part;
		g_free (mime_type);
	}

	return preferred_part;
}

static void
handle_multipart_alternative (CamelMimePart *part, const char *mime_type,
			      struct mail_format_data *mfd)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *multipart;
	CamelMimePart *mime_part;

	g_return_if_fail (CAMEL_IS_MULTIPART (wrapper));
	multipart = CAMEL_MULTIPART (wrapper);

	mime_part = find_preferred_alternative (multipart);	
	if (mime_part)
		call_handler_function (mime_part, mfd);
	else
		handle_unknown_type (part, mime_type, mfd);
}

/* RFC 1740 */
static void
handle_multipart_appledouble (CamelMimePart *part, const char *mime_type,
			      struct mail_format_data *mfd)
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
	part = camel_multipart_get_part (multipart, 1);
	call_handler_function (part, mfd);
}

static const char *
get_url_for_icon (const char *icon_name, struct mail_format_data *mfd)
{
	static GHashTable *icons;
	char *icon_path = gnome_pixmap_file (icon_name), buf[1024], *url;
	GByteArray *ba;

	if (!icons)
		icons = g_hash_table_new (g_str_hash, g_str_equal);

	if (!icon_path)
		return "file:///dev/null";

	ba = g_hash_table_lookup (icons, icon_path);
	if (!ba) {
		int fd, nread;

		fd = open (icon_path, O_RDONLY);
		if (fd == -1) {
			g_free (icon_path);
			return "file:///dev/null";
		}

		ba = g_byte_array_new ();

		while (1) {
			nread = read (fd, buf, sizeof (buf));
			if (nread < 1)
				break;
			g_byte_array_append (ba, buf, nread);
		}
		close (fd);

		g_hash_table_insert (icons, icon_path, ba);
	}
	g_free (icon_path);

	url = g_strdup_printf ("x-evolution-data:%p", ba);
	g_hash_table_insert (mfd->urls, url, ba);

	return url;
}

static void
handle_mystery (CamelMimePart *part, struct mail_format_data *mfd,
		const char *url, const char *icon_name, const char *id,
		const char *action)
{
	const char *info;
	char *htmlinfo;
	GMimeContentField *content_type;

	mail_html_write (mfd->html, mfd->stream, "<table><tr><td>");

	/* Draw the icon, surrounded by an <a href> if we have a URL,
	 * or a plain inactive border if not.
	 */
	if (url) {
		mail_html_write (mfd->html, mfd->stream,
				 "<a href=\"%s\">", url);
	} else {
		mail_html_write (mfd->html, mfd->stream,
				 "<table border=2><tr><td>");
	}
	mail_html_write (mfd->html, mfd->stream, "<img src=\"%s\">",
			 get_url_for_icon (icon_name, mfd));

	if (url)
		mail_html_write (mfd->html, mfd->stream, "</a>");
	else
		mail_html_write (mfd->html, mfd->stream, "</td></tr></table>");
	mail_html_write (mfd->html, mfd->stream, "</td><td>%s<br>", id);

	/* Write a description, if we have one. */
	info = camel_mime_part_get_description (part);
	if (info) {
		htmlinfo = e_text_to_html (info, E_TEXT_TO_HTML_CONVERT_URLS);
		mail_html_write (mfd->html, mfd->stream, "Description: %s<br>",
				 htmlinfo);
		g_free (htmlinfo);
	}

	/* Write the name, if we have it. */
	content_type = camel_mime_part_get_content_type (part);
	info = gmime_content_field_get_parameter (content_type, "name");
	if (!info)
		info = camel_mime_part_get_filename (part);
	if (info) {
		htmlinfo = e_text_to_html (info, 0);
		mail_html_write (mfd->html, mfd->stream, "Name: %s<br>",
				 htmlinfo);
		g_free (htmlinfo);
	}

	/* Describe the click action, if any. */
	if (action) {
		mail_html_write (mfd->html, mfd->stream,
				 "<br>Click on the icon to %s.", action);
	}

	mail_html_write (mfd->html, mfd->stream, "</td></tr></table>");
}

static void
handle_audio (CamelMimePart *part, const char *mime_type,
	      struct mail_format_data *mfd)
{
	char *id;
	const char *desc;

	desc = gnome_mime_get_value (mime_type, "description");
	if (desc)
		id = g_strdup_printf ("%s data", desc);
	else {
		id = g_strdup_printf ("Audio data in \"%s\" format.",
				      mime_type);
	}
	handle_mystery (part, mfd, get_cid (part, mfd), "gnome-audio2.png",
			id, "play it");
	g_free (id);
}

static void
handle_message_rfc822 (CamelMimePart *part, const char *mime_type,
		       struct mail_format_data *mfd)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (wrapper));

	mail_html_write (mfd->html, mfd->stream, "<center>"
			 "<table border=1 width=\"95%%\"><tr><td>");
	mail_format_mime_message (CAMEL_MIME_MESSAGE (wrapper),
				  mfd->html, mfd->stream, mfd->root);
	mail_html_write (mfd->html, mfd->stream, "</td></tr></table></center>");
}

static void
handle_message_external_body (CamelMimePart *part, const char *mime_type,
			      struct mail_format_data *mfd)
{
	GMimeContentField *type;
	const char *access_type;
	char *url = NULL, *desc = NULL;

	type = camel_mime_part_get_content_type (part);
	access_type = gmime_content_field_get_parameter (type, "access-type");
	if (!access_type)
		goto fallback;

	if (!g_strcasecmp (access_type, "ftp") ||
	    !g_strcasecmp (access_type, "anon-ftp")) {
		const char *name, *site, *dir, *mode, *ftype;
		char *path;

		name = gmime_content_field_get_parameter (type, "name");
		site = gmime_content_field_get_parameter (type, "site");
		if (name == NULL || site == NULL)
			goto fallback;
		dir = gmime_content_field_get_parameter (type, "directory");
		mode = gmime_content_field_get_parameter (type, "mode");

		/* Generate the path. */
		if (dir) {
			const char *p = dir + strlen (dir);

			path = g_strdup_printf ("%s%s%s%s",
						*dir == '/' ? "" : "/",
						dir,
						*p == '/' ? "" : "/",
						name);
		} else {
			path = g_strdup_printf ("%s%s",
						*name == '/' ? "" : "/",
						name);
		}

		if (mode && *mode == 'A')
			ftype = ";type=A";
		else if (mode && *mode == 'I')
			ftype = ";type=I";
		else
			ftype = "";

		url = g_strdup_printf ("ftp://%s%s%s", site, path, ftype);
		g_free (path);
		desc = g_strdup_printf ("Pointer to FTP site (%s)", url);
	} else if (!g_strcasecmp (access_type, "local-file")) {
		const char *name, *site;

		name = gmime_content_field_get_parameter (type, "name");
		if (name == NULL)
			goto fallback;
		site = gmime_content_field_get_parameter (type, "site");

		url = g_strdup_printf ("file://%s%s", *name == '/' ? "" : "/",
				       name);
		desc = g_strdup_printf ("Pointer to local file (%s)%s%s%s",
					name, site ? " valid at site \"" : "",
					site ? site : "", site ? "\"" : "");
	} else if (!g_strcasecmp (access_type, "URL")) {
		const char *urlparam;
		char *s, *d;

		/* RFC 2017 */

		urlparam = gmime_content_field_get_parameter (type, "url");
		if (urlparam == NULL)
			goto fallback;

		/* For obscure MIMEy reasons, the URL may be split into
		 * multiple words, and needs to be rejoined. (The URL
		 * must have any real whitespace %-encoded, so we just
		 * get rid of all of it.
		 */
		url = g_strdup (urlparam);
		s = d = url;

		while (*s) {
			if (!isspace ((unsigned char)*s))
				*d++ = *s;
			s++;
		}
		*d = *s;

		desc = g_strdup_printf ("Pointer to remote data (%s)", url);
	}

 fallback:
	if (!desc) {
		if (access_type) {
			desc = g_strdup_printf ("Pointer to unknown external "
						"data (\"%s\" type)",
						access_type);
		} else
			desc = g_strdup ("Malformed external-body part.");
	}

	handle_mystery (part, mfd, url, "gnome-globe.png", desc,
			url ? "open it in a browser" : NULL);

	g_free (desc);
	g_free (url);
}

static void
handle_undisplayable (CamelMimePart *part, const char *mime_type,
		      struct mail_format_data *mfd)
{
	const char *desc;
	char *id;

	desc = gnome_mime_get_value (mime_type, "description");
	if (desc)
		id = g_strdup (desc);
	else
		id = g_strdup_printf ("Data of type \"%s\".", mime_type);
	handle_mystery (part, mfd, get_cid (part, mfd), "gnome-question.png",
			id, "save it to disk");
	g_free (id);
}

static void
handle_unknown_type (CamelMimePart *part, const char *mime_type,
		     struct mail_format_data *mfd)
{
	char *type;

	/* Don't give up quite yet. */
	type = mail_identify_mime_part (part);
	if (type) {
		mime_handler_fn handler_function;
		gboolean generic;

		handler_function = lookup_handler (type, &generic);
		if (handler_function &&
		    handler_function != handle_unknown_type) {
			(*handler_function) (part, type, mfd);
			g_free (type);
			return;
		}
	} else
		type = g_strdup (mime_type);

	/* OK. Give up. */
	handle_undisplayable (part, type, mfd);
	g_free (type);
}

static void
handle_via_bonobo (CamelMimePart *part, const char *mime_type,
		   struct mail_format_data *mfd)
{
	mail_html_write (mfd->html, mfd->stream,
			 "<object classid=\"%s\" type=\"%s\">",
			 get_cid (part, mfd), mime_type);

	/* Call handle_undisplayable to output its HTML inside the
	 * <object> ... </object>. It will only be displayed if the
	 * object loading fails.
	 */
	handle_undisplayable (part, mime_type, mfd);

	mail_html_write (mfd->html, mfd->stream, "</object>");
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
	if (g_strcasecmp (mime_type->type, "message") == 0) {
		*html = FALSE;
		return get_data_wrapper_text (data);
	}

	if (g_strcasecmp (mime_type->type, "text") == 0) {
		*html = !g_strcasecmp (mime_type->subtype, "html");
		return get_data_wrapper_text (data);
	}

	/* If it's not message and it's not text, and it's not
	 * multipart, we don't want to deal with it.
	 */
	if (g_strcasecmp (mime_type->type, "multipart") != 0)
		return NULL;

	mp = CAMEL_MULTIPART (data);

	if (g_strcasecmp (mime_type->subtype, "alternative") == 0) {
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
		subpart = camel_multipart_get_part (mp, i);

		disp = camel_mime_part_get_disposition (subpart);
		if (disp && g_strcasecmp (disp, "inline") != 0)
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
			repl_text = e_text_to_html (quoted_text,
						    E_TEXT_TO_HTML_PRE);
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
		const CamelInternetAddress *recip;
		const char *name, *addr;
		char *fulladdr;
		int i;

		recip = camel_mime_message_get_recipients (message, 
			CAMEL_RECIPIENT_TYPE_TO);
		i = 0;
		cc = NULL;
		while (camel_internet_address_get (recip, i++, &name, &addr)) {
			fulladdr = g_strdup_printf ("%s <%s>", name, addr);
			cc = g_list_append (cc, fulladdr);
		}

		recip = camel_mime_message_get_recipients (message,
			CAMEL_RECIPIENT_TYPE_CC);
		i = 0;
		while (camel_internet_address_get (recip, i++, &name, &addr)) {
			fulladdr = g_strdup_printf ("%s <%s>", name, addr);
			cc = g_list_append (cc, fulladdr);
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
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (mime_message), stream);
	camel_stream_flush (stream);
	gtk_object_unref (GTK_OBJECT (stream));

	composer = E_MSG_COMPOSER (e_msg_composer_new ());
	e_msg_composer_attachment_bar_attach (E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar), tmpfile);
	g_free (tmpfile);

	/* FIXME: should we default a subject? */

	return composer;
}
