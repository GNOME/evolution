/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_LABEL_DIALOG_H
#define E_MAIL_LABEL_DIALOG_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_LABEL_DIALOG \
	(e_mail_label_dialog_get_type ())
#define E_MAIL_LABEL_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_LABEL_DIALOG, EMailLabelDialog))
#define E_MAIL_LABEL_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_LABEL_DIALOG, EMailLabelDialogClass))
#define E_IS_MAIL_LABEL_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_LABEL_DIALOG))
#define E_IS_MAIL_LABEL_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_LABEL_DIALOG))
#define E_MAIL_LABEL_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_LABEL_DIALOG, EMailLabelDialogClass))

G_BEGIN_DECLS

typedef struct _EMailLabelDialog EMailLabelDialog;
typedef struct _EMailLabelDialogClass EMailLabelDialogClass;
typedef struct _EMailLabelDialogPrivate EMailLabelDialogPrivate;

struct _EMailLabelDialog {
	GtkDialog parent;
	EMailLabelDialogPrivate *priv;
};

struct _EMailLabelDialogClass {
	GtkDialogClass parent_class;
};

GType		e_mail_label_dialog_get_type	(void);
GtkWidget *	e_mail_label_dialog_new		(GtkWindow *parent);
const gchar *	e_mail_label_dialog_get_label_name
						(EMailLabelDialog *dialog);
void		e_mail_label_dialog_set_label_name
						(EMailLabelDialog *dialog,
						 const gchar *label_name);
void		e_mail_label_dialog_get_label_color
						(EMailLabelDialog *dialog,
						 GdkRGBA *label_color);
void		e_mail_label_dialog_set_label_color
						(EMailLabelDialog *dialog,
						 const GdkRGBA *label_color);

G_END_DECLS

#endif /* E_MAIL_LABEL_DIALOG_H */
