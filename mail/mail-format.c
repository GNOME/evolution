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
#include "mail-tools.h"
#include "mail-display.h"
#include "mail.h"
#include "e-util/e-html-utils.h"
#include <libgnome/libgnome.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <liboaf/liboaf.h>

#include <ctype.h>    /* for isprint */
#include <string.h>   /* for strstr  */
#include <fcntl.h>

static char *try_inline_pgp (char *start, MailDisplay *md);
static char *try_uudecoding (char *start, MailDisplay *md);
static char *try_inline_binhex (char *start, MailDisplay *md);

static gboolean handle_text_plain            (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);
static gboolean handle_text_plain_flowed     (char *text,
					      MailDisplay *md);
static gboolean handle_text_enriched         (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);
static gboolean handle_text_html             (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);
static gboolean handle_image                 (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);
static gboolean handle_multipart_mixed       (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);
static gboolean handle_multipart_related     (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);
static gboolean handle_multipart_alternative (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);
static gboolean handle_multipart_appledouble (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);
static gboolean handle_multipart_encrypted   (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);
static gboolean handle_message_rfc822        (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);
static gboolean handle_message_external_body (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);

static gboolean handle_via_bonobo            (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md);

/* writes the header info for a mime message into an html stream */
static void write_headers (CamelMimeMessage *message, MailDisplay *md);

/* dispatch html printing via mimetype */
static gboolean call_handler_function (CamelMimePart *part, MailDisplay *md);

static void
free_url (gpointer key, gpointer value, gpointer data)
{
	g_free (key);
}

static void
free_urls (gpointer urls)
{
	g_hash_table_foreach (urls, free_url, NULL);
	g_hash_table_destroy (urls);
}

static char *
add_url (char *url, gpointer data, MailDisplay *md)
{
	GHashTable *urls;
	gpointer old_key, old_value;

	urls = g_datalist_get_data (md->data, "urls");
	g_return_val_if_fail (urls != NULL, NULL);

	if (g_hash_table_lookup_extended (urls, url, &old_key, &old_value)) {
		g_free (url);
		url = old_key;
	}
	g_hash_table_insert (urls, url, data);
	return url;
}

/**
 * mail_format_mime_message: 
 * @mime_message: the input mime message
 * @md: the MailDisplay to render into
 *
 * Writes a CamelMimeMessage out into a MailDisplay
 **/
void
mail_format_mime_message (CamelMimeMessage *mime_message, MailDisplay *md)
{
	GHashTable *urls;

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (mime_message));

	urls = g_datalist_get_data (md->data, "urls");
	if (!urls) {
		urls = g_hash_table_new (g_str_hash, g_str_equal);
		g_datalist_set_data_full (md->data, "urls", urls,
					  free_urls);
	}

	write_headers (mime_message, md);
	call_handler_function (CAMEL_MIME_PART (mime_message), md);
}

static const char *
get_cid (CamelMimePart *part, MailDisplay *md)
{
	GHashTable *urls;
	char *cid;
	gpointer orig_name, value;

	urls = g_datalist_get_data (md->data, "urls");

	/* If we have a real Content-ID, use it. If we don't,
	 * make a (syntactically invalid) fake one.
	 */
	if (camel_mime_part_get_content_id (part)) {
		cid = g_strdup_printf ("cid:%s",
				       camel_mime_part_get_content_id (part));
	} else
		cid = g_strdup_printf ("cid:@@@%p", part);

	if (g_hash_table_lookup_extended (urls, cid, &orig_name, &value)) {
		g_free (cid);
		return orig_name;
	} else
		g_hash_table_insert (urls, cid, part);

	return cid;
}

static const char *
get_url_for_icon (const char *icon_name, MailDisplay *md)
{
	static GHashTable *icons;
	char *icon_path, buf[1024], *url;
	GByteArray *ba;

	if (!icons)
		icons = g_hash_table_new (g_str_hash, g_str_equal);

	if (*icon_name == '/')
		icon_path = g_strdup (icon_name);
	else {
		icon_path = gnome_pixmap_file (icon_name);
		if (!icon_path)
			return "file:///dev/null";
	}

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

		/* FIXME: these aren't freed. */
		g_hash_table_insert (icons, g_strdup (icon_path), ba);
	}
	g_free (icon_path);

	url = g_strdup_printf ("x-evolution-data:%p", ba);
	return add_url (url, ba, md);
}


static GHashTable *mime_handler_table, *mime_function_table;

static void
setup_mime_tables (void)
{
	mime_handler_table = g_hash_table_new (g_str_hash, g_str_equal);
	mime_function_table = g_hash_table_new (g_str_hash, g_str_equal);

	g_hash_table_insert (mime_function_table, "text/plain",
			     handle_text_plain);
	g_hash_table_insert (mime_function_table, "text/richtext",
			     handle_text_enriched);
	g_hash_table_insert (mime_function_table, "text/enriched",
			     handle_text_enriched);
	g_hash_table_insert (mime_function_table, "text/html",
			     handle_text_html);

	g_hash_table_insert (mime_function_table, "image/gif",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/jpeg",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/png",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/x-png",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/tiff",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/x-bmp",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/bmp",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/x-cmu-raster",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/x-ico",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/x-portable-anymap",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/x-portable-bitmap",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/x-portable-graymap",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/x-portable-pixmap",
			     handle_image);
	g_hash_table_insert (mime_function_table, "image/x-xpixmap",
			     handle_image);

	g_hash_table_insert (mime_function_table, "message/rfc822",
			     handle_message_rfc822);
	g_hash_table_insert (mime_function_table, "message/news",
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
	g_hash_table_insert (mime_function_table, "multipart/encrypted",
			     handle_multipart_encrypted);

	/* RFC 2046 says unrecognized text subtypes can be treated
	 * as text/plain (as long as you recognize the character set),
	 * and unrecognized multipart subtypes as multipart/mixed.
	 */
	g_hash_table_insert (mime_function_table, "text/*",
			     handle_text_plain);
	g_hash_table_insert (mime_function_table, "multipart/*",
			     handle_multipart_mixed);
}

static gboolean
component_supports (OAF_ServerInfo *component, const char *mime_type)
{
	OAF_Property *prop;
	CORBA_sequence_CORBA_string stringv;
	int i;

	prop = oaf_server_info_prop_find (component,
					  "bonobo:supported_mime_types");
	if (!prop || prop->v._d != OAF_P_STRINGV)
		return FALSE;

	stringv = prop->v._u.value_stringv;
	for (i = 0; i < stringv._length; i++) {
		if (!g_strcasecmp (mime_type, stringv._buffer[i]))
			return TRUE;
	}
	return FALSE;
}

/**
 * mail_lookup_handler:
 * @mime_type: a MIME type
 *
 * Looks up the MIME type in its own tables and GNOME-VFS's and returns
 * a MailMimeHandler structure detailing the component, application,
 * and built-in handlers (if any) for that MIME type. (If the component
 * is non-%NULL, the built-in handler will always be handle_via_bonobo().)
 * The MailMimeHandler's @generic field is set if the match was for the
 * MIME supertype rather than the exact type.
 *
 * Return value: a MailMimeHandler (which should not be freed), or %NULL
 * if no handlers are available.
 **/
MailMimeHandler *
mail_lookup_handler (const char *mime_type)
{
	MailMimeHandler *handler;
	char *mime_type_main;

	if (mime_handler_table == NULL)
		setup_mime_tables ();

	/* See if we've already found it. */
	handler = g_hash_table_lookup (mime_handler_table, mime_type);
	if (handler)
		return handler;

	/* No. Create a new one and look up application and full type
	 * handler. If we find a builtin, create the handler and
	 * register it.
	 */
	handler = g_new0 (MailMimeHandler, 1);
	handler->application =
		gnome_vfs_mime_get_default_application (mime_type);
	handler->builtin =
		g_hash_table_lookup (mime_function_table, mime_type);

	if (handler->builtin) {
		handler->generic = FALSE;
		goto reg;
	}

	/* Try for a exact component match. */
	handler->component = gnome_vfs_mime_get_default_component (mime_type);
	if (handler->component &&
	    component_supports (handler->component, mime_type)) {
		handler->generic = FALSE;
		handler->builtin = handle_via_bonobo;
		goto reg;
	}

	/* Try for a generic builtin match. */
	mime_type_main = g_strdup_printf ("%.*s/*",
					  (int)strcspn (mime_type, "/"),
					  mime_type);
	handler->builtin = g_hash_table_lookup (mime_function_table,
						mime_type_main);
	g_free (mime_type_main);

	if (handler->builtin) {
		handler->generic = TRUE;
		if (handler->component) {
			CORBA_free (handler->component);
			handler->component = NULL;
		}
		goto reg;
	}

	/* Try for a generic component match. */
	if (handler->component) {
		handler->generic = TRUE;
		handler->builtin = handle_via_bonobo;
		goto reg;
	}

	/* If we at least got an application, use that. */
	if (handler->application) {
		handler->generic = TRUE;
		goto reg;
	}

	/* Nada. */
	g_free (handler);
	return NULL;

 reg:
	g_hash_table_insert (mime_handler_table, g_strdup (mime_type),
			     handler);
	return handler;
}

/* An "anonymous" MIME part is one that we shouldn't call attention
 * to the existence of, but simply display.
 */
static gboolean
is_anonymous (CamelMimePart *part, const char *mime_type)
{
	if (!g_strncasecmp (mime_type, "multipart/", 10) ||
	    !g_strncasecmp (mime_type, "message/", 8))
		return TRUE;

	if (!g_strncasecmp (mime_type, "text/", 5) &&
	    !camel_mime_part_get_filename (part))
		return TRUE;

	return FALSE;
}

/**
 * mail_part_is_inline:
 * @part: a CamelMimePart
 *
 * Return value: whether or not the part should/will be displayed inline.
 **/
gboolean
mail_part_is_inline (CamelMimePart *part)
{
	const char *disposition;
	GMimeContentField *content_type;
	const char *mime_type;

	/* If it has an explicit disposition, return that. */
	disposition = camel_mime_part_get_disposition (part);
	if (disposition)
		return g_strcasecmp (disposition, "inline") == 0;

	/* Certain types should default to inline. FIXME: this should
	 * be customizable.
	 */
	content_type = camel_mime_part_get_content_type (part);
	mime_type = gmime_content_field_get_mime_type (content_type);
	if (!g_strncasecmp (mime_type, "message/", 8))
		return TRUE;

	/* Otherwise, display it inline if it's "anonymous", and
	 * as an attachment otherwise.
	 */
	return is_anonymous (part, mime_type);
}

static void
attachment_header (CamelMimePart *part, const char *mime_type,
		   gboolean is_inline, MailDisplay *md)
{
	const char *info;
	char *htmlinfo;

	/* No header for anonymous inline parts. */
	if (is_inline && is_anonymous (part, mime_type))
		return;

	/* Start the table, create the pop-up object. */
	mail_html_write (md->html, md->stream, "<table><tr><td>"
			 "<object classid=\"popup:%s\" type=\"%s\">"
			 "</object></td><td><font size=-1>",
			 get_cid (part, md), mime_type);

	/* Write the MIME type */
	info = gnome_vfs_mime_get_value (mime_type, "description");
	htmlinfo = e_text_to_html (info ? info : mime_type, 0);
	mail_html_write (md->html, md->stream, "%s attachment", htmlinfo);
	g_free (htmlinfo);

	/* Write the name, if we have it. */
	info = camel_mime_part_get_filename (part);
	if (info) {
		htmlinfo = e_text_to_html (info, 0);
		mail_html_write (md->html, md->stream, " (%s)", htmlinfo);
		g_free (htmlinfo);
	}

	/* Write a description, if we have one. */
	info = camel_mime_part_get_description (part);
	if (info) {
		htmlinfo = e_text_to_html (info, E_TEXT_TO_HTML_CONVERT_URLS);
		mail_html_write (md->html, md->stream, ", \"%s\"", htmlinfo);
		g_free (htmlinfo);
	}

#if 0
	/* Describe the click action, if any. */
	if (action) {
		mail_html_write (md->html, md->stream,
				 "<br>Click on the icon to %s.", action);
	}
#endif

	mail_html_write (md->html, md->stream, "</font></td></tr></table>");
}

static gboolean
call_handler_function (CamelMimePart *part, MailDisplay *md)
{
	CamelDataWrapper *wrapper;
	char *mime_type;
	MailMimeHandler *handler;
	gboolean output, is_inline;

	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	mime_type = camel_data_wrapper_get_mime_type (wrapper);
	g_strdown (mime_type);

	handler = mail_lookup_handler (mime_type);
	if (!handler) {
		char *id_type;

		id_type = mail_identify_mime_part (part);
		if (id_type) {
			g_free (mime_type);
			mime_type = id_type;
			handler = mail_lookup_handler (id_type);
		}
	}

	is_inline = mail_part_is_inline (part);
	attachment_header (part, mime_type, is_inline, md);
	if (handler && handler->builtin && is_inline)
		output = (*handler->builtin) (part, mime_type, md);
	else
		output = TRUE;

	g_free (mime_type);
	return output;
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
write_headers (CamelMimeMessage *message, MailDisplay *md)
{
	const CamelInternetAddress *recipients;
	const char *reply_to;
	char *string;

	mail_html_write (md->html, md->stream,
			 "<table bgcolor=\"#EEEEEE\" width=\"100%%\" "
			 "cellspacing=0 border=1>"
			 "<tr><td><table>\n");

	write_field_to_stream ("From:",
			       camel_mime_message_get_from (message),
			       TRUE, md->html, md->stream);

	reply_to = camel_mime_message_get_reply_to (message);
	if (reply_to) {
		write_field_to_stream ("Reply-To:", reply_to, FALSE,
				       md->html, md->stream);
	}

	recipients = camel_mime_message_get_recipients (
		message, CAMEL_RECIPIENT_TYPE_TO);
	string = camel_address_encode (CAMEL_ADDRESS (recipients));
	write_field_to_stream ("To:", string ? string : "", TRUE,
			       md->html, md->stream);
	g_free (string);

	recipients = camel_mime_message_get_recipients (
		message, CAMEL_RECIPIENT_TYPE_CC);
	string = camel_address_encode (CAMEL_ADDRESS (recipients));
	if (string) {
		write_field_to_stream ("Cc:", string, TRUE,
				       md->html, md->stream);
	}
	g_free (string);

	write_field_to_stream ("Subject:",
			       camel_mime_message_get_subject (message),
			       TRUE, md->html, md->stream);

	mail_html_write (md->html, md->stream,
			 "</table></td></tr></table></center><p>");
}


/* Return the contents of a text-based data wrapper, or NULL if it
 * contains only whitespace.
 */
static char *
get_data_wrapper_text (CamelDataWrapper *data)
{
	CamelStream *memstream;
	GByteArray *ba;
	char *text, *end;

	ba = g_byte_array_new ();
	memstream = camel_stream_mem_new_with_byte_array (ba);

	camel_data_wrapper_write_to_stream (data, memstream);

	for (text = ba->data, end = ba->data + ba->len; text < end; text++) {
		if (!isspace ((unsigned char)*text))
			break;
	}

	if (text < end) {
		text = g_malloc (ba->len + 1);
		memcpy (text, ba->data, ba->len);
		text[ba->len] = '\0';
	} else
		text = NULL;

	camel_object_unref (CAMEL_OBJECT (memstream));
	return text;
}

/*----------------------------------------------------------------------*
 *                     Mime handling functions
 *----------------------------------------------------------------------*/

struct {
	char *start;
	char * (*handler) (char *start, MailDisplay *md);
} text_specials[] = {
	{ "-----BEGIN PGP MESSAGE-----\n", try_inline_pgp },
	{ "begin ", try_uudecoding },
	{ "(This file must be converted with BinHex 4.0)\n", try_inline_binhex }
};
#define NSPECIALS (sizeof (text_specials) / sizeof (*text_specials))

static gboolean
handle_text_plain (CamelMimePart *part, const char *mime_type,
		   MailDisplay *md)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	char *text, *p, *start, *subtext;
	GMimeContentField *type;
	const char *format;
	int i;

	text = get_data_wrapper_text (wrapper);
	if (!text)
		return FALSE;

	/* Check for RFC 2646 flowed text. */
	type = camel_mime_part_get_content_type (part);
	format = gmime_content_field_get_parameter (type, "format");
	if (format && !g_strcasecmp (format, "flowed"))
		return handle_text_plain_flowed (text, md);

	mail_html_write (md->html, md->stream, "\n<!-- text/plain -->\n");

	p = text;
	while (p) {
		/* Look for special cases. */
		for (i = 0; i < NSPECIALS; i++) {
			start = strstr (p, text_specials[i].start);
			if (start && (start == p || start[-1] == '\n'))
				break;
		}
		if (!start)
			break;

		/* Deal with special case */
		if (start != p) {
			subtext = g_strndup (p, start - p);
			mail_text_write (md->html, md->stream,
					 "%s", subtext);
			g_free (subtext);
		}
		p = text_specials[i].handler (start, md);
		if (p == start) {
			/* Oops. That failed. Output this line normally and
			 * skip over it.
			 */
			p = strchr (start, '\n');
			if (!p++)
				break;
			subtext = g_strndup (start, p - start);
			mail_text_write (md->html, md->stream,
					 "%s", subtext);
			g_free (subtext);
		} else if (p)
			mail_html_write (md->html, md->stream, "<hr>");
	}
	/* Finish up (or do the whole thing if there were no specials). */
	if (p)
		mail_text_write (md->html, md->stream, "%s", p);

	g_free (text);
	return TRUE;
}

static gboolean
handle_text_plain_flowed (char *buf, MailDisplay *md)
{
	char *text, *line, *eol, *p;
	int prevquoting = 0, quoting, len;
	gboolean br_pending = FALSE;

	mail_html_write (md->html, md->stream,
			 "\n<!-- text/plain, flowed -->\n<tt>\n");

	for (line = buf; *line; line = eol + 1) {
		/* Process next line */
		eol = strchr (line, '\n');
		if (eol)
			*eol = '\0';

		quoting = 0;
		for (p = line; *p == '>'; p++)
			quoting++;
		if (quoting != prevquoting) {
			mail_html_write (md->html, md->stream, "%s\n",
					 prevquoting == 0 ? "<i>\n" : "");
			while (quoting > prevquoting) {
				mail_html_write (md->html, md->stream,
						 "<blockquote>");
				prevquoting++;
			}
			while (quoting < prevquoting) {
				mail_html_write (md->html, md->stream,
						 "</blockquote>");
				prevquoting--;
			}
			mail_html_write (md->html, md->stream, "%s\n",
					 prevquoting == 0 ? "</i>\n" : "");
		} else if (br_pending) {
			mail_html_write (md->html, md->stream, "<br>\n");
			br_pending = FALSE;
		}

		if (*p == ' ')
			p++;

		/* replace '<' with '&lt;', etc. */
		text = e_text_to_html (p, E_TEXT_TO_HTML_CONVERT_SPACES |
				       E_TEXT_TO_HTML_CONVERT_URLS);
		if (text && *text)
			mail_html_write (md->html, md->stream, "%s", text);
		g_free (text);

		len = strlen (p);
		if (len == 0 || p[len - 1] != ' ' || !strcmp (p, "-- "))
			br_pending = TRUE;

		if (!eol)
			break;
	}
	g_free (buf);

	mail_html_write (md->html, md->stream, "</tt>\n");
	return TRUE;
}

static CamelMimePart *
fake_mime_part_from_data (const char *data, int len, const char *type)
{
	CamelStream *memstream;
	CamelDataWrapper *wrapper;
	CamelMimePart *part;

	memstream = camel_stream_mem_new_with_buffer (data, len);
	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream (wrapper, memstream);
	camel_data_wrapper_set_mime_type (wrapper, type);
	camel_object_unref (CAMEL_OBJECT (memstream));
	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (CAMEL_OBJECT (wrapper));
	camel_mime_part_set_disposition (part, "inline");
	return part;
}

static void
destroy_part (CamelObject *root, gpointer event_data, gpointer user_data)
{
	camel_object_unref (user_data);
}

static char *
decode_pgp (const char *ciphertext, MailDisplay *md)
{
	CamelException ex;
	char *plaintext;

	camel_exception_init (&ex);
#ifdef PGP_PROGRAM
	/* FIXME: multipart parts */
	if (g_datalist_get_data (md->data, "show_pgp")) {
		plaintext = mail_crypto_openpgp_decrypt (ciphertext, &ex);
		if (plaintext)
			return plaintext;
	}
#else
	camel_exception_set (&ex, CAMEL_EXCEPTION_SYSTEM,
			     "No GPG/PGP support available in this copy "
			     "of Evolution.");
#endif

	mail_html_write (md->html, md->stream,
			 "<table><tr valign=top><td>"
			 "<a href=\"x-evolution-decode-pgp:\">"
			 "<img src=\"%s\"></a></td><td>",
			 get_url_for_icon ("gnome-lockscreen.png", md));

	if (camel_exception_is_set (&ex)) {
		mail_html_write (md->html, md->stream,
				 "Encrypted message not displayed<br><br>\n");
		mail_error_write (md->html, md->stream,
				  camel_exception_get_description (&ex));
		camel_exception_clear (&ex);
	} else {
		mail_html_write (md->html, md->stream,
				 "Encrypted message<br><br>\n"
				 "Click icon to decrypt.");
	}

	mail_html_write (md->html, md->stream, "</td></tr></table>");
	return NULL;
}

static char *
try_inline_pgp (char *start, MailDisplay *md)
{
	char *end, *ciphertext, *plaintext;

	/* FIXME: This should deal with signed data as well. */

	end = strstr (start, "-----END PGP MESSAGE-----");
	if (!end)
		return start;

	end += sizeof ("-----END PGP MESSAGE-----") - 1;

	mail_html_write (md->html, md->stream, "<hr>");

	ciphertext = g_strndup (start, end - start);
	plaintext = decode_pgp (ciphertext, md);
	g_free (ciphertext);
	if (plaintext) {
		mail_html_write (md->html, md->stream,
				 "<table width=\"100%%\" border=2 "
				 "cellpadding=4><tr><td>");
		mail_text_write (md->html, md->stream, "%s", plaintext);
		mail_html_write (md->html, md->stream, "</td></tr></table>");
		g_free (plaintext);
	}

	return end;
}

static char *
try_uudecoding (char *start, MailDisplay *md)
{
	int mode, len, state = 0;
	char *filename, *estart, *p, *out, uulen = 0;
	guint32 save = 0;
	CamelMimePart *part;

	/* Make sure it's a real uudecode begin line:
	 * begin [0-7]+ .*
	 */
	mode = strtoul (start + 6, &p, 8);
	if (p == start + 6 || *p != ' ')
		return start;
	estart = strchr (start, '\n');
	if (!estart)
		return start;

	while (isspace ((unsigned char)*p))
		p++;
	filename = g_strndup (p, estart++ - p);

	/* Make sure there's an end line. */
	p = strstr (p, "\nend\n");
	if (!p) {
		g_free (filename);
		return start;
	}

	out = g_malloc (p - estart);
	len = uudecode_step (estart, p - estart, out, &state, &save, &uulen);

	part = fake_mime_part_from_data (out, len, "application/octet-stream");
	g_free (out);
	camel_mime_part_set_filename (part, filename);
	g_free (filename);
	camel_object_hook_event (CAMEL_OBJECT (md->current_message),
				 "finalize", destroy_part, part);

	mail_html_write (md->html, md->stream, "<hr>");
	call_handler_function (part, md);

	return p + 4;
}

static char *
try_inline_binhex (char *start, MailDisplay *md)
{
	char *p;
	CamelMimePart *part;

	/* Find data start. */
	p = strstr (start, "\n:");
	if (!p)
		return start;

	/* And data end. */
	p = strchr (p + 2, ':');
	if (!p || (*(p + 1) != '\n' && *(p + 1) != '\0'))
		return start;
	p += 2;

	part = fake_mime_part_from_data (start, p - start,
					 "application/mac-binhex40");
	camel_object_hook_event (CAMEL_OBJECT (md->current_message),
				 "finalize", destroy_part, part);

	mail_html_write (md->html, md->stream, "<hr>");
	call_handler_function (part, md);

	return p;
}

static void
free_byte_array (CamelObject *obj, gpointer event_data, gpointer user_data)
{
	/* We don't have to do a forward event here right now */
	g_byte_array_free (user_data, TRUE);
}

/* text/enriched (RFC 1896) or text/richtext (included in RFC 1341) */
static gboolean
handle_text_enriched (CamelMimePart *part, const char *mime_type,
		      MailDisplay *md)
{
	static GHashTable *translations = NULL;
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	GString *string;
	GByteArray *ba;
	char *text, *p, *xed;
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

	text = get_data_wrapper_text (wrapper);
	if (!text)
		return FALSE;

	if (!g_strcasecmp (mime_type, "text/richtext")) {
		enriched = FALSE;
		mail_html_write (md->html, md->stream,
				 "\n<!-- text/richtext -->\n");
	} else {
		enriched = TRUE;
		mail_html_write (md->html, md->stream,
				 "\n<!-- text/enriched -->\n");
	}

	/* This is not great code, but I don't feel like fixing it right
	 * now. I mean, it's just text/enriched...
	 */
	p = text;
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
	g_free (text);

	ba = g_byte_array_new ();
	g_byte_array_append (ba, (const guint8 *)string->str,
			     strlen (string->str));
	g_string_free (string, TRUE);

	xed = g_strdup_printf ("x-evolution-data:%p", part);
	mail_html_write (md->html, md->stream,
			 "<iframe src=\"%s\" frameborder=0 scrolling=no>"
			 "</iframe>", xed);
	add_url (xed, ba, md);
	camel_object_hook_event (CAMEL_OBJECT (md->current_message),
				 "finalize", free_byte_array, ba);

	return TRUE;
}

static gboolean
handle_text_html (CamelMimePart *part, const char *mime_type,
		  MailDisplay *md)
{
	mail_html_write (md->html, md->stream, "\n<!-- text/html -->\n");
	mail_html_write (md->html, md->stream,
			 "<iframe src=\"%s\" frameborder=0 scrolling=no>"
			 "</iframe>", get_cid (part, md));
	return TRUE;
}

static gboolean
handle_image (CamelMimePart *part, const char *mime_type, MailDisplay *md)
{
	mail_html_write (md->html, md->stream, "<img src=\"%s\">",
			 get_cid (part, md));
	return TRUE;
}

static gboolean
handle_multipart_mixed (CamelMimePart *part, const char *mime_type,
			MailDisplay *md)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *mp;
	int i, nparts;
	gboolean output = FALSE;

	g_return_val_if_fail (CAMEL_IS_MULTIPART (wrapper), FALSE);
	mp = CAMEL_MULTIPART (wrapper);

	nparts = camel_multipart_get_number (mp);	
	for (i = 0; i < nparts; i++) {
		if (i != 0 && output)
			mail_html_write (md->html, md->stream, "<hr>\n");

		part = camel_multipart_get_part (mp, i);

		output = call_handler_function (part, md);
	}

	return TRUE;
}

static gboolean
is_rfc2015 (CamelMimePart *part)
{
	int nparts;
	char *text;
	CamelDataWrapper *wrapper;
	CamelMultipart *mp;
	GMimeContentField *type;

	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	mp = CAMEL_MULTIPART (wrapper);
	nparts = camel_multipart_get_number (mp);
	if (nparts != 2)
		return FALSE;

	/* Check for application/pgp-encrypted in the first part. */
	part = camel_multipart_get_part (mp, 0);
	type = camel_mime_part_get_content_type (part);
	if (!gmime_content_field_is_type (type, "application", "pgp-encrypted"))
		return FALSE;

	/* Check version. */
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	text = get_data_wrapper_text (wrapper);
	if (!text || !strstr(text, "Version: 1")) {
		g_free(text);
		return FALSE;
	}
	g_free(text);

	/* Check for application/octet-stream in the second part. */
	part = camel_multipart_get_part(mp, 1);
	type = camel_mime_part_get_content_type (part);
	if (!gmime_content_field_is_type (type, "application", "octet-stream"))
		return FALSE;

	return TRUE;
}

static gboolean
handle_multipart_encrypted (CamelMimePart *part, const char *mime_type,
			    MailDisplay *md)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *mp;
	char *ciphertext, *plaintext;

	g_return_val_if_fail (CAMEL_IS_MULTIPART (wrapper), FALSE);
	mp = CAMEL_MULTIPART (wrapper);

	/* Currently we only handle RFC2015-style PGP encryption. */
	if (!is_rfc2015 (part))
		return handle_multipart_mixed (part, mime_type, md);

	part = camel_multipart_get_part (mp, 1);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	ciphertext = get_data_wrapper_text (wrapper);
	if (!ciphertext)
		return FALSE;

	plaintext = decode_pgp (ciphertext, md);
	if (plaintext) {
		CamelStream *memstream;

		memstream = camel_stream_mem_new_with_buffer (plaintext,
							      strlen (plaintext));
		part = camel_mime_part_new ();
		camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (part),
							  memstream);
		camel_object_unref (CAMEL_OBJECT (memstream));

		mail_html_write (md->html, md->stream,
				 "<table width=\"100%%\" border=2 "
				 "cellpadding=4><tr><td>");
		call_handler_function (part, md);
		mail_html_write (md->html, md->stream, "</td></tr></table>");
		camel_object_hook_event (CAMEL_OBJECT (md->current_message),
					 "finalize", destroy_part, part);
		g_free (plaintext);
	}

	return TRUE;
}

/* As seen in RFC 2387! */
static gboolean
handle_multipart_related (CamelMimePart *part, const char *mime_type,
			  MailDisplay *md)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *mp;
	CamelMimePart *body_part, *display_part = NULL;
	GMimeContentField *content_type;
	const char *start;
	int i, nparts;

	g_return_val_if_fail (CAMEL_IS_MULTIPART (wrapper), FALSE);
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
			return handle_multipart_mixed (part, mime_type, md);
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

		get_cid (body_part, md);
	}

	/* Now, display the displayed part. */
	return call_handler_function (display_part, md);
}

/* RFC 2046 says "display the last part that you are able to display". */
static CamelMimePart *
find_preferred_alternative (CamelMultipart *multipart, gboolean want_plain)
{
	int i, nparts;
	CamelMimePart *preferred_part = NULL;
	MailMimeHandler *handler;

	nparts = camel_multipart_get_number (multipart);
	for (i = 0; i < nparts; i++) {
		CamelMimePart *part = camel_multipart_get_part (multipart, i);
		char *mime_type = gmime_content_field_get_mime_type (
			camel_mime_part_get_content_type (part));

		g_strdown (mime_type);
		if (want_plain && !strcmp (mime_type, "text/plain"))
			return part;
		handler = mail_lookup_handler (mime_type);
		if (handler && (!preferred_part || !handler->generic))
			preferred_part = part;
		g_free (mime_type);
	}

	return preferred_part;
}

static gboolean
handle_multipart_alternative (CamelMimePart *part, const char *mime_type,
			      MailDisplay *md)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *multipart;
	CamelMimePart *mime_part;

	g_return_val_if_fail (CAMEL_IS_MULTIPART (wrapper), FALSE);
	multipart = CAMEL_MULTIPART (wrapper);

	mime_part = find_preferred_alternative (multipart, FALSE);
	if (mime_part)
		return call_handler_function (mime_part, md);
	else
		return handle_multipart_mixed (part, mime_type, md);
}

/* RFC 1740 */
static gboolean
handle_multipart_appledouble (CamelMimePart *part, const char *mime_type,
			      MailDisplay *md)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *multipart;

	g_return_val_if_fail (CAMEL_IS_MULTIPART (wrapper), FALSE);
	multipart = CAMEL_MULTIPART (wrapper);

	/* The first part is application/applefile and is not useful
	 * to us. The second part _may_ be displayable data. Most
	 * likely it's application/octet-stream though.
	 */
	part = camel_multipart_get_part (multipart, 1);
	return call_handler_function (part, md);
}

static gboolean
handle_message_rfc822 (CamelMimePart *part, const char *mime_type,
		       MailDisplay *md)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (wrapper), FALSE);

	mail_html_write (md->html, md->stream, "<blockquote>");
	mail_format_mime_message (CAMEL_MIME_MESSAGE (wrapper), md);
	mail_html_write (md->html, md->stream, "</blockquote>");

	return TRUE;
}

static gboolean
handle_message_external_body (CamelMimePart *part, const char *mime_type,
			      MailDisplay *md)
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

#if 0 /* FIXME */
	handle_mystery (part, md, url, "gnome-globe.png", desc,
			url ? "open it in a browser" : NULL);
#endif

	g_free (desc);
	g_free (url);
	return TRUE;
}

static gboolean
handle_via_bonobo (CamelMimePart *part, const char *mime_type,
		   MailDisplay *md)
{
	mail_html_write (md->html, md->stream,
			 "<object classid=\"%s\" type=\"%s\"></object>",
			 get_cid (part, md), mime_type);
	return TRUE;
}

char *
mail_get_message_body (CamelDataWrapper *data, gboolean want_plain, gboolean *is_html)
{
	CamelMultipart *mp;
	CamelMimePart *subpart;
	int i, nparts;
	char *subtext, *old;
	const char *boundary;
	char *text = NULL;
	GMimeContentField *mime_type;

	/* We only include text, message, and multipart bodies. */
	mime_type = camel_data_wrapper_get_mime_type_field (data);

	/* FIXME: This is wrong. We don't want to include large
	 * images. But if we don't do it this way, we don't get
	 * the headers...
	 */
	if (g_strcasecmp (mime_type->type, "message") == 0) {
		*is_html = FALSE;
		return get_data_wrapper_text (data);
	}

	if (g_strcasecmp (mime_type->type, "text") == 0) {
		*is_html = !g_strcasecmp (mime_type->subtype, "html");
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

		subpart = find_preferred_alternative (mp, want_plain);
		if (!subpart)
			return NULL;

		data = camel_medium_get_content_object (
			CAMEL_MEDIUM (subpart));
		return mail_get_message_body (data, want_plain, is_html);
	}

	nparts = camel_multipart_get_number (mp);

	/* Otherwise, concatenate all the parts that we can. If we find
	 * an HTML part in there though, return just that: We don't want
	 * to deal with merging HTML and non-HTML parts.
	 */
	boundary = camel_multipart_get_boundary (mp);
	for (i = 0; i < nparts; i++) {
		subpart = camel_multipart_get_part (mp, i);

		if (!mail_part_is_inline (subpart))
			continue;

		data = camel_medium_get_content_object (
			CAMEL_MEDIUM (subpart));
		subtext = mail_get_message_body (data, want_plain, is_html);
		if (!subtext)
			continue;
		if (*is_html) {
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
	gboolean want_plain, is_html;
	const char *repl_to, *message_id, *references;
	GList *to, *cc;
	MailConfigIdentity *id;
	gchar *sig_file = NULL;
	
	want_plain = !mail_config_send_html ();
	
	contents = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	text = mail_get_message_body (contents, want_plain, &is_html);
	
	id = mail_config_get_default_identity ();
	if (id)
	      sig_file = id->sig;
	
	composer = E_MSG_COMPOSER (e_msg_composer_new_with_sig_file (sig_file));
	
	/* Set the quoted reply text. */
	if (text) {
		char *repl_text;
		
		if (is_html) {
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
			if (*name) {
				fulladdr = g_strdup_printf ("\"%s\" <%s>",
							    name, addr);
			} else
				fulladdr = g_strdup (addr);
			
			/* Here I'll check to see if the cc:'d address is the address
			of the sender, and if so, don't add it to the cc: list; this
			is to fix Bugzilla bug #455. */
			
			if (strcmp (addr, id->address) != 0)
                                cc = g_list_append (cc, fulladdr);
		}
		
		recip = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
		i = 0;
		while (camel_internet_address_get (recip, i++, &name, &addr)) {
			if (*name) {
				fulladdr = g_strdup_printf ("\"%s\" <%s>", name, addr);
			} else
				fulladdr = g_strdup (addr);

			if (strcmp (addr, id->address) != 0)
                                cc = g_list_append (cc, fulladdr);
		}
	} else
		cc = NULL;
	
	/* Set the subject of the new message. */
	subject = (char *)camel_mime_message_get_subject (message);
	if (!subject)
		subject = g_strdup ("");
	else {
		if (!g_strncasecmp (subject, "Re: ", 4))
			subject = g_strdup (subject);
		else
			subject = g_strdup_printf ("Re: %s", subject);
	}
	
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
	} else if (references) {
		e_msg_composer_add_header (composer, "References", references);
	}
	
	return composer;
}
