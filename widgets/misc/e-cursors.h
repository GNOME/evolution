/*
 *
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
 *		Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef GNOME_APP_LIB_CURSORS_H
#define GNOME_APP_LIB_CURSORS_H

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef enum {
	E_CURSOR_FAT_CROSS,
	E_CURSOR_THIN_CROSS,
	E_CURSOR_ARROW,
	E_CURSOR_MOVE,
	E_CURSOR_ZOOM_IN,
	E_CURSOR_ZOOM_OUT,
	E_CURSOR_SIZE_X,
	E_CURSOR_SIZE_Y,
	E_CURSOR_SIZE_TL,
	E_CURSOR_SIZE_TR,
	E_CURSOR_PRESS,
	E_CURSOR_HAND_OPEN,
	E_CURSOR_HAND_CLOSED,
	E_CURSOR_XTERM,
	E_CURSOR_NUM_CURSORS
} ECursorType;

void    e_cursors_init      (void);
void    e_cursors_shutdown  (void);

#define e_cursor_set(win, c)					   \
G_STMT_START {							   \
     if (win)							   \
         gdk_window_set_cursor (win, e_cursor_get (c)); \
} G_STMT_END

#define e_cursor_set_widget(w, c)							     \
G_STMT_START {									     \
     if (GTK_WIDGET (w)->window)						     \
	gdk_window_set_cursor (GTK_WIDGET (w)->window, e_cursor_get (c)); \
} G_STMT_END

GdkCursor *e_cursor_get (ECursorType type);

G_END_DECLS

#endif /* GNOME_APP_LIB_CURSORS_H */
