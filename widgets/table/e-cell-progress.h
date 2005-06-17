/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-cell-progress.h - Progress display cell object.
 * Copyright 1999-2002, Ximian, Inc.
 * Copyright 2001, 2002, Krisztian Pifko <monsta@users.sourceforge.net>
 *
 * Authors:
 *   Krisztian Pifko <monsta@users.sourceforge.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_CELL_PROGRESS_H_
#define _E_CELL_PROGRESS_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <table/e-cell.h>

G_BEGIN_DECLS

#define E_CELL_PROGRESS_TYPE        (e_cell_progress_get_type ())
#define E_CELL_PROGRESS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_PROGRESS_TYPE, ECellProgress))
#define E_CELL_PROGRESS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_PROGRESS_TYPE, ECellProgressClass))
#define E_IS_CELL_PROGRESS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_PROGRESS_TYPE))
#define E_IS_CELL_PROGRESS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_PROGRESS_TYPE))

typedef struct {
	ECell parent;

	int        padding;
	int        border;
	int        min;
	int        max;
	guchar 	   red;
	guchar 	   green;
	guchar 	   blue;

	guchar *buffer;
	GdkPixbuf *image;

	int        width;
	int        height;
} ECellProgress;

typedef struct {
	ECellClass parent_class;
} ECellProgressClass;

GType      e_cell_progress_get_type  (void);
ECell     *e_cell_progress_new       (int min, int max, int width, int height);
void       e_cell_progress_construct (ECellProgress *eprog, int padding, int border,
				      int min, int max, int width, int height, guchar red, guchar green, guchar blue);
void	   e_cell_progress_set_padding (ECellProgress *eprog, int padding);
void	   e_cell_progress_set_border (ECellProgress *eprog, int border);
void	   e_cell_progress_set_color (ECellProgress *eprog, guchar red, guchar green, guchar blue);

G_END_DECLS

#endif /* _E_CELL_PROGRESS_H_ */


