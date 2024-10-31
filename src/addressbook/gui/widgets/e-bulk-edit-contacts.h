/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_BULK_EDIT_CONTACTS_H
#define E_BULK_EDIT_CONTACTS_H

#include <e-util/e-util.h>
#include <libebook/libebook.h>

/* Standard GObject macros */
#define E_TYPE_BULK_EDIT_CONTACTS \
	(e_bulk_edit_contacts_get_type ())
#define E_BULK_EDIT_CONTACTS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BULK_EDIT_CONTACTS, EBulkEditContacts))
#define E_BULK_EDIT_CONTACTS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_BULK_EDIT_CONTACTS, EBulkEditContactsClass))
#define E_IS_BULK_EDIT_CONTACTS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_BULK_EDIT_CONTACTS))
#define E_IS_BULK_EDIT_CONTACTS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_BULK_EDIT_CONTACTS))
#define E_BULK_EDIT_CONTACTS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_BULK_EDIT_CONTACTS, EBulkEditContactsClass))

G_BEGIN_DECLS

typedef struct _EBulkEditContacts EBulkEditContacts;
typedef struct _EBulkEditContactsClass EBulkEditContactsClass;
typedef struct _EBulkEditContactsPrivate EBulkEditContactsPrivate;

struct _EBulkEditContacts {
	GtkDialog parent;
	EBulkEditContactsPrivate *priv;
};

struct _EBulkEditContactsClass {
	GtkDialogClass parent_class;
};

GType		e_bulk_edit_contacts_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_bulk_edit_contacts_new	(GtkWindow *parent,
						 EBookClient *book_client,
						 GPtrArray *contacts); /* EContact * */

G_END_DECLS

#endif /* E_BULK_EDIT_CONTACTS_H */
