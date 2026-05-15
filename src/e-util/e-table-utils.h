/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_UTILS_H_
#define _E_TABLE_UTILS_H_

#include <e-util/e-table-extras.h>
#include <e-util/e-table-header.h>
#include <e-util/e-table-specification.h>
#include <e-util/e-table-state.h>

G_BEGIN_DECLS

ETableHeader *	e_table_state_to_header		(GtkWidget *widget,
						 ETableHeader *full_header,
						 ETableState *state);

ETableHeader *	e_table_spec_to_full_header	(ETableSpecification *spec,
						 ETableExtras *ete);

ETableCol *	e_table_util_calculate_current_search_col
						(ETableHeader *header,
						 ETableHeader *full_header,
						 ETableSortInfo *sort_info,
						 gboolean always_search);

G_END_DECLS

#endif /* _E_TABLE_UTILS_H_ */

