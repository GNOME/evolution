/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-utils.h
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

