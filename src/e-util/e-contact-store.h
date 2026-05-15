/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Hans Petter Jansson <hpj@novell.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CONTACT_STORE_H
#define E_CONTACT_STORE_H

#include <gtk/gtk.h>
#include <libebook/libebook.h>

/* Standard GObject macros */
#define E_TYPE_CONTACT_STORE \
	(e_contact_store_get_type ())
#define E_CONTACT_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACT_STORE, EContactStore))
#define E_CONTACT_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTACT_STORE, EContactStoreClass))
#define E_IS_CONTACT_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACT_STORE))
#define E_IS_CONTACT_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONTACT_STORE))
#define E_CONTACT_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONTACT_STORE, EContactStoreClass))

G_BEGIN_DECLS

typedef struct _EContactStore EContactStore;
typedef struct _EContactStoreClass EContactStoreClass;
typedef struct _EContactStorePrivate EContactStorePrivate;

struct _EContactStore {
	GObject parent;
	EContactStorePrivate *priv;
};

struct _EContactStoreClass {
	GObjectClass parent_class;

	/* signals */
	void		(*start_client_view)	(EContactStore *contact_store,
						 EBookClientView *client_view);
	void		(*stop_client_view)	(EContactStore *contact_store,
						 EBookClientView *client_view);
	void		(*start_update)		(EContactStore *contact_store,
						 EBookClientView *client_view);
	void		(*stop_update)		(EContactStore *contact_store,
						 EBookClientView *client_view);
};

GType		e_contact_store_get_type	(void) G_GNUC_CONST;
EContactStore *	e_contact_store_new		(void);

EBookClient *	e_contact_store_get_client	(EContactStore *contact_store,
						 GtkTreeIter *iter);
EContact *	e_contact_store_get_contact	(EContactStore *contact_store,
						 GtkTreeIter *iter);
gboolean	e_contact_store_find_contact	(EContactStore *contact_store,
						 const gchar *uid,
						 GtkTreeIter *iter);

/* Returns a shallow copy; free the list when done, but don't unref elements */
GSList *	e_contact_store_get_clients	(EContactStore *contact_store);
void		e_contact_store_add_client	(EContactStore *contact_store,
						 EBookClient *book_client);
gboolean	e_contact_store_remove_client	(EContactStore *contact_store,
						 EBookClient *book_client);
void		e_contact_store_set_query	(EContactStore *contact_store,
						 EBookQuery *book_query);
EBookQuery *	e_contact_store_peek_query	(EContactStore *contact_store);

G_END_DECLS

#endif  /* E_CONTACT_STORE_H */
