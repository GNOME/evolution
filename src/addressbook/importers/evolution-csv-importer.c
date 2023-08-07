/*
 * Evolution CSV and TAB importer
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
 *
 * Authors:
 *		Devashish Sharma <sdevashish@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libebook/libebook.h>

#include <shell/e-shell.h>

#include "evolution-addressbook-importers.h"

#define NOMAP -1
#define EVOLUTION_IMPORTER 3
#define MOZILLA_IMPORTER 2
#define OUTLOOK_IMPORTER 1
#define CSV_FILE_DELIMITER ','
#define TAB_FILE_DELIMITER '\t'

#define E_CONTACT_ADDITIONAL_NAME (E_CONTACT_FIELD_LAST + 10)
#define E_CONTACT_SUFFIXES_NAME   (E_CONTACT_FIELD_LAST + 11)
#define E_CONTACT_PREFIXES_NAME   (E_CONTACT_FIELD_LAST + 12)

typedef struct {
	EImport *import;
	EImportTarget *target;

	guint idle_id;

	gint state;
	FILE *file;
	gulong size;
	gint count;

	/* gint -> gint -- Column index in the CSV
	 * file to an index in the known fields array. */
	GHashTable *fields_map;

	EBookClient *book_client;
	GSList *contacts;
} CSVImporter;

static gint importer;
static gchar delimiter;

static void csv_import_done (CSVImporter *gci);

typedef struct {
	const gchar *csv_attribute;
	EContactField contact_field;
#define FLAG_HOME_ADDRESS  0x01
#define FLAG_WORK_ADDRESS  0x02
#define FLAG_OTHER_ADDRESS 0x04
#define FLAG_STREET        0x08
#define FLAG_CITY          0x10
#define FLAG_STATE	   0x20
#define FLAG_POSTAL_CODE   0x40
#define FLAG_COUNTRY       0x80
#define FLAG_POBOX         0x70
#define FLAG_DATE_BDAY     0x03
#define FLAG_BIRTH_DAY	   0x05
#define FLAG_BIRTH_YEAR    0x07
#define FLAG_BIRTH_MONTH   0x50
#define FLAG_DATE_ANNIVERSARY 0x30
#define FLAG_INVALID       0xff
	gint flags;
}import_fields;

static import_fields csv_fields_outlook[] = {
	{"Title", E_CONTACT_PREFIXES_NAME},
	{"First Name", E_CONTACT_GIVEN_NAME},
	{"Middle Name", E_CONTACT_ADDITIONAL_NAME},
	{"Last Name", E_CONTACT_FAMILY_NAME},
	{"Suffix", E_CONTACT_SUFFIXES_NAME},
	{"Company", E_CONTACT_ORG},
	{"Department", E_CONTACT_ORG_UNIT},
	{"Job Title", E_CONTACT_TITLE},
	{"Business Street", NOMAP, FLAG_WORK_ADDRESS | FLAG_STREET },
	{"Business Street 2", NOMAP, FLAG_WORK_ADDRESS | FLAG_STREET },
	{"Business Street 3", NOMAP, FLAG_WORK_ADDRESS | FLAG_STREET},
	{"Business City", NOMAP, FLAG_WORK_ADDRESS | FLAG_CITY},
	{"Business State", NOMAP, FLAG_WORK_ADDRESS | FLAG_STATE},
	{"Business Postal Code", NOMAP, FLAG_WORK_ADDRESS | FLAG_POSTAL_CODE},
	{"Business Country", NOMAP, FLAG_WORK_ADDRESS | FLAG_COUNTRY},
	{"Home Street", NOMAP, FLAG_HOME_ADDRESS | FLAG_STREET},
	{"Home Street 2", NOMAP, FLAG_HOME_ADDRESS | FLAG_STREET},
	{"Home Street 3", NOMAP, FLAG_HOME_ADDRESS | FLAG_STREET},
	{"Home City", NOMAP, FLAG_HOME_ADDRESS | FLAG_CITY},
	{"Home State", NOMAP, FLAG_HOME_ADDRESS | FLAG_STATE},
	{"Home Postal Code", NOMAP,FLAG_HOME_ADDRESS | FLAG_POSTAL_CODE},
	{"Home Country", NOMAP, FLAG_HOME_ADDRESS | FLAG_COUNTRY},
	{"Other Street", NOMAP, FLAG_OTHER_ADDRESS | FLAG_STREET},
	{"Other Street 2", NOMAP, FLAG_OTHER_ADDRESS | FLAG_STREET},
	{"Other Street 3", NOMAP, FLAG_OTHER_ADDRESS | FLAG_STREET},
	{"Other City", NOMAP, FLAG_OTHER_ADDRESS | FLAG_CITY},
	{"Other State", NOMAP, FLAG_OTHER_ADDRESS | FLAG_STATE},
	{"Other Postal Code", NOMAP, FLAG_OTHER_ADDRESS | FLAG_POSTAL_CODE},
	{"Other Country", NOMAP, FLAG_OTHER_ADDRESS | FLAG_COUNTRY},
	{"Assistant's Phone", E_CONTACT_PHONE_ASSISTANT},
	{"Business Fax", E_CONTACT_PHONE_BUSINESS_FAX},
	{"Business Phone", E_CONTACT_PHONE_BUSINESS},
	{"Business Phone 2", E_CONTACT_PHONE_BUSINESS_2},
	{"Callback", E_CONTACT_PHONE_CALLBACK},
	{"Car Phone", E_CONTACT_PHONE_CAR},
	{"Company Main Phone", E_CONTACT_PHONE_COMPANY},
	{"Home Fax", E_CONTACT_PHONE_HOME_FAX},
	{"Home Phone", E_CONTACT_PHONE_HOME},
	{"Home Phone 2", E_CONTACT_PHONE_HOME_2},
	{"ISDN", E_CONTACT_PHONE_ISDN},
	{"Mobile Phone", E_CONTACT_PHONE_MOBILE},
	{"Other Fax", E_CONTACT_PHONE_OTHER_FAX},
	{"Other Phone", E_CONTACT_PHONE_OTHER},
	{"Pager", E_CONTACT_PHONE_PAGER},
	{"Primary Phone", E_CONTACT_PHONE_PRIMARY},
	{"Radio Phone", E_CONTACT_PHONE_RADIO},
	{"TTY/TDD Phone", E_CONTACT_PHONE_TTYTDD},
	{"Telex", E_CONTACT_PHONE_TELEX},
	{"Account", NOMAP},
	{"Anniversary", NOMAP, FLAG_DATE_ANNIVERSARY},
	{"Assistant's Name", E_CONTACT_ASSISTANT},
	{"Billing Information", NOMAP},
	{"Birthday", NOMAP, FLAG_DATE_BDAY},
	{"Business Address PO Box", NOMAP, FLAG_WORK_ADDRESS | FLAG_POBOX},
	{"Categories", E_CONTACT_CATEGORIES},
	{"Children", NOMAP},
	{"Directory Server", NOMAP},
	{"E-mail Address", E_CONTACT_EMAIL_1},
	{"E-mail Type", NOMAP},
	{"E-mail Display Name", NOMAP},
	{"E-mail 2 Address", E_CONTACT_EMAIL_2},
	{"E-mail 2 Type", NOMAP},
	{"E-mail 2 Display Name", NOMAP},
	{"E-mail 3 Address", E_CONTACT_EMAIL_3},
	{"E-mail 3 Type", NOMAP},
	{"E-mail 3 Display Name", NOMAP},
	{"Gender", NOMAP},
	{"Government ID Number", NOMAP},
	{"Hobby", NOMAP},
	{"Home Address PO Box", NOMAP, FLAG_HOME_ADDRESS | FLAG_POBOX},
	{"Initials", NOMAP},
	{"Internet FREE/BUSY", E_CONTACT_FREEBUSY_URL},
	{"Keywords", NOMAP},
	{"Language", NOMAP},
	{"Location", NOMAP},
	{"Managers Name", E_CONTACT_MANAGER},
	{"Mileage", NOMAP},
	{"Nickname", E_CONTACT_NICKNAME},
	{"Notes", E_CONTACT_NOTE},
	{"Office Location", NOMAP},
	{"Organizational ID Number", NOMAP},
	{"Other Address PO Box", NOMAP, FLAG_OTHER_ADDRESS | FLAG_POBOX},
	{"Personal Web Page", NOMAP},
	{"Priority", NOMAP},
	{"Private", NOMAP},
	{"Profession", NOMAP},
	{"Referred By", NOMAP},
	{"Senstivity", NOMAP},
	{"Spouse", E_CONTACT_SPOUSE},
	{"User 1", NOMAP},
	{"User 2", NOMAP},
	{"User 3", NOMAP},
	{"User 4", NOMAP},
	{"Web Page", E_CONTACT_HOMEPAGE_URL},
};

static import_fields csv_fields_mozilla[] = {
	{"First Name", E_CONTACT_GIVEN_NAME},
	{"Last Name", E_CONTACT_FAMILY_NAME},
	{"Display Name", NOMAP},
	{"NickName", E_CONTACT_NICKNAME},
	{"E-mail Address", E_CONTACT_EMAIL_1},
	{"E-mail 2 Address", E_CONTACT_EMAIL_2},
	{"Business Phone", E_CONTACT_PHONE_BUSINESS},
	{"Home Phone", E_CONTACT_PHONE_HOME},
	{"Business Fax", E_CONTACT_PHONE_BUSINESS_FAX},
	{"Pager", E_CONTACT_PHONE_PAGER},
	{"Mobile Phone", E_CONTACT_PHONE_MOBILE},
	{"Home Street", NOMAP, FLAG_HOME_ADDRESS | FLAG_STREET},
	{"Home Street 2", NOMAP, FLAG_HOME_ADDRESS | FLAG_STREET},
	{"Home City", NOMAP, FLAG_HOME_ADDRESS | FLAG_CITY},
	{"Home State", NOMAP, FLAG_HOME_ADDRESS | FLAG_STATE},
	{"Home Postal Code", NOMAP,FLAG_HOME_ADDRESS | FLAG_POSTAL_CODE},
	{"Home Country", NOMAP, FLAG_HOME_ADDRESS | FLAG_COUNTRY},
	{"Business Street", NOMAP, FLAG_WORK_ADDRESS | FLAG_STREET },
	{"Business Street 2", NOMAP, FLAG_WORK_ADDRESS | FLAG_STREET },
	{"Business City", NOMAP, FLAG_WORK_ADDRESS | FLAG_CITY},
	{"Business State", NOMAP, FLAG_WORK_ADDRESS | FLAG_STATE},
	{"Business Postal Code", NOMAP, FLAG_WORK_ADDRESS | FLAG_POSTAL_CODE},
	{"Business Country", NOMAP, FLAG_WORK_ADDRESS | FLAG_COUNTRY},
	{"Job Title", E_CONTACT_TITLE},
	{"Department", E_CONTACT_ORG_UNIT},
	{"Company", E_CONTACT_ORG},
	{"Web Page", E_CONTACT_HOMEPAGE_URL},
	{"Home Web Page", NOMAP},
	{"Birth Year", NOMAP, FLAG_BIRTH_YEAR},
	{"Birth Month", NOMAP,FLAG_BIRTH_MONTH},
	{"Birth Day", NOMAP, FLAG_BIRTH_DAY},
	{"Custom 1", NOMAP},
	{"Custom 2", NOMAP},
	{"Custom 3", NOMAP},
	{"Custom 4", NOMAP},
	{"Notes", E_CONTACT_NOTE},

};

static import_fields csv_fields_evolution[] = {
	{"First Name", E_CONTACT_GIVEN_NAME},
	{"Last Name", E_CONTACT_FAMILY_NAME},
	{"id", NOMAP, FLAG_INVALID},
	{"NickName", E_CONTACT_NICKNAME},
	{"E-mail Address", E_CONTACT_EMAIL_1},
	{"E-mail 2 Address", E_CONTACT_EMAIL_2},
	{"E-mail 3 Address", E_CONTACT_EMAIL_3},
	{"E-mail 4 Address", E_CONTACT_EMAIL_4},
	{"Wants HTML", E_CONTACT_WANTS_HTML},
	{"Business Phone", E_CONTACT_PHONE_BUSINESS},
	{"Home Phone", E_CONTACT_PHONE_HOME},
	{"Business Fax", E_CONTACT_PHONE_BUSINESS_FAX},
	{"Pager", E_CONTACT_PHONE_PAGER},
	{"Mobile Phone", E_CONTACT_PHONE_MOBILE},
	{"Home Street", NOMAP, FLAG_HOME_ADDRESS | FLAG_STREET},
	{"Home Street 2", NOMAP, FLAG_INVALID},
	{"Home City", NOMAP, FLAG_HOME_ADDRESS | FLAG_CITY},
	{"Home State", NOMAP, FLAG_HOME_ADDRESS | FLAG_STATE},
	{"Home Postal Code", NOMAP,FLAG_HOME_ADDRESS | FLAG_POSTAL_CODE},
	{"Home Country", NOMAP, FLAG_HOME_ADDRESS | FLAG_COUNTRY},
	{"Business Street", NOMAP, FLAG_WORK_ADDRESS | FLAG_STREET },
	{"Business Street 2", NOMAP, FLAG_INVALID },
	{"Business City", NOMAP, FLAG_WORK_ADDRESS | FLAG_CITY},
	{"Business State", NOMAP, FLAG_WORK_ADDRESS | FLAG_STATE},
	{"Business Postal Code", NOMAP, FLAG_WORK_ADDRESS | FLAG_POSTAL_CODE},
	{"Business Country", NOMAP, FLAG_WORK_ADDRESS | FLAG_COUNTRY},
	{"Job Title", E_CONTACT_TITLE},
	{"Office", E_CONTACT_OFFICE},
	{"Company", E_CONTACT_ORG},
	{"Web Page", E_CONTACT_HOMEPAGE_URL},
	{"Cal uri", E_CONTACT_CALENDAR_URI},
	{"Birth Year", NOMAP, FLAG_BIRTH_YEAR},
	{"Birth Month", NOMAP,FLAG_BIRTH_MONTH},
	{"Birth Day", NOMAP, FLAG_BIRTH_DAY},
	{"Notes", E_CONTACT_NOTE},
};

static void
add_to_notes (EContact *contact,
              const gchar *field_text,
              gchar *val)
{
	GString *new_text;

	if (!val || !*val)
		return;

	new_text = g_string_new (e_contact_get_const (contact, E_CONTACT_NOTE));
	if (strlen (new_text->str) != 0)
		g_string_append_c (new_text, '\n');
	if (field_text) {
		g_string_append (new_text, field_text);
		g_string_append_c (new_text, ':');
	}
	g_string_append (new_text, val);

	e_contact_set (contact, E_CONTACT_NOTE, new_text->str);
	g_string_free (new_text, TRUE);
}

/* @str: a date string in the format MM-DD-YYYY, MMDDYYYY or YYYY-MM-DD
 * with MM and DD in the first two cases having possibly a leading zero, or
 * being one digit data.  Note, that e_contact_date_from_string parses
 * YYYY-MM-DD and YYYYMMDD, followed by 'T' or '\0'.
 */
static EContactDate *
date_from_string (const gchar *str)
{
	EContactDate * date;
	gint i = 0;

	g_return_val_if_fail (str != NULL, NULL);

	date = e_contact_date_new ();

	if (strlen (str) == 10 && str[4] == '-') { /* YYYY-MM-DD */
		date->year = str[0] * 1000 + str[1] * 100 + str[2] * 10 + str[3] - '0' * 1111;
		date->month = str[5] * 10 + str[6] - '0' * 11;
		date->day = str[8] * 10 + str[9] - '0' * 11;
		/* If the year is not set in the web interface, outlook.com exports 1604 */
		if (date->year == 1604)
			date->year = 1;
		return date;
	}

	if (g_ascii_isdigit (str[i]) && g_ascii_isdigit (str[i + 1])) {
		date->month = str[i] * 10 + str[i + 1] - '0' * 11;
		i = i + 3;
	}
	else {
		date->month = str[i] - '0' * 1;
		i = i + 2;
	}

	if (g_ascii_isdigit (str[i]) && g_ascii_isdigit (str[i + 1])) {
		date->day = str[i] * 10 + str[i + 1] - '0' * 11;
		i = i + 3;
	}
	else {
		date->day = str[i] - '0' * 1;
		i = i + 2;
	}
	date->year = str[i] * 1000 + str[i + 1] * 100 +
		str[i + 2] * 10 + str[i + 3] - '0' * 1111;

	return date;
}

static GString *
parseNextValue (const gchar **pptr)
{
	GString *value;
	const gchar *ptr = *pptr;

	g_return_val_if_fail (pptr != NULL, NULL);
	g_return_val_if_fail (*pptr != NULL, NULL);

	if (!*ptr || *ptr == '\n')
		return NULL;

	value = g_string_new ("");

	while (*ptr != delimiter) {
		if (*ptr == '\n')
			break;
		if (*ptr != '"') {
			g_string_append_unichar (value, g_utf8_get_char (ptr));
		} else {
			ptr = g_utf8_next_char (ptr);
			while (*ptr && *ptr != '"') {
				g_string_append_unichar (value, g_utf8_get_char (ptr));
				ptr = g_utf8_next_char (ptr);
			}

			if (!*ptr)
				break;
		}

		ptr = g_utf8_next_char (ptr);
	}

	if (*ptr != 0 && *ptr != '\n')
		ptr = g_utf8_next_char (ptr);

	*pptr = ptr;

	return value;
}

static GHashTable *
map_fields (const gchar *header_line,
            gint pimporter)
{
	import_fields *fields_array = NULL;
	gint n_fields = -1, idx, j;
	GString *value;
	GHashTable *fmap;
	const gchar *pptr = header_line;
	gboolean any_found = FALSE;

	if (pimporter == OUTLOOK_IMPORTER) {
		fields_array = csv_fields_outlook;
		n_fields = G_N_ELEMENTS (csv_fields_outlook);
	} else if (pimporter == EVOLUTION_IMPORTER) {
		fields_array = csv_fields_evolution;
		n_fields = G_N_ELEMENTS (csv_fields_evolution);
	}

	g_return_val_if_fail (fields_array != NULL, NULL);
	g_return_val_if_fail (n_fields > 0, NULL);

	fmap = g_hash_table_new (g_direct_hash, g_direct_equal);
	idx = 0;
	while (value = parseNextValue (&pptr), value != NULL) {
		for (j = 0; j < n_fields; j++) {
			if (g_ascii_strcasecmp (fields_array[j].csv_attribute, value->str) == 0) {
				g_hash_table_insert (
					fmap, GINT_TO_POINTER (idx),
					GINT_TO_POINTER (j + 1));
				any_found = TRUE;
				break;
			}
		}

		if (j >= n_fields)
			g_hash_table_insert (fmap, GINT_TO_POINTER (idx), GINT_TO_POINTER (-1));

		g_string_free (value, TRUE);
		idx++;
	}

	if (!any_found) {
		/* column names not in English? */
		g_hash_table_destroy (fmap);
		fmap = NULL;
	} else {
		/* also add last index, to be always skipped */
		g_hash_table_insert (fmap, GINT_TO_POINTER (idx), GINT_TO_POINTER (-1));
	}

	return fmap;
}

static gboolean
parseLine (CSVImporter *gci,
           EContact *contact,
           gchar *buf)
{
	const gchar *pptr = buf, *field_text;
	gchar *do_free = NULL;
	GString *value;
	gint ii = 0, idx;
	gint flags = 0;
	gint contact_field;
	EContactAddress *home_address, *work_address, *other_address;
	EContactDate *bday = NULL;
	GString *home_street, *work_street, *other_street;
	home_street = g_string_new ("");
	work_street = g_string_new ("");
	other_street = g_string_new ("");
	home_address = e_contact_address_new ();
	work_address = e_contact_address_new ();
	other_address = e_contact_address_new ();
	bday = e_contact_date_new ();

	if (!g_utf8_validate (pptr, -1, NULL)) {
		do_free = g_convert (pptr, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
		pptr = do_free;
	}

	while (value = parseNextValue (&pptr), value != NULL) {
		contact_field = NOMAP;
		flags = FLAG_INVALID;
		field_text = NULL;

		idx = ii;
		if (gci->fields_map) {
			gpointer found;

			found = g_hash_table_lookup (
				gci->fields_map, GINT_TO_POINTER (idx));

			if (found == NULL) {
				g_warning ("%s: No map for index %d, skipping it", G_STRFUNC, idx);
				idx = -1;
			} else {
				idx = GPOINTER_TO_INT (found) - 1;
			}
		}

		if (importer == OUTLOOK_IMPORTER) {
			if (idx >= 0 && idx < G_N_ELEMENTS (csv_fields_outlook)) {
				contact_field = csv_fields_outlook[idx].contact_field;
				flags = csv_fields_outlook[idx].flags;
				field_text = csv_fields_outlook[idx].csv_attribute;
			}
		}
		else if (importer == MOZILLA_IMPORTER) {
			if (idx >= 0 && idx < G_N_ELEMENTS (csv_fields_mozilla)) {
				contact_field = csv_fields_mozilla[idx].contact_field;
				flags = csv_fields_mozilla[idx].flags;
				field_text = csv_fields_mozilla[idx].csv_attribute;
			}
		}
		else {
			if (idx >= 0 && idx < G_N_ELEMENTS (csv_fields_evolution)) {
				contact_field = csv_fields_evolution[idx].contact_field;
				flags = csv_fields_evolution[idx].flags;
				field_text = csv_fields_evolution[idx].csv_attribute;
			}
		}

		if (*value->str) {
			if (contact_field != NOMAP) {
				if (importer == OUTLOOK_IMPORTER || importer == MOZILLA_IMPORTER) {
					EContactName *cname;

					switch (contact_field) {
					case E_CONTACT_ADDITIONAL_NAME:
						cname = e_contact_get (contact, E_CONTACT_NAME);
						g_free (cname->additional);
						cname->additional = value->str;
						e_contact_set (contact, E_CONTACT_NAME, cname);
						cname->additional = NULL;
						e_contact_name_free (cname);
						break;
					case E_CONTACT_PREFIXES_NAME:
						cname = e_contact_get (contact, E_CONTACT_NAME);
						g_free (cname->prefixes);
						cname->prefixes = value->str;
						e_contact_set (contact, E_CONTACT_NAME, cname);
						cname->prefixes = NULL;
						e_contact_name_free (cname);
						break;
					case E_CONTACT_SUFFIXES_NAME:
						cname = e_contact_get (contact, E_CONTACT_NAME);
						g_free (cname->suffixes);
						cname->suffixes = value->str;
						e_contact_set (contact, E_CONTACT_NAME, cname);
						cname->suffixes = NULL;
						e_contact_name_free (cname);
						break;
					case E_CONTACT_NOTE:
						add_to_notes (contact, NULL, value->str);
						break;
					default:
						e_contact_set (contact, contact_field, value->str);
						break;
					}
				} else {
					if (contact_field == E_CONTACT_WANTS_HTML)
						e_contact_set (
							contact, contact_field,
							GINT_TO_POINTER (
							g_ascii_strcasecmp (
							value->str, "TRUE") == 0));
					else
						e_contact_set (contact, contact_field, value->str);
				}
			}
			else {
				switch (flags) {

				case FLAG_HOME_ADDRESS | FLAG_STREET:
					if (strlen (home_street->str) != 0) {
						g_string_append (home_street, ",\n");
					}
					g_string_append (home_street, value->str);
					break;
				case FLAG_HOME_ADDRESS | FLAG_CITY:
					home_address->locality = g_strdup (value->str);
					break;
				case FLAG_HOME_ADDRESS | FLAG_STATE:
					home_address->region = g_strdup (value->str);
					break;
				case FLAG_HOME_ADDRESS | FLAG_POSTAL_CODE:
					home_address->code = g_strdup (value->str);
					break;
				case FLAG_HOME_ADDRESS | FLAG_POBOX:
					home_address->po = g_strdup (value->str);
					break;
				case FLAG_HOME_ADDRESS | FLAG_COUNTRY:
					home_address->country = g_strdup (value->str);
					break;

				case FLAG_WORK_ADDRESS | FLAG_STREET:
					if (strlen (work_street->str) != 0) {
						g_string_append (work_street, ",\n");
					}
					g_string_append (work_street, value->str);
					break;
				case FLAG_WORK_ADDRESS | FLAG_CITY:
					work_address->locality = g_strdup (value->str);
					break;
				case FLAG_WORK_ADDRESS | FLAG_STATE:
					work_address->region = g_strdup (value->str);
					break;
				case FLAG_WORK_ADDRESS | FLAG_POSTAL_CODE:
					work_address->code = g_strdup (value->str);
					break;
				case FLAG_WORK_ADDRESS | FLAG_POBOX:
					work_address->po = g_strdup (value->str);
					break;
				case FLAG_WORK_ADDRESS | FLAG_COUNTRY:
					work_address->country = g_strdup (value->str);
					break;

				case FLAG_OTHER_ADDRESS | FLAG_STREET:
					if (strlen (other_street->str) != 0) {
						g_string_append (other_street, ",\n");
					}
					g_string_append (other_street, value->str);
					break;
				case FLAG_OTHER_ADDRESS | FLAG_CITY:
					other_address->locality = g_strdup (value->str);
					break;
				case FLAG_OTHER_ADDRESS | FLAG_STATE:
					other_address->region = g_strdup (value->str);
					break;
				case FLAG_OTHER_ADDRESS | FLAG_POSTAL_CODE:
					other_address->code = g_strdup (value->str);
					break;
				case FLAG_OTHER_ADDRESS | FLAG_POBOX:
					other_address->po = g_strdup (value->str);
					break;
				case FLAG_OTHER_ADDRESS | FLAG_COUNTRY:
					other_address->country = g_strdup (value->str);
					break;

				case FLAG_DATE_BDAY:
					e_contact_set (
						contact,
						E_CONTACT_BIRTH_DATE,
						date_from_string (value->str));
					break;

				case FLAG_DATE_ANNIVERSARY:
					e_contact_set (
						contact,
						E_CONTACT_ANNIVERSARY,
						date_from_string (value->str));
					break;

				case FLAG_BIRTH_DAY:
					bday->day = atoi (value->str);
					break;
				case FLAG_BIRTH_YEAR:
					bday->year = atoi (value->str);
					break;
				case FLAG_BIRTH_MONTH:
					bday->month = atoi (value->str);
					break;

				case FLAG_INVALID:
					break;

				default:
					add_to_notes (contact, field_text, value->str);

				}
			}
		}
		ii++;
		g_string_free (value, TRUE);
	}

	/* This inserts FN: in the vCard. */
	g_free (e_contact_get (contact, E_CONTACT_FULL_NAME));
	home_address->street = g_string_free (home_street, !home_street->len);
	work_address->street = g_string_free (work_street, !work_street->len);
	other_address->street = g_string_free (other_street, !other_street->len);

	if (home_address->locality || home_address->country ||
	   home_address->code || home_address->region || home_address->street)
		e_contact_set (contact, E_CONTACT_ADDRESS_HOME, home_address);
	if (work_address->locality || work_address->country ||
	   work_address->code || work_address->region || work_address->street)
		e_contact_set (contact, E_CONTACT_ADDRESS_WORK, work_address);
	if (other_address->locality || other_address->country ||
	   other_address->code || other_address->region || other_address->street)
		e_contact_set (contact, E_CONTACT_ADDRESS_OTHER, other_address);

	if (importer != OUTLOOK_IMPORTER) {
		if (bday->day || bday->year || bday->month)
			e_contact_set (contact, E_CONTACT_BIRTH_DATE, bday);
	}

	e_contact_address_free (home_address);
	e_contact_address_free (work_address);
	e_contact_address_free (other_address);
	e_contact_date_free (bday);
	g_free (do_free);

	return TRUE;
}

static EContact *
getNextCSVEntry (CSVImporter *gci,
                 FILE *f)
{
	EContact *contact = NULL;
	GString *line;
	gint c;

	line = g_string_new ("");
	while (1) {
		c = fgetc (f);
		if (c == EOF) {
			g_string_free (line, TRUE);
			return NULL;
		}
		if (c == '\r')
			c = fgetc (f);
		if (c == '\n') {
			g_string_append_c (line, c);
			break;
		}
		if (c == '"') {
			g_string_append_c (line, c);
			c = fgetc (f);
			while (!feof (f) && c != '"') {
				g_string_append_c (line, c);
				c = fgetc (f);
			}
		}
		g_string_append_c (line, c);
	}

	if (gci->count == 0 && importer != MOZILLA_IMPORTER) {
		gci->fields_map = map_fields (line->str, importer);
		g_string_set_size (line, 0);
		while (1) {
			c = fgetc (f);
			if (c == EOF) {
				g_string_free (line, TRUE);
				return NULL;
			}
			if (c == '\r')
				c = fgetc (f);
			if (c == '\n') {
				g_string_append_c (line, c);
				break;
			}
			if (c == '"') {
				g_string_append_c (line, c);
				c = fgetc (f);
				while (!feof (f) && c != '"') {
					g_string_append_c (line, c);
					c = fgetc (f);
				}
			}
			g_string_append_c (line, c);
		}
		gci->count++;
	}

	if (line->len == 0) {
		g_string_free (line, TRUE);
		return NULL;
	}

	contact = e_contact_new ();

	if (!parseLine (gci, contact, line->str)) {
		g_object_unref (contact);
		g_string_free (line, TRUE);
		return NULL;
	}
	gci->count++;

	g_string_free (line, TRUE);

	return contact;
}

static gboolean
csv_import_contacts (gpointer d)
{
	CSVImporter *gci = d;
	EContact *contact = NULL;

	while ((contact = getNextCSVEntry (gci, gci->file))) {
		gchar *uid = NULL;

		e_book_client_add_contact_sync (
			gci->book_client, contact, E_BOOK_OPERATION_FLAG_NONE, &uid, NULL, NULL);
		if (uid != NULL) {
			e_contact_set (contact, E_CONTACT_UID, uid);
			g_free (uid);
		}
		gci->contacts = g_slist_prepend (gci->contacts, contact);
	}
	if (contact == NULL) {
		gci->state = 1;
	}
	if (gci->state == 1) {
		csv_import_done (gci);
		return FALSE;
	}
	else {
		e_import_status (
			gci->import, gci->target, _("Importing…"),
			ftell (gci->file) * 100 / gci->size);
		return TRUE;
	}
}

static void
primary_selection_changed_cb (ESourceSelector *selector,
                              EImportTarget *target)
{
	ESource *source;

	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	g_datalist_set_data_full (
		&target->data, "csv-source",
		source, (GDestroyNotify) g_object_unref);
}

static GtkWidget *
csv_getwidget (EImport *ei,
               EImportTarget *target,
               EImportImporter *im)
{
	EShell *shell;
	GtkWidget *vbox, *selector, *scrolled_window;
	ESourceRegistry *registry;
	ESource *primary;
	const gchar *extension_name;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (scrolled_window),
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 6);

	selector = e_source_selector_new (registry, extension_name);
	e_source_selector_set_show_toggles (
		E_SOURCE_SELECTOR (selector), FALSE);
	gtk_container_add (GTK_CONTAINER (scrolled_window), selector);

	primary = g_datalist_get_data (&target->data, "csv-source");
	if (primary == NULL) {
		GList *list;

		list = e_source_registry_list_sources (registry, extension_name);
		if (list != NULL) {
			primary = g_object_ref (list->data);
			g_datalist_set_data_full (
				&target->data, "csv-source", primary,
				(GDestroyNotify) g_object_unref);
		}

		g_list_free_full (list, (GDestroyNotify) g_object_unref);
	}
	e_source_selector_set_primary_selection (
		E_SOURCE_SELECTOR (selector), primary);

	g_signal_connect (
		selector, "primary_selection_changed",
		G_CALLBACK (primary_selection_changed_cb), target);

	gtk_widget_show_all (vbox);

	return vbox;
}

static const gchar *supported_extensions[4] = {
	".csv", ".tab" , ".txt", NULL
};

static gboolean
csv_supported (EImport *ei,
               EImportTarget *target,
               EImportImporter *im)
{
	gchar *ext;
	gint i;
	EImportTargetURI *s;

	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *) target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp (s->uri_src, "file:///", 8) != 0)
		return FALSE;

	ext = strrchr (s->uri_src, '.');
	if (ext == NULL)
		return FALSE;

	for (i = 0; supported_extensions[i] != NULL; i++) {
		if (g_ascii_strcasecmp (supported_extensions[i], ext) == 0) {
			if (i == 0) {
				delimiter = CSV_FILE_DELIMITER;
			}
			else {
				delimiter = TAB_FILE_DELIMITER;
			}
			return TRUE;
		}
	}

	return FALSE;
}

static void
csv_import_done (CSVImporter *gci)
{
	if (gci->idle_id)
		g_source_remove (gci->idle_id);

	fclose (gci->file);
	g_object_unref (gci->book_client);
	g_slist_foreach (gci->contacts, (GFunc) g_object_unref, NULL);
	g_slist_free (gci->contacts);

	if (gci->fields_map)
		g_hash_table_destroy (gci->fields_map);

	e_import_complete (gci->import, gci->target, NULL);
	g_object_unref (gci->import);

	g_free (gci);
}

static void
book_client_connect_cb (GObject *source_object,
                        GAsyncResult *result,
                        gpointer user_data)
{
	CSVImporter *gci = user_data;
	EClient *client;

	client = e_book_client_connect_finish (result, NULL);

	if (client == NULL) {
		csv_import_done (gci);
		return;
	}

	gci->book_client = E_BOOK_CLIENT (client);
	gci->idle_id = g_idle_add (csv_import_contacts, gci);
}

static void
csv_import (EImport *ei,
            EImportTarget *target,
            EImportImporter *im)
{
	CSVImporter *gci;
	ESource *source;
	gchar *filename;
	FILE *file;
	gint errn;
	EImportTargetURI *s = (EImportTargetURI *) target;
	GError *error = NULL;

	filename = g_filename_from_uri (s->uri_src, NULL, &error);
	if (filename == NULL) {
		e_import_complete (ei, target, error);
		g_clear_error (&error);

		return;
	}

	file = g_fopen (filename, "r");
	errn = errno;
	g_free (filename);

	if (file == NULL) {
		error = g_error_new_literal (G_IO_ERROR, g_io_error_from_errno (errn), _("Can’t open .csv file"));
		e_import_complete (ei, target, error);
		g_clear_error (&error);

		return;
	}

	gci = g_malloc0 (sizeof (*gci));
	g_datalist_set_data (&target->data, "csv-data", gci);
	gci->import = g_object_ref (ei);
	gci->target = target;
	gci->file = file;
	gci->fields_map = NULL;
	gci->count = 0;
	fseek (file, 0, SEEK_END);
	gci->size = ftell (file);
	fseek (file, 0, SEEK_SET);

	/* Consume byte order mark EF BB BF, if at the beginning of the file */
	if (fgetc (file) != 0xEF || fgetc (file) != 0xBB || fgetc (file) != 0xBF)
		fseek (file, 0, SEEK_SET);

	source = g_datalist_get_data (&target->data, "csv-source");

	e_book_client_connect (source, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, NULL, book_client_connect_cb, gci);
}

static void
outlook_csv_import (EImport *ei,
                    EImportTarget *target,
                    EImportImporter *im)
{
	importer = OUTLOOK_IMPORTER;
	csv_import (ei, target, im);
}

static void
mozilla_csv_import (EImport *ei,
                    EImportTarget *target,
                    EImportImporter *im)
{
	importer = MOZILLA_IMPORTER;
	csv_import (ei, target, im);
}

static void
evolution_csv_import (EImport *ei,
                      EImportTarget *target,
                      EImportImporter *im)
{
	importer = EVOLUTION_IMPORTER;
	csv_import (ei, target, im);
}

static void
csv_cancel (EImport *ei,
            EImportTarget *target,
            EImportImporter *im)
{
	CSVImporter *gci = g_datalist_get_data (&target->data, "csv-data");

	if (gci)
		gci->state = 1;
}

static GtkWidget *
csv_get_preview (EImport *ei,
                 EImportTarget *target,
                 EImportImporter *im)
{
	GtkWidget *preview;
	GSList *contacts = NULL;
	EContact *contact;
	EImportTargetURI *s = (EImportTargetURI *) target;
	gchar *filename;
	FILE *file;
	CSVImporter *gci;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (filename == NULL) {
		g_message (G_STRLOC ": Couldn't get filename from URI '%s'", s->uri_src);
		return NULL;
	}

	file = g_fopen (filename, "r");
	g_free (filename);
	if (file == NULL) {
		g_message (G_STRLOC ": Can't open .csv file");
		return NULL;
	}

	gci = g_malloc0 (sizeof (*gci));
	gci->file = file;
	gci->fields_map = NULL;
	gci->count = 0;
	fseek (file, 0, SEEK_END);
	gci->size = ftell (file);
	fseek (file, 0, SEEK_SET);

	while (contact = getNextCSVEntry (gci, gci->file), contact != NULL) {
		contacts = g_slist_prepend (contacts, contact);
	}

	contacts = g_slist_reverse (contacts);
	preview = evolution_contact_importer_get_preview_widget (contacts);

	g_slist_free_full (contacts, (GDestroyNotify) g_object_unref);
	fclose (file);
	g_free (gci);

	return preview;
}

static GtkWidget *
outlook_csv_get_preview (EImport *ei,
                         EImportTarget *target,
                         EImportImporter *im)
{
	importer = OUTLOOK_IMPORTER;
	return csv_get_preview (ei, target, im);
}

static GtkWidget *
mozilla_csv_get_preview (EImport *ei,
                         EImportTarget *target,
                         EImportImporter *im)
{
	importer = MOZILLA_IMPORTER;
	return csv_get_preview (ei, target, im);
}

static GtkWidget *
evolution_csv_get_preview (EImport *ei,
                           EImportTarget *target,
                           EImportImporter *im)
{
	importer = EVOLUTION_IMPORTER;
	return csv_get_preview (ei, target, im);
}

static EImportImporter csv_outlook_importer = {
	E_IMPORT_TARGET_URI,
	0,
	csv_supported,
	csv_getwidget,
	outlook_csv_import,
	csv_cancel,
	outlook_csv_get_preview,
};

static EImportImporter csv_mozilla_importer = {
	E_IMPORT_TARGET_URI,
	0,
	csv_supported,
	csv_getwidget,
	mozilla_csv_import,
	csv_cancel,
	mozilla_csv_get_preview,
};

static EImportImporter csv_evolution_importer = {
	E_IMPORT_TARGET_URI,
	0,
	csv_supported,
	csv_getwidget,
	evolution_csv_import,
	csv_cancel,
	evolution_csv_get_preview,
};

EImportImporter *
evolution_csv_outlook_importer_peek (void)
{
	csv_outlook_importer.name = _("Outlook Contacts CSV or Tab (.csv, .tab)");
	csv_outlook_importer.description = _("Outlook Contacts CSV and Tab Importer");

	return &csv_outlook_importer;
}

EImportImporter *
evolution_csv_mozilla_importer_peek (void)
{
	csv_mozilla_importer.name = _("Mozilla Contacts CSV or Tab (.csv, .tab)");
	csv_mozilla_importer.description = _("Mozilla Contacts CSV and Tab Importer");

	return &csv_mozilla_importer;
}

EImportImporter *
evolution_csv_evolution_importer_peek (void)
{
	csv_evolution_importer.name = _("Evolution Contacts CSV or Tab (.csv, .tab)");
	csv_evolution_importer.description = _("Evolution Contacts CSV and Tab Importer");

	return &csv_evolution_importer;
}
