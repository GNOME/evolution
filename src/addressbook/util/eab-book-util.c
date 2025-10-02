/*
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
 *
 * Authors:
 *		Jon Trowbridge <trow@ximian.com>
 *      Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n.h>
#include <string.h>

#include "e-util/e-util.h"
#include "eab-book-util.h"

static EABTypeLabel
email_types[] =
{
	{ -1, "WORK",  NULL, NC_ ("addressbook-label", "Work Email")  },
	{ -1, "HOME",  NULL, NC_ ("addressbook-label", "Home Email")  },
	{ -1, "OTHER", NULL, NC_ ("addressbook-label", "Other Email") }
};

static EABTypeLabel
sip_types[] =
{
	{ E_CONTACT_SIP, "WORK",  NULL, NC_ ("addressbook-label", "Work SIP")  },
	{ E_CONTACT_SIP, "HOME",  NULL, NC_ ("addressbook-label", "Home SIP")  },
	{ E_CONTACT_SIP, "OTHER", NULL, NC_ ("addressbook-label", "Other SIP") }
};

static EABTypeLabel
eab_phone_types[] = {
	{ E_CONTACT_PHONE_ASSISTANT,    EVC_X_ASSISTANT,       NULL,    NULL   },
	{ E_CONTACT_PHONE_BUSINESS,     "WORK",                "VOICE", NULL   },
	{ E_CONTACT_PHONE_BUSINESS_FAX, "WORK",                "FAX",   NULL   },
	{ E_CONTACT_PHONE_CALLBACK,     EVC_X_CALLBACK,        NULL,    NULL   },
	{ E_CONTACT_PHONE_CAR,          "CAR",                 NULL,    NULL   },
	{ E_CONTACT_PHONE_COMPANY,      "X-EVOLUTION-COMPANY", NULL,    NULL   },
	{ E_CONTACT_PHONE_HOME,         "HOME",                "VOICE", NULL   },
	{ E_CONTACT_PHONE_HOME_FAX,     "HOME",                "FAX",   NULL   },
	{ E_CONTACT_PHONE_ISDN,         "ISDN",                NULL,    NULL   },
	{ E_CONTACT_PHONE_MOBILE,       "CELL",                NULL,    NULL   },
	{ E_CONTACT_PHONE_OTHER,        "VOICE",               NULL,    NULL   },
	{ E_CONTACT_PHONE_OTHER_FAX,    "FAX",                 NULL,    NULL   },
	{ E_CONTACT_PHONE_PAGER,        "PAGER",               NULL,    NULL   },
	{ E_CONTACT_PHONE_PRIMARY,      "PREF",                NULL,    NULL   },
	{ E_CONTACT_PHONE_RADIO,        EVC_X_RADIO,           NULL,    NULL   },
	{ E_CONTACT_PHONE_TELEX,        EVC_X_TELEX,           NULL,    NULL   },
	{ E_CONTACT_PHONE_TTYTDD,       EVC_X_TTYTDD,          NULL,    NULL   }
};
static gboolean eab_phone_types_init = TRUE;

static EABTypeLabel
eab_im_service[] =
{
	{ E_CONTACT_IM_AIM,         NULL, NULL, NC_ ("addressbook-label", "AIM")       },
	{ E_CONTACT_IM_JABBER,      NULL, NULL, NC_ ("addressbook-label", "Jabber")    },
	{ E_CONTACT_IM_YAHOO,       NULL, NULL, NC_ ("addressbook-label", "Yahoo")     },
	{ E_CONTACT_IM_GADUGADU,    NULL, NULL, NC_ ("addressbook-label", "Gadu-Gadu") },
	{ E_CONTACT_IM_MSN,         NULL, NULL, NC_ ("addressbook-label", "MSN")       },
	{ E_CONTACT_IM_ICQ,         NULL, NULL, NC_ ("addressbook-label", "ICQ")       },
	{ E_CONTACT_IM_GROUPWISE,   NULL, NULL, NC_ ("addressbook-label", "GroupWise") },
	{ E_CONTACT_IM_SKYPE,       NULL, NULL, NC_ ("addressbook-label", "Skype")     },
	{ E_CONTACT_IM_TWITTER,     NULL, NULL, NC_ ("addressbook-label", "Twitter")   },
	{ E_CONTACT_IM_GOOGLE_TALK, NULL, NULL, NC_ ("addressbook-label", "Google Talk")},
	{ E_CONTACT_IM_MATRIX,      NULL, NULL, NC_ ("addressbook-label", "Matrix")    }
};

const EABTypeLabel*
eab_get_email_type_labels (gint *n_elements)
{
	*n_elements = G_N_ELEMENTS (email_types);
	return email_types;
}

gint
eab_get_email_type_index (EVCardAttribute *attr)
{
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (email_types); ii++) {
		if (e_vcard_attribute_has_type (attr, email_types[ii].type_1))
			return ii;
	}

	return -1;
}

void
eab_email_index_to_type (gint index, const gchar **type_1)
{
	*type_1 = email_types[index].type_1;
}

const gchar*
eab_get_email_label_text (EVCardAttribute *attr)
{
	const gchar *result;
	gint n_elements;
	gint index = eab_get_email_type_index (attr);

	if (index >= 0) {
		result = _(eab_get_email_type_labels (&n_elements) [index].text);
	} else {
		/* To Translators:
		 * if an email address type is not one of the predefined types,
		 * this generic label is used instead of one of the predefined labels.
		 */
		result = C_("addressbook-label", "Email");
	}

	return result;
}

const EABTypeLabel*
eab_get_sip_type_labels (gint *n_elements)
{
	*n_elements = G_N_ELEMENTS (sip_types);
	return sip_types;
}

gint
eab_get_sip_type_index (EVCardAttribute *attr)
{
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (sip_types); ii++) {
		if (e_vcard_attribute_has_type (attr, sip_types[ii].type_1))
			return ii;
	}

	return -1;
}

void
eab_sip_index_to_type (gint index, const gchar **type_1)
{
	*type_1 = sip_types[index].type_1;
}

const gchar*
eab_get_sip_label_text (EVCardAttribute *attr)
{
	const gchar *result;
	gint n_elements;
	gint index = eab_get_sip_type_index (attr);

	if (index >= 0) {
		result = _(eab_get_sip_type_labels (&n_elements) [index].text);
	} else {
		/* To Translators:
		 * if a SIP address type is not one of the predefined types,
		 * this generic label is used instead of one of the predefined labels.
		 * SIP=Session Initiation Protocol, used for voice over IP
		 */
		result = C_("addressbook-label", "SIP");
	}

	return result;
}

const EABTypeLabel*
eab_get_im_type_labels (gint *n_elements)
{
	*n_elements = G_N_ELEMENTS (eab_im_service);
	return eab_im_service;
}

gint
eab_get_im_type_index (EVCardAttribute *attr)
{
	gint ii;
	const gchar *name;
	EContactField field;

	for (ii = 0; ii < G_N_ELEMENTS (eab_im_service); ii++) {
		name = e_vcard_attribute_get_name (attr);
		field = e_contact_field_id_from_vcard (name);
		if (field == eab_im_service[ii].field_id)
			return ii;
		if (g_ascii_strcasecmp (name, EVC_IMPP) == 0 ||
		    g_ascii_strcasecmp (name, EVC_X_EVOLUTION_IMPP) == 0) {
			GList *values;

			values = e_vcard_attribute_get_values (attr);
			if (values && values->data) {
				field = e_contact_impp_scheme_to_field (values->data, NULL);
				if (field == eab_im_service[ii].field_id)
					return ii;
			}
		}
	}
	return -1;
}

/* Translators: if an IM address type is not one of the predefined types,
   this generic label is used instead of one of the predefined labels.
   IM=Instant Messaging
 */
#define FALLBACK_IM_LABEL C_("addressbook-label", "IM")

const gchar *
eab_get_im_label_text (EVCardAttribute *attr)
{
	const gchar *result;
	gint index = eab_get_im_type_index (attr);

	if (index >= 0) {
		result = _(eab_im_service [index].text);
	} else {
		result = FALLBACK_IM_LABEL;
	}

	return result;
}

const gchar *
eab_get_impp_label_text (const gchar *impp_value,
			 EContactField *out_equiv_field,
			 guint *out_scheme_len)
{
	EContactField field_id;
	guint ii;

	if (impp_value) {
		field_id = e_contact_impp_scheme_to_field (impp_value, out_scheme_len);

		if (out_equiv_field)
			*out_equiv_field = field_id;

		if (field_id != E_CONTACT_FIELD_LAST) {
			for (ii = 0; ii < G_N_ELEMENTS (eab_im_service); ii++) {
				if (eab_im_service[ii].field_id == field_id)
					return _(eab_im_service[ii].text);
			}
		}
	}

	if (out_equiv_field)
		*out_equiv_field = E_CONTACT_FIELD_LAST;

	return FALLBACK_IM_LABEL;
}

const EABTypeLabel*
eab_get_phone_type_labels (gint *n_elements)
{
	*n_elements = G_N_ELEMENTS (eab_phone_types);

	if (eab_phone_types_init) {
		gint i;
		eab_phone_types_init = FALSE;
		for (i = 0; i < *n_elements; i++) {
			eab_phone_types[i].text = e_contact_pretty_name (eab_phone_types[i].field_id);
		}
	}

	return eab_phone_types;
}

/*
 * return the index within eab_phone_types[]
 */
gint
eab_get_phone_type_index (EVCardAttribute *attr)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (eab_phone_types); i++) {
		if (e_vcard_attribute_has_type (attr, eab_phone_types[i].type_1) &&
		    (eab_phone_types[i].type_2 == NULL || e_vcard_attribute_has_type (attr, eab_phone_types[i].type_2) ||
		    (g_ascii_strcasecmp (eab_phone_types[i].type_2, "VOICE") == 0 &&
		     g_list_length (e_vcard_attribute_get_param (attr, EVC_TYPE)) == 1)))
			return i;
	}

	return -1;
}

const gchar*
eab_get_phone_label_text (EVCardAttribute *attr)
{
	const gchar *result;
	gint n_elements;
	gint index = eab_get_phone_type_index (attr);

	if (index >= 0) {
		result = _(eab_get_phone_type_labels (&n_elements) [index].text);
	} else {
		/* To Translators:
		 * if a phone number type is not one of the predefined types,
		 * this generic label is used instead of one of the predefined labels.
		 */
		result = C_("addressbook-label", "Phone");
	}

	return result;
}

void
eab_phone_index_to_type (gint index,
                         const gchar **type_1,
                         const gchar **type_2)
{
	*type_1 = eab_phone_types [index].type_1;
	*type_2 = eab_phone_types [index].type_2;
}

/* Copied from camel_strstrcase */
static gchar *
eab_strstrcase (const gchar *haystack,
                const gchar *needle)
{
	/* find the needle in the haystack neglecting case */
	const gchar *ptr;
	guint len;

	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);

	len = strlen (needle);
	if (len > strlen (haystack))
		return NULL;

	if (len == 0)
		return (gchar *) haystack;

	for (ptr = haystack; *(ptr + len - 1) != '\0'; ptr++)
		if (!g_ascii_strncasecmp (ptr, needle, len))
			return (gchar *) ptr;

	return NULL;
}

GSList *
eab_contact_list_from_string (const gchar *str)
{
	GSList *contacts = NULL;
	GString *gstr = g_string_new (NULL);
	gchar *str_stripped;
	gchar *p = (gchar *) str;
	gchar *q;

	if (!p)
		return NULL;

	if (!strncmp (p, "Book: ", 6)) {
		p = strchr (p, '\n');
		if (!p) {
			g_warning (G_STRLOC ": Got book but no newline!");
			return NULL;
		}
		p++;
	}

	while (*p) {
		if (*p != '\r') g_string_append_c (gstr, *p);

		p++;
	}

	p = str_stripped = g_string_free (gstr, FALSE);

	/* Note: The vCard standard says
	 *
	 * vcard = "BEGIN" [ws] ":" [ws] "VCARD" [ws] 1*CRLF
	 *         items *CRLF "END" [ws] ":" [ws] "VCARD"
	 *
	 * which means we can have whitespace (e.g. "BEGIN : VCARD"). So we're not being
	 * fully compliant here, although I'm not sure it matters. The ideal solution
	 * would be to have a vcard parsing function that returned the end of the vcard
	 * parsed. Arguably, contact list parsing should all be in libebook's e-vcard.c,
	 * where we can do proper parsing and validation without code duplication. */

	for (p = eab_strstrcase (p, "BEGIN:VCARD"); p; p = eab_strstrcase (q, "\nBEGIN:VCARD")) {
		gchar *card_str;

		if (*p == '\n')
			p++;

		for (q = eab_strstrcase (p, "END:VCARD"); q; q = eab_strstrcase (q, "END:VCARD")) {
			gchar *temp;

			q += 9;
			temp = q;
			if (*temp)
				temp += strspn (temp, "\r\n\t ");

			if (*temp == '\0' || !g_ascii_strncasecmp (temp, "BEGIN:VCARD", 11))
				break;  /* Found the outer END:VCARD */
		}

		if (!q)
			break;

		card_str = g_strndup (p, q - p);
		contacts = g_slist_prepend (contacts, e_contact_new_from_vcard (card_str));
		g_free (card_str);
	}

	g_free (str_stripped);

	return g_slist_reverse (contacts);
}

static void
eab_add_contact_to_string (GString *str,
			   EContact *contact)
{
	gchar *vcard_str;

	e_contact_inline_local_photos (contact, NULL);
	vcard_str = e_vcard_to_string (E_VCARD (contact));

	if (str->len)
		g_string_append (str, "\r\n\r\n");

	g_string_append (str, vcard_str);

	g_free (vcard_str);
}

gchar *
eab_contact_list_to_string (const GSList *contacts)
{
	GString *str = g_string_new ("");
	const GSList *l;

	for (l = contacts; l; l = l->next) {
		EContact *contact = l->data;

		eab_add_contact_to_string (str, contact);
	}

	return g_string_free (str, FALSE);
}

gchar *
eab_contact_array_to_string (const GPtrArray *contacts)
{
	GString *str = g_string_new ("");
	guint ii;

	for (ii = 0; contacts && ii < contacts->len; ii++) {
		EContact *contact = g_ptr_array_index (contacts, ii);

		eab_add_contact_to_string (str, contact);
	}

	return g_string_free (str, FALSE);
}

gboolean
eab_source_and_contact_list_from_string (ESourceRegistry *registry,
                                         const gchar *str,
                                         ESource **out_source,
                                         GSList **out_contacts)
{
	ESource *source;
	const gchar *s0, *s1;
	gchar *uid;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);
	g_return_val_if_fail (str != NULL, FALSE);

	if (out_source != NULL)
		*out_source = NULL;  /* in case we fail */

	if (out_contacts != NULL)
		*out_contacts = NULL;  /* in case we fail */

	if (!strncmp (str, "Book: ", 6)) {
		s0 = str + 6;
		s1 = strchr (str, '\r');

		if (!s1)
			s1 = strchr (str, '\n');
	} else {
		s0 = NULL;
		s1 = NULL;
	}

	if (!s0 || !s1)
		return FALSE;

	uid = g_strndup (s0, s1 - s0);
	source = e_source_registry_ref_source (registry, uid);
	if (source != NULL) {
		if (out_source != NULL)
			*out_source = g_object_ref (source);
		g_object_unref (source);
		success = TRUE;
	}
	g_free (uid);

	if (success && out_contacts != NULL)
		*out_contacts = eab_contact_list_from_string (str);

	return success;
}

static gchar *
eab_add_book_to_string (EBookClient *book_client,
			gchar *inout_str)
{
	ESource *source;
	const gchar *uid;
	gchar *str;

	if (!book_client)
		return inout_str;

	source = e_client_get_source (E_CLIENT (book_client));
	uid = e_source_get_uid (source);

	str = g_strconcat ("Book: ", uid, "\r\n", inout_str, NULL);

	g_free (inout_str);

	return str;
}

gchar *
eab_book_and_contact_array_to_string (EBookClient *book_client,
				      const GPtrArray *contacts)
{
	gchar *str;

	str = eab_contact_array_to_string (contacts);
	if (!str)
		str = g_strdup ("");

	return eab_add_book_to_string (book_client, str);
}

/* bad place for this i know. */
gint
e_utf8_casefold_collate_len (const gchar *str1,
                             const gchar *str2,
                             gint len)
{
	gchar *s1 = g_utf8_casefold (str1, len);
	gchar *s2 = g_utf8_casefold (str2, len);
	gint rv;

	rv = g_utf8_collate (s1, s2);

	g_free (s1);
	g_free (s2);

	return rv;
}

gint
e_utf8_casefold_collate (const gchar *str1,
                         const gchar *str2)
{
	return e_utf8_casefold_collate_len (str1, str2, -1);
}

/* To parse something like...
 * =?UTF-8?Q?=E0=A4=95=E0=A4=95=E0=A4=AC=E0=A5=82=E0=A5=8B=E0=A5=87?=\t\n=?UTF-8?Q?=E0=A4=B0?=\t\n<aa@aa.ccom>
 * and return the decoded representation of name & email parts. */
gboolean
eab_parse_qp_email (const gchar *string,
                    gchar **name,
                    gchar **email)
{
	struct _camel_header_address *address;
	gboolean res = FALSE;

	address = camel_header_address_decode (string, "UTF-8");

	if (address) {
		/* report success only when we have filled both name and email address */
		if (address->type == CAMEL_HEADER_ADDRESS_NAME && address->name && *address->name && address->v.addr && *address->v.addr) {
			*name = g_strdup (address->name);
			*email = g_strdup (address->v.addr);
			res = TRUE;
		}

		camel_header_address_unref (address);
	}

	if (!res) {
		CamelInternetAddress *addr = camel_internet_address_new ();
		const gchar *const_name = NULL, *const_email = NULL;

		if (camel_address_unformat (CAMEL_ADDRESS (addr), string) == 1 &&
		    camel_internet_address_get (addr, 0, &const_name, &const_email) &&
		    const_name && *const_name && const_email && *const_email) {
			*name = g_strdup (const_name);
			*email = g_strdup (const_email);
			res = TRUE;
		}

		g_clear_object (&addr);
	}

	return res;
}

/* This is only wrapper to parse_qp_mail, it decodes string and if returned TRUE,
 * then makes one string and returns it, otherwise returns NULL.
 * Returned string is usable to place directly into GtkHtml stream.
 * Returned value should be freed with g_free. */
gchar *
eab_parse_qp_email_to_html (const gchar *string)
{
	gchar *name = NULL, *mail = NULL;
	gchar *html_name, *html_mail;
	gchar *value;

	if (!eab_parse_qp_email (string, &name, &mail))
		return NULL;

	html_name = e_text_to_html (name, 0);
	html_mail = e_text_to_html (mail, E_TEXT_TO_HTML_CONVERT_ADDRESSES);

	value = g_strdup_printf ("%s &lt;%s&gt;", html_name, html_mail);

	g_free (html_name);
	g_free (html_mail);
	g_free (name);
	g_free (mail);

	return value;
}

EContact *
eab_new_contact_for_book (EBookClient *book_client)
{
	EContact *contact;

	contact = e_contact_new ();

	if (book_client) {
		EVCardVersion version;

		version = e_book_client_get_prefer_vcard_version (book_client);
		if (version != E_VCARD_VERSION_UNKNOWN)
			e_vcard_add_attribute_with_value (E_VCARD (contact), e_vcard_attribute_new (NULL, EVC_VERSION), e_vcard_version_to_string (version));
	}

	return contact;
}
