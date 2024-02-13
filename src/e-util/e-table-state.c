/*
 * e-table-state.c
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

#include "e-table-state.h"

#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <libedataserver/libedataserver.h>

#include "e-table-specification.h"
#include "e-xml-utils.h"

#define STATE_VERSION 0.1

typedef struct _ParseData ParseData;

struct _ETableStatePrivate {
	GWeakRef specification;
};

enum {
	PROP_0,
	PROP_SPECIFICATION
};

struct _ParseData {
	ETableState *state;
	GVariantBuilder *column_info;
};

G_DEFINE_TYPE_WITH_PRIVATE (ETableState, e_table_state, G_TYPE_OBJECT)

static ParseData *
parse_data_new (ETableSpecification *specification)
{
	ParseData *parse_data;
	const GVariantType *type;

	type = G_VARIANT_TYPE ("a(xd)");

	parse_data = g_slice_new0 (ParseData);
	parse_data->state = e_table_state_new (specification);
	parse_data->column_info = g_variant_builder_new (type);

	return parse_data;
}

static void
parse_data_free (ParseData *parse_data)
{
	g_object_unref (parse_data->state);
	g_variant_builder_unref (parse_data->column_info);
	g_slice_free (ParseData, parse_data);
}

static void
table_state_parser_start_column (GMarkupParseContext *context,
                                 const gchar *element_name,
                                 const gchar **attribute_names,
                                 const gchar **attribute_values,
                                 GVariantBuilder *column_info,
                                 GError **error)
{
	const gchar *index_str;
	const gchar *expansion_str;
	gboolean success;

	success = g_markup_collect_attributes (
		element_name,
		attribute_names,
		attribute_values,
		error,

		G_MARKUP_COLLECT_STRING,
		"source",
		&index_str,

		G_MARKUP_COLLECT_STRING |
		G_MARKUP_COLLECT_OPTIONAL,
		"expansion",
		&expansion_str,

		G_MARKUP_COLLECT_INVALID);

	if (success) {
		gint64 index;
		gdouble expansion = 1.0;

		g_return_if_fail (index_str != NULL);
		index = g_ascii_strtoll (index_str, NULL, 10);

		if (expansion_str != NULL)
			expansion = g_ascii_strtod (expansion_str, NULL);

		g_variant_builder_add (
			column_info, "(xd)", index, expansion);
	}
}

static void
table_state_parser_start_element (GMarkupParseContext *context,
                                  const gchar *element_name,
                                  const gchar **attribute_names,
                                  const gchar **attribute_values,
                                  gpointer user_data,
                                  GError **error)
{
	ParseData *parse_data = user_data;
	ETableSpecification *specification;

	specification = e_table_state_ref_specification (parse_data->state);

	if (g_str_equal (element_name, "column"))
		table_state_parser_start_column (
			context,
			element_name,
			attribute_names,
			attribute_values,
			parse_data->column_info,
			error);

	if (g_str_equal (element_name, "grouping"))
		e_table_sort_info_parse_context_push (
			context, specification);

	g_object_unref (specification);
}

static void
table_state_parser_end_element (GMarkupParseContext *context,
                                const gchar *element_name,
                                gpointer user_data,
                                GError **error)
{
	ParseData *parse_data = user_data;

	if (g_str_equal (element_name, "grouping")) {
		ETableSortInfo *sort_info;

		sort_info = e_table_sort_info_parse_context_pop (context);
		g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));

		g_clear_object (&parse_data->state->sort_info);
		parse_data->state->sort_info = g_object_ref (sort_info);

		g_object_unref (sort_info);
	}
}

static void
table_state_parser_error (GMarkupParseContext *context,
                          GError *error,
                          gpointer user_data)
{
	parse_data_free ((ParseData *) user_data);
}

static const GMarkupParser table_state_parser = {
	table_state_parser_start_element,
	table_state_parser_end_element,
	NULL,
	NULL,
	table_state_parser_error
};

static void
table_state_set_specification (ETableState *state,
                               ETableSpecification *specification)
{
	g_return_if_fail (E_IS_TABLE_SPECIFICATION (specification));

	g_weak_ref_set (&state->priv->specification, specification);
}

static void
table_state_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SPECIFICATION:
			table_state_set_specification (
				E_TABLE_STATE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
table_state_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SPECIFICATION:
			g_value_take_object (
				value,
				e_table_state_ref_specification (
				E_TABLE_STATE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
table_state_dispose (GObject *object)
{
	ETableState *state = E_TABLE_STATE (object);
	gint ii;

	for (ii = 0; ii < state->col_count; ii++)
		g_clear_object (&state->column_specs[ii]);
	state->col_count = 0;

	g_clear_object (&state->sort_info);
	g_weak_ref_set (&state->priv->specification, NULL);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_state_parent_class)->dispose (object);
}

static void
table_state_finalize (GObject *object)
{
	ETableState *state = E_TABLE_STATE (object);

	g_free (state->column_specs);
	g_free (state->expansions);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_table_state_parent_class)->finalize (object);
}

static void
table_state_constructed (GObject *object)
{
	ETableState *state;
	ETableSpecification *specification;

	state = E_TABLE_STATE (object);

	specification = e_table_state_ref_specification (state);
	state->sort_info = e_table_sort_info_new (specification);
	g_object_unref (specification);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_table_state_parent_class)->constructed (object);
}

static void
e_table_state_class_init (ETableStateClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = table_state_set_property;
	object_class->get_property = table_state_get_property;
	object_class->dispose = table_state_dispose;
	object_class->finalize = table_state_finalize;
	object_class->constructed = table_state_constructed;

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
}

static void
e_table_state_init (ETableState *state)
{
	state->priv = e_table_state_get_instance_private (state);
}

ETableState *
e_table_state_new (ETableSpecification *specification)
{
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);

	return g_object_new (
		E_TYPE_TABLE_STATE,
		"specification", specification, NULL);
}

ETableState *
e_table_state_vanilla (ETableSpecification *specification)
{
	ETableState *state;
	GPtrArray *columns;
	GString *str;
	guint ii;

	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);

	columns = e_table_specification_ref_columns (specification);

	str = g_string_new ("<ETableState>\n");
	for (ii = 0; ii < columns->len; ii++)
		g_string_append_printf (str, "  <column source=\"%d\"/>\n", ii);
	g_string_append (str, "  <grouping></grouping>\n");
	g_string_append (str, "</ETableState>\n");

	g_ptr_array_unref (columns);

	state = e_table_state_new (specification);
	e_table_state_load_from_string (state, str->str);

	g_string_free (str, TRUE);

	return state;
}

/**
 * e_table_state_parse_context_push:
 * @context: a #GMarkupParseContext
 * @specification: an #ETableSpecification
 *
 * Creates a new #ETableState from a segment of XML data being fed to
 * @context.  Call this function for the appropriate opening tag from the
 * <structfield>start_element</structfield> callback of a #GMarkupParser,
 * then call e_table_state_parse_context_pop() for the corresponding
 * closing tag from the <structfield>end_element</structfield> callback.
 **/
void
e_table_state_parse_context_push (GMarkupParseContext *context,
                                  ETableSpecification *specification)
{
	g_return_if_fail (context != NULL);
	g_return_if_fail (E_IS_TABLE_SPECIFICATION (specification));

	g_markup_parse_context_push (
		context, &table_state_parser,
		parse_data_new (specification));
}

/**
 * e_table_state_parse_context_pop:
 * @context: a #GMarkupParseContext
 *
 * Creates a new #ETableState from a segment of XML data being fed to
 * @context.  Call e_table_state_parse_context_push() for the appropriate
 * opening tag from the <structfield>start_element</structfield> callback of
 * a #GMarkupParser, then call this function for the corresponding closing
 * tag from the <structfield>end_element</structfield> callback.
 *
 * Unreference the newly-created #ETableState with g_object_unref() when
 * finished with it.
 *
 * Returns: an #ETableState
 **/
ETableState *
e_table_state_parse_context_pop (GMarkupParseContext *context)
{
	ETableSpecification *specification;
	ParseData *parse_data;
	GPtrArray *columns;
	ETableState *state;
	GVariant *variant;
	GVariantIter iter;
	gint64 index;
	gdouble expansion;
	gsize length, ii = 0;

	g_return_val_if_fail (context != NULL, NULL);

	parse_data = g_markup_parse_context_pop (context);
	g_return_val_if_fail (parse_data != NULL, NULL);

	state = g_object_ref (parse_data->state);

	specification = e_table_state_ref_specification (state);
	columns = e_table_specification_ref_columns (specification);

	variant = g_variant_builder_end (parse_data->column_info);
	length = g_variant_iter_init (&iter, variant);

	state->column_specs = g_new0 (ETableColumnSpecification *, length);
	state->expansions = g_new0 (gdouble, length);
	state->col_count = length;

	while (g_variant_iter_next (&iter, "(xd)", &index, &expansion)) {
		if (index < columns->len) {
			ETableColumnSpecification *column_spec;

			column_spec = g_ptr_array_index (columns, index);
			state->column_specs[ii] = g_object_ref (column_spec);
			state->expansions[ii] = expansion;

			ii++;
		}
	}

	g_variant_unref (variant);

	g_object_unref (specification);
	g_ptr_array_unref (columns);

	parse_data_free (parse_data);

	return state;
}

/**
 * e_table_state_ref_specification:
 * @state: an #ETableState
 *
 * Returns the #ETableSpecification passed to e_table_state_new().
 *
 * The returned #ETableSpecification is referenced for thread-safety and must
 * be unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #ETableSpecification
 **/
ETableSpecification *
e_table_state_ref_specification (ETableState *state)
{
	g_return_val_if_fail (E_IS_TABLE_STATE (state), NULL);

	return g_weak_ref_get (&state->priv->specification);
}

gboolean
e_table_state_load_from_file (ETableState *state,
                              const gchar *filename)
{
	xmlDoc *doc;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_TABLE_STATE (state), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	doc = e_xml_parse_file (filename);
	if (doc != NULL) {
		xmlNode *node = xmlDocGetRootElement (doc);
		e_table_state_load_from_node (state, node);
		xmlFreeDoc (doc);
		success = TRUE;
	}

	return success;
}

void
e_table_state_load_from_string (ETableState *state,
                                const gchar *xml)
{
	xmlDoc *doc;

	g_return_if_fail (E_IS_TABLE_STATE (state));
	g_return_if_fail (xml != NULL);

	doc = xmlParseMemory ((gchar *) xml, strlen (xml));
	if (doc != NULL) {
		xmlNode *node = xmlDocGetRootElement (doc);
		e_table_state_load_from_node (state, node);
		xmlFreeDoc (doc);
	}
}

typedef struct {
	gint column;
	gdouble expansion;
} int_and_double;

void
e_table_state_load_from_node (ETableState *state,
                              const xmlNode *node)
{
	ETableSpecification *specification;
	xmlNode *children;
	GList *list = NULL, *iterator;
	GPtrArray *columns;
	gdouble state_version;
	gint i;
	gboolean can_group = TRUE;

	g_return_if_fail (E_IS_TABLE_STATE (state));
	g_return_if_fail (node != NULL);

	specification = e_table_state_ref_specification (state);
	columns = e_table_specification_ref_columns (specification);

	state_version = e_xml_get_double_prop_by_name_with_default (
		node, (const guchar *)"state-version", STATE_VERSION);

	if (state->sort_info) {
		can_group = e_table_sort_info_get_can_group (state->sort_info);
		g_object_unref (state->sort_info);
	}

	state->sort_info = NULL;
	children = node->xmlChildrenNode;
	for (; children; children = children->next) {
		if (!strcmp ((gchar *) children->name, "column")) {
			int_and_double *column_info;
			gint column_source;

			column_source = e_xml_get_integer_prop_by_name (children, (const guchar *) "source");
			if (column_source < 0 || column_source >= columns->len)
				continue;

			column_info = g_new (int_and_double, 1);
			column_info->column = column_source;
			column_info->expansion =
				e_xml_get_double_prop_by_name_with_default (
					children, (const guchar *)"expansion", 1);

			list = g_list_append (list, column_info);
		} else if (state->sort_info == NULL &&
			   !strcmp ((gchar *) children->name, "grouping")) {
			state->sort_info =
				e_table_sort_info_new (specification);
			e_table_sort_info_load_from_node (
				state->sort_info, children, state_version);
		}
	}

	for (i = 0; i < state->col_count; i++)
		g_clear_object (&state->column_specs[i]);
	g_free (state->column_specs);
	g_free (state->expansions);

	state->col_count = g_list_length (list);
	state->column_specs = g_new (
		ETableColumnSpecification *, state->col_count);
	state->expansions = g_new (double, state->col_count);

	if (state->sort_info == NULL)
		state->sort_info = e_table_sort_info_new (specification);
	e_table_sort_info_set_can_group (state->sort_info, can_group);

	for (iterator = list, i = 0; iterator; i++) {
		ETableColumnSpecification *column_spec;
		int_and_double *column_info = iterator->data;

		column_spec = columns->pdata[column_info->column];

		state->column_specs[i] = g_object_ref (column_spec);
		state->expansions[i] = column_info->expansion;

		g_free (column_info);

		iterator = g_list_next (iterator);
	}
	g_list_free (list);

	g_object_unref (specification);
	g_ptr_array_unref (columns);
}

void
e_table_state_save_to_file (ETableState *state,
                            const gchar *filename)
{
	xmlDoc *doc;
	xmlNode *node;

	doc = xmlNewDoc ((const guchar *)"1.0");

	node = e_table_state_save_to_node (state, NULL);
	xmlDocSetRootElement (doc, node);

	e_xml_save_file (filename, doc);

	xmlFreeDoc (doc);
}

gchar *
e_table_state_save_to_string (ETableState *state)
{
	gchar *ret_val;
	xmlChar *string;
	gint length;
	xmlDoc *doc;
	xmlNode *node;

	g_return_val_if_fail (E_IS_TABLE_STATE (state), NULL);

	doc = xmlNewDoc ((const guchar *)"1.0");

	node = e_table_state_save_to_node (state, NULL);
	xmlDocSetRootElement (doc, node);

	xmlDocDumpMemory (doc, &string, &length);
	ret_val = g_strdup ((gchar *) string);
	xmlFree (string);

	xmlFreeDoc (doc);

	return ret_val;
}

xmlNode *
e_table_state_save_to_node (ETableState *state,
                            xmlNode *parent)
{
	ETableSpecification *specification;
	xmlNode *node;
	gint ii;

	g_return_val_if_fail (E_IS_TABLE_STATE (state), NULL);

	specification = e_table_state_ref_specification (state);

	if (parent)
		node = xmlNewChild (
			parent, NULL, (const guchar *) "ETableState", NULL);
	else
		node = xmlNewNode (NULL, (const guchar *) "ETableState");

	e_xml_set_double_prop_by_name (
		node, (const guchar *) "state-version", STATE_VERSION);

	for (ii = 0; ii < state->col_count; ii++) {
		xmlNode *new_node;
		gint index;

		index = e_table_specification_get_column_index (
			specification, state->column_specs[ii]);

		if (index < 0) {
			g_warn_if_reached ();
			continue;
		}

		new_node = xmlNewChild (
			node, NULL, (const guchar *) "column", NULL);
		e_xml_set_integer_prop_by_name (
			new_node, (const guchar *) "source", index);
		if (state->expansions[ii] >= -1)
			e_xml_set_double_prop_by_name (
				new_node, (const guchar *)
				"expansion", state->expansions[ii]);
	}

	e_table_sort_info_save_to_node (state->sort_info, node);

	g_object_unref (specification);

	return node;
}

/**
 * e_table_state_duplicate:
 * @state: an #ETableState
 *
 * Creates a new #ETableState cloned from @state.
 *
 * Returns: a new #ETableState
 */
ETableState *
e_table_state_duplicate (ETableState *state)
{
	ETableState *new_state;
	ETableSpecification *specification;
	gboolean can_group;
	gchar *copy;

	g_return_val_if_fail (E_IS_TABLE_STATE (state), NULL);

	specification = e_table_state_ref_specification (state);
	new_state = e_table_state_new (specification);
	g_object_unref (specification);

	copy = e_table_state_save_to_string (state);
	e_table_state_load_from_string (new_state, copy);
	g_free (copy);

	can_group = e_table_sort_info_get_can_group (state->sort_info);
	e_table_sort_info_set_can_group (new_state->sort_info, can_group);

	return new_state;
}
