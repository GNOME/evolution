/*
 * e-attachment-paned.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ATTACHMENT_PANED_H
#define E_ATTACHMENT_PANED_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_PANED \
	(e_attachment_paned_get_type ())
#define E_ATTACHMENT_PANED(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_PANED, EAttachmentPaned))
#define E_ATTACHMENT_PANED_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_PANED, EAttachmentPanedClass))
#define E_IS_ATTACHMENT_PANED(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_PANED))
#define E_IS_ATTACHMENT_PANED_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_PANED))
#define E_ATTACHMENT_PANED_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT_PANED, EAttachmentPanedClass))

G_BEGIN_DECLS

typedef struct _EAttachmentPaned EAttachmentPaned;
typedef struct _EAttachmentPanedClass EAttachmentPanedClass;
typedef struct _EAttachmentPanedPrivate EAttachmentPanedPrivate;

struct _EAttachmentPaned {
	GtkVPaned parent;
	EAttachmentPanedPrivate *priv;
};

struct _EAttachmentPanedClass {
	GtkVPanedClass parent_class;
};

GType		e_attachment_paned_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_attachment_paned_new		(void);
GtkWidget *	e_attachment_paned_get_content_area
						(EAttachmentPaned *paned);
gint		e_attachment_paned_get_active_view
						(EAttachmentPaned *paned);
void		e_attachment_paned_set_active_view
						(EAttachmentPaned *paned,
						 gint active_view);
gboolean	e_attachment_paned_get_expanded	(EAttachmentPaned *paned);
void		e_attachment_paned_set_expanded	(EAttachmentPaned *paned,
						 gboolean expanded);
gboolean	e_attachment_paned_get_resize_toplevel
						(EAttachmentPaned *paned);
void		e_attachment_paned_set_resize_toplevel
						(EAttachmentPaned *paned,
						 gboolean resize_toplevel);
void		e_attachment_paned_drag_data_received
						(EAttachmentPaned *paned,
						 GdkDragContext *context,
						 gint x,
						 gint y,
						 GtkSelectionData *selection,
						 guint info,
						 guint time);
GtkWidget *	e_attachment_paned_get_controls_container
						(EAttachmentPaned *paned);
GtkWidget *	e_attachment_paned_get_view_combo
						(EAttachmentPaned *paned);
void		e_attachment_paned_set_default_height
						(gint height);

G_END_DECLS

#endif /* E_ATTACHMENT_PANED_H */
