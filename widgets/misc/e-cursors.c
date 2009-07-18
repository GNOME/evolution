/*
 * e-cursors.c - cursor handling for gnumeric
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

#include <config.h>

#include <stdio.h>

#include "e-colors.h"
#include "e-cursors.h"

#include "pixmaps/cursor_cross.xpm"
#include "pixmaps/cursor_zoom_in.xpm"
#include "pixmaps/cursor_zoom_out.xpm"
#include "pixmaps/cursor_hand_open.xpm"
#include "pixmaps/cursor_hand_closed.xpm"

#define GDK_INTERNAL_CURSOR -1

typedef struct {
	GdkCursor *cursor;
	gint hot_x, hot_y;
	const gchar **xpm;
} CursorDef;

static CursorDef cursors [] = {
	{ NULL, 17, 17, cursor_cross_xpm },
	{ NULL, GDK_INTERNAL_CURSOR,   GDK_CROSSHAIR,         NULL },
	{ NULL, GDK_INTERNAL_CURSOR,   GDK_ARROW,             NULL },
	{ NULL, GDK_INTERNAL_CURSOR,   GDK_FLEUR,             NULL },
	{ NULL, 24, 24, cursor_zoom_in_xpm },
	{ NULL, 24, 24, cursor_zoom_out_xpm },
	{ NULL, GDK_INTERNAL_CURSOR,   GDK_SB_H_DOUBLE_ARROW, NULL },
	{ NULL, GDK_INTERNAL_CURSOR,   GDK_SB_V_DOUBLE_ARROW, NULL },
	{ NULL, GDK_INTERNAL_CURSOR,   GDK_SIZING,            NULL },
	{ NULL, GDK_INTERNAL_CURSOR,   GDK_SIZING,            NULL },
	{ NULL, GDK_INTERNAL_CURSOR,   GDK_HAND2,             NULL },
	{ NULL, 10, 10, cursor_hand_open_xpm },
	{ NULL, 10, 10, cursor_hand_closed_xpm },
	{ NULL, GDK_INTERNAL_CURSOR,   GDK_XTERM,             NULL },
	{ NULL, 0,    0,  NULL }
};

static void
create_bitmap_and_mask_from_xpm (GdkBitmap **bitmap, GdkBitmap **mask, const gchar **xpm)
{
	gint height, width, colors;
	gchar pixmap_buffer [(32 * 32)/8];
	gchar mask_buffer [(32 * 32)/8];
	gint x, y, pix, yofs;
	gint transparent_color, black_color;

	sscanf (xpm [0], "%d %d %d %d", &height, &width, &colors, &pix);

	g_return_if_fail (height == 32);
	g_return_if_fail (width  == 32);
	g_return_if_fail (colors <= 3);

	transparent_color = ' ';
	black_color = '.';

	yofs = colors + 1;
	for (y = 0; y < 32; y++) {
		for (x = 0; x < 32;) {
			gchar value = 0, maskv = 0;

			for (pix = 0; pix < 8; pix++, x++) {
				if (xpm [y + yofs][x] != transparent_color) {
					maskv |= 1 << pix;

					/*
					 * Invert the colours here because it seems
					 * to workaround a bug the Matrox G100 Xserver?
					 * We reverse the foreground & background in the next
					 * routine to compensate.
					 */
					if (xpm [y + yofs][x] == black_color) {
						value |= 1 << pix;
					}
				}
			}
			pixmap_buffer [(y * 4 + x/8)-1] = value;
			mask_buffer [(y * 4 + x/8)-1] = maskv;
		}
	}
	*bitmap = gdk_bitmap_create_from_data (NULL, pixmap_buffer, 32, 32);
	*mask   = gdk_bitmap_create_from_data (NULL, mask_buffer, 32, 32);
}

void
e_cursors_init (void)
{
	gint i;

	e_color_init ();

	for (i = 0; cursors [i].hot_x; i++) {
		if (cursors [i].hot_x < 0)
			cursors [i].cursor = gdk_cursor_new (cursors [i].hot_y);
		else {
			GdkBitmap *bitmap = NULL, *mask = NULL;

			create_bitmap_and_mask_from_xpm (&bitmap, &mask, cursors [i].xpm);

			/* The foreground and background colours are reversed.
			 * See comment above for explanation.
			 */
			cursors [i].cursor =
				gdk_cursor_new_from_pixmap (
					bitmap, mask,
					&e_black, &e_white,
					cursors [i].hot_x,
					cursors [i].hot_y);

			g_object_unref (bitmap);
			g_object_unref (mask);
		}
	}

	g_return_if_fail (i == E_CURSOR_NUM_CURSORS);
}

void
e_cursors_shutdown (void)
{
	gint i;

	for (i = 0; cursors [i].hot_x; i++)
		gdk_cursor_unref (cursors [i].cursor);
}

/* Returns a cursor given its type */
GdkCursor *
e_cursor_get (ECursorType type)
{
	g_return_val_if_fail (type < E_CURSOR_NUM_CURSORS, NULL);

	return cursors [type].cursor;
}
