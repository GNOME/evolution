/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-state.h
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

#ifndef _E_TABLE_STATE_H_
#define _E_TABLE_STATE_H_

#include <gtk/gtkobject.h>
#include <tree.h>
#include <gal/e-table/e-table-sort-info.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define E_TABLE_STATE_TYPE        (e_table_state_get_type ())
#define E_TABLE_STATE(o)          (GTK_CHECK_CAST ((o), E_TABLE_STATE_TYPE, ETableState))
#define E_TABLE_STATE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_STATE_TYPE, ETableStateClass))
#define E_IS_TABLE_STATE(o)       (GTK_CHECK_TYPE ((o), E_TABLE_STATE_TYPE))
#define E_IS_TABLE_STATE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_STATE_TYPE))

typedef struct {
	GtkObject base;

	ETableSortInfo *sort_info;
	int             col_count;
	int            *columns;
	double         *expansions;
} ETableState;

typedef struct {
	GtkObjectClass parent_class;
} ETableStateClass;

GtkType      e_table_state_get_type          (void);
ETableState *e_table_state_new               (void);

gboolean     e_table_state_load_from_file    (ETableState   *state,
					      const char    *filename);
void         e_table_state_load_from_string  (ETableState   *state,
					      const char    *xml);
void         e_table_state_load_from_node    (ETableState   *state,
					      const xmlNode *node);

void         e_table_state_save_to_file      (ETableState   *state,
					      const char    *filename);
char        *e_table_state_save_to_string    (ETableState   *state);
xmlNode     *e_table_state_save_to_node      (ETableState   *state,
					      xmlNode       *parent);
ETableState *e_table_state_duplicate         (ETableState   *state);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TABLE_STATE_H_ */
