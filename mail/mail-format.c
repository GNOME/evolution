/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors:
 *  Dan Winship <danw@ximian.com>
 *
 *  Copyright 2000, 2001 Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>    /* for isprint */
#include <string.h>   /* for strstr  */
#include <fcntl.h>

#include <liboaf/liboaf.h>
#include <libgnome/libgnome.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <gal/widgets/e-unicode.h>

#include <camel/camel-mime-utils.h>
#include <camel/camel-pgp-mime.h>
#include <camel/camel-stream-null.h>
#include <shell/e-setup.h>
#include <e-util/e-html-utils.h>

#include "mail.h"
#include "mail-tools.h"
#include "mail-display.h"
#include "mail-mt.h"
#include "mail-crypto.h"

static char *get_data_wrapper_text (CamelDataWrapper *data);

static char *try_inline_pgp (char *start, MailDisplay *md);
static char *try_inline_pgp_sig (char *start, MailDisplay *md);
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
static gboolean handle_multipart_signed      (CamelMimePart *part,
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
static gboolean format_mime_part (CamelMimePart *part, MailDisplay *md);

static void
free_url (gpointer key, gpointer value, gpointer data)
{
	g_free (key);
	if (data)
		g_byte_array_free (value, TRUE);
}

static void
free_part_urls (gpointer urls)
{
	g_hash_table_foreach (urls, free_url, NULL);
	g_hash_table_destroy (urls);
}

static void
free_data_urls (gpointer urls)
{
	g_hash_table_foreach (urls, free_url, GINT_TO_POINTER (1));
	g_hash_table_destroy (urls);
}

static char *
add_url (const char *kind, char *url, gpointer data, MailDisplay *md)
{
	GHashTable *urls;
	gpointer old_key, old_value;

	urls = g_datalist_get_data (md->data, kind);
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

	urls = g_datalist_get_data (md->data, "part_urls");
	if (!urls) {
		urls = g_hash_table_new (g_str_hash, g_str_equal);
		g_datalist_set_data_full (md->data, "part_urls", urls,
					  free_part_urls);
	}
	urls = g_datalist_get_data (md->data, "data_urls");
	if (!urls) {
		urls = g_hash_table_new (g_str_hash, g_str_equal);
		g_datalist_set_data_full (md->data, "data_urls", urls,
					  free_data_urls);
	}

	write_headers (mime_message, md);
	format_mime_part (CAMEL_MIME_PART (mime_message), md);
}


/**
 * mail_format_raw_message: 
 * @mime_message: the input mime message
 * @md: the MailDisplay to render into
 *
 * Writes a CamelMimeMessage source out into a MailDisplay
 **/
void
mail_format_raw_message (CamelMimeMessage *mime_message, MailDisplay *md)
{
	char *text, *html;

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (mime_message));
	
	text = get_data_wrapper_text (CAMEL_DATA_WRAPPER (mime_message));
	html = e_text_to_html (text, E_TEXT_TO_HTML_CONVERT_NL | E_TEXT_TO_HTML_CONVERT_SPACES | E_TEXT_TO_HTML_ESCAPE_8BIT);
	gtk_html_write (md->html, md->stream, "<tt>", 4);
	gtk_html_write (md->html, md->stream, html, strlen (html));
	gtk_html_write (md->html, md->stream, "</tt>", 5);
	g_free (html);
	g_free (text);
}

static const char *
get_cid (CamelMimePart *part, MailDisplay *md)
{
	char *cid;
	static int fake_cid_counter = 0;

	/* If we have a real Content-ID, use it. If we don't,
	 * make a (syntactically invalid, unique) fake one.
	 */
	if (camel_mime_part_get_content_id (part)) {
		cid = g_strdup_printf ("cid:%s",
				       camel_mime_part_get_content_id (part));
	} else
		cid = g_strdup_printf ("cid:@@@%d", fake_cid_counter++);

	return add_url ("part_urls", cid, part, md);
}

static const char *
get_location (CamelMimePart *part, MailDisplay *md)
{
	const char *loc;

	/* FIXME: relative URLs */
	loc = camel_mime_part_get_content_location (part);
	if (!loc)
		return NULL;

	return add_url ("part_urls", g_strdup (loc), part, md);
}

static const char *
get_url_for_icon (const char *icon_name, MailDisplay *md)
{
	char *icon_path, buf[1024], *url;
	int fd, nread;
	GByteArray *ba;

	/* FIXME: cache */

	if (*icon_name == '/')
		icon_path = g_strdup (icon_name);
	else {
		icon_path = gnome_pixmap_file (icon_name);
		if (!icon_path)
			return "file:///dev/null";
	}

	fd = open (icon_path, O_RDONLY);
	g_free (icon_path);
	if (fd == -1)
		return "file:///dev/null";

	ba = g_byte_array_new ();
	while (1) {
		nread = read (fd, buf, sizeof (buf));
		if (nread < 1)
			break;
		g_byte_array_append (ba, buf, nread);
	}
	close (fd);

	url = g_strdup_printf ("x-evolution-data:%p", ba);
	return add_url ("data_urls", url, ba, md);
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
	g_hash_table_insert (mime_function_table, "multipart/signed",
			     handle_multipart_signed);

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
	CamelContentType *content_type;
	char *type;
	gboolean anon;

	/* If it has an explicit disposition, return that. */
	disposition = camel_mime_part_get_disposition (part);
	if (disposition)
		return g_strcasecmp (disposition, "inline") == 0;

	/* Certain types should default to inline. FIXME: this should
	 * be customizable.
	 */
	content_type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (content_type, "message", "*"))
		return TRUE;

	/* Otherwise, display it inline if it's "anonymous", and
	 * as an attachment otherwise.
	 */
	type = header_content_type_simple (content_type);
	anon = is_anonymous (part, type);
	g_free (type);
	return anon;
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
	mail_html_write (md->html, md->stream, _("%s attachment"), htmlinfo);
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
format_mime_part (CamelMimePart *part, MailDisplay *md)
{
	CamelDataWrapper *wrapper;
	char *mime_type;
	MailMimeHandler *handler;
	gboolean output, is_inline;

	/* Record URLs associated with this part */
	get_cid (part, md);
	get_location (part, md);

	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));

	if (CAMEL_IS_MULTIPART (wrapper) &&
	    camel_multipart_get_number (CAMEL_MULTIPART (wrapper)) == 0) {
		mail_error_write (md->html, md->stream, _("Could not parse MIME message. Displaying as source."));
		if (mail_content_loaded (wrapper, md))
			handle_text_plain (part, "text/plain", md);
		return TRUE;
	}

	mime_type = camel_data_wrapper_get_mime_type (wrapper);
	g_strdown (mime_type);

	handler = mail_lookup_handler (mime_type);
	if (!handler) {
		char *id_type;

		id_type = mail_identify_mime_part (part, md);
		if (id_type) {
			g_free (mime_type);
			mime_type = id_type;
			handler = mail_lookup_handler (id_type);
		}
	}

	is_inline = mail_part_is_inline (part);
	attachment_header (part, mime_type, is_inline, md);
	if (handler && handler->builtin && is_inline &&
	    mail_content_loaded (wrapper, md))
		output = (*handler->builtin) (part, mime_type, md);
	else
		output = TRUE;

	g_free (mime_type);
	return output;
}

/* flags for write_field_to_stream */
enum {
	WRITE_BOLD=1,
	WRITE_NOCOLUMNS=2,
};

static void
write_field_row_begin (const char *name, gint flags, GtkHTML *html, GtkHTMLStream *stream)
{
	char *encoded_name;
	gboolean bold = (flags & WRITE_BOLD);
	gboolean nocolumns = (flags & WRITE_NOCOLUMNS);

	encoded_name = e_utf8_from_gtk_string (GTK_WIDGET (html), name);

	if (nocolumns) {
		mail_html_write (html, stream, "<tr><td>%s%s:%s ",
				 bold ? "<b>" : "", encoded_name,
				 bold ? "</b>" : "");
	} else {
		mail_html_write (html, stream, "<tr><%s align=\"right\" valign=\"top\">%s:</%s><td>",
				 bold ? "th" : "td", encoded_name, bold ? "th" : "td");
	}

	g_free (encoded_name);
}

static void
write_date (CamelMimeMessage *message, int flags, GtkHTML *html, GtkHTMLStream *stream)
{
	char *datestr;
	time_t date;
	int offset;
	
	write_field_row_begin (_("Date"), flags, html, stream);
	
	date = camel_mime_message_get_date (message, &offset);
	datestr = header_format_date (date, offset);
	
	mail_html_write (html, stream, "%s</td> </tr>", datestr);
	
	g_free (datestr);
}

static void
write_text_header (const char *name, const char *value, int flags, GtkHTML *html, GtkHTMLStream *stream)
{
	char *encoded;

	if (value && *value)
		encoded = e_text_to_html (value, E_TEXT_TO_HTML_CONVERT_NL | E_TEXT_TO_HTML_CONVERT_URLS);
	else
		encoded = "";

	write_field_row_begin (name, flags, html, stream);

	mail_html_write (html, stream, "%s</td> </tr>", encoded);

	if (value)
		g_free (encoded);
}

static gchar *
elide_quotes (const gchar *str)
{
	gchar *cpy = g_strdup (str);
	gchar *c = cpy;

	if (c) {
		while (*c) {
			if (*c == '"')
				*c = '\'';
			++c;
		}
	}
	return cpy;
}

static void
write_address(MailDisplay *md, const CamelInternetAddress *addr, const char *field_name, int flags)
{
	const char *name, *email;
	gint i;

	if (addr == NULL || !camel_internet_address_get (addr, 0, NULL, NULL))
		return;

	write_field_row_begin (field_name, flags, md->html, md->stream);

	i = 0;
	while (camel_internet_address_get (addr, i, &name, &email)) {
		gboolean have_name = name && *name;
		gboolean have_email = email && *email;
		gchar *name_arg = NULL;
		gchar *email_arg = NULL;
		gchar *name_disp = NULL;
		gchar *email_disp = NULL;

		if (have_name) {
			name_arg = elide_quotes (name);
			name_disp = e_text_to_html (name, 0);
		}

		if (have_email) {
			email_arg = elide_quotes (email);
			email_disp = e_text_to_html (email, 0);
		}

		if (i)
			mail_html_write (md->html, md->stream, ", ");
		
		if (have_email || have_name) {

			if (!have_email) {
				email_arg = g_strdup ("???");
				email_disp = g_strdup ("???");
			}
			
			if (have_name) {
				mail_html_write (md->html, md->stream,
						 "%s &lt;<a href=\"mailto:%s <%s>\">%s</a>&gt;",
						 name_disp, name_arg, email_arg, email_disp);
			} else {
				mail_html_write (md->html, md->stream,
						 "<a href=\"mailto:%s\">%s</a>",
						 email_arg, email_disp);
			}

		} else {

			mail_html_write (md->html, md->stream, "<i>Bad Address</i>");
		}

		g_free (name_arg);
		g_free (email_arg);
		g_free (name_disp);
		g_free (email_disp);

		++i;
	}
	mail_html_write (md->html, md->stream, "</td></tr>");
}


static void
write_header (CamelMimeMessage *message, MailDisplay *md,
	      const char *name, const char *value, int flags)
{
	if (!g_strcasecmp (name, "From")) {
		write_address (md, camel_mime_message_get_from (message),
			       _("From"), flags | WRITE_BOLD);
	} else if (!g_strcasecmp (name, "Reply-To")) {
		write_address (md, camel_mime_message_get_reply_to (message),
			       _("Reply-To"), flags);
	} else if (!g_strcasecmp (name, "To")) {
		write_address (md, camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO),
			       _("To"), flags | WRITE_BOLD);
	} else if (!g_strcasecmp (name, "Cc")) {
		write_address (md, camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC),
			       _("Cc"), flags | WRITE_BOLD);
	} else if (!g_strcasecmp (name, "Subject")) {	
		write_text_header (_("Subject"), camel_mime_message_get_subject (message), flags | WRITE_BOLD, md->html, md->stream);
	} else if (!g_strcasecmp (name, "Date")) {	
		write_date (message, flags | WRITE_BOLD, md->html, md->stream);
	} else
		write_text_header (name, value, flags, md->html, md->stream);
}

static void
write_headers (CamelMimeMessage *message, MailDisplay *md)
{
	GArray *gheaders;
	CamelMediumHeader *headers, default_headers[] = {
		{ "From", NULL }, { "Reply-To", NULL },
		{ "To", NULL }, { "Cc" , NULL }, { "Subject", NULL },
		{ "Date", NULL }
	};
	int i, len, flags;
	gboolean full = GPOINTER_TO_INT (g_datalist_get_data (md->data, "full_headers"));

	mail_html_write (md->html, md->stream,
			 "<font color=\"#000000\">"
			 "<table bgcolor=\"#000000\" width=\"100%%\" "
			 "cellspacing=0 cellpadding=1><tr><td>"
			 "<table bgcolor=\"#EEEEEE\" width=\"100%%\" cellpadding=0 cellspacing=0>"
			 "<tr><td><table>\n");

	if (full) {
		gheaders = camel_medium_get_headers (CAMEL_MEDIUM (message));
		headers = (CamelMediumHeader *)gheaders->data;
		len = gheaders->len;
		flags = WRITE_NOCOLUMNS;
	} else {
		gheaders = NULL;
		headers = default_headers;
		len = sizeof (default_headers) / sizeof (default_headers[0]);
		flags = 0;
	}

	for (i = 0; i < len; i++)
		write_header (message, md, headers[i].name, headers[i].value, flags);

	if (gheaders)
		camel_medium_free_headers (CAMEL_MEDIUM (message), gheaders);

	mail_html_write (md->html, md->stream,
			 "</table></td></tr></table></td></tr></table></font>");
}

static void
load_offline_content (MailDisplay *md, gpointer data)
{
	CamelDataWrapper *wrapper = data;
	CamelStream *stream;

	stream = camel_stream_null_new ();
	camel_data_wrapper_write_to_stream (wrapper, stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	camel_object_unref (CAMEL_OBJECT (wrapper));
}

gboolean
mail_content_loaded (CamelDataWrapper *wrapper, MailDisplay *md)
{
	if (!camel_data_wrapper_is_offline (wrapper))
		return TRUE;

	camel_object_ref (CAMEL_OBJECT (wrapper));
	mail_display_redisplay_when_loaded (md, wrapper, load_offline_content, wrapper);
	return FALSE;
}

/* Return the contents of a data wrapper, or %NULL if it contains only
 * whitespace.
 */
static char *
get_data_wrapper_text (CamelDataWrapper *wrapper)
{
	CamelStream *memstream;
	GByteArray *ba;
	char *text, *end;

	memstream = camel_stream_mem_new ();
	ba = g_byte_array_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (memstream), ba);
	camel_data_wrapper_write_to_stream (wrapper, memstream);
	camel_object_unref (CAMEL_OBJECT (memstream));

	for (text = ba->data, end = text + ba->len; text < end; text++) {
		if (!isspace ((unsigned char)*text))
			break;
	}

	if (text >= end) {
		g_byte_array_free (ba, TRUE);
		return NULL;
	}

	g_byte_array_append (ba, "", 1);
	text = ba->data;
	g_byte_array_free (ba, FALSE);
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
	{ "-----BEGIN PGP SIGNED MESSAGE-----\n", try_inline_pgp_sig },
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
	char *text, *p, *start;
	CamelContentType *type;
	gboolean check_specials;
	const char *format;
	int i;

	text = get_data_wrapper_text (wrapper);
	if (!text)
		return FALSE;

	/* Check for RFC 2646 flowed text. */
	type = camel_mime_part_get_content_type (part);
	format = header_content_type_param (type, "format");
	if (format && !g_strcasecmp (format, "flowed"))
		return handle_text_plain_flowed (text, md);

	mail_html_write (md->html, md->stream, "\n<!-- text/plain -->\n<font size=\"-3\">&nbsp</font><br>\n");

	/* Only look for binhex and stuff if this is real text/plain.
	 * (and not, say, application/mac-binhex40 that mail-identify
	 * has decided to call text/plain because it starts with English
	 * text...)
	 */
	check_specials = (g_strcasecmp (mime_type, "text/plain") == 0);

	p = text;
	while (p && check_specials) {
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
			/* the %.*s thing just grabs upto start-p chars; go read ANSI C */
			mail_text_write (md->html, md->stream, "%.*s", start-p, p);
		}
		p = text_specials[i].handler (start, md);
		if (p == start) {
			/* Oops. That failed. Output this line normally and
			 * skip over it.
			 */
			p = strchr (start, '\n');
			if (!p++)
				break;
			mail_text_write (md->html, md->stream, "%.*s", p-start, start);
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
	int prevquoting = 0, quoting, len, br_pending = 0;
	guint32 citation_color = mail_config_get_citation_color ();

	mail_html_write (md->html, md->stream,
			 "\n<!-- text/plain, flowed -->\n<font size=\"-3\">&nbsp</font><br>\n<tt>\n");

	for (line = buf; *line; line = eol + 1) {
		/* Process next line */
		eol = strchr (line, '\n');
		if (eol)
			*eol = '\0';

		quoting = 0;
		for (p = line; *p == '>'; p++)
			quoting++;
		if (quoting != prevquoting) {
			if (prevquoting == 0) {
				mail_html_write (md->html, md->stream,
						 "<font color=\"#%06x\">",
						 citation_color);
				if (br_pending)
					br_pending--;
			}
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
			if (quoting == 0) {
				mail_html_write (md->html, md->stream,
						 "</font>\n");
				if (br_pending)
					br_pending--;
			}
		}

		if (*p == ' ')
			p++;
		len = strlen (p);
		if (len == 0) {
			br_pending++;
			continue;
		}

		while (br_pending) {
			mail_html_write (md->html, md->stream, "<br>\n");
			br_pending--;
		}

		/* replace '<' with '&lt;', etc. */
		text = e_text_to_html (p, E_TEXT_TO_HTML_CONVERT_SPACES |
				       E_TEXT_TO_HTML_CONVERT_URLS);
		if (text && *text)
			mail_html_write (md->html, md->stream, "%s", text);
		g_free (text);

		if (p[len - 1] != ' ' || !strcmp (p, "-- "))
			br_pending++;

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

static void
decode_pgp (CamelStream *ciphertext, CamelStream *plaintext, MailDisplay *md)
{
	CamelException ex;
	
	camel_exception_init (&ex);
	
	/* FIXME: multipart parts */
	/* another FIXME: this doesn't have to return plaintext you realize... */
	if (g_datalist_get_data (md->data, "show_pgp")) {
		CamelPgpContext *ctx;
		
		ctx = camel_pgp_context_new (session, mail_config_get_pgp_type (),
					     mail_config_get_pgp_path ());
		
		if (ctx) {
			camel_pgp_decrypt (ctx, ciphertext, plaintext, &ex);
			camel_object_unref (CAMEL_OBJECT (ctx));
			camel_stream_reset (plaintext);
		} else {
			camel_exception_setv (&ex, CAMEL_EXCEPTION_SYSTEM,
					      _("No GPG/PGP program configured."));
		}
		
		if (!camel_exception_is_set (&ex))
			return;
	}
	
	mail_html_write (md->html, md->stream,
			 "<table><tr valign=top><td>"
			 "<a href=\"x-evolution-decode-pgp:\">"
			 "<img src=\"%s\"></a></td><td>",
			 get_url_for_icon ("gnome-lockscreen.png", md));
	
	if (camel_exception_is_set (&ex)) {
		mail_html_write (md->html, md->stream, "%s<br><br>\n",
				 _("Encrypted message not displayed"));
		mail_error_write (md->html, md->stream,
				  camel_exception_get_description (&ex));
		camel_exception_clear (&ex);
	} else {
		mail_html_write (md->html, md->stream, "%s<br><br>\n%s",
				 _("Encrypted message"),
				 _("Click icon to decrypt."));
	}
	
	mail_html_write (md->html, md->stream, "</td></tr></table>");
}

static char *
try_inline_pgp (char *start, MailDisplay *md)
{
	CamelStream *ciphertext, *plaintext;
	GByteArray *buffer;
	char *end;
	
	end = strstr (start, "-----END PGP MESSAGE-----");
	if (!end)
		return start;
	
	end += strlen ("-----END PGP MESSAGE-----") - 1;
	
	mail_html_write (md->html, md->stream, "<hr>");
	
	/* FIXME: uhm, pgp decrypted data doesn't have to be plaintext
	 * however, I suppose that since it was 'inline', it probably is */
	
	ciphertext = camel_stream_mem_new ();
	camel_stream_write (ciphertext, start, end - start);
	camel_stream_reset (ciphertext);
	
	plaintext = camel_stream_mem_new ();
	decode_pgp (ciphertext, plaintext, md);
	camel_object_unref (CAMEL_OBJECT (ciphertext));
	
	buffer = CAMEL_STREAM_MEM (plaintext)->buffer;
	if (buffer && buffer->len) {
		mail_html_write (md->html, md->stream,
				 "<table width=\"100%%\" border=2 "
				 "cellpadding=4><tr><td>");
		mail_text_write (md->html, md->stream, "%.*s", buffer->len, buffer->data);
		mail_html_write (md->html, md->stream, "</td></tr></table>");
	}
	
	camel_object_unref (CAMEL_OBJECT (plaintext));
	
	return end;
}

static void
mail_write_authenticity (MailDisplay *md, CamelCipherValidity *valid)
{
	/* Now display the "seal-of-authenticity" or something... */
	if (valid && camel_cipher_validity_get_valid (valid)) {
		mail_html_write (md->html, md->stream,
				 "<hr>\n<table><tr valign=top>"
				 "<td><img src=\"%s\"></td>"
				 "<td><font size=-1>%s<br><br>",
				 get_url_for_icon ("wax-seal2.png", md),
				 _("This message is digitally signed and "
				   "has been found to be authentic."));
	} else {
		mail_html_write (md->html, md->stream,
				 "<hr>\n<table><tr valign=top>"
				 "<td><img src=\"%s\"></td>"
				 "<td><font size=-1>%s<br><br>",
				 get_url_for_icon ("wax-seal-broken.png", md),
				 _("This message is digitally signed but can "
				   "not be proven to be authentic."));
	}
	
	if (valid && camel_cipher_validity_get_description (valid)) {
		mail_error_write (md->html, md->stream,
				  camel_cipher_validity_get_description (valid));
		mail_html_write (md->html, md->stream, "<br><br>");
	}
}

static char *
try_inline_pgp_sig (char *start, MailDisplay *md)
{
	CamelCipherValidity *valid = NULL;
	CamelPgpContext *context;
	char *end, *msg_start, *pgp_start;
	
	msg_start = strstr (start, "-----BEGIN PGP SIGNED MESSAGE-----");
	if (msg_start) {
		/* skip over -----BEGIN PGP SIGNED MESSAGE----- */
		msg_start = strchr (msg_start, '\n');
		if (!msg_start++)
			return start;
		
		/* skip over Hash: header */
		msg_start = strchr (msg_start, '\n');
		if (!msg_start++)
			return start;
	} else {
		/* Some MUAs don't enclose the signed text in
                   -----BEGIN PGP SIGNED MESSAGE----- */
		msg_start = start;
	}
	
	/* find the beginning of the signature block */
	pgp_start = strstr (msg_start, "-----BEGIN PGP SIGNATURE-----");
	if (!pgp_start)
		return start;
	
	/* find the end of the pgp signature block */
	end = strstr (pgp_start, "-----END PGP SIGNATURE-----");
	if (!end)
		return start;
	
	end += strlen ("-----END PGP SIGNATURE-----");
	
	mail_html_write (md->html, md->stream, "<hr>");
	
	context = camel_pgp_context_new (session, mail_config_get_pgp_type (),
					 mail_config_get_pgp_path ());
	
	if (context) {
		CamelStream *ciphertext;
		CamelException *ex;
		
		ex = camel_exception_new ();
		
		ciphertext = camel_stream_mem_new ();
		camel_stream_write (ciphertext, start, end - start);
		camel_stream_reset (ciphertext);
		
		valid = camel_pgp_verify (context, ciphertext, NULL, ex);
		camel_object_unref (CAMEL_OBJECT (ciphertext));
		camel_object_unref (CAMEL_OBJECT (context));
		
		camel_exception_free (ex);
	}
	
	mail_text_write (md->html, md->stream, "%.*s", pgp_start - msg_start, msg_start);
	
	mail_write_authenticity (md, valid);
	
	mail_html_write (md->html, md->stream, "</font></td></table>");
	
	camel_cipher_validity_free (valid);
	
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
	format_mime_part (part, md);

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
	format_mime_part (part, md);

	return p;
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
	add_url ("data_urls", xed, ba, md);

	return TRUE;
}

static gboolean
handle_text_html (CamelMimePart *part, const char *mime_type,
		  MailDisplay *md)
{
	const char *location;
	
	mail_html_write (md->html, md->stream, "\n<!-- text/html -->\n");
	
	/* FIXME: deal with relative URLs */
	location = get_location (part, md);
	if (!location)
		location = get_cid (part, md);
	mail_html_write (md->html, md->stream,
			 "<iframe src=\"%s\" frameborder=0 scrolling=no>"
			 "</iframe>", location);
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
		
		output = format_mime_part (part, md);
	}
	
	return TRUE;
}

static gboolean
handle_multipart_encrypted (CamelMimePart *part, const char *mime_type,
			    MailDisplay *md)
{
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;
	CamelException ex;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	
	g_return_val_if_fail (CAMEL_IS_MULTIPART (wrapper), FALSE);
	
	/* Currently we only handle RFC2015-style PGP encryption. */
	if (!camel_pgp_mime_is_rfc2015_encrypted (part))
		return handle_multipart_mixed (part, mime_type, md);
	
	camel_exception_init (&ex);
	mime_part = mail_crypto_pgp_mime_part_decrypt (part, &ex);
	
	if (camel_exception_is_set (&ex)) {
		/* I guess we just treat this as a multipart/mixed */
		camel_exception_clear (&ex);
		return handle_multipart_mixed (part, mime_type, md);
	} else {
		gboolean retcode;
		
		retcode = format_mime_part (mime_part, md);
		camel_object_unref (CAMEL_OBJECT (mime_part));
		
		return retcode;
	}
}

static gboolean
handle_multipart_signed (CamelMimePart *part, const char *mime_type,
			 MailDisplay *md)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *mp;
	CamelException *ex;
	gboolean output = FALSE;
	CamelCipherValidity *valid;
	int nparts, i;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	
	g_return_val_if_fail (CAMEL_IS_MULTIPART (wrapper), FALSE);
	
	ex = camel_exception_new ();
	
	if (camel_pgp_mime_is_rfc2015_signed (part)) {
		valid = mail_crypto_pgp_mime_part_verify (part, ex);
	} else if (camel_smime_is_smime_v3_signed (part)) {
		valid = mail_crypto_smime_part_verify (part, ex);
	} else {
		camel_exception_free (ex);
		return handle_multipart_mixed (part, mime_type, md);
	}
	
	if (!valid) {
		camel_exception_free (ex);
		return handle_multipart_mixed (part, mime_type, md);
	}
	
	camel_exception_free (ex);
	
	/* now display all the subparts *except* the signature (last part) */
	mp = CAMEL_MULTIPART (wrapper);
	
	nparts = camel_multipart_get_number (mp);	
	for (i = 0; i < nparts - 1; i++) {
		if (i != 0 && output)
			mail_html_write (md->html, md->stream, "<hr>\n");
		
		part = camel_multipart_get_part (mp, i);
		
		output = format_mime_part (part, md);
	}
	
	mail_write_authenticity (md, valid);
	camel_cipher_validity_free (valid);
	
	mail_html_write (md->html, md->stream, "</font></td></table>");
	
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
	CamelContentType *content_type;
	const char *start;
	int i, nparts;

	g_return_val_if_fail (CAMEL_IS_MULTIPART (wrapper), FALSE);
	
	mp = CAMEL_MULTIPART (wrapper);
	nparts = camel_multipart_get_number (mp);	

	content_type = camel_mime_part_get_content_type (part);
	start = header_content_type_param (content_type, "start");
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
	} else {
		/* No start parameter, so it defaults to the first part. */
		display_part = camel_multipart_get_part (mp, 0);
	}

	if (!display_part) {
		/* Oops. Hrmph. */
		return handle_multipart_mixed (part, mime_type, md);
	}

	/* Record the Content-ID/Content-Location of any non-displayed parts. */
	for (i = 0; i < nparts; i++) {
		body_part = camel_multipart_get_part (mp, i);
		if (body_part == display_part)
			continue;

		get_cid (body_part, md);
		get_location (body_part, md);
	}

	/* Now, display the displayed part. */
	return format_mime_part (display_part, md);
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
		CamelContentType *type = camel_mime_part_get_content_type (part);
		char *mime_type = header_content_type_simple (type);

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
		return format_mime_part (mime_part, md);
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
	return format_mime_part (part, md);
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
	CamelContentType *type;
	const char *access_type;
	char *url = NULL, *desc = NULL;

	type = camel_mime_part_get_content_type (part);
	access_type = header_content_type_param (type, "access-type");
	if (!access_type)
		goto fallback;

	if (!g_strcasecmp (access_type, "ftp") ||
	    !g_strcasecmp (access_type, "anon-ftp")) {
		const char *name, *site, *dir, *mode, *ftype;
		char *path;

		name = header_content_type_param (type, "name");
		site = header_content_type_param (type, "site");
		if (name == NULL || site == NULL)
			goto fallback;
		dir = header_content_type_param (type, "directory");
		mode = header_content_type_param (type, "mode");

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
		desc = g_strdup_printf (_("Pointer to FTP site (%s)"), url);
	} else if (!g_strcasecmp (access_type, "local-file")) {
		const char *name, *site;

		name = header_content_type_param (type, "name");
		if (name == NULL)
			goto fallback;
		site = header_content_type_param (type, "site");

		url = g_strdup_printf ("file://%s%s", *name == '/' ? "" : "/",
				       name);
		if (site) {
			desc = g_strdup_printf (_("Pointer to local file (%s) "
						  "valid at site \"%s\""),
						name, site);
		} else {
			desc = g_strdup_printf (_("Pointer to local file (%s)"),
						name);
		}
	} else if (!g_strcasecmp (access_type, "URL")) {
		const char *urlparam;
		char *s, *d;

		/* RFC 2017 */

		urlparam = header_content_type_param (type, "url");
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
			desc = g_strdup_printf (_("Pointer to unknown "
						  "external data "
						  "(\"%s\" type)"),
						access_type);
		} else
			desc = g_strdup (_("Malformed external-body part."));
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
	CamelContentType *mime_type;
	
	/* We only include text, message, and multipart bodies. */
	mime_type = camel_data_wrapper_get_mime_type_field (data);
	
	/* FIXME: This is wrong. We don't want to include large
	 * images. But if we don't do it this way, we don't get
	 * the headers...
	 */
	if (header_content_type_is (mime_type, "message", "*")) {
		*is_html = FALSE;
		return get_data_wrapper_text (data);
	}
	
	if (header_content_type_is (mime_type, "text", "*")) {
		*is_html = header_content_type_is (mime_type, "text", "html");
		return get_data_wrapper_text (data);
	}
	
	/* If it's not message and it's not text, and it's not
	 * multipart, we don't want to deal with it.
	 */
	if (!header_content_type_is (mime_type, "multipart", "*"))
		return NULL;
	
	mp = CAMEL_MULTIPART (data);
	
	if (header_content_type_is (mime_type, "multipart", "alternative")) {
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
			text = g_strdup_printf ("%s\n--%s\n%s", old,
						boundary, subtext);
			g_free (subtext);
			g_free (old);
		} else
			text = subtext;
	}
	
	return text;
}
