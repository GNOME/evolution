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

#ifndef _E_TABLE_UTILS_H_
#define _E_TABLE_UTILS_H_

#include <table/e-table-header.h>
#include <table/e-table-state.h>
#include <table/e-table-specification.h>
#include <table/e-table-extras.h>

G_BEGIN_DECLS

ETableHeader *e_table_state_to_header                    (GtkWidget           *widget,
							  ETableHeader        *full_header,
							  ETableState         *state);

ETableHeader *e_table_spec_to_full_header                (ETableSpecification *spec,
							  ETableExtras        *ete);

ETableCol    *e_table_util_calculate_current_search_col  (ETableHeader        *header,
							  ETableHeader        *full_header,
							  ETableSortInfo      *sort_info,
							  gboolean             always_search);

G_END_DECLS

#endif /* _E_TABLE_UTILS_H_ */

