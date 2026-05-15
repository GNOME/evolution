/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Hans Petter Jansson <hpj@novell.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_NAME_SELECTOR_ENTRY_H
#define E_NAME_SELECTOR_ENTRY_H

#include <gtk/gtk.h>
#include <libebook/libebook.h>

#include <e-util/e-client-cache.h>
#include <e-util/e-contact-store.h>
#include <e-util/e-destination-store.h>
#include <e-util/e-tree-model-generator.h>

/* Standard GObject macros */
#define E_TYPE_NAME_SELECTOR_ENTRY \
	(e_name_selector_entry_get_type ())
#define E_NAME_SELECTOR_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_NAME_SELECTOR_ENTRY, ENameSelectorEntry))
#define E_NAME_SELECTOR_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_NAME_SELECTOR_ENTRY, ENameSelectorEntryClass))
#define E_IS_NAME_SELECTOR_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_NAME_SELECTOR_ENTRY))
#define E_IS_NAME_SELECTOR_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_NAME_SELECTOR_ENTRY))
#define E_NAME_SELECTOR_ENTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_NAME_SELECTOR_ENTRY, ENameSelectorEntryClass))

G_BEGIN_DECLS

typedef struct _ENameSelectorEntry ENameSelectorEntry;
typedef struct _ENameSelectorEntryClass ENameSelectorEntryClass;
typedef struct _ENameSelectorEntryPrivate ENameSelectorEntryPrivate;

struct _ENameSelectorEntry {
	GtkEntry parent;
	ENameSelectorEntryPrivate *priv;
};

struct _ENameSelectorEntryClass {
	GtkEntryClass parent_class;

	void (*updated) (ENameSelectorEntry *entry, gchar *email);

	gpointer reserved1;
	gpointer reserved2;
};

GType		e_name_selector_entry_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_name_selector_entry_new	(EClientCache *client_cache);
EClientCache *	e_name_selector_entry_ref_client_cache
						(ENameSelectorEntry *name_selector_entry);
void		e_name_selector_entry_set_client_cache
						(ENameSelectorEntry *name_selector_entry,
						 EClientCache *client_cache);
gint		e_name_selector_entry_get_minimum_query_length
						(ENameSelectorEntry *name_selector_entry);
void		e_name_selector_entry_set_minimum_query_length
						(ENameSelectorEntry *name_selector_entry,
						 gint length);
gboolean	e_name_selector_entry_get_show_address
						(ENameSelectorEntry *name_selector_entry);
void		e_name_selector_entry_set_show_address
						(ENameSelectorEntry *name_selector_entry,
						 gboolean show);
EContactStore *	e_name_selector_entry_peek_contact_store
						(ENameSelectorEntry *name_selector_entry);
void		e_name_selector_entry_set_contact_store
						(ENameSelectorEntry *name_selector_entry,
						 EContactStore *contact_store);
EDestinationStore *
		e_name_selector_entry_peek_destination_store
						(ENameSelectorEntry *name_selector_entry);
void		e_name_selector_entry_set_destination_store
						(ENameSelectorEntry *name_selector_entry,
						 EDestinationStore *destination_store);
EDestination *	e_name_selector_entry_get_popup_destination
						(ENameSelectorEntry *name_selector_entry);

/* TEMPORARY API - DO NOT USE */
void		e_name_selector_entry_set_contact_editor_func
						(ENameSelectorEntry *name_selector_entry,
						 gpointer func);
void		e_name_selector_entry_set_contact_list_editor_func
						(ENameSelectorEntry *name_selector_entry,
						 gpointer func);

G_END_DECLS

#endif /* E_NAME_SELECTOR_ENTRY_H */
