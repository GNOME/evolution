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
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <libgnome/gnome-defs.h>
#include "e-book.h"

BEGIN_GNOME_DECLS

/* Callbacks for asynchronous functions. */
typedef void (*EBookCommonCallback)      (EBook *book, gpointer closure);
typedef void (*EBookSimpleQueryCallback) (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure);
typedef void (*EBookHaveAddressCallback) (EBook *book, const gchar *addr, ECard *card, gpointer closure);

gboolean e_book_load_local_address_book (EBook *book, 
					 EBookCallback open_response, 
					 gpointer closure);

void     e_book_use_local_address_book  (EBookCommonCallback cb, gpointer closure);

/* Simple Query Interface. */

guint     e_book_simple_query             (EBook                    *book,
					   const char               *query,
					   EBookSimpleQueryCallback  cb,
					   gpointer                  closure);
void      e_book_simple_query_cancel      (EBook                    *book,
					   guint                     tag);

/* Specialized Name/Email Queries */

guint e_book_name_and_email_query (EBook *book,
				   const char *name,
				   const char *email,
				   EBookSimpleQueryCallback cb,
				   gpointer closure);

/* Returns the ECard associated to email in the callback,
   or NULL if no match is found in the local address book. */
void e_book_query_address_locally (const gchar *email,
				   EBookHaveAddressCallback cb,
				   gpointer closure);

END_GNOME_DECLS


#endif /* __E_BOOK_UTIL_H__ */

