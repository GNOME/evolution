/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-dialog-utils.h
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef E_DIALOG_UTILS_H
#define E_DIALOG_UTILS_H

#include <gtk/gtkwindow.h>
#include <gtk/gtkwidget.h>
#include <libgnomeui/gnome-types.h>
#include <libgnomeui/gnome-dialog.h>

#include <X11/Xlib.h>		/* Window */

void       e_set_dialog_parent               (GtkWindow          *dialog,
					      GtkWidget          *parent_widget);
void       e_set_dialog_parent_from_xid      (GtkWindow          *dialog,
					      Window              xid);
void       e_gnome_dialog_set_parent         (GnomeDialog        *dialog,
					      GtkWindow          *parent);
GtkWidget *e_gnome_warning_dialog_parented   (const char         *warning,
					      GtkWindow          *parent);
GtkWidget *e_gnome_ok_cancel_dialog_parented (const char         *message,
					      GnomeReplyCallback  callback,
					      gpointer            data,
					      GtkWindow          *parent);
char      *e_file_dialog_save                (const char         *title);


#endif
