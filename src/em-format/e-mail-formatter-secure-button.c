/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#if defined (ENABLE_SMIME)
#include "certificate-manager.h"
#include "e-cert-db.h"
#endif

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
	const gchar *icon, *shortdesc, *shortdesc_mismatch, *description;
} smime_sign_table[5] = {
	{ "stock_signature-bad",
	  N_("Unsigned"),
	  NULL,
	  N_("This message is not signed. There is no guarantee that this message is authentic.") },
	{ "stock_signature-ok",
	  N_("Valid signature"),
	  N_("Valid signature, but sender address and signer address do not match"),
	  N_("This message is signed and is valid meaning that it is very likely that this message is authentic.") },
	{ "stock_signature-bad",
	  N_("Invalid signature"),
	  NULL,
	  N_("The signature of this message cannot be verified, it may have been altered in transit.") },
	{ "stock_signature",
	  N_("Valid signature, but cannot verify sender"),
	  NULL,
	  N_("This message is signed with a valid signature, but the sender of the message cannot be verified.") },
	{ "stock_signature-bad",
	  N_("This message is signed, but the public key is not in your keyring"),
	  NULL,
	  N_("This message was digitally signed, but the corresponding"
		" public key is not present in your keyring. If you want to be able to verify the authenticity of messages from this person, you should"
		" obtain the public key through a trusted method and add it to your keyring. Until then, there is no guarantee that this message truly"
		" came from that person and that it arrived unaltered.") }
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

static const gchar *
secure_button_get_sign_description (CamelCipherValiditySign status)
{
	g_return_val_if_fail (status >= 0 && status < G_N_ELEMENTS (smime_sign_table), NULL);

	return _(smime_sign_table[status].description);
}

static const gchar *
secure_button_get_encrypt_description (CamelCipherValidityEncrypt status)
{
	g_return_val_if_fail (status >= 0 && ((gint) status) < G_N_ELEMENTS (smime_encrypt_table), NULL);

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

#if defined (ENABLE_SMIME)
static gboolean
secure_button_smime_cert_exists (const gchar *email,
				 ECert *ec)
{
	CERTCertificate *found_cert;
	ECert *found_ec;
	gboolean found = FALSE;

	if (!email || !*email)
		return FALSE;

	g_return_val_if_fail (E_IS_CERT (ec), FALSE);

	found_cert = CERT_FindCertByNicknameOrEmailAddr (CERT_GetDefaultCertDB (), email);
	if (!found_cert)
		return FALSE;

	found_ec = e_cert_new (found_cert);
	if (!found_ec)
		return FALSE;

	#define compare_nonnull(_func) (!_func (ec) || g_strcmp0 (_func (ec), _func (found_ec)) == 0)

	if (compare_nonnull (e_cert_get_serial_number) &&
	    compare_nonnull (e_cert_get_sha256_fingerprint) &&
	    compare_nonnull (e_cert_get_md5_fingerprint)) {
		found = TRUE;
	}

	#undef compare_nonnull

	g_object_unref (found_ec);

	return found;
}
#endif /* defined (ENABLE_SMIME) */

static void
add_cert_table (GString *html,
		const gchar *label,
		GQueue *certlist,
		guint length,
		EMailPart *part,
		CamelCipherValidity *validity)
{
	GList *link;

	if (length == 1)
		e_util_markup_append_escaped (html, "%s&nbsp;", label);
	else
		e_util_markup_append_escaped (html, "%s<br><div style=\"margin-left:12px;\">", label);

	for (link = g_queue_peek_head_link (certlist); link; link = g_list_next (link)) {
		CamelCipherCertInfo *info = link->data;
		gchar *tmp = NULL;
		const gchar *desc = NULL;

		if (info->name) {
			if (info->email && strcmp (info->name, info->email) != 0)
				desc = tmp = g_strdup_printf ("%s <%s>", info->name, info->email);
			else
				desc = info->name;
		} else {
			if (info->email)
				desc = info->email;
		}

		if (desc) {
			e_util_markup_append_escaped (html, "%s&nbsp;", desc);

#if defined (ENABLE_SMIME)
			if (info->cert_data) {
				ECert *ec;

				e_util_markup_append_escaped (html,
					"<button type=\"button\" class=\"secure-button-view-certificate\" id=\"%s\" value=\"%p:%p:%p\">%s</button>",
					e_mail_part_get_id (part), part, validity, info->cert_data, _("View Certificate"));

				ec = e_cert_new (CERT_DupCertificate (info->cert_data));

				if (validity->sign.status == CAMEL_CIPHER_VALIDITY_SIGN_NEED_PUBLIC_KEY ||
				    !secure_button_smime_cert_exists (info->email, ec)) {
					e_util_markup_append_escaped (html,
						"&nbsp;<button type=\"button\" class=\"secure-button-import-certificate\" id=\"%s.%p\" value=\"%p:%p:%p\">%s</button>",
						e_mail_part_get_id (part), info->cert_data, part, validity, info->cert_data, _("Import Certificate"));
				}

				g_clear_object (&ec);
			}
#endif

			g_string_append (html, "<br>");
		}

		g_free (tmp);
	}

	if (length != 1)
		g_string_append (html, "</div>");
}

static void
add_details_part (GString *html,
		  EMailPart *part,
		  CamelCipherValidity *validity,
		  const gchar *details,
		  const gchar *ident)
{
	gint icon_width, icon_height;

	if (!part || !validity || !details || !*details || !ident)
		return;

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, &icon_height)) {
		icon_width = 16;
		icon_height = 16;
	}

	/* no need for -evo-color-scheme-light/-evo-color-scheme-dark, because the background color is defined in the code */
	e_util_markup_append_escaped (html,
		"<span class=\"secure-button-details\" id=\"%p:spn\" value=\"secure-button-details-%p-%s\" style=\"vertical-align:bottom;\">"
		"<img id=\"secure-button-details-%p-%s-img\" style=\"vertical-align:middle;\" width=\"%dpx\" height=\"%dpx\""
		" src=\"gtk-stock://x-evolution-pan-end?size=%d\" othersrc=\"gtk-stock://x-evolution-pan-down?size=%d\">&nbsp;"
		"%s</span><br>"
		"<div id=\"secure-button-details-%p-%s\" style=\"white-space:pre; margin-left:12px; font-size:smaller;\" hidden>%s</div>",
		part, validity, ident,
		validity, ident, icon_width, icon_height, GTK_ICON_SIZE_MENU, GTK_ICON_SIZE_MENU, /* image */
		_("Details"),
		validity, ident,
		details);
}

/* Higher is worse */
static gint
secure_button_sign_severity (CamelCipherValiditySign status)
{
	switch (status) {
	case CAMEL_CIPHER_VALIDITY_SIGN_NONE:
		return 0;
	case CAMEL_CIPHER_VALIDITY_SIGN_GOOD:
		return 1;
	case CAMEL_CIPHER_VALIDITY_SIGN_NEED_PUBLIC_KEY:
		return 2;
	case CAMEL_CIPHER_VALIDITY_SIGN_UNKNOWN:
		return 3;
	case CAMEL_CIPHER_VALIDITY_SIGN_BAD:
		return 4;
	}

	return 0;
}

static gboolean
secure_button_cert_info_listed (GQueue *listed,
				CamelCipherCertInfo *info)
{
	GList *link;

	for (link = g_queue_peek_head_link (listed); link; link = g_list_next (link)) {
		CamelCipherCertInfo *known = link->data;

		if (g_strcmp0 (known->email, info->email) == 0 && g_strcmp0 (known->name, info->name) == 0)
			return TRUE;
	}

	return FALSE;
}

#define SECURE_BUTTON_CRYPTO_SYSTEMS (E_MAIL_PART_VALIDITY_PGP | E_MAIL_PART_VALIDITY_SMIME)

static gboolean
secure_button_pair_is_system (EMailPartValidityPair *pair,
			      EMailPartValidityFlags crypto_system)
{
	if (!pair)
		return FALSE;

	return (pair->validity_type & SECURE_BUTTON_CRYPTO_SYSTEMS) == crypto_system;
}

static gboolean
secure_button_format_summary (EMailPart *part,
			      EMailPartValidityFlags crypto_system,
			      const gchar *system_label,
			      GString *html)
{
	CamelCipherValidity *first_validity = NULL;
	CamelCipherValidityEncrypt encrypt_status = CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE;
	CamelCipherValiditySign sign_status = CAMEL_CIPHER_VALIDITY_SIGN_NONE;
	GQueue signers = G_QUEUE_INIT;
	GQueue *encrypters = NULL;
	GString *buffer;
	GList *link;
	const gchar *icon_name;
	gchar *description;
	gboolean sender_signer_mismatch = FALSE;
	gint icon_width, icon_height;
	gint info_index;

	for (link = g_queue_peek_head_link (&part->validities); link; link = g_list_next (link)) {
		EMailPartValidityPair *pair = link->data;
		GList *link2;

		if (!secure_button_pair_is_system (pair, crypto_system))
			continue;

		if (!first_validity)
			first_validity = pair->validity;

		if (secure_button_sign_severity (pair->validity->sign.status) > secure_button_sign_severity (sign_status))
			sign_status = pair->validity->sign.status;

		if ((pair->validity_type & E_MAIL_PART_VALIDITY_SENDER_SIGNER_MISMATCH) != 0)
			sender_signer_mismatch = TRUE;

		if (pair->validity->encrypt.status > encrypt_status) {
			encrypt_status = pair->validity->encrypt.status;
			encrypters = &pair->validity->encrypt.encrypters;
		}

		for (link2 = g_queue_peek_head_link (&pair->validity->sign.signers); link2; link2 = g_list_next (link2)) {
			CamelCipherCertInfo *info = link2->data;

			if (!secure_button_cert_info_listed (&signers, info))
				g_queue_push_tail (&signers, info);
		}
	}

	if (!first_validity)
		return FALSE;

	buffer = g_string_new ("");

	if (sign_status != CAMEL_CIPHER_VALIDITY_SIGN_NONE) {
		const gchar *desc = NULL;

		format_cert_infos (&signers, buffer);

		if (sender_signer_mismatch)
			desc = smime_sign_table[sign_status].shortdesc_mismatch;
		if (!desc)
			desc = smime_sign_table[sign_status].shortdesc;

		g_string_prepend (buffer, gettext (desc));
	}

	if (encrypt_status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE) {
		if (sign_status != CAMEL_CIPHER_VALIDITY_SIGN_NONE)
			g_string_append (buffer, "<br>\n");

		g_string_append (buffer, gettext (smime_encrypt_table[encrypt_status].shortdesc));
	}

	if (system_label && *system_label) {
		gchar *markup;

		markup = g_markup_printf_escaped ("<b>%s</b>&nbsp;&mdash; ", system_label);
		g_string_prepend (buffer, markup);

		g_free (markup);
	}

	description = g_string_free (buffer, FALSE);

	info_index = sign_status;
	if (sender_signer_mismatch && info_index == CAMEL_CIPHER_VALIDITY_SIGN_GOOD)
		info_index = CAMEL_CIPHER_VALIDITY_SIGN_UNKNOWN;

	/* FIXME: need to have it based on encryption and signing too */
	if (sign_status != CAMEL_CIPHER_VALIDITY_SIGN_NONE)
		icon_name = smime_sign_table[info_index].icon;
	else
		icon_name = smime_encrypt_table[encrypt_status].icon;

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_LARGE_TOOLBAR, &icon_width, &icon_height)) {
		icon_width = 24;
		icon_height = 24;
	}

	g_string_append (html, "<table width=\"100%\" style=\"margin-bottom:4px; vertical-align:middle;");
	if (sign_status != CAMEL_CIPHER_VALIDITY_SIGN_NONE &&
	    smime_sign_colour[info_index].alpha > 1e-9) {
		g_string_append_printf (html, " background:#%06x; color:#%06x;",
			e_rgba_to_value (&smime_sign_colour[info_index]),
			e_rgba_to_value (&smime_sign_colour[5]));
	}
	g_string_append (html, "\"><tr>");

	g_string_append_printf (html,
		"<td style=\"width:1px;\"><button type=\"button\" class=\"secure-button\" id=\"secure-button\" value=\"%p:%p\" accesskey=\"\" style=\"vertical-align:middle;\">"
		"<img src=\"gtk-stock://%s?size=%d\" width=\"%dpx\" height=\"%dpx\" style=\"vertical-align:middle;\"></button></td><td><span style=\"vertical-align:middle;\">",
		part, first_validity, icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR, icon_width, icon_height);

	g_queue_foreach (&signers, add_photo_cb, html);
	if (encrypters)
		g_queue_foreach (encrypters, add_photo_cb, html);

	g_string_append_printf (html, "%s</span></td></tr>", description);

	g_queue_clear (&signers);
	g_free (description);

	return TRUE;
}

static void
secure_button_format_encryption (EMailPart *part,
				 CamelCipherValidity *validity,
				 GString *html)
{
	guint length;

	e_util_markup_append_escaped (html, "<b>%s</b><br>%s<br>", _("Encryption"), secure_button_get_encrypt_description (validity->encrypt.status));

	length = g_queue_get_length (&validity->encrypt.encrypters);
	if (length) {
		add_cert_table (html, _("Encrypted by:"),
			&validity->encrypt.encrypters,
			length, part, validity);
	}

	add_details_part (html, part, validity, validity->encrypt.description, "encr");
}

static void
secure_button_format_details (EMailPart *part,
			      CamelCipherValidity *validity,
			      GString *html)
{
	guint length;

	g_return_if_fail (validity != NULL);

	e_util_markup_append_escaped (html,
		"<tr id=\"secure-button-details-%p\" class=\"secure-button-details\" hidden><td></td><td><small>"
		"<b>%s</b><br>"
		"%s<br>",
		validity,
		_("Digital Signature"),
		secure_button_get_sign_description (validity->sign.status));

	length = g_queue_get_length (&validity->sign.signers);
	if (length) {
		add_cert_table (html,
			g_dngettext (GETTEXT_PACKAGE, "Signer:", "Signers:", length),
			&validity->sign.signers,
			length, part, validity);
	}

	add_details_part (html, part, validity, validity->sign.description, "sign");

	g_string_append (html, "<br>");
	secure_button_format_encryption (part, validity, html);

	g_string_append (html, "</small></td></tr>\n");
}

#if defined (ENABLE_SMIME)
static gboolean
secure_button_smime_signer_listed (GQueue *signers,
				   CamelCipherCertInfo *info)
{
	GList *link;

	for (link = g_queue_peek_head_link (signers); link; link = g_list_next (link)) {
		CamelCipherCertInfo *known = link->data;

		if (known->cert_data && CERT_CompareCerts (known->cert_data, info->cert_data))
			return TRUE;
	}

	return FALSE;
}

/* Every outer signer also signed the inner layer */
static gboolean
secure_button_same_signers_smime (CamelCipherValidity *outer,
				  CamelCipherValidity *inner)
{
	GList *link;

	if (g_queue_is_empty (&outer->sign.signers))
		return FALSE;

	for (link = g_queue_peek_head_link (&outer->sign.signers); link; link = g_list_next (link)) {
		CamelCipherCertInfo *info = link->data;

		if (!info->cert_data || !secure_button_smime_signer_listed (&inner->sign.signers, info))
			return FALSE;
	}

	return TRUE;
}
#endif

static void
secure_button_format_layer (EMailPart *part,
			    EMailPartValidityPair *pair,
			    const gchar *layer_label,
			    GString *html)
{
	CamelCipherValidity *validity = pair->validity;
	const gchar *desc = NULL;
	guint length;

	if ((pair->validity_type & E_MAIL_PART_VALIDITY_SENDER_SIGNER_MISMATCH) != 0)
		desc = smime_sign_table[validity->sign.status].shortdesc_mismatch;
	if (!desc)
		desc = smime_sign_table[validity->sign.status].shortdesc;

	e_util_markup_append_escaped (html, "<div style=\"margin-left:12px; margin-top:4px;\"><b>%s</b><br>%s<br>", layer_label, gettext (desc));

	length = g_queue_get_length (&validity->sign.signers);
	if (length) {
		add_cert_table (html,
			g_dngettext (GETTEXT_PACKAGE, "Signer:", "Signers:", length),
			&validity->sign.signers,
			length, part, validity);
	}

	add_details_part (html, part, validity, validity->sign.description, "sign");

	g_string_append (html, "</div>");
}

static void
secure_button_format_layered_details (EMailPart *part,
				      EMailPartValidityPair *outer,
				      EMailPartValidityPair *inner,
				      GString *html)
{
	CamelCipherValiditySign sign_status = inner->validity->sign.status;

	if (secure_button_sign_severity (outer->validity->sign.status) > secure_button_sign_severity (sign_status))
		sign_status = outer->validity->sign.status;

	e_util_markup_append_escaped (html,
		"<tr id=\"secure-button-details-%p\" class=\"secure-button-details\" hidden><td></td><td><small>"
		"<b>%s</b><br>%s<br>",
		inner->validity, _("Digital Signature"), secure_button_get_sign_description (sign_status));

	secure_button_format_layer (part, outer, _("Outer signature (over the encrypted message)"), html);
	secure_button_format_layer (part, inner, _("Inner signature (over the message content)"), html);

	g_string_append (html, "<br>");
	secure_button_format_encryption (part, inner->validity, html);

	g_string_append (html, "</small></td></tr>\n");
}

static void
secure_button_format_system (EMailPart *part,
			     EMailPartValidityFlags crypto_system,
			     const gchar *system_label,
			     GString *html)
{
	EMailPartValidityPair *outer = NULL, *inner = NULL;
	GList *head, *link;
	gboolean merge_layers = FALSE;

	head = g_queue_peek_head_link (&part->validities);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPartValidityPair *pair = link->data;

		if (!secure_button_pair_is_system (pair, crypto_system))
			continue;

		if ((pair->validity_type & E_MAIL_PART_VALIDITY_OUTER) != 0) {
			if (!outer)
				outer = pair;
		} else if (!inner) {
			inner = pair;
		}
	}

	if (!secure_button_format_summary (part, crypto_system, system_label, html))
		return;

#if defined (ENABLE_SMIME)
	merge_layers = outer && inner && crypto_system == E_MAIL_PART_VALIDITY_SMIME &&
		outer->validity->sign.status == CAMEL_CIPHER_VALIDITY_SIGN_GOOD &&
		inner->validity->sign.status == CAMEL_CIPHER_VALIDITY_SIGN_GOOD &&
		secure_button_same_signers_smime (outer->validity, inner->validity);
#endif

	if (outer && inner && !merge_layers) {
		secure_button_format_layered_details (part, outer, inner, html);
	} else {
		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPartValidityPair *pair = link->data;

			if (!secure_button_pair_is_system (pair, crypto_system) || (pair == outer && inner))
				continue;

			secure_button_format_details (part, pair->validity, html);
		}
	}

	g_string_append (html, "</table>\n");
}

static gboolean
emfe_secure_button_format (EMailFormatterExtension *extension,
                           EMailFormatter *formatter,
                           EMailFormatterContext *context,
                           EMailPart *part,
                           GOutputStream *stream,
                           GCancellable *cancellable)
{
	/* PGP, S/MIME and none */
	EMailPartValidityFlags crypto_systems[3];
	GList *head, *link;
	GString *html;
	guint n_crypto_systems = 0, ii;

	if ((context->mode != E_MAIL_FORMATTER_MODE_NORMAL) &&
	    (context->mode != E_MAIL_FORMATTER_MODE_RAW) &&
	    (context->mode != E_MAIL_FORMATTER_MODE_ALL_HEADERS))
		return FALSE;

	head = g_queue_peek_head_link (&part->validities);

	/* One bar per crypto system */
	for (link = head; link != NULL && n_crypto_systems < G_N_ELEMENTS (crypto_systems); link = g_list_next (link)) {
		EMailPartValidityPair *pair = link->data;
		EMailPartValidityFlags crypto_system;
		gboolean known = FALSE;

		if (!pair)
			continue;

		crypto_system = pair->validity_type & SECURE_BUTTON_CRYPTO_SYSTEMS;

		for (ii = 0; ii < n_crypto_systems && !known; ii++) {
			known = crypto_systems[ii] == crypto_system;
		}

		if (!known)
			crypto_systems[n_crypto_systems++] = crypto_system;
	}

	html = g_string_new ("");

	for (ii = 0; ii < n_crypto_systems; ii++) {
		const gchar *system_label = NULL;

		if (n_crypto_systems > 1) {
			if (crypto_systems[ii] == E_MAIL_PART_VALIDITY_PGP)
				system_label = _("GPG");
			else if (crypto_systems[ii] == E_MAIL_PART_VALIDITY_SMIME)
				system_label = _("S/MIME");
		}

		secure_button_format_system (part, crypto_systems[ii], system_label, html);
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
