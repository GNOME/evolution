/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * The Evolution addressbook client object.
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 1999, 2000, Ximian, Inc.
 */

#ifndef __E_BOOK_H__
#define __E_BOOK_H__

#include <glib.h>
#include <glib-object.h>

#include <ebook/e-card.h>
#include <ebook/e-card-cursor.h>
#include <ebook/e-book-view.h>
#include <ebook/e-book-types.h>

#define E_TYPE_BOOK        (e_book_get_type ())
#define E_BOOK(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_BOOK, EBook))
#define E_BOOK_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_BOOK, EBookClass))
#define E_IS_BOOK(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_BOOK))
#define E_IS_BOOK_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_BOOK))
#define E_BOOK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_BOOK, EBookClass))

G_BEGIN_DECLS

typedef struct _EBook        EBook;
typedef struct _EBookClass   EBookClass;
typedef struct _EBookPrivate EBookPrivate;

struct _EBook {
	GObject       parent;
	EBookPrivate *priv;
};

struct _EBookClass {
	GObjectClass parent;

	/*
	 * Signals.
	 */
	void (* open_progress)   (EBook *book, const char *msg, short percent);
	void (* link_status)     (EBook *book, gboolean connected);
	void (* writable_status) (EBook *book, gboolean writable);
	void (* backend_died)    (EBook *book);
};

/* Callbacks for asynchronous functions. */
typedef void (*EBookCallback) (EBook *book, EBookStatus status, gpointer closure);
typedef void (*EBookOpenProgressCallback)     (EBook          *book,
					       const char     *status_message,
					       short           percent,
					       gpointer        closure);
typedef void (*EBookIdCallback)       (EBook *book, EBookStatus status, const char *id, gpointer closure);
typedef void (*EBookCardCallback)     (EBook *book, EBookStatus status, ECard *card, gpointer closure);
typedef void (*EBookCursorCallback)   (EBook *book, EBookStatus status, ECardCursor *cursor, gpointer closure);
typedef void (*EBookBookViewCallback) (EBook *book, EBookStatus status, EBookView *book_view, gpointer closure);
typedef void (*EBookFieldsCallback)   (EBook *book, EBookStatus status, EList *fields, gpointer closure);
typedef void (*EBookAuthMethodsCallback) (EBook *book, EBookStatus status, EList *auth_methods, gpointer closure);

/* Creating a new addressbook. */
EBook    *e_book_new                      (void);

void      e_book_load_uri                 (EBook                 *book,
					   const char            *uri,
					   EBookCallback          open_response,
					   gpointer               closure);
void      e_book_unload_uri               (EBook                 *book);

const char *e_book_get_uri                (EBook                 *book);

char     *e_book_get_static_capabilities  (EBook                 *book);
gboolean  e_book_check_static_capability  (EBook                 *book, const char *cap);

guint     e_book_get_supported_fields     (EBook                 *book,
					   EBookFieldsCallback    cb,
					   gpointer               closure);

guint     e_book_get_supported_auth_methods (EBook                    *book,
					     EBookAuthMethodsCallback  cb,
					     gpointer                  closure);

/* User authentication. */
void      e_book_authenticate_user        (EBook                 *book,
					   const char            *user,
					   const char            *passwd,
					   const char            *auth_method,
					   EBookCallback         cb,
					   gpointer              closure);

/* Fetching cards. */
guint     e_book_get_card                 (EBook                 *book,
					   const char            *id,
					   EBookCardCallback      cb,
					   gpointer               closure);

/* Deleting cards. */
gboolean  e_book_remove_card              (EBook                 *book,
					   ECard                 *card,
					   EBookCallback          cb,
					   gpointer               closure);
gboolean  e_book_remove_card_by_id        (EBook                 *book,
					   const char            *id,
					   EBookCallback          cb,
					   gpointer               closure);

gboolean e_book_remove_cards              (EBook                 *book,
					   GList                 *id_list,
					   EBookCallback          cb,
					   gpointer               closure);

/* Adding cards. */
gboolean  e_book_add_card                 (EBook                 *book,
					   ECard                 *card,
					   EBookIdCallback        cb,
					   gpointer               closure);
gboolean  e_book_add_vcard                (EBook                 *book,
					   const char            *vcard,
					   EBookIdCallback        cb,
					   gpointer               closure);

/* Modifying cards. */
gboolean  e_book_commit_card              (EBook                 *book,
					   ECard                 *card,
					   EBookCallback          cb,
					   gpointer               closure);
gboolean  e_book_commit_vcard             (EBook                 *book,
					   const char            *vcard,
					   EBookCallback          cb,
					   gpointer               closure);

/* Checking to see if we're connected to the card repository. */
gboolean  e_book_check_connection         (EBook                 *book);
guint     e_book_get_cursor               (EBook                 *book,
					   char                  *query,
					   EBookCursorCallback    cb,
					   gpointer               closure);

guint     e_book_get_book_view            (EBook                 *book,
					   const gchar           *query,
					   EBookBookViewCallback  cb,
					   gpointer               closure);

guint     e_book_get_completion_view      (EBook                 *book,
					   const gchar           *query,
					   EBookBookViewCallback  cb,
					   gpointer               closure);

guint     e_book_get_changes              (EBook                 *book,
					   char                  *changeid,
					   EBookBookViewCallback  cb,
					   gpointer               closure);

/* Cancel a pending operation. */
void      e_book_cancel                   (EBook                 *book,
					   guint                  tag);


/* Getting the name of the repository. */
char     *e_book_get_name                 (EBook                 *book);

GType     e_book_get_type                 (void);

G_END_DECLS

#endif /* ! __E_BOOK_H__ */
