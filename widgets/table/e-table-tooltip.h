/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
