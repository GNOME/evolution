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
#include <stdio.h>

#include "e-util/e-util.h"
#include "eab-book-util.h"

/* Template tags for address format localization */
#define ADDRESS_REALNAME   			"%n" /* this is not used intentionally */
#define ADDRESS_REALNAME_UPPER			"%N" /* this is not used intentionally */
#define ADDRESS_COMPANY				"%m"
#define ADDRESS_COMPANY_UPPER			"%M"
#define ADDRESS_POBOX				"%p"
#define ADDRESS_STREET				"%s"
#define ADDRESS_STREET_UPPER			"%S"
#define ADDRESS_ZIPCODE				"%z"
#define ADDRESS_LOCATION			"%l"
#define ADDRESS_LOCATION_UPPER			"%L"
#define ADDRESS_REGION				"%r"
#define ADDRESS_REGION_UPPER			"%R"
#define ADDRESS_CONDCOMMA			"%,"	/* Conditional comma is removed when a surrounding tag is evaluated to zero */
#define ADDRESS_CONDWHITE			"%w"	/* Conditional whitespace is removed when a surrounding tag is evaluated to zero */
#define ADDRESS_COND_PURGEEMPTY			"%0"	/* Purge empty has following syntax: %0(...) and is removed when no tag within () is evaluated non-zero */

/* Fallback formats */
#define ADDRESS_DEFAULT_FORMAT 			"%0(%n\n)%0(%m\n)%0(%s\n)%0(PO BOX %p\n)%0(%l%w%r)%,%z"
#define ADDRESS_DEFAULT_COUNTRY_POSITION	"below"

enum {
	LOCALES_LANGUAGE = 0,
	LOCALES_COUNTRY = 1
};

typedef enum {
	ADDRESS_FORMAT_HOME = 0,
	ADDRESS_FORMAT_BUSINESS = 1
} AddressFormat;

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

/*
 * eab_format_address helper function
 *
 * Splits locales from en_US to array "en","us",NULL. When
 * locales don't have the second part (for example "C"),
 * the output array is "c",NULL
 */
static gchar **
get_locales (void)
{
	gchar *locale, *l_locale;
	gchar *dot;
	gchar **split;

#ifdef LC_ADDRESS
	locale = g_strdup (setlocale (LC_ADDRESS, NULL));
#else
	locale = NULL;
#endif
	if (!locale)
		return NULL;

	l_locale = g_utf8_strdown (locale, -1);
	g_free (locale);

	dot = strchr (l_locale, '.');
	if (dot != NULL) {
		gchar *p = l_locale;
		l_locale = g_strndup (l_locale, dot - l_locale);
		g_free (p);
	}

	split = g_strsplit (l_locale, "_", 2);

	g_free (l_locale);
	return split;

}

static gchar *
get_locales_str (void)
{
	gchar *ret = NULL;
	gchar **loc = get_locales ();

	if (!loc)
		return g_strdup ("C");

	if (!loc[0] ||
	    (loc[0] && !loc[1])) /* We don't care about language now, we need a country at first! */
		ret = g_strdup ("C");
	else if (loc[0] && loc[1]) {
		if (*loc[0])
			ret = g_strconcat (loc[LOCALES_COUNTRY], "_", loc[LOCALES_LANGUAGE], NULL);
		else
			ret = g_strdup (loc[LOCALES_COUNTRY]);
	}

	g_strfreev (loc);
	return ret;
}

/*
 * Reads countrytransl.map file, which contains map of localized
 * country names and their ISO codes and tries to find matching record
 * for given country. The search is case insensitive.
 * When no record is found (country is probably in untranslated language), returns
 * code of local computer country (from locales)
 */
static gchar *
country_to_ISO (const gchar *country)
{
	FILE *file = fopen (EVOLUTION_PRIVDATADIR "/countrytransl.map", "r");
	gchar buffer[100];
	gint length = 100;
	gchar **pair;
	gchar *res;
	gchar *l_country = g_utf8_strdown (country, -1);

	if (!file) {
		gchar **loc;
		g_warning ("%s: Failed to open countrytransl.map. Check your installation.", G_STRFUNC);
		loc = get_locales ();
		res = g_strdup (loc ? loc[LOCALES_COUNTRY] : NULL);
		g_free (l_country);
		g_strfreev (loc);
		return res;
	}

	while (fgets (buffer, length, file) != NULL) {
		gchar *low = NULL;
		pair = g_strsplit (buffer, "\t", 2);

		if (pair[0]) {
			low = g_utf8_strdown (pair[0], -1);
			if (g_utf8_collate (low, l_country) == 0) {
				gchar *ret = g_strdup (pair[1]);
				gchar *pos;
				/* Remove trailing newline character */
				if ((pos = g_strrstr (ret, "\n")) != NULL)
					pos[0] = '\0';
				fclose (file);
				g_strfreev (pair);
				g_free (low);
				g_free (l_country);
				return ret;
			}
		}

		g_strfreev (pair);
		g_free (low);
	}

	/* If we get here, then no match was found in the map file and we
	 * fallback to local system locales */
	fclose (file);

	pair = get_locales ();
	res = g_strdup (pair ? pair[LOCALES_COUNTRY] : NULL);
	g_strfreev (pair);
	g_free (l_country);
	return res;
}

/*
 * Tries to find given key in "country_LANGUAGE" group. When fails to find
 * such group, then fallbacks to "country" group. When such group does not
 * exist either, NULL is returned
 */
static gchar *
get_key_file_locale_string (GKeyFile *key_file,
                            const gchar *key,
                            const gchar *locale)
{
	gchar *result;
	gchar *group;

	g_return_val_if_fail (locale, NULL);

	/* Default locale is in "country_lang", but such group may not exist. In such case use group "country" */
	if (g_key_file_has_group (key_file, locale))
		group = g_strdup (locale);
	else {
		gchar **locales = g_strsplit (locale, "_", 0);
		group = g_strdup (locales[LOCALES_COUNTRY]);
		g_strfreev (locales);
	}

	/* When group or key does not exist, returns NULL and fallback string will be used */
	result = g_key_file_get_string (key_file, group, key, NULL);
	g_free (group);
	return result;
}

static void
get_address_format (AddressFormat address_format,
                    const gchar *locale,
                    gchar **format,
                    gchar **country_position)
{
	GKeyFile *key_file;
	GError *error;
	gchar *loc;
	const gchar *addr_key, *country_key;

	if (address_format == ADDRESS_FORMAT_HOME) {
		addr_key = "AddressFormat";
		country_key = "CountryPosition";
	} else if (address_format == ADDRESS_FORMAT_BUSINESS) {
		addr_key = "BusinessAddressFormat";
		country_key = "BusinessCountryPosition";
	} else {
		return;
	}

	if (locale == NULL)
		loc = get_locales_str ();
	else
		loc = g_strdup (locale);

	error = NULL;
	key_file = g_key_file_new ();
	g_key_file_load_from_file (key_file, EVOLUTION_PRIVDATADIR "/address_formats.dat", 0, &error);
	if (error != NULL) {
		g_warning ("%s: Failed to load address_formats.dat file: %s", G_STRFUNC, error->message);
		if (format)
			*format = g_strdup (ADDRESS_DEFAULT_FORMAT);
		if (country_position)
			*country_position = g_strdup (ADDRESS_DEFAULT_COUNTRY_POSITION);
		g_key_file_free (key_file);
		g_free (loc);
		g_error_free (error);
		return;
	}

	if (format) {
		g_free (*format);
		*format = get_key_file_locale_string (key_file, addr_key, loc);
		if (!*format && address_format == ADDRESS_FORMAT_HOME) {
			*format = g_strdup (ADDRESS_DEFAULT_FORMAT);
		} else if (!*format && address_format == ADDRESS_FORMAT_BUSINESS)
			get_address_format (ADDRESS_FORMAT_HOME, loc, format, NULL);
	}

	if (country_position) {
		g_free (*country_position);
		*country_position = get_key_file_locale_string (key_file, country_key, loc);
		if (!*country_position && address_format == ADDRESS_FORMAT_HOME)
			*country_position = g_strdup (ADDRESS_DEFAULT_COUNTRY_POSITION);
		else if (!*country_position && address_format == ADDRESS_FORMAT_BUSINESS)
			get_address_format (ADDRESS_FORMAT_HOME, loc, NULL, country_position);
	}

	g_free (loc);
	g_key_file_free (key_file);
}

static const gchar *
find_balanced_bracket (const gchar *str)
{
	gint balance_counter = 0;
	gint i = 0;

	do {
		if (str[i] == '(')
			balance_counter++;

		if (str[i] == ')')
			balance_counter--;

		i++;

	} while ((balance_counter > 0) && (str[i]));

	if (balance_counter > 0)
		return str;

	return str + i;
}

static GString *
string_append_upper (GString *str,
                     const gchar *c)
{
	gchar *up_c;

	g_return_val_if_fail (str, NULL);

	if (!c || !*c)
		return str;

	up_c = g_utf8_strup (c, -1);
	g_string_append (str, up_c);
	g_free (up_c);

	return str;
}

static gboolean
parse_address_template_section (const gchar *format,
                                const gchar *realname,
                                const gchar *org_name,
                                const EContactAddress *address,
                                gchar **result)

{
	const gchar *pos, *old_pos;
	gboolean ret = FALSE; /* Indicates, wheter at least something was replaced */

	GString *res = g_string_new ("");

	pos = format;
	old_pos = pos;
	while ((pos = strchr (pos, '%')) != NULL) {

		if (old_pos != pos)
			g_string_append_len (res, old_pos, pos - old_pos);

		switch (pos[1]) {
			case 'n':
				if (realname && *realname) {
					g_string_append (res, realname);
					ret = TRUE;
				}
				pos += 2; /* Jump behind the modifier, see what's next */
				break;
			case 'N':
				if (realname && *realname) {
					string_append_upper (res, realname);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'm':
				if (org_name && *org_name) {
					g_string_append (res, org_name);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'M':
				if (org_name && *org_name) {
					string_append_upper (res, org_name);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'p':
				if (address->po && *(address->po)) {
					g_string_append (res, address->po);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 's':
				if (address->street && *(address->street)) {
					g_string_append (res, address->street);
					if (address->ext && *(address->ext))
						g_string_append_printf (
							res, "\n%s",
							address->ext);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'S':
				if (address->street && *(address->street)) {
					string_append_upper (res, address->street);
					if (address->ext && *(address->ext)) {
						g_string_append_c (res, '\n');
						string_append_upper (res, address->ext);
					}
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'z':
				if (address->code && *(address->code)) {
					g_string_append (res, address->code);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'l':
				if (address->locality && *(address->locality)) {
					g_string_append (res, address->locality);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'L':
				if (address->locality && *(address->locality)) {
					string_append_upper (res, address->locality);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'r':
				if (address->region && *(address->region)) {
					g_string_append (res, address->region);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'R':
				if (address->region && *(address->region)) {
					string_append_upper (res, address->region);
					ret = TRUE;
				}
				pos += 2;
				break;
			case ',':
				if (ret && (pos >= format + 2) &&		/* If there's something before %, */
				    (g_ascii_strcasecmp (pos - 2, "\n") != 0) &&  /* And if it is not a newline */
				    (g_ascii_strcasecmp (pos - 2, "%w") != 0))    /* Nor whitespace */
					g_string_append (res, ", ");
				pos += 2;
				break;
			case 'w':
				if (ret && (pos >= format + 2) &&
				    (g_ascii_strcasecmp (pos - 2, "\n") != 0) &&
				    (g_ascii_strcasecmp (pos - 1, " ") != 0))
					g_string_append_c (res, ' ');
				pos += 2;
				break;
			case '0': {
				const gchar *bpos1, *bpos2;
				gchar *inner;
				gchar *ires;
				gboolean replaced;

				bpos1 = pos + 2;
				bpos2 = find_balanced_bracket (bpos1);

				inner = g_strndup (bpos1 + 1, bpos2 - bpos1 - 2); /* Get inner content of the %0 (...) */
				replaced = parse_address_template_section (inner, realname, org_name, address, &ires);
				if (replaced)
					g_string_append (res, ires);

				g_free (ires);
				g_free (inner);

				ret = replaced;
				pos += (bpos2 - bpos1 + 2);
			} break;
		}

		old_pos = pos;
	}
	g_string_append (res, old_pos);

	*result = g_string_free (res, FALSE);

	return ret;
}

static gchar *
eab_format_address_field (const EContactAddress *addr,
			  EContactField address_type,
			  const gchar *organization)
{
	gchar *result;
	gchar *format = NULL;
	gchar *country_position = NULL;
	gchar *locale;

	if (address_type != E_CONTACT_ADDRESS_HOME && address_type != E_CONTACT_ADDRESS_WORK)
		return NULL;

	if (!addr->po && !addr->ext && !addr->street && !addr->locality && !addr->region &&
	    !addr->code && !addr->country) {
		return NULL;
	}

	if (addr->country) {
		gchar *cntry = country_to_ISO (addr->country);
		gchar **loc = get_locales ();
		locale = g_strconcat (loc ? loc[LOCALES_LANGUAGE] : "C", "_", cntry, NULL);
		g_strfreev (loc);
		g_free (cntry);
	} else
		locale = get_locales_str ();

	if (address_type == E_CONTACT_ADDRESS_HOME)
		get_address_format (ADDRESS_FORMAT_HOME, locale, &format, &country_position);
	else
		get_address_format (ADDRESS_FORMAT_BUSINESS, locale, &format, &country_position);

	/* Expand all the variables in format.
	 * Don't display organization in home address;
	 * and skip full names, as it's part of the EContact itself,
	 * check this bug for reason: https://bugzilla.gnome.org/show_bug.cgi?id=667912
	 */
	parse_address_template_section (
		format,
		NULL,
		(address_type == E_CONTACT_ADDRESS_WORK) ? organization : NULL,
		addr,
		&result);

	/* Add the country line. In some countries, the address can be located above the
	 * rest of the address */
	if (addr->country && country_position) {
		gchar *country_upper = g_utf8_strup (addr->country, -1);
		gchar *p = result;
		if (g_strcmp0 (country_position, "BELOW") == 0) {
			result = g_strconcat (p, "\n\n", country_upper, NULL);
			g_free (p);
		} else if (g_strcmp0 (country_position, "below") == 0) {
			result = g_strconcat (p, "\n\n", addr->country, NULL);
			g_free (p);
		} else if (g_strcmp0 (country_position, "ABOVE") == 0) {
			result = g_strconcat (country_upper, "\n\n", p, NULL);
			g_free (p);
		} else if (g_strcmp0 (country_position, "above") == 0) {
			result = g_strconcat (addr->country, "\n\n", p, NULL);
			g_free (p);
		}
		g_free (country_upper);
	}

	g_free (locale);
	g_free (format);
	g_free (country_position);

	return result;
}

gchar *
eab_format_address (EContact *contact,
                    EContactField address_type)
{
	EContactAddress *addr = e_contact_get (contact, address_type);
	gchar *str;

	if (!addr)
		return NULL;

	str = eab_format_address_field (addr, address_type, e_contact_get_const (contact, E_CONTACT_ORG));

	e_contact_address_free (addr);

	return str;
}

static void
append_to_address_label (GString *str,
			 const gchar *part,
			 gboolean newline)
{
	if (!part || !*part)
		return;

	if (str->len)
		g_string_append (str, newline ? "\n" : ", ");

	g_string_append (str, part);
}

gchar *
eab_format_address_label (const EContactAddress *addr,
			  EContactField address_type,
			  const gchar *organization)
{
	GSettings *settings;
	gchar *label = NULL;

	if (!addr)
		return NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");
	if (g_settings_get_boolean (settings, "address-formatting"))
		label = eab_format_address_field (addr, address_type, organization);
	g_object_unref (settings);

	if (!label) {
		GString *str = g_string_new ("");

		append_to_address_label (str, addr->street, TRUE);
		append_to_address_label (str, addr->ext, TRUE);
		append_to_address_label (str, addr->locality, TRUE);
		append_to_address_label (str, addr->region, FALSE);
		append_to_address_label (str, addr->code, TRUE);
		append_to_address_label (str, addr->po, TRUE);
		append_to_address_label (str, addr->country, TRUE);

		label = g_string_free (str, !str->len);
	}

	return label;
}
