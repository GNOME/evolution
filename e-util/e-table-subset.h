/*
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
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TABLE_SUBSET_H
#define E_TABLE_SUBSET_H

#include <e-util/e-table-model.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_SUBSET \
	(e_table_subset_get_type ())
#define E_TABLE_SUBSET(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_SUBSET, ETableSubset))
#define E_TABLE_SUBSET_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_SUBSET, ETableSubsetClass))
#define E_IS_TABLE_SUBSET(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_SUBSET))
#define E_IS_TABLE_SUBSET_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_SUBSET))
#define E_TABLE_SUBSET_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_SUBSET, ETableSubsetClass))

G_BEGIN_DECLS

typedef struct _ETableSubset ETableSubset;
typedef struct _ETableSubsetClass ETableSubsetClass;
typedef struct _ETableSubsetPrivate ETableSubsetPrivate;

struct _ETableSubset {
	GObject parent;
	ETableSubsetPrivate *priv;

	/* protected - subclasses modify this directly */
	gint n_map;
	gint *map_table;
};

struct _ETableSubsetClass {
	GObjectClass parent_class;

	void		(*proxy_model_pre_change)
						(ETableSubset *table_subset,
						 ETableModel *source_model);
	void		(*proxy_model_no_change)
						(ETableSubset *table_subset,
						 ETableModel *source_model);
	void		(*proxy_model_changed)	(ETableSubset *table_subset,
						 ETableModel *source_model);
	void		(*proxy_model_row_changed)
						(ETableSubset *table_subset,
						 ETableModel *source_model,
						 gint row);
	void		(*proxy_model_cell_changed)
						(ETableSubset *table_subset,
						 ETableModel *source_model,
						 gint col,
						 gint row);
	void		(*proxy_model_rows_inserted)
						(ETableSubset *table_subset,
						 ETableModel *source_model,
						 gint row,
						 gint count);
	void		(*proxy_model_rows_deleted)
						(ETableSubset *table_subset,
						 ETableModel *source_model,
						 gint row,
						 gint count);
};

GType		e_table_subset_get_type		(void) G_GNUC_CONST;
ETableModel *	e_table_subset_new		(ETableModel *source_model,
						 gint n_vals);
ETableModel *	e_table_subset_construct	(ETableSubset *table_subset,
						 ETableModel *source_model,
						 gint nvals);
ETableModel *	e_table_subset_get_source_model	(ETableSubset *table_subset);
gint		e_table_subset_model_to_view_row
						(ETableSubset *table_subset,
						 gint model_row);
gint		e_table_subset_view_to_model_row
						(ETableSubset *table_subset,
						 gint view_row);
ETableModel *	e_table_subset_get_toplevel	(ETableSubset *table_subset);
void		e_table_subset_print_debugging	(ETableSubset *table_subset);

G_END_DECLS

#endif /* E_TABLE_SUBSET_H */

