/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Arturo Espinosa
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#ifndef __E_CARD_H__
#define __E_CARD_H__

#include <time.h>
#include <gtk/gtkobject.h>
#include <stdio.h>
#include <ebook/e-card-types.h>
#include <e-util/e-list.h>

#define E_TYPE_CARD            (e_card_get_type ())
#define E_CARD(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_CARD, ECard))
#define E_CARD_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_CARD, ECardClass))
#define E_IS_CARD(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_CARD))
#define E_IS_CARD_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_CARD))

typedef struct _ECard ECard;
typedef struct _ECardClass ECardClass;

struct _EBook; /* Forward reference */

struct _ECard {
	GtkObject object;
	char *id;

	struct _EBook   *book;          /* The EBook this card is from.     */

	char            *file_as;       /* The File As field.               */
	char            *fname;         /* The full name.                   */
	ECardName       *name;          /* The structured name.             */
	EList           *address;  	/* Delivery addresses (ECardDeliveryAddress *) */
	EList           *address_label; /* Delivery address labels
					 * (ECardAddrLabel *)               */

	EList           *phone;         /* Phone numbers (ECardPhone *)     */
	EList           *email;         /* Email addresses (char *)         */
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

	char            *caluri;        /* Calendar URI                     */
	char            *fburl;         /* Free Busy URL                    */
	char            *icscalendar;   /* Default server calendar          */

	gint             timezone;      /* number of minutes from UTC as an int */

	ECardDate       *last_use;
	float            raw_use_score;

	char            *related_contacts;  /* EDestinationV (serialized) of related contacts. */

	EList           *categories;    /* Categories.                      */

	EList           *arbitrary;     /* Arbitrary fields.                */

	

	guint32         wants_html : 1;     /* Wants html mail. */
	guint32         wants_html_set : 1; /* Wants html mail. */
	guint32		list : 1; /* If the card corresponds to a contact list */
	guint32		list_show_addresses : 1; /* Whether to show the addresses
						    in the To: or Bcc: field */

#if 0
	ECardPhoto      *logo;          /* This person's org's logo.        */

	ECardPhoto      *photo;    	/* A photo of the person.           */
	
	ECard           *agent;         /* A person who sereves as this
					   guy's agent/secretary/etc.       */

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


/* Simple functions */
ECard                *e_card_new                                          (char                       *vcard); /* Assumes utf8 */
ECard                *e_card_new_with_default_charset                     (char                       *vcard,
									   char                       *default_charset);
const char           *e_card_get_id                                       (ECard                      *card);
void                  e_card_set_id                                       (ECard                      *card,
									   const char                 *character);

struct _EBook        *e_card_get_book                                     (ECard                      *card);
void                  e_card_set_book                                     (ECard                      *card,
									   struct _EBook              *book);
char                 *e_card_get_vcard                                    (ECard                      *card);
char                 *e_card_get_vcard_assume_utf8                        (ECard                      *card);
char                 *e_card_list_get_vcard                               (const GList                *list);
ECard                *e_card_duplicate                                    (ECard                      *card);
float                 e_card_get_use_score                                (ECard                      *card);
void                  e_card_touch                                        (ECard                      *card);

/* Evolution List convenience functions */
/*   used for encoding uids in email addresses */
gboolean              e_card_evolution_list                               (ECard                      *card);
gboolean              e_card_evolution_list_show_addresses                (ECard                      *card);

/* ECardPhone manipulation */
ECardPhone           *e_card_phone_new                                    (void);
ECardPhone           *e_card_phone_copy                                   (const ECardPhone           *phone);
ECardPhone           *e_card_phone_ref                                    (const ECardPhone           *phone);
void                  e_card_phone_unref                                  (ECardPhone                 *phone);

/* ECardDeliveryAddress manipulation */
ECardDeliveryAddress *e_card_delivery_address_new                         (void);
ECardDeliveryAddress *e_card_delivery_address_copy                        (const ECardDeliveryAddress *addr);
ECardDeliveryAddress *e_card_delivery_address_ref                         (const ECardDeliveryAddress *addr);
void                  e_card_delivery_address_unref                       (ECardDeliveryAddress       *addr);
gboolean              e_card_delivery_address_is_empty                    (const ECardDeliveryAddress *addr);
char                 *e_card_delivery_address_to_string                   (const ECardDeliveryAddress *addr);
ECardDeliveryAddress *e_card_delivery_address_from_label                  (const ECardAddrLabel       *label);
ECardAddrLabel       *e_card_delivery_address_to_label                    (const ECardDeliveryAddress *addr);

/* ECardAddrLabel manipulation */
ECardAddrLabel       *e_card_address_label_new                            (void);
ECardAddrLabel       *e_card_address_label_copy                           (const ECardAddrLabel       *addr);
ECardAddrLabel       *e_card_address_label_ref                            (const ECardAddrLabel       *addr);
void                  e_card_address_label_unref                          (ECardAddrLabel             *addr);

/* ECardName manipulation */
ECardName            *e_card_name_new                                     (void);
ECardName            *e_card_name_copy                                    (const ECardName            *name);
ECardName            *e_card_name_ref                                     (const ECardName            *name);
void                  e_card_name_unref                                   (ECardName                  *name);
char                 *e_card_name_to_string                               (const ECardName            *name);
ECardName            *e_card_name_from_string                             (const char                 *full_name);

/* ECardDate */
ECardDate             e_card_date_from_string                             (const gchar                *str);
gchar                *e_card_date_to_string                               (ECardDate                  *dt);

/* ECardArbitrary manipulation */
ECardArbitrary       *e_card_arbitrary_new                                (void);
ECardArbitrary       *e_card_arbitrary_copy                               (const ECardArbitrary       *arbitrary);
ECardArbitrary       *e_card_arbitrary_ref                                (const ECardArbitrary       *arbitrary);
void                  e_card_arbitrary_unref                              (ECardArbitrary             *arbitrary);

/* ECard email manipulation */
gboolean              e_card_email_match_string                           (const ECard                *card,
									   const gchar                *str);
gint                  e_card_email_find_number                            (const ECard                *card,
									   const gchar                *email);

/* Specialized functionality */
GList                *e_card_load_cards_from_file                         (const char                 *filename);
GList                *e_card_load_cards_from_file_with_default_charset    (const char                 *filename,
									   char                       *default_charset);
GList                *e_card_load_cards_from_string                       (const char                 *str);
GList                *e_card_load_cards_from_string_with_default_charset  (const char                 *str,
									   char                       *default_charset);
void                  e_card_free_empty_lists                             (ECard                      *card);

enum _ECardDisposition {
	E_CARD_DISPOSITION_AS_ATTACHMENT,
	E_CARD_DISPOSITION_AS_TO,
};
typedef enum _ECardDisposition ECardDisposition;
void                  e_card_send                           (ECard                      *card,
							     ECardDisposition            disposition);
void                  e_card_list_send                      (GList                      *cards,
							     ECardDisposition            disposition);

/* Getting ECards via their URIs */
typedef void (*ECardCallback) (ECard *card, gpointer closure);
void                  e_card_load_uri                       (const gchar                *book_uri,
							     const gchar                *uid,
							     ECardCallback               cb,
							     gpointer                    closure);


/* Standard Gtk function */
GtkType               e_card_get_type                       (void);

#endif /* ! __E_CARD_H__ */
