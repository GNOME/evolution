/*
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
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ETaskTable - displays the ECalComponent objects in a table (an ETable).
 */

#include "evolution-config.h"

#include "e-task-table.h"

#include <sys/stat.h>
#include <unistd.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-dialogs.h"
#include "e-cal-model-tasks.h"
#include "e-cal-ops.h"
#include "e-calendar-view.h"
#include "e-cell-date-edit-text.h"
#include "e-cell-estimated-duration.h"
#include "itip-utils.h"
#include "print.h"

struct _ETaskTablePrivate {
	gpointer shell_view;  /* weak pointer */
	ECalModel *model;
	GCancellable *completed_cancellable; /* when processing completed tasks */

	/* Fields used for cut/copy/paste */
	ICalComponent *tmp_vcal;

	GtkTargetList *copy_target_list;
	GtkTargetList *paste_target_list;

	gulong notify_highlight_due_today_id;
	gulong notify_color_due_today_id;
	gulong notify_highlight_overdue_id;
	gulong notify_color_overdue_id;
};

enum {
	PROP_0,
	PROP_COPY_TARGET_LIST,
	PROP_MODEL,
	PROP_PASTE_TARGET_LIST,
	PROP_SHELL_VIEW
};

enum {
	OPEN_COMPONENT,
	POPUP_EVENT,
	LAST_SIGNAL
};

static struct tm e_task_table_get_current_time (ECellDateEdit *ecde, gpointer data);

static guint signals[LAST_SIGNAL];

/* The icons to represent the task. */
static const gchar *icon_names[] = {
	"stock_task",
	"stock_task-recurring",
	"stock_task-assigned",
	"stock_task-assigned-to"
};

/* Forward Declarations */
static void	e_task_table_selectable_init
					(ESelectableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ETaskTable, e_task_table, E_TYPE_TABLE,
	G_ADD_PRIVATE (ETaskTable)
	G_IMPLEMENT_INTERFACE (E_TYPE_SELECTABLE, e_task_table_selectable_init))

static void
task_table_emit_open_component (ETaskTable *task_table,
                                ECalModelComponent *comp_data)
{
	guint signal_id = signals[OPEN_COMPONENT];

	g_signal_emit (task_table, signal_id, 0, comp_data);
}

static void
task_table_emit_popup_event (ETaskTable *task_table,
                             GdkEvent *event)
{
	g_signal_emit (task_table, signals[POPUP_EVENT], 0, event);
}

static gint
task_table_percent_compare_cb (gconstpointer a,
                               gconstpointer b,
                               gpointer cmp_cache)
{
	gint percent1 = GPOINTER_TO_INT (a);
	gint percent2 = GPOINTER_TO_INT (b);

	return (percent1 < percent2) ? -1 : (percent1 > percent2);
}

static gint
task_table_priority_compare_cb (gconstpointer a,
                                gconstpointer b,
                                gpointer cmp_cache)
{
	gint priority1, priority2;

	priority1 = e_cal_util_priority_from_string ((const gchar *) a);
	priority2 = e_cal_util_priority_from_string ((const gchar *) b);

	/* We change undefined priorities so they appear after 'Low'. */
	if (priority1 <= 0)
		priority1 = 10;
	if (priority2 <= 0)
		priority2 = 10;

	/* We'll just use the ordering of the priority values. */
	return (priority1 < priority2) ? -1 : (priority1 > priority2);
}

/* Deletes all of the selected components in the table */
static void
delete_selected_components (ETaskTable *task_table)
{
	GSList *objs;

	objs = e_task_table_get_selected (task_table);
	e_cal_ops_delete_ecalmodel_components (task_table->priv->model, objs);
	g_slist_free (objs);
}

static void
task_table_queue_draw_cb (ECalModelTasks *tasks_model,
                          GParamSpec *param,
                          GtkWidget *task_table)
{
	g_return_if_fail (task_table != NULL);

	gtk_widget_queue_draw (task_table);
}

static void
task_table_dates_cell_before_popup_cb (ECellDateEdit *dates_cell,
				       gint row,
				       gint view_col,
				       gpointer user_data)
{
	ECalModel *model;
	ECalModelComponent *comp_data;
	ETaskTable *task_table = user_data;
	ESelectionModel *esm;
	gboolean date_only;

	g_return_if_fail (E_IS_TASK_TABLE (task_table));

	esm = e_table_get_selection_model (E_TABLE (task_table));
	if (esm && esm->sorter && e_sorter_needs_sorting (esm->sorter))
		row = e_sorter_sorted_to_model (esm->sorter, row);

	model = e_task_table_get_model (task_table);
	comp_data = e_cal_model_get_component_at (model, row);
	date_only = comp_data && comp_data->client && e_client_check_capability (E_CLIENT (comp_data->client), E_CAL_STATIC_CAPABILITY_TASK_DATE_ONLY);

	g_object_set (G_OBJECT (dates_cell), "show-time", !date_only, NULL);
}

static void
task_table_set_model (ETaskTable *task_table,
                      ECalModel *model)
{
	g_return_if_fail (task_table->priv->model == NULL);

	task_table->priv->model = g_object_ref (model);

	/* redraw on drawing options change */
	task_table->priv->notify_highlight_due_today_id = e_signal_connect_notify (
		model, "notify::highlight-due-today",
		G_CALLBACK (task_table_queue_draw_cb),
		task_table);

	task_table->priv->notify_color_due_today_id = e_signal_connect_notify (
		model, "notify::color-due-today",
		G_CALLBACK (task_table_queue_draw_cb),
		task_table);

	task_table->priv->notify_highlight_overdue_id = e_signal_connect_notify (
		model, "notify::highlight-overdue",
		G_CALLBACK (task_table_queue_draw_cb),
		task_table);

	task_table->priv->notify_color_overdue_id = e_signal_connect_notify (
		model, "notify::color-overdue",
		G_CALLBACK (task_table_queue_draw_cb),
		task_table);
}

static void
task_table_set_shell_view (ETaskTable *task_table,
                           EShellView *shell_view)
{
	g_return_if_fail (task_table->priv->shell_view == NULL);

	task_table->priv->shell_view = shell_view;

	g_object_add_weak_pointer (
		G_OBJECT (shell_view),
		&task_table->priv->shell_view);
}

static void
task_table_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MODEL:
			task_table_set_model (
				E_TASK_TABLE (object),
				g_value_get_object (value));
			return;

		case PROP_SHELL_VIEW:
			task_table_set_shell_view (
				E_TASK_TABLE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_table_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COPY_TARGET_LIST:
			g_value_set_boxed (
				value, e_task_table_get_copy_target_list (
				E_TASK_TABLE (object)));
			return;

		case PROP_MODEL:
			g_value_set_object (
				value, e_task_table_get_model (
				E_TASK_TABLE (object)));
			return;

		case PROP_PASTE_TARGET_LIST:
			g_value_set_boxed (
				value, e_task_table_get_paste_target_list (
				E_TASK_TABLE (object)));
			return;

		case PROP_SHELL_VIEW:
			g_value_set_object (
				value, e_task_table_get_shell_view (
				E_TASK_TABLE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_table_dispose (GObject *object)
{
	ETaskTable *self = E_TASK_TABLE (object);

	if (self->priv->completed_cancellable) {
		g_cancellable_cancel (self->priv->completed_cancellable);
		g_clear_object (&self->priv->completed_cancellable);
	}

	if (self->priv->shell_view != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (self->priv->shell_view), &self->priv->shell_view);
		self->priv->shell_view = NULL;
	}

	if (self->priv->model != NULL) {
		g_signal_handlers_disconnect_by_data (self->priv->model, object);

		e_signal_disconnect_notify_handler (self->priv->model, &self->priv->notify_highlight_due_today_id);
		e_signal_disconnect_notify_handler (self->priv->model, &self->priv->notify_color_due_today_id);
		e_signal_disconnect_notify_handler (self->priv->model, &self->priv->notify_highlight_overdue_id);
		e_signal_disconnect_notify_handler (self->priv->model, &self->priv->notify_color_overdue_id);

		g_clear_object (&self->priv->model);
	}

	g_clear_pointer (&self->priv->copy_target_list, gtk_target_list_unref);
	g_clear_pointer (&self->priv->paste_target_list, gtk_target_list_unref);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_task_table_parent_class)->dispose (object);
}

static void
task_table_constructed (GObject *object)
{
	ETaskTable *task_table;
	ECalModel *model;
	ECell *cell, *popup_cell;
	ETableExtras *extras;
	ETableSpecification *specification;
	GList *strings;
	AtkObject *a11y;
	gchar *etspecfile;
	gint percent;
	GError *local_error = NULL;

	task_table = E_TASK_TABLE (object);
	model = e_task_table_get_model (task_table);

	/* Create the header columns */

	extras = e_table_extras_new ();

	/*
	 * Normal string fields.
	 */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		"bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		NULL);

	e_table_extras_add_cell (extras, "calstring", cell);
	g_object_unref (cell);

	/*
	 * Date fields.
	 */
	cell = e_cell_date_edit_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		"bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		NULL);

	e_binding_bind_property (
		model, "timezone",
		cell, "timezone",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		model, "use-24-hour-format",
		cell, "use-24-hour-format",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	popup_cell = e_cell_date_edit_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	e_binding_bind_property (
		model, "use-24-hour-format",
		popup_cell, "use-24-hour-format",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_table_extras_add_cell (extras, "dateedit", popup_cell);

	g_signal_connect (popup_cell, "before-popup",
		G_CALLBACK (task_table_dates_cell_before_popup_cb), task_table);

	g_object_unref (popup_cell);

	e_cell_date_edit_set_get_time_callback (
		E_CELL_DATE_EDIT (popup_cell),
		e_task_table_get_current_time, task_table, NULL);

	cell = e_cell_estimated_duration_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		"bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		NULL);
	e_table_extras_add_cell (extras, "estimatedduration", cell);
	g_object_unref (cell);

	/*
	 * Combo fields.
	 */

	/* Classification field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		"bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		"editable", FALSE,
		NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (gchar *) _("Public"));
	strings = g_list_append (strings, (gchar *) _("Private"));
	strings = g_list_append (strings, (gchar *) _("Confidential"));
	e_cell_combo_set_popdown_strings (
		E_CELL_COMBO (popup_cell),
		strings);
	g_list_free (strings);

	e_table_extras_add_cell (extras, "classification", popup_cell);
	g_object_unref (popup_cell);

	/* Priority field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		"bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		"editable", FALSE,
		NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (gchar *) _("High"));
	strings = g_list_append (strings, (gchar *) _("Normal"));
	strings = g_list_append (strings, (gchar *) _("Low"));
	strings = g_list_append (strings, (gchar *) _("Undefined"));
	e_cell_combo_set_popdown_strings (
		E_CELL_COMBO (popup_cell),
		strings);
	g_list_free (strings);

	e_table_extras_add_cell (extras, "priority", popup_cell);
	g_object_unref (popup_cell);

	/* Percent field. */
	cell = e_cell_percent_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		"bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_combo_use_tabular_numbers (E_CELL_COMBO (popup_cell));
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	for (percent = 0; percent <= 100; percent += 10) {
		/* Translators: "%d%%" is the percentage of a task done.
		 * %d is the actual value, %% is replaced with a percent sign.
		 * Result values will be 0%, 10%, 20%, ... 100%
		*/
		strings = g_list_append (strings, g_strdup_printf (_("%d%%"), percent));
	}
	e_cell_combo_set_popdown_strings (
		E_CELL_COMBO (popup_cell),
		strings);

	g_list_foreach (strings, (GFunc) g_free, NULL);
	g_list_free (strings);

	e_table_extras_add_cell (extras, "percent", popup_cell);
	g_object_unref (popup_cell);

	/* Transparency field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		"bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		"editable", FALSE,
		NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (gchar *) _("Free"));
	strings = g_list_append (strings, (gchar *) _("Busy"));
	e_cell_combo_set_popdown_strings (
		E_CELL_COMBO (popup_cell),
		strings);
	g_list_free (strings);

	e_table_extras_add_cell (extras, "transparency", popup_cell);
	g_object_unref (popup_cell);

	/* Status field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		"bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		"editable", FALSE,
		NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = cal_comp_util_get_status_list_for_kind (e_cal_model_get_component_kind (model));
	e_cell_combo_set_popdown_strings (
		E_CELL_COMBO (popup_cell),
		strings);
	g_list_free (strings);

	e_table_extras_add_cell (extras, "calstatus", popup_cell);
	g_object_unref (popup_cell);

	e_table_extras_add_compare (
		extras, "date-compare",
		e_cell_date_edit_compare_cb);
	e_table_extras_add_compare (
		extras, "percent-compare",
		task_table_percent_compare_cb);
	e_table_extras_add_compare (
		extras, "priority-compare",
		task_table_priority_compare_cb);
	e_table_extras_add_compare (
		extras, "status-compare",
		e_cal_model_util_status_compare_cb);

	/* Create pixmaps */

	cell = e_cell_toggle_new (icon_names, G_N_ELEMENTS (icon_names));
	g_object_set (cell,
		"bg-color-column", E_CAL_MODEL_FIELD_COLOR,
		NULL);
	e_table_extras_add_cell (extras, "icon", cell);
	g_object_unref (cell);

	e_table_extras_add_icon_name (extras, "icon", "stock_task");

	e_table_extras_add_icon_name (extras, "complete", "stock_check-filled-symbolic");

	/* Set background column for the checkbox */
	cell = e_table_extras_get_cell (extras, "checkbox");
	g_object_set (cell,
		"bg-color-column", E_CAL_MODEL_FIELD_COLOR,
		NULL);

	/* set proper format component for a default 'date' cell renderer */
	cell = e_table_extras_get_cell (extras, "date");
	e_cell_date_set_format_component (E_CELL_DATE (cell), "calendar");

	/* Create the table */

	etspecfile = g_build_filename (
		EVOLUTION_ETSPECDIR, "e-task-table.etspec", NULL);
	specification = e_table_specification_new (etspecfile, &local_error);

	/* Failure here is fatal. */
	if (local_error != NULL) {
		g_error ("%s: %s", etspecfile, local_error->message);
		g_return_if_reached ();
	}

	e_table_construct (
		E_TABLE (task_table),
		E_TABLE_MODEL (model),
		extras, specification);

	g_object_unref (specification);
	g_free (etspecfile);

	gtk_widget_set_has_tooltip (GTK_WIDGET (task_table), TRUE);

	g_object_unref (extras);

	a11y = gtk_widget_get_accessible (GTK_WIDGET (task_table));
	if (a11y)
		atk_object_set_name (a11y, _("Tasks"));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_task_table_parent_class)->constructed (object);
}

static gboolean
task_table_popup_menu (GtkWidget *widget)
{
	ETaskTable *task_table;

	task_table = E_TASK_TABLE (widget);
	task_table_emit_popup_event (task_table, NULL);

	return TRUE;
}

static gboolean
task_table_query_tooltip (GtkWidget *widget,
			  gint x,
			  gint y,
			  gboolean keyboard_mode,
			  GtkTooltip *tooltip)
{
	ETaskTable *task_table;
	ECalModel *model;
	ECalModelComponent *comp_data;
	gint row = -1, col = -1, row_y = -1, row_height = -1;
	gchar *markup;
	ECalComponent *new_comp;
	ESelectionModel *esm;

	if (keyboard_mode)
		return FALSE;

	task_table = E_TASK_TABLE (widget);

	e_table_get_mouse_over_cell (E_TABLE (task_table), &row, &col);
	if (row == -1)
		return FALSE;

	/* Respect sorting option; the 'e_table_get_mouse_over_cell'
	 * returns sorted row, not the model one. */
	esm = e_table_get_selection_model (E_TABLE (task_table));
	if (esm && esm->sorter && e_sorter_needs_sorting (esm->sorter))
		row = e_sorter_sorted_to_model (esm->sorter, row);

	model = e_task_table_get_model (task_table);
	comp_data = e_cal_model_get_component_at (model, row);

	if (!comp_data || !comp_data->icalcomp)
		return FALSE;

	new_comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (comp_data->icalcomp));
	if (!new_comp)
		return FALSE;

	markup = cal_comp_util_dup_tooltip (new_comp, comp_data->client,
		e_cal_model_get_registry (model),
		e_cal_model_get_timezone (model));

	gtk_tooltip_set_markup (tooltip, markup);

	g_free (markup);
	g_object_unref (new_comp);

	if (esm && esm->sorter && e_sorter_needs_sorting (esm->sorter))
		row = e_sorter_model_to_sorted (esm->sorter, row);

	e_table_get_cell_geometry (E_TABLE (task_table), row, 0, NULL, &row_y, NULL, &row_height);

	if (row_y != -1 && row_height != -1) {
		ETable *etable;
		GdkRectangle rect;
		GtkAllocation allocation;

		etable = E_TABLE (task_table);

		if (etable && etable->table_canvas) {
			gtk_widget_get_allocation (GTK_WIDGET (etable->table_canvas), &allocation);
		} else {
			allocation.x = 0;
			allocation.y = 0;
			allocation.width = 0;
			allocation.height = 0;
		}

		rect.x = allocation.x;
		rect.y = allocation.y + row_y - BUTTON_PADDING;
		rect.width = allocation.width;
		rect.height = row_height + 2 * BUTTON_PADDING;

		if (etable && etable->click_to_add && !etable->use_click_to_add_end) {
			gdouble spacing = 0.0;

			g_object_get (etable->canvas_vbox, "spacing", &spacing, NULL);

			rect.y += spacing + BUTTON_PADDING;
		}

		gtk_tooltip_set_tip_area (tooltip, &rect);
	}

	return TRUE;
}

static void
task_table_open_at_row (ETable *table,
			gint row)
{
	ETaskTable *task_table;
	ECalModel *model;
	ECalModelComponent *comp_data;

	task_table = E_TASK_TABLE (table);
	model = e_task_table_get_model (task_table);
	comp_data = e_cal_model_get_component_at (model, row);
	task_table_emit_open_component (task_table, comp_data);
}

static void
task_table_double_click (ETable *table,
                         gint row,
                         gint col,
                         GdkEvent *event)
{
	task_table_open_at_row (table, row);
}

static gint
task_table_right_click (ETable *table,
                        gint row,
                        gint col,
                        GdkEvent *event)
{
	ETaskTable *task_table;

	task_table = E_TASK_TABLE (table);
	task_table_emit_popup_event (task_table, event);

	return TRUE;
}

static gboolean
task_table_key_press (ETable *table,
		      gint row,
		      gint col,
		      GdkEvent *event)
{
	if (event && event->type == GDK_KEY_PRESS &&
	    (event->key.keyval == GDK_KEY_Return || event->key.keyval == GDK_KEY_KP_Enter) &&
	    (event->key.state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)) == 0 &&
	    !e_table_is_editing (table)) {
		task_table_open_at_row (table, row);
		return TRUE;
	}

	return FALSE;
}

static gboolean
task_table_white_space_event (ETable *table,
			      GdkEvent *event)
{
	guint event_button = 0;

	g_return_val_if_fail (E_IS_TASK_TABLE (table), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (event->type == GDK_BUTTON_PRESS &&
	    gdk_event_get_button (event, &event_button) &&
	    event_button == 3) {
		GtkWidget *table_canvas;

		table_canvas = GTK_WIDGET (table->table_canvas);

		if (!gtk_widget_has_focus (table_canvas))
			gtk_widget_grab_focus (table_canvas);

		task_table_emit_popup_event (E_TASK_TABLE (table), event);

		return TRUE;
	}

	return FALSE;
}

static void
task_table_update_actions (ESelectable *selectable,
                           EFocusTracker *focus_tracker,
                           GdkAtom *clipboard_targets,
                           gint n_clipboard_targets)
{
	ETaskTable *task_table;
	EUIAction *action;
	GtkTargetList *target_list;
	GSList *list, *iter;
	gboolean can_paste = FALSE;
	gboolean sources_are_editable = TRUE;
	gboolean is_editing;
	gboolean sensitive;
	const gchar *tooltip;
	gint n_selected;
	gint ii;

	task_table = E_TASK_TABLE (selectable);
	n_selected = e_table_selected_count (E_TABLE (task_table));
	is_editing = e_table_is_editing (E_TABLE (task_table));

	list = e_task_table_get_selected (task_table);
	for (iter = list; iter != NULL && sources_are_editable; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;

		if (!comp_data)
			continue;

		sources_are_editable = sources_are_editable &&
			!e_client_is_readonly (E_CLIENT (comp_data->client));
	}
	g_slist_free (list);

	target_list = e_selectable_get_paste_target_list (selectable);
	for (ii = 0; ii < n_clipboard_targets && !can_paste; ii++)
		can_paste = gtk_target_list_find (
			target_list, clipboard_targets[ii], NULL);

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0) && sources_are_editable && !is_editing;
	tooltip = _("Cut selected tasks to the clipboard");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0) && !is_editing;
	tooltip = _("Copy selected tasks to the clipboard");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	sensitive = sources_are_editable && can_paste && !is_editing;
	tooltip = _("Paste tasks from the clipboard");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	sensitive = (n_selected > 0) && sources_are_editable && !is_editing;
	tooltip = _("Delete selected tasks");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	sensitive = TRUE;
	tooltip = _("Select all visible tasks");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);
}

static void
task_table_cut_clipboard (ESelectable *selectable)
{
	ETaskTable *task_table;

	task_table = E_TASK_TABLE (selectable);

	e_selectable_copy_clipboard (selectable);
	delete_selected_components (task_table);
}

/* Helper for task_table_copy_clipboard() */
static void
copy_row_cb (gint model_row,
             gpointer data)
{
	ETaskTable *task_table;
	ECalModelComponent *comp_data;
	ECalModel *model;
	ICalComponent *child;

	task_table = E_TASK_TABLE (data);

	g_return_if_fail (task_table->priv->tmp_vcal != NULL);

	model = e_task_table_get_model (task_table);
	comp_data = e_cal_model_get_component_at (model, model_row);
	if (!comp_data)
		return;

	/* Add timezones to the VCALENDAR component. */
	e_cal_util_add_timezones_from_component (
		task_table->priv->tmp_vcal, comp_data->icalcomp);

	/* Add the new component to the VCALENDAR component. */
	child = i_cal_component_clone (comp_data->icalcomp);
	if (child) {
		i_cal_component_take_component (task_table->priv->tmp_vcal, child);
	}
}

static void
task_table_copy_clipboard (ESelectable *selectable)
{
	ETaskTable *task_table;
	GtkClipboard *clipboard;
	gchar *comp_str;

	task_table = E_TASK_TABLE (selectable);

	/* Create a temporary VCALENDAR object. */
	task_table->priv->tmp_vcal = e_cal_util_new_top_level ();

	e_table_selected_row_foreach (
		E_TABLE (task_table), copy_row_cb, task_table);
	comp_str = i_cal_component_as_ical_string (task_table->priv->tmp_vcal);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	e_clipboard_set_calendar (clipboard, comp_str, -1);
	gtk_clipboard_store (clipboard);

	g_free (comp_str);

	g_clear_object (&task_table->priv->tmp_vcal);
}

/* Helper for calenable_table_paste_clipboard() */
static void
clipboard_get_calendar_data (ETaskTable *task_table,
                             const gchar *text)
{
	g_return_if_fail (E_IS_TASK_TABLE (task_table));

	if (!text || !*text)
		return;

	e_cal_ops_paste_components (e_task_table_get_model (task_table), text);
}

static void
task_table_paste_clipboard (ESelectable *selectable)
{
	ETaskTable *task_table;
	GtkClipboard *clipboard;
	GnomeCanvasItem *item;
	GnomeCanvas *table_canvas;

	task_table = E_TASK_TABLE (selectable);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	table_canvas = E_TABLE (task_table)->table_canvas;
	item = table_canvas->focused_item;

	/* XXX Should ECellText implement GtkEditable? */

	/* Paste text into a cell being edited. */
	if (gtk_clipboard_wait_is_text_available (clipboard) &&
		gtk_widget_has_focus (GTK_WIDGET (table_canvas)) &&
		E_IS_TABLE_ITEM (item) &&
		E_TABLE_ITEM (item)->editing_col >= 0 &&
		E_TABLE_ITEM (item)->editing_row >= 0) {

		ETableItem *etable_item = E_TABLE_ITEM (item);

		e_cell_text_paste_clipboard (
			etable_item->cell_views[etable_item->editing_col],
			etable_item->editing_col,
			etable_item->editing_row);

	/* Paste iCalendar data into the table. */
	} else if (e_clipboard_wait_is_calendar_available (clipboard)) {
		gchar *calendar_source;

		calendar_source = e_clipboard_wait_for_calendar (clipboard);
		clipboard_get_calendar_data (task_table, calendar_source);
		g_free (calendar_source);
	}
}

/* Used from e_table_selected_row_foreach(); puts the selected row number in an
 * gint pointed to by the closure data.
 */
static void
get_selected_row_cb (gint model_row,
                     gpointer data)
{
	gint *row;

	row = data;
	*row = model_row;
}

/*
 * Returns the component that is selected in the table; only works if there is
 * one and only one selected row.
 */
static ECalModelComponent *
get_selected_comp (ETaskTable *task_table)
{
	ECalModel *model;
	gint row;

	model = e_task_table_get_model (task_table);
	if (e_table_selected_count (E_TABLE (task_table)) != 1)
		return NULL;

	row = -1;
	e_table_selected_row_foreach (
		E_TABLE (task_table), get_selected_row_cb, &row);
	if (row < 0) {
		g_warn_if_reached ();
		return NULL;
	}

	return e_cal_model_get_component_at (model, row);
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
task_table_delete_selection (ESelectable *selectable)
{
	ECalModel *model;
	ETaskTable *task_table;
	ECalModelComponent *comp_data;
	ECalComponent *comp = NULL;
	gboolean delete = TRUE;
	gint n_selected;

	task_table = E_TASK_TABLE (selectable);
	model = e_task_table_get_model (task_table);

	n_selected = e_table_selected_count (E_TABLE (task_table));
	if (n_selected <= 0)
		return;

	if (n_selected == 1)
		comp_data = get_selected_comp (task_table);
	else
		comp_data = NULL;

	/* FIXME: this may be something other than a TODO component */

	if (comp_data) {
		comp = e_cal_component_new_from_icalcomponent (
			i_cal_component_clone (comp_data->icalcomp));
	}

	if ((n_selected == 1) && comp && check_for_retract (comp, comp_data->client)) {
		gchar *retract_comment = NULL;
		gboolean retract = FALSE;

		delete = e_cal_dialogs_prompt_retract (GTK_WIDGET (task_table), comp, &retract_comment, &retract);
		if (retract) {
			ICalComponent *icomp;

			add_retract_data (comp, retract_comment);
			icomp = e_cal_component_get_icalcomponent (comp);
			i_cal_component_set_method (icomp, I_CAL_METHOD_CANCEL);

			e_cal_ops_send_component (model, comp_data->client, icomp);
		}

		g_free (retract_comment);
	} else if (e_cal_model_get_confirm_delete (model))
		delete = e_cal_dialogs_delete_component (
			comp, FALSE, n_selected,
			E_CAL_COMPONENT_TODO,
			GTK_WIDGET (task_table));

	if (delete)
		delete_selected_components (task_table);

	g_clear_object (&comp);
}

static void
task_table_select_all (ESelectable *selectable)
{
	e_table_select_all (E_TABLE (selectable));
}

static void
e_task_table_class_init (ETaskTableClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ETableClass *table_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = task_table_set_property;
	object_class->get_property = task_table_get_property;
	object_class->dispose = task_table_dispose;
	object_class->constructed = task_table_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->popup_menu = task_table_popup_menu;
	widget_class->query_tooltip = task_table_query_tooltip;

	table_class = E_TABLE_CLASS (class);
	table_class->double_click = task_table_double_click;
	table_class->right_click = task_table_right_click;
	table_class->key_press = task_table_key_press;
	table_class->white_space_event = task_table_white_space_event;

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_COPY_TARGET_LIST,
		"copy-target-list");

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			"Model",
			NULL,
			E_TYPE_CAL_MODEL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_PASTE_TARGET_LIST,
		"paste-target-list");

	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			"Shell View",
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[OPEN_COMPONENT] = g_signal_new (
		"open-component",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ETaskTableClass, open_component),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL_MODEL_COMPONENT);

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ETaskTableClass, popup_event),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
e_task_table_init (ETaskTable *task_table)
{
	GtkTargetList *target_list;

	task_table->priv = e_task_table_get_instance_private (task_table);

	task_table->priv->completed_cancellable = NULL;

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	task_table->priv->copy_target_list = target_list;

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	task_table->priv->paste_target_list = target_list;
}

static void
e_task_table_selectable_init (ESelectableInterface *iface)
{
	iface->update_actions = task_table_update_actions;
	iface->cut_clipboard = task_table_cut_clipboard;
	iface->copy_clipboard = task_table_copy_clipboard;
	iface->paste_clipboard = task_table_paste_clipboard;
	iface->delete_selection = task_table_delete_selection;
	iface->select_all = task_table_select_all;
}

/**
 * e_task_table_new:
 * @shell_view: an #EShellView
 * @model: an #ECalModel for the table
 *
 * Returns a new #ETaskTable.
 *
 * Returns: a new #ETaskTable
 **/
GtkWidget *
e_task_table_new (EShellView *shell_view,
                  ECalModel *model)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return g_object_new (
		E_TYPE_TASK_TABLE,
		"model", model, "shell-view", shell_view, NULL);
}

/**
 * e_task_table_get_model:
 * @task_table: A calendar table.
 *
 * Queries the calendar data model that a calendar table is using.
 *
 * Return value: A calendar model.
 **/
ECalModel *
e_task_table_get_model (ETaskTable *task_table)
{
	g_return_val_if_fail (E_IS_TASK_TABLE (task_table), NULL);

	return task_table->priv->model;
}

EShellView *
e_task_table_get_shell_view (ETaskTable *task_table)
{
	g_return_val_if_fail (E_IS_TASK_TABLE (task_table), NULL);

	return task_table->priv->shell_view;
}

struct get_selected_uids_closure {
	ETaskTable *task_table;
	GSList *objects;
};

static void
add_comp_data_cb (gint model_row,
		  gpointer data)
{
	struct get_selected_uids_closure *closure = data;
	ECalModelComponent *comp_data;
	ECalModel *model;

	model = e_task_table_get_model (closure->task_table);
	comp_data = e_cal_model_get_component_at (model, model_row);

	if (comp_data)
		closure->objects = g_slist_prepend (closure->objects, comp_data);
}

/**
 * e_task_table_get_selected:
 * @task_table:
 *
 * Get the currently selected ECalModelComponent's on the table.
 *
 * Return value: A GSList of the components, which should be
 * g_slist_free'd when finished with.
 **/
GSList *
e_task_table_get_selected (ETaskTable *task_table)
{
	struct get_selected_uids_closure closure;

	closure.task_table = task_table;
	closure.objects = NULL;

	e_table_selected_row_foreach (E_TABLE (task_table), add_comp_data_cb, &closure);

	return closure.objects;
}

GtkTargetList *
e_task_table_get_copy_target_list (ETaskTable *task_table)
{
	g_return_val_if_fail (E_IS_TASK_TABLE (task_table), NULL);

	return task_table->priv->copy_target_list;
}

GtkTargetList *
e_task_table_get_paste_target_list (ETaskTable *task_table)
{
	g_return_val_if_fail (E_IS_TASK_TABLE (task_table), NULL);

	return task_table->priv->paste_target_list;
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
	gboolean changed = FALSE;
	GPtrArray *comp_objects;
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

	comp_objects = e_cal_model_get_object_array (model);
	g_return_if_fail (comp_objects != NULL);

	for (m = objects; m; m = m->next) {
		ECalModelComponent *comp_data;
		ECalComponentId *id;
		ECalComponent *comp = e_cal_component_new ();

		e_cal_component_set_icalcomponent (
			comp, i_cal_component_clone (m->data));
		id = e_cal_component_get_id (comp);

		comp_data = e_cal_model_get_component_for_client_and_uid (model, cal_client, id);
		if (comp_data != NULL) {
			guint pos;

			if (g_ptr_array_find (comp_objects, comp_data, &pos)) {
				e_table_model_pre_change (E_TABLE_MODEL (model));
				g_ptr_array_remove_index (comp_objects, pos);
				g_object_unref (comp_data);
				e_table_model_row_deleted (E_TABLE_MODEL (model), pos);
				changed = TRUE;
			}
		}
		e_cal_component_id_free (id);
		g_object_unref (comp);
	}

	e_util_free_nullable_object_slist (objects);

	if (changed) {
		/* To notify about changes, because in call of
		 * row_deleted there are still all events. */
		e_table_model_changed (E_TABLE_MODEL (model));
	}
}

static void
show_completed_rows_ready (GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
	ECalClient *cal_client;
	ECalModel *model = user_data;
	GSList *m, *objects = NULL;
	GPtrArray *comp_objects;
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

	comp_objects = e_cal_model_get_object_array (model);
	g_return_if_fail (comp_objects != NULL);

	for (m = objects; m; m = m->next) {
		ECalModelComponent *comp_data;
		ECalComponentId *id;
		ECalComponent *comp = e_cal_component_new ();

		e_cal_component_set_icalcomponent (
			comp, i_cal_component_clone (m->data));
		id = e_cal_component_get_id (comp);

		if (!(e_cal_model_get_component_for_client_and_uid (model, cal_client, id))) {
			e_table_model_pre_change (E_TABLE_MODEL (model));
			comp_data = g_object_new (
				E_TYPE_CAL_MODEL_COMPONENT, NULL);
			comp_data->client = g_object_ref (cal_client);
			comp_data->icalcomp = i_cal_component_clone (m->data);
			e_cal_model_set_instance_times (
				comp_data,
				e_cal_model_get_timezone (model));
			comp_data->dtstart = NULL;
			comp_data->dtend = NULL;
			comp_data->due = NULL;
			comp_data->completed = NULL;
			comp_data->color = NULL;

			g_ptr_array_add (comp_objects, comp_data);
			e_table_model_row_inserted (
				E_TABLE_MODEL (model),
				comp_objects->len - 1);
		}
		e_cal_component_id_free (id);
		g_object_unref (comp);
	}

	e_util_free_nullable_object_slist (objects);
}

/* Returns the current time, for the ECellDateEdit items.
 * FIXME: Should probably use the timezone of the item rather than the
 * current timezone, though that may be difficult to get from here. */
static struct tm
e_task_table_get_current_time (ECellDateEdit *ecde,
                               gpointer data)
{
	ETaskTable *task_table = data;
	ECalModel *model;
	ICalTimezone *zone;
	ICalTime *tt;
	struct tm tmp_tm;

	/* Get the current timezone. */
	model = e_task_table_get_model (task_table);
	zone = e_cal_model_get_timezone (model);

	tt = i_cal_time_new_from_timet_with_zone (time (NULL), FALSE, zone);

	tmp_tm = e_cal_util_icaltime_to_tm (tt);

	g_clear_object (&tt);

	return tmp_tm;
}

/**
 * e_task_table_process_completed_tasks:
 * @table: A calendar table model.
 *
 * Process completed tasks.
 */
void
e_task_table_process_completed_tasks (ETaskTable *task_table,
                                      gboolean config_changed)
{
	ECalModel *model;
	ECalDataModel *data_model;
	GList *client_list;
	GCancellable *cancellable;
	gchar *hide_sexp, *show_sexp;

	if (task_table->priv->completed_cancellable) {
		g_cancellable_cancel (task_table->priv->completed_cancellable);
		g_object_unref (task_table->priv->completed_cancellable);
	}

	task_table->priv->completed_cancellable = g_cancellable_new ();
	cancellable = task_table->priv->completed_cancellable;

	model = e_task_table_get_model (task_table);
	data_model = e_cal_model_get_data_model (model);
	hide_sexp = calendar_config_get_hide_completed_tasks_sexp (TRUE);
	show_sexp = calendar_config_get_hide_completed_tasks_sexp (FALSE);

	/* If hide option is unchecked */
	if (!(hide_sexp && show_sexp))
		show_sexp = g_strdup ("(is-completed?)");

	client_list = e_cal_data_model_get_clients (data_model);

	/* Delete rows from model */
	if (hide_sexp) {
		task_table_get_object_list_async (
			client_list, hide_sexp, cancellable,
			hide_completed_rows_ready, model);
	}

	/* Insert rows into model */
	if (config_changed) {
		task_table_get_object_list_async (
			client_list, show_sexp, cancellable,
			show_completed_rows_ready, model);
	}

	g_list_free_full (client_list, (GDestroyNotify) g_object_unref);

	g_free (hide_sexp);
	g_free (show_sexp);
}
