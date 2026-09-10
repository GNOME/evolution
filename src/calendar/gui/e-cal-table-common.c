/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "comp-util.h"

#include "e-cal-table-common.h"

void
e_cal_table_common_setup_columns (EVirtualTree *vtree,
				  gpointer owner,
				  const ECalTableColumnDef *defs,
				  guint n_defs,
				  EVirtualTreeCellDataFunc data_func,
				  ECalTableCreateSpecialRendererFunc create_special_renderer,
				  gpointer create_special_renderer_user_data,
				  ECalTableColumnCreatedFunc on_column_created,
				  gpointer on_column_created_user_data,
				  gint expander_field)
{
	guint ii, color_col = 0;

	for (ii = 0; ii < n_defs; ii++) {
		const ECalTableColumnDef *def = &defs[ii];
		ECalTableColumnFuncData *func_data;
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *tvc;
		gboolean center_align = FALSE;
		guint col;

		col = e_virtual_tree_add_column (vtree, def->id, gettext (def->title));

		if (def->field == E_CAL_MODEL_FIELD_COLOR)
			color_col = col;

		func_data = g_new0 (ECalTableColumnFuncData, 1);
		func_data->self = owner;
		func_data->def = def;

		renderer = create_special_renderer ? create_special_renderer (def->field, create_special_renderer_user_data, &center_align) : NULL;

		if (!renderer) {
			if (def->field == E_CAL_MODEL_FIELD_COLOR) {
				renderer = e_cell_renderer_color_new ();
			} else if (def->field == E_CAL_MODEL_FIELD_ICON) {
				renderer = gtk_cell_renderer_pixbuf_new ();
				g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
			} else {
				renderer = gtk_cell_renderer_text_new ();
				g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
			}
		}

		e_virtual_tree_column_pack_start (vtree, col, 0,
			renderer, TRUE, data_func, func_data, g_free);

		g_object_set_data (G_OBJECT (renderer), "e-cal-field", GINT_TO_POINTER (def->field));

		tvc = e_virtual_tree_get_column (vtree, col);
		gtk_tree_view_column_set_visible (tvc, def->default_visible);

		if (def->field == E_CAL_MODEL_FIELD_ICON || def->field == E_CAL_MODEL_FIELD_COLOR || center_align) {
			gtk_tree_view_column_set_alignment (tvc, 0.5);
			gtk_tree_view_column_set_resizable (tvc, FALSE);
		} else {
			e_virtual_tree_set_column_min_width (vtree, col, 40);
		}

		if (on_column_created)
			on_column_created (def->field, col, on_column_created_user_data);

		if (expander_field >= 0 && def->field == expander_field)
			e_virtual_tree_set_expander_column (vtree, col, 0);
	}

	gtk_tree_view_move_column_after (e_virtual_tree_get_tree_view (vtree),
		e_virtual_tree_get_column (vtree, color_col), NULL);
}

void
e_cal_table_common_set_color_cell (ECalModel *model,
				   ECalModelComponent *comp_data,
				   GtkCellRenderer *renderer)
{
	const gchar *color_str = e_cal_model_get_color_for_component (model, comp_data);
	GdkRGBA rgba;

	if (color_str && *color_str && gdk_rgba_parse (&rgba, color_str))
		g_object_set (renderer, "rgba", &rgba, NULL);
	else
		g_object_set (renderer, "rgba", NULL, NULL);
}

void
e_cal_table_common_set_icon_cell (ECalModel *model,
				  ECalModelComponent *comp_data,
				  GtkCellRenderer *renderer,
				  const gchar * const *icon_names,
				  guint n_icon_names)
{
	gint icon_index = GPOINTER_TO_INT (e_cal_model_get_field_value (model, comp_data, E_CAL_MODEL_FIELD_ICON));

	icon_index = CLAMP (icon_index, 0, (gint) n_icon_names - 1);

	g_object_set (renderer, "icon-name", icon_names[icon_index], NULL);
}

gchar *
e_cal_table_common_date_value_to_text (ECalModel *model,
				       ECalModelComponent *comp_data,
				       gint field)
{
	gpointer value = e_cal_model_get_field_value (model, comp_data, field);
	gchar *text = e_cal_model_date_value_to_string (model, value);

	e_date_edit_value_free (value);

	return text;
}

gchar *
e_cal_table_common_row_tooltip_markup_cb (EVirtualTree *vtree,
					  GObject *row_object,
					  gpointer user_data)
{
	ECalModel *model = user_data;
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);
	ECalComponent *new_comp;
	gchar *markup;

	if (!comp_data->icalcomp)
		return NULL;

	new_comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (comp_data->icalcomp));
	if (!new_comp)
		return NULL;

	markup = cal_comp_util_dup_tooltip (new_comp, comp_data->client,
		e_cal_model_get_registry (model),
		e_cal_model_get_timezone (model));

	g_object_unref (new_comp);

	return markup;
}

GtkListStore *
e_cal_table_common_build_status_store (ECalModel *model)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GList *strings, *link;

	store = gtk_list_store_new (1, G_TYPE_STRING);

	strings = cal_comp_util_get_status_list_for_kind (e_cal_model_get_component_kind (model));

	for (link = strings; link; link = g_list_next (link)) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, (const gchar *) link->data, -1);
	}

	g_list_free (strings);

	return store;
}

void
e_cal_table_common_column_state_changed (EVirtualTree *vtree,
					 const ECalTableColumnDef *defs,
					 guint n_defs,
					 ECalModel *model)
{
	ECalModelSortColumn *sort_columns;
	gint *priorities;
	guint n_sort_columns = 0;
	guint ii, jj;

	sort_columns = g_new (ECalModelSortColumn, n_defs);
	priorities = g_new (gint, n_defs);

	for (ii = 0; ii < n_defs; ii++) {
		GtkSortType sort_order;
		gint priority;

		if (!e_virtual_tree_get_column_sort (vtree, ii, &sort_order, &priority))
			continue;

		jj = n_sort_columns;
		while (jj > 0 && priorities[jj - 1] > priority) {
			priorities[jj] = priorities[jj - 1];
			sort_columns[jj] = sort_columns[jj - 1];
			jj--;
		}

		priorities[jj] = priority;
		sort_columns[jj].field = defs[ii].field;
		sort_columns[jj].order = sort_order;
		n_sort_columns++;
	}

	e_cal_model_set_sort_columns (model, sort_columns, n_sort_columns);

	g_free (sort_columns);
	g_free (priorities);
}

const gchar * const *
e_cal_table_common_get_legacy_etable_column_ids (const ECalTableColumnDef *defs,
						 guint n_defs,
						 const gchar **out_array,
						 guint *out_n_column_ids)
{
	guint ii;

	for (ii = 0; ii < n_defs - 1; ii++) {
		out_array[ii] = defs[ii].id;
	}

	if (out_n_column_ids)
		*out_n_column_ids = n_defs - 1;

	return out_array;
}

gpointer
e_cal_table_common_get_legacy_etable_column_map_cb (EVirtualTree *vtree,
						    guint *out_n_column_ids,
						    gpointer user_data)
{
	ECalTableGetLegacyColumnIdsFunc get_legacy_etable_column_ids_func = user_data;

	return (gpointer) get_legacy_etable_column_ids_func (out_n_column_ids);
}
