/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "e-cal-table-tasks.h"

#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-dialogs.h"
#include "e-cal-ops.h"
#include "e-cal-table-common.h"

static const ECalTableColumnDef task_table_columns[] = {
	{ "summary", N_("Summary"), E_CAL_MODEL_FIELD_SUMMARY, TRUE },
	{ "icon", N_("Type"), E_CAL_MODEL_FIELD_ICON, TRUE },
	{ "complete", N_("Complete"), E_CAL_MODEL_FIELD_COMPLETE, TRUE },
	{ "start-date", N_("Start date"), E_CAL_MODEL_FIELD_DTSTART, FALSE },
	{ "completion-date", N_("Completion date"), E_CAL_MODEL_FIELD_COMPLETED, FALSE },
	{ "due-date", N_("Due date"), E_CAL_MODEL_FIELD_DUE, FALSE },
	{ "percent-complete", N_("% Complete"), E_CAL_MODEL_FIELD_PERCENT, FALSE },
	{ "priority", N_("Priority"), E_CAL_MODEL_FIELD_PRIORITY, FALSE },
	{ "status", N_("Status"), E_CAL_MODEL_FIELD_STATUS, FALSE },
	{ "categories", N_("Categories"), E_CAL_MODEL_FIELD_CATEGORIES, FALSE },
	{ "location", N_("Location"), E_CAL_MODEL_FIELD_LOCATION, FALSE },
	{ "created", N_("Created"), E_CAL_MODEL_FIELD_CREATED, FALSE },
	{ "last-modified", N_("Last modified"), E_CAL_MODEL_FIELD_LASTMODIFIED, FALSE },
	{ "source", N_("Source"), E_CAL_MODEL_FIELD_SOURCE, FALSE },
	{ "estimated-duration", N_("Estimated duration"), E_CAL_MODEL_FIELD_ESTIMATED_DURATION, FALSE },
	{ "tasklist-color", N_("Color"), E_CAL_MODEL_FIELD_COLOR, TRUE }
};

#define N_TASK_TABLE_COLUMNS (G_N_ELEMENTS (task_table_columns))

static const gchar *task_table_icon_names[] = {
	"stock_task",
	"stock_task-recurring",
	"stock_task-assigned",
	"stock_task-assigned-to"
};

struct _ECalTableTasks {
	ECalTableListBase parent_instance;

	GCancellable *completed_cancellable;

	gulong notify_highlight_due_today_id;
	gulong notify_color_due_today_id;
	gulong notify_highlight_overdue_id;
	gulong notify_color_overdue_id;

	guint complete_column_index;

	ECalModelComponent *drag_comp_data; /* owned, set between tree-drag-begin and tree-drag-end */
};

#define TASK_TABLE_DND_TARGET_ROW "application/x-evolution-cal-model-row"

G_DEFINE_TYPE (ECalTableTasks, e_cal_table_tasks, E_TYPE_CAL_TABLE_LIST_BASE)

static void
task_table_column_data_func (EVirtualTree *vtree,
			     GtkCellRenderer *renderer,
			     GObject *row_object,
			     guint visible_row,
			     gpointer user_data)
{
	const ECalTableColumnFuncData *func_data = user_data;
	const ECalTableColumnDef *def = func_data->def;
	ECalTableTasks *self = func_data->self;
	ECalModel *model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);
	gboolean strikeout, overdue;

	if (def->field == E_CAL_MODEL_FIELD_COLOR) {
		e_cal_table_common_set_color_cell (model, comp_data, renderer);
		return;
	}

	strikeout = GPOINTER_TO_INT (e_cal_model_get_field_value (model, comp_data, E_CAL_MODEL_FIELD_STRIKEOUT)) != 0;
	overdue = GPOINTER_TO_INT (e_cal_model_get_field_value (model, comp_data, E_CAL_MODEL_FIELD_OVERDUE)) != 0;

	if (def->field == E_CAL_MODEL_FIELD_ICON) {
		e_cal_table_common_set_icon_cell (model, comp_data, renderer, task_table_icon_names, G_N_ELEMENTS (task_table_icon_names));
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_COMPLETE) {
		gboolean complete = GPOINTER_TO_INT (e_cal_model_get_field_value (model, comp_data, E_CAL_MODEL_FIELD_COMPLETE)) != 0;

		g_object_set (renderer, "active", complete, NULL);
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_DTSTART || def->field == E_CAL_MODEL_FIELD_CREATED ||
	    def->field == E_CAL_MODEL_FIELD_LASTMODIFIED || def->field == E_CAL_MODEL_FIELD_COMPLETED ||
	    def->field == E_CAL_MODEL_FIELD_DUE) {
		gchar *text = e_cal_table_common_date_value_to_text (model, comp_data, def->field);

		g_object_set (renderer, "text", text, "editable", FALSE,
			"strikethrough", strikeout, "weight", overdue ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, NULL);

		g_free (text);
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_PERCENT) {
		gint percent = GPOINTER_TO_INT (e_cal_model_get_field_value (model, comp_data, E_CAL_MODEL_FIELD_PERCENT));
		gchar *text = percent < 0 ? g_strdup (_("N/A")) : g_strdup_printf (_("%d%%"), percent);

		g_object_set (renderer, "text", text, "strikethrough", strikeout, "weight", overdue ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, NULL);

		g_free (text);
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_ESTIMATED_DURATION) {
		gpointer value = e_cal_model_get_field_value (model, comp_data, def->field);
		gchar *text = value ? e_cal_util_seconds_to_string (*(gint64 *) value) : g_strdup ("");

		g_object_set (renderer, "text", text, "editable", FALSE,
			"strikethrough", strikeout, "weight", overdue ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, NULL);

		g_free (text);
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_SOURCE) {
		gchar *text = e_cal_model_get_field_value (model, comp_data, def->field);

		g_object_set (renderer, "text", text, "editable", FALSE,
			"strikethrough", strikeout, "weight", overdue ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, NULL);

		g_free (text);
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_SUMMARY) {
		gchar *text = e_cal_model_get_field_value (model, comp_data, def->field);

		g_object_set (renderer, "text", text, "strikethrough", strikeout, "weight", overdue ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, NULL);

		g_free (text);
		return;
	}

	g_object_set (renderer, "text", e_cal_model_get_field_value (model, comp_data, def->field),
		"strikethrough", strikeout, "weight", overdue ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, NULL);
}

static gboolean
task_table_cell_clicked_cb (EVirtualTree *vtree,
			    guint visible_row,
			    GObject *row_object,
			    guint col_idx,
			    GtkCellRenderer *hit_renderer,
			    gpointer user_data)
{
	static const gint date_fields[] = {
		E_CAL_MODEL_FIELD_DTSTART,
		E_CAL_MODEL_FIELD_COMPLETED,
		E_CAL_MODEL_FIELD_DUE
	};
	ECalTableTasks *self = user_data;
	ECalModel *model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));

	if (col_idx == self->complete_column_index) {
		ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);
		gboolean complete;

		complete = GPOINTER_TO_INT (e_cal_model_get_field_value (model, comp_data, E_CAL_MODEL_FIELD_COMPLETE)) == 0;

		e_cal_model_set_field_value (model, comp_data, E_CAL_MODEL_FIELD_COMPLETE, GINT_TO_POINTER (complete), TRUE);

		return TRUE;
	}

	return cal_comp_util_cell_clicked_edit_datetime_popover (vtree, visible_row, row_object, col_idx, hit_renderer,
		model, date_fields, G_N_ELEMENTS (date_fields), TRUE);
}

static void
task_table_cell_edited_cb (EVirtualTree *vtree,
			   guint visible_row,
			   GObject *row_object,
			   guint col_idx,
			   GtkCellRenderer *renderer,
			   const gchar *new_text,
			   gpointer user_data)
{
	ECalTableTasks *self = user_data;
	ECalModel *model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);
	gint field = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "e-cal-field"));

	if (field == E_CAL_MODEL_FIELD_PERCENT) {
		gint percent = (gint) g_ascii_strtoll (new_text, NULL, 10);

		e_cal_model_set_field_value (model, comp_data, field, GINT_TO_POINTER (percent), TRUE);
	} else {
		e_cal_model_set_field_value (model, comp_data, field, new_text, TRUE);
	}
}

static GtkListStore *
task_table_build_combo_store (const gchar * const *values,
			      guint n_values)
{
	GtkListStore *store;
	GtkTreeIter iter;
	guint ii;

	store = gtk_list_store_new (1, G_TYPE_STRING);

	for (ii = 0; ii < n_values; ii++) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, values[ii], -1);
	}

	return store;
}

static GtkListStore *
task_table_build_percent_store (void)
{
	GtkListStore *store;
	GtkTreeIter iter;
	gint percent;

	store = gtk_list_store_new (1, G_TYPE_STRING);

	for (percent = 0; percent <= 100; percent += 10) {
		gchar *text = g_strdup_printf (_("%d%%"), percent);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, text, -1);

		g_free (text);
	}

	return store;
}

static GtkListStore *
task_table_build_priority_store (void)
{
	const gchar *values[4];

	values[0] = e_cal_util_priority_to_string (3);
	values[1] = e_cal_util_priority_to_string (5);
	values[2] = e_cal_util_priority_to_string (7);
	values[3] = C_("Priority", "Undefined");

	return task_table_build_combo_store (values, G_N_ELEMENTS (values));
}

static GtkCellRenderer *
task_table_create_special_renderer (gint field,
				    gpointer user_data,
				    gboolean *out_center_align)
{
	ECalTableTasks *self = user_data;
	GtkCellRenderer *renderer;
	GtkListStore *store;

	if (field == E_CAL_MODEL_FIELD_COMPLETE) {
		*out_center_align = TRUE;
		return e_cell_renderer_toggle_new ();
	}

	if (field == E_CAL_MODEL_FIELD_PERCENT ||
	    field == E_CAL_MODEL_FIELD_PRIORITY ||
	    field == E_CAL_MODEL_FIELD_STATUS) {
		if (field == E_CAL_MODEL_FIELD_PERCENT)
			store = task_table_build_percent_store ();
		else if (field == E_CAL_MODEL_FIELD_PRIORITY)
			store = task_table_build_priority_store ();
		else
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

	return NULL;
}

static void
task_table_on_column_created (gint field,
			      guint column_index,
			      gpointer user_data)
{
	ECalTableTasks *self = user_data;

	if (field == E_CAL_MODEL_FIELD_COMPLETE)
		self->complete_column_index = column_index;
}

static void
task_table_setup_columns (gpointer self)
{
	EVirtualTree *vtree = e_cal_table_list_base_get_virtual_tree (E_CAL_TABLE_LIST_BASE (self));

	e_cal_table_common_setup_columns (vtree, self, task_table_columns, N_TASK_TABLE_COLUMNS,
		task_table_column_data_func, task_table_create_special_renderer, self,
		task_table_on_column_created, self, E_CAL_MODEL_FIELD_SUMMARY);
}

static void
task_table_column_state_changed_cb (EVirtualTree *vtree,
				    gpointer user_data)
{
	ECalTableListBase *list_base = user_data;

	e_cal_table_common_column_state_changed (e_cal_table_list_base_get_virtual_tree (list_base),
		task_table_columns, N_TASK_TABLE_COLUMNS, e_cal_table_list_base_get_model (list_base));
}

const gchar * const *
e_cal_table_tasks_get_legacy_etable_column_ids (guint *out_n_column_ids)
{
	static const gchar *column_ids[N_TASK_TABLE_COLUMNS - 1];

	return e_cal_table_common_get_legacy_etable_column_ids (task_table_columns, N_TASK_TABLE_COLUMNS, column_ids, out_n_column_ids);
}

static void
task_table_row_activated_cb (EVirtualTree *vtree,
			     guint visible_row,
			     GObject *row_object,
			     gpointer user_data)
{
	ECalTableListBase *list_base = user_data;

	e_cal_table_list_base_emit_open_component (list_base, E_CAL_MODEL_COMPONENT (row_object));
}

static gboolean
task_table_right_click_cb (EVirtualTree *vtree,
			   guint visible_row,
			   GObject *row_object,
			   GdkEvent *event,
			   gpointer user_data)
{
	ECalTableListBase *list_base = user_data;

	e_cal_table_list_base_emit_popup_event (list_base, event);

	return TRUE;
}

static void
task_table_drag_begin_cb (EVirtualTree *vtree,
			  guint row,
			  GdkDragContext *context,
			  gpointer user_data)
{
	ECalTableTasks *self = user_data;
	ECalModel *model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));
	GObject *row_object;

	g_clear_object (&self->drag_comp_data);

	row_object = e_virtual_tree_model_dup_row (E_VIRTUAL_TREE_MODEL (model), row);
	if (row_object)
		self->drag_comp_data = E_CAL_MODEL_COMPONENT (row_object);
}

static void
task_table_drag_end_cb (EVirtualTree *vtree,
			GdkDragContext *context,
			gpointer user_data)
{
	ECalTableTasks *self = user_data;

	g_clear_object (&self->drag_comp_data);
}

static void
task_table_drag_data_get_cb (EVirtualTree *vtree,
			     GdkDragContext *context,
			     GtkSelectionData *selection_data,
			     guint info,
			     guint time,
			     gpointer user_data)
{
	ECalTableTasks *self = user_data;
	ECalModel *model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));
	GdkAtom target;
	const gchar *key;

	if (!self->drag_comp_data)
		return;

	target = gtk_selection_data_get_target (selection_data);
	key = e_virtual_tree_model_get_row_key (E_VIRTUAL_TREE_MODEL (model), G_OBJECT (self->drag_comp_data));

	gtk_selection_data_set (selection_data, target, 8, (const guchar *) key, (gint) strlen (key));
}

static gboolean
task_table_drag_motion_cb (EVirtualTree *vtree,
			   GdkDragContext *context,
			   guint row,
			   guint time,
			   gpointer user_data)
{
	ECalTableTasks *self = user_data;
	ECalModel *model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));
	GObject *target_row_object;
	gboolean can_reparent;

	if (!self->drag_comp_data)
		return FALSE;

	if (row == G_MAXUINT) {
		can_reparent = e_cal_model_can_reparent_component (model, self->drag_comp_data, NULL);
	} else {
		target_row_object = e_virtual_tree_model_dup_row (E_VIRTUAL_TREE_MODEL (model), row);
		can_reparent = target_row_object &&
			e_cal_model_can_reparent_component (model, self->drag_comp_data, E_CAL_MODEL_COMPONENT (target_row_object));
		g_clear_object (&target_row_object);
	}

	gdk_drag_status (context, can_reparent ? GDK_ACTION_MOVE : 0, time);

	return TRUE;
}

static gboolean
task_table_drag_drop_cb (EVirtualTree *vtree,
			 GdkDragContext *context,
			 guint row,
			 guint time,
			 gpointer user_data)
{
	ECalTableTasks *self = user_data;
	ECalModel *model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));
	GObject *target_row_object;
	ECalModelComponent *new_parent;
	gboolean handled = FALSE;

	if (!self->drag_comp_data)
		return FALSE;

	target_row_object = row == G_MAXUINT ? NULL : e_virtual_tree_model_dup_row (E_VIRTUAL_TREE_MODEL (model), row);
	new_parent = target_row_object ? E_CAL_MODEL_COMPONENT (target_row_object) : NULL;

	if (e_cal_model_can_reparent_component (model, self->drag_comp_data, new_parent)) {
		e_cal_model_reparent_component (model, self->drag_comp_data, new_parent);
		handled = TRUE;
	}

	g_clear_object (&target_row_object);
	gtk_drag_finish (context, handled, FALSE, time);

	return TRUE;
}

static void
task_table_queue_draw_cb (ECalModel *model,
			  GParamSpec *param,
			  gpointer user_data)
{
	ECalTableTasks *self = user_data;
	EVirtualTree *vtree = e_cal_table_list_base_get_virtual_tree (E_CAL_TABLE_LIST_BASE (self));

	gtk_widget_queue_draw (GTK_WIDGET (vtree));
}

static gboolean
check_for_retract (ECalComponent *comp,
                   ECalClient *client)
{
	ECalComponentOrganizer *org;
	gchar *email = NULL;
	const gchar *strip = NULL;
	gboolean ret_val;

	if (!e_cal_component_has_attendees (comp))
		return FALSE;

	if (!e_cal_client_check_save_schedules (client))
		return FALSE;

	org = e_cal_component_get_organizer (comp);
	strip = e_cal_util_get_organizer_email (org);

	if (!strip || !*strip) {
		e_cal_component_organizer_free (org);
		return FALSE;
	}

	ret_val = e_client_get_backend_property_sync (
		E_CLIENT (client),
		E_CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS,
		&email, NULL, NULL) && email != NULL &&
		e_cal_util_email_addresses_equal (email, strip);

	e_cal_component_organizer_free (org);
	g_free (email);

	return ret_val;
}

static void
add_retract_data (ECalComponent *comp,
                  const gchar *retract_comment)
{
	if (retract_comment && *retract_comment) {
		ECalComponentText *text;
		GSList lst = { NULL, NULL };

		text = e_cal_component_text_new (retract_comment, NULL);
		lst.data = text;

		e_cal_component_set_comments (comp, &lst);
		e_cal_component_text_free (text);
	} else {
		e_cal_component_set_comments (comp, NULL);
	}
}

static ECalComponentVType
task_table_get_delete_confirm_vtype (ECalTableListBase *list_base)
{
	return E_CAL_COMPONENT_TODO;
}

static gboolean
task_table_maybe_retract_selection (ECalTableListBase *list_base,
				    ECalComponent *comp,
				    ECalModelComponent *comp_data,
				    gboolean *out_should_delete)
{
	gchar *retract_comment = NULL;
	gboolean retract = FALSE;

	if (!check_for_retract (comp, comp_data->client))
		return FALSE;

	*out_should_delete = e_cal_dialogs_prompt_retract (GTK_WIDGET (list_base), comp, &retract_comment, &retract);

	if (retract) {
		ICalComponent *icomp;
		ECalModel *model = e_cal_table_list_base_get_model (list_base);

		add_retract_data (comp, retract_comment);
		icomp = e_cal_component_get_icalcomponent (comp);
		i_cal_component_set_method (icomp, I_CAL_METHOD_CANCEL);

		e_cal_ops_send_component (model, comp_data->client, icomp);
	}

	g_free (retract_comment);

	return TRUE;
}

static void
e_cal_table_tasks_dispose (GObject *object)
{
	ECalTableTasks *self = E_CAL_TABLE_TASKS (object);
	ECalModel *model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));

	g_clear_object (&self->drag_comp_data);

	if (self->completed_cancellable) {
		g_cancellable_cancel (self->completed_cancellable);
		g_clear_object (&self->completed_cancellable);
	}

	if (model != NULL) {
		e_signal_disconnect_notify_handler (model, &self->notify_highlight_due_today_id);
		e_signal_disconnect_notify_handler (model, &self->notify_color_due_today_id);
		e_signal_disconnect_notify_handler (model, &self->notify_highlight_overdue_id);
		e_signal_disconnect_notify_handler (model, &self->notify_color_overdue_id);
	}

	G_OBJECT_CLASS (e_cal_table_tasks_parent_class)->dispose (object);
}

static void
e_cal_table_tasks_class_init (ECalTableTasksClass *class)
{
	GObjectClass *object_class;
	ECalTableListBaseClass *list_base_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = e_cal_table_tasks_dispose;

	list_base_class = E_CAL_TABLE_LIST_BASE_CLASS (class);
	list_base_class->get_delete_confirm_vtype = task_table_get_delete_confirm_vtype;
	list_base_class->maybe_retract_selection = task_table_maybe_retract_selection;
}

static void
e_cal_table_tasks_init (ECalTableTasks *self)
{
}

GtkWidget *
e_cal_table_tasks_new (EShellView *shell_view,
		       ECalModel *model)
{
	ECalTableTasks *self;
	EVirtualTree *vtree;
	GtkTargetEntry dnd_target = { (gchar *) TASK_TABLE_DND_TARGET_ROW, 0, 0 };
	ECalTableListBaseSetup setup;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	self = g_object_new (E_TYPE_CAL_TABLE_TASKS, NULL);

	memset (&setup, 0, sizeof (ECalTableListBaseSetup));
	setup.accessible_name = _("Tasks");
	setup.cut_tooltip = _("Cut selected tasks to the clipboard");
	setup.copy_tooltip = _("Copy selected tasks to the clipboard");
	setup.paste_tooltip = _("Paste tasks from the clipboard");
	setup.delete_tooltip = _("Delete selected tasks");
	setup.select_all_tooltip = _("Select all visible tasks");
	setup.no_search_results_message = _("No task satisfies your search criteria. "
		"Change search criteria by selecting a new "
		"Show tasks filter from the drop down list "
		"above or by running a new search either by "
		"clearing it with Search->Clear menu item or "
		"by changing the query above.");
	setup.setup_columns_func = task_table_setup_columns;
	setup.column_state_changed_cb = G_CALLBACK (task_table_column_state_changed_cb);
	setup.row_activated_cb = G_CALLBACK (task_table_row_activated_cb);
	setup.right_click_cb = G_CALLBACK (task_table_right_click_cb);
	setup.cell_clicked_cb = G_CALLBACK (task_table_cell_clicked_cb);
	setup.cell_edited_cb = G_CALLBACK (task_table_cell_edited_cb);
	setup.get_legacy_etable_column_ids_func = e_cal_table_tasks_get_legacy_etable_column_ids;

	e_cal_table_list_base_construct (E_CAL_TABLE_LIST_BASE (self), shell_view, model, &setup);

	vtree = e_cal_table_list_base_get_virtual_tree (E_CAL_TABLE_LIST_BASE (self));

	self->notify_highlight_due_today_id = e_signal_connect_notify (
		model, "notify::highlight-due-today",
		G_CALLBACK (task_table_queue_draw_cb), self);
	self->notify_color_due_today_id = e_signal_connect_notify (
		model, "notify::color-due-today",
		G_CALLBACK (task_table_queue_draw_cb), self);
	self->notify_highlight_overdue_id = e_signal_connect_notify (
		model, "notify::highlight-overdue",
		G_CALLBACK (task_table_queue_draw_cb), self);
	self->notify_color_overdue_id = e_signal_connect_notify (
		model, "notify::color-overdue",
		G_CALLBACK (task_table_queue_draw_cb), self);

	e_cal_model_set_reparent_by_dnd (model, TRUE);

	e_virtual_tree_enable_drag_source (vtree, GDK_BUTTON1_MASK, &dnd_target, 1, GDK_ACTION_MOVE);
	e_virtual_tree_enable_drag_dest (vtree, &dnd_target, 1, GDK_ACTION_MOVE);

	g_signal_connect (vtree, "tree-drag-begin",
		G_CALLBACK (task_table_drag_begin_cb), self);
	g_signal_connect (vtree, "tree-drag-end",
		G_CALLBACK (task_table_drag_end_cb), self);
	g_signal_connect (vtree, "tree-drag-data-get",
		G_CALLBACK (task_table_drag_data_get_cb), self);
	g_signal_connect (vtree, "tree-drag-motion",
		G_CALLBACK (task_table_drag_motion_cb), self);
	g_signal_connect (vtree, "tree-drag-drop",
		G_CALLBACK (task_table_drag_drop_cb), self);

	return GTK_WIDGET (self);
}

static void
task_table_get_object_list_async (GList *clients_list,
                                  const gchar *sexp,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer callback_data)
{
	GList *l;

	for (l = clients_list; l != NULL; l = l->next) {
		ECalClient *client = l->data;

		e_cal_client_get_object_list (
			client, sexp, cancellable,
			callback, callback_data);
	}
}

static void
hide_completed_rows_ready (GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
	ECalModel *model = user_data;
	ECalClient *cal_client;
	GSList *m, *objects = NULL;
	GError *error = NULL;

	cal_client = E_CAL_CLIENT (source_object);

	if (!e_cal_client_get_object_list_finish (cal_client, result, &objects, &error))
		objects = NULL;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return;

	} else if (error != NULL) {
		ESource *source;

		source = e_client_get_source (E_CLIENT (source_object));

		g_warning (
			"%s: Could not get the objects from '%s': %s",
			G_STRFUNC,
			e_source_get_display_name (source),
			error->message);

		g_error_free (error);
		return;
	}

	for (m = objects; m; m = m->next) {
		ICalComponent *icalcomp = m->data;

		e_cal_model_remove_component (model, cal_client, i_cal_component_get_uid (icalcomp), NULL);
	}

	e_util_free_nullable_object_slist (objects);
}

static void
show_completed_rows_ready (GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
	ECalModel *model = user_data;
	ECalClient *cal_client;
	GSList *m, *objects = NULL;
	GError *error = NULL;

	cal_client = E_CAL_CLIENT (source_object);
	g_return_if_fail (cal_client != NULL);

	if (!e_cal_client_get_object_list_finish (cal_client, result, &objects, &error))
		objects = NULL;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return;

	} else if (error != NULL) {
		ESource *source;

		source = e_client_get_source (E_CLIENT (source_object));

		g_debug (
			"%s: Could not get the objects from '%s': %s",
			G_STRFUNC,
			e_source_get_display_name (source),
			error->message);

		g_error_free (error);
		return;
	}

	for (m = objects; m; m = m->next) {
		e_cal_model_add_component (model, cal_client, m->data);
	}

	e_util_free_nullable_object_slist (objects);
}

void
e_cal_table_tasks_process_completed_tasks (ECalTableTasks *self,
					   gboolean config_changed)
{
	ECalModel *model;
	ECalDataModel *data_model;
	GList *client_list;
	GCancellable *cancellable;
	gchar *hide_sexp, *show_sexp;

	g_return_if_fail (E_IS_CAL_TABLE_TASKS (self));

	model = e_cal_table_list_base_get_model (E_CAL_TABLE_LIST_BASE (self));

	if (self->completed_cancellable) {
		g_cancellable_cancel (self->completed_cancellable);
		g_object_unref (self->completed_cancellable);
	}

	self->completed_cancellable = g_cancellable_new ();
	cancellable = self->completed_cancellable;

	data_model = e_cal_model_get_data_model (model);
	hide_sexp = calendar_config_get_hide_completed_tasks_sexp (TRUE);
	show_sexp = calendar_config_get_hide_completed_tasks_sexp (FALSE);

	if (!(hide_sexp && show_sexp))
		show_sexp = g_strdup ("(is-completed?)");

	client_list = e_cal_data_model_get_clients (data_model);

	if (hide_sexp) {
		task_table_get_object_list_async (
			client_list, hide_sexp, cancellable,
			hide_completed_rows_ready, model);
	}

	if (config_changed) {
		task_table_get_object_list_async (
			client_list, show_sexp, cancellable,
			show_completed_rows_ready, model);
	}

	g_list_free_full (client_list, (GDestroyNotify) g_object_unref);

	g_free (hide_sexp);
	g_free (show_sexp);
}
