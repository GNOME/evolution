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
 *		Gilbert Fang <gilbert.fang@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libebook/libebook.h>

#include "evolution-addressbook-export.h"

#define COMMA_SEPARATOR ","

typedef enum _CARD_FORMAT CARD_FORMAT;
typedef enum _DeliveryAddressField DeliveryAddressField;
typedef enum _EContactFieldCSV EContactFieldCSV;
typedef struct _EContactCSVFieldData EContactCSVFieldData;

enum _CARD_FORMAT
{
	CARD_FORMAT_CSV,
	CARD_FORMAT_VCARD
};

enum _DeliveryAddressField
{
	DELIVERY_ADDRESS_STREET,
	DELIVERY_ADDRESS_EXT,
	DELIVERY_ADDRESS_LOCALITY,
	DELIVERY_ADDRESS_REGION,
	DELIVERY_ADDRESS_CODE,
	DELIVERY_ADDRESS_COUNTRY
};

enum _EContactFieldCSV
{
	E_CONTACT_CSV_FILE_AS,
	E_CONTACT_CSV_FULL_NAME,
	E_CONTACT_CSV_EMAIL_1,
	E_CONTACT_CSV_EMAIL_2,
	E_CONTACT_CSV_EMAIL_3,
	E_CONTACT_CSV_EMAIL_4,
	E_CONTACT_CSV_PHONE_PRIMARY,
	E_CONTACT_CSV_PHONE_ASSISTANT,
	E_CONTACT_CSV_PHONE_BUSINESS,
	E_CONTACT_CSV_PHONE_CALLBACK,
	E_CONTACT_CSV_PHONE_COMPANY,
	E_CONTACT_CSV_PHONE_HOME,
	E_CONTACT_CSV_ORG,
	/*E_CONTACT_CSV_ADDRESS_BUSINESS, */
	E_CONTACT_CSV_ADDRESS_BUSINESS_STREET,
	E_CONTACT_CSV_ADDRESS_BUSINESS_EXT,
	E_CONTACT_CSV_ADDRESS_BUSINESS_CITY,
	E_CONTACT_CSV_ADDRESS_BUSINESS_REGION,
	E_CONTACT_CSV_ADDRESS_BUSINESS_POSTCODE,
	E_CONTACT_CSV_ADDRESS_BUSINESS_COUNTRY,
	/*E_CONTACT_CSV_ADDRESS_HOME, */
	E_CONTACT_CSV_ADDRESS_HOME_STREET,
	E_CONTACT_CSV_ADDRESS_HOME_EXT,
	E_CONTACT_CSV_ADDRESS_HOME_CITY,
	E_CONTACT_CSV_ADDRESS_HOME_REGION,
	E_CONTACT_CSV_ADDRESS_HOME_POSTCODE,
	E_CONTACT_CSV_ADDRESS_HOME_COUNTRY,
	E_CONTACT_CSV_PHONE_MOBILE,
	E_CONTACT_CSV_PHONE_CAR,
	E_CONTACT_CSV_PHONE_BUSINESS_FAX,
	E_CONTACT_CSV_PHONE_HOME_FAX,
	E_CONTACT_CSV_PHONE_BUSINESS_2,
	E_CONTACT_CSV_PHONE_HOME_2,
	E_CONTACT_CSV_PHONE_ISDN,
	E_CONTACT_CSV_PHONE_OTHER,
	E_CONTACT_CSV_PHONE_OTHER_FAX,
	E_CONTACT_CSV_PHONE_PAGER,
	E_CONTACT_CSV_PHONE_RADIO,
	E_CONTACT_CSV_PHONE_TELEX,
	E_CONTACT_CSV_PHONE_TTYTDD,
	/*E_CONTACT_CSV_ADDRESS_OTHER, */
	E_CONTACT_CSV_ADDRESS_OTHER_STREET,
	E_CONTACT_CSV_ADDRESS_OTHER_EXT,
	E_CONTACT_CSV_ADDRESS_OTHER_CITY,
	E_CONTACT_CSV_ADDRESS_OTHER_REGION,
	E_CONTACT_CSV_ADDRESS_OTHER_POSTCODE,
	E_CONTACT_CSV_ADDRESS_OTHER_COUNTRY,
	E_CONTACT_CSV_HOMEPAGE_URL,
	E_CONTACT_CSV_ORG_UNIT,
	E_CONTACT_CSV_OFFICE,
	E_CONTACT_CSV_TITLE,
	E_CONTACT_CSV_ROLE,
	E_CONTACT_CSV_MANAGER,
	E_CONTACT_CSV_ASSISTANT,
	E_CONTACT_CSV_NICKNAME,
	E_CONTACT_CSV_SPOUSE,
	E_CONTACT_CSV_NOTE,
	E_CONTACT_CSV_CALENDAR_URI,
	E_CONTACT_CSV_FREEBUSY_URL,
	/*E_CONTACT_CSV_ANNIVERSARY, */
	E_CONTACT_CSV_ANNIVERSARY_YEAR,
	E_CONTACT_CSV_ANNIVERSARY_MONTH,
	E_CONTACT_CSV_ANNIVERSARY_DAY,
	/*E_CONTACT_CSV_BIRTH_DATE, */
	E_CONTACT_CSV_BIRTH_DATE_YEAR,
	E_CONTACT_CSV_BIRTH_DATE_MONTH,
	E_CONTACT_CSV_BIRTH_DATE_DAY,
	E_CONTACT_CSV_MAILER,
	E_CONTACT_CSV_NAME_OR_ORG,
	E_CONTACT_CSV_CATEGORIES,
	E_CONTACT_CSV_FAMILY_NAME,
	E_CONTACT_CSV_GIVEN_NAME,
	E_CONTACT_CSV_WANTS_HTML,
	E_CONTACT_CSV_IS_LIST,
	E_CONTACT_CSV_LAST
};

typedef enum {
	DT_STRING,
	DT_BOOLEAN
} EContactCSVDataType;

struct _EContactCSVFieldData
{
	gint csv_field;
	gint contact_field;
	const gchar *csv_name;
	EContactCSVDataType data_type;
};

#define NOMAP -1
static EContactCSVFieldData csv_field_data[] = {
	{E_CONTACT_CSV_FILE_AS,		E_CONTACT_FILE_AS,	   "", DT_STRING},
	{E_CONTACT_CSV_FULL_NAME,	E_CONTACT_CSV_FULL_NAME,   "", DT_STRING},
	{E_CONTACT_CSV_EMAIL_1,		E_CONTACT_EMAIL_1,	   "", DT_STRING},
	{E_CONTACT_CSV_EMAIL_2,		E_CONTACT_EMAIL_2,	   "", DT_STRING},
	{E_CONTACT_CSV_EMAIL_3,		E_CONTACT_EMAIL_3,	   "", DT_STRING},
	{E_CONTACT_CSV_EMAIL_4,		E_CONTACT_EMAIL_4,	   "", DT_STRING},
	{E_CONTACT_CSV_PHONE_PRIMARY,	E_CONTACT_PHONE_PRIMARY,   "", DT_STRING},
	{E_CONTACT_CSV_PHONE_ASSISTANT,	E_CONTACT_PHONE_ASSISTANT, "", DT_STRING},
	{E_CONTACT_CSV_PHONE_BUSINESS,	E_CONTACT_PHONE_BUSINESS,  "", DT_STRING},
	{E_CONTACT_CSV_PHONE_CALLBACK,	E_CONTACT_PHONE_CALLBACK,  "", DT_STRING},
	{E_CONTACT_CSV_PHONE_COMPANY,	E_CONTACT_PHONE_COMPANY,   "", DT_STRING},
	{E_CONTACT_CSV_PHONE_HOME,	E_CONTACT_PHONE_HOME,	   "", DT_STRING},
	{E_CONTACT_CSV_ORG,		E_CONTACT_ORG,		   "", DT_STRING},
	/*E_CONTACT_CSV_ADDRESS_BUSINESS, */
	{E_CONTACT_CSV_ADDRESS_BUSINESS_STREET,	  NOMAP, "Business Address",	      DT_STRING},
	{E_CONTACT_CSV_ADDRESS_BUSINESS_EXT,	  NOMAP, "Business Address2",         DT_STRING},
	{E_CONTACT_CSV_ADDRESS_BUSINESS_CITY,	  NOMAP, "Business Address City",     DT_STRING},
	{E_CONTACT_CSV_ADDRESS_BUSINESS_REGION,	  NOMAP, "Business Address State",    DT_STRING},
	{E_CONTACT_CSV_ADDRESS_BUSINESS_POSTCODE, NOMAP, "Business Address PostCode", DT_STRING},
	{E_CONTACT_CSV_ADDRESS_BUSINESS_COUNTRY,  NOMAP, "Business Address Country",  DT_STRING},
	/*E_CONTACT_CSV_ADDRESS_HOME, */
	{E_CONTACT_CSV_ADDRESS_HOME_STREET,   NOMAP, "Home Address",          DT_STRING},
	{E_CONTACT_CSV_ADDRESS_HOME_EXT,      NOMAP, "Home Address2",         DT_STRING},
	{E_CONTACT_CSV_ADDRESS_HOME_CITY,     NOMAP, "Home Address City",     DT_STRING},
	{E_CONTACT_CSV_ADDRESS_HOME_REGION,   NOMAP, "Home Address State",    DT_STRING},
	{E_CONTACT_CSV_ADDRESS_HOME_POSTCODE, NOMAP, "Home Address PostCode", DT_STRING},
	{E_CONTACT_CSV_ADDRESS_HOME_COUNTRY,  NOMAP, "Home Address Country",  DT_STRING},
	{E_CONTACT_CSV_PHONE_MOBILE,	      E_CONTACT_PHONE_MOBILE,       "", DT_STRING},
	{E_CONTACT_CSV_PHONE_CAR,	      E_CONTACT_PHONE_CAR,          "", DT_STRING},
	{E_CONTACT_CSV_PHONE_BUSINESS_FAX,    E_CONTACT_PHONE_BUSINESS_FAX, "", DT_STRING},
	{E_CONTACT_CSV_PHONE_HOME_FAX,        E_CONTACT_PHONE_HOME_FAX,     "", DT_STRING},
	{E_CONTACT_CSV_PHONE_BUSINESS_2,      E_CONTACT_PHONE_BUSINESS_2,   "", DT_STRING},
	{E_CONTACT_CSV_PHONE_HOME_2,          E_CONTACT_PHONE_HOME_2,       "", DT_STRING},
	{E_CONTACT_CSV_PHONE_ISDN,            E_CONTACT_PHONE_ISDN,         "", DT_STRING},
	{E_CONTACT_CSV_PHONE_OTHER,           E_CONTACT_PHONE_OTHER,        "", DT_STRING},
	{E_CONTACT_CSV_PHONE_OTHER_FAX,       E_CONTACT_PHONE_OTHER_FAX,    "", DT_STRING},
	{E_CONTACT_CSV_PHONE_PAGER,           E_CONTACT_PHONE_PAGER,        "", DT_STRING},
	{E_CONTACT_CSV_PHONE_RADIO,           E_CONTACT_PHONE_RADIO,        "", DT_STRING},
	{E_CONTACT_CSV_PHONE_TELEX,           E_CONTACT_PHONE_TELEX,        "", DT_STRING},
	{E_CONTACT_CSV_PHONE_TTYTDD,          E_CONTACT_PHONE_TTYTDD,       "", DT_STRING},
	/*E_CONTACT_CSV_ADDRESS_OTHER, */
	{E_CONTACT_CSV_ADDRESS_OTHER_STREET,   NOMAP, "Other Address",          DT_STRING},
	{E_CONTACT_CSV_ADDRESS_OTHER_EXT,      NOMAP, "Other Address2",         DT_STRING},
	{E_CONTACT_CSV_ADDRESS_OTHER_CITY,     NOMAP, "Other Address City",     DT_STRING},
	{E_CONTACT_CSV_ADDRESS_OTHER_REGION,   NOMAP, "Other Address State",    DT_STRING},
	{E_CONTACT_CSV_ADDRESS_OTHER_POSTCODE, NOMAP, "Other Address PostCode", DT_STRING},
	{E_CONTACT_CSV_ADDRESS_OTHER_COUNTRY,  NOMAP, "Other Address Country",  DT_STRING},
	{E_CONTACT_CSV_HOMEPAGE_URL,           E_CONTACT_HOMEPAGE_URL, "", DT_STRING},
	{E_CONTACT_CSV_ORG_UNIT,               E_CONTACT_ORG_UNIT,     "", DT_STRING},
	{E_CONTACT_CSV_OFFICE,                 E_CONTACT_OFFICE,       "", DT_STRING},
	{E_CONTACT_CSV_TITLE,                  E_CONTACT_TITLE,        "", DT_STRING},
	{E_CONTACT_CSV_ROLE,                   E_CONTACT_ROLE,         "", DT_STRING},
	{E_CONTACT_CSV_MANAGER,                E_CONTACT_MANAGER,      "", DT_STRING},
	{E_CONTACT_CSV_ASSISTANT,              E_CONTACT_ASSISTANT,    "", DT_STRING},
	{E_CONTACT_CSV_NICKNAME,               E_CONTACT_NICKNAME,     "", DT_STRING},
	{E_CONTACT_CSV_SPOUSE,                 E_CONTACT_SPOUSE,       "", DT_STRING},
	{E_CONTACT_CSV_NOTE,                   E_CONTACT_NOTE,         "", DT_STRING},
	{E_CONTACT_CSV_CALENDAR_URI,           E_CONTACT_CALENDAR_URI, "", DT_STRING},
	{E_CONTACT_CSV_FREEBUSY_URL,           E_CONTACT_FREEBUSY_URL, "", DT_STRING},
	/*E_CONTACT_ANNIVERSARY, */
	{E_CONTACT_CSV_ANNIVERSARY_YEAR,       NOMAP, "Anniversary Year",  DT_STRING},
	{E_CONTACT_CSV_ANNIVERSARY_MONTH,      NOMAP, "Anniversary Month", DT_STRING},
	{E_CONTACT_CSV_ANNIVERSARY_DAY,        NOMAP, "Anniversary Day",   DT_STRING},
	/*E_CONTACT_BIRTH_DATE, */
	{E_CONTACT_CSV_BIRTH_DATE_YEAR,  NOMAP, "Birth Year",  DT_STRING},
	{E_CONTACT_CSV_BIRTH_DATE_MONTH, NOMAP, "Birth Month", DT_STRING},
	{E_CONTACT_CSV_BIRTH_DATE_DAY,   NOMAP, "Birth Day",   DT_STRING},
	{E_CONTACT_CSV_MAILER,           E_CONTACT_MAILER,      "", DT_STRING},
	{E_CONTACT_CSV_NAME_OR_ORG,      E_CONTACT_NAME_OR_ORG, "", DT_STRING},
	{E_CONTACT_CSV_CATEGORIES,       E_CONTACT_CATEGORIES,  "", DT_STRING},
	{E_CONTACT_CSV_FAMILY_NAME,      E_CONTACT_FAMILY_NAME, "", DT_STRING},
	{E_CONTACT_CSV_GIVEN_NAME,       E_CONTACT_GIVEN_NAME,  "", DT_STRING},
	{E_CONTACT_CSV_WANTS_HTML,       E_CONTACT_WANTS_HTML,  "", DT_BOOLEAN},
	{E_CONTACT_CSV_IS_LIST,          E_CONTACT_IS_LIST,     "", DT_BOOLEAN},
	{E_CONTACT_CSV_LAST,             NOMAP,                 "", DT_STRING}

};

static GSList *pre_defined_fields;

/*function prototypes*/
gint e_contact_csv_get_contact_field (EContactFieldCSV csv_field);
gchar *e_contact_csv_get_name (EContactFieldCSV csv_field);
gchar *e_contact_csv_get (EContact * contact, EContactFieldCSV csv_field);
gchar *e_contact_csv_get_header_line (GSList * csv_all_fields);
gchar *e_contact_to_csv (EContact * contact, GSList * csv_all_fields);
gchar *e_contact_get_csv (EContact * contact, GSList * csv_all_fields);
gchar *delivery_address_get_sub_field (const EContactAddress * delivery_address, DeliveryAddressField sub_field);
gchar *check_null_pointer (gchar * orig);
gchar *escape_string (gchar * orig);
gint output_n_cards_file (FILE * outputfile, GSList *contacts, gint size, gint begin_no, CARD_FORMAT format);
void set_pre_defined_field (GSList ** pre_defined_fields);

/* function declarations*/
gint
e_contact_csv_get_contact_field (EContactFieldCSV csv_field)
{
	return csv_field_data[csv_field].contact_field;
}

static EContactCSVDataType
e_contact_csv_get_data_type (EContactFieldCSV csv_field)
{
	return csv_field_data[csv_field].data_type;
}

gchar *
e_contact_csv_get_name (EContactFieldCSV csv_field)
{
	gint contact_field;
	gchar *name;
	gchar *quoted_name;

	contact_field = e_contact_csv_get_contact_field (csv_field);

	if (contact_field != NOMAP) {
		name = g_strdup (e_contact_field_name (contact_field));
	} else {
		name = g_strdup (csv_field_data[csv_field].csv_name);
	}
	quoted_name = escape_string (name);
	g_free (name);
	return quoted_name;
}

gchar *
e_contact_csv_get (EContact *contact,
                   EContactFieldCSV csv_field)
{
	gint contact_field;
	gchar *field_value;
	gchar *quoted_field_value;

	EContactAddress *delivery_address = NULL;
	EContactDate *date;

	contact_field = e_contact_csv_get_contact_field (csv_field);

	if (contact_field != NOMAP) {
		field_value = e_contact_get (contact, contact_field);
		if (e_contact_csv_get_data_type (csv_field) == DT_BOOLEAN) {
			field_value = g_strdup ((GPOINTER_TO_INT (field_value)) ? "TRUE" : "FALSE");
		}
	} else {
		switch (csv_field) {
		case E_CONTACT_CSV_ADDRESS_HOME_STREET:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_STREET);
			break;
		case E_CONTACT_CSV_ADDRESS_HOME_EXT:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_EXT);
			break;
		case E_CONTACT_CSV_ADDRESS_HOME_CITY:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_LOCALITY);
			break;
		case E_CONTACT_CSV_ADDRESS_HOME_REGION:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_REGION);
			break;
		case E_CONTACT_CSV_ADDRESS_HOME_POSTCODE:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_CODE);
			break;
		case E_CONTACT_CSV_ADDRESS_HOME_COUNTRY:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_COUNTRY);
			break;
		case E_CONTACT_CSV_ADDRESS_BUSINESS_STREET:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_WORK);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_STREET);
			break;
		case E_CONTACT_CSV_ADDRESS_BUSINESS_EXT:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_WORK);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_EXT);
			break;
		case E_CONTACT_CSV_ADDRESS_BUSINESS_CITY:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_WORK);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_LOCALITY);
			break;
		case E_CONTACT_CSV_ADDRESS_BUSINESS_REGION:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_WORK);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_REGION);
			break;
		case E_CONTACT_CSV_ADDRESS_BUSINESS_POSTCODE:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_WORK);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_CODE);
			break;
		case E_CONTACT_CSV_ADDRESS_BUSINESS_COUNTRY:
			delivery_address = e_contact_get (contact, E_CONTACT_ADDRESS_WORK);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_COUNTRY);
			break;
		case E_CONTACT_CSV_BIRTH_DATE_YEAR:
			date = e_contact_get (contact, E_CONTACT_BIRTH_DATE);
			if (date) {
				field_value = g_strdup_printf ("%04d", date->year);
				e_contact_date_free (date);
			}
			else {
				field_value = g_strdup ("");
			}
			break;

		case E_CONTACT_CSV_BIRTH_DATE_MONTH:
			date = e_contact_get (contact, E_CONTACT_BIRTH_DATE);
			if (date) {
				field_value = g_strdup_printf ("%04d", date->month);
				e_contact_date_free (date);
			}
			else {
				field_value = g_strdup ("");
			}
			break;

		case E_CONTACT_CSV_BIRTH_DATE_DAY:
			date = e_contact_get (contact, E_CONTACT_BIRTH_DATE);
			if (date) {
				field_value = g_strdup_printf ("%04d", date->day);
				e_contact_date_free (date);
			}
			else {
				field_value = g_strdup ("");
			}
			break;

		default:
			field_value = g_strdup ("");
		}
	}

	/*checking to avoid the NULL pointer */
	if (field_value == NULL)
		field_value = g_strdup ("");

	quoted_field_value = escape_string (field_value);
	g_free (field_value);

	if (delivery_address)
		e_contact_address_free (delivery_address);

	return quoted_field_value;
}

gchar *
e_contact_csv_get_header_line (GSList *csv_all_fields)
{

	guint field_number;
	gint csv_field;
	gchar **field_name_array;
	gchar *header_line;

	gint loop_counter;

	field_number = g_slist_length (csv_all_fields);
	field_name_array = g_new0 (gchar *, field_number + 1);

	for (loop_counter = 0; loop_counter < field_number; loop_counter++) {
		csv_field = GPOINTER_TO_INT (g_slist_nth_data (csv_all_fields, loop_counter));
		*(field_name_array + loop_counter) = e_contact_csv_get_name (csv_field);
	}

	header_line = g_strjoinv (COMMA_SEPARATOR, field_name_array);

	for (loop_counter = 0; loop_counter < field_number; loop_counter++) {
		g_free (*(field_name_array + loop_counter));
	}
	g_free (field_name_array);

	return header_line;

}

gchar *
e_contact_to_csv (EContact *contact,
                  GSList *csv_all_fields)
{
	guint field_number;
	gint csv_field;
	gchar **field_value_array;
	gchar *aline;

	gint loop_counter;

	field_number = g_slist_length (csv_all_fields);
	field_value_array = g_new0 (gchar *, field_number + 1);

	for (loop_counter = 0; loop_counter < field_number; loop_counter++) {
		csv_field = GPOINTER_TO_INT (g_slist_nth_data (csv_all_fields, loop_counter));
		*(field_value_array + loop_counter) = e_contact_csv_get (contact, csv_field);
	}

	aline = g_strjoinv (COMMA_SEPARATOR, field_value_array);

	for (loop_counter = 0; loop_counter < field_number; loop_counter++) {
		g_free (*(field_value_array + loop_counter));
	}
	g_free (field_value_array);

	return aline;

}

gchar *
e_contact_get_csv (EContact *contact,
                   GSList *csv_all_fields)
{
	gchar *aline;
	GList *emails;
	guint n_emails;
	gchar *full_name;

	emails = e_contact_get_attributes (contact, E_CONTACT_EMAIL);
	n_emails = g_list_length (emails);
	full_name = e_contact_get (contact, E_CONTACT_FULL_NAME);
	if (n_emails > 4)
		g_warning ("%s: only 4 out of %i emails have been exported", full_name, n_emails);
	g_free (full_name);
	g_list_free_full (emails, (GDestroyNotify) e_vcard_attribute_free);

	aline = e_contact_to_csv (contact, csv_all_fields);
	return aline;
}

gchar *
check_null_pointer (gchar *orig)
{
	gchar *result;
	if (orig == NULL)
		result = g_strdup ("");
	else
		result = g_strdup (orig);
	return result;
}

gchar *
delivery_address_get_sub_field (const EContactAddress *address,
                                DeliveryAddressField sub_field)
{
	gchar *sub_field_value;
	gchar *str_temp, *str_temp_a;
	if (address != NULL) {
		switch (sub_field) {
		case DELIVERY_ADDRESS_STREET:
			str_temp_a = check_null_pointer (address->po);
			str_temp = check_null_pointer (address->street);
			sub_field_value = g_strdup_printf ("%s %s", str_temp_a, str_temp);
			g_free (str_temp);
			g_free (str_temp_a);
			break;
		case DELIVERY_ADDRESS_EXT:
			sub_field_value = check_null_pointer (address->ext);
			break;
		case DELIVERY_ADDRESS_LOCALITY:
			sub_field_value = check_null_pointer (address->locality);
			break;
		case DELIVERY_ADDRESS_REGION:
			sub_field_value = check_null_pointer (address->region);
			break;
		case DELIVERY_ADDRESS_CODE:
			sub_field_value = check_null_pointer (address->code);
			break;
		case DELIVERY_ADDRESS_COUNTRY:
			sub_field_value = check_null_pointer (address->country);
			break;
		default:
			sub_field_value = g_strdup ("");
		}
	} else {
		sub_field_value = g_strdup ("");
	}
	return sub_field_value;
}

gchar *
escape_string (gchar *orig)
{
	const guchar *p;
	gchar *dest;
	gchar *q;

	if (orig == NULL)
		return g_strdup ("\"\"");

	p = (guchar *) orig;
	/* Each source byte needs maximally two destination chars (\n), and the extra 2 is for the leading and trailing '"' */
	q = dest = g_malloc (strlen (orig) * 2 + 1 + 2);

	*q++ = '\"';
	while (*p)
	{
		switch (*p)
		{
		case '\n':
			*q++ = '\\';
			*q++ = 'n';
			break;
		case '\r':
			*q++ = '\\';
			*q++ = 'r';
			break;
		case '\\':
			*q++ = '\\';
			*q++ = '\\';
			break;
		case '"':
			*q++ = '"';
			*q++ = '"';
			break;
		default:
			*q++ = *p;
		}
		p++;
	}

	*q++ = '\"';
	*q = 0;

	return dest;
}

gint
output_n_cards_file (FILE *outputfile,
                     GSList *contacts,
                     gint size,
                     gint begin_no,
                     CARD_FORMAT format)
{
	gint i;
	if (format == CARD_FORMAT_VCARD) {
		for (i = begin_no; i < size + begin_no; i++) {
			EContact *contact = g_slist_nth_data (contacts, i);
			gchar *vcard = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);
			fprintf (outputfile, "%s\n", vcard);
			g_free (vcard);
		}
	} else if (format == CARD_FORMAT_CSV) {
		gchar *csv_fields_name;

		if (!pre_defined_fields)
			set_pre_defined_field (&pre_defined_fields);

		csv_fields_name = e_contact_csv_get_header_line (pre_defined_fields);
		fprintf (outputfile, "%s\n", csv_fields_name);
		g_free (csv_fields_name);

		for (i = begin_no; i < size + begin_no; i++) {
			EContact *contact = g_slist_nth_data (contacts, i);
			gchar *csv = e_contact_get_csv (contact, pre_defined_fields);
			fprintf (outputfile, "%s\n", csv);
			g_free (csv);
		}
	}

	return SUCCESS;

}

static void
action_list_cards (GSList *contacts,
                   ActionContext *p_actctx)
{
	FILE *outputfile;
	long length;
	CARD_FORMAT format;

	length = g_slist_length (contacts);

	if (length <= 0) {
		g_warning ("Couldn't load addressbook correctly!!!! %s####", p_actctx->addressbook_source_uid ?
				p_actctx->addressbook_source_uid : "NULL");
		exit (-1);
	}

	if (p_actctx->output_file == NULL) {
		outputfile = stdout;
	} else {
		/* fopen output file */
		if (!(outputfile = g_fopen (p_actctx->output_file, "w"))) {
			g_warning (_("Can not open file"));
			exit (-1);
		}
	}

	if (p_actctx->IsVCard == TRUE)
		format = CARD_FORMAT_VCARD;
	else
		format = CARD_FORMAT_CSV;

	output_n_cards_file (outputfile, contacts, length, 0, format);

	if (p_actctx->output_file != NULL) {
		fclose (outputfile);
	}
}

void
set_pre_defined_field (GSList **pre_defined_fields)
{
	*pre_defined_fields = NULL;
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_GIVEN_NAME));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_FAMILY_NAME));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_FULL_NAME));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_NICKNAME));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_EMAIL_1));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_EMAIL_2));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_EMAIL_3));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_EMAIL_4));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_WANTS_HTML));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_PHONE_BUSINESS));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_PHONE_HOME));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_PHONE_BUSINESS_FAX));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_PHONE_PAGER));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_PHONE_MOBILE));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_HOME_STREET));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_HOME_EXT));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_HOME_CITY));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_HOME_REGION));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_HOME_POSTCODE));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_HOME_COUNTRY));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_BUSINESS_STREET));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_BUSINESS_EXT));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_BUSINESS_CITY));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_BUSINESS_REGION));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_BUSINESS_POSTCODE));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ADDRESS_BUSINESS_COUNTRY));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_TITLE));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_OFFICE));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_ORG));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_HOMEPAGE_URL));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_CALENDAR_URI));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_BIRTH_DATE_YEAR));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_BIRTH_DATE_MONTH));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_BIRTH_DATE_DAY));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CONTACT_CSV_NOTE));
}

void
action_list_cards_init (ActionContext *p_actctx)
{
	ESourceRegistry *registry;
	EClient *client;
	EBookClient *book_client;
	EBookQuery *query;
	ESource *source;
	GSList *contacts;
	const gchar *uid;
	gchar *query_str;
	GError *error = NULL;

	registry = p_actctx->registry;
	uid = p_actctx->addressbook_source_uid;

	if (uid != NULL)
		source = e_source_registry_ref_source (registry, uid);
	else
		source = e_source_registry_ref_default_address_book (registry);

	client = e_book_client_connect_sync (source, 30, NULL, &error);

	g_object_unref (source);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		g_warning (
			"Couldn't load addressbook %s: %s",
			p_actctx->addressbook_source_uid ?
			p_actctx->addressbook_source_uid :
			"'default'", error->message);
		g_error_free (error);
		exit (-1);
	}

	book_client = E_BOOK_CLIENT (client);

	query = e_book_query_any_field_contains ("");
	query_str = e_book_query_to_string (query);
	e_book_query_unref (query);

	e_book_client_get_contacts_sync (
		book_client, query_str, &contacts, NULL, &error);

	action_list_cards (contacts, p_actctx);

	g_slist_foreach (contacts, (GFunc) g_object_unref, NULL);
	g_slist_free (contacts);
	g_object_unref (book_client);

	if (error != NULL) {
		g_warning ("Failed to get contacts: %s", error->message);
		g_error_free (error);
	}
}
