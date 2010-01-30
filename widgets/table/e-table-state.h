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

#ifndef _E_TABLE_STATE_H_
#define _E_TABLE_STATE_H_

#include <glib-object.h>
#include <libxml/tree.h>
#include <table/e-table-sort-info.h>

G_BEGIN_DECLS

#define E_TABLE_STATE_TYPE        (e_table_state_get_type ())
#define E_TABLE_STATE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_STATE_TYPE, ETableState))
#define E_TABLE_STATE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_STATE_TYPE, ETableStateClass))
#define E_IS_TABLE_STATE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_STATE_TYPE))
#define E_IS_TABLE_STATE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_STATE_TYPE))
#define E_TABLE_STATE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_STATE_TYPE, ETableStateClass))

typedef struct {
	GObject base;

	ETableSortInfo *sort_info;
	gint             col_count;
	gint            *columns;
	gdouble         *expansions;
} ETableState;

typedef struct {
	GObjectClass parent_class;
} ETableStateClass;

GType        e_table_state_get_type          (void);
ETableState *e_table_state_new               (void);

ETableState *e_table_state_vanilla	     (gint col_count);

gboolean     e_table_state_load_from_file    (ETableState   *state,
					      const gchar    *filename);
void         e_table_state_load_from_string  (ETableState   *state,
					      const gchar    *xml);
void         e_table_state_load_from_node    (ETableState   *state,
					      const xmlNode *node);

void         e_table_state_save_to_file      (ETableState   *state,
					      const gchar    *filename);
gchar        *e_table_state_save_to_string    (ETableState   *state);
xmlNode     *e_table_state_save_to_node      (ETableState   *state,
					      xmlNode       *parent);
ETableState *e_table_state_duplicate         (ETableState   *state);

G_END_DECLS

#endif /* _E_TABLE_STATE_H_ */
