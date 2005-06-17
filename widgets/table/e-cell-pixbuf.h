/*
 * e-cell-pixbuf.h - An ECell that displays a GdkPixbuf
 * Copyright 2001, Ximian, Inc.
 *
 * Authors:
 *  Vladimir Vukicevic <vladimir@ximian.com>
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

	int selected_column;
	int focused_column;
	int unselected_column;
};

struct _ECellPixbufClass {
    ECellClass parent_class;
};

GType   e_cell_pixbuf_get_type (void);
ECell *e_cell_pixbuf_new (void);
void e_cell_pixbuf_construct (ECellPixbuf *ecp);

#endif
