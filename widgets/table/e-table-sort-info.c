/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-sort-info.c
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

#include <config.h>

#include <string.h>

#include "gal/util/e-util.h"
#include "gal/util/e-xml-utils.h"

#include "e-table-sort-info.h"

#define ETM_CLASS(e) (E_TABLE_SORT_INFO_GET_CLASS (e))

static GObjectClass *e_table_sort_info_parent_class;

enum {
	SORT_INFO_CHANGED,
	GROUP_INFO_CHANGED,
	LAST_SIGNAL
};

static guint e_table_sort_info_signals [LAST_SIGNAL] = { 0, };

static void
etsi_finalize (GObject *object)
{
	ETableSortInfo *etsi = E_TABLE_SORT_INFO (object);
	
	if (etsi->groupings)
		g_free(etsi->groupings);
	etsi->groupings = NULL;

	if (etsi->sortings)
		g_free(etsi->sortings);
	etsi->sortings = NULL;

	G_OBJECT_CLASS (e_table_sort_info_parent_class)->finalize (object);
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
	info->can_group = 1;
}

static void
e_table_sort_info_class_init (ETableSortInfoClass *klass)
{
	GObjectClass * object_class = G_OBJECT_CLASS (klass);

	e_table_sort_info_parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = etsi_finalize;

	e_table_sort_info_signals [SORT_INFO_CHANGED] =
		g_signal_new ("sort_info_changed",
			      E_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableSortInfoClass, sort_info_changed),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	e_table_sort_info_signals [GROUP_INFO_CHANGED] =
		g_signal_new ("group_info_changed",
			      E_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableSortInfoClass, group_info_changed),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	klass->sort_info_changed = NULL;
	klass->group_info_changed = NULL;
}

E_MAKE_TYPE(e_table_sort_info, "ETableSortInfo", ETableSortInfo,
	    e_table_sort_info_class_init, e_table_sort_info_init, G_TYPE_OBJECT)

static void
e_table_sort_info_sort_info_changed (ETableSortInfo *info)
{
	g_return_if_fail (info != NULL);
	g_return_if_fail (E_IS_TABLE_SORT_INFO (info));
	
	if (info->frozen) {
		info->sort_info_changed = 1;
	} else {
		g_signal_emit (G_OBJECT (info), e_table_sort_info_signals [SORT_INFO_CHANGED], 0);
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
		g_signal_emit (G_OBJECT (info), e_table_sort_info_signals [GROUP_INFO_CHANGED], 0);
	}
}

/**
 * e_table_sort_info_freeze:
 * @info: The ETableSortInfo object
 *
 * This functions allows the programmer to cluster various changes to the
 * ETableSortInfo (grouping and sorting) without having the object emit
 * "group_info_changed" or "sort_info_changed" signals on each change.
 *
 * To thaw, invoke the e_table_sort_info_thaw() function, which will
 * trigger any signals that might have been queued.
 */
void 
e_table_sort_info_freeze             (ETableSortInfo *info)
{
	info->frozen++;
}

/**
 * e_table_sort_info_thaw:
 * @info: The ETableSortInfo object
 *
 * This functions allows the programmer to cluster various changes to the
 * ETableSortInfo (grouping and sorting) without having the object emit
 * "group_info_changed" or "sort_info_changed" signals on each change.
 *
 * This function will flush any pending signals that might be emited by
 * this object.
 */
void
e_table_sort_info_thaw               (ETableSortInfo *info)
{
	info->frozen--;
	if (info->frozen != 0)
		return;
	
	if (info->sort_info_changed) {
		info->sort_info_changed = 0;
		e_table_sort_info_sort_info_changed(info);
	}
	if (info->group_info_changed) {
		info->group_info_changed = 0;
		e_table_sort_info_group_info_changed(info);
	}
}

/**
 * e_table_sort_info_grouping_get_count:
 * @info: The ETableSortInfo object
 *
 * Returns: the number of grouping criteria in the object.
 */
guint
e_table_sort_info_grouping_get_count (ETableSortInfo *info)
{
	if (info->can_group)
		return info->group_count;
	else
		return 0;
}

static void
e_table_sort_info_grouping_real_truncate  (ETableSortInfo *info, int length)
{
	if (length < info->group_count) {
		info->group_count = length;
	}
	if (length > info->group_count) {
		info->groupings = g_realloc(info->groupings, length * sizeof(ETableSortColumn));
		info->group_count = length;
	}
}

/**
 * e_table_sort_info_grouping_truncate:
 * @info: The ETableSortInfo object
 * @lenght: position where the truncation happens.
 *
 * This routine can be used to reduce or grow the number of grouping
 * criteria in the object.  
 */
void
e_table_sort_info_grouping_truncate  (ETableSortInfo *info, int length)
{
	e_table_sort_info_grouping_real_truncate(info, length);
	e_table_sort_info_group_info_changed(info);
}

/**
 * e_table_sort_info_grouping_get_nth:
 * @info: The ETableSortInfo object
 * @n: Item information to fetch.
 *
 * Returns: the description of the @n-th grouping criteria in the @info object.
 */
ETableSortColumn
e_table_sort_info_grouping_get_nth   (ETableSortInfo *info, int n)
{
	if (info->can_group && n < info->group_count) {
		return info->groupings[n];
	} else {
		ETableSortColumn fake = {0, 0};
		return fake;
	}
}

/**
 * e_table_sort_info_grouping_set_nth:
 * @info: The ETableSortInfo object
 * @n: Item information to fetch.
 * @column: new values for the grouping
 *
 * Sets the grouping criteria for index @n to be given by @column (a column number and
 * whether it is ascending or descending).
 */
void
e_table_sort_info_grouping_set_nth   (ETableSortInfo *info, int n, ETableSortColumn column)
{
	if (n >= info->group_count) {
		e_table_sort_info_grouping_real_truncate(info, n + 1);
	}
	info->groupings[n] = column;
	e_table_sort_info_group_info_changed(info);
}


/**
 * e_table_sort_info_get_count:
 * @info: The ETableSortInfo object
 *
 * Returns: the number of sorting criteria in the object.
 */
guint
e_table_sort_info_sorting_get_count (ETableSortInfo *info)
{
	return info->sort_count;
}

static void
e_table_sort_info_sorting_real_truncate  (ETableSortInfo *info, int length)
{
	if (length < info->sort_count) {
		info->sort_count = length;
	}
	if (length > info->sort_count) {
		info->sortings = g_realloc(info->sortings, length * sizeof(ETableSortColumn));
		info->sort_count = length;
	}
}

/**
 * e_table_sort_info_sorting_truncate:
 * @info: The ETableSortInfo object
 * @lenght: position where the truncation happens.
 *
 * This routine can be used to reduce or grow the number of sort
 * criteria in the object.  
 */
void
e_table_sort_info_sorting_truncate  (ETableSortInfo *info, int length)
{
	e_table_sort_info_sorting_real_truncate  (info, length);
	e_table_sort_info_sort_info_changed(info);
}

/**
 * e_table_sort_info_sorting_get_nth:
 * @info: The ETableSortInfo object
 * @n: Item information to fetch.
 *
 * Returns: the description of the @n-th grouping criteria in the @info object.
 */
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

/**
 * e_table_sort_info_sorting_get_nth:
 * @info: The ETableSortInfo object
 * @n: Item information to fetch.
 * @column: new values for the sorting
 *
 * Sets the sorting criteria for index @n to be given by @column (a
 * column number and whether it is ascending or descending).
 */
void
e_table_sort_info_sorting_set_nth   (ETableSortInfo *info, int n, ETableSortColumn column)
{
	if (n >= info->sort_count) {
		e_table_sort_info_sorting_real_truncate(info, n + 1);
	}
	info->sortings[n] = column;
	e_table_sort_info_sort_info_changed(info);
}

/**
 * e_table_sort_info_new:
 *
 * This creates a new e_table_sort_info object that contains no
 * grouping and no sorting defined as of yet.  This object is used
 * to keep track of multi-level sorting and multi-level grouping of
 * the ETable.  
 *
 * Returns: A new %ETableSortInfo object
 */
ETableSortInfo *
e_table_sort_info_new (void)
{
	return g_object_new (E_TABLE_SORT_INFO_TYPE, NULL);
}

/**
 * e_table_sort_info_load_from_node:
 * @info: The ETableSortInfo object
 * @node: pointer to the xmlNode that describes the sorting and grouping information
 * @state_version:
 *
 * This loads the state for the %ETableSortInfo object @info from the
 * xml node @node.
 */
void
e_table_sort_info_load_from_node (ETableSortInfo *info,
				  xmlNode        *node,
				  gdouble         state_version)
{
	int i;
	xmlNode *grouping;

	if (state_version <= 0.05) {
		i = 0;
		for (grouping = node->xmlChildrenNode; grouping && !strcmp (grouping->name, "group"); grouping = grouping->xmlChildrenNode) {
			ETableSortColumn column;
			column.column = e_xml_get_integer_prop_by_name (grouping, "column");
			column.ascending = e_xml_get_bool_prop_by_name (grouping, "ascending");
			e_table_sort_info_grouping_set_nth(info, i++, column);
		}
		i = 0;
		for (; grouping && !strcmp (grouping->name, "leaf"); grouping = grouping->xmlChildrenNode) {
			ETableSortColumn column;
			column.column = e_xml_get_integer_prop_by_name (grouping, "column");
			column.ascending = e_xml_get_bool_prop_by_name (grouping, "ascending");
			e_table_sort_info_sorting_set_nth(info, i++, column);
		}
	} else {
		gint gcnt = 0;
		gint scnt = 0;
		for (grouping = node->children; grouping; grouping = grouping->next) {
			ETableSortColumn column;

			if (grouping->type != XML_ELEMENT_NODE)
				continue;

			if (!strcmp (grouping->name, "group")) {
				column.column = e_xml_get_integer_prop_by_name (grouping, "column");
				column.ascending = e_xml_get_bool_prop_by_name (grouping, "ascending");
				e_table_sort_info_grouping_set_nth(info, gcnt++, column);
			} else if (!strcmp (grouping->name, "leaf")) {
				column.column = e_xml_get_integer_prop_by_name (grouping, "column");
				column.ascending = e_xml_get_bool_prop_by_name (grouping, "ascending");
				e_table_sort_info_sorting_set_nth(info, scnt++, column);
			}
		}
	}
	g_signal_emit (G_OBJECT (info), e_table_sort_info_signals [SORT_INFO_CHANGED], 0);
}

/**
 * e_table_sort_info_save_to_node:
 * @info: The ETableSortInfo object
 * @parent: xmlNode that will be hosting the saved state of the @info object.
 *
 * This function is used
 *
 * Returns: the node that has been appended to @parent as a child containing
 * the sorting and grouping information for this ETableSortInfo object.
 */
xmlNode *
e_table_sort_info_save_to_node (ETableSortInfo *info,
				xmlNode        *parent)
{
	xmlNode *grouping;
	xmlNode *node;
	int i;
	const int sort_count = e_table_sort_info_sorting_get_count (info);
	const int group_count = e_table_sort_info_grouping_get_count (info);
	
	grouping = xmlNewChild (parent, NULL, "grouping", NULL);

	for (i = 0; i < group_count; i++) {
		ETableSortColumn column = e_table_sort_info_grouping_get_nth(info, i);
		xmlNode *new_node = xmlNewChild(grouping, NULL, "group", NULL);

		e_xml_set_integer_prop_by_name (new_node, "column", column.column);
		e_xml_set_bool_prop_by_name (new_node, "ascending", column.ascending);
		node = new_node;
	}

	for (i = 0; i < sort_count; i++) {
		ETableSortColumn column = e_table_sort_info_sorting_get_nth(info, i);
		xmlNode *new_node = xmlNewChild(grouping, NULL, "leaf", NULL);
		
		e_xml_set_integer_prop_by_name (new_node, "column", column.column);
		e_xml_set_bool_prop_by_name (new_node, "ascending", column.ascending);
		node = new_node;
	}

	return grouping;
}

ETableSortInfo *
e_table_sort_info_duplicate (ETableSortInfo *info)
{
	ETableSortInfo *new_info;

	new_info = e_table_sort_info_new();

	new_info->group_count = info->group_count;
	new_info->groupings = g_new(ETableSortColumn, new_info->group_count);
	memmove(new_info->groupings, info->groupings, sizeof (ETableSortColumn) * new_info->group_count);

	new_info->sort_count = info->sort_count;
	new_info->sortings = g_new(ETableSortColumn, new_info->sort_count);
	memmove(new_info->sortings, info->sortings, sizeof (ETableSortColumn) * new_info->sort_count);

	new_info->can_group = info->can_group;

	return new_info;
}

void
e_table_sort_info_set_can_group       (ETableSortInfo   *info,
				       gboolean          can_group)
{
	info->can_group = can_group;
}

gboolean
e_table_sort_info_get_can_group       (ETableSortInfo   *info)
{
	return info->can_group;
}


