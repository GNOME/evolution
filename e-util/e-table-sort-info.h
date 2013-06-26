/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_SORT_INFO_H_
#define _E_TABLE_SORT_INFO_H_

#include <glib-object.h>
#include <libxml/tree.h>

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

typedef struct _ETableSortColumn ETableSortColumn;

typedef struct _ETableSortInfo ETableSortInfo;
typedef struct _ETableSortInfoClass ETableSortInfoClass;

struct _ETableSortColumn {
	guint column : 31;
	guint ascending : 1;
};

struct _ETableSortInfo {
	GObject parent;

	gint group_count;
	ETableSortColumn *groupings;
	gint sort_count;
	ETableSortColumn *sortings;

	guint sort_info_changed : 1;
	guint group_info_changed : 1;

	guint can_group : 1;
};

struct _ETableSortInfoClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*sort_info_changed)	(ETableSortInfo *info);
	void		(*group_info_changed)	(ETableSortInfo *info);
};

GType		e_table_sort_info_get_type	(void) G_GNUC_CONST;

guint		e_table_sort_info_grouping_get_count
						(ETableSortInfo *info);
void		e_table_sort_info_grouping_truncate
						(ETableSortInfo *info,
						 gint length);
ETableSortColumn
		e_table_sort_info_grouping_get_nth
						(ETableSortInfo *info,
						 gint n);
void		e_table_sort_info_grouping_set_nth
						(ETableSortInfo *info,
						 gint n,
						 ETableSortColumn column);

guint		e_table_sort_info_sorting_get_count
						(ETableSortInfo *info);
void		e_table_sort_info_sorting_truncate
						(ETableSortInfo *info,
						 gint length);
ETableSortColumn
		e_table_sort_info_sorting_get_nth
						(ETableSortInfo *info,
						 gint n);
void		e_table_sort_info_sorting_set_nth
						(ETableSortInfo *info,
						 gint n,
						 ETableSortColumn column);

ETableSortInfo *e_table_sort_info_new		(void);
void		e_table_sort_info_load_from_node
						(ETableSortInfo *info,
						 xmlNode *node,
						 gdouble state_version);
xmlNode *	e_table_sort_info_save_to_node	(ETableSortInfo *info,
						 xmlNode *parent);
ETableSortInfo *e_table_sort_info_duplicate	(ETableSortInfo *info);
void		e_table_sort_info_set_can_group	(ETableSortInfo *info,
						 gboolean can_group);
gboolean	e_table_sort_info_get_can_group (ETableSortInfo *info);

G_END_DECLS

#endif /* _E_TABLE_SORT_INFO_H_ */
