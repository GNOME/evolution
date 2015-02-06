/*
 * e-mail-part-utils.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include "e-mail-part-utils.h"
#include "e-mail-parser-extension.h"

#include <e-util/e-util.h>
#include <gdk/gdk.h>

#include <libsoup/soup.h>

#include <string.h>

#define d(x)

/**
 * e_mail_part_is_secured:
 * @part: a #CamelMimePart
 *
 * Whether @part is signed or encrypted or not.
 *
 * Return Value: TRUE/FALSE
 */
gboolean
e_mail_part_is_secured (CamelMimePart *part)
{
	CamelContentType *ct = camel_mime_part_get_content_type (part);

	return (camel_content_type_is (ct, "multipart", "signed") ||
		camel_content_type_is (ct, "multipart", "encrypted") ||
		camel_content_type_is (ct, "application", "x-inlinepgp-signed") ||
		camel_content_type_is (ct, "application", "x-inlinepgp-encrypted") ||
		camel_content_type_is (ct, "application", "x-pkcs7-mime") ||
		camel_content_type_is (ct, "application", "pkcs7-mime"));
}

/*
 * Returns: one of -e-mail-formatter-frame-security-* style classes
 */
const gchar *
e_mail_part_get_frame_security_style (EMailPart *part)
{
	const gchar *frame_style = NULL;

	g_return_val_if_fail (part != NULL, "-e-mail-formatter-frame-security-none");

	if (e_mail_part_get_validity_flags (part) == E_MAIL_PART_VALIDITY_NONE) {
		return "-e-mail-formatter-frame-security-none";
	} else {
		GList *head, *link;

		head = g_queue_peek_head_link (&part->validities);

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPartValidityPair *pair = link->data;
			if (pair->validity->sign.status == CAMEL_CIPHER_VALIDITY_SIGN_BAD) {
				return "-e-mail-formatter-frame-security-bad";
			} else if (pair->validity->sign.status == CAMEL_CIPHER_VALIDITY_SIGN_UNKNOWN) {
				frame_style = "-e-mail-formatter-frame-security-unknown";
			} else if (frame_style == NULL &&
				pair->validity->sign.status == CAMEL_CIPHER_VALIDITY_SIGN_NEED_PUBLIC_KEY) {
				frame_style = "-e-mail-formatter-frame-security-need-key";
			}
		}
	}

	if (frame_style == NULL)
		frame_style = "-e-mail-formatter-frame-security-good";

	return frame_style;
}

/**
 * e_mail_part_snoop_type:
 * @part: a #CamelMimePart
 *
 * Tries to snoop the mime type of a part.
 *
 * Return value: %NULL if unknown (more likely application/octet-stream).
 **/
const gchar *
e_mail_part_snoop_type (CamelMimePart *part)
{
	/* cache is here only to be able still return const gchar * */
	static GHashTable *types_cache = NULL;

	const gchar *filename;
	gchar *name_type = NULL, *magic_type = NULL, *res, *tmp;
	CamelDataWrapper *dw;

	filename = camel_mime_part_get_filename (part);
	if (filename != NULL)
		name_type = e_util_guess_mime_type (filename, FALSE);

	dw = camel_medium_get_content ((CamelMedium *) part);
	if (!camel_data_wrapper_is_offline (dw)) {
		GByteArray *byte_array;
		CamelStream *stream;

		byte_array = g_byte_array_new ();
		stream = camel_stream_mem_new_with_byte_array (byte_array);

		if (camel_data_wrapper_decode_to_stream_sync (dw, stream, NULL, NULL) > 0) {
			gchar *content_type;

			content_type = g_content_type_guess (
				filename, byte_array->data,
				byte_array->len, NULL);

			if (content_type != NULL)
				magic_type = g_content_type_get_mime_type (content_type);

			g_free (content_type);
		}

		g_object_unref (stream);
	}

	/* If gvfs doesn't recognize the data by magic, but it
	 * contains English words, it will call it text/plain. If the
	 * filename-based check came up with something different, use
	 * that instead and if it returns "application/octet-stream"
	 * try to do better with the filename check.
	 */

	if (magic_type) {
		if (name_type
		    && (!strcmp (magic_type, "text/plain")
			|| !strcmp (magic_type, "application/octet-stream")))
			res = name_type;
		else
			res = magic_type;
	} else
		res = name_type;

	if (res != name_type)
		g_free (name_type);

	if (res != magic_type)
		g_free (magic_type);

	if (!types_cache)
		types_cache = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) NULL);

	if (res) {
		tmp = g_hash_table_lookup (types_cache, res);
		if (tmp) {
			g_free (res);
			res = tmp;
		} else {
			g_hash_table_insert (types_cache, res, res);
		}
	}

	d (printf ("Snooped mime type %s\n", res));
	return res;

	/* We used to load parts to check their type, we don't anymore,
	 * see bug #211778 for some discussion */
}

/**
 * e_mail_part_is_attachment
 * @part: Part to check.
 *
 * Returns true if the part is an attachment.
 *
 * A part is not considered an attachment if it is a
 * multipart, or a text part with no filename.  It is used
 * to determine if an attachment header should be displayed for
 * the part.
 *
 * Content-Disposition is not checked.
 *
 * Return value: TRUE/FALSE
 **/
gboolean
e_mail_part_is_attachment (CamelMimePart *part)
{
	/*CamelContentType *ct = camel_mime_part_get_content_type(part);*/
	CamelDataWrapper *dw = camel_medium_get_content ((CamelMedium *) part);

	if (!dw)
		return 0;

	d (printf ("checking is attachment %s/%s\n", dw->mime_type->type, dw->mime_type->subtype));
	return !(camel_content_type_is (dw->mime_type, "multipart", "*")
		 || camel_content_type_is (
			dw->mime_type, "application", "x-pkcs7-mime")
		 || camel_content_type_is (
			dw->mime_type, "application", "pkcs7-mime")
		 || camel_content_type_is (
			dw->mime_type, "application", "x-inlinepgp-signed")
		 || camel_content_type_is (
			dw->mime_type, "application", "x-inlinepgp-encrypted")
		 || camel_content_type_is (
			dw->mime_type, "x-evolution", "evolution-rss-feed")
		 || camel_content_type_is (dw->mime_type, "text", "calendar")
		 || camel_content_type_is (dw->mime_type, "text", "x-calendar")
		 || (camel_content_type_is (dw->mime_type, "text", "*")
		     && camel_mime_part_get_filename (part) == NULL));
}

/**
 * e_mail_part_preserve_charset_in_content_type:
 * @ipart: Source #CamelMimePart
 * @opart: Target #CamelMimePart
 *
 * Copies 'charset' part of content-type header from @ipart to @opart.
 */
void
e_mail_part_preserve_charset_in_content_type (CamelMimePart *ipart,
                                              CamelMimePart *opart)
{
	CamelDataWrapper *data_wrapper;
	CamelContentType *content_type;
	const gchar *charset;

	g_return_if_fail (ipart != NULL);
	g_return_if_fail (opart != NULL);

	data_wrapper = camel_medium_get_content (CAMEL_MEDIUM (ipart));
	content_type = camel_data_wrapper_get_mime_type_field (data_wrapper);

	if (content_type == NULL)
		return;

	charset = camel_content_type_param (content_type, "charset");

	if (charset == NULL || *charset == '\0')
		return;

	data_wrapper = camel_medium_get_content (CAMEL_MEDIUM (opart));
	content_type = camel_data_wrapper_get_mime_type_field (data_wrapper);

	if (content_type)
		camel_content_type_set_param (content_type, "charset", charset);

	/* update charset also on the part itself */
	data_wrapper = CAMEL_DATA_WRAPPER (opart);
	content_type = camel_data_wrapper_get_mime_type_field (data_wrapper);
	if (content_type)
		camel_content_type_set_param (content_type, "charset", charset);
}

/**
 * e_mail_part_get_related_display_part:
 * @part: a multipart/related or multipart/alternative #CamelMimePart
 * @out_displayid: (out) returns index of the returned part
 *
 * Goes through all subparts of given @part and tries to determine which
 * part should be displayed and which parts are just attachments to the
 * part.
 *
 * Return Value: A #CamelMimePart that should be displayed
 */
CamelMimePart *
e_mail_part_get_related_display_part (CamelMimePart *part,
                                      gint *out_displayid)
{
	CamelMultipart *mp;
	CamelMimePart *body_part, *display_part = NULL;
	CamelContentType *content_type;
	const gchar *start;
	gint i, nparts, displayid = 0;

	mp = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);

	if (!CAMEL_IS_MULTIPART (mp))
		return NULL;

	nparts = camel_multipart_get_number (mp);
	content_type = camel_mime_part_get_content_type (part);
	start = camel_content_type_param (content_type, "start");
	if (start && strlen (start) > 2) {
		gint len;
		const gchar *cid;

		/* strip <>'s from CID */
		len = strlen (start) - 2;
		start++;

		for (i = 0; i < nparts; i++) {
			body_part = camel_multipart_get_part (mp, i);
			cid = camel_mime_part_get_content_id (body_part);

			if (cid && !strncmp (cid, start, len) && strlen (cid) == len) {
				display_part = body_part;
				displayid = i;
				break;
			}
		}
	} else {
		display_part = camel_multipart_get_part (mp, 0);
	}

	if (out_displayid)
		*out_displayid = displayid;

	return display_part;
}

void
e_mail_part_animation_extract_frame (GBytes *bytes,
                                     gchar **out_frame,
                                     gsize *out_len)
{
	GdkPixbufLoader *loader;
	GdkPixbufAnimation *animation;
	GdkPixbuf *frame_buf;
	const guchar *bytes_data;
	gsize bytes_size;

	/* GIF89a (GIF image signature) */
	const guchar GIF_HEADER[] = { 0x47, 0x49, 0x46, 0x38, 0x39, 0x61 };
	const gint GIF_HEADER_LEN = sizeof (GIF_HEADER);

	/* NETSCAPE2.0 (extension describing animated GIF, starts on 0x310) */
	const guchar GIF_APPEXT[] = { 0x4E, 0x45, 0x54, 0x53, 0x43, 0x41,
				     0x50, 0x45, 0x32, 0x2E, 0x30 };
	const gint GIF_APPEXT_LEN = sizeof (GIF_APPEXT);

	g_return_if_fail (out_frame != NULL);
	g_return_if_fail (out_len != NULL);

	*out_frame = NULL;
	*out_len = 0;

	if (bytes == NULL)
		return;

	bytes_data = g_bytes_get_data (bytes, &bytes_size);

	if (bytes_size == 0)
		return;

	/* Check if the image is an animated GIF. We don't care about any
	 * other animated formats (APNG or MNG) as WebKit does not support them
	 * and displays only the first frame. */
	if ((bytes_size < 0x331)
	    || (memcmp (bytes_data, GIF_HEADER, GIF_HEADER_LEN) != 0)
	    || (memcmp (&bytes_data[0x310], GIF_APPEXT, GIF_APPEXT_LEN) != 0)) {
		*out_frame = g_memdup (bytes_data, bytes_size);
		*out_len = bytes_size;
		return;
	}

	loader = gdk_pixbuf_loader_new ();
	gdk_pixbuf_loader_write (loader, bytes_data, bytes_size, NULL);
	gdk_pixbuf_loader_close (loader, NULL);
	animation = gdk_pixbuf_loader_get_animation (loader);
	if (!animation) {
		*out_frame = g_memdup (bytes_data, bytes_size);
		*out_len = bytes_size;
		g_object_unref (loader);
		return;
	}

	/* Extract first frame */
	frame_buf = gdk_pixbuf_animation_get_static_image (animation);
	if (!frame_buf) {
		*out_frame = g_memdup (bytes_data, bytes_size);
		*out_len = bytes_size;
		g_object_unref (loader);
		g_object_unref (animation);
		return;
	}

	/* Unforunately, GdkPixbuf cannot save to GIF, but WebKit does not
	 * have any trouble displaying PNG image despite the part having
	 * image/gif mime-type */
	gdk_pixbuf_save_to_buffer (
		frame_buf, out_frame, out_len, "png", NULL, NULL);

	g_object_unref (loader);
}

/**
 * e_mail_part_build_url:
 * @folder: (allow-none) a #CamelFolder with the message or %NULL
 * @message_uid: uid of the message within the @folder
 * @first_param_name: Name of first query parameter followed by GType of it's value and value
 * terminated by %NULL.
 *
 * Construct a URI for message.
 *
 * The URI can contain multiple query parameters. The list of parameters must be
 * NULL-terminated. Each query must contain name, GType of value and value.
 *
 * Return Value: a URL of a message or part
 */
gchar *
e_mail_part_build_uri (CamelFolder *folder,
                       const gchar *message_uid,
                       const gchar *first_param_name,
                       ...)
{
	CamelStore *store;
	gchar *uri, *tmp;
	va_list ap;
	const gchar *name;
	const gchar *service_uid, *folder_name;
	gchar *encoded_message_uid;
	gchar separator;

	g_return_val_if_fail (message_uid && *message_uid, NULL);

	if (!folder) {
		folder_name = "generic";
		service_uid = "generic";
	} else {
		tmp = (gchar *) camel_folder_get_full_name (folder);
		folder_name = (const gchar *) soup_uri_encode (tmp, NULL);
		store = camel_folder_get_parent_store (folder);
		if (store)
			service_uid = camel_service_get_uid (CAMEL_SERVICE (store));
		else
			service_uid = "generic";
	}

	encoded_message_uid = soup_uri_encode (message_uid, NULL);
	tmp = g_strdup_printf (
		"mail://%s/%s/%s",
		service_uid,
		folder_name,
		encoded_message_uid);
	g_free (encoded_message_uid);

	if (folder) {
		g_free ((gchar *) folder_name);
	}

	va_start (ap, first_param_name);
	name = first_param_name;
	separator = '?';
	while (name) {
		gchar *tmp2;
		gint type = va_arg (ap, gint);
		switch (type) {
			case G_TYPE_INT:
			case G_TYPE_BOOLEAN: {
				gint val = va_arg (ap, gint);
				tmp2 = g_strdup_printf (
					"%s%c%s=%d", tmp,
						separator, name, val);
				break;
			}
			case G_TYPE_FLOAT:
			case G_TYPE_DOUBLE: {
				gdouble val = va_arg (ap, double);
				tmp2 = g_strdup_printf (
					"%s%c%s=%f", tmp,
						separator, name, val);
				break;
			}
			case G_TYPE_STRING: {
				gchar *val = va_arg (ap, gchar *);
				gchar *escaped = soup_uri_encode (val, NULL);
				tmp2 = g_strdup_printf (
					"%s%c%s=%s", tmp,
						separator, name, escaped);
				g_free (escaped);
				break;
			}
			default:
				g_warning ("Invalid param type %s", g_type_name (type));
				va_end (ap);
				return NULL;
		}

		g_free (tmp);
		tmp = tmp2;

		if (separator == '?')
			separator = '&';

		name = va_arg (ap, gchar *);
	}
	va_end (ap);

	uri = tmp;
	if (uri == NULL)
		return NULL;

	/* For some reason, webkit won't accept URL with username, but
	 * without password (mail://store@host/folder/mail), so we
	 * will replace the '@' symbol by '/' to get URL like
	 * mail://store/host/folder/mail which is OK
	 */
	while ((tmp = strchr (uri, '@')) != NULL) {
		tmp[0] = '/';
	}

	return uri;
}

/**
 * e_mail_part_describe:
 * @part: a #CamelMimePart
 * @mime_type: MIME type of the content
 *
 * Generate a simple textual description of a part, @mime_type represents
 * the content.
 *
 * Return value:
 **/
gchar *
e_mail_part_describe (CamelMimePart *part,
                      const gchar *mime_type)
{
	GString *stext;
	const gchar *filename, *description;
	gchar *content_type, *desc;

	stext = g_string_new ("");
	content_type = g_content_type_from_mime_type (mime_type);
	desc = g_content_type_get_description (
		content_type != NULL ? content_type : mime_type);
	g_free (content_type);
	g_string_append_printf (
		stext, _("%s attachment"), desc ? desc : mime_type);
	g_free (desc);

	filename = camel_mime_part_get_filename (part);
	description = camel_mime_part_get_description (part);

	if (!filename || !*filename) {
		CamelDataWrapper *content;

		content = camel_medium_get_content (CAMEL_MEDIUM (part));

		if (CAMEL_IS_MIME_MESSAGE (content))
			filename = camel_mime_message_get_subject (
				CAMEL_MIME_MESSAGE (content));
	}

	if (filename != NULL && *filename != '\0') {
		gchar *basename = g_path_get_basename (filename);
		g_string_append_printf (stext, " (%s)", basename);
		g_free (basename);
	}

	if (description != NULL && *description != '\0' &&
		g_strcmp0 (filename, description) != 0)
		g_string_append_printf (stext, ", \"%s\"", description);

	return g_string_free (stext, FALSE);
}

gboolean
e_mail_part_is_inline (CamelMimePart *mime_part,
                       GQueue *extensions)
{
	EMailParserExtension *extension;
	EMailParserExtensionClass *class;
	const gchar *disposition;
	gboolean is_inline = FALSE;

	disposition = camel_mime_part_get_disposition (mime_part);

	if (disposition != NULL)
		is_inline = (g_ascii_strcasecmp (disposition, "inline") == 0);

	if ((extensions == NULL) || g_queue_is_empty (extensions))
		return is_inline;

	extension = g_queue_peek_head (extensions);
	class = E_MAIL_PARSER_EXTENSION_GET_CLASS (extension);

	/* Some types need to override the disposition.
	 * e.g. application/x-pkcs7-mime */
	if (class->flags & E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION)
		return TRUE;

	if (disposition != NULL)
		return is_inline;

	/* Otherwise, use the default for this handler type. */
	return (class->flags & E_MAIL_PARSER_EXTENSION_INLINE) != 0;
}

/**
 * e_mail_part_utils_body_refers:
 * @body: text body to search for references in; can be %NULL, then returns %FALSE
 * @cid: a Content-ID to search for; if found in body, it should be of form "cid:xxxxx"; can be %NULL
 *
 * Returns whether @body contains a reference to @cid enclosed in quotes;
 *    returns %FALSE if any of the arguments is %NULL.
 **/
gboolean
e_mail_part_utils_body_refers (const gchar *body,
                               const gchar *cid)
{
	const gchar *ptr;

	if (!body || !cid || !*cid)
		return FALSE;

	ptr = body;
	while (ptr = strstr (ptr, cid), ptr != NULL) {
		if (ptr - body > 1 && ptr[-1] == '\"' && ptr[strlen (cid)] == '\"')
			return TRUE;

		ptr++;
	}

	return FALSE;
}
