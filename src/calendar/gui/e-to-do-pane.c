/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
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
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libedataserverui/libedataserverui.h>
#include <libecal/libecal.h>

#include "comp-util.h"
#include "e-cal-data-model.h"
#include "e-cal-data-model-subscriber.h"
#include "e-cal-dialogs.h"
#include "e-cal-ops.h"
#include "itip-utils.h"

#include "e-to-do-pane.h"

#define MAX_TOOLTIP_DESCRIPTION_LEN 128

struct _EToDoPanePrivate {
	GWeakRef shell_view_weakref; /* EShellView * */
	gboolean highlight_overdue;
	GdkRGBA *overdue_color;
	gboolean show_completed_tasks;
	gboolean show_no_duedate_tasks;
	gboolean use_24hour_format;
	gboolean time_in_smaller_font;

	EClientCache *client_cache;
	ESourceRegistryWatcher *watcher;
	GtkTreeStore *tree_store;
	GtkTreeView *tree_view;
	ECalDataModel *events_data_model;
	ECalDataModel *tasks_data_model;
	GHashTable *component_refs; /* ComponentIdent * ~> GSList * { GtkTreeRowRefenrece * } */
	GHashTable *client_colors; /* ESource * ~> GdkRGBA * */

	GCancellable *cancellable;

	guint time_checker_id;
	guint last_today;
	time_t nearest_due;

	gulong source_changed_id;

	GPtrArray *roots; /* GtkTreeRowReference * */
};

enum {
	PROP_0,
	PROP_HIGHLIGHT_OVERDUE,
	PROP_OVERDUE_COLOR,
	PROP_SHELL_VIEW,
	PROP_SHOW_COMPLETED_TASKS,
	PROP_SHOW_NO_DUEDATE_TASKS,
	PROP_USE_24HOUR_FORMAT,
	PROP_SHOW_N_DAYS,
	PROP_TIME_IN_SMALLER_FONT
};

static void e_to_do_pane_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EToDoPane, e_to_do_pane, GTK_TYPE_GRID,
	G_ADD_PRIVATE (EToDoPane)
	G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER, e_to_do_pane_cal_data_model_subscriber_init))

enum {
	COLUMN_BGCOLOR = 0,
	COLUMN_FGCOLOR,
	COLUMN_HAS_ICON_NAME,
	COLUMN_ICON_NAME,
	COLUMN_SUMMARY,
	COLUMN_TOOLTIP,
	COLUMN_SORTKEY,
	COLUMN_DATE_MARK,
	COLUMN_CAL_CLIENT,
	COLUMN_CAL_COMPONENT,
	N_COLUMNS
};

typedef struct _ComponentIdent {
	gconstpointer client;
	gchar *uid;
	gchar *rid;
} ComponentIdent;

static ComponentIdent *
component_ident_new (gconstpointer client,
		     const gchar *uid,
		     const gchar *rid)
{
	ComponentIdent *ci;

	ci = g_new0 (ComponentIdent, 1);
	ci->client = client;
	ci->uid = g_strdup (uid);
	ci->rid = (rid && *rid) ? g_strdup (rid) : NULL;

	return ci;
}

static ComponentIdent *
component_ident_copy (const ComponentIdent *src)
{
	if (!src)
		return NULL;

	return component_ident_new (src->client, src->uid, src->rid);
}

static void
component_ident_free (gpointer ptr)
{
	ComponentIdent *ci = ptr;

	if (ci) {
		g_free (ci->uid);
		g_free (ci->rid);
		g_free (ci);
	}
}

static guint
component_ident_hash (gconstpointer ptr)
{
	const ComponentIdent *ci = ptr;

	if (!ci)
		return 0;

	return g_direct_hash (ci->client) ^
		(ci->uid ? g_str_hash (ci->uid) : 0) ^
		(ci->rid ? g_str_hash (ci->rid) : 0);
}

static gboolean
component_ident_equal (gconstpointer ptr1,
		       gconstpointer ptr2)
{
	const ComponentIdent *ci1 = ptr1, *ci2 = ptr2;

	if (!ci1 || !ci2)
		return ci1 == ci2;

	return ci1->client == ci2->client &&
		g_strcmp0 (ci1->uid, ci2->uid) == 0 &&
		g_strcmp0 (ci1->rid, ci2->rid) == 0;
}

static void
etdp_free_component_refs (gpointer ptr)
{
	GSList *roots = ptr;

	g_slist_free_full (roots, (GDestroyNotify) gtk_tree_row_reference_free);
}

static guint
etdp_create_date_mark (/* const */ ICalTime *itt)
{
	if (!itt)
		return 0;

	return i_cal_time_get_year (itt) * 10000 +
	       i_cal_time_get_month (itt) * 100 +
	       i_cal_time_get_day (itt);
}

static void
etdp_itt_to_zone (ICalTime *itt,
		  const gchar *itt_tzid,
		  ECalClient *client,
		  ICalTimezone *default_zone)
{
	ICalTimezone *zone = NULL;

	g_return_if_fail (itt != NULL);

	if (itt_tzid) {
		if (!e_cal_client_get_timezone_sync (client, itt_tzid, &zone, NULL, NULL))
			zone = NULL;
	} else if (i_cal_time_is_utc (itt)) {
		zone = i_cal_timezone_get_utc_timezone ();
	}

	if (zone) {
		i_cal_time_convert_timezone (itt, zone, default_zone);
		i_cal_time_set_timezone (itt, default_zone);
	}
}

static gboolean
etdp_task_is_overdue (ICalTime *itt,
		      guint today_date_mark)
{
	gboolean is_overdue;

	if (!i_cal_time_is_date (itt))
		return etdp_create_date_mark (itt) < today_date_mark;

	/* The DATE value means it's overdue at the beginning of the day */
	i_cal_time_adjust (itt, -1, 0, 0, 0);

	is_overdue = etdp_create_date_mark (itt) < today_date_mark;

	/* Restore the original date */
	i_cal_time_adjust (itt, 1, 0, 0, 0);

	return is_overdue;
}

static gchar *
etdp_date_time_to_string (const ECalComponentDateTime *dt,
			  ECalClient *client,
			  ICalTimezone *default_zone,
			  guint today_date_mark,
			  gboolean is_task,
			  gboolean use_24hour_format,
			  ICalTime **out_itt)
{
	gboolean is_overdue;
	gchar *res;

	g_return_val_if_fail (dt != NULL, NULL);
	g_return_val_if_fail (e_cal_component_datetime_get_value (dt) != NULL, NULL);
	g_return_val_if_fail (out_itt != NULL, NULL);

	*out_itt = i_cal_time_clone (e_cal_component_datetime_get_value (dt));

	etdp_itt_to_zone (*out_itt, e_cal_component_datetime_get_tzid (dt), client, default_zone);

	is_overdue = is_task && etdp_task_is_overdue (*out_itt, today_date_mark);

	if (i_cal_time_is_date (*out_itt) && !is_overdue)
		return NULL;

	if (is_overdue) {
		struct tm tm;

		tm = e_cal_util_icaltime_to_tm (*out_itt);

		res = e_datetime_format_format_tm ("calendar", "table", i_cal_time_is_date (*out_itt) ? DTFormatKindDate : DTFormatKindDateTime, &tm);
	} else {
		if (use_24hour_format) {
			res = g_strdup_printf ("%d:%02d", i_cal_time_get_hour (*out_itt), i_cal_time_get_minute (*out_itt));
		} else {
			gint hour = i_cal_time_get_hour (*out_itt);
			const gchar *suffix;

			if (hour < 12) {
				/* String to use in 12-hour time format for times in the morning. */
				suffix = _("am");
			} else {
				hour -= 12;
				/* String to use in 12-hour time format for times in the afternoon. */
				suffix = _("pm");
			}

			if (hour == 0)
				hour = 12;

			if (!i_cal_time_get_minute (*out_itt))
				res = g_strdup_printf ("%d %s", hour, suffix);
			else
				res = g_strdup_printf ("%d:%02d %s", hour, i_cal_time_get_minute (*out_itt), suffix);
		}
	}

	return res;
}

static void
etdp_append_to_string_escaped (GString *str,
			       const gchar *format,
			       const gchar *value1,
			       const gchar *value2)
{
	gchar *escaped;

	g_return_if_fail (str != NULL);
	g_return_if_fail (format != NULL);

	if (!value1 || !*value1)
		return;

	escaped = g_markup_printf_escaped (format, value1, value2);
	g_string_append (str, escaped);
	g_free (escaped);
}

static gchar *
etdp_format_date_time (ECalClient *client,
		       ICalTimezone *default_zone,
		       const ICalTime *in_itt,
		       const gchar *tzid)
{
	ICalTime *itt;
	struct tm tm;
	gchar *res;

	if (!in_itt)
		return NULL;

	itt = i_cal_time_clone ((ICalTime *) in_itt);

	etdp_itt_to_zone (itt, tzid, client, default_zone);

	tm = e_cal_util_icaltime_to_tm (itt);

	res = e_datetime_format_format_tm ("calendar", "table", i_cal_time_is_date (itt) ? DTFormatKindDate : DTFormatKindDateTime, &tm);

	g_clear_object (&itt);

	return res;
}

static gchar *
etdp_dup_component_summary (ICalComponent *icomp)
{
	gchar *summary;

	if (!icomp)
		return g_strdup ("");

	summary = e_calendar_view_dup_component_summary (icomp);

	if (!summary)
		summary = g_strdup ("");

	return summary;
}

static gboolean
etdp_get_component_data (EToDoPane *to_do_pane,
			 ECalClient *client,
			 ECalComponent *comp,
			 ICalTimezone *default_zone,
			 guint today_date_mark,
			 gchar **out_summary,
			 gchar **out_summary_no_time,
			 gchar **out_tooltip,
			 gboolean *out_is_task,
			 gboolean *out_is_completed,
			 gchar **out_sort_key,
			 guint *out_date_mark)
{
	ICalComponent *icomp;
	ECalComponentDateTime *dt;
	ECalComponentId *id;
	ICalTime *itt = NULL;
	const gchar *prefix, *location, *description, *uid_str, *rid_str;
	gboolean task_has_due_date = TRUE, is_cancelled = FALSE; /* ignored for events, thus like being set */
	ICalPropertyStatus status;
	ICalProperty *prop;
	gchar *comp_summary;
	GString *tooltip;

	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), FALSE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);
	g_return_val_if_fail (out_summary, FALSE);
	g_return_val_if_fail (out_summary_no_time, FALSE);
	g_return_val_if_fail (out_tooltip, FALSE);
	g_return_val_if_fail (out_is_task, FALSE);
	g_return_val_if_fail (out_is_completed, FALSE);
	g_return_val_if_fail (out_sort_key, FALSE);
	g_return_val_if_fail (out_date_mark, FALSE);

	icomp = e_cal_component_get_icalcomponent (comp);
	g_return_val_if_fail (icomp != NULL, FALSE);

	location = i_cal_component_get_location (icomp);
	if (location && !*location)
		location = NULL;

	tooltip = g_string_sized_new (512);

	comp_summary = etdp_dup_component_summary (icomp);

	etdp_append_to_string_escaped (tooltip, "<b>%s</b>", comp_summary, NULL);

	if (location) {
		g_string_append_c (tooltip, '\n');
		/* Translators: It will display "Location: LocationOfTheAppointment" */
		etdp_append_to_string_escaped (tooltip, _("Location: %s"), location, NULL);
	}

	*out_is_task = e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO;
	*out_is_completed = FALSE;

	status = e_cal_component_get_status (comp);
	is_cancelled = status == I_CAL_STATUS_CANCELLED;

	if (*out_is_task) {
		ECalComponentDateTime *dtstart;
		ICalTime *completed;

		/* Tasks after events */
		prefix = "1";

		dtstart = e_cal_component_get_dtstart (comp);
		/* Do not use etdp_get_task_due() here, to show the set date in the GUI */
		dt = e_cal_component_get_due (comp);
		completed = e_cal_component_get_completed (comp);

		if (dtstart && e_cal_component_datetime_get_value (dtstart)) {
			gchar *tmp;

			tmp = etdp_format_date_time (client, default_zone,
				e_cal_component_datetime_get_value (dtstart),
				e_cal_component_datetime_get_tzid (dtstart));

			g_string_append_c (tooltip, '\n');
			/* Translators: It will display "Start: StartDateAndTime" */
			etdp_append_to_string_escaped (tooltip, _("Start: %s"), tmp, NULL);

			g_free (tmp);

			if (!dt || !e_cal_component_datetime_get_value (dt)) {
				/* Fill the itt structure in case the task has no Due date */
				itt = i_cal_time_clone (e_cal_component_datetime_get_value (dtstart));
				etdp_itt_to_zone (itt, e_cal_component_datetime_get_tzid (dtstart), client, default_zone);
			}
		}

		e_cal_component_datetime_free (dtstart);

		if (dt && e_cal_component_datetime_get_value (dt)) {
			gchar *tmp;

			tmp = etdp_format_date_time (client, default_zone,
				e_cal_component_datetime_get_value (dt),
				e_cal_component_datetime_get_tzid (dt));

			g_string_append_c (tooltip, '\n');
			/* Translators: It will display "Due: DueDateAndTime" */
			etdp_append_to_string_escaped (tooltip, _("Due: %s"), tmp, NULL);

			g_free (tmp);
		} else {
			task_has_due_date = FALSE;
		}

		if (completed) {
			gchar *tmp;

			tmp = etdp_format_date_time (client, default_zone, completed, NULL);

			g_string_append_c (tooltip, '\n');
			/* Translators: It will display "Completed: DateAndTimeWhenCompleted" */
			etdp_append_to_string_escaped (tooltip, _("Completed: %s"), tmp, NULL);

			g_free (tmp);

			*out_is_completed = TRUE;
		} else {
			*out_is_completed = *out_is_completed || status == I_CAL_STATUS_COMPLETED;
		}

		g_clear_object (&completed);
	} else {
		/* Events first */
		prefix = "0";

		dt = e_cal_component_get_dtstart (comp);

		if (dt && e_cal_component_datetime_get_value (dt)) {
			ECalComponentDateTime *dtend;
			ICalTime *ittstart, *ittend;
			gchar *strstart, *strduration;

			dtend = e_cal_component_get_dtend (comp);

			ittstart = i_cal_time_clone (e_cal_component_datetime_get_value (dt));
			if (dtend && e_cal_component_datetime_get_value (dtend))
				ittend = i_cal_time_clone (e_cal_component_datetime_get_value (dtend));
			else
				ittend = i_cal_time_clone (ittstart);

			etdp_itt_to_zone (ittstart, e_cal_component_datetime_get_tzid (dt), client, default_zone);
			etdp_itt_to_zone (ittend, (dtend && e_cal_component_datetime_get_value (dtend)) ?
				e_cal_component_datetime_get_tzid (dtend) : e_cal_component_datetime_get_tzid (dt), client, default_zone);

			g_string_append_c (tooltip, '\n');

			strstart = etdp_format_date_time (client, default_zone, ittstart, NULL);
			strduration = e_cal_util_seconds_to_string (i_cal_time_as_timet (ittend) - i_cal_time_as_timet (ittstart));
			if (strduration && *strduration) {
				/* Translators: It will display "Time: StartDateAndTime (Duration)" */
				etdp_append_to_string_escaped (tooltip, _("Time: %s (%s)"), strstart, strduration);
			} else {
				/* Translators: It will display "Time: StartDateAndTime" */
				etdp_append_to_string_escaped (tooltip, _("Time: %s"), strstart, NULL);
			}

			g_free (strduration);
			g_free (strstart);

			e_cal_component_datetime_free (dtend);
			g_clear_object (&ittstart);
			g_clear_object (&ittend);
		}
	}

	*out_summary = NULL;
	*out_summary_no_time = g_markup_printf_escaped ("%s%s%s%s", comp_summary,
		location ? " (" : "", location ? location : "", location ? ")" : "");

	if (dt && e_cal_component_datetime_get_value (dt)) {
		gchar *time_str;

		g_clear_object (&itt);

		time_str = etdp_date_time_to_string (dt, client, default_zone, today_date_mark, *out_is_task,
			to_do_pane->priv->use_24hour_format, &itt);

		if (time_str) {
			if (to_do_pane->priv->time_in_smaller_font) {
				if (gtk_widget_get_direction (GTK_WIDGET (to_do_pane)) == GTK_TEXT_DIR_RTL) {
					*out_summary = g_markup_printf_escaped ("%s%s%s%s <span size=\"xx-small\">%s</span>",
						location ? ")" : "", location ? location : "", location ? " (" : "",
						comp_summary, time_str);
				} else {
					*out_summary = g_markup_printf_escaped ("<span size=\"xx-small\">%s</span> %s%s%s%s",
						time_str, comp_summary, location ? " (" : "",
						location ? location : "", location ? ")" : "");
				}
			} else {
				if (gtk_widget_get_direction (GTK_WIDGET (to_do_pane)) == GTK_TEXT_DIR_RTL) {
					*out_summary = g_markup_printf_escaped ("%s%s%s%s %s",
						location ? ")" : "", location ? location : "", location ? " (" : "",
						comp_summary, time_str);
				} else {
					*out_summary = g_markup_printf_escaped ("%s %s%s%s%s",
						time_str, comp_summary, location ? " (" : "",
						location ? location : "", location ? ")" : "");
				}
			}
		}

		g_free (time_str);
	}

	if (!*out_summary)
		*out_summary = g_strdup (*out_summary_no_time);

	if (*out_is_completed || is_cancelled) {
		gchar *tmp;

		/* With leading space, to have proper row height in GtkTreeView */

		tmp = *out_summary;
		*out_summary = g_strdup_printf (" <s>%s</s>", *out_summary);
		g_free (tmp);

		tmp = *out_summary_no_time;
		*out_summary_no_time = g_strdup_printf (" <s>%s</s>", *out_summary_no_time);
		g_free (tmp);
	} else {
		gchar *tmp;

		/* With leading space, to have proper row height in GtkTreeView */

		tmp = *out_summary;
		*out_summary = g_strconcat (" ", *out_summary, NULL);
		g_free (tmp);

		tmp = *out_summary_no_time;
		*out_summary_no_time = g_strconcat (" ", *out_summary_no_time, NULL);
		g_free (tmp);
	}

	e_cal_component_datetime_free (dt);

	id = e_cal_component_get_id (comp);
	uid_str = (id && e_cal_component_id_get_uid (id)) ? e_cal_component_id_get_uid (id) : "";
	rid_str = (id && e_cal_component_id_get_rid (id)) ? e_cal_component_id_get_rid (id) : "";

	if (!task_has_due_date) {
		if (!itt || i_cal_time_is_null_time (itt)) {
			/* Sort those without Start date after those with it */
			*out_sort_key = g_strdup_printf ("%s-Z-%s-%s-%s",
				prefix, comp_summary,
				uid_str, rid_str);
		} else {
			*out_sort_key = g_strdup_printf ("%s-%04d%02d%02d%02d%02d%02d-%s-%s-%s",
				prefix,
				i_cal_time_get_year (itt),
				i_cal_time_get_month (itt),
				i_cal_time_get_day (itt),
				i_cal_time_get_hour (itt),
				i_cal_time_get_minute (itt),
				i_cal_time_get_second (itt),
				comp_summary, uid_str, rid_str);
		}
	} else {
		*out_sort_key = g_strdup_printf ("%s-%04d%02d%02d%02d%02d%02d-%s-%s",
			prefix,
			itt ? i_cal_time_get_year (itt) : 0,
			itt ? i_cal_time_get_month (itt) : 0,
			itt ? i_cal_time_get_day (itt) : 0,
			itt ? i_cal_time_get_hour (itt) : 0,
			itt ? i_cal_time_get_minute (itt) : 0,
			itt ? i_cal_time_get_second (itt) : 0,
			uid_str, rid_str);
	}

	e_cal_component_id_free (id);

	prop = e_cal_util_component_find_property_for_locale (icomp, I_CAL_DESCRIPTION_PROPERTY, NULL);
	description = prop ? i_cal_property_get_description (prop) : NULL;
	if (description && *description && g_utf8_validate (description, -1, NULL)) {
		gchar *tmp = NULL;
		glong len;

		len = g_utf8_strlen (description, -1);
		if (len > MAX_TOOLTIP_DESCRIPTION_LEN) {
			GString *str;
			const gchar *end;

			end = g_utf8_offset_to_pointer (description, MAX_TOOLTIP_DESCRIPTION_LEN);
			str = g_string_new_len (description, end - description);
			g_string_append (str, _("…"));

			tmp = g_string_free (str, FALSE);
		}

		g_string_append (tooltip, "\n\n");
		etdp_append_to_string_escaped (tooltip, "%s", tmp ? tmp : description, NULL);

		g_free (tmp);
	}

	g_clear_object (&prop);

	*out_date_mark = etdp_create_date_mark (itt);
	*out_tooltip = g_string_free (tooltip, FALSE);

	g_clear_object (&itt);
	g_free (comp_summary);

	return TRUE;
}

static GdkRGBA
etdp_get_fgcolor_for_bgcolor (const GdkRGBA *bgcolor)
{
	GdkRGBA fgcolor = { 1.0, 1.0, 1.0, 1.0 };

	if (bgcolor)
		fgcolor = e_utils_get_text_color_for_background (bgcolor);

	return fgcolor;
}

static ECalComponentDateTime *
etdp_get_task_due (ECalComponent *comp)
{
	ECalComponentDateTime *dt;

	dt = e_cal_component_get_due (comp);

	if (dt && e_cal_component_datetime_get_value (dt)) {
		ICalTime *itt;

		itt = e_cal_component_datetime_get_value (dt);
		if (i_cal_time_is_date (itt)) {
			/* The DATE value means it's overdue at the beginning of the day */
			i_cal_time_adjust (itt, -1, 0, 0, 0);
		}
	}

	return dt;
}

static GSList * /* GtkTreePath * */
etdp_get_component_root_paths (EToDoPane *to_do_pane,
			       ECalClient *client,
			       ECalComponent *comp,
			       ICalTimezone *default_zone)
{
	ECalComponentDateTime *dt;
	ICalTime *itt;
	GtkTreePath *first_root_path = NULL;
	GtkTreeModel *model;
	GSList *roots = NULL;
	guint start_date_mark, end_date_mark, prev_date_mark = 0;
	gint ii;

	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), NULL);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), NULL);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), NULL);

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO) {
		dt = etdp_get_task_due (comp);

		if (dt && e_cal_component_datetime_get_value (dt)) {
			itt = e_cal_component_datetime_get_value (dt);

			etdp_itt_to_zone (itt, e_cal_component_datetime_get_tzid (dt), client, default_zone);
			start_date_mark = etdp_create_date_mark (itt);
		} else {
			start_date_mark = 0;
		}

		end_date_mark = start_date_mark;

		e_cal_component_datetime_free (dt);
	} else {
		dt = e_cal_component_get_dtstart (comp);

		if (dt && e_cal_component_datetime_get_value (dt)) {
			itt = e_cal_component_datetime_get_value (dt);

			etdp_itt_to_zone (itt, e_cal_component_datetime_get_tzid (dt), client, default_zone);
			start_date_mark = etdp_create_date_mark (itt);
		} else {
			start_date_mark = 0;
		}

		e_cal_component_datetime_free (dt);

		dt = e_cal_component_get_dtend (comp);

		if (dt && e_cal_component_datetime_get_value (dt)) {
			itt = e_cal_component_datetime_get_value (dt);

			etdp_itt_to_zone (itt, e_cal_component_datetime_get_tzid (dt), client, default_zone);

			/* The end time is excluded */
			if (i_cal_time_is_date (itt))
				i_cal_time_adjust (itt, -1, 0, 0, 0);
			else
				i_cal_time_adjust (itt, 0, 0, 0, -1);

			end_date_mark = etdp_create_date_mark (itt);

			/* Multiday event, the end_date_mark is excluded, thus add one more day */
			if (end_date_mark > start_date_mark)
				end_date_mark++;
			else if (end_date_mark < start_date_mark)
				end_date_mark = start_date_mark;
		} else {
			end_date_mark = start_date_mark;
		}

		e_cal_component_datetime_free (dt);
	}

	model = GTK_TREE_MODEL (to_do_pane->priv->tree_store);

	if (start_date_mark == 0 && e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO) {
		GtkTreeRowReference *rowref;

		if (!to_do_pane->priv->show_no_duedate_tasks)
			return NULL;

		rowref = g_ptr_array_index (to_do_pane->priv->roots, to_do_pane->priv->roots->len - 1);

		if (gtk_tree_row_reference_valid (rowref)) {
			GtkTreePath *root_path;
			GtkTreeIter root_iter;

			root_path = gtk_tree_row_reference_get_path (rowref);
			if (root_path && gtk_tree_model_get_iter (model, &root_iter, root_path)) {
				roots = g_slist_prepend (roots, root_path);
				root_path = NULL;
			}

			gtk_tree_path_free (root_path);
		}

		return roots;
	}

	for (ii = 0; ii < to_do_pane->priv->roots->len - 1; ii++) {
		GtkTreeRowReference *rowref;

		rowref = g_ptr_array_index (to_do_pane->priv->roots, ii);

		if (gtk_tree_row_reference_valid (rowref)) {
			GtkTreePath *root_path;
			GtkTreeIter root_iter;
			guint root_date_mark = 0;

			root_path = gtk_tree_row_reference_get_path (rowref);
			if (root_path && gtk_tree_model_get_iter (model, &root_iter, root_path)) {
				gtk_tree_model_get (model, &root_iter, COLUMN_DATE_MARK, &root_date_mark, -1);

				if (start_date_mark < root_date_mark && (end_date_mark > prev_date_mark ||
				    (start_date_mark == end_date_mark && end_date_mark >= prev_date_mark))) {
					roots = g_slist_prepend (roots, gtk_tree_path_copy (root_path));
				} else if (!first_root_path) {
					first_root_path = gtk_tree_path_copy (root_path);
				}

				prev_date_mark = root_date_mark;
			}

			gtk_tree_path_free (root_path);
		}
	}

	if (!roots && first_root_path && start_date_mark < prev_date_mark)
		roots = g_slist_prepend (roots, first_root_path);
	else
		gtk_tree_path_free (first_root_path);

	return g_slist_reverse (roots);
}

static GSList * /* GtkTreeRowReference * */
etdp_merge_with_root_paths (EToDoPane *to_do_pane,
			    GtkTreeModel *model,
			    const GSList *new_root_paths, /* GtkTreePath * */
			    const GSList *current_refs) /* GtkTreeRowReference * */
{
	GSList *new_references = NULL;
	const GSList *paths_link, *refs_link;
	GtkTreeIter iter, parent;

	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), NULL);
	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
	g_return_val_if_fail (new_root_paths != NULL, NULL);

	refs_link = current_refs;
	for (paths_link = new_root_paths; paths_link; paths_link = g_slist_next (paths_link)) {
		GtkTreePath *root_path = paths_link->data;
		gboolean found = FALSE;

		while (refs_link && !found) {
			GtkTreeRowReference *reference = refs_link->data;
			GtkTreePath *ref_path;

			ref_path = gtk_tree_row_reference_get_path (reference);
			if (ref_path &&
			    gtk_tree_model_get_iter (model, &iter, ref_path) &&
			    gtk_tree_model_iter_parent (model, &parent, &iter)) {
				GtkTreePath *parent_path;
				gint cmp;

				parent_path = gtk_tree_model_get_path (model, &parent);
				cmp = gtk_tree_path_compare (parent_path, root_path);
				gtk_tree_path_free (parent_path);

				if (cmp == 0) {
					found = TRUE;
					new_references = g_slist_prepend (new_references, gtk_tree_row_reference_copy (reference));
				} else if (cmp > 0) {
					gtk_tree_path_free (ref_path);
					break;
				} else {
					gtk_tree_store_remove (to_do_pane->priv->tree_store, &iter);
				}
			}
			gtk_tree_path_free (ref_path);

			refs_link = g_slist_next (refs_link);
		}

		if (!found) {
			GtkTreePath *path;

			g_warn_if_fail (gtk_tree_model_get_iter (model, &parent, root_path));

			gtk_tree_store_append (to_do_pane->priv->tree_store, &iter, &parent);
			path = gtk_tree_model_get_path (model, &iter);

			if (gtk_tree_model_iter_n_children (model, &parent) == 1) {
				gtk_tree_view_expand_row (to_do_pane->priv->tree_view, root_path, TRUE);
			}

			new_references = g_slist_prepend (new_references, gtk_tree_row_reference_new (model, path));

			gtk_tree_path_free (path);
		}
	}

	while (refs_link) {
		GtkTreeRowReference *reference = refs_link->data;
		GtkTreePath *ref_path;

		ref_path = gtk_tree_row_reference_get_path (reference);
		if (ref_path &&
		    gtk_tree_model_get_iter (model, &iter, ref_path)) {
			gtk_tree_store_remove (to_do_pane->priv->tree_store, &iter);
		}

		gtk_tree_path_free (ref_path);

		refs_link = g_slist_next (refs_link);
	}

	g_warn_if_fail (g_slist_length (new_references) == g_slist_length ((GSList *) new_root_paths));

	return g_slist_reverse (new_references);
}

static void
etdp_get_comp_colors (EToDoPane *to_do_pane,
		      ECalClient *client,
		      ECalComponent *comp,
		      GdkRGBA *out_bgcolor,
		      gboolean *out_bgcolor_set,
		      GdkRGBA *out_fgcolor,
		      gboolean *out_fgcolor_set,
		      time_t *out_nearest_due)
{
	GdkRGBA *bgcolor = NULL, fgcolor;
	GdkRGBA stack_bgcolor;
	ICalProperty *prop;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));
	g_return_if_fail (out_bgcolor);
	g_return_if_fail (out_bgcolor_set);
	g_return_if_fail (out_fgcolor);
	g_return_if_fail (out_fgcolor_set);

	*out_bgcolor_set = FALSE;
	*out_fgcolor_set = FALSE;

	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	prop = i_cal_component_get_first_property (e_cal_component_get_icalcomponent (comp), I_CAL_COLOR_PROPERTY);
	if (prop) {
		const gchar *color_spec;

		color_spec = i_cal_property_get_color (prop);
		if (color_spec && gdk_rgba_parse (&stack_bgcolor, color_spec)) {
			bgcolor = &stack_bgcolor;
		}

		g_clear_object (&prop);
	}

	if (!bgcolor)
		bgcolor = g_hash_table_lookup (to_do_pane->priv->client_colors, e_client_get_source (E_CLIENT (client)));

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO &&
	    to_do_pane->priv->highlight_overdue &&
	    to_do_pane->priv->overdue_color) {
		ECalComponentDateTime *dt;

		dt = etdp_get_task_due (comp);

		if (dt && e_cal_component_datetime_get_value (dt)) {
			ICalTimezone *default_zone;
			ICalTime *itt, *now;
			gboolean is_date;

			default_zone = e_cal_data_model_get_timezone (to_do_pane->priv->events_data_model);

			itt = e_cal_component_datetime_get_value (dt);
			is_date = i_cal_time_is_date (itt);
			etdp_itt_to_zone (itt, e_cal_component_datetime_get_tzid (dt), client, default_zone);

			now = i_cal_time_new_current_with_zone (default_zone);
			i_cal_time_set_timezone (now, default_zone);

			if ((is_date && i_cal_time_compare_date_only_tz (itt, now, default_zone) < 0) ||
			    (!is_date && i_cal_time_compare (itt, now) <= 0)) {
				bgcolor = to_do_pane->priv->overdue_color;
			} else if (out_nearest_due) {
				time_t due_tt;

				due_tt = i_cal_time_as_timet_with_zone (itt, default_zone);
				if (*out_nearest_due == (time_t) -1 ||
				    *out_nearest_due > due_tt)
					*out_nearest_due = due_tt;
			}

			g_clear_object (&now);
		}

		e_cal_component_datetime_free (dt);
	}

	fgcolor = etdp_get_fgcolor_for_bgcolor (bgcolor);

	*out_bgcolor_set = bgcolor != NULL;
	if (bgcolor)
		*out_bgcolor = *bgcolor;

	*out_fgcolor_set = *out_bgcolor_set;
	*out_fgcolor = fgcolor;
}

static void
etdp_remove_ident (EToDoPane *to_do_pane,
		   ComponentIdent *ident)
{
	GSList *link;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));
	g_return_if_fail (ident != NULL);

	for (link = g_hash_table_lookup (to_do_pane->priv->component_refs, ident); link; link = g_slist_next (link)) {
		GtkTreeRowReference *reference = link->data;

		if (reference && gtk_tree_row_reference_valid (reference)) {
			GtkTreePath *path;
			GtkTreeIter iter;

			path = gtk_tree_row_reference_get_path (reference);

			if (path && gtk_tree_model_get_iter (gtk_tree_row_reference_get_model (reference), &iter, path)) {
				gtk_tree_store_remove (to_do_pane->priv->tree_store, &iter);
			}

			gtk_tree_path_free (path);
		}
	}

	g_hash_table_remove (to_do_pane->priv->component_refs, ident);
}

static void
etdp_add_component (EToDoPane *to_do_pane,
		    ECalClient *client,
		    ECalComponent *comp)
{
	ECalComponentId *id;
	ComponentIdent *ident;
	ICalTimezone *default_zone;
	GSList *new_root_paths, *new_references, *link;
	GtkTreeModel *model;
	GtkTreeIter iter = { 0 };
	GdkRGBA bgcolor, fgcolor;
	gboolean bgcolor_set = FALSE, fgcolor_set = FALSE;
	gchar *summary = NULL, *summary_no_time = NULL, *tooltip = NULL, *sort_key = NULL;
	gboolean is_task = FALSE, is_completed = FALSE, use_summary_no_time;
	const gchar *icon_name;
	guint date_mark = 0;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	id = e_cal_component_get_id (comp);
	g_return_if_fail (id != NULL);

	default_zone = e_cal_data_model_get_timezone (to_do_pane->priv->events_data_model);

	if (!etdp_get_component_data (to_do_pane, client, comp, default_zone, to_do_pane->priv->last_today,
		&summary, &summary_no_time, &tooltip, &is_task, &is_completed, &sort_key, &date_mark)) {
		e_cal_component_id_free (id);
		return;
	}

	model = GTK_TREE_MODEL (to_do_pane->priv->tree_store);
	ident = component_ident_new (client, e_cal_component_id_get_uid (id), e_cal_component_id_get_rid (id));

	new_root_paths = etdp_get_component_root_paths (to_do_pane, client, comp, default_zone);
	/* This can happen with "Show Tasks without Due date", which returns
	   basically all tasks, even with Due date in the future, out of
	   the interval used by the To Do bar. */
	if (!new_root_paths) {
		etdp_remove_ident (to_do_pane, ident);
		goto exit;
	}

	new_references = etdp_merge_with_root_paths (to_do_pane, model, new_root_paths,
		g_hash_table_lookup (to_do_pane->priv->component_refs, ident));

	g_slist_free_full (new_root_paths, (GDestroyNotify) gtk_tree_path_free);

	if (is_task && e_cal_component_has_recurrences (comp)) {
		icon_name = "stock_task-recurring";
	} else if (e_cal_component_has_attendees (comp)) {
		if (is_task) {
			ESourceRegistry *registry;

			icon_name = "stock_task-assigned";
			registry = e_source_registry_watcher_get_registry (to_do_pane->priv->watcher);

			if (itip_organizer_is_user (registry, comp, client)) {
				icon_name = "stock_task-assigned-to";
			} else {
				GSList *attendees = NULL;

				attendees = e_cal_component_get_attendees (comp);
				for (link = attendees; link; link = g_slist_next (link)) {
					ECalComponentAttendee *ca = link->data;
					const gchar *text;

					text = e_cal_util_get_attendee_email (ca);
					if (itip_address_is_user (registry, text)) {
						if (e_cal_component_attendee_get_delegatedto (ca))
							icon_name = "stock_task-assigned-to";
						break;
					}
				}

				g_slist_free_full (attendees, e_cal_component_attendee_free);
			}
		} else
			icon_name = "stock_people";
	} else {
		if (is_task)
			icon_name = "stock_task";
		else
			icon_name = "appointment-new";
	}

	etdp_get_comp_colors (to_do_pane, client, comp, &bgcolor, &bgcolor_set, &fgcolor, &fgcolor_set,
		&to_do_pane->priv->nearest_due);

	use_summary_no_time = !is_task && to_do_pane->priv->last_today > date_mark;

	for (link = new_references; link; link = g_slist_next (link)) {
		GtkTreeRowReference *reference = link->data;

		if (gtk_tree_row_reference_valid (reference)) {
			GtkTreePath *path;

			path = gtk_tree_row_reference_get_path (reference);
			if (path && gtk_tree_model_get_iter (model, &iter, path)) {
				gtk_tree_store_set (to_do_pane->priv->tree_store, &iter,
					COLUMN_BGCOLOR, bgcolor_set ? &bgcolor : NULL,
					COLUMN_FGCOLOR, fgcolor_set ? &fgcolor : NULL,
					COLUMN_HAS_ICON_NAME, TRUE,
					COLUMN_ICON_NAME, icon_name,
					COLUMN_SUMMARY, (is_task || !use_summary_no_time) ? summary : summary_no_time,
					COLUMN_TOOLTIP, tooltip,
					COLUMN_SORTKEY, sort_key,
					COLUMN_DATE_MARK, date_mark,
					COLUMN_CAL_CLIENT, client,
					COLUMN_CAL_COMPONENT, comp,
					-1);

				/* If an event is split between multiple days, then the next day should be without the start time */
				if (!is_task && !use_summary_no_time)
					use_summary_no_time = TRUE;
			}

			gtk_tree_path_free (path);
		}
	}

	g_hash_table_insert (to_do_pane->priv->component_refs, component_ident_copy (ident), new_references);

 exit:
	component_ident_free (ident);
	e_cal_component_id_free (id);
	g_free (summary_no_time);
	g_free (summary);
	g_free (tooltip);
	g_free (sort_key);
}

static void
etdp_got_client_cb (GObject *source_object,
		    GAsyncResult *result,
		    gpointer user_data)
{
	GWeakRef *weakref = user_data;
	EToDoPane *to_do_pane;
	EClient *client;
	GError *error = NULL;

	client = e_client_cache_get_client_finish (E_CLIENT_CACHE (source_object), result, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&error);
		e_weak_ref_free (weakref);
		return;
	}

	to_do_pane = g_weak_ref_get (weakref);

	e_weak_ref_free (weakref);

	if (!to_do_pane) {
		g_clear_object (&client);
		g_clear_error (&error);
		return;
	}

	if (client && gtk_widget_get_visible (GTK_WIDGET (to_do_pane))) {
		ECalClient *cal_client = E_CAL_CLIENT (client);
		ESource *source;
		ESourceSelectable *selectable = NULL;
		ECalDataModel *data_model = NULL;

		g_warn_if_fail (cal_client != NULL);

		source = e_client_get_source (client);

		if (e_cal_client_get_source_type (cal_client) == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
			selectable = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
			data_model = to_do_pane->priv->events_data_model;
		} else if (e_cal_client_get_source_type (cal_client) == E_CAL_CLIENT_SOURCE_TYPE_TASKS) {
			selectable = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);
			data_model = to_do_pane->priv->tasks_data_model;
		}

		if (data_model) {
			g_hash_table_remove (to_do_pane->priv->client_colors, source);
			if (selectable) {
				GdkRGBA rgba;
				gchar *color_spec;

				color_spec = e_source_selectable_dup_color (selectable);
				if (color_spec && gdk_rgba_parse (&rgba, color_spec)) {
					g_hash_table_insert (to_do_pane->priv->client_colors, source, gdk_rgba_copy (&rgba));
				}

				g_free (color_spec);
			}

			e_cal_data_model_add_client (data_model, cal_client);
		}
	} else if (!client) {
		/* Ignore errors */
	}

	g_clear_object (&to_do_pane);
	g_clear_object (&client);
	g_clear_error (&error);
}

static gboolean
e_to_do_pane_watcher_filter_cb (ESourceRegistryWatcher *watcher,
				ESource *source,
				gpointer user_data)
{
	ESourceSelectable *selectable = NULL;

	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
		selectable = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
	else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
		selectable = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);

	return selectable && e_source_selectable_get_selected (selectable);
}

static void
e_to_do_pane_watcher_appeared_cb (ESourceRegistryWatcher *watcher,
				  ESource *source,
				  gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;
	const gchar *extension_name = NULL;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if (!gtk_widget_get_visible (GTK_WIDGET (to_do_pane)))
		return;

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
		extension_name = E_SOURCE_EXTENSION_CALENDAR;
	else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
		extension_name = E_SOURCE_EXTENSION_TASK_LIST;

	g_return_if_fail (extension_name != NULL);

	e_client_cache_get_client (to_do_pane->priv->client_cache, source, extension_name,
		(guint32) -1, to_do_pane->priv->cancellable, etdp_got_client_cb, e_weak_ref_new (to_do_pane));
}

static void
e_to_do_pane_watcher_disappeared_cb (ESourceRegistryWatcher *watcher,
				     ESource *source,
				     gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	g_hash_table_remove (to_do_pane->priv->client_colors, source);

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
		e_cal_data_model_remove_client (to_do_pane->priv->events_data_model, e_source_get_uid (source));
	else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
		e_cal_data_model_remove_client (to_do_pane->priv->tasks_data_model, e_source_get_uid (source));
}

static void
etdp_data_subscriber_component_added (ECalDataModelSubscriber *subscriber,
				      ECalClient *client,
				      ECalComponent *comp)
{
	g_return_if_fail (E_IS_TO_DO_PANE (subscriber));

	etdp_add_component (E_TO_DO_PANE (subscriber), client, comp);
}

static void
etdp_data_subscriber_component_modified (ECalDataModelSubscriber *subscriber,
					 ECalClient *client,
					 ECalComponent *comp)
{
	g_return_if_fail (E_IS_TO_DO_PANE (subscriber));

	etdp_add_component (E_TO_DO_PANE (subscriber), client, comp);
}

static void
etdp_data_subscriber_component_removed (ECalDataModelSubscriber *subscriber,
					ECalClient *client,
					const gchar *uid,
					const gchar *rid)
{
	EToDoPane *to_do_pane;
	ComponentIdent ident;

	g_return_if_fail (E_IS_TO_DO_PANE (subscriber));

	to_do_pane = E_TO_DO_PANE (subscriber);

	ident.client = client;
	ident.uid = (gchar *) uid;
	ident.rid = (gchar *) (rid && *rid ? rid : NULL);

	etdp_remove_ident (to_do_pane, &ident);
}

static void
etdp_data_subscriber_freeze (ECalDataModelSubscriber *subscriber)
{
	g_return_if_fail (E_IS_TO_DO_PANE (subscriber));
}

static void
etdp_data_subscriber_thaw (ECalDataModelSubscriber *subscriber)
{
	g_return_if_fail (E_IS_TO_DO_PANE (subscriber));
}

static GCancellable *
e_to_do_pane_submit_thread_job (GObject *responder,
				const gchar *description,
				const gchar *alert_ident,
				const gchar *alert_arg_0,
				EAlertSinkThreadJobFunc func,
				gpointer user_data,
				GDestroyNotify free_user_data)
{
	EShellView *shell_view;
	EActivity *activity;
	GCancellable *cancellable = NULL;

	g_return_val_if_fail (E_IS_TO_DO_PANE (responder), NULL);

	shell_view = e_to_do_pane_ref_shell_view (E_TO_DO_PANE (responder));
	if (!shell_view)
		return NULL;

	activity = e_shell_view_submit_thread_job (shell_view, description,
		alert_ident, alert_arg_0, func, user_data, free_user_data);

	if (activity) {
		cancellable = e_activity_get_cancellable (activity);
		if (cancellable)
			g_object_ref (cancellable);
		g_object_unref (activity);
	}

	g_clear_object (&shell_view);

	return cancellable;
}

static void
etdp_update_comps (EToDoPane *to_do_pane)
{
	GtkTreeModel *model;
	GtkTreeIter iter, next;
	gint level = 0;
	gboolean done = FALSE;
	GHashTable *comps_by_client; /* ECalClient ~> GHashTable { ECalComponent *, NULL } */
	GHashTableIter htiter;
	gpointer key, value;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	to_do_pane->priv->nearest_due = (time_t) -1;

	if (!to_do_pane->priv->tree_store)
		return;

	model = GTK_TREE_MODEL (to_do_pane->priv->tree_store);

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	comps_by_client = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, (GDestroyNotify) g_hash_table_unref);

	while (!done) {
		if (level != 0) {
			ECalClient *client = NULL;
			ECalComponent *comp = NULL;

			gtk_tree_model_get (model, &iter,
				COLUMN_CAL_CLIENT, &client,
				COLUMN_CAL_COMPONENT, &comp,
				-1);

			if (client && comp) {
				GHashTable *comps;

				comps = g_hash_table_lookup (comps_by_client, client);
				if (comps) {
					g_hash_table_ref (comps);
				} else {
					comps = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
				}

				g_hash_table_insert (comps, g_object_ref (comp), NULL);
				g_hash_table_insert (comps_by_client, g_object_ref (client), comps);
			}

			g_clear_object (&client);
			g_clear_object (&comp);
		}

		done = !gtk_tree_model_iter_children (model, &next, &iter);

		if (done) {
			next = iter;
			done = !gtk_tree_model_iter_next (model, &next);
		} else {
			level++;
		}

		if (done) {
			while (done = !gtk_tree_model_iter_parent (model, &next, &iter), !done) {
				level--;

				iter = next;
				done = !gtk_tree_model_iter_next (model, &next);

				if (!done)
					break;
			}
		}

		iter = next;
	}

	g_hash_table_iter_init (&htiter, comps_by_client);
	while (g_hash_table_iter_next (&htiter, &key, &value)) {
		ECalClient *client = key;
		GHashTable *comps = value;
		GHashTableIter citer;

		g_hash_table_iter_init (&citer, comps);
		while (g_hash_table_iter_next (&citer, &key, NULL)) {
			ECalComponent *comp = key;

			etdp_add_component (to_do_pane, client, comp);
		}
	}

	g_hash_table_destroy (comps_by_client);
}

static void
etdp_update_colors (EToDoPane *to_do_pane,
		    gboolean only_overdue)
{
	GtkTreeModel *model;
	GtkTreeIter iter, next;
	gint level = 0;
	time_t nearest_due = (time_t) -1;
	gboolean done = FALSE;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	model = GTK_TREE_MODEL (to_do_pane->priv->tree_store);

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	while (!done) {
		if (level != 0) {
			ECalClient *client = NULL;
			ECalComponent *comp = NULL;

			gtk_tree_model_get (model, &iter,
				COLUMN_CAL_CLIENT, &client,
				COLUMN_CAL_COMPONENT, &comp,
				-1);

			if (client && comp) {
				GdkRGBA bgcolor, fgcolor;
				gboolean bgcolor_set = FALSE, fgcolor_set = FALSE;

				etdp_get_comp_colors (to_do_pane, client, comp, &bgcolor, &bgcolor_set, &fgcolor, &fgcolor_set, &nearest_due);

				gtk_tree_store_set (to_do_pane->priv->tree_store, &iter,
					COLUMN_BGCOLOR, bgcolor_set ? &bgcolor : NULL,
					COLUMN_FGCOLOR, fgcolor_set ? &fgcolor : NULL,
					-1);
			}

			g_clear_object (&client);
			g_clear_object (&comp);
		}

		done = !gtk_tree_model_iter_children (model, &next, &iter);

		if (done) {
			next = iter;
			done = !gtk_tree_model_iter_next (model, &next);

			/* Overdue can be only those 'Today', thus under the first child. */
			if (only_overdue && !level)
				break;
		} else {
			level++;
		}

		if (done) {
			while (done = !gtk_tree_model_iter_parent (model, &next, &iter), !done) {
				level--;

				iter = next;
				done = !gtk_tree_model_iter_next (model, &next);

				/* Overdue can be only those 'Today', thus under the first child. */
				if (only_overdue && !level)
					done = TRUE;

				if (!done)
					break;
			}
		}

		iter = next;
	}

	to_do_pane->priv->nearest_due = nearest_due;
}

static void
etdp_update_day_labels (EToDoPane *to_do_pane)
{
	ICalTime *itt;
	ICalTimezone *zone;
	guint ii;

	zone = e_cal_data_model_get_timezone (to_do_pane->priv->events_data_model);
	itt = i_cal_time_new_current_with_zone (zone);
	i_cal_time_set_timezone (itt, zone);

	for (ii = 0; ii < to_do_pane->priv->roots->len; ii++) {
		GtkTreeRowReference *rowref;
		GtkTreePath *path;
		GtkTreeIter iter;

		rowref = g_ptr_array_index (to_do_pane->priv->roots, ii);

		if (!gtk_tree_row_reference_valid (rowref)) {
			if (ii == to_do_pane->priv->roots->len - 1) {
				GtkTreeModel *model;
				gchar *sort_key;

				if (!to_do_pane->priv->show_no_duedate_tasks)
					continue;

				sort_key = g_strdup_printf ("A%05u", ii);

				gtk_tree_store_append (to_do_pane->priv->tree_store, &iter, NULL);
				gtk_tree_store_set (to_do_pane->priv->tree_store, &iter,
					COLUMN_SORTKEY, sort_key,
					COLUMN_HAS_ICON_NAME, FALSE,
					-1);

				g_free (sort_key);

				model = GTK_TREE_MODEL (to_do_pane->priv->tree_store);
				path = gtk_tree_model_get_path (model, &iter);

				gtk_tree_row_reference_free (rowref);
				rowref = gtk_tree_row_reference_new (model, path);
				to_do_pane->priv->roots->pdata[ii] = rowref;
				g_warn_if_fail (rowref != NULL);

				gtk_tree_path_free (path);
			} else {
				continue;
			}
		}

		path = gtk_tree_row_reference_get_path (rowref);

		if (gtk_tree_model_get_iter (gtk_tree_row_reference_get_model (rowref), &iter, path)) {
			struct tm tm;
			gchar *markup;
			guint date_mark;

			tm = e_cal_util_icaltime_to_tm (itt);

			i_cal_time_adjust (itt, 1, 0, 0, 0);

			date_mark = etdp_create_date_mark (itt);

			if (ii == 0) {
				markup = g_markup_printf_escaped ("<b>%s</b>", _("Today"));
			} else if (ii == 1) {
				markup = g_markup_printf_escaped ("<b>%s</b>", _("Tomorrow"));
			} else if (ii == to_do_pane->priv->roots->len - 1) {
				if (!to_do_pane->priv->show_no_duedate_tasks) {
					gtk_tree_store_remove (to_do_pane->priv->tree_store, &iter);
					gtk_tree_row_reference_free (rowref);
					to_do_pane->priv->roots->pdata[ii] = NULL;
					gtk_tree_path_free (path);
					break;
				}

				markup = g_markup_printf_escaped ("<b>%s</b>", _("Tasks without Due date"));
			} else {
				gchar *date;

				date = e_datetime_format_format_tm ("calendar", "table", DTFormatKindDate, &tm);
				markup = g_markup_printf_escaped ("<span font_features='tnum=1'><b>%s</b></span>", date);
				g_free (date);
			}

			gtk_tree_store_set (to_do_pane->priv->tree_store, &iter,
				COLUMN_SUMMARY, markup,
				COLUMN_DATE_MARK, date_mark,
				-1);

			g_free (markup);
		} else {
			i_cal_time_adjust (itt, 1, 0, 0, 0);
		}

		gtk_tree_path_free (path);
	}

	g_clear_object (&itt);
}

static void
etdp_check_time_changed (EToDoPane *to_do_pane,
			 gboolean force_update)
{
	ICalTime *itt;
	ICalTimezone *zone;
	guint new_today;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	zone = e_cal_data_model_get_timezone (to_do_pane->priv->events_data_model);
	itt = i_cal_time_new_current_with_zone (zone);
	i_cal_time_set_timezone (itt, zone);
	new_today = etdp_create_date_mark (itt);

	if (force_update || new_today != to_do_pane->priv->last_today) {
		gchar *tasks_filter;
		time_t tt_begin, tt_end;
		gchar *iso_begin_all, *iso_begin, *iso_end;

		to_do_pane->priv->last_today = new_today;

		tt_begin = i_cal_time_as_timet_with_zone (itt, zone);
		tt_begin = time_day_begin_with_zone (tt_begin, zone);
		tt_end = time_add_day_with_zone (tt_begin, to_do_pane->priv->roots->len ? to_do_pane->priv->roots->len - 1 : 1, zone) - 1;

		iso_begin_all = isodate_from_time_t (0);
		iso_begin = isodate_from_time_t (tt_begin);
		iso_end = isodate_from_time_t (tt_end);
		if (to_do_pane->priv->show_no_duedate_tasks) {
			if (to_do_pane->priv->show_completed_tasks) {
				tasks_filter = g_strdup_printf (
					"(or"
					   " (not (has-due?))"
					   " (due-in-time-range? (make-time \"%s\") (make-time \"%s\"))"
					")",
					iso_begin_all, iso_end);
			} else {
				tasks_filter = g_strdup_printf (
					"(and"
					 " (not (is-completed?))"
					 " (not (contains? \"status\" \"CANCELLED\"))"
					 " (or"
					   " (not (has-due?))"
					   " (due-in-time-range? (make-time \"%s\") (make-time \"%s\"))"
					  ")"
					")",
					iso_begin_all, iso_end);
			}
		} else if (to_do_pane->priv->show_completed_tasks) {
			tasks_filter = g_strdup_printf (
					"(or"
					" (and"
					 " (not (is-completed?))"
					 " (not (contains? \"status\" \"CANCELLED\"))"
					 " (due-in-time-range? (make-time \"%s\") (make-time \"%s\"))"
					 ")"
					" (and"
					 " (due-in-time-range? (make-time \"%s\") (make-time \"%s\"))"
					 ")"
					")",
					iso_begin_all, iso_begin, iso_begin, iso_end);
		} else {
			tasks_filter = g_strdup_printf (
					"(and"
					" (not (is-completed?))"
					" (not (contains? \"status\" \"CANCELLED\"))"
					" (due-in-time-range? (make-time \"%s\") (make-time \"%s\"))"
					")",
					iso_begin_all, iso_end);
		}

		/* Re-label the roots */
		etdp_update_day_labels (to_do_pane);

		/* Update data-model-s */
		e_cal_data_model_subscribe (to_do_pane->priv->events_data_model,
			E_CAL_DATA_MODEL_SUBSCRIBER (to_do_pane), tt_begin, tt_end);

		e_cal_data_model_set_filter (to_do_pane->priv->tasks_data_model, tasks_filter);

		e_cal_data_model_subscribe (to_do_pane->priv->tasks_data_model,
			E_CAL_DATA_MODEL_SUBSCRIBER (to_do_pane), 0, 0);

		g_free (tasks_filter);
		g_free (iso_begin_all);
		g_free (iso_begin);
		g_free (iso_end);

		etdp_update_comps (to_do_pane);
	} else {
		time_t now_tt = i_cal_time_as_timet_with_zone (itt, zone);

		if (to_do_pane->priv->nearest_due != (time_t) -1 &&
		    to_do_pane->priv->nearest_due <= now_tt)
			etdp_update_colors (to_do_pane, TRUE);
	}

	g_clear_object (&itt);
}

static gboolean
etdp_check_time_cb (gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), FALSE);

	etdp_check_time_changed (to_do_pane, FALSE);

	return TRUE;
}

static void
etdp_update_queries (EToDoPane *to_do_pane)
{
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	etdp_check_time_changed (to_do_pane, TRUE);
}

static void
etdp_timezone_changed_cb (ECalDataModel *data_model,
			  GParamSpec *param,
			  gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	etdp_check_time_changed (to_do_pane, TRUE);
}

static gboolean
etdp_settings_map_string_to_icaltimezone (GValue *value,
					  GVariant *variant,
					  gpointer user_data)
{
	GSettings *settings;
	const gchar *location = NULL;
	ICalTimezone *timezone = NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (g_settings_get_boolean (settings, "use-system-timezone"))
		timezone = e_cal_util_get_system_timezone ();
	else
		location = g_variant_get_string (variant, NULL);

	if (location != NULL && *location != '\0')
		timezone = i_cal_timezone_get_builtin_timezone (location);

	if (timezone == NULL)
		timezone = i_cal_timezone_get_utc_timezone ();

	g_value_set_object (value, timezone);

	g_object_unref (settings);

	return TRUE;
}

static gboolean
etdp_settings_map_string_to_rgba (GValue *value,
				  GVariant *variant,
				  gpointer user_data)
{
	GdkRGBA rgba;
	const gchar *color_str;

	color_str = g_variant_get_string (variant, NULL);

	if (color_str && gdk_rgba_parse (&rgba, color_str))
		g_value_set_boxed (value, &rgba);
	else
		g_value_set_boxed (value, NULL);

	return TRUE;
}

static void
etdp_row_activated_cb (GtkTreeView *tree_view,
		       GtkTreePath *path,
		       GtkTreeViewColumn *column,
		       gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	model = gtk_tree_view_get_model (tree_view);

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		ECalClient *client = NULL;
		ECalComponent *comp = NULL;

		gtk_tree_model_get (model, &iter,
			COLUMN_CAL_CLIENT, &client,
			COLUMN_CAL_COMPONENT, &comp,
			-1);

		if (client && comp) {
			e_cal_ops_open_component_in_editor_sync (NULL, client,
				e_cal_component_get_icalcomponent (comp), FALSE);
		}

		g_clear_object (&client);
		g_clear_object (&comp);
	}
}

static void
etdp_source_changed_cb (ESourceRegistry *registry,
			ESource *source,
			gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if (g_hash_table_contains (to_do_pane->priv->client_colors, source)) {
		ESourceSelectable *selectable = NULL;

		if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
			selectable = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
		else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
			selectable = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);

		if (selectable) {
			GdkRGBA rgba;
			gchar *color_spec;

			color_spec = e_source_selectable_dup_color (selectable);
			if (color_spec && gdk_rgba_parse (&rgba, color_spec)) {
				GdkRGBA *current_rgba;

				current_rgba = g_hash_table_lookup (to_do_pane->priv->client_colors, source);
				if (!gdk_rgba_equal (current_rgba, &rgba)) {
					g_hash_table_insert (to_do_pane->priv->client_colors, source, gdk_rgba_copy (&rgba));
					etdp_update_colors (to_do_pane, FALSE);
				}
			}

			g_free (color_spec);
		}
	}
}

static gboolean
etdp_get_tree_view_selected_one (EToDoPane *to_do_pane,
				 ECalClient **out_client,
				 ECalComponent **out_comp)
{
	GtkTreeSelection *selection;
	GList *rows;
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	gboolean had_any = FALSE;

	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), FALSE);

	if (out_client)
		*out_client = NULL;

	if (out_comp)
		*out_comp = NULL;

	selection = gtk_tree_view_get_selection (to_do_pane->priv->tree_view);
	rows = gtk_tree_selection_get_selected_rows (selection, &model);

	if (rows && gtk_tree_model_get_iter (model, &iter, rows->data)) {
		ECalClient *client = NULL;
		ECalComponent *comp = NULL;

		gtk_tree_model_get (model, &iter,
			COLUMN_CAL_CLIENT, &client,
			COLUMN_CAL_COMPONENT, &comp,
			-1);

		if (out_client && client)
			*out_client = g_object_ref (client);

		if (out_comp && comp)
			*out_comp = g_object_ref (comp);

		had_any = client || comp;

		g_clear_object (&client);
		g_clear_object (&comp);
	}

	g_list_free_full (rows, (GDestroyNotify) gtk_tree_path_free);

	return had_any;
}

static void
etdp_new_common (EToDoPane *to_do_pane,
		 ECalClientSourceType source_type,
		 gboolean is_assigned)
{
	ECalClient *client = NULL;
	EShellView *shell_view;
	EShellWindow *shell_window;
	gchar *client_source_uid = NULL;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if (etdp_get_tree_view_selected_one (to_do_pane, &client, NULL) && client) {
		ESource *source;

		source = e_client_get_source (E_CLIENT (client));
		if (source) {
			const gchar *extension_name = NULL;

			switch (source_type) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
				extension_name = E_SOURCE_EXTENSION_CALENDAR;
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				extension_name = E_SOURCE_EXTENSION_TASK_LIST;
				break;
			default:
				break;
			}

			/* Cannot ask to create an event in a task list or vice versa. */
			if (!extension_name || !e_source_has_extension (source, extension_name))
				source = NULL;
		}

		if (source)
			client_source_uid = e_source_dup_uid (source);
	}

	g_clear_object (&client);

	shell_view = e_to_do_pane_ref_shell_view (to_do_pane);
	shell_window = shell_view ? e_shell_view_get_shell_window (shell_view) : NULL;

	if (source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
		GSettings *settings;
		time_t dtstart = 0, dtend = 0;
		GtkTreeSelection *selection;
		GList *rows;
		GtkTreeIter iter;
		GtkTreeModel *model = NULL;

		settings = e_util_ref_settings ("org.gnome.evolution.calendar");

		selection = gtk_tree_view_get_selection (to_do_pane->priv->tree_view);
		rows = gtk_tree_selection_get_selected_rows (selection, &model);

		if (rows && gtk_tree_model_get_iter (model, &iter, rows->data)) {
			GtkTreeIter parent;
			guint date_mark = 0;

			while (gtk_tree_model_iter_parent (model, &parent, &iter))
				iter = parent;

			gtk_tree_model_get (model, &iter, COLUMN_DATE_MARK, &date_mark, -1);

			if (date_mark > 0) {
				ICalTime *now;
				ICalTimezone *zone;
				gint time_divisions_secs;

				time_divisions_secs = g_settings_get_int (settings, "time-divisions") * 60;
				zone = e_cal_data_model_get_timezone (to_do_pane->priv->events_data_model);
				now = i_cal_time_new_current_with_zone (zone);
				i_cal_time_set_timezone (now, zone);

				i_cal_time_set_year (now, date_mark / 10000);
				i_cal_time_set_month (now, (date_mark / 100) % 100);
				i_cal_time_set_day (now, date_mark % 100);

				/* The date_mark is the next day, not the day it belongs to */
				i_cal_time_adjust (now, -1, 0, 0, 0);

				dtstart = i_cal_time_as_timet_with_zone (now, zone);
				if (dtstart > 0 && time_divisions_secs > 0) {
					dtstart = dtstart + time_divisions_secs - (dtstart % time_divisions_secs);
					dtend = dtstart + time_divisions_secs;
				} else {
					dtstart = 0;
				}

				g_clear_object (&now);
			}
		}

		g_list_free_full (rows, (GDestroyNotify) gtk_tree_path_free);

		e_cal_ops_new_event_editor (shell_window, client_source_uid, is_assigned, FALSE,
			g_settings_get_boolean (settings, "use-default-reminder"),
			g_settings_get_int (settings, "default-reminder-interval"),
			g_settings_get_enum (settings, "default-reminder-units"),
			dtstart, dtend);

		g_clear_object (&settings);
	} else {
		e_cal_ops_new_component_editor (shell_window, source_type, client_source_uid, is_assigned);
	}

	g_clear_object (&shell_view);
	g_free (client_source_uid);
}

static void
etdp_new_appointment_cb (GtkMenuItem *item,
			 gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	etdp_new_common (to_do_pane, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, FALSE);
}

static void
etdp_new_meeting_cb (GtkMenuItem *item,
		     gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	etdp_new_common (to_do_pane, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, TRUE);
}

static void
etdp_new_task_cb (GtkMenuItem *item,
		  gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	etdp_new_common (to_do_pane, E_CAL_CLIENT_SOURCE_TYPE_TASKS, FALSE);
}

static void
etdp_new_assigned_task_cb (GtkMenuItem *item,
			   gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	etdp_new_common (to_do_pane, E_CAL_CLIENT_SOURCE_TYPE_TASKS, TRUE);
}

static void
etdp_open_selected_cb (GtkMenuItem *item,
		       gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;
	ECalClient *client = NULL;
	ECalComponent *comp = NULL;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if (etdp_get_tree_view_selected_one (to_do_pane, &client, &comp) && client && comp) {
		e_cal_ops_open_component_in_editor_sync (NULL, client,
			e_cal_component_get_icalcomponent (comp), FALSE);
	}

	g_clear_object (&client);
	g_clear_object (&comp);
}

static void
etdp_delete_common (EToDoPane *to_do_pane,
		    ECalObjModType mod)
{
	ECalClient *client = NULL;
	ECalComponent *comp = NULL;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if (etdp_get_tree_view_selected_one (to_do_pane, &client, &comp) && client && comp) {
		GtkWindow *parent_window;
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (to_do_pane));
		parent_window = GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL;

		if (!e_cal_component_is_instance (comp))
			mod = E_CAL_OBJ_MOD_ALL;

		/* It doesn't matter which data-model is picked, because it's used
		   only for thread creation and manipulation, not for its content. */
		cal_comp_util_remove_component (parent_window, to_do_pane->priv->events_data_model,
			client, comp, mod, TRUE);
	}

	g_clear_object (&client);
	g_clear_object (&comp);
}

static void
etdp_delete_selected_cb (GtkMenuItem *item,
			 gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	etdp_delete_common (to_do_pane, E_CAL_OBJ_MOD_THIS);
}

static void
etdp_delete_this_and_future_cb (GtkMenuItem *item,
				gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	etdp_delete_common (to_do_pane, E_CAL_OBJ_MOD_THIS_AND_FUTURE);
}

static void
etdp_delete_series_cb (GtkMenuItem *item,
		       gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	etdp_delete_common (to_do_pane, E_CAL_OBJ_MOD_ALL);
}

typedef struct _MarkCompleteData {
	ECalClient *client;
	ECalComponent *comp;
} MarkCompleteData;

static void
mark_complete_data_free (gpointer ptr)
{
	MarkCompleteData *mcd = ptr;

	if (mcd) {
		g_clear_object (&mcd->client);
		g_clear_object (&mcd->comp);
		g_free (mcd);
	}
}

static void
etdp_mark_task_complete_thread (EAlertSinkThreadJobData *job_data,
				gpointer user_data,
				GCancellable *cancellable,
				GError **error)
{
	MarkCompleteData *mcd = user_data;
	ICalComponent *icomp;

	g_return_if_fail (mcd != NULL);

	icomp = e_cal_component_get_icalcomponent (mcd->comp);

	if (e_cal_util_mark_task_complete_sync (icomp, -1, mcd->client, cancellable, error))
		e_cal_client_modify_object_sync (mcd->client, icomp, E_CAL_OBJ_MOD_ALL, E_CAL_OPERATION_FLAG_NONE, cancellable, error);
}

static void
etdp_mark_task_as_complete_cb (GtkMenuItem *item,
			       gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;
	ECalClient *client = NULL;
	ECalComponent *comp = NULL;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if (etdp_get_tree_view_selected_one (to_do_pane, &client, &comp) && client && comp) {
		ESource *source;
		GCancellable *cancellable;
		MarkCompleteData *mcd;
		gchar *display_name;

		source = e_client_get_source (E_CLIENT (client));
		display_name = e_util_get_source_full_name (e_source_registry_watcher_get_registry (to_do_pane->priv->watcher), source);

		mcd = g_new0 (MarkCompleteData, 1);
		mcd->client = g_steal_pointer (&client);
		mcd->comp = g_steal_pointer (&comp);

		/* It doesn't matter which data-model is picked, because it's used
		   only for thread creation and manipulation, not for its content. */
		cancellable = e_cal_data_model_submit_thread_job (to_do_pane->priv->tasks_data_model, _("Marking a task as complete"),
			"calendar:failed-modify-task", display_name, etdp_mark_task_complete_thread, mcd, mark_complete_data_free);

		g_clear_object (&cancellable);
		g_free (display_name);
	}

	g_clear_object (&client);
	g_clear_object (&comp);
}

static void
etdp_show_tasks_without_due_date_cb (GtkCheckMenuItem *check_menu_item,
				     gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	e_to_do_pane_set_show_no_duedate_tasks (to_do_pane, !e_to_do_pane_get_show_no_duedate_tasks (to_do_pane));
}

static void
etdp_fill_popup_menu (EToDoPane *to_do_pane,
		      GtkMenu *menu)
{
	GtkWidget *item;
	GtkMenuShell *menu_shell;
	ECalClient *client = NULL;
	ECalComponent *comp = NULL;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));
	g_return_if_fail (GTK_IS_MENU (menu));

	etdp_get_tree_view_selected_one (to_do_pane, &client, &comp);

	menu_shell = GTK_MENU_SHELL (menu);

	item = gtk_image_menu_item_new_with_mnemonic (_("New _Appointment…"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
		gtk_image_new_from_icon_name ("appointment-new", GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
		G_CALLBACK (etdp_new_appointment_cb), to_do_pane);
	gtk_widget_show (item);
	gtk_menu_shell_append (menu_shell, item);

	item = gtk_image_menu_item_new_with_mnemonic (_("New _Meeting…"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
		gtk_image_new_from_icon_name ("stock_people", GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
		G_CALLBACK (etdp_new_meeting_cb), to_do_pane);
	gtk_widget_show (item);
	gtk_menu_shell_append (menu_shell, item);

	item = gtk_image_menu_item_new_with_mnemonic (_("New _Task…"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
		gtk_image_new_from_icon_name ("stock_task", GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
		G_CALLBACK (etdp_new_task_cb), to_do_pane);
	gtk_widget_show (item);
	gtk_menu_shell_append (menu_shell, item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_New Assigned Task…"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
		gtk_image_new_from_icon_name ("stock_task-assigned-to", GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
		G_CALLBACK (etdp_new_assigned_task_cb), to_do_pane);
	gtk_widget_show (item);
	gtk_menu_shell_append (menu_shell, item);

	if (client && comp) {
		item = gtk_separator_menu_item_new ();
		gtk_widget_show (item);
		gtk_menu_shell_append (menu_shell, item);

		item = gtk_image_menu_item_new_with_mnemonic (_("_Open…"));
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
			gtk_image_new_from_icon_name ("document-open", GTK_ICON_SIZE_MENU));
		g_signal_connect (item, "activate",
			G_CALLBACK (etdp_open_selected_cb), to_do_pane);
		gtk_widget_show (item);
		gtk_menu_shell_append (menu_shell, item);

		if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO &&
		    !e_cal_util_component_has_property (e_cal_component_get_icalcomponent (comp), I_CAL_COMPLETED_PROPERTY)) {
			item = gtk_menu_item_new_with_mnemonic (_("Mark Task as _Complete"));
			g_signal_connect (item, "activate",
				G_CALLBACK (etdp_mark_task_as_complete_cb), to_do_pane);
			gtk_widget_show (item);
			gtk_menu_shell_append (menu_shell, item);
		}

		item = gtk_separator_menu_item_new ();
		gtk_widget_show (item);
		gtk_menu_shell_append (menu_shell, item);

		if (!e_client_is_readonly (E_CLIENT (client))) {
			if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_EVENT &&
			    e_cal_component_is_instance (comp)) {
				item = gtk_image_menu_item_new_with_mnemonic (_("_Delete This Instance…"));
				gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
					gtk_image_new_from_icon_name ("edit-delete", GTK_ICON_SIZE_MENU));
				g_signal_connect (item, "activate",
					G_CALLBACK (etdp_delete_selected_cb), to_do_pane);
				gtk_widget_show (item);
				gtk_menu_shell_append (menu_shell, item);

				if (!e_client_check_capability (E_CLIENT (client), E_CAL_STATIC_CAPABILITY_NO_THISANDFUTURE)) {
					item = gtk_image_menu_item_new_with_mnemonic (_("Delete This and F_uture Occurrences…"));
					gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
						gtk_image_new_from_icon_name ("edit-delete", GTK_ICON_SIZE_MENU));
					g_signal_connect (item, "activate",
						G_CALLBACK (etdp_delete_this_and_future_cb), to_do_pane);
					gtk_widget_show (item);
					gtk_menu_shell_append (menu_shell, item);
				}

				item = gtk_image_menu_item_new_with_mnemonic (_("D_elete All Instances…"));
				gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
					gtk_image_new_from_icon_name ("edit-delete", GTK_ICON_SIZE_MENU));
				g_signal_connect (item, "activate",
					G_CALLBACK (etdp_delete_series_cb), to_do_pane);
				gtk_widget_show (item);
				gtk_menu_shell_append (menu_shell, item);
			} else {
				item = gtk_image_menu_item_new_with_mnemonic (_("_Delete…"));
				gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
					gtk_image_new_from_icon_name ("edit-delete", GTK_ICON_SIZE_MENU));
				g_signal_connect (item, "activate",
					G_CALLBACK (etdp_delete_series_cb), to_do_pane);
				gtk_widget_show (item);
				gtk_menu_shell_append (menu_shell, item);
			}
		}
	}

	g_clear_object (&client);
	g_clear_object (&comp);

	item = gtk_separator_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_shell_append (menu_shell, item);

	item = gtk_check_menu_item_new_with_mnemonic (_("_Show Tasks without Due date"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), to_do_pane->priv->show_no_duedate_tasks);
	g_signal_connect (item, "toggled",
		G_CALLBACK (etdp_show_tasks_without_due_date_cb), to_do_pane);
	gtk_widget_show (item);
	gtk_menu_shell_append (menu_shell, item);
}

static void
etdp_popup_menu (EToDoPane *to_do_pane,
		 GdkEvent *event)
{
	GtkMenu *menu;

	menu = GTK_MENU (gtk_menu_new ());

	etdp_fill_popup_menu (to_do_pane, menu);

	gtk_menu_attach_to_widget (menu, GTK_WIDGET (to_do_pane->priv->tree_view), NULL);
	g_signal_connect (menu, "deactivate", G_CALLBACK (gtk_menu_detach), NULL);
	gtk_menu_popup_at_pointer (menu, event);
}

static gboolean
etdp_button_press_event_cb (GtkWidget *widget,
			    GdkEvent *event,
			    gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), FALSE);

	if (event->type == GDK_BUTTON_PRESS &&
	    gdk_event_triggers_context_menu (event)) {
		GtkTreeSelection *selection;
		GtkTreePath *path;

		selection = gtk_tree_view_get_selection (to_do_pane->priv->tree_view);
		if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE)
			gtk_tree_selection_unselect_all (selection);

		if (gtk_tree_view_get_path_at_pos (to_do_pane->priv->tree_view, event->button.x, event->button.y, &path, NULL, NULL, NULL)) {
			gtk_tree_selection_select_path (selection, path);
			gtk_tree_view_set_cursor (to_do_pane->priv->tree_view, path, NULL, FALSE);

			gtk_tree_path_free (path);
		}

		etdp_popup_menu (to_do_pane, event);

		return TRUE;
	}

	return FALSE;
}

static gboolean
etdp_popup_menu_cb (GtkWidget *widget,
		    gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), FALSE);

	etdp_popup_menu (to_do_pane, NULL);

	return TRUE;
}

static void
etdp_notify_visible_cb (EToDoPane *to_do_pane,
			GParamSpec *param,
			gpointer user_data)
{
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if (gtk_widget_get_visible (GTK_WIDGET (to_do_pane))) {
		e_source_registry_watcher_reclaim (to_do_pane->priv->watcher);
	} else {
		GList *clients, *link;

		clients = e_cal_data_model_get_clients (to_do_pane->priv->events_data_model);
		for (link = clients; link; link = g_list_next (link)) {
			ECalClient *client = link->data;
			ESource *source = e_client_get_source (E_CLIENT (client));

			e_cal_data_model_remove_client (to_do_pane->priv->events_data_model, e_source_get_uid (source));
		}
		g_list_free_full (clients, g_object_unref);

		clients = e_cal_data_model_get_clients (to_do_pane->priv->tasks_data_model);
		for (link = clients; link; link = g_list_next (link)) {
			ECalClient *client = link->data;
			ESource *source = e_client_get_source (E_CLIENT (client));

			e_cal_data_model_remove_client (to_do_pane->priv->tasks_data_model, e_source_get_uid (source));
		}
		g_list_free_full (clients, g_object_unref);
	}
}

static void
etdp_datetime_format_changed_cb (const gchar *component,
				 const gchar *part,
				 DTFormatKind kind,
				 gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if ((kind == DTFormatKindDate || kind == DTFormatKindDateTime) &&
	    g_strcmp0 (component, "calendar") == 0 &&
	    g_strcmp0 (part, "table") == 0) {
		etdp_update_day_labels (to_do_pane);
		etdp_update_comps (to_do_pane);
	}
}

static void
e_to_do_pane_set_shell_view (EToDoPane *to_do_pane,
			     EShellView *shell_view)
{
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	g_weak_ref_set (&to_do_pane->priv->shell_view_weakref, shell_view);
}

static void
e_to_do_pane_set_property (GObject *object,
			   guint property_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HIGHLIGHT_OVERDUE:
			e_to_do_pane_set_highlight_overdue (
				E_TO_DO_PANE (object),
				g_value_get_boolean (value));
			return;

		case PROP_OVERDUE_COLOR:
			e_to_do_pane_set_overdue_color (
				E_TO_DO_PANE (object),
				g_value_get_boxed (value));
			return;

		case PROP_SHELL_VIEW:
			e_to_do_pane_set_shell_view (
				E_TO_DO_PANE (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_COMPLETED_TASKS:
			e_to_do_pane_set_show_completed_tasks (
				E_TO_DO_PANE (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_NO_DUEDATE_TASKS:
			e_to_do_pane_set_show_no_duedate_tasks (
				E_TO_DO_PANE (object),
				g_value_get_boolean (value));
			return;

		case PROP_USE_24HOUR_FORMAT:
			e_to_do_pane_set_use_24hour_format (
				E_TO_DO_PANE (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_N_DAYS:
			e_to_do_pane_set_show_n_days (
				E_TO_DO_PANE (object),
				g_value_get_uint (value));
			return;

		case PROP_TIME_IN_SMALLER_FONT:
			e_to_do_pane_set_time_in_smaller_font (
				E_TO_DO_PANE (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_to_do_pane_get_property (GObject *object,
			   guint property_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HIGHLIGHT_OVERDUE:
			g_value_set_boolean (
				value,
				e_to_do_pane_get_highlight_overdue (
				E_TO_DO_PANE (object)));
			return;

		case PROP_OVERDUE_COLOR:
			g_value_set_boxed (
				value,
				e_to_do_pane_get_overdue_color (
				E_TO_DO_PANE (object)));
			return;

		case PROP_SHELL_VIEW:
			g_value_set_object (
				value,
				e_to_do_pane_ref_shell_view (
				E_TO_DO_PANE (object)));
			return;

		case PROP_SHOW_COMPLETED_TASKS:
			g_value_set_boolean (
				value,
				e_to_do_pane_get_show_completed_tasks (
				E_TO_DO_PANE (object)));
			return;

		case PROP_SHOW_NO_DUEDATE_TASKS:
			g_value_set_boolean (
				value,
				e_to_do_pane_get_show_no_duedate_tasks (
				E_TO_DO_PANE (object)));
			return;

		case PROP_USE_24HOUR_FORMAT:
			g_value_set_boolean (
				value,
				e_to_do_pane_get_use_24hour_format (
				E_TO_DO_PANE (object)));
			return;

		case PROP_SHOW_N_DAYS:
			g_value_set_uint (
				value,
				e_to_do_pane_get_show_n_days (
				E_TO_DO_PANE (object)));
			return;

		case PROP_TIME_IN_SMALLER_FONT:
			g_value_set_boolean (
				value,
				e_to_do_pane_get_time_in_smaller_font (
				E_TO_DO_PANE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_to_do_pane_constructed (GObject *object)
{
	EToDoPane *to_do_pane = E_TO_DO_PANE (object);
	EShellView *shell_view;
	EShell *shell;
	ESourceRegistry *registry;
	GtkGrid *grid;
	GtkWidget *widget;
	GtkCellRenderer *renderer;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeModel *model, *sort_model;
	GtkTreeIter iter;
	GSettings *settings;
	PangoAttrList *bold;
	guint ii;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_to_do_pane_parent_class)->constructed (object);

	shell_view = e_to_do_pane_ref_shell_view (to_do_pane);
	shell = e_shell_backend_get_shell (e_shell_view_get_shell_backend (shell_view));
	registry = e_shell_get_registry (shell);

	to_do_pane->priv->client_cache = g_object_ref (e_shell_get_client_cache (shell));
	to_do_pane->priv->watcher = e_source_registry_watcher_new (registry, NULL);
	to_do_pane->priv->source_changed_id =
		g_signal_connect (registry, "source-changed",
			G_CALLBACK (etdp_source_changed_cb), to_do_pane);

	g_signal_connect (to_do_pane->priv->watcher, "filter",
		G_CALLBACK (e_to_do_pane_watcher_filter_cb), NULL);

	g_signal_connect (to_do_pane->priv->watcher, "appeared",
		G_CALLBACK (e_to_do_pane_watcher_appeared_cb), to_do_pane);

	g_signal_connect (to_do_pane->priv->watcher, "disappeared",
		G_CALLBACK (e_to_do_pane_watcher_disappeared_cb), to_do_pane);

	to_do_pane->priv->tree_store = GTK_TREE_STORE (gtk_tree_store_new (N_COLUMNS,
		GDK_TYPE_RGBA,		/* COLUMN_BGCOLOR */
		GDK_TYPE_RGBA,		/* COLUMN_FGCOLOR */
		G_TYPE_BOOLEAN,		/* COLUMN_HAS_ICON_NAME */
		G_TYPE_STRING,		/* COLUMN_ICON_NAME */
		G_TYPE_STRING,		/* COLUMN_SUMMARY */
		G_TYPE_STRING,		/* COLUMN_TOOLTIP */
		G_TYPE_STRING,		/* COLUMN_SORTKEY */
		G_TYPE_UINT,		/* COLUMN_DATE_MARK */
		E_TYPE_CAL_CLIENT,	/* COLUMN_CAL_CLIENT */
		E_TYPE_CAL_COMPONENT));	/* COLUMN_CAL_COMPONENT */

	grid = GTK_GRID (to_do_pane);

	bold = pango_attr_list_new ();
	pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

	widget = gtk_label_new (_("To Do"));
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_CENTER,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"attributes", bold,
		NULL);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	pango_attr_list_unref (bold);

	model = GTK_TREE_MODEL (to_do_pane->priv->tree_store);

	sort_model = gtk_tree_model_sort_new_with_model (model);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model), COLUMN_SORTKEY, GTK_SORT_ASCENDING);

	widget = gtk_tree_view_new_with_model (sort_model);

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);

	tree_view = GTK_TREE_VIEW (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (tree_view));

	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);

	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_pixbuf_new ();

	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_set_attributes (column, renderer,
		"icon-name", COLUMN_ICON_NAME,
		"visible", COLUMN_HAS_ICON_NAME,
		NULL);

	renderer = gtk_cell_renderer_text_new ();

	g_object_set (G_OBJECT (renderer),
		"ellipsize", PANGO_ELLIPSIZE_END,
		NULL);

	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	gtk_tree_view_column_set_attributes (column, renderer,
		"markup", COLUMN_SUMMARY,
		"background-rgba", COLUMN_BGCOLOR,
		"foreground-rgba", COLUMN_FGCOLOR,
		NULL);

	gtk_tree_view_append_column (tree_view, column);
	gtk_tree_view_set_expander_column (tree_view, column);

	for (ii = 0; ii < to_do_pane->priv->roots->len - 1; ii++) {
		GtkTreePath *path;
		gchar *sort_key;

		sort_key = g_strdup_printf ("A%05u", ii);

		gtk_tree_store_append (to_do_pane->priv->tree_store, &iter, NULL);
		gtk_tree_store_set (to_do_pane->priv->tree_store, &iter,
			COLUMN_SORTKEY, sort_key,
			COLUMN_HAS_ICON_NAME, FALSE,
			-1);

		g_free (sort_key);

		path = gtk_tree_model_get_path (model, &iter);

		to_do_pane->priv->roots->pdata[ii] = gtk_tree_row_reference_new (model, path);
		g_warn_if_fail (to_do_pane->priv->roots->pdata[ii] != NULL);

		gtk_tree_path_free (path);
	}

	gtk_tree_view_set_headers_visible (tree_view, FALSE);
	gtk_tree_view_set_tooltip_column (tree_view, COLUMN_TOOLTIP);

	gtk_widget_show_all (GTK_WIDGET (grid));

	to_do_pane->priv->events_data_model = e_cal_data_model_new (registry, e_to_do_pane_submit_thread_job, G_OBJECT (to_do_pane));
	to_do_pane->priv->tasks_data_model = e_cal_data_model_new (registry, e_to_do_pane_submit_thread_job, G_OBJECT (to_do_pane));
	to_do_pane->priv->time_checker_id = g_timeout_add_seconds (60, etdp_check_time_cb, to_do_pane);

	e_cal_data_model_set_expand_recurrences (to_do_pane->priv->events_data_model, TRUE);
	e_cal_data_model_set_expand_recurrences (to_do_pane->priv->tasks_data_model, FALSE);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind_with_mapping (
		settings, "timezone",
		to_do_pane->priv->events_data_model, "timezone",
		G_SETTINGS_BIND_GET,
		etdp_settings_map_string_to_icaltimezone,
		NULL, /* one-way binding */
		NULL, NULL);

	g_settings_bind_with_mapping (
		settings, "timezone",
		to_do_pane->priv->tasks_data_model, "timezone",
		G_SETTINGS_BIND_GET,
		etdp_settings_map_string_to_icaltimezone,
		NULL, /* one-way binding */
		NULL, NULL);

	g_settings_bind (
		settings, "hide-cancelled-events",
		to_do_pane->priv->events_data_model, "skip-cancelled",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "task-overdue-highlight",
		to_do_pane, "highlight-overdue",
		G_SETTINGS_BIND_GET);

	g_settings_bind_with_mapping (
		settings, "task-overdue-color",
		to_do_pane, "overdue-color",
		G_SETTINGS_BIND_GET,
		etdp_settings_map_string_to_rgba,
		NULL, /* one-way binding */
		NULL, NULL);

	g_settings_bind (
		settings, "use-24hour-format",
		to_do_pane, "use-24hour-format",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	g_signal_connect (to_do_pane->priv->events_data_model, "notify::timezone",
		G_CALLBACK (etdp_timezone_changed_cb), to_do_pane);

	g_signal_connect (tree_view, "row-activated",
		G_CALLBACK (etdp_row_activated_cb), to_do_pane);

	g_signal_connect (tree_view, "button-press-event",
		G_CALLBACK (etdp_button_press_event_cb), to_do_pane);

	g_signal_connect (tree_view, "popup-menu",
		G_CALLBACK (etdp_popup_menu_cb), to_do_pane);

	to_do_pane->priv->tree_view = tree_view;

	etdp_check_time_changed (to_do_pane, TRUE);

	g_clear_object (&shell_view);
	g_clear_object (&sort_model);

	g_signal_connect (to_do_pane, "notify::visible",
		G_CALLBACK (etdp_notify_visible_cb), NULL);

	if (gtk_widget_get_visible (GTK_WIDGET (to_do_pane)))
		e_source_registry_watcher_reclaim (to_do_pane->priv->watcher);

	e_datetime_format_add_change_listener (etdp_datetime_format_changed_cb, to_do_pane);
}

static void
e_to_do_pane_dispose (GObject *object)
{
	EToDoPane *to_do_pane = E_TO_DO_PANE (object);
	guint ii;

	e_datetime_format_remove_change_listener (etdp_datetime_format_changed_cb, to_do_pane);

	if (to_do_pane->priv->cancellable) {
		g_cancellable_cancel (to_do_pane->priv->cancellable);
		g_clear_object (&to_do_pane->priv->cancellable);
	}

	if (to_do_pane->priv->time_checker_id) {
		g_source_remove (to_do_pane->priv->time_checker_id);
		to_do_pane->priv->time_checker_id = 0;
	}

	if (to_do_pane->priv->source_changed_id) {
		g_signal_handler_disconnect (e_source_registry_watcher_get_registry (to_do_pane->priv->watcher),
			to_do_pane->priv->source_changed_id);
		to_do_pane->priv->source_changed_id = 0;
	}

	for (ii = 0; ii < to_do_pane->priv->roots->len; ii++) {
		gtk_tree_row_reference_free (to_do_pane->priv->roots->pdata[ii]);
		to_do_pane->priv->roots->pdata[ii] = NULL;
	}

	g_hash_table_remove_all (to_do_pane->priv->component_refs);
	g_hash_table_remove_all (to_do_pane->priv->client_colors);

	g_clear_object (&to_do_pane->priv->client_cache);
	g_clear_object (&to_do_pane->priv->watcher);
	g_clear_object (&to_do_pane->priv->tree_store);
	g_clear_object (&to_do_pane->priv->events_data_model);
	g_clear_object (&to_do_pane->priv->tasks_data_model);

	g_weak_ref_set (&to_do_pane->priv->shell_view_weakref, NULL);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_to_do_pane_parent_class)->dispose (object);
}

static void
e_to_do_pane_finalize (GObject *object)
{
	EToDoPane *to_do_pane = E_TO_DO_PANE (object);

	g_weak_ref_clear (&to_do_pane->priv->shell_view_weakref);

	g_hash_table_destroy (to_do_pane->priv->component_refs);
	g_hash_table_destroy (to_do_pane->priv->client_colors);
	g_ptr_array_unref (to_do_pane->priv->roots);

	if (to_do_pane->priv->overdue_color)
		gdk_rgba_free (to_do_pane->priv->overdue_color);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_to_do_pane_parent_class)->finalize (object);
}

static void
e_to_do_pane_init (EToDoPane *to_do_pane)
{
	to_do_pane->priv = e_to_do_pane_get_instance_private (to_do_pane);
	to_do_pane->priv->cancellable = g_cancellable_new ();
	to_do_pane->priv->roots = g_ptr_array_new ();

	to_do_pane->priv->component_refs = g_hash_table_new_full (component_ident_hash, component_ident_equal,
		component_ident_free, etdp_free_component_refs);

	to_do_pane->priv->client_colors = g_hash_table_new_full (g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) gdk_rgba_free);

	to_do_pane->priv->nearest_due = (time_t) -1;

	g_weak_ref_init (&to_do_pane->priv->shell_view_weakref, NULL);
}

static void
e_to_do_pane_class_init (EToDoPaneClass *klass)
{
	GObjectClass *object_class;

	gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "EToDoPane");

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_to_do_pane_set_property;
	object_class->get_property = e_to_do_pane_get_property;
	object_class->constructed = e_to_do_pane_constructed;
	object_class->dispose = e_to_do_pane_dispose;
	object_class->finalize = e_to_do_pane_finalize;

	g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHT_OVERDUE,
		g_param_spec_boolean (
			"highlight-overdue",
			"Highlight Overdue Tasks",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_OVERDUE_COLOR,
		g_param_spec_boxed (
			"overdue-color",
			"Overdue Color",
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			"EShellView",
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_COMPLETED_TASKS,
		g_param_spec_boolean (
			"show-completed-tasks",
			"Show Completed Tasks",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_NO_DUEDATE_TASKS,
		g_param_spec_boolean (
			"show-no-duedate-tasks",
			"Show tasks without Due date",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_USE_24HOUR_FORMAT,
		g_param_spec_boolean (
			"use-24hour-format",
			"Use 24hour Format",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_N_DAYS,
		g_param_spec_uint (
			"show-n-days",
			"show-n-days",
			NULL,
			0, G_MAXUINT, 8,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TIME_IN_SMALLER_FONT,
		g_param_spec_boolean (
			"time-in-smaller-font",
			"Time In Smaller Font",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
e_to_do_pane_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface)
{
	iface->component_added = etdp_data_subscriber_component_added;
	iface->component_modified = etdp_data_subscriber_component_modified;
	iface->component_removed = etdp_data_subscriber_component_removed;
	iface->freeze = etdp_data_subscriber_freeze;
	iface->thaw = etdp_data_subscriber_thaw;
}

/**
 * e_to_do_pane_new:
 * @shell_view: an #EShellView
 *
 * Creates a new #EToDoPane.
 *
 * Returns: (transfer full): A new #EToDoPane.
 *
 * Since: 3.26
 **/
GtkWidget *
e_to_do_pane_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (E_TYPE_TO_DO_PANE,
		"shell-view", shell_view,
		NULL);
}

/**
 * e_to_do_pane_ref_shell_view:
 * @to_do_pane: an #EToDoPane
 *
 * Returns: (transfer full): an #EShellView used to create the @to_do_pane with added reference.
 *    Free it with g_object_unref() when no longer needed.
 *
 * Since: 3.26
 **/
EShellView *
e_to_do_pane_ref_shell_view (EToDoPane *to_do_pane)
{
	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), NULL);

	return g_weak_ref_get (&to_do_pane->priv->shell_view_weakref);
}

/**
 * e_to_do_pane_get_highlight_overdue:
 * @to_do_pane: an #EToDoPane
 *
 * Returns: Whether highlights overdue tasks with overdue-color.
 *
 * Since: 3.26
 **/
gboolean
e_to_do_pane_get_highlight_overdue (EToDoPane *to_do_pane)
{
	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), FALSE);

	return to_do_pane->priv->highlight_overdue;
}

/**
 * e_to_do_pane_set_highlight_overdue:
 * @to_do_pane: an #EToDoPane
 * @highlight_overdue: a value to set
 *
 * Sets whether should highlight overdue tasks with overdue-color.
 *
 * Since: 3.26
 **/
void
e_to_do_pane_set_highlight_overdue (EToDoPane *to_do_pane,
				    gboolean highlight_overdue)
{
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if ((to_do_pane->priv->highlight_overdue ? 1 : 0) == (highlight_overdue ? 1 : 0))
		return;

	to_do_pane->priv->highlight_overdue = highlight_overdue;

	if (to_do_pane->priv->overdue_color)
		etdp_update_colors (to_do_pane, TRUE);

	g_object_notify (G_OBJECT (to_do_pane), "highlight-overdue");
}

/**
 * e_to_do_pane_get_overdue_color:
 * @to_do_pane: an #EToDoPane
 *
 * Returns: (transfer none) (nullable): Currently set color to use for overdue tasks.
 *
 * Since: 3.26
 **/
const GdkRGBA *
e_to_do_pane_get_overdue_color (EToDoPane *to_do_pane)
{
	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), NULL);

	return to_do_pane->priv->overdue_color;
}

/**
 * e_to_do_pane_set_overdue_color:
 * @to_do_pane: an #EToDoPane
 * @overdue_color: (nullable): a color to set, or %NULL
 *
 * Sets a color to use for overdue tasks, or unsets the previous,
 * when it's %NULL.
 *
 * Since: 3.26
 **/
void
e_to_do_pane_set_overdue_color (EToDoPane *to_do_pane,
				const GdkRGBA *overdue_color)
{
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if (to_do_pane->priv->overdue_color == overdue_color ||
	    (to_do_pane->priv->overdue_color && overdue_color &&
	     gdk_rgba_equal (to_do_pane->priv->overdue_color, overdue_color)))
		return;

	g_clear_pointer (&to_do_pane->priv->overdue_color, gdk_rgba_free);

	if (overdue_color)
		to_do_pane->priv->overdue_color = gdk_rgba_copy (overdue_color);

	if (to_do_pane->priv->highlight_overdue)
		etdp_update_colors (to_do_pane, TRUE);

	g_object_notify (G_OBJECT (to_do_pane), "overdue-color");
}

/**
 * e_to_do_pane_get_show_completed_tasks:
 * @to_do_pane: an #EToDoPane
 *
 * Returns: Whether completed tasks should be shown in the view.
 *
 * Since: 3.26
 **/
gboolean
e_to_do_pane_get_show_completed_tasks (EToDoPane *to_do_pane)
{
	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), FALSE);

	return to_do_pane->priv->show_completed_tasks;
}

/**
 * e_to_do_pane_set_show_completed_tasks:
 * @to_do_pane: an #EToDoPane
 * @show_completed_tasks: a value to set
 *
 * Sets whether completed tasks should be shown in the view.
 *
 * Since: 3.26
 **/
void
e_to_do_pane_set_show_completed_tasks (EToDoPane *to_do_pane,
				       gboolean show_completed_tasks)
{
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if ((to_do_pane->priv->show_completed_tasks ? 1 : 0) == (show_completed_tasks ? 1 : 0))
		return;

	to_do_pane->priv->show_completed_tasks = show_completed_tasks;

	etdp_update_queries (to_do_pane);

	g_object_notify (G_OBJECT (to_do_pane), "show-completed-tasks");
}

/**
 * e_to_do_pane_get_show_no_duedate_tasks:
 * @to_do_pane: an #EToDoPane
 *
 * Returns: Whether tasks without Due date should be shown in the view.
 *
 * Since: 3.28
 **/
gboolean
e_to_do_pane_get_show_no_duedate_tasks (EToDoPane *to_do_pane)
{
	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), FALSE);

	return to_do_pane->priv->show_no_duedate_tasks;
}

/**
 * e_to_do_pane_set_show_no_duedate_tasks:
 * @to_do_pane: an #EToDoPane
 * @show_no_duedate_tasks: a value to set
 *
 * Sets whether tasks without Due date should be shown in the view.
 *
 * Since: 3.28
 **/
void
e_to_do_pane_set_show_no_duedate_tasks (EToDoPane *to_do_pane,
					gboolean show_no_duedate_tasks)
{
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if ((to_do_pane->priv->show_no_duedate_tasks ? 1 : 0) == (show_no_duedate_tasks ? 1 : 0))
		return;

	to_do_pane->priv->show_no_duedate_tasks = show_no_duedate_tasks;

	etdp_update_queries (to_do_pane);

	g_object_notify (G_OBJECT (to_do_pane), "show-no-duedate-tasks");
}

/**
 * e_to_do_pane_get_use_24hour_format:
 * @to_do_pane: an #EToDoPane
 *
 * Returns: Whether uses 24-hour format for time display.
 *
 * Since: 3.26
 **/
gboolean
e_to_do_pane_get_use_24hour_format (EToDoPane *to_do_pane)
{
	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), FALSE);

	return to_do_pane->priv->use_24hour_format;
}

/**
 * e_to_do_pane_set_use_24hour_format:
 * @to_do_pane: an #EToDoPane
 * @use_24hour_format: a value to set
 *
 * Sets whether to use 24-hour format for time display.
 *
 * Since: 3.26
 **/
void
e_to_do_pane_set_use_24hour_format (EToDoPane *to_do_pane,
				    gboolean use_24hour_format)
{
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if ((to_do_pane->priv->use_24hour_format ? 1 : 0) == (use_24hour_format ? 1 : 0))
		return;

	to_do_pane->priv->use_24hour_format = use_24hour_format;

	etdp_update_comps (to_do_pane);

	g_object_notify (G_OBJECT (to_do_pane), "use-24hour-format");
}

/**
 * e_to_do_pane_get_show_n_days:
 * @to_do_pane: an #EToDoPane
 *
 * Returns: How many days are shown in the @to_do_pane.
 *
 * Since: 3.42
 **/
guint
e_to_do_pane_get_show_n_days (EToDoPane *to_do_pane)
{
	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), 0);

	/* The 1 is for the no-due-day-tasks */
	return to_do_pane->priv->roots->len ? to_do_pane->priv->roots->len - 1 : 0;
}

/**
 * e_to_do_pane_set_show_n_days:
 * @to_do_pane: an #EToDoPane
 * @show_n_days: a value to set
 *
 * Sets how many days to show in the @to_do_pane. The value out range is clamped.
 *
 * Since: 3.42
 **/
void
e_to_do_pane_set_show_n_days (EToDoPane *to_do_pane,
			      guint show_n_days)
{
	GtkTreeModel *model;
	GtkTreeRowReference *last_rowref = NULL;
	guint old_len, ii;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if (show_n_days < E_TO_DO_PANE_MIN_SHOW_N_DAYS)
		show_n_days = E_TO_DO_PANE_MIN_SHOW_N_DAYS;
	else if (show_n_days > E_TO_DO_PANE_MAX_SHOW_N_DAYS)
		show_n_days = E_TO_DO_PANE_MAX_SHOW_N_DAYS;

	/* One more, for the no-due-day-tasks */
	show_n_days++;

	if (to_do_pane->priv->roots->len == show_n_days)
		return;

	model = to_do_pane->priv->tree_store ? GTK_TREE_MODEL (to_do_pane->priv->tree_store) : NULL;

	/* Remember the rowref for the no-due-day-tasks, to move it to the end later */
	if (to_do_pane->priv->roots->len) {
		last_rowref = g_ptr_array_index (to_do_pane->priv->roots, to_do_pane->priv->roots->len - 1);
		g_ptr_array_remove_index (to_do_pane->priv->roots, to_do_pane->priv->roots->len - 1);
	}

	if (to_do_pane->priv->roots->len >= show_n_days) {
		for (ii = show_n_days - 1; ii < to_do_pane->priv->roots->len; ii++) {
			GtkTreeRowReference *rowref;

			rowref = g_ptr_array_index (to_do_pane->priv->roots, ii);

			if (rowref) {
				if (gtk_tree_row_reference_valid (rowref)) {
					GtkTreePath *ref_path;
					GtkTreeIter iter;

					ref_path = gtk_tree_row_reference_get_path (rowref);

					if (ref_path && gtk_tree_model_get_iter (model, &iter, ref_path))
						gtk_tree_store_remove (to_do_pane->priv->tree_store, &iter);

					gtk_tree_path_free (ref_path);
				}

				gtk_tree_row_reference_free (rowref);
				to_do_pane->priv->roots->pdata[ii] = NULL;
			}
		}
	}

	old_len = to_do_pane->priv->roots->len;
	g_ptr_array_set_size (to_do_pane->priv->roots, show_n_days);
	to_do_pane->priv->roots->pdata[to_do_pane->priv->roots->len - 1] = last_rowref;

	if (to_do_pane->priv->tree_store) {
		for (ii = old_len; ii < show_n_days - 1; ii++) {
			GtkTreeRowReference *rowref;
			GtkTreePath *path;
			GtkTreeIter iter;
			gchar *sort_key;

			sort_key = g_strdup_printf ("A%05u", ii);

			gtk_tree_store_append (to_do_pane->priv->tree_store, &iter, NULL);
			gtk_tree_store_set (to_do_pane->priv->tree_store, &iter,
				COLUMN_SORTKEY, sort_key,
				COLUMN_HAS_ICON_NAME, FALSE,
				-1);

			g_free (sort_key);

			path = gtk_tree_model_get_path (model, &iter);
			rowref = gtk_tree_row_reference_new (model, path);

			to_do_pane->priv->roots->pdata[ii] = rowref;
			g_warn_if_fail (rowref != NULL);

			gtk_tree_path_free (path);
		}

		if (last_rowref) {
			GtkTreePath *ref_path;
			GtkTreeIter iter;

			ref_path = gtk_tree_row_reference_get_path (last_rowref);

			if (ref_path && gtk_tree_model_get_iter (model, &iter, ref_path)) {
				gchar *sort_key;

				sort_key = g_strdup_printf ("A%05u", to_do_pane->priv->roots->len - 1);

				gtk_tree_store_set (to_do_pane->priv->tree_store, &iter,
					COLUMN_SORTKEY, sort_key,
					-1);

				g_free (sort_key);

				gtk_tree_store_move_before (to_do_pane->priv->tree_store, &iter, NULL);
			}

			gtk_tree_path_free (ref_path);
		}

		etdp_update_queries (to_do_pane);
	}

	g_object_notify (G_OBJECT (to_do_pane), "show-n-days");
}

/**
 * e_to_do_pane_get_time_in_smaller_font:
 * @to_do_pane: an #EToDoPane
 *
 * Returns: Whether the time should be shown in smaller font.
 *
 * Since: 3.56
 **/
gboolean
e_to_do_pane_get_time_in_smaller_font (EToDoPane *to_do_pane)
{
	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), FALSE);

	return to_do_pane->priv->time_in_smaller_font;
}

/**
 * e_to_do_pane_set_time_in_smaller_font:
 * @to_do_pane: an #EToDoPane
 * @time_in_smaller_font: a value to set
 *
 * Sets whether the time should be shown in a smaller font.
 *
 * Since: 3.56
 **/
void
e_to_do_pane_set_time_in_smaller_font (EToDoPane *to_do_pane,
				       gboolean time_in_smaller_font)
{
	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if ((to_do_pane->priv->time_in_smaller_font ? 1 : 0) == (time_in_smaller_font ? 1 : 0))
		return;

	to_do_pane->priv->time_in_smaller_font = time_in_smaller_font;

	etdp_update_comps (to_do_pane);

	g_object_notify (G_OBJECT (to_do_pane), "time-in-smaller-font");
}
