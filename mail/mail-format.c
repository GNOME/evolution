/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: Dan Winship <danw@ximian.com>
 *          Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000, 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>   /* for strstr  */
#include <ctype.h>
#include <fcntl.h>

#include <liboaf/liboaf.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <gal/widgets/e-unicode.h>
#include <gal/util/e-iconv.h>

#include <camel/camel-mime-utils.h>
#include <camel/camel-pgp-mime.h>
#include <camel/camel-stream-null.h>
#include <camel/camel-multipart-signed.h>
#include <shell/e-setup.h>
#include <e-util/e-html-utils.h>
#include <gal/util/e-unicode-i18n.h>

#include "mail.h"
#include "mail-tools.h"
#include "mail-display.h"
#include "mail-mt.h"
#include "mail-crypto.h"

static char *try_inline_pgp (char *start, CamelMimePart *part,
			     guint offset, MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static char *try_inline_pgp_sig (char *start, CamelMimePart *part,
				 guint offset, MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static char *try_uudecoding (char *start, CamelMimePart *part,
			     guint offset, MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static char *try_inline_binhex (char *start, CamelMimePart *part,
				guint offset, MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);

static gboolean handle_text_plain            (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_text_plain_flowed     (char *text,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_text_enriched         (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_text_html             (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_image                 (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_multipart_mixed       (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_multipart_related     (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_multipart_alternative (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_multipart_appledouble (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_multipart_encrypted   (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_multipart_signed      (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_message_rfc822        (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
static gboolean handle_message_external_body (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);

static gboolean handle_via_bonobo            (CamelMimePart *part,
					      const char *mime_type,
					      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);

/* writes the header info for a mime message into an html stream */
static void write_headers (CamelMimeMessage *message, MailDisplay *md,
			   GtkHTML *html, GtkHTMLStream *stream);

/* dispatch html printing via mimetype */
static gboolean format_mime_part (CamelMimePart *part, MailDisplay *md,
				  GtkHTML *html, GtkHTMLStream *stream);

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

/**
 * mail_format_mime_message: 
 * @mime_message: the input mime message
 * @md: the MailDisplay to render into
 *
 * Writes a CamelMimeMessage out into a MailDisplay
 **/
void
mail_format_mime_message (CamelMimeMessage *mime_message, MailDisplay *md,
			  GtkHTML *html, GtkHTMLStream *stream)
{
	GHashTable *hash;
	
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (mime_message));

	hash = g_datalist_get_data (md->data, "part_urls");
	if (!hash) {
		hash = g_hash_table_new (g_str_hash, g_str_equal);
		g_datalist_set_data_full (md->data, "part_urls", hash,
					  free_part_urls);
	}
	hash = g_datalist_get_data (md->data, "data_urls");
	if (!hash) {
		hash = g_hash_table_new (g_str_hash, g_str_equal);
		g_datalist_set_data_full (md->data, "data_urls", hash,
					  free_data_urls);
	}
	
	hash = g_datalist_get_data (md->data, "attachment_states");
	if (!hash) {
		hash = g_hash_table_new (NULL, NULL);
		g_datalist_set_data_full (md->data, "attachment_states", hash,
					  (GDestroyNotify) g_hash_table_destroy);
	}
	hash = g_datalist_get_data (md->data, "fake_parts");
	if (!hash) {
		hash = g_hash_table_new (NULL, NULL);
		g_datalist_set_data_full (md->data, "fake_parts", hash,
					  (GDestroyNotify) g_hash_table_destroy);
	}
	
	write_headers (mime_message, md, html, stream);
	format_mime_part (CAMEL_MIME_PART (mime_message), md, html, stream);
}


/**
 * mail_format_raw_message: 
 * @mime_message: the input mime message
 * @md: the MailDisplay to render into
 *
 * Writes a CamelMimeMessage source out into a MailDisplay
 **/
void
mail_format_raw_message (CamelMimeMessage *mime_message, MailDisplay *md,
			 GtkHTML *html, GtkHTMLStream *stream)
{
	GByteArray *bytes;
	char *html_str;
	
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (mime_message));
	
	if (!mail_content_loaded (CAMEL_DATA_WRAPPER (mime_message), md,
				  TRUE, NULL, html, NULL))
		return;
	
	mail_html_write (html, stream,
			 "<table cellspacing=0 cellpadding=10 width=\"100%\"><tr><td><tt>\n");
	
	bytes = mail_format_get_data_wrapper_text (CAMEL_DATA_WRAPPER (mime_message), md);
	if (bytes) {
		g_byte_array_append (bytes, "", 1);
		html_str = e_text_to_html (bytes->data, E_TEXT_TO_HTML_CONVERT_NL |
					   E_TEXT_TO_HTML_CONVERT_SPACES | E_TEXT_TO_HTML_ESCAPE_8BIT);
		g_byte_array_free (bytes, TRUE);
		
		mail_html_write (html, stream, html_str);
		g_free (html_str);
	}
	
	mail_html_write (html, stream, "</tt></td></tr></table>");
}

static const char *
get_cid (CamelMimePart *part, MailDisplay *md)
{
	static int fake_cid_counter = 0;
	char *cid;
	
	/* If we have a real Content-ID, use it. If we don't,
	 * make a (syntactically invalid, unique) fake one.
	 */
	if (camel_mime_part_get_content_id (part)) {
		cid = g_strdup_printf ("cid:%s",
				       camel_mime_part_get_content_id (part));
	} else
		cid = g_strdup_printf ("cid:@@@%d", fake_cid_counter++);
	
	return mail_display_add_url (md, "part_urls", cid, part);
}

static const char *
get_location (CamelMimePart *part, MailDisplay *md)
{
	CamelURL *base;
	const char *loc;
	char *location;
	
	base = mail_display_get_content_location (md);
	
	loc = camel_mime_part_get_content_location (part);
	if (!loc) {
		if (!base)
			return NULL;
		
		location = camel_url_to_string (base, 0);
		return mail_display_add_url (md, "part_urls", location, part);
	}
	
	/* kludge: If the multipart/related does not have a
           Content-Location header and the HTML part doesn't contain a
           Content-Location header either, then we will end up
           generating a invalid unique identifier in the form of
           "cid:@@@%d" for use in GtkHTML's iframe src url. This means
           that when GtkHTML requests a relative URL, it will request
           "cid:/%s" */
	mail_display_add_url (md, "part_urls", g_strdup_printf ("cid:/%s", loc), part);
	
	if (!strchr (loc, ':') && base) {
		CamelURL *url;
		
		mail_display_add_url (md, "part_urls", g_strdup (loc), part);
		
		url = camel_url_new_with_base (base, loc);
		location = camel_url_to_string (url, 0);
		camel_url_free (url);
	} else {
		location = g_strdup (loc);
	}
	
	return mail_display_add_url (md, "part_urls", location, part);
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
	 * and unrecognized multipart subtypes as multipart/mixed.  */
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

	prop = oaf_server_info_prop_find (component, "repo_ids");
	if (!prop || prop->v._d != OAF_P_STRINGV)
		return FALSE;

	stringv = prop->v._u.value_stringv;
	for (i = 0; i < stringv._length; i++) {
		if (!g_strcasecmp ("IDL:Bonobo/PersistStream:1.0", stringv._buffer[i]))
			break;
	}

	/* got to end of list with no persist stream? */

	if (i >= stringv._length)
		return FALSE;

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
	const char *p;
	GList *components, *iter;

	if (mime_handler_table == NULL)
		setup_mime_tables ();

	/* See if we've already found it. */
	handler = g_hash_table_lookup (mime_handler_table, mime_type);
	if (handler)
		return handler;

	/* Special case MIME type: application/octet-stream
	 * The point of this type is that there isn't a handler. 
	 */
	if (strcmp (mime_type, "application/octet-stream") == 0)
		return NULL;
	
	/* No. Create a new one and look up application and full type
	 * handler. If we find a builtin, create the handler and
	 * register it.
	 */
	handler = g_new0 (MailMimeHandler, 1);
	handler->applications =
		gnome_vfs_mime_get_short_list_applications (mime_type);
	handler->builtin =
		g_hash_table_lookup (mime_function_table, mime_type);

	if (handler->builtin) {
		handler->generic = FALSE;
		goto reg;
	}

	/* Try for the first matching component. (we don't use get_short_list_comps
	 * as that will return NULL if the oaf files don't have the short_list properties
	 * defined). */
	components = gnome_vfs_mime_get_all_components (mime_type);
	for (iter = components; iter; iter = iter->next) {
		if (component_supports (iter->data, mime_type)) {
			handler->generic = FALSE;
			handler->is_bonobo = TRUE;
			handler->builtin = handle_via_bonobo;
			handler->component = OAF_ServerInfo_duplicate (iter->data);
			gnome_vfs_mime_component_list_free (components);
			goto reg;
		}
	}

	gnome_vfs_mime_component_list_free (components);

	/* Try for a generic builtin match. */
	p = strchr (mime_type, '/');
	if (p == NULL)
		p = mime_type + strlen (mime_type);
	mime_type_main = alloca ((p - mime_type) + 3);
	memcpy (mime_type_main, mime_type, p - mime_type);
	memcpy (mime_type_main + (p - mime_type), "/*", 3);
	
	handler->builtin = g_hash_table_lookup (mime_function_table,
						mime_type_main);
	
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
		handler->is_bonobo = TRUE;
		goto reg;
	}

	/* If we at least got an application, use that. */
	if (handler->applications) {
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

enum inline_states {
	I_VALID     = (1 << 0),
	I_ACTUALLY  = (1 << 1),
	I_DISPLAYED = (1 << 2)
};

static gint
get_inline_flags (CamelMimePart *part, MailDisplay *md)
{
	GHashTable *asht;
	gint val;

	/* check if we already know. */

	asht = g_datalist_get_data (md->data, "attachment_states");
	val = GPOINTER_TO_INT (g_hash_table_lookup (asht, part));
	if (val)
		return val;

	/* ok, we don't know. Figure it out. */

	if (mail_part_is_inline (part))
		val = (I_VALID | I_ACTUALLY | I_DISPLAYED);
	else
		val = (I_VALID);

	g_hash_table_insert (asht, part, GINT_TO_POINTER (val));

	return val;
}

gboolean
mail_part_is_displayed_inline (CamelMimePart *part, MailDisplay *md)
{
	return (gboolean) (get_inline_flags (part, md) & I_DISPLAYED);
}

void
mail_part_toggle_displayed (CamelMimePart *part, MailDisplay *md)
{
	GHashTable *asht = g_datalist_get_data (md->data, "attachment_states");
	gpointer ostate, opart;
	gint state;
	
	if (g_hash_table_lookup_extended (asht, part, &opart, &ostate)) {
		g_hash_table_remove (asht, part);
		
		state = GPOINTER_TO_INT (ostate);
		
		if (state & I_DISPLAYED)
			state &= ~I_DISPLAYED;
		else
			state |= I_DISPLAYED;
	} else {
		state = I_VALID | I_DISPLAYED;
	}
	
	g_hash_table_insert (asht, part, GINT_TO_POINTER (state));
}

static void
mail_part_set_default_displayed_inline (CamelMimePart *part, MailDisplay *md,
					gboolean displayed)
{
	GHashTable *asht = g_datalist_get_data (md->data, "attachment_states");
	gint state;
	
	if (g_hash_table_lookup (asht, part))
		return;

	state = I_VALID | (displayed ? I_DISPLAYED : 0);
	g_hash_table_insert (asht, part, GINT_TO_POINTER (state));
}

static void
attachment_header (CamelMimePart *part, const char *mime_type, MailDisplay *md,
		   GtkHTML *html, GtkHTMLStream *stream)
{
	char *htmlinfo, *html_str, *fmt;
	const char *info;
	
	/* Start the table, create the pop-up object. */
	mail_html_write (html, stream, 
			 "<table cellspacing=0 cellpadding=0>"
			 "<tr><td><table width=10 cellspacing=0 cellpadding=0><tr><td></td></tr></table></td>");

	if (! md->printing) {
		gtk_html_stream_printf (stream, "<td><object classid=\"popup:%s\" type=\"%s\"></object></td>",
					get_cid (part, md), mime_type);
	}
	
	mail_html_write (html, stream,
			 "<td><table width=3 cellspacing=0 cellpadding=0><tr><td></td></tr></table></td>"
			 "<td><font size=-1>");

	
	/* Write the MIME type */
	info = gnome_vfs_mime_get_value (mime_type, "description");
	html_str = e_text_to_html (info ? info : mime_type, 0);
	htmlinfo = e_utf8_from_locale_string (html_str);
	g_free (html_str);
	fmt = e_utf8_from_locale_string (_("%s attachment"));
	gtk_html_stream_printf (stream, fmt, htmlinfo);
	g_free (htmlinfo);
	g_free (fmt);
		
	/* Write the name, if we have it. */
	info = camel_mime_part_get_filename (part);
	if (info) {
		htmlinfo = e_text_to_html (info, 0);
		gtk_html_stream_printf (stream, " (%s)", htmlinfo);
		g_free (htmlinfo);
	}
	
	/* Write a description, if we have one. */
	info = camel_mime_part_get_description (part);
	if (info) {
		htmlinfo = e_text_to_html (info, md->printing ? 0 : E_TEXT_TO_HTML_CONVERT_URLS);
		gtk_html_stream_printf (stream, ", \"%s\"", htmlinfo);
		g_free (htmlinfo);
	}
	
	mail_html_write (html, stream, "</font></td></tr><tr>"
			 "<td height=10><table height=10 cellspacing=0 cellpadding=0>"
			 "<tr><td></td></tr></table></td></tr></table>\n");
}

static gboolean
format_mime_part (CamelMimePart *part, MailDisplay *md,
		  GtkHTML *html, GtkHTMLStream *stream)
{
	CamelDataWrapper *wrapper;
	char *mime_type;
	MailMimeHandler *handler;
	gboolean output;
	int inline_flags;
	
	/* Record URLs associated with this part */
	get_cid (part, md);
	get_location (part, md);
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	
	if (CAMEL_IS_MULTIPART (wrapper) &&
	    camel_multipart_get_number (CAMEL_MULTIPART (wrapper)) == 0) {
		
		mail_error_printf (html, stream, "\n%s\n", U_("Could not parse MIME message. Displaying as source."));
		if (mail_content_loaded (wrapper, md, TRUE, NULL, html, NULL))
			handle_text_plain (part, "text/plain", md, html, stream);
		return TRUE;
	}
	
	mime_type = camel_data_wrapper_get_mime_type (wrapper);
	g_strdown (mime_type);
	
	handler = mail_lookup_handler (mime_type);
	if (!handler) {
		char *id_type;
		
		/* Special case MIME types that we know that we can't
		 * display but are some kind of plain text to prevent
		 * evil infinite recursion.
		 */
		
		if (!strcmp (mime_type, "application/mac-binhex40")) {
			handler = NULL;
		} else {
			id_type = mail_identify_mime_part (part, md);
			if (id_type) {
				g_free (mime_type);
				mime_type = id_type;
				handler = mail_lookup_handler (id_type);
			}
		}
	}
	
	inline_flags = get_inline_flags (part, md);
	
	/* No header for anonymous inline parts. */
	if (!((inline_flags & I_ACTUALLY) && is_anonymous (part, mime_type)))
		attachment_header (part, mime_type, md, html, stream);
	
	if (handler && handler->builtin && inline_flags & I_DISPLAYED &&
	    mail_content_loaded (wrapper, md, TRUE, NULL, html, NULL))
		output = (*handler->builtin) (part, mime_type, md, html, stream);
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
		gtk_html_stream_printf (stream, "<tr><td>%s%s:%s ",
					bold ? "<b>" : "", encoded_name,
					bold ? "</b>" : "");
	} else {
		gtk_html_stream_printf (stream, "<tr><%s align=\"right\" valign=\"top\">%s:"
					"<b>&nbsp;</%s><td>", bold ? "th" : "td",
					encoded_name, bold ? "th" : "td");
	}
	
	g_free (encoded_name);
}

static void
write_date (CamelMimeMessage *message, int flags, GtkHTML *html, GtkHTMLStream *stream)
{
	const char *datestr;
	
	datestr = camel_medium_get_header (CAMEL_MEDIUM (message), "Date");
	
	if (datestr) {
		write_field_row_begin (_("Date"), flags, html, stream);
		gtk_html_stream_printf (stream, "%s</td> </tr>", datestr);
	}
}

static void
write_text_header (const char *name, const char *value, int flags, GtkHTML *html, GtkHTMLStream *stream)
{
	char *encoded;

	if (value && *value)
		encoded = e_text_to_html (value, E_TEXT_TO_HTML_CONVERT_NL |
					  E_TEXT_TO_HTML_CONVERT_SPACES |
					  E_TEXT_TO_HTML_CONVERT_URLS);
	else
		encoded = "";
	
	write_field_row_begin (name, flags, html, stream);
	
	gtk_html_stream_printf (stream, "%s</td> </tr>", encoded);
	
	if (value && *value)
		g_free (encoded);
}

static void
write_address (MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream,
	       const CamelInternetAddress *addr, const char *field_name, int flags)
{
	const char *name, *email;
	gint i;
	
	if (addr == NULL || !camel_internet_address_get (addr, 0, NULL, NULL))
		return;
	
	write_field_row_begin (field_name, flags, html, stream);
	
	i = 0;
	while (camel_internet_address_get (addr, i, &name, &email)) {
		CamelInternetAddress *subaddr;
		gchar *addr_txt, *addr_url;
		gboolean have_name = name && *name;
		gboolean have_email = email && *email;
		gchar *name_disp = NULL;
		gchar *email_disp = NULL;

		subaddr = camel_internet_address_new ();
		camel_internet_address_add (subaddr, name, email);
		addr_txt = camel_address_format (CAMEL_ADDRESS (subaddr));
		addr_url = camel_url_encode (addr_txt, TRUE, NULL);
		camel_object_unref (CAMEL_OBJECT (subaddr));
		
		if (have_name) {
			name_disp = e_text_to_html (name, 0);
		}
		
		if (have_email) {
			email_disp = e_text_to_html (email, 0);
		}
		
		if (i)
			mail_html_write (html, stream, ", ");
		
		if (have_email || have_name) {
			if (!have_email) {
				email_disp = g_strdup ("???");
			}
			
			if (have_name) {
				if (md->printing) {
					gtk_html_stream_printf (stream, "%s &lt;%s&gt;", name_disp, email_disp);
				} else {
					gtk_html_stream_printf (stream,
								"%s &lt;<a href=\"mailto:%s\">%s</a>&gt;",
								name_disp, addr_url, email_disp);
				}
			} else {
				if (md->printing) {
					mail_html_write (html, stream, email_disp);
				} else {
					gtk_html_stream_printf (stream,
								"<a href=\"mailto:%s\">%s</a>",
								addr_url, email_disp);
				}
			}			

		} else {
			char *str;
			
			str = e_utf8_from_locale_string (_("Bad Address"));
			gtk_html_stream_printf (stream, "<i>%s</i>", str);
			g_free (str);
		}

		g_free (name_disp);
		g_free (email_disp);
		g_free (addr_txt);
		g_free (addr_url);
		
		i++;
	}
	
	mail_html_write (html, stream, "</td></tr>");
}

/* order of these must match write_header code */
static char *default_headers[] = {
	"From", "Reply-To", "To", "Cc", "Bcc", "Subject", "Date",
};

/* return index of header in default_headers array */
static int
default_header_index(const char *name)
{
	int i;

	for (i=0;i<sizeof(default_headers)/sizeof(default_headers[0]);i++)
		if (!g_strcasecmp(name, default_headers[i]))
			return i;
	
	return -1;
}

/* index is index of header in default_headers array */
static void
write_default_header(CamelMimeMessage *message, MailDisplay *md, 
		     GtkHTML *html, GtkHTMLStream *stream,
		     int index, int flags)
{
	switch(index) {
	case 0:
		write_address (md, html, stream,
			       camel_mime_message_get_from (message), _("From"), flags | WRITE_BOLD);
		break;
	case 1:
		write_address (md, html, stream, 
			       camel_mime_message_get_reply_to (message), _("Reply-To"), flags | WRITE_BOLD);
		break;
	case 2:
		write_address(md, html, stream,
			      camel_mime_message_get_recipients(message, CAMEL_RECIPIENT_TYPE_TO),
			      _("To"), flags | WRITE_BOLD);
		break;
	case 3:
		write_address (md, html, stream,
			       camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC),
			       _("Cc"), flags | WRITE_BOLD);
		break;
	case 4:
		write_address (md, html, stream,
			       camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_BCC),
			       _("Bcc"), flags | WRITE_BOLD);
		break;
	case 5:
		write_text_header (_("Subject"), camel_mime_message_get_subject (message),
				   flags | WRITE_BOLD, html, stream);
		break;
	case 6:
		write_date (message, flags | WRITE_BOLD, html, stream);
		break;
	default:
		g_assert_not_reached();
	}
}

static gboolean
write_xmailer_header (CamelMimeMessage *message, MailDisplay *md,
		      GtkHTML *html, GtkHTMLStream *stream,
		      MailConfigXMailerDisplayStyle xm)
{
	const char *xmailer, *evolution;

	xmailer = camel_medium_get_header (CAMEL_MEDIUM (message), "X-Mailer");
	if (!xmailer) {
		xmailer = camel_medium_get_header (CAMEL_MEDIUM (message), "User-Agent");
		if (!xmailer)
			return FALSE;
	}

	evolution = strstr (xmailer, "Evolution");
	if ((xm & MAIL_CONFIG_XMAILER_OTHER) ||
	    (evolution && (xm & MAIL_CONFIG_XMAILER_EVO)))
		write_text_header (_("Mailer"), xmailer, WRITE_BOLD, html, stream);

	return evolution != NULL && (xm & MAIL_CONFIG_XMAILER_RUPERT_APPROVED);
}

#define COLOR_IS_LIGHT(r, g, b)  ((r + g + b) > (128 * 3))

static void
write_headers (CamelMimeMessage *message, MailDisplay *md,
	       GtkHTML *html, GtkHTMLStream *stream)
{
	MailConfigXMailerDisplayStyle xm = mail_config_get_x_mailer_display_style ();
	gboolean full = (md->display_style == MAIL_CONFIG_DISPLAY_FULL_HEADERS);
	char bgcolor[7], fontcolor[7];
	GtkStyle *style = NULL;
	gboolean evo_icon = FALSE;
	int i;

	/* My favorite thing to do... muck around with colors so we respect people's stupid themes.
	   However, we only do this if we are rendering to the screen -- we ignore the theme
	   when we are printing. */
	style = gtk_widget_get_style (GTK_WIDGET (html));
	if (style && !md->printing) {
		int state = GTK_WIDGET_STATE (GTK_WIDGET (html));
		gushort r, g, b;
		
		r = style->base[state].red / 256;
		g = style->base[state].green / 256;
		b = style->base[state].blue / 256;
		
		if (COLOR_IS_LIGHT (r, g, b)) {
			r *= 0.92;
			g *= 0.92;
			b *= 0.92;
		} else {
			r = 255 - (0.92 * (255 - r));
			g = 255 - (0.92 * (255 - g));
			b = 255 - (0.92 * (255 - b));
		}
		
		sprintf (bgcolor, "%.2X%.2X%.2X", r, g, b);
		
		r = style->text[state].red / 256;
		g = style->text[state].green / 256;
		b = style->text[state].blue / 256;
		
		sprintf (fontcolor, "%.2X%.2X%.2X", r, g, b);
	} else {
		strcpy (bgcolor, "EEEEEE");
		strcpy (fontcolor, "000000");
	}
	
	gtk_html_stream_printf (
		stream,
		"<table width=\"100%%\" cellpadding=0 cellspacing=0>"
		/* Top margin */
		"<tr><td colspan=3 height=10><table height=10 cellpadding=0 cellspacing=0><tr><td></td></tr></table></td></tr>"
		/* Left margin */
		"<tr><td><table width=10 cellpadding=0 cellspacing=0><tr><td></td></tr></table></td>"
		/* Black border */
		"<td width=\"100%%\"><table bgcolor=\"#000000\" width=\"100%%\" cellspacing=0 cellpadding=1>"
		/* Main header box */
		"<tr><td><table bgcolor=\"#%s\" width=\"100%%\" cellpadding=0 cellspacing=0>"
		/* Internal header table */
		"<tr valign=top><td><table><font color=\"#%s\">\n",
		bgcolor, fontcolor);
	
	if (full) {
		struct _header_raw *header;
		const char *charset;
		CamelContentType *ct;
		char *value;
		
		ct = camel_mime_part_get_content_type(CAMEL_MIME_PART(message));
		charset = header_content_type_param(ct, "charset");
		charset = e_iconv_charset_name(charset);
		
		header = CAMEL_MIME_PART(message)->headers;
		while (header) {
			i = default_header_index(header->name);
			if (i == -1) {
				value = header_decode_string(header->value, charset);
				write_text_header(header->name, value, WRITE_NOCOLUMNS, html, stream);
				g_free(value);
			} else
				write_default_header(message, md, html, stream, i, WRITE_NOCOLUMNS);
			header = header->next;
		}
	} else {
		for (i=0;i<sizeof(default_headers)/sizeof(default_headers[0]);i++)
			write_default_header(message, md, html, stream, i, 0);
		if (xm != MAIL_CONFIG_XMAILER_NONE)
			evo_icon = write_xmailer_header(message, md, html, stream, xm);
	}
	
	/* Close off the internal header table */
	mail_html_write (html, stream, "</font></table></td>");

	if (!md->printing && evo_icon) {
		gtk_html_stream_printf (stream,
					"<td align=right><table><tr><td width=16>"
					"<img src=\"%s\">"
					"</td></tr></table></td>",
					mail_display_get_url_for_icon (md, EVOLUTION_ICONSDIR "/monkey-16.png"));
	}

	mail_html_write (html, stream,
			 /* Main header box */
			 "</tr></table>"
			 /* Black border */
			 "</td></tr></table></td>"
			 /* Right margin */
			 "<td><table width=10 cellpadding=0 cellspacing=0><tr><td></td></tr></table></td>"
			 "</tr></table>\n");
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
mail_content_loaded (CamelDataWrapper *wrapper, MailDisplay *md, gboolean redisplay, const gchar *url,
		     GtkHTML *html, GtkHTMLStream *handle)
{
	if (!camel_data_wrapper_is_offline (wrapper))
		return TRUE;
	
	camel_object_ref (CAMEL_OBJECT (wrapper));
	if (redisplay)
		mail_display_redisplay_when_loaded (md, wrapper, load_offline_content, html, wrapper);
	else
		mail_display_stream_write_when_loaded (md, wrapper, url, load_offline_content, html, handle, wrapper);
	
	return FALSE;
}

/* Return the contents of a data wrapper, or %NULL if it contains only
 * whitespace.
 */
GByteArray *
mail_format_get_data_wrapper_text (CamelDataWrapper *wrapper, MailDisplay *mail_display)
{
	CamelStream *memstream;
	CamelStreamFilter *filtered_stream;
	GByteArray *ba;
	char *text, *end;
	
	memstream = camel_stream_mem_new ();
	ba = g_byte_array_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (memstream), ba);
	
	filtered_stream = camel_stream_filter_new_with_stream (memstream);
	camel_object_unref (CAMEL_OBJECT (memstream));
	
	if (wrapper->rawtext || (mail_display && mail_display->charset)) {
		CamelMimeFilterCharset *filter;
		const char *charset;
		
		if (!wrapper->rawtext) {
			/* data wrapper had been successfully converted to UTF-8 using the mime
			   part's charset, but the user thinks he knows best so we'll let him
			   shoot himself in the foot here... */
			CamelContentType *content_type;
			
			/* get the original charset of the mime part */
			content_type = camel_data_wrapper_get_mime_type_field (wrapper);
			charset = content_type ? header_content_type_param (content_type, "charset") : NULL;
			if (!charset)
				charset = mail_config_get_default_charset ();
			
			/* since the content is already in UTF-8, we need to decode into the
			   original charset before we can convert back to UTF-8 using the charset
			   the user is overriding with... */
			filter = camel_mime_filter_charset_new_convert ("utf-8", charset);
			if (filter) {
				camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (filter));
				camel_object_unref (CAMEL_OBJECT (filter));
			}
		}
		
		/* find out the charset the user wants to override to */
		if (mail_display && mail_display->charset)
			charset = mail_display->charset;
		else
			charset = mail_config_get_default_charset ();
		
		filter = camel_mime_filter_charset_new_convert (charset, "utf-8");
		if (filter) {
			camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (filter));
			camel_object_unref (CAMEL_OBJECT (filter));
		}
	}
	
	camel_data_wrapper_write_to_stream (wrapper, CAMEL_STREAM (filtered_stream));
	camel_stream_flush (CAMEL_STREAM (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	
	for (text = ba->data, end = text + ba->len; text < end; text++) {
		if (!isspace ((unsigned char) *text))
			break;
	}
	
	if (text >= end) {
		g_byte_array_free (ba, TRUE);
		return NULL;
	}
	
	return ba;
}

static void
write_hr (GtkHTML *html, GtkHTMLStream *stream)
{
	mail_html_write (html, stream,
			 "<table cellspacing=0 cellpadding=10 width=\"100%\"><tr><td width=\"100%\">"
			 "<hr noshadow size=1></td></tr></table>\n");
}

/*----------------------------------------------------------------------*
 *                     Mime handling functions
 *----------------------------------------------------------------------*/

struct {
	char *start;
	char * (*handler) (char *start, CamelMimePart *part,
			   guint offset, MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream);
} text_specials[] = {
	{ "-----BEGIN PGP MESSAGE-----\n", try_inline_pgp },
	{ "-----BEGIN PGP SIGNED MESSAGE-----\n", try_inline_pgp_sig },
	{ "begin ", try_uudecoding },
	{ "(This file must be converted with BinHex 4.0)\n", try_inline_binhex }
};

static int num_specials = (sizeof (text_specials) / sizeof (text_specials[0]));

static void
write_one_text_plain_chunk (const char *text, int len, GtkHTML *html, GtkHTMLStream *stream, gboolean printing)
{
	char *buf;
	
	mail_html_write (html, stream,
			 "<table cellspacing=0 cellpadding=10 width=\"100%\"><tr><td>\n");
	
	buf = g_strndup (text, len);
	mail_text_write  (html, stream, printing, buf);
	g_free (buf);
	
	mail_html_write (html, stream, "</td></tr></table>\n");
}

static gboolean
handle_text_plain (CamelMimePart *part, const char *mime_type,
		   MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelContentType *type;
	gboolean check_specials;
	char *p, *start, *text;
	const char *format;
	GByteArray *bytes;
	int i;
	
	bytes = mail_format_get_data_wrapper_text (wrapper, md);
	if (!bytes)
		return FALSE;
	
	g_byte_array_append (bytes, "", 1);
	text = bytes->data;
	g_byte_array_free (bytes, FALSE);
	
	/* Check to see if this is a broken text/html part with content-type text/plain */
	start = text;
	while (isspace ((unsigned) *start))
		start++;
	if (!g_strncasecmp (start, "<html>", 6) || !g_strncasecmp (start, "<!DOCTYPE HTML", 14)) {
		g_free (text);
		return handle_text_html (part, "text/html", md, html, stream);
	}
	
	/* Check for RFC 2646 flowed text. */
	type = camel_mime_part_get_content_type (part);
	format = header_content_type_param (type, "format");
	if (format && !g_strcasecmp (format, "flowed"))
		return handle_text_plain_flowed (text, md, html, stream);
	
	/* Only look for binhex and stuff if this is real text/plain.
	 * (and not, say, application/mac-binhex40 that mail-identify
	 * has decided to call text/plain because it starts with English
	 * text...)
	 */
	check_specials = header_content_type_is (type, "text", "plain");
	
	p = text;
	while (p && check_specials) {
		/* Look for special cases. */
		for (i = 0; i < num_specials; i++) {
			start = strstr (p, text_specials[i].start);
			if (start && (start == p || start[-1] == '\n'))
				break;
		}
		if (i == num_specials)
			break;
		
		/* Deal with special case */
		if (start != p)
			write_one_text_plain_chunk (p, start - p, html, stream, md->printing);
		
		p = text_specials[i].handler (start, part, start - text, md, html, stream);
		if (p == start) {
			/* Oops. That failed. Output this line normally and
			 * skip over it.
			 */
			p = strchr (start, '\n');
			/* Last line, drop out, and dump */
			if (p == NULL) {
				p = start;
				break;
			}
			p++;
			write_one_text_plain_chunk (start, p - start, html, stream, md->printing);
		} else if (p)
			write_hr (html, stream);
	}
	/* Finish up (or do the whole thing if there were no specials). */
	if (p)
		write_one_text_plain_chunk (p, strlen (p), html, stream, md->printing);
	
	g_free (text);
	
	return TRUE;
}

static gboolean
handle_text_plain_flowed (char *buf, MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	char *text, *line, *eol, *p;
	int prevquoting = 0, quoting, len, br_pending = 0;
	guint32 citation_color = mail_config_get_citation_color ();
	
	mail_html_write (html, stream,
			 "\n<!-- text/plain, flowed -->\n"
			 "<table cellspacing=0 cellpadding=10 width=\"100%\"><tr><td>\n<tt>\n");
	
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
				if (md->printing)
					mail_html_write (html, stream, "<i>");
				else
					gtk_html_stream_printf (stream, "<font color=\"#%06x\">", citation_color);
				if (br_pending)
					br_pending--;
			}
			while (quoting > prevquoting) {
				mail_html_write (html, stream, "<blockquote type=\"cite\">");
				prevquoting++;
			}
			while (quoting < prevquoting) {
				mail_html_write (html, stream, "</blockquote>");
				prevquoting--;
			}
			if (quoting == 0) {
				mail_html_write (html, stream, md->printing ? "</i>" : "</font>\n");
				if (br_pending)
					br_pending--;
			}
		}
		
		if (*p == ' ')
			p++;
		len = strlen (p);
		if (len == 0) {
			br_pending++;
			if (!eol)
				break;
			continue;
		}
		
		while (br_pending) {
			mail_html_write (html, stream, "<br>\n");
			br_pending--;
		}
		
		/* replace '<' with '&lt;', etc. */
		text = e_text_to_html (p, 
				       md->printing ?
				       E_TEXT_TO_HTML_CONVERT_SPACES :
				       E_TEXT_TO_HTML_CONVERT_SPACES | E_TEXT_TO_HTML_CONVERT_URLS);
		if (text && *text)
			mail_html_write (html, stream, text);
		g_free (text);
		
		if ((len > 0 && p[len - 1]) != ' ' || !strcmp (p, "-- "))
			br_pending++;
		
		if (!eol)
			break;
	}
	
	g_free (buf);
	
	mail_html_write (html, stream, "</tt>\n</td></tr></table>\n");
	
	return TRUE;
}

static CamelMimePart *
fake_mime_part_from_data (const char *data, int len, const char *type,
			  guint offset, MailDisplay *md)
{
	GHashTable *fake_parts = g_datalist_get_data (md->data, "fake_parts");
	CamelStream *memstream;
	CamelDataWrapper *wrapper;
	CamelMimePart *part;

	part = g_hash_table_lookup (fake_parts, GUINT_TO_POINTER (offset));
	if (part)
		return part;

	memstream = camel_stream_mem_new_with_buffer (data, len);
	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream (wrapper, memstream);
	camel_data_wrapper_set_mime_type (wrapper, type);
	camel_object_unref (CAMEL_OBJECT (memstream));
	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (CAMEL_OBJECT (wrapper));
	camel_mime_part_set_disposition (part, "inline");

	g_hash_table_insert (fake_parts, GUINT_TO_POINTER (offset), part);
	return part;
}

static void
destroy_part (CamelObject *root, gpointer event_data, gpointer user_data)
{
	camel_object_unref (user_data);
}

static char *
try_inline_pgp (char *start, CamelMimePart *mime_part,
		guint offset, MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	return start;
}

static char *
try_inline_pgp_sig (char *start, CamelMimePart *mime_part,
		    guint offset, MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	return start;
}

static char *
try_uudecoding (char *start, CamelMimePart *mime_part,
		guint offset, MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	int mode, len, state = CAMEL_UUDECODE_STATE_INIT;
	char *filename, *eoln, *p, *out;
	CamelMimePart *part;
	guint32 save = 0;
	
	/* Make sure it's a real uudecode begin line:
	 * begin [0-7]+ .*
	 */
	mode = strtoul (start + 6, &p, 8);
	if (p == start + 6 || *p != ' ')
		return start;
	
	if (!(eoln = strchr (start, '\n')))
		return start;
	
	while (*p == ' ' || *p == '\t')
		p++;
	
	if (p == eoln)
		return start;
	
	filename = g_strndup (p, eoln - p);
	
	/* Make sure there's an end line. */
	if (!(p = strstr (p, "\nend\n"))) {
		g_free (filename);
		return start;
	}
	
	eoln++;
	out = g_malloc (p - eoln);
	len = uudecode_step (eoln, p - eoln, out, &state, &save);
	
	part = fake_mime_part_from_data (out, len, "application/octet-stream",
					 offset, md);
	g_free (out);
	
	camel_mime_part_set_filename (part, filename);
	g_free (filename);
	
	camel_object_hook_event (CAMEL_OBJECT (md->current_message),
				 "finalize", destroy_part, part);
	
	write_hr (html, stream);
	format_mime_part (part, md, html, stream);
	
	return p + 4;
}

static char *
try_inline_binhex (char *start, CamelMimePart *mime_part,
		   guint offset, MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
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
					 "application/mac-binhex40",
					 offset, md);
	camel_object_hook_event (CAMEL_OBJECT (md->current_message),
				 "finalize", destroy_part, part);
	
	write_hr (html, stream);
	format_mime_part (part, md, html, stream);
	
	return p;
}

static void
g_string_append_len (GString *string, const char *str, int len)
{
	char *tmp;
	
	tmp = g_malloc (len + 1);
	tmp[len] = 0;
	memcpy (tmp, str, len);
	g_string_append (string, tmp);
	g_free (tmp);
}

/* text/enriched (RFC 1896) or text/richtext (included in RFC 1341) */
static gboolean
handle_text_enriched (CamelMimePart *part, const char *mime_type,
		      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	static GHashTable *translations = NULL;
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	GByteArray *ba, *bytes;
	char *text, *p, *xed;
	int len, nofill = 0;
	gboolean enriched;
	GString *string;
	
	if (!translations) {
		translations = g_hash_table_new (g_strcase_hash, g_strcase_equal);
		
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
	
	bytes = mail_format_get_data_wrapper_text (wrapper, md);
	if (!bytes)
		return FALSE;
	
	if (!g_strcasecmp (mime_type, "text/richtext")) {
		enriched = FALSE;
		mail_html_write (html, stream,
				 "\n<!-- text/richtext -->\n");
	} else {
		enriched = TRUE;
		mail_html_write (html, stream,
				 "\n<!-- text/enriched -->\n");
	}
	
	/* This is not great code, but I don't feel like fixing it right
	 * now. I mean, it's just text/enriched...
	 */
	string = g_string_sized_new (2 * bytes->len);
	g_byte_array_append (bytes, "", 1);
	p = text = bytes->data;
	g_byte_array_free (bytes, FALSE);
	
	while (p) {
		len = strcspn (p, " <>&\n");
		if (len)
			g_string_append_len (string, p, len);
		
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
			     string->len);
	g_string_free (string, TRUE);
	
	xed = g_strdup_printf ("x-evolution-data:%p", part);
	gtk_html_stream_printf (stream, "<iframe src=\"%s\" frameborder=0 scrolling=no></iframe>", xed);
	mail_display_add_url (md, "data_urls", xed, ba);
	
	return TRUE;
}

static gboolean
handle_text_html (CamelMimePart *part, const char *mime_type,
		  MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	const char *location, *base;
	
	mail_html_write (html, stream, "\n<!-- text/html -->\n");
	
	if ((base = camel_medium_get_header (CAMEL_MEDIUM (part), "Content-Base"))) {
		char *base_url;
		size_t len;
		
		len = strlen (base);
		
		if (*base == '"' && *(base + len - 1) == '"') {
			len -= 2;
			base_url = alloca (len + 1);
			memcpy (base_url, base + 1, len);
			base_url[len] = '\0';
			base = base_url;
		}
		
		gtk_html_set_base (html, base);
	}
	
	location = get_location (part, md);
	if (!location)
		location = get_cid (part, md);
	gtk_html_stream_printf (stream, "<iframe src=\"%s\" frameborder=0 scrolling=no></iframe>", location);
	return TRUE;
}

static gboolean
handle_image (CamelMimePart *part, const char *mime_type, MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	gtk_html_stream_printf (stream, "<img hspace=10 vspace=10 src=\"%s\">",
				get_cid (part, md));
	return TRUE;
}

static gboolean
handle_multipart_mixed (CamelMimePart *part, const char *mime_type,
			MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *mp;
	int i, nparts;
	gboolean output = FALSE;
	
	if (!CAMEL_IS_MULTIPART (wrapper)) {
		mail_error_printf (html, stream, "\n%s\n", U_("Could not parse MIME message. Displaying as source."));
		if (mail_content_loaded (wrapper, md, TRUE, NULL, html, NULL))
			handle_text_plain (part, "text/plain", md, html, stream);
		return TRUE;
	}

	mp = CAMEL_MULTIPART (wrapper);

	nparts = camel_multipart_get_number (mp);	
	for (i = 0; i < nparts; i++) {
		if (i != 0 && output)
			write_hr (html, stream);

		part = camel_multipart_get_part (mp, i);
		
		output = format_mime_part (part, md, html, stream);
	}

	return TRUE;
}

static gboolean
handle_multipart_encrypted (CamelMimePart *part, const char *mime_type,
			    MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	CamelMultipartEncrypted *mpe;
	CamelMimePart *mime_part;
	CamelCipherContext *cipher;
	CamelDataWrapper *wrapper;
	CamelException ex;
	gboolean handled;
	
	/* Currently we only handle RFC2015-style PGP encryption. */
	if (!camel_pgp_mime_is_rfc2015_encrypted (part))
		return handle_multipart_mixed (part, mime_type, md, html, stream);
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	
	mpe = CAMEL_MULTIPART_ENCRYPTED (wrapper);
	
	camel_exception_init (&ex);
	cipher = camel_gpg_context_new (session);
	mime_part = camel_multipart_encrypted_decrypt (mpe, cipher, &ex);
	camel_object_unref (cipher);
	
	if (camel_exception_is_set (&ex)) {
		char *error;
		
		error = e_utf8_from_locale_string (camel_exception_get_description (&ex));
		
		mail_error_printf (html, stream, "\n%s\n", error);
		g_free (error);
		
		camel_exception_clear (&ex);
		return TRUE;
	}
	
	handled = format_mime_part (mime_part, md, html, stream);
	camel_object_unref (mime_part);
	
	return handled;
}

static gboolean
handle_multipart_signed (CamelMimePart *part, const char *mime_type,
			 MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	CamelMimePart *subpart;
	CamelDataWrapper *wrapper;
	CamelMultipartSigned *mps;
	gboolean output = FALSE;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	
	if (!CAMEL_IS_MULTIPART_SIGNED (wrapper)) {
		mail_error_printf (html, stream, "\n%s\n", U_("Could not parse MIME message. Displaying as source."));
		if (mail_content_loaded (wrapper, md, TRUE, NULL, html, NULL))
			handle_text_plain (part, "text/plain", md, html, stream);
		return TRUE;
	}

	mps = CAMEL_MULTIPART_SIGNED (wrapper);
	
	/* if subpart & signature is null, what do we do?  just write it out raw?
	   multipart_signed will, if it cannot parse properly, put everything in the first part
	   this includes: more or less than 2 parts */
	
	/* output the content */
	subpart = camel_multipart_get_part((CamelMultipart *)mps, CAMEL_MULTIPART_SIGNED_CONTENT);
	if (subpart == NULL)
		return FALSE;
	
	output = format_mime_part (subpart, md, html, stream);
	
	/* now handle the signature */
	subpart = camel_multipart_get_part((CamelMultipart *)mps, CAMEL_MULTIPART_SIGNED_SIGNATURE);
	if (subpart == NULL)
		return FALSE;
	
	mail_part_set_default_displayed_inline(subpart, md, FALSE);
	
	if (!mail_part_is_displayed_inline (subpart, md) && !md->printing) {
		char *url;
		
		/* Write out the click-for-info object */
		url = g_strdup_printf ("signature:%p/%lu", subpart,
				       (unsigned long)time (NULL));
		gtk_html_stream_printf (stream,
					"<br><table cellspacing=0 cellpadding=0>"
					"<tr><td><table width=10 cellspacing=0 cellpadding=0>"
					"<tr><td></td></tr></table></td>"
					"<td><object classid=\"%s\"></object></td>"
					"<td><table width=3 cellspacing=0 cellpadding=0>"
					"<tr><td></td></tr></table></td>"
					"<td><font size=-1>", url);
		mail_display_add_url (md, "part_urls", url, subpart);
		
		mail_html_write (html, stream, 
				 U_("This message is digitally signed. "
				    "Click the lock icon for more information."));
		
		mail_html_write (html, stream,
				 "</font></td></tr><tr><td height=10><table height=10 cellspacing=0 cellpadding=0>"
				 "<tr><td></td></tr></table></td></tr></table>\n");
	} else {
		CamelCipherValidity *valid = NULL;
		CamelException ex;
		const char *message = NULL;
		gboolean good = FALSE;
		CamelCipherContext *cipher;
		
		/* Write out the verification results */
		/* TODO: use the right context for the right message ... */
		camel_exception_init (&ex);
		cipher = camel_gpg_context_new (session);
		if (cipher) {
			valid = camel_multipart_signed_verify (mps, cipher, &ex);
			camel_object_unref (CAMEL_OBJECT (cipher));
			if (valid) {
				good = camel_cipher_validity_get_valid (valid);
				message = camel_cipher_validity_get_description (valid);
			} else {
				message = camel_exception_get_description (&ex);
			}
		} else {
			message = U_("Could not create a PGP verfication context");
		}
		
		if (good) {
			gtk_html_stream_printf (stream,
						"<table><tr valign=top>"
						"<td><img src=\"%s\"></td>"
						"<td>%s<br><br>",
						mail_display_get_url_for_icon (md, EVOLUTION_ICONSDIR "/pgp-signature-ok.png"),
						U_("This message is digitally signed and "
						   "has been found to be authentic."));
		} else {
			gtk_html_stream_printf (stream,
						"<table><tr valign=top>"
						"<td><img src=\"%s\"></td>"
						"<td>%s<br><br>",
						mail_display_get_url_for_icon (md, EVOLUTION_ICONSDIR "/pgp-signature-bad.png"),
						U_("This message is digitally signed but can "
						   "not be proven to be authentic."));
		}
		
		if (message) {
			gtk_html_stream_printf (stream, "<font size=-1 %s>", good || md->printing ? "" : "color=red");
			mail_text_write (html, stream, md->printing, message);
			mail_html_write (html, stream, "</font>");
		}
		
		mail_html_write (html, stream, "</td></tr></table>");
		camel_exception_clear (&ex);
		camel_cipher_validity_free (valid);
	}

	return TRUE;
}

/* As seen in RFC 2387! */
static gboolean
handle_multipart_related (CamelMimePart *part, const char *mime_type,
			  MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	CamelDataWrapper *wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *mp;
	CamelMimePart *body_part, *display_part = NULL;
	CamelContentType *content_type;
	const char *location, *start;
	int i, nparts;
	GHashTable *related_save;
	int ret;
	
	if (!CAMEL_IS_MULTIPART (wrapper)) {
		mail_error_printf (html, stream, "\n%s\n", U_("Could not parse MIME message. Displaying as source."));
		if (mail_content_loaded (wrapper, md, TRUE, NULL, html, NULL))
			handle_text_plain (part, "text/plain", md, html, stream);
		return TRUE;
	}

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
			
			if (cid && !strncmp (cid, start + 1, len) &&
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
		return handle_multipart_mixed (part, mime_type, md, html, stream);
	}
	
	/* setup a 'stack' of related parts */
	related_save = md->related;
	md->related = g_hash_table_new(NULL, NULL);
	
	location = camel_mime_part_get_content_location (part);
	if (location)
		mail_display_push_content_location (md, location);
	
	/* Record the Content-ID/Content-Location of any non-displayed parts. */
	for (i = 0; i < nparts; i++) {
		body_part = camel_multipart_get_part (mp, i);
		if (body_part == display_part)
			continue;
		
		get_cid (body_part, md);
		get_location (body_part, md);
		g_hash_table_insert(md->related, body_part, body_part);
	}
	
	/* Now, display the displayed part. */
	ret = format_mime_part (display_part, md, html, stream);
	
	/* FIXME: flush html stream via gtkhtml_stream_flush which doens't exist yet ... */
	while (gtk_events_pending ())
		gtk_main_iteration ();
	
	/* Now check for related parts which didn't display, display them as attachments */
	for (i = 0; i < nparts; i++) {
		body_part = camel_multipart_get_part (mp, i);
		if (body_part == display_part)
			continue;
		
		if (g_hash_table_lookup(md->related, body_part)) {
			if (ret)
				write_hr (html, stream);
			ret |= format_mime_part (body_part, md, html, stream);
		}
	}
	
	g_hash_table_destroy (md->related);
	md->related = related_save;
	
	if (location)
		mail_display_pop_content_location (md);
	
	return ret;
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
			      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *multipart;
	CamelMimePart *mime_part;
	
	if (!CAMEL_IS_MULTIPART (wrapper)) {
		mail_error_printf (html, stream, "\n%s\n", U_("Could not parse MIME message. Displaying as source."));
		if (mail_content_loaded (wrapper, md, TRUE, NULL, html, NULL))
			handle_text_plain (part, "text/plain", md, html, stream);
		return TRUE;
	}
	
	multipart = CAMEL_MULTIPART (wrapper);
	
	mime_part = find_preferred_alternative (multipart, FALSE);
	if (mime_part)
		return format_mime_part (mime_part, md, html, stream);
	else
		return handle_multipart_mixed (part, mime_type, md, html, stream);
}

/* RFC 1740 */
static gboolean
handle_multipart_appledouble (CamelMimePart *part, const char *mime_type,
			      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	CamelMultipart *multipart;

	if (!CAMEL_IS_MULTIPART (wrapper)) {
		mail_error_printf (html, stream, "\n%s\n", U_("Could not parse MIME message. Displaying as source."));
		if (mail_content_loaded (wrapper, md, TRUE, NULL, html, NULL))
			handle_text_plain (part, "text/plain", md, html, stream);
		return TRUE;
	}

	multipart = CAMEL_MULTIPART (wrapper);

	/* The first part is application/applefile and is not useful
	 * to us. The second part _may_ be displayable data. Most
	 * likely it's application/octet-stream though.
	 */
	part = camel_multipart_get_part (multipart, 1);
	return format_mime_part (part, md, html, stream);
}

static gboolean
handle_message_rfc822 (CamelMimePart *part, const char *mime_type,
		       MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	CamelDataWrapper *wrapper =
		camel_medium_get_content_object (CAMEL_MEDIUM (part));
	
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (wrapper), FALSE);
	
	mail_html_write (html, stream, "<blockquote>");
	mail_format_mime_message (CAMEL_MIME_MESSAGE (wrapper), md, html, stream);
	mail_html_write (html, stream, "</blockquote>");
	
	return TRUE;
}

static gboolean
handle_message_external_body (CamelMimePart *part, const char *mime_type,
			      MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	CamelContentType *type;
	const char *access_type;
	char *url = NULL, *desc = NULL;
	char *fmt;
	
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
		fmt = e_utf8_from_locale_string (_("Pointer to FTP site (%s)"));
		desc = g_strdup_printf (fmt, url);
		g_free (fmt);
	} else if (!g_strcasecmp (access_type, "local-file")) {
		const char *name, *site;
		
		name = header_content_type_param (type, "name");
		if (name == NULL)
			goto fallback;
		site = header_content_type_param (type, "site");
		
		url = g_strdup_printf ("file://%s%s", *name == '/' ? "" : "/",
				       name);
		if (site) {
			fmt = e_utf8_from_locale_string (_("Pointer to local file (%s) "
							   "valid at site \"%s\""));
			desc = g_strdup_printf (fmt, name, site);
			g_free (fmt);
		} else {
			fmt = e_utf8_from_locale_string (_("Pointer to local file (%s)"));
			desc = g_strdup_printf (fmt, name);
			g_free (fmt);
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
		
		fmt = e_utf8_from_locale_string (_("Pointer to remote data (%s)"));
		desc = g_strdup_printf (fmt, url);
		g_free (fmt);
	}
	
 fallback:
	if (!desc) {
		if (access_type) {
			fmt = e_utf8_from_locale_string (_("Pointer to unknown external data "
							   "(\"%s\" type)"));
			desc = g_strdup_printf (fmt, access_type);
			g_free (fmt);
		} else
			desc = e_utf8_from_locale_string (_("Malformed external-body part."));
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
		   MailDisplay *md, GtkHTML *html, GtkHTMLStream *stream)
{
	if (! md->printing) {
		gtk_html_stream_printf (stream,
					"<object classid=\"%s\" type=\"%s\"></object>",
					get_cid (part, md), mime_type);
	}

	return TRUE;
}

/**
 * mail_get_message_rfc822:
 * @message: the message
 * @want_plain: whether the caller prefers plain to html
 * @cite: whether or not to cite the message text
 *
 * See mail_get_message_body() below for more details.
 *
 * Return value: an HTML string representing the text parts of @message.
 **/
static char *
mail_get_message_rfc822 (CamelMimeMessage *message, gboolean want_plain, gboolean cite)
{
	CamelDataWrapper *contents;
	GString *retval;
	const CamelInternetAddress *cia;
	char *text, *citation, *buf, *html;
	time_t date_val;
	int offset;

	contents = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	text = mail_get_message_body (contents, want_plain, cite);
	if (!text)
		text = g_strdup ("");
	citation = cite ? "&gt; " : "";
	retval = g_string_new (NULL);

	/* Kludge: if text starts with "<PRE>", wrap it around the
	 * headers too so we won't get a blank line between them for the
	 * <P> to <PRE> switch.
	 */
	if (!g_strncasecmp (text, "<pre>", 5))
		g_string_sprintfa (retval, "<PRE>");

	/* create credits */
	cia = camel_mime_message_get_from (message);
	buf = camel_address_format (CAMEL_ADDRESS (cia));
	if (buf) {
		html = e_text_to_html (buf, E_TEXT_TO_HTML_CONVERT_NL);
		g_string_sprintfa (retval, "%s<b>From:</b> %s<br>",
				   citation, html);
		g_free (html);
		g_free (buf);
	}

	cia = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	buf = camel_address_format (CAMEL_ADDRESS (cia));
	if (buf) {
		html = e_text_to_html (buf, E_TEXT_TO_HTML_CONVERT_NL);
		g_string_sprintfa (retval, "%s<b>To:</b> %s<br>",
				   citation, html);
		g_free (html);
		g_free (buf);
	}

	cia = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	buf = camel_address_format (CAMEL_ADDRESS (cia));
	if (buf) {
		html = e_text_to_html (buf, E_TEXT_TO_HTML_CONVERT_NL);
		g_string_sprintfa (retval, "%s<b>Cc:</b> %s<br>",
				   citation, html);
		g_free (html);
		g_free (buf);
	}

	buf = (char *) camel_mime_message_get_subject (message);
	if (buf) {
		html = e_text_to_html (buf, E_TEXT_TO_HTML_CONVERT_NL | E_TEXT_TO_HTML_CONVERT_URLS);
		g_string_sprintfa (retval, "%s<b>Subject:</b> %s<br>",
				   citation, html);
		g_free (html);
	}

	date_val = camel_mime_message_get_date (message, &offset);
	buf = header_format_date (date_val, offset);
	html = e_text_to_html (buf, E_TEXT_TO_HTML_CONVERT_NL);
	g_string_sprintfa (retval, "%s<b>Date:</b> %s<br>", citation, html);
	g_free (html);
	g_free (buf);
	
	if (!g_strncasecmp (text, "<pre>", 5))
		g_string_sprintfa (retval, "%s<br>%s", citation, text + 5);
	else
		g_string_sprintfa (retval, "%s<br>%s", citation, text);
	g_free (text);

	buf = retval->str;
	g_string_free (retval, FALSE);
	return buf;
}

/**
 * mail_get_message_body:
 * @data: the message or mime part content
 * @want_plain: whether the caller prefers plain to html
 * @cite: whether or not to cite the message text
 *
 * This creates an HTML string representing @data. If @want_plain is %TRUE,
 * it will be an HTML string that looks like a text/plain representation
 * of @data (but it will still be HTML).
 *
 * If @cite is %TRUE, the message will be cited as a reply, using "> "s.
 *
 * Return value: the HTML string, which the caller must free, or
 * %NULL if @data doesn't include any data which should be forwarded or
 * replied to.
 **/
char *
mail_get_message_body (CamelDataWrapper *data, gboolean want_plain, gboolean cite)
{
	char *subtext, *old, *div, *text = NULL;
	CamelContentType *mime_type;
	CamelCipherContext *cipher;
	GByteArray *bytes = NULL;
	CamelMimePart *subpart;
	CamelMultipart *mp;
	int i, nparts;
	
	mime_type = camel_data_wrapper_get_mime_type_field (data);
	
	/* If it is message/rfc822 or message/news, extract the
	 * important headers and recursively process the body.
	 */
	if (header_content_type_is (mime_type, "message", "rfc822") ||
	    header_content_type_is (mime_type, "message", "news"))
		return mail_get_message_rfc822 (CAMEL_MIME_MESSAGE (data), want_plain, cite);
	
	/* If it's a vcard or icalendar, ignore it. */
	if (header_content_type_is (mime_type, "text", "x-vcard") ||
	    header_content_type_is (mime_type, "text", "calendar"))
		return NULL;
	
	/* Get the body data for other text/ or message/ types */
	if (header_content_type_is (mime_type, "text", "*") ||
	    header_content_type_is (mime_type, "message", "*")) {
		bytes = mail_format_get_data_wrapper_text (data, NULL);
		
		if (bytes) {
			g_byte_array_append (bytes, "", 1);
			text = bytes->data;
			g_byte_array_free (bytes, FALSE);
		}
		
		if (text && !header_content_type_is (mime_type, "text", "html")) {
			char *html;
			
			html = e_text_to_html (text, E_TEXT_TO_HTML_PRE | E_TEXT_TO_HTML_CONVERT_URLS |
					       (cite ? E_TEXT_TO_HTML_CITE : 0));
			g_free (text);
			text = html;
		}
		return text;
	}
	
	/* If it's not message and it's not text, and it's not
	 * multipart, we don't want to deal with it.
	 */
	if (!header_content_type_is (mime_type, "multipart", "*"))
		return NULL;
	
	mp = CAMEL_MULTIPART (data);
	
	if (CAMEL_IS_MULTIPART_ENCRYPTED (mp)) {
		cipher = camel_gpg_context_new (session);
		subpart = camel_multipart_encrypted_decrypt (CAMEL_MULTIPART_ENCRYPTED (mp),
							     cipher, NULL);
		if (!subpart)
			return NULL;
		
		data = camel_medium_get_content_object (CAMEL_MEDIUM (subpart));
		return mail_get_message_body (data, want_plain, cite);
	} else if (header_content_type_is (mime_type, "multipart", "alternative")) {
		/* Pick our favorite alternative and reply to it. */
		
		subpart = find_preferred_alternative (mp, want_plain);
		if (!subpart)
			return NULL;
		
		data = camel_medium_get_content_object (CAMEL_MEDIUM (subpart));
		return mail_get_message_body (data, want_plain, cite);
	}
	
	/* Otherwise, concatenate all the parts that we can. */
	if (want_plain) {
		if (cite)
			div = "<br>\n&gt; ----<br>\n&gt; <br>\n";
		else
			div = "<br>\n----<br>\n<br>\n";
	} else
		div = "<br><hr><br>";
	
	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		subpart = camel_multipart_get_part (mp, i);
		
		/* only add to the body contents if it is marked as "inline" */
		if (!mail_part_is_inline (subpart))
			continue;
		
		data = camel_medium_get_content_object (CAMEL_MEDIUM (subpart));
		subtext = mail_get_message_body (data, want_plain, cite);
		if (!subtext)
			continue;
		
		if (text) {
			old = text;
			text = g_strdup_printf ("%s%s%s", old, div, subtext);
			g_free (subtext);
			g_free (old);
		} else
			text = subtext;
	}
	
	return text;
}
