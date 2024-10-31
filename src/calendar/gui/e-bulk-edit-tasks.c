/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <libecal/libecal.h>

#include "e-util/e-util.h"
#include "e-cal-model.h"
#include "e-comp-editor-property-parts.h"

#include "e-bulk-edit-tasks.h"

typedef struct _CompRef {
	ECalClient *client;
	ICalComponent *icomp;
} CompRef;

typedef enum {
	EDIT_ITEM_DTSTART = 0,
	EDIT_ITEM_DUE,
	EDIT_ITEM_COMPLETED,
	EDIT_ITEM_STATUS,
	EDIT_ITEM_PRIORITY,
	EDIT_ITEM_PERCENTCOMPLETE,
	EDIT_ITEM_CLASSIFICATION,
	EDIT_ITEM_ESTIMATED_DURATION,
	EDIT_ITEM_TIMEZONE,
	N_EDIT_ITEMS
} EditItems;

typedef struct _EditItem {
	GtkToggleButton *check;
	ECompEditorPropertyPart *part;
} EditItem;

struct _EBulkEditTasksPrivate {
	GtkWidget *items_grid;
	GtkWidget *alert_bar;
	GtkWidget *activity_bar;

	GPtrArray *comps; /* CompRef * */
	EditItem items[N_EDIT_ITEMS];
	ECategoriesSelector *categories;
	GCancellable *cancellable;

	gint last_duration;
	gboolean updating;
	gboolean dtstart_is_unset;
	gboolean due_is_unset;
};

static void e_bulk_edit_tasks_alert_sink_init (EAlertSinkInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EBulkEditTasks, e_bulk_edit_tasks, GTK_TYPE_DIALOG,
	G_ADD_PRIVATE (EBulkEditTasks)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_bulk_edit_tasks_alert_sink_init))

static CompRef *
comp_ref_new (ECalClient *client,
	      ICalComponent *icomp)
{
	CompRef *cr;

	cr = g_new0 (CompRef, 1);
	cr->client = g_object_ref (client);
	cr->icomp = g_object_ref (icomp);

	return cr;
}

static void
comp_ref_free (gpointer ptr)
{
	CompRef *cr = ptr;

	if (cr) {
		g_clear_object (&cr->client);
		g_clear_object (&cr->icomp);
		g_free (cr);
	}
}

static GPtrArray * /* CompRef * */
e_bulk_edit_tasks_apply_changes_gui (EBulkEditTasks *self)
{
	GHashTable *categories_checked = NULL, *categories_unchecked = NULL;
	GPtrArray *comps;
	guint ii, jj;

	e_categories_selector_get_changes (self->priv->categories, &categories_checked, &categories_unchecked);

	comps = g_ptr_array_new_full (self->priv->comps->len, comp_ref_free);

	for (jj = 0; jj < self->priv->comps->len; jj++) {
		CompRef *src_cr = g_ptr_array_index (self->priv->comps, jj);
		CompRef *des_cr;

		des_cr = comp_ref_new (src_cr->client, i_cal_component_clone (src_cr->icomp));
		/* because comp_ref_new() adds a reference on the icomp */
		g_object_unref (des_cr->icomp);

		for (ii = 0; ii < N_EDIT_ITEMS; ii++) {
			if (!gtk_toggle_button_get_active (self->priv->items[ii].check))
				continue;

			e_comp_editor_property_part_fill_component (self->priv->items[ii].part, des_cr->icomp);
		}

		if (categories_checked || categories_unchecked) {
			ICalProperty *prop;
			const gchar *old_value = NULL;
			gchar *new_value;

			prop = i_cal_component_get_first_property (des_cr->icomp, I_CAL_CATEGORIES_PROPERTY);
			if (prop)
				old_value = i_cal_property_get_categories (prop);

			new_value = e_categories_selector_util_apply_changes (old_value, categories_checked, categories_unchecked);

			if (g_strcmp0 (old_value, new_value) != 0) {
				if (new_value && *new_value) {
					if (prop) {
						i_cal_property_set_categories (prop, new_value);
					} else {
						prop = i_cal_property_new_categories (new_value);
						i_cal_component_add_property (des_cr->icomp, prop);
					}
				} else if (prop) {
					i_cal_component_remove_property (des_cr->icomp, prop);
				}
			}

			g_clear_object (&prop);
			g_free (new_value);
		}

		g_ptr_array_add (comps, des_cr);
	}

	g_clear_pointer (&categories_checked, g_hash_table_destroy);
	g_clear_pointer (&categories_unchecked, g_hash_table_destroy);

	return comps;
}

typedef struct _SaveChangesData {
	EBulkEditTasks *self;
	GPtrArray *comps; /* CompRef * */
	time_t completed_time;
	gboolean write_completed;
	gboolean success;
} SaveChangesData;

static void
e_bulk_edit_tasks_save_changes_thread (EAlertSinkThreadJobData *job_data,
				       gpointer user_data,
				       GCancellable *cancellable,
				       GError **error)
{
	SaveChangesData *scd = user_data;
	guint ii;

	for (ii = 0; ii < scd->comps->len && scd->success && !g_cancellable_is_cancelled (cancellable); ii++) {
		CompRef *cr = g_ptr_array_index (scd->comps, ii);

		if (scd->write_completed)
			e_cal_util_mark_task_complete_sync (cr->icomp, scd->completed_time, cr->client, cancellable, NULL);

		scd->success = e_cal_client_modify_object_sync (cr->client, cr->icomp, E_CAL_OBJ_MOD_ALL, E_CAL_OPERATION_FLAG_NONE, cancellable, error);
	}

	scd->success = scd->success && !g_cancellable_set_error_if_cancelled (cancellable, error);
}

static void
e_bulk_edit_tasks_save_changes_done_cb (gpointer ptr)
{
	SaveChangesData *scd = ptr;

	if (scd->self->priv->items_grid) {
		gtk_widget_set_sensitive (scd->self->priv->items_grid, TRUE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (scd->self), GTK_RESPONSE_APPLY, TRUE);

		if (scd->success)
			gtk_widget_destroy (GTK_WIDGET (scd->self));
	}

	g_clear_object (&scd->self->priv->cancellable);
	g_ptr_array_unref (scd->comps);
	g_object_unref (scd->self);
	g_free (scd);
}

static void
e_bulk_edit_tasks_response_cb (GtkDialog *dialog,
			       gint response_id,
			       gpointer user_data)
{
	EBulkEditTasks *self = E_BULK_EDIT_TASKS (dialog);

	g_cancellable_cancel (self->priv->cancellable);
	g_clear_object (&self->priv->cancellable);

	if (response_id == GTK_RESPONSE_APPLY) {
		SaveChangesData *scd;
		EActivity *activity;

		e_alert_bar_clear (E_ALERT_BAR (self->priv->alert_bar));

		scd = g_new0 (SaveChangesData, 1);
		scd->self = g_object_ref (self);
		scd->comps = e_bulk_edit_tasks_apply_changes_gui (self);
		scd->completed_time = (time_t) -1;
		scd->write_completed = FALSE;
		scd->success = TRUE;

		if (gtk_toggle_button_get_active (self->priv->items[EDIT_ITEM_STATUS].check)) {
			if (e_comp_editor_property_part_picker_with_map_get_selected (
				E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (self->priv->items[EDIT_ITEM_STATUS].part)) == I_CAL_STATUS_COMPLETED)
				scd->write_completed = TRUE;
		}

		if (!scd->write_completed && gtk_toggle_button_get_active (self->priv->items[EDIT_ITEM_PERCENTCOMPLETE].check)) {
			GtkSpinButton *percent_spin = GTK_SPIN_BUTTON (e_comp_editor_property_part_get_edit_widget (self->priv->items[EDIT_ITEM_PERCENTCOMPLETE].part));

			scd->write_completed = gtk_spin_button_get_value_as_int (percent_spin) >= 100;
		}

		if (scd->write_completed || gtk_toggle_button_get_active (self->priv->items[EDIT_ITEM_COMPLETED].check)) {
			GtkWidget *widget;

			widget = e_comp_editor_property_part_get_edit_widget (self->priv->items[EDIT_ITEM_COMPLETED].part);

			if (E_IS_DATE_EDIT (widget)) {
				scd->completed_time = e_date_edit_get_time (E_DATE_EDIT (widget));
				scd->write_completed = scd->completed_time != (time_t) -1;
			}
		}

		activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (self),
			_("Saving changes, please wait…"),
			"system:generic-error",
			_("Failed to save changes"),
			e_bulk_edit_tasks_save_changes_thread, scd,
			e_bulk_edit_tasks_save_changes_done_cb);

		if (activity) {
			self->priv->cancellable = e_activity_get_cancellable (activity);

			if (self->priv->cancellable)
				g_object_ref (self->priv->cancellable);

			e_activity_bar_set_activity (E_ACTIVITY_BAR (self->priv->activity_bar), activity);

			g_object_unref (activity);

			gtk_widget_set_sensitive (self->priv->items_grid, FALSE);
			gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_APPLY, FALSE);
		}
	} else {
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}

static void
e_bulk_edit_tasks_dtstart_changed_cb (EDateEdit *date_edit,
				      EBulkEditTasks *self)
{
	CompRef *cr;
	gboolean was_unset;

	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_BULK_EDIT_TASKS (self));

	was_unset = self->priv->dtstart_is_unset;
	self->priv->dtstart_is_unset = e_date_edit_get_time (date_edit) == (time_t) -1;

	if (self->priv->updating)
		return;

	self->priv->updating = TRUE;

	cr = g_ptr_array_index (self->priv->comps, 0);

	e_comp_editor_property_part_util_ensure_start_before_end (cr->icomp,
		self->priv->items[EDIT_ITEM_DTSTART].part, self->priv->items[EDIT_ITEM_DUE].part,
		TRUE, &self->priv->last_duration);

	/* When setting DTSTART for the first time, derive the type from the DUE,
	   otherwise the DUE has changed the type to the DATE only. */
	if (was_unset) {
		e_comp_editor_property_part_util_ensure_same_value_type (
			self->priv->items[EDIT_ITEM_DUE].part, self->priv->items[EDIT_ITEM_DTSTART].part);
	} else {
		e_comp_editor_property_part_util_ensure_same_value_type (
			self->priv->items[EDIT_ITEM_DTSTART].part, self->priv->items[EDIT_ITEM_DUE].part);
	}

	self->priv->updating = FALSE;
}

static void
e_bulk_edit_tasks_due_changed_cb (EDateEdit *date_edit,
				  EBulkEditTasks *self)
{
	CompRef *cr;
	gboolean was_unset;

	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_BULK_EDIT_TASKS (self));

	was_unset = self->priv->due_is_unset;
	self->priv->due_is_unset = e_date_edit_get_time (date_edit) == (time_t) -1;

	if (self->priv->updating || !self->priv->comps->len)
		return;

	self->priv->updating = TRUE;

	cr = g_ptr_array_index (self->priv->comps, 0);

	e_comp_editor_property_part_util_ensure_start_before_end (cr->icomp,
		self->priv->items[EDIT_ITEM_DTSTART].part, self->priv->items[EDIT_ITEM_DUE].part,
		FALSE, &self->priv->last_duration);

	/* When setting DUE for the first time, derive the type from the DTSTART,
	   otherwise the DTSTART has changed the type to the DATE only. */
	if (was_unset) {
		e_comp_editor_property_part_util_ensure_same_value_type (
			self->priv->items[EDIT_ITEM_DTSTART].part, self->priv->items[EDIT_ITEM_DUE].part);
	} else {
		e_comp_editor_property_part_util_ensure_same_value_type (
			self->priv->items[EDIT_ITEM_DUE].part, self->priv->items[EDIT_ITEM_DTSTART].part);
	}

	self->priv->updating = FALSE;
}

static void
e_bulk_edit_tasks_completed_changed_cb (EDateEdit *date_edit,
					EBulkEditTasks *self)
{
	GtkSpinButton *percent_spin;
	ICalTime *itt;
	gint status;

	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_BULK_EDIT_TASKS (self));

	if (self->priv->updating)
		return;

	self->priv->updating = TRUE;

	status = e_comp_editor_property_part_picker_with_map_get_selected (
		E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (self->priv->items[EDIT_ITEM_STATUS].part));
	itt = e_comp_editor_property_part_datetime_get_value (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (self->priv->items[EDIT_ITEM_COMPLETED].part));
	percent_spin = GTK_SPIN_BUTTON (e_comp_editor_property_part_get_edit_widget (self->priv->items[EDIT_ITEM_PERCENTCOMPLETE].part));

	if (!itt || i_cal_time_is_null_time (itt)) {
		if (status == I_CAL_STATUS_COMPLETED) {
			e_comp_editor_property_part_picker_with_map_set_selected (
				E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (self->priv->items[EDIT_ITEM_STATUS].part),
				I_CAL_STATUS_NONE);

			gtk_spin_button_set_value (percent_spin, 0);
		}
	} else {
		if (status != I_CAL_STATUS_COMPLETED) {
			e_comp_editor_property_part_picker_with_map_set_selected (
				E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (self->priv->items[EDIT_ITEM_STATUS].part),
				I_CAL_STATUS_COMPLETED);
		}

		gtk_spin_button_set_value (percent_spin, 100);
	}

	self->priv->updating = FALSE;

	g_clear_object (&itt);
}

static void
e_bulk_edit_tasks_status_changed_cb (GtkComboBox *combo_box,
				     EBulkEditTasks *self)
{
	GtkSpinButton *percent_spin;
	EDateEdit *completed_date;
	gint status;

	g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
	g_return_if_fail (E_IS_BULK_EDIT_TASKS (self));

	if (self->priv->updating)
		return;

	self->priv->updating = TRUE;

	percent_spin = GTK_SPIN_BUTTON (e_comp_editor_property_part_get_edit_widget (self->priv->items[EDIT_ITEM_PERCENTCOMPLETE].part));
	completed_date = E_DATE_EDIT (e_comp_editor_property_part_get_edit_widget (self->priv->items[EDIT_ITEM_COMPLETED].part));
	status = e_comp_editor_property_part_picker_with_map_get_selected (
		E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (self->priv->items[EDIT_ITEM_STATUS].part));

	if (status == I_CAL_STATUS_NONE) {
		gtk_spin_button_set_value (percent_spin, 0);
		e_date_edit_set_time (completed_date, (time_t) -1);
	} else if (status == I_CAL_STATUS_INPROCESS) {
		gint percent_complete = gtk_spin_button_get_value_as_int (percent_spin);

		if (percent_complete <= 0 || percent_complete >= 100)
			gtk_spin_button_set_value (percent_spin, 50);

		e_date_edit_set_time (completed_date, (time_t) -1);
	} else if (status == I_CAL_STATUS_COMPLETED) {
		gtk_spin_button_set_value (percent_spin, 100);
		e_date_edit_set_time (completed_date, time (NULL));
	}

	self->priv->updating = FALSE;
}

static void
e_bulk_edit_tasks_percentcomplete_value_changed_cb (GtkSpinButton *spin_button,
						    EBulkEditTasks *self)
{
	GtkSpinButton *percent_spin;
	EDateEdit *completed_date;
	gint status, percent;
	time_t ctime;

	g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
	g_return_if_fail (E_IS_BULK_EDIT_TASKS (self));

	if (self->priv->updating)
		return;

	self->priv->updating = TRUE;

	percent_spin = GTK_SPIN_BUTTON (e_comp_editor_property_part_get_edit_widget (self->priv->items[EDIT_ITEM_PERCENTCOMPLETE].part));
	completed_date = E_DATE_EDIT (e_comp_editor_property_part_get_edit_widget (self->priv->items[EDIT_ITEM_COMPLETED].part));

	percent = gtk_spin_button_get_value_as_int (percent_spin);
	if (percent == 100) {
		ctime = time (NULL);
		status = I_CAL_STATUS_COMPLETED;
	} else {
		ctime = (time_t) -1;

		if (percent == 0)
			status = I_CAL_STATUS_NONE;
		else
			status = I_CAL_STATUS_INPROCESS;
	}

	e_comp_editor_property_part_picker_with_map_set_selected (
		E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (self->priv->items[EDIT_ITEM_STATUS].part), status);
	e_date_edit_set_time (completed_date, ctime);

	self->priv->updating = FALSE;
}

static ICalTimezone *
e_bulk_edit_tasks_lookup_timezone (EBulkEditTasks *self,
				   const gchar *tzid)
{
	ICalTimezone *zone;

	g_return_val_if_fail (E_IS_BULK_EDIT_TASKS (self), NULL);

	if (!tzid || !*tzid)
		return NULL;

	zone = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);

	if (!zone)
		zone = i_cal_timezone_get_builtin_timezone (tzid);

	if (!zone) {
		GHashTable *checked_clients;
		guint ii;

		checked_clients = g_hash_table_new (NULL, NULL);

		for (ii = 0; ii < self->priv->comps->len && !zone; ii++) {
			CompRef *cr = g_ptr_array_index (self->priv->comps, ii);

			if (g_hash_table_contains (checked_clients, cr->client))
				continue;

			g_hash_table_add (checked_clients, cr->client);

			if (!e_cal_client_get_timezone_sync (cr->client, tzid, &zone, NULL, NULL))
				zone = NULL;
		}

		g_hash_table_destroy (checked_clients);
	}

	return zone;
}

static gboolean
e_bulk_edit_tasks_dates_to_timezone_cb (GBinding *binding,
					const GValue *from_value,
					GValue *to_value,
					gpointer user_data)
{
	EBulkEditTasks *self = user_data;

	g_value_set_boolean (to_value,
		gtk_toggle_button_get_active (self->priv->items[EDIT_ITEM_DTSTART].check) ||
		gtk_toggle_button_get_active (self->priv->items[EDIT_ITEM_DUE].check) ||
		gtk_toggle_button_get_active (self->priv->items[EDIT_ITEM_COMPLETED].check));

	return TRUE;
}

static void
e_bulk_edit_tasks_set_edit_item (EBulkEditTasks *self,
				 EditItems item,
				 ECompEditorPropertyPart *part,
				 GtkGrid *grid,
				 gint left,
				 gint top,
				 gboolean full_width)
{
	GtkWidget *label_widget, *edit_widget, *check;

	g_return_if_fail (item < N_EDIT_ITEMS);

	label_widget = e_comp_editor_property_part_get_label_widget (part);
	edit_widget = e_comp_editor_property_part_get_edit_widget (part);

	if (GTK_IS_LABEL (label_widget)) {
		check = gtk_check_button_new_with_mnemonic (gtk_label_get_label (GTK_LABEL (label_widget)));
		gtk_widget_show (check);

		gtk_grid_attach (grid, check, left, top, 1, 1);
		gtk_grid_attach (grid, label_widget, left, top, 1, 1);
		gtk_widget_hide (label_widget);
	} else {
		GtkWidget *hbox;

		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
		gtk_widget_show (hbox);

		gtk_grid_attach (grid, hbox, left, top, 1, 1);

		check = gtk_check_button_new ();
		gtk_widget_show (check);

		gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), label_widget, FALSE, FALSE, 0);

		gtk_widget_show (label_widget);

		e_binding_bind_property (check, "active",
			label_widget, "sensitive",
			G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
	}

	gtk_grid_attach (grid, edit_widget, left + 1, top, 1 + (full_width ? 2 : 0), 1);
	gtk_widget_show (edit_widget);

	e_binding_bind_property (check, "active",
		edit_widget, "sensitive",
		G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	self->priv->items[item].check = GTK_TOGGLE_BUTTON (check);
	self->priv->items[item].part = part;
}

static void
e_bulk_edit_tasks_fill_content (EBulkEditTasks *self)
{
	GtkWidget *widget;
	GtkGrid *grid;
	GtkNotebook *notebook;
	GtkWidget *scrolled_window;
	ICalComponent *first_icomp = NULL;
	ECompEditorPropertyPart *part;
	gchar *str;
	gboolean date_only = FALSE;
	guint ii;

	for (ii = 0; !date_only && ii < self->priv->comps->len; ii++) {
		CompRef *cr = g_ptr_array_index (self->priv->comps, ii);

		if (!first_icomp)
			first_icomp = cr->icomp;

		date_only = !cr->client || e_client_check_capability (E_CLIENT (cr->client), E_CAL_STATIC_CAPABILITY_TASK_DATE_ONLY);
	}

	self->priv->items_grid = gtk_grid_new ();
	grid = GTK_GRID (self->priv->items_grid);

	g_object_set (grid,
		"margin", 12,
		"column-spacing", 4,
		"row-spacing", 4,
		NULL);

	str = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
		"Modify a task",
		"Modify %u tasks", self->priv->comps->len), self->priv->comps->len);
	gtk_window_set_title (GTK_WINDOW (self), str);
	g_free (str);

	widget = gtk_label_new (_("Select values to be modified."));
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_CENTER,
		"margin-bottom", 4,
		"visible", TRUE,
		"xalign", 0.0,
		"yalign", 0.5,
		"wrap", TRUE,
		"width-chars", 80,
		"max-width-chars", 100,
		NULL);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	widget = gtk_notebook_new ();
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"vexpand", TRUE,
		"visible", TRUE,
		NULL);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);
	notebook = GTK_NOTEBOOK (widget);

	widget = gtk_grid_new ();
	g_object_set (widget,
		"visible", TRUE,
		"margin", 12,
		"column-spacing", 4,
		"row-spacing", 4,
		NULL);

	gtk_notebook_append_page (notebook, widget, gtk_label_new_with_mnemonic (_("_General")));

	grid = GTK_GRID (widget);

	part = e_comp_editor_property_part_dtstart_new (C_("ECompEditor", "Sta_rt date:"), date_only, TRUE, FALSE);
	e_bulk_edit_tasks_set_edit_item (self, EDIT_ITEM_DTSTART, part, grid, 0, 0, FALSE);
	g_signal_connect (e_comp_editor_property_part_get_edit_widget (part), "changed",
		G_CALLBACK (e_bulk_edit_tasks_dtstart_changed_cb), self);

	part = e_comp_editor_property_part_due_new (date_only, TRUE);
	e_bulk_edit_tasks_set_edit_item (self, EDIT_ITEM_DUE, part, grid, 0, 1, FALSE);
	g_signal_connect (e_comp_editor_property_part_get_edit_widget (part), "changed",
		G_CALLBACK (e_bulk_edit_tasks_due_changed_cb), self);

	part = e_comp_editor_property_part_completed_new (date_only, TRUE);
	e_bulk_edit_tasks_set_edit_item (self, EDIT_ITEM_COMPLETED, part, grid, 0, 2, FALSE);
	g_signal_connect (e_comp_editor_property_part_get_edit_widget (part), "changed",
		G_CALLBACK (e_bulk_edit_tasks_completed_changed_cb), self);

	part = e_comp_editor_property_part_estimated_duration_new ();
	e_bulk_edit_tasks_set_edit_item (self, EDIT_ITEM_ESTIMATED_DURATION, part, grid, 0, 3, FALSE);

	part = e_comp_editor_property_part_status_new (I_CAL_VTODO_COMPONENT);
	e_bulk_edit_tasks_set_edit_item (self, EDIT_ITEM_STATUS, part, grid, 2, 0, FALSE);
	g_signal_connect (e_comp_editor_property_part_get_edit_widget (part), "changed",
		G_CALLBACK (e_bulk_edit_tasks_status_changed_cb), self);

	part = e_comp_editor_property_part_priority_new ();
	e_bulk_edit_tasks_set_edit_item (self, EDIT_ITEM_PRIORITY, part, grid, 2, 1, FALSE);

	part = e_comp_editor_property_part_percentcomplete_new ();
	e_bulk_edit_tasks_set_edit_item (self, EDIT_ITEM_PERCENTCOMPLETE, part, grid, 2, 2, FALSE);
	g_signal_connect (e_comp_editor_property_part_get_edit_widget (part), "value-changed",
		G_CALLBACK (e_bulk_edit_tasks_percentcomplete_value_changed_cb), self);

	part = e_comp_editor_property_part_classification_new ();
	e_bulk_edit_tasks_set_edit_item (self, EDIT_ITEM_CLASSIFICATION, part, grid, 2, 3, FALSE);

	part = e_comp_editor_property_part_timezone_new ();
	e_bulk_edit_tasks_set_edit_item (self, EDIT_ITEM_TIMEZONE, part, grid, 0, 4, TRUE);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (scrolled_window,
		"visible", TRUE,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"can-focus", FALSE,
		"shadow-type", GTK_SHADOW_NONE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"propagate-natural-width", FALSE,
		"propagate-natural-height", FALSE,
		NULL);

	gtk_notebook_append_page (notebook, scrolled_window, gtk_label_new_with_mnemonic (_("_Categories")));

	widget = e_categories_selector_new ();
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"vexpand", TRUE,
		"use-inconsistent", TRUE,
		NULL);

	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

	self->priv->categories = E_CATEGORIES_SELECTOR (widget);

	gtk_widget_show (self->priv->items_grid);

	self->priv->alert_bar = e_alert_bar_new ();
	gtk_widget_set_margin_bottom (self->priv->alert_bar, 6);

	self->priv->activity_bar = e_activity_bar_new ();
	gtk_widget_set_margin_bottom (self->priv->activity_bar, 6);

	widget = gtk_dialog_get_content_area (GTK_DIALOG (self));
	gtk_box_pack_start (GTK_BOX (widget), self->priv->items_grid, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (widget), self->priv->alert_bar, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (widget), self->priv->activity_bar, FALSE, FALSE, 0);

	gtk_dialog_add_buttons (GTK_DIALOG (self),
		_("M_odify"), GTK_RESPONSE_APPLY,
		_("Ca_ncel"), GTK_RESPONSE_CANCEL,
		NULL);

	g_signal_connect (self, "response",
		G_CALLBACK (e_bulk_edit_tasks_response_cb), NULL);

	widget = e_comp_editor_property_part_get_edit_widget (self->priv->items[EDIT_ITEM_TIMEZONE].part);
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (self->priv->items[EDIT_ITEM_DTSTART].part),
		E_TIMEZONE_ENTRY (widget));
	g_signal_connect_swapped (self->priv->items[EDIT_ITEM_DTSTART].part, "lookup-timezone",
		G_CALLBACK (e_bulk_edit_tasks_lookup_timezone), self);
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (self->priv->items[EDIT_ITEM_DUE].part),
		E_TIMEZONE_ENTRY (widget));
	g_signal_connect_swapped (self->priv->items[EDIT_ITEM_DUE].part, "lookup-timezone",
		G_CALLBACK (e_bulk_edit_tasks_lookup_timezone), self);
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (self->priv->items[EDIT_ITEM_COMPLETED].part),
		E_TIMEZONE_ENTRY (widget));
	g_signal_connect_swapped (self->priv->items[EDIT_ITEM_COMPLETED].part, "lookup-timezone",
		G_CALLBACK (e_bulk_edit_tasks_lookup_timezone), self);

	if (first_icomp) {
		ICalProperty *prop;

		for (ii = 0; ii < N_EDIT_ITEMS; ii++) {
			e_comp_editor_property_part_fill_widget (self->priv->items[ii].part, first_icomp);
		}

		prop = i_cal_component_get_first_property (first_icomp, I_CAL_CATEGORIES_PROPERTY);
		if (prop) {
			const gchar *value = i_cal_property_get_categories (prop);

			if (value && *value)
				e_categories_selector_set_checked (self->priv->categories, value);

			g_clear_object (&prop);
		}
	}

	e_binding_bind_property_full (self->priv->items[EDIT_ITEM_DTSTART].check, "active",
		self->priv->items[EDIT_ITEM_TIMEZONE].check, "active",
		G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
		e_bulk_edit_tasks_dates_to_timezone_cb,
		NULL, self, NULL);

	e_binding_bind_property_full (self->priv->items[EDIT_ITEM_DUE].check, "active",
		self->priv->items[EDIT_ITEM_TIMEZONE].check, "active",
		G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
		e_bulk_edit_tasks_dates_to_timezone_cb,
		NULL, self, NULL);

	e_binding_bind_property_full (self->priv->items[EDIT_ITEM_COMPLETED].check, "active",
		self->priv->items[EDIT_ITEM_TIMEZONE].check, "active",
		G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
		e_bulk_edit_tasks_dates_to_timezone_cb,
		NULL, self, NULL);

	/* when changing one of these, change all of them, because they are closely related
	   and the GUI part shows what will be saved, instead of sudden surprises */

	e_binding_bind_property (self->priv->items[EDIT_ITEM_COMPLETED].check, "active",
		self->priv->items[EDIT_ITEM_PERCENTCOMPLETE].check, "active",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	e_binding_bind_property (self->priv->items[EDIT_ITEM_COMPLETED].check, "active",
		self->priv->items[EDIT_ITEM_STATUS].check, "active",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

static void
e_bulk_edit_tasks_submit_alert (EAlertSink *alert_sink,
				EAlert *alert)
{
	EBulkEditTasks *self;

	g_return_if_fail (E_IS_BULK_EDIT_TASKS (alert_sink));

	self = E_BULK_EDIT_TASKS (alert_sink);

	e_alert_bar_submit_alert (E_ALERT_BAR (self->priv->alert_bar), alert);
}

static void
e_bulk_edit_tasks_dispose (GObject *object)
{
	EBulkEditTasks *self = E_BULK_EDIT_TASKS (object);
	guint ii;

	for (ii = 0; ii < N_EDIT_ITEMS; ii++) {
		g_clear_object (&self->priv->items[ii].part);
	}

	g_cancellable_cancel (self->priv->cancellable);
	g_clear_object (&self->priv->cancellable);

	self->priv->items_grid = NULL;
	self->priv->alert_bar = NULL;
	self->priv->activity_bar = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_bulk_edit_tasks_parent_class)->dispose (object);
}

static void
e_bulk_edit_tasks_finalize (GObject *object)
{
	EBulkEditTasks *self = E_BULK_EDIT_TASKS (object);

	g_clear_pointer (&self->priv->comps, g_ptr_array_unref);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_bulk_edit_tasks_parent_class)->finalize (object);
}

static void
e_bulk_edit_tasks_class_init (EBulkEditTasksClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = e_bulk_edit_tasks_dispose;
	object_class->finalize = e_bulk_edit_tasks_finalize;
}

static void
e_bulk_edit_tasks_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = e_bulk_edit_tasks_submit_alert;
}

static void
e_bulk_edit_tasks_init (EBulkEditTasks *self)
{
	self->priv = e_bulk_edit_tasks_get_instance_private (self);
	self->priv->last_duration = -1;
}

/**
 * e_bulk_edit_tasks_new:
 * @parent: (nullable): a parent #GtkWindow for the dialog, or %NULL
 * @components: (element-type ECalModelComponent): a #GSList with components to edit, as #ECalModelComponent
 *
 * Creates a new dialog for bulk edit of tasks provided by the @components.
 *
 * Returns: (transfer full): a new #EBulkEditTasks dialog.
 *
 * Since: 3.52
 */
GtkWidget *
e_bulk_edit_tasks_new (GtkWindow *parent,
		       GSList *components)
{
	EBulkEditTasks *self;
	GSList *link;

	self = g_object_new (E_TYPE_BULK_EDIT_TASKS,
		"transient-for", parent,
		"destroy-with-parent", TRUE,
		"modal", TRUE,
		"use-header-bar", e_util_get_use_header_bar (),
		NULL);

	self->priv->comps = g_ptr_array_new_full (g_slist_length (components), comp_ref_free);

	for (link = components; link; link = g_slist_next (link)) {
		ECalModelComponent *model_comp = link->data;

		if (model_comp->client && model_comp->icalcomp)
			g_ptr_array_add (self->priv->comps, comp_ref_new (model_comp->client, model_comp->icalcomp));
	}

	e_bulk_edit_tasks_fill_content (self);

	return GTK_WIDGET (self);
}
