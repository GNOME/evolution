/*
 * The Evolution addressbook client object.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 1999, Helix Code, Inc.
 */

#ifndef __E_BOOK_H__
#define __E_BOOK_H__

typedef struct {
	GtkObject     parent;
	EBookPrivate *priv;
} EBook;

typedef struct {
	GtkObjectClass parent;

	/*
	 * Signals.
	 */
	void (card_changed *) (const char *id);
	void (card_removed *) (const char *id);
	void (card_added *)   (const char *id);
} EBookClass;

/* Creating a new addressbook. */
EBook      *e_book_new          (const char *uri);
GtkType     e_book_get_type     (void);

/* Fetching cards and card IDs out of the addressbook. */
ECard      *e_book_get_card     (EBook      *book,
				 const char *id);
GList      *e_book_get_cards    (EBook      *book);
GList      *e_book_get_ids      (EBook      *book);

/* Getting/putting card changes. */
void        e_book_sync_card    (EBook      *book,
				 ECard      *card);
void        e_book_update_card  (EBook      *book,
				 ECard      *card);

/* Adding and deleting cards. */
const char *e_book_add_card     (EBook      *book,
				 ECard      *card);
void        e_book_remove_card  (EBook      *book,
				 const char *id);

/* Typing completion... */
GList      *e_book_complete     (EBook      *book,
				 const char *str);

/* Information about this addresbook. */
char       *e_book_get_name     (EBook      *book);
void        e_book_set_name     (EBook      *book,
				 const char *name);

#endif /* ! __E_BOOK_H__ */
