/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-cell-vbox.h - Vbox cell object.
 * Copyright 1999 - 2002, Ximian, Inc.
 *
 * Authors:
 *   Chris Toshok <toshok@ximian.com>
 *   Chris Lahey  <clahey@ximina.com
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_CELL_VBOX_H_
#define _E_CELL_VBOX_H_

#include <libgnomeui/gnome-canvas.h>
#include <gal/e-table/e-cell.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

#define E_CELL_VBOX_TYPE        (e_cell_vbox_get_type ())
#define E_CELL_VBOX(o)          (GTK_CHECK_CAST ((o), E_CELL_VBOX_TYPE, ECellVbox))
#define E_CELL_VBOX_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_VBOX_TYPE, ECellVboxClass))
#define E_IS_CELL_VBOX(o)       (GTK_CHECK_TYPE ((o), E_CELL_VBOX_TYPE))
#define E_IS_CELL_VBOX_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_VBOX_TYPE))

typedef struct {
	ECell parent;

	int     subcell_count;
	ECell **subcells;
	int    *model_cols;
} ECellVbox;

typedef struct {
	ECellClass parent_class;
} ECellVboxClass;

GtkType  e_cell_vbox_get_type  (void);
ECell   *e_cell_vbox_new       (void);
void     e_cell_vbox_append    (ECellVbox *vbox,
				ECell     *subcell,
				int model_col);


END_GNOME_DECLS

#endif /* _E_CELL_VBOX_H_ */


