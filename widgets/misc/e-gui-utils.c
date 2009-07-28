/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas-pixbuf.h>

#include "e-gui-utils.h"

void
e_popup_menu (GtkMenu *menu, GdkEvent *event)
{
	g_return_if_fail (GTK_IS_MENU (menu));

	g_signal_connect (
		menu, "selection-done",
		G_CALLBACK (gtk_widget_destroy), NULL);

	if (event) {
		if (event->type == GDK_KEY_PRESS)
			gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 0,
					event->key.time);
		else if ((event->type == GDK_BUTTON_PRESS) ||
			 (event->type == GDK_BUTTON_RELEASE) ||
			 (event->type == GDK_2BUTTON_PRESS) ||
			 (event->type == GDK_3BUTTON_PRESS)) {
			gtk_menu_popup (menu, NULL, NULL, NULL, NULL,
					event->button.button,
					event->button.time);
		}
	} else
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 0,
				GDK_CURRENT_TIME);
}
