/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@helixcode.com>
 *   Arturo Espinosa
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#ifndef __E_CARD_H__
#define __E_CARD_H__

#include <time.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <e-card-types.h>
#include <e-card-list.h>

#define E_TYPE_CARD            (e_card_get_type ())
#define E_CARD(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_CARD, ECard))
#define E_CARD_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_CARD, ECardClass))
#define E_IS_CARD(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_CARD))
#define E_IS_CARD_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_CARD))

typedef struct _ECard ECard;
typedef struct _ECardClass ECardClass;

struct _ECard {
	GtkObject object;
	char *id;

	char            *fname;         /* The full name.                   */
	ECardName       *name;          /* The structured name.             */
	ECardList       *address;  	/* Delivery addresses (ECardDeliveryAddress *) */
#if 0
	GList           *del_labels;    /* Delivery address labels
					 * (ECardAddrLabel *)               */
#endif
	ECardList       *phone;         /* Phone numbers (ECardPhone *)     */
	ECardList       *email;         /* Email addresses (char *)         */
	char            *url;	        /* The person's web page.           */

	ECardDate       *bday;	        /* The person's birthday.           */
#if 0

	ECardOrg        *org;	        /* The person's organization.       */
	char            *title;	        /* The person's title w/in his org  */
	char            *role;	        /* The person's role w/in his org   */
	ECardPhoto      *logo;          /* This person's org's logo.        */

	ECardPhoto      *photo;    	/* A photo of the person.           */
	
	ECard           *agent;         /* A person who sereves as this
					   guy's agent/secretary/etc.       */
	

	char            *categories;    /* A list of the categories to which
					   this card belongs.               */
	
	char            *comment;       /* An unstructured comment string.  */

	ECardSound      *sound;
	
	ECardKey        *key;	        /* The person's public key.         */
	ECardTimeZone   *timezn;        /* The person's time zone.          */
	ECardGeoPos     *geopos;        /* The person's long/lat.           */

	char            *mailer;        /* The user's mailer.               */

	char            *uid;	        /* This card's unique identifier.   */
	ECardRev        *rev;	        /* The time this card was last
					   modified.                        */

	ECardList        xtension;
#endif
};

struct _ECardClass {
	GtkObjectClass parent_class;
	GHashTable    *attribute_jump_table;
};


ECard         *e_card_new (char *vcard);
char          *e_card_get_id (ECard *card);
void           e_card_set_id (ECard *card, const gchar *character);
char          *e_card_get_vcard (ECard *card);

void e_card_phone_free (ECardPhone *phone);
ECardPhone *e_card_phone_copy (const ECardPhone *phone);
void e_card_delivery_address_free (ECardDeliveryAddress *addr);
ECardDeliveryAddress *e_card_delivery_address_copy (const ECardDeliveryAddress *addr);


/* Standard Gtk function */
GtkType        e_card_get_type (void);


#if 0
void          e_card_free (ECard *crd);
void          e_card_prop_free (CardProperty prop);
CardProperty  e_card_prop_empty (void);
int           e_card_check_prop (CardProperty prop);
GList        *e_card_load (GList *crdlist, char *fname);
void          e_card_save (ECard *crd, FILE *fp);
char         *e_card_to_vobj_string (ECard *card);
char         *e_card_to_string (ECard *card);

char *e_card_bday_str (ECardDate bday);
char *e_card_timezn_str (ECardTimeZone timezn);
char *e_card_geopos_str (ECardGeoPos geopos);
#endif

#endif /* ! __E_CARD_H__ */
