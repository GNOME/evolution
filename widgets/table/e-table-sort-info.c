/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-sort-info.c: a Table Model
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include "e-table-sort-info.h"
#include "e-util/e-util.h"

#define ETM_CLASS(e) ((ETableSortInfoClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ()
					  

static GtkObjectClass *e_table_sort_info_parent_class;

enum {
	SORT_INFO_CHANGED,
	GROUP_INFO_CHANGED,
	LAST_SIGNAL
};

static guint e_table_sort_info_signals [LAST_SIGNAL] = { 0, };

static void
etsi_destroy (GtkObject *object)
{
	ETableSortInfo *etsi;

	etsi = E_TABLE_SORT_INFO (object);
	
	if (etsi->groupings)
		g_free(etsi->groupings);
	if (etsi->sortings)
		g_free(etsi->sortings);
}

static void
e_table_sort_info_init (ETableSortInfo *info)
{
	info->group_count = 0;
	info->groupings = NULL;
	info->sort_count = 0;
	info->sortings = NULL;
	info->frozen = 0;
	info->sort_info_changed = 0;
	info->group_info_changed = 0;
}

static void
e_table_sort_info_class_init (ETableSortInfoClass *klass)
{
	GtkObjectClass *object_class;

	e_table_sort_info_parent_class = gtk_type_class (gtk_object_get_type ());

	object_class = GTK_OBJECT_CLASS(klass);
	
	object_class->destroy = etsi_destroy;

	e_table_sort_info_signals [SORT_INFO_CHANGED] =
		gtk_signal_new ("sort_info_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableSortInfoClass, sort_info_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_table_sort_info_signals [GROUP_INFO_CHANGED] =
		gtk_signal_new ("group_info_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableSortInfoClass, group_info_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	klass->sort_info_changed = NULL;
	klass->group_info_changed = NULL;

	gtk_object_class_add_signals (object_class, e_table_sort_info_signals, LAST_SIGNAL);
}

E_MAKE_TYPE(e_table_sort_info, "ETableSortInfo", ETableSortInfo,
	    e_table_sort_info_class_init, e_table_sort_info_init, PARENT_TYPE);

static void
e_table_sort_info_sort_info_changed (ETableSortInfo *info)
{
	g_return_if_fail (info != NULL);
	g_return_if_fail (E_IS_TABLE_SORT_INFO (info));
	
	if (info->frozen) {
		info->sort_info_changed = 1;
	} else {
		gtk_signal_emit (GTK_OBJECT (info),
				 e_table_sort_info_signals [SORT_INFO_CHANGED]);
	}
}

static void
e_table_sort_info_group_info_changed (ETableSortInfo *info)
{
	g_return_if_fail (info != NULL);
	g_return_if_fail (E_IS_TABLE_SORT_INFO (info));
	
	if (info->frozen) {
		info->group_info_changed = 1;
	} else {
		gtk_signal_emit (GTK_OBJECT (info),
				 e_table_sort_info_signals [GROUP_INFO_CHANGED]);
	}
}

void 
e_table_sort_info_freeze             (ETableSortInfo *info)
{
	info->frozen = 1;
}

void
e_table_sort_info_thaw               (ETableSortInfo *info)
{
	info->frozen = 0;
	if (info->sort_info_changed) {
		info->sort_info_changed = 0;
		e_table_sort_info_sort_info_changed(info);
	}
	if (info->group_info_changed) {
		info->group_info_changed = 0;
		e_table_sort_info_group_info_changed(info);
	}
}


guint
e_table_sort_info_grouping_get_count (ETableSortInfo *info)
{
	return info->group_count;
}

void
e_table_sort_info_grouping_truncate  (ETableSortInfo *info, int length)
{
	if (length < info->group_count) {
		info->group_count = length;
	}
	if (length > info->group_count) {
		info->groupings = g_realloc(info->groupings, length * sizeof(ETableSortColumn));
		info->group_count = length;
	}
	e_table_sort_info_group_info_changed(info);
}

ETableSortColumn
e_table_sort_info_grouping_get_nth   (ETableSortInfo *info, int n)
{
	if (n < info->group_count) {
		return info->groupings[n];
	} else {
		ETableSortColumn fake = {0, 0};
		return fake;
	}
}

void
e_table_sort_info_grouping_set_nth   (ETableSortInfo *info, int n, ETableSortColumn column)
{
	if (n >= info->group_count) {
		e_table_sort_info_grouping_truncate(info, n + 1);
	}
	info->groupings[n] = column;
	e_table_sort_info_group_info_changed(info);
}


guint
e_table_sort_info_sorting_get_count (ETableSortInfo *info)
{
	return info->sort_count;
}

void
e_table_sort_info_sorting_truncate  (ETableSortInfo *info, int length)
{
	if (length < info->sort_count) {
		info->sort_count = length;
	}
	if (length > info->sort_count) {
		info->sortings = g_realloc(info->sortings, length * sizeof(ETableSortColumn));
		info->sort_count = length;
	}
	e_table_sort_info_sort_info_changed(info);
}

ETableSortColumn
e_table_sort_info_sorting_get_nth   (ETableSortInfo *info, int n)
{
	if (n < info->sort_count) {
		return info->sortings[n];
	} else {
		ETableSortColumn fake = {0, 0};
		return fake;
	}
}

void
e_table_sort_info_sorting_set_nth   (ETableSortInfo *info, int n, ETableSortColumn column)
{
	if (n >= info->sort_count) {
		e_table_sort_info_sorting_truncate(info, n + 1);
	}
	info->sortings[n] = column;
	e_table_sort_info_sort_info_changed(info);
}


ETableSortInfo *
e_table_sort_info_new (void)
{
	return gtk_type_new (e_table_sort_info_get_type ());
}
