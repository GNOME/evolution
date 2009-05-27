/*
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

#ifndef _E_TABLE_SORT_INFO_H_
#define _E_TABLE_SORT_INFO_H_

#include <glib-object.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

#define E_TABLE_SORT_INFO_TYPE        (e_table_sort_info_get_type ())
#define E_TABLE_SORT_INFO(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_SORT_INFO_TYPE, ETableSortInfo))
#define E_TABLE_SORT_INFO_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_SORT_INFO_TYPE, ETableSortInfoClass))
#define E_IS_TABLE_SORT_INFO(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_SORT_INFO_TYPE))
#define E_IS_TABLE_SORT_INFO_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_SORT_INFO_TYPE))
#define E_TABLE_SORT_INFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_SORT_INFO_TYPE, ETableSortInfoClass))

typedef struct _ETableSortColumn ETableSortColumn;

struct _ETableSortColumn {
	guint column : 31;
	guint ascending : 1;
};

typedef struct {
	GObject   base;

	gint group_count;
	ETableSortColumn *groupings;
	gint sort_count;
	ETableSortColumn *sortings;

	guint frozen : 1;
	guint sort_info_changed : 1;
	guint group_info_changed : 1;

	guint can_group : 1;
} ETableSortInfo;

typedef struct {
	GObjectClass parent_class;

	/*
	 * Signals
	 */
	void        (*sort_info_changed)      (ETableSortInfo *info);
	void        (*group_info_changed)     (ETableSortInfo *info);
} ETableSortInfoClass;

GType             e_table_sort_info_get_type            (void);

void              e_table_sort_info_freeze              (ETableSortInfo   *info);
void              e_table_sort_info_thaw                (ETableSortInfo   *info);

guint             e_table_sort_info_grouping_get_count  (ETableSortInfo   *info);
void              e_table_sort_info_grouping_truncate   (ETableSortInfo   *info,
							 gint               length);
ETableSortColumn  e_table_sort_info_grouping_get_nth    (ETableSortInfo   *info,
							 gint               n);
void              e_table_sort_info_grouping_set_nth    (ETableSortInfo   *info,
							 gint               n,
							 ETableSortColumn  column);

guint             e_table_sort_info_sorting_get_count   (ETableSortInfo   *info);
void              e_table_sort_info_sorting_truncate    (ETableSortInfo   *info,
							 gint               length);
ETableSortColumn  e_table_sort_info_sorting_get_nth     (ETableSortInfo   *info,
							 gint               n);
void              e_table_sort_info_sorting_set_nth     (ETableSortInfo   *info,
							 gint               n,
							 ETableSortColumn  column);

ETableSortInfo   *e_table_sort_info_new                 (void);
void              e_table_sort_info_load_from_node      (ETableSortInfo   *info,
							 xmlNode          *node,
							 gdouble           state_version);
xmlNode          *e_table_sort_info_save_to_node        (ETableSortInfo   *info,
							 xmlNode          *parent);
ETableSortInfo   *e_table_sort_info_duplicate           (ETableSortInfo   *info);
void              e_table_sort_info_set_can_group       (ETableSortInfo   *info,
							 gboolean          can_group);
gboolean          e_table_sort_info_get_can_group       (ETableSortInfo   *info);

G_END_DECLS

#endif /* _E_TABLE_SORT_INFO_H_ */
