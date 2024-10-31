/* e-source-selector-dialog.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * published by the Free Software Foundation; either the version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser  General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Rodrigo Moya <rodrigo@novell.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SOURCE_SELECTOR_DIALOG_H
#define E_SOURCE_SELECTOR_DIALOG_H

#include <gtk/gtk.h>
#include <e-util/e-source-selector.h>

/* Standard GObject macros */
#define E_TYPE_SOURCE_SELECTOR_DIALOG \
	(e_source_selector_dialog_get_type ())
#define E_SOURCE_SELECTOR_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SOURCE_SELECTOR_DIALOG, ESourceSelectorDialog))
#define E_SOURCE_SELECTOR_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SOURCE_SELECTOR_DIALOG, ESourceSelectorDialogClass))
#define E_IS_SOURCE_SELECTOR_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SOURCE_SELECTOR_DIALOG))
#define E_IS_SOURCE_SELECTOR_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SOURCE_SELECTOR_DIALOG))
#define E_SOURCE_SELECTOR_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SOURCE_SELECTOR_DIALOG, ESourceSelectorDialogClass))

G_BEGIN_DECLS

typedef struct _ESourceSelectorDialog ESourceSelectorDialog;
typedef struct _ESourceSelectorDialogClass ESourceSelectorDialogClass;
typedef struct _ESourceSelectorDialogPrivate ESourceSelectorDialogPrivate;

struct _ESourceSelectorDialog {
	GtkDialog parent;
	ESourceSelectorDialogPrivate *priv;
};

struct _ESourceSelectorDialogClass {
	GtkDialogClass parent_class;
};

GType		e_source_selector_dialog_get_type (void) G_GNUC_CONST;
GtkWidget *	e_source_selector_dialog_new	(GtkWindow *parent,
						 ESourceRegistry *registry,
						 const gchar *extension_name);
ESourceRegistry *
		e_source_selector_dialog_get_registry
						(ESourceSelectorDialog *dialog);
const gchar *	e_source_selector_dialog_get_extension_name
						(ESourceSelectorDialog *dialog);
ESourceSelector *
		e_source_selector_dialog_get_selector
						(ESourceSelectorDialog *dialog);
ESource *	e_source_selector_dialog_peek_primary_selection
						(ESourceSelectorDialog *dialog);
ESource *	e_source_selector_dialog_get_except_source
						(ESourceSelectorDialog *dialog);
void		e_source_selector_dialog_set_except_source
						(ESourceSelectorDialog *dialog,
						 ESource *except_source);

G_END_DECLS

#endif /* E_SOURCE_SELECTOR_DIALOG_H */
