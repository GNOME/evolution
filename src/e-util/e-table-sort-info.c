/*
 * e-table-sort-info.c
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

#include "e-table-sort-info.h"

#include <string.h>

#include "e-table-specification.h"
#include "e-xml-utils.h"

typedef struct _ColumnData ColumnData;

struct _ETableSortInfoPrivate {
	ETableSpecification *specification;
	GArray *groupings;
	GArray *sortings;
	gboolean can_group;
};

struct _ColumnData {
	ETableColumnSpecification *column_spec;
	GtkSortType sort_type;
};

enum {
	PROP_0,
	PROP_SPECIFICATION
};

enum {
	SORT_INFO_CHANGED,
	GROUP_INFO_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (ETableSortInfo , e_table_sort_info, G_TYPE_OBJECT)

static void
column_data_clear (ColumnData *data)
{
	g_clear_object (&data->column_spec);
}

static void
table_sort_info_parser_start_group (GMarkupParseContext *context,
                                    const gchar *element_name,
                                    const gchar **attribute_names,
                                    const gchar **attribute_values,
                                    ETableSortInfo *sort_info,
                                    GPtrArray *columns,
                                    GError **error)
{
	const gchar *index_str;
	gboolean ascending;
	gboolean success;

	success = g_markup_collect_attributes (
		element_name,
		attribute_names,
		attribute_values,
		error,

		G_MARKUP_COLLECT_STRING,
		"column",
		&index_str,

		G_MARKUP_COLLECT_BOOLEAN |
		G_MARKUP_COLLECT_OPTIONAL,
		"ascending",
		&ascending,

		G_MARKUP_COLLECT_INVALID);

	if (success) {
		ETableColumnSpecification *column_spec;
		ColumnData column_data;
		gint64 index;

		g_return_if_fail (index_str != NULL);
		index = g_ascii_strtoll (index_str, NULL, 10);

		g_return_if_fail (index < columns->len);
		column_spec = g_ptr_array_index (columns, index);

		column_data.column_spec = g_object_ref (column_spec);

		if (ascending)
			column_data.sort_type = GTK_SORT_ASCENDING;
		else
			column_data.sort_type = GTK_SORT_DESCENDING;

		g_array_append_val (sort_info->priv->groupings, column_data);
	}
}

static void
table_sort_info_parser_start_leaf (GMarkupParseContext *context,
                                   const gchar *element_name,
                                   const gchar **attribute_names,
                                   const gchar **attribute_values,
                                   ETableSortInfo *sort_info,
                                   GPtrArray *columns,
                                   GError **error)
{
	const gchar *index_str;
	gboolean ascending;
	gboolean success;

	success = g_markup_collect_attributes (
		element_name,
		attribute_names,
		attribute_values,
		error,

		G_MARKUP_COLLECT_STRING,
		"column",
		&index_str,

		G_MARKUP_COLLECT_BOOLEAN |
		G_MARKUP_COLLECT_OPTIONAL,
		"ascending",
		&ascending,

		G_MARKUP_COLLECT_INVALID);

	if (success) {
		ETableColumnSpecification *column_spec;
		ColumnData column_data;
		gint64 index;

		g_return_if_fail (index_str != NULL);
		index = g_ascii_strtoll (index_str, NULL, 10);

		g_return_if_fail (index < columns->len);
		column_spec = g_ptr_array_index (columns, index);

		column_data.column_spec = g_object_ref (column_spec);

		if (ascending)
			column_data.sort_type = GTK_SORT_ASCENDING;
		else
			column_data.sort_type = GTK_SORT_DESCENDING;

		g_array_append_val (sort_info->priv->sortings, column_data);
	}
}

static void
table_sort_info_parser_start_element (GMarkupParseContext *context,
                                      const gchar *element_name,
                                      const gchar **attribute_names,
                                      const gchar **attribute_values,
                                      gpointer user_data,
                                      GError **error)
{
	ETableSpecification *specification;
	ETableSortInfo *sort_info;
	GPtrArray *columns;

	sort_info = E_TABLE_SORT_INFO (user_data);
	specification = e_table_sort_info_ref_specification (sort_info);
	columns = e_table_specification_ref_columns (specification);

	if (g_str_equal (element_name, "group"))
		table_sort_info_parser_start_group (
			context,
			element_name,
			attribute_names,
			attribute_values,
			sort_info,
			columns,
			error);

	if (g_str_equal (element_name, "leaf"))
		table_sort_info_parser_start_leaf (
			context,
			element_name,
			attribute_names,
			attribute_values,
			sort_info,
			columns,
			error);

	g_object_unref (specification);
	g_ptr_array_unref (columns);
}

static void
table_sort_info_parser_error (GMarkupParseContext *context,
                              GError *error,
                              gpointer user_data)
{
	g_object_unref (E_TABLE_SORT_INFO (user_data));
}

static const GMarkupParser table_sort_info_parser = {
	table_sort_info_parser_start_element,
	NULL,
	NULL,
	NULL,
	table_sort_info_parser_error
};

static void
table_sort_info_set_specification (ETableSortInfo *sort_info,
                                   ETableSpecification *specification)
{
	g_return_if_fail (E_IS_TABLE_SPECIFICATION (specification));

	if (sort_info->priv->specification != specification) {
		g_clear_object (&sort_info->priv->specification);
		sort_info->priv->specification = g_object_ref (specification);
	}
}

static void
table_sort_info_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SPECIFICATION:
			table_sort_info_set_specification (
				E_TABLE_SORT_INFO (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
table_sort_info_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SPECIFICATION:
			g_value_take_object (
				value,
				e_table_sort_info_ref_specification (
				E_TABLE_SORT_INFO (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
table_sort_info_dispose (GObject *object)
{
	ETableSortInfo *self = E_TABLE_SORT_INFO (object);

	g_clear_object (&self->priv->specification);

	g_array_set_size (self->priv->groupings, 0);
	g_array_set_size (self->priv->sortings, 0);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_sort_info_parent_class)->dispose (object);
}

static void
table_sort_info_finalize (GObject *object)
{
	ETableSortInfo *self = E_TABLE_SORT_INFO (object);

	g_array_free (self->priv->groupings, TRUE);
	g_array_free (self->priv->sortings, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_table_sort_info_parent_class)->finalize (object);
}

static void
e_table_sort_info_class_init (ETableSortInfoClass *class)
{
	GObjectClass * object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = table_sort_info_set_property;
	object_class->get_property = table_sort_info_get_property;
	object_class->dispose = table_sort_info_dispose;
	object_class->finalize = table_sort_info_finalize;

	g_object_class_install_property (
		object_class,
		PROP_SPECIFICATION,
		g_param_spec_object (
			"specification",
			"Table Specification",
			"Specification for the table state",
			E_TYPE_TABLE_SPECIFICATION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

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
	sort_info->priv = e_table_sort_info_get_instance_private (sort_info);

	sort_info->priv->groupings = g_array_new (
		FALSE, TRUE, sizeof (ColumnData));
	g_array_set_clear_func (
		sort_info->priv->groupings,
		(GDestroyNotify) column_data_clear);

	sort_info->priv->sortings = g_array_new (
		FALSE, TRUE, sizeof (ColumnData));
	g_array_set_clear_func (
		sort_info->priv->sortings,
		(GDestroyNotify) column_data_clear);

	sort_info->priv->can_group = TRUE;
}

/**
 * e_table_sort_info_new:
 * @specification: an #ETableSpecification
 *
 * This creates a new #ETableSortInfo object that contains no
 * grouping and no sorting defined as of yet.  This object is used
 * to keep track of multi-level sorting and multi-level grouping of
 * an #ETable.
 *
 * Returns: A new #ETableSortInfo object
 */
ETableSortInfo *
e_table_sort_info_new (ETableSpecification *specification)
{
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);

	return g_object_new (
		E_TYPE_TABLE_SORT_INFO,
		"specification", specification, NULL);
}

/**
 * e_table_sort_info_parse_context_push:
 * @context: a #GMarkupParseContext
 * @specification: an #ETableSpecification
 *
 * Creates a new #ETableSortInfo from a segment of XML data being fed to
 * @context.  Call this function for the appropriate opening tag from the
 * <structfield>start_element</structfield> callback of a #GMarkupParser,
 * then call e_table_sort_info_parse_context_pop() for the corresponding
 * closing tag from the <structfield>end_element</structfield> callback.
 **/
void
e_table_sort_info_parse_context_push (GMarkupParseContext *context,
                                      ETableSpecification *specification)
{
	g_return_if_fail (context != NULL);
	g_return_if_fail (E_IS_TABLE_SPECIFICATION (specification));

	g_markup_parse_context_push (
		context, &table_sort_info_parser,
		e_table_sort_info_new (specification));
}

/**
 * e_table_sort_info_parse_context_pop:
 * @context: a #GMarkupParseContext
 *
 * Creates a new #ETableSortInfo from a segment of XML data being fed to
 * @context.  Call e_table_sort_info_parse_context_push() for the appropriate
 * opening tag from the <structfield>start_element</structfield> callback of a
 * #GMarkupParser, then call this function for the corresponding closing tag
 * from the <structfield>end_element</structfield> callback.
 *
 * Unreference the newly-created #ETableSortInfo with g_object_unref() when
 * finished with it.
 *
 * Returns: an #ETableSortInfo
 **/
ETableSortInfo *
e_table_sort_info_parse_context_pop (GMarkupParseContext *context)
{
	gpointer user_data;

	g_return_val_if_fail (context != NULL, NULL);

	user_data = g_markup_parse_context_pop (context);

	return E_TABLE_SORT_INFO (user_data);
}

/**
 * e_table_sort_info_ref_specification:
 * @sort_info: an #ETableSortInfo
 *
 * Returns the #ETableSpecification passed to e_table_sort_info_new().
 *
 * The returned #ETableSpecification is referenced for thread-safety and must
 * be unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #ETableSpecification
 **/
ETableSpecification *
e_table_sort_info_ref_specification (ETableSortInfo *sort_info)
{
	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), NULL);

	if (sort_info->priv->specification)
		return g_object_ref (sort_info->priv->specification);

	return NULL;
}

gboolean
e_table_sort_info_get_can_group (ETableSortInfo *sort_info)
{
	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), FALSE);

	return sort_info->priv->can_group;
}

void
e_table_sort_info_set_can_group (ETableSortInfo *sort_info,
                                 gboolean can_group)
{
	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));

	sort_info->priv->can_group = can_group;
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
	guint count = 0;

	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), 0);

	if (e_table_sort_info_get_can_group (sort_info))
		count = sort_info->priv->groupings->len;

	return count;
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
                                     guint length)
{
	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));

	g_array_set_size (sort_info->priv->groupings, length);

	g_signal_emit (sort_info, signals[GROUP_INFO_CHANGED], 0);
}

/**
 * e_table_sort_info_grouping_get_nth:
 * @sort_info: an #ETableSortInfo
 * @n: Item information to fetch.
 * @out_sort_type: return location for a #GtkSortType value, or %NULL
 *
 * Returns: the description of the @n-th grouping criteria in the @info object.
 */
ETableColumnSpecification *
e_table_sort_info_grouping_get_nth (ETableSortInfo *sort_info,
                                    guint n,
                                    GtkSortType *out_sort_type)
{
	ETableColumnSpecification *column_spec = NULL;
	GArray *array;
	gboolean can_group;

	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), NULL);

	array = sort_info->priv->groupings;
	can_group = e_table_sort_info_get_can_group (sort_info);

	if (can_group && n < array->len) {
		ColumnData *column_data;

		column_data = &g_array_index (array, ColumnData, n);

		if (out_sort_type != NULL)
			*out_sort_type = column_data->sort_type;

		column_spec = column_data->column_spec;
	}

	return column_spec;
}

/**
 * e_table_sort_info_grouping_set_nth:
 * @sort_info: an #ETableSortInfo
 * @n: Item information to fetch.
 * @spec: an #ETableColumnSpecification
 * @sort_type: a #GtkSortType
 *
 * Sets the grouping criteria for index @n to @spec and @sort_type.
 */
void
e_table_sort_info_grouping_set_nth (ETableSortInfo *sort_info,
                                    guint n,
                                    ETableColumnSpecification *spec,
                                    GtkSortType sort_type)
{
	GArray *array;
	ColumnData *column_data;

	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));
	g_return_if_fail (E_IS_TABLE_COLUMN_SPECIFICATION (spec));

	array = sort_info->priv->groupings;
	g_array_set_size (array, MAX (n + 1, array->len));
	column_data = &g_array_index (array, ColumnData, n);

	/* In case it's setting the same specification, to not free it */
	g_object_ref (spec);

	column_data_clear (column_data);

	column_data->column_spec = spec;
	column_data->sort_type = sort_type;

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

	return sort_info->priv->sortings->len;
}

/**
 * e_table_sort_info_sorting_remove:
 * @sort_info: an #ETableSortInfo
 * @n: the index of the element to remove
 *
 * Removes the sorting element at the given index.  The following sorting
 * elements are moved down one place.
 **/
void
e_table_sort_info_sorting_remove (ETableSortInfo *sort_info,
                                  guint n)
{
	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));

	g_array_remove_index (sort_info->priv->sortings, n);

	g_signal_emit (sort_info, signals[SORT_INFO_CHANGED], 0);
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
                                    guint length)
{
	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));

	g_array_set_size (sort_info->priv->sortings, length);

	g_signal_emit (sort_info, signals[SORT_INFO_CHANGED], 0);
}

/**
 * e_table_sort_info_sorting_get_nth:
 * @sort_info: an #ETableSortInfo
 * @n: Item information to fetch.
 * @out_sort_type: return location for a #GtkSortType value, or %NULL
 *
 * Returns: the description of the @n-th grouping criteria in the @info object.
 */
ETableColumnSpecification *
e_table_sort_info_sorting_get_nth (ETableSortInfo *sort_info,
                                   guint n,
                                   GtkSortType *out_sort_type)
{
	ETableColumnSpecification *column_spec = NULL;
	GArray *array;

	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), NULL);

	array = sort_info->priv->sortings;

	if (n < array->len) {
		ColumnData *column_data;

		column_data = &g_array_index (array, ColumnData, n);

		if (out_sort_type != NULL)
			*out_sort_type = column_data->sort_type;

		column_spec = column_data->column_spec;
	}

	return column_spec;
}

/**
 * e_table_sort_info_sorting_set_nth:
 * @sort_info: an #ETableSortInfo
 * @n: Item information to fetch.
 * @spec: an #ETableColumnSpecification
 * @sort_type: a #GtkSortType
 *
 * Sets the sorting criteria for index @n to @spec and @sort_type.
 */
void
e_table_sort_info_sorting_set_nth (ETableSortInfo *sort_info,
                                   guint n,
                                   ETableColumnSpecification *spec,
                                   GtkSortType sort_type)
{
	GArray *array;
	ColumnData *column_data;

	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));
	g_return_if_fail (E_IS_TABLE_COLUMN_SPECIFICATION (spec));

	array = sort_info->priv->sortings;
	g_array_set_size (array, MAX (n + 1, array->len));
	column_data = &g_array_index (array, ColumnData, n);

	/* In case it's setting the same specification, to not free it */
	g_object_ref (spec);

	column_data_clear (column_data);

	column_data->column_spec = spec;
	column_data->sort_type = sort_type;

	g_signal_emit (sort_info, signals[SORT_INFO_CHANGED], 0);
}

/**
 * @sort_info: an #ETableSortInfo
 * @n: Index to insert to.
 * @spec: an #ETableColumnSpecification
 * @sort_type: a #GtkSortType
 *
 * Inserts the sorting criteria for index @n to @spec and @sort_type.
 *
 * Since: 3.12
 **/
void
e_table_sort_info_sorting_insert (ETableSortInfo *sort_info,
				  guint n,
				  ETableColumnSpecification *spec,
				  GtkSortType sort_type)
{
	GArray *array;
	ColumnData *column_data, tmp;

	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));
	g_return_if_fail (E_IS_TABLE_COLUMN_SPECIFICATION (spec));

	array = sort_info->priv->sortings;
	if (array->len == 0) {
		e_table_sort_info_sorting_set_nth (sort_info, 0, spec, sort_type);
		return;
	}

	if ((gint) n == -1)
		n = 0;
	else if (n > array->len)
		n = array->len;

	tmp.column_spec = NULL;
	tmp.sort_type = sort_type;
	column_data = &tmp;

	if (n == array->len)
		g_array_append_val (array, column_data);
	else
		g_array_insert_val (array, n, column_data);

	column_data = &g_array_index (array, ColumnData, n);
	column_data->column_spec = g_object_ref (spec);
	column_data->sort_type = sort_type;

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
	ETableSpecification *specification;
	GPtrArray *columns;
	xmlNode *grouping;
	guint gcnt = 0;
	guint scnt = 0;

	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));
	g_return_if_fail (node != NULL);

	specification = e_table_sort_info_ref_specification (sort_info);
	columns = e_table_specification_ref_columns (specification);

	for (grouping = node->children; grouping; grouping = grouping->next) {

		if (grouping->type != XML_ELEMENT_NODE)
			continue;

		if (g_str_equal ((gchar *) grouping->name, "group")) {
			GtkSortType sort_type;
			gboolean ascending;
			guint index;

			index = e_xml_get_integer_prop_by_name (
				grouping, (guchar *) "column");
			ascending = e_xml_get_bool_prop_by_name (
				grouping, (guchar *) "ascending");

			if (ascending)
				sort_type = GTK_SORT_ASCENDING;
			else
				sort_type = GTK_SORT_DESCENDING;

			if (index < columns->len)
				e_table_sort_info_grouping_set_nth (
					sort_info, gcnt++,
					columns->pdata[index],
					sort_type);
		}

		if (g_str_equal ((gchar *) grouping->name, "leaf")) {
			GtkSortType sort_type;
			gboolean ascending;
			gint index;

			index = e_xml_get_integer_prop_by_name (
				grouping, (guchar *) "column");
			ascending = e_xml_get_bool_prop_by_name (
				grouping, (guchar *) "ascending");

			if (ascending)
				sort_type = GTK_SORT_ASCENDING;
			else
				sort_type = GTK_SORT_DESCENDING;

			if (index < columns->len)
				e_table_sort_info_sorting_set_nth (
					sort_info, scnt++,
					columns->pdata[index],
				sort_type);
		}
	}

	g_object_unref (specification);
	g_ptr_array_unref (columns);

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
	ETableSpecification *specification;
	xmlNode *grouping;
	guint sort_count;
	guint group_count;
	guint ii;

	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), NULL);

	sort_count = e_table_sort_info_sorting_get_count (sort_info);
	group_count = e_table_sort_info_grouping_get_count (sort_info);

	grouping = xmlNewChild (parent, NULL, (guchar *) "grouping", NULL);

	specification = e_table_sort_info_ref_specification (sort_info);

	for (ii = 0; ii < group_count; ii++) {
		ETableColumnSpecification *column_spec;
		GtkSortType sort_type = GTK_SORT_ASCENDING;
		xmlNode *new_node;
		gint index;

		column_spec = e_table_sort_info_grouping_get_nth (
			sort_info, ii, &sort_type);

		index = e_table_specification_get_column_index (
			specification, column_spec);

		if (index < 0) {
			g_warn_if_reached ();
			continue;
		}

		new_node = xmlNewChild (
			grouping, NULL, (guchar *) "group", NULL);

		e_xml_set_integer_prop_by_name (
			new_node, (guchar *) "column", index);
		e_xml_set_bool_prop_by_name (
			new_node, (guchar *) "ascending",
			(sort_type == GTK_SORT_ASCENDING));
	}

	for (ii = 0; ii < sort_count; ii++) {
		ETableColumnSpecification *column_spec;
		GtkSortType sort_type = GTK_SORT_ASCENDING;
		xmlNode *new_node;
		gint index;

		column_spec = e_table_sort_info_sorting_get_nth (
			sort_info, ii, &sort_type);

		index = e_table_specification_get_column_index (
			specification, column_spec);

		if (index < 0) {
			g_warn_if_reached ();
			continue;
		}

		new_node = xmlNewChild (
			grouping, NULL, (guchar *) "leaf", NULL);

		e_xml_set_integer_prop_by_name (
			new_node, (guchar *) "column", index);
		e_xml_set_bool_prop_by_name (
			new_node, (guchar *) "ascending",
			(sort_type == GTK_SORT_ASCENDING));
	}

	g_object_unref (specification);

	return grouping;
}

ETableSortInfo *
e_table_sort_info_duplicate (ETableSortInfo *sort_info)
{
	ETableSpecification *specification;
	ETableSortInfo *new_info;
	gint ii;

	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), NULL);

	specification = e_table_sort_info_ref_specification (sort_info);
	new_info = e_table_sort_info_new (specification);
	g_object_unref (specification);

	g_array_set_size (
		new_info->priv->groupings,
		sort_info->priv->groupings->len);
	if (new_info->priv->groupings->data &&
	    sort_info->priv->groupings->data &&
	    sort_info->priv->groupings->len) {
		memmove (
			new_info->priv->groupings->data,
			sort_info->priv->groupings->data,
			sort_info->priv->groupings->len *
			g_array_get_element_size (sort_info->priv->groupings));
	}

	for (ii = 0; ii < new_info->priv->groupings->len; ii++) {
		ColumnData *column_data;

		column_data = &g_array_index (new_info->priv->groupings, ColumnData, ii);

		g_object_ref (column_data->column_spec);
	}

	g_array_set_size (
		new_info->priv->sortings,
		sort_info->priv->sortings->len);
	if (new_info->priv->sortings->data &&
	    sort_info->priv->sortings->data &&
	    sort_info->priv->sortings->len) {
		memmove (
			new_info->priv->sortings->data,
			sort_info->priv->sortings->data,
			sort_info->priv->sortings->len *
			g_array_get_element_size (sort_info->priv->sortings));
	}

	for (ii = 0; ii < new_info->priv->sortings->len; ii++) {
		ColumnData *column_data;

		column_data = &g_array_index (new_info->priv->sortings, ColumnData, ii);

		g_object_ref (column_data->column_spec);
	}

	new_info->priv->can_group = sort_info->priv->can_group;

	return new_info;
}

