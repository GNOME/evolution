/*
 * e-table-sorter.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TABLE_SORTER_H
#define E_TABLE_SORTER_H

#include <e-util/e-sorter.h>
#include <e-util/e-table-header.h>
#include <e-util/e-table-model.h>
#include <e-util/e-table-sort-info.h>
#include <e-util/e-table-subset-variable.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_SORTER \
	(e_table_sorter_get_type ())
#define E_TABLE_SORTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_SORTER, ETableSorter))
#define E_TABLE_SORTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_SORTER, ETableSorterClass))
#define E_IS_TABLE_SORTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_SORTER))
#define E_IS_TABLE_SORTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_SORTER))
#define E_TABLE_SORTER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_SORTER, ETableSorterClass))

G_BEGIN_DECLS

typedef struct _ETableSorter ETableSorter;
typedef struct _ETableSorterClass ETableSorterClass;

struct _ETableSorter {
	GObject parent;

	ETableModel *source;
	ETableHeader *full_header;
	ETableSortInfo *sort_info;

	/* If needs_sorting is 0, then model_to_sorted
	 * and sorted_to_model are no-ops. */
	gint needs_sorting;

	gint *sorted;
	gint *backsorted;

	gulong table_model_changed_id;
	gulong table_model_row_changed_id;
	gulong table_model_cell_changed_id;
	gulong table_model_rows_inserted_id;
	gulong table_model_rows_deleted_id;
	gulong sort_info_changed_id;
	gulong group_info_changed_id;
};

struct _ETableSorterClass {
	GObjectClass parent_class;
};

GType		e_table_sorter_get_type		(void) G_GNUC_CONST;
ETableSorter *	e_table_sorter_new		(ETableModel *etm,
						 ETableHeader *full_header,
						 ETableSortInfo *sort_info);

G_END_DECLS

#endif /* E_TABLE_SORTER_H */
