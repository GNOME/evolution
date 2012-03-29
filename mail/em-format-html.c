/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE /* Enable strcasestr in string.h */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#include <gtk/gtk.h>
#ifdef G_OS_WIN32
/* Work around 'DATADIR' and 'interface' lossage in <windows.h> */
#define DATADIR crap_DATADIR
#include <windows.h>
#undef DATADIR
#undef interface
#endif

#include <libebackend/e-extensible.h>
#include <libedataserver/e-time-utils.h>
#include <libedataserver/e-data-server-util.h>	/* for e_utf8_strftime, what about e_time_format_time? */

#include "e-util/e-datetime-format.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util.h"
#include "misc/e-web-view.h"

#include <shell/e-shell.h>

#include <glib/gi18n.h>

#include <JavaScriptCore/JavaScript.h>
#include <webkit/webkit.h>

#include <libemail-utils/mail-mt.h>
#include <libemail-engine/e-mail-enumtypes.h>
#include <libemail-engine/e-mail-utils.h>
#include <libemail-engine/mail-config.h>

#include "em-format-html.h"
#include "em-utils.h"
#include "e-mail-display.h"
#include <em-format/em-inline-filter.h>

#define EM_FORMAT_HTML_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FORMAT_HTML, EMFormatHTMLPrivate))

#define d(x)

struct _EMFormatHTMLPrivate {
	GdkColor colors[EM_FORMAT_HTML_NUM_COLOR_TYPES];
	EMailImageLoadingPolicy image_loading_policy;

	guint can_load_images	: 1;
	guint only_local_photos	: 1;
	guint show_sender_photo	: 1;
	guint show_real_date	: 1;
        guint animate_images    : 1;
};

static gpointer parent_class;

enum {
	PROP_0,
	PROP_BODY_COLOR,
	PROP_CITATION_COLOR,
	PROP_CONTENT_COLOR,
	PROP_FRAME_COLOR,
	PROP_HEADER_COLOR,
	PROP_IMAGE_LOADING_POLICY,
	PROP_MARK_CITATIONS,
	PROP_ONLY_LOCAL_PHOTOS,
	PROP_SHOW_SENDER_PHOTO,
	PROP_SHOW_REAL_DATE,
	PROP_TEXT_COLOR,
        PROP_ANIMATE_IMAGES
};

#define EFM_MESSAGE_START_ANAME "evolution_message_start"
#define EFH_MESSAGE_START "<A name=\"" EFM_MESSAGE_START_ANAME "\"></A>"

static void efh_parse_image			(EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void efh_parse_text_enriched		(EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void efh_parse_text_plain		(EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void efh_parse_text_html			(EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void efh_parse_message_external		(EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void efh_parse_message_deliverystatus	(EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void efh_parse_message_rfc822		(EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);

static void efh_write_image			(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efh_write_text_enriched		(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efh_write_text_plain		(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efh_write_text_html			(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efh_write_source			(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efh_write_headers			(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efh_write_attachment		(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efh_write_error			(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efh_write_message_rfc822            (EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);

static void efh_format_full_headers		(EMFormatHTML *efh, GString *buffer, CamelMedium *part, gboolean all_headers, gboolean visible, GCancellable *cancellable);
static void efh_format_short_headers		(EMFormatHTML *efh, GString *buffer, CamelMedium *part, gboolean visible, GCancellable *cancellable);

static void efh_write_message                   (EMFormat *emf, GList *puris, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);

/*****************************************************************************/
static void
efh_parse_image (EMFormat *emf,
                 CamelMimePart *part,
                 GString *part_id,
                 EMFormatParserInfo *info,
                 GCancellable *cancellable)
{
	EMFormatPURI *puri;
	const gchar *tmp;
	gchar *cid;
	gint len;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	tmp = camel_mime_part_get_content_id (part);
	if (!tmp) {
		em_format_parse_part_as (emf, part, part_id, info,
                                "x-evolution/message/attachment", cancellable);
		return;
	}

	cid = g_strdup_printf ("cid:%s", tmp);
	len = part_id->len;
	g_string_append (part_id, ".image");
	puri = em_format_puri_new (emf, sizeof (EMFormatPURI), part, part_id->str);
	puri->cid = cid;
	puri->write_func = efh_write_image;
	puri->mime_type = g_strdup (info->handler->mime_type);
	puri->is_attachment = TRUE;
	puri->validity = info->validity ? camel_cipher_validity_clone (info->validity) : NULL;
	puri->validity_type = info->validity_type;

	em_format_add_puri (emf, puri);
	g_string_truncate (part_id, len);
}

static void
efh_parse_text_enriched (EMFormat *emf,
                         CamelMimePart *part,
                         GString *part_id,
                         EMFormatParserInfo *info,
                         GCancellable *cancellable)
{
	EMFormatPURI *puri;
	const gchar *tmp;
	gchar *cid;
	gint len;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	tmp = camel_mime_part_get_content_id (part);
	if (!tmp) {
		cid = g_strdup_printf ("em-no-cid:%s", part_id->str);
	} else {
		cid = g_strdup_printf ("cid:%s", tmp);
	}

	len = part_id->len;
	g_string_append (part_id, ".text_enriched");
	puri = em_format_puri_new (emf, sizeof (EMFormatPURI), part, part_id->str);
	puri->cid = cid;
	puri->mime_type = g_strdup (info->handler->mime_type);
	puri->write_func = efh_write_text_enriched;
	puri->validity = info->validity ? camel_cipher_validity_clone (info->validity) : NULL;
	puri->validity_type = info->validity_type;
	puri->is_attachment = info->is_attachment;

	em_format_add_puri (emf, puri);
	g_string_truncate (part_id, len);
}

static void
efh_parse_text_plain (EMFormat *emf,
                      CamelMimePart *part,
                      GString *part_id,
                      EMFormatParserInfo *info,
                      GCancellable *cancellable)
{
	EMFormatPURI *puri;
	CamelStream *filtered_stream, *null;
	CamelMultipart *mp;
	CamelDataWrapper *dw;
	CamelContentType *type;
	gint i, count, len;
	EMInlineFilter *inline_filter;
	gboolean charset_added = FALSE;
	const gchar *snoop_type = NULL;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	dw = camel_medium_get_content ((CamelMedium *) part);
	if (!dw)
		return;

	/* This scans the text part for inline-encoded data, creates
	 * a multipart of all the parts inside it. */

	/* FIXME: We should discard this multipart if it only contains
	 * the original text, but it makes this hash lookup more complex */

	/* TODO: We could probably put this in the superclass, since
	 * no knowledge of html is required - but this messes with
	 * filters a bit.  Perhaps the superclass should just deal with
	 * html anyway and be done with it ... */

	if (!dw->mime_type)
		snoop_type = em_format_snoop_type (part);

	/* if we had to snoop the part type to get here, then
		* use that as the base type, yuck */
	if (snoop_type == NULL
		|| (type = camel_content_type_decode (snoop_type)) == NULL) {
		type = dw->mime_type;
		camel_content_type_ref (type);
	}

	if (dw->mime_type && type != dw->mime_type && camel_content_type_param (dw->mime_type, "charset")) {
		camel_content_type_set_param (type, "charset", camel_content_type_param (dw->mime_type, "charset"));
		charset_added = TRUE;
	}

	null = camel_stream_null_new ();
	filtered_stream = camel_stream_filter_new (null);
	g_object_unref (null);
	inline_filter = em_inline_filter_new (camel_mime_part_get_encoding (part), type);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream),
		CAMEL_MIME_FILTER (inline_filter));
	camel_data_wrapper_decode_to_stream_sync (
		dw, (CamelStream *) filtered_stream, cancellable, NULL);
	camel_stream_close ((CamelStream *) filtered_stream, cancellable, NULL);
	g_object_unref (filtered_stream);

	mp = em_inline_filter_get_multipart (inline_filter);

	if (charset_added) {
		camel_content_type_set_param (type, "charset", NULL);
	}

	g_object_unref (inline_filter);
	camel_content_type_unref (type);

	/* We handle our made-up multipart here, so we don't recursively call ourselves */
	len = part_id->len;
	count = camel_multipart_get_number (mp);
	for (i = 0; i < count; i++) {
		CamelMimePart *newpart = camel_multipart_get_part (mp, i);

		if (!newpart)
			continue;

		type = camel_mime_part_get_content_type (newpart);
		if (camel_content_type_is (type, "text", "*") && (!camel_content_type_is (type, "text", "calendar"))) {
			gint s_len = part_id->len;

			g_string_append (part_id, ".plain_text");
			puri = em_format_puri_new (emf, sizeof (EMFormatPURI), newpart, part_id->str);
			puri->write_func = efh_write_text_plain;
			puri->mime_type = g_strdup ("text/html");
			puri->validity = info->validity ? camel_cipher_validity_clone (info->validity) : NULL;
			puri->validity_type = info->validity_type;
			puri->is_attachment = info->is_attachment;
			g_string_truncate (part_id, s_len);
			em_format_add_puri (emf, puri);
		} else {
			g_string_append_printf (part_id, ".inline.%d", i);
			em_format_parse_part (emf, CAMEL_MIME_PART (newpart), part_id, info, cancellable);
			g_string_truncate (part_id, len);
		}
	}

	g_object_unref (mp);
}

static void
efh_parse_text_html (EMFormat *emf,
                     CamelMimePart *part,
                     GString *part_id,
                     EMFormatParserInfo *info,
                     GCancellable *cancellable)
{
	EMFormatPURI *puri;
	const gchar *location;
	gchar *cid = NULL;
	CamelURL *base;
	gint len;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	base = em_format_get_base_url (emf);
	location = camel_mime_part_get_content_location (part);
	if (location == NULL) {
		if (base)
			cid = camel_url_to_string (base, 0);
		else
			cid = g_strdup (part_id->str);
	} else {
		if (strchr (location, ':') == NULL && base != NULL) {
			CamelURL *uri;

			uri = camel_url_new_with_base (base, location);
			cid = camel_url_to_string (uri, 0);
			camel_url_free (uri);
		} else {
			cid = g_strdup (location);
		}
	}

	len = part_id->len;
	g_string_append (part_id, ".text_html");
	puri = em_format_puri_new (emf, sizeof (EMFormatPURI), part, part_id->str);
	puri->write_func = efh_write_text_html;
	puri->validity = info->validity ? camel_cipher_validity_clone (info->validity) : NULL;
	puri->validity_type = info->validity_type;
	puri->is_attachment = info->is_attachment;

	em_format_add_puri (emf, puri);
	g_string_truncate (part_id, len);

	if (cid)
		g_free (cid);
}

static void
efh_parse_message_external (EMFormat *emf,
                            CamelMimePart *part,
                            GString *part_id,
                            EMFormatParserInfo *info,
                            GCancellable *cancellable)
{
	EMFormatPURI *puri;
	CamelMimePart *newpart;
	CamelContentType *type;
	const gchar *access_type;
	gchar *url = NULL, *desc = NULL;
	gchar *content;
	gint len;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	newpart = camel_mime_part_new ();

	/* needs to be cleaner */
	type = camel_mime_part_get_content_type (part);
	access_type = camel_content_type_param (type, "access-type");
	if (!access_type) {
		const gchar *msg = _("Malformed external-body part");
		camel_mime_part_set_content (newpart, msg, strlen (msg),
				"text/plain");
		goto addPart;
	}

	if (!g_ascii_strcasecmp(access_type, "ftp") ||
	    !g_ascii_strcasecmp(access_type, "anon-ftp")) {
		const gchar *name, *site, *dir, *mode;
		gchar *path;
		gchar ftype[16];

		name = camel_content_type_param (type, "name");
		site = camel_content_type_param (type, "site");
		dir = camel_content_type_param (type, "directory");
		mode = camel_content_type_param (type, "mode");
		if (name == NULL || site == NULL)
			goto fail;

		/* Generate the path. */
		if (dir)
			path = g_strdup_printf("/%s/%s", *dir=='/'?dir+1:dir, name);
		else
			path = g_strdup_printf("/%s", *name=='/'?name+1:name);

		if (mode && *mode)
			sprintf(ftype, ";type=%c",  *mode);
		else
			ftype[0] = 0;

		url = g_strdup_printf ("ftp://%s%s%s", site, path, ftype);
		g_free (path);
		desc = g_strdup_printf (_("Pointer to FTP site (%s)"), url);
	} else if (!g_ascii_strcasecmp (access_type, "local-file")) {
		const gchar *name, *site;

		name = camel_content_type_param (type, "name");
		site = camel_content_type_param (type, "site");
		if (name == NULL)
			goto fail;

		url = g_filename_to_uri (name, NULL, NULL);
		if (site)
			desc = g_strdup_printf(_("Pointer to local file (%s) valid at site \"%s\""), name, site);
		else
			desc = g_strdup_printf(_("Pointer to local file (%s)"), name);
	} else if (!g_ascii_strcasecmp (access_type, "URL")) {
		const gchar *urlparam;
		gchar *s, *d;

		/* RFC 2017 */
		urlparam = camel_content_type_param (type, "url");
		if (urlparam == NULL)
			goto fail;

		/* For obscure MIMEy reasons, the URL may be split into words */
		url = g_strdup (urlparam);
		s = d = url;
		while (*s) {
			if (!isspace ((guchar) * s))
				*d++ = *s;
			s++;
		}
		*d = 0;
		desc = g_strdup_printf (_("Pointer to remote data (%s)"), url);
	} else
		goto fail;

	content = g_strdup_printf ("<a href=\"%s\">%s</a>", url, desc);
	camel_mime_part_set_content (newpart, content, strlen (content), "text/html");
	g_free (content);

	g_free (url);
	g_free (desc);

fail:
	content = g_strdup_printf (
		_("Pointer to unknown external data (\"%s\" type)"),
		access_type);
	camel_mime_part_set_content (newpart, content, strlen (content), "text/plain");
	g_free (content);

addPart:
	len = part_id->len;
	g_string_append (part_id, ".msg_external");
	puri = em_format_puri_new (emf, sizeof (EMFormatPURI), part, part_id->str);
	puri->write_func = efh_write_text_html;
	puri->mime_type = g_strdup ("text/html");

	em_format_add_puri (emf, puri);
	g_string_truncate (part_id, len);
}

static void
efh_parse_message_deliverystatus (EMFormat *emf,
                                  CamelMimePart *part,
                                  GString *part_id,
                                  EMFormatParserInfo *info,
                                  GCancellable *cancellable)
{
	EMFormatPURI *puri;
	gint len;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	len = part_id->len;
	g_string_append (part_id, ".deliverystatus");
	puri = em_format_puri_new (emf, sizeof (EMFormatPURI), part, part_id->str);
	puri->write_func = efh_write_source;
	puri->mime_type = g_strdup ("text/html");
	puri->validity = info->validity ? camel_cipher_validity_clone (info->validity) : NULL;
	puri->validity_type = info->validity_type;
	puri->is_attachment = info->is_attachment;

	em_format_add_puri (emf, puri);
	g_string_truncate (part_id, len);
}

static void
efh_parse_message_rfc822 (EMFormat *emf,
                          CamelMimePart *part,
                          GString *part_id,
                          EMFormatParserInfo *info,
                          GCancellable *cancellable)
{
	CamelDataWrapper *dw;
	CamelMimePart *opart;
	CamelStream *stream;
	CamelMimeParser *parser;
	gint len;
	EMFormatParserInfo oinfo = *info;
	EMFormatPURI *puri;

	len = part_id->len;
	g_string_append (part_id, ".rfc822");

        /* Create an empty PURI that will represent start of the RFC message */
	puri = em_format_puri_new (emf, sizeof (EMFormatPURI), part, part_id->str);
	puri->write_func = efh_write_message_rfc822;
        puri->mime_type = g_strdup ("text/html");
	puri->is_attachment = info->is_attachment;
	em_format_add_puri (emf, puri);

        /* Now parse the message, creating multiple sub-PURIs */
	stream = camel_stream_mem_new ();
	dw = camel_medium_get_content ((CamelMedium *) part);
	camel_data_wrapper_write_to_stream_sync (dw, stream, cancellable, NULL);
	g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, cancellable, NULL);

	parser = camel_mime_parser_new ();
	camel_mime_parser_init_with_stream (parser, stream, NULL);

	opart = camel_mime_part_new ();
	camel_mime_part_construct_from_parser_sync (opart, parser, cancellable, NULL);

	em_format_parse_part_as (emf, opart, part_id, &oinfo,
		"x-evolution/message", cancellable);

        /* Add another generic PURI that represents end of the RFC message.
         * The em_format_write() function will skip all PURIs between the ".rfc822" 
         * PURI and ".rfc822.end" PURI as they will be rendered in an <iframe> */
        g_string_append (part_id, ".end");
	puri = em_format_puri_new (emf, sizeof (EMFormatPURI), NULL, part_id->str);
	em_format_add_puri (emf, puri);

	g_string_truncate (part_id, len);

	g_object_unref (opart);
	g_object_unref (parser);
	g_object_unref (stream);
}

/*****************************************************************************/

static void
efh_write_image (EMFormat *emf,
                 EMFormatPURI *puri,
                 CamelStream *stream,
                  EMFormatWriterInfo *info,
                 GCancellable *cancellable)
{
	gchar *content;
	EMFormatHTML *efh;
	CamelDataWrapper *dw;
	GByteArray *ba;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	efh = (EMFormatHTML *) emf;

	dw = camel_medium_get_content (CAMEL_MEDIUM (puri->part));
	g_return_if_fail (dw);

	ba = camel_data_wrapper_get_byte_array (dw);

	if (info->mode == EM_FORMAT_WRITE_MODE_RAW) {

		if (!efh->priv->animate_images) {

			gchar *buff;
			gsize len;
			gchar *data;
			GByteArray anim;

			data = g_strndup ((gchar *) ba->data, (gsize) ba->len);
			anim.data = g_base64_decode (data, (gsize *) &(anim.len));
			g_free (data);

			em_format_html_animation_extract_frame (&anim, &buff, &len);

			camel_stream_write (stream, buff, len, cancellable, NULL);

			g_free (buff);
			g_free (anim.data);

		} else {
			CamelStream *stream_filter;
			CamelMimeFilter *filter;

			stream_filter = camel_stream_filter_new (stream);
			filter = camel_mime_filter_basic_new (
					CAMEL_MIME_FILTER_BASIC_BASE64_DEC);

			camel_stream_write (
				stream_filter,
				(gchar *) ba->data, ba->len,
				cancellable, NULL);
			g_object_unref (stream_filter);
			g_object_unref (filter);
		}

	} else {

		gchar *buffer;

		if (!efh->priv->animate_images) {

			gchar *buff;
			gsize len;
			gchar *data;
			GByteArray raw_data;

			data = g_strndup ((gchar *) ba->data, ba->len);
			raw_data.data =  (guint8 *) g_base64_decode (
						data, (gsize *) &(raw_data.len));
			g_free (data);

			em_format_html_animation_extract_frame (&raw_data, &buff, &len);

			content = g_base64_encode ((guchar *) buff, len);
			g_free (buff);
			g_free (raw_data.data);

		} else {
			content = g_strndup ((gchar *) ba->data, ba->len);
		}

		/* The image is already base64-encrypted so we can directly
		 * paste it to the output */
		buffer = g_strdup_printf (
			"<img src=\"data:%s;base64,%s\" style=\"max-width: 100%%;\" />",
			puri->mime_type ? puri->mime_type : "image/*", content);

		camel_stream_write_string (stream, buffer, cancellable, NULL);
		g_free (buffer);
		g_free (content);
	}
}

static void
efh_write_text_enriched (EMFormat *emf,
                         EMFormatPURI *puri,
                         CamelStream *stream,
                         EMFormatWriterInfo *info,
                         GCancellable *cancellable)
{
	EMFormatHTML *efh = EM_FORMAT_HTML (emf);
	CamelStream *filtered_stream;
	CamelMimeFilter *enriched;
	guint32 flags = 0;
	GString *buffer;
	CamelContentType *ct;
	gchar *mime_type = NULL;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	ct = camel_mime_part_get_content_type (puri->part);
	if (ct) {
		mime_type = camel_content_type_simple (ct);
	}

	if (!g_strcmp0(mime_type, "text/richtext")) {
		flags = CAMEL_MIME_FILTER_ENRICHED_IS_RICHTEXT;
		camel_stream_write_string (
			stream, "\n<!-- text/richtext -->\n",
			cancellable, NULL);
	} else {
		camel_stream_write_string (
			stream, "\n<!-- text/enriched -->\n",
			cancellable, NULL);
	}

	if (mime_type)
		g_free (mime_type);

	enriched = camel_mime_filter_enriched_new (flags);
	filtered_stream = camel_stream_filter_new (stream);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream), enriched);
	g_object_unref (enriched);

	buffer = g_string_new ("");

	g_string_append_printf (buffer,
		"<div class=\"part-container\" style=\"border-color: #%06x; "
		"background-color: #%06x; color: #%06x;\">"
		"<div class=\"part-container-inner-margin\">\n",
		e_color_to_value (&efh->priv->colors[
			EM_FORMAT_HTML_COLOR_FRAME]),
		e_color_to_value (&efh->priv->colors[
			EM_FORMAT_HTML_COLOR_CONTENT]),
		e_color_to_value (&efh->priv->colors[
			EM_FORMAT_HTML_COLOR_TEXT]));

	camel_stream_write_string (stream, buffer->str, cancellable, NULL);
	g_string_free (buffer, TRUE);

	em_format_format_text (
		emf, (CamelStream *) filtered_stream,
		(CamelDataWrapper *) puri->part, cancellable);

	g_object_unref (filtered_stream);
	camel_stream_write_string (stream, "</div></div>", cancellable, NULL);
}

static void
efh_write_text_plain (EMFormat *emf,
                      EMFormatPURI *puri,
                      CamelStream *stream,
                       EMFormatWriterInfo *info,
                      GCancellable *cancellable)
{
	CamelDataWrapper *dw;
	CamelStream *filtered_stream;
	CamelMimeFilter *html_filter;
	EMFormatHTML *efh = (EMFormatHTML *) emf;
	gchar *content;
	const gchar *format;
	guint32 flags;
	guint32 rgb;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	flags = efh->text_html_flags;

	dw = camel_medium_get_content (CAMEL_MEDIUM (puri->part));

	/* Check for RFC 2646 flowed text. */
	if (camel_content_type_is(dw->mime_type, "text", "plain")
	    && (format = camel_content_type_param(dw->mime_type, "format"))
	    && !g_ascii_strcasecmp(format, "flowed"))
		flags |= CAMEL_MIME_FILTER_TOHTML_FORMAT_FLOWED;

	rgb = e_color_to_value (
		&efh->priv->colors[EM_FORMAT_HTML_COLOR_CITATION]);
	filtered_stream = camel_stream_filter_new (stream);
	html_filter = camel_mime_filter_tohtml_new (flags, rgb);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream), html_filter);
	g_object_unref (html_filter);

	content = g_strdup_printf (
		"<div class=\"part-container\" style=\"border-color: #%06x; "
		"background-color: #%06x; color: #%06x;\">"
		"<div class=\"part-container-inner-margin\">\n",
		e_color_to_value (&efh->priv->colors[
			EM_FORMAT_HTML_COLOR_FRAME]),
		e_color_to_value (&efh->priv->colors[
			EM_FORMAT_HTML_COLOR_CONTENT]),
		e_color_to_value (&efh->priv->colors[
			EM_FORMAT_HTML_COLOR_TEXT]));

	camel_stream_write_string (stream, content, cancellable, NULL);
	em_format_format_text (emf, filtered_stream, (CamelDataWrapper *) puri->part, cancellable);

	g_object_unref (filtered_stream);
	g_free (content);

	camel_stream_write_string (stream, "</div></div>\n", cancellable, NULL);
}

static gchar *
get_tag (const gchar *tag_name,
         gchar *opening,
         gchar *closing)
{
	gchar *t;
	gboolean has_end;

	for (t = closing - 1; t != opening; t--) {
		if (*t != ' ')
			break;
	}

	/* Not a pair tag */
	if (*t == '/')
		return g_strndup (opening, closing - opening + 1);

	for (t = closing; t && *t; t++) {
		if (*t == '<')
			break;
	}

	do {
		if (*t == '/') {
			has_end = TRUE;
			break;
		}

		if (*t == '>') {
			has_end = FALSE;
			break;
		}

		t++;

	} while (t && *t);

	/* Broken HTML? */
	if (!has_end)
		return g_strndup (opening, closing - opening + 1);

	do {
		if ((*t != ' ') && (*t != '/'))
			break;

		t++;
	} while (t && *t);

	if (g_strncasecmp (t, tag_name, strlen (tag_name)) == 0) {

		closing = strstr (t, ">");

		return g_strndup (opening, closing - opening + strlen (tag_name));
	}

	/* Broken HTML? */
	return g_strndup (opening, closing - opening + 1);
}

static void
efh_write_text_html (EMFormat *emf,
                     EMFormatPURI *puri,
                     CamelStream *stream,
                     EMFormatWriterInfo *info,
                     GCancellable *cancellable)
{
	EMFormatHTML *efh = (EMFormatHTML *) emf;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	if (info->mode == EM_FORMAT_WRITE_MODE_RAW) {
		em_format_format_text (emf, stream,
			(CamelDataWrapper *) puri->part, cancellable);

	} else if (info->mode == EM_FORMAT_WRITE_MODE_PRINTING) {
		GString *string;
		GByteArray *ba;
		gchar *pos;
		GList *tags, *iter;
		gboolean valid;
		gchar *tag;
		const gchar *document_end;
		gint length;
		gint i;
		CamelStream *decoded_stream;

		decoded_stream = camel_stream_mem_new ();
		em_format_format_text (emf, decoded_stream,
			(CamelDataWrapper *) puri->part, cancellable);
		g_seekable_seek (G_SEEKABLE (decoded_stream), 0, G_SEEK_SET, cancellable, NULL);

		ba = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (decoded_stream));
		string = g_string_new_len ((gchar *) ba->data, ba->len);

		g_object_unref (decoded_stream);

		tags = NULL;
		pos = string->str;
		valid = FALSE;
		do {
			gchar *closing;
			gchar *opening;

			pos = strstr (pos + 1, "<");
			if (!pos)
				break;

			opening = pos;
			closing = strstr (pos, ">");

			/* Find where the actual tag name begins */
			for (tag = pos + 1; tag && *tag; tag++) {
				if (*tag != ' ')
					break;
			}

			if (g_ascii_strncasecmp (tag, "style", 5) == 0) {
				tags = g_list_append (
					tags,
					get_tag ("style", opening, closing));
			} else if (g_ascii_strncasecmp (tag, "script", 6) == 0) {
				tags = g_list_append (
					tags,
                                        get_tag ("script", opening, closing));
			} else if (g_ascii_strncasecmp (tag, "link", 4) == 0) {
				tags = g_list_append (
					tags,
                                        get_tag ("link", opening, closing));
			} else if (g_ascii_strncasecmp (tag, "body", 4) == 0) {
				valid = TRUE;
				break;
			}

		} while (TRUE);

		/* Something's wrong, let's write the entire HTML and hope
		 * that WebKit can handle it */
		if (!valid) {
			EMFormatWriterInfo i = *info;
			i.mode = EM_FORMAT_WRITE_MODE_RAW;
			efh_write_text_html (emf, puri, stream, &i, cancellable);
			return;
		}

		/*	        include the "body" as well -----v */
		g_string_erase (string, 0, tag - string->str + 4);
		g_string_prepend (string, "<div ");

		for (iter = tags; iter; iter = iter->next) {
			g_string_prepend (string, iter->data);
		}

		g_list_free_full (tags, g_free);

		/* that's reversed </body></html>... */
		document_end = ">lmth/<>ydob/<";
		length = strlen (document_end);
		tag = string->str + string->len - 1;
		i = 0;
		valid = FALSE;
		while (i < length - 1) {
			gchar c;

			if (g_ascii_isspace (*tag)) {
				tag--;
				continue;
			}

			if ((*tag >= 'A') && (*tag <= 'Z'))
				c = *tag + 32;
			else
				c = *tag;

			if (c == document_end[i]) {
				tag--;
				i++;
				valid = TRUE;
				continue;
			}

			valid = FALSE;
		}

		if (valid)
			g_string_truncate (string, tag - string->str);

		camel_stream_write_string (stream, string->str, cancellable, NULL);

		g_string_free (string, TRUE);
	} else {
		gchar *str;
		gchar *uri;

		uri = em_format_build_mail_uri (
				emf->folder, emf->message_uid,
				"part_id", G_TYPE_STRING, puri->uri,
				"mode", G_TYPE_INT, EM_FORMAT_WRITE_MODE_RAW,
				NULL);

		str = g_strdup_printf (
			"<div class=\"part-container\" style=\"border-color: #%06x; "
			"background-color: #%06x;\">"
			"<div class=\"part-container-inner-margin\">\n"
			"<iframe width=\"100%%\" height=\"auto\""
			" frameborder=\"0\" src=\"%s\"></iframe>"
                        "</div></div>",
			e_color_to_value (&efh->priv->colors[EM_FORMAT_HTML_COLOR_FRAME]),
			e_color_to_value (&efh->priv->colors[EM_FORMAT_HTML_COLOR_CONTENT]),
			uri);

		camel_stream_write_string (stream, str, cancellable, NULL);

		g_free (str);
		g_free (uri);
	}
}

static void
efh_write_source (EMFormat *emf,
                  EMFormatPURI *puri,
                  CamelStream *stream,
                  EMFormatWriterInfo *info,
                  GCancellable *cancellable)
{
	EMFormatHTML *efh = (EMFormatHTML *) emf;
	GString *buffer;
	CamelStream *filtered_stream;
	CamelMimeFilter *filter;
	CamelDataWrapper *dw = (CamelDataWrapper *) puri->part;

	filtered_stream = camel_stream_filter_new (stream);

	filter = camel_mime_filter_tohtml_new (
		CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
		CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT, 0);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream), filter);
	g_object_unref (filter);

	buffer = g_string_new ("");

	g_string_append_printf (
		buffer, "<div class=\"part-container\" style=\"border: 0; background: #%06x; color: #%06x;\" >",
		e_color_to_value (
			&efh->priv->colors[
			EM_FORMAT_HTML_COLOR_BODY]),
		e_color_to_value (
			&efh->priv->colors[
			EM_FORMAT_HTML_COLOR_HEADER]));

	camel_stream_write_string (
		stream, buffer->str, cancellable, NULL);
	camel_stream_write_string (
		stream, "<code class=\"pre\">", cancellable, NULL);
	camel_data_wrapper_write_to_stream_sync (dw, filtered_stream,
		cancellable, NULL);
	camel_stream_write_string (
		stream, "</code>", cancellable, NULL);

	g_object_unref (filtered_stream);
	g_string_free (buffer, TRUE);
}

static void
efh_write_headers (EMFormat *emf,
                   EMFormatPURI *puri,
                   CamelStream *stream,
                   EMFormatWriterInfo *info,
                   GCancellable *cancellable)
{
	GString *buffer;
	EMFormatHTML *efh = (EMFormatHTML *) emf;
	gint bg_color;

	if (!puri->part)
		return;

	buffer = g_string_new ("");

	if (info->mode & EM_FORMAT_WRITE_MODE_PRINTING) {
		GdkColor white = { 0, G_MAXUINT16, G_MAXUINT16, G_MAXUINT16 };
		bg_color = e_color_to_value (&white);
	} else {
		bg_color = e_color_to_value (&efh->priv->colors[EM_FORMAT_HTML_COLOR_BODY]);
	}

	g_string_append_printf (
		buffer,
		"<div class=\"headers\" style=\"background: #%06x;\">"
		"<table border=\"0\" width=\"100%%\" style=\"color: #%06x;\">\n"
		"<tr><td valign=\"top\" width=\"16\">\n",
		bg_color,
		e_color_to_value (&efh->priv->colors[EM_FORMAT_HTML_COLOR_HEADER]));

	if (info->headers_collapsable) {
		g_string_append_printf (buffer,
			"<img src=\"evo-file://%s/%s\" class=\"navigable\" "
			     "id=\"__evo-collapse-headers-img\" />"
                        "</td><td>",
			EVOLUTION_IMAGESDIR,
			(info->headers_collapsed) ? "plus.png" : "minus.png");

		efh_format_short_headers (efh, buffer, (CamelMedium *) puri->part,
			info->headers_collapsed,
			cancellable);
	}

	efh_format_full_headers (efh, buffer, (CamelMedium *) puri->part,
		(info->mode == EM_FORMAT_WRITE_MODE_ALL_HEADERS),
		!info->headers_collapsed,
		cancellable);

	g_string_append (buffer, "</td></tr></table></div>");

	camel_stream_write_string (stream, buffer->str, cancellable, NULL);

	g_string_free (buffer, true);
}

static void
efh_write_error (EMFormat *emf,
                 EMFormatPURI *puri,
                 CamelStream *stream,
                 EMFormatWriterInfo *info,
                 GCancellable *cancellable)
{
	CamelStream *filtered_stream;
	CamelMimeFilter *filter;
	CamelDataWrapper *dw;

	dw = camel_medium_get_content ((CamelMedium *) puri->part);

	camel_stream_write_string (stream, "<em><font color=\"red\">", cancellable, NULL);

	filtered_stream = camel_stream_filter_new (stream);
	filter = camel_mime_filter_tohtml_new (CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	camel_stream_filter_add (CAMEL_STREAM_FILTER (filtered_stream), filter);
	g_object_unref (filter);

	camel_data_wrapper_decode_to_stream_sync (dw, filtered_stream, cancellable, NULL);

	g_object_unref (filtered_stream);

	camel_stream_write_string (stream, "</font></em><br>", cancellable, NULL);
}

static void
efh_write_message_rfc822 (EMFormat *emf,
                          EMFormatPURI *puri,
                          CamelStream *stream,
                          EMFormatWriterInfo *info,
                          GCancellable *cancellable)
{
	if (info->mode == EM_FORMAT_WRITE_MODE_RAW) {

		GList *puris;
		GList *iter;

                /* Create a new fake list of PURIs which will contain only
                 * PURIs from this message. */
		iter = g_hash_table_lookup (emf->mail_part_table, puri->uri);
		if (!iter || !iter->next)
			return;

		iter = iter->next;
		puris = NULL;
		while (iter) {

			EMFormatPURI *p;
			p = iter->data;

                        if (g_str_has_suffix (p->uri, ".rfc822.end"))
				break;

			puris = g_list_append (puris, p);
			iter = iter->next;

		};

		efh_write_message (emf, puris, stream, info, cancellable);

		g_list_free (puris);

	} else if (info->mode == EM_FORMAT_WRITE_MODE_PRINTING) {

		GList *iter;
		gboolean can_write = FALSE;

		iter = g_hash_table_lookup (emf->mail_part_table, puri->uri);
		if (!iter || !iter->next)
			return;

                /* Skip everything before attachment bar, inclusive */\
		iter = iter->next;
		while (iter) {

			EMFormatPURI *p = iter->data;

                        /* EMFormatHTMLPrint has registered a special writer
                         * for headers, try to find it and use it. */
                        if (g_str_has_suffix (p->uri, ".headers")) {

				const EMFormatHandler *handler;

				handler = em_format_find_handler (
                                        emf, "x-evolution/message/headers");
				if (handler && handler->write_func)
					handler->write_func (emf, p, stream, info, cancellable);

				iter = iter->next;
				continue;
			}

                        if (g_str_has_suffix (p->uri, ".rfc822.end"))
				break;

                        if (g_str_has_suffix (p->uri, ".attachment-bar"))
				can_write = TRUE;

			if (can_write && p->write_func) {
				p->write_func (
					emf, p, stream, info, cancellable);
			}

			iter = iter->next;
		}

	} else {
		gchar *str;
		gchar *uri;

		EMFormatHTML *efh = (EMFormatHTML *) emf;
		EMFormatPURI *p;
		GList *iter;

		iter = g_hash_table_lookup (emf->mail_part_table, puri->uri);
		if (!iter || !iter->next)
			return;

		iter = iter->next;
		p = iter->data;

		uri = em_format_build_mail_uri (emf->folder, emf->message_uid,
                        "part_id", G_TYPE_STRING, p->uri,
                        "mode", G_TYPE_INT, EM_FORMAT_WRITE_MODE_RAW,
			NULL);

		str = g_strdup_printf (
                        "<div class=\"part-container\" style=\"border-color: #%06x; "
                        "background-color: #%06x;\">"
                        "<div class=\"part-container-inner-margin\">\n"
                        "<iframe width=\"100%%\" height=\"auto\""
                        " frameborder=\"0\" src=\"%s\" name=\"%s\"></iframe>"
                        "</div></div>",
			e_color_to_value (&efh->priv->colors[EM_FORMAT_HTML_COLOR_FRAME]),
			e_color_to_value (&efh->priv->colors[EM_FORMAT_HTML_COLOR_CONTENT]),
			uri, puri->uri);

		camel_stream_write_string (stream, str, cancellable, NULL);

		g_free (str);
		g_free (uri);
	}

}

/*****************************************************************************/

/* Notes:
 *
 * image/tiff is omitted because it's a multi-page image format, but
 * gdk-pixbuf unconditionally renders the first page only, and doesn't
 * even indicate through meta-data whether multiple pages are present
 * (see bug 335959).  Therefore, make no attempt to render TIFF images
 * inline and defer to an application that can handle multi-page TIFF
 * files properly like Evince or Gimp.  Once the referenced bug is
 * fixed we can reevaluate this policy.
 */
static EMFormatHandler type_builtin_table[] = {
	{ (gchar *) "image/gif", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/jpeg", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/png", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/x-png", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/x-bmp", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/bmp", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/svg", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/x-cmu-raster", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/x-ico", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/x-portable-anymap", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/x-portable-bitmap", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/x-portable-graymap", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/x-portable-pixmap", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/x-xpixmap", efh_parse_image, efh_write_image, },
	{ (gchar *) "text/enriched", efh_parse_text_enriched, efh_write_text_enriched, },
	{ (gchar *) "text/plain", efh_parse_text_plain, efh_write_text_plain, },
	{ (gchar *) "text/html", efh_parse_text_html, efh_write_text_html, },
	{ (gchar *) "text/richtext", efh_parse_text_enriched, efh_write_text_enriched, },
	{ (gchar *) "text/*", efh_parse_text_plain, efh_write_text_plain, },
        { (gchar *) "message/rfc822", efh_parse_message_rfc822, efh_write_message_rfc822, EM_FORMAT_HANDLER_INLINE | EM_FORMAT_HANDLER_COMPOUND_TYPE }, 
        { (gchar *) "message/news", efh_parse_message_rfc822, 0, EM_FORMAT_HANDLER_INLINE | EM_FORMAT_HANDLER_COMPOUND_TYPE },
        { (gchar *) "message/delivery-status", efh_parse_message_deliverystatus, efh_write_text_plain, },
	{ (gchar *) "message/external-body", efh_parse_message_external, efh_write_text_plain, },
        { (gchar *) "message/*", efh_parse_message_rfc822, 0, EM_FORMAT_HANDLER_INLINE },

	/* This is where one adds those busted, non-registered types,
	 * that some idiot mailer writers out there decide to pull out
	 * of their proverbials at random. */
	{ (gchar *) "image/jpg", efh_parse_image, efh_write_image, },
	{ (gchar *) "image/pjpeg", efh_parse_image, efh_write_image, },

	/* special internal types */
	{ (gchar *) "x-evolution/message/rfc822", 0, efh_write_text_plain, },
	{ (gchar *) "x-evolution/message/headers", 0, efh_write_headers, },
	{ (gchar *) "x-evolution/message/source", 0, efh_write_source, },
	{ (gchar *) "x-evolution/message/attachment", 0, efh_write_attachment, },
	{ (gchar *) "x-evolution/message/error", 0, efh_write_error, },
};

static void
efh_builtin_init (EMFormatHTMLClass *efhc)
{
	EMFormatClass *emfc;
	gint ii;

	emfc = (EMFormatClass *) efhc;

	for (ii = 0; ii < G_N_ELEMENTS (type_builtin_table); ii++)
		em_format_class_add_handler (
			emfc, &type_builtin_table[ii]);
}

static void
efh_set_property (GObject *object,
                  guint property_id,
                  const GValue *value,
                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BODY_COLOR:
			em_format_html_set_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_BODY,
				g_value_get_boxed (value));
			return;

		case PROP_CITATION_COLOR:
			em_format_html_set_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_CITATION,
				g_value_get_boxed (value));
			return;

		case PROP_CONTENT_COLOR:
			em_format_html_set_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_CONTENT,
				g_value_get_boxed (value));
			return;

		case PROP_FRAME_COLOR:
			em_format_html_set_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_FRAME,
				g_value_get_boxed (value));
			return;

		case PROP_HEADER_COLOR:
			em_format_html_set_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_HEADER,
				g_value_get_boxed (value));
			return;

		case PROP_IMAGE_LOADING_POLICY:
			em_format_html_set_image_loading_policy (
				EM_FORMAT_HTML (object),
				g_value_get_enum (value));
			return;

		case PROP_MARK_CITATIONS:
			em_format_html_set_mark_citations (
				EM_FORMAT_HTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_ONLY_LOCAL_PHOTOS:
			em_format_html_set_only_local_photos (
				EM_FORMAT_HTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_SENDER_PHOTO:
			em_format_html_set_show_sender_photo (
				EM_FORMAT_HTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_REAL_DATE:
			em_format_html_set_show_real_date (
				EM_FORMAT_HTML (object),
				g_value_get_boolean (value));
			return;

		case PROP_TEXT_COLOR:
			em_format_html_set_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_TEXT,
				g_value_get_boxed (value));
			return;

		case PROP_ANIMATE_IMAGES:
			em_format_html_set_animate_images (
				EM_FORMAT_HTML (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
efh_get_property (GObject *object,
                  guint property_id,
                  GValue *value,
                  GParamSpec *pspec)
{
	GdkColor color;

	switch (property_id) {
		case PROP_BODY_COLOR:
			em_format_html_get_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_BODY,
				&color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_CITATION_COLOR:
			em_format_html_get_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_CITATION,
				&color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_CONTENT_COLOR:
			em_format_html_get_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_CONTENT,
				&color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_FRAME_COLOR:
			em_format_html_get_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_FRAME,
				&color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_HEADER_COLOR:
			em_format_html_get_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_HEADER,
				&color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_IMAGE_LOADING_POLICY:
			g_value_set_enum (
				value,
				em_format_html_get_image_loading_policy (
				EM_FORMAT_HTML (object)));
			return;

		case PROP_MARK_CITATIONS:
			g_value_set_boolean (
				value, em_format_html_get_mark_citations (
				EM_FORMAT_HTML (object)));
			return;

		case PROP_ONLY_LOCAL_PHOTOS:
			g_value_set_boolean (
				value, em_format_html_get_only_local_photos (
				EM_FORMAT_HTML (object)));
			return;

		case PROP_SHOW_SENDER_PHOTO:
			g_value_set_boolean (
				value, em_format_html_get_show_sender_photo (
				EM_FORMAT_HTML (object)));
			return;

		case PROP_SHOW_REAL_DATE:
			g_value_set_boolean (
				value, em_format_html_get_show_real_date (
				EM_FORMAT_HTML (object)));
			return;

		case PROP_TEXT_COLOR:
			em_format_html_get_color (
				EM_FORMAT_HTML (object),
				EM_FORMAT_HTML_COLOR_TEXT,
				&color);
			g_value_set_boxed (value, &color);
			return;
		case PROP_ANIMATE_IMAGES:
			g_value_set_boolean (
				value, em_format_html_get_animate_images (
				EM_FORMAT_HTML (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
efh_finalize (GObject *object)
{
	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
efh_write_attachment (EMFormat *emf,
                                          EMFormatPURI *puri,
                                          CamelStream *stream,
                                          EMFormatWriterInfo *info,
                      GCancellable *cancellable)
{
	gchar *text, *html;
	CamelContentType *ct;
	gchar *mime_type;
	const EMFormatHandler *handler;

	/* we display all inlined attachments only */

	/* this could probably be cleaned up ... */
	camel_stream_write_string (
		stream,
		"<table border=1 cellspacing=0 cellpadding=0><tr><td>"
		"<table width=10 cellspacing=0 cellpadding=0>"
		"<tr><td></td></tr></table></td>"
		"<td><table width=3 cellspacing=0 cellpadding=0>"
		"<tr><td></td></tr></table></td><td><font size=-1>\n",
		cancellable, NULL);

	ct = camel_mime_part_get_content_type (puri->part);
	mime_type = camel_content_type_simple (ct);

	/* output some info about it */
	text = em_format_describe_part (puri->part, mime_type);
	html = camel_text_to_html (
		text, ((EMFormatHTML *) emf)->text_html_flags &
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	camel_stream_write_string (stream, html, cancellable, NULL);
	g_free (html);
	g_free (text);

	camel_stream_write_string (
		stream, "</font></td></tr><tr></table>", cancellable, NULL);

	handler = em_format_find_handler (emf, mime_type);
	if (handler && handler->write_func && handler->write_func != efh_write_attachment) {
		if (em_format_is_inline (emf, puri->uri, puri->part, handler))
			handler->write_func (emf, puri, stream, info, cancellable);
	}

	g_free (mime_type);
}

static void
efh_preparse (EMFormat *emf)
{
	CamelInternetAddress *addr;

	EMFormatHTML *efh = EM_FORMAT_HTML (emf);

	if (!emf->message) {
		efh->priv->can_load_images = FALSE;
		return;
	}

	addr = camel_mime_message_get_from (emf->message);
	efh->priv->can_load_images = em_utils_in_addressbook (addr, FALSE);
}

static void
efh_write_message (EMFormat *emf,
                   GList *puris,
                   CamelStream *stream,
                   EMFormatWriterInfo *info,
                   GCancellable *cancellable)
{
	GList *iter;
	EMFormatHTML *efh;
	gchar *header;

	efh = (EMFormatHTML *) emf;

	header = g_strdup_printf (
                "<!DOCTYPE HTML>\n<html>\n"
                "<head>\n<meta name=\"generator\" content=\"Evolution Mail Component\" />\n"
                "<title>Evolution Mail Display</title>\n"
                "<link type=\"text/css\" rel=\"stylesheet\" href=\"evo-file://" EVOLUTION_PRIVDATADIR "/theme/webview.css\" />\n"
                "<style type=\"text/css\">\n"
                "  table th { color: #000; font-weight: bold; }\n"
                "</style>\n"
                "</head><body bgcolor=\"#%06x\">",
		e_color_to_value (&efh->priv->colors[
		EM_FORMAT_HTML_COLOR_BODY]));

	camel_stream_write_string (stream, header, cancellable, NULL);
	g_free (header);

	if (info->mode == EM_FORMAT_WRITE_MODE_SOURCE) {

		efh_write_source (emf, emf->mail_part_list->data,
				  stream, info, cancellable);

                camel_stream_write_string (stream, "</body></html>", cancellable, NULL);
		return;
	}

	for (iter = puris; iter; iter = iter->next) {

		EMFormatPURI *puri = iter->data;

		if (!puri)
			continue;

                /* If current PURI has suffix .rfc822 then iterate through all
                 * subsequent PURIs until PURI with suffix .rfc822.end is found.
                 * These skipped PURIs contain entire RFC message which will
                 * be written in <iframe> as attachment.
                 */
                if (g_str_has_suffix (puri->uri, ".rfc822")) {

                        /* If the PURI is not an attachment, then we must
                         * inline it here otherwise it would not be displayed. */
			if (!puri->is_attachment && puri->write_func) {
                                /* efh_write_message_rfc822 starts parsing _after_
                                 * the passed PURI, so we must give it previous PURI here */
				EMFormatPURI *p;
				if (!iter->prev)
					continue;

				p = iter->prev->data;
				puri->write_func (emf, p, stream, info, cancellable);
			}

                        while (iter && !g_str_has_suffix (puri->uri, ".rfc822.end")) {

				iter = iter->next;
				if (iter)
					puri = iter->data;

                                d(printf(".rfc822 - skipping %s\n", puri->uri));
			}

                        /* Skip the .rfc822.end PURI as well. */
			if (!iter)
				break;

			continue;
		}

		if (puri->write_func && !puri->is_attachment) {
			puri->write_func (emf, puri, stream, info, cancellable);
			d(printf("Writing PURI %s\n", puri->uri));
		} else {
			d(printf("Skipping PURI %s\n", puri->uri));
		}
	}

        camel_stream_write_string (stream, "</body></html>", cancellable, NULL);
}

static void
efh_write (EMFormat *emf,
           CamelStream *stream,
           EMFormatWriterInfo *info,
           GCancellable *cancellable)
{
	efh_write_message (emf, emf->mail_part_list, stream, info, cancellable);
}

static void
efh_base_init (EMFormatHTMLClass *klass)
{
	efh_builtin_init (klass);
}

static void
efh_class_init (EMFormatHTMLClass *klass)
{
	GObjectClass *object_class;
	EMFormatClass *emf_class;

	parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EMFormatHTMLPrivate));

	emf_class = EM_FORMAT_CLASS (klass);
	emf_class->preparse = efh_preparse;
	emf_class->write = efh_write;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = efh_set_property;
	object_class->get_property = efh_get_property;
	object_class->finalize = efh_finalize;

	g_object_class_install_property (
		object_class,
		PROP_BODY_COLOR,
		g_param_spec_boxed (
			"body-color",
			"Body Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CITATION_COLOR,
		g_param_spec_boxed (
			"citation-color",
			"Citation Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CONTENT_COLOR,
		g_param_spec_boxed (
			"content-color",
			"Content Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FRAME_COLOR,
		g_param_spec_boxed (
			"frame-color",
			"Frame Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HEADER_COLOR,
		g_param_spec_boxed (
			"header-color",
			"Header Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	/* FIXME Make this a proper enum property. */
	g_object_class_install_property (
		object_class,
		PROP_IMAGE_LOADING_POLICY,
		g_param_spec_enum (
			"image-loading-policy",
			"Image Loading Policy",
			NULL,
			E_TYPE_MAIL_IMAGE_LOADING_POLICY,
			E_MAIL_IMAGE_LOADING_POLICY_ALWAYS,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MARK_CITATIONS,
		g_param_spec_boolean (
			"mark-citations",
			"Mark Citations",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ONLY_LOCAL_PHOTOS,
		g_param_spec_boolean (
			"only-local-photos",
			"Only Local Photos",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_SENDER_PHOTO,
		g_param_spec_boolean (
			"show-sender-photo",
			"Show Sender Photo",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_REAL_DATE,
		g_param_spec_boolean (
			"show-real-date",
			"Show real Date header value",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_TEXT_COLOR,
		g_param_spec_boxed (
			"text-color",
			"Text Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ANIMATE_IMAGES,
		g_param_spec_boolean (
                        "animate-images",
                        "Animate images",
			NULL,
			FALSE,
			G_PARAM_READWRITE));
}

static void
efh_init (EMFormatHTML *efh,
          EMFormatHTMLClass *klass)
{
	GdkColor *color;

	efh->priv = EM_FORMAT_HTML_GET_PRIVATE (efh);

	g_queue_init (&efh->pending_object_list);

	color = &efh->priv->colors[EM_FORMAT_HTML_COLOR_BODY];
	gdk_color_parse ("#eeeeee", color);

	color = &efh->priv->colors[EM_FORMAT_HTML_COLOR_CONTENT];
	gdk_color_parse ("#ffffff", color);

	color = &efh->priv->colors[EM_FORMAT_HTML_COLOR_FRAME];
	gdk_color_parse ("#3f3f3f", color);

	color = &efh->priv->colors[EM_FORMAT_HTML_COLOR_HEADER];
	gdk_color_parse ("#eeeeee", color);

	color = &efh->priv->colors[EM_FORMAT_HTML_COLOR_TEXT];
	gdk_color_parse ("#000000", color);

	efh->text_html_flags =
		CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
		CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;
	efh->show_icon = TRUE;

	e_extensible_load_extensions (E_EXTENSIBLE (efh));
}

GType
em_format_html_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMFormatHTMLClass),
			(GBaseInitFunc) efh_base_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) efh_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMFormatHTML),
			0,     /* n_preallocs */
			(GInstanceInitFunc) efh_init,
			NULL   /* value_table */
		};

		static const GInterfaceInfo extensible_info = {
			(GInterfaceInitFunc) NULL,
			(GInterfaceFinalizeFunc) NULL,
			NULL   /* interface_data */
		};

		type = g_type_register_static (
			em_format_get_type(), "EMFormatHTML",
			&type_info, G_TYPE_FLAG_ABSTRACT);

		g_type_add_interface_static (
			type, E_TYPE_EXTENSIBLE, &extensible_info);
	}

	return type;
}

/*****************************************************************************/
void
em_format_html_get_color (EMFormatHTML *efh,
                          EMFormatHTMLColorType type,
                          GdkColor *color)
{
	GdkColor *format_color;

	g_return_if_fail (EM_IS_FORMAT_HTML (efh));
	g_return_if_fail (type < EM_FORMAT_HTML_NUM_COLOR_TYPES);
	g_return_if_fail (color != NULL);

	format_color = &efh->priv->colors[type];

	color->red   = format_color->red;
	color->green = format_color->green;
	color->blue  = format_color->blue;
}

void
em_format_html_set_color (EMFormatHTML *efh,
                          EMFormatHTMLColorType type,
                          const GdkColor *color)
{
	GdkColor *format_color;
	const gchar *property_name;

	g_return_if_fail (EM_IS_FORMAT_HTML (efh));
	g_return_if_fail (type < EM_FORMAT_HTML_NUM_COLOR_TYPES);
	g_return_if_fail (color != NULL);

	format_color = &efh->priv->colors[type];

	if (gdk_color_equal (color, format_color))
		return;

	format_color->red   = color->red;
	format_color->green = color->green;
	format_color->blue  = color->blue;

	switch (type) {
		case EM_FORMAT_HTML_COLOR_BODY:
			property_name = "body-color";
			break;
		case EM_FORMAT_HTML_COLOR_CITATION:
			property_name = "citation-color";
			break;
		case EM_FORMAT_HTML_COLOR_CONTENT:
			property_name = "content-color";
			break;
		case EM_FORMAT_HTML_COLOR_FRAME:
			property_name = "frame-color";
			break;
		case EM_FORMAT_HTML_COLOR_HEADER:
			property_name = "header-color";
			break;
		case EM_FORMAT_HTML_COLOR_TEXT:
			property_name = "text-color";
			break;
		default:
			g_return_if_reached ();
	}

	g_object_notify (G_OBJECT (efh), property_name);
}

EMailImageLoadingPolicy
em_format_html_get_image_loading_policy (EMFormatHTML *efh)
{
	g_return_val_if_fail (EM_IS_FORMAT_HTML (efh), 0);

	return efh->priv->image_loading_policy;
}

void
em_format_html_set_image_loading_policy (EMFormatHTML *efh,
                                         EMailImageLoadingPolicy policy)
{
	g_return_if_fail (EM_IS_FORMAT_HTML (efh));

	if (policy == efh->priv->image_loading_policy)
		return;

	efh->priv->image_loading_policy = policy;

	g_object_notify (G_OBJECT (efh), "image-loading-policy");
}

gboolean
em_format_html_get_mark_citations (EMFormatHTML *efh)
{
	guint32 flags;

	g_return_val_if_fail (EM_IS_FORMAT_HTML (efh), FALSE);

	flags = efh->text_html_flags;

	return ((flags & CAMEL_MIME_FILTER_TOHTML_MARK_CITATION) != 0);
}

void
em_format_html_set_mark_citations (EMFormatHTML *efh,
                                   gboolean mark_citations)
{
	g_return_if_fail (EM_IS_FORMAT_HTML (efh));

	if (mark_citations)
		efh->text_html_flags |=
			CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;
	else
		efh->text_html_flags &=
			~CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;

	g_object_notify (G_OBJECT (efh), "mark-citations");
}

gboolean
em_format_html_get_only_local_photos (EMFormatHTML *efh)
{
	g_return_val_if_fail (EM_IS_FORMAT_HTML (efh), FALSE);

	return efh->priv->only_local_photos;
}

void
em_format_html_set_only_local_photos (EMFormatHTML *efh,
                                      gboolean only_local_photos)
{
	g_return_if_fail (EM_IS_FORMAT_HTML (efh));

	efh->priv->only_local_photos = only_local_photos;

	g_object_notify (G_OBJECT (efh), "only-local-photos");
}

gboolean
em_format_html_get_show_sender_photo (EMFormatHTML *efh)
{
	g_return_val_if_fail (EM_IS_FORMAT_HTML (efh), FALSE);

	return efh->priv->show_sender_photo;
}

void
em_format_html_set_show_sender_photo (EMFormatHTML *efh,
                                      gboolean show_sender_photo)
{
	g_return_if_fail (EM_IS_FORMAT_HTML (efh));

	efh->priv->show_sender_photo = show_sender_photo;

	g_object_notify (G_OBJECT (efh), "show-sender-photo");
}

gboolean
em_format_html_get_show_real_date (EMFormatHTML *efh)
{
	g_return_val_if_fail (EM_IS_FORMAT_HTML (efh), FALSE);

	return efh->priv->show_real_date;
}

void
em_format_html_set_show_real_date (EMFormatHTML *efh,
                                   gboolean show_real_date)
{
	g_return_if_fail (EM_IS_FORMAT_HTML (efh));

	efh->priv->show_real_date = show_real_date;

	g_object_notify (G_OBJECT (efh), "show-real-date");
}

gboolean
em_format_html_get_animate_images (EMFormatHTML *efh)
{
	g_return_val_if_fail (EM_IS_FORMAT_HTML (efh), FALSE);

	return efh->priv->animate_images;
}

void
em_format_html_set_animate_images (EMFormatHTML *efh,
                                   gboolean animate_images)
{
	g_return_if_fail (EM_IS_FORMAT_HTML (efh));

	efh->priv->animate_images = animate_images;

        g_object_notify (G_OBJECT (efh), "animate-images");
}

CamelMimePart *
em_format_html_file_part (EMFormatHTML *efh,
                          const gchar *mime_type,
                          const gchar *filename,
                          GCancellable *cancellable)
{
	CamelMimePart *part;
	CamelStream *stream;
	CamelDataWrapper *dw;
	gchar *basename;

	stream = camel_stream_fs_new_with_name (filename, O_RDONLY, 0, NULL);
	if (stream == NULL)
		return NULL;

	dw = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream_sync (
		dw, stream, cancellable, NULL);
	g_object_unref (stream);
	if (mime_type)
		camel_data_wrapper_set_mime_type (dw, mime_type);
	part = camel_mime_part_new ();
	camel_medium_set_content ((CamelMedium *) part, dw);
	g_object_unref (dw);
	basename = g_path_get_basename (filename);
	camel_mime_part_set_filename (part, basename);
	g_free (basename);

	return part;
}

void
em_format_html_format_cert_infos (GQueue *cert_infos,
                                  GString *output_buffer)
{
	GQueue valid = G_QUEUE_INIT;
	GList *head, *link;

	g_return_if_fail (cert_infos != NULL);
	g_return_if_fail (output_buffer != NULL);

	head = g_queue_peek_head_link (cert_infos);

	/* Make sure we have a valid CamelCipherCertInfo before
	 * appending anything to the output buffer, so we don't
	 * end up with "()". */
	for (link = head; link != NULL; link = g_list_next (link)) {
		CamelCipherCertInfo *cinfo = link->data;

		if ((cinfo->name != NULL && *cinfo->name != '\0') ||
		    (cinfo->email != NULL && *cinfo->email != '\0')) {
			g_queue_push_tail (&valid, cinfo);
		}
	}

	if (g_queue_is_empty (&valid))
		return;

	g_string_append (output_buffer, " (");

	while (!g_queue_is_empty (&valid)) {
		CamelCipherCertInfo *cinfo;

		cinfo = g_queue_pop_head (&valid);

		if (cinfo->name != NULL && *cinfo->name != '\0') {
			g_string_append (output_buffer, cinfo->name);

			if (cinfo->email != NULL && *cinfo->email != '\0') {
				g_string_append (output_buffer, " <");
				g_string_append (output_buffer, cinfo->email);
				g_string_append (output_buffer, ">");
			}

		} else if (cinfo->email != NULL && *cinfo->email != '\0') {
			g_string_append (output_buffer, cinfo->email);
		}

		if (!g_queue_is_empty (&valid))
			g_string_append (output_buffer, ", ");
	}

	g_string_append_c (output_buffer, ')');
}

static void
efh_format_text_header (EMFormatHTML *emfh,
                        GString *buffer,
                        const gchar *label,
                        const gchar *value,
                        guint32 flags)
{
	const gchar *fmt, *html;
	gchar *mhtml = NULL;
	gboolean is_rtl;

	if (value == NULL)
		return;

	while (*value == ' ')
		value++;

	if (!(flags & EM_FORMAT_HTML_HEADER_HTML))
		html = mhtml = camel_text_to_html (value, emfh->text_html_flags, 0);
	else
		html = value;

	is_rtl = gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL;

	if (flags & EM_FORMAT_HTML_HEADER_NOCOLUMNS) {
		if (flags & EM_FORMAT_HEADER_BOLD) {
			fmt = "<tr class=\"header-item\" style=\"display: %s\"><td><b>%s:</b> %s</td></tr>";
		} else {
                        fmt = "<tr class=\"header-item\" style=\"display: %s\"><td>%s: %s</td></tr>";
		}
	} else if (flags & EM_FORMAT_HTML_HEADER_NODEC) {
		if (is_rtl)
                        fmt = "<tr class=\"header-item rtl\" style=\"display: %s\"><td align=\"right\" valign=\"top\" width=\"100%%\">%2$s</td><th valign=top align=\"left\" nowrap>%1$s<b>&nbsp;</b></th></tr>";
		else
                        fmt = "<tr class=\"header-item\" style=\"display: %s\"><th align=\"right\" valign=\"top\" nowrap>%s<b>&nbsp;</b></th><td valign=top>%s</td></tr>";
	} else {
		if (flags & EM_FORMAT_HEADER_BOLD) {
			if (is_rtl)
                                fmt = "<tr class=\"header-item rtl\" style=\"display: %s\"><td align=\"right\" valign=\"top\" width=\"100%%\">%2$s</td><th align=\"left\" nowrap>%1$s:<b>&nbsp;</b></th></tr>";
			else
                                fmt = "<tr class=\"header-item\" style=\"display: %s\"><th align=\"right\" valign=\"top\" nowrap>%s:<b>&nbsp;</b></th><td>%s</td></tr>";
		} else {
			if (is_rtl)
                                fmt = "<tr class=\"header-item rtl\" style=\"display: %s\"><td align=\"right\" valign=\"top\" width=\"100%\">%2$s</td><td align=\"left\" nowrap>%1$s:<b>&nbsp;</b></td></tr>";
			else
                                fmt = "<tr class=\"header-item\" style=\"display: %s\"><td align=\"right\" valign=\"top\" nowrap>%s:<b>&nbsp;</b></td><td>%s</td></tr>";
		}
	}

	g_string_append_printf (buffer, fmt,
                (flags & EM_FORMAT_HTML_HEADER_HIDDEN ? "none" : "table-row"), label, html);

	g_free (mhtml);
}

static const gchar *addrspec_hdrs[] = {
	"Sender", "From", "Reply-To", "To", "Cc", "Bcc",
	"Resent-Sender", "Resent-From", "Resent-Reply-To",
	"Resent-To", "Resent-Cc", "Resent-Bcc", NULL
};

static gchar *
efh_format_address (EMFormatHTML *efh,
                    GString *out,
                    struct _camel_header_address *a,
                    gchar *field,
                    gboolean no_links)
{
	guint32 flags = CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES;
	gchar *name, *mailto, *addr;
	gint i = 0;
	gchar *str = NULL;
	gint limit = mail_config_get_address_count ();

	while (a) {
		if (a->name)
			name = camel_text_to_html (a->name, flags, 0);
		else
			name = NULL;

		switch (a->type) {
		case CAMEL_HEADER_ADDRESS_NAME:
			if (name && *name) {
				gchar *real, *mailaddr;

				if (strchr (a->name, ',') || strchr (a->name, ';'))
					g_string_append_printf (out, "&quot;%s&quot;", name);
				else
					g_string_append (out, name);

				g_string_append (out, " &lt;");

				/* rfc2368 for mailto syntax and url encoding extras */
				if ((real = camel_header_encode_phrase ((guchar *) a->name))) {
					mailaddr = g_strdup_printf("%s <%s>", real, a->v.addr);
					g_free (real);
					mailto = camel_url_encode (mailaddr, "?=&()");
					g_free (mailaddr);
				} else {
					mailto = camel_url_encode (a->v.addr, "?=&()");
				}
			} else {
				mailto = camel_url_encode (a->v.addr, "?=&()");
			}
			addr = camel_text_to_html (a->v.addr, flags, 0);
			if (no_links)
				g_string_append_printf (out, "%s", addr);
			else
				g_string_append_printf (out, "<a href=\"mailto:%s\">%s</a>", mailto, addr);
			g_free (mailto);
			g_free (addr);

			if (name && *name)
				g_string_append (out, "&gt;");
			break;
		case CAMEL_HEADER_ADDRESS_GROUP:
			g_string_append_printf (out, "%s: ", name);
			efh_format_address (efh, out, a->v.members, field, no_links);
			g_string_append_printf (out, ";");
			break;
		default:
			g_warning ("Invalid address type");
			break;
		}

		g_free (name);

		i++;
		a = a->next;
		if (a)
			g_string_append (out, ", ");

		/* Let us add a '...' if we have more addresses */
		if (limit > 0 && (i == limit - 1)) {
			const gchar *id = NULL;

			if (strcmp (field, _("To")) == 0) {
				id = "to";
			} else if (strcmp (field, _("Cc")) == 0) {
				id = "cc";
			} else if (strcmp (field, _("Bcc")) == 0) {
				id = "bcc";
			}

			if (id) {
				g_string_append_printf (out,
					"<span id=\"__evo-moreaddr-%s\" "
					      "style=\"display: none;\">", id);
				str = g_strdup_printf (
					"<img src=\"evo-file://%s/plus.png\" "
					     "id=\"__evo-moreaddr-img-%s\" class=\"navigable\">",
					EVOLUTION_IMAGESDIR, id);
			}
		}
	}

	if (str) {
		const gchar *id = NULL;

		if (strcmp (field, _("To")) == 0) {
			id = "to";
		} else if (strcmp (field, _("Cc")) == 0) {
			id = "cc";
		} else if (strcmp (field, _("Bcc")) == 0) {
			id = "bcc";
		}

		if (id) {
			g_string_append_printf (out,
				"</span>"
				"<span class=\"navigable\" "
					"id=\"__evo-moreaddr-ellipsis-%s\" "
					"style=\"display: inline;\">...</span>",
				id);
		}
	}

	return str;
}

static void
canon_header_name (gchar *name)
{
	gchar *inptr = name;

	/* canonicalise the header name... first letter is
	 * capitalised and any letter following a '-' also gets
	 * capitalised */

	if (*inptr >= 'a' && *inptr <= 'z')
		*inptr -= 0x20;

	inptr++;

	while (*inptr) {
		if (inptr[-1] == '-' && *inptr >= 'a' && *inptr <= 'z')
			*inptr -= 0x20;
		else if (*inptr >= 'A' && *inptr <= 'Z')
			*inptr += 0x20;

		inptr++;
	}
}

void
em_format_html_format_header (EMFormat *emf,
                              GString *buffer,
                              CamelMedium *part,
                              struct _camel_header_raw *header,
                              guint32 flags,
                              const gchar *charset)
{
	EMFormatHTML *efh = EM_FORMAT_HTML (emf);
	gchar *name, *buf, *value = NULL;
	const gchar *label, *txt;
	gboolean addrspec = FALSE;
	gchar *str_field = NULL;
	gint i;

	name = g_alloca (strlen (header->name) + 1);
	strcpy (name, header->name);
	canon_header_name (name);

	for (i = 0; addrspec_hdrs[i]; i++) {
		if (!strcmp (name, addrspec_hdrs[i])) {
			addrspec = TRUE;
			break;
		}
	}

	label = _(name);

	if (addrspec) {
		struct _camel_header_address *addrs;
		GString *html;
		gchar *img;
		const gchar *charset = em_format_get_charset (emf) ?
				em_format_get_charset (emf) : em_format_get_default_charset (emf);

		buf = camel_header_unfold (header->value);
		if (!(addrs = camel_header_address_decode (buf, charset))) {
			g_free (buf);
			return;
		}

		g_free (buf);

		html = g_string_new("");
		img = efh_format_address (efh, html, addrs, (gchar *) label,
			(flags & EM_FORMAT_HTML_HEADER_NOLINKS));

		if (img) {
			str_field = g_strdup_printf ("%s%s:", img, label);
			label = str_field;
			flags |= EM_FORMAT_HTML_HEADER_NODEC;
			g_free (img);
		}

		camel_header_address_list_clear (&addrs);
		txt = value = html->str;
		g_string_free (html, FALSE);

		flags |= EM_FORMAT_HEADER_BOLD | EM_FORMAT_HTML_HEADER_HTML;
	} else if (!strcmp (name, "Subject")) {
		buf = camel_header_unfold (header->value);
		txt = value = camel_header_decode_string (buf, charset);
		g_free (buf);

		flags |= EM_FORMAT_HEADER_BOLD;
	} else if (!strcmp(name, "X-evolution-mailer")) {
		/* pseudo-header */
		label = _("Mailer");
		txt = value = camel_header_format_ctext (header->value, charset);
		flags |= EM_FORMAT_HEADER_BOLD;
	} else if (!strcmp (name, "Date") || !strcmp (name, "Resent-Date")) {
		gint msg_offset, local_tz;
		time_t msg_date;
		struct tm local;
		gchar *html;
		gboolean hide_real_date;

		hide_real_date = !em_format_html_get_show_real_date (efh);

		txt = header->value;
		while (*txt == ' ' || *txt == '\t')
			txt++;

		html = camel_text_to_html (txt, efh->text_html_flags, 0);

		msg_date = camel_header_decode_date (txt, &msg_offset);
		e_localtime_with_offset (msg_date, &local, &local_tz);

		/* Convert message offset to minutes (e.g. -0400 --> -240) */
		msg_offset = ((msg_offset / 100) * 60) + (msg_offset % 100);
		/* Turn into offset from localtime, not UTC */
		msg_offset -= local_tz / 60;

		/* value will be freed at the end */
		if (!hide_real_date && !msg_offset) {
			/* No timezone difference; just show the real Date: header */
			txt = value = html;
		} else {
			gchar *date_str;

			date_str = e_datetime_format_format ("mail", "header",
							     DTFormatKindDateTime, msg_date);

			if (hide_real_date) {
				/* Show only the local-formatted date, losing all timezone
				 * information like Outlook does. Should we attempt to show
				 * it somehow? */
				txt = value = date_str;
			} else {
				txt = value = g_strdup_printf ("%s (<I>%s</I>)", html, date_str);
				g_free (date_str);
			}
			g_free (html);
		}
		flags |= EM_FORMAT_HTML_HEADER_HTML | EM_FORMAT_HEADER_BOLD;
	} else if (!strcmp(name, "Newsgroups")) {
		struct _camel_header_newsgroup *ng, *scan;
		GString *html;

		buf = camel_header_unfold (header->value);

		if (!(ng = camel_header_newsgroups_decode (buf))) {
			g_free (buf);
			return;
		}

		g_free (buf);

		html = g_string_new("");
		scan = ng;
		while (scan) {
			if (flags & EM_FORMAT_HTML_HEADER_NOLINKS)
				g_string_append_printf (html, "%s", scan->newsgroup);
			else
				g_string_append_printf(html, "<a href=\"news:%s\">%s</a>",
					scan->newsgroup, scan->newsgroup);
			scan = scan->next;
			if (scan)
				g_string_append_printf(html, ", ");
		}

		camel_header_newsgroups_free (ng);

		txt = html->str;
		g_string_free (html, FALSE);
		flags |= EM_FORMAT_HEADER_BOLD | EM_FORMAT_HTML_HEADER_HTML;
	} else if (!strcmp (name, "Received") || !strncmp (name, "X-", 2)) {
		/* don't unfold Received nor extension headers */
		txt = value = camel_header_decode_string (header->value, charset);
	} else {
		/* don't unfold Received nor extension headers */
		buf = camel_header_unfold (header->value);
		txt = value = camel_header_decode_string (buf, charset);
		g_free (buf);
	}

	efh_format_text_header (efh, buffer, label, txt, flags);

	g_free (value);
	g_free (str_field);
}

static void
efh_format_short_headers (EMFormatHTML *efh,
                          GString *buffer,
                          CamelMedium *part,
                          gboolean visible,
                          GCancellable *cancellable)
{
	EMFormat *emf = EM_FORMAT (efh);
	const gchar *charset;
	CamelContentType *ct;
	const gchar *hdr_charset;
	gchar *evolution_imagesdir;
	gchar *subject = NULL;
	struct _camel_header_address *addrs = NULL;
	struct _camel_header_raw *header;
	GString *from;
	gboolean is_rtl;

	if (cancellable && g_cancellable_is_cancelled (cancellable))
		return;

	ct = camel_mime_part_get_content_type ((CamelMimePart *) part);
	charset = camel_content_type_param (ct, "charset");
	charset = camel_iconv_charset_name (charset);
	hdr_charset = em_format_get_charset (emf) ?
			em_format_get_charset (emf) : em_format_get_default_charset (emf);

	evolution_imagesdir = g_filename_to_uri (EVOLUTION_IMAGESDIR, NULL, NULL);
	from = g_string_new ("");

	g_string_append_printf (buffer,
                "<table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" "
                       "id=\"__evo-short-headers\" style=\"display: %s\">",
		visible ? "block" : "none");

	header = ((CamelMimePart *) part)->headers;
	while (header) {
		if (!g_ascii_strcasecmp (header->name, "From")) {
			GString *tmp;
			if (!(addrs = camel_header_address_decode (header->value, hdr_charset))) {
				header = header->next;
				continue;
			}
			tmp = g_string_new ("");
			efh_format_address (efh, tmp, addrs, header->name, FALSE);

			if (tmp->len)
				g_string_printf (from, _("From: %s"), tmp->str);
			g_string_free (tmp, TRUE);

		} else if (!g_ascii_strcasecmp (header->name, "Subject")) {
			gchar *buf = NULL;
			subject = camel_header_unfold (header->value);
			buf = camel_header_decode_string (subject, hdr_charset);
			g_free (subject);
			subject = camel_text_to_html (buf, CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT, 0);
			g_free (buf);
		}
		header = header->next;
	}

	is_rtl = gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL;
	if (is_rtl) {
		g_string_append_printf (
			buffer,
                        "<tr><td width=\"100%%\" align=\"right\">%s%s%s <strong>%s</strong></td></tr>",
                        from->len ? "(" : "", from->str, from->len ? ")" : "",
                        subject ? subject : _("(no subject)"));
	} else {
		g_string_append_printf (
			buffer,
                        "<tr><td><strong>%s</strong> %s%s%s</td></tr>",
                        subject ? subject : _("(no subject)"),
                        from->len ? "(" : "", from->str, from->len ? ")" : "");
	}

	g_string_append (buffer, "</table>");

	g_free (subject);
	if (addrs)
		camel_header_address_list_clear (&addrs);

	g_string_free (from, TRUE);
	g_free (evolution_imagesdir);
}

static void
efh_format_full_headers (EMFormatHTML *efh,
                         GString *buffer,
                         CamelMedium *part,
                         gboolean all_headers,
                         gboolean visible,
                         GCancellable *cancellable)
{
	EMFormat *emf = EM_FORMAT (efh);
	const gchar *charset;
	CamelContentType *ct;
	struct _camel_header_raw *header;
	gboolean have_icon = FALSE;
	const gchar *photo_name = NULL;
	CamelInternetAddress *cia = NULL;
	gboolean face_decoded  = FALSE, contact_has_photo = FALSE;
	guchar *face_header_value = NULL;
	gsize face_header_len = 0;
	gchar *header_sender = NULL, *header_from = NULL, *name;
	gboolean mail_from_delegate = FALSE;
	const gchar *hdr_charset;
	gchar *evolution_imagesdir;

	if (cancellable && g_cancellable_is_cancelled (cancellable))
		return;

	ct = camel_mime_part_get_content_type ((CamelMimePart *) part);
	charset = camel_content_type_param (ct, "charset");
	charset = camel_iconv_charset_name (charset);
	hdr_charset = em_format_get_charset (emf) ?
			em_format_get_charset (emf) : em_format_get_default_charset (emf);

	evolution_imagesdir = g_filename_to_uri (EVOLUTION_IMAGESDIR, NULL, NULL);

	g_string_append_printf (buffer,
                "<table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" "
                       "id=\"__evo-full-headers\" style=\"display: %s\" width=\"100%%\">",
		visible ? "block" : "none");

	header = ((CamelMimePart *) part)->headers;
	while (header) {
		if (!g_ascii_strcasecmp (header->name, "Sender")) {
			struct _camel_header_address *addrs;
			GString *html;

			if (!(addrs = camel_header_address_decode (header->value, hdr_charset)))
				break;

			html = g_string_new("");
			name = efh_format_address (efh, html, addrs, header->name, FALSE);

			header_sender = html->str;
			camel_header_address_list_clear (&addrs);

			g_string_free (html, FALSE);
			g_free (name);
		} else if (!g_ascii_strcasecmp (header->name, "From")) {
			struct _camel_header_address *addrs;
			GString *html;

			if (!(addrs = camel_header_address_decode (header->value, hdr_charset)))
				break;

			html = g_string_new("");
			name = efh_format_address (efh, html, addrs, header->name, FALSE);

			header_from = html->str;
			camel_header_address_list_clear (&addrs);

			g_string_free (html, FALSE);
			g_free (name);
		} else if (!g_ascii_strcasecmp (header->name, "X-Evolution-Mail-From-Delegate")) {
			mail_from_delegate = TRUE;
		}

		header = header->next;
	}

	if (header_sender && header_from && mail_from_delegate) {
		gchar *bold_sender, *bold_from;

		g_string_append (
			buffer,
			"<tr><td><table border=1 width=\"100%%\" "
			"cellspacing=2 cellpadding=2><tr>");
		if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
			g_string_append (
				buffer, "<td align=\"right\" width=\"100%%\">");
		else
			g_string_append (
				buffer, "<td align=\"left\" width=\"100%%\">");
		bold_sender = g_strconcat ("<b>", header_sender, "</b>", NULL);
		bold_from = g_strconcat ("<b>", header_from, "</b>", NULL);
		/* Translators: This message suggests to the receipients
		 * that the sender of the mail is different from the one
		 * listed in From field. */
		g_string_append_printf (
			buffer,
			_("This message was sent by %s on behalf of %s"),
			bold_sender, bold_from);
		g_string_append (buffer, "</td></tr></table></td></tr>");
		g_free (bold_sender);
		g_free (bold_from);
	}

	g_free (header_sender);
	g_free (header_from);

	g_string_append (buffer, "<tr><td><table border=0 cellpadding=\"0\">\n");

	g_free (evolution_imagesdir);

	/* dump selected headers */
	if (all_headers) {
		header = ((CamelMimePart *) part)->headers;
		while (header) {
			em_format_html_format_header (
				emf, buffer, part, header,
				EM_FORMAT_HTML_HEADER_NOCOLUMNS, charset);
			header = header->next;
		}
	} else {
		GList *link;
		gint mailer_shown = FALSE;

		link = g_queue_peek_head_link (&emf->header_list);

		while (link != NULL) {
			EMFormatHeader *h = link->data;
			gint mailer, face;

			header = ((CamelMimePart *) part)->headers;
			mailer = !g_ascii_strcasecmp (h->name, "X-Evolution-Mailer");
			face = !g_ascii_strcasecmp (h->name, "Face");

			while (header) {
				if (em_format_html_get_show_sender_photo (efh) &&
					!photo_name && !g_ascii_strcasecmp (header->name, "From"))
					photo_name = header->value;

				if (!mailer_shown && mailer && (
				    !g_ascii_strcasecmp (header->name, "X-Mailer") ||
				    !g_ascii_strcasecmp (header->name, "User-Agent") ||
				    !g_ascii_strcasecmp (header->name, "X-Newsreader") ||
				    !g_ascii_strcasecmp (header->name, "X-MimeOLE"))) {
					struct _camel_header_raw xmailer, *use_header = NULL;

					if (!g_ascii_strcasecmp (header->name, "X-MimeOLE")) {
						for (use_header = header->next; use_header; use_header = use_header->next) {
							if (!g_ascii_strcasecmp (use_header->name, "X-Mailer") ||
							    !g_ascii_strcasecmp (use_header->name, "User-Agent") ||
							    !g_ascii_strcasecmp (use_header->name, "X-Newsreader")) {
								/* even we have X-MimeOLE, then use rather the standard one, when available */
								break;
							}
						}
					}

					if (!use_header)
						use_header = header;

					xmailer.name = (gchar *) "X-Evolution-Mailer";
					xmailer.value = use_header->value;
					mailer_shown = TRUE;

					em_format_html_format_header (
						emf, buffer, part,
						&xmailer, h->flags, charset);
					if (strstr(use_header->value, "Evolution"))
						have_icon = TRUE;
				} else if (!face_decoded && face && !g_ascii_strcasecmp (header->name, "Face")) {
					gchar *cp = header->value;

					/* Skip over spaces */
					while (*cp == ' ')
						cp++;

					face_header_value = g_base64_decode (
						cp, &face_header_len);
					face_header_value = g_realloc (
						face_header_value,
						face_header_len + 1);
					face_header_value[face_header_len] = 0;
					face_decoded = TRUE;
				/* Showing an encoded "Face" header makes little sense */
				} else if (!g_ascii_strcasecmp (header->name, h->name) && !face) {
					em_format_html_format_header (
						emf, buffer, part,
						header, h->flags, charset);
				}

				header = header->next;
			}

			link = g_list_next (link);
		}
	}

	g_string_append (buffer, "</table></td>");

	if (photo_name) {
		const gchar *classid;
		CamelMimePart *photopart;
		gboolean only_local_photo;

		cia = camel_internet_address_new ();
		camel_address_decode ((CamelAddress *) cia, (const gchar *) photo_name);
		only_local_photo = em_format_html_get_only_local_photos (efh);
		photopart = em_utils_contact_photo (cia, only_local_photo);

		if (photopart) {
			EMFormatPURI *puri;
			contact_has_photo = TRUE;
			classid = "icon:///em-format-html/headers/photo";
			g_string_append_printf (
				buffer,
				"<td align=\"right\" valign=\"top\">"
				"<img width=64 src=\"%s\"></td>",
				classid);
			puri = em_format_puri_new (
					emf, sizeof (EMFormatPURI), photopart, classid);
			puri->write_func = efh_write_image;
			em_format_add_puri (emf, puri);
			g_object_unref (photopart);
		}
		g_object_unref (cia);
	}

	if (!contact_has_photo && face_decoded) {
		const gchar *classid;
		CamelMimePart *part;
		EMFormatPURI *puri;

		part = camel_mime_part_new ();
		camel_mime_part_set_content (
			(CamelMimePart *) part,
			(const gchar *) face_header_value,
			face_header_len, "image/png");
		classid = "icon:///em-format-html/headers/face/photo";
		g_string_append_printf (
			buffer,
			"<td align=\"right\" valign=\"top\">"
			"<img width=48 src=\"%s\"></td>",
			classid);

		puri = em_format_puri_new (
			emf, sizeof (EMFormatPURI), part, classid);
		puri->write_func = efh_write_image;
		em_format_add_puri (emf, puri);

		g_object_unref (part);
		g_free (face_header_value);
	}

	if (have_icon && efh->show_icon) {
		GtkIconInfo *icon_info;
		const gchar *classid;
		CamelMimePart *iconpart = NULL;
		EMFormatPURI *puri;

		classid = "icon:///em-format-html/header/icon";
		g_string_append_printf (
			buffer,
			"<td align=\"right\" valign=\"top\">"
			"<img width=16 height=16 src=\"%s\"></td>",
			classid);
			icon_info = gtk_icon_theme_lookup_icon (
			gtk_icon_theme_get_default (),
			"evolution", 16, GTK_ICON_LOOKUP_NO_SVG);
		if (icon_info != NULL) {
			iconpart = em_format_html_file_part (
				(EMFormatHTML *) emf, "image/png",
				gtk_icon_info_get_filename (icon_info),
				cancellable);
			gtk_icon_info_free (icon_info);
		}
		if (iconpart) {
			puri = em_format_puri_new (
					emf, sizeof (EMFormatPURI), iconpart, classid);
			puri->write_func = efh_write_image;
			em_format_add_puri (emf, puri);
			g_object_unref (iconpart);
		}
	}

	g_string_append (buffer, "</tr></table>");
}

gboolean
em_format_html_can_load_images (EMFormatHTML *efh)
{
	g_return_val_if_fail (EM_IS_FORMAT_HTML (efh), FALSE);

	return ((efh->priv->image_loading_policy == E_MAIL_IMAGE_LOADING_POLICY_ALWAYS) ||
		((efh->priv->image_loading_policy == E_MAIL_IMAGE_LOADING_POLICY_SOMETIMES) &&
		  efh->priv->can_load_images));
}

void
em_format_html_animation_extract_frame (const GByteArray *anim,
                                        gchar **frame,
                                        gsize *len)
{
	GdkPixbufLoader *loader;
	GdkPixbufAnimation *animation;
	GdkPixbuf *frame_buf;

        /* GIF89a (GIF image signature) */
	const gchar GIF_HEADER[] = { 0x47, 0x49, 0x46, 0x38, 0x39, 0x61 };
	const gint   GIF_HEADER_LEN = sizeof (GIF_HEADER);

        /* NETSCAPE2.0 (extension describing animated GIF, starts on 0x310) */
	const gchar GIF_APPEXT[] = { 0x4E, 0x45, 0x54, 0x53, 0x43, 0x41,
				     0x50, 0x45, 0x32, 0x2E, 0x30 };
	const gint   GIF_APPEXT_LEN = sizeof (GIF_APPEXT);

        /* Check if the image is an animated GIF. We don't care about any
         * other animated formats (APNG or MNG) as WebKit does not support them
         * and displays only the first frame. */
	if ((anim->len < 0x331)
	    || (memcmp (anim->data, GIF_HEADER, GIF_HEADER_LEN) != 0)
	    || (memcmp (&anim->data[0x310], GIF_APPEXT, GIF_APPEXT_LEN) != 0)) {

                *frame = g_memdup (anim->data, anim->len);
                *len = anim->len;
		return;
	}

	loader = gdk_pixbuf_loader_new ();
	gdk_pixbuf_loader_write (loader, (guchar *) anim->data, anim->len, NULL);
	gdk_pixbuf_loader_close (loader, NULL);
	animation = gdk_pixbuf_loader_get_animation (loader);
	if (!animation) {

                *frame = g_memdup (anim->data, anim->len);
                *len = anim->len;
		g_object_unref (loader);
		return;
	}

        /* Extract first frame */
	frame_buf = gdk_pixbuf_animation_get_static_image (animation);
	if (!frame_buf) {
                *frame = g_memdup (anim->data, anim->len);
                *len = anim->len;
		g_object_unref (loader);
		g_object_unref (animation);
		return;
	}

        /* Unforunatelly, GdkPixbuf cannot save to GIF, but WebKit does not
         * have any trouble displaying PNG image despite the part having
         * image/gif mime-type */
        gdk_pixbuf_save_to_buffer (frame_buf, frame, len, "png", NULL, NULL);

	g_object_unref (loader);
}
