/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

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

GType		e_attachment_icon_view_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_attachment_icon_view_new		(void);

G_END_DECLS

#endif /* E_ATTACHMENT_ICON_VIEW_H */
