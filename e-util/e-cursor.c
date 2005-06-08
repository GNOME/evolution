/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 *  Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 *  Copyright 2004 Novell Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "e-cursor.h"

/**
 * e_cursor_set:
 * @widget: Any widget in a window, to which busy cursor has to be set
 * cursor: The type of cursor to be set defined in e-cursor.h
 * 
 * Sets the cursor specified, to the top level window of the given widget.
 * It is not window aware, so if you popup a window, it will not have 
 * busy cursor set. That has to be handled seperately with a new call to this
 * function.
 *
 * Return value: 
 **/
void e_cursor_set (GtkWidget *widget, ECursorType cursor)
{
	GtkWidget *toplevel;
	GdkCursor *window_cursor;

	toplevel = gtk_widget_get_toplevel (widget);
	if (GTK_WIDGET_TOPLEVEL (toplevel)) {

		switch (cursor) {
			case E_CURSOR_NORMAL :
				window_cursor = gdk_cursor_new (GDK_LEFT_PTR);
				break;
			case E_CURSOR_BUSY :
				window_cursor = gdk_cursor_new (GDK_WATCH);
				break;

			default :
				window_cursor = gdk_cursor_new (GDK_LEFT_PTR);
		}

		gdk_window_set_cursor (toplevel->window, window_cursor);
		gdk_cursor_destroy (window_cursor);
	}

}
