/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-cell-hbox.h - Hbox cell object.
 * Copyright 2006, Novell, Inc.
 *
 * Authors:
 *   Srinivasa Ragavan <sragavan@novell.com>
 *
 * A majority of code taken from:
 *
 * the ECellText renderer.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _E_CELL_HBOX_H_
#define _E_CELL_HBOX_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <table/e-cell.h>

G_BEGIN_DECLS

#define E_CELL_HBOX_TYPE        (e_cell_hbox_get_type ())
#define E_CELL_HBOX(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_HBOX_TYPE, ECellHbox))
#define E_CELL_HBOX_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_HBOX_TYPE, ECellHboxClass))
#define E_IS_CELL_HBOX(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_HBOX_TYPE))
#define E_IS_CELL_HBOX_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_HBOX_TYPE))

typedef struct {
	ECell parent;

	int     subcell_count;
	ECell **subcells;
	int    *model_cols;
	int    *def_size_cols;
} ECellHbox;

typedef struct {
	ECellView     cell_view;
	int           subcell_view_count;
	ECellView   **subcell_views;
	int          *model_cols;
	int    *def_size_cols;
} ECellHboxView;

typedef struct {
	ECellClass parent_class;
} ECellHboxClass;

GType    e_cell_hbox_get_type  (void);
ECell   *e_cell_hbox_new       (void);
void     e_cell_hbox_append    (ECellHbox *vbox,
				ECell     *subcell,
				int model_col,
	                        int size);


G_END_DECLS

#endif /* _E_CELL_HBOX_H_ */
