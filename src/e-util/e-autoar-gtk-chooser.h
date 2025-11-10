/*
 * SPDX-FileCopyrightText: (C) 2013, 2014  Ting-Wei Lan
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_AUTOAR_GTK_CHOOSER_H
#define E_AUTOAR_GTK_CHOOSER_H

#ifdef HAVE_AUTOAR
#include <gtk/gtk.h>
#include <gnome-autoar/gnome-autoar.h>

G_BEGIN_DECLS

GtkWidget *	e_autoar_gtk_chooser_simple_new	(AutoarFormat default_format,
						 AutoarFilter default_filter);
gboolean	e_autoar_gtk_chooser_simple_get	(GtkWidget *simple,
						 int *format,
						 int *filter);

G_END_DECLS
#endif /* HAVE_AUTOAR */

#endif /* E_AUTOAR_GTK_CHOOSER_H */
