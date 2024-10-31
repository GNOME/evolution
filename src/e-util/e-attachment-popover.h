/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ATTACHMENT_POPOVER_H
#define E_ATTACHMENT_POPOVER_H

#include <gtk/gtk.h>
#include <e-util/e-attachment.h>

#define E_TYPE_ATTACHMENT_POPOVER (e_attachment_popover_get_type ())

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (EAttachmentPopover, e_attachment_popover, E, ATTACHMENT_POPOVER, GtkPopover)

GtkWidget *	e_attachment_popover_new	(GtkWidget *relative_to,
						 EAttachment *attachment);
EAttachment *	e_attachment_popover_get_attachment
						(EAttachmentPopover *self);
void		e_attachment_popover_set_attachment
						(EAttachmentPopover *self,
						 EAttachment *attachment);
gboolean	e_attachment_popover_get_changes_saved
						(EAttachmentPopover *self);
void		e_attachment_popover_set_changes_saved
						(EAttachmentPopover *self,
						 gboolean changes_saved);
gboolean	e_attachment_popover_get_allow_disposition
						(EAttachmentPopover *self);
void		e_attachment_popover_set_allow_disposition
						(EAttachmentPopover *self,
						 gboolean allow_disposition);
void		e_attachment_popover_popup	(EAttachmentPopover *self);

G_END_DECLS

#endif /* E_ATTACHMENT_POPOVER_H */
