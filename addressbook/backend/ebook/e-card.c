/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Arturo Espinosa (arturo@nuclecu.unam.mx)
 *   Nat Friedman    (nat@ximian.com)
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#include <config.h>

#include "e-card.h"

#include <gal/util/e-i18n.h>
#include <gal/widgets/e-unicode.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <gtk/gtkobject.h>
#include <bonobo/bonobo-object-client.h>
#include <gal/util/e-util.h>

#include <libversit/vcc.h>
#include "e-util/ename/e-name-western.h"
#include "e-util/ename/e-address-western.h"
#include "e-book.h"
#include "e-destination.h"

#define is_a_prop_of(obj,prop) (isAPropertyOf ((obj),(prop)))
#define str_val(obj) (the_str = (vObjectValueType (obj))? fakeCString (vObjectUStringZValue (obj)) : calloc (1, 1))
#define has(obj,prop) (vo = isAPropertyOf ((obj), (prop)))

#define XEV_WANTS_HTML "X-MOZILLA-HTML"
#define XEV_ARBITRARY "X-EVOLUTION-ARBITRARY"
#define XEV_LIST "X-EVOLUTION-LIST"
#define XEV_LIST_SHOW_ADDRESSES "X-EVOLUTION-LIST-SHOW_ADDRESSES"
#define XEV_RELATED_CONTACTS "X-EVOLUTION-RELATED_CONTACTS"

/* Object argument IDs */
enum {
	ARG_0,
	ARG_FILE_AS,
	ARG_FULL_NAME,
	ARG_NAME,
	ARG_ADDRESS,
	ARG_ADDRESS_LABEL,
	ARG_PHONE,
	ARG_EMAIL,
	ARG_BIRTH_DATE,
	ARG_URL,
	ARG_ORG,
	ARG_ORG_UNIT,
	ARG_OFFICE,
	ARG_TITLE,
	ARG_ROLE,
	ARG_MANAGER,
	ARG_ASSISTANT,
	ARG_NICKNAME,
	ARG_SPOUSE,
	ARG_ANNIVERSARY,
	ARG_MAILER,
	ARG_CALURI,
	ARG_FBURL,
	ARG_ICSCALENDAR,
	ARG_NOTE,
	ARG_RELATED_CONTACTS,
	ARG_CATEGORIES,
	ARG_CATEGORY_LIST,
	ARG_WANTS_HTML,
	ARG_WANTS_HTML_SET,
	ARG_EVOLUTION_LIST,
	ARG_EVOLUTION_LIST_SHOW_ADDRESSES,
	ARG_ARBITRARY,
	ARG_ID,
	ARG_LAST_USE,
	ARG_USE_SCORE,
};

static void parse(ECard *card, VObject *vobj, char *default_charset);
static void e_card_init (ECard *card);
static void e_card_class_init (ECardClass *klass);

static void e_card_destroy (GtkObject *object);
static void e_card_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_card_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static void assign_string(VObject *vobj, char *default_charset, char **string);

char *e_v_object_get_child_value(VObject *vobj, char *name, char *default_charset);

static void parse_bday(ECard *card, VObject *object, char *default_charset);
static void parse_full_name(ECard *card, VObject *object, char *default_charset);
static void parse_file_as(ECard *card, VObject *object, char *default_charset);
static void parse_name(ECard *card, VObject *object, char *default_charset);
static void parse_email(ECard *card, VObject *object, char *default_charset);
static void parse_phone(ECard *card, VObject *object, char *default_charset);
static void parse_address(ECard *card, VObject *object, char *default_charset);
static void parse_address_label(ECard *card, VObject *object, char *default_charset);
static void parse_url(ECard *card, VObject *object, char *default_charset);
static void parse_org(ECard *card, VObject *object, char *default_charset);
static void parse_office(ECard *card, VObject *object, char *default_charset);
static void parse_title(ECard *card, VObject *object, char *default_charset);
static void parse_role(ECard *card, VObject *object, char *default_charset);
static void parse_manager(ECard *card, VObject *object, char *default_charset);
static void parse_assistant(ECard *card, VObject *object, char *default_charset);
static void parse_nickname(ECard *card, VObject *object, char *default_charset);
static void parse_spouse(ECard *card, VObject *object, char *default_charset);
static void parse_anniversary(ECard *card, VObject *object, char *default_charset);
static void parse_mailer(ECard *card, VObject *object, char *default_charset);
static void parse_caluri(ECard *card, VObject *object, char *default_charset);
static void parse_fburl(ECard *card, VObject *object, char *default_charset);
static void parse_icscalendar(ECard *card, VObject *object, char *default_charset);
static void parse_note(ECard *card, VObject *object, char *default_charset);
static void parse_related_contacts(ECard *card, VObject *object, char *default_charset);
static void parse_categories(ECard *card, VObject *object, char *default_charset);
static void parse_wants_html(ECard *card, VObject *object, char *default_charset);
static void parse_list(ECard *card, VObject *object, char *default_charset);
static void parse_list_show_addresses(ECard *card, VObject *object, char *default_charset);
static void parse_arbitrary(ECard *card, VObject *object, char *default_charset);
static void parse_id(ECard *card, VObject *object, char *default_charset);
static void parse_last_use(ECard *card, VObject *object, char *default_charset);
static void parse_use_score(ECard *card, VObject *object, char *default_charset);

static ECardPhoneFlags get_phone_flags (VObject *vobj);
static void set_phone_flags (VObject *vobj, ECardPhoneFlags flags);
static ECardAddressFlags get_address_flags (VObject *vobj);
static void set_address_flags (VObject *vobj, ECardAddressFlags flags);

typedef void (* ParsePropertyFunc) (ECard *card, VObject *object, char *default_charset);

struct {
	char *key;
	ParsePropertyFunc function;
} attribute_jump_array[] = 
{
	{ VCFullNameProp,            parse_full_name },
	{ "X-EVOLUTION-FILE-AS",     parse_file_as },
	{ VCNameProp,                parse_name },
	{ VCBirthDateProp,           parse_bday },
	{ VCEmailAddressProp,        parse_email },
	{ VCTelephoneProp,           parse_phone },
	{ VCAdrProp,                 parse_address },
	{ VCDeliveryLabelProp,       parse_address_label },
	{ VCURLProp,                 parse_url },
	{ VCOrgProp,                 parse_org },
	{ "X-EVOLUTION-OFFICE",      parse_office },
	{ VCTitleProp,               parse_title },
	{ VCBusinessRoleProp,        parse_role },
	{ "X-EVOLUTION-MANAGER",     parse_manager },
	{ "X-EVOLUTION-ASSISTANT",   parse_assistant },
	{ "NICKNAME",                parse_nickname },
	{ "X-EVOLUTION-SPOUSE",      parse_spouse },
	{ "X-EVOLUTION-ANNIVERSARY", parse_anniversary },   
	{ VCMailerProp,              parse_mailer },
	{ "CALURI",                  parse_caluri },
	{ "FBURL",                   parse_fburl },
	{ "ICSCALENDAR",             parse_icscalendar },
	{ VCNoteProp,                parse_note },
	{ XEV_RELATED_CONTACTS,      parse_related_contacts },
	{ "CATEGORIES",              parse_categories },
	{ XEV_WANTS_HTML,            parse_wants_html },
	{ XEV_ARBITRARY,             parse_arbitrary },
	{ VCUniqueStringProp,        parse_id },
	{ "X-EVOLUTION-LAST-USE",    parse_last_use },
	{ "X-EVOLUTION-USE-SCORE",   parse_use_score },
	{ XEV_LIST,                  parse_list },
	{ XEV_LIST_SHOW_ADDRESSES,   parse_list_show_addresses },
	{ VCUniqueStringProp,        parse_id }
};

/**
 * e_card_get_type:
 * @void: 
 * 
 * Registers the &ECard class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ECard class.
 **/
GtkType
e_card_get_type (void)
{
	static GtkType card_type = 0;

	if (!card_type) {
		GtkTypeInfo card_info = {
			"ECard",
			sizeof (ECard),
			sizeof (ECardClass),
			(GtkClassInitFunc) e_card_class_init,
			(GtkObjectInitFunc) e_card_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		card_type = gtk_type_unique (gtk_object_get_type (), &card_info);
	}

	return card_type;
}

ECard *
e_card_new_with_default_charset (char *vcard, char *default_charset)
{
	ECard *card = E_CARD(gtk_type_new(e_card_get_type()));
	VObject *vobj = Parse_MIME(vcard, strlen(vcard));
	while(vobj) {
		VObject *next;
		parse(card, vobj, default_charset);
		next = nextVObjectInList(vobj);
		cleanVObject(vobj);
		vobj = next;
	}
	if (card->name == NULL)
		card->name = e_card_name_new();
	if (card->file_as == NULL)
		card->file_as = g_strdup("");
	if (card->fname == NULL)
		card->fname = g_strdup("");
	return card;
}

/**
 * e_card_new:
 * @vcard: a string in vCard format
 *
 * Returns: a new #ECard that wraps the @vcard.
 */
ECard *
e_card_new (char *vcard)
{
	return e_card_new_with_default_charset (vcard, "UTF-8");
}

ECard *
e_card_duplicate(ECard *card)
{
	char *vcard = e_card_get_vcard_assume_utf8(card);
	ECard *new_card = e_card_new(vcard);
	g_free (vcard);
	
	if (card->book) {
		new_card->book = card->book;
		gtk_object_ref (GTK_OBJECT (new_card->book));
	}

	return new_card;
}

static void
e_card_get_today (GDate *dt)
{
	time_t now;
	struct tm *now_tm;
	if (dt == NULL)
		return;
	
	time (&now);
	now_tm = localtime (&now);

	g_date_set_dmy (dt, now_tm->tm_mday, now_tm->tm_mon + 1, now_tm->tm_year + 1900);
}

float
e_card_get_use_score(ECard *card)
{
	GDate today, last_use;
	gint days_since_last_use;

	g_return_val_if_fail (card != NULL && E_IS_CARD (card), 0);

	if (card->last_use == NULL)
		return 0.0;

	e_card_get_today (&today);
	g_date_set_dmy (&last_use, card->last_use->day, card->last_use->month, card->last_use->year);

	days_since_last_use = g_date_julian (&today) - g_date_julian (&last_use);
	
	/* Apply a seven-day "grace period" to the use score decay. */
	days_since_last_use -= 7;
	if (days_since_last_use < 0)
		days_since_last_use = 0;

	return MAX (card->raw_use_score, 0) * exp (- days_since_last_use / 30.0);
}

void
e_card_touch(ECard *card)
{
	GDate today;
	double use_score;

	g_return_if_fail (card != NULL && E_IS_CARD (card));

	e_card_get_today (&today);
	use_score = e_card_get_use_score (card);

	if (card->last_use == NULL)
		card->last_use = g_new (ECardDate, 1);

	card->last_use->day   = g_date_day (&today);
	card->last_use->month = g_date_month (&today);
	card->last_use->year  = g_date_year (&today);

	card->raw_use_score   = use_score + 1.0;
}

/**
 * e_card_get_id:
 * @card: an #ECard
 *
 * Returns: a string representing the id of the card, which is unique
 * within its book.
 */
const char *
e_card_get_id (ECard *card)
{
	g_return_val_if_fail (card && E_IS_CARD (card), NULL);

	return card->id ? card->id : "";
}

/**
 * e_card_get_id:
 * @card: an #ECard
 * @id: a id in string format
 *
 * Sets the identifier of a card, which should be unique within its
 * book.
 */
void
e_card_set_id (ECard *card, const char *id)
{
	g_return_if_fail (card && E_IS_CARD (card));

	if ( card->id )
		g_free(card->id);
	card->id = g_strdup(id ? id : "");
}

EBook *
e_card_get_book (ECard *card)
{
	g_return_val_if_fail (card && E_IS_CARD (card), NULL);

	return card->book;
}

void
e_card_set_book (ECard *card, EBook *book)
{
	g_return_if_fail (card && E_IS_CARD (card));
	
	if (card->book)
		gtk_object_unref (GTK_OBJECT (card->book));
	card->book = book;
	if (card->book)
		gtk_object_ref (GTK_OBJECT (card->book));
}

gchar *
e_card_date_to_string (ECardDate *dt)
{
	if (dt) 
		return g_strdup_printf ("%04d-%02d-%02d",
					CLAMP(dt->year, 1000, 9999),
					CLAMP(dt->month, 1, 12),
					CLAMP(dt->day, 1, 31));
	else
		return NULL;
}

static VObject *
addPropValueUTF8(VObject *o, const char *p, const char *v)
{
	VObject *prop = addPropValue (o, p, v);
	for (; *v; v++) {
		if ((*v) & 0x80) {
			addPropValue (prop, "CHARSET", "UTF-8");
			addProp(prop, VCQuotedPrintableProp);

			return prop;
		}
		if (*v == '\n') {
			addProp(prop, VCQuotedPrintableProp);
			for (; *v; v++) {
				if ((*v) & 0x80) {
					addPropValue (prop, "CHARSET", "UTF-8");
					return prop;
				}
			}
			return prop;
		}
	}
	return prop;
}

static VObject *
addPropValueQP(VObject *o, const char *p, const char *v)
{
	VObject *prop = addPropValue (o, p, v);
	for (; *v; v++) {
		if (*v == '\n') {
			addProp(prop, VCQuotedPrintableProp);
			break;
		}
	}
	return prop;
}

static void
addPropValueSets (VObject *o, const char *p, const char *v, gboolean assumeUTF8, gboolean *is_ascii, gboolean *has_return)
{
	addPropValue (o, p, v);
	if (*has_return && (assumeUTF8 || !*is_ascii))
		return;
	if (*has_return) {
		for (; *v; v++) {
			if (*v & 0x80) {
				*is_ascii = FALSE;
				return;
			}
		}
		return;
	}
	if (assumeUTF8 || !*is_ascii) {
		for (; *v; v++) {
			if (*v == '\n') {
				*has_return = TRUE;
				return;
			}
		}
		return;
	}
	for (; *v; v++) {
		if (*v & 0x80) {
			*is_ascii = FALSE;
			for (; *v; v++) {
				if (*v == '\n') {
					*has_return = TRUE;
					return;
				}
			}
			return;
		}
		if (*v == '\n') {
			*has_return = TRUE;
			for (; *v; v++) {
				if (*v & 0x80) {
					*is_ascii = FALSE;
					return;
				}
			}
			return;
		}
	}
	return;
}

#define ADD_PROP_VALUE(o, p, v)              (assumeUTF8 ? (addPropValueQP ((o), (p), (v))) : addPropValueUTF8 ((o), (p), (v)))
#define ADD_PROP_VALUE_SET_IS_ASCII(o, p, v) (addPropValueSets ((o), (p), (v), assumeUTF8, &is_ascii, &has_return))


static VObject *
e_card_get_vobject (const ECard *card, gboolean assumeUTF8)
{
	VObject *vobj;
	
	vobj = newVObject (VCCardProp);

	ADD_PROP_VALUE(vobj, VCVersionProp, "2.1");

	if (card->file_as && *card->file_as)
		ADD_PROP_VALUE(vobj, "X-EVOLUTION-FILE-AS", card->file_as);
	else if (card->file_as)
		addProp(vobj, "X-EVOLUTION-FILE_AS");

	if (card->fname && *card->fname)
		ADD_PROP_VALUE(vobj, VCFullNameProp, card->fname);
	else if (card->fname)
		addProp(vobj, VCFullNameProp);

	if ( card->name && (card->name->prefix || card->name->given || card->name->additional || card->name->family || card->name->suffix) ) {
		VObject *nameprop;
		gboolean is_ascii = TRUE;
		gboolean has_return = FALSE;
		nameprop = addProp(vobj, VCNameProp);
		if ( card->name->prefix )
			ADD_PROP_VALUE_SET_IS_ASCII(nameprop, VCNamePrefixesProp, card->name->prefix);
		if ( card->name->given ) 
			ADD_PROP_VALUE_SET_IS_ASCII(nameprop, VCGivenNameProp, card->name->given);
		if ( card->name->additional )
			ADD_PROP_VALUE_SET_IS_ASCII(nameprop, VCAdditionalNamesProp, card->name->additional);
		if ( card->name->family )
			ADD_PROP_VALUE_SET_IS_ASCII(nameprop, VCFamilyNameProp, card->name->family);
		if ( card->name->suffix )
			ADD_PROP_VALUE_SET_IS_ASCII(nameprop, VCNameSuffixesProp, card->name->suffix);
		if (has_return)
			addProp(nameprop, VCQuotedPrintableProp);
		if (!(is_ascii || assumeUTF8))
			addPropValue (nameprop, "CHARSET", "UTF-8");
	}
	else if (card->name)
		addProp(vobj, VCNameProp);


	if ( card->address ) {
		EIterator *iterator = e_list_get_iterator(card->address);
		for ( ; e_iterator_is_valid(iterator) ;e_iterator_next(iterator) ) {
			VObject *addressprop;
			ECardDeliveryAddress *address = (ECardDeliveryAddress *) e_iterator_get(iterator);
			gboolean is_ascii = TRUE;
			gboolean has_return = FALSE;

			addressprop = addProp(vobj, VCAdrProp);
			
			set_address_flags (addressprop, address->flags);
			if (address->po)
				ADD_PROP_VALUE_SET_IS_ASCII(addressprop, VCPostalBoxProp, address->po);
			if (address->ext)
				ADD_PROP_VALUE_SET_IS_ASCII(addressprop, VCExtAddressProp, address->ext);
			if (address->street)
				ADD_PROP_VALUE_SET_IS_ASCII(addressprop, VCStreetAddressProp, address->street);
			if (address->city)
				ADD_PROP_VALUE_SET_IS_ASCII(addressprop, VCCityProp, address->city);
			if (address->region)
				ADD_PROP_VALUE_SET_IS_ASCII(addressprop, VCRegionProp, address->region);
			if (address->code)
				ADD_PROP_VALUE_SET_IS_ASCII(addressprop, VCPostalCodeProp, address->code);
			if (address->country)
				ADD_PROP_VALUE_SET_IS_ASCII(addressprop, VCCountryNameProp, address->country);

			if (has_return)
				addProp(addressprop, VCQuotedPrintableProp);
			if (!(is_ascii || assumeUTF8))
				addPropValue (addressprop, "CHARSET", "UTF-8");
		}
		gtk_object_unref(GTK_OBJECT(iterator));
	}

	if ( card->address_label ) {
		EIterator *iterator = e_list_get_iterator(card->address_label);
		for ( ; e_iterator_is_valid(iterator) ;e_iterator_next(iterator) ) {
			VObject *labelprop;
			ECardAddrLabel *address_label = (ECardAddrLabel *) e_iterator_get(iterator);
			if (address_label->data)
				labelprop = ADD_PROP_VALUE(vobj, VCDeliveryLabelProp, address_label->data);
			else
				labelprop = addProp(vobj, VCDeliveryLabelProp);
			
			set_address_flags (labelprop, address_label->flags);
		}
		gtk_object_unref(GTK_OBJECT(iterator));
	}

	if ( card->phone ) { 
		EIterator *iterator = e_list_get_iterator(card->phone);
		for ( ; e_iterator_is_valid(iterator) ;e_iterator_next(iterator) ) {
			VObject *phoneprop;
			ECardPhone *phone = (ECardPhone *) e_iterator_get(iterator);
			phoneprop = ADD_PROP_VALUE(vobj, VCTelephoneProp, phone->number);
			
			set_phone_flags (phoneprop, phone->flags);
		}
		gtk_object_unref(GTK_OBJECT(iterator));
	}

	if ( card->email ) { 
		EIterator *iterator = e_list_get_iterator(card->email);
		for ( ; e_iterator_is_valid(iterator) ;e_iterator_next(iterator) ) {
			VObject *emailprop;
			emailprop = ADD_PROP_VALUE(vobj, VCEmailAddressProp, (char *) e_iterator_get(iterator));
			addProp (emailprop, VCInternetProp);
		}
		gtk_object_unref(GTK_OBJECT(iterator));
	}

	if ( card->bday ) {
		char *value;
		value = e_card_date_to_string (card->bday);
		ADD_PROP_VALUE(vobj, VCBirthDateProp, value);
		g_free(value);
	}

	if (card->url)
		ADD_PROP_VALUE(vobj, VCURLProp, card->url);

	if (card->org || card->org_unit) {
		VObject *orgprop;
		gboolean is_ascii = TRUE;
		gboolean has_return = FALSE;
		orgprop = addProp(vobj, VCOrgProp);
		
		if (card->org)
			ADD_PROP_VALUE_SET_IS_ASCII(orgprop, VCOrgNameProp, card->org);
		if (card->org_unit)
			ADD_PROP_VALUE_SET_IS_ASCII(orgprop, VCOrgUnitProp, card->org_unit);

		if (has_return)
			addProp(orgprop, VCQuotedPrintableProp);
		if (!(is_ascii || assumeUTF8))
			addPropValue (orgprop, "CHARSET", "UTF-8");
	}
	
	if (card->office)
		ADD_PROP_VALUE(vobj, "X-EVOLUTION-OFFICE", card->office);

	if (card->title)
		ADD_PROP_VALUE(vobj, VCTitleProp, card->title);

	if (card->role)
		ADD_PROP_VALUE(vobj, VCBusinessRoleProp, card->role);
	
	if (card->manager)
		ADD_PROP_VALUE(vobj, "X-EVOLUTION-MANAGER", card->manager);
	
	if (card->assistant)
		ADD_PROP_VALUE(vobj, "X-EVOLUTION-ASSISTANT", card->assistant);
	
	if (card->nickname)
		ADD_PROP_VALUE(vobj, "NICKNAME", card->nickname);

	if (card->spouse)
		ADD_PROP_VALUE(vobj, "X-EVOLUTION-SPOUSE", card->spouse);

	if ( card->anniversary ) {
		char *value;
		value = e_card_date_to_string (card->anniversary);
		ADD_PROP_VALUE(vobj, "X-EVOLUTION-ANNIVERSARY", value);
		g_free(value);
	}

	if (card->mailer) {
		ADD_PROP_VALUE(vobj, VCMailerProp, card->mailer);
	}
	
	if (card->caluri)
		addPropValueQP(vobj, "CALURI", card->caluri);

	if (card->fburl)
		ADD_PROP_VALUE(vobj, "FBURL", card->fburl);

	if (card->icscalendar)
		ADD_PROP_VALUE(vobj, "ICSCALENDAR", card->icscalendar);
	
	if (card->note) {
		VObject *noteprop;

		noteprop = ADD_PROP_VALUE(vobj, VCNoteProp, card->note);
	}

	if (card->last_use) {
		char *value;
		value = e_card_date_to_string (card->last_use);
		ADD_PROP_VALUE (vobj, "X-EVOLUTION-LAST-USE", value);
		g_free (value);
	}

	if (card->raw_use_score > 0) {
		char *value;
		value = g_strdup_printf ("%f", card->raw_use_score);
		ADD_PROP_VALUE (vobj, "X-EVOLUTION-USE-SCORE", value);
		g_free (value);
	}

	if (card->related_contacts && *card->related_contacts) {
		ADD_PROP_VALUE(vobj, XEV_RELATED_CONTACTS, card->related_contacts);
	}

	if (card->categories) {
		EIterator *iterator;
		int length = 0;
		char *string;
		char *stringptr;
		for (iterator = e_list_get_iterator(card->categories); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			length += strlen(e_iterator_get(iterator)) + 1;
		}
		string = g_new(char, length + 1);
		stringptr = string;
		*stringptr = 0;
		for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			strcpy(stringptr, e_iterator_get(iterator));
			stringptr += strlen(stringptr);
			*stringptr = ',';
			stringptr++;
			*stringptr = 0;
		}
		if (stringptr > string) {
			stringptr --;
			*stringptr = 0;
		}
		ADD_PROP_VALUE (vobj, "CATEGORIES", string);
		g_free(string);
	}

	if (card->wants_html_set) {
		ADD_PROP_VALUE (vobj, XEV_WANTS_HTML, card->wants_html ? "TRUE" : "FALSE");
	}

	if (card->list) {
		ADD_PROP_VALUE (vobj, XEV_LIST, "TRUE");
		ADD_PROP_VALUE (vobj, XEV_LIST_SHOW_ADDRESSES, card->list_show_addresses ? "TRUE" : "FALSE");
	}

	if (card->arbitrary) {
		EIterator *iterator;
		for (iterator = e_list_get_iterator(card->arbitrary); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			const ECardArbitrary *arbitrary = e_iterator_get(iterator);
			VObject *arb_object;
			if (arbitrary->value) {
				arb_object = ADD_PROP_VALUE (vobj, XEV_ARBITRARY, arbitrary->value);
			} else {
				arb_object = addProp (vobj, XEV_ARBITRARY);
			}
			if (arbitrary->type) {
				ADD_PROP_VALUE (arb_object, "TYPE", arbitrary->type);
			}
			if (arbitrary->key) {
				addProp (arb_object, arbitrary->key);
			}
		}
	}

	addPropValueQP (vobj, VCUniqueStringProp, (card->id ? card->id : ""));

	return vobj;
}

/**
 * e_card_get_vcard:
 * @card: an #ECard
 *
 * Returns: a string in vCard format, which is wrapped by the @card.
 */
char *
e_card_get_vcard (ECard *card)
{
	VObject *vobj;
	char *temp, *ret_val;

	vobj = e_card_get_vobject (card, FALSE);
	temp = writeMemVObject(NULL, NULL, vobj);
	ret_val = g_strdup(temp);
	free(temp);
	cleanVObject(vobj);
	return ret_val;
}

char *
e_card_get_vcard_assume_utf8 (ECard *card)
{
	VObject *vobj;
	char *temp, *ret_val;

	vobj = e_card_get_vobject (card, TRUE);
	temp = writeMemVObject(NULL, NULL, vobj);
	ret_val = g_strdup(temp);
	free(temp);
	cleanVObject(vobj);
	return ret_val;
}

/**
 * e_card_list_get_vcard:
 * @list: a list of #ECards
 *
 * Returns: a string in vCard format.
 */
char *
e_card_list_get_vcard (const GList *list)
{
	VObject *vobj;

	char *temp, *ret_val;

	vobj = NULL;

	for (; list; list = list->next) {
		VObject *tempvobj;
		ECard *card = list->data;

		tempvobj = e_card_get_vobject (card, FALSE);
		addList (&vobj, tempvobj);
	}
	temp = writeMemVObjects(NULL, NULL, vobj);
	ret_val = g_strdup(temp);
	free(temp);
	cleanVObjects(vobj);
	return ret_val;
}

static void
parse_file_as(ECard *card, VObject *vobj, char *default_charset)
{
	if ( card->file_as )
		g_free(card->file_as);
	assign_string(vobj, default_charset, &(card->file_as));
}

static void
parse_name(ECard *card, VObject *vobj, char *default_charset)
{
	e_card_name_unref(card->name);

	card->name = e_card_name_new();

	card->name->family     = e_v_object_get_child_value (vobj, VCFamilyNameProp,      default_charset);
	card->name->given      = e_v_object_get_child_value (vobj, VCGivenNameProp,       default_charset);
	card->name->additional = e_v_object_get_child_value (vobj, VCAdditionalNamesProp, default_charset);
	card->name->prefix     = e_v_object_get_child_value (vobj, VCNamePrefixesProp,    default_charset);
	card->name->suffix     = e_v_object_get_child_value (vobj, VCNameSuffixesProp,    default_charset);
}

static void
parse_full_name(ECard *card, VObject *vobj, char *default_charset)
{
	if ( card->fname )
		g_free(card->fname);
	assign_string(vobj, default_charset, &(card->fname));
}

static void
parse_email(ECard *card, VObject *vobj, char *default_charset)
{
	char *next_email;
	EList *list;

	assign_string(vobj, default_charset, &next_email);
	gtk_object_get(GTK_OBJECT(card),
		       "email", &list,
		       NULL);
	e_list_append(list, next_email);
	g_free (next_email);
}

/* Deal with charset */
static void
parse_bday(ECard *card, VObject *vobj, char *default_charset)
{
	if ( vObjectValueType (vobj) ) {
		char *str = fakeCString (vObjectUStringZValue (vobj));
		if ( card->bday )
			g_free(card->bday);
		card->bday = g_new(ECardDate, 1);
		*(card->bday) = e_card_date_from_string(str);
		free(str);
	}
}

static void
parse_phone(ECard *card, VObject *vobj, char *default_charset)
{
	ECardPhone *next_phone = e_card_phone_new ();
	EList *list;

	assign_string(vobj, default_charset, &(next_phone->number));
	next_phone->flags = get_phone_flags(vobj);

	gtk_object_get(GTK_OBJECT(card),
		       "phone", &list,
		       NULL);
	e_list_append(list, next_phone);
	e_card_phone_unref (next_phone);
}

static void
parse_address(ECard *card, VObject *vobj, char *default_charset)
{
	ECardDeliveryAddress *next_addr = e_card_delivery_address_new ();
	EList *list;

	next_addr->flags   = get_address_flags (vobj);
	next_addr->po      = e_v_object_get_child_value (vobj, VCPostalBoxProp,     default_charset);
	next_addr->ext     = e_v_object_get_child_value (vobj, VCExtAddressProp,    default_charset);
	next_addr->street  = e_v_object_get_child_value (vobj, VCStreetAddressProp, default_charset);
	next_addr->city    = e_v_object_get_child_value (vobj, VCCityProp,          default_charset);
	next_addr->region  = e_v_object_get_child_value (vobj, VCRegionProp,        default_charset);
	next_addr->code    = e_v_object_get_child_value (vobj, VCPostalCodeProp,    default_charset);
	next_addr->country = e_v_object_get_child_value (vobj, VCCountryNameProp,   default_charset);

	gtk_object_get(GTK_OBJECT(card),
		       "address", &list,
		       NULL);
	e_list_append(list, next_addr);
	e_card_delivery_address_unref (next_addr);
}

static void
parse_address_label(ECard *card, VObject *vobj, char *default_charset)
{
	ECardAddrLabel *next_addr = e_card_address_label_new ();
	EList *list;

	next_addr->flags   = get_address_flags (vobj);
	assign_string(vobj, default_charset, &next_addr->data);

	gtk_object_get(GTK_OBJECT(card),
		       "address_label", &list,
		       NULL);
	e_list_append(list, next_addr);
	e_card_address_label_unref (next_addr);
}

static void
parse_url(ECard *card, VObject *vobj, char *default_charset)
{
	if (card->url)
		g_free(card->url);
	assign_string(vobj, default_charset, &(card->url));
}

static void
parse_org(ECard *card, VObject *vobj, char *default_charset)
{
	char *temp;
	
	temp = e_v_object_get_child_value(vobj, VCOrgNameProp, default_charset);
	g_free(card->org);
	card->org = temp;

	temp = e_v_object_get_child_value(vobj, VCOrgUnitProp, default_charset);
	g_free(card->org_unit);
	card->org_unit = temp;
}

static void
parse_office(ECard *card, VObject *vobj, char *default_charset)
{
	if ( card->office )
		g_free(card->office);
	assign_string(vobj, default_charset, &(card->office));
}

static void
parse_title(ECard *card, VObject *vobj, char *default_charset)
{
	if ( card->title )
		g_free(card->title);
	assign_string(vobj, default_charset, &(card->title));
}

static void
parse_role(ECard *card, VObject *vobj, char *default_charset)
{
	if (card->role)
		g_free(card->role);
	assign_string(vobj, default_charset, &(card->role));
}

static void
parse_manager(ECard *card, VObject *vobj, char *default_charset)
{
	if ( card->manager )
		g_free(card->manager);
	assign_string(vobj, default_charset, &(card->manager));
}

static void
parse_assistant(ECard *card, VObject *vobj, char *default_charset)
{
	if ( card->assistant )
		g_free(card->assistant);
	assign_string(vobj, default_charset, &(card->assistant));
}

static void
parse_nickname(ECard *card, VObject *vobj, char *default_charset)
{
	if (card->nickname)
		g_free(card->nickname);
	assign_string(vobj, default_charset, &(card->nickname));
}

static void
parse_spouse(ECard *card, VObject *vobj, char *default_charset)
{
	if ( card->spouse )
		g_free(card->spouse);
	assign_string(vobj, default_charset, &(card->spouse));
}

/* Deal with charset */
static void
parse_anniversary(ECard *card, VObject *vobj, char *default_charset)
{
	if ( vObjectValueType (vobj) ) {
		char *str = fakeCString (vObjectUStringZValue (vobj));
		if (card->anniversary)
			g_free(card->anniversary);
		card->anniversary = g_new(ECardDate, 1);
		*(card->anniversary) = e_card_date_from_string(str);
		free(str);
	}
}

static void
parse_mailer(ECard *card, VObject *vobj, char *default_charset)
{
	if ( card->mailer )
		g_free(card->mailer);
	assign_string(vobj, default_charset, &(card->mailer));
}

static void
parse_caluri(ECard *card, VObject *vobj, char *default_charset)
{
 	g_free(card->caluri);
 	assign_string(vobj, default_charset, &(card->caluri));
}

static void
parse_fburl(ECard *card, VObject *vobj, char *default_charset)
{
	g_free(card->fburl);
	assign_string(vobj, default_charset, &(card->fburl));
}

static void
parse_icscalendar(ECard *card, VObject *vobj, char *default_charset)
{
	g_free(card->icscalendar);
	assign_string(vobj, default_charset, &(card->icscalendar));
}

static void
parse_note(ECard *card, VObject *vobj, char *default_charset)
{
	g_free(card->note);
	assign_string(vobj, default_charset, &(card->note));
}

static void
parse_related_contacts(ECard *card, VObject *vobj, char *default_charset)
{
	g_free(card->related_contacts);
	assign_string(vobj, default_charset, &(card->related_contacts));
}

static void
add_list_unique(ECard *card, EList *list, char *string)
{
	char *temp = e_strdup_strip(string);
	EIterator *iterator;

	if (!*temp) {
		g_free(temp);
		return;
	}
	for ( iterator = e_list_get_iterator(list); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
		if (!strcmp(e_iterator_get(iterator), temp)) {
			break;
		}
	}
	if (!e_iterator_is_valid(iterator)) {
		e_list_append(list, temp);
	}
	g_free(temp);
	gtk_object_unref(GTK_OBJECT(iterator));
}

static void
do_parse_categories(ECard *card, char *str)
{
	int length = strlen(str);
	char *copy = g_new(char, length + 1);
	int i, j;
	EList *list;
	gtk_object_get(GTK_OBJECT(card),
		       "category_list", &list,
		       NULL);
	for (i = 0, j = 0; str[i]; i++, j++) {
		switch (str[i]) {
		case '\\':
			i++;
			if (str[i]) {
				copy[j] = str[i];
			} else
				i--;
			break;
		case ',':
			copy[j] = 0;
			add_list_unique(card, list, copy);
			j = -1;
			break;
		default:
			copy[j] = str[i];
			break;
		}
	}
	copy[j] = 0;
	add_list_unique(card, list, copy);
	g_free(copy);
}

/* Deal with charset */
static void
parse_categories(ECard *card, VObject *vobj, char *default_charset)
{
	if ( vObjectValueType (vobj) ) {
		char *str = fakeCString (vObjectUStringZValue (vobj));
		do_parse_categories(card, str);
		free(str);
	}
}

/* Deal with charset */
static void
parse_wants_html(ECard *card, VObject *vobj, char *default_charset)
{
	if ( vObjectValueType (vobj) ) {
		char *str = fakeCString (vObjectUStringZValue (vobj));
		if (!strcasecmp(str, "true")) {
			card->wants_html = TRUE;
			card->wants_html_set = TRUE;
		}
		if (!strcasecmp(str, "false")) {
			card->wants_html = FALSE;
			card->wants_html_set = TRUE;
		}
		free(str);
	}
}

/* Deal with charset */
static void
parse_list(ECard *card, VObject *vobj, char *default_charset)
{
	if ( vObjectValueType (vobj) ) {
		char *str = fakeCString (vObjectUStringZValue (vobj));
		if (!strcasecmp(str, "true")) {
			card->list = TRUE;
		}
		if (!strcasecmp(str, "false")) {
			card->list = FALSE;
		}
		free(str);
	}
}

/* Deal with charset */
static void
parse_list_show_addresses(ECard *card, VObject *vobj, char *default_charset)
{
	if ( vObjectValueType (vobj) ) {
		char *str = fakeCString (vObjectUStringZValue (vobj));
		if (!strcasecmp(str, "true")) {
			card->list_show_addresses = TRUE;
		}
		if (!strcasecmp(str, "false")) {
			card->list_show_addresses = FALSE;
		}
		free(str);
	}
}

typedef union ValueItem {
    const char *strs;
    const wchar_t *ustrs;
    unsigned int i;
    unsigned long l;
    void *any;
    VObject *vobj;
} ValueItem;

struct VObject {
    VObject *next;
    const char *id;
    VObject *prop;
    unsigned short valType;
    ValueItem val;
};

static void
parse_arbitrary(ECard *card, VObject *vobj, char *default_charset)
{
	ECardArbitrary *arbitrary = e_card_arbitrary_new();
	VObjectIterator iterator;
	EList *list;
	for ( initPropIterator (&iterator, vobj); moreIteration(&iterator); ) {
		VObject *temp = nextVObject(&iterator);
		const char *name = vObjectName(temp);
		if (name && !strcmp(name, "TYPE")) {
			g_free(arbitrary->type);
			assign_string(temp, default_charset, &(arbitrary->type));
		} else {
			g_free(arbitrary->key);
			arbitrary->key = g_strdup(name);
		}
	}

	assign_string(vobj, default_charset, &(arbitrary->value));
	
	gtk_object_get(GTK_OBJECT(card),
		       "arbitrary", &list,
		       NULL);
	e_list_append(list, arbitrary);
	e_card_arbitrary_unref(arbitrary);
}

static void
parse_id(ECard *card, VObject *vobj, char *default_charset)
{
	g_free(card->id);
	assign_string(vobj, default_charset, &(card->id));
}

/* Deal with charset */
static void
parse_last_use(ECard *card, VObject *vobj, char *default_charset)
{
	if ( vObjectValueType (vobj) ) {
		char *str = fakeCString (vObjectUStringZValue (vobj));
		if ( card->last_use )
			g_free(card->last_use);
		card->last_use = g_new(ECardDate, 1);
		*(card->last_use) = e_card_date_from_string(str);
		free(str);
	}
}

/* Deal with charset */
static void
parse_use_score(ECard *card, VObject *vobj, char *default_charset)
{
	card->raw_use_score = 0;
	
	if ( vObjectValueType (vobj) ) {
		char *str = fakeCString (vObjectUStringZValue (vobj));
		card->raw_use_score = MAX(0, atof (str));
		free (str);
	}
}

static void
parse_attribute(ECard *card, VObject *vobj, char *default_charset)
{
	ParsePropertyFunc function = g_hash_table_lookup(E_CARD_CLASS(GTK_OBJECT(card)->klass)->attribute_jump_table, vObjectName(vobj));
	if ( function )
		function(card, vobj, default_charset);
}

static void
parse(ECard *card, VObject *vobj, char *default_charset)
{
	VObjectIterator iterator;
	initPropIterator(&iterator, vobj);
	while(moreIteration (&iterator)) {
		parse_attribute(card, nextVObject(&iterator), default_charset);
	}
	if (!card->fname) {
		card->fname = g_strdup("");
	}
	if (!card->name) {
		card->name = e_card_name_from_string(card->fname);
	}
	if (!card->file_as) {
		ECardName *name = card->name;
		char *strings[3], **stringptr;
		char *string;
		stringptr = strings;
		if (name->family && *name->family)
			*(stringptr++) = name->family;
		if (name->given && *name->given)
			*(stringptr++) = name->given;
		*stringptr = NULL;
		string = g_strjoinv(", ", strings);
		card->file_as = string;
	}
}

static void
e_card_class_init (ECardClass *klass)
{
	int i;
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS(klass);

	klass->attribute_jump_table = g_hash_table_new(g_str_hash, g_str_equal);

	for ( i = 0; i < sizeof(attribute_jump_array) / sizeof(attribute_jump_array[0]); i++ ) {
		g_hash_table_insert(klass->attribute_jump_table, attribute_jump_array[i].key, attribute_jump_array[i].function);
	}

	gtk_object_add_arg_type ("ECard::file_as",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_FILE_AS);
	gtk_object_add_arg_type ("ECard::full_name",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_FULL_NAME);  
	gtk_object_add_arg_type ("ECard::name",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_NAME);
	gtk_object_add_arg_type ("ECard::address",
				 GTK_TYPE_OBJECT, GTK_ARG_READABLE, ARG_ADDRESS);
	gtk_object_add_arg_type ("ECard::address_label",
				 GTK_TYPE_OBJECT, GTK_ARG_READABLE, ARG_ADDRESS_LABEL);
	gtk_object_add_arg_type ("ECard::phone",
				 GTK_TYPE_OBJECT, GTK_ARG_READABLE, ARG_PHONE);
	gtk_object_add_arg_type ("ECard::email",
				 GTK_TYPE_OBJECT, GTK_ARG_READABLE, ARG_EMAIL);
	gtk_object_add_arg_type ("ECard::birth_date",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_BIRTH_DATE);
	gtk_object_add_arg_type ("ECard::url",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_URL);  
	gtk_object_add_arg_type ("ECard::org",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ORG);
	gtk_object_add_arg_type ("ECard::org_unit",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ORG_UNIT);
	gtk_object_add_arg_type ("ECard::office",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_OFFICE);
	gtk_object_add_arg_type ("ECard::title",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_TITLE);  
	gtk_object_add_arg_type ("ECard::role",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ROLE);
	gtk_object_add_arg_type ("ECard::manager",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_MANAGER);
	gtk_object_add_arg_type ("ECard::assistant",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ASSISTANT);
	gtk_object_add_arg_type ("ECard::nickname",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_NICKNAME);
	gtk_object_add_arg_type ("ECard::spouse",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_SPOUSE);
	gtk_object_add_arg_type ("ECard::anniversary",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_ANNIVERSARY);
	gtk_object_add_arg_type ("ECard::mailer",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_MAILER);
	gtk_object_add_arg_type ("ECard::caluri",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_CALURI);
	gtk_object_add_arg_type ("ECard::fburl",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_FBURL);
	gtk_object_add_arg_type ("ECard::icscalendar",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ICSCALENDAR);
	gtk_object_add_arg_type ("ECard::note",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_NOTE);
	gtk_object_add_arg_type ("ECard::related_contacts",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_RELATED_CONTACTS);
	gtk_object_add_arg_type ("ECard::categories",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_CATEGORIES);
	gtk_object_add_arg_type ("ECard::category_list",
				 GTK_TYPE_OBJECT, GTK_ARG_READWRITE, ARG_CATEGORY_LIST);
	gtk_object_add_arg_type ("ECard::wants_html",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_WANTS_HTML);
	gtk_object_add_arg_type ("ECard::wants_html_set",
				 GTK_TYPE_BOOL, GTK_ARG_READABLE, ARG_WANTS_HTML);
	gtk_object_add_arg_type ("ECard::list",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_EVOLUTION_LIST);
	gtk_object_add_arg_type ("ECard::list_show_addresses",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_EVOLUTION_LIST_SHOW_ADDRESSES);
	gtk_object_add_arg_type ("ECard::arbitrary",
				 GTK_TYPE_OBJECT, GTK_ARG_READWRITE, ARG_ARBITRARY);
	gtk_object_add_arg_type ("ECard::id",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ID);
	gtk_object_add_arg_type ("ECard::last_use",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_LAST_USE);
	gtk_object_add_arg_type ("ECard::use_score",
				 GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_USE_SCORE);


	object_class->destroy = e_card_destroy;
	object_class->get_arg = e_card_get_arg;
	object_class->set_arg = e_card_set_arg;
}

ECardPhone *
e_card_phone_new (void)
{
	ECardPhone *newphone = g_new(ECardPhone, 1);

	newphone->ref_count = 1;
	newphone->number = NULL;
	newphone->flags = 0;
	
	return newphone;
}

void
e_card_phone_unref (ECardPhone *phone)
{
	if (phone) {
		phone->ref_count --;
		if (phone->ref_count == 0) {
			g_free(phone->number);
			g_free(phone);
		}
	}
}

ECardPhone *
e_card_phone_ref (const ECardPhone *phone)
{
	ECardPhone *phone_mutable = (ECardPhone *) phone;
	if (phone_mutable)
		phone_mutable->ref_count ++;
	return phone_mutable;
}

ECardPhone *
e_card_phone_copy (const ECardPhone *phone)
{
	if ( phone ) {
		ECardPhone *phone_copy = e_card_phone_new();
		phone_copy->number = g_strdup(phone->number);
		phone_copy->flags  = phone->flags;
		return phone_copy;
	} else
		return NULL;
}

ECardDeliveryAddress *
e_card_delivery_address_new (void)
{
	ECardDeliveryAddress *newaddr = g_new(ECardDeliveryAddress, 1);

	newaddr->ref_count = 1;
	newaddr->po      = NULL;
	newaddr->ext     = NULL;
	newaddr->street  = NULL;
	newaddr->city    = NULL;
	newaddr->region  = NULL;
	newaddr->code    = NULL;
	newaddr->country = NULL;
	newaddr->flags   = 0;

	return newaddr;
}

void
e_card_delivery_address_unref (ECardDeliveryAddress *addr)
{
	if ( addr ) {
		addr->ref_count --;
		if (addr->ref_count == 0) {
			g_free(addr->po);
			g_free(addr->ext);
			g_free(addr->street);
			g_free(addr->city);
			g_free(addr->region);
			g_free(addr->code);
			g_free(addr->country);
			g_free(addr);
		}
	}
}

ECardDeliveryAddress *
e_card_delivery_address_ref (const ECardDeliveryAddress *addr)
{
	ECardDeliveryAddress *addr_mutable = (ECardDeliveryAddress *) addr;
	if (addr_mutable)
		addr_mutable->ref_count ++;
	return addr_mutable;
}

ECardDeliveryAddress *
e_card_delivery_address_copy (const ECardDeliveryAddress *addr)
{
	if ( addr ) {
		ECardDeliveryAddress *addr_copy = e_card_delivery_address_new ();
		addr_copy->po      = g_strdup(addr->po     );
		addr_copy->ext     = g_strdup(addr->ext    );
		addr_copy->street  = g_strdup(addr->street );
		addr_copy->city    = g_strdup(addr->city   );
		addr_copy->region  = g_strdup(addr->region );
		addr_copy->code    = g_strdup(addr->code   );
		addr_copy->country = g_strdup(addr->country);
		addr_copy->flags   = addr->flags;
		return addr_copy;
	} else
		return NULL;
}

gboolean
e_card_delivery_address_is_empty (const ECardDeliveryAddress *addr)
{
	return (((addr->po      == NULL) || (*addr->po      == 0)) &&
		((addr->ext     == NULL) || (*addr->ext     == 0)) &&
		((addr->street  == NULL) || (*addr->street  == 0)) &&
		((addr->city    == NULL) || (*addr->city    == 0)) &&
		((addr->region  == NULL) || (*addr->region  == 0)) &&
		((addr->code    == NULL) || (*addr->code    == 0)) &&
		((addr->country == NULL) || (*addr->country == 0)));
}

ECardDeliveryAddress *
e_card_delivery_address_from_label(const ECardAddrLabel *label)
{
	ECardDeliveryAddress *addr = e_card_delivery_address_new ();
	EAddressWestern *western = e_address_western_parse (label->data);
	
	addr->po      = g_strdup (western->po_box     );
	addr->ext     = g_strdup (western->extended   );
	addr->street  = g_strdup (western->street     );
	addr->city    = g_strdup (western->locality   );
	addr->region  = g_strdup (western->region     );
	addr->code    = g_strdup (western->postal_code);
	addr->country = g_strdup (western->country    );
	addr->flags   = label->flags;
	
	e_address_western_free(western);
	
	return addr;
}

char *
e_card_delivery_address_to_string(const ECardDeliveryAddress *addr)
{
	char *strings[5], **stringptr = strings;
	char *line1, *line22, *line2;
	char *final;
	if (addr->po && *addr->po)
		*(stringptr++) = addr->po;
	if (addr->street && *addr->street)
		*(stringptr++) = addr->street;
	*stringptr = NULL;
	line1 = g_strjoinv(" ", strings);
	stringptr = strings;
	if (addr->region && *addr->region)
		*(stringptr++) = addr->region;
	if (addr->code && *addr->code)
		*(stringptr++) = addr->code;
	*stringptr = NULL;
	line22 = g_strjoinv(" ", strings);
	stringptr = strings;
	if (addr->city && *addr->city)
		*(stringptr++) = addr->city;
	if (line22 && *line22)
		*(stringptr++) = line22;
	*stringptr = NULL;
	line2 = g_strjoinv(", ", strings);
	stringptr = strings;
	if (line1 && *line1)
		*(stringptr++) = line1;
	if (addr->ext && *addr->ext)
		*(stringptr++) = addr->ext;
	if (line2 && *line2)
		*(stringptr++) = line2;
	if (addr->country && *addr->country)
		*(stringptr++) = addr->country;
	*stringptr = NULL;
	final = g_strjoinv("\n", strings);
	g_free(line1);
	g_free(line22);
	g_free(line2);
	return final;
}

ECardAddrLabel *
e_card_delivery_address_to_label    (const ECardDeliveryAddress *addr)
{
	ECardAddrLabel *label;
	label = e_card_address_label_new();
	label->flags = addr->flags;
	label->data = e_card_delivery_address_to_string(addr);

	return label;
}

ECardAddrLabel *
e_card_address_label_new (void)
{
	ECardAddrLabel *newaddr = g_new(ECardAddrLabel, 1);

	newaddr->ref_count = 1;
	newaddr->data = NULL;
	newaddr->flags = 0;
	
	return newaddr;
}

void
e_card_address_label_unref (ECardAddrLabel *addr)
{
	if (addr) {
		addr->ref_count --;
		if (addr->ref_count == 0) {
			g_free(addr->data);
			g_free(addr);
		}
	}
}

ECardAddrLabel *
e_card_address_label_ref (const ECardAddrLabel *addr)
{
	ECardAddrLabel *addr_mutable = (ECardAddrLabel *) addr;
	if (addr_mutable)
		addr_mutable->ref_count ++;
	return addr_mutable;
}

ECardAddrLabel *
e_card_address_label_copy (const ECardAddrLabel *addr)
{
	if ( addr ) {
		ECardAddrLabel *addr_copy = e_card_address_label_new ();
		addr_copy->data  = g_strdup(addr->data);
		addr_copy->flags = addr->flags;
		return addr_copy;
	} else
		return NULL;
}

ECardName *e_card_name_new(void)
{
	ECardName *newname  = g_new(ECardName, 1);

	newname->ref_count  = 1;
	newname->prefix     = NULL;
	newname->given      = NULL;
	newname->additional = NULL;
	newname->family     = NULL;
	newname->suffix     = NULL;

	return newname;
}

void
e_card_name_unref(ECardName *name)
{
	if (name) {
		name->ref_count --;
		if (name->ref_count == 0) {
			g_free (name->prefix);
			g_free (name->given);
			g_free (name->additional);
			g_free (name->family);
			g_free (name->suffix);
			g_free (name);
		}
	}
}

ECardName *
e_card_name_ref(const ECardName *name)
{
	ECardName *name_mutable = (ECardName *) name;
	if (name_mutable)
		name_mutable->ref_count ++;
	return name_mutable;
}

ECardName *
e_card_name_copy(const ECardName *name)
{
	if (name) {
		ECardName *newname = e_card_name_new ();
               
		newname->prefix = g_strdup(name->prefix);
		newname->given = g_strdup(name->given);
		newname->additional = g_strdup(name->additional);
		newname->family = g_strdup(name->family);
		newname->suffix = g_strdup(name->suffix);

		return newname;
	} else
		return NULL;
}


char *
e_card_name_to_string(const ECardName *name)
{
	char *strings[6], **stringptr = strings;

	g_return_val_if_fail (name != NULL, NULL);

	if (name->prefix && *name->prefix)
		*(stringptr++) = name->prefix;
	if (name->given && *name->given)
		*(stringptr++) = name->given;
	if (name->additional && *name->additional)
		*(stringptr++) = name->additional;
	if (name->family && *name->family)
		*(stringptr++) = name->family;
	if (name->suffix && *name->suffix)
		*(stringptr++) = name->suffix;
	*stringptr = NULL;
	return g_strjoinv(" ", strings);
}

ECardName *
e_card_name_from_string(const char *full_name)
{
	ECardName *name = e_card_name_new ();
	ENameWestern *western = e_name_western_parse (full_name);
	
	name->prefix     = g_strdup (western->prefix);
	name->given      = g_strdup (western->first );
	name->additional = g_strdup (western->middle);
	name->family     = g_strdup (western->last  );
	name->suffix     = g_strdup (western->suffix);
	
	e_name_western_free(western);
	
	return name;
}

ECardArbitrary *
e_card_arbitrary_new(void)
{
	ECardArbitrary *arbitrary = g_new(ECardArbitrary, 1);
	arbitrary->ref_count = 1;
	arbitrary->key = NULL;
	arbitrary->type = NULL;
	arbitrary->value = NULL;
	return arbitrary;
}

void
e_card_arbitrary_unref(ECardArbitrary *arbitrary)
{
	if (arbitrary) {
		arbitrary->ref_count --;
		if (arbitrary->ref_count == 0) {
			g_free(arbitrary->key);
			g_free(arbitrary->type);
			g_free(arbitrary->value);
			g_free(arbitrary);
		}
	}
}

ECardArbitrary *
e_card_arbitrary_copy(const ECardArbitrary *arbitrary)
{
	if (arbitrary) {
		ECardArbitrary *arb_copy = e_card_arbitrary_new ();
		arb_copy->key = g_strdup(arbitrary->key);
		arb_copy->type = g_strdup(arbitrary->type);
		arb_copy->value = g_strdup(arbitrary->value);
		return arb_copy;
	} else
		return NULL;
}

ECardArbitrary *
e_card_arbitrary_ref(const ECardArbitrary *arbitrary)
{
	ECardArbitrary *arbitrary_mutable = (ECardArbitrary *) arbitrary;
	if (arbitrary_mutable)
		arbitrary_mutable->ref_count ++;
	return arbitrary_mutable;
}

/* EMail matching */
static gboolean
e_card_email_match_single_string (const gchar *a, const gchar *b)
{
	const gchar *xa = NULL, *xb = NULL;
	gboolean match = TRUE;

	for (xa=a; *xa && *xa != '@'; ++xa);
	for (xb=b; *xb && *xb != '@'; ++xb);

	if (xa-a != xb-b || *xa != *xb || g_strncasecmp (a, b, xa-a))
		return FALSE;

	if (*xa == '\0')
		return TRUE;
	
	/* Find the end of the string, then walk through backwards comparing.
	   This is so that we'll match joe@foobar.com and joe@mail.foobar.com.
	*/
	while (*xa)
		++xa;
	while (*xb)
		++xb;

	while (match && *xa != '@' && *xb != '@') {
		match = (tolower (*xa) == tolower (*xb));
		--xa;
		--xb;
	}

	match = match && ((tolower (*xa) == tolower (*xb)) || (*xa == '.') || (*xb == '.'));

	return match;
}

gboolean
e_card_email_match_string (const ECard *card, const gchar *str)
{
	EIterator *iter;
	
	g_return_val_if_fail (card && E_IS_CARD (card), FALSE);
	g_return_val_if_fail (str != NULL, FALSE);

	if (!card->email)
		return FALSE;

	iter = e_list_get_iterator (card->email);
	for (e_iterator_reset (iter); e_iterator_is_valid (iter); e_iterator_next (iter)) {
		if (e_card_email_match_single_string (e_iterator_get (iter), str))
			return TRUE;
	}
	gtk_object_unref (GTK_OBJECT (iter));

	return FALSE;
}

gint
e_card_email_find_number (const ECard *card, const gchar *email)
{
	EIterator *iter;
	gint count = 0;

	g_return_val_if_fail (E_IS_CARD (card), -1);
	g_return_val_if_fail (email != NULL, -1);

	if (!card->email)
		return -1;

	iter = e_list_get_iterator (card->email);
	for (e_iterator_reset (iter); e_iterator_is_valid (iter); e_iterator_next (iter)) {
		if (!g_strcasecmp (e_iterator_get (iter), email))
			goto finished;
		++count;
	}
	count = -1;

 finished:
	gtk_object_unref (GTK_OBJECT (iter));

	return count;
}

/*
 * ECard lifecycle management and vCard loading/saving.
 */

static void
e_card_destroy (GtkObject *object)
{
	ECard *card = E_CARD(object);
	g_free(card->id);
	if (card->book)
		gtk_object_unref (GTK_OBJECT (card->book));
	g_free(card->file_as);
	g_free(card->fname);
	e_card_name_unref(card->name);
	g_free(card->bday);

	g_free(card->url);
	g_free(card->org);
	g_free(card->org_unit);
	g_free(card->office);
	g_free(card->title);
	g_free(card->role);
	g_free(card->manager);
	g_free(card->assistant);
	g_free(card->nickname);
	g_free(card->spouse);
	g_free(card->anniversary);
	g_free(card->caluri);
	g_free(card->fburl);
	g_free(card->icscalendar);
	g_free(card->note);
	g_free(card->related_contacts);

	if (card->categories)
		gtk_object_unref(GTK_OBJECT(card->categories));
	if (card->email)
		gtk_object_unref(GTK_OBJECT(card->email));
	if (card->phone)
		gtk_object_unref(GTK_OBJECT(card->phone));
	if (card->address)
		gtk_object_unref(GTK_OBJECT(card->address));
	if (card->address_label)
		gtk_object_unref(GTK_OBJECT(card->address_label));
}


/* Set_arg handler for the card */
static void
e_card_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ECard *card;
	
	card = E_CARD (object);

	switch (arg_id) {
	case ARG_FILE_AS:
		g_free(card->file_as);
		card->file_as = g_strdup(GTK_VALUE_STRING(*arg));
		if (card->file_as == NULL)
			card->file_as = g_strdup("");
		break;

	case ARG_FULL_NAME:
		g_free(card->fname);
		card->fname = g_strdup(GTK_VALUE_STRING(*arg));
		if (card->fname == NULL)
			card->fname = g_strdup("");

		e_card_name_unref (card->name);
		card->name = e_card_name_from_string (card->fname);
		break;
	case ARG_NAME:
		e_card_name_unref (card->name);
		card->name = e_card_name_ref(GTK_VALUE_POINTER(*arg));
		if (card->name == NULL)
			card->name = e_card_name_new();
		if (card->fname == NULL) {
			card->fname = e_card_name_to_string(card->name);
		}
		if (card->file_as == NULL) {
			ECardName *name = card->name;
			char *strings[3], **stringptr;
			char *string;
			stringptr = strings;
			if (name->family && *name->family)
				*(stringptr++) = name->family;
			if (name->given && *name->given)
				*(stringptr++) = name->given;
			*stringptr = NULL;
			string = g_strjoinv(", ", strings);
			card->file_as = string;
		}
		break;
	case ARG_CATEGORIES:
		if (card->categories)
			gtk_object_unref(GTK_OBJECT(card->categories));
		card->categories = NULL;
		if (GTK_VALUE_STRING(*arg))
			do_parse_categories(card, GTK_VALUE_STRING(*arg));
		break;
	case ARG_CATEGORY_LIST:
		if (card->categories)
			gtk_object_unref(GTK_OBJECT(card->categories));
		card->categories = E_LIST(GTK_VALUE_OBJECT(*arg));
		if (card->categories)
			gtk_object_ref(GTK_OBJECT(card->categories));
		break;
	case ARG_BIRTH_DATE:
		g_free(card->bday);
		if (GTK_VALUE_POINTER (*arg)) {
			card->bday = g_new (ECardDate, 1);
			memcpy (card->bday, GTK_VALUE_POINTER (*arg), sizeof (ECardDate));
		} else {
			card->bday = NULL;
		}
		break;
	case ARG_URL:
		g_free(card->url);
		card->url = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ORG:
		g_free(card->org);
		card->org = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ORG_UNIT:
		g_free(card->org_unit);
		card->org_unit = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_OFFICE:
		g_free(card->office);
		card->office = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_TITLE:
		g_free(card->title);
		card->title = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ROLE:
		g_free(card->role);
		card->role = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_MANAGER:
		g_free(card->manager);
		card->manager = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ASSISTANT:
		g_free(card->assistant);
		card->assistant = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_NICKNAME:
		g_free(card->nickname);
		card->nickname = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_SPOUSE:
		g_free(card->spouse);
		card->spouse = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ANNIVERSARY:
		g_free(card->anniversary);
		if (GTK_VALUE_POINTER (*arg)) {
			card->anniversary = g_new (ECardDate, 1);
			memcpy (card->anniversary, GTK_VALUE_POINTER (*arg), sizeof (ECardDate));
		} else {
			card->anniversary = NULL;
		}
		break;
	case ARG_MAILER:
		g_free(card->mailer);
		card->mailer = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_CALURI:
		g_free(card->caluri);
		card->caluri = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_FBURL:
		g_free(card->fburl);
		card->fburl = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ICSCALENDAR:
		g_free(card->icscalendar);
		card->icscalendar = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_NOTE:
		g_free (card->note);
		card->note = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_RELATED_CONTACTS:
		g_free (card->related_contacts);
		card->related_contacts = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_WANTS_HTML:
		card->wants_html = GTK_VALUE_BOOL(*arg);
		card->wants_html_set = TRUE;
		break;
	case ARG_ARBITRARY:
		if (card->arbitrary)
			gtk_object_unref(GTK_OBJECT(card->arbitrary));
		card->arbitrary = E_LIST(GTK_VALUE_OBJECT(*arg));
		if (card->arbitrary)
			gtk_object_ref(GTK_OBJECT(card->arbitrary));
		break;
	case ARG_ID:
		g_free(card->id);
		card->id = g_strdup(GTK_VALUE_STRING(*arg));
		if (card->id == NULL)
			card->id = g_strdup ("");
		break;
	case ARG_LAST_USE:
		g_free(card->last_use);
		if (GTK_VALUE_POINTER (*arg)) {
			card->last_use = g_new (ECardDate, 1);
			memcpy (card->last_use, GTK_VALUE_POINTER (*arg), sizeof (ECardDate));
		} else {
			card->last_use = NULL;
		}
		break;
	case ARG_USE_SCORE:
		card->raw_use_score = GTK_VALUE_FLOAT(*arg);
		break;
	case ARG_EVOLUTION_LIST:
		card->list = GTK_VALUE_BOOL(*arg);
		break;
	case ARG_EVOLUTION_LIST_SHOW_ADDRESSES:
		card->list_show_addresses = GTK_VALUE_BOOL(*arg);
		break;
	default:
		return;
	}
}

/* Get_arg handler for the card */
static void
e_card_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ECard *card;

	card = E_CARD (object);

	switch (arg_id) {
	case ARG_FILE_AS:
		GTK_VALUE_STRING (*arg) = card->file_as;
		break;
	case ARG_FULL_NAME:
		GTK_VALUE_STRING (*arg) = card->fname;
		break;
	case ARG_NAME:
		GTK_VALUE_POINTER(*arg) = card->name;
		break;
	case ARG_ADDRESS:
		if (!card->address)
			card->address = e_list_new((EListCopyFunc) e_card_delivery_address_ref,
						   (EListFreeFunc) e_card_delivery_address_unref,
						   NULL);
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(card->address);
		break;
	case ARG_ADDRESS_LABEL:
		if (!card->address_label)
			card->address_label = e_list_new((EListCopyFunc) e_card_address_label_ref,
							 (EListFreeFunc) e_card_address_label_unref,
							 NULL);
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(card->address_label);
		break;
	case ARG_PHONE:
		if (!card->phone)
			card->phone = e_list_new((EListCopyFunc) e_card_phone_ref,
						 (EListFreeFunc) e_card_phone_unref,
						 NULL);
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(card->phone);
		break;
	case ARG_EMAIL:
		if (!card->email)
			card->email = e_list_new((EListCopyFunc) g_strdup, 
						 (EListFreeFunc) g_free,
						 NULL);
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(card->email);
		break;
	case ARG_CATEGORIES:
		{
			int i;
			char ** strs;
			int length;
			EIterator *iterator;
			if (!card->categories)
				card->categories = e_list_new((EListCopyFunc) g_strdup, 
							      (EListFreeFunc) g_free,
							      NULL);
			length = e_list_length(card->categories);
			strs = g_new(char *, length + 1);
			for (iterator = e_list_get_iterator(card->categories), i = 0; e_iterator_is_valid(iterator); e_iterator_next(iterator), i++) {
				strs[i] = (char *)e_iterator_get(iterator);
			}
			strs[i] = 0;
			GTK_VALUE_STRING(*arg) = g_strjoinv(", ", strs);
			g_free(strs);
		}
		break;
	case ARG_CATEGORY_LIST:
		if (!card->categories)
			card->categories = e_list_new((EListCopyFunc) g_strdup, 
						      (EListFreeFunc) g_free,
						      NULL);
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(card->categories);
		break;
	case ARG_BIRTH_DATE:
		GTK_VALUE_POINTER(*arg) = card->bday;
		break;
	case ARG_URL:
		GTK_VALUE_STRING(*arg) = card->url;
		break;
	case ARG_ORG:
		GTK_VALUE_STRING(*arg) = card->org;
		break;
	case ARG_ORG_UNIT:
		GTK_VALUE_STRING(*arg) = card->org_unit;
		break;
	case ARG_OFFICE:
		GTK_VALUE_STRING(*arg) = card->office;
		break;
	case ARG_TITLE:
		GTK_VALUE_STRING(*arg) = card->title;
		break;
	case ARG_ROLE:
		GTK_VALUE_STRING(*arg) = card->role;
		break;
	case ARG_MANAGER:
		GTK_VALUE_STRING(*arg) = card->manager;
		break;
	case ARG_ASSISTANT:
		GTK_VALUE_STRING(*arg) = card->assistant;
		break;
	case ARG_NICKNAME:
		GTK_VALUE_STRING(*arg) = card->nickname;
		break;
	case ARG_SPOUSE:
		GTK_VALUE_STRING(*arg) = card->spouse;
		break;
	case ARG_ANNIVERSARY:
		GTK_VALUE_POINTER(*arg) = card->anniversary;
		break;
	case ARG_MAILER:
		GTK_VALUE_STRING(*arg) = card->mailer;
		break;
	case ARG_CALURI:
		GTK_VALUE_STRING(*arg) = card->caluri;
		break;
	case ARG_FBURL:
		GTK_VALUE_STRING(*arg) = card->fburl;
		break;
	case ARG_ICSCALENDAR:
		GTK_VALUE_STRING(*arg) = card->icscalendar;
		break;
	case ARG_NOTE:
		GTK_VALUE_STRING(*arg) = card->note;
		break;
	case ARG_RELATED_CONTACTS:
		GTK_VALUE_STRING(*arg) = card->related_contacts;
		break;
	case ARG_WANTS_HTML:
		GTK_VALUE_BOOL(*arg) = card->wants_html;
		break;
	case ARG_WANTS_HTML_SET:
		GTK_VALUE_BOOL(*arg) = card->wants_html_set;
		break;
	case ARG_ARBITRARY:
		if (!card->arbitrary)
			card->arbitrary = e_list_new((EListCopyFunc) e_card_arbitrary_ref,
						     (EListFreeFunc) e_card_arbitrary_unref,
						     NULL);

		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(card->arbitrary);
		break;
	case ARG_ID:
		GTK_VALUE_STRING(*arg) = card->id;
		break;
	case ARG_LAST_USE:
		GTK_VALUE_POINTER(*arg) = card->last_use;
		break;

	case ARG_USE_SCORE:
		GTK_VALUE_FLOAT(*arg) = e_card_get_use_score (card);
		break;
	case ARG_EVOLUTION_LIST:
		GTK_VALUE_BOOL(*arg) = card->list;
		break;
	case ARG_EVOLUTION_LIST_SHOW_ADDRESSES:
		GTK_VALUE_BOOL(*arg) = card->list_show_addresses;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}


/**
 * e_card_init:
 */
static void
e_card_init (ECard *card)
{
	card->id                  = g_strdup("");
	
	card->file_as             = NULL;
	card->fname               = NULL;
	card->name                = NULL;
	card->bday                = NULL;
	card->email               = NULL;
	card->phone               = NULL;
	card->address             = NULL;
	card->address_label       = NULL;
	card->url                 = NULL;
	card->org                 = NULL;
	card->org_unit            = NULL;
	card->office              = NULL;
	card->title               = NULL;
	card->role                = NULL;
	card->manager             = NULL;
	card->assistant           = NULL;
	card->nickname            = NULL;
	card->spouse              = NULL;
	card->anniversary         = NULL;
	card->mailer              = NULL;
	card->caluri              = NULL;
	card->fburl               = NULL;
	card->icscalendar         = NULL;
	card->note                = NULL;
	card->related_contacts    = NULL;
	card->categories          = NULL;
	card->wants_html          = FALSE;
	card->wants_html_set      = FALSE;
	card->list                = FALSE;
	card->list_show_addresses = FALSE;
	card->arbitrary           = NULL;
	card->last_use            = NULL;
	card->raw_use_score       = 0;
}

GList *
e_card_load_cards_from_file_with_default_charset(const char *filename, char *default_charset)
{
	VObject *vobj = Parse_MIME_FromFileName((char *) filename);
	GList *list = NULL;
	while(vobj) {
		VObject *next;
		ECard *card = E_CARD(gtk_type_new(e_card_get_type()));
		parse(card, vobj, default_charset);
		next = nextVObjectInList(vobj);
		cleanVObject(vobj);
		vobj = next;
		list = g_list_prepend(list, card);
	}
	list = g_list_reverse(list);
	return list;
}

GList *
e_card_load_cards_from_file(const char *filename)
{
	return e_card_load_cards_from_file_with_default_charset (filename, "UTF-8");
}

GList *
e_card_load_cards_from_string_with_default_charset(const char *str, char *default_charset)
{
	VObject *vobj = Parse_MIME(str, strlen (str));
	GList *list = NULL;
	while(vobj) {
		VObject *next;
		ECard *card = E_CARD(gtk_type_new(e_card_get_type()));
		parse(card, vobj, default_charset);
		next = nextVObjectInList(vobj);
		cleanVObject(vobj);
		vobj = next;
		list = g_list_prepend(list, card);
	}
	list = g_list_reverse(list);
	return list;
}

GList *
e_card_load_cards_from_string(const char *str)
{
	return e_card_load_cards_from_string_with_default_charset (str, "UTF-8");
}

void
e_card_free_empty_lists (ECard *card)
{
	if (card->address && e_list_length (card->address) == 0) {
		gtk_object_unref (GTK_OBJECT (card->address));
		card->address = NULL;
	}

	if (card->address_label && e_list_length (card->address_label) == 0) {
		gtk_object_unref (GTK_OBJECT (card->address_label));
		card->address_label = NULL;
	}

	if (card->phone && e_list_length (card->phone) == 0) {
		gtk_object_unref (GTK_OBJECT (card->phone));
		card->phone = NULL;
	}

	if (card->email && e_list_length (card->email) == 0) {
		gtk_object_unref (GTK_OBJECT (card->email));
		card->email = NULL;
	}

	if (card->categories && e_list_length (card->categories) == 0) {
		gtk_object_unref (GTK_OBJECT (card->categories));
		card->categories = NULL;
	}

	if (card->arbitrary && e_list_length (card->arbitrary) == 0) {
		gtk_object_unref (GTK_OBJECT (card->arbitrary));
		card->arbitrary = NULL;
	}
}

static void
assign_string(VObject *vobj, char *default_charset, char **string)
{
	int type = vObjectValueType(vobj);
	char *str;
	char *charset = default_charset;
	gboolean free_charset = FALSE;
	VObject *charset_obj;

	if ((charset_obj = isAPropertyOf (vobj, "CHARSET"))) {
		switch (vObjectValueType (charset_obj)) {
		case VCVT_STRINGZ:
			charset = (char *) vObjectStringZValue(charset_obj);
			break;
		case VCVT_USTRINGZ:
			charset = fakeCString (vObjectUStringZValue (charset_obj));
			free_charset = TRUE;
			break;
		}
	}

	switch(type) {
	case VCVT_STRINGZ:
		if (strcmp (charset, "UTF-8"))
			*string = e_utf8_from_charset_string (charset, vObjectStringZValue(vobj));
		else
			*string = g_strdup(vObjectStringZValue(vobj));
		break;
	case VCVT_USTRINGZ:
		str = fakeCString (vObjectUStringZValue (vobj));
		if (strcmp (charset, "UTF-8"))
			*string = e_utf8_from_charset_string (charset, str);
		else
			*string = g_strdup(str);
		free(str);
		break;
	default:
		*string = g_strdup("");
		break;
	}

	if (free_charset) {
		free (charset);
	}
}


ECardDate
e_card_date_from_string (const char *str)
{
	ECardDate date;
	int length;

	date.year = 0;
	date.month = 0;
	date.day = 0;

	length = strlen(str);
	
	if (length == 10 ) {
		date.year = str[0] * 1000 + str[1] * 100 + str[2] * 10 + str[3] - '0' * 1111;
		date.month = str[5] * 10 + str[6] - '0' * 11;
		date.day = str[8] * 10 + str[9] - '0' * 11;
	} else if ( length == 8 ) {
		date.year = str[0] * 1000 + str[1] * 100 + str[2] * 10 + str[3] - '0' * 1111;
		date.month = str[4] * 10 + str[5] - '0' * 11;
		date.day = str[6] * 10 + str[7] - '0' * 11;
	}
	
	return date;
}

char *
e_v_object_get_child_value(VObject *vobj, char *name, char *default_charset)
{
	char *ret_val;
	VObjectIterator iterator;
	gboolean free_charset = FALSE;
	VObject *charset_obj;

	if ((charset_obj = isAPropertyOf (vobj, "CHARSET"))) {
		switch (vObjectValueType (charset_obj)) {
		case VCVT_STRINGZ:
			default_charset = (char *) vObjectStringZValue(charset_obj);
			break;
		case VCVT_USTRINGZ:
			default_charset = fakeCString (vObjectUStringZValue (charset_obj));
			free_charset = TRUE;
			break;
		}
	}

	initPropIterator(&iterator, vobj);
	while(moreIteration (&iterator)) {
		VObject *attribute = nextVObject(&iterator);
		const char *id = vObjectName(attribute);
		if ( ! strcmp(id, name) ) {
			assign_string(attribute, default_charset, &ret_val);
			return ret_val;
		}
	}
	if (free_charset)
		free (default_charset);

	return NULL;
}

static struct { 
	char *id;
	ECardPhoneFlags flag;
} phone_pairs[] = {
	{ VCPreferredProp,         E_CARD_PHONE_PREF },
	{ VCWorkProp,              E_CARD_PHONE_WORK },
	{ VCHomeProp,              E_CARD_PHONE_HOME },
	{ VCVoiceProp,             E_CARD_PHONE_VOICE },
	{ VCFaxProp,               E_CARD_PHONE_FAX },
	{ VCMessageProp,           E_CARD_PHONE_MSG },
	{ VCCellularProp,          E_CARD_PHONE_CELL },
	{ VCPagerProp,             E_CARD_PHONE_PAGER },
	{ VCBBSProp,               E_CARD_PHONE_BBS },
	{ VCModemProp,             E_CARD_PHONE_MODEM },
	{ VCCarProp,               E_CARD_PHONE_CAR },
	{ VCISDNProp,              E_CARD_PHONE_ISDN },
	{ VCVideoProp,             E_CARD_PHONE_VIDEO },
	{ "X-EVOLUTION-ASSISTANT", E_CARD_PHONE_ASSISTANT },
	{ "X-EVOLUTION-CALLBACK",  E_CARD_PHONE_CALLBACK  },
	{ "X-EVOLUTION-RADIO",     E_CARD_PHONE_RADIO     },
	{ "X-EVOLUTION-TELEX",     E_CARD_PHONE_TELEX     },
	{ "X-EVOLUTION-TTYTDD",    E_CARD_PHONE_TTYTDD    },
};

static ECardPhoneFlags
get_phone_flags (VObject *vobj)
{
	ECardPhoneFlags ret = 0;
	int i;

	for (i = 0; i < sizeof(phone_pairs) / sizeof(phone_pairs[0]); i++) {
		if (isAPropertyOf (vobj, phone_pairs[i].id)) {
			ret |= phone_pairs[i].flag;
		}
	}
	
	return ret;
}

static void
set_phone_flags (VObject *vobj, ECardPhoneFlags flags)
{
	int i;

	for (i = 0; i < sizeof(phone_pairs) / sizeof(phone_pairs[0]); i++) {
		if (flags & phone_pairs[i].flag) {
				addProp (vobj, phone_pairs[i].id);
		}
	}
}

static struct { 
	char *id;
	ECardAddressFlags flag;
} addr_pairs[] = {
	{ VCDomesticProp, E_CARD_ADDR_DOM },
	{ VCInternationalProp, E_CARD_ADDR_INTL },
	{ VCPostalProp, E_CARD_ADDR_POSTAL },
	{ VCParcelProp, E_CARD_ADDR_PARCEL },
	{ VCHomeProp, E_CARD_ADDR_HOME },
	{ VCWorkProp, E_CARD_ADDR_WORK },
	{ "PREF", E_CARD_ADDR_DEFAULT },
};

static ECardAddressFlags
get_address_flags (VObject *vobj)
{
	ECardAddressFlags ret = 0;
	int i;
	
	for (i = 0; i < sizeof(addr_pairs) / sizeof(addr_pairs[0]); i++) {
		if (isAPropertyOf (vobj, addr_pairs[i].id)) {
			ret |= addr_pairs[i].flag;
		}
	}
	
	return ret;
}

static void
set_address_flags (VObject *vobj, ECardAddressFlags flags)
{
	int i;
	
	for (i = 0; i < sizeof(addr_pairs) / sizeof(addr_pairs[0]); i++) {
		if (flags & addr_pairs[i].flag) {
			addProp (vobj, addr_pairs[i].id);
		}
	}
}

#include <Evolution-Composer.h>

#define COMPOSER_OAFID "OAFIID:GNOME_Evolution_Mail_Composer"

void
e_card_list_send (GList *cards, ECardDisposition disposition)
{
	BonoboObjectClient *bonobo_server;
	GNOME_Evolution_Composer composer_server;
	CORBA_Environment ev;

	if (cards == NULL)
		return;
	
	/* First, I obtain an object reference that represents the Composer. */
	bonobo_server = bonobo_object_activate (COMPOSER_OAFID, 0);

	g_return_if_fail (bonobo_server != NULL);

	composer_server = bonobo_object_corba_objref (BONOBO_OBJECT (bonobo_server));

	CORBA_exception_init (&ev);

	if (disposition == E_CARD_DISPOSITION_AS_TO) {
		GNOME_Evolution_Composer_RecipientList *to_list, *cc_list, *bcc_list;
		CORBA_char *subject;
		int to_i, bcc_i;
		GList *iter;
		gint to_length = 0, bcc_length = 0;

		/* Figure out how many addresses of each kind we have. */
		for (iter = cards; iter != NULL; iter = g_list_next (iter)) {
			ECard *card = E_CARD (iter->data);
			if (e_card_evolution_list (card)) {
				gint len = card->email ? e_list_length (card->email) : 0;
				if (e_card_evolution_list_show_addresses (card))
					to_length += len;
				else
					bcc_length += len;
			} else {
				if (card->email != NULL)
					++to_length;
			}
		}

		/* Now I have to make a CORBA sequences that represents a recipient list with
		   the right number of entries, for the cards. */
		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = to_length;
		to_list->_length = to_length;
		if (to_length > 0) {
			to_list->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (to_length);
		}

		cc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		cc_list->_maximum = cc_list->_length = 0;
		
		bcc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		bcc_list->_maximum = bcc_length;
		bcc_list->_length = bcc_length;
		if (bcc_length > 0) {
			bcc_list->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (bcc_length);
		}

		to_i = 0;
		bcc_i = 0;
		while (cards != NULL) {
			ECard *card = cards->data;
			EIterator *iterator;
			gchar *name, *addr;
			gboolean is_list, is_hidden, free_name_addr;
			GNOME_Evolution_Composer_Recipient *recipient;

			if (card->email != NULL) {

				is_list = e_card_evolution_list (card);
				is_hidden = is_list && !e_card_evolution_list_show_addresses (card);
			
				for (iterator = e_list_get_iterator (card->email); e_iterator_is_valid (iterator); e_iterator_next (iterator)) {
					
					if (is_hidden) {
						recipient = &(bcc_list->_buffer[bcc_i]);
						++bcc_i;
					} else {
						recipient = &(to_list->_buffer[to_i]);
						++to_i;
					}
					
					name = "";
					addr = "";
					free_name_addr = FALSE;
					if (e_iterator_is_valid (iterator)) {
						
						if (is_list) {
							/* We need to decode the list entries, which are XMLified EDestinations. */
							EDestination *dest = e_destination_import (e_iterator_get (iterator));
							if (dest != NULL) {
								name = g_strdup (e_destination_get_name (dest));
								addr = g_strdup (e_destination_get_email (dest));
								free_name_addr = TRUE;
								gtk_object_unref (GTK_OBJECT (dest));
							}
							
						} else { /* is just a plain old card */
							if (card->name)
								name = e_card_name_to_string (card->name);
							addr = g_strdup ((char *) e_iterator_get (iterator));
							free_name_addr = TRUE;
						}
					}
					
					recipient->name    = CORBA_string_dup (name ? name : "");
					recipient->address = CORBA_string_dup (addr ? addr : "");
					
					if (free_name_addr) {
						g_free ((gchar *) name);
						g_free ((gchar *) addr);
					}
					
					/* If this isn't a list, we quit after the first (i.e. the default) address. */
					if (!is_list)
						break;
					
				}
				gtk_object_unref (GTK_OBJECT (iterator));
			}

			cards = g_list_next (cards);
		}

		subject = CORBA_string_dup ("");

		GNOME_Evolution_Composer_setHeaders (composer_server, "", to_list, cc_list, bcc_list, subject, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_printerr ("gui/e-meeting-edit.c: I couldn't set the composer headers via CORBA! Aagh.\n");
			CORBA_exception_free (&ev);
			return;
		}

		CORBA_free (to_list);
		CORBA_free (cc_list);
		CORBA_free (bcc_list);
		CORBA_free (subject);
	}

	if (disposition == E_CARD_DISPOSITION_AS_ATTACHMENT) {
		CORBA_char *content_type, *filename, *description;
		GNOME_Evolution_Composer_AttachmentData *attach_data;
		CORBA_boolean show_inline;
		char *tempstr;

		GNOME_Evolution_Composer_RecipientList *to_list, *cc_list, *bcc_list;
		CORBA_char *subject;
		
		content_type = CORBA_string_dup ("text/x-vcard");
		filename = CORBA_string_dup ("");

		if (cards->next) {
			description = CORBA_string_dup (_("Multiple VCards"));
		} else {
			char *file_as;

			gtk_object_get(GTK_OBJECT(cards->data),
				       "file_as", &file_as,
				       NULL);

			tempstr = g_strdup_printf (_("VCard for %s"), file_as);
			description = CORBA_string_dup (tempstr);
			g_free (tempstr);
		}

		show_inline = FALSE;

		tempstr = e_card_list_get_vcard (cards);
		attach_data = GNOME_Evolution_Composer_AttachmentData__alloc();
		attach_data->_maximum = attach_data->_length = strlen (tempstr);
		attach_data->_buffer = CORBA_sequence_CORBA_char_allocbuf (attach_data->_length);
		strcpy (attach_data->_buffer, tempstr);
		g_free (tempstr);

		GNOME_Evolution_Composer_attachData (composer_server, 
						     content_type, filename, description,
						     show_inline, attach_data,
						     &ev);
	
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_printerr ("gui/e-meeting-edit.c: I couldn't attach data to the composer via CORBA! Aagh.\n");
			CORBA_exception_free (&ev);
			return;
		}
	
		CORBA_free (content_type);
		CORBA_free (filename);
		CORBA_free (description);
		CORBA_free (attach_data);

		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = to_list->_length = 0;
		
		cc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		cc_list->_maximum = cc_list->_length = 0;

		bcc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		bcc_list->_maximum = bcc_list->_length = 0;

		if (!cards || cards->next) {
			subject = CORBA_string_dup ("Contact information");
		} else {
			ECard *card = cards->data;
			const gchar *tempstr2;

			tempstr2 = NULL;
			gtk_object_get(GTK_OBJECT(card),
				       "file_as", &tempstr2,
				       NULL);
			if (!tempstr2 || !*tempstr2)
				gtk_object_get(GTK_OBJECT(card),
					       "full_name", &tempstr2,
					       NULL);
			if (!tempstr2 || !*tempstr2)
				gtk_object_get(GTK_OBJECT(card),
					       "org", &tempstr2,
					       NULL);
			if (!tempstr2 || !*tempstr2) {
				EList *list;
				EIterator *iterator;
				gtk_object_get(GTK_OBJECT(card),
					       "email", &list,
					       NULL);
				iterator = e_list_get_iterator (list);
				if (e_iterator_is_valid (iterator)) {
					tempstr2 = e_iterator_get (iterator);
				}
				gtk_object_unref (GTK_OBJECT (iterator));
			}

			if (!tempstr2 || !*tempstr2)
				tempstr = g_strdup_printf ("Contact information");
			else
				tempstr = g_strdup_printf ("Contact information for %s", tempstr2);
			subject = CORBA_string_dup (tempstr);
			g_free (tempstr);
		}
		
		GNOME_Evolution_Composer_setHeaders (composer_server, "", to_list, cc_list, bcc_list, subject, &ev);

		CORBA_free (to_list);
		CORBA_free (cc_list);
		CORBA_free (bcc_list);
		CORBA_free (subject);
	}

	GNOME_Evolution_Composer_show (composer_server, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_printerr ("gui/e-meeting-edit.c: I couldn't show the composer via CORBA! Aagh.\n");
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);
}

void
e_card_send (ECard *card, ECardDisposition disposition)
{
	GList *list;
	list = g_list_prepend (NULL, card);
	e_card_list_send (list, disposition);
	g_list_free (list);
}

gboolean
e_card_evolution_list (ECard *card)
{
	g_return_val_if_fail (card && E_IS_CARD (card), FALSE);
	return card->list;
}

gboolean
e_card_evolution_list_show_addresses (ECard *card)
{
	g_return_val_if_fail (card && E_IS_CARD (card), FALSE);
	return card->list_show_addresses;
}

typedef struct _CardLoadData CardLoadData;
struct _CardLoadData {
	gchar *card_id;
	ECardCallback cb;
	gpointer closure;
};

static void
get_card_cb (EBook *book, EBookStatus status, ECard *card, gpointer closure)
{
	CardLoadData *data = (CardLoadData *) closure;

	if (data->cb != NULL) {
		if (status == E_BOOK_STATUS_SUCCESS)
			data->cb (card, data->closure);
		else
			data->cb (NULL, data->closure);
	}

	g_free (data->card_id);
	g_free (data);
}

static void
card_load_cb (EBook *book, EBookStatus status, gpointer closure)
{
	CardLoadData *data = (CardLoadData *) closure;

	if (status == E_BOOK_STATUS_SUCCESS)
		e_book_get_card (book, data->card_id, get_card_cb, closure);
	else {
		data->cb (NULL, data->closure);
		g_free (data->card_id);
		g_free (data);
	}
}

void
e_card_load_uri (const gchar *book_uri, const gchar *uid, ECardCallback cb, gpointer closure)
{
	CardLoadData *data;
	EBook *book;
	
	data          = g_new (CardLoadData, 1);
	data->card_id = g_strdup (uid);
	data->cb      = cb;
	data->closure = closure;

	book = e_book_new ();
	e_book_load_uri (book, book_uri, card_load_cb, data);
}
