/*
 * e-mail-attachment-bar.h
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

#ifndef E_MAIL_ATTACHMENT_BAR_H
#define E_MAIL_ATTACHMENT_BAR_H

#include <gtk/gtk.h>
#include <misc/e-attachment-view.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_ATTACHMENT_BAR \
	(e_mail_attachment_bar_get_type ())
#define E_MAIL_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_ATTACHMENT_BAR, EMailAttachmentBar))
#define E_MAIL_ATTACHMENT_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_ATTACHMENT_BAR, EMailAttachmentBarClass))
#define E_IS_MAIL_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_ATTACHMENT_BAR))
#define E_IS_MAIL_ATTACHMENT_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_ATTACHMENT_BAR))
#define E_MAIL_ATTACHMENT_BAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_ATTACHMENT_BAR, EMailAttachmentBarClass))

G_BEGIN_DECLS

typedef struct _EMailAttachmentBar EMailAttachmentBar;
typedef struct _EMailAttachmentBarClass EMailAttachmentBarClass;
typedef struct _EMailAttachmentBarPrivate EMailAttachmentBarPrivate;

struct _EMailAttachmentBar {
	GtkVBox parent;
	EMailAttachmentBarPrivate *priv;
};

struct _EMailAttachmentBarClass {
	GtkVBoxClass parent_class;
};

GType		e_mail_attachment_bar_get_type	(void);
GtkWidget *	e_mail_attachment_bar_new	(void);
gint		e_mail_attachment_bar_get_active_view
						(EMailAttachmentBar *bar);
void		e_mail_attachment_bar_set_active_view
						(EMailAttachmentBar *bar,
						 gint active_view);
gboolean	e_mail_attachment_bar_get_expanded
						(EMailAttachmentBar *bar);
void		e_mail_attachment_bar_set_expanded
						(EMailAttachmentBar *bar,
						 gboolean expanded);

G_END_DECLS

#endif /* E_MAIL_ATTACHMENT_BAR_H */
