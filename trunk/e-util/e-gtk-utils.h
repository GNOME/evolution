/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-gtk-utils.c
 *
 * Copyright (C) 2000  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef E_GTK_UTILS_H
#define E_GTK_UTILS_H

#include <gtk/gtkobject.h>
#include <gtk/gtkradiobutton.h>

void  e_signal_connect_while_alive (void *object,
				    const char *name,
				    GCallback callback,
				    void *data,
				    void *alive_instance);

void  e_signal_connect_full_while_alive  (void               *instance,
					  const char         *name,
					  GtkSignalFunc       func,
					  GtkCallbackMarshal  marshal,
					  void               *data,
					  GtkDestroyNotify    destroy_func,
					  gboolean            object_signal,
					  gboolean            after,
					  void               *alive_instance);

void  e_make_widget_backing_stored  (GtkWidget *widget);

GtkWidget *e_gtk_button_new_with_icon(const char *text, const char *stock);

#endif
