
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SORT_INFO_H_
#define _E_TABLE_SORT_INFO_H_

#include <gtk/gtkobject.h>

#define E_TABLE_SORT_INFO_TYPE        (e_table_sort_info_get_type ())
#define E_TABLE_SORT_INFO(o)          (GTK_CHECK_CAST ((o), E_TABLE_SORT_INFO_TYPE, ETableSortInfo))
#define E_TABLE_SORT_INFO_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SORT_INFO_TYPE, ETableSortInfoClass))
#define E_IS_TABLE_SORT_INFO(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SORT_INFO_TYPE))
#define E_IS_TABLE_SORT_INFO_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SORT_INFO_TYPE))

typedef struct {
	GtkObject   base;
  
	xmlNode    *grouping;
} ETableSortInfo;

typedef struct {
	GtkObjectClass parent_class;

	/*
	 * Signals
	 */
	void        (*sort_info_changed)      (ETableSortInfo *etm);
} ETableSortInfoClass;

GtkType     e_table_sort_info_get_type (void);

/*
 * Routines for emitting signals on the e_table
 */
void        e_table_sort_info_changed          (ETableSortInfo *e_table_sort_info);
ETableSortInfo *e_table_sort_info_new (void);

#endif /* _E_TABLE_SORT_INFO_H_ */
