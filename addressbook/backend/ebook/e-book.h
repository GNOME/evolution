/*
 * The Evolution addressbook client object.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 1999, 2000, Helix Code, Inc.
 */

#ifndef __E_BOOK_H__
#define __E_BOOK_H__

#include <libgnome/gnome-defs.h>

#include <e-card.h>

BEGIN_GNOME_DECLS

typedef enum {
	E_BOOK_STATUS_SUCCESS,
	E_BOOK_STATUS_UNKNOWN,
	E_BOOK_STATUS_REPOSITORY_OFFLINE,
	E_BOOK_STATUS_PERMISSION_DENIED,
	E_BOOK_STATUS_CARD_NOT_FOUND
} EBookStatus;

typedef struct _EBookPrivate EBookPrivate;

typedef struct {
	GtkObject     parent;
	EBookPrivate *priv;
} EBook;

typedef struct {
	GtkObjectClass parent;

	/*
	 * Signals.
	 */
	void (* card_changed) (const char *id);
	void (* card_removed) (const char *id);
	void (* card_added)   (const char *id);
	void (* link_status)  (gboolean connected);
} EBookClass;

/* Callbacks for asynchronous functions. */
typedef void (*EBookCallback) (EBook *book, EBookStatus status, gpointer closure);
typedef void (*EBookOpenProgressCallback)     (EBook          *book,
					       const char     *status_message,
					       short           percent,
					       gpointer        closure);
						     		      
  
/* Creating a new addressbook. */
EBook   *e_book_new                (const char                *uri,
				    EBookOpenProgressCallback  progress_cb,
				    EBookCallback              open_response,
				    gpointer                   closure);
GtkType  e_book_get_type           (void);

/* Fetching cards. */
ECard   *e_book_get_card           (EBook                     *book,
				    char                      *id);
char    *e_book_get_vcard          (EBook                     *book,
				    char                      *id);

/* Deleting cards. */
void     e_book_remove_card        (EBook                     *book,
				    ECard                     *card,
				    EBookCallback              cb,
				    gpointer                   closure);
void     e_book_remove_card_by_id  (EBook                     *book,
				    char                      *id,
				    EBookCallback              cb,
				    gpointer                   closure);

/* Adding cards. */
void     e_book_add_card           (EBook                     *book,
				    ECard                     *card,
				    EBookCallback              cb,
				    gpointer                   closure);
void     e_book_add_vcard          (EBook                     *book,
				    char                      *vcard,
				    char                      *id,
				    EBookCallback              cb,
				    gpointer                   closure);

/* Modifying cards. */
void     e_book_commit_card        (EBook                     *book,
				    ECard                     *card,
				    EBookCallback              cb,
				    gpointer                   closure);
void     e_book_commit_vcard       (EBook                     *book,
				    char                      *vcard,
				    EBookCallback              cb,
				    gpointer                   closure);

#define E_BOOK_TYPE        (e_book_get_type ())
#define E_BOOK(o)          (GTK_CHECK_CAST ((o), E_BOOK_TYPE, EBook))
#define E_BOOK_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_BOOK_TYPE, EBookClass))
#define E_IS_BOOK(o)       (GTK_CHECK_TYPE ((o), E_BOOK_TYPE))
#define E_IS_BOOK_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_BOOK_TYPE))

END_GNOME_DECLS

#endif /* ! __E_BOOK_H__ */
