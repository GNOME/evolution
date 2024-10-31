/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HTML_EDITOR_LINK_POPOVER_H
#define E_HTML_EDITOR_LINK_POPOVER_H

#include <gtk/gtk.h>
#include <e-util/e-html-editor.h>

#define E_TYPE_HTML_EDITOR_LINK_POPOVER (e_html_editor_link_popover_get_type ())

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (EHTMLEditorLinkPopover, e_html_editor_link_popover, E, HTML_EDITOR_LINK_POPOVER, GtkPopover)

GtkWidget *	e_html_editor_link_popover_new	(EHTMLEditor *editor);
void		e_html_editor_link_popover_popup(EHTMLEditorLinkPopover *self);

G_END_DECLS

#endif /* E_HTML_EDITOR_LINK_POPOVER_H */
