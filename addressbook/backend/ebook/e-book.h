/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * The Evolution addressbook client object.
 *
 * Author:
 *   Chris Toshok (toshok@ximian.com)
 *
 * Copyright (C) 1999-2003, Ximian, Inc.
 */

#ifndef __E_BOOK_H__
#define __E_BOOK_H__

#include <glib.h>
#include <glib-object.h>

#include <ebook/e-contact.h>
#include <ebook/e-book-query.h>
#include <ebook/e-book-view.h>
#include <ebook/e-book-types.h>
#if notyet
#include <e-util/e-source-list.h>
#endif

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
	void (* writable_status) (EBook *book, gboolean writable);
	void (* backend_died)    (EBook *book);

	/* Padding for future expansion */
	void (*_ebook_reserved0) (void);
	void (*_ebook_reserved1) (void);
	void (*_ebook_reserved2) (void);
	void (*_ebook_reserved3) (void);
	void (*_ebook_reserved4) (void);
};

/* Creating a new addressbook. */
EBook    *e_book_new                       (void);

/* loading arbitrary addressbooks */
gboolean e_book_load_uri                   (EBook       *book,
					    const char  *uri,
					    gboolean     only_if_exists,
					    GError     **error);

gboolean e_book_unload_uri                 (EBook       *book,
					    GError     **error);

gboolean e_book_remove                     (EBook       *book,
					    GError     **error);

/* convenience function for loading the "local" contact folder */
gboolean e_book_load_local_addressbook     (EBook   *book,
					    GError **error);

gboolean e_book_get_supported_fields       (EBook       *book,
					    GList      **fields,
					    GError     **error);

gboolean e_book_get_supported_auth_methods (EBook       *book,
					    GList      **auth_methods,
					    GError     **error);

/* User authentication. */
gboolean e_book_authenticate_user          (EBook       *book,
					    const char  *user,
					    const char  *passwd,
					    const char  *auth_method,
					    GError     **error);

/* Fetching contacts. */
gboolean e_book_get_contact                (EBook       *book,
					    const char  *id,
					    EContact   **contact,
					    GError     **error);

/* Deleting contacts. */
gboolean e_book_remove_contact             (EBook       *book,
					    const char  *id,
					    GError     **error);

gboolean e_book_remove_contacts            (EBook       *book,
					    GList       *id_list,
					    GError     **error);

/* Adding contacts. */
gboolean e_book_add_contact                (EBook       *book,
					    EContact    *contact,
					    GError     **error);

/* Modifying contacts. */
gboolean e_book_commit_contact             (EBook       *book,
					    EContact    *contact,
					    GError     **error);

/* Returns a live view of a query. */
gboolean e_book_get_book_view              (EBook       *book,
					    EBookQuery  *query,
					    GList       *requested_fields,
					    int          max_results,
					    EBookView  **book_view,
					    GError     **error);

/* Returns a static snapshot of a query. */
gboolean e_book_get_contacts               (EBook       *book,
					    EBookQuery  *query,
					    GList      **contacts,
					    GError     **error);

gboolean e_book_get_changes                (EBook       *book,
					    char        *changeid,
					    GList      **changes,
					    GError     **error);

void     e_book_free_change_list           (GList       *change_list);

const char *e_book_get_uri                 (EBook       *book);

const char *e_book_get_static_capabilities (EBook    *book,
					    GError  **error);
gboolean    e_book_check_static_capability (EBook       *book,
					    const char  *cap);
gboolean    e_book_is_writable             (EBook       *book);

/* Cancel a pending operation. */
gboolean    e_book_cancel                  (EBook   *book,
					    GError **error);

/* Identity */
gboolean    e_book_get_self                (EContact **contact, EBook **book, GError **error);
gboolean    e_book_set_self                (EBook   *book, const char *id, GError **error);

/* Addressbook Discovery */
gboolean    e_book_get_default_addressbook (EBook **book, GError **error);
#if notyet
gboolean    e_book_get_addressbooks        (ESourceList** addressbook_sources, GError **error);
#endif

GType        e_book_get_type                  (void);

G_END_DECLS

#endif /* ! __E_BOOK_H__ */
