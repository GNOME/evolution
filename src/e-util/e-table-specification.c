/*
 * e-table-specification.c
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

#include "e-table-specification.h"

#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include <libedataserver/libedataserver.h>

struct _ETableSpecificationPrivate {
	GPtrArray *columns;
	gchar *filename;
};

enum {
	PROP_0,
	PROP_FILENAME
};

/* Forward Declarations */
static void	e_table_specification_initable_init
						(GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ETableSpecification, e_table_specification, G_TYPE_OBJECT,
	G_ADD_PRIVATE (ETableSpecification)
	G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, e_table_specification_initable_init))

static void
table_specification_start_specification (GMarkupParseContext *context,
                                         const gchar *element_name,
                                         const gchar **attribute_names,
                                         const gchar **attribute_values,
                                         ETableSpecification *specification,
                                         GError **error)
{
	const gchar *cursor_mode = NULL;
	const gchar *selection_mode = NULL;
	gboolean fallback_draw_grid = FALSE;
	gboolean missing;

	g_free (specification->click_to_add_message);
	specification->click_to_add_message = NULL;

	g_free (specification->domain);
	specification->domain = NULL;

	/* Use G_MARKUP_COLLECT_TRISTATE to identify
	 * missing attributes that default to TRUE. */
	g_markup_collect_attributes (
		element_name,
		attribute_names,
		attribute_values,
		error,

		G_MARKUP_COLLECT_TRISTATE,
		"alternating-row-colors",
		&specification->alternating_row_colors,

		G_MARKUP_COLLECT_BOOLEAN |
		G_MARKUP_COLLECT_OPTIONAL,
		"no-headers",
		&specification->no_headers,

		G_MARKUP_COLLECT_BOOLEAN |
		G_MARKUP_COLLECT_OPTIONAL,
		"click-to-add",
		&specification->click_to_add,

		G_MARKUP_COLLECT_BOOLEAN |
		G_MARKUP_COLLECT_OPTIONAL,
		"click-to-add-end",
		&specification->click_to_add_end,

		G_MARKUP_COLLECT_TRISTATE,
		"horizontal-draw-grid",
		&specification->horizontal_draw_grid,

		G_MARKUP_COLLECT_TRISTATE,
		"vertical-draw-grid",
		&specification->vertical_draw_grid,

		G_MARKUP_COLLECT_BOOLEAN |
		G_MARKUP_COLLECT_OPTIONAL,
		"draw-grid",
		&fallback_draw_grid,

		G_MARKUP_COLLECT_TRISTATE,
		"draw-focus",
		&specification->draw_focus,

		G_MARKUP_COLLECT_BOOLEAN |
		G_MARKUP_COLLECT_OPTIONAL,
		"horizontal-scrolling",
		&specification->horizontal_scrolling,

		G_MARKUP_COLLECT_BOOLEAN |
		G_MARKUP_COLLECT_OPTIONAL,
		"horizontal-resize",
		&specification->horizontal_resize,

		G_MARKUP_COLLECT_TRISTATE,
		"allow-grouping",
		&specification->allow_grouping,

		G_MARKUP_COLLECT_STRING |
		G_MARKUP_COLLECT_OPTIONAL,
		"selection-mode",
		&selection_mode,

		G_MARKUP_COLLECT_STRING |
		G_MARKUP_COLLECT_OPTIONAL,
		"cursor-mode",
		&cursor_mode,

		G_MARKUP_COLLECT_STRDUP |
		G_MARKUP_COLLECT_OPTIONAL,
		"_click-to-add-message",
		&specification->click_to_add_message,

		G_MARKUP_COLLECT_STRDUP |
		G_MARKUP_COLLECT_OPTIONAL,
		"gettext-domain",
		&specification->domain,

		G_MARKUP_COLLECT_INVALID);

	/* Additional tweaks. */

	missing =
		(specification->alternating_row_colors != TRUE) &&
		(specification->alternating_row_colors != FALSE);
	if (missing)
		specification->alternating_row_colors = TRUE;

	if (!specification->click_to_add)
		specification->click_to_add_end = FALSE;

	missing =
		(specification->horizontal_draw_grid != TRUE) &&
		(specification->horizontal_draw_grid != FALSE);
	if (missing)
		specification->horizontal_draw_grid = fallback_draw_grid;

	missing =
		(specification->vertical_draw_grid != TRUE) &&
		(specification->vertical_draw_grid != FALSE);
	if (missing)
		specification->vertical_draw_grid = fallback_draw_grid;

	missing =
		(specification->draw_focus != TRUE) &&
		(specification->draw_focus != FALSE);
	if (missing)
		specification->draw_focus = TRUE;

	missing =
		(specification->allow_grouping != TRUE) &&
		(specification->allow_grouping != FALSE);
	if (missing)
		specification->allow_grouping = TRUE;

	if (selection_mode == NULL)  /* attribute missing */
		specification->selection_mode = GTK_SELECTION_MULTIPLE;
	else if (g_ascii_strcasecmp (selection_mode, "single") == 0)
		specification->selection_mode = GTK_SELECTION_SINGLE;
	else if (g_ascii_strcasecmp (selection_mode, "browse") == 0)
		specification->selection_mode = GTK_SELECTION_BROWSE;
	else if (g_ascii_strcasecmp (selection_mode, "extended") == 0)
		specification->selection_mode = GTK_SELECTION_MULTIPLE;
	else  /* unrecognized attribute value */
		specification->selection_mode = GTK_SELECTION_MULTIPLE;

	if (cursor_mode == NULL)  /* attribute missing */
		specification->cursor_mode = E_CURSOR_SIMPLE;
	else if (g_ascii_strcasecmp (cursor_mode, "line") == 0)
		specification->cursor_mode = E_CURSOR_LINE;
	else if (g_ascii_strcasecmp (cursor_mode, "spreadsheet") == 0)
		specification->cursor_mode = E_CURSOR_SPREADSHEET;
	else  /* unrecognized attribute value */
		specification->cursor_mode = E_CURSOR_SIMPLE;

	if (specification->domain != NULL && *specification->domain == '\0') {
		g_free (specification->domain);
		specification->domain = NULL;
	}
}

static void
table_specification_start_column (GMarkupParseContext *context,
                                  const gchar *element_name,
                                  const gchar **attribute_names,
                                  const gchar **attribute_values,
                                  GPtrArray *columns,
                                  GError **error)
{
	ETableColumnSpecification *column_spec;
	const gchar *model_col_str = NULL;
	const gchar *compare_col_str = NULL;
	const gchar *expansion_str = NULL;
	const gchar *minimum_width_str = NULL;
	const gchar *priority_str = NULL;
	gint64 int_value;
	gboolean missing;

	column_spec = e_table_column_specification_new ();

	/* Use G_MARKUP_COLLECT_TRISTATE to identify
	 * missing attributes that default to TRUE. */
	g_markup_collect_attributes (
		element_name,
		attribute_names,
		attribute_values,
		error,

		G_MARKUP_COLLECT_STRING |
		G_MARKUP_COLLECT_OPTIONAL,
		"model_col",
		&model_col_str,

		G_MARKUP_COLLECT_STRING |
		G_MARKUP_COLLECT_OPTIONAL,
		"compare_col",
		&compare_col_str,

		G_MARKUP_COLLECT_STRDUP |
		G_MARKUP_COLLECT_OPTIONAL,
		"_title",
		&column_spec->title,

		G_MARKUP_COLLECT_STRDUP |
		G_MARKUP_COLLECT_OPTIONAL,
		"pixbuf",
		&column_spec->pixbuf,

		G_MARKUP_COLLECT_STRING |
		G_MARKUP_COLLECT_OPTIONAL,
		"expansion",
		&expansion_str,

		G_MARKUP_COLLECT_STRING |
		G_MARKUP_COLLECT_OPTIONAL,
		"minimum_width",
		&minimum_width_str,

		G_MARKUP_COLLECT_BOOLEAN |
		G_MARKUP_COLLECT_OPTIONAL,
		"resizable",
		&column_spec->resizable,

		G_MARKUP_COLLECT_BOOLEAN |
		G_MARKUP_COLLECT_OPTIONAL,
		"disabled",
		&column_spec->disabled,

		G_MARKUP_COLLECT_STRDUP |
		G_MARKUP_COLLECT_OPTIONAL,
		"cell",
		&column_spec->cell,

		G_MARKUP_COLLECT_STRDUP |
		G_MARKUP_COLLECT_OPTIONAL,
		"compare",
		&column_spec->compare,

		G_MARKUP_COLLECT_STRDUP |
		G_MARKUP_COLLECT_OPTIONAL,
		"search",
		&column_spec->search,

		G_MARKUP_COLLECT_TRISTATE,
		"sortable",
		&column_spec->sortable,

		G_MARKUP_COLLECT_STRING |
		G_MARKUP_COLLECT_OPTIONAL,
		"priority",
		&priority_str,

		G_MARKUP_COLLECT_INVALID);

	/* Additional tweaks. */

	if (model_col_str != NULL) {
		int_value = g_ascii_strtoll (model_col_str, NULL, 10);
		column_spec->model_col = (gint) int_value;
		column_spec->compare_col = (gint) int_value;
	}

	if (compare_col_str != NULL) {
		int_value = g_ascii_strtoll (compare_col_str, NULL, 10);
		column_spec->compare_col = (gint) int_value;
	}

	if (column_spec->title == NULL)
		column_spec->title = g_strdup ("");

	if (expansion_str != NULL)
		column_spec->expansion = g_ascii_strtod (expansion_str, NULL);

	if (minimum_width_str != NULL) {
		int_value = g_ascii_strtoll (minimum_width_str, NULL, 10);
		column_spec->minimum_width = (gint) int_value;
	}

	if (priority_str != NULL) {
		int_value = g_ascii_strtoll (priority_str, NULL, 10);
		column_spec->priority = (gint) int_value;
	}

	missing =
		(column_spec->sortable != TRUE) &&
		(column_spec->sortable != FALSE);
	if (missing)
		column_spec->sortable = TRUE;

	g_ptr_array_add (columns, g_object_ref (column_spec));

	g_object_unref (column_spec);
}

static void
table_specification_start_element (GMarkupParseContext *context,
                                   const gchar *element_name,
                                   const gchar **attribute_names,
                                   const gchar **attribute_values,
                                   gpointer user_data,
                                   GError **error)
{
	ETableSpecification *specification;
	GPtrArray *columns;

	specification = E_TABLE_SPECIFICATION (user_data);
	columns = e_table_specification_ref_columns (specification);

	if (g_str_equal (element_name, "ETableSpecification"))
		table_specification_start_specification (
			context,
			element_name,
			attribute_names,
			attribute_values,
			specification,
			error);

	if (g_str_equal (element_name, "ETableColumn"))
		table_specification_start_column (
			context,
			element_name,
			attribute_names,
			attribute_values,
			columns,
			error);

	if (g_str_equal (element_name, "ETableState"))
		e_table_state_parse_context_push (context, specification);

	g_ptr_array_unref (columns);
}

static void
table_specification_end_element (GMarkupParseContext *context,
                                 const gchar *element_name,
                                 gpointer user_data,
                                 GError **error)
{
	ETableSpecification *specification;

	specification = E_TABLE_SPECIFICATION (user_data);

	if (g_str_equal (element_name, "ETableState")) {
		ETableState *state;

		state = e_table_state_parse_context_pop (context);
		g_return_if_fail (E_IS_TABLE_STATE (state));

		g_clear_object (&specification->state);
		specification->state = g_object_ref (state);

		g_object_unref (state);
	}
}

static const GMarkupParser table_specification_parser = {
	table_specification_start_element,
	table_specification_end_element,
	NULL,
	NULL,
	NULL
};

static void
table_specification_set_filename (ETableSpecification *specification,
                                  const gchar *filename)
{
	g_return_if_fail (filename != NULL);
	g_return_if_fail (specification->priv->filename == NULL);

	specification->priv->filename = g_strdup (filename);
}

static void
table_specification_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILENAME:
			table_specification_set_filename (
				E_TABLE_SPECIFICATION (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
table_specification_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILENAME:
			g_value_set_string (
				value,
				e_table_specification_get_filename (
				E_TABLE_SPECIFICATION (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
table_specification_dispose (GObject *object)
{
	ETableSpecification *specification;

	specification = E_TABLE_SPECIFICATION (object);

	g_clear_object (&specification->state);

	g_ptr_array_set_size (specification->priv->columns, 0);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_specification_parent_class)->dispose (object);
}

static void
table_specification_finalize (GObject *object)
{
	ETableSpecification *specification;

	specification = E_TABLE_SPECIFICATION (object);

	g_free (specification->click_to_add_message);
	g_free (specification->domain);

	g_ptr_array_unref (specification->priv->columns);
	g_free (specification->priv->filename);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_table_specification_parent_class)->finalize (object);
}

static gboolean
table_specification_initable_init (GInitable *initable,
                                   GCancellable *cancellable,
                                   GError **error)
{
	ETableSpecification *specification;
	GMarkupParseContext *context;
	const gchar *filename;
	gchar *contents = NULL;
	gboolean success = FALSE;

	specification = E_TABLE_SPECIFICATION (initable);
	filename = e_table_specification_get_filename (specification);
	g_return_val_if_fail (filename != NULL, FALSE);

	if (!g_file_get_contents (filename, &contents, NULL, error)) {
		g_warn_if_fail (contents == NULL);
		return FALSE;
	}

	context = g_markup_parse_context_new (
		&table_specification_parser,
		0,  /* no flags */
		g_object_ref (specification),
		(GDestroyNotify) g_object_unref);

	if (g_markup_parse_context_parse (context, contents, -1, error))
		success = g_markup_parse_context_end_parse (context, error);

	g_markup_parse_context_free (context);

	if (specification->state == NULL)
		specification->state = e_table_state_vanilla (specification);

	e_table_sort_info_set_can_group (
		specification->state->sort_info,
		specification->allow_grouping);

	g_free (contents);

	return success;
}

static void
e_table_specification_class_init (ETableSpecificationClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = table_specification_set_property;
	object_class->get_property = table_specification_get_property;
	object_class->dispose = table_specification_dispose;
	object_class->finalize = table_specification_finalize;

	g_object_class_install_property (
		object_class,
		PROP_FILENAME,
		g_param_spec_string (
			"filename",
			"Filename",
			"Name of the table specification file",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_table_specification_initable_init (GInitableIface *iface)
{
	iface->init = table_specification_initable_init;
}

static void
e_table_specification_init (ETableSpecification *specification)
{
	specification->priv = e_table_specification_get_instance_private (specification);
	specification->priv->columns =
		g_ptr_array_new_with_free_func (
		(GDestroyNotify) g_object_unref);

	specification->alternating_row_colors = TRUE;
	specification->no_headers = FALSE;
	specification->click_to_add = FALSE;
	specification->click_to_add_end = FALSE;
	specification->horizontal_draw_grid = FALSE;
	specification->vertical_draw_grid = FALSE;
	specification->draw_focus = TRUE;
	specification->horizontal_scrolling = FALSE;
	specification->horizontal_resize = FALSE;
	specification->allow_grouping = TRUE;

	specification->cursor_mode = E_CURSOR_SIMPLE;
	specification->selection_mode = GTK_SELECTION_MULTIPLE;
}

/**
 * e_table_specification_new:
 * @filename: a table specification file
 * @error: return location for a #GError, or %NULL
 *
 * Creates a new #ETableSpecification from @filename.  If a file or parse
 * error occurs, the function sets @error and returns %NULL.
 *
 * Returns: an #ETableSpecification, or %NULL
 */
ETableSpecification *
e_table_specification_new (const gchar *filename,
                           GError **error)
{
	return g_initable_new (
		E_TYPE_TABLE_SPECIFICATION, NULL, error,
		"filename", filename, NULL);
}

/**
 * e_table_specification_get_filename:
 * @specification: an #ETableSpecification
 *
 * Returns the filename from which @specification was loaded.
 *
 * Returns: the table specification filename
 **/
const gchar *
e_table_specification_get_filename (ETableSpecification *specification)
{
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);

	return specification->priv->filename;
}

/**
 * e_table_specification_ref_columns:
 * @specification: an #ETableSpecification
 *
 * Returns a #GPtrArray containing #ETableColumnSpecification instances for
 * all columns defined by @specification.  The array contents are owned by
 * the @specification and should not be modified.  Unreference the array
 * with g_ptr_array_unref() when finished with it.
 *
 * Returns: a #GPtrArray of #ETableColumnSpecification instances
 **/
GPtrArray *
e_table_specification_ref_columns (ETableSpecification *specification)
{
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);

	return g_ptr_array_ref (specification->priv->columns);
}

/**
 * e_table_specification_get_column_index:
 * @specification: an #ETableSpecification
 * @column_spec: an #ETableColumnSpecification
 *
 * Returns the zero-based index of @column_spec within @specification,
 * or a negative value if @column_spec is not defined by @specification.
 *
 * Returns: the column index of @column_spec, or a negative value
 **/
gint
e_table_specification_get_column_index (ETableSpecification *specification,
                                        ETableColumnSpecification *column_spec)
{
	GPtrArray *columns;
	gint column_index = -1;
	guint ii;

	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), -1);
	g_return_val_if_fail (E_IS_TABLE_COLUMN_SPECIFICATION (column_spec), -1);

	columns = e_table_specification_ref_columns (specification);

	for (ii = 0; ii < columns->len; ii++) {
		gboolean column_specs_equal;

		column_specs_equal =
			e_table_column_specification_equal (
			column_spec, columns->pdata[ii]);

		if (column_specs_equal) {
			column_index = (gint) ii;
			break;
		}
	}

	g_ptr_array_unref (columns);

	return column_index;
}

/**
 * e_table_specification_get_column_by_model_col:
 * @specification: an #ETableSpecification
 * @model_col: a model column index to get
 *
 * Get an #ETableColumnSpecification for the given @model_col.
 *
 * Returns: (transfer none) (nullable): an #ETableColumnSpecification for the given @model_col
 *    or %NULL, when not found.
 *
 * Since: 3.42
 **/
ETableColumnSpecification *
e_table_specification_get_column_by_model_col (ETableSpecification *specification,
					       gint model_col)
{
	GPtrArray *columns;
	ETableColumnSpecification *col_spec = NULL;
	guint ii;

	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);

	columns = e_table_specification_ref_columns (specification);

	for (ii = 0; ii < columns->len; ii++) {
		ETableColumnSpecification *adept = columns->pdata[ii];

		if (adept && adept->model_col == model_col) {
			col_spec = adept;
			break;
		}
	}

	g_ptr_array_unref (columns);

	return col_spec;
}
