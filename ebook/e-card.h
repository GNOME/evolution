/*
 * The Evolution addressbook card object.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 1999, Helix Code, Inc.
 */

#ifndef __E_CARD_H__
#define __E_CARD_H__

#include <ebook/e-card-fields.h>

typedef enum {
} ECardDirtyFlags;

typedef struct _ECardPrivate ECardPrivate;

typedef struct {
	GtkObject     parent;
	ECardPrivate *priv;
} ECard;

typedef struct {
	GtkObjectClass parent;

	/*
	 * Signals.
	 */
	void (changed *) (ECardDirtyFlags dirty);
} ECardClass;


ECard        *e_card_new             (void);
GtkType       e_card_get_type        (void);

/* Name */
char         *e_card_get_full_name   (ECard *card);

/* Email */
GList        *e_card_get_emails      (ECard *card);
ECardEmail   *e_card_get_email       (ECard *card);

/* Snail mail */
GList        *e_card_get_addresses   (ECard *card);
ECardAddress *e_card_get_address     (ECard *card);

/* Telephone */
GList        *e_card_get_phones      (ECard *card);
ECardPhone   *e_card_get_phone       (Ecard *card);

/* Title, position, groups */
char         *e_card_get_title       (ECard *card);
GList        *e_card_get_categories  (ECard *card);

/* Home page, other URLs associated with this person */
GList        *e_card_get_urls        (ECard *card);
ECardURL     *e_card_get_url         (ECard *card);

#endif /* ! __E_CARD_H__ */
