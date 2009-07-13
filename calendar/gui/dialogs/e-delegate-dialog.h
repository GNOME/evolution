/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_DELEGATE_DIALOG_H__
#define __E_DELEGATE_DIALOG_H__

#include <gtk/gtk.h>



#define E_TYPE_DELEGATE_DIALOG       (e_delegate_dialog_get_type ())
#define E_DELEGATE_DIALOG(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_DELEGATE_DIALOG, EDelegateDialog))
#define E_DELEGATE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_DELEGATE_DIALOG,	\
				      EDelegateDialogClass))
#define E_IS_DELEGATE_DIALOG(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_DELEGATE_DIALOG))
#define E_IS_DELEGATE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_DELEGATE_DIALOG))

typedef struct _EDelegateDialog		EDelegateDialog;
typedef struct _EDelegateDialogClass	EDelegateDialogClass;
typedef struct _EDelegateDialogPrivate	EDelegateDialogPrivate;

struct _EDelegateDialog {
	GtkObject object;

	/* Private data */
	EDelegateDialogPrivate *priv;
};

struct _EDelegateDialogClass {
	GtkObjectClass parent_class;
};

GType            e_delegate_dialog_get_type          (void);

EDelegateDialog* e_delegate_dialog_construct         (EDelegateDialog *etd,
						      const gchar      *name,
						      const gchar      *address);

EDelegateDialog* e_delegate_dialog_new               (const gchar      *name,
						      const gchar      *address);

gchar *            e_delegate_dialog_get_delegate      (EDelegateDialog *etd);

gchar *            e_delegate_dialog_get_delegate_name (EDelegateDialog *etd);

void             e_delegate_dialog_set_delegate      (EDelegateDialog *etd,
						      const gchar      *address);

GtkWidget*       e_delegate_dialog_get_toplevel      (EDelegateDialog *etd);



#endif /* __E_DELEGATE_DIALOG_H__ */
