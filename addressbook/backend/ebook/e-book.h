/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include <ebook/e-card.h>
#include <ebook/e-card-cursor.h>
#include <ebook/e-book-view.h>
#include <ebook/e-book-types.h>

BEGIN_GNOME_DECLS

typedef struct _EBook        EBook;
typedef struct _EBookClass   EBookClass;
typedef struct _EBookPrivate EBookPrivate;

struct _EBook {
	GtkObject     parent;
	EBookPrivate *priv;
};

struct _EBookClass {
	GtkObjectClass parent;

	/*
	 * Signals.
	 */
	void (* open_progress) (EBook *book, const char *msg, short percent);
	void (* link_status)   (EBook *book, gboolean connected);
};

/* Callbacks for asynchronous functions. */
typedef void (*EBookCallback) (EBook *book, EBookStatus status, gpointer closure);
typedef void (*EBookOpenProgressCallback)     (EBook          *book,
					       const char     *status_message,
					       short           percent,
					       gpointer        closure);
typedef void (*EBookIdCallback) (EBook *book, EBookStatus status, const char *id, gpointer closure);
typedef void (*EBookCursorCallback) (EBook *book, EBookStatus status, ECardCursor *cursor, gpointer closure);
typedef void (*EBookBookViewCallback) (EBook *book, EBookStatus status, EBookView *book_view, gpointer closure);


/* Creating a new addressbook. */
EBook    *e_book_new                (void);
gboolean  e_book_load_uri           (EBook         *book,
				     const char    *uri,
				     EBookCallback  open_response,
				     gpointer       closure);
void      e_book_unload_uri         (EBook         *book);

/* Fetching cards. */
ECard    *e_book_get_card           (EBook         *book,
				     const char    *id);
char     *e_book_get_vcard          (EBook         *book,
				     const char    *id);

/* Deleting cards. */
gboolean  e_book_remove_card        (EBook         *book,
				     ECard         *card,
				     EBookCallback  cb,
				     gpointer       closure);
gboolean  e_book_remove_card_by_id  (EBook         *book,
				     const char    *id,
				     EBookCallback  cb,
				     gpointer       closure);

/* Adding cards. */
gboolean  e_book_add_card           (EBook           *book,
				     ECard           *card,
				     EBookIdCallback  cb,
				     gpointer         closure);
gboolean  e_book_add_vcard          (EBook           *book,
				     const char      *vcard,
				     EBookIdCallback  cb,
				     gpointer         closure);

/* Modifying cards. */
gboolean  e_book_commit_card        (EBook         *book,
				     ECard         *card,
				     EBookCallback  cb,
				     gpointer       closure);
gboolean  e_book_commit_vcard       (EBook         *book,
				     const char    *vcard,
				     EBookCallback  cb,
				     gpointer       closure);

/* Checking to see if we're connected to the card repository. */
gboolean  e_book_check_connection   (EBook         *book);

gboolean e_book_get_cursor          (EBook               *book,
				     char                *query,
				     EBookCursorCallback  cb,
				     gpointer             closure);

gboolean e_book_get_book_view       (EBook                 *book,
				     char                  *query,
				     EBookBookViewCallback  cb,
				     gpointer               closure);

/* Getting the name of the repository. */
char     *e_book_get_name           (EBook         *book);

GtkType   e_book_get_type           (void);

#define E_BOOK_TYPE        (e_book_get_type ())
#define E_BOOK(o)          (GTK_CHECK_CAST ((o), E_BOOK_TYPE, EBook))
#define E_BOOK_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_BOOK_TYPE, EBookClass))
#define E_IS_BOOK(o)       (GTK_CHECK_TYPE ((o), E_BOOK_TYPE))
#define E_IS_BOOK_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_BOOK_TYPE))

END_GNOME_DECLS

#endif /* ! __E_BOOK_H__ */
