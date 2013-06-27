/*
 * e-table-sort-info.c
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

#include "e-table-sort-info.h"

#include <string.h>

#include "e-xml-utils.h"

enum {
	SORT_INFO_CHANGED,
	GROUP_INFO_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (ETableSortInfo , e_table_sort_info, G_TYPE_OBJECT)

static void
table_sort_info_finalize (GObject *object)
{
	ETableSortInfo *sort_info = E_TABLE_SORT_INFO (object);

	g_free (sort_info->groupings);
	g_free (sort_info->sortings);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_table_sort_info_parent_class)->finalize (object);
}

static void
e_table_sort_info_class_init (ETableSortInfoClass *class)
{
	GObjectClass * object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = table_sort_info_finalize;

	signals[SORT_INFO_CHANGED] = g_signal_new (
		"sort_info_changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableSortInfoClass, sort_info_changed),
		(GSignalAccumulator) NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[GROUP_INFO_CHANGED] = g_signal_new (
		"group_info_changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableSortInfoClass, group_info_changed),
		(GSignalAccumulator) NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_table_sort_info_init (ETableSortInfo *sort_info)
{
	sort_info->can_group = TRUE;
}

/**
 * e_table_sort_info_new:
 *
 * This creates a new #ETableSortInfo object that contains no
 * grouping and no sorting defined as of yet.  This object is used
 * to keep track of multi-level sorting and multi-level grouping of
 * an #ETable.
 *
 * Returns: A new #ETableSortInfo object
 */
ETableSortInfo *
e_table_sort_info_new (void)
{
	return g_object_new (E_TYPE_TABLE_SORT_INFO, NULL);
}

gboolean
e_table_sort_info_get_can_group (ETableSortInfo *sort_info)
{
	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), FALSE);

	return sort_info->can_group;
}

void
e_table_sort_info_set_can_group (ETableSortInfo *sort_info,
                                 gboolean can_group)
{
	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));

	sort_info->can_group = can_group;
}

/**
 * e_table_sort_info_grouping_get_count:
 * @sort_info: an #ETableSortInfo
 *
 * Returns: the number of grouping criteria in the object.
 */
guint
e_table_sort_info_grouping_get_count (ETableSortInfo *sort_info)
{
	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), 0);

	return (sort_info->can_group) ? sort_info->group_count : 0;
}

static void
table_sort_info_grouping_real_truncate (ETableSortInfo *sort_info,
                                        gint length)
{
	if (length < sort_info->group_count)
		sort_info->group_count = length;

	if (length > sort_info->group_count) {
		sort_info->groupings = g_realloc (
			sort_info->groupings,
			length * sizeof (ETableSortColumn));
		sort_info->group_count = length;
	}
}

/**
 * e_table_sort_info_grouping_truncate:
 * @sort_info: an #ETableSortInfo
 * @length: position where the truncation happens.
 *
 * This routine can be used to reduce or grow the number of grouping
 * criteria in the object.
 */
void
e_table_sort_info_grouping_truncate (ETableSortInfo *sort_info,
                                     gint length)
{
	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));

	table_sort_info_grouping_real_truncate (sort_info, length);

	g_signal_emit (sort_info, signals[GROUP_INFO_CHANGED], 0);
}

/**
 * e_table_sort_info_grouping_get_nth:
 * @sort_info: an #ETableSortInfo
 * @n: Item information to fetch.
 *
 * Returns: the description of the @n-th grouping criteria in the @info object.
 */
ETableSortColumn
e_table_sort_info_grouping_get_nth (ETableSortInfo *sort_info,
                                    gint n)
{
	ETableSortColumn fake = {0, 0};

	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), fake);

	if (sort_info->can_group && n < sort_info->group_count)
		return sort_info->groupings[n];

	return fake;
}

/**
 * e_table_sort_info_grouping_set_nth:
 * @sort_info: an #ETableSortInfo
 * @n: Item information to fetch.
 * @column: new values for the grouping
 *
 * Sets the grouping criteria for index @n to be given by @column
 * (a column number and whether it is ascending or descending).
 */
void
e_table_sort_info_grouping_set_nth (ETableSortInfo *sort_info,
                                    gint n,
                                    ETableSortColumn column)
{
	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));

	if (n >= sort_info->group_count)
		table_sort_info_grouping_real_truncate (sort_info, n + 1);

	sort_info->groupings[n] = column;

	g_signal_emit (sort_info, signals[GROUP_INFO_CHANGED], 0);
}

/**
 * e_table_sort_info_get_count:
 * @sort_info: an #ETableSortInfo
 *
 * Returns: the number of sorting criteria in the object.
 */
guint
e_table_sort_info_sorting_get_count (ETableSortInfo *sort_info)
{
	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), 0);

	return sort_info->sort_count;
}

static void
table_sort_info_sorting_real_truncate (ETableSortInfo *sort_info,
                                       gint length)
{
	if (length < sort_info->sort_count)
		sort_info->sort_count = length;

	if (length > sort_info->sort_count) {
		sort_info->sortings = g_realloc (
			sort_info->sortings,
			length * sizeof (ETableSortColumn));
		sort_info->sort_count = length;
	}
}

/**
 * e_table_sort_info_sorting_truncate:
 * @sort_info: an #ETableSortInfo
 * @length: position where the truncation happens.
 *
 * This routine can be used to reduce or grow the number of sort
 * criteria in the object.
 */
void
e_table_sort_info_sorting_truncate (ETableSortInfo *sort_info,
                                    gint length)
{
	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));

	table_sort_info_sorting_real_truncate  (sort_info, length);

	g_signal_emit (sort_info, signals[SORT_INFO_CHANGED], 0);
}

/**
 * e_table_sort_info_sorting_get_nth:
 * @sort_info: an #ETableSortInfo
 * @n: Item information to fetch.
 *
 * Returns: the description of the @n-th grouping criteria in the @info object.
 */
ETableSortColumn
e_table_sort_info_sorting_get_nth (ETableSortInfo *sort_info,
                                   gint n)
{
	ETableSortColumn fake = {0, 0};

	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), fake);

	if (n < sort_info->sort_count)
		return sort_info->sortings[n];

	return fake;
}

/**
 * e_table_sort_info_sorting_set_nth:
 * @sort_info: an #ETableSortInfo
 * @n: Item information to fetch.
 * @column: new values for the sorting
 *
 * Sets the sorting criteria for index @n to be given by @column (a
 * column number and whether it is ascending or descending).
 */
void
e_table_sort_info_sorting_set_nth (ETableSortInfo *sort_info,
                                   gint n,
                                   ETableSortColumn column)
{
	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));

	if (n >= sort_info->sort_count)
		table_sort_info_sorting_real_truncate (sort_info, n + 1);

	sort_info->sortings[n] = column;

	g_signal_emit (sort_info, signals[SORT_INFO_CHANGED], 0);
}

/**
 * e_table_sort_info_load_from_node:
 * @sort_info: an #ETableSortInfo
 * @node: pointer to the xmlNode that describes the sorting and grouping information
 * @state_version:
 *
 * This loads the state for the #ETableSortInfo object @info from the
 * xml node @node.
 */
void
e_table_sort_info_load_from_node (ETableSortInfo *sort_info,
                                  xmlNode *node,
                                  gdouble state_version)
{
	gint i;
	xmlNode *grouping;

	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));
	g_return_if_fail (node != NULL);

	if (state_version <= 0.05) {
		i = 0;
		for (grouping = node->xmlChildrenNode; grouping && !strcmp ((gchar *) grouping->name, "group"); grouping = grouping->xmlChildrenNode) {
			ETableSortColumn column;
			column.column = e_xml_get_integer_prop_by_name (grouping, (const guchar *)"column");
			column.ascending = e_xml_get_bool_prop_by_name (grouping, (const guchar *)"ascending");
			e_table_sort_info_grouping_set_nth (sort_info, i++, column);
		}
		i = 0;
		for (; grouping && !strcmp ((gchar *) grouping->name, "leaf"); grouping = grouping->xmlChildrenNode) {
			ETableSortColumn column;
			column.column = e_xml_get_integer_prop_by_name (grouping, (const guchar *)"column");
			column.ascending = e_xml_get_bool_prop_by_name (grouping, (const guchar *)"ascending");
			e_table_sort_info_sorting_set_nth (sort_info, i++, column);
		}
	} else {
		gint gcnt = 0;
		gint scnt = 0;
		for (grouping = node->children; grouping; grouping = grouping->next) {
			ETableSortColumn column;

			if (grouping->type != XML_ELEMENT_NODE)
				continue;

			if (!strcmp ((gchar *) grouping->name, "group")) {
				column.column = e_xml_get_integer_prop_by_name (grouping, (const guchar *)"column");
				column.ascending = e_xml_get_bool_prop_by_name (grouping, (const guchar *)"ascending");
				e_table_sort_info_grouping_set_nth (sort_info, gcnt++, column);
			} else if (!strcmp ((gchar *) grouping->name, "leaf")) {
				column.column = e_xml_get_integer_prop_by_name (grouping, (const guchar *)"column");
				column.ascending = e_xml_get_bool_prop_by_name (grouping, (const guchar *)"ascending");
				e_table_sort_info_sorting_set_nth (sort_info, scnt++, column);
			}
		}
	}
	g_signal_emit (sort_info, signals[SORT_INFO_CHANGED], 0);
}

/**
 * e_table_sort_info_save_to_node:
 * @sort_info: an #ETableSortInfo
 * @parent: xmlNode that will be hosting the saved state of the @info object.
 *
 * This function is used
 *
 * Returns: the node that has been appended to @parent as a child containing
 * the sorting and grouping information for this ETableSortInfo object.
 */
xmlNode *
e_table_sort_info_save_to_node (ETableSortInfo *sort_info,
                                xmlNode *parent)
{
	xmlNode *grouping;
	gint sort_count;
	gint group_count;
	gint i;

	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), NULL);

	sort_count = e_table_sort_info_sorting_get_count (sort_info);
	group_count = e_table_sort_info_grouping_get_count (sort_info);

	grouping = xmlNewChild (parent, NULL, (const guchar *)"grouping", NULL);

	for (i = 0; i < group_count; i++) {
		ETableSortColumn column;
		xmlNode *new_node;

		column = e_table_sort_info_grouping_get_nth (sort_info, i);
		new_node = xmlNewChild (grouping, NULL, (const guchar *)"group", NULL);

		e_xml_set_integer_prop_by_name (new_node, (const guchar *)"column", column.column);
		e_xml_set_bool_prop_by_name (new_node, (const guchar *)"ascending", column.ascending);
	}

	for (i = 0; i < sort_count; i++) {
		ETableSortColumn column;
		xmlNode *new_node;

		column = e_table_sort_info_sorting_get_nth (sort_info, i);
		new_node = xmlNewChild (grouping, NULL, (const guchar *)"leaf", NULL);

		e_xml_set_integer_prop_by_name (new_node, (const guchar *)"column", column.column);
		e_xml_set_bool_prop_by_name (new_node, (const guchar *)"ascending", column.ascending);
	}

	return grouping;
}

ETableSortInfo *
e_table_sort_info_duplicate (ETableSortInfo *sort_info)
{
	ETableSortInfo *new_info;

	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), NULL);

	new_info = e_table_sort_info_new ();

	new_info->group_count = sort_info->group_count;
	new_info->groupings = g_new (ETableSortColumn, new_info->group_count);
	memmove (
		new_info->groupings,
		sort_info->groupings,
		sizeof (ETableSortColumn) * new_info->group_count);

	new_info->sort_count = sort_info->sort_count;
	new_info->sortings = g_new (ETableSortColumn, new_info->sort_count);
	memmove (
		new_info->sortings,
		sort_info->sortings,
		sizeof (ETableSortColumn) * new_info->sort_count);

	new_info->can_group = sort_info->can_group;

	return new_info;
}

