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

#ifndef E_CELL_TOGGLE_H
#define E_CELL_TOGGLE_H

#include <libgnomecanvas/gnome-canvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <table/e-cell.h>

/* Standard GObject macros */
#define E_TYPE_CELL_TOGGLE \
	(e_cell_toggle_get_type ())
#define E_CELL_TOGGLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_TOGGLE, ECellToggle))
#define E_CELL_TOGGLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_TOGGLE, ECellToggleClass))
#define E_IS_CELL_TOGGLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_TOGGLE))
#define E_IS_CELL_TOGGLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_TOGGLE))
#define E_CELL_TOGGLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_TOGGLE, ECellToggleClass))

G_BEGIN_DECLS

typedef struct _ECellToggle ECellToggle;
typedef struct _ECellToggleClass ECellToggleClass;
typedef struct _ECellTogglePrivate ECellTogglePrivate;

struct _ECellToggle {
	ECell parent;
	ECellTogglePrivate *priv;
};

struct _ECellToggleClass {
	ECellClass parent_class;
};

GType		e_cell_toggle_get_type		(void);
ECell *		e_cell_toggle_new		(const gchar **icon_names,
						 guint n_icon_names);
void		e_cell_toggle_construct		(ECellToggle *cell_toggle,
						 const gchar **icon_names,
						 guint n_icon_names);
GPtrArray *	e_cell_toggle_get_pixbufs	(ECellToggle *cell_toggle);

G_END_DECLS

#endif /* E_CELL_TOGGLE_H */

