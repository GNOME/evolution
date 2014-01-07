/*
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
 *
 * Authors:
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_DELEGATE_DIALOG_H
#define E_DELEGATE_DIALOG_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_DELEGATE_DIALOG \
	(e_delegate_dialog_get_type ())
#define E_DELEGATE_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_DELEGATE_DIALOG, EDelegateDialog))
#define E_DELEGATE_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_DELEGATE_DIALOG, EDelegateDialogClass))
#define E_IS_DELEGATE_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_DELEGATE_DIALOG))
#define E_IS_DELEGATE_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_DELEGATE_DIALOG))
#define E_DELEGATE_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_DELEGATE_DIALOG, EDelegateDialogClass))

G_BEGIN_DECLS

typedef struct _EDelegateDialog EDelegateDialog;
typedef struct _EDelegateDialogClass EDelegateDialogClass;
typedef struct _EDelegateDialogPrivate EDelegateDialogPrivate;

struct _EDelegateDialog {
	GObject object;
	EDelegateDialogPrivate *priv;
};

struct _EDelegateDialogClass {
	GObjectClass parent_class;
};

GType		e_delegate_dialog_get_type	(void);
EDelegateDialog *
		e_delegate_dialog_construct	(EDelegateDialog *etd,
						 EClientCache *client_cache,
						 const gchar *name,
						 const gchar *address);
EDelegateDialog *
		e_delegate_dialog_new		(EClientCache *client_cache,
						 const gchar *name,
						 const gchar *address);
gchar *		e_delegate_dialog_get_delegate	(EDelegateDialog *etd);
gchar *		e_delegate_dialog_get_delegate_name
						(EDelegateDialog *etd);
void		e_delegate_dialog_set_delegate	(EDelegateDialog *etd,
						 const gchar *address);
GtkWidget *	e_delegate_dialog_get_toplevel	(EDelegateDialog *etd);

G_END_DECLS

#endif /* E_DELEGATE_DIALOG_H */
