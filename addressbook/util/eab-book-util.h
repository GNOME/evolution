/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-book-util.h
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
 *
 * Authors: Jon Trowbridge <trow@ximian.com>
 *          Chris Toshok <toshok@ximian.com>
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

#ifndef __EAB_UTIL_H__
#define __EAB_UTIL_H__

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-moniker-util.h>
#include <libebook/e-book.h>
#include "e-util/e-config-listener.h"

G_BEGIN_DECLS

typedef void (*EABHaveAddressCallback) (EBook *book, const gchar *addr, EContact *contact, gpointer closure);

/* config database interface. */
EConfigListener       *eab_get_config_database       (void);

/* Specialized Name/Email Queries */
guint                  eab_name_and_email_query      (EBook                    *book,
						      const char               *name,
						      const char               *email,
						      EBookListCallback         cb,
						      gpointer                  closure);
guint                  eab_nickname_query            (EBook                    *book,
						      const char               *nickname,
						      EBookListCallback         cb,
						      gpointer                  closure);

GList                 *eab_contact_list_from_string (const char *str);
char                  *eab_contact_list_to_string    (GList *contacts);

gboolean               eab_book_and_contact_list_from_string (const char *str, EBook **book, GList **contacts);
char                  *eab_book_and_contact_list_to_string   (EBook *book, GList *contacts);

/* Returns the EContact associated to email in the callback,
   or NULL if no match is found in the default address book. */
void                   eab_query_address_default     (const gchar              *email,
						      EABHaveAddressCallback  cb,
						      gpointer                  closure);

int                    e_utf8_casefold_collate_len (const gchar *str1, const gchar *str2, int len);
int                    e_utf8_casefold_collate (const gchar *str1, const gchar *str2);

G_END_DECLS

#endif /* __EAB_UTIL_H__ */

