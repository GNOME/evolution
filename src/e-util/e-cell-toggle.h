/*
 *
 * Multi-state image toggle cell object.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CELL_TOGGLE_H
#define E_CELL_TOGGLE_H

#include <libgnomecanvas/libgnomecanvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <e-util/e-cell.h>

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

GType		e_cell_toggle_get_type		(void) G_GNUC_CONST;
ECell *		e_cell_toggle_new		(const gchar **icon_names,
						 guint n_icon_names);
void		e_cell_toggle_construct		(ECellToggle *cell_toggle,
						 const gchar **icon_names,
						 guint n_icon_names);

void		e_cell_toggle_set_icon_descriptions	(ECellToggle *cell_toggle,
							 const gchar **descriptions,
							 gint n_descriptions);

const gchar *	e_cell_toggle_get_icon_description	(ECellToggle *cell_toggle,
							 gint n);
G_END_DECLS

#endif /* E_CELL_TOGGLE_H */

