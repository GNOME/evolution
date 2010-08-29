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

#ifndef E_CELL_DATE_H
#define E_CELL_DATE_H

#include <table/e-cell-text.h>

/* Standard GObject macros */
#define E_TYPE_CELL_DATE \
	(e_cell_date_get_type ())
#define E_CELL_DATE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_DATE, ECellDate))
#define E_CELL_DATE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_DATE, ECellDateClass))
#define E_IS_CELL_DATE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_DATE))
#define E_IS_CELL_DATE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_DATE))
#define E_CELL_DATE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_DATE, ECellDateClass))

G_BEGIN_DECLS

typedef struct _ECellDate ECellDate;
typedef struct _ECellDateClass ECellDateClass;

struct _ECellDate {
	ECellText parent;
};

struct _ECellDateClass {
	ECellTextClass parent_class;
};

GType		e_cell_date_get_type		(void);
ECell *		e_cell_date_new			(const gchar *fontname,
						 GtkJustification justify);
void		e_cell_date_set_format_component
						(ECellDate *ecd,
						 const gchar *fmt_component);

G_END_DECLS

#endif /* E_CELL_DATE_H */
