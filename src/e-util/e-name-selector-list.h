/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Srinivasa Ragavan <sragavan@novell.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_NAME_SELECTOR_LIST_H
#define E_NAME_SELECTOR_LIST_H

#include <gtk/gtk.h>
#include <libebook/libebook.h>

#include <e-util/e-client-cache.h>
#include <e-util/e-contact-store.h>
#include <e-util/e-destination-store.h>
#include <e-util/e-tree-model-generator.h>
#include <e-util/e-name-selector-entry.h>

/* Standard GObject macros */
#define E_TYPE_NAME_SELECTOR_LIST \
	(e_name_selector_list_get_type ())
#define E_NAME_SELECTOR_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_NAME_SELECTOR_LIST, ENameSelectorList))
#define E_NAME_SELECTOR_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_NAME_SELECTOR_LIST, ENameSelectorListClass))
#define E_IS_NAME_SELECTOR_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_NAME_SELECTOR_LIST))
#define E_IS_NAME_SELECTOR_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_NAME_SELECTOR_LIST))
#define E_NAME_SELECTOR_LIST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_NAME_SELECTOR_LIST, ENameSelectorListClass))

G_BEGIN_DECLS

typedef struct _ENameSelectorList ENameSelectorList;
typedef struct _ENameSelectorListClass ENameSelectorListClass;
typedef struct _ENameSelectorListPrivate ENameSelectorListPrivate;

struct _ENameSelectorList {
	ENameSelectorEntry parent;
	ENameSelectorListPrivate *priv;
};

struct _ENameSelectorListClass {
	ENameSelectorEntryClass parent_class;
};

GType		e_name_selector_list_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_name_selector_list_new	(EClientCache *client_cache);
void		e_name_selector_list_expand_clicked
						(ENameSelectorList *list);

G_END_DECLS

#endif /* E_NAME_SELECTOR_LIST_H */
