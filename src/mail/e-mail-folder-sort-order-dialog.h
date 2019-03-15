/*
 * Copyright (C) 2019 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_MAIL_FOLDER_SORT_ORDER_DIALOG_H
#define E_MAIL_FOLDER_SORT_ORDER_DIALOG_H

#include <gtk/gtk.h>

#include <mail/em-folder-tree.h>
#include <mail/em-folder-tree-model.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_FOLDER_SORT_ORDER_DIALOG \
	(e_mail_folder_sort_order_dialog_get_type ())
#define E_MAIL_FOLDER_SORT_ORDER_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_FOLDER_SORT_ORDER_DIALOG, EMailFolderSortOrderDialog))
#define E_MAIL_FOLDER_SORT_ORDER_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_FOLDER_SORT_ORDER_DIALOG, EMailFolderSortOrderDialogClass))
#define E_IS_MAIL_FOLDER_SORT_ORDER_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_FOLDER_SORT_ORDER_DIALOG))
#define E_IS_MAIL_FOLDER_SORT_ORDER_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_FOLDER_SORT_ORDER_DIALOG))
#define E_MAIL_FOLDER_SORT_ORDER_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_FOLDER_SORT_ORDER_DIALOG, EMailFolderSortOrderDialogClass))

G_BEGIN_DECLS

typedef struct _EMailFolderSortOrderDialog EMailFolderSortOrderDialog;
typedef struct _EMailFolderSortOrderDialogClass EMailFolderSortOrderDialogClass;
typedef struct _EMailFolderSortOrderDialogPrivate EMailFolderSortOrderDialogPrivate;

struct _EMailFolderSortOrderDialog {
	/*< private >*/
	GtkDialog parent;
	EMailFolderSortOrderDialogPrivate *priv;
};

struct _EMailFolderSortOrderDialogClass {
	/*< private >*/
	GtkDialogClass parent_class;
};

GType		e_mail_folder_sort_order_dialog_get_type	(void);

GtkWidget *	e_mail_folder_sort_order_dialog_new		(GtkWindow *parent,
								 CamelStore *store,
								 const gchar *folder_uri);

G_END_DECLS

#endif /* E_MAIL_FOLDER_SORT_ORDER_DIALOG_H */
