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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_TABLE_COLUMN_SPECIFICATION_H_
#define _E_TABLE_COLUMN_SPECIFICATION_H_

#include <glib.h>
#include <gtk/gtkobject.h>
#include <tree.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define E_TABLE_COLUMN_SPECIFICATION_TYPE        (e_table_column_specification_get_type ())
#define E_TABLE_COLUMN_SPECIFICATION(o)          (GTK_CHECK_CAST ((o), E_TABLE_COLUMN_SPECIFICATION_TYPE, ETableColumnSpecification))
#define E_TABLE_COLUMN_SPECIFICATION_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_COLUMN_SPECIFICATION_TYPE, ETableColumnSpecificationClass))
#define E_IS_TABLE_COLUMN_SPECIFICATION(o)       (GTK_CHECK_TYPE ((o), E_TABLE_COLUMN_SPECIFICATION_TYPE))
#define E_IS_TABLE_COLUMN_SPECIFICATION_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_COLUMN_SPECIFICATION_TYPE))

typedef struct {
	GtkObject base;
	int model_col;
	char *title;
	char *pixbuf;

	double expansion;
	int minimum_width;
	guint resizable : 1;
	guint disabled : 1;

	char *cell;
	char *compare;
	int priority;
} ETableColumnSpecification;

typedef struct {
	GtkObjectClass parent_class;
} ETableColumnSpecificationClass;

GtkType                    e_table_column_specification_get_type        (void);

ETableColumnSpecification *e_table_column_specification_new             (void);

void                       e_table_column_specification_load_from_node  (ETableColumnSpecification *state,
									 const xmlNode             *node);
xmlNode                   *e_table_column_specification_save_to_node    (ETableColumnSpecification *state,
									 xmlNode                   *parent);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TABLE_COLUMN_SPECIFICATION_H_ */
