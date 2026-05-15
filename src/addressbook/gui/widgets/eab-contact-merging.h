/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Christopher James Lahey <clahey@ximian.com>
 * SPDX-FileContributor: Chris Toshok <toshok@ximian.com>
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
						 GCancellable *cancellable,
						 EABMergingIdAsyncCallback cb,
						 gpointer closure,
						 gboolean can_add_copy);

gboolean	eab_merging_book_modify_contact	(ESourceRegistry *registry,
						 EBookClient *book_client,
						 EContact *contact,
						 GCancellable *cancellable,
						 EABMergingAsyncCallback cb,
						 gpointer closure);

gboolean	eab_merging_book_find_contact	(ESourceRegistry *registry,
						 EBookClient *book_client,
						 EContact *contact,
						 GCancellable *cancellable,
						 EABMergingContactAsyncCallback cb,
						 gpointer closure);

G_END_DECLS

#endif /* __EAB_CONTACT_MERGING_H__ */
