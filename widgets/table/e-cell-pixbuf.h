/*
 * e-cell-pixbuf.h - An ECell that displays a GdkPixbuf
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
 *		Vladimir Vukicevic <vladimir@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CELL_PIXBUF_H_
#define _E_CELL_PIXBUF_H_

#include <table/e-table.h>

#define E_CELL_PIXBUF_TYPE		(e_cell_pixbuf_get_type ())
#define E_CELL_PIXBUF(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_PIXBUF_TYPE, ECellPixbuf))
#define E_CELL_PIXBUF_CLASS(k)	(G_TYPE_CHECK_INSTANCE_CAST_CLASS ((k), E_CELL_PIXBUF_TYPE, ECellPixbufClass))
#define E_IS_CELL_PIXBUF(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_PIXBUF_TYPE))
#define E_IS_CELL_PIXBUF_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_PIXBUF_TYPE))

typedef struct _ECellPixbuf ECellPixbuf;
typedef struct _ECellPixbufClass ECellPixbufClass;

struct _ECellPixbuf {
    ECell parent;

	gint selected_column;
	gint focused_column;
	gint unselected_column;
};

struct _ECellPixbufClass {
    ECellClass parent_class;
};

GType   e_cell_pixbuf_get_type (void);
ECell *e_cell_pixbuf_new (void);
void e_cell_pixbuf_construct (ECellPixbuf *ecp);

#endif
