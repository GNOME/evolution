/*
 * SPDX-FileCopyrightText: (C) 2014 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Milan Crha <mcrha@redhat.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_WIDGET_UNDO_H
#define E_WIDGET_UNDO_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

struct _EFocusTracker;

void		e_widget_undo_attach		(GtkWidget *widget,
						 struct _EFocusTracker *focus_tracker);
gboolean	e_widget_undo_is_attached	(GtkWidget *widget);
gboolean	e_widget_undo_has_undo		(GtkWidget *widget);
gboolean	e_widget_undo_has_redo		(GtkWidget *widget);
gchar *		e_widget_undo_describe_undo	(GtkWidget *widget);
gchar *		e_widget_undo_describe_redo	(GtkWidget *widget);
void		e_widget_undo_do_undo		(GtkWidget *widget);
void		e_widget_undo_do_redo		(GtkWidget *widget);
void		e_widget_undo_reset		(GtkWidget *widget);

G_END_DECLS

#endif /* E_WIDGET_UNDO_H */
