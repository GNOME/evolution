/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Chris Lahey     <clahey@ximian.com>
 *   Arturo Espinosa (arturo@nuclecu.unam.mx)
 *   Nat Friedman    (nat@ximian.com)
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <gal/util/e-util.h>

#include <libversit/vcc.h>
#include "e-card-simple.h"

/* Object property IDs */
enum {
	PROP_0,
	PROP_CARD,
};

static GObjectClass *parent_class;

typedef enum _ECardSimpleInternalType ECardSimpleInternalType;
typedef struct _ECardSimpleFieldData ECardSimpleFieldData;

enum _ECardSimpleInternalType {
	E_CARD_SIMPLE_INTERNAL_TYPE_STRING,
	E_CARD_SIMPLE_INTERNAL_TYPE_DATE,
	E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS,
	E_CARD_SIMPLE_INTERNAL_TYPE_PHONE,
	E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL,
	E_CARD_SIMPLE_INTERNAL_TYPE_SPECIAL,
	E_CARD_SIMPLE_INTERNAL_TYPE_BOOL,
};

struct _ECardSimpleFieldData {
	ECardSimpleField         field;
	char                    *ecard_field;
	char                    *name;
	char                    *short_name;
	int                      list_type_index;
	ECardSimpleInternalType  type;
};

/* This order must match the order in the .h. */

/* the ecard_field data below should only be used for TYPE_STRING,
   TYPE_DATE, and TYPE_SPECIAL fields.  that is, it's only valid for
   e-cards for those types.  it is used as a unique name for fields
   for the get_supported functionality.  */
static ECardSimpleFieldData field_data[] =
{
	{ E_CARD_SIMPLE_FIELD_FILE_AS,            "file_as",         N_("File As"),       "",             0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_FULL_NAME,          "full_name",       N_("Name"),          N_("Name"),     0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_EMAIL,              "email",           N_("Email"),         N_("Email"),    E_CARD_SIMPLE_EMAIL_ID_EMAIL,        E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL },
	{ E_CARD_SIMPLE_FIELD_PHONE_PRIMARY,      "primary_phone",   N_("Primary"),       N_("Prim"),     E_CARD_SIMPLE_PHONE_ID_PRIMARY,      E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_ASSISTANT,    "assistant_phone", N_("Assistant"),     N_("Assistant"),E_CARD_SIMPLE_PHONE_ID_ASSISTANT,    E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_BUSINESS,     "business_phone",  N_("Business"),      N_("Bus"),      E_CARD_SIMPLE_PHONE_ID_BUSINESS,     E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_CALLBACK,     "callback_phone",  N_("Callback"),      N_("Callback"), E_CARD_SIMPLE_PHONE_ID_CALLBACK,     E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_COMPANY,      "company_phone",   N_("Company"),       N_("Comp"),     E_CARD_SIMPLE_PHONE_ID_COMPANY,      E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_HOME,         "home_phone",      N_("Home"),          N_("Home"),     E_CARD_SIMPLE_PHONE_ID_HOME,         E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_ORG,                "org",             N_("Organization"),  N_("Org"),      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_ADDRESS_BUSINESS,   "business_address",N_("Business"),      N_("Bus"),      E_CARD_SIMPLE_ADDRESS_ID_BUSINESS,   E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS },
	{ E_CARD_SIMPLE_FIELD_ADDRESS_HOME,       "home_address",    N_("Home"),          N_("Home"),     E_CARD_SIMPLE_ADDRESS_ID_HOME,       E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS },
	{ E_CARD_SIMPLE_FIELD_PHONE_MOBILE,       "mobile_phone",    N_("Mobile"),        N_("Mobile"),   E_CARD_SIMPLE_PHONE_ID_MOBILE,       E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_CAR,          "car_phone",       N_("Car"),           N_("Car"),      E_CARD_SIMPLE_PHONE_ID_CAR,          E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_FAX, "business_fax",    N_("Business Fax"),  N_("Bus Fax"),  E_CARD_SIMPLE_PHONE_ID_BUSINESS_FAX, E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_HOME_FAX,     "home_fax",        N_("Home Fax"),      N_("Home Fax"), E_CARD_SIMPLE_PHONE_ID_HOME_FAX,     E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_2,   "business_phone_2",N_("Business 2"),    N_("Bus 2"),    E_CARD_SIMPLE_PHONE_ID_BUSINESS_2,   E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_HOME_2,       "home_phone_2",    N_("Home 2"),        N_("Home 2"),   E_CARD_SIMPLE_PHONE_ID_HOME_2,       E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_ISDN,         "isdn",            N_("ISDN"),          N_("ISDN"),     E_CARD_SIMPLE_PHONE_ID_ISDN,         E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_OTHER,        "other_phone",     N_("Other"),         N_("Other"),    E_CARD_SIMPLE_PHONE_ID_OTHER,        E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_OTHER_FAX,    "other_fax",       N_("Other Fax"),     N_("Other Fax"), E_CARD_SIMPLE_PHONE_ID_OTHER_FAX,   E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_PAGER,        "pager",           N_("Pager"),         N_("Pager"),    E_CARD_SIMPLE_PHONE_ID_PAGER,        E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_RADIO,        "radio",           N_("Radio"),         N_("Radio"),    E_CARD_SIMPLE_PHONE_ID_RADIO,        E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_TELEX,        "telex",           N_("Telex"),         N_("Telex"),    E_CARD_SIMPLE_PHONE_ID_TELEX,        E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_TTYTDD,       "tty",             N_("TTY"),           N_("TTY"),      E_CARD_SIMPLE_PHONE_ID_TTYTDD,       E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_ADDRESS_OTHER,      "other_address",   N_("Other"),         N_("Other"),    E_CARD_SIMPLE_ADDRESS_ID_OTHER,      E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS },
	{ E_CARD_SIMPLE_FIELD_EMAIL_2,            "email_2",         N_("Email 2"),       N_("Email 2"),  E_CARD_SIMPLE_EMAIL_ID_EMAIL_2,      E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL },
	{ E_CARD_SIMPLE_FIELD_EMAIL_3,            "email_3",         N_("Email 3"),       N_("Email 3"),  E_CARD_SIMPLE_EMAIL_ID_EMAIL_3,      E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL },
	{ E_CARD_SIMPLE_FIELD_URL,                "url",             N_("Web Site"),      N_("Url"),      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_ORG_UNIT,           "org_unit",        N_("Department"),    N_("Dep"),      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_OFFICE,             "office",          N_("Office"),        N_("Off"),      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_TITLE,              "title",           N_("Title"),         N_("Title"),    0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_ROLE,               "role",            N_("Profession"),    N_("Prof"),     0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_MANAGER,            "manager",         N_("Manager"),       N_("Man"),      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_ASSISTANT,          "assistant",       N_("Assistant"),     N_("Ass"),      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_NICKNAME,           "nickname",        N_("Nickname"),      N_("Nick"),     0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_SPOUSE,             "spouse",          N_("Spouse"),        N_("Spouse"),   0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_NOTE,               "note",            N_("Note"),          N_("Note"),     0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
 	{ E_CARD_SIMPLE_FIELD_CALURI,             "caluri",          N_("Calendar URI"),  N_("CALUri"),   0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_FBURL,              "fburl",           N_("Free-busy URL"), N_("FBUrl"),    0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_ICSCALENDAR,        "icscalendar",     N_("Default server calendar"), N_("icsCalendar"),    0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_ANNIVERSARY,        "anniversary",     N_("Anniversary"),   N_("Anniv"),    0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_DATE },
	{ E_CARD_SIMPLE_FIELD_BIRTH_DATE,         "birth_date",      N_("Birth Date"),    "",             0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_DATE },
	{ E_CARD_SIMPLE_FIELD_MAILER,             "mailer",          "",                  "",             0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_NAME_OR_ORG,        "nameororg",       "",                  "",             0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_SPECIAL },
	{ E_CARD_SIMPLE_FIELD_CATEGORIES,         "categories",      N_("Categories"),    N_("Categories"), 0,                                 E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_FAMILY_NAME,        "family_name",     N_("Family Name"),   N_("Family Name"), 0,                                E_CARD_SIMPLE_INTERNAL_TYPE_SPECIAL },
 	{ E_CARD_SIMPLE_FIELD_GIVEN_NAME,         "given_name",      "Given Name",    "Given Name",  0,                                E_CARD_SIMPLE_INTERNAL_TYPE_SPECIAL },
 	{ E_CARD_SIMPLE_FIELD_ADDITIONAL_NAME,    "additional_name",  "Additional Name", "Additional Name",  0,                        E_CARD_SIMPLE_INTERNAL_TYPE_SPECIAL },
 	{ E_CARD_SIMPLE_FIELD_NAME_SUFFIX,        "name_suffix",     "Name Suffix",   "Name Suffix",  0,                               E_CARD_SIMPLE_INTERNAL_TYPE_SPECIAL },
 	{ E_CARD_SIMPLE_FIELD_WANTS_HTML,         "wants_html",      "Wants HTML",    "Wants HTML",   0,                               E_CARD_SIMPLE_INTERNAL_TYPE_BOOL },
 	{ E_CARD_SIMPLE_FIELD_IS_LIST,            "list",            "Is List",       "Is List",      0,                               E_CARD_SIMPLE_INTERNAL_TYPE_BOOL },
};
static int field_data_count = sizeof (field_data) / sizeof (field_data[0]);

static void e_card_simple_init (ECardSimple *simple);
static void e_card_simple_class_init (ECardSimpleClass *klass);

static void e_card_simple_dispose (GObject *object);
static void e_card_simple_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_card_simple_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static void fill_in_info(ECardSimple *simple);

ECardPhoneFlags phone_correspondences[] = {
	E_CARD_PHONE_ASSISTANT, /* E_CARD_SIMPLE_PHONE_ID_ASSISTANT,    */
	E_CARD_PHONE_WORK | E_CARD_PHONE_VOICE, /* E_CARD_SIMPLE_PHONE_ID_BUSINESS,	   */
	E_CARD_PHONE_WORK | E_CARD_PHONE_VOICE, /* E_CARD_SIMPLE_PHONE_ID_BUSINESS_2,   */
	E_CARD_PHONE_WORK | E_CARD_PHONE_FAX, /* E_CARD_SIMPLE_PHONE_ID_BUSINESS_FAX, */
	E_CARD_PHONE_CALLBACK, /* E_CARD_SIMPLE_PHONE_ID_CALLBACK,	   */
	E_CARD_PHONE_CAR, /* E_CARD_SIMPLE_PHONE_ID_CAR,	   */
	E_CARD_PHONE_WORK, /* E_CARD_SIMPLE_PHONE_ID_COMPANY,	   */
	E_CARD_PHONE_HOME, /* E_CARD_SIMPLE_PHONE_ID_HOME,	   */
	E_CARD_PHONE_HOME, /* E_CARD_SIMPLE_PHONE_ID_HOME_2,	   */
	E_CARD_PHONE_HOME | E_CARD_PHONE_FAX, /* E_CARD_SIMPLE_PHONE_ID_HOME_FAX,	   */
	E_CARD_PHONE_ISDN, /* E_CARD_SIMPLE_PHONE_ID_ISDN,	   */
	E_CARD_PHONE_CELL, /* E_CARD_SIMPLE_PHONE_ID_MOBILE,	   */
	E_CARD_PHONE_VOICE, /* E_CARD_SIMPLE_PHONE_ID_OTHER,	   */
	E_CARD_PHONE_FAX, /* E_CARD_SIMPLE_PHONE_ID_OTHER_FAX,	   */
	E_CARD_PHONE_PAGER, /* E_CARD_SIMPLE_PHONE_ID_PAGER,	   */
	E_CARD_PHONE_PREF, /* E_CARD_SIMPLE_PHONE_ID_PRIMARY,	   */
	E_CARD_PHONE_RADIO, /* E_CARD_SIMPLE_PHONE_ID_RADIO,	   */
	E_CARD_PHONE_TELEX, /* E_CARD_SIMPLE_PHONE_ID_TELEX,	   */
	E_CARD_PHONE_TTYTDD, /* E_CARD_SIMPLE_PHONE_ID_TTYTDD,	   */
};

char *phone_names[] = {
	NULL, /* E_CARD_SIMPLE_PHONE_ID_ASSISTANT,    */
	"Business",
	"Business 2",
	"Business Fax",
	NULL, /* E_CARD_SIMPLE_PHONE_ID_CALLBACK,	   */
	"Car",
	NULL, /* E_CARD_SIMPLE_PHONE_ID_COMPANY,	   */
	"Home",
	"Home 2",
	"Home Fax",
	"ISDN",
	"Mobile",
	"Other",
	NULL, /* E_CARD_SIMPLE_PHONE_ID_OTHER_FAX,	   */
	"Pager",
	"Primary",
	NULL, /* E_CARD_SIMPLE_PHONE_ID_RADIO,	   */
	NULL, /* E_CARD_SIMPLE_PHONE_ID_TELEX,	   */
	NULL, /* E_CARD_SIMPLE_PHONE_ID_TTYTDD,	   */
};

char *phone_short_names[] = {
	NULL, /* E_CARD_SIMPLE_PHONE_ID_ASSISTANT,    */
	"Bus",
	"Bus 2",
	"Bus Fax",
	NULL, /* E_CARD_SIMPLE_PHONE_ID_CALLBACK,	   */
	"Car",
	NULL, /* E_CARD_SIMPLE_PHONE_ID_COMPANY,	   */
	"Home",
	"Home 2",
	"Home Fax",
	"ISDN",
	"Mob",
	"Other",
	NULL, /* E_CARD_SIMPLE_PHONE_ID_OTHER_FAX,	   */
	"Pag",
	"Prim",
	NULL, /* E_CARD_SIMPLE_PHONE_ID_RADIO,	   */
	NULL, /* E_CARD_SIMPLE_PHONE_ID_TELEX,	   */
	NULL, /* E_CARD_SIMPLE_PHONE_ID_TTYTDD,	   */
};

ECardAddressFlags addr_correspondences[] = {
	E_CARD_ADDR_WORK, /* E_CARD_SIMPLE_ADDRESS_ID_BUSINESS, */
	E_CARD_ADDR_HOME, /* E_CARD_SIMPLE_ADDRESS_ID_HOME,	 */
	E_CARD_ADDR_POSTAL, /* E_CARD_SIMPLE_ADDRESS_ID_OTHER,    */
};

char *address_names[] = {
	"Business",
	"Home",
	"Other",
};

/**
 * e_card_simple_get_type:
 * @void: 
 * 
 * Registers the &ECardSimple class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ECardSimple class.
 **/
GType
e_card_simple_get_type (void)
{
	static GType simple_type = 0;

	if (!simple_type) {
		static const GTypeInfo simple_info =  {
			sizeof (ECardSimpleClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_card_simple_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ECardSimple),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_card_simple_init,
		};

		simple_type = g_type_register_static (G_TYPE_OBJECT, "ECardSimple", &simple_info, 0);
	}

	return simple_type;
}

/**
 * e_card_simple_new:
 * @VCard: a string in vCard format
 *
 * Returns: a new #ECardSimple that wraps the @VCard.
 */
ECardSimple *
e_card_simple_new (ECard *card)
{
	ECardSimple *simple = g_object_new (E_TYPE_CARD_SIMPLE, NULL);
	g_object_set(simple,
		     "card", card,
		     NULL);
	return simple;
}

ECardSimple *
e_card_simple_duplicate(ECardSimple *simple)
{
	ECard *card = simple->card ? e_card_duplicate (simple->card) : e_card_new ("");
	ECardSimple *new_simple = e_card_simple_new(card);
	return new_simple;
}

/**
 * e_card_simple_get_id:
 * @simple: an #ECardSimple
 *
 * Returns: a string representing the id of the simple, which is unique
 * within its book.
 */
const char *
e_card_simple_get_id (ECardSimple *simple)
{
	if (simple->card)
		return e_card_get_id(simple->card);
	else
		return "";
}

/**
 * e_card_simple_get_id:
 * @simple: an #ECardSimple
 * @id: a id in string format
 *
 * Sets the identifier of a simple, which should be unique within its
 * book.
 */
void
e_card_simple_set_id (ECardSimple *simple, const char *id)
{
	if ( simple->card )
		e_card_set_id(simple->card, id);
}

/**
 * e_card_simple_get_vcard:
 * @simple: an #ECardSimple
 *
 * Returns: a string in vcard format, which is wrapped by the @simple.
 */
char *
e_card_simple_get_vcard (ECardSimple *simple)
{
	if (simple->card)
		return e_card_get_vcard(simple->card);
	else
		return g_strdup("");
}

/**
 * e_card_simple_get_vcard_assume_utf8:
 * @simple: an #ECardSimple
 *
 * Returns: a string in vcard format, which is wrapped by the @simple.
 */
char *
e_card_simple_get_vcard_assume_utf8 (ECardSimple *simple)
{
	if (simple->card)
		return e_card_get_vcard_assume_utf8(simple->card);
	else
		return g_strdup("");
}

static void
e_card_simple_class_init (ECardSimpleClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	object_class->dispose = e_card_simple_dispose;
	object_class->get_property = e_card_simple_get_property;
	object_class->set_property = e_card_simple_set_property;

	g_object_class_install_property (object_class, PROP_CARD, 
					 g_param_spec_object ("card",
							      _("ECard"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_CARD,
							      G_PARAM_READWRITE));
}

/*
 * ECardSimple lifecycle management and vcard loading/saving.
 */

static void
e_card_simple_dispose (GObject *object)
{
	ECardSimple *simple;
	int i;
	
	simple = E_CARD_SIMPLE (object);

	if (simple->card) {
		g_object_unref(simple->card);
		simple->card = NULL;
	}
	if (simple->temp_fields) {
		g_list_foreach(simple->temp_fields, (GFunc) g_free, NULL);
		g_list_free(simple->temp_fields);
		simple->temp_fields = NULL;
	}

	for(i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i++) {
		if (simple->phone[i]) {
			e_card_phone_unref (simple->phone[i]);
			simple->phone[i] = NULL;
		}
	}
	for(i = 0; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i++) {
		if (simple->email[i]) {
			g_free(simple->email[i]);
			simple->email[i] = NULL;
		}
	}
	for(i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i++) {
		if (simple->address[i]) {
			e_card_address_label_unref(simple->address[i]);
			simple->address[i] = NULL;
		}
	}
	for(i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i++) {
		if (simple->delivery[i]) {
			e_card_delivery_address_unref(simple->delivery[i]);
			simple->delivery[i] = NULL;
		}
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}


/* Set_arg handler for the simple */
static void
e_card_simple_set_property (GObject *object,
			    guint prop_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	ECardSimple *simple;
	
	simple = E_CARD_SIMPLE (object);

	switch (prop_id) {
	case PROP_CARD:
		if (simple->card)
			g_object_unref(simple->card);
		g_list_foreach(simple->temp_fields, (GFunc) g_free, NULL);
		g_list_free(simple->temp_fields);
		simple->temp_fields = NULL;
		if (g_value_get_object (value))
			simple->card = E_CARD(g_value_get_object (value));
		else
			simple->card = NULL;
		if(simple->card)
			g_object_ref(simple->card);
		fill_in_info(simple);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* Get_arg handler for the simple */
static void
e_card_simple_get_property (GObject *object,
			    guint prop_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	ECardSimple *simple;

	simple = E_CARD_SIMPLE (object);

	switch (prop_id) {
	case PROP_CARD:
		e_card_simple_sync_card(simple);
		g_value_set_object (value, simple->card);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


/**
 * e_card_simple_init:
 */
static void
e_card_simple_init (ECardSimple *simple)
{
	int i;
	simple->card = NULL;
	for(i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i++)
		simple->phone[i] = NULL;
	for(i = 0; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i++)
		simple->email[i] = NULL;
	for(i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i++)
		simple->address[i] = NULL;
	simple->temp_fields = NULL;

	simple->changed = TRUE;
}

static void
fill_in_info(ECardSimple *simple)
{
	ECard *card = simple->card;
	if (card) {
		EList *address_list;
		EList *phone_list;
		EList *email_list;
		EList *delivery_list;
		const ECardPhone *phone;
		const char *email;
		const ECardAddrLabel *address;
		const ECardDeliveryAddress *delivery;
		int i;

		EIterator *iterator;

		g_object_get(card,
			     "address_label", &address_list,
			     "address",       &delivery_list,
			     "phone",         &phone_list,
			     "email",         &email_list,
			     NULL);
		for (i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i++) {
			e_card_phone_unref(simple->phone[i]);
			simple->phone[i] = NULL;
		}
		for (iterator = e_list_get_iterator(phone_list); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			gboolean found = FALSE;
			phone = e_iterator_get(iterator);
			for (i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i ++) {
				if ((phone->flags == phone_correspondences[i]) && (simple->phone[i] == NULL)) {
					simple->phone[i] = e_card_phone_ref(phone);
					found = TRUE;
					break;
				}
			}
			if (found)
				continue;
			for (i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i ++) {
				if (((phone->flags & phone_correspondences[i]) == phone_correspondences[i]) && (simple->phone[i] == NULL)) {
					simple->phone[i] = e_card_phone_ref(phone);
					break;
				}
			}
		}
		g_object_unref(iterator);

		for (i = 0; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i++) {
			g_free(simple->email[i]);
			simple->email[i] = NULL;
		}
		for (iterator = e_list_get_iterator(email_list); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			email = e_iterator_get(iterator);
			for (i = 0; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i ++) {
				if ((simple->email[i] == NULL)) {
					simple->email[i] = g_strdup(email);
					break;
				}
			}
		}
		g_object_unref(iterator);

		for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i++) {
			e_card_address_label_unref(simple->address[i]);
			simple->address[i] = NULL;
		}
		for (iterator = e_list_get_iterator(address_list); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			address = e_iterator_get(iterator);
			for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i ++) {
				if (((address->flags & addr_correspondences[i]) == addr_correspondences[i]) && (simple->address[i] == NULL)) {
					simple->address[i] = e_card_address_label_ref(address);
					break;
				}
			}
		}
		g_object_unref(iterator);

		for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i++) {
			e_card_delivery_address_unref(simple->delivery[i]);
			simple->delivery[i] = NULL;
		}
		for (iterator = e_list_get_iterator(delivery_list); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			delivery = e_iterator_get(iterator);
			for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i ++) {
				if (((delivery->flags & addr_correspondences[i]) == addr_correspondences[i]) && (simple->delivery[i] == NULL)) {
					simple->delivery[i] = e_card_delivery_address_ref(delivery);
					break;
				}
			}
		}
		g_object_unref(iterator);

		g_object_unref(phone_list);
		g_object_unref(email_list);
		g_object_unref(address_list);
		g_object_unref(delivery_list);
		e_card_free_empty_lists (card);
	}
}

void
e_card_simple_sync_card(ECardSimple *simple)
{
	ECard *card = simple->card;
	if (card && simple->changed) {
		EList *address_list;
		EList *phone_list;
		EList *email_list;
		EList *delivery_list;
		const ECardPhone *phone;
		const ECardAddrLabel *address;
		const ECardDeliveryAddress *delivery;
		const char *email;
		int i;

		EIterator *iterator;

		g_object_get(card,
			     "address_label", &address_list,
			     "address",       &delivery_list,
			     "phone",         &phone_list,
			     "email",         &email_list,
			     NULL);

		for (iterator = e_list_get_iterator(phone_list); e_iterator_is_valid(iterator); e_iterator_next(iterator) ) {
			int i;
			gboolean found = FALSE;
			phone = e_iterator_get(iterator);
			for (i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i ++) {
				if (phone->flags == phone_correspondences[i]) {
					if (simple->phone[i]) {
						simple->phone[i]->flags = phone_correspondences[i];
						if (simple->phone[i]->number && *simple->phone[i]->number) {
							e_iterator_set(iterator, simple->phone[i]);
						} else {
							e_iterator_delete(iterator);
						}
						e_card_phone_unref(simple->phone[i]);
						simple->phone[i] = NULL;
						found = TRUE;
						break;
					}
				}
			}
			if (found)
				continue;
			for (i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i ++) {
				if ((phone->flags & phone_correspondences[i]) == phone_correspondences[i]) {
					if (simple->phone[i]) {
						simple->phone[i]->flags = phone_correspondences[i];
						if (simple->phone[i]->number && *simple->phone[i]->number) {
							e_iterator_set(iterator, simple->phone[i]);
						} else {
							e_iterator_delete(iterator);
						}
						e_card_phone_unref(simple->phone[i]);
						simple->phone[i] = NULL;
						break;
					}
				}
			}
		}	
		g_object_unref(iterator);
		for (i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i ++) {
			if (simple->phone[i]) {
				simple->phone[i]->flags = phone_correspondences[i];
				e_list_append(phone_list, simple->phone[i]);
				e_card_phone_unref(simple->phone[i]);
				simple->phone[i] = NULL;
			}
		}

		for (iterator = e_list_get_iterator(email_list); e_iterator_is_valid(iterator); e_iterator_next(iterator) ) {
			int i;
			email = e_iterator_get(iterator);
			for (i = 0; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i ++) {
				if (simple->email[i]) {
					if (*simple->email[i]) {
						e_iterator_set(iterator, simple->email[i]);
					} else {
						e_iterator_delete(iterator);
					}
					g_free(simple->email[i]);
					simple->email[i] = NULL;
					break;
				}
			}
		}	
		g_object_unref(iterator);
		for (i = 0; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i ++) {
			if (simple->email[i]) {
				e_list_append(email_list, simple->email[i]);
				g_free(simple->email[i]);
				simple->email[i] = NULL;
			}
		}

		for (iterator = e_list_get_iterator(address_list); e_iterator_is_valid(iterator); e_iterator_next(iterator) ) {
			int i;
			address = e_iterator_get(iterator);
			for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i ++) {
				if ((address->flags & addr_correspondences[i]) == addr_correspondences[i]) {
					if (simple->address[i]) {
						simple->address[i]->flags &= ~E_CARD_ADDR_MASK;
						simple->address[i]->flags |= addr_correspondences[i];
						if (simple->address[i]->data && *simple->address[i]->data) {
							e_iterator_set(iterator, simple->address[i]);
						} else {
							e_iterator_delete(iterator);
						}
						e_card_address_label_unref(simple->address[i]);
						simple->address[i] = NULL;
						break;
					}
				}
			}
		}	
		g_object_unref(iterator);
		for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i ++) {
			if (simple->address[i]) {
				simple->address[i]->flags &= ~E_CARD_ADDR_MASK;
				simple->address[i]->flags |= addr_correspondences[i];
				e_list_append(address_list, simple->address[i]);
				e_card_address_label_unref(simple->address[i]);
				simple->address[i] = NULL;
			}
		}

		for (iterator = e_list_get_iterator(delivery_list); e_iterator_is_valid(iterator); e_iterator_next(iterator) ) {
			int i;
			delivery = e_iterator_get(iterator);
			for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i ++) {
				if ((delivery->flags & addr_correspondences[i]) == addr_correspondences[i]) {
					if (simple->delivery[i]) {
						simple->delivery[i]->flags &= ~E_CARD_ADDR_MASK;
						simple->delivery[i]->flags |= addr_correspondences[i];
						if (!e_card_delivery_address_is_empty(simple->delivery[i])) {
							e_iterator_set(iterator, simple->delivery[i]);
						} else {
							e_iterator_delete(iterator);
						}
						e_card_delivery_address_unref(simple->delivery[i]);
						simple->delivery[i] = NULL;
						break;
					}
				}
			}
		}	
		g_object_unref(iterator);
		for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i ++) {
			if (simple->delivery[i]) {
				simple->delivery[i]->flags &= ~E_CARD_ADDR_MASK;
				simple->delivery[i]->flags |= addr_correspondences[i];
				e_list_append(delivery_list, simple->delivery[i]);
				e_card_delivery_address_unref(simple->delivery[i]);
				simple->delivery[i] = NULL;
			}
		}
		fill_in_info(simple);

		g_object_unref(phone_list);
		g_object_unref(email_list);
		g_object_unref(address_list);
		g_object_unref(delivery_list);
		e_card_free_empty_lists (card);
	}

	simple->changed = FALSE;
}

const ECardPhone     *e_card_simple_get_phone   (ECardSimple          *simple,
						 ECardSimplePhoneId    id)
{
	return simple->phone[id];
}

const char           *e_card_simple_get_email   (ECardSimple          *simple,
						 ECardSimpleEmailId    id)
{
	return simple->email[id];
}

const ECardAddrLabel *e_card_simple_get_address (ECardSimple          *simple,
						 ECardSimpleAddressId  id)
{
	return simple->address[id];
}

const ECardDeliveryAddress *e_card_simple_get_delivery_address (ECardSimple          *simple,
								ECardSimpleAddressId  id)
{
	return simple->delivery[id];
}

void            e_card_simple_set_phone   (ECardSimple          *simple,
					   ECardSimplePhoneId    id,
					   const ECardPhone           *phone)
{
	e_card_phone_unref(simple->phone[id]);
	simple->phone[id] = e_card_phone_ref(phone);
	simple->changed = TRUE;
}

void            e_card_simple_set_email   (ECardSimple          *simple,
					   ECardSimpleEmailId    id,
					   const char                 *email)
{
	g_free(simple->email[id]);
	simple->email[id] = g_strdup(email);
	simple->changed = TRUE;
}

void
e_card_simple_set_address (ECardSimple *simple, ECardSimpleAddressId id, const ECardAddrLabel *address)
{
	e_card_address_label_unref(simple->address[id]);
	simple->address[id] = e_card_address_label_ref(address);
	e_card_delivery_address_unref(simple->delivery[id]);
	simple->delivery[id] = e_card_delivery_address_from_label(simple->address[id]);
	simple->changed = TRUE;
}

void            e_card_simple_set_delivery_address (ECardSimple          *simple,
						    ECardSimpleAddressId  id,
						    const ECardDeliveryAddress *delivery)
{
	e_card_delivery_address_unref(simple->delivery[id]);
	simple->delivery[id] = e_card_delivery_address_ref(delivery);
	e_card_address_label_unref(simple->address[id]);
	simple->address[id] = e_card_delivery_address_to_label(simple->delivery[id]);
	simple->changed = TRUE;
}

const char *e_card_simple_get_const    (ECardSimple          *simple,
					ECardSimpleField      field)
{
	char *ret_val = e_card_simple_get(simple, field);
	if (ret_val)
		simple->temp_fields = g_list_prepend(simple->temp_fields, ret_val);
	return ret_val;
}

char     *e_card_simple_get            (ECardSimple          *simple,
					ECardSimpleField      field)
{
	ECardSimpleInternalType type = field_data[field].type;
	const ECardAddrLabel *addr;
	const ECardPhone *phone;
	char *string;
	ECardDate *date;
	ECardName *name;
	switch(type) {
	case E_CARD_SIMPLE_INTERNAL_TYPE_STRING:
		if (simple->card) {
			g_object_get(simple->card,
				     field_data[field].ecard_field, &string,
				     NULL);
			return string;
		} else
			return NULL;
	case E_CARD_SIMPLE_INTERNAL_TYPE_DATE:
		if (simple->card) {
			g_object_get(simple->card,
				     field_data[field].ecard_field, &date,
				     NULL);
			if (date != NULL) {
				char buf[26];
				struct tm then;
				then.tm_year = date->year;
				then.tm_mon  = date->month - 1;
				then.tm_mday = date->day;
				then.tm_hour = 12;
				then.tm_min  = 0;
				then.tm_sec  = 0;
				e_strftime_fix_am_pm (buf, 26, _("%x"), &then);
				return g_strdup (buf);
			} else {
				return NULL;
			}
		} else
			return NULL;
	case E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS:
		addr = e_card_simple_get_address(simple,
						 field_data[field].list_type_index);
		if (addr)
			return g_strdup(addr->data);
		else
			return NULL;
	case E_CARD_SIMPLE_INTERNAL_TYPE_PHONE:
		phone = e_card_simple_get_phone(simple,
						field_data[field].list_type_index);
		if (phone)
			return g_strdup(phone->number);
		else
			return NULL;
	case E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL:
		string = e_card_simple_get_email(simple,
						 field_data[field].list_type_index);
		return g_strdup(string);
	case E_CARD_SIMPLE_INTERNAL_TYPE_BOOL:
		if (simple->card) {
			gboolean boole;
			g_object_get (simple->card,
				      field_data[field].ecard_field, &boole,
				      NULL);
			if (boole)
				return g_strdup("true");
			else
				return NULL;
		} else {
			return NULL;
		}
	case E_CARD_SIMPLE_INTERNAL_TYPE_SPECIAL:
		switch (field) {
		case E_CARD_SIMPLE_FIELD_NAME_OR_ORG:
			if (simple->card) {
				gboolean is_list;

				g_object_get(simple->card,
					     "file_as", &string,
					     NULL);
				if (string && *string)
					return string
;				else 
					g_free (string);

				g_object_get(simple->card,
					     "full_name", &string,
					     NULL);
				if (string && *string)
					return g_strdup(string);
				else 
					g_free (string);

				g_object_get(simple->card,
					     "org", &string,
					     NULL);
				if (string && *string)
					return g_strdup(string);
				else 
					g_free (string);

				is_list = e_card_evolution_list (simple->card);
				if (is_list)
					string = _("Unnamed List");
				else
					string = e_card_simple_get_email(simple,
									 E_CARD_SIMPLE_EMAIL_ID_EMAIL); 
				return g_strdup(string);
			} else
				return NULL;
		case E_CARD_SIMPLE_FIELD_FAMILY_NAME:
			if (simple->card) {
				g_object_get (simple->card,
					      "name", &name,
					      NULL);
				return g_strdup (name->family);
			} else
				return NULL;
		case E_CARD_SIMPLE_FIELD_GIVEN_NAME:
			if (simple->card) {
				g_object_get (simple->card,
					      "name", &name,
					      NULL);
				return g_strdup (name->given);
			} else
				return NULL;
		case E_CARD_SIMPLE_FIELD_ADDITIONAL_NAME:
			if (simple->card) {
				g_object_get (simple->card,
					      "name", &name,
					      NULL);
				return g_strdup (name->additional);
			} else
				return NULL;
		case E_CARD_SIMPLE_FIELD_NAME_SUFFIX:
			if (simple->card) {
				g_object_get (simple->card,
					      "name", &name,
					      NULL);
				return g_strdup (name->suffix);
			} else
				return NULL;
		default:
			return NULL;
		}
	default:
		return NULL;
	}
}

static char *
name_to_style(const ECardName *name, char *company, int style)
{
	char *string;
	char *strings[4], **stringptr;
	char *substring;
	switch (style) {
	case 0:
		stringptr = strings;
		if (name->family && *name->family)
			*(stringptr++) = name->family;
		if (name->given && *name->given)
			*(stringptr++) = name->given;
		*stringptr = NULL;
		string = g_strjoinv(", ", strings);
		break;
	case 1:
		stringptr = strings;
		if (name->given && *name->given)
			*(stringptr++) = name->given;
		if (name->family && *name->family)
			*(stringptr++) = name->family;
		*stringptr = NULL;
		string = g_strjoinv(" ", strings);
		break;
	case 2:
		string = g_strdup(company);
		break;
	case 3: /* Fall Through */
	case 4:
		stringptr = strings;
		if (name->family && *name->family)
			*(stringptr++) = name->family;
		if (name->given && *name->given)
			*(stringptr++) = name->given;
		*stringptr = NULL;
		substring = g_strjoinv(", ", strings);
		if (!(company && *company))
			company = "";
		if (style == 3)
			string = g_strdup_printf("%s (%s)", substring, company);
		else
			string = g_strdup_printf("%s (%s)", company, substring);
		g_free(substring);
		break;
	default:
		string = g_strdup("");
	}
	return string;
}

static int
file_as_get_style (ECardSimple *simple)
{
	char *filestring = e_card_simple_get(simple, E_CARD_SIMPLE_FIELD_FILE_AS);
	char *trystring;
	char *company = e_card_simple_get(simple, E_CARD_SIMPLE_FIELD_ORG);
	ECardName *name = NULL;
	int i;
	int style;
	style = 0;
	if (!company)
		company = g_strdup("");
	if (filestring) {
		g_object_get (simple->card,
			      "name", &name,
			      NULL);
		
		if (!name) {
			goto end;
		}

		style = -1;
		
		for (i = 0; i < 5; i++) {
			trystring = name_to_style(name, company, i);
			if (!strcmp(trystring, filestring)) {
				g_free(trystring);
				style = i;
				goto end;
			}
			g_free(trystring);
		}
	}		
 end:
		
	g_free(filestring);
	g_free(company);
	
	return style;
}

static void
file_as_set_style(ECardSimple *simple, int style)
{
	if (style != -1) {
		char *string;
		char *company = e_card_simple_get(simple, E_CARD_SIMPLE_FIELD_ORG);
		ECardName *name;

		if (!company)
			company = g_strdup("");
		g_object_get (simple->card,
			      "name", &name,
			      NULL);
		if (name) {
			string = name_to_style(name, company, style);
			e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_FILE_AS, string);
			g_free(string);
		}
		g_free(company);
	}
}

void            e_card_simple_set            (ECardSimple          *simple,
					      ECardSimpleField      field,
					      const char           *data)
{
	ECardSimpleInternalType type = field_data[field].type;
	ECardAddrLabel *address;
	ECardPhone *phone;
	int style;
	simple->changed = TRUE;
	switch (field) {
	case E_CARD_SIMPLE_FIELD_FULL_NAME:
	case E_CARD_SIMPLE_FIELD_ORG:
		style = file_as_get_style(simple);
		g_object_set(simple->card,
			     field_data[field].ecard_field, data,
			     NULL);
		file_as_set_style(simple, style);
		break;
	default:
		switch(type) {
		case E_CARD_SIMPLE_INTERNAL_TYPE_STRING:
			g_object_set(simple->card,
				     field_data[field].ecard_field, data,
				     NULL);
			break;
		case E_CARD_SIMPLE_INTERNAL_TYPE_DATE:
			break; /* FIXME!!!! */
		case E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS:
			address = e_card_address_label_new();
			address->data = g_strdup (data);
			e_card_simple_set_address(simple,
						  field_data[field].list_type_index,
						  address);
			e_card_address_label_unref(address);
			break;
		case E_CARD_SIMPLE_INTERNAL_TYPE_PHONE:
			phone = e_card_phone_new();
			phone->number = g_strdup (data);
			e_card_simple_set_phone(simple,
						field_data[field].list_type_index,
						phone);
			e_card_phone_unref(phone);
			break;
		case E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL:
			e_card_simple_set_email(simple,
						field_data[field].list_type_index,
						data);
			break;
		case E_CARD_SIMPLE_INTERNAL_TYPE_SPECIAL:
			break;
		case E_CARD_SIMPLE_INTERNAL_TYPE_BOOL:
			if (simple->card) {
				gboolean boole = TRUE;
				if (data == NULL)
					boole = FALSE;
				else if (!strcasecmp (data, "false"))
					boole = FALSE;
				g_object_set (simple->card,
					      field_data[field].ecard_field, boole,
					      NULL);
			}
			break;
		}
		break;
	}
}
					     
ECardSimpleType e_card_simple_type       (ECardSimple          *simple,
					  ECardSimpleField      field)
{
	ECardSimpleInternalType type = field_data[field].type;
	switch(type) {
	case E_CARD_SIMPLE_INTERNAL_TYPE_STRING:
	case E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS:
	case E_CARD_SIMPLE_INTERNAL_TYPE_PHONE:
	case E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL:
	default:
		return E_CARD_SIMPLE_TYPE_STRING;

	case E_CARD_SIMPLE_INTERNAL_TYPE_BOOL:
		return E_CARD_SIMPLE_TYPE_BOOL;

	case E_CARD_SIMPLE_INTERNAL_TYPE_DATE:
		return E_CARD_SIMPLE_TYPE_DATE;

	case E_CARD_SIMPLE_INTERNAL_TYPE_SPECIAL:
		return E_CARD_SIMPLE_TYPE_STRING;
	}
}

const char     *e_card_simple_get_ecard_field (ECardSimple         *simple,
					       ECardSimpleField     field)
{
	return field_data[field].ecard_field;
}

const char     *e_card_simple_get_name       (ECardSimple          *simple,
					      ECardSimpleField      field)
{
	return _(field_data[field].name);
}

gboolean
e_card_simple_get_allow_newlines (ECardSimple          *simple,
				  ECardSimpleField      field)
{
	ECardSimpleInternalType type = field_data[field].type;
	switch(type) {
	case E_CARD_SIMPLE_INTERNAL_TYPE_STRING:
	case E_CARD_SIMPLE_INTERNAL_TYPE_PHONE:
	case E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL:
	case E_CARD_SIMPLE_INTERNAL_TYPE_BOOL:
	case E_CARD_SIMPLE_INTERNAL_TYPE_DATE:
	case E_CARD_SIMPLE_INTERNAL_TYPE_SPECIAL:
	default:
		switch (field) {
		case E_CARD_SIMPLE_FIELD_NOTE:
			return TRUE;
		default:
			return FALSE;
		}

	case E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS:
		return TRUE;
	}
}

const char     *e_card_simple_get_short_name (ECardSimple          *simple,
					      ECardSimpleField      field)
{
	return _(field_data[field].short_name);
}

void                  e_card_simple_arbitrary_foreach (ECardSimple                  *simple,
						       ECardSimpleArbitraryCallback *callback,
						       gpointer                      closure)
{
	if (simple->card) {
		EList *list;
		EIterator *iterator;
		g_object_get(simple->card,
			     "arbitrary", &list,
			     NULL);
		for (iterator = e_list_get_iterator(list); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			const ECardArbitrary *arbitrary = e_iterator_get(iterator);
			if (callback)
				(*callback) (arbitrary, closure);
		}
		
		g_object_unref (list);
		e_card_free_empty_lists (simple->card);
	}
}

const ECardArbitrary *e_card_simple_get_arbitrary     (ECardSimple          *simple,
						       const char           *key)
{
	if (simple->card) {
		EList *list;
		EIterator *iterator;
		g_object_get(simple->card,
			     "arbitrary", &list,
			     NULL);
		for (iterator = e_list_get_iterator(list); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			const ECardArbitrary *arbitrary = e_iterator_get(iterator);
			if (!strcasecmp(arbitrary->key, key))
				return arbitrary;
		}

		g_object_unref (list);
		e_card_free_empty_lists (simple->card);
	}
	return NULL;
}

/* Any of these except key can be NULL */	      
void                  e_card_simple_set_arbitrary     (ECardSimple          *simple,
						       const char           *key,
						       const char           *type,
						       const char           *value)
{
	if (simple->card) {
		ECardArbitrary *new_arb;
		EList *list;
		EIterator *iterator;

		simple->changed = TRUE;
		g_object_get(simple->card,
			     "arbitrary", &list,
			     NULL);
		for (iterator = e_list_get_iterator(list); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			const ECardArbitrary *arbitrary = e_iterator_get(iterator);
			if (!strcasecmp(arbitrary->key, key)) {
				new_arb = e_card_arbitrary_new();
				new_arb->key = g_strdup(key);
				new_arb->type = g_strdup(type);
				new_arb->value = g_strdup(value);
				e_iterator_set(iterator, new_arb);
				e_card_arbitrary_unref(new_arb);
				return;
			}
		}
		new_arb = e_card_arbitrary_new();
		new_arb->key = g_strdup(key);
		new_arb->type = g_strdup(type);
		new_arb->value = g_strdup(value);
		e_list_append(list, new_arb);
		g_object_unref(list);
		e_card_arbitrary_unref(new_arb);
	}
}

void
e_card_simple_set_name (ECardSimple *simple, ECardName *name)
{
	int style;
	style = file_as_get_style(simple);
	g_object_set (simple->card,
		      "name", name,
		      NULL);
	file_as_set_style(simple, style);
}

/* These map between the individual list types and ECardSimpleField */
ECardSimpleField
e_card_simple_map_phone_to_field (ECardSimplePhoneId phone_id)
{
	int i;

	g_return_val_if_fail (phone_id < E_CARD_SIMPLE_PHONE_ID_LAST, 0);

	for (i = 0; i < field_data_count; i ++)
		if (field_data[i].list_type_index == phone_id
		    && field_data[i].type == E_CARD_SIMPLE_INTERNAL_TYPE_PHONE)
			return i;

	g_warning ("couldn't find phone id %d, returning 0 (which is almost assuredly incorrect)\n", phone_id);

	return 0;
}

ECardSimpleField
e_card_simple_map_email_to_field (ECardSimpleEmailId email_id)
{
	int i;

	g_return_val_if_fail (email_id < E_CARD_SIMPLE_EMAIL_ID_LAST, 0);

	for (i = 0; i < field_data_count; i ++)
		if (field_data[i].list_type_index == email_id
		    && field_data[i].type == E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL)
			return i;

	g_warning ("couldn't find email id %d, returning 0 (which is almost assuredly incorrect)\n", email_id);
	return 0;
}

ECardSimpleField
e_card_simple_map_address_to_field (ECardSimpleAddressId address_id)
{
	int i;

	g_return_val_if_fail (address_id < E_CARD_SIMPLE_ADDRESS_ID_LAST, 0);

	for (i = 0; i < field_data_count; i ++)
		if (field_data[i].list_type_index == address_id
		    && field_data[i].type == E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS)
			return i;

	g_warning ("couldn't find address id %d, returning 0 (which is almost assuredly incorrect)\n", address_id);
	return 0;
}
