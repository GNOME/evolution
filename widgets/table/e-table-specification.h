/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-specification.h
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

#ifndef _E_TABLE_SPECIFICATION_H_
#define _E_TABLE_SPECIFICATION_H_

#include <gtk/gtkobject.h>
#include <tree.h>
#include <gal/widgets/e-selection-model.h>
#include <gal/e-table/e-table-state.h>
#include <gal/e-table/e-table-column-specification.h>
#include <gal/e-table/e-table-defines.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TABLE_SPECIFICATION_TYPE        (e_table_specification_get_type ())
#define E_TABLE_SPECIFICATION(o)          (GTK_CHECK_CAST ((o), E_TABLE_SPECIFICATION_TYPE, ETableSpecification))
#define E_TABLE_SPECIFICATION_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SPECIFICATION_TYPE, ETableSpecificationClass))
#define E_IS_TABLE_SPECIFICATION(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SPECIFICATION_TYPE))
#define E_IS_TABLE_SPECIFICATION_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SPECIFICATION_TYPE))

typedef struct {
	GtkObject base;

	ETableColumnSpecification **columns;
	ETableState *state;

	guint alternating_row_colors : 1;
	guint no_headers : 1;
	guint click_to_add : 1;
	guint click_to_add_end : 1;
	guint horizontal_draw_grid : 1;
	guint vertical_draw_grid : 1;
	guint draw_focus : 1;
	guint horizontal_scrolling : 1;
	guint allow_grouping : 1;
	GtkSelectionMode selection_mode;
	ECursorMode cursor_mode;

	char *click_to_add_message;
} ETableSpecification;

typedef struct {
	GtkObjectClass parent_class;
} ETableSpecificationClass;

GtkType              e_table_specification_get_type          (void);
ETableSpecification *e_table_specification_new               (void);

gboolean             e_table_specification_load_from_file    (ETableSpecification *specification,
							      const char          *filename);
gboolean             e_table_specification_load_from_string  (ETableSpecification *specification,
							      const char          *xml);
void                 e_table_specification_load_from_node    (ETableSpecification *specification,
							      const xmlNode       *node);

int                  e_table_specification_save_to_file      (ETableSpecification *specification,
							      const char          *filename);
char                *e_table_specification_save_to_string    (ETableSpecification *specification);
xmlNode             *e_table_specification_save_to_node      (ETableSpecification *specification,
							      xmlDoc              *doc);
ETableSpecification *e_table_specification_duplicate         (ETableSpecification *spec);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TABLE_SPECIFICATION_H_ */
