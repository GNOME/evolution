/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SORT_INFO_H_
#define _E_TABLE_SORT_INFO_H_

#include <gtk/gtkobject.h>

#define E_TABLE_SORT_INFO_TYPE        (e_table_sort_info_get_type ())
#define E_TABLE_SORT_INFO(o)          (GTK_CHECK_CAST ((o), E_TABLE_SORT_INFO_TYPE, ETableSortInfo))
#define E_TABLE_SORT_INFO_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SORT_INFO_TYPE, ETableSortInfoClass))
#define E_IS_TABLE_SORT_INFO(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SORT_INFO_TYPE))
#define E_IS_TABLE_SORT_INFO_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SORT_INFO_TYPE))

typedef struct _ETableSortColumn ETableSortColumn;

struct _ETableSortColumn {
	guint column : 31;
	guint ascending : 1;
};

typedef struct {
	GtkObject   base;
	
	gint group_count;
	ETableSortColumn *groupings;
	gint sort_count;
	ETableSortColumn *sortings;
	
	guint frozen : 1;
	guint sort_info_changed : 1;
	guint group_info_changed : 1;
} ETableSortInfo;

typedef struct {
	GtkObjectClass parent_class;

	/*
	 * Signals
	 */
	void        (*sort_info_changed)      (ETableSortInfo *info);
	void        (*group_info_changed)     (ETableSortInfo *info);
} ETableSortInfoClass;

GtkType      	 e_table_sort_info_get_type (void);

void             e_table_sort_info_freeze             (ETableSortInfo *info);
void             e_table_sort_info_thaw               (ETableSortInfo *info);

guint        	 e_table_sort_info_grouping_get_count (ETableSortInfo *info);
void             e_table_sort_info_grouping_truncate  (ETableSortInfo *info, int length);
ETableSortColumn e_table_sort_info_grouping_get_nth   (ETableSortInfo *info, int n);
void             e_table_sort_info_grouping_set_nth   (ETableSortInfo *info, int n, ETableSortColumn column);

guint        	 e_table_sort_info_sorting_get_count  (ETableSortInfo *info);
void             e_table_sort_info_sorting_truncate   (ETableSortInfo *info, int length);
ETableSortColumn e_table_sort_info_sorting_get_nth    (ETableSortInfo *info, int n);
void             e_table_sort_info_sorting_set_nth    (ETableSortInfo *info, int n, ETableSortColumn column);

ETableSortInfo  *e_table_sort_info_new                (void);

#endif /* _E_TABLE_SORT_INFO_H_ */
