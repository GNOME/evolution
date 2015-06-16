/*
 * Evolution calendar - Scheduling page
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
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Miguel de Icaza <miguel@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "../e-meeting-time-sel.h"
#include "../itip-utils.h"
#include "comp-editor-util.h"
#include "e-delegate-dialog.h"
#include "schedule-page.h"

#define SCHEDULE_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_SCHEDULE_PAGE, SchedulePagePrivate))

/* Private part of the SchedulePage structure */
struct _SchedulePagePrivate {
	GtkBuilder *builder;

	/* Widgets from the UI file */
	GtkWidget *main;

	/* Model */
	EMeetingStore *model;

	/* Selector */
	EMeetingTimeSelector *sel;

	/* The timezone we use. Note that we use the same timezone for the
	 * start and end date. We convert the end date if it is passed in in
	 * another timezone. */
	icaltimezone *zone;
};

static void times_changed_cb (GtkWidget *widget, SchedulePage *spage);

G_DEFINE_TYPE (SchedulePage, schedule_page, TYPE_COMP_EDITOR_PAGE)

static void
sensitize_widgets (SchedulePage *spage)
{
	SchedulePagePrivate *priv = spage->priv;
	CompEditor *editor;
	ECalClient *client;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (spage));
	client = comp_editor_get_client (editor);

	e_meeting_time_selector_set_read_only (
		priv->sel, e_client_is_readonly (E_CLIENT (client)));
}

/* Set date/time */
static void
update_time (SchedulePage *spage,
             ECalComponentDateTime *start_date,
             ECalComponentDateTime *end_date)
{
	SchedulePagePrivate *priv = spage->priv;
	CompEditor *editor;
	struct icaltimetype start_tt, end_tt;
	icaltimezone *start_zone = NULL, *end_zone = NULL;
	ECalClient *client;
	gboolean all_day;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (spage));
	client = comp_editor_get_client (editor);

	if (start_date->tzid) {
		/* Note that if we are creating a new event, the timezones may not be
		 * on the server, so we try to get the builtin timezone with the TZID
		 * first. */
		start_zone = icaltimezone_get_builtin_timezone_from_tzid (start_date->tzid);
		if (!start_zone) {
			GError *error = NULL;

			e_cal_client_get_timezone_sync (
				client, start_date->tzid,
				&start_zone, NULL, &error);

			if (error != NULL) {
				/* FIXME: Handle error better. */
				g_warning (
					"Couldn't get timezone '%s' from server: %s",
					start_date->tzid ? start_date->tzid : "",
					error->message);
				g_error_free (error);
			}
		}
	}

	if (end_date->tzid) {
		end_zone = icaltimezone_get_builtin_timezone_from_tzid (end_date->tzid);
		if (!end_zone) {
			GError *error = NULL;

			e_cal_client_get_timezone_sync (
				client, end_date->tzid,
				&end_zone, NULL, &error);

			if (error != NULL) {
				/* FIXME: Handle error better. */
				g_warning (
					"Couldn't get timezone '%s' from server: %s",
					end_date->tzid ? end_date->tzid : "",
					error->message);
				g_error_free (error);
			}
		}
	}

	start_tt = *start_date->value;
	if (!end_date->value && start_tt.is_date) {
		end_tt = start_tt;
		icaltime_adjust (&end_tt, 1, 0, 0, 0);
	} else if (!end_date->value) {
		end_tt = start_tt;
	} else {
		end_tt = *end_date->value;
	}

	/* If the end zone is not the same as the start zone, we convert it. */
	priv->zone = start_zone;
	if (start_zone != end_zone) {
		icaltimezone_convert_time (&end_tt, end_zone, start_zone);
	}
	e_meeting_store_set_timezone (priv->model, priv->zone);

	all_day = (start_tt.is_date && end_tt.is_date) ? TRUE : FALSE;

	/* For All Day Events, if DTEND is after DTSTART, we subtract 1 day
	 * from it. */
	if (all_day) {
		if (icaltime_compare_date_only (end_tt, start_tt) > 0) {
			icaltime_adjust (&end_tt, -1, 0, 0, 0);
		}
	}

	e_date_edit_set_date (
		E_DATE_EDIT (priv->sel->start_date_edit), start_tt.year,
		start_tt.month, start_tt.day);
	e_date_edit_set_time_of_day (
		E_DATE_EDIT (priv->sel->start_date_edit),
		start_tt.hour, start_tt.minute);

	e_date_edit_set_date (
		E_DATE_EDIT (priv->sel->end_date_edit), end_tt.year,
		end_tt.month, end_tt.day);
	e_date_edit_set_time_of_day (
		E_DATE_EDIT (priv->sel->end_date_edit),
		end_tt.hour, end_tt.minute);

}

static void
schedule_page_dispose (GObject *object)
{
	SchedulePagePrivate *priv;

	priv = SCHEDULE_PAGE_GET_PRIVATE (object);

	if (priv->main != NULL) {
		g_object_unref (priv->main);
		priv->main = NULL;
	}

	if (priv->builder != NULL) {
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}

	if (priv->model != NULL) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (schedule_page_parent_class)->dispose (object);
}

static GtkWidget *
schedule_page_get_widget (CompEditorPage *page)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;

	spage = SCHEDULE_PAGE (page);
	priv = spage->priv;

	return priv->main;
}

static void
schedule_page_focus_main_widget (CompEditorPage *page)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;

	spage = SCHEDULE_PAGE (page);
	priv = spage->priv;

	gtk_widget_grab_focus (GTK_WIDGET (priv->sel));
}

static gboolean
schedule_page_fill_widgets (CompEditorPage *page,
                            ECalComponent *comp)
{
	SchedulePage *spage;
	ECalComponentDateTime start_date, end_date;
	gboolean validated = TRUE;

	spage = SCHEDULE_PAGE (page);

	/* Start and end times */
	e_cal_component_get_dtstart (comp, &start_date);
	e_cal_component_get_dtend (comp, &end_date);
	if (!start_date.value)
		validated = FALSE;
	else if (!end_date.value)
		validated = FALSE;
	else
		update_time (spage, &start_date, &end_date);

	e_cal_component_free_datetime (&start_date);
	e_cal_component_free_datetime (&end_date);

	sensitize_widgets (spage);

	return validated;
}

static gboolean
schedule_page_fill_component (CompEditorPage *page,
                              ECalComponent *comp)
{
	return TRUE;
}

static void
schedule_page_set_dates (CompEditorPage *page,
                         CompEditorPageDates *dates)
{
	SchedulePage *spage;

	spage = SCHEDULE_PAGE (page);

	comp_editor_page_set_updating (page, TRUE);
	update_time (spage, dates->start, dates->end);
	comp_editor_page_set_updating (page, FALSE);
}

static void
schedule_page_class_init (SchedulePageClass *class)
{
	GObjectClass *object_class;
	CompEditorPageClass *editor_page_class;

	g_type_class_add_private (class, sizeof (SchedulePagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = schedule_page_dispose;

	editor_page_class = COMP_EDITOR_PAGE_CLASS (class);
	editor_page_class->get_widget = schedule_page_get_widget;
	editor_page_class->focus_main_widget = schedule_page_focus_main_widget;
	editor_page_class->fill_widgets = schedule_page_fill_widgets;
	editor_page_class->fill_component = schedule_page_fill_component;
	editor_page_class->set_dates = schedule_page_set_dates;
}

static void
schedule_page_init (SchedulePage *spage)
{
	spage->priv = SCHEDULE_PAGE_GET_PRIVATE (spage);
}

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (SchedulePage *spage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (spage);
	SchedulePagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;
	GtkWidget *parent;

	priv = spage->priv;

#define GW(name) e_builder_get_widget (priv->builder, name)

	priv->main = GW ("schedule-page");
	if (!priv->main)
		return FALSE;

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	 * it when the notebook page is mapped. */
	toplevel = gtk_widget_get_toplevel (priv->main);
	accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
	if (accel_groups)
		page->accel_group = g_object_ref (accel_groups->data);

	g_object_ref (priv->main);
	parent = gtk_widget_get_parent (priv->main);
	gtk_container_remove (GTK_CONTAINER (parent), priv->main);

#undef GW

	return TRUE;
}

static gboolean
init_widgets (SchedulePage *spage)
{
	SchedulePagePrivate *priv;

	priv = spage->priv;

	g_signal_connect (
		priv->sel, "changed",
		G_CALLBACK (times_changed_cb), spage);

	return TRUE;
}

void
schedule_page_set_meeting_time (SchedulePage *spage,
                                icaltimetype *start_tt,
                                icaltimetype *end_tt)
{
	SchedulePagePrivate *priv;
	gboolean all_day;

	priv = spage->priv;

	all_day = (start_tt->is_date && end_tt->is_date) ? TRUE : FALSE;

	if (all_day) {
		if (icaltime_compare_date_only (*end_tt, *start_tt) > 0) {
			icaltime_adjust (end_tt, -1, 0, 0, 0);
		}
	}

	e_meeting_time_selector_set_meeting_time (
		priv->sel, start_tt->year, start_tt->month,
		start_tt->day, start_tt->hour, start_tt->minute,
		end_tt->year, end_tt->month, end_tt->day,
		end_tt->hour, end_tt->minute);
	e_meeting_time_selector_set_all_day (priv->sel, all_day);

}

/**
 * schedule_page_construct:
 * @spage: An schedule page.
 *
 * Constructs an schedule page by loading its Glade data.
 *
 * Return value: The same object as @spage, or NULL if the widgets could not
 * be created.
 **/
SchedulePage *
schedule_page_construct (SchedulePage *spage,
                         EMeetingStore *ems)
{
	SchedulePagePrivate *priv = spage->priv;
	CompEditor *editor;
	gint weekday;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (spage));

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (priv->builder, "schedule-page.ui");

	if (!get_widgets (spage)) {
		g_message (
			"schedule_page_construct(): "
			"Could not find all widgets in the XML file!");
		return NULL;
	}

	/* Model */
	g_object_ref (ems);
	priv->model = ems;

	/* Selector */
	priv->sel = E_MEETING_TIME_SELECTOR (e_meeting_time_selector_new (ems));
	gtk_widget_set_size_request ((GtkWidget *) priv->sel, -1, 400);

	for (weekday = G_DATE_BAD_WEEKDAY; weekday <= G_DATE_SUNDAY; weekday++) {
		gint start_hour, start_minute, end_hour, end_minute;

		comp_editor_get_work_day_range_for (editor, weekday,
			&start_hour, &start_minute, &end_hour, &end_minute);

		e_meeting_time_selector_set_working_hours (priv->sel, weekday,
			start_hour, start_minute, end_hour, end_minute);
	}

	gtk_widget_show (GTK_WIDGET (priv->sel));
	gtk_box_pack_start (GTK_BOX (priv->main), GTK_WIDGET (priv->sel), TRUE, TRUE, 6);

	if (!init_widgets (spage)) {
		g_message (
			"schedule_page_construct(): "
			"Could not initialize the widgets!");
		return NULL;
	}

	e_signal_connect_notify_swapped (
		editor, "notify::client",
		G_CALLBACK (sensitize_widgets), spage);

	return spage;
}

/**
 * schedule_page_new:
 *
 * Creates a new schedule page.
 *
 * Return value: A newly-created schedule page, or NULL if the page could
 * not be created.
 **/
SchedulePage *
schedule_page_new (EMeetingStore *ems,
                   CompEditor *editor)
{
	SchedulePage *spage;

	spage = g_object_new (TYPE_SCHEDULE_PAGE, "editor", editor, NULL);
	if (!schedule_page_construct (spage, ems)) {
		g_object_unref (spage);
		g_return_val_if_reached (NULL);
	}

	return spage;
}

void
schedule_page_update_free_busy (SchedulePage *spage)
{
	SchedulePagePrivate *priv;

	g_return_if_fail (spage != NULL);
	g_return_if_fail (IS_SCHEDULE_PAGE (spage));

	priv = spage->priv;

	e_meeting_time_selector_refresh_free_busy (priv->sel, 0, TRUE);
}

void
schedule_page_set_name_selector (SchedulePage *spage,
                                 ENameSelector *name_selector)
{
	SchedulePagePrivate *priv;

	g_return_if_fail (spage != NULL);
	g_return_if_fail (IS_SCHEDULE_PAGE (spage));

	priv = spage->priv;

	e_meeting_list_view_set_name_selector (priv->sel->list_view, name_selector);
}

static void
times_changed_cb (GtkWidget *widget,
                  SchedulePage *spage)
{
	SchedulePagePrivate *priv;
	CompEditorPageDates dates;
	CompEditor *editor;
	ECalComponentDateTime start_dt, end_dt;
	struct icaltimetype start_tt = icaltime_null_time ();
	struct icaltimetype end_tt = icaltime_null_time ();

	priv = spage->priv;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (spage)))
		return;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (spage));

	e_date_edit_get_date (
		E_DATE_EDIT (priv->sel->start_date_edit),
		&start_tt.year,
		&start_tt.month,
		&start_tt.day);
	e_date_edit_get_time_of_day (
		E_DATE_EDIT (priv->sel->start_date_edit),
		&start_tt.hour,
		&start_tt.minute);
	e_date_edit_get_date (
		E_DATE_EDIT (priv->sel->end_date_edit),
		&end_tt.year,
		&end_tt.month,
		&end_tt.day);
	e_date_edit_get_time_of_day (
		E_DATE_EDIT (priv->sel->end_date_edit),
		&end_tt.hour,
		&end_tt.minute);

	start_dt.value = &start_tt;
	end_dt.value = &end_tt;

	if (e_date_edit_get_show_time (E_DATE_EDIT (priv->sel->start_date_edit))) {
		/* We set the start and end to the same timezone. */
		start_dt.tzid = icaltimezone_get_tzid (priv->zone);
		end_dt.tzid = start_dt.tzid;
	} else {
		/* For All-Day Events, we set the timezone to NULL, and add
		 * 1 day to DTEND. */
		start_dt.value->is_date = TRUE;
		start_dt.tzid = NULL;
		end_dt.value->is_date = TRUE;
		icaltime_adjust (&end_tt, 1, 0, 0, 0);
		end_dt.tzid = NULL;
	}

	dates.start = &start_dt;
	dates.end = &end_dt;
	dates.due = NULL;
	dates.complete = NULL;

	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (spage), &dates);
	comp_editor_set_changed (editor, TRUE);
}
