/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jon Trowbridge <trow@ximian.com>
 *      Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EAB_UTIL_H__
#define __EAB_UTIL_H__

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-moniker-util.h>
#include <libebook/e-book.h>

G_BEGIN_DECLS

typedef void (*EABHaveAddressCallback) (EBook *book, const gchar *addr, EContact *contact, gpointer closure);

/* Specialized Name/Email Queries */
guint                  eab_name_and_email_query      (EBook                    *book,
						      const gchar               *name,
						      const gchar               *email,
						      EBookListCallback         cb,
						      gpointer                  closure);
guint                  eab_nickname_query            (EBook                    *book,
						      const gchar               *nickname,
						      EBookListCallback         cb,
						      gpointer                  closure);

GList                 *eab_contact_list_from_string (const gchar *str);
gchar                  *eab_contact_list_to_string    (GList *contacts);

gboolean               eab_book_and_contact_list_from_string (const gchar *str, EBook **book, GList **contacts);
gchar                  *eab_book_and_contact_list_to_string   (EBook *book, GList *contacts);

/* Returns the EContact associated to email in the callback,
   or NULL if no match is found in the default address book. */
void                   eab_query_address_default     (const gchar              *email,
						      EABHaveAddressCallback  cb,
						      gpointer                  closure);

gint                    e_utf8_casefold_collate_len (const gchar *str1, const gchar *str2, gint len);
gint                    e_utf8_casefold_collate (const gchar *str1, const gchar *str2);

G_END_DECLS

#endif /* __EAB_UTIL_H__ */

