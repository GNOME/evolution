/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_ADDRESSBOOK_TABLE_H
#define E_ADDRESSBOOK_TABLE_H

#include <gtk/gtk.h>
#include <libebook/libebook.h>

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_ADDRESSBOOK_TABLE \
	(e_addressbook_table_get_type ())
#define E_ADDRESSBOOK_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ADDRESSBOOK_TABLE, EAddressbookTable))
#define E_ADDRESSBOOK_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ADDRESSBOOK_TABLE, EAddressbookTableClass))
#define E_IS_ADDRESSBOOK_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ADDRESSBOOK_TABLE))
#define E_IS_ADDRESSBOOK_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ADDRESSBOOK_TABLE))
#define E_ADDRESSBOOK_TABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ADDRESSBOOK_TABLE, EAddressbookTableClass))

G_BEGIN_DECLS

typedef struct _EAddressbookTable EAddressbookTable;
typedef struct _EAddressbookTableClass EAddressbookTableClass;
typedef struct _EAddressbookTablePrivate EAddressbookTablePrivate;

struct _EAddressbookTable {
	GtkBox parent;
	EAddressbookTablePrivate *priv;
};

struct _EAddressbookTableClass {
	GtkBoxClass parent_class;

	void		(*status_message)	(EAddressbookTable *table,
						 const gchar *message,
						 gint percentage);
	void		(*count_changed)	(EAddressbookTable *table);

	/* Padding for future expansion */
	gpointer padding[12];
};

GType		e_addressbook_table_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_addressbook_table_new		(void);
EVirtualTree *	e_addressbook_table_get_virtual_tree
						(EAddressbookTable *self);
EBookClient *	e_addressbook_table_get_book_client
						(EAddressbookTable *self);
void		e_addressbook_table_set_book_client
						(EAddressbookTable *self,
						 EBookClient *book_client);
const gchar *	e_addressbook_table_get_query	(EAddressbookTable *self);
void		e_addressbook_table_set_query	(EAddressbookTable *self,
						 const gchar *query);
guint		e_addressbook_table_get_n_total	(EAddressbookTable *self);
gboolean	e_addressbook_table_get_loading	(EAddressbookTable *self);
void		e_addressbook_table_set_search_active
							(EAddressbookTable *self,
							 gboolean search_active);
gboolean	e_addressbook_table_get_search_active
							(EAddressbookTable *self);
void		e_addressbook_table_prefetch_all_contacts
							(EAddressbookTable *self,
							 GCancellable *cancellable,
							 GAsyncReadyCallback callback,
							 gpointer user_data);
gboolean	e_addressbook_table_prefetch_all_contacts_finish
							(EAddressbookTable *self,
							 GAsyncResult *result,
							 GError **error);
guint		e_addressbook_table_get_n_selected
						(EAddressbookTable *self);
void		e_addressbook_table_select_all	(EAddressbookTable *self);
guint		e_addressbook_table_get_cursor_row
						(EAddressbookTable *self);
void		e_addressbook_table_set_cursor_row
						(EAddressbookTable *self,
						 guint row);
EPrintable *	e_addressbook_table_get_printable
						(EAddressbookTable *self);

GPtrArray *	e_addressbook_table_peek_selected_contacts
						(EAddressbookTable *self);
void		e_addressbook_table_dup_selected_contacts
						(EAddressbookTable *self,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
GPtrArray *	e_addressbook_table_dup_selected_contacts_finish
						(EAddressbookTable *self,
						 GAsyncResult *result,
						 GError **error);
const gchar * const *
		e_addressbook_table_get_legacy_etable_column_ids
						(guint *out_n_column_ids);
EContact *	e_addressbook_table_row_ref_contact
						(GObject *row_object);

G_END_DECLS

#endif /* E_ADDRESSBOOK_TABLE_H */
