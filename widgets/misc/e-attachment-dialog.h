/*
 * e-attachment-dialog.h
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

#ifndef E_ATTACHMENT_DIALOG_H
#define E_ATTACHMENT_DIALOG_H

#include <gtk/gtk.h>
#include <misc/e-attachment.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_DIALOG \
	(e_attachment_dialog_get_type ())
#define E_ATTACHMENT_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_DIALOG, EAttachmentDialog))
#define E_ATTACHMENT_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_DIALOG, EAttachmentDialogClass))
#define E_IS_ATTACHMENT_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_DIALOG))
#define E_IS_ATTACHMENT_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_DIALOG))
#define E_ATTACHMENT_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT_DIALOG, EAttachmentDialogClass))

G_BEGIN_DECLS

typedef struct _EAttachmentDialog EAttachmentDialog;
typedef struct _EAttachmentDialogClass EAttachmentDialogClass;
typedef struct _EAttachmentDialogPrivate EAttachmentDialogPrivate;

struct _EAttachmentDialog {
	GtkDialog parent;
	EAttachmentDialogPrivate *priv;
};

struct _EAttachmentDialogClass {
	GtkDialogClass parent_class;
};

GType		e_attachment_dialog_get_type	(void);
GtkWidget *	e_attachment_dialog_new		(GtkWindow *parent,
						 EAttachment *attachment);
EAttachment *	e_attachment_dialog_get_attachment
						(EAttachmentDialog *dialog);
void		e_attachment_dialog_set_attachment
						(EAttachmentDialog *dialog,
						 EAttachment *attachment);

G_END_DECLS

#endif /* E_ATTACHMENT_DIALOG_H */
