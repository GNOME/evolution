/*
 * e-mail-attachment-button.h
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

#ifndef E_MAIL_ATTACHMENT_BUTTON_H
#define E_MAIL_ATTACHMENT_BUTTON_H

#include <gtk/gtk.h>
#include <widgets/misc/e-attachment.h>
#include <widgets/misc/e-attachment-view.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_ATTACHMENT_BUTTON \
	(e_mail_attachment_button_get_type ())
#define E_MAIL_ATTACHMENT_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_ATTACHMENT_BUTTON, EMailAttachmentButton))
#define E_MAIL_ATTACHMENT_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_ATTACHMENT_BUTTON, EMailAttachmentButtonClass))
#define E_IS_MAIL_ATTACHMENT_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_ATTACHMENT_BUTTON))
#define E_IS_MAIL_ATTACHMENT_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_ATTACHMENT_BUTTON))
#define E_MAIL_ATTACHMENT_BUTTON_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_ATTACHMENT_BUTTON, EMailAttachmentButtonClass))

G_BEGIN_DECLS

typedef struct _EMailAttachmentButton EMailAttachmentButton;
typedef struct _EMailAttachmentButtonClass EMailAttachmentButtonClass;
typedef struct _EMailAttachmentButtonPrivate EMailAttachmentButtonPrivate;

struct _EMailAttachmentButton {
	GtkHBox parent;
	EMailAttachmentButtonPrivate *priv;
};

struct _EMailAttachmentButtonClass {
	GtkHBoxClass parent_class;

	/* Signals */
	void		(*clicked)		(EMailAttachmentButton *button);
};

GType		e_mail_attachment_button_get_type	(void);
GtkWidget *	e_mail_attachment_button_new	(EAttachmentView *view);
void		e_mail_attachment_button_clicked(EMailAttachmentButton *button);
EAttachmentView *
		e_mail_attachment_button_get_view
						(EMailAttachmentButton *button);
EAttachment *	e_mail_attachment_button_get_attachment
						(EMailAttachmentButton *button);
void		e_mail_attachment_button_set_attachment
						(EMailAttachmentButton *button,
						 EAttachment *attachment);
gboolean	e_mail_attachment_button_get_expandable
						(EMailAttachmentButton *button);
void		e_mail_attachment_button_set_expandable
						(EMailAttachmentButton *button,
						 gboolean expandable);
gboolean	e_mail_attachment_button_get_expanded
						(EMailAttachmentButton *button);
void		e_mail_attachment_button_set_expanded
						(EMailAttachmentButton *button,
						 gboolean expanded);

G_END_DECLS

#endif /* E_MAIL_ATTACHMENT_BUTTON_H */
