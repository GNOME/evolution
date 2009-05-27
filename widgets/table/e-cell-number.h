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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CELL_NUMBER_H_
#define _E_CELL_NUMBER_H_

#include <table/e-cell-text.h>

G_BEGIN_DECLS

#define E_CELL_NUMBER_TYPE        (e_cell_number_get_type ())
#define E_CELL_NUMBER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_NUMBER_TYPE, ECellNumber))
#define E_CELL_NUMBER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_NUMBER_TYPE, ECellNumberClass))
#define E_IS_CELL_NUMBER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_NUMBER_TYPE))
#define E_IS_CELL_NUMBER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_NUMBER_TYPE))

typedef struct {
	ECellText base;
} ECellNumber;

typedef struct {
	ECellTextClass parent_class;
} ECellNumberClass;

GType      e_cell_number_get_type (void);
ECell     *e_cell_number_new      (const gchar *fontname, GtkJustification justify);

G_END_DECLS

#endif /* _E_CELL_NUMBER_H_ */
