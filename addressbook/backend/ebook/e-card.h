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
#include <addressbook/backend/ebook/e-card-types.h>
#include <e-util/e-list.h>

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

	char            *file_as;       /* The File As field.               */
	char            *fname;         /* The full name.                   */
	ECardName       *name;          /* The structured name.             */
	EList       *address;  	/* Delivery addresses (ECardDeliveryAddress *) */
	EList       *address_label; /* Delivery address labels
					 * (ECardAddrLabel *)               */

	EList       *phone;         /* Phone numbers (ECardPhone *)     */
	EList       *email;         /* Email addresses (char *)         */
	char            *url;	        /* The person's web page.           */

	ECardDate       *bday;	        /* The person's birthday.           */

	char            *note;


	char            *org;           /* The person's organization.       */
	char            *org_unit;      /* The person's organization unit.  */
	char            *office;        /* The person's office.             */
	char            *role;          /* The person's role w/in his org   */
	char            *title;	        /* The person's title w/in his org  */

	char            *manager;
	char            *assistant;

	char            *nickname;      /* The person's nickname            */
	
	char            *spouse;        /* The person's spouse.             */
	ECardDate       *anniversary;   /* The person's anniversary.        */

	char            *mailer;        /* Mailer                           */

	char            *fburl;         /* Free Busy URL                    */

	gint             timezone;      /* number of minutes from UTC as an int */

	EList       *categories;    /* Categories.                      */

	EList       *arbitrary;     /* Arbitrary fields.                */

	guint32         pilot_id;       /* id of the corresponding pilot */
	guint32         pilot_status;   /* status information */
#if 0
	ECardPhoto      *logo;          /* This person's org's logo.        */

	ECardPhoto      *photo;    	/* A photo of the person.           */
	
	ECard           *agent;         /* A person who sereves as this
					   guy's agent/secretary/etc.       */
	

	char            *categories;    /* A list of the categories to which
					   this card belongs.               */
	
	ECardSound      *sound;
	
	ECardKey        *key;	        /* The person's public key.         */
	ECardTimeZone   *timezn;        /* The person's time zone.          */
	ECardGeoPos     *geopos;        /* The person's long/lat.           */

	ECardRev        *rev;	        /* The time this card was last
					   modified.                        */

	EList        xtension;
#endif
};

struct _ECardClass {
	GtkObjectClass parent_class;
	GHashTable    *attribute_jump_table;
};


ECard         *e_card_new       (      char  *vcard);
char          *e_card_get_id    (      ECard *card);
void           e_card_set_id    (      ECard *card,
				 const char  *character);
char          *e_card_get_vcard (      ECard *card);
ECard         *e_card_duplicate (      ECard *card);

ECardPhone *e_card_phone_new  (void);
ECardPhone *e_card_phone_copy (const ECardPhone *phone);
void        e_card_phone_free (      ECardPhone *phone);

ECardDeliveryAddress *e_card_delivery_address_new         (void);
ECardDeliveryAddress *e_card_delivery_address_copy        (const ECardDeliveryAddress *addr);
void                  e_card_delivery_address_free        (      ECardDeliveryAddress *addr);
gboolean              e_card_delivery_address_is_empty    (const ECardDeliveryAddress *addr);
char                 *e_card_delivery_address_to_string   (const ECardDeliveryAddress *addr);
ECardDeliveryAddress *e_card_delivery_address_from_label  (const ECardAddrLabel *label);

ECardAddrLabel *e_card_address_label_new  (void);
ECardAddrLabel *e_card_address_label_copy (const ECardAddrLabel *addr);
void            e_card_address_label_free (      ECardAddrLabel *addr);

ECardName *e_card_name_new         (void);
ECardName *e_card_name_copy        (const ECardName *name);
void       e_card_name_free        (      ECardName *name);
char      *e_card_name_to_string   (const ECardName *name);
ECardName *e_card_name_from_string (const char      *full_name);

ECardArbitrary *e_card_arbitrary_new  (void);
ECardArbitrary *e_card_arbitrary_copy (const ECardArbitrary *arbitrary);
void            e_card_arbitrary_free (      ECardArbitrary *arbitrary);

GList *e_card_load_cards_from_file(const char *filename);

/* Standard Gtk function */
GtkType        e_card_get_type (void);

#endif /* ! __E_CARD_H__ */
