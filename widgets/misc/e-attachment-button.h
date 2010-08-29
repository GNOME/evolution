/*
 * e-attachment-button.h
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

#ifndef E_ATTACHMENT_BUTTON_H
#define E_ATTACHMENT_BUTTON_H

#include <gtk/gtk.h>
#include <misc/e-attachment.h>
#include <misc/e-attachment-view.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_BUTTON \
	(e_attachment_button_get_type ())
#define E_ATTACHMENT_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_BUTTON, EAttachmentButton))
#define E_ATTACHMENT_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_BUTTON, EAttachmentButtonClass))
#define E_IS_ATTACHMENT_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_BUTTON))
#define E_IS_ATTACHMENT_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_BUTTON))
#define E_ATTACHMENT_BUTTON_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT_BUTTON, EAttachmentButtonClass))

G_BEGIN_DECLS

typedef struct _EAttachmentButton EAttachmentButton;
typedef struct _EAttachmentButtonClass EAttachmentButtonClass;
typedef struct _EAttachmentButtonPrivate EAttachmentButtonPrivate;

struct _EAttachmentButton {
	GtkHBox parent;
	EAttachmentButtonPrivate *priv;
};

struct _EAttachmentButtonClass {
	GtkHBoxClass parent_class;
};

GType		e_attachment_button_get_type	(void);
GtkWidget *	e_attachment_button_new	(EAttachmentView *view);
EAttachmentView *
		e_attachment_button_get_view	(EAttachmentButton *button);
EAttachment *	e_attachment_button_get_attachment
						(EAttachmentButton *button);
void		e_attachment_button_set_attachment
						(EAttachmentButton *button,
						 EAttachment *attachment);
gboolean	e_attachment_button_get_expandable
						(EAttachmentButton *button);
void		e_attachment_button_set_expandable
						(EAttachmentButton *button,
						 gboolean expandable);
gboolean	e_attachment_button_get_expanded
						(EAttachmentButton *button);
void		e_attachment_button_set_expanded
						(EAttachmentButton *button,
						 gboolean expanded);

G_END_DECLS

#endif /* E_ATTACHMENT_BUTTON_H */
