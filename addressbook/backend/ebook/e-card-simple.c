/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Chris Lahey     <clahey@helixcode.com>
 *   Arturo Espinosa (arturo@nuclecu.unam.mx)
 *   Nat Friedman    (nat@helixcode.com)
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include <libversit/vcc.h>
#include "e-card-simple.h"

/* Object argument IDs */
enum {
	ARG_0,
	ARG_CARD,
#if 0
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
	ARG_FBURL,
	ARG_NOTE,
	ARG_ID
#endif
};


typedef enum _ECardSimpleInternalType ECardSimpleInternalType;
typedef struct _ECardSimpleFieldData ECardSimpleFieldData;

enum _ECardSimpleInternalType {
	E_CARD_SIMPLE_INTERNAL_TYPE_STRING,
	E_CARD_SIMPLE_INTERNAL_TYPE_DATE,
	E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS,
	E_CARD_SIMPLE_INTERNAL_TYPE_PHONE,
	E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL,
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

static ECardSimpleFieldData field_data[] =
{
	{ E_CARD_SIMPLE_FIELD_FILE_AS,            "file_as",     "File As",       "",         0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_FULL_NAME,          "full_name",   "Name",          "Name",     0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_EMAIL,              "",            "Email",         "Email",    E_CARD_SIMPLE_EMAIL_ID_EMAIL,   	   E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL },
	{ E_CARD_SIMPLE_FIELD_PHONE_PRIMARY,      "",            "Primary",       "Prim",     E_CARD_SIMPLE_PHONE_ID_PRIMARY,      E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_BUSINESS,     "",            "Business",      "Bus",      E_CARD_SIMPLE_PHONE_ID_BUSINESS,     E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_HOME,         "",            "Home",          "Home",     E_CARD_SIMPLE_PHONE_ID_HOME,         E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_ORG,                "org",         "Organization",  "Org",      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_ADDRESS_BUSINESS,   "",            "Business",      "Bus",      E_CARD_SIMPLE_ADDRESS_ID_BUSINESS,   E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS },
	{ E_CARD_SIMPLE_FIELD_ADDRESS_HOME,       "",            "Home",          "Home",     E_CARD_SIMPLE_ADDRESS_ID_HOME,       E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS },
	{ E_CARD_SIMPLE_FIELD_PHONE_MOBILE,       "",            "Mobile",        "Mobile",   E_CARD_SIMPLE_PHONE_ID_MOBILE,       E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_CAR,          "",            "Car",           "Car",      E_CARD_SIMPLE_PHONE_ID_CAR,          E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_FAX, "",            "Business Fax",  "Bus Fax",  E_CARD_SIMPLE_PHONE_ID_BUSINESS_FAX, E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_HOME_FAX,     "",            "Home Fax",      "Home Fax", E_CARD_SIMPLE_PHONE_ID_HOME_FAX,     E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_2,   "",            "Business 2",    "Bus 2",    E_CARD_SIMPLE_PHONE_ID_BUSINESS_2,   E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_HOME_2,       "",            "Home 2",        "Home 2",   E_CARD_SIMPLE_PHONE_ID_HOME_2,       E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_ISDN,         "",            "ISDN",          "ISDN",     E_CARD_SIMPLE_PHONE_ID_ISDN,         E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_OTHER,        "",            "Other",         "Other",    E_CARD_SIMPLE_PHONE_ID_OTHER,        E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_PHONE_PAGER,        "",            "Pager",         "Pager",    E_CARD_SIMPLE_PHONE_ID_PAGER,        E_CARD_SIMPLE_INTERNAL_TYPE_PHONE },
	{ E_CARD_SIMPLE_FIELD_ADDRESS_OTHER,      "",            "Other",         "Other",    E_CARD_SIMPLE_ADDRESS_ID_OTHER,      E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS },
	{ E_CARD_SIMPLE_FIELD_EMAIL_2,            "",            "Email 2",       "Email 2",  E_CARD_SIMPLE_EMAIL_ID_EMAIL_2, 	   E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL },
	{ E_CARD_SIMPLE_FIELD_EMAIL_3,            "",            "Email 3",       "Email 3",  E_CARD_SIMPLE_EMAIL_ID_EMAIL_3, 	   E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL },
	{ E_CARD_SIMPLE_FIELD_URL,                "url",         "Web Site",      "Url",      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_ORG_UNIT,           "org_unit",    "Department",    "Dep",      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_OFFICE,             "office",      "Office",        "Off",      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_TITLE,              "title",       "Title",         "Title",    0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_ROLE,               "role",        "Profession",    "Prof",     0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_MANAGER,            "manager",     "Manager",       "Man",      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_ASSISTANT,          "assistant",   "Assistant",     "Ass",      0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_NICKNAME,           "nickname",    "Nickname",      "Nick",     0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_SPOUSE,             "spouse",      "Spouse",        "Spouse",   0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_NOTE,               "note",        "Note",          "Note",     0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_FBURL,              "fburl",       "Free-busy URL", "FBUrl",    0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_STRING },
	{ E_CARD_SIMPLE_FIELD_ANNIVERSARY,        "anniversary", "Anniversary",   "Anniv",    0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_DATE },
	{ E_CARD_SIMPLE_FIELD_BIRTH_DATE,         "birth_date",  "Birth Date",    "",         0,                                   E_CARD_SIMPLE_INTERNAL_TYPE_DATE },
};

static void e_card_simple_init (ECardSimple *simple);
static void e_card_simple_class_init (ECardSimpleClass *klass);

static void e_card_simple_destroy (GtkObject *object);
static void e_card_simple_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_card_simple_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static void fill_in_info(ECardSimple *simple);

ECardPhoneFlags phone_correspondences[] = {
	0xFF, /* E_CARD_SIMPLE_PHONE_ID_ASSISTANT,    */
	E_CARD_PHONE_WORK | E_CARD_PHONE_VOICE, /* E_CARD_SIMPLE_PHONE_ID_BUSINESS,	   */
	E_CARD_PHONE_WORK | E_CARD_PHONE_VOICE, /* E_CARD_SIMPLE_PHONE_ID_BUSINESS_2,   */
	E_CARD_PHONE_WORK | E_CARD_PHONE_FAX, /* E_CARD_SIMPLE_PHONE_ID_BUSINESS_FAX, */
	0xFF, /* E_CARD_SIMPLE_PHONE_ID_CALLBACK,	   */
	E_CARD_PHONE_CAR | E_CARD_PHONE_VOICE, /* E_CARD_SIMPLE_PHONE_ID_CAR,	   */
	0xFF, /* E_CARD_SIMPLE_PHONE_ID_COMPANY,	   */
	E_CARD_PHONE_HOME | E_CARD_PHONE_VOICE, /* E_CARD_SIMPLE_PHONE_ID_HOME,	   */
	E_CARD_PHONE_HOME | E_CARD_PHONE_VOICE, /* E_CARD_SIMPLE_PHONE_ID_HOME_2,	   */
	E_CARD_PHONE_HOME | E_CARD_PHONE_FAX, /* E_CARD_SIMPLE_PHONE_ID_HOME_FAX,	   */
	E_CARD_PHONE_ISDN, /* E_CARD_SIMPLE_PHONE_ID_ISDN,	   */
	E_CARD_PHONE_CELL | E_CARD_PHONE_VOICE, /* E_CARD_SIMPLE_PHONE_ID_MOBILE,	   */
	E_CARD_PHONE_VOICE, /* E_CARD_SIMPLE_PHONE_ID_OTHER,	   */
	0xFF, /* E_CARD_SIMPLE_PHONE_ID_OTHER_FAX,	   */
	E_CARD_PHONE_PAGER | E_CARD_PHONE_VOICE, /* E_CARD_SIMPLE_PHONE_ID_PAGER,	   */
	E_CARD_PHONE_PREF, /* E_CARD_SIMPLE_PHONE_ID_PRIMARY,	   */
	0xFF, /* E_CARD_SIMPLE_PHONE_ID_RADIO,	   */
	0xFF, /* E_CARD_SIMPLE_PHONE_ID_TELEX,	   */
	0xFF, /* E_CARD_SIMPLE_PHONE_ID_TTYTTD,	   */
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
	NULL, /* E_CARD_SIMPLE_PHONE_ID_TTYTTD,	   */
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
	NULL, /* E_CARD_SIMPLE_PHONE_ID_TTYTTD,	   */
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
GtkType
e_card_simple_get_type (void)
{
	static GtkType simple_type = 0;

	if (!simple_type) {
		GtkTypeInfo simple_info = {
			"ECardSimple",
			sizeof (ECardSimple),
			sizeof (ECardSimpleClass),
			(GtkClassInitFunc) e_card_simple_class_init,
			(GtkObjectInitFunc) e_card_simple_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		simple_type = gtk_type_unique (gtk_object_get_type (), &simple_info);
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
	ECardSimple *simple = E_CARD_SIMPLE(gtk_type_new(e_card_simple_get_type()));
	gtk_object_set(GTK_OBJECT(simple),
		       "card", card,
		       NULL);
	return simple;
}

ECardSimple *e_card_simple_duplicate(ECardSimple *simple)
{
	char *vcard = e_card_simple_get_vcard(simple);
	ECard *card = e_card_new(vcard);
	ECardSimple *new_simple = e_card_simple_new(card);
	g_free (vcard);
	return new_simple;
}

/**
 * e_card_simple_get_id:
 * @simple: an #ECardSimple
 *
 * Returns: a string representing the id of the simple, which is unique
 * within its book.
 */
char *
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
char
*e_card_simple_get_vcard (ECardSimple *simple)
{
	if (simple->card)
		return e_card_get_vcard(simple->card);
	else
		return g_strdup("");
}

static void
e_card_simple_class_init (ECardSimpleClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS(klass);

	gtk_object_add_arg_type ("ECardSimple::card",
				 GTK_TYPE_OBJECT, GTK_ARG_READWRITE, ARG_CARD);
#if 0
	gtk_object_add_arg_type ("ECardSimple::file_as",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_FILE_AS);
	gtk_object_add_arg_type ("ECardSimple::full_name",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_FULL_NAME);  
	gtk_object_add_arg_type ("ECardSimple::name",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_NAME);
	gtk_object_add_arg_type ("ECardSimple::address",
				 GTK_TYPE_OBJECT, GTK_ARG_READABLE, ARG_ADDRESS);
	gtk_object_add_arg_type ("ECardSimple::address_label",
				 GTK_TYPE_OBJECT, GTK_ARG_READABLE, ARG_ADDRESS_LABEL);
	gtk_object_add_arg_type ("ECardSimple::phone",
				 GTK_TYPE_OBJECT, GTK_ARG_READABLE, ARG_PHONE);
	gtk_object_add_arg_type ("ECardSimple::email",
				 GTK_TYPE_OBJECT, GTK_ARG_READABLE, ARG_EMAIL);
	gtk_object_add_arg_type ("ECardSimple::birth_date",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_BIRTH_DATE);
	gtk_object_add_arg_type ("ECardSimple::url",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_URL);  
	gtk_object_add_arg_type ("ECardSimple::org",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ORG);
	gtk_object_add_arg_type ("ECardSimple::org_unit",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ORG_UNIT);
	gtk_object_add_arg_type ("ECardSimple::office",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_OFFICE);
	gtk_object_add_arg_type ("ECardSimple::title",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_TITLE);  
	gtk_object_add_arg_type ("ECardSimple::role",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ROLE);
	gtk_object_add_arg_type ("ECardSimple::manager",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_MANAGER);
	gtk_object_add_arg_type ("ECardSimple::assistant",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ASSISTANT);
	gtk_object_add_arg_type ("ECardSimple::nickname",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_NICKNAME);
	gtk_object_add_arg_type ("ECardSimple::spouse",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_SPOUSE);
	gtk_object_add_arg_type ("ECardSimple::anniversary",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_ANNIVERSARY);
	gtk_object_add_arg_type ("ECardSimple::fburl",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_FBURL);
	gtk_object_add_arg_type ("ECardSimple::note",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_NOTE);
	gtk_object_add_arg_type ("ECardSimple::id",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ID);
#endif

	object_class->destroy = e_card_simple_destroy;
	object_class->get_arg = e_card_simple_get_arg;
	object_class->set_arg = e_card_simple_set_arg;
}

/*
 * ECardSimple lifecycle management and vcard loading/saving.
 */

static void
e_card_simple_destroy (GtkObject *object)
{
	ECardSimple *simple;
	
	simple = E_CARD_SIMPLE (object);

	if (simple->card)
		gtk_object_unref(GTK_OBJECT(simple->card));
	g_list_foreach(simple->temp_fields, (GFunc) g_free, NULL);
	g_list_free(simple->temp_fields);
	simple->temp_fields = NULL;
}


/* Set_arg handler for the simple */
static void
e_card_simple_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ECardSimple *simple;
	
	simple = E_CARD_SIMPLE (object);

	switch (arg_id) {
	case ARG_CARD:
		if (simple->card)
			gtk_object_unref(GTK_OBJECT(simple->card));
		g_list_foreach(simple->temp_fields, (GFunc) g_free, NULL);
		g_list_free(simple->temp_fields);
		simple->temp_fields = NULL;
		if (GTK_VALUE_OBJECT(*arg))
			simple->card = E_CARD(GTK_VALUE_OBJECT(*arg));
		else
			simple->card = NULL;
		if(simple->card)
			gtk_object_ref(GTK_OBJECT(simple->card));
		fill_in_info(simple);
		break;
#if 0
	case ARG_FILE_AS:
		if (simple->file_as)
			g_free(simple->file_as);
		simple->file_as = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_FULL_NAME:
		if ( simple->fname )
			g_free(simple->fname);
		simple->fname = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_NAME:
		if ( simple->name )
			e_card_simple_name_free(simple->name);
		simple->name = GTK_VALUE_POINTER(*arg);
		break;
	case ARG_BIRTH_DATE:
		if ( simple->bday )
			g_free(simple->bday);
		simple->bday = GTK_VALUE_POINTER(*arg);
		break;
	case ARG_URL:
		if ( simple->url )
			g_free(simple->url);
		simple->url = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ORG:
		if (simple->org)
			g_free(simple->org);
		simple->org = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ORG_UNIT:
		if (simple->org_unit)
			g_free(simple->org_unit);
		simple->org_unit = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_OFFICE:
		if (simple->office)
			g_free(simple->office);
		simple->office = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_TITLE:
		if ( simple->title )
			g_free(simple->title);
		simple->title = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ROLE:
		if (simple->role)
			g_free(simple->role);
		simple->role = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_MANAGER:
		if (simple->manager)
			g_free(simple->manager);
		simple->manager = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ASSISTANT:
		if (simple->assistant)
			g_free(simple->assistant);
		simple->assistant = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_NICKNAME:
		if (simple->nickname)
			g_free(simple->nickname);
		simple->nickname = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_SPOUSE:
		if (simple->spouse)
			g_free(simple->spouse);
		simple->spouse = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ANNIVERSARY:
		if ( simple->anniversary )
			g_free(simple->anniversary);
		simple->anniversary = GTK_VALUE_POINTER(*arg);
		break;
	case ARG_FBURL:
		if (simple->fburl)
			g_free(simple->fburl);
		simple->fburl = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_NOTE:
		if (simple->note)
			g_free (simple->note);
		simple->note = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_ID:
		if (simple->id)
			g_free(simple->id);
		simple->id = g_strdup(GTK_VALUE_STRING(*arg));
		break;
#endif
	default:
		return;
	}
}

/* Get_arg handler for the simple */
static void
e_card_simple_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ECardSimple *simple;

	simple = E_CARD_SIMPLE (object);

	switch (arg_id) {
	case ARG_CARD:
		e_card_simple_sync_card(simple);
		if (simple->card)
			GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(simple->card);
		else
			GTK_VALUE_OBJECT (*arg) = NULL;
		break;
#if 0
	case ARG_FILE_AS:
		GTK_VALUE_STRING (*arg);
		break;
	case ARG_FULL_NAME:
		GTK_VALUE_STRING (*arg) = simple->fname;
		break;
	case ARG_NAME:
		GTK_VALUE_POINTER(*arg) = simple->name;
		break;
	case ARG_ADDRESS:
		if (!simple->address)
			simple->address = e_card_simple_list_new((ECardSimpleListCopyFunc) e_card_simple_delivery_address_copy, 
							(ECardSimpleListFreeFunc) e_card_simple_delivery_address_free,
							NULL);
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(simple->address);
		break;
	case ARG_ADDRESS_LABEL:
		if (!simple->address_label)
			simple->address_label = e_card_simple_list_new((ECardSimpleListCopyFunc) e_card_simple_address_label_copy, 
							      (ECardSimpleListFreeFunc) e_card_simple_address_label_free,
							      NULL);
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(simple->address_label);
		break;
	case ARG_PHONE:
		if (!simple->phone)
			simple->phone = e_card_simple_list_new((ECardSimpleListCopyFunc) e_card_simple_phone_copy, 
						      (ECardSimpleListFreeFunc) e_card_simple_phone_free,
						      NULL);
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(simple->phone);
		break;
	case ARG_EMAIL:
		if (!simple->email)
			simple->email = e_card_simple_list_new((ECardSimpleListCopyFunc) g_strdup, 
						      (ECardSimpleListFreeFunc) g_free,
						      NULL);
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(simple->email);
		break;
	case ARG_BIRTH_DATE:
		GTK_VALUE_POINTER(*arg) = simple->bday;
		break;
	case ARG_URL:
		GTK_VALUE_STRING(*arg) = simple->url;
		break;
	case ARG_ORG:
		GTK_VALUE_STRING(*arg) = simple->org;
		break;
	case ARG_ORG_UNIT:
		GTK_VALUE_STRING(*arg) = simple->org_unit;
		break;
	case ARG_OFFICE:
		GTK_VALUE_STRING(*arg) = simple->office;
		break;
	case ARG_TITLE:
		GTK_VALUE_STRING(*arg) = simple->title;
		break;
	case ARG_ROLE:
		GTK_VALUE_STRING(*arg) = simple->role;
		break;
	case ARG_MANAGER:
		GTK_VALUE_STRING(*arg) = simple->manager;
		break;
	case ARG_ASSISTANT:
		GTK_VALUE_STRING(*arg) = simple->assistant;
		break;
	case ARG_NICKNAME:
		GTK_VALUE_STRING(*arg) = simple->nickname;
		break;
	case ARG_SPOUSE:
		GTK_VALUE_STRING(*arg) = simple->spouse;
		break;
	case ARG_ANNIVERSARY:
		GTK_VALUE_POINTER(*arg) = simple->anniversary;
		break;
	case ARG_FBURL:
		GTK_VALUE_STRING(*arg) = simple->fburl;
		break;
	case ARG_NOTE:
		GTK_VALUE_STRING(*arg) = simple->note;
		break;
	case ARG_ID:
		GTK_VALUE_STRING(*arg) = simple->id;
		break;
#endif
	default:
		arg->type = GTK_TYPE_INVALID;
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
}

static void
fill_in_info(ECardSimple *simple)
{
	ECard *card = simple->card;
	if (card) {
		ECardList *address_list;
		ECardList *phone_list;
		ECardList *email_list;
		const ECardPhone *phone;
		const char *email;
		const ECardAddrLabel *address;
		int i;

		ECardIterator *iterator;

		gtk_object_get(GTK_OBJECT(card),
			       "address_label", &address_list,
			       "phone",         &phone_list,
			       "email",         &email_list,
			       NULL);
		for (i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i++) {
			e_card_phone_free(simple->phone[i]);
			simple->phone[i] = NULL;
		}
		for (iterator = e_card_list_get_iterator(phone_list); e_card_iterator_is_valid(iterator); e_card_iterator_next(iterator)) {
			phone = e_card_iterator_get(iterator);
			for (i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i ++) {
				if (((phone->flags & phone_correspondences[i]) == phone_correspondences[i]) && (simple->phone[i] == NULL)) {
					simple->phone[i] = e_card_phone_copy(phone);
					break;
				}
			}
		}
		gtk_object_unref(GTK_OBJECT(iterator));

		for (i = 0; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i++) {
			g_free(simple->email[i]);
			simple->email[i] = NULL;
		}
		for (iterator = e_card_list_get_iterator(email_list); e_card_iterator_is_valid(iterator); e_card_iterator_next(iterator)) {
			email = e_card_iterator_get(iterator);
			for (i = 0; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i ++) {
				if ((simple->email[i] == NULL)) {
					simple->email[i] = g_strdup(email);
					break;
				}
			}
		}
		gtk_object_unref(GTK_OBJECT(iterator));

		for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i++) {
			e_card_address_label_free(simple->address[i]);
			simple->address[i] = NULL;
		}
		for (iterator = e_card_list_get_iterator(address_list); e_card_iterator_is_valid(iterator); e_card_iterator_next(iterator)) {
			address = e_card_iterator_get(iterator);
			for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i ++) {
				if (((address->flags & addr_correspondences[i]) == addr_correspondences[i]) && (simple->address[i] == NULL)) {
					simple->address[i] = e_card_address_label_copy(address);
					break;
				}
			}
		}
		gtk_object_unref(GTK_OBJECT(iterator));
	}
}

void
e_card_simple_sync_card(ECardSimple *simple)
{
	ECard *card = simple->card;
	if (card) {
		ECardList *address_list;
		ECardList *phone_list;
		ECardList *email_list;
		const ECardPhone *phone;
		const ECardAddrLabel *address;
		const char *email;
		int i;
		int iterator_next = 1;

		ECardIterator *iterator;

		gtk_object_get(GTK_OBJECT(card),
			       "address_label", &address_list,
			       "phone",         &phone_list,
			       "email",         &email_list,
			       NULL);

		for (iterator = e_card_list_get_iterator(phone_list); e_card_iterator_is_valid(iterator); iterator_next ? e_card_iterator_next(iterator) : FALSE ) {
			int i;
			phone = e_card_iterator_get(iterator);
			iterator_next = 1;
			for (i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i ++) {
				if ((phone->flags & phone_correspondences[i]) == phone_correspondences[i]) {
					if (simple->phone[i]) {
						simple->phone[i]->flags = phone_correspondences[i];
						if (simple->phone[i]->number && *simple->phone[i]->number) {
							e_card_iterator_set(iterator, simple->phone[i]);
						} else {
							e_card_iterator_delete(iterator);
							iterator_next = 0;
						}
						e_card_phone_free(simple->phone[i]);
						simple->phone[i] = NULL;
						break;
					}
				}
			}
		}	
		gtk_object_unref(GTK_OBJECT(iterator));
		for (i = 0; i < E_CARD_SIMPLE_PHONE_ID_LAST; i ++) {
			if (simple->phone[i]) {
				simple->phone[i]->flags = phone_correspondences[i];
				e_card_list_append(phone_list, simple->phone[i]);
				e_card_phone_free(simple->phone[i]);
				simple->phone[i] = NULL;
			}
		}

		for (iterator = e_card_list_get_iterator(email_list); e_card_iterator_is_valid(iterator); iterator_next ? e_card_iterator_next(iterator) : FALSE ) {
			int i;
			email = e_card_iterator_get(iterator);
			iterator_next = 1;
			for (i = 0; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i ++) {
				if (simple->email[i]) {
					if (*simple->email[i]) {
						e_card_iterator_set(iterator, simple->email[i]);
					} else {
						e_card_iterator_delete(iterator);
						iterator_next = 0;
					}
					g_free(simple->email[i]);
					simple->email[i] = NULL;
					break;
				}
			}
		}	
		gtk_object_unref(GTK_OBJECT(iterator));
		for (i = 0; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i ++) {
			if (simple->email[i]) {
				e_card_list_append(email_list, simple->email[i]);
				g_free(simple->email[i]);
				simple->email[i] = NULL;
			}
		}

		for (iterator = e_card_list_get_iterator(address_list); e_card_iterator_is_valid(iterator); iterator_next ? e_card_iterator_next(iterator) : FALSE ) {
			int i;
			address = e_card_iterator_get(iterator);
			iterator_next = 1;
			for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i ++) {
				if ((address->flags & addr_correspondences[i]) == addr_correspondences[i]) {
					if (simple->address[i]) {
						simple->address[i]->flags = addr_correspondences[i];
						if (simple->address[i]->data && *simple->address[i]->data) {
							e_card_iterator_set(iterator, simple->address[i]);
						} else {
							e_card_iterator_delete(iterator);
							iterator_next = 0;
						}
						e_card_address_label_free(simple->address[i]);
						simple->address[i] = NULL;
						break;
					}
				}
			}
		}	
		gtk_object_unref(GTK_OBJECT(iterator));
		for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i ++) {
			if (simple->address[i]) {
				simple->address[i]->flags = addr_correspondences[i];
				e_card_list_append(address_list, simple->address[i]);
				e_card_address_label_free(simple->address[i]);
				simple->address[i] = NULL;
			}
		}
		fill_in_info(simple);
	}
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

void            e_card_simple_set_phone   (ECardSimple          *simple,
					   ECardSimplePhoneId    id,
					   const ECardPhone           *phone)
{
	if (simple->phone[id])
		e_card_phone_free(simple->phone[id]);
	simple->phone[id] = e_card_phone_copy(phone);
}

void            e_card_simple_set_email   (ECardSimple          *simple,
					   ECardSimpleEmailId    id,
					   const char                 *email)
{
	if (simple->email[id])
		g_free(simple->email[id]);
	simple->email[id] = g_strdup(email);
}

void            e_card_simple_set_address (ECardSimple          *simple,
					   ECardSimpleAddressId  id,
					   const ECardAddrLabel       *address)
{
	if (simple->address[id])
		e_card_address_label_free(simple->address[id]);
	simple->address[id] = e_card_address_label_copy(address);
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
	const char *string;
	ECardDate *date;
	switch(type) {
	case E_CARD_SIMPLE_INTERNAL_TYPE_STRING:
		gtk_object_get(GTK_OBJECT(simple->card),
			       field_data[field].ecard_field, &string,
			       NULL);
		return g_strdup(string);
	case E_CARD_SIMPLE_INTERNAL_TYPE_DATE:
		gtk_object_get(GTK_OBJECT(simple->card),
			       field_data[field].ecard_field, &date,
			       NULL);
		return NULL; /* FIXME!!!! */
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
	default:
		return NULL;
	}
}

void            e_card_simple_set            (ECardSimple          *simple,
					      ECardSimpleField      field,
					      const char           *data)
{
	ECardSimpleInternalType type = field_data[field].type;
	ECardAddrLabel *address;
	ECardPhone *phone;
	switch(type) {
	case E_CARD_SIMPLE_INTERNAL_TYPE_STRING:
		gtk_object_set(GTK_OBJECT(simple->card),
			       field_data[field].ecard_field, data,
			       NULL);
		break;
	case E_CARD_SIMPLE_INTERNAL_TYPE_DATE:
		break; /* FIXME!!!! */
	case E_CARD_SIMPLE_INTERNAL_TYPE_ADDRESS:
		address = e_card_address_label_new();
		address->data = (char *) data;
		e_card_simple_set_address(simple,
					  field_data[field].list_type_index,
					  address);
		address->data = NULL;
		e_card_address_label_free(address);
		break;
	case E_CARD_SIMPLE_INTERNAL_TYPE_PHONE:
		phone = e_card_phone_new();
		phone->number = (char *) data;
		e_card_simple_set_phone(simple,
					field_data[field].list_type_index,
					phone);
		phone->number = NULL;
		e_card_phone_free(phone);
		break;
	case E_CARD_SIMPLE_INTERNAL_TYPE_EMAIL:
		e_card_simple_set_email(simple,
					field_data[field].list_type_index,
					data);
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
	case E_CARD_SIMPLE_INTERNAL_TYPE_DATE:
		return E_CARD_SIMPLE_TYPE_DATE;
	}
}

const char     *e_card_simple_get_name       (ECardSimple          *simple,
					      ECardSimpleField      field)
{
	return field_data[field].name;
}

const char     *e_card_simple_get_short_name (ECardSimple          *simple,
					      ECardSimpleField      field)
{
	return field_data[field].short_name;
}
