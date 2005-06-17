/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-cell-toggle.h - Multi-state image toggle cell object.
 * Copyright 1999, 2000, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
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

#ifndef _E_CELL_TOGGLE_H_
#define _E_CELL_TOGGLE_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <table/e-cell.h>

G_BEGIN_DECLS

#define E_CELL_TOGGLE_TYPE        (e_cell_toggle_get_type ())
#define E_CELL_TOGGLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_TOGGLE_TYPE, ECellToggle))
#define E_CELL_TOGGLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_TOGGLE_TYPE, ECellToggleClass))
#define E_IS_CELL_TOGGLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_TOGGLE_TYPE))
#define E_IS_CELL_TOGGLE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_TOGGLE_TYPE))

typedef struct {
	ECell parent;

	int        border;
	int        n_states;
	GdkPixbuf **images;

	int        height;
} ECellToggle;

typedef struct {
	ECellClass parent_class;
} ECellToggleClass;

GType      e_cell_toggle_get_type  (void);
ECell     *e_cell_toggle_new       (int border, int n_states, GdkPixbuf **images);
void       e_cell_toggle_construct (ECellToggle *etog, int border,
				    int n_states, GdkPixbuf **images);

G_END_DECLS

#endif /* _E_CELL_TOGGLE_H_ */


