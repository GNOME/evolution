/*
 * Authors:
 *   Arturo Espinosa
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#ifndef __E_CARD_H__
#define __E_CARD_H__

#include <time.h>
#include <glib.h>
#include <stdio.h>
#include <e-card-types.h>

typedef struct _ECard ECard;

struct _ECard {

	char            *fname;         /* The full name.                   */
	ECardName       *name;          /* The structured name.             */

	GList           *del_addrs;  	/* Delivery addresses (ECardAddr *) */
	GList           *del_labels;    /* Delivery address labels
					 * (ECardAddrLabel *)               */
	GList           *phone;         /* Phone numbers (ECardPhone *)     */
	GList           *email;         /* Email addresses (char *)         */
	char            *url;	        /* The person's web page.           */
	
	ECardDate       *bday;	        /* The person's birthday.           */

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

	CardList        xtension;
};

Card         *card_new (void);
void          card_free (Card *crd);
void          card_prop_free (CardProperty prop);
CardProperty  card_prop_empty (void);
int           card_check_prop (CardProperty prop);
GList        *card_load (GList *crdlist, char *fname);
void          card_save (Card *crd, FILE *fp);
char         *card_to_vobj_string (Card *card);
char         *card_to_string (Card *card);

char *card_bday_str (CardBDay bday);
char *card_timezn_str (CardTimeZone timezn);
char *card_geopos_str (CardGeoPos geopos);

#endif /* ! __E_CARD_H__ */
