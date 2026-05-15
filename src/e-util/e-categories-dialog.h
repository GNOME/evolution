/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CATEGORIES_DIALOG_H
#define E_CATEGORIES_DIALOG_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_CATEGORIES_DIALOG \
	(e_categories_dialog_get_type ())
#define E_CATEGORIES_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CATEGORIES_DIALOG, ECategoriesDialog))
#define E_CATEGORIES_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CATEGORIES_DIALOG, ECategoriesDialogClass))
#define E_IS_CATEGORIES_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CATEGORIES_DIALOG))
#define E_IS_CATEGORIES_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CATEGORIES_DIALOG))
#define E_CATEGORIES_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CATEGORIES_DIALOG, ECategoriesDialogClass))

G_BEGIN_DECLS

typedef struct _ECategoriesDialog ECategoriesDialog;
typedef struct _ECategoriesDialogClass ECategoriesDialogClass;
typedef struct _ECategoriesDialogPrivate ECategoriesDialogPrivate;

struct _ECategoriesDialog {
	GtkDialog parent;
	ECategoriesDialogPrivate *priv;
};

struct _ECategoriesDialogClass {
	GtkDialogClass parent_class;
};

GType		e_categories_dialog_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_categories_dialog_new		(const gchar *categories);
gchar *		e_categories_dialog_get_categories
						(ECategoriesDialog *dialog);
void		e_categories_dialog_set_categories
						(ECategoriesDialog *dialog,
						 const gchar *categories);

G_END_DECLS

#endif /* E_CATEGORIES_DIALOG_H */
