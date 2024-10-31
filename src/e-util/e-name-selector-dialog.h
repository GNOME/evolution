/* e-name-selector-dialog.c - Dialog that lets user pick EDestinations.
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

#ifndef E_NAME_SELECTOR_DIALOG_H
#define E_NAME_SELECTOR_DIALOG_H

#include <gtk/gtk.h>

#include <e-util/e-client-cache.h>
#include <e-util/e-contact-store.h>
#include <e-util/e-name-selector-model.h>

/* Standard GObject macros */
#define E_TYPE_NAME_SELECTOR_DIALOG \
	(e_name_selector_dialog_get_type ())
#define E_NAME_SELECTOR_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_NAME_SELECTOR_DIALOG, ENameSelectorDialog))
#define E_NAME_SELECTOR_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_NAME_SELECTOR_DIALOG, ENameSelectorDialogClass))
#define E_IS_NAME_SELECTOR_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	(obj, E_TYPE_NAME_SELECTOR_DIALOG))
#define E_IS_NAME_SELECTOR_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_NAME_SELECTOR_DIALOG))
#define E_NAME_SELECTOR_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_NAME_SELECTOR_DIALOG, ENameSelectorDialogClass))

G_BEGIN_DECLS

typedef struct _ENameSelectorDialog ENameSelectorDialog;
typedef struct _ENameSelectorDialogClass ENameSelectorDialogClass;
typedef struct _ENameSelectorDialogPrivate ENameSelectorDialogPrivate;

struct _ENameSelectorDialog {
	GtkDialog parent;
	ENameSelectorDialogPrivate *priv;
};

struct _ENameSelectorDialogClass {
	GtkDialogClass parent_class;
};

GType		e_name_selector_dialog_get_type	(void) G_GNUC_CONST;
ENameSelectorDialog *
		e_name_selector_dialog_new	(EClientCache *client_cache);
EClientCache *	e_name_selector_dialog_ref_client_cache
						(ENameSelectorDialog *name_selector_dialog);
ENameSelectorModel *
		e_name_selector_dialog_peek_model
						(ENameSelectorDialog *name_selector_dialog);
void		e_name_selector_dialog_set_model
						(ENameSelectorDialog *name_selector_dialog,
						 ENameSelectorModel  *model);
void		e_name_selector_dialog_set_destination_index
						(ENameSelectorDialog *name_selector_dialog,
						 guint index);
void		e_name_selector_dialog_set_scrolling_policy
						(ENameSelectorDialog *name_selector_dialog,
						 GtkPolicyType hscrollbar_policy,
						 GtkPolicyType vscrollbar_policy);
gboolean	e_name_selector_dialog_get_section_visible
						(ENameSelectorDialog *name_selector_dialog,
						 const gchar *name);
void		e_name_selector_dialog_set_section_visible
						(ENameSelectorDialog *name_selector_dialog,
						 const gchar *name,
						 gboolean visible);

G_END_DECLS

#endif /* E_NAME_SELECTOR_DIALOG_H */
