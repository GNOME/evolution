/*
 * evolution-secure-button.c
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

#include <e-util/e-util.h>

#include "e-mail-formatter-extension.h"

typedef EMailFormatterExtension EMailFormatterSecureButton;
typedef EMailFormatterExtensionClass EMailFormatterSecureButtonClass;

GType e_mail_formatter_secure_button_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterSecureButton,
	e_mail_formatter_secure_button,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"application/vnd.evolution.secure-button",
	NULL
};

static const struct {
	const gchar *icon, *shortdesc, *description;
} smime_sign_table[5] = {
	{ "stock_signature-bad", N_("Unsigned"), N_("This message is not signed. There is no guarantee that this message is authentic.") },
	{ "stock_signature-ok", N_("Valid signature"), N_("This message is signed and is valid meaning that it is very likely that this message is authentic.") },
	{ "stock_signature-bad", N_("Invalid signature"), N_("The signature of this message cannot be verified, it may have been altered in transit.") },
	{ "stock_signature", N_("Valid signature, but cannot verify sender"), N_("This message is signed with a valid signature, but the sender of the message cannot be verified.") },
	{ "stock_signature-bad", N_("Signature exists, but need public key"), N_("This message is signed with a signature, but there is no corresponding public key.") },

};

static const struct {
	const gchar *icon, *shortdesc, *description;
} smime_encrypt_table[4] = {
	{ "security-low", N_("Unencrypted"), N_("This message is not encrypted. Its content may be viewed in transit across the Internet.") },
	{ "security-high", N_("Encrypted, weak"), N_("This message is encrypted, but with a weak encryption algorithm. It would be difficult, but not impossible for an outsider to view the content of this message in a practical amount of time.") },
	{ "security-high", N_("Encrypted"), N_("This message is encrypted.  It would be difficult for an outsider to view the content of this message.") },
	{ "security-high", N_("Encrypted, strong"), N_("This message is encrypted, with a strong encryption algorithm. It would be very difficult for an outsider to view the content of this message in a practical amount of time.") },
};

static const GdkRGBA smime_sign_colour[6] = {
	{ 0.0, 0.0, 0.0, 0.0 },
	{ 0.53, 0.73, 0.53, 1.0 },
	{ 0.73, 0.53, 0.53, 1.0 },
	{ 0.91, 0.82, 0.13, 1.0 },
	{ 0.0, 0.0, 0.0, 0.0 },
	{ 0.0, 0.0, 0.0, 1.0 }
};

/* This is awkward, but there is no header file for it. On the other hand,
   the functions are meant private, where they really are, being defined this way. */
const gchar *e_mail_formatter_secure_button_get_encrypt_description (CamelCipherValidityEncrypt status);
const gchar *e_mail_formatter_secure_button_get_sign_description (CamelCipherValiditySign status);

const gchar *
e_mail_formatter_secure_button_get_sign_description (CamelCipherValiditySign status)
{
	g_return_val_if_fail (status >= 0 && status < G_N_ELEMENTS (smime_sign_table), NULL);

	return _(smime_sign_table[status].description);
}

const gchar *
e_mail_formatter_secure_button_get_encrypt_description (CamelCipherValidityEncrypt status)
{
	g_return_val_if_fail (status >= 0 && status < G_N_ELEMENTS (smime_encrypt_table), NULL);

	return _(smime_encrypt_table[status].description);
}

static void
format_cert_infos (GQueue *cert_infos,
                   GString *output_buffer)
{
	GQueue valid = G_QUEUE_INIT;
	GList *head, *link;

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
				g_string_append (output_buffer, " &lt;");
				g_string_append (output_buffer, cinfo->email);
				g_string_append (output_buffer, "&gt;");
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
add_photo_cb (gpointer data,
	      gpointer user_data)
{
	CamelCipherCertInfo *cert_info = data;
	gint width, height;
	GString *html = user_data;
	const gchar *photo_filename;
	gchar *uri;

	g_return_if_fail (cert_info != NULL);
	g_return_if_fail (html != NULL);

	photo_filename = camel_cipher_certinfo_get_property (cert_info, CAMEL_CIPHER_CERT_INFO_PROPERTY_PHOTO_FILENAME);
	if (!photo_filename || !g_file_test (photo_filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
		return;

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &width, &height)) {
		width = 32;
		height = 32;
	}

	if (width < 32)
		width = 32;
	if (height < 32)
		height = 32;

	uri = g_filename_to_uri (photo_filename, NULL, NULL);

	g_string_append_printf (html, "<img src=\"evo-%s\" width=\"%dpx\" height=\"%dpx\" style=\"vertical-align:middle; margin-right:4px;\">",
		uri, width, height);

	g_free (uri);
}

static void
secure_button_format_validity (EMailPart *part,
			       CamelCipherValidity *validity,
			       GString *html)
{
	const gchar *icon_name;
	gchar *description;
	gint icon_width, icon_height;
	GString *buffer;

	g_return_if_fail (validity != NULL);

	buffer = g_string_new ("");

	if (validity->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE) {
		const gchar *desc;
		gint status;

		status = validity->sign.status;
		desc = smime_sign_table[status].shortdesc;

		g_string_append (buffer, gettext (desc));

		format_cert_infos (&validity->sign.signers, buffer);
	}

	if (validity->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE) {
		const gchar *desc;
		gint status;

		if (validity->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE)
			g_string_append (buffer, "<br>\n");

		status = validity->encrypt.status;
		desc = smime_encrypt_table[status].shortdesc;
		g_string_append (buffer, gettext (desc));
	}

	description = g_string_free (buffer, FALSE);

	/* FIXME: need to have it based on encryption and signing too */
	if (validity->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE)
		icon_name = smime_sign_table[validity->sign.status].icon;
	else
		icon_name = smime_encrypt_table[validity->encrypt.status].icon;

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_LARGE_TOOLBAR, &icon_width, &icon_height)) {
		icon_width = 24;
		icon_height = 24;
	}

	g_string_append (html, "<table width=\"100%\" style=\"vertical-align:middle;");
	if (validity->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE &&
	    smime_sign_colour[validity->sign.status].alpha > 1e-9)
		g_string_append_printf (html, " background:#%06x;",
			e_rgba_to_value (&smime_sign_colour[validity->sign.status]));
	g_string_append (html, "\"><tr>");

	g_string_append_printf (html,
		"<td style=\"width:1px;\"><button type=\"button\" class=\"secure-button\" id=\"secure-button\" value=\"%p:%p\" accesskey=\"\" style=\"vertical-align:middle;\">"
		"<img src=\"gtk-stock://%s?size=%d\" width=\"%dpx\" height=\"%dpx\" style=\"vertical-align:middle;\"></button></td><td><span style=\"color:#%06x; vertical-align:middle;\">",
		part, validity, icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR,
		icon_width, icon_height, e_rgba_to_value (&smime_sign_colour[5]));

	g_queue_foreach (&validity->sign.signers, add_photo_cb, html);
	g_queue_foreach (&validity->encrypt.encrypters, add_photo_cb, html);

	g_string_append_printf (html, "%s</span></td></tr></table>\n", description);

	g_free (description);
}

static gboolean
emfe_secure_button_format (EMailFormatterExtension *extension,
                           EMailFormatter *formatter,
                           EMailFormatterContext *context,
                           EMailPart *part,
                           GOutputStream *stream,
                           GCancellable *cancellable)
{
	GList *head, *link;
	GString *html;

	if ((context->mode != E_MAIL_FORMATTER_MODE_NORMAL) &&
	    (context->mode != E_MAIL_FORMATTER_MODE_RAW) &&
	    (context->mode != E_MAIL_FORMATTER_MODE_ALL_HEADERS))
		return FALSE;

	html = g_string_new ("");
	head = g_queue_peek_head_link (&part->validities);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPartValidityPair *pair = link->data;

		if (!pair)
			continue;

		secure_button_format_validity (part, pair->validity, html);
	}

	g_output_stream_write_all (stream, html->str, html->len, NULL, cancellable, NULL);

	g_string_free (html, TRUE);

	return TRUE;
}

static void
e_mail_formatter_secure_button_class_init (EMailFormatterExtensionClass *class)
{
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_secure_button_format;
}

static void
e_mail_formatter_secure_button_init (EMailFormatterExtension *extension)
{
}
