/* e-name-selector-model.h - Model for contact selection.
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

#ifndef E_NAME_SELECTOR_MODEL_H
#define E_NAME_SELECTOR_MODEL_H

#include <e-util/e-tree-model-generator.h>
#include <e-util/e-contact-store.h>
#include <e-util/e-destination-store.h>

/* Standard GObject macros */
#define E_TYPE_NAME_SELECTOR_MODEL \
	(e_name_selector_model_get_type ())
#define E_NAME_SELECTOR_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_NAME_SELECTOR_MODEL, ENameSelectorModel))
#define E_NAME_SELECTOR_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_NAME_SELECTOR_MODEL, ENameSelectorModelClass))
#define E_IS_NAME_SELECTOR_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_NAME_SELECTOR_MODEL))
#define E_IS_NAME_SELECTOR_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_NAME_SELECTOR_MODEL))
#define E_NAME_SELECTOR_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_NAME_SELECTOR_MODEL, ENameSelectorModelClass))

G_BEGIN_DECLS

typedef struct _ENameSelectorModel ENameSelectorModel;
typedef struct _ENameSelectorModelClass ENameSelectorModelClass;
typedef struct _ENameSelectorModelPrivate ENameSelectorModelPrivate;

struct _ENameSelectorModel {
	GObject parent;
	ENameSelectorModelPrivate *priv;
};

struct _ENameSelectorModelClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*section_added)	(gchar *name);
	void		(*section_removed)	(gchar *name);
};

GType		e_name_selector_model_get_type	(void) G_GNUC_CONST;
ENameSelectorModel *
		e_name_selector_model_new	(void);
EContactStore *	e_name_selector_model_peek_contact_store
						(ENameSelectorModel *name_selector_model);
ETreeModelGenerator *
		e_name_selector_model_peek_contact_filter
						(ENameSelectorModel *name_selector_model);

/* Deep copy of section names; free strings and list when you're done */
GList *		e_name_selector_model_list_sections
						(ENameSelectorModel *name_selector_model);

/* pretty_name will be newly allocated, but destination_store must be reffed if you keep it */
gboolean	e_name_selector_model_peek_section
						(ENameSelectorModel *name_selector_model,
						 const gchar *name,
						 gchar **pretty_name,
						 EDestinationStore **destination_store);
void		e_name_selector_model_add_section
						(ENameSelectorModel *name_selector_model,
						 const gchar *name,
						 const gchar *pretty_name,
						 EDestinationStore *destination_store);
void		e_name_selector_model_remove_section
						(ENameSelectorModel *name_selector_model,
						 const gchar *name);
GList *		e_name_selector_model_get_contact_emails_without_used
						(ENameSelectorModel *name_selector_model,
						 EContact *contact,
						 gboolean remove_used);
void		e_name_selector_model_free_emails_list
						(GList *email_list);

G_END_DECLS

#endif /* E_NAME_SELECTOR_MODEL_H */
