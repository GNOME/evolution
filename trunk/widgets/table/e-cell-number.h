/* 
 * e-cell-number.h - Number item for e-table.
 * Copyright 2001, Ximian, Inc.
 *
 * Authors:
 *  Chris Lahey <clahey@ximian.com>
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
ECell     *e_cell_number_new      (const char *fontname, GtkJustification justify);

G_END_DECLS

#endif /* _E_CELL_NUMBER_H_ */
