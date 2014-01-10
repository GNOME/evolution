/*
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
 * Authors:
 *		Milan Crha <mcrha@redhat.com>
 *
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
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
