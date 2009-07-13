/*
 *
 * Multi-state image toggle cell object.
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
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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

	gint        border;
	gint        n_states;
	GdkPixbuf **images;

	gint        height;
} ECellToggle;

typedef struct {
	ECellClass parent_class;
} ECellToggleClass;

GType      e_cell_toggle_get_type  (void);
ECell     *e_cell_toggle_new       (gint border, gint n_states, GdkPixbuf **images);
void       e_cell_toggle_construct (ECellToggle *etog, gint border,
				    gint n_states, GdkPixbuf **images);

G_END_DECLS

#endif /* _E_CELL_TOGGLE_H_ */

