/*
 * e-mail-part.c
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

/**
 * EMailPart:
 *
 * The #EMailPart is a wrapper around #CamelMimePart which holds additional
 * information about the mime part, like it's ID, encryption type etc.
 *
 * Each #EMailPart must have a unique ID. The ID is a dot-separated
 * hierarchical description of the location of the part within the email
 * message.
 */

#include "evolution-config.h"

#include "e-mail-part.h"

#include <string.h>

#include "e-mail-part-attachment.h"
#include "e-mail-part-list.h"

struct _EMailPartPrivate {
	GWeakRef part_list;
	CamelMimePart *mime_part;

	gchar *id;
	gchar *cid;
	gchar *mime_type;

	gboolean is_attachment;
	gboolean is_printable;
	gboolean converted_to_utf8;
};

enum {
	PROP_0,
	PROP_CID,
	PROP_CONVERTED_TO_UTF8,
	PROP_ID,
	PROP_IS_ATTACHMENT,
	PROP_IS_PRINTABLE,
	PROP_MIME_PART,
	PROP_MIME_TYPE,
	PROP_PART_LIST
};

G_DEFINE_TYPE_WITH_CODE (EMailPart, e_mail_part, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EMailPart)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
mail_part_validity_pair_free (gpointer ptr)
{
	EMailPartValidityPair *pair = ptr;

	if (!pair)
		return;

	camel_cipher_validity_free (pair->validity);
	g_free (pair);
}

static void
mail_part_set_id (EMailPart *part,
                  const gchar *id)
{
	g_return_if_fail (part->priv->id == NULL);

	part->priv->id = g_strdup (id);
}

static void
mail_part_set_mime_part (EMailPart *part,
                         CamelMimePart *mime_part)
{
	g_return_if_fail (part->priv->mime_part == NULL);

	/* The CamelMimePart is optional. */
	if (mime_part != NULL)
		part->priv->mime_part = g_object_ref (mime_part);
}

static void
mail_part_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CID:
			e_mail_part_set_cid (
				E_MAIL_PART (object),
				g_value_get_string (value));
			return;

		case PROP_CONVERTED_TO_UTF8:
			e_mail_part_set_converted_to_utf8 (
				E_MAIL_PART (object),
				g_value_get_boolean (value));
			return;

		case PROP_ID:
			mail_part_set_id (
				E_MAIL_PART (object),
				g_value_get_string (value));
			return;

		case PROP_IS_ATTACHMENT:
			e_mail_part_set_is_attachment (
				E_MAIL_PART (object),
				g_value_get_boolean (value));
			return;

		case PROP_IS_PRINTABLE:
			e_mail_part_set_is_printable (
				E_MAIL_PART (object),
				g_value_get_boolean (value));
			return;

		case PROP_MIME_PART:
			mail_part_set_mime_part (
				E_MAIL_PART (object),
				g_value_get_object (value));
			return;

		case PROP_MIME_TYPE:
			e_mail_part_set_mime_type (
				E_MAIL_PART (object),
				g_value_get_string (value));
			return;

		case PROP_PART_LIST:
			e_mail_part_set_part_list (
				E_MAIL_PART (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_part_get_property (GObject *object,
                        guint property_id,
                        GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CID:
			g_value_set_string (
				value,
				e_mail_part_get_cid (
				E_MAIL_PART (object)));
			return;

		case PROP_CONVERTED_TO_UTF8:
			g_value_set_boolean (
				value,
				e_mail_part_get_converted_to_utf8 (
				E_MAIL_PART (object)));
			return;

		case PROP_ID:
			g_value_set_string (
				value,
				e_mail_part_get_id (
				E_MAIL_PART (object)));
			return;

		case PROP_IS_ATTACHMENT:
			g_value_set_boolean (
				value,
				e_mail_part_get_is_attachment (
				E_MAIL_PART (object)));
			return;

		case PROP_IS_PRINTABLE:
			g_value_set_boolean (
				value,
				e_mail_part_get_is_printable (
				E_MAIL_PART (object)));
			return;

		case PROP_MIME_PART:
			g_value_take_object (
				value,
				e_mail_part_ref_mime_part (
				E_MAIL_PART (object)));
			return;

		case PROP_MIME_TYPE:
			g_value_set_string (
				value,
				e_mail_part_get_mime_type (
				E_MAIL_PART (object)));
			return;

		case PROP_PART_LIST:
			g_value_take_object (
				value,
				e_mail_part_ref_part_list (
				E_MAIL_PART (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_part_dispose (GObject *object)
{
	EMailPart *self = E_MAIL_PART (object);

	g_weak_ref_set (&self->priv->part_list, NULL);

	g_clear_object (&self->priv->mime_part);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_part_parent_class)->dispose (object);
}

static void
mail_part_finalize (GObject *object)
{
	EMailPart *part = E_MAIL_PART (object);
	EMailPartValidityPair *pair;

	g_free (part->priv->id);
	g_free (part->priv->cid);
	g_free (part->priv->mime_type);

	while ((pair = g_queue_pop_head (&part->validities)) != NULL)
		mail_part_validity_pair_free (pair);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_part_parent_class)->finalize (object);
}

static void
mail_part_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_part_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
e_mail_part_class_init (EMailPartClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_part_set_property;
	object_class->get_property = mail_part_get_property;
	object_class->dispose = mail_part_dispose;
	object_class->finalize = mail_part_finalize;
	object_class->constructed = mail_part_constructed;

	g_object_class_install_property (
		object_class,
		PROP_CID,
		g_param_spec_string (
			"cid",
			"Content ID",
			"The MIME Content-ID",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_CONVERTED_TO_UTF8,
		g_param_spec_boolean (
			"converted-to-utf8",
			"Converted To UTF8",
			"Whether the part content was already converted to UTF-8",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_ID,
		g_param_spec_string (
			"id",
			"Part ID",
			"The part ID",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_IS_ATTACHMENT,
		g_param_spec_boolean (
			"is-attachment",
			"Is Attachment",
			"Format the part as an attachment",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_IS_PRINTABLE,
		g_param_spec_boolean (
			"is-printable",
			"Is Printable",
			"Whether this part can be printed",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MIME_PART,
		g_param_spec_object (
			"mime-part",
			"MIME Part",
			"The MIME part",
			CAMEL_TYPE_MIME_PART,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MIME_TYPE,
		g_param_spec_string (
			"mime-type",
			"MIME Type",
			"The MIME type",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_PART_LIST,
		g_param_spec_object (
			"part-list",
			"Part List",
			"The part list that owns the part",
			E_TYPE_MAIL_PART_LIST,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_part_init (EMailPart *part)
{
	part->priv = e_mail_part_get_instance_private (part);
}

/**
 * e_mail_part_new:
 * @mime_part: (allow-none) a #CamelMimePart or %NULL
 * @id: part ID
 *
 * Creates a new #EMailPart for the given @mime_part.
 *
 * Return value: a new #EMailPart
 */
EMailPart *
e_mail_part_new (CamelMimePart *mime_part,
                 const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);

	return g_object_new (
		E_TYPE_MAIL_PART,
		"id", id, "mime-part", mime_part, NULL);
}

const gchar *
e_mail_part_get_id (EMailPart *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART (part), NULL);

	return part->priv->id;
}

const gchar *
e_mail_part_get_cid (EMailPart *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART (part), NULL);

	return part->priv->cid;
}

void
e_mail_part_set_cid (EMailPart *part,
                     const gchar *cid)
{
	g_return_if_fail (E_IS_MAIL_PART (part));

	g_free (part->priv->cid);
	part->priv->cid = g_strdup (cid);

	g_object_notify (G_OBJECT (part), "cid");
}

gboolean
e_mail_part_id_has_prefix (EMailPart *part,
                           const gchar *prefix)
{
	g_return_val_if_fail (E_IS_MAIL_PART (part), FALSE);
	g_return_val_if_fail (prefix != NULL, FALSE);

	return part->priv->id && g_str_has_prefix (part->priv->id, prefix);
}

gboolean
e_mail_part_id_has_suffix (EMailPart *part,
                           const gchar *suffix)
{
	g_return_val_if_fail (E_IS_MAIL_PART (part), FALSE);
	g_return_val_if_fail (suffix != NULL, FALSE);

	return part->priv->id && g_str_has_suffix (part->priv->id, suffix);
}

gboolean
e_mail_part_id_has_substr (EMailPart *part,
                           const gchar *substr)
{
	g_return_val_if_fail (E_IS_MAIL_PART (part), FALSE);
	g_return_val_if_fail (substr != NULL, FALSE);

	return part->priv->id && strstr (part->priv->id, substr) != NULL;
}

CamelMimePart *
e_mail_part_ref_mime_part (EMailPart *part)
{
	CamelMimePart *mime_part = NULL;

	g_return_val_if_fail (E_IS_MAIL_PART (part), NULL);

	if (part->priv->mime_part != NULL)
		mime_part = g_object_ref (part->priv->mime_part);

	return mime_part;
}

const gchar *
e_mail_part_get_mime_type (EMailPart *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART (part), NULL);

	return part->priv->mime_type;
}

void
e_mail_part_set_mime_type (EMailPart *part,
                           const gchar *mime_type)
{
	g_return_if_fail (E_IS_MAIL_PART (part));

	if (g_strcmp0 (mime_type, part->priv->mime_type) == 0)
		return;

	g_free (part->priv->mime_type);
	part->priv->mime_type = g_strdup (mime_type);

	g_object_notify (G_OBJECT (part), "mime-type");
}

gboolean
e_mail_part_get_converted_to_utf8 (EMailPart *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART (part), FALSE);

	return part->priv->converted_to_utf8;
}

void
e_mail_part_set_converted_to_utf8 (EMailPart *part,
				   gboolean converted_to_utf8)
{
	g_return_if_fail (E_IS_MAIL_PART (part));

	if (converted_to_utf8 == part->priv->converted_to_utf8)
		return;

	part->priv->converted_to_utf8 = converted_to_utf8;

	g_object_notify (G_OBJECT (part), "converted-to-utf8");
}

gboolean
e_mail_part_should_show_inline (EMailPart *part)
{
	CamelMimePart *mime_part;
	const CamelContentDisposition *disposition;
	gboolean res = FALSE;

	g_return_val_if_fail (E_IS_MAIL_PART (part), FALSE);

	/* Automatically expand attachments that have inline
	 * disposition or the EMailParts have specific
	 * force_inline flag set. */

	if (part->force_collapse)
		return FALSE;

	if (part->force_inline)
		return TRUE;

	if (E_IS_MAIL_PART_ATTACHMENT (part)) {
		EMailPartAttachment *empa = E_MAIL_PART_ATTACHMENT (part);
		const gchar *guessed_mime_type = e_mail_part_attachment_get_guessed_mime_type (empa);

		if (guessed_mime_type && g_ascii_strcasecmp (guessed_mime_type, "message/rfc822") == 0)
			return TRUE;
	}

	mime_part = e_mail_part_ref_mime_part (part);
	if (!mime_part)
		return FALSE;

	disposition = camel_mime_part_get_content_disposition (mime_part);
	if (disposition && disposition->disposition &&
	    g_ascii_strncasecmp (disposition->disposition, "inline", 6) == 0) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		res = g_settings_get_boolean (settings, "display-content-disposition-inline");
		g_clear_object (&settings);
	}

	g_object_unref (mime_part);

	return res;
}

EMailPartList *
e_mail_part_ref_part_list (EMailPart *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART (part), NULL);

	return g_weak_ref_get (&part->priv->part_list);
}

void
e_mail_part_set_part_list (EMailPart *part,
                           EMailPartList *part_list)
{
	g_return_if_fail (E_IS_MAIL_PART (part));

	if (part_list != NULL)
		g_return_if_fail (E_IS_MAIL_PART_LIST (part_list));

	g_weak_ref_set (&part->priv->part_list, part_list);

	g_object_notify (G_OBJECT (part), "part-list");
}

gboolean
e_mail_part_get_is_attachment (EMailPart *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART (part), FALSE);

	return part->priv->is_attachment;
}

void
e_mail_part_set_is_attachment (EMailPart *part,
                               gboolean is_attachment)
{
	g_return_if_fail (E_IS_MAIL_PART (part));

	if (is_attachment == part->priv->is_attachment)
		return;

	part->priv->is_attachment = is_attachment;

	g_object_notify (G_OBJECT (part), "is-attachment");
}

gboolean
e_mail_part_get_is_printable (EMailPart *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART (part), FALSE);

	return part->priv->is_printable;
}

void
e_mail_part_set_is_printable (EMailPart *part,
			      gboolean is_printable)
{
	g_return_if_fail (E_IS_MAIL_PART (part));

	if ((is_printable ? 1 : 0) == (part->priv->is_printable ? 1 : 0))
		return;

	part->priv->is_printable = is_printable;

	g_object_notify (G_OBJECT (part), "is-printable");
}

void
e_mail_part_content_loaded (EMailPart *part,
			    EWebView *web_view,
			    const gchar *iframe_id)
{
	EMailPartClass *class;

	g_return_if_fail (E_IS_MAIL_PART (part));
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	class = E_MAIL_PART_GET_CLASS (part);
	g_return_if_fail (class != NULL);

	if (class->content_loaded)
		class->content_loaded (part, web_view, iframe_id);
}

static EMailPartValidityPair *
mail_part_find_validity_pair (EMailPart *part,
                              EMailPartValidityFlags validity_type)
{
	GList *head, *link;

	head = g_queue_peek_head_link (&part->validities);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPartValidityPair *pair = link->data;

		if (pair == NULL)
			continue;

		if ((pair->validity_type & validity_type) == validity_type)
			return pair;
	}

	return NULL;
}

/**
 * e_mail_part_update_validity:
 * @part: An #EMailPart
 * @validity: a #CamelCipherValidity
 * @validity_type: E_MAIL_PART_VALIDITY_* flags
 *
 * Updates validity of the @part. When the part already has some validity
 * set, the new @validity and @validity_type are just appended, preserving
 * the original validity. Validities of the same type (PGP or S/MIME) are
 * merged together.
 */
void
e_mail_part_update_validity (EMailPart *part,
                             CamelCipherValidity *validity,
                             EMailPartValidityFlags validity_type)
{
	EMailPartValidityPair *pair;
	EMailPartValidityFlags mask;

	g_return_if_fail (E_IS_MAIL_PART (part));
	g_return_if_fail (validity != NULL);

	mask = E_MAIL_PART_VALIDITY_PGP | E_MAIL_PART_VALIDITY_SMIME;

	/* Auto-add flags when the related part is present */
	if (!(validity_type & E_MAIL_PART_VALIDITY_SIGNED) &&
	    validity->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE)
		validity_type |= E_MAIL_PART_VALIDITY_SIGNED;

	if (!(validity_type & E_MAIL_PART_VALIDITY_ENCRYPTED) &&
	    validity->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE)
		validity_type |= E_MAIL_PART_VALIDITY_ENCRYPTED;

	pair = mail_part_find_validity_pair (part, validity_type & mask);
	if (pair != NULL) {
		pair->validity_type |= validity_type;
		camel_cipher_validity_envelope (pair->validity, validity);
	} else {
		pair = g_new0 (EMailPartValidityPair, 1);
		pair->validity_type = validity_type;
		pair->validity = camel_cipher_validity_clone (validity);

		g_queue_push_tail (&part->validities, pair);
	}
}

/**
 * e_mail_part_get_validity:
 * @part: An #EMailPart
 * @validity_type: E_MAIL_PART_VALIDITY_* flags
 *
 * Returns, validity of @part contains any validity with the same bits
 * as @validity_type set. It should contain all bits of it.
 *
 * Returns: a #CamelCipherValidity of the given type, %NULL if not found
 *
 * Since: 3.8
 */
CamelCipherValidity *
e_mail_part_get_validity (EMailPart *part,
                          EMailPartValidityFlags validity_type)
{
	EMailPartValidityPair *pair;

	g_return_val_if_fail (E_IS_MAIL_PART (part), NULL);

	pair = mail_part_find_validity_pair (part, validity_type);

	return (pair != NULL) ? pair->validity : NULL;
}

gboolean
e_mail_part_has_validity (EMailPart *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART (part), FALSE);

	return !g_queue_is_empty (&part->validities);
}

EMailPartValidityFlags
e_mail_part_get_validity_flags (EMailPart *part)
{
	EMailPartValidityFlags flags = 0;
	GList *head, *link;

	g_return_val_if_fail (E_IS_MAIL_PART (part), 0);

	head = g_queue_peek_head_link (&part->validities);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPartValidityPair *pair = link->data;

		if (pair != NULL)
			flags |= pair->validity_type;
	}

	return flags;
}

static gboolean
from_matches_signers_alt_emails (CamelInternetAddress *from_address,
				 CamelCipherCertInfo *cinfo)
{
	GSList *props_link;
	gboolean matches = FALSE;

	for (props_link = cinfo->properties; props_link && !matches; props_link = g_slist_next (props_link)) {
		const CamelCipherCertInfoProperty *prop = props_link->data;

		if (prop && g_strcmp0 (prop->name, CAMEL_CIPHER_CERT_INFO_PROPERTY_SIGNERS_ALT_EMAILS) == 0 && prop->value) {
			CamelInternetAddress *address;
			gint count, ii;

			address = camel_internet_address_new ();
			count = camel_address_unformat (CAMEL_ADDRESS (address), prop->value);
			for (ii = 0; ii < count && !matches; ii++) {
				const gchar *email = NULL;

				if (camel_internet_address_get (address, ii, NULL, &email) && email && *email) {
					matches = camel_internet_address_find_address (from_address, email, NULL) >= 0;
				}
			}
			g_object_unref (address);
			break;
		}
	}

	return matches;
}

void
e_mail_part_verify_validity_sender (EMailPart *part,
				    CamelInternetAddress *from_address)
{
	GList *link;

	g_return_if_fail (E_IS_MAIL_PART (part));

	if (!from_address)
		return;

	for (link = g_queue_peek_head_link (&part->validities); link; link = g_list_next (link)) {
		EMailPartValidityPair *pair = link->data;

		if (pair && pair->validity && !(pair->validity_type & E_MAIL_PART_VALIDITY_VERIFIED)) {
			pair->validity_type |= E_MAIL_PART_VALIDITY_VERIFIED;

			if (pair->validity->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE) {
				GList *link2;
				gboolean from_matches_signer = FALSE;

				for (link2 = g_queue_peek_head_link (&pair->validity->sign.signers); link2 && !from_matches_signer; link2 = g_list_next (link2)) {
					CamelCipherCertInfo *cinfo = link2->data;

					if (cinfo->email && *cinfo->email) {
						from_matches_signer = from_matches_signer ||
							(from_address && camel_internet_address_find_address (from_address, cinfo->email, NULL) >= 0) ||
							(from_address && from_matches_signers_alt_emails (from_address, cinfo));
					}
				}

				if (!from_matches_signer)
					pair->validity_type |= E_MAIL_PART_VALIDITY_SENDER_SIGNER_MISMATCH;
			}
		}
	}
}
