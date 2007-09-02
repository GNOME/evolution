/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-tooltip.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _E_TABLE_TOOLTIP_H_
#define _E_TABLE_TOOLTIP_H_

#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

typedef struct {
	gint timer;
	int col, row;
	int row_height;
	int x, y;
	int cx, cy;
	GdkColor *foreground;
	GdkColor *background;
	GnomeCanvasItem *eti;
} ETableTooltip;

G_END_DECLS

#endif
