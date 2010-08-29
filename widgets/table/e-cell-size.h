/*
 * e-cell-size.h: Size item for e-table.
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_CELL_SIZE_H
#define E_CELL_SIZE_H

#include <table/e-cell-text.h>

/* Standard GObject macros */
#define E_TYPE_CELL_SIZE \
	(e_cell_size_get_type ())
#define E_CELL_SIZE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_SIZE, ECellSize))
#define E_CELL_SIZE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_SIZE, ECellSizeClass))
#define E_IS_CELL_SIZE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_SIZE))
#define E_IS_CELL_SIZE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_SIZE))
#define E_CELL_SIZE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_SIZE, ECellSizeClass))

G_BEGIN_DECLS

typedef struct _ECellSize ECellSize;
typedef struct _ECellSizeClass ECellSizeClass;

struct _ECellSize {
	ECellText parent;
};

struct _ECellSizeClass {
	ECellTextClass parent_class;
};

GType		e_cell_size_get_type		(void);
ECell *		e_cell_size_new			(const gchar *fontname,
						 GtkJustification justify);

G_END_DECLS

#endif /* E_CELL_SIZE_H */
