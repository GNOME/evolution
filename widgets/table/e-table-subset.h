/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-subset.h - Implements a table that contains a subset of another table.
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Miguel de Icaza <miguel@ximian.com>
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

#ifndef _E_TABLE_SUBSET_H_
#define _E_TABLE_SUBSET_H_

#include <glib-object.h>
#include <table/e-table-model.h>

G_BEGIN_DECLS

#define E_TABLE_SUBSET_TYPE        (e_table_subset_get_type ())
#define E_TABLE_SUBSET(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_SUBSET_TYPE, ETableSubset))
#define E_TABLE_SUBSET_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_SUBSET_TYPE, ETableSubsetClass))
#define E_IS_TABLE_SUBSET(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_SUBSET_TYPE))
#define E_IS_TABLE_SUBSET_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_SUBSET_TYPE))
#define E_TABLE_SUBSET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), E_TABLE_SUBSET_TYPE, ETableSubsetClass))

typedef struct {
	ETableModel base;

	ETableModel  *source;
	int  n_map;
	int *map_table;

	int last_access;

	int              table_model_pre_change_id;
	int              table_model_no_change_id;
	int              table_model_changed_id;
	int              table_model_row_changed_id;
	int              table_model_cell_changed_id;
	int              table_model_rows_inserted_id;
	int              table_model_rows_deleted_id;
} ETableSubset;

typedef struct {
	ETableModelClass parent_class;

	void (*proxy_model_pre_change)   (ETableSubset *etss, ETableModel *etm);
	void (*proxy_model_no_change)   (ETableSubset *etss, ETableModel *etm);
	void (*proxy_model_changed)      (ETableSubset *etss, ETableModel *etm);
	void (*proxy_model_row_changed)  (ETableSubset *etss, ETableModel *etm, int row);
	void (*proxy_model_cell_changed) (ETableSubset *etss, ETableModel *etm, int col, int row);
	void (*proxy_model_rows_inserted) (ETableSubset *etss, ETableModel *etm, int row, int count);
	void (*proxy_model_rows_deleted)  (ETableSubset *etss, ETableModel *etm, int row, int count);
} ETableSubsetClass;

GType        e_table_subset_get_type           (void);
ETableModel *e_table_subset_new                (ETableModel  *etm,
						int           n_vals);
ETableModel *e_table_subset_construct          (ETableSubset *ets,
						ETableModel  *source,
						int           nvals);

int          e_table_subset_model_to_view_row  (ETableSubset *ets,
						int           model_row);
int          e_table_subset_view_to_model_row  (ETableSubset *ets,
						int           view_row);

ETableModel *e_table_subset_get_toplevel       (ETableSubset *table_model);

void         e_table_subset_print_debugging    (ETableSubset *table_model);

G_END_DECLS

#endif /* _E_TABLE_SUBSET_H_ */

