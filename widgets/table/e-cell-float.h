/*
 * e-cell-float.h - Float item for e-table.
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
 *		Mikael Hallendal <micke@imendio.com>
 *
 * Derived from e-cell-number by Chris Lahey <clahey@ximian.com>
 * ECellFloat - Float item for e-table.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CELL_FLOAT_H_
#define _E_CELL_FLOAT_H_

#include <table/e-cell-text.h>

G_BEGIN_DECLS

#define E_CELL_FLOAT_TYPE        (e_cell_float_get_type ())
#define E_CELL_FLOAT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_FLOAT_TYPE, ECellFloat))
#define E_CELL_FLOAT_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_FLOAT_TYPE, ECellFloatClass))
#define E_IS_CELL_FLOAT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_FLOAT_TYPE))
#define E_IS_CELL_FLOAT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_FLOAT_TYPE))

typedef struct {
	ECellText base;
} ECellFloat;

typedef struct {
	ECellTextClass parent_class;
} ECellFloatClass;

GType      e_cell_float_get_type (void);
ECell     *e_cell_float_new      (const char *fontname, GtkJustification justify);

G_END_DECLS

#endif /* _E_CELL_FLOAT_H_ */
