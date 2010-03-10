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

#ifndef _E_TABLE_SPECIFICATION_H_
#define _E_TABLE_SPECIFICATION_H_

#include <glib-object.h>
#include <libxml/tree.h>
#include <misc/e-selection-model.h>
#include <table/e-table-state.h>
#include <table/e-table-column-specification.h>
#include <table/e-table-defines.h>

G_BEGIN_DECLS

#define E_TYPE_TABLE_SPECIFICATION        (e_table_specification_get_type ())
#define E_TABLE_SPECIFICATION(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_TABLE_SPECIFICATION, ETableSpecification))
#define E_TABLE_SPECIFICATION_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_TABLE_SPECIFICATION, ETableSpecificationClass))
#define E_IS_TABLE_SPECIFICATION(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_TABLE_SPECIFICATION))
#define E_IS_TABLE_SPECIFICATION_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_TABLE_SPECIFICATION))
#define E_TABLE_SPECIFICATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TYPE_TABLE_SPECIFICATION, ETableSpecificationClass))

typedef struct {
	GObject base;

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
	guint horizontal_resize : 1;
	guint allow_grouping : 1;
	GtkSelectionMode selection_mode;
	ECursorMode cursor_mode;

	gchar *click_to_add_message;
	gchar *domain;
} ETableSpecification;

typedef struct {
	GObjectClass parent_class;
} ETableSpecificationClass;

GType                e_table_specification_get_type          (void);
ETableSpecification *e_table_specification_new               (void);

gboolean             e_table_specification_load_from_file    (ETableSpecification *specification,
							      const gchar          *filename);
gboolean             e_table_specification_load_from_string  (ETableSpecification *specification,
							      const gchar          *xml);
void                 e_table_specification_load_from_node    (ETableSpecification *specification,
							      const xmlNode       *node);

gint                  e_table_specification_save_to_file      (ETableSpecification *specification,
							      const gchar          *filename);
gchar                *e_table_specification_save_to_string    (ETableSpecification *specification);
xmlNode             *e_table_specification_save_to_node      (ETableSpecification *specification,
							      xmlDoc              *doc);
ETableSpecification *e_table_specification_duplicate         (ETableSpecification *spec);

G_END_DECLS

#endif /* _E_TABLE_SPECIFICATION_H_ */
