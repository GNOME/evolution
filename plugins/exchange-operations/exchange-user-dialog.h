/*
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E2K_USER_DIALOG_H__
#define __E2K_USER_DIALOG_H__

#include <gtk/gtk.h>

#define E2K_TYPE_USER_DIALOG		(e2k_user_dialog_get_type ())
#define E2K_USER_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E2K_TYPE_USER_DIALOG, E2kUserDialog))
#define E2K_USER_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E2K_TYPE_USER_DIALOG,	\
					 E2kUserDialogClass))
#define E2K_IS_USER_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E2K_TYPE_USER_DIALOG))
#define E2K_IS_USER_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), E2K_TYPE_USER_DIALOG))

typedef struct _E2kUserDialog		E2kUserDialog;
typedef struct _E2kUserDialogClass	E2kUserDialogClass;
typedef struct _E2kUserDialogPrivate	E2kUserDialogPrivate;

struct _E2kUserDialog {
	GtkDialog parent;

	/* Private data */
	E2kUserDialogPrivate *priv;
};

struct _E2kUserDialogClass {
	GtkDialogClass parent_class;
};

GType      e2k_user_dialog_get_type (void);
GtkWidget *e2k_user_dialog_new      (GtkWidget     *parent_window,
				     const gchar    *label_text,
				     const gchar    *section_name);
gchar      *e2k_user_dialog_get_user (E2kUserDialog *dialog);
GList      *e2k_user_dialog_get_user_list (E2kUserDialog *dialog);

#endif /* __E2K_USER_DIALOG_H__ */
