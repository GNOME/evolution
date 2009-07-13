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
 *		Chris Toshok <toshok@ximian.com>
 *		Chris Lahey  <clahey@ximina.com
 *
 * A majority of code taken from:
 * the ECellText renderer.
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CELL_VBOX_H_
#define _E_CELL_VBOX_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <table/e-cell.h>

G_BEGIN_DECLS

#define E_CELL_VBOX_TYPE        (e_cell_vbox_get_type ())
#define E_CELL_VBOX(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_VBOX_TYPE, ECellVbox))
#define E_CELL_VBOX_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_VBOX_TYPE, ECellVboxClass))
#define E_IS_CELL_VBOX(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_VBOX_TYPE))
#define E_IS_CELL_VBOX_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_VBOX_TYPE))

typedef struct {
	ECell parent;

	gint     subcell_count;
	ECell **subcells;
	gint    *model_cols;
} ECellVbox;

typedef struct {
	ECellView     cell_view;
	gint           subcell_view_count;
	ECellView   **subcell_views;
	gint          *model_cols;
} ECellVboxView;

typedef struct {
	ECellClass parent_class;
} ECellVboxClass;

GType    e_cell_vbox_get_type  (void);
ECell   *e_cell_vbox_new       (void);
void     e_cell_vbox_append    (ECellVbox *vbox,
				ECell     *subcell,
				gint model_col);

G_END_DECLS

#endif /* _E_CELL_VBOX_H_ */
