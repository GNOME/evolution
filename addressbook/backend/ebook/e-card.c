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

#include <gal/widgets/e-unicode.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <bonobo/bonobo-i18n.h>
#include <gal/util/e-util.h>

#include <libversit/vcc.h>
#include "e-util/ename/e-name-western.h"
#include "e-util/ename/e-address-western.h"
#include "e-book.h"

#define is_a_prop_of(obj,prop) (isAPropertyOf ((obj),(prop)))
#define str_val(obj) (the_str = (vObjectValueType (obj))? fakeCString (vObjectUStringZValue (obj)) : calloc (1, 1))
#define has(obj,prop) (vo = isAPropertyOf ((obj), (prop)))

#define XEV_WANTS_HTML "X-MOZILLA-HTML"
#define XEV_ARBITRARY "X-EVOLUTION-ARBITRARY"
#define XEV_LIST "X-EVOLUTION-LIST"
#define XEV_LIST_SHOW_ADDRESSES "X-EVOLUTION-LIST-SHOW_ADDRESSES"
#define XEV_RELATED_CONTACTS "X-EVOLUTION-RELATED_CONTACTS"

/* Object property IDs */
enum {
	PROP_0,
	PROP_FILE_AS,
	PROP_FULL_NAME,
	PROP_NAME,
	PROP_ADDRESS,
	PROP_ADDRESS_LABEL,
	PROP_PHONE,
	PROP_EMAIL,
	PROP_BIRTH_DATE,
	PROP_URL,
	PROP_ORG,
	PROP_ORG_UNIT,
	PROP_OFFICE,
	PROP_TITLE,
	PROP_ROLE,
	PROP_MANAGER,
	PROP_ASSISTANT,
	PROP_NICKNAME,
	PROP_SPOUSE,
	PROP_ANNIVERSARY,
	PROP_MAILER,
	PROP_CALURI,
	PROP_FBURL,
	PROP_ICSCALENDAR,
	PROP_NOTE,
	PROP_RELATED_CONTACTS,
	PROP_CATEGORIES,
	PROP_CATEGORY_LIST,
	PROP_WANTS_HTML,
	PROP_WANTS_HTML_SET,
	PROP_EVOLUTION_LIST,
	PROP_EVOLUTION_LIST_SHOW_ADDRESSES,
	PROP_ARBITRARY,
	PROP_ID,
	PROP_LAST_USE,
	PROP_USE_SCORE,
};

static GObjectClass *parent_class;

static void parse(ECard *card, VObject *vobj, const char *default_charset);
static void e_card_init (ECard *card);
static void e_card_class_init (ECardClass *klass);

static void e_card_dispose (GObject *object);
static void e_card_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_card_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static void assign_string(VObject *vobj, const char *default_charset, char **string);

char *e_v_object_get_child_value(VObject *vobj, char *name, const char *default_charset);

static void parse_bday(ECard *card, VObject *object, const char *default_charset);
static void parse_full_name(ECard *card, VObject *object, const char *default_charset);
static void parse_file_as(ECard *card, VObject *object, const char *default_charset);
static void parse_name(ECard *card, VObject *object, const char *default_charset);
static void parse_email(ECard *card, VObject *object, const char *default_charset);
static void parse_phone(ECard *card, VObject *object, const char *default_charset);
static void parse_address(ECard *card, VObject *object, const char *default_charset);
static void parse_address_label(ECard *card, VObject *object, const char *default_charset);
static void parse_url(ECard *card, VObject *object, const char *default_charset);
static void parse_org(ECard *card, VObject *object, const char *default_charset);
static void parse_office(ECard *card, VObject *object, const char *default_charset);
static void parse_title(ECard *card, VObject *object, const char *default_charset);
static void parse_role(ECard *card, VObject *object, const char *default_charset);
static void parse_manager(ECard *card, VObject *object, const char *default_charset);
static void parse_assistant(ECard *card, VObject *object, const char *default_charset);
static void parse_nickname(ECard *card, VObject *object, const char *default_charset);
static void parse_spouse(ECard *card, VObject *object, const char *default_charset);
static void parse_anniversary(ECard *card, VObject *object, const char *default_charset);
static void parse_mailer(ECard *card, VObject *object, const char *default_charset);
static void parse_caluri(ECard *card, VObject *object, const char *default_charset);
static void parse_fburl(ECard *card, VObject *object, const char *default_charset);
static void parse_icscalendar(ECard *card, VObject *object, const char *default_charset);
static void parse_note(ECard *card, VObject *object, const char *default_charset);
static void parse_related_contacts(ECard *card, VObject *object, const char *default_charset);
static void parse_categories(ECard *card, VObject *object, const char *default_charset);
static void parse_wants_html(ECard *card, VObject *object, const char *default_charset);
static void parse_list(ECard *card, VObject *object, const char *default_charset);
static void parse_list_show_addresses(ECard *card, VObject *object, const char *default_charset);
static void parse_arbitrary(ECard *card, VObject *object, const char *default_charset);
static void parse_id(ECard *card, VObject *object, const char *default_charset);
static void parse_last_use(ECard *card, VObject *object, const char *default_charset);
static void parse_use_score(ECard *card, VObject *object, const char *default_charset);

static ECardPhoneFlags get_phone_flags (VObject *vobj);
static void set_phone_flags (VObject *vobj, ECardPhoneFlags flags);
static ECardAddressFlags get_address_flags (VObject *vobj);
static void set_address_flags (VObject *vobj, ECardAddressFlags flags);

typedef void (* ParsePropertyFunc) (ECard *card, VObject *object, const char *default_charset);

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
GType
e_card_get_type (void)
{
	static GType card_type = 0;

	if (!card_type) {
		static const GTypeInfo card_info =  {
			sizeof (ECardClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_card_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ECard),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_card_init,
		};

		card_type = g_type_register_static (G_TYPE_OBJECT, "ECard", &card_info, 0);
	}

	return card_type;
}

ECard *
e_card_new_with_default_charset (const char *vcard, const char *default_charset)
{
	ECard *card = g_object_new (E_TYPE_CARD, NULL);
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
e_card_new (const char *vcard)
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
		g_object_ref (new_card->book);
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

	days_since_last_use = g_date_get_julian (&today) - g_date_get_julian (&last_use);
	
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

	card->last_use->day   = g_date_get_day (&today);
	card->last_use->month = g_date_get_month (&today);
	card->last_use->year  = g_date_get_year (&today);

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
		g_object_unref (card->book);
	card->book = book;
	if (card->book)
		g_object_ref (card->book);
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
		g_object_unref(iterator);
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
		g_object_unref(iterator);
	}

	if ( card->phone ) { 
		EIterator *iterator = e_list_get_iterator(card->phone);
		for ( ; e_iterator_is_valid(iterator) ;e_iterator_next(iterator) ) {
			VObject *phoneprop;
			ECardPhone *phone = (ECardPhone *) e_iterator_get(iterator);
			phoneprop = ADD_PROP_VALUE(vobj, VCTelephoneProp, phone->number);
			
			set_phone_flags (phoneprop, phone->flags);
		}
		g_object_unref(iterator);
	}

	if ( card->email ) { 
		EIterator *iterator = e_list_get_iterator(card->email);
		for ( ; e_iterator_is_valid(iterator) ;e_iterator_next(iterator) ) {
			VObject *emailprop;
			emailprop = ADD_PROP_VALUE(vobj, VCEmailAddressProp, (char *) e_iterator_get(iterator));
			addProp (emailprop, VCInternetProp);
		}
		g_object_unref(iterator);
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
parse_file_as(ECard *card, VObject *vobj, const char *default_charset)
{
	if ( card->file_as )
		g_free(card->file_as);
	assign_string(vobj, default_charset, &(card->file_as));
}

static void
parse_name(ECard *card, VObject *vobj, const char *default_charset)
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
parse_full_name(ECard *card, VObject *vobj, const char *default_charset)
{
	if ( card->fname )
		g_free(card->fname);
	assign_string(vobj, default_charset, &(card->fname));
}

static void
parse_email(ECard *card, VObject *vobj, const char *default_charset)
{
	char *next_email;
	EList *list;

	assign_string(vobj, default_charset, &next_email);
	g_object_get(card,
		     "email", &list,
		     NULL);
	e_list_append(list, next_email);
	g_free (next_email);
	g_object_unref(list);
}

/* Deal with charset */
static void
parse_bday(ECard *card, VObject *vobj, const char *default_charset)
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
parse_phone(ECard *card, VObject *vobj, const char *default_charset)
{
	ECardPhone *next_phone = e_card_phone_new ();
	EList *list;

	assign_string(vobj, default_charset, &(next_phone->number));
	next_phone->flags = get_phone_flags(vobj);

	g_object_get(card,
		     "phone", &list,
		     NULL);
	e_list_append(list, next_phone);
	e_card_phone_unref (next_phone);
	g_object_unref(list);
}

static void
parse_address(ECard *card, VObject *vobj, const char *default_charset)
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

	g_object_get(card,
		     "address", &list,
		     NULL);
	e_list_append(list, next_addr);
	e_card_delivery_address_unref (next_addr);
	g_object_unref(list);
}

static void
parse_address_label(ECard *card, VObject *vobj, const char *default_charset)
{
	ECardAddrLabel *next_addr = e_card_address_label_new ();
	EList *list;

	next_addr->flags   = get_address_flags (vobj);
	assign_string(vobj, default_charset, &next_addr->data);

	g_object_get(card,
		     "address_label", &list,
		     NULL);
	e_list_append(list, next_addr);
	e_card_address_label_unref (next_addr);
	g_object_unref(list);
}

static void
parse_url(ECard *card, VObject *vobj, const char *default_charset)
{
	if (card->url)
		g_free(card->url);
	assign_string(vobj, default_charset, &(card->url));
}

static void
parse_org(ECard *card, VObject *vobj, const char *default_charset)
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
parse_office(ECard *card, VObject *vobj, const char *default_charset)
{
	if ( card->office )
		g_free(card->office);
	assign_string(vobj, default_charset, &(card->office));
}

static void
parse_title(ECard *card, VObject *vobj, const char *default_charset)
{
	if ( card->title )
		g_free(card->title);
	assign_string(vobj, default_charset, &(card->title));
}

static void
parse_role(ECard *card, VObject *vobj, const char *default_charset)
{
	if (card->role)
		g_free(card->role);
	assign_string(vobj, default_charset, &(card->role));
}

static void
parse_manager(ECard *card, VObject *vobj, const char *default_charset)
{
	if ( card->manager )
		g_free(card->manager);
	assign_string(vobj, default_charset, &(card->manager));
}

static void
parse_assistant(ECard *card, VObject *vobj, const char *default_charset)
{
	if ( card->assistant )
		g_free(card->assistant);
	assign_string(vobj, default_charset, &(card->assistant));
}

static void
parse_nickname(ECard *card, VObject *vobj, const char *default_charset)
{
	if (card->nickname)
		g_free(card->nickname);
	assign_string(vobj, default_charset, &(card->nickname));
}

static void
parse_spouse(ECard *card, VObject *vobj, const char *default_charset)
{
	if ( card->spouse )
		g_free(card->spouse);
	assign_string(vobj, default_charset, &(card->spouse));
}

/* Deal with charset */
static void
parse_anniversary(ECard *card, VObject *vobj, const char *default_charset)
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
parse_mailer(ECard *card, VObject *vobj, const char *default_charset)
{
	if ( card->mailer )
		g_free(card->mailer);
	assign_string(vobj, default_charset, &(card->mailer));
}

static void
parse_caluri(ECard *card, VObject *vobj, const char *default_charset)
{
 	g_free(card->caluri);
 	assign_string(vobj, default_charset, &(card->caluri));
}

static void
parse_fburl(ECard *card, VObject *vobj, const char *default_charset)
{
	g_free(card->fburl);
	assign_string(vobj, default_charset, &(card->fburl));
}

static void
parse_icscalendar(ECard *card, VObject *vobj, const char *default_charset)
{
	g_free(card->icscalendar);
	assign_string(vobj, default_charset, &(card->icscalendar));
}

static void
parse_note(ECard *card, VObject *vobj, const char *default_charset)
{
	g_free(card->note);
	assign_string(vobj, default_charset, &(card->note));
}

static void
parse_related_contacts(ECard *card, VObject *vobj, const char *default_charset)
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
	g_object_unref(iterator);
}

static void
do_parse_categories(ECard *card, char *str)
{
	int length = strlen(str);
	char *copy = g_new(char, length + 1);
	int i, j;
	EList *list;
	g_object_get(card,
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
	g_object_unref(list);
	g_free(copy);
}

/* Deal with charset */
static void
parse_categories(ECard *card, VObject *vobj, const char *default_charset)
{
	if ( vObjectValueType (vobj) ) {
		char *str = fakeCString (vObjectUStringZValue (vobj));
		do_parse_categories(card, str);
		free(str);
	}
}

/* Deal with charset */
static void
parse_wants_html(ECard *card, VObject *vobj, const char *default_charset)
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
parse_list(ECard *card, VObject *vobj, const char *default_charset)
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
parse_list_show_addresses(ECard *card, VObject *vobj, const char *default_charset)
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
parse_arbitrary(ECard *card, VObject *vobj, const char *default_charset)
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
	
	g_object_get(card,
		     "arbitrary", &list,
		     NULL);
	e_list_append(list, arbitrary);
	e_card_arbitrary_unref(arbitrary);
	g_object_unref(list);
}

static void
parse_id(ECard *card, VObject *vobj, const char *default_charset)
{
	g_free(card->id);
	assign_string(vobj, default_charset, &(card->id));
}

/* Deal with charset */
static void
parse_last_use(ECard *card, VObject *vobj, const char *default_charset)
{
	if ( vObjectValueType (vobj) ) {
		char *str = fakeCString (vObjectUStringZValue (vobj));
		g_free(card->last_use);
		card->last_use = g_new(ECardDate, 1);
		*(card->last_use) = e_card_date_from_string(str);
		free(str);
	}
}

/* Deal with charset */
static void
parse_use_score(ECard *card, VObject *vobj, const char *default_charset)
{
	card->raw_use_score = 0;
	
	if ( vObjectValueType (vobj) ) {
		char *str = fakeCString (vObjectUStringZValue (vobj));
		card->raw_use_score = MAX(0, atof (str));
		free (str);
	}
}

static void
parse_attribute(ECard *card, VObject *vobj, const char *default_charset)
{
	ParsePropertyFunc function = g_hash_table_lookup(E_CARD_GET_CLASS(card)->attribute_jump_table, vObjectName(vobj));
	if ( function )
		function(card, vobj, default_charset);
}

static void
parse(ECard *card, VObject *vobj, const char *default_charset)
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
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	klass->attribute_jump_table = g_hash_table_new(g_str_hash, g_str_equal);

	for ( i = 0; i < sizeof(attribute_jump_array) / sizeof(attribute_jump_array[0]); i++ ) {
		g_hash_table_insert(klass->attribute_jump_table, attribute_jump_array[i].key, attribute_jump_array[i].function);
	}

	object_class->dispose = e_card_dispose;
	object_class->get_property = e_card_get_property;
	object_class->set_property = e_card_set_property;

	g_object_class_install_property (object_class, PROP_FILE_AS, 
					 g_param_spec_string ("file_as",
							      _("File As"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FULL_NAME, 
					 g_param_spec_string ("full_name",
							      _("Full Name"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_NAME, 
					 g_param_spec_pointer ("name",
							       _("Name"),
							       /*_( */"XXX blurb" /*)*/,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ADDRESS, 
					 g_param_spec_object ("address",
							      _("Address"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_LIST,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_ADDRESS_LABEL, 
					 g_param_spec_object ("address_label",
							      _("Address Label"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_LIST,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_PHONE, 
					 g_param_spec_object ("phone",
							      _("Phone"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_LIST,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_EMAIL, 
					 g_param_spec_object ("email",
							      _("Email"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_LIST,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_BIRTH_DATE, 
					 g_param_spec_pointer ("birth_date",
							       _("Birth date"),
							       /*_( */"XXX blurb" /*)*/,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_URL, 
					 g_param_spec_string ("url",
							      _("URL"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ORG, 
					 g_param_spec_string ("org",
							      _("Organization"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ORG_UNIT, 
					 g_param_spec_string ("org_unit",
							      _("Organizational Unit"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_OFFICE, 
					 g_param_spec_string ("office",
							      _("Office"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_TITLE, 
					 g_param_spec_string ("title",
							      _("Title"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ROLE, 
					 g_param_spec_string ("role",
							      _("Role"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_MANAGER, 
					 g_param_spec_string ("manager",
							      _("Manager"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ASSISTANT, 
					 g_param_spec_string ("assistant",
							      _("Assistant"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_NICKNAME, 
					 g_param_spec_string ("nickname",
							      _("Nickname"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_SPOUSE, 
					 g_param_spec_string ("spouse",
							      _("Spouse"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ANNIVERSARY, 
					 g_param_spec_pointer ("anniversary",
							       _("Anniversary"),
							       /*_( */"XXX blurb" /*)*/,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_MAILER, 
					 g_param_spec_string ("mailer",
							      _("Mailer"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CALURI, 
					 g_param_spec_string ("caluri",
							      _("Calendar URI"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FBURL, 
					 g_param_spec_string ("fburl",
							      _("Free/Busy URL"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ICSCALENDAR, 
					 g_param_spec_string ("icscalendar",
							      _("ICS Calendar"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_NOTE, 
					 g_param_spec_string ("note",
							      _("Note"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_RELATED_CONTACTS, 
					 g_param_spec_string ("related_contacts",
							      _("Related Contacts"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CATEGORIES, 
					 g_param_spec_string ("categories",
							      _("Categories"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CATEGORY_LIST, 
					 g_param_spec_object ("category list",
							      _("Category List"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_LIST,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_WANTS_HTML, 
					 g_param_spec_boolean ("wants_html",
							       _("Wants HTML"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_WANTS_HTML_SET, 
					 g_param_spec_boolean ("wants_html_set",
							       _("Wants HTML set"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_EVOLUTION_LIST, 
					 g_param_spec_boolean ("list",
							       _("List"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EVOLUTION_LIST_SHOW_ADDRESSES, 
					 g_param_spec_boolean ("list_show_addresses",
							       _("List Show Addresses"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ARBITRARY, 
					 g_param_spec_object ("arbitrary",
							      _("Arbitrary"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_LIST,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ID, 
					 g_param_spec_string ("id",
							      _("ID"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_LAST_USE, 
					 g_param_spec_pointer ("last_use",
							       _("Last Use"),
							       /*_( */"XXX blurb" /*)*/,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_USE_SCORE, 
					 /* XXX at some point we
					    should remove
					    LAX_VALIDATION and figure
					    out some hard min & max
					    scores. */
					 g_param_spec_float ("use_score",
							     _("Use Score"),
							     /*_( */"XXX blurb" /*)*/,
							     0.0,
							     0.0,
							     0.0,
							     G_PARAM_READWRITE | G_PARAM_LAX_VALIDATION));
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

	if (xa-a != xb-b || *xa != *xb || g_ascii_strncasecmp (a, b, xa-a))
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
	g_object_unref (iter);

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
		if (!g_ascii_strcasecmp (e_iterator_get (iter), email))
			goto finished;
		++count;
	}
	count = -1;

 finished:
	g_object_unref (iter);

	return count;
}

/*
 * ECard lifecycle management and vCard loading/saving.
 */

static void
e_card_dispose (GObject *object)
{
	ECard *card = E_CARD(object);

#define FREE_IF(x) do { if ((x)) { g_free (x); x = NULL; } } while (0)
#define UNREF_IF(x) do { if ((x)) { g_object_unref (x); x = NULL; } } while (0)

	FREE_IF (card->id);
	UNREF_IF (card->book);
	FREE_IF(card->file_as);
	FREE_IF(card->fname);
	if (card->name) {
		e_card_name_unref(card->name);
		card->name = NULL;
	}
	FREE_IF(card->bday);

	FREE_IF(card->url);
	FREE_IF(card->org);
	FREE_IF(card->org_unit);
	FREE_IF(card->office);
	FREE_IF(card->title);
	FREE_IF(card->role);
	FREE_IF(card->manager);
	FREE_IF(card->assistant);
	FREE_IF(card->nickname);
	FREE_IF(card->spouse);
	FREE_IF(card->anniversary);
	FREE_IF(card->caluri);
	FREE_IF(card->fburl);
	FREE_IF(card->icscalendar);
	FREE_IF(card->last_use);
	FREE_IF(card->note);
	FREE_IF(card->related_contacts);

	UNREF_IF (card->categories);
	UNREF_IF (card->email);
	UNREF_IF (card->phone);
	UNREF_IF (card->address);
	UNREF_IF (card->address_label);

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}


/* Set_arg handler for the card */
static void
e_card_set_property (GObject *object,
		     guint prop_id,
		     const GValue *value,
		     GParamSpec *pspec)
{
	ECard *card;
	
	card = E_CARD (object);

	switch (prop_id) {
	case PROP_FILE_AS:
		g_free(card->file_as);
		card->file_as = g_strdup(g_value_get_string (value));
		if (card->file_as == NULL)
			card->file_as = g_strdup("");
		break;

	case PROP_FULL_NAME:
		g_free(card->fname);
		card->fname = g_strdup(g_value_get_string (value));
		if (card->fname == NULL)
			card->fname = g_strdup("");

		e_card_name_unref (card->name);
		card->name = e_card_name_from_string (card->fname);
		break;
	case PROP_NAME:
		e_card_name_unref (card->name);
		card->name = e_card_name_ref(g_value_get_pointer (value));
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
	case PROP_CATEGORIES:
		if (card->categories)
			g_object_unref(card->categories);
		card->categories = NULL;
		if (g_value_get_string (value))
			do_parse_categories(card, (char*)g_value_get_string (value));
		break;
	case PROP_CATEGORY_LIST:
		if (card->categories)
			g_object_unref(card->categories);
		card->categories = E_LIST(g_value_get_object(value));
		if (card->categories)
			g_object_ref(card->categories);
		break;
	case PROP_BIRTH_DATE:
		g_free(card->bday);
		if (g_value_get_pointer (value)) {
			card->bday = g_new (ECardDate, 1);
			memcpy (card->bday, g_value_get_pointer (value), sizeof (ECardDate));
		} else {
			card->bday = NULL;
		}
		break;
	case PROP_URL:
		g_free(card->url);
		card->url = g_strdup(g_value_get_string(value));
		break;
	case PROP_ORG:
		g_free(card->org);
		card->org = g_strdup(g_value_get_string(value));
		break;
	case PROP_ORG_UNIT:
		g_free(card->org_unit);
		card->org_unit = g_strdup(g_value_get_string(value));
		break;
	case PROP_OFFICE:
		g_free(card->office);
		card->office = g_strdup(g_value_get_string(value));
		break;
	case PROP_TITLE:
		g_free(card->title);
		card->title = g_strdup(g_value_get_string(value));
		break;
	case PROP_ROLE:
		g_free(card->role);
		card->role = g_strdup(g_value_get_string(value));
		break;
	case PROP_MANAGER:
		g_free(card->manager);
		card->manager = g_strdup(g_value_get_string(value));
		break;
	case PROP_ASSISTANT:
		g_free(card->assistant);
		card->assistant = g_strdup(g_value_get_string(value));
		break;
	case PROP_NICKNAME:
		g_free(card->nickname);
		card->nickname = g_strdup(g_value_get_string(value));
		break;
	case PROP_SPOUSE:
		g_free(card->spouse);
		card->spouse = g_strdup(g_value_get_string(value));
		break;
	case PROP_ANNIVERSARY:
		g_free(card->anniversary);
		if (g_value_get_pointer (value)) {
			card->anniversary = g_new (ECardDate, 1);
			memcpy (card->anniversary, g_value_get_pointer (value), sizeof (ECardDate));
		} else {
			card->anniversary = NULL;
		}
		break;
	case PROP_MAILER:
		g_free(card->mailer);
		card->mailer = g_strdup(g_value_get_string(value));
		break;
	case PROP_CALURI:
		g_free(card->caluri);
		card->caluri = g_strdup(g_value_get_string(value));
		break;
	case PROP_FBURL:
		g_free(card->fburl);
		card->fburl = g_strdup(g_value_get_string(value));
		break;
	case PROP_ICSCALENDAR:
		g_free(card->icscalendar);
		card->icscalendar = g_strdup(g_value_get_string(value));
		break;
	case PROP_NOTE:
		g_free (card->note);
		card->note = g_strdup(g_value_get_string(value));
		break;
	case PROP_RELATED_CONTACTS:
		g_free (card->related_contacts);
		card->related_contacts = g_strdup(g_value_get_string(value));
		break;
	case PROP_WANTS_HTML:
		card->wants_html = g_value_get_boolean (value);
		card->wants_html_set = TRUE;
		break;
	case PROP_ARBITRARY:
		if (card->arbitrary)
			g_object_unref(card->arbitrary);
		card->arbitrary = E_LIST(g_value_get_pointer(value));
		if (card->arbitrary)
			g_object_ref(card->arbitrary);
		break;
	case PROP_ID:
		g_free(card->id);
		card->id = g_strdup(g_value_get_string(value));
		if (card->id == NULL)
			card->id = g_strdup ("");
		break;
	case PROP_LAST_USE:
		g_free(card->last_use);
		if (g_value_get_pointer (value)) {
			card->last_use = g_new (ECardDate, 1);
			memcpy (card->last_use, g_value_get_pointer (value), sizeof (ECardDate));
		} else {
			card->last_use = NULL;
		}
		break;
	case PROP_USE_SCORE:
		card->raw_use_score = g_value_get_float (value);
		break;
	case PROP_EVOLUTION_LIST:
		card->list = g_value_get_boolean (value);
		break;
	case PROP_EVOLUTION_LIST_SHOW_ADDRESSES:
		card->list_show_addresses = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* Get_arg handler for the card */
static void
e_card_get_property (GObject *object,
		     guint prop_id,
		     GValue *value,
		     GParamSpec *pspec)
{
	ECard *card;

	card = E_CARD (object);

	switch (prop_id) {
	case PROP_FILE_AS:
		g_value_set_string (value, card->file_as);
		break;
	case PROP_FULL_NAME:
		g_value_set_string (value, card->fname);
		break;
	case PROP_NAME:
		g_value_set_pointer (value, card->name);
		break;
	case PROP_ADDRESS:
		if (!card->address)
			card->address = e_list_new((EListCopyFunc) e_card_delivery_address_ref,
						   (EListFreeFunc) e_card_delivery_address_unref,
						   NULL);
		g_value_set_object (value, card->address);
		break;
	case PROP_ADDRESS_LABEL:
		if (!card->address_label)
			card->address_label = e_list_new((EListCopyFunc) e_card_address_label_ref,
							 (EListFreeFunc) e_card_address_label_unref,
							 NULL);
		g_value_set_object (value, card->address_label);
		break;
	case PROP_PHONE:
		if (!card->phone)
			card->phone = e_list_new((EListCopyFunc) e_card_phone_ref,
						 (EListFreeFunc) e_card_phone_unref,
						 NULL);
		g_value_set_object (value, card->phone);
		break;
	case PROP_EMAIL:
		if (!card->email)
			card->email = e_list_new((EListCopyFunc) g_strdup, 
						 (EListFreeFunc) g_free,
						 NULL);
		g_value_set_object (value, card->email);
		break;
	case PROP_CATEGORIES:
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
			g_value_set_string_take_ownership(value, g_strjoinv(", ", strs));
			g_free(strs);
		}
		break;
	case PROP_CATEGORY_LIST:
		if (!card->categories)
			card->categories = e_list_new((EListCopyFunc) g_strdup, 
						      (EListFreeFunc) g_free,
						      NULL);
		g_value_set_object (value, card->categories);
		break;
	case PROP_BIRTH_DATE:
		g_value_set_pointer (value, card->bday);
		break;
	case PROP_URL:
		g_value_set_string (value, card->url);
		break;
	case PROP_ORG:
		g_value_set_string (value, card->org);
		break;
	case PROP_ORG_UNIT:
		g_value_set_string (value, card->org_unit);
		break;
	case PROP_OFFICE:
		g_value_set_string (value, card->office);
		break;
	case PROP_TITLE:
		g_value_set_string (value, card->title);
		break;
	case PROP_ROLE:
		g_value_set_string (value, card->role);
		break;
	case PROP_MANAGER:
		g_value_set_string (value, card->manager);
		break;
	case PROP_ASSISTANT:
		g_value_set_string (value, card->assistant);
		break;
	case PROP_NICKNAME:
		g_value_set_string (value, card->nickname);
		break;
	case PROP_SPOUSE:
		g_value_set_string (value, card->spouse);
		break;
	case PROP_ANNIVERSARY:
		g_value_set_pointer (value, card->anniversary);
		break;
	case PROP_MAILER:
		g_value_set_string (value, card->mailer);
		break;
	case PROP_CALURI:
		g_value_set_string (value, card->caluri);
		break;
	case PROP_FBURL:
		g_value_set_string (value, card->fburl);
		break;
	case PROP_ICSCALENDAR:
		g_value_set_string (value, card->icscalendar);
		break;
	case PROP_NOTE:
		g_value_set_string (value, card->note);
		break;
	case PROP_RELATED_CONTACTS:
		g_value_set_string (value, card->related_contacts);
		break;
	case PROP_WANTS_HTML:
		g_value_set_boolean (value, card->wants_html);
		break;
	case PROP_WANTS_HTML_SET:
		g_value_set_boolean (value, card->wants_html_set);
		break;
	case PROP_ARBITRARY:
		if (!card->arbitrary)
			card->arbitrary = e_list_new((EListCopyFunc) e_card_arbitrary_ref,
						     (EListFreeFunc) e_card_arbitrary_unref,
						     NULL);

		g_value_set_object (value, card->arbitrary);
		break;
	case PROP_ID:
		g_value_set_string (value, card->id);
		break;
	case PROP_LAST_USE:
		g_value_set_pointer (value, card->last_use);
		break;
	case PROP_USE_SCORE:
		g_value_set_float (value, e_card_get_use_score (card));
		break;
	case PROP_EVOLUTION_LIST:
		g_value_set_boolean (value, card->list);
		break;
	case PROP_EVOLUTION_LIST_SHOW_ADDRESSES:
		g_value_set_boolean (value, card->list_show_addresses);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
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
e_card_load_cards_from_file_with_default_charset(const char *filename, const char *default_charset)
{
	VObject *vobj = Parse_MIME_FromFileName((char *) filename);
	GList *list = NULL;
	while(vobj) {
		VObject *next;
		ECard *card = g_object_new (E_TYPE_CARD, NULL);
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
e_card_load_cards_from_string_with_default_charset(const char *str, const char *default_charset)
{
	VObject *vobj = Parse_MIME(str, strlen (str));
	GList *list = NULL;
	while(vobj) {
		VObject *next;
		ECard *card = g_object_new (E_TYPE_CARD, NULL);
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
		g_object_unref (card->address);
		card->address = NULL;
	}

	if (card->address_label && e_list_length (card->address_label) == 0) {
		g_object_unref (card->address_label);
		card->address_label = NULL;
	}

	if (card->phone && e_list_length (card->phone) == 0) {
		g_object_unref (card->phone);
		card->phone = NULL;
	}

	if (card->email && e_list_length (card->email) == 0) {
		g_object_unref (card->email);
		card->email = NULL;
	}

	if (card->categories && e_list_length (card->categories) == 0) {
		g_object_unref (card->categories);
		card->categories = NULL;
	}

	if (card->arbitrary && e_list_length (card->arbitrary) == 0) {
		g_object_unref (card->arbitrary);
		card->arbitrary = NULL;
	}
}

static void
assign_string(VObject *vobj, const char *default_charset, char **string)
{
	int type = vObjectValueType(vobj);
	char *str;
	const char *charset = default_charset;
	char *charset_buf = NULL;
	VObject *charset_obj;

	if ((charset_obj = isAPropertyOf (vobj, "CHARSET"))) {
		switch (vObjectValueType (charset_obj)) {
		case VCVT_STRINGZ:
			charset = vObjectStringZValue(charset_obj);
			break;
		case VCVT_USTRINGZ:
			charset_buf = fakeCString (vObjectUStringZValue (charset_obj));
			charset = charset_buf;
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

	if (charset_buf) {
		free (charset_buf);
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
e_v_object_get_child_value(VObject *vobj, char *name, const char *default_charset)
{
	char *ret_val;
	VObjectIterator iterator;
	char *charset_buf = NULL;
	VObject *charset_obj;

	if ((charset_obj = isAPropertyOf (vobj, "CHARSET"))) {
		switch (vObjectValueType (charset_obj)) {
		case VCVT_STRINGZ:
			default_charset = vObjectStringZValue(charset_obj);
			break;
		case VCVT_USTRINGZ:
			charset_buf = fakeCString (vObjectUStringZValue (charset_obj));
			default_charset = charset_buf;
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
	if (charset_buf)
		free (charset_buf);

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
