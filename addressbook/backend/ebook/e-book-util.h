/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-book-util.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#ifndef __E_BOOK_UTIL_H__
#define __E_BOOK_UTIL_H__

#include "e-book.h"
#include "e-util/e-config-listener.h"
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-moniker-util.h>

G_BEGIN_DECLS

/* Callbacks for asynchronous functions. */
typedef void (*EBookCommonCallback)      (EBook *book, gpointer closure);
typedef void (*EBookSimpleQueryCallback) (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure);
typedef void (*EBookHaveAddressCallback) (EBook *book, const gchar *addr, ECard *card, gpointer closure);

/* expand file:///foo/foo/ to file:///foo/foo/addressbook.db */
char                  *e_book_expand_uri                (const char               *uri);

void                   e_book_load_address_book_by_uri  (EBook                    *book,
							 const char               *uri,
							 EBookCallback             open_response,
							 gpointer                  closure);
void                   e_book_use_address_book_by_uri   (const char               *uri,
							 EBookCommonCallback       cb,
							 gpointer                  closure);

void                   e_book_use_default_book          (EBookCommonCallback       cb,
							 gpointer                  closure);
void                   e_book_load_default_book         (EBook                    *book,
							 EBookCallback             open_response,
							 gpointer                  closure);
const char            *e_book_get_default_book_uri      (void);

/* config database interface. */
EConfigListener       *e_book_get_config_database       (void);

/* Simple Query Interface. */
guint                  e_book_simple_query              (EBook                    *book,
							 const char               *query,
							 EBookSimpleQueryCallback  cb,
							 gpointer                  closure);
void                   e_book_simple_query_cancel       (EBook                    *book,
							 guint                     tag);

/* Specialized Name/Email Queries */
guint                  e_book_name_and_email_query      (EBook                    *book,
							 const char               *name,
							 const char               *email,
							 EBookSimpleQueryCallback  cb,
							 gpointer                  closure);
guint                  e_book_nickname_query            (EBook                    *book,
							 const char               *nickname,
							 EBookSimpleQueryCallback  cb,
							 gpointer                  closure);

/* Returns the ECard associated to email in the callback,
   or NULL if no match is found in the default address book. */
void                   e_book_query_address_default     (const gchar              *email,
							 EBookHaveAddressCallback  cb,
							 gpointer                  closure);

int                    e_utf8_casefold_collate_len (const gchar *str1, const gchar *str2, int len);
int                    e_utf8_casefold_collate (const gchar *str1, const gchar *str2);

G_END_DECLS

#endif /* __E_BOOK_UTIL_H__ */

