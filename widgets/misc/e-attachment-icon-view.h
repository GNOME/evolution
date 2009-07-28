/*
 * e-attachment-icon-view.h
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

#ifndef E_ATTACHMENT_ICON_VIEW_H
#define E_ATTACHMENT_ICON_VIEW_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_ICON_VIEW \
	(e_attachment_icon_view_get_type ())
#define E_ATTACHMENT_ICON_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_ICON_VIEW, EAttachmentIconView))
#define E_ATTACHMENT_ICON_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_ICON_VIEW, EAttachmentIconView))
#define E_IS_ATTACHMENT_ICON_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_ICON_VIEW))
#define E_IS_ATTACHMENT_ICON_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_ICON_VIEW))
#define E_ATTACHMENT_ICON_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT_ICON_VIEW))

G_BEGIN_DECLS

typedef struct _EAttachmentIconView EAttachmentIconView;
typedef struct _EAttachmentIconViewClass EAttachmentIconViewClass;
typedef struct _EAttachmentIconViewPrivate EAttachmentIconViewPrivate;

struct _EAttachmentIconView {
	GtkIconView parent;
	EAttachmentIconViewPrivate *priv;
};

struct _EAttachmentIconViewClass {
	GtkIconViewClass parent_class;
};

GType		e_attachment_icon_view_get_type		(void);
GtkWidget *	e_attachment_icon_view_new		(void);
void		e_attachment_icon_view_set_default_icon_size
							(gint size);
G_END_DECLS

#endif /* E_ATTACHMENT_ICON_VIEW_H */
