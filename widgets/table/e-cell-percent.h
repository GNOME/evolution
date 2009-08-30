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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ECellPercent - a subclass of ECellText used to show an integer percentage
 * in an ETable.
 */

#ifndef _E_CELL_PERCENT_H_
#define _E_CELL_PERCENT_H_

#include <table/e-cell-text.h>

#define E_CELL_PERCENT_TYPE        (e_cell_percent_get_type ())
#define E_CELL_PERCENT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_PERCENT_TYPE, ECellPercent))
#define E_CELL_PERCENT_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), E_CELL_PERCENT_TYPE, ECellPercentClass))
#define E_IS_CELL_NUMBER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_PERCENT_TYPE))
#define E_IS_CELL_NUMBER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_PERCENT_TYPE))

typedef struct {
	ECellText base;
} ECellPercent;

typedef struct {
	ECellTextClass parent_class;
} ECellPercentClass;

GType      e_cell_percent_get_type (void);
ECell     *e_cell_percent_new      (const gchar *fontname, GtkJustification justify);

#endif /* _E_CELL_PERCENT_H_ */
