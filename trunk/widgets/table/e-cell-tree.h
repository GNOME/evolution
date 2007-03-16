/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-cell-tree.h - Tree cell object.
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Toshok <toshok@ximian.com>
 *
 * A majority of code taken from:
 *
 * the ECellText renderer.
 * Copyright 1998, The Free Software Foundation
 * Copyright 1999, 2000, Ximian, Inc.
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

#ifndef _E_CELL_TREE_H_
#define _E_CELL_TREE_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <table/e-cell.h>

G_BEGIN_DECLS

#define E_CELL_TREE_TYPE        (e_cell_tree_get_type ())
#define E_CELL_TREE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_TREE_TYPE, ECellTree))
#define E_CELL_TREE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_TREE_TYPE, ECellTreeClass))
#define E_IS_CELL_TREE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_TREE_TYPE))
#define E_IS_CELL_TREE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_TREE_TYPE))


typedef struct {
	ECell parent;

	gboolean draw_lines;

	GdkPixbuf   *open_pixbuf;
	GdkPixbuf   *closed_pixbuf;

	ECell *subcell;
} ECellTree;

typedef struct {
	ECellClass parent_class;
} ECellTreeClass;

GType      e_cell_tree_get_type (void);
ECell     *e_cell_tree_new      (GdkPixbuf *open_pixbuf,
				 GdkPixbuf *closed_pixbuf,
				 gboolean draw_lines,
				 ECell *subcell);
void       e_cell_tree_construct (ECellTree *ect,
				  GdkPixbuf *open_pixbuf,
				  GdkPixbuf *closed_pixbuf,
				  gboolean draw_lines,
				  ECell *subcell);

ECellView *e_cell_tree_view_get_subcell_view (ECellView *ect);

G_END_DECLS

#endif /* _E_CELL_TREE_H_ */


