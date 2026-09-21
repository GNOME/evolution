/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "e-cal-table-memos.h"

#include "comp-util.h"
#include "e-cal-table-common.h"

static const ECalTableColumnDef memo_table_columns[] = {
	{ "summary", N_("Summary"), E_CAL_MODEL_FIELD_SUMMARY, TRUE },
	{ "icon", N_("Type"), E_CAL_MODEL_FIELD_ICON, TRUE },
	{ "categories", N_("Categories"), E_CAL_MODEL_FIELD_CATEGORIES, TRUE },
	{ "start-date", N_("Start Date"), E_CAL_MODEL_FIELD_DTSTART, FALSE },
	{ "created", N_("Created"), E_CAL_MODEL_FIELD_CREATED, FALSE },
	{ "last-modified", N_("Last modified"), E_CAL_MODEL_FIELD_LASTMODIFIED, FALSE },
	{ "source", N_("Source"), E_CAL_MODEL_FIELD_SOURCE, FALSE },
	{ "status", N_("Status"), E_CAL_MODEL_FIELD_STATUS, FALSE },
	{ "memolist-color", N_("Color"), E_CAL_MODEL_FIELD_COLOR, TRUE }
};

#define N_MEMO_TABLE_COLUMNS (G_N_ELEMENTS (memo_table_columns))

static const gchar *memo_table_icon_names[] = {
	"stock_notes",
	"stock_insert-note"
};

struct _ECalTableMemos {
	ECalTableListBase parent_instance;
};

G_DEFINE_TYPE (ECalTableMemos, e_cal_table_memos, E_TYPE_CAL_TABLE_LIST_BASE)

static void
memo_table_column_data_func (EVirtualTree *vtree,
			     GtkCellRenderer *renderer,
			     GObject *row_object,
			     guint visible_row,
			     gpointer user_data)
{
	const ECalTableColumnFuncData *func_data = user_data;
	const ECalTableColumnDef *def = func_data->def;
	ECalTableMemos *self = func_data->self;
	ECalModel *model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);
	gboolean cancelled;

	if (def->field == E_CAL_MODEL_FIELD_COLOR) {
		e_cal_table_common_set_color_cell (model, comp_data, renderer);
		return;
	}

	cancelled = GPOINTER_TO_INT (e_cal_model_get_field_value (model, comp_data, E_CAL_MODEL_FIELD_CANCELLED)) != 0;

	if (def->field == E_CAL_MODEL_FIELD_ICON) {
		e_cal_table_common_set_icon_cell (model, comp_data, renderer, memo_table_icon_names, G_N_ELEMENTS (memo_table_icon_names));
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_DTSTART || def->field == E_CAL_MODEL_FIELD_CREATED ||
	    def->field == E_CAL_MODEL_FIELD_LASTMODIFIED) {
		gchar *text = e_cal_table_common_date_value_to_text (model, comp_data, def->field);

		g_object_set (renderer, "text", text, "editable", FALSE, "strikethrough", cancelled, NULL);

		g_free (text);
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_SOURCE) {
		gchar *text = e_cal_model_get_field_value (model, comp_data, def->field);

		g_object_set (renderer, "text", text, "editable", FALSE, "strikethrough", cancelled, NULL);

		g_free (text);
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_SUMMARY) {
		gchar *text = e_cal_model_get_field_value (model, comp_data, def->field);

		g_object_set (renderer, "text", text, "strikethrough", cancelled, NULL);

		g_free (text);
		return;
	}

	g_object_set (renderer, "text", e_cal_model_get_field_value (model, comp_data, def->field), "strikethrough", cancelled, NULL);
}

static gboolean
memo_table_cell_clicked_cb (EVirtualTree *vtree,
			    guint visible_row,
			    GObject *row_object,
			    guint col_idx,
			    GtkCellRenderer *hit_renderer,
			    gpointer user_data)
{
	static const gint date_fields[] = { E_CAL_MODEL_FIELD_DTSTART };
	ECalTableMemos *self = user_data;
	ECalModel *model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));

	return cal_comp_util_cell_clicked_edit_datetime_popover (vtree, visible_row, row_object, col_idx, hit_renderer,
		model, date_fields, G_N_ELEMENTS (date_fields), TRUE);
}

static void
memo_table_cell_edited_cb (EVirtualTree *vtree,
			   guint visible_row,
			   GObject *row_object,
			   guint col_idx,
			   GtkCellRenderer *renderer,
			   const gchar *new_text,
			   gpointer user_data)
{
	ECalTableMemos *self = user_data;
	ECalModel *model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);
	gint field = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "e-cal-field"));

	e_cal_model_set_field_value (model, comp_data, field, new_text, TRUE);
}

static GtkCellRenderer *
memo_table_create_special_renderer (gint field,
				    gpointer user_data,
				    gboolean *out_center_align)
{
	ECalTableMemos *self = user_data;
	GtkCellRenderer *renderer;
	GtkListStore *store;

	if (field != E_CAL_MODEL_FIELD_STATUS)
		return NULL;

	store = e_cal_table_common_build_status_store (e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self)));

	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (renderer,
		"model", store,
		"text-column", 0,
		"has-entry", FALSE,
		"ellipsize", PANGO_ELLIPSIZE_END,
		NULL);
	g_object_unref (store);

	return renderer;
}

static void
memo_table_setup_columns (gpointer self)
{
	EVirtualTree *vtree = e_cal_table_list_base_get_virtual_tree (E_CAL_TABLE_LIST_BASE (self));

	e_cal_table_common_setup_columns (vtree, self, memo_table_columns, N_MEMO_TABLE_COLUMNS,
		memo_table_column_data_func, memo_table_create_special_renderer, self,
		NULL, NULL, -1);
}

static void
memo_table_column_state_changed_cb (EVirtualTree *vtree,
				    gpointer user_data)
{
	ECalTableListBase *list_base = user_data;

	e_cal_table_common_column_state_changed (e_cal_table_list_base_get_virtual_tree (list_base),
		memo_table_columns, N_MEMO_TABLE_COLUMNS, e_cal_table_list_base_get_model (list_base));
}

const gchar * const *
e_cal_table_memos_get_legacy_etable_column_ids (guint *out_n_column_ids)
{
	static const gchar *column_ids[N_MEMO_TABLE_COLUMNS - 1];

	return e_cal_table_common_get_legacy_etable_column_ids (memo_table_columns, N_MEMO_TABLE_COLUMNS, column_ids, out_n_column_ids);
}

static void
memo_table_row_activated_cb (EVirtualTree *vtree,
			     guint visible_row,
			     GObject *row_object,
			     gpointer user_data)
{
	ECalTableListBase *list_base = user_data;

	e_cal_table_list_base_emit_open_component (list_base, E_CAL_MODEL_COMPONENT (row_object));
}

static gboolean
memo_table_right_click_cb (EVirtualTree *vtree,
			   guint visible_row,
			   GObject *row_object,
			   GdkEvent *event,
			   gpointer user_data)
{
	ECalTableListBase *list_base = user_data;

	e_cal_table_list_base_emit_popup_event (list_base, event);

	return TRUE;
}

static ECalComponentVType
memo_table_get_delete_confirm_vtype (ECalTableListBase *list_base)
{
	return E_CAL_COMPONENT_JOURNAL;
}

static void
e_cal_table_memos_class_init (ECalTableMemosClass *class)
{
	ECalTableListBaseClass *list_base_class;

	list_base_class = E_CAL_TABLE_LIST_BASE_CLASS (class);
	list_base_class->get_delete_confirm_vtype = memo_table_get_delete_confirm_vtype;
}

static void
e_cal_table_memos_init (ECalTableMemos *self)
{
}

GtkWidget *
e_cal_table_memos_new (EShellView *shell_view,
		       ECalModel *model)
{
	ECalTableMemos *self;
	ECalTableListBaseSetup setup;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	self = g_object_new (E_TYPE_CAL_TABLE_MEMOS, NULL);

	memset (&setup, 0, sizeof (ECalTableListBaseSetup));
	setup.accessible_name = _("Memos");
	setup.cut_tooltip = _("Cut selected memos to the clipboard");
	setup.copy_tooltip = _("Copy selected memos to the clipboard");
	setup.paste_tooltip = _("Paste memos from the clipboard");
	setup.delete_tooltip = _("Delete selected memos");
	setup.select_all_tooltip = _("Select all visible memos");
	setup.no_search_results_message = _("No memo satisfies your search criteria. "
		"Change search criteria by selecting a new "
		"Show memos filter from the drop down list "
		"above or by running a new search either by "
		"clearing it with Search->Clear menu item or "
		"by changing the query above.");
	setup.setup_columns_func = memo_table_setup_columns;
	setup.column_state_changed_cb = G_CALLBACK (memo_table_column_state_changed_cb);
	setup.row_activated_cb = G_CALLBACK (memo_table_row_activated_cb);
	setup.right_click_cb = G_CALLBACK (memo_table_right_click_cb);
	setup.cell_clicked_cb = G_CALLBACK (memo_table_cell_clicked_cb);
	setup.cell_edited_cb = G_CALLBACK (memo_table_cell_edited_cb);
	setup.get_legacy_etable_column_ids_func = e_cal_table_memos_get_legacy_etable_column_ids;

	e_cal_table_list_base_construct (E_CAL_TABLE_LIST_BASE (self), shell_view, model, &setup);

	return GTK_WIDGET (self);
}

