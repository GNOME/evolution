/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Federico Mena-Quintero <federico@ximian.com>
 *	Miguel de Icaza <miguel@ximian.com>
 *	Seth Alves <alves@hungry.com>
 *	JP Rosevear <jpr@ximian.com>
 *	Hans Petter Jansson <hpj@ximiman.com>
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <e-util/e-util.h>

#include "calendar-config.h"
#include "comp-util.h"
#include "e-weekday-chooser.h"
#include "e-date-time-list.h"
#include "tag-calendar.h"

#include "e-comp-editor-page-recurrence.h"

enum month_num_options {
	MONTH_NUM_INVALID = -1,
	MONTH_NUM_FIRST,
	MONTH_NUM_SECOND,
	MONTH_NUM_THIRD,
	MONTH_NUM_FOURTH,
	MONTH_NUM_FIFTH,
	MONTH_NUM_LAST,
	MONTH_NUM_DAY,
	MONTH_NUM_OTHER
};

static const gint month_num_options_map[] = {
	MONTH_NUM_FIRST,
	MONTH_NUM_SECOND,
	MONTH_NUM_THIRD,
	MONTH_NUM_FOURTH,
	MONTH_NUM_FIFTH,
	MONTH_NUM_LAST,
	MONTH_NUM_DAY,
	MONTH_NUM_OTHER,
	-1
};

enum month_day_options {
	MONTH_DAY_NTH,
	MONTH_DAY_MON,
	MONTH_DAY_TUE,
	MONTH_DAY_WED,
	MONTH_DAY_THU,
	MONTH_DAY_FRI,
	MONTH_DAY_SAT,
	MONTH_DAY_SUN
};

static const gint month_day_options_map[] = {
	MONTH_DAY_NTH,
	MONTH_DAY_MON,
	MONTH_DAY_TUE,
	MONTH_DAY_WED,
	MONTH_DAY_THU,
	MONTH_DAY_FRI,
	MONTH_DAY_SAT,
	MONTH_DAY_SUN,
	-1
};

enum recur_type {
	RECUR_NONE,
	RECUR_SIMPLE,
	RECUR_CUSTOM
};

static const gint freq_map[] = {
	ICAL_DAILY_RECURRENCE,
	ICAL_WEEKLY_RECURRENCE,
	ICAL_MONTHLY_RECURRENCE,
	ICAL_YEARLY_RECURRENCE,
	-1
};

enum ending_type {
	ENDING_FOR,
	ENDING_UNTIL,
	ENDING_FOREVER
};

static const gint ending_types_map[] = {
	ENDING_FOR,
	ENDING_UNTIL,
	ENDING_FOREVER,
	-1
};

struct _ECompEditorPageRecurrencePrivate {
	GtkWidget *recr_check_box;
	GtkWidget *recr_hbox;
	GtkWidget *recr_interval_value_spin;
	GtkWidget *recr_interval_unit_combo;
	GtkWidget *recr_interval_special_box;
	GtkWidget *recr_ending_combo;
	GtkWidget *recr_ending_special_box;
	GtkWidget *recr_cannot_edit_label;
	GtkWidget *exceptions_tree_view;
	GtkWidget *exceptions_button_box;
	GtkWidget *exceptions_add_button;
	GtkWidget *exceptions_edit_button;
	GtkWidget *exceptions_remove_button;
	GtkWidget *preview;

	gboolean is_custom;
	EDateTimeList *exceptions_store;
	GCancellable *cancellable;

	/* For weekly recurrences, created by hand */
	GtkWidget *weekday_chooser;
	guint8 weekday_day_mask;
	guint8 weekday_blocked_day_mask;

	/* For monthly recurrences, created by hand */
	gint month_index;

	GtkWidget *month_day_combo;
	enum month_day_options month_day;

	GtkWidget *month_num_combo;
	enum month_num_options month_num;

	/* For ending date, created by hand */
	GtkWidget *ending_date_edit;
	struct icaltimetype ending_date_tt;

	/* For ending count of occurrences, created by hand */
	GtkWidget *ending_count_spin;
	gint ending_count;
};

G_DEFINE_TYPE (ECompEditorPageRecurrence, e_comp_editor_page_recurrence, E_TYPE_COMP_EDITOR_PAGE)

static void
ecep_recurrence_update_preview (ECompEditorPageRecurrence *page_recurrence)
{
	ECompEditor *comp_editor;
	ECalClient *client;
	ECalComponent *comp;
	icalcomponent *icalcomp;
	const icalcomponent *editing_comp;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));
	g_return_if_fail (E_IS_CALENDAR (page_recurrence->priv->preview));

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_recurrence));
	client = e_comp_editor_get_source_client (comp_editor);
	if (!client)
		client = e_comp_editor_get_target_client (comp_editor);

	e_calendar_item_clear_marks (e_calendar_get_item (E_CALENDAR (page_recurrence->priv->preview)));

	editing_comp = e_comp_editor_get_component (comp_editor);
	if (!editing_comp || e_cal_util_component_is_instance ((icalcomponent *) editing_comp)) {
		g_clear_object (&comp_editor);
		return;
	}

	icalcomp = icalcomponent_new_clone ((icalcomponent *) editing_comp);

	e_comp_editor_set_updating (comp_editor, TRUE);
	e_comp_editor_fill_component (comp_editor, icalcomp);
	e_comp_editor_set_updating (comp_editor, FALSE);

	comp = e_cal_component_new_from_icalcomponent (icalcomp);

	if (comp) {
		icaltimezone *zone = NULL;

		icalcomp = e_cal_component_get_icalcomponent (comp);

		if (icalcomponent_get_first_property (icalcomp, ICAL_DTSTART_PROPERTY)) {
			struct icaltimetype dt;

			dt = icalcomponent_get_dtstart (icalcomp);
			zone = (icaltimezone *) dt.zone;
		}

		e_cal_component_rescan (comp);

		if (!zone)
			zone = calendar_config_get_icaltimezone ();

		tag_calendar_by_comp (
			E_CALENDAR (page_recurrence->priv->preview), comp,
			client, zone, TRUE, FALSE, FALSE, page_recurrence->priv->cancellable);
		g_object_unref (comp);
	}

	g_clear_object (&comp_editor);
}

static void
ecep_recurrence_changed (ECompEditorPageRecurrence *page_recurrence)
{
	ECompEditorPage *page;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	page = E_COMP_EDITOR_PAGE (page_recurrence);

	if (e_comp_editor_page_get_updating (page))
		return;

	e_comp_editor_page_emit_changed (page);
	ecep_recurrence_update_preview (page_recurrence);
}

static void
ecep_recurrence_append_exception (ECompEditorPageRecurrence *page_recurrence,
				  const struct icaltimetype itt)
{
	GtkTreeView *view;
	GtkTreeIter  iter;

	view = GTK_TREE_VIEW (page_recurrence->priv->exceptions_tree_view);

	e_date_time_list_append (page_recurrence->priv->exceptions_store, &iter, itt);
	gtk_tree_selection_select_iter (gtk_tree_view_get_selection (view), &iter);
}

static void
ecep_recurrence_fill_exception_widgets (ECompEditorPageRecurrence *page_recurrence,
					icalcomponent *component)
{
	icalproperty *prop;

	e_date_time_list_clear (page_recurrence->priv->exceptions_store);

	for (prop = icalcomponent_get_first_property (component, ICAL_EXDATE_PROPERTY);
	     prop;
	     prop = icalcomponent_get_next_property (component, ICAL_EXDATE_PROPERTY)) {
		struct icaltimetype itt;

		itt = icalproperty_get_exdate (prop);
		if (!icaltime_is_valid_time (itt) ||
		    icaltime_is_null_time (itt))
			continue;

		ecep_recurrence_append_exception (page_recurrence, itt);
	}
}

static void
ecep_recurrence_exceptions_selection_changed_cb (GtkTreeSelection *selection,
						 ECompEditorPageRecurrence *page_recurrence)
{
	gboolean any_selected;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	any_selected = gtk_tree_selection_count_selected_rows (selection) > 0;

	gtk_widget_set_sensitive (page_recurrence->priv->exceptions_edit_button, any_selected);
	gtk_widget_set_sensitive (page_recurrence->priv->exceptions_remove_button, any_selected);
}

static GtkWidget *
ecep_recurrence_create_exception_dialog (ECompEditorPageRecurrence *page_recurrence,
					 const gchar *title,
					 GtkWidget **out_date_edit)
{
	GtkWidget *dialog, *toplevel;
	GtkWidget *container;
	EDateEdit *date_edit;

	toplevel = gtk_widget_get_toplevel (page_recurrence->priv->recr_check_box);

	if (!GTK_IS_WINDOW (toplevel))
		toplevel = NULL;

	dialog = gtk_dialog_new_with_buttons (
		title, GTK_WINDOW (toplevel),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_REJECT,
		_("_OK"), GTK_RESPONSE_ACCEPT,
		NULL);

	*out_date_edit = e_date_edit_new ();
	date_edit = E_DATE_EDIT (*out_date_edit);
	e_date_edit_set_show_date (date_edit, TRUE);
	e_date_edit_set_show_time (date_edit, FALSE);

	gtk_widget_show (*out_date_edit);
	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (container), *out_date_edit, FALSE, TRUE, 6);

	return dialog;
}

static void
ecep_recurrence_exceptions_add_clicked_cb (GtkButton *button,
					   ECompEditorPageRecurrence *page_recurrence)
{
	GtkWidget *dialog, *date_edit;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	dialog = ecep_recurrence_create_exception_dialog (page_recurrence, _("Add exception"), &date_edit);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		struct icaltimetype itt = icaltime_null_time ();

		/* We use DATE values for exceptions, so we don't need a TZID. */
		itt.zone = NULL;
		itt.hour = 0;
		itt.minute = 0;
		itt.second = 0;
		itt.is_date = 1;

		if (e_date_edit_get_date (E_DATE_EDIT (date_edit), &itt.year, &itt.month, &itt.day)) {
			ecep_recurrence_append_exception (page_recurrence, itt);
			ecep_recurrence_changed (page_recurrence);
		}
	}

	gtk_widget_destroy (dialog);
}

static void
ecep_recurrence_exceptions_edit_clicked_cb (GtkButton *button,
					    ECompEditorPageRecurrence *page_recurrence)
{
	GtkWidget *dialog, *date_edit;
	const struct icaltimetype *current_itt;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_recurrence->priv->exceptions_tree_view));
	g_return_if_fail (gtk_tree_selection_get_selected (selection, NULL, &iter));

	current_itt = e_date_time_list_get_date_time (page_recurrence->priv->exceptions_store, &iter);
	g_return_if_fail (current_itt != NULL);

	dialog = ecep_recurrence_create_exception_dialog (page_recurrence, _("Modify exception"), &date_edit);
	e_date_edit_set_date (E_DATE_EDIT (date_edit),
		current_itt->year, current_itt->month, current_itt->day);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		struct icaltimetype itt = icaltime_null_time ();

		/* We use DATE values for exceptions, so we don't need a TZID. */
		itt.zone = NULL;
		itt.hour = 0;
		itt.minute = 0;
		itt.second = 0;
		itt.is_date = 1;

		if (e_date_edit_get_date (E_DATE_EDIT (date_edit), &itt.year, &itt.month, &itt.day)) {
			e_date_time_list_set_date_time (page_recurrence->priv->exceptions_store, &iter, itt);
			ecep_recurrence_changed (page_recurrence);
		}
	}

	gtk_widget_destroy (dialog);
}

static void
ecep_recurrence_exceptions_remove_clicked_cb (GtkButton *button,
					      ECompEditorPageRecurrence *page_recurrence)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean valid_iter;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_recurrence->priv->exceptions_tree_view));
	g_return_if_fail (gtk_tree_selection_get_selected (selection, NULL, &iter));

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (page_recurrence->priv->exceptions_store), &iter);
	e_date_time_list_remove (page_recurrence->priv->exceptions_store, &iter);

	/* Select closest item after removal */
	valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (page_recurrence->priv->exceptions_store), &iter, path);
	if (!valid_iter) {
		gtk_tree_path_prev (path);
		valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (page_recurrence->priv->exceptions_store), &iter, path);
	}

	if (valid_iter)
		gtk_tree_selection_select_iter (selection, &iter);

	gtk_tree_path_free (path);

	ecep_recurrence_changed (page_recurrence);
}

static void
ecep_recurrence_checkbox_toggled_cb (GtkToggleButton *checkbox,
				     ECompEditorPageRecurrence *page_recurrence)
{
	ECompEditor *comp_editor;
	ECompEditorPage *page;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	page = E_COMP_EDITOR_PAGE (page_recurrence);
	comp_editor = e_comp_editor_page_ref_editor (page);
	e_comp_editor_sensitize_widgets (comp_editor);
	g_clear_object (&comp_editor);

	e_comp_editor_page_emit_changed (page);
}

static struct tm
ecep_recurrence_get_current_time_cb (ECalendarItem *calitem,
				     gpointer user_data)
{
	struct icaltimetype today;

	today = icaltime_today ();

	return icaltimetype_to_tm (&today);
}

static GtkWidget *
ecep_recurrence_get_box_first_child (GtkWidget *box)
{
	GtkWidget *first_child;
	GList *children;

	if (!box)
		return NULL;

	g_return_val_if_fail (GTK_IS_BOX (box), NULL);

	children = gtk_container_get_children (GTK_CONTAINER (box));
	if (!children)
		return NULL;

	first_child = children->data;

	g_list_free (children);

	return first_child;
}

/* Creates the special contents for weekly recurrences */
static void
ecep_recurrence_make_weekly_special (ECompEditorPageRecurrence *page_recurrence)
{
	GtkWidget *hbox;
	GtkWidget *label;
	EWeekdayChooser *chooser;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	g_return_if_fail (ecep_recurrence_get_box_first_child (page_recurrence->priv->recr_interval_special_box) == NULL);
	g_return_if_fail (page_recurrence->priv->weekday_chooser == NULL);

	/* Create the widgets */

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add (GTK_CONTAINER (page_recurrence->priv->recr_interval_special_box), hbox);

	/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] week(s) on [Wednesday] [forever]'
	 * (dropdown menu options are in [square brackets]). This means that after the 'on', name of a week day always follows. */
	label = gtk_label_new (_("on"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);

	page_recurrence->priv->weekday_chooser = e_weekday_chooser_new ();
	chooser = E_WEEKDAY_CHOOSER (page_recurrence->priv->weekday_chooser);

	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (chooser), FALSE, FALSE, 6);

	gtk_widget_show_all (hbox);

	/* Set the weekdays */

	e_weekday_chooser_set_selected (
		chooser, G_DATE_SUNDAY,
		(page_recurrence->priv->weekday_day_mask & (1 << 0)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_MONDAY,
		(page_recurrence->priv->weekday_day_mask & (1 << 1)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_TUESDAY,
		(page_recurrence->priv->weekday_day_mask & (1 << 2)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_WEDNESDAY,
		(page_recurrence->priv->weekday_day_mask & (1 << 3)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_THURSDAY,
		(page_recurrence->priv->weekday_day_mask & (1 << 4)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_FRIDAY,
		(page_recurrence->priv->weekday_day_mask & (1 << 5)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_SATURDAY,
		(page_recurrence->priv->weekday_day_mask & (1 << 6)) != 0);

	g_signal_connect_swapped (
		chooser, "changed",
		G_CALLBACK (ecep_recurrence_changed), page_recurrence);
}

/* Creates the subtree for the monthly recurrence number */
static void
ecep_recurrence_make_recur_month_num_subtree (GtkTreeStore *store,
					      GtkTreeIter *par,
					      const gchar *title,
					      gint start,
					      gint end)
{
	GtkTreeIter iter, parent;
	gint i;

	gtk_tree_store_append (store, &parent, par);
	gtk_tree_store_set (store, &parent, 0, _(title), 1, -1, -1);

	for (i = start; i < end; i++) {
		gtk_tree_store_append (store, &iter, &parent);
		gtk_tree_store_set (store, &iter, 0, e_cal_recur_get_localized_nth (i), 1, i + 1, -1);
	}
}

static void
ecep_recurrence_only_leaf_sensitive (GtkCellLayout *cell_layout,
				     GtkCellRenderer *cell,
				     GtkTreeModel *tree_model,
				     GtkTreeIter *iter,
				     gpointer data)
{
	gboolean sensitive;

	sensitive = !gtk_tree_model_iter_has_child (tree_model, iter);

	g_object_set (cell, "sensitive", sensitive, NULL);
}

static GtkWidget *
ecep_recurrence_make_recur_month_num_combo (gint month_index)
{
	static const gchar *options[] = {
		/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [first] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'first', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		NC_("ECompEditorPageRecur", "first"),
		/* TRANSLATORS: here, "second" is the ordinal number (like "third"), not the time division (like "minute")
		 * Entire string is for example: This appointment recurs/Every [x] month(s) on the [second] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'second', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		NC_("ECompEditorPageRecur", "second"),
		/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [third] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'third', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		NC_("ECompEditorPageRecur", "third"),
		/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [fourth] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'fourth', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		NC_("ECompEditorPageRecur", "fourth"),
		/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [fifth] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'fifth', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		NC_("ECompEditorPageRecur", "fifth"),
		/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [last] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'last', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		NC_("ECompEditorPageRecur", "last")
	};

	gint i;
	GtkTreeStore *store;
	GtkTreeIter iter;
	GtkWidget *combo;
	GtkCellRenderer *cell;

	store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_INT);

	/* Relation */
	for (i = 0; i < G_N_ELEMENTS (options); i++) {
		gtk_tree_store_append (store, &iter, NULL);
		gtk_tree_store_set (store, &iter,
			0, g_dpgettext2 (GETTEXT_PACKAGE, "ECompEditorPageRecur", options[i]),
			1, month_num_options_map[i],
			-1);
	}

	/* Current date */
	gtk_tree_store_append (store, &iter, NULL);
	gtk_tree_store_set (store, &iter, 0, e_cal_recur_get_localized_nth (month_index - 1), 1, MONTH_NUM_DAY, -1);

	gtk_tree_store_append (store, &iter, NULL);
	/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [Other date] [11th to 20th] [17th] [forever]'
	 * (dropdown menu options are in [square brackets]). */
	gtk_tree_store_set (store, &iter, 0, C_("ECompEditorPageRecur", "Other Date"), 1, MONTH_NUM_OTHER, -1);

	/* TRANSLATORS: This is a submenu option string to split the date range into three submenus to choose the exact day of
	 * the month to setup an appointment recurrence. The entire string is for example: This appointment recurs/Every [x] month(s)
	 * on the [Other date] [1st to 10th] [7th] [forever]' (dropdown menu options are in [square brackets]).
	 */
	ecep_recurrence_make_recur_month_num_subtree (store, &iter, C_("ECompEditorPageRecur", "1st to 10th"), 0, 10);

	/* TRANSLATORS: This is a submenu option string to split the date range into three submenus to choose the exact day of
	 * the month to setup an appointment recurrence. The entire string is for example: This appointment recurs/Every [x] month(s)
	 * on the [Other date] [11th to 20th] [17th] [forever]' (dropdown menu options are in [square brackets]).
	 */
	ecep_recurrence_make_recur_month_num_subtree (store, &iter, C_("ECompEditorPageRecur", "11th to 20th"), 10, 20);

	/* TRANSLATORS: This is a submenu option string to split the date range into three submenus to choose the exact day of
	 * the month to setup an appointment recurrence. The entire string is for example: This appointment recurs/Every [x] month(s)
	 * on the [Other date] [21th to 31th] [27th] [forever]' (dropdown menu options are in [square brackets]).
	 */
	ecep_recurrence_make_recur_month_num_subtree (store, &iter, C_("ECompEditorPageRecur", "21st to 31st"), 20, 31);

	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell, "text", 0, NULL);

	gtk_cell_layout_set_cell_data_func (
		GTK_CELL_LAYOUT (combo),
		cell,
		ecep_recurrence_only_leaf_sensitive,
		NULL, NULL);

	return combo;
}

/* Creates the combo box for the monthly recurrence days */
static GtkWidget *
ecep_recurrence_make_recur_month_combobox (void)
{
	static const gchar *options[] = {
		/* For Translator : 'day' is part of the sentence of the form 'appointment recurs/Every [x] month(s) on the [first] [day] [forever]'
		 * (dropdown menu options are in[square brackets]). This means that after 'first', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow. */
		NC_("ECompEditorPageRecur", "day"),
		NC_("ECompEditorPageRecur", "Monday"),
		NC_("ECompEditorPageRecur", "Tuesday"),
		NC_("ECompEditorPageRecur", "Wednesday"),
		NC_("ECompEditorPageRecur", "Thursday"),
		NC_("ECompEditorPageRecur", "Friday"),
		NC_("ECompEditorPageRecur", "Saturday"),
		NC_("ECompEditorPageRecur", "Sunday")
	};

	GtkWidget *combo;
	gint i;

	combo = gtk_combo_box_text_new ();

	for (i = 0; i < G_N_ELEMENTS (options); i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
			g_dpgettext2 (GETTEXT_PACKAGE, "ECompEditorPageRecur", options[i]));
	}

	return combo;
}

static void
ecep_recurrence_month_num_combo_changed_cb (GtkComboBox *combo,
					    ECompEditorPageRecurrence *page_recurrence)
{
	GtkTreeIter iter;
	enum month_num_options month_num;
	enum month_day_options month_day;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	month_day = e_dialog_combo_box_get (
		page_recurrence->priv->month_day_combo,
		month_day_options_map);

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (page_recurrence->priv->month_num_combo), &iter)) {
		gint value;
		GtkTreeIter parent;
		GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (page_recurrence->priv->month_num_combo));

		gtk_tree_model_get (model, &iter, 1, &value, -1);

		if (value == -1) {
			return;
		}

		if (gtk_tree_model_iter_parent (model, &parent, &iter)) {
			/* it's a leaf, thus the day number */
			month_num = MONTH_NUM_DAY;
			page_recurrence->priv->month_index = value;

			g_return_if_fail (gtk_tree_model_iter_nth_child (model, &iter, NULL, month_num));

			gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 0, e_cal_recur_get_localized_nth (page_recurrence->priv->month_index - 1), -1);
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (page_recurrence->priv->month_num_combo), &iter);
		} else {
			/* top level node */
			month_num = value;

			if (month_num == MONTH_NUM_OTHER)
				month_num = MONTH_NUM_DAY;
		}
	} else {
		month_num = 0;
	}

	if (month_num == MONTH_NUM_DAY && month_day != MONTH_DAY_NTH)
		e_dialog_combo_box_set (
			page_recurrence->priv->month_day_combo,
			MONTH_DAY_NTH,
			month_day_options_map);
	else if (month_num != MONTH_NUM_DAY && month_num != MONTH_NUM_LAST && month_day == MONTH_DAY_NTH)
		e_dialog_combo_box_set (
			page_recurrence->priv->month_day_combo,
			MONTH_DAY_MON,
			month_num_options_map);

	ecep_recurrence_changed (page_recurrence);
}

/* Callback used when the monthly day selection changes.  We need
 * to change the valid range of the day index spin button; e.g. days
 * are 1-31 while a Sunday is the 1st through 5th.
 */
static void
ecep_recurrence_month_day_combo_changed_cb (GtkComboBox *combo,
					    ECompEditorPageRecurrence *page_recurrence)
{
	enum month_num_options month_num;
	enum month_day_options month_day;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	month_num = e_dialog_combo_box_get (
		page_recurrence->priv->month_num_combo,
		month_num_options_map);
	month_day = e_dialog_combo_box_get (
		page_recurrence->priv->month_day_combo,
		month_day_options_map);
	if (month_day == MONTH_DAY_NTH && month_num != MONTH_NUM_LAST && month_num != MONTH_NUM_DAY)
		e_dialog_combo_box_set (
			page_recurrence->priv->month_num_combo,
			MONTH_NUM_DAY,
			month_num_options_map);
	else if (month_day != MONTH_DAY_NTH && month_num == MONTH_NUM_DAY)
		e_dialog_combo_box_set (
			page_recurrence->priv->month_num_combo,
			MONTH_NUM_FIRST,
			month_num_options_map);

	ecep_recurrence_changed (page_recurrence);
}

/* Creates the special contents for monthly recurrences */
static void
ecep_recurrence_make_monthly_special (ECompEditorPageRecurrence *page_recurrence)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkAdjustment *adj;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	g_return_if_fail (ecep_recurrence_get_box_first_child (page_recurrence->priv->recr_interval_special_box) == NULL);
	g_return_if_fail (page_recurrence->priv->month_day_combo == NULL);

	/* Create the widgets */

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add (GTK_CONTAINER (page_recurrence->priv->recr_interval_special_box), hbox);

	/* TRANSLATORS: Entire string is for example: 'This appointment recurs/Every [x] month(s) on the [second] [Tuesday] [forever]'
	 * (dropdown menu options are in [square brackets])."
	 */
	label = gtk_label_new (C_("ECompEditorPageRecur", "on the"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (1, 1, 31, 1, 10, 10));

	page_recurrence->priv->month_num_combo = ecep_recurrence_make_recur_month_num_combo (page_recurrence->priv->month_index);
	gtk_box_pack_start (
		GTK_BOX (hbox), page_recurrence->priv->month_num_combo,
		FALSE, FALSE, 6);

	page_recurrence->priv->month_day_combo = ecep_recurrence_make_recur_month_combobox ();
	gtk_box_pack_start (
		GTK_BOX (hbox), page_recurrence->priv->month_day_combo,
		FALSE, FALSE, 6);

	gtk_widget_show_all (hbox);

	/* Set the options */
	e_dialog_combo_box_set (
		page_recurrence->priv->month_num_combo,
		page_recurrence->priv->month_num,
		month_num_options_map);
	e_dialog_combo_box_set (
		page_recurrence->priv->month_day_combo,
		page_recurrence->priv->month_day,
		month_day_options_map);

	g_signal_connect_swapped (
		adj, "value-changed",
		G_CALLBACK (ecep_recurrence_changed), page_recurrence);

	g_signal_connect (
		page_recurrence->priv->month_num_combo, "changed",
		G_CALLBACK (ecep_recurrence_month_num_combo_changed_cb), page_recurrence);

	g_signal_connect (
		page_recurrence->priv->month_day_combo, "changed",
		G_CALLBACK (ecep_recurrence_month_day_combo_changed_cb), page_recurrence);
}

/* Changes the recurrence-special widget to match the interval units.
 *
 * For daily recurrences: nothing.
 * For weekly recurrences: weekday selector.
 * For monthly recurrences: "on the" <nth> [day, Weekday]
 * For yearly recurrences: nothing.
 */
static void
ecep_recurrence_make_recurrence_special (ECompEditorPageRecurrence *page_recurrence)
{
	icalrecurrencetype_frequency frequency;
	GtkWidget *child;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	if (page_recurrence->priv->month_num_combo != NULL) {
		gtk_widget_destroy (page_recurrence->priv->month_num_combo);
		page_recurrence->priv->month_num_combo = NULL;
	}

	child = ecep_recurrence_get_box_first_child (page_recurrence->priv->recr_interval_special_box);
	if (child != NULL) {
		gtk_widget_destroy (child);

		page_recurrence->priv->weekday_chooser = NULL;
		page_recurrence->priv->month_day_combo = NULL;
	}

	frequency = e_dialog_combo_box_get (page_recurrence->priv->recr_interval_unit_combo, freq_map);

	switch (frequency) {
	case ICAL_DAILY_RECURRENCE:
		gtk_widget_hide (page_recurrence->priv->recr_interval_special_box);
		break;

	case ICAL_WEEKLY_RECURRENCE:
		ecep_recurrence_make_weekly_special (page_recurrence);
		gtk_widget_show (page_recurrence->priv->recr_interval_special_box);
		break;

	case ICAL_MONTHLY_RECURRENCE:
		ecep_recurrence_make_monthly_special (page_recurrence);
		gtk_widget_show (page_recurrence->priv->recr_interval_special_box);
		break;

	case ICAL_YEARLY_RECURRENCE:
		gtk_widget_hide (page_recurrence->priv->recr_interval_special_box);
		break;

	default:
		g_return_if_reached ();
	}
}

/* Counts the elements in the by_xxx fields of an icalrecurrencetype */
static gint
ecep_recurrence_count_by_xxx (gshort *field,
			      gint max_elements)
{
	gint ii;

	for (ii = 0; ii < max_elements; ii++)
		if (field[ii] == ICAL_RECURRENCE_ARRAY_MAX)
			break;

	return ii;
}

/* Creates the special contents for "ending until" (end date) recurrences */
static void
ecep_recurrence_make_ending_until_special (ECompEditorPageRecurrence *page_recurrence)
{
	ECompEditor *comp_editor;
	guint32 flags;
	const icalcomponent *icomp;
	EDateEdit *date_edit;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));
	g_return_if_fail (ecep_recurrence_get_box_first_child (page_recurrence->priv->recr_ending_special_box) == NULL);
	g_return_if_fail (page_recurrence->priv->ending_date_edit == NULL);

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_recurrence));
	flags = e_comp_editor_get_flags (comp_editor);

	/* Create the widget */

	page_recurrence->priv->ending_date_edit = e_date_edit_new ();
	date_edit = E_DATE_EDIT (page_recurrence->priv->ending_date_edit);
	e_date_edit_set_show_date (date_edit, TRUE);
	e_date_edit_set_show_time (date_edit, FALSE);

	gtk_container_add (
		GTK_CONTAINER (page_recurrence->priv->recr_ending_special_box),
		page_recurrence->priv->ending_date_edit);
	gtk_widget_show (page_recurrence->priv->ending_date_edit);

	/* Set the value */

	icomp = e_comp_editor_get_component (comp_editor);
	if ((flags & E_COMP_EDITOR_FLAG_IS_NEW) != 0 && icomp) {
		struct icaltimetype itt;

		itt = icalcomponent_get_dtstart ((icalcomponent *) icomp);
		/* Setting the default until time to 2 weeks */
		icaltime_adjust (&itt, 14, 0, 0, 0);

		e_date_edit_set_date (date_edit, itt.year, itt.month, itt.day);
	} else {
		e_date_edit_set_date (date_edit,
			page_recurrence->priv->ending_date_tt.year,
			page_recurrence->priv->ending_date_tt.month,
			page_recurrence->priv->ending_date_tt.day);
	}

	g_signal_connect_swapped (
		date_edit, "changed",
		G_CALLBACK (ecep_recurrence_changed), page_recurrence);

	e_date_edit_set_get_time_callback (date_edit,
		(EDateEditGetTimeCallback) ecep_recurrence_get_current_time_cb,
		NULL, NULL);

	g_clear_object (&comp_editor);
}

/* Creates the special contents for the occurrence count case */
static void
ecep_recurrence_make_ending_count_special (ECompEditorPageRecurrence *page_recurrence)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkAdjustment *adj;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	g_return_if_fail (ecep_recurrence_get_box_first_child (page_recurrence->priv->recr_ending_special_box) == NULL);
	g_return_if_fail (page_recurrence->priv->ending_count_spin == NULL);

	/* Create the widgets */

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add (GTK_CONTAINER (page_recurrence->priv->recr_ending_special_box), hbox);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (1, 1, 10000, 1, 10, 0));
	page_recurrence->priv->ending_count_spin = gtk_spin_button_new (adj, 1, 0);
	gtk_spin_button_set_numeric ((GtkSpinButton *) page_recurrence->priv->ending_count_spin, TRUE);
	gtk_box_pack_start (
		GTK_BOX (hbox), page_recurrence->priv->ending_count_spin,
		FALSE, FALSE, 6);

	label = gtk_label_new (C_("ECompEditorPageRecur", "occurrences"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);

	gtk_widget_show_all (hbox);

	/* Set the values */

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (page_recurrence->priv->ending_count_spin),
		page_recurrence->priv->ending_count);

	g_signal_connect_swapped (
		adj, "value-changed",
		G_CALLBACK (ecep_recurrence_changed), page_recurrence);
}

/* Changes the recurrence-ending-special widget to match the ending date option
 *
 * For: <n> [days, weeks, months, years, occurrences]
 * Until: <date selector>
 * Forever: nothing.
 */
static void
ecep_recurrence_make_ending_special (ECompEditorPageRecurrence *page_recurrence)
{
	enum ending_type ending_type;
	GtkWidget *child;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	child = ecep_recurrence_get_box_first_child (page_recurrence->priv->recr_ending_special_box);
	if (child != NULL) {
		gtk_widget_destroy (child);

		page_recurrence->priv->ending_date_edit = NULL;
		page_recurrence->priv->ending_count_spin = NULL;
	}

	ending_type = e_dialog_combo_box_get (page_recurrence->priv->recr_ending_combo, ending_types_map);

	switch (ending_type) {
	case ENDING_FOR:
		ecep_recurrence_make_ending_count_special (page_recurrence);
		gtk_widget_show (page_recurrence->priv->recr_ending_special_box);
		break;

	case ENDING_UNTIL:
		ecep_recurrence_make_ending_until_special (page_recurrence);
		gtk_widget_show (page_recurrence->priv->recr_ending_special_box);
		break;

	case ENDING_FOREVER:
		gtk_widget_hide (page_recurrence->priv->recr_ending_special_box);
		break;

	default:
		g_return_if_reached ();
	}
}

/* Fills the recurrence ending date widgets with the values from the calendar
 * component.
 */
static void
ecep_recurrence_fill_ending_date (ECompEditorPageRecurrence *page_recurrence,
				  struct icalrecurrencetype *rrule,
				  icalcomponent *component)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	g_signal_handlers_block_matched (page_recurrence->priv->recr_ending_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);

	if (rrule->count == 0) {
		if (rrule->until.year == 0) {
			/* Forever */

			e_dialog_combo_box_set (
				page_recurrence->priv->recr_ending_combo,
				ENDING_FOREVER,
				ending_types_map);
		} else {
			/* Ending date */

			if (!rrule->until.is_date) {
				icaltimezone *from_zone, *to_zone;
				struct icaltimetype dtstart;

				dtstart = icalcomponent_get_dtstart (component);

				from_zone = icaltimezone_get_utc_timezone ();
				to_zone = (icaltimezone *) dtstart.zone;

				if (to_zone)
					icaltimezone_convert_time (&rrule->until, from_zone, to_zone);

				rrule->until.hour = 0;
				rrule->until.minute = 0;
				rrule->until.second = 0;
				rrule->until.is_date = TRUE;
			}

			page_recurrence->priv->ending_date_tt = rrule->until;
			e_dialog_combo_box_set (
				page_recurrence->priv->recr_ending_combo,
				ENDING_UNTIL,
				ending_types_map);
		}
	} else {
		/* Count of occurrences */

		page_recurrence->priv->ending_count = rrule->count;
		e_dialog_combo_box_set (
			page_recurrence->priv->recr_ending_combo,
			ENDING_FOR,
			ending_types_map);
	}

	g_signal_handlers_unblock_matched (page_recurrence->priv->recr_ending_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);

	ecep_recurrence_make_ending_special (page_recurrence);
}

/* Computes a weekday mask for the start day of a calendar component,
 * for use in a WeekdayPicker widget.
 */
static guint8
ecep_recurrence_get_start_weekday_mask (icalcomponent *component)
{
	struct icaltimetype dtstart;
	guint8 retval;

	if (!component)
		return 0;

	dtstart = icalcomponent_get_dtstart (component);

	if (icaltime_is_valid_time (dtstart)) {
		gshort weekday;

		weekday = icaltime_day_of_week (dtstart);
		retval = 0x1 << (weekday - 1);
	} else
		retval = 0;

	return retval;
}

/* Sets some sane defaults for the data sources for the recurrence special
 * widgets, even if they will not be used immediately.
 */
static void
ecep_recurrence_set_special_defaults (ECompEditorPageRecurrence *page_recurrence,
				      icalcomponent *component)
{
	guint8 mask;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	mask = ecep_recurrence_get_start_weekday_mask (component);

	page_recurrence->priv->weekday_day_mask = mask;
	page_recurrence->priv->weekday_blocked_day_mask = mask;
}

static void
ecep_recurrence_clear_widgets (ECompEditorPageRecurrence *page_recurrence)
{
	GtkAdjustment *adj;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	page_recurrence->priv->is_custom = FALSE;

	page_recurrence->priv->weekday_day_mask = 0;

	page_recurrence->priv->month_index = 1;
	page_recurrence->priv->month_num = MONTH_NUM_DAY;
	page_recurrence->priv->month_day = MONTH_DAY_NTH;

	g_signal_handlers_block_matched (page_recurrence->priv->recr_check_box, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_recurrence->priv->recr_check_box), FALSE);
	g_signal_handlers_unblock_matched (page_recurrence->priv->recr_check_box, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (page_recurrence->priv->recr_interval_value_spin));
	g_signal_handlers_block_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_recurrence->priv->recr_interval_value_spin), 1);
	g_signal_handlers_unblock_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);

	g_signal_handlers_block_matched (page_recurrence->priv->recr_interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
	e_dialog_combo_box_set (page_recurrence->priv->recr_interval_unit_combo, ICAL_DAILY_RECURRENCE, freq_map);
	g_signal_handlers_unblock_matched (page_recurrence->priv->recr_interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);

	page_recurrence->priv->ending_date_tt = icaltime_today ();
	page_recurrence->priv->ending_count = 2;

	g_signal_handlers_block_matched (page_recurrence->priv->recr_ending_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
	e_dialog_combo_box_set (page_recurrence->priv->recr_ending_combo,
		page_recurrence->priv->ending_count == -1 ? ENDING_FOREVER : ENDING_FOR,
		ending_types_map);
	g_signal_handlers_unblock_matched (page_recurrence->priv->recr_ending_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
	if (page_recurrence->priv->ending_count == -1)
		page_recurrence->priv->ending_count = 2;
	ecep_recurrence_make_ending_special (page_recurrence);

	/* Exceptions list */
	e_date_time_list_clear (page_recurrence->priv->exceptions_store);
}

static void
ecep_recurrence_simple_recur_to_comp (ECompEditorPageRecurrence *page_recurrence,
				      icalcomponent *component)
{
	struct icalrecurrencetype r;
	enum ending_type ending_type;
	icalproperty *prop;
	gboolean date_set;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	icalrecurrencetype_clear (&r);

	/* Frequency, interval, week start */

	r.freq = e_dialog_combo_box_get (page_recurrence->priv->recr_interval_unit_combo, freq_map);
	r.interval = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (page_recurrence->priv->recr_interval_value_spin));

	switch (calendar_config_get_week_start_day ()) {
		case G_DATE_MONDAY:
			r.week_start = ICAL_MONDAY_WEEKDAY;
			break;
		case G_DATE_TUESDAY:
			r.week_start = ICAL_TUESDAY_WEEKDAY;
			break;
		case G_DATE_WEDNESDAY:
			r.week_start = ICAL_WEDNESDAY_WEEKDAY;
			break;
		case G_DATE_THURSDAY:
			r.week_start = ICAL_THURSDAY_WEEKDAY;
			break;
		case G_DATE_FRIDAY:
			r.week_start = ICAL_FRIDAY_WEEKDAY;
			break;
		case G_DATE_SATURDAY:
			r.week_start = ICAL_SATURDAY_WEEKDAY;
			break;
		case G_DATE_SUNDAY:
			r.week_start = ICAL_SUNDAY_WEEKDAY;
			break;
		default:
			g_warn_if_reached ();
			break;
	}

	/* Frequency-specific data */

	switch (r.freq) {
	case ICAL_DAILY_RECURRENCE:
		/* Nothing else is required */
		break;

	case ICAL_WEEKLY_RECURRENCE: {
		EWeekdayChooser *chooser;
		gint ii;

		g_return_if_fail (ecep_recurrence_get_box_first_child (page_recurrence->priv->recr_interval_special_box) != NULL);
		g_return_if_fail (E_IS_WEEKDAY_CHOOSER (page_recurrence->priv->weekday_chooser));

		chooser = E_WEEKDAY_CHOOSER (page_recurrence->priv->weekday_chooser);

		ii = 0;

		if (e_weekday_chooser_get_selected (chooser, G_DATE_SUNDAY))
			r.by_day[ii++] = ICAL_SUNDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, G_DATE_MONDAY))
			r.by_day[ii++] = ICAL_MONDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, G_DATE_TUESDAY))
			r.by_day[ii++] = ICAL_TUESDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, G_DATE_WEDNESDAY))
			r.by_day[ii++] = ICAL_WEDNESDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, G_DATE_THURSDAY))
			r.by_day[ii++] = ICAL_THURSDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, G_DATE_FRIDAY))
			r.by_day[ii++] = ICAL_FRIDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, G_DATE_SATURDAY))
			r.by_day[ii] = ICAL_SATURDAY_WEEKDAY;

		break;
	}

	case ICAL_MONTHLY_RECURRENCE: {
		enum month_num_options month_num;
		enum month_day_options month_day;

		g_return_if_fail (ecep_recurrence_get_box_first_child (page_recurrence->priv->recr_interval_special_box) != NULL);
		g_return_if_fail (page_recurrence->priv->month_day_combo != NULL);
		g_return_if_fail (GTK_IS_COMBO_BOX (page_recurrence->priv->month_day_combo));
		g_return_if_fail (page_recurrence->priv->month_num_combo != NULL);
		g_return_if_fail (GTK_IS_COMBO_BOX (page_recurrence->priv->month_num_combo));

		month_num = e_dialog_combo_box_get (page_recurrence->priv->month_num_combo, month_num_options_map);
		month_day = e_dialog_combo_box_get (page_recurrence->priv->month_day_combo, month_day_options_map);

		if (month_num == MONTH_NUM_LAST)
			month_num = MONTH_NUM_INVALID;
		else if (month_num != MONTH_NUM_INVALID)
			month_num++;
		else
			g_warn_if_reached ();

		switch (month_day) {
		case MONTH_DAY_NTH:
			if (month_num == MONTH_NUM_INVALID)
				r.by_month_day[0] = -1;
			else
				r.by_month_day[0] = page_recurrence->priv->month_index;
			break;

		/* Outlook 2000 uses BYDAY=TU;BYSETPOS=2, and will not
		 * accept BYDAY=2TU. So we now use the same as Outlook
		 * by default. */
		case MONTH_DAY_MON:
			r.by_day[0] = ICAL_MONDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_TUE:
			r.by_day[0] = ICAL_TUESDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_WED:
			r.by_day[0] = ICAL_WEDNESDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_THU:
			r.by_day[0] = ICAL_THURSDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_FRI:
			r.by_day[0] = ICAL_FRIDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_SAT:
			r.by_day[0] = ICAL_SATURDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_SUN:
			r.by_day[0] = ICAL_SUNDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		default:
			g_return_if_reached ();
		}

		break;
	}

	case ICAL_YEARLY_RECURRENCE:
		/* Nothing else is required */
		break;

	default:
		g_return_if_reached ();
	}

	/* Ending date */

	ending_type = e_dialog_combo_box_get (page_recurrence->priv->recr_ending_combo, ending_types_map);

	switch (ending_type) {
	case ENDING_FOR:
		g_return_if_fail (page_recurrence->priv->ending_count_spin != NULL);
		g_return_if_fail (GTK_IS_SPIN_BUTTON (page_recurrence->priv->ending_count_spin));

		r.count = gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (page_recurrence->priv->ending_count_spin));
		break;

	case ENDING_UNTIL:
		g_return_if_fail (page_recurrence->priv->ending_date_edit != NULL);
		g_return_if_fail (E_IS_DATE_EDIT (page_recurrence->priv->ending_date_edit));

		/* We only allow a DATE value to be set for the UNTIL property,
		 * since we don't support sub-day recurrences. */
		date_set = e_date_edit_get_date (
			E_DATE_EDIT (page_recurrence->priv->ending_date_edit),
			&r.until.year,
			&r.until.month,
			&r.until.day);
		g_return_if_fail (date_set);

		r.until.is_date = 1;

		break;

	case ENDING_FOREVER:
		/* Nothing to be done */
		break;

	default:
		g_return_if_reached ();
	}

	cal_comp_util_remove_all_properties (component, ICAL_RRULE_PROPERTY);

	/* Set the recurrence */
	prop = icalproperty_new_rrule (r);
	icalcomponent_add_property (component, prop);
}

static void
ecep_recurrence_sensitize_widgets (ECompEditorPage *page,
				   gboolean force_insensitive)
{
	ECompEditorPageRecurrence *page_recurrence;
	ECompEditor *comp_editor;
	GtkTreeSelection *selection;
	gboolean create_recurrence, any_selected, is_organizer;
	guint32 flags;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page));

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_recurrence_parent_class)->sensitize_widgets (page, force_insensitive);

	comp_editor = e_comp_editor_page_ref_editor (page);
	flags = e_comp_editor_get_flags (comp_editor);
	g_clear_object (&comp_editor);

	page_recurrence = E_COMP_EDITOR_PAGE_RECURRENCE (page);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_recurrence->priv->exceptions_tree_view));

	force_insensitive = force_insensitive || page_recurrence->priv->is_custom;
	create_recurrence = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page_recurrence->priv->recr_check_box));
	any_selected = gtk_tree_selection_count_selected_rows (selection) > 0;
	is_organizer = (flags & (E_COMP_EDITOR_FLAG_IS_NEW | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER)) != 0;
	force_insensitive = force_insensitive || !is_organizer;

	gtk_widget_set_sensitive (page_recurrence->priv->recr_check_box, !force_insensitive);
	gtk_widget_set_sensitive (page_recurrence->priv->recr_hbox, !force_insensitive && create_recurrence);

	gtk_widget_set_sensitive (page_recurrence->priv->exceptions_tree_view, !force_insensitive && create_recurrence);
	gtk_widget_set_sensitive (page_recurrence->priv->exceptions_button_box, !force_insensitive && create_recurrence);
	gtk_widget_set_sensitive (page_recurrence->priv->exceptions_add_button, create_recurrence);
	gtk_widget_set_sensitive (page_recurrence->priv->exceptions_edit_button, any_selected);
	gtk_widget_set_sensitive (page_recurrence->priv->exceptions_remove_button, any_selected);

	if (page_recurrence->priv->is_custom) {
		gtk_widget_hide (page_recurrence->priv->recr_hbox);
		gtk_widget_show (page_recurrence->priv->recr_cannot_edit_label);
	} else {
		gtk_widget_show (page_recurrence->priv->recr_hbox);
		gtk_widget_hide (page_recurrence->priv->recr_cannot_edit_label);
	}

	ecep_recurrence_update_preview (page_recurrence);
}

static void
ecep_recurrence_fill_widgets (ECompEditorPage *page,
			      icalcomponent *component)
{
	ECompEditorPageRecurrence *page_recurrence;
	struct icalrecurrencetype rrule;
	icalproperty *prop;
	GtkAdjustment *adj;
	gint n_by_second, n_by_minute, n_by_hour;
	gint n_by_day, n_by_month_day, n_by_year_day;
	gint n_by_week_no, n_by_month, n_by_set_pos;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page));
	g_return_if_fail (component != NULL);

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_recurrence_parent_class)->fill_widgets (page, component);

	page_recurrence = E_COMP_EDITOR_PAGE_RECURRENCE (page);

	switch (icalcomponent_isa (component)) {
		case ICAL_VEVENT_COMPONENT:
			gtk_button_set_label (GTK_BUTTON (page_recurrence->priv->recr_check_box),
				/* Translators: Entire string is for example:     'This appointment recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
				C_("ECompEditorPageRecur", "This appointment rec_urs"));
			break;
		case ICAL_VTODO_COMPONENT:
			gtk_button_set_label (GTK_BUTTON (page_recurrence->priv->recr_check_box),
				/* Translators: Entire string is for example:     'This task recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
				C_("ECompEditorPageRecur", "This task rec_urs"));
			break;
		case ICAL_VJOURNAL_COMPONENT:
			gtk_button_set_label (GTK_BUTTON (page_recurrence->priv->recr_check_box),
				/* Translators: Entire string is for example:     'This memo recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
				C_("ECompEditorPageRecur", "This memo rec_urs"));
			break;
		default:
			gtk_button_set_label (GTK_BUTTON (page_recurrence->priv->recr_check_box),
				/* Translators: Entire string is for example:     'This component recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
				C_("ECompEditorPageRecur", "This component rec_urs"));
			break;
	}

	/* Clean the page */
	ecep_recurrence_clear_widgets (page_recurrence);
	page_recurrence->priv->is_custom = FALSE;

	/* Exceptions */
	ecep_recurrence_fill_exception_widgets (page_recurrence, component);

	/* Set up defaults for the special widgets */
	ecep_recurrence_set_special_defaults (page_recurrence, component);

	/* No recurrences? */
	if (!icalcomponent_get_first_property (component, ICAL_RDATE_PROPERTY)
	    && !icalcomponent_get_first_property (component, ICAL_RRULE_PROPERTY)
	    && !icalcomponent_get_first_property (component, ICAL_EXRULE_PROPERTY)) {
		g_signal_handlers_block_matched (page_recurrence->priv->recr_check_box, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_recurrence->priv->recr_check_box), FALSE);
		g_signal_handlers_unblock_matched (page_recurrence->priv->recr_check_box, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);

		ecep_recurrence_update_preview (page_recurrence);

		return;
	}

	/* See if it is a custom set we don't support */

	if ((icalcomponent_get_first_property (component, ICAL_RRULE_PROPERTY) &&
	     icalcomponent_get_next_property (component, ICAL_RRULE_PROPERTY)) ||
	    icalcomponent_get_first_property (component, ICAL_RDATE_PROPERTY) ||
	    icalcomponent_get_first_property (component, ICAL_EXRULE_PROPERTY))
		goto custom;

	/* Down to one rule, so test that one */

	prop = icalcomponent_get_first_property (component, ICAL_RRULE_PROPERTY);
	g_return_if_fail (prop != NULL);

	rrule = icalproperty_get_rrule (prop);

	/* Any lower frequency? */

	if (rrule.freq == ICAL_SECONDLY_RECURRENCE
	    || rrule.freq == ICAL_MINUTELY_RECURRENCE
	    || rrule.freq == ICAL_HOURLY_RECURRENCE)
		goto custom;

	/* Any unusual values? */

#define N_HAS_BY(field) (ecep_recurrence_count_by_xxx (field, G_N_ELEMENTS (field)))

	n_by_second = N_HAS_BY (rrule.by_second);
	n_by_minute = N_HAS_BY (rrule.by_minute);
	n_by_hour = N_HAS_BY (rrule.by_hour);
	n_by_day = N_HAS_BY (rrule.by_day);
	n_by_month_day = N_HAS_BY (rrule.by_month_day);
	n_by_year_day = N_HAS_BY (rrule.by_year_day);
	n_by_week_no = N_HAS_BY (rrule.by_week_no);
	n_by_month = N_HAS_BY (rrule.by_month);
	n_by_set_pos = N_HAS_BY (rrule.by_set_pos);

	if (n_by_second != 0
	    || n_by_minute != 0
	    || n_by_hour != 0)
		goto custom;

	/* Filter the funky shit based on the frequency; if there is nothing
	 * weird we can actually set the widgets.
	 */

	switch (rrule.freq) {
	case ICAL_DAILY_RECURRENCE:
		if (n_by_day != 0
		    || n_by_month_day != 0
		    || n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto custom;

		g_signal_handlers_block_matched (page_recurrence->priv->recr_interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
		e_dialog_combo_box_set (
			page_recurrence->priv->recr_interval_unit_combo,
			ICAL_DAILY_RECURRENCE,
			freq_map);
		g_signal_handlers_unblock_matched (page_recurrence->priv->recr_interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
		break;

	case ICAL_WEEKLY_RECURRENCE: {
		gint ii;
		guint8 day_mask;

		if (n_by_month_day != 0
		    || n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto custom;

		day_mask = 0;

		for (ii = 0; ii < 8 && rrule.by_day[ii] != ICAL_RECURRENCE_ARRAY_MAX; ii++) {
			enum icalrecurrencetype_weekday weekday;
			gint pos;

			weekday = icalrecurrencetype_day_day_of_week (rrule.by_day[ii]);
			pos = icalrecurrencetype_day_position (rrule.by_day[ii]);

			if (pos != 0)
				goto custom;

			switch (weekday) {
			case ICAL_SUNDAY_WEEKDAY:
				day_mask |= 1 << 0;
				break;

			case ICAL_MONDAY_WEEKDAY:
				day_mask |= 1 << 1;
				break;

			case ICAL_TUESDAY_WEEKDAY:
				day_mask |= 1 << 2;
				break;

			case ICAL_WEDNESDAY_WEEKDAY:
				day_mask |= 1 << 3;
				break;

			case ICAL_THURSDAY_WEEKDAY:
				day_mask |= 1 << 4;
				break;

			case ICAL_FRIDAY_WEEKDAY:
				day_mask |= 1 << 5;
				break;

			case ICAL_SATURDAY_WEEKDAY:
				day_mask |= 1 << 6;
				break;

			default:
				break;
			}
		}

		page_recurrence->priv->weekday_day_mask = day_mask;

		g_signal_handlers_block_matched (page_recurrence->priv->recr_interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
		e_dialog_combo_box_set (
			page_recurrence->priv->recr_interval_unit_combo,
			ICAL_WEEKLY_RECURRENCE,
			freq_map);
		g_signal_handlers_unblock_matched (page_recurrence->priv->recr_interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
		break;
	}

	case ICAL_MONTHLY_RECURRENCE:
		if (n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos > 1)
			goto custom;

		if (n_by_month_day == 1) {
			gint nth;

			if (n_by_set_pos != 0)
				goto custom;

			nth = rrule.by_month_day[0];
			if (nth < 1 && nth != -1)
				goto custom;

			if (nth == -1) {
				struct icaltimetype dtstart;

				dtstart = icalcomponent_get_dtstart (component);

				page_recurrence->priv->month_index = dtstart.day;
				page_recurrence->priv->month_num = MONTH_NUM_LAST;
			} else {
				page_recurrence->priv->month_index = nth;
				page_recurrence->priv->month_num = MONTH_NUM_DAY;
			}
			page_recurrence->priv->month_day = MONTH_DAY_NTH;

		} else if (n_by_day == 1) {
			enum icalrecurrencetype_weekday weekday;
			gint pos;
			enum month_day_options month_day;

			/* Outlook 2000 uses BYDAY=TU;BYSETPOS=2, and will not
			 * accept BYDAY=2TU. So we now use the same as Outlook
			 * by default. */

			weekday = icalrecurrencetype_day_day_of_week (rrule.by_day[0]);
			pos = icalrecurrencetype_day_position (rrule.by_day[0]);

			if (pos == 0) {
				if (n_by_set_pos != 1)
					goto custom;
				pos = rrule.by_set_pos[0];
			} else if (pos < 0) {
				goto custom;
			}

			switch (weekday) {
			case ICAL_MONDAY_WEEKDAY:
				month_day = MONTH_DAY_MON;
				break;

			case ICAL_TUESDAY_WEEKDAY:
				month_day = MONTH_DAY_TUE;
				break;

			case ICAL_WEDNESDAY_WEEKDAY:
				month_day = MONTH_DAY_WED;
				break;

			case ICAL_THURSDAY_WEEKDAY:
				month_day = MONTH_DAY_THU;
				break;

			case ICAL_FRIDAY_WEEKDAY:
				month_day = MONTH_DAY_FRI;
				break;

			case ICAL_SATURDAY_WEEKDAY:
				month_day = MONTH_DAY_SAT;
				break;

			case ICAL_SUNDAY_WEEKDAY:
				month_day = MONTH_DAY_SUN;
				break;

			default:
				goto custom;
			}

			if (pos == -1)
				page_recurrence->priv->month_num = MONTH_NUM_LAST;
			else
				page_recurrence->priv->month_num = pos - 1;
			page_recurrence->priv->month_day = month_day;
		} else
			goto custom;

		g_signal_handlers_block_matched (page_recurrence->priv->recr_interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
		e_dialog_combo_box_set (
			page_recurrence->priv->recr_interval_unit_combo,
			ICAL_MONTHLY_RECURRENCE,
			freq_map);
		g_signal_handlers_unblock_matched (page_recurrence->priv->recr_interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
		break;

	case ICAL_YEARLY_RECURRENCE:
		if (n_by_day != 0
		    || n_by_month_day != 0
		    || n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto custom;

		g_signal_handlers_block_matched (page_recurrence->priv->recr_interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
		e_dialog_combo_box_set (
			page_recurrence->priv->recr_interval_unit_combo,
			ICAL_YEARLY_RECURRENCE,
			freq_map);
		g_signal_handlers_unblock_matched (page_recurrence->priv->recr_interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
		break;

	default:
		goto custom;
	}

	/* If we got here it means it is a simple recurrence */

	g_signal_handlers_block_matched (page_recurrence->priv->recr_check_box, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_recurrence->priv->recr_check_box), TRUE);
	g_signal_handlers_unblock_matched (page_recurrence->priv->recr_check_box, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);

	ecep_recurrence_make_recurrence_special (page_recurrence);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (page_recurrence->priv->recr_interval_value_spin));
	g_signal_handlers_block_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_recurrence->priv->recr_interval_value_spin), rrule.interval);
	g_signal_handlers_unblock_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);

	ecep_recurrence_fill_ending_date (page_recurrence, &rrule, component);

	return;

 custom:

	g_signal_handlers_block_matched (page_recurrence->priv->recr_check_box, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
	page_recurrence->priv->is_custom = TRUE;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_recurrence->priv->recr_check_box), TRUE);
	g_signal_handlers_unblock_matched (page_recurrence->priv->recr_check_box, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page_recurrence);
}

static gboolean
ecep_recurrence_fill_component (ECompEditorPage *page,
				icalcomponent *component)
{
	ECompEditorPageRecurrence *page_recurrence;
	ECompEditor *comp_editor;
	GtkTreeModel *model;
	GtkTreeIter iter;
	icalproperty *prop;
	gboolean recurs;
	gboolean valid_iter;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page), FALSE);
	g_return_val_if_fail (component != NULL, FALSE);

	page_recurrence = E_COMP_EDITOR_PAGE_RECURRENCE (page);

	recurs = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page_recurrence->priv->recr_check_box));

	if (recurs && page_recurrence->priv->is_custom) {
		/* We just keep whatever the component has currently */
		return TRUE;
	} else if (recurs) {
		cal_comp_util_remove_all_properties (component, ICAL_RRULE_PROPERTY);
		cal_comp_util_remove_all_properties (component, ICAL_RDATE_PROPERTY);
		cal_comp_util_remove_all_properties (component, ICAL_EXRULE_PROPERTY);
		cal_comp_util_remove_all_properties (component, ICAL_EXDATE_PROPERTY);
		ecep_recurrence_simple_recur_to_comp (page_recurrence, component);
	} else {
		gboolean had_recurrences = e_cal_util_component_has_recurrences (component);

		cal_comp_util_remove_all_properties (component, ICAL_RRULE_PROPERTY);
		cal_comp_util_remove_all_properties (component, ICAL_RDATE_PROPERTY);
		cal_comp_util_remove_all_properties (component, ICAL_EXRULE_PROPERTY);
		cal_comp_util_remove_all_properties (component, ICAL_EXDATE_PROPERTY);

		if (had_recurrences)
			cal_comp_util_remove_all_properties (component, ICAL_RECURRENCEID_PROPERTY);

		return TRUE;
	}

	comp_editor = e_comp_editor_page_ref_editor (page);

	/* Set exceptions */

	model = GTK_TREE_MODEL (page_recurrence->priv->exceptions_store);

	for (valid_iter = gtk_tree_model_get_iter_first (model, &iter); valid_iter;
	     valid_iter = gtk_tree_model_iter_next (model, &iter)) {
		const icaltimetype *dt;

		dt = e_date_time_list_get_date_time (E_DATE_TIME_LIST (model), &iter);
		g_return_val_if_fail (dt != NULL, FALSE);

		if (!icaltime_is_valid_time (*dt)) {
			e_comp_editor_set_validation_error (comp_editor,
				page, page_recurrence->priv->exceptions_tree_view,
				_("Recurrence exception date is invalid"));
			g_clear_object (&comp_editor);
			return FALSE;
		}

		prop = icalproperty_new_exdate (*dt);
		cal_comp_util_update_tzid_parameter (prop, *dt);

		icalcomponent_add_property (component, prop);
	}

	if (gtk_widget_get_visible (page_recurrence->priv->recr_ending_combo) &&
	    gtk_widget_get_sensitive (page_recurrence->priv->recr_ending_combo) &&
	    e_dialog_combo_box_get (page_recurrence->priv->recr_ending_combo, ending_types_map) == ENDING_UNTIL) {
		/* check whether the "until" date is in the future */
		struct icaltimetype tt = icaltime_null_time ();
		gboolean ok = TRUE;

		if (e_date_edit_get_date (E_DATE_EDIT (page_recurrence->priv->ending_date_edit), &tt.year, &tt.month, &tt.day)) {
			ECompEditorPropertyPart *dtstart_part = NULL;
			struct icaltimetype dtstart = icaltime_null_time ();

			e_comp_editor_get_time_parts (comp_editor, &dtstart_part, NULL);
			if (dtstart_part) {
				dtstart = e_comp_editor_property_part_datetime_get_value (
					E_COMP_EDITOR_PROPERTY_PART_DATETIME (dtstart_part));
			}

			tt.is_date = 1;
			tt.zone = NULL;

			if (icaltime_is_valid_time (dtstart)) {
				ok = icaltime_compare_date_only (dtstart, tt) <= 0;

				if (!ok) {
					e_date_edit_set_date (E_DATE_EDIT (page_recurrence->priv->ending_date_edit),
						dtstart.year, dtstart.month, dtstart.day);
				} else {
					/* to have the date shown in "normalized" format */
					e_date_edit_set_date (E_DATE_EDIT (page_recurrence->priv->ending_date_edit),
						tt.year, tt.month, tt.day);
				}
			}
		}

		if (!ok) {
			e_comp_editor_set_validation_error (comp_editor,
				page, page_recurrence->priv->ending_date_edit,
				_("End time of the recurrence is before the start"));
			g_clear_object (&comp_editor);

			return FALSE;
		}
	}

	g_clear_object (&comp_editor);

	return E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_recurrence_parent_class)->fill_component (page, component);
}

static void
ecep_recurrence_select_page_cb (GtkAction *action,
				ECompEditorPage *page)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page));

	e_comp_editor_page_select (page);
}

static void
ecep_recurrence_setup_ui (ECompEditorPageRecurrence *page_recurrence)
{
	const gchar *ui =
		"<ui>"
		"  <menubar action='main-menu'>"
		"    <menu action='options-menu'>"
		"      <placeholder name='tabs'>"
		"        <menuitem action='page-recurrence'/>"
		"      </placeholder>"
		"    </menu>"
		"  </menubar>"
		"  <toolbar name='main-toolbar'>"
		"    <placeholder name='content'>\n"
		"      <toolitem action='page-recurrence'/>\n"
		"    </placeholder>"
		"  </toolbar>"
		"</ui>";

	const GtkActionEntry options_actions[] = {
		{ "page-recurrence",
		  "stock_task-recurring",
		  N_("R_ecurrence"),
		  NULL,
		  N_("Set or unset recurrence"),
		  G_CALLBACK (ecep_recurrence_select_page_cb) }
	};

	ECompEditor *comp_editor;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GError *error = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_RECURRENCE (page_recurrence));

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_recurrence));
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);
	action_group = e_comp_editor_get_action_group (comp_editor, "individual");

	gtk_action_group_add_actions (action_group,
		options_actions, G_N_ELEMENTS (options_actions), page_recurrence);

	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	g_clear_object (&comp_editor);

	if (error) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

static void
ecep_recurrence_constructed (GObject *object)
{
	ECompEditorPageRecurrence *page_recurrence;
	ECompEditor *comp_editor;
	GtkWidget *widget, *container;
	GtkComboBoxText *text_combo;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;
	PangoAttrList *bold;
	GtkGrid *grid;
	ECalendar *ecal;

	G_OBJECT_CLASS (e_comp_editor_page_recurrence_parent_class)->constructed (object);

	page_recurrence = E_COMP_EDITOR_PAGE_RECURRENCE (object);
	grid = GTK_GRID (page_recurrence);

	bold = pango_attr_list_new ();
	pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

	widget = gtk_label_new (_("Recurrence"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"attributes", bold,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 0, 2, 1);

	/* Translators: Entire string is for example:     'This appointment recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
	widget = gtk_check_button_new_with_mnemonic (C_("ECompEditorPageRecur", "This appointment rec_urs"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-start", 12,
		"margin-bottom", 6,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 1, 2, 1);
	page_recurrence->priv->recr_check_box = widget;

	g_signal_connect (page_recurrence->priv->recr_check_box, "toggled",
		G_CALLBACK (ecep_recurrence_checkbox_toggled_cb), page_recurrence);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-start", 12,
		"margin-bottom", 6,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 2, 2, 1);
	page_recurrence->priv->recr_hbox = widget;

	container = page_recurrence->priv->recr_hbox;

	/* Translators: Entire string is for example:     'This appointment recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
	widget = gtk_label_new (C_("ECompEditorPageRecur", "Every"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	widget = gtk_spin_button_new_with_range (1, 999, 1);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"digits", 0,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_recurrence->priv->recr_interval_value_spin = widget;

	widget = gtk_combo_box_text_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_recurrence->priv->recr_interval_unit_combo = widget;

	text_combo = GTK_COMBO_BOX_TEXT (widget);
	/* Translators: Entire string is for example:     'This appointment recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
	gtk_combo_box_text_append_text (text_combo, C_("ECompEditorPageRecur", "day(s)"));
	/* Translators: Entire string is for example:     'This appointment recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
	gtk_combo_box_text_append_text (text_combo, C_("ECompEditorPageRecur", "week(s)"));
	/* Translators: Entire string is for example:     'This appointment recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
	gtk_combo_box_text_append_text (text_combo, C_("ECompEditorPageRecur", "month(s)"));
	/* Translators: Entire string is for example:     'This appointment recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
	gtk_combo_box_text_append_text (text_combo, C_("ECompEditorPageRecur", "year(s)"));

	g_signal_connect_swapped (page_recurrence->priv->recr_interval_unit_combo, "changed",
		G_CALLBACK (ecep_recurrence_make_recurrence_special), page_recurrence);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_recurrence->priv->recr_interval_special_box = widget;

	widget = gtk_combo_box_text_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_recurrence->priv->recr_ending_combo = widget;

	text_combo = GTK_COMBO_BOX_TEXT (widget);
	/* Translators: Entire string is for example:     'This appointment recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
	gtk_combo_box_text_append_text (text_combo, C_("ECompEditorPageRecur", "for"));
	/* Translators: Entire string is for example:     'This appointment recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
	gtk_combo_box_text_append_text (text_combo, C_("ECompEditorPageRecur", "until"));
	/* Translators: Entire string is for example:     'This appointment recurs/Every[x][day(s)][for][1]occurrences' (combobox options are in [square brackets]) */
	gtk_combo_box_text_append_text (text_combo, C_("ECompEditorPageRecur", "forever"));

	g_signal_connect_swapped (page_recurrence->priv->recr_ending_combo, "changed",
		G_CALLBACK (ecep_recurrence_make_ending_special), page_recurrence);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_recurrence->priv->recr_ending_special_box = widget;

	widget = gtk_label_new (_("This appointment contains recurrences that Evolution cannot edit."));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-start", 12,
		"margin-bottom", 6,
		NULL);
	gtk_widget_hide (widget);
	gtk_grid_attach (grid, widget, 0, 3, 2, 1);
	page_recurrence->priv->recr_cannot_edit_label = widget;

	widget = gtk_label_new (_("Exceptions"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"attributes", bold,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 4, 2, 1);

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		"margin-start", 12,
		"margin-bottom", 6,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_IN,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 5, 1, 1);

	container = widget;

	page_recurrence->priv->exceptions_store = e_date_time_list_new ();

	widget = gtk_tree_view_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"model", page_recurrence->priv->exceptions_store,
		"headers-visible", FALSE,
		NULL);
	gtk_widget_show (widget);

	gtk_container_add (GTK_CONTAINER (container), widget);
	page_recurrence->priv->exceptions_tree_view = widget;

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, "Date/Time");
	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", E_DATE_TIME_LIST_COLUMN_DESCRIPTION);
	gtk_tree_view_append_column (GTK_TREE_VIEW (page_recurrence->priv->exceptions_tree_view), column);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (page_recurrence->priv->exceptions_tree_view)),
		"changed", G_CALLBACK (ecep_recurrence_exceptions_selection_changed_cb), page_recurrence);

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 1, 5, 1, 1);
	page_recurrence->priv->exceptions_button_box = widget;

	widget = gtk_button_new_with_mnemonic (_("A_dd"));
	gtk_box_pack_start (GTK_BOX (page_recurrence->priv->exceptions_button_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	page_recurrence->priv->exceptions_add_button = widget;

	g_signal_connect (page_recurrence->priv->exceptions_add_button, "clicked",
		G_CALLBACK (ecep_recurrence_exceptions_add_clicked_cb), page_recurrence);

	widget = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_box_pack_start (GTK_BOX (page_recurrence->priv->exceptions_button_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	page_recurrence->priv->exceptions_edit_button = widget;

	g_signal_connect (page_recurrence->priv->exceptions_edit_button, "clicked",
		G_CALLBACK (ecep_recurrence_exceptions_edit_clicked_cb), page_recurrence);

	widget = gtk_button_new_with_mnemonic (_("Re_move"));
	gtk_box_pack_start (GTK_BOX (page_recurrence->priv->exceptions_button_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	page_recurrence->priv->exceptions_remove_button = widget;

	g_signal_connect (page_recurrence->priv->exceptions_remove_button, "clicked",
		G_CALLBACK (ecep_recurrence_exceptions_remove_clicked_cb), page_recurrence);

	widget = gtk_label_new (_("Preview"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"attributes", bold,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 6, 2, 1);

	widget = e_calendar_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"margin-start", 12,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 7, 2, 1);
	page_recurrence->priv->preview = widget;

	pango_attr_list_unref (bold);

	ecal = E_CALENDAR (page_recurrence->priv->preview);
	g_signal_connect_swapped (
		e_calendar_get_item (ecal), "date-range-changed",
		G_CALLBACK (ecep_recurrence_update_preview), page_recurrence);
	e_calendar_item_set_max_days_sel (e_calendar_get_item (ecal), 0);
	e_calendar_item_set_get_time_callback (e_calendar_get_item (ecal), ecep_recurrence_get_current_time_cb, NULL, NULL);

	g_signal_connect_swapped (page_recurrence->priv->recr_interval_value_spin, "value-changed",
		G_CALLBACK (ecep_recurrence_changed), page_recurrence);
	g_signal_connect_swapped (page_recurrence->priv->recr_interval_unit_combo, "changed",
		G_CALLBACK (ecep_recurrence_changed), page_recurrence);
	g_signal_connect_swapped (page_recurrence->priv->recr_ending_combo, "changed",
		G_CALLBACK (ecep_recurrence_changed), page_recurrence);

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_recurrence));
	if (comp_editor) {
		g_signal_connect_swapped (comp_editor, "times-changed",
			G_CALLBACK (ecep_recurrence_update_preview), page_recurrence);
		g_clear_object (&comp_editor);
	}

	ecep_recurrence_setup_ui (page_recurrence);
}

static void
ecep_recurrence_dispose (GObject *object)
{
	ECompEditorPageRecurrence *page_recurrence;
	ECompEditor *comp_editor;

	page_recurrence = E_COMP_EDITOR_PAGE_RECURRENCE (object);

	if (page_recurrence->priv->cancellable) {
		g_cancellable_cancel (page_recurrence->priv->cancellable);
		g_clear_object (&page_recurrence->priv->cancellable);
	}

	g_clear_object (&page_recurrence->priv->exceptions_store);

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_recurrence));
	if (comp_editor) {
		g_signal_handlers_disconnect_by_func (comp_editor,
			G_CALLBACK (ecep_recurrence_update_preview), page_recurrence);
		g_clear_object (&comp_editor);
	}

	G_OBJECT_CLASS (e_comp_editor_page_recurrence_parent_class)->dispose (object);
}

static void
e_comp_editor_page_recurrence_init (ECompEditorPageRecurrence *page_recurrence)
{
	page_recurrence->priv = G_TYPE_INSTANCE_GET_PRIVATE (page_recurrence,
		E_TYPE_COMP_EDITOR_PAGE_RECURRENCE,
		ECompEditorPageRecurrencePrivate);

	page_recurrence->priv->cancellable = g_cancellable_new ();
}

static void
e_comp_editor_page_recurrence_class_init (ECompEditorPageRecurrenceClass *klass)
{
	ECompEditorPageClass *page_class;
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (ECompEditorPageRecurrencePrivate));

	page_class = E_COMP_EDITOR_PAGE_CLASS (klass);
	page_class->sensitize_widgets = ecep_recurrence_sensitize_widgets;
	page_class->fill_widgets = ecep_recurrence_fill_widgets;
	page_class->fill_component = ecep_recurrence_fill_component;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = ecep_recurrence_constructed;
	object_class->dispose = ecep_recurrence_dispose;
}

ECompEditorPage *
e_comp_editor_page_recurrence_new (ECompEditor *editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (editor), NULL);

	return g_object_new (E_TYPE_COMP_EDITOR_PAGE_RECURRENCE,
		"editor", editor,
		NULL);
}
