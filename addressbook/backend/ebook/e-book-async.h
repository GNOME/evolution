/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * The Evolution addressbook client object.
 *
 * Author:
 *   Chris Toshok (toshok@ximian.com)
 *
 * Copyright (C) 2003, Ximian, Inc.
 */

#ifndef __E_BOOK_ASYNC_H__
#define __E_BOOK_ASYNC_H__

#include <glib.h>
#include <glib-object.h>

#include <e-util/e-list.h>
#include <ebook/e-contact.h>
#include <ebook/e-book.h>

G_BEGIN_DECLS

/* Callbacks for asynchronous functions. */
typedef void (*EBookCallback) (EBook *book, EBookStatus status, gpointer closure);
typedef void (*EBookOpenProgressCallback)     (EBook          *book,
					       const char     *status_message,
					       short           percent,
					       gpointer        closure);
typedef void (*EBookIdCallback)       (EBook *book, EBookStatus status, const char *id, gpointer closure);
typedef void (*EBookContactCallback)  (EBook *book, EBookStatus status, EContact *contact, gpointer closure);
typedef void (*EBookContactsCallback) (EBook *book, EBookStatus status, GList *contacts, gpointer closure);
typedef void (*EBookBookViewCallback) (EBook *book, EBookStatus status, EBookView *book_view, gpointer closure);
typedef void (*EBookFieldsCallback)   (EBook *book, EBookStatus status, EList *fields, gpointer closure);
typedef void (*EBookAuthMethodsCallback) (EBook *book, EBookStatus status, EList *auth_methods, gpointer closure);

void      e_book_async_load_uri                 (EBook                 *book,
						 const char            *uri,
						 EBookCallback          open_response,
						 gpointer               closure);

void      e_book_async_get_default_addressbook  (EBookCallback          open_response,
						 gpointer               closure);

void      e_book_async_unload_uri               (EBook                 *book);

guint     e_book_async_get_supported_fields     (EBook                 *book,
						 EBookFieldsCallback    cb,
						 gpointer               closure);

guint     e_book_async_get_supported_auth_methods (EBook                    *book,
						   EBookAuthMethodsCallback  cb,
						   gpointer                  closure);

/* User authentication. */
void      e_book_async_authenticate_user        (EBook                 *book,
						 const char            *user,
						 const char            *passwd,
						 const char            *auth_method,
						 EBookCallback         cb,
						 gpointer              closure);

/* Fetching cards. */
guint     e_book_async_get_contact              (EBook                 *book,
						 const char            *id,
						 EBookContactCallback   cb,
						 gpointer               closure);

guint     e_book_async_get_contacts             (EBook                 *book,
						 const char            *query,
						 EBookContactsCallback  cb,
						 gpointer               closure);

/* Deleting cards. */
gboolean  e_book_async_remove_contact           (EBook                 *book,
						 EContact              *contact,
						 EBookCallback          cb,
						 gpointer               closure);
gboolean  e_book_async_remove_contact_by_id     (EBook                 *book,
						 const char            *id,
						 EBookCallback          cb,
						 gpointer               closure);

gboolean e_book_async_remove_contacts           (EBook                 *book,
						 GList                 *id_list,
						 EBookCallback          cb,
						 gpointer               closure);

/* Adding cards. */
gboolean  e_book_async_add_contact              (EBook                 *book,
						 EContact              *contact,
						 EBookIdCallback        cb,
						 gpointer               closure);

/* Modifying cards. */
gboolean  e_book_async_commit_contact           (EBook                 *book,
						 EContact              *contact,
						 EBookCallback          cb,
						 gpointer               closure);

guint     e_book_async_get_book_view            (EBook                 *book,
						 const gchar           *query, /* XXX this needs to change to an EBookQuery */
						 EBookBookViewCallback  cb,
						 gpointer               closure);

G_END_DECLS

#endif /* ! __E_BOOK_H__ */
