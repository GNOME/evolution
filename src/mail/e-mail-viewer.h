/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_VIEWER_H
#define E_MAIL_VIEWER_H

#include <gtk/gtk.h>
#include <mail/e-mail-backend.h>

#define E_TYPE_MAIL_VIEWER (e_mail_viewer_get_type ())
#define E_MAIL_VIEWER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_VIEWER, EMailViewer))
#define E_MAIL_VIEWER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_VIEWER, EMailViewerClass))
#define E_IS_MAIL_VIEWER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_VIEWER))
#define E_IS_MAIL_VIEWER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_VIEWER))
#define E_MAIL_VIEWER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_VIEWER, EMailViewerClass))

G_BEGIN_DECLS

typedef struct _EMailViewer EMailViewer;
typedef struct _EMailViewerClass EMailViewerClass;
typedef struct _EMailViewerPrivate EMailViewerPrivate;

struct _EMailViewer {
	GtkWindow parent;
	EMailViewerPrivate *priv;
};

struct _EMailViewerClass {
	GtkWindowClass parent_class;
};

GType		e_mail_viewer_get_type		(void);
EMailViewer *	e_mail_viewer_new		(EMailBackend *backend);
EMailBackend *	e_mail_viewer_get_backend	(EMailViewer *self);
gboolean	e_mail_viewer_assign_file	(EMailViewer *self,
						 GFile *file);
GFile *		e_mail_viewer_get_file		(EMailViewer *self);

G_END_DECLS

#endif /* E_MAIL_VIEWER_H */
