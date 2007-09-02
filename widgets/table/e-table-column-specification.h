/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-column-specification.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
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
	int model_col;
	int compare_col;
	char *title;
	char *pixbuf;

	double expansion;
	int minimum_width;
	guint resizable : 1;
	guint disabled : 1;

	char *cell;
	char *compare;
	char *search;
	char *sortable;
	int priority;
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
