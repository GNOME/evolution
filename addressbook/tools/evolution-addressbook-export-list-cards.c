/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-addressbook-export-list-cards.c
 *
 * Copyright (C) 2003 Ximian, Inc.
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
 * Author: Gilbert Fang <gilbert.fang@sun.com>
 *
 */

#include <config.h>

#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libbonobo.h>
#include <libgnome/libgnome.h>

#include <ebook/e-book.h>
#include <ebook/e-card-simple.h>
#include <ebook/e-book-util.h>

#include "evolution-addressbook-export.h"

#define COMMA_SEPARATOR ","

typedef enum _CARD_FORMAT CARD_FORMAT;
typedef enum _DeliveryAddressField DeliveryAddressField;
typedef enum _ECardSimpleFieldCSV ECardSimpleFieldCSV;
typedef struct _ECardCSVFieldData ECardCSVFieldData;

enum _CARD_FORMAT
{
	CARD_FORMAT_CSV,
	CARD_FORMAT_VCARD
};

enum _DeliveryAddressField
{
	DELIVERY_ADDRESS_STREET,
	DELIVERY_ADDRESS_EXT,
	DELIVERY_ADDRESS_CITY,
	DELIVERY_ADDRESS_REGION,
	DELIVERY_ADDRESS_CODE,
	DELIVERY_ADDRESS_COUNTRY
};

enum _ECardSimpleFieldCSV
{
	E_CARD_SIMPLE_FIELD_CSV_FILE_AS,
	E_CARD_SIMPLE_FIELD_CSV_FULL_NAME,
	E_CARD_SIMPLE_FIELD_CSV_EMAIL,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_PRIMARY,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_ASSISTANT,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_BUSINESS,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_CALLBACK,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_COMPANY,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_HOME,
	E_CARD_SIMPLE_FIELD_CSV_ORG,
	/*E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS, */
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_STREET,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_EXT,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_CITY,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_REGION,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_POSTCODE,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_COUNTRY,
	/*E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME, */
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_STREET,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_EXT,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_CITY,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_REGION,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_POSTCODE,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_COUNTRY,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_MOBILE,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_CAR,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_BUSINESS_FAX,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_HOME_FAX,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_BUSINESS_2,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_HOME_2,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_ISDN,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_OTHER,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_OTHER_FAX,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_PAGER,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_RADIO,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_TELEX,
	E_CARD_SIMPLE_FIELD_CSV_PHONE_TTYTDD,
	/*E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER, */
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_STREET,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_EXT,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_CITY,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_REGION,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_POSTCODE,
	E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_COUNTRY,
	E_CARD_SIMPLE_FIELD_CSV_EMAIL_2,
	E_CARD_SIMPLE_FIELD_CSV_EMAIL_3,
	E_CARD_SIMPLE_FIELD_CSV_URL,
	E_CARD_SIMPLE_FIELD_CSV_ORG_UNIT,
	E_CARD_SIMPLE_FIELD_CSV_OFFICE,
	E_CARD_SIMPLE_FIELD_CSV_TITLE,
	E_CARD_SIMPLE_FIELD_CSV_ROLE,
	E_CARD_SIMPLE_FIELD_CSV_MANAGER,
	E_CARD_SIMPLE_FIELD_CSV_ASSISTANT,
	E_CARD_SIMPLE_FIELD_CSV_NICKNAME,
	E_CARD_SIMPLE_FIELD_CSV_SPOUSE,
	E_CARD_SIMPLE_FIELD_CSV_NOTE,
	E_CARD_SIMPLE_FIELD_CSV_CALURI,
	E_CARD_SIMPLE_FIELD_CSV_FBURL,
	/*E_CARD_SIMPLE_FIELD_CSV_ANNIVERSARY, */
	E_CARD_SIMPLE_FIELD_CSV_ANNIVERSARY_YEAR,
	E_CARD_SIMPLE_FIELD_CSV_ANNIVERSARY_MONTH,
	E_CARD_SIMPLE_FIELD_CSV_ANNIVERSARY_DAY,
	/*E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE, */
	E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_YEAR,
	E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_MONTH,
	E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_DAY,
	E_CARD_SIMPLE_FIELD_CSV_MAILER,
	E_CARD_SIMPLE_FIELD_CSV_NAME_OR_ORG,
	E_CARD_SIMPLE_FIELD_CSV_CATEGORIES,
	E_CARD_SIMPLE_FIELD_CSV_FAMILY_NAME,
	E_CARD_SIMPLE_FIELD_CSV_GIVEN_NAME,
	E_CARD_SIMPLE_FIELD_CSV_ADDITIONAL_NAME,
	E_CARD_SIMPLE_FIELD_CSV_NAME_SUFFIX,
	E_CARD_SIMPLE_FIELD_CSV_WANTS_HTML,
	E_CARD_SIMPLE_FIELD_CSV_IS_LIST,
	E_CARD_SIMPLE_FIELD_CSV_LAST
};

struct _ECardCSVFieldData
{
	gint csv_field;
	gint simple_field;
	gchar *csv_name;
};

#define NOMAP -1
static ECardCSVFieldData csv_field_data[] = {
	{E_CARD_SIMPLE_FIELD_CSV_FILE_AS, E_CARD_SIMPLE_FIELD_FILE_AS, ""},
	{E_CARD_SIMPLE_FIELD_CSV_FULL_NAME, E_CARD_SIMPLE_FIELD_CSV_FULL_NAME, ""},
	{E_CARD_SIMPLE_FIELD_CSV_EMAIL, E_CARD_SIMPLE_FIELD_EMAIL, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_PRIMARY, E_CARD_SIMPLE_FIELD_PHONE_PRIMARY,
	 ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_ASSISTANT,
	 E_CARD_SIMPLE_FIELD_PHONE_ASSISTANT, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_BUSINESS,
	 E_CARD_SIMPLE_FIELD_PHONE_BUSINESS, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_CALLBACK,
	 E_CARD_SIMPLE_FIELD_PHONE_CALLBACK, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_COMPANY, E_CARD_SIMPLE_FIELD_PHONE_COMPANY,
	 ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_HOME, E_CARD_SIMPLE_FIELD_PHONE_HOME, ""},
	{E_CARD_SIMPLE_FIELD_CSV_ORG, E_CARD_SIMPLE_FIELD_ORG, ""},
	/*E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS, */
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_STREET, NOMAP,
	 "Business Address"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_EXT, NOMAP,
	 "Business Address2"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_CITY, NOMAP,
	 "Business Address City"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_REGION, NOMAP,
	 "Business Address State"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_POSTCODE, NOMAP,
	 "Business Address PostCode"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_COUNTRY, NOMAP,
	 "Business Address Country"},
	/*E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME, */
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_STREET, NOMAP, "Home Address"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_EXT, NOMAP, "Home Address2"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_CITY, NOMAP, "Home Address City"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_REGION, NOMAP,
	 "Home Address State"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_POSTCODE, NOMAP,
	 "Home Address PostCode"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_COUNTRY, NOMAP,
	 "Home Address Country"},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_MOBILE, E_CARD_SIMPLE_FIELD_PHONE_MOBILE,
	 ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_CAR, E_CARD_SIMPLE_FIELD_PHONE_CAR, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_BUSINESS_FAX,
	 E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_FAX, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_HOME_FAX,
	 E_CARD_SIMPLE_FIELD_PHONE_HOME_FAX, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_BUSINESS_2,
	 E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_2, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_HOME_2, E_CARD_SIMPLE_FIELD_PHONE_HOME_2,
	 ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_ISDN, E_CARD_SIMPLE_FIELD_PHONE_ISDN, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_OTHER, E_CARD_SIMPLE_FIELD_PHONE_OTHER, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_OTHER_FAX,
	 E_CARD_SIMPLE_FIELD_PHONE_OTHER_FAX, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_PAGER, E_CARD_SIMPLE_FIELD_PHONE_PAGER, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_RADIO, E_CARD_SIMPLE_FIELD_PHONE_RADIO, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_TELEX, E_CARD_SIMPLE_FIELD_PHONE_TELEX, ""},
	{E_CARD_SIMPLE_FIELD_CSV_PHONE_TTYTDD, E_CARD_SIMPLE_FIELD_PHONE_TTYTDD,
	 ""},
	/*E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER, */
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_STREET, NOMAP, "Other Address"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_EXT, NOMAP, "Other Address2"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_CITY, NOMAP,
	 "Other Address City"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_REGION, NOMAP,
	 "Other Address State"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_POSTCODE, NOMAP,
	 "Other Address PostCode"},
	{E_CARD_SIMPLE_FIELD_CSV_ADDRESS_OTHER_COUNTRY, NOMAP,
	 "Other Address Country"},
	{E_CARD_SIMPLE_FIELD_CSV_EMAIL_2, E_CARD_SIMPLE_FIELD_EMAIL_2, ""},
	{E_CARD_SIMPLE_FIELD_CSV_EMAIL_3, E_CARD_SIMPLE_FIELD_EMAIL_3, ""},
	{E_CARD_SIMPLE_FIELD_CSV_URL, E_CARD_SIMPLE_FIELD_URL, ""},
	{E_CARD_SIMPLE_FIELD_CSV_ORG_UNIT, E_CARD_SIMPLE_FIELD_ORG_UNIT, ""},
	{E_CARD_SIMPLE_FIELD_CSV_OFFICE, E_CARD_SIMPLE_FIELD_OFFICE, ""},
	{E_CARD_SIMPLE_FIELD_CSV_TITLE, E_CARD_SIMPLE_FIELD_TITLE, ""},
	{E_CARD_SIMPLE_FIELD_CSV_ROLE, E_CARD_SIMPLE_FIELD_ROLE, ""},
	{E_CARD_SIMPLE_FIELD_CSV_MANAGER, E_CARD_SIMPLE_FIELD_MANAGER, ""},
	{E_CARD_SIMPLE_FIELD_CSV_ASSISTANT, E_CARD_SIMPLE_FIELD_ASSISTANT, ""},
	{E_CARD_SIMPLE_FIELD_CSV_NICKNAME, E_CARD_SIMPLE_FIELD_NICKNAME, ""},
	{E_CARD_SIMPLE_FIELD_CSV_SPOUSE, E_CARD_SIMPLE_FIELD_SPOUSE, ""},
	{E_CARD_SIMPLE_FIELD_CSV_NOTE, E_CARD_SIMPLE_FIELD_NOTE, ""},
	{E_CARD_SIMPLE_FIELD_CSV_CALURI, E_CARD_SIMPLE_FIELD_CALURI, ""},
	{E_CARD_SIMPLE_FIELD_CSV_FBURL, E_CARD_SIMPLE_FIELD_FBURL, ""},
	/*E_CARD_SIMPLE_FIELD_ANNIVERSARY, */
	{E_CARD_SIMPLE_FIELD_CSV_ANNIVERSARY_YEAR, NOMAP, "Anniversary Year"},
	{E_CARD_SIMPLE_FIELD_CSV_ANNIVERSARY_MONTH, NOMAP, "Anniversary Month"},
	{E_CARD_SIMPLE_FIELD_CSV_ANNIVERSARY_DAY, NOMAP, "Anniversary Day"},
	/*E_CARD_SIMPLE_FIELD_BIRTH_DATE, */
	{E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_YEAR, NOMAP, "Birth Year"},
	{E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_MONTH, NOMAP, "Birth Month"},
	{E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_DAY, NOMAP, "Birth Day"},
	{E_CARD_SIMPLE_FIELD_CSV_MAILER, E_CARD_SIMPLE_FIELD_MAILER, ""},
	{E_CARD_SIMPLE_FIELD_CSV_NAME_OR_ORG, E_CARD_SIMPLE_FIELD_NAME_OR_ORG, ""},
	{E_CARD_SIMPLE_FIELD_CSV_CATEGORIES, E_CARD_SIMPLE_FIELD_CATEGORIES, ""},
	{E_CARD_SIMPLE_FIELD_CSV_FAMILY_NAME, E_CARD_SIMPLE_FIELD_FAMILY_NAME, ""},
	{E_CARD_SIMPLE_FIELD_CSV_GIVEN_NAME, E_CARD_SIMPLE_FIELD_GIVEN_NAME, ""},
	{E_CARD_SIMPLE_FIELD_CSV_ADDITIONAL_NAME,
	 E_CARD_SIMPLE_FIELD_ADDITIONAL_NAME, ""},
	{E_CARD_SIMPLE_FIELD_CSV_NAME_SUFFIX, E_CARD_SIMPLE_FIELD_NAME_SUFFIX, ""},
	{E_CARD_SIMPLE_FIELD_CSV_WANTS_HTML, E_CARD_SIMPLE_FIELD_WANTS_HTML, ""},
	{E_CARD_SIMPLE_FIELD_CSV_IS_LIST, E_CARD_SIMPLE_FIELD_IS_LIST, ""},
	{E_CARD_SIMPLE_FIELD_CSV_LAST, NOMAP, ""}

};

static GSList *pre_defined_fields;

/*function prototypes*/
gint e_card_simple_csv_get_simple_field (ECardSimpleFieldCSV csv_field);
gchar *e_card_simple_csv_get_name (ECardSimpleFieldCSV csv_field);
gchar *e_card_simple_csv_get (ECardSimple * simple, ECardSimpleFieldCSV csv_field);
gchar *e_card_simple_csv_get_header_line (GSList * csv_all_fields);
gchar *e_card_simple_to_csv (ECardSimple * simple, GSList * csv_all_fields);
gchar *e_card_get_csv (ECard * card, GSList * csv_all_fields);
gchar *delivery_address_get_sub_field (const ECardDeliveryAddress * delivery_address, DeliveryAddressField sub_field);
gchar *check_null_pointer (gchar * orig);
gchar *quote_string (gchar * orig);
int output_n_cards_file (FILE * outputfile, ECardCursor * cursor, int size, int begin_no, CARD_FORMAT format);
static void fork_to_background (void);
static void action_list_cards_get_cursor_cb (EBook * book, EBookStatus status, ECardCursor * cursor, ActionContext * p_actctx);
static void action_list_cards_open_cb (EBook * book, EBookStatus status, ActionContext * p_actctx);
static guint action_list_cards_run (ActionContext * p_actctx);
void set_pre_defined_field (GSList ** pre_defined_fields);
guint action_list_cards_init (ActionContext * p_actctx);


/* function declarations*/
gint
e_card_simple_csv_get_simple_field (ECardSimpleFieldCSV csv_field)
{
	return csv_field_data[csv_field].simple_field;
}

gchar *
e_card_simple_csv_get_name (ECardSimpleFieldCSV csv_field)
{
	gint simple_field;
	gchar *name;
	gchar *esc_name;
	gchar *quoted_name;

	ECardSimple *a_simple_card;

	simple_field = e_card_simple_csv_get_simple_field (csv_field);

	if (simple_field != NOMAP) {
		a_simple_card = E_CARD_SIMPLE (g_object_new (E_TYPE_CARD_SIMPLE, NULL));
		name = g_strdup (e_card_simple_get_ecard_field (a_simple_card, simple_field));
		g_object_unref (G_OBJECT (a_simple_card));
	} else {
		name = g_strdup (csv_field_data[csv_field].csv_name);
	}
	esc_name = g_strescape (name, NULL);
	g_free (name);
	quoted_name = quote_string (esc_name);
	g_free (esc_name);
	return quoted_name;
}


gchar *
e_card_simple_csv_get (ECardSimple * simple, ECardSimpleFieldCSV csv_field)
{
	gint simple_field;
	gchar *field_value;
	gchar *esc_field_value;
	gchar *quoted_field_value;

	const ECardDeliveryAddress *delivery_address = NULL;

	simple_field = e_card_simple_csv_get_simple_field (csv_field);

	if (simple_field != NOMAP) {
		field_value = e_card_simple_get (simple, simple_field);
	} else {

		switch (csv_field) {
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_STREET:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_STREET);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_EXT:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_EXT);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_CITY:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_CITY);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_REGION:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_REGION);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_POSTCODE:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_CODE);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_COUNTRY:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_HOME);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_COUNTRY);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_STREET:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_BUSINESS);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_STREET);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_EXT:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_BUSINESS);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_EXT);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_CITY:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_BUSINESS);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_CITY);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_REGION:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_BUSINESS);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_REGION);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_POSTCODE:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_BUSINESS);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_CODE);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_COUNTRY:
			delivery_address = e_card_simple_get_delivery_address (simple, E_CARD_SIMPLE_ADDRESS_ID_BUSINESS);
			field_value = delivery_address_get_sub_field (delivery_address, DELIVERY_ADDRESS_COUNTRY);
			break;
		case E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_YEAR:
			if (simple->card->bday != NULL) {
				field_value = g_strdup_printf ("%04d", simple->card->bday->year);
			} else {
				field_value = g_strdup ("");
			}
			break;

		case E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_MONTH:
			if (simple->card->bday != NULL) {
				field_value = g_strdup_printf ("%02d", simple->card->bday->month);
			} else {
				field_value = g_strdup ("");
			}
			break;

		case E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_DAY:
			if (simple->card->bday != NULL) {
				field_value = g_strdup_printf ("%02d", simple->card->bday->day);
			} else {
				field_value = g_strdup ("");
			}
			break;

		default:
			field_value = g_strdup ("");
		}
	}

	/*checking to avoid the NULL pointer */
	if (field_value == NULL)
		field_value =  g_strdup ("");

	esc_field_value = g_strescape (field_value, NULL);
	g_free (field_value);

	quoted_field_value = quote_string (esc_field_value);
	g_free (esc_field_value);

	return quoted_field_value;
}


gchar *
e_card_simple_csv_get_header_line (GSList * csv_all_fields)
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
		*(field_name_array + loop_counter) = e_card_simple_csv_get_name (csv_field);
	}

	header_line = g_strjoinv (COMMA_SEPARATOR, field_name_array);

	for (loop_counter = 0; loop_counter < field_number; loop_counter++) {
		g_free (*(field_name_array + loop_counter));
	}
	g_free (field_name_array);

	return header_line;

}


gchar *
e_card_simple_to_csv (ECardSimple * simple, GSList * csv_all_fields)
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
		*(field_value_array + loop_counter) = e_card_simple_csv_get (simple, csv_field);
	}

	aline = g_strjoinv (COMMA_SEPARATOR, field_value_array);

	for (loop_counter = 0; loop_counter < field_number; loop_counter++) {
		g_free (*(field_value_array + loop_counter));
	}
	g_free (field_value_array);

	return aline;

}


gchar *
e_card_get_csv (ECard * card, GSList * csv_all_fields)
{
	ECardSimple *simple_card;
	gchar *aline;

	simple_card = e_card_simple_new (card);
	aline = e_card_simple_to_csv (simple_card, csv_all_fields);
	g_object_unref (G_OBJECT (simple_card));
	return aline;
}


gchar *
check_null_pointer (gchar * orig)
{
	gchar *result;
	if (orig == NULL)
		result = g_strdup ("");
	else
		result = g_strdup (orig);
	return result;
}

gchar *delivery_address_get_sub_field (const ECardDeliveryAddress * address, DeliveryAddressField sub_field)
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
		case DELIVERY_ADDRESS_CITY:
			sub_field_value = check_null_pointer (address->city);
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
quote_string (gchar *orig)
{
	if (orig == NULL)
		return g_strdup ("\"\"");
	return g_strdup_printf("\"%s\"", orig);
}

int
output_n_cards_file (FILE * outputfile, ECardCursor * cursor, int size, int begin_no, CARD_FORMAT format)
{
	int i;
	if (format == CARD_FORMAT_VCARD) {
		for (i = begin_no; i < size + begin_no; i++) {
			ECard *card = e_card_cursor_get_nth (cursor, i);
			gchar *vcard = e_card_get_vcard_assume_utf8 (card);
			fprintf (outputfile, "%s\n", vcard);
			g_free (vcard);
			g_object_unref (G_OBJECT (card));
		}
	} else if (format == CARD_FORMAT_CSV) {
		gchar *csv_fields_name = e_card_simple_csv_get_header_line (pre_defined_fields);
		fprintf (outputfile, "%s\n", csv_fields_name);
		g_free (csv_fields_name);

		for (i = begin_no; i < size + begin_no; i++) {
			ECard *card = e_card_cursor_get_nth (cursor, i);
			gchar *csv = e_card_get_csv (card, pre_defined_fields);
			fprintf (outputfile, "%s\n", csv);
			g_free (csv);
			g_object_unref (G_OBJECT (card));

		}
	}

	return SUCCESS;

}

static void
fork_to_background (void)
{
	pid_t pid;
	pid = fork ();
	if (pid == -1) {
		/* ouch, fork() failed */
		perror ("fork");
		exit (-1);
	} else if (pid == 0) {
		/* child */
		/*contunue */

	} else {
		/* parent exit,  note the use of _exit() instead of exit() */
		_exit (-1);
	}
}




static void
action_list_cards_get_cursor_cb (EBook * book, EBookStatus status, ECardCursor * cursor, ActionContext * p_actctx)
{
	FILE *outputfile;
	long length;
	int IsFirstOne;
	int series_no;
	gchar *file_series_name;
	CARD_FORMAT format;
	int size;

	length = e_card_cursor_get_length (cursor);

	if (length <= 0) {
		g_warning ("Couldn't load addressbook correctly!!!! %s####", p_actctx->action_list_cards.addressbook_folder_uri);
		exit (-1);
	}


	if (p_actctx->action_list_cards.async_mode == FALSE) {	/* normal mode */

		if (p_actctx->action_list_cards.output_file == NULL) {
			outputfile = stdout;
		} else {
			/* fopen output file */
			if (!(outputfile = fopen (p_actctx->action_list_cards.output_file, "w"))) {
				g_warning (_("Can not open file"));
				exit (-1);
			}
		}

		if (p_actctx->action_list_cards.IsVCard == TRUE)
			format = CARD_FORMAT_VCARD;
		else
			format = CARD_FORMAT_CSV;

		output_n_cards_file (outputfile, cursor, length, 0, format);



		if (p_actctx->action_list_cards.output_file != NULL) {
			fclose (outputfile);
		}
	}


	/*async mode */
	else {

		size = p_actctx->action_list_cards.file_size;
		IsFirstOne = TRUE;
		series_no = 0;

		do {
			/* whether it is the last file */
			if ((series_no + 1) * size >= length) {	/*last one */
				file_series_name = g_strdup_printf ("%s.end", p_actctx->action_list_cards.output_file);

			} else {	/*next one */
				file_series_name =
					g_strdup_printf ("%s.%04d", p_actctx->action_list_cards.output_file, series_no);
			}

			if (!(outputfile = fopen (file_series_name, "w"))) {
				g_warning (_("Can not open file"));
				exit (-1);
			}


			if (p_actctx->action_list_cards.IsVCard == TRUE)
				format = CARD_FORMAT_VCARD;
			else
				format = CARD_FORMAT_CSV;
			output_n_cards_file (outputfile, cursor, size, series_no * size, format);

			fclose (outputfile);

			series_no++;

			if (IsFirstOne == TRUE) {
				fork_to_background ();
				IsFirstOne = FALSE;
			}


		}
		while (series_no * size < length);
		g_free (file_series_name);
	}

	bonobo_main_quit ();
}




static void
action_list_cards_open_cb (EBook * book, EBookStatus status, ActionContext * p_actctx)
{
	if (status != E_BOOK_STATUS_SUCCESS) {
		g_warning ("Couldn't load addressbook %s", p_actctx->action_list_cards.addressbook_folder_uri);
		exit (-1);
	}
	e_book_get_cursor (book, "(contains \"full_name\" \"\")", (EBookCursorCallback)action_list_cards_get_cursor_cb, p_actctx);
}


static guint
action_list_cards_run (ActionContext * p_actctx)
{
	EBook *book;
	book = e_book_new ();
	
	if (p_actctx->action_list_cards.addressbook_folder_uri != NULL) {
		e_book_load_uri (book, p_actctx->action_list_cards.addressbook_folder_uri,
				 (EBookCallback)action_list_cards_open_cb, p_actctx);
	} else {
		e_book_load_default_book (book, (EBookCallback)action_list_cards_open_cb, p_actctx);
	}
	return SUCCESS;
}


void
set_pre_defined_field (GSList ** pre_defined_fields)
{
	*pre_defined_fields = NULL;
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_GIVEN_NAME));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_FAMILY_NAME));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_FULL_NAME));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_NICKNAME));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_EMAIL));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_EMAIL_2));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_WANTS_HTML));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_PHONE_BUSINESS));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_PHONE_HOME));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_PHONE_BUSINESS_FAX));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_PHONE_PAGER));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_PHONE_MOBILE));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_STREET));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_EXT));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_CITY));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_REGION));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_POSTCODE));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_HOME_COUNTRY));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_STREET));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_EXT));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_CITY));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_REGION));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_POSTCODE));
	*pre_defined_fields =
		g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ADDRESS_BUSINESS_COUNTRY));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_TITLE));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_OFFICE));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_ORG));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_URL));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_CALURI));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_YEAR));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_MONTH));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_BIRTH_DATE_DAY));
	*pre_defined_fields = g_slist_append (*pre_defined_fields, GINT_TO_POINTER (E_CARD_SIMPLE_FIELD_CSV_NOTE));
}

guint
action_list_cards_init (ActionContext * p_actctx)
{
	g_idle_add ((GSourceFunc) action_list_cards_run, p_actctx);
	set_pre_defined_field (&pre_defined_fields);

	return SUCCESS;
}
