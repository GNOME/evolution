/*
 * The Evolution addressbook client object.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Christopher James Lahey <clahey@ximian.com>
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_CONTACT_MERGING_H__
#define __E_CONTACT_MERGING_H__

#include <libebook/libebook.h>

G_BEGIN_DECLS

typedef void	(*EABMergingAsyncCallback)	(EBookClient *book_client,
						 const GError *error,
						 gpointer closure);
typedef void	(*EABMergingIdAsyncCallback)	(EBookClient *book_client,
						 const GError *error,
						 const gchar *id,
						 gpointer closure);
typedef void	(*EABMergingContactAsyncCallback)
						(EBookClient *book_client,
						 const GError *error,
						 EContact *contact,
						 gpointer closure);

gboolean	eab_merging_book_add_contact	(ESourceRegistry *registry,
						 EBookClient *book_client,
						 EContact *contact,
						 EABMergingIdAsyncCallback cb,
						 gpointer closure,
						 gboolean can_add_copy);

gboolean	eab_merging_book_modify_contact	(ESourceRegistry *registry,
						 EBookClient *book_client,
						 EContact *contact,
						 EABMergingAsyncCallback cb,
						 gpointer closure);

gboolean	eab_merging_book_find_contact	(ESourceRegistry *registry,
						 EBookClient *book_client,
						 EContact *contact,
						 EABMergingContactAsyncCallback cb,
						 gpointer closure);

G_END_DECLS

#endif /* __EAB_CONTACT_MERGING_H__ */
