/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
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

#include "libversit/vcc.h"
#include <e-card.h>
#include <e-card-pairs.h>

#define is_a_prop_of(obj,prop) (isAPropertyOf ((obj),(prop)))
#define str_val(obj) (the_str = (vObjectValueType (obj))? fakeCString (vObjectUStringZValue (obj)) : calloc (1, 1))
#define has(obj,prop) (vo = isAPropertyOf ((obj), (prop)))

/* Object argument IDs */
enum {
	ARG_0,
	ARG_FULL_NAME,
	ARG_NAME,
	ARG_ADDRESS,
	ARG_PHONE,
	ARG_EMAIL,
	ARG_BIRTH_DATE
};

#if 0
static VObject *card_convert_to_vobject (ECard *crd);
#endif
static void parse(ECard *card, VObject *vobj);
static void e_card_init (ECard *card);
static void e_card_class_init (ECardClass *klass);

static void e_card_destroy (GtkObject *object);
static void e_card_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_card_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static void assign_string(VObject *vobj, char **string);

static void e_card_name_free(ECardName *name);
char *e_v_object_get_child_value(VObject *vobj, char *name);
static ECardDate e_card_date_from_string (char *str);

static void parse_bday(ECard *card, VObject *object);
static void parse_full_name(ECard *card, VObject *object);
static void parse_name(ECard *card, VObject *object);
static void parse_email(ECard *card, VObject *object);
static void parse_phone(ECard *card, VObject *object);
static void parse_address(ECard *card, VObject *object);

static ECardPhoneFlags get_phone_flags (VObject *vobj);
static void set_phone_flags (VObject *vobj, ECardPhoneFlags flags);
static ECardAddressFlags get_address_flags (VObject *vobj);
static void set_address_flags (VObject *vobj, ECardAddressFlags flags);

typedef void (* ParsePropertyFunc) (ECard *card, VObject *object);

struct {
	char *key;
	ParsePropertyFunc function;
} attribute_jump_array[] = 
{
	{ VCFullNameProp,     parse_full_name },
	{ VCNameProp,         parse_name },
	{ VCBirthDateProp,    parse_bday },
	{ VCEmailAddressProp, parse_email },
	{ VCTelephoneProp,    parse_phone },
	{ VCAdrProp,          parse_address }
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

/**
 * e_card_new:
 * @vcard: a string in vCard format
 *
 * Returns: a new #ECard that wraps the @vcard.
 */
ECard
*e_card_new (char *vcard)
{
	ECard *card = E_CARD(gtk_type_new(e_card_get_type()));
	VObject *vobj = Parse_MIME(vcard, strlen(vcard));
	while(vobj) {
		VObject *next;
		parse(card, vobj);
		next = nextVObjectInList(vobj);
		cleanVObject(vobj);
		vobj = next;
	}
	return card;
}

/**
 * e_card_get_id:
 * @card: an #ECard
 *
 * Returns: a string representing the id of the card, which is unique
 * within its book.
 */
char *
e_card_get_id (ECard *card)
{
	return card->id;
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
	if ( card->id )
		g_free(card->id);
	card->id = g_strdup(id);
}

/**
 * e_card_get_vcard:
 * @card: an #ECard
 *
 * Returns: a string in vCard format, which is wrapped by the @card.
 */
char
*e_card_get_vcard (ECard *card)
{
	VObject *vobj; /*, *vprop; */
	char *temp, *ret_val;
	
	vobj = newVObject (VCCardProp);

	if ( card->fname )
		addPropValue(vobj, VCFullNameProp, card->fname);

	if ( card->name ) {
		VObject *nameprop;
		nameprop = addProp(vobj, VCNameProp);
		if ( card->name->prefix )
			addPropValue(nameprop, VCNamePrefixesProp, card->name->prefix);
		if ( card->name->given )
			addPropValue(nameprop, VCGivenNameProp, card->name->given);
		if ( card->name->additional )
			addPropValue(nameprop, VCAdditionalNamesProp, card->name->additional);
		if ( card->name->family )
			addPropValue(nameprop, VCFamilyNameProp, card->name->family);
		if ( card->name->suffix )
			addPropValue(nameprop, VCNameSuffixesProp, card->name->suffix);
	}


	if ( card->address ) {
		GList *list = card->address;
		for ( ; list; list = list->next ) {
			VObject *addressprop;
			ECardDeliveryAddress *address = (ECardDeliveryAddress *) list->data;
			addressprop = addProp(vobj, VCAdrProp);
			
			set_address_flags (addressprop, address->flags);
			if ( address->po )
				addPropValue(addressprop, VCPostalBoxProp, address->po);
			if ( address->ext )
				addPropValue(addressprop, VCExtAddressProp, address->ext);
			if ( address->street )
				addPropValue(addressprop, VCStreetAddressProp, address->street);
			if ( address->city )
				addPropValue(addressprop, VCCityProp, address->city);
			if ( address->region )
				addPropValue(addressprop, VCRegionProp, address->region);
			if ( address->code )
				addPropValue(addressprop, VCPostalCodeProp, address->code);
			if ( address->country )
				addPropValue(addressprop, VCCountryNameProp, address->country);
		}
	}

	if ( card->phone ) { 
		GList *list = card->phone;
		for ( ; list; list = list->next ) {
			VObject *phoneprop;
			ECardPhone *phone = (ECardPhone *) list->data;
			phoneprop = addPropValue(vobj, VCTelephoneProp, phone->number);
			
			set_phone_flags (phoneprop, phone->flags);
		}
	}

	if ( card->email ) { 
		GList *list = card->email;
		for ( ; list; list = list->next ) {
			VObject *emailprop;
			emailprop = addPropValue(vobj, VCEmailAddressProp, (char *) list->data);
			addProp (emailprop, VCInternetProp);
		}
	}

	if ( card->bday ) {
		ECardDate date;
		char *value;
		date = *card->bday;
		date.year = MIN(date.year, 9999);
		date.month = MIN(date.month, 12);
		date.day = MIN(date.day, 31);
		value = g_strdup_printf("%04d-%02d-%02d", date.year, date.month, date.day);
		addPropValue(vobj, VCBirthDateProp, value);
		g_free(value);
	}
	
#if 0
	
	
	if (crd->photo.prop.used) {
		vprop = addPropSizedValue (vobj, VCPhotoProp, 
					  crd->photo.data, crd->photo.size);
		add_PhotoType (vprop, crd->photo.type);
		add_CardProperty (vprop, &crd->photo.prop);
	}

	if (crd->xtension.l) {
		GList *node;
		
		for (node = crd->xtension.l; node; node = node->next) {
			CardXProperty *xp = (CardXProperty *) node->data;
			addPropValue (vobj, xp->name, xp->data);
			add_CardProperty (vobj, &xp->prop);
		}
	}
	
	if (crd->dellabel.l) {
		GList *node;
		
		for (node = crd->dellabel.l; node; node = node->next) {
			CardDelLabel *dellabel = (CardDelLabel *) node->data;
			
			vprop = add_strProp (vobj, VCDeliveryLabelProp, 
					    dellabel->data);
			add_AddrType (vprop, dellabel->type);
			add_CardProperty (vprop, &dellabel->prop);
		}
	}
	
	add_CardStrProperty (vobj, VCMailerProp, &crd->mailer);
	
	if (crd->timezn.prop.used) {
		char *str;
		
		str = card_timezn_str (crd->timezn);
		vprop = addPropValue (vobj, VCTimeZoneProp, str);
		free (str);
		add_CardProperty (vprop, &crd->timezn.prop);
	}
	
	if (crd->geopos.prop.used) {
		char *str;
		
		str = card_geopos_str (crd->geopos);
		vprop = addPropValue (vobj, VCGeoLocationProp, str);
		free (str);
		add_CardProperty (vprop, &crd->geopos.prop);
	}
	
        add_CardStrProperty (vobj, VCTitleProp, &crd->title);
        add_CardStrProperty (vobj, VCBusinessRoleProp, &crd->role);
	
	if (crd->logo.prop.used) {
		vprop = addPropSizedValue (vobj, VCLogoProp, 
					  crd->logo.data, crd->logo.size);
		add_PhotoType (vprop, crd->logo.type);
		add_CardProperty (vprop, &crd->logo.prop);
	}
	
	if (crd->agent)
	  addVObjectProp (vobj, card_convert_to_vobject (crd->agent));
	
	if (crd->org.prop.used) {
		vprop = addProp (vobj, VCOrgProp);
		add_strProp (vprop, VCOrgNameProp, crd->org.name);
		add_strProp (vprop, VCOrgUnitProp, crd->org.unit1);
		add_strProp (vprop, VCOrgUnit2Prop, crd->org.unit2);
		add_strProp (vprop, VCOrgUnit3Prop, crd->org.unit3);
		add_strProp (vprop, VCOrgUnit4Prop, crd->org.unit4);
		add_CardProperty (vprop, &crd->org.prop);
	}
	
        add_CardStrProperty (vobj, VCCategoriesProp, &crd->categories);
        add_CardStrProperty (vobj, VCCommentProp, &crd->comment);
	
	if (crd->sound.prop.used) {
		if (crd->sound.type != SOUND_PHONETIC)
		  vprop = addPropSizedValue (vobj, VCPronunciationProp,
					    crd->sound.data, crd->sound.size);
		else
		  vprop = addPropValue (vobj, VCPronunciationProp, 
				       crd->sound.data);
		
		add_SoundType (vprop, crd->sound.type);
		add_CardProperty (vprop, &crd->sound.prop);
	}
	
        add_CardStrProperty (vobj, VCURLProp, &crd->url);
        add_CardStrProperty (vobj, VCUniqueStringProp, &crd->uid);
	
	if (crd->key.prop.used) {
		vprop = addPropValue (vobj, VCPublicKeyProp, crd->key.data);
		add_KeyType (vprop, crd->key.type);
		add_CardProperty (vprop, &crd->key.prop);
	}
#endif
	temp = writeMemVObject(NULL, NULL, vobj);
	ret_val = g_strdup(temp);
	free(temp);
	return ret_val;
}

static void
parse_name(ECard *card, VObject *vobj)
{
	if ( card->name ) {
		e_card_name_free(card->name);
	}
	card->name = g_new(ECardName, 1);

	card->name->family     = e_v_object_get_child_value (vobj, VCFamilyNameProp);
	card->name->given      = e_v_object_get_child_value (vobj, VCGivenNameProp);
	card->name->additional = e_v_object_get_child_value (vobj, VCAdditionalNamesProp);
	card->name->prefix     = e_v_object_get_child_value (vobj, VCNamePrefixesProp);
	card->name->suffix     = e_v_object_get_child_value (vobj, VCNameSuffixesProp);
}

static void
parse_full_name(ECard *card, VObject *vobj)
{
	if ( card->fname )
		g_free(card->fname);
	assign_string(vobj, &(card->fname));
}

static void
parse_email(ECard *card, VObject *vobj)
{
	char *next_email;
	assign_string(vobj, &next_email);
	card->email = g_list_append(card->email, next_email);
}

static void
parse_bday(ECard *card, VObject *vobj)
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
parse_phone(ECard *card, VObject *vobj)
{
	ECardPhone *next_phone = g_new(ECardPhone, 1);
	assign_string(vobj, &(next_phone->number));
	next_phone->flags = get_phone_flags(vobj);
	card->phone = g_list_append(card->phone, next_phone);
}

static void
parse_address(ECard *card, VObject *vobj)
{
	ECardDeliveryAddress *next_addr = g_new(ECardDeliveryAddress, 1);

	next_addr->flags   = get_address_flags (vobj);
	next_addr->po      = e_v_object_get_child_value (vobj, VCPostalBoxProp);
	next_addr->ext     = e_v_object_get_child_value (vobj, VCExtAddressProp);
	next_addr->street  = e_v_object_get_child_value (vobj, VCStreetAddressProp);
	next_addr->city    = e_v_object_get_child_value (vobj, VCCityProp);
	next_addr->region  = e_v_object_get_child_value (vobj, VCRegionProp);
	next_addr->code    = e_v_object_get_child_value (vobj, VCPostalCodeProp);
	next_addr->country = e_v_object_get_child_value (vobj, VCCountryNameProp);

	card->address = g_list_append(card->address, next_addr);
}

static void
parse_attribute(ECard *card, VObject *vobj)
{
	ParsePropertyFunc function = g_hash_table_lookup(E_CARD_CLASS(GTK_OBJECT(card)->klass)->attribute_jump_table, vObjectName(vobj));
	if ( function )
		function(card, vobj);
}

static void
parse(ECard *card, VObject *vobj)
{
	VObjectIterator iterator;
	initPropIterator(&iterator, vobj);
	while(moreIteration (&iterator)) {
		parse_attribute(card, nextVObject(&iterator));
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

	gtk_object_add_arg_type ("ECard::full_name",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_FULL_NAME);  
	gtk_object_add_arg_type ("ECard::name",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_NAME);
	gtk_object_add_arg_type ("ECard::address",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_ADDRESS);
	gtk_object_add_arg_type ("ECard::phone",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_PHONE);
	gtk_object_add_arg_type ("ECard::email",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_EMAIL);
	gtk_object_add_arg_type ("ECard::birth_date",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_BIRTH_DATE);


	object_class->destroy = e_card_destroy;
	object_class->get_arg = e_card_get_arg;
	object_class->set_arg = e_card_set_arg;
}

static void
e_card_phone_free (ECardPhone *phone)
{
	if ( phone ) {
		if ( phone->number )
			g_free(phone->number);
		g_free(phone);
	}
}

static void
e_card_delivery_address_free (ECardDeliveryAddress *addr)
{
	if ( addr ) {
		if ( addr->po )
			g_free(addr->po);
		if ( addr->ext )
			g_free(addr->ext);
		if ( addr->street )
			g_free(addr->street);
		if ( addr->city )
			g_free(addr->city);
		if ( addr->region )
			g_free(addr->region);
		if ( addr->code )
			g_free(addr->code);
		if ( addr->country )
			g_free(addr->country);
		g_free(addr);
	}
}

/*
 * ECard lifecycle management and vCard loading/saving.
 */

static void
e_card_destroy (GtkObject *object)
{
	ECard *card = E_CARD(object);
	if ( card->id )
		g_free(card->id);
	if ( card->fname )
		g_free(card->fname);
	if ( card->name )
		e_card_name_free(card->name);
	if ( card->bday )
		g_free(card->bday);

	g_list_foreach(card->email, (GFunc)g_free, NULL);
	g_list_free(card->email);

	g_list_foreach(card->phone, (GFunc)e_card_phone_free, NULL);
	g_list_free(card->phone);

	g_list_foreach(card->address, (GFunc)e_card_delivery_address_free, NULL);
	g_list_free(card->address);
}


/* Set_arg handler for the card */
static void
e_card_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ECard *card;
	
	card = E_CARD (object);

	switch (arg_id) {
	case ARG_FULL_NAME:
		if ( card->fname )
			g_free(card->fname);
		card->fname = g_strdup(GTK_VALUE_STRING(*arg));
		break;
	case ARG_NAME:
		if ( card->name )
			e_card_name_free(card->name);
		card->name = GTK_VALUE_POINTER(*arg);
		break;
	case ARG_ADDRESS:
		g_list_foreach(card->address, (GFunc)e_card_delivery_address_free, NULL);
		g_list_free(card->address);
		card->address = GTK_VALUE_POINTER(*arg);
		break;
	case ARG_PHONE:
		g_list_foreach(card->phone, (GFunc)e_card_phone_free, NULL);
		g_list_free(card->phone);
		card->phone = GTK_VALUE_POINTER(*arg);
		break;
	case ARG_EMAIL:
		g_list_foreach(card->email, (GFunc)g_free, NULL);
		g_list_free(card->email);
		card->email = GTK_VALUE_POINTER(*arg);
		break;
	case ARG_BIRTH_DATE:
		if ( card->bday )
			g_free(card->bday);
		card->bday = GTK_VALUE_POINTER(*arg);
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
	case ARG_FULL_NAME:
		if ( card->fname )
			GTK_VALUE_STRING (*arg) = g_strdup (card->fname);
		else
			GTK_VALUE_STRING (*arg) = NULL;
		break;
	case ARG_NAME:
		GTK_VALUE_POINTER(*arg) = card->name;
		break;
	case ARG_ADDRESS:
		GTK_VALUE_POINTER(*arg) = card->address;
		break;
	case ARG_PHONE:
		GTK_VALUE_POINTER(*arg) = card->phone;
		break;
	case ARG_EMAIL:
		GTK_VALUE_POINTER(*arg) = card->email;
		break;
	case ARG_BIRTH_DATE:
		GTK_VALUE_POINTER(*arg) = card->bday;
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
	card->id = g_strdup("");

	card->fname = NULL;
	card->name = NULL;
	card->bday = NULL;
	card->email = NULL;
	card->phone = NULL;
	card->address = NULL;
#if 0

	c = g_new0 (ECard, 1);
	
	c->fname      = 
	c->mailer     = 
	c->title      = 
	c->role       = 
	c->comment    = 
	c->categories = 
	c->url        = 
	c->uid        = e_card_prop_str_empty ();
	
	c->photo.type = PHOTO_JPEG;
	c->logo.type  = PHOTO_JPEG;
	c->rev.utc    = -1;
	c->sound.type = SOUND_PHONETIC;
	c->key.type   = KEY_PGP;

	c->categories.prop.encod = ENC_QUOTED_PRINTABLE;
	c->comment.prop.encod    = ENC_QUOTED_PRINTABLE;
	
	c->name.prop   = c->photo.prop = c->bday.prop     = c->timezn.prop   = 
	c->geopos.prop = c->logo.prop  = c->org.prop      = c->rev.prop      =
	c->sound.prop  = c->key.prop   = c->deladdr.prop  = c->dellabel.prop =
	c->phone.prop  = c->email.prop = c->xtension.prop = c->prop = e_card_prop_empty ();
	
	c->prop.type            = PROP_CARD;
	c->fname.prop.type      = PROP_FNAME;
	c->name.prop.type       = PROP_NAME;
	c->photo.prop.type      = PROP_PHOTO;
	c->bday.prop.type       = PROP_BDAY;
	
	c->deladdr.prop.type    = PROP_DELADDR_LIST;
	c->dellabel.prop.type   = PROP_DELLABEL_LIST;
	c->phone.prop.type      = PROP_PHONE_LIST;
	c->email.prop.type      = PROP_EMAIL_LIST;
	c->xtension.prop.type   = PROP_XTENSION_LIST;
	c->mailer.prop.type     = PROP_MAILER;
	c->timezn.prop.type     = PROP_TIMEZN;
	c->geopos.prop.type     = PROP_GEOPOS;
	c->title.prop.type      = PROP_TITLE;
	c->role.prop.type       = PROP_ROLE;
	c->logo.prop.type       = PROP_LOGO;
	c->org.prop.type        = PROP_ORG;
	c->categories.prop.type = PROP_CATEGORIES;
	c->comment.prop.type    = PROP_COMMENT;
	c->rev.prop.type        = PROP_REV;
	c->sound.prop.type      = PROP_SOUND;
	c->url.prop.type 	= PROP_URL;
	c->uid.prop.type 	= PROP_UID;
	c->key.prop.type 	= PROP_KEY;
	
	return c;
#endif
}

static void
assign_string(VObject *vobj, char **string)
{
	char *str = (vObjectValueType (vobj) ? fakeCString (vObjectUStringZValue (vobj)) : calloc(1, 1));
	*string = g_strdup(str);
	free(str);
}

#if 0
static void
e_card_str_free (CardStrProperty *sp)
{
	g_free (sp->str);

	e_card_prop_free (sp->prop);
}

static void
e_card_name_free (CardName *name)
{
	g_free (name->family);
	g_free (name->given);
	g_free (name->additional);
	g_free (name->prefix);
	g_free (name->suffix);

	e_card_prop_free (name->prop);
}

static void
e_card_photo_free (CardPhoto *photo)
{
	g_free (photo->data);

	e_card_prop_free (photo->prop);
}

/**
 * e_card_free:
 */
void
e_card_free (ECard *card)
{
	GList *l;

	g_return_if_fail (card != NULL);

	e_card_name_free (& card->name);
	e_card_str_free  (& card->fname);

	e_card_photo_free (card->photo);

	e_card_logo_free (card->logo);
	e_card_org_free (card->org);
	e_card_key_free (card->key);
	e_card_sound_free (card->sound);

	e_card_prop_str_free (& card->mailer);
	e_card_prop_str_free (& card->title);
	e_card_prop_str_free (& card->role);
	e_card_prop_str_free (& card->categories);
	e_card_prop_str_free (& card->comment);
	e_card_prop_str_free (& card->url);
	e_card_prop_str_free (& card->uid);

	/* address is a little more complicated */
	card_prop_free (card->deladdr.prop);
	while ((l = card->deladdr.l)) {

		e_card_deladdr_free ((CardDelAddr *) l->data);

		card->deladdr.l = g_list_remove_link (card->deladdr.l, l);
		g_list_free (l);
	}
	
	g_free (card);
}

typedef struct
{
	char c;
	int id;
	
	GList *sons;
} tree;

extern CardProperty 
e_card_prop_empty (void)
{
	CardProperty prop;
	
	prop.used = FALSE;
	
	prop.type = PROP_NONE;
	prop.encod = ENC_7BIT;
	prop.value = VAL_INLINE;
	prop.charset = NULL;
	prop.lang = NULL;
	prop.grp = NULL;
	prop.xtension = NULL;
	
	prop.user_data = NULL;
	
	return prop;
}

static CardStrProperty 
e_card_prop_str_empty (void)
{
	CardStrProperty strprop;
	
	strprop.prop = card_prop_empty ();
	strprop.str = NULL;
	
	return strprop;
}

/* Intended to check asserts. */
extern int card_check_prop (ECardProperty prop)
{
	if (((prop.used == FALSE) || (prop.used == TRUE)) &&
	    ((prop.type >= PROP_NONE) && (prop.type <= PROP_LAST)) &&
	    ((prop.encod >= ENC_NONE) && (prop.encod <= ENC_LAST)) &&
	    ((prop.value >= VAL_NONE) && (prop.value <= VAL_LAST)))
	  return TRUE;
	    
	return FALSE;
}

extern void
card_prop_free (CardProperty prop)
{
	GList *l;
	
	g_free (prop.charset);
	g_free (prop.lang);
	
	for (l = prop.xtension; l; l = l->next) {
		CardXAttribute *xa = (CardXAttribute *) l->data;
		g_free (xa->name);
		g_free (xa->data);
	}
	
	g_list_free (l);
	
	prop.used = FALSE;
}
	
e_card_deladdr_free (ECardDelAddr *c)
{
	card_prop_free (c->prop);

	g_free (p->pobox);
	g_free (p->ext);
	g_free (p->street);
	g_free (p->city);
	g_free (p->region);
	g_free (p->code);
	g_free (p->country);
}

void 
card_free (Card *crd)
{
}

static tree *
new_tree (char c, int id)
{
	tree *t;
	
	t = malloc (sizeof (tree));
	t->c = c;
	t->id = id;
	t->sons = NULL;
	
	return t;
}

static void 
add_branch (tree *t, char *str, int id)
{
	tree *tmp;
	char *end;
	
	end = str + strlen (str) + 1;

	while (str != end) {
		tmp = new_tree (*str, id);
		t->sons = g_list_append (t->sons, (gpointer) tmp);
		t = tmp;
		
		str ++;
	}
}

static tree *
add_to_tree (tree *t, struct pair p)
{
	GList *node;
	char *c, *end;
	tree *tmp;
	
	  c = p.str;
	end = c + strlen (c) + 1;
	tmp = t;
	
	while (c != end) {
		for (node = tmp->sons; node; node = node->next)
		  if (((tree *) node->data)->c == *c) {
			  break;
		  }
		
		if (node) {
			tmp = (tree *) node->data;
			tmp->id = 0;
			c++;
		}
		else {
			add_branch (tmp, c, p.id);
			break;
		}
	}
	
	return t;
}
		
static tree *
create_search_tree (void)
{
	tree *t;
	int i;

	t = new_tree (0, 0);
	for (i = 0; prop_lookup[i].str; i++)
	  t = add_to_tree (t, prop_lookup[i]);
	
	return t;
}
		
static int 
card_lookup_name (const char *c)
{
	static tree *search_tree = NULL;
	GList *node;
	tree *tmp;
	const char *end;
	
	if (!search_tree)
	  search_tree = create_search_tree ();
	
	tmp = search_tree;
	end = c + strlen (c) + 1;
	
	while (tmp->id == 0 && c != end) {
		for (node = tmp->sons; node; node = node->next)
		  if (((tree *) node->data)->c == *c) {
			  break;
		  }
	
		if (node) {
			tmp = (tree *) node->data;
			c++;
		}
		else
		  return 0;
	}
	
	return tmp->id;
}

static enum PhotoType 
get_photo_type (VObject *o)
{
	VObject *vo;
	int i;
	
	for (i = 0; photo_pairs[i].str; i++)
	  if (has (o, photo_pairs[i].str))
	    return photo_pairs[i].id;

	g_warning ("? < No PhotoType for Photo property. Falling back to JPEG.");
	return PHOTO_JPEG;
}

static CardProperty 
get_CardProperty (VObject *o)
{
	VObjectIterator i;
	CardProperty prop;

	prop = card_prop_empty ();
	prop.used = TRUE;
	
	initPropIterator (&i, o);
	while (moreIteration (&i)) {
		VObject *vo = nextVObject (&i);
		const char *n = vObjectName (vo);
		int propid;
	
		propid = card_lookup_name (n);
		
		switch (propid) {
			
		 case PROP_VALUE:
		
			if (has (vo, VCContentIDProp))
			  prop.value = VAL_CID;
			else if (has (vo, VCURLValueProp))
			  prop.value = VAL_URL;
			break;
			
		 case PROP_ENCODING:
			if (has (vo, VCQuotedPrintableProp))
			  prop.encod = ENC_QUOTED_PRINTABLE;
			else if (has (vo, VC8bitProp))
			  prop.encod = ENC_8BIT;
			else if (has (vo, VCBase64Prop))
			  prop.encod = ENC_BASE64;
			break;
			
		 case PROP_QUOTED_PRINTABLE:
			prop.encod = ENC_QUOTED_PRINTABLE;
			break;
			
		 case PROP_8BIT:
			prop.encod = ENC_8BIT;
			break;
			
		 case PROP_BASE64:
			prop.encod = ENC_BASE64;
			break;
	
		 case PROP_LANG:
			if (vObjectValueType (vo)) {
				prop.lang = 
				  g_strdup (vObjectStringZValue (vo));
			} else
			  g_warning ("? < No value for LANG attribute.");
			break;
			
		 case PROP_CHARSET:
			if (vObjectValueType (vo)) {
				prop.charset = 
				  g_strdup (vObjectStringZValue (vo));
				g_warning (prop.charset); 
			} else
			  g_warning ("? < No value for CHARSET attribute.");
			break;
		 default:
				{
					CardXAttribute *c;

					c = malloc (sizeof (CardXAttribute));
					c->name = g_strdup (n);
					
					if (vObjectValueType (vo))
					  c->data = 
					  g_strdup (vObjectStringZValue (vo));
					else
					  c->data = NULL;
					
					prop.xtension = 
					  g_list_append (prop.xtension, c);
				}
		}
	}

	return prop;
}

static gboolean
e_card_prop_has (VObject    *o,
		 const char *id)
{
	g_assert (o  != NULL);
	g_assert (id != NULL);

	if (isAPropertyOf (o, id) == NULL)
		return FALSE;

	return TRUE;
}

static const char *
e_card_prop_get_str (VObject    *o,
		     const char *id)
{
	VObject *strobj;

	g_assert (o  != NULL);
	g_assert (id != NULL);

	strobj = isAPropertyOf (o, id);

	if (strobj == NULL)
		return g_strdup ("");

	if (vObjectValueType (strobj) != NULL) {
		char *str;
		char *g_str;

		str = fakeCString (vObjectStringZValue (strobj));
		g_str = g_strdup (str);
		free (str);

		return g_str;
	}

	return g_strdup ("");
}

static ECardName *
e_card_get_name (VObject *o)
{
	CardName *name;
	VObject *vo;
	char *the_str;

	name = g_new0 (ECardName, 1);

	name->family     = e_card_prop_get_substr (o, VCFamilyNameProp);
	name->given      = e_card_prop_get_substr (o, VCGivenNameProp);
	name->additional = e_card_prop_get_substr (o, VCAdditionalNamesProp);
	name->prefix     = e_card_prop_get_substr (o, VCNamePrefixesProp);
	name->suffix     = e_card_prop_get_substr (o, VCNameSuffixesProp);

	return name;
}

static CardDelLabel *
get_CardDelLabel (VObject *o)
{
	CardDelLabel *dellabel;
	char *the_str;
	
	dellabel = malloc (sizeof (CardDelLabel));
	
	dellabel->type = get_addr_type (o);
	dellabel->data = g_strdup (str_val (o));
	
	free (the_str);
	return dellabel;
}

static CardPhone *
get_CardPhone (VObject *o)
{
	CardPhone *ret;
	char *the_str;
	
	ret = malloc (sizeof (CardPhone));
	ret->type = get_phone_type (o);
	ret->data = g_strdup (str_val (o));
	
	free (the_str);

	return ret;
}

static CardEMail *
get_CardEMail (VObject *o)
{
	CardEMail *ret;
	char *the_str; 
	
	ret = malloc (sizeof (CardEMail)); 
	ret->type = get_email_type (o);
	ret->data = g_strdup (str_val (o));
	
	free (the_str);
	
	return ret;
}
	
static CardTimeZone 
strtoCardTimeZone (char *str)
{
	char s[3];
	CardTimeZone tz;
	
	if (*str == '-') {
		tz.sign = -1;
		str++;
	} else
	  tz.sign = 1;
	
	tz.hours = 0;
	tz.mins = 0;
	
	s[2] = 0;
	if (strlen (str) > 2) {
		s[0] = str[0];
		s[1] = str[1];
		tz.hours = atoi (s);
	} else {
		g_warning ("? < TimeZone value is too short.");
		return tz;
	}
	
	str += 2;
	if (*str == ':')
	  str++;
	
	if (strlen (str) >= 2) {
		s[0] = str[0];
		s[1] = str[1];
		tz.mins = atoi (s);
	} else {
		g_warning ("? < TimeZone value is too short.");
		return tz;
	}
	
	if (strlen (str) > 3)
		g_warning ("? < TimeZone value is too long.");

	return tz;
}

static CardGeoPos 
strtoCardGeoPos (char *str)
{
	CardGeoPos gp;
	char *s;
	
	gp.lon = 0;
	gp.lat = 0;
	  
	s = strchr (str, ',');
	
	if (! s) {
		g_warning ("? < Bad format for GeoPos property.");
		return gp;
	}
	
	*s = 0;
	s++;
	
	gp.lon = atof (str);
	gp.lat = atof (s);
	
	return gp;
}
	
static CardOrg *
e_card_vobject_to_org (VObject *o)
{
	VObject *vo;
	char *the_str;
	CardOrg *org;

	org = g_new0 (CardOrg, 1);

	if (has (o, VCOrgNameProp)) {
		org.name = g_strdup (str_val (vo));
		free (the_str);
	}
	if (has (o, VCOrgUnitProp)) {
		org.unit1 = g_strdup (str_val (vo));
		free (the_str);
	}
	if (has (o, VCOrgUnit2Prop)) {
		org.unit2 = g_strdup (str_val (vo));
		free (the_str);
	}
	if (has (o, VCOrgUnit3Prop)) {
		org.unit3 = g_strdup (str_val (vo));
		free (the_str);
	}
	if (has (o, VCOrgUnit4Prop)) {
		org.unit4 = g_strdup (str_val (vo));
		free (the_str);
	}
	
	return org;
}

static CardXProperty *
get_XProp (VObject *o)
{
	char *the_str;
	CardXProperty *ret;
	
	ret = malloc (sizeof (CardXProperty)); 
	ret->name = g_strdup (vObjectName (o));
	ret->data = g_strdup (str_val (o));
	free (the_str);
	
	return ret;
}

static CardRev 
strtoCardRev (char *str)
{
	char s[3], *t, *ss;
	int len, i;
	CardRev rev;
	
	rev.utc = 0;
	len = strlen (str);
	
	if (str[len] == 'Z') {              /* Is it UTC? */
		rev.utc = 1;
		str[len] = 0;
	}
	  
	s[2] = 0;
	t = strchr (str, 'T');
	if (t) {                            /* Take the Time */
		*t = 0;
		t++;
		if (strlen (t) > 2) {
			s[0] = t[0];
			s[1] = t[1];
			rev.tm.tm_hour = atoi (s);
		} else {
			g_warning ("? < Rev value is too short.");
			return rev;
		}
		
		t += 2;
		if (*t == ':')             /* Ignore ':' separator */
		  t++;
		
		if (strlen (t) > 2) {
			s[0] = t[0];
			s[1] = t[1];
			rev.tm.tm_min = atoi (s);
		} else {
			g_warning ("? < Rev value is too short.");
			return rev;
		}
		
		t += 2;
		if (*t == ':')
		  t++;
		
		if (strlen (t) > 2) {
			s[0] = t[0];
			s[1] = t[1];
			rev.tm.tm_sec = atoi (s);
		} else {
			g_warning ("? < Rev value is too short.");
			return rev;
		}

		if (strlen (str) > 3)
		  g_warning ("? < Rev value is too long.");
		
	} else {
		g_warning ("? < No time value for Rev property.");
	}

	/* Now the date (the part before the T) */
	
	if (strchr (str, '-')) {                        /* extended iso 8601 */
		for (ss = strtok (str, "-"), i = 0; ss;
		     ss = strtok (NULL, "-"), i++)
		  switch (i) {
		   case 0:
			  rev.tm.tm_year = atoi (ss);
			  break;
		   case 1:
			  rev.tm.tm_mon = atoi (ss);
			  break;
		   case 2:
			  rev.tm.tm_mday = atoi (ss);
			  break;
		   default:
			  g_warning ("? < Too many values for Rev property.");
		  }
		
		if (i < 2)
		  g_warning ("? < Too few values for Rev property.");
	} else {
		if (strlen (str) >= 8) {             /* short representation */
			rev.tm.tm_mday = atoi (str + 6);
			str[6] = 0;
			rev.tm.tm_mon = atoi (str + 4);
			str[4] = 0;
			rev.tm.tm_year = atoi (str);
		} else
		  g_warning ("? < Bad format for Rev property.");
	}
	
	return rev;
}
		
static enum KeyType 
get_key_type (VObject *o)
{
	VObject *vo;
	int i;
	
	for (i = 0; key_pairs[i].str; i++)
	  if (has (o, key_pairs[i].str))
	    return key_pairs[i].id;

	g_warning ("? < No KeyType for Key property. Falling back to PGP.");
	return KEY_PGP;
}

static CardPhoto 
get_CardPhoto (VObject *o)
{
	VObject *vo;
	char *the_str;
	CardPhoto photo;

	photo.type = get_photo_type (o);
	
	if (has (o, VCDataSizeProp)) {
		photo.size = vObjectIntegerValue (vo);
		photo.data = malloc (photo.size);
		memcpy (photo.data, vObjectAnyValue (o), photo.size);
	} else {
		photo.size = strlen (str_val (o)) + 1;
		photo.data = g_strdup (the_str);
		free (the_str);
	}
	
	return photo;
}

static enum SoundType 
get_sound_type (VObject *o)
{
	VObject *vo;
	int i;
	
	for (i = 0; sound_pairs[i].str; i++)
	  if (has (o, sound_pairs[i].str))
	    return sound_pairs[i].id;

	return SOUND_PHONETIC;
}
	
static CardSound 
get_CardSound (VObject *o)
{
	VObject *vo;
	char *the_str;
	CardSound sound;

	sound.type = get_sound_type (o);
	
	if (has (o, VCDataSizeProp)) {
		sound.size = vObjectIntegerValue (vo);
		sound.data = malloc (sound.size);
		memcpy (sound.data, vObjectAnyValue (o), sound.size);
	} else {
		sound.size = strlen (str_val (o));
		sound.data = g_strdup (the_str);
		free (the_str);
	}
	
	return sound;
}

/* Loads our card contents from a VObject */
static ECard *
e_card_construct_from_vobject (ECard   *card,
			       VObject *vcrd)
{
	VObjectIterator i;
	Card *crd;
	char *the_str;

	initPropIterator (&i, vcrd);
	crd = card_new ();

	while (moreIteration (&i)) {
		VObject *o = nextVObject (&i);
		const char *n = vObjectName (o);
		int propid;
		CardProperty *prop = NULL;

		propid = card_lookup_name (n);
		
		switch (propid) {
		case PROP_FNAME:
			prop = &crd->fname.prop;
			crd->fname.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_NAME:
			prop = &crd->name.prop;
			crd->name = e_card_get_name (o);
			break;
		 case PROP_PHOTO:
			prop = &crd->photo.prop;
			crd->photo = get_CardPhoto (o);
			break;
		 case PROP_BDAY:
			prop = &crd->bday.prop;
			crd->bday = strtoCardBDay (str_val (o));
			free (the_str);
			break;
		 case PROP_DELADDR:
				{
					CardDelAddr *c;
					c = get_CardDelAddr (o);
					prop = &c->prop;
					crd->deladdr.l = g_list_append (crd->deladdr.l, c);
				}
			break;
		 case PROP_DELLABEL:
				{
					CardDelLabel *c;
					c = get_CardDelLabel (o);
					prop = &c->prop;
					crd->dellabel.l = g_list_append (crd->dellabel.l, c);
				}
			break;
		 case PROP_PHONE:
				{
					CardPhone *c;
					
					c = get_CardPhone (o);
					prop = &c->prop;
					crd->phone.l = g_list_append (crd->phone.l, c);
				}
			break;
		 case PROP_EMAIL:
				{
					CardEMail *c;
					
					c = get_CardEMail (o);
					prop = &c->prop;
					crd->email.l = g_list_append (crd->email.l, c);
				}
			break;
		 case PROP_MAILER:
			prop = &crd->mailer.prop;
			crd->mailer.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_TIMEZN:
			prop = &crd->timezn.prop;
			crd->timezn = strtoCardTimeZone (str_val (o));
			free (the_str);
			break;
		 case PROP_GEOPOS:
			prop = &crd->geopos.prop;
			crd->geopos = strtoCardGeoPos (str_val (o));
			break;
		 case PROP_TITLE:
			prop = &crd->title.prop;
			crd->title.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_ROLE:
			prop = &crd->role.prop;
			crd->role.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_LOGO:
			prop = &crd->logo.prop;
			crd->logo = get_CardPhoto (o);
			break;
		 case PROP_AGENT:
			crd->agent = card_create_from_vobject (o);
			break;
		 case PROP_ORG:
			prop = &crd->org.prop;
			crd->org = get_CardOrg (o);
			break;
		 case PROP_CATEGORIES:
			prop = &crd->categories.prop;
			crd->categories.str = g_strdup (str_val (o));
			crd->categories.prop.encod = ENC_QUOTED_PRINTABLE;
			free (the_str);
			break;
		 case PROP_COMMENT:
			prop = &crd->comment.prop;
			crd->comment.str = g_strdup (str_val (o));
			crd->comment.prop.encod = ENC_QUOTED_PRINTABLE;
			free (the_str);
			break;
		 case PROP_REV:
			prop = &crd->rev.prop;
			crd->rev = strtoCardRev (str_val (o));
			free (the_str);
			break;
		 case PROP_SOUND:
			prop = &crd->sound.prop;
			crd->sound = get_CardSound (o);
			break;
		 case PROP_URL:
			prop = &crd->url.prop;
			crd->url.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_UID:
			prop = &crd->uid.prop;
			crd->uid.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_VERSION:
				{
					char *str;
					str = str_val (o);
					if (strcmp (str, "2.1"))
					  g_warning ("? < Version doesn't match.");
					free (the_str);
				}
			break;
		 case PROP_KEY:
			prop = &crd->key.prop;
			crd->key.type = get_key_type (o);
			crd->key.data = g_strdup (str_val (o));
			free (the_str);
			break;
		 default:
				{
					CardXProperty *c;
				
					c = get_XProp (o);
					prop = &c->prop;
					crd->xtension.l = g_list_append (crd->xtension.l, c);
				}
			break;
		}
		
		if (prop) {
			*prop = get_CardProperty (o);
			prop->type = propid;
		}
	}
	
	return crd;
}
		
/* Loads a card from a file */
GList *
card_load (GList *crdlist, char *fname)
{
	VObject *vobj, *tmp;
	
	vobj = Parse_MIME_FromFileName (fname);
	if (!vobj) {
		g_warning ("Could not load the cardfile");
		return NULL;
	}

	while (vobj) {
		const char *n = vObjectName (vobj);
		
		if (strcmp (n, VCCardProp) == 0) {
			crdlist = g_list_append (crdlist, (gpointer)
					    card_create_from_vobject (vobj));
		}
		tmp = vobj;
		vobj = nextVObjectInList (vobj);
		cleanVObject (tmp);
	}

	cleanVObject (vobj);
	cleanStrTbl ();
	return crdlist;
}

static VObject *
add_strProp (VObject *o, const char *id, char *val)
{
	VObject *vo = NULL;
	
	if (val)
	  vo = addPropValue (o, id, val);

	return vo;
}

static VObject *
add_CardProperty (VObject *o, CardProperty *prop)
{
	GList *node;
	
	switch (prop->encod) {
	 case ENC_BASE64:
		addProp (o, VCBase64Prop);
		break;
	 case ENC_QUOTED_PRINTABLE:
		addProp (o, VCQuotedPrintableProp);
		break;
	 case ENC_8BIT:
		addProp (o, VC8bitProp);
		break;
	 case ENC_7BIT:
		/* Do nothing: 7BIT is the default. Avoids file clutter. */
		break;
	 default:
		g_warning ("? < Card had invalid encoding type.");
	}
	
	switch (prop->value) {
	 case VAL_CID:
		addProp (o, VCContentIDProp);
		break;
	 case VAL_URL:
		addProp (o, VCURLValueProp);
		break;
	 case VAL_INLINE:
		/* Do nothing: INLINE is the default. Avoids file clutter. */
		break;
	 default:
		g_warning ("? < Card had invalid value type.");
	}
	
	for (node = prop->xtension; node; node = node->next) {
		CardXAttribute *xa = (CardXAttribute *) node->data;
		if (xa->data)
		  addPropValue (o, xa->name, xa->data);
		else
		  addProp (o, xa->name);
	}

	add_strProp (o, VCCharSetProp, prop->charset);
	add_strProp (o, VCLanguageProp, prop->lang);
	
	return o;
}

static VObject *
add_CardStrProperty (VObject *vobj, const char *id, CardStrProperty *strprop)
{
	VObject *vprop;
	
	if (strprop->prop.used) {
		vprop = add_strProp (vobj, id, strprop->str);
		add_CardProperty (vprop, &strprop->prop);
	}
	
	return vobj;
}

static VObject *
add_PhotoType (VObject *o, enum PhotoType photo_type)
{
	int i;
	
	for (i = 0; photo_pairs[i].str; i++)
	  if (photo_type == photo_pairs[i].id) {
		  addProp (o, photo_pairs[i].str);
		  return o;
	  }

	g_warning ("? > No PhotoType for Photo property. Falling back to JPEG.");
	addProp (o, VCJPEGProp);
	
	return o;
}

static VObject *
add_AddrType (VObject *o, int addr_type)
{
	int i;
	
	for (i = 0; addr_pairs[i].str; i++)
	  if (addr_type & addr_pairs[i].id)
	    addProp (o, addr_pairs[i].str);
	
	return o;
}

static void
add_strAddrType (GString *string, int addr_type)
{
	int i, first = 1;
	char *str;
	
	if (addr_type) {
		g_string_append (string, " (");
		
		for (i = 0; addr_pairs[i].str; i++)
		  if (addr_type & addr_pairs[i].id) {
			  if (!first)
			    g_string_append (string, ", ");
			  first = 0;
			  str = my_cap (addr_pairs[i].str);
			  g_string_append (string, str);
			  g_free (str);
		  }
		
		g_string_append_c (string, ')');
	}
}

static VObject *
add_PhoneType (VObject *o, int phone_type)
{
	int i;
	
	for (i = 0; phone_pairs[i].str; i++)
	  if (phone_type & phone_pairs[i].id)
	    addProp (o, phone_pairs[i].str);
	
	return o;
}

static void
add_strPhoneType (GString *string, int phone_type)
{
	int i, first = 1;
	char *str;
	
	if (phone_type) {
		g_string_append (string, " (");
		
		for (i = 0; phone_pairs[i].str; i++)
		  if (phone_type & phone_pairs[i].id) {
			  if (!first)
			    g_string_append (string, ", ");
			  first = 0;
			  str = my_cap (phone_pairs[i].str);
			  g_string_append (string, str);
			  g_free (str);
		  }
		
		g_string_append_c (string, ')');
	}
}

static VObject *
add_EMailType (VObject *o, enum EMailType email_type)
{
	int i;
	
	for (i = 0; email_pairs[i].str; i++)
	  if (email_type == email_pairs[i].id) {
		  addProp (o, email_pairs[i].str);
		  return o;
	  }

	g_warning ("? > No EMailType for EMail property. Falling back to INET.");
	addProp (o, VCInternetProp);
	
	return o;
}

static void
add_strEMailType (GString *string, int email_type)
{
	int i;
	char *str;
	
	if (email_type) {
		g_string_append (string, " (");
		
		for (i = 0; email_pairs[i].str; i++)
		  if (email_type == email_pairs[i].id) {
			  str = my_cap (email_pairs[i].str);
			  g_string_append (string, str);
			  g_free (str);
			  break;
		  }
		
		g_string_append_c (string, ')');
	}
}

static VObject *
add_KeyType (VObject *o, enum KeyType key_type)
{
	int i;
	
	for (i = 0; key_pairs[i].str; i++)
	  if (key_type == key_pairs[i].id) {
		  addProp (o, key_pairs[i].str);
		  return o;
	  }

	g_warning ("? > No KeyType for Key property. Falling back to PGP.");
	addProp (o, VCPGPProp);
	
	return o;
}

static void
add_strKeyType (GString *string, int key_type)
{
	int i;
	char *str;
	
	if (key_type) {
		g_string_append (string, " (");
		
		for (i = 0; key_pairs[i].str; i++)
		  if (key_type == key_pairs[i].id) {
			  str = my_cap (key_pairs[i].str);
			  g_string_append (string, str);
			  g_free (str);
			  break;
		  }
		
		g_string_append_c (string, ')');
	}
}

static VObject *
add_SoundType (VObject *o, enum SoundType sound_type)
{
	int i;
	
	for (i = 0; sound_pairs[i].str; i++)
	  if (sound_type == sound_pairs[i].id) {
		  addProp (o, sound_pairs[i].str);
		  return o;
	  }

	return o;
}

char *card_bday_str (CardBDay bday)
{
	char *str;
	
	str = malloc (12);
	snprintf (str, 12, "%04d-%02d-%02d", bday.year, bday.month, bday.day);
	
	return str;
}

char *card_timezn_str (CardTimeZone timezn)
{
	char *str;
	
	str = malloc (7);
	snprintf (str, 7, (timezn.sign == -1)? "-%02d:%02d" : "%02d:%02d",
		 timezn.hours, timezn.mins);
	return str;
}

char *card_geopos_str (CardGeoPos geopos)
{
	char *str;
	
	str = malloc (15);
	snprintf (str, 15, "%03.02f,%03.02f", geopos.lon, geopos.lat);
	return str;
}

static void add_CardStrProperty_to_string (GString *string, char *prop_name,
					   CardStrProperty *strprop)
{
	if (strprop->prop.used) {
		if (prop_name)
		  g_string_append (string, prop_name);
		
		g_string_append (string, strprop->str);
	}
}

static void add_strProp_to_string (GString *string, char *prop_name, char *val)
{
	if (val) {
		if (prop_name)
		  g_string_append (string, prop_name);
		
		g_string_append (string, val);
	}
}

static void addProp_to_string (GString *string, char *prop_name)
{
	if (prop_name)
	  g_string_append (string, prop_name);
}

char *
card_to_string (Card *crd)
{
	GString *string;
	char *ret;
	
	string = g_string_new ("");
	
	add_CardStrProperty_to_string (string, _ ("Card: "), &crd->fname);
	if (crd->name.prop.used) {
		addProp_to_string (string, _ ("\nName: "));
		add_strProp_to_string (string, _ ("\n  Prefix:     "), crd->name.prefix);
		add_strProp_to_string (string, _ ("\n  Given:      "), crd->name.given);
		add_strProp_to_string (string, _ ("\n  Additional: "), crd->name.additional);
		add_strProp_to_string (string, _ ("\n  Family:     "), crd->name.family);
		add_strProp_to_string (string, _ ("\n  Suffix:     "), crd->name.suffix);
		g_string_append_c (string, '\n');
	}
	
/*	if (crd->photo.prop.used) {
		addPropSizedValue (string, _ ("\nPhoto: "), 
					  crd->photo.data, crd->photo.size);
		add_PhotoType (string, crd->photo.type);
	}*/
	
	if (crd->bday.prop.used) {
		char *date_str;
		
		date_str = card_bday_str (crd->bday);
		add_strProp_to_string (string, _ ("\nBirth Date: "), date_str);
		free (date_str);
	}
	
	if (crd->deladdr.l) {
		GList *node;
		
		for (node = crd->deladdr.l; node; node = node->next) {
			CardDelAddr *deladdr = (CardDelAddr *) node->data;
			
			if (deladdr->prop.used) {
				addProp_to_string (string, _ ("\nAddress:"));
				add_strAddrType (string, deladdr->type);
				add_strProp_to_string (string, _ ("\n  Postal Box:  "), deladdr->po);
				add_strProp_to_string (string, _ ("\n  Ext:         "),deladdr->ext);
				add_strProp_to_string (string, _ ("\n  Street:      "),deladdr->street);
				add_strProp_to_string (string, _ ("\n  City:        "), deladdr->city);
				add_strProp_to_string (string, _ ("\n  Region:      "), deladdr->region);
				add_strProp_to_string (string, _ ("\n  Postal Code: "), deladdr->code);
				add_strProp_to_string (string, _ ("\n  Country:     "), deladdr->country);
			}
		}
		
		g_string_append_c (string, '\n');
	}
	
	if (crd->dellabel.l) {
		GList *node;
		
		for (node = crd->dellabel.l; node; node = node->next) {
			CardDelLabel *dellabel = (CardDelLabel *) node->data;
			
			add_strProp_to_string (string, _ ("\nDelivery Label: "),
					    dellabel->data);
			add_strAddrType (string, dellabel->type);
		}
	}
	
	if (crd->phone.l) {
		GList *node;
		char *sep;
		
		if (crd->phone.l->next) {
			sep = "  ";
			g_string_append (string, _ ("\nTelephones:\n"));
		} else {
			sep = " ";
			g_string_append (string, _ ("\nTelephone:"));
		}
		
		for (node = crd->phone.l; node; node = node->next) {
			CardPhone *phone = (CardPhone *) node->data;

			if (phone->prop.used) {
				g_string_append (string, sep);
				g_string_append (string, phone->data);
				add_strPhoneType (string, phone->type);
				g_string_append_c (string, '\n');
			}
		}
		
		if (crd->phone.l->next)
		  g_string_append_c (string, '\n');
	}

	if (crd->email.l) {
		GList *node;
		char *sep;
		
		if (crd->email.l->next) {
			sep = "  ";
			g_string_append (string, _ ("\nE-mail:\n"));
		} else {
			sep = " ";
			g_string_append (string, _ ("\nE-mail:"));
		}
		
		
		for (node = crd->email.l; node; node = node->next) {
			CardEMail *email = (CardEMail *) node->data;
			
			if (email->prop.used) {
				g_string_append (string, sep);
				g_string_append (string, email->data);
				add_strEMailType (string, email->type);
				g_string_append_c (string, '\n');
			}
		}
		
		if (crd->email.l->next)
		  g_string_append_c (string, '\n');
	}

	add_CardStrProperty_to_string (string, _ ("\nMailer: "), &crd->mailer);
	
	if (crd->timezn.prop.used) {
		char *str;
		
		str = card_timezn_str (crd->timezn);
		add_strProp_to_string (string, _ ("\nTime Zone: "), str);
		free (str);
	}
	
	if (crd->geopos.prop.used) {
		char *str;
		
		str = card_geopos_str (crd->geopos);
		add_strProp_to_string (string, _ ("\nGeo Location: "), str);
		free (str);
	}
	
        add_CardStrProperty_to_string (string, _ ("\nTitle: "), &crd->title);
        add_CardStrProperty_to_string (string, _ ("\nBusiness Role: "), &crd->role);
	
/*	if (crd->logo.prop.used) {
		addPropSizedValue (string, _ ("\nLogo: "), 
					  crd->logo.data, crd->logo.size);
		add_PhotoType (string, crd->logo.type);
	}*/
	
/*	if (crd->agent)
	  addstringectProp (string, card_convert_to_stringect (crd->agent));*/
	
	if (crd->org.prop.used) {
		addProp_to_string (string, _ ("\nOrg: "));
		add_strProp_to_string (string, _ ("\n  Name:  "), crd->org.name);
		add_strProp_to_string (string, _ ("\n  Unit:  "), crd->org.unit1);
		add_strProp_to_string (string, _ ("\n  Unit2: "), crd->org.unit2);
		add_strProp_to_string (string, _ ("\n  Unit3: "), crd->org.unit3);
		add_strProp_to_string (string, _ ("\n  Unit4: "), crd->org.unit4);
		g_string_append_c (string, '\n');
	}
	
        add_CardStrProperty_to_string (string, _ ("\nCategories: "), &crd->categories);
        add_CardStrProperty_to_string (string, _ ("\nComment: "), &crd->comment);
	
/*	if (crd->sound.prop.used) {
		if (crd->sound.type != SOUND_PHONETIC)
		  addPropSizedValue (string, _ ("\nPronunciation: "),
					    crd->sound.data, crd->sound.size);
		else
		  add_strProp_to_string (string, _ ("\nPronunciation: "), 
				       crd->sound.data);
		
		add_SoundType (string, crd->sound.type);
	}*/
	
        add_CardStrProperty_to_string (string, _ ("\nURL: "), &crd->url);
        add_CardStrProperty_to_string (string, _ ("\nUnique String: "), &crd->uid);
	
	if (crd->key.prop.used) {
		add_strProp_to_string (string, _ ("\nPublic Key: "), crd->key.data);
		add_strKeyType (string, crd->key.type);
	}
	
	ret = g_strdup (string->str);
	g_string_free (string, TRUE);
	
	return ret;
}

char *
card_to_vobj_string (Card *crd)
{
	VObject *object;
	char *data, *ret_val;
	
	g_assert (crd != NULL);

	object = card_convert_to_vobject (crd);
	data = writeMemVObject (0, 0, object);
        ret_val = g_strdup (data);
	free (data);
		
	cleanVObject (object);

	return ret_val;
}

void 
card_save (Card *crd, FILE *fp)
{
	VObject *object;
	
	g_return_if_fail (crd != NULL);

	object = card_convert_to_vobject (crd);
	writeVObject (fp, object);
	cleanVObject (object);
}
#endif

static ECardDate
e_card_date_from_string (char *str)
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

static void
e_card_name_free(ECardName *name)
{
	if ( name ) {
		if ( name->prefix )
			g_free(name->prefix);
		if ( name->given )
			g_free(name->given);
		if ( name->additional )
			g_free(name->additional);
		if ( name->family )
			g_free(name->family);
		if ( name->suffix )
			g_free(name->suffix);
		g_free ( name );
	}
}

char *
e_v_object_get_child_value(VObject *vobj, char *name)
{
	char *ret_val;
	VObjectIterator iterator;
	initPropIterator(&iterator, vobj);
	while(moreIteration (&iterator)) {
		VObject *attribute = nextVObject(&iterator);
		const char *id = vObjectName(attribute);
		if ( ! strcmp(id, name) ) {
			assign_string(attribute, &ret_val);
			return ret_val;
		}
	}
	ret_val = g_new(char, 1);
	*ret_val = 0;
	return ret_val;
}

static ECardPhoneFlags
get_phone_flags (VObject *vobj)
{
	ECardPhoneFlags ret = 0;
	int i;

	struct { 
		char *id;
		ECardPhoneFlags flag;
	} phone_pairs[] = {
		{ VCPreferredProp, E_CARD_PHONE_PREF },
		{ VCWorkProp,      E_CARD_PHONE_WORK },
		{ VCHomeProp,      E_CARD_PHONE_HOME },
		{ VCVoiceProp,     E_CARD_PHONE_VOICE },
		{ VCFaxProp,       E_CARD_PHONE_FAX },
		{ VCMessageProp,   E_CARD_PHONE_MSG },
		{ VCCellularProp,  E_CARD_PHONE_CELL },
		{ VCPagerProp,     E_CARD_PHONE_PAGER },
		{ VCBBSProp,       E_CARD_PHONE_BBS },
		{ VCModemProp,     E_CARD_PHONE_MODEM },
		{ VCCarProp,       E_CARD_PHONE_CAR },
		{ VCISDNProp,      E_CARD_PHONE_ISDN },
		{ VCVideoProp,     E_CARD_PHONE_VIDEO },
	};
	
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

	struct { 
		char *id;
		ECardPhoneFlags flag;
	} phone_pairs[] = {
		{ VCPreferredProp, E_CARD_PHONE_PREF },
		{ VCWorkProp,      E_CARD_PHONE_WORK },
		{ VCHomeProp,      E_CARD_PHONE_HOME },
		{ VCVoiceProp,     E_CARD_PHONE_VOICE },
		{ VCFaxProp,       E_CARD_PHONE_FAX },
		{ VCMessageProp,   E_CARD_PHONE_MSG },
		{ VCCellularProp,  E_CARD_PHONE_CELL },
		{ VCPagerProp,     E_CARD_PHONE_PAGER },
		{ VCBBSProp,       E_CARD_PHONE_BBS },
		{ VCModemProp,     E_CARD_PHONE_MODEM },
		{ VCCarProp,       E_CARD_PHONE_CAR },
		{ VCISDNProp,      E_CARD_PHONE_ISDN },
		{ VCVideoProp,     E_CARD_PHONE_VIDEO },
	};
	
	for (i = 0; i < sizeof(phone_pairs) / sizeof(phone_pairs[0]); i++) {
		if (flags & phone_pairs[i].flag) {
				addProp (vobj, phone_pairs[i].id);
		}
	}
}

static ECardAddressFlags
get_address_flags (VObject *vobj)
{
	ECardAddressFlags ret = 0;
	int i;

	struct { 
		char *id;
		ECardAddressFlags flag;
	} addr_pairs[] = {
		{ VCDomesticProp, ADDR_DOM },
		{ VCInternationalProp, ADDR_INTL },
		{ VCPostalProp, ADDR_POSTAL },
		{ VCParcelProp, ADDR_PARCEL },
		{ VCHomeProp, ADDR_HOME },
		{ VCWorkProp, ADDR_WORK },
	};
	
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

	struct { 
		char *id;
		ECardAddressFlags flag;
	} addr_pairs[] = {
		{ VCDomesticProp, ADDR_DOM },
		{ VCInternationalProp, ADDR_INTL },
		{ VCPostalProp, ADDR_POSTAL },
		{ VCParcelProp, ADDR_PARCEL },
		{ VCHomeProp, ADDR_HOME },
		{ VCWorkProp, ADDR_WORK },
	};
	
	for (i = 0; i < sizeof(addr_pairs) / sizeof(addr_pairs[0]); i++) {
		if (flags & addr_pairs[i].flag) {
				addProp (vobj, addr_pairs[i].id);
		}
	}
}
