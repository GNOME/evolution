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

void       e_dialog_set_transient_for        (GtkWindow        *dialog,
					      GtkWidget        *parent_widget);
void       e_dialog_set_transient_for_xid    (GtkWindow        *dialog,
					      GdkNativeWindow   xid);

/* FIXME These functions should go away completely at some point.  */

#ifndef GNOME_DISABLE_DEPRECATED
void       e_gnome_dialog_set_parent         (GnomeDialog      *dialog,
					      GtkWindow        *parent);
#endif

char      *e_file_dialog_save                (const char       *title);


#endif
