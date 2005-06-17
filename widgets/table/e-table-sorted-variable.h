/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-sorted-variable.h
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

#ifndef _E_TABLE_SORTED_VARIABLE_H_
#define _E_TABLE_SORTED_VARIABLE_H_

#include <glib-object.h>
#include <table/e-table-model.h>
#include <table/e-table-subset-variable.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-header.h>

G_BEGIN_DECLS

#define E_TABLE_SORTED_VARIABLE_TYPE        (e_table_sorted_variable_get_type ())
#define E_TABLE_SORTED_VARIABLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_SORTED_VARIABLE_TYPE, ETableSortedVariable))
#define E_TABLE_SORTED_VARIABLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_SORTED_VARIABLE_TYPE, ETableSortedVariableClass))
#define E_IS_TABLE_SORTED_VARIABLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_SORTED_VARIABLE_TYPE))
#define E_IS_TABLE_SORTED_VARIABLE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_SORTED_VARIABLE_TYPE))
#define E_TABLE_SORTED_VARIABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_SORTED_VARIABLE_TYPE, ETableSortedVariableClass))

typedef struct {
	ETableSubsetVariable base;

	ETableSortInfo *sort_info;
	
	ETableHeader *full_header;

	int              sort_info_changed_id;
	int              sort_idle_id;
	int		 insert_idle_id;
	int		 insert_count;

} ETableSortedVariable;

typedef struct {
	ETableSubsetVariableClass parent_class;
} ETableSortedVariableClass;

GType        e_table_sorted_variable_get_type (void);
ETableModel *e_table_sorted_variable_new      (ETableModel *etm, ETableHeader *header, ETableSortInfo *sort_info);

G_END_DECLS

#endif /* _E_TABLE_SORTED_VARIABLE_H_ */
