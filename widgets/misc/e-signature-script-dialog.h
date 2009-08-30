/*
 * e-signature-script-dialog.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_SIGNATURE_SCRIPT_DIALOG_H
#define E_SIGNATURE_SCRIPT_DIALOG_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_SIGNATURE_SCRIPT_DIALOG \
	(e_signature_script_dialog_get_type ())
#define E_SIGNATURE_SCRIPT_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SIGNATURE_SCRIPT_DIALOG, ESignatureScriptDialog))
#define E_SIGNATURE_SCRIPT_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SIGNATURE_SCRIPT_DIALOG, ESignatureScriptDialogClass))
#define E_IS_SIGNATURE_SCRIPT_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SIGNATURE_SCRIPT_DIALOG))
#define E_IS_SIGNATURE_SCRIPT_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SIGNATURE_SCRIPT_DIALOG))
#define E_SIGNATURE_SCRIPT_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SIGNATURE_SCRIPT_DIALOG, ESignatureScriptDialogClass))

G_BEGIN_DECLS

typedef struct _ESignatureScriptDialog ESignatureScriptDialog;
typedef struct _ESignatureScriptDialogClass ESignatureScriptDialogClass;
typedef struct _ESignatureScriptDialogPrivate ESignatureScriptDialogPrivate;

struct _ESignatureScriptDialog {
	GtkDialog parent;
	ESignatureScriptDialogPrivate *priv;
};

struct _ESignatureScriptDialogClass {
	GtkDialogClass parent_class;
};

GType		e_signature_script_dialog_get_type	(void);
GtkWidget *	e_signature_script_dialog_new		(GtkWindow *parent);
GFile *		e_signature_script_dialog_get_script_file
					(ESignatureScriptDialog *dialog);
void		e_signature_script_dialog_set_script_file
					(ESignatureScriptDialog *dialog,
					 GFile *script_file);
const gchar *	e_signature_script_dialog_get_script_name
					(ESignatureScriptDialog *dialog);
void		e_signature_script_dialog_set_script_name
					(ESignatureScriptDialog *dialog,
					 const gchar *script_name);

G_END_DECLS

#endif /* E_SIGNATURE_SCRIPT_DIALOG_H */
