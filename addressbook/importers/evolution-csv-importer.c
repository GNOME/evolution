/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution CSV and TAB importer
 *
 * Copyright (C) 2005  Novell, Inc.
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
 * Authors: Devashish Sharma <sdevashish@novell.com>
 */
#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gtk/gtkvbox.h>

#include <libebook/e-book.h>
#include <libedataserverui/e-source-selector.h>

#include <libebook/e-destination.h>

#include "e-util/e-import.h"

#include "evolution-addressbook-importers.h"

#define NOMAP -1
#define EVOLUTION_IMPORTER 3
#define MOZILLA_IMPORTER 2
#define OUTLOOK_IMPORTER 1
#define CSV_FILE_DELIMITER ','
#define TAB_FILE_DELIMITER '\t'

typedef struct {
	EImport *import;
	EImportTarget *target;

	guint idle_id;
	
	int state;	
	FILE *file;
	gulong size;
	gint count;

	EBook *book;
	GSList *contacts;
} CSVImporter;

static gint importer;
static char delimiter;

static void csv_import_done(CSVImporter *gci);

typedef struct {
	char *csv_attribute;
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
	int flags;
}import_fields;

import_fields csv_fields_outlook[] = {
	{"Title", NOMAP},
	{"First Name", E_CONTACT_GIVEN_NAME},
	{"Middle Name", NOMAP},
	{"Last Name", E_CONTACT_FAMILY_NAME},
	{"Suffix", NOMAP},
	{"Company", E_CONTACT_ORG},
	{"Department", E_CONTACT_ORG_UNIT},
	{"Job Title", E_CONTACT_TITLE},
	{"Business Street", NOMAP, FLAG_WORK_ADDRESS|FLAG_STREET },
	{"Business Street 2", NOMAP, FLAG_WORK_ADDRESS|FLAG_STREET },
	{"Business Street 3", NOMAP, FLAG_WORK_ADDRESS|FLAG_STREET},
	{"Business City", NOMAP, FLAG_WORK_ADDRESS|FLAG_CITY},
	{"Business State", NOMAP, FLAG_WORK_ADDRESS|FLAG_STATE},
	{"Business Postal Code", NOMAP, FLAG_WORK_ADDRESS|FLAG_POSTAL_CODE},
	{"Business Country", NOMAP, FLAG_WORK_ADDRESS|FLAG_COUNTRY},
	{"Home Street", NOMAP, FLAG_HOME_ADDRESS|FLAG_STREET},
	{"Home Street 2", NOMAP, FLAG_HOME_ADDRESS|FLAG_STREET},
	{"Home Street 3", NOMAP, FLAG_HOME_ADDRESS|FLAG_STREET},
	{"Home City", NOMAP, FLAG_HOME_ADDRESS|FLAG_CITY},
	{"Home State", NOMAP, FLAG_HOME_ADDRESS|FLAG_STATE},
	{"Home Postal Code", NOMAP,FLAG_HOME_ADDRESS|FLAG_POSTAL_CODE},
	{"Home Country", NOMAP, FLAG_HOME_ADDRESS|FLAG_COUNTRY},
	{"Other Street", NOMAP, FLAG_OTHER_ADDRESS|FLAG_STREET},
	{"Other Street 2", NOMAP, FLAG_OTHER_ADDRESS|FLAG_STREET},
	{"Other Street 3", NOMAP, FLAG_OTHER_ADDRESS|FLAG_STREET},
	{"Other City", NOMAP, FLAG_OTHER_ADDRESS|FLAG_CITY},
	{"Other State", NOMAP, FLAG_OTHER_ADDRESS|FLAG_STATE},
	{"Other Postal Code", NOMAP, FLAG_OTHER_ADDRESS|FLAG_POSTAL_CODE},
	{"Other Country", NOMAP, FLAG_OTHER_ADDRESS|FLAG_COUNTRY},
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
	{"Business Address PO Box", NOMAP, FLAG_WORK_ADDRESS|FLAG_POBOX},
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
	{"Home Address PO Box", NOMAP, FLAG_HOME_ADDRESS|FLAG_POBOX},
	{"Initials", NOMAP},
	{"Internet FREE/BUSY", E_CONTACT_FREEBUSY_URL}, 
	{"Keywords", NOMAP},
	{"Language", NOMAP},
	{"Location", NOMAP},
	{"Managers Name", E_CONTACT_MANAGER},
	{"Mileage", NOMAP},
	{"Notes", NOMAP},
	{"Office Location", NOMAP},
	{"Organizational ID Number", NOMAP},
	{"Other Address PO Box", NOMAP, FLAG_OTHER_ADDRESS|FLAG_POBOX},
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

import_fields csv_fields_mozilla[] = {
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
	{"Home Street", NOMAP, FLAG_HOME_ADDRESS|FLAG_STREET},
	{"Home Street 2", NOMAP, FLAG_HOME_ADDRESS|FLAG_STREET},
	{"Home City", NOMAP, FLAG_HOME_ADDRESS|FLAG_CITY},
	{"Home State", NOMAP, FLAG_HOME_ADDRESS|FLAG_STATE},
	{"Home Postal Code", NOMAP,FLAG_HOME_ADDRESS|FLAG_POSTAL_CODE},
	{"Home Country", NOMAP, FLAG_HOME_ADDRESS|FLAG_COUNTRY},
	{"Business Street", NOMAP, FLAG_WORK_ADDRESS|FLAG_STREET },
	{"Business Street 2", NOMAP, FLAG_WORK_ADDRESS|FLAG_STREET },
	{"Business City", NOMAP, FLAG_WORK_ADDRESS|FLAG_CITY},
	{"Business State", NOMAP, FLAG_WORK_ADDRESS|FLAG_STATE},
	{"Business Postal Code", NOMAP, FLAG_WORK_ADDRESS|FLAG_POSTAL_CODE},
	{"Business Country", NOMAP, FLAG_WORK_ADDRESS|FLAG_COUNTRY},
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
	{"Notes", NOMAP},
	
	
};

import_fields csv_fields_evolution[] = {
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
	{"Home Street", NOMAP, FLAG_HOME_ADDRESS|FLAG_STREET},
	{"Home Street 2", NOMAP, FLAG_INVALID},
	{"Home City", NOMAP, FLAG_HOME_ADDRESS|FLAG_CITY},
	{"Home State", NOMAP, FLAG_HOME_ADDRESS|FLAG_STATE},
	{"Home Postal Code", NOMAP,FLAG_HOME_ADDRESS|FLAG_POSTAL_CODE},
	{"Home Country", NOMAP, FLAG_HOME_ADDRESS|FLAG_COUNTRY},
	{"Business Street", NOMAP, FLAG_WORK_ADDRESS|FLAG_STREET },
	{"Business Street 2", NOMAP, FLAG_INVALID },
	{"Business City", NOMAP, FLAG_WORK_ADDRESS|FLAG_CITY},
	{"Business State", NOMAP, FLAG_WORK_ADDRESS|FLAG_STATE},
	{"Business Postal Code", NOMAP, FLAG_WORK_ADDRESS|FLAG_POSTAL_CODE},
	{"Business Country", NOMAP, FLAG_WORK_ADDRESS|FLAG_COUNTRY},
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
add_to_notes(EContact *contact, gint i, char *val) {
	const gchar *old_text;
	const gchar *field_text = NULL;
	GString *new_text;
	
	old_text = e_contact_get_const(contact, E_CONTACT_NOTE);
	if(importer == OUTLOOK_IMPORTER)
		field_text = csv_fields_outlook[i].csv_attribute;
	else if(importer == MOZILLA_IMPORTER)
		field_text = csv_fields_mozilla[i].csv_attribute;
	else
		field_text = csv_fields_evolution[i].csv_attribute;	
	
	new_text = g_string_new(old_text);
	if(strlen(new_text->str) != 0)
		new_text = g_string_append_c(new_text, '\n');
	new_text = g_string_append(new_text, field_text);
	new_text = g_string_append_c(new_text, ':');
	new_text = g_string_append(new_text, val);

	e_contact_set(contact, E_CONTACT_NOTE, new_text->str);
	g_string_free(new_text, TRUE);
}

static gboolean 
parseLine (CSVImporter *gci, EContact *contact, char **buf) {
	
	char *ptr = *buf;
	GString *value;
	gint i = 0;
	int flags = 0;
	int contact_field;
	EContactAddress *home_address = NULL, *work_address = NULL, *other_address = NULL;
	EContactDate *bday = NULL;
	GString *home_street, *work_street, *other_street;
	home_street = g_string_new("");
	work_street = g_string_new("");
	other_street = g_string_new("");
	home_address = g_new0(EContactAddress, 1);
	work_address = g_new0(EContactAddress, 1);
	other_address = g_new0(EContactAddress, 1);
	bday = g_new0(EContactDate, 1);
	
	while(*ptr != '\n') {
		value = g_string_new("");
		while(*ptr != delimiter) {
			if(*ptr == '\n')
				break;
			if(*ptr != '"') {
				g_string_append_unichar(value, *ptr);
			}
			ptr++;
		}
		if(importer == OUTLOOK_IMPORTER) {
			contact_field = csv_fields_outlook[i].contact_field;
			flags = csv_fields_outlook[i].flags;
		}
		else if(importer == MOZILLA_IMPORTER) {
			contact_field = csv_fields_mozilla[i].contact_field;
			flags = csv_fields_mozilla[i].flags;
		}
		else {
			contact_field = csv_fields_evolution[i].contact_field;
			flags = csv_fields_evolution[i].flags;
		}

		if(strlen(value->str) != 0) {
			if (contact_field != NOMAP) {
				if(importer == OUTLOOK_IMPORTER)
					e_contact_set(contact, csv_fields_outlook[i].contact_field, value->str);
				else if(importer == MOZILLA_IMPORTER)
					e_contact_set(contact, csv_fields_mozilla[i].contact_field, value->str);
				else
					e_contact_set(contact, csv_fields_evolution[i].contact_field, value->str);
			}
			else {
				switch (flags) {

				case FLAG_HOME_ADDRESS|FLAG_STREET:
					if(strlen(home_street->str) != 0) {
						home_street = g_string_append(home_street, ",\n");
					}
					home_street = g_string_append(home_street, value->str);
					break;
				case FLAG_HOME_ADDRESS|FLAG_CITY:
					home_address->locality = g_strdup(value->str);
					break;
				case FLAG_HOME_ADDRESS|FLAG_STATE:
					home_address->region = g_strdup(value->str);
					break;
				case FLAG_HOME_ADDRESS|FLAG_POSTAL_CODE:
					home_address->code = g_strdup(value->str);
					break;
				case FLAG_HOME_ADDRESS|FLAG_POBOX:
					home_address->po = g_strdup(value->str);
					break;
				case FLAG_HOME_ADDRESS|FLAG_COUNTRY:
					home_address->country = g_strdup(value->str);
					break;

				case FLAG_WORK_ADDRESS|FLAG_STREET:
					if(strlen(work_street->str) != 0) {
						work_street = g_string_append(work_street, ",\n");
					}
					work_street = g_string_append(work_street, value->str);
					break;
				case FLAG_WORK_ADDRESS|FLAG_CITY:
					work_address->locality = g_strdup(value->str);
					break;
				case FLAG_WORK_ADDRESS|FLAG_STATE:
					work_address->region = g_strdup(value->str);
					break;
				case FLAG_WORK_ADDRESS|FLAG_POSTAL_CODE:
					work_address->code = g_strdup(value->str);
					break;
				case FLAG_WORK_ADDRESS|FLAG_POBOX:
					work_address->po = g_strdup(value->str);
					break;
				case FLAG_WORK_ADDRESS|FLAG_COUNTRY:
					work_address->country = g_strdup(value->str);
					break;

				case FLAG_OTHER_ADDRESS|FLAG_STREET:
					if(strlen(other_street->str) != 0) {
						other_street = g_string_append(other_street, ",\n");
					}
					other_street = g_string_append(other_street, value->str);
					break;
				case FLAG_OTHER_ADDRESS|FLAG_CITY:
					other_address->locality = g_strdup(value->str);
					break;
				case FLAG_OTHER_ADDRESS|FLAG_STATE:
					other_address->region = g_strdup(value->str);
					break;
				case FLAG_OTHER_ADDRESS|FLAG_POSTAL_CODE:
					other_address->code = g_strdup(value->str);
					break;
				case FLAG_OTHER_ADDRESS|FLAG_COUNTRY:
					other_address->country = g_strdup(value->str);
					break;

				case FLAG_DATE_BDAY:
					e_contact_set(contact, E_CONTACT_BIRTH_DATE, e_contact_date_from_string(value->str));
					break;
					
				case FLAG_DATE_ANNIVERSARY:
					e_contact_set(contact, E_CONTACT_ANNIVERSARY, e_contact_date_from_string(value->str));
					break;

				case FLAG_BIRTH_DAY:
					bday->day = atoi(value->str);
					break;
				case FLAG_BIRTH_YEAR:
					bday->year = atoi(value->str);
					break;
				case FLAG_BIRTH_MONTH:
					bday->month = atoi(value->str);
					break;

				case FLAG_INVALID:
					break;
					
				default:
					add_to_notes(contact, i, value->str);	

				}
			}
		}
		i++;
		g_string_free(value, TRUE);
		if(*ptr != '\n')
			ptr++;
	}
	if(strlen(home_street->str) != 0)
		home_address->street = g_strdup(home_street->str);
	if(strlen(work_street->str) != 0)
		work_address->street = g_strdup(work_street->str);
	if(strlen(other_street->str) != 0)
		other_address->street = g_strdup(other_street->str);
	g_string_free(home_street, TRUE);
	g_string_free(work_street, TRUE);
	g_string_free(other_street, TRUE);

	if(home_address->locality || home_address->country ||
	   home_address->code || home_address->region || home_address->street)		
		e_contact_set (contact, E_CONTACT_ADDRESS_HOME, home_address);
	if(work_address->locality || work_address->country ||
	   work_address->code || work_address->region || work_address->street)		
		e_contact_set (contact, E_CONTACT_ADDRESS_WORK, work_address);
	if(other_address->locality || other_address->country ||
	   other_address->code || other_address->region || other_address->street)		
		e_contact_set (contact, E_CONTACT_ADDRESS_OTHER, other_address);

	if(importer !=  OUTLOOK_IMPORTER) {
		if (bday->day || bday->year || bday->month)
			e_contact_set(contact, E_CONTACT_BIRTH_DATE, bday);
	}

	return TRUE;
}

static EContact *
getNextCSVEntry(CSVImporter *gci, FILE *f) {
	EContact *contact = NULL;
	char line[2048];
	GString *str;
	char *buf;

	str = g_string_new("");
	if(!fgets(line, sizeof(line),f)) {
		g_string_free(str, TRUE);
		break;
	}

	if(gci->count == 0 && importer != MOZILLA_IMPORTER) {
		if(!fgets(line, sizeof(line),f)) {
			g_string_free(str, TRUE);
			break;
		}
		gci->count ++;
	}

	str = g_string_append (str, line);
	
	if(strlen(str->str) == 0) {
		g_string_free(str, TRUE);
		return NULL;
	}

	contact = e_contact_new();

	buf = str->str;

	if(!parseLine (gci, contact, &buf)) {
		g_object_unref(contact);
		return NULL;
	}
	gci->count++;

	g_string_free(str, TRUE);

	return contact;
}

static gboolean
csv_import_contacts(void *d) {
	CSVImporter *gci = d;
	EContact *contact = NULL;

	while ((contact = getNextCSVEntry(gci, gci->file))) {
		e_book_add_contact(gci->book, contact, NULL);
		gci->contacts = g_slist_prepend(gci->contacts, contact);
	}
	if(contact == NULL) {
		gci->state = 1;
	}
	if(gci->state == 1) {
		csv_import_done(gci);
		return FALSE;
	}
	else {
		e_import_status(gci->import, gci->target, _("Importing..."), ftell(gci->file) *100 / gci->size);
		return TRUE;
	}
}

static void
primary_selection_changed_cb (ESourceSelector *selector, EImportTarget *target)
{
	g_datalist_set_data_full(&target->data, "csv-source",
				 g_object_ref(e_source_selector_peek_primary_selection(selector)),
				 g_object_unref);
}

static GtkWidget *
csv_getwidget(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *vbox, *selector;
	ESource *primary;
	ESourceList *source_list;	

	/* FIXME Better error handling */
	if (!e_book_get_addressbooks (&source_list, NULL))
		return NULL;

	vbox = gtk_vbox_new (FALSE, FALSE);
	
	selector = e_source_selector_new (source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), selector, FALSE, TRUE, 6);

	primary = g_datalist_get_data(&target->data, "csv-source");
	if (primary == NULL) {
		primary = e_source_list_peek_source_any (source_list);
		g_object_ref(primary);
		g_datalist_set_data_full(&target->data, "csv-source", primary, g_object_unref);
	}
	e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (selector), primary);
	g_object_unref (source_list);

	g_signal_connect (selector, "primary_selection_changed", G_CALLBACK (primary_selection_changed_cb), target);

	gtk_widget_show_all (vbox);

	return vbox;
}

static char *supported_extensions[4] = {
	".csv", ".tab" , ".txt", NULL
};

static gboolean
csv_supported(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	char *ext;
	int i;
	EImportTargetURI *s;
	
	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *)target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp(s->uri_src, "file:///", 8) != 0)
		return FALSE;

	ext = strrchr(s->uri_src, '.');
	if (ext == NULL)
		return FALSE;

	for (i = 0; supported_extensions[i] != NULL; i++) {
		if (g_ascii_strcasecmp(supported_extensions[i], ext) == 0) {
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
csv_import_done(CSVImporter *gci)
{
	if (gci->idle_id)
		g_source_remove(gci->idle_id);

	fclose (gci->file);
	g_object_unref(gci->book);
	g_slist_foreach(gci->contacts, (GFunc) g_object_unref, NULL);
	g_slist_free(gci->contacts);

	e_import_complete(gci->import, gci->target);
	g_object_unref(gci->import);

	g_free (gci);
}

static void
csv_import (EImport *ei, EImportTarget *target, EImportImporter *im)
{
	CSVImporter *gci;
	EBook *book;
	FILE *file;
	EImportTargetURI *s = (EImportTargetURI *) target;

	book = e_book_new(g_datalist_get_data(&target->data, "csv-source"), NULL);
	if(book == NULL) {
		g_message("Couldn't Create EBook");
		e_import_complete(ei, target);
		return;
	}

	file = g_fopen (g_filename_from_uri(s->uri_src, NULL, NULL), "r");
	if (file == NULL) {
		g_message("Can't open .csv file");
		e_import_complete(ei, target);
		g_object_unref(book);
		return;
	}

	gci = g_malloc0(sizeof(*gci));
	g_datalist_set_data(&target->data, "csv-data", gci);
	gci->import = g_object_ref(ei);
	gci->target = target;
	gci->book = book;
	gci->file = file;
	gci->count = 0;
	fseek(file, 0, SEEK_END);
	gci->size = ftell(file);
	fseek(file, 0, SEEK_SET);

	e_book_open(gci->book, TRUE, NULL);
       	
	gci->idle_id = g_idle_add (csv_import_contacts, gci);
}

static void
outlook_csv_import(EImport *ei, EImportTarget *target, EImportImporter *im) 
{
	importer = OUTLOOK_IMPORTER;
	csv_import(ei, target, im);
}

static void
mozilla_csv_import(EImport *ei, EImportTarget *target, EImportImporter *im) 
{
	importer = MOZILLA_IMPORTER;
	csv_import(ei, target, im);
}

static void
evolution_csv_import(EImport *ei, EImportTarget *target, EImportImporter *im) 
{
	importer = EVOLUTION_IMPORTER;
	csv_import(ei, target, im);
}

static void
csv_cancel(EImport *ei, EImportTarget *target, EImportImporter *im) {
	CSVImporter *gci = g_datalist_get_data(&target->data, "csv-data");

	if(gci)
		gci->state = 1;
}
	

static EImportImporter csv_outlook_importer = {
	E_IMPORT_TARGET_URI,
	0,
	csv_supported,
	csv_getwidget,
	outlook_csv_import,
	csv_cancel,
};

static EImportImporter csv_mozilla_importer = {
	E_IMPORT_TARGET_URI,
	0,
	csv_supported,
	csv_getwidget,
	mozilla_csv_import,
	csv_cancel,
};

static EImportImporter csv_evolution_importer = {
	E_IMPORT_TARGET_URI,
	0,
	csv_supported,
	csv_getwidget,
	evolution_csv_import,
	csv_cancel,
};

EImportImporter *
evolution_csv_outlook_importer_peek(void)
{
	csv_outlook_importer.name = _("Outlook CSV or Tab (.csv, .tab)");
	csv_outlook_importer.description = _("Outlook CSV and Tab Importer");
	
	return &csv_outlook_importer;
}

EImportImporter *
evolution_csv_mozilla_importer_peek(void)
{
	csv_mozilla_importer.name = _("Mozilla CSV or Tab (.csv, .tab)");
	csv_mozilla_importer.description = _("Mozilla CSV and Tab Importer"); 	
	
	return &csv_mozilla_importer;
}

EImportImporter *
evolution_csv_evolution_importer_peek(void)
{
	csv_evolution_importer.name = _("Evolution CSV or Tab (.csv, .tab)");
	csv_evolution_importer.description = _("Evolution CSV and Tab Importer"); 	
	
	return &csv_evolution_importer;
}
