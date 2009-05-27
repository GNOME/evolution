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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_TABLE_TOOLTIP_H_
#define _E_TABLE_TOOLTIP_H_

#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

typedef struct {
	gint timer;
	gint col, row;
	gint row_height;
	gint x, y;
	gint cx, cy;
	GdkColor *foreground;
	GdkColor *background;
	GnomeCanvasItem *eti;
} ETableTooltip;

G_END_DECLS

#endif
