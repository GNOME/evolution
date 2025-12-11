/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_MESSAGE_POPOVER_H
#define E_MESSAGE_POPOVER_H

#include <stdarg.h>
#include <gtk/gtk.h>

#include <e-util/e-util-enums.h>

G_BEGIN_DECLS

#define E_TYPE_MESSAGE_POPOVER (e_message_popover_get_type ())
G_DECLARE_FINAL_TYPE (EMessagePopover, e_message_popover, E, MESSAGE_POPOVER, GtkPopover)

EMessagePopover *
		e_message_popover_new		(GtkWidget *relative_to,
						 EMessagePopoverFlags flags);
void		e_message_popover_set_text	(EMessagePopover *self,
						 const gchar *format,
						 ...) G_GNUC_PRINTF (2, 3);
void		e_message_popover_set_text_literal
						(EMessagePopover *self,
						 const gchar *text);
void		e_message_popover_set_markup	(EMessagePopover *self,
						 const gchar *format,
						 ...) G_GNUC_PRINTF (2, 3);
void		e_message_popover_set_markup_literal
						(EMessagePopover *self,
						 const gchar *markup);
void		e_message_popover_show		(EMessagePopover *self);

G_END_DECLS

#endif /* E_MESSAGE_POPOVER_H */
