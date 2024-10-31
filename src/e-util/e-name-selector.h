/* e-name-selector.h - Unified context for contact/destination selection UI.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Authors: Hans Petter Jansson <hpj@novell.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_NAME_SELECTOR_H
#define E_NAME_SELECTOR_H

#include <e-util/e-client-cache.h>
#include <e-util/e-name-selector-model.h>
#include <e-util/e-name-selector-dialog.h>
#include <e-util/e-name-selector-entry.h>
#include <e-util/e-name-selector-list.h>

/* Standard GObject macros */
#define E_TYPE_NAME_SELECTOR \
	(e_name_selector_get_type ())
#define E_NAME_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_NAME_SELECTOR, ENameSelector))
#define E_NAME_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_NAME_SELECTOR, ENameSelectorClass))
#define E_IS_NAME_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_NAME_SELECTOR))
#define E_IS_NAME_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_NAME_SELECTOR))
#define E_NAME_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_NAME_SELECTOR, ENameSelectorClass))

G_BEGIN_DECLS

typedef struct _ENameSelector ENameSelector;
typedef struct _ENameSelectorClass ENameSelectorClass;
typedef struct _ENameSelectorPrivate ENameSelectorPrivate;

struct _ENameSelector {
	GObject parent;
	ENameSelectorPrivate *priv;
};

struct _ENameSelectorClass {
	GObjectClass parent_class;
};

GType		e_name_selector_get_type	(void) G_GNUC_CONST;
ENameSelector *	e_name_selector_new		(EClientCache *client_cache);
EClientCache *	e_name_selector_ref_client_cache
						(ENameSelector *name_selector);
ENameSelectorModel *
		e_name_selector_peek_model	(ENameSelector *name_selector);
ENameSelectorDialog *
		e_name_selector_peek_dialog	(ENameSelector *name_selector);
ENameSelectorEntry *
		e_name_selector_peek_section_entry
						(ENameSelector *name_selector,
						 const gchar *name);
ENameSelectorList *
		e_name_selector_peek_section_list
						(ENameSelector *name_selector,
						 const gchar *name);
void		e_name_selector_show_dialog	(ENameSelector *name_selector,
						 GtkWidget *for_transient_widget);
void		e_name_selector_load_books	(ENameSelector *name_selector);
void		e_name_selector_cancel_loading	(ENameSelector *name_selector);

G_END_DECLS

#endif /* E_NAME_SELECTOR_H */
