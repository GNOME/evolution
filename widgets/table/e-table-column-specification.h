/*
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

#ifndef _E_TABLE_COLUMN_SPECIFICATION_H_
#define _E_TABLE_COLUMN_SPECIFICATION_H_

#include <glib.h>
#include <glib-object.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

#define E_TABLE_COLUMN_SPECIFICATION_TYPE        (e_table_column_specification_get_type ())
#define E_TABLE_COLUMN_SPECIFICATION(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_COLUMN_SPECIFICATION_TYPE, ETableColumnSpecification))
#define E_TABLE_COLUMN_SPECIFICATION_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_COLUMN_SPECIFICATION_TYPE, ETableColumnSpecificationClass))
#define E_IS_TABLE_COLUMN_SPECIFICATION(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_COLUMN_SPECIFICATION_TYPE))
#define E_IS_TABLE_COLUMN_SPECIFICATION_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_COLUMN_SPECIFICATION_TYPE))
#define E_TABLE_COLUMN_SPECIFICATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_COLUMN_SPECIFICATION_TYPE, ETableColumnSpecificationClass))

typedef struct {
	GObject base;
	gint model_col;
	gint compare_col;
	gchar *title;
	gchar *pixbuf;

	double expansion;
	gint minimum_width;
	guint resizable : 1;
	guint disabled : 1;

	gchar *cell;
	gchar *compare;
	gchar *search;
	gchar *sortable;
	gint priority;
} ETableColumnSpecification;

typedef struct {
	GObjectClass parent_class;
} ETableColumnSpecificationClass;

GType                      e_table_column_specification_get_type        (void);

ETableColumnSpecification *e_table_column_specification_new             (void);

void                       e_table_column_specification_load_from_node  (ETableColumnSpecification *state,
									 const xmlNode             *node);
xmlNode                   *e_table_column_specification_save_to_node    (ETableColumnSpecification *state,
									 xmlNode                   *parent);

G_END_DECLS

#endif /* _E_TABLE_COLUMN_SPECIFICATION_H_ */
