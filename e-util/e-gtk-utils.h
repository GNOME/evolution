/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-gtk-utils.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <gtk/gtk.h>

void  e_gtk_signal_connect_full_while_alive  (GtkObject          *object,
					      const char         *name,
					      GtkSignalFunc       func,
					      GtkCallbackMarshal  marshal,
					      void               *data,
					      GtkDestroyNotify    destroy_func,
					      gboolean            object_signal,
					      gboolean            after,
					      GtkObject          *alive_object);

#endif
