/*
 * e-table-sort-info.h
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TABLE_SORT_INFO_H
#define E_TABLE_SORT_INFO_H

#include <glib-object.h>
#include <libxml/tree.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_SORT_INFO \
	(e_table_sort_info_get_type ())
#define E_TABLE_SORT_INFO(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_SORT_INFO, ETableSortInfo))
#define E_TABLE_SORT_INFO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_SORT_INFO, ETableSortInfoClass))
#define E_IS_TABLE_SORT_INFO(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_SORT_INFO))
#define E_IS_TABLE_SORT_INFO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_SORT_INFO))
#define E_TABLE_SORT_INFO_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_SORT_INFO, ETableSortInfoClass))

G_BEGIN_DECLS

/* Avoid a circular dependency. */
struct _ETableSpecification;

typedef struct _ETableSortColumn ETableSortColumn;

typedef struct _ETableSortInfo ETableSortInfo;
typedef struct _ETableSortInfoClass ETableSortInfoClass;
typedef struct _ETableSortInfoPrivate ETableSortInfoPrivate;

struct _ETableSortColumn {
	guint column : 31;
	guint ascending : 1;
};

struct _ETableSortInfo {
	GObject parent;
	ETableSortInfoPrivate *priv;

	gint group_count;
	ETableSortColumn *groupings;
	gint sort_count;
	ETableSortColumn *sortings;

	gboolean can_group;
};

struct _ETableSortInfoClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*sort_info_changed)	(ETableSortInfo *sort_info);
	void		(*group_info_changed)	(ETableSortInfo *sort_info);
};

GType		e_table_sort_info_get_type	(void) G_GNUC_CONST;
ETableSortInfo *e_table_sort_info_new		(struct _ETableSpecification *specification);
struct _ETableSpecification *
		e_table_sort_info_ref_specification
						(ETableSortInfo *sort_info);
gboolean	e_table_sort_info_get_can_group (ETableSortInfo *sort_info);
void		e_table_sort_info_set_can_group	(ETableSortInfo *sort_info,
						 gboolean can_group);
guint		e_table_sort_info_grouping_get_count
						(ETableSortInfo *sort_info);
void		e_table_sort_info_grouping_truncate
						(ETableSortInfo *sort_info,
						 gint length);
ETableSortColumn
		e_table_sort_info_grouping_get_nth
						(ETableSortInfo *sort_info,
						 gint n);
void		e_table_sort_info_grouping_set_nth
						(ETableSortInfo *sort_info,
						 gint n,
						 ETableSortColumn column);

guint		e_table_sort_info_sorting_get_count
						(ETableSortInfo *sort_info);
void		e_table_sort_info_sorting_truncate
						(ETableSortInfo *sort_info,
						 gint length);
ETableSortColumn
		e_table_sort_info_sorting_get_nth
						(ETableSortInfo *sort_info,
						 gint n);
void		e_table_sort_info_sorting_set_nth
						(ETableSortInfo *sort_info,
						 gint n,
						 ETableSortColumn column);
void		e_table_sort_info_load_from_node
						(ETableSortInfo *sort_info,
						 xmlNode *node,
						 gdouble state_version);
xmlNode *	e_table_sort_info_save_to_node	(ETableSortInfo *sort_info,
						 xmlNode *parent);
ETableSortInfo *e_table_sort_info_duplicate	(ETableSortInfo *sort_info);

G_END_DECLS

#endif /* E_TABLE_SORT_INFO_H */
