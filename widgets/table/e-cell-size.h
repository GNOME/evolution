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

#ifndef _E_CELL_SIZE_H_
#define _E_CELL_SIZE_H_

#include <table/e-cell-text.h>

G_BEGIN_DECLS

#define E_CELL_SIZE_TYPE        (e_cell_size_get_type ())
#define E_CELL_SIZE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_SIZE_TYPE, ECellSize))
#define E_CELL_SIZE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_SIZE_TYPE, ECellSizeClass))
#define E_IS_CELL_SIZE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_SIZE_TYPE))
#define E_IS_CELL_SIZE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_SIZE_TYPE))

typedef struct {
	ECellText base;
} ECellSize;

typedef struct {
	ECellTextClass parent_class;
} ECellSizeClass;

GType      e_cell_size_get_type (void);
ECell     *e_cell_size_new      (const gchar *fontname, GtkJustification justify);

G_END_DECLS

#endif /* _E_CELL_SIZE_H_ */
