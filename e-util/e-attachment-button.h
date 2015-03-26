/*
 * e-attachment-button.h
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

#ifndef E_ATTACHMENT_BUTTON_H
#define E_ATTACHMENT_BUTTON_H

#include <gtk/gtk.h>
#include <e-util/e-attachment.h>
#include <e-util/e-attachment-view.h>

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
	GtkBox parent;
	EAttachmentButtonPrivate *priv;
};

struct _EAttachmentButtonClass {
	GtkBoxClass parent_class;
};

GType		e_attachment_button_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_attachment_button_new	(void);
EAttachmentView *
		e_attachment_button_get_view	(EAttachmentButton *button);
void		e_attachment_button_set_view	(EAttachmentButton *button,
						 EAttachmentView *view);
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
gboolean	e_attachment_button_get_zoom_to_window
						(EAttachmentButton *button);
void		e_attachment_button_set_zoom_to_window
						(EAttachmentButton *button,
						 gboolean zoom_to_window);

G_END_DECLS

#endif /* E_ATTACHMENT_BUTTON_H */
