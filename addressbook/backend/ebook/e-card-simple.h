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

#ifndef __E_CARD_SIMPLE_H__
#define __E_CARD_SIMPLE_H__

#include <time.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <ebook/e-card.h>
#include <ebook/e-card-types.h>
#include <ebook/e-card-list.h>

#define E_TYPE_CARD_SIMPLE            (e_card_simple_get_type ())
#define E_CARD_SIMPLE(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_CARD_SIMPLE, ECardSimple))
#define E_CARD_SIMPLE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_CARD_SIMPLE, ECardSimpleClass))
#define E_IS_CARD_SIMPLE(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_CARD_SIMPLE))
#define E_IS_CARD_SIMPLE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_CARD_SIMPLE))

typedef enum _ECardSimplePhoneId ECardSimplePhoneId;
typedef enum _ECardSimpleEmailId ECardSimpleEmailId;
typedef enum _ECardSimpleAddressId ECardSimpleAddressId;

enum _ECardSimplePhoneId {
	E_CARD_SIMPLE_PHONE_ID_ASSISTANT,
	E_CARD_SIMPLE_PHONE_ID_BUSINESS,
	E_CARD_SIMPLE_PHONE_ID_BUSINESS_2,
	E_CARD_SIMPLE_PHONE_ID_BUSINESS_FAX,
	E_CARD_SIMPLE_PHONE_ID_CALLBACK,
	E_CARD_SIMPLE_PHONE_ID_CAR,
	E_CARD_SIMPLE_PHONE_ID_COMPANY,
	E_CARD_SIMPLE_PHONE_ID_HOME,
	E_CARD_SIMPLE_PHONE_ID_HOME_2,
	E_CARD_SIMPLE_PHONE_ID_HOME_FAX,
	E_CARD_SIMPLE_PHONE_ID_ISDN,
	E_CARD_SIMPLE_PHONE_ID_MOBILE,
	E_CARD_SIMPLE_PHONE_ID_OTHER,
	E_CARD_SIMPLE_PHONE_ID_OTHER_FAX,
	E_CARD_SIMPLE_PHONE_ID_PAGER,
	E_CARD_SIMPLE_PHONE_ID_PRIMARY,
	E_CARD_SIMPLE_PHONE_ID_RADIO,
	E_CARD_SIMPLE_PHONE_ID_TELEX,
	E_CARD_SIMPLE_PHONE_ID_TTYTTD,
	E_CARD_SIMPLE_PHONE_ID_LAST
};

/* We need HOME and WORK email addresses here. */
enum _ECardSimpleEmailId {
	E_CARD_SIMPLE_EMAIL_ID_EMAIL,
	E_CARD_SIMPLE_EMAIL_ID_EMAIL_2,
	E_CARD_SIMPLE_EMAIL_ID_EMAIL_3,
	E_CARD_SIMPLE_EMAIL_ID_LAST
};

/* Should this include (BILLING/SHIPPING)? */
enum _ECardSimpleAddressId {
	E_CARD_SIMPLE_ADDRESS_ID_BUSINESS,
	E_CARD_SIMPLE_ADDRESS_ID_HOME,
	E_CARD_SIMPLE_ADDRESS_ID_OTHER,
	E_CARD_SIMPLE_ADDRESS_ID_LAST
};

typedef struct _ECardSimple ECardSimple;
typedef struct _ECardSimpleClass ECardSimpleClass;

struct _ECardSimple {
	GtkObject object;
	ECard *card;

	ECardPhone *phone[E_CARD_SIMPLE_PHONE_ID_LAST];
	char *email[E_CARD_SIMPLE_EMAIL_ID_LAST];
	ECardAddrLabel *address[E_CARD_SIMPLE_ADDRESS_ID_LAST];
};

struct _ECardSimpleClass {
	GtkObjectClass parent_class;
};
	       
ECardSimple    *e_card_simple_new (ECard *card);
char           *e_card_simple_get_id (ECardSimple *simple);
void           	e_card_simple_set_id (ECardSimple *simple, const gchar *character);
char           *e_card_simple_get_vcard (ECardSimple *simple);
	       
ECardSimple    *e_card_simple_duplicate (ECardSimple *simple);

ECardPhone     *e_card_simple_get_phone   (ECardSimple          *simple,
					   ECardSimplePhoneId    id);
char           *e_card_simple_get_email   (ECardSimple          *simple,
					   ECardSimpleEmailId    id);
ECardAddrLabel *e_card_simple_get_address (ECardSimple          *simple,
					   ECardSimpleAddressId  id);
void            e_card_simple_set_phone   (ECardSimple          *simple,
					   ECardSimplePhoneId    id,
					   ECardPhone           *phone);
void            e_card_simple_set_email   (ECardSimple          *simple,
					   ECardSimpleEmailId    id,
					   char                 *email);
void            e_card_simple_set_address (ECardSimple          *simple,
					   ECardSimpleAddressId  id,
					   ECardAddrLabel       *address);

void            e_card_simple_sync_card   (ECardSimple *simple);

/* Standard Gtk function */
GtkType         e_card_simple_get_type (void);

#endif /* ! __E_CARD_SIMPLE_H__ */
