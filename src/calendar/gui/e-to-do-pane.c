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

#include "e-cal-data-model.h"
#include "e-cal-data-model-subscriber.h"
#include "e-cal-dialogs.h"
#include "e-cal-ops.h"
#include "misc.h"

#include "e-to-do-pane.h"

#define N_ROOTS 7
#define MAX_TOOLTIP_DESCRIPTION_LEN 128

struct _EToDoPanePrivate {
	GWeakRef shell_view_weakref; /* EShellView * */
	gboolean highlight_overdue;
	GdkRGBA *overdue_color;
	gboolean show_completed_tasks;
	gboolean use_24hour_format;

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

	GtkTreeRowReference *roots[N_ROOTS];
};

enum {
	PROP_0,
	PROP_HIGHLIGHT_OVERDUE,
	PROP_OVERDUE_COLOR,
	PROP_SHELL_VIEW,
	PROP_SHOW_COMPLETED_TASKS,
	PROP_USE_24HOUR_FORMAT
};

static void e_to_do_pane_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EToDoPane, e_to_do_pane, GTK_TYPE_GRID,
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
etdp_create_date_mark (const struct icaltimetype *itt)
{
	if (!itt)
		return 0;

	return itt->year * 10000 + itt->month * 100  + itt->day;
}

static void
etdp_itt_to_zone (struct icaltimetype *itt,
		  const gchar *itt_tzid,
		  ECalClient *client,
		  icaltimezone *default_zone)
{
	icaltimezone *zone = NULL;

	g_return_if_fail (itt != NULL);

	if (itt_tzid) {
		e_cal_client_get_timezone_sync (client, itt_tzid, &zone, NULL, NULL);
	} else if (itt->is_utc) {
		zone = icaltimezone_get_utc_timezone ();
	}

	if (zone)
		icaltimezone_convert_time (itt, zone, default_zone);
}

static gchar *
etdp_date_time_to_string (const ECalComponentDateTime *dt,
			  ECalClient *client,
			  icaltimezone *default_zone,
			  guint today_date_mark,
			  gboolean is_task,
			  gboolean use_24hour_format,
			  struct icaltimetype *out_itt)
{
	gboolean is_overdue;
	gchar *res;

	g_return_val_if_fail (dt != NULL, NULL);
	g_return_val_if_fail (dt->value != NULL, NULL);
	g_return_val_if_fail (out_itt != NULL, NULL);

	*out_itt = *dt->value;

	etdp_itt_to_zone (out_itt, dt->tzid, client, default_zone);

	is_overdue = is_task && etdp_create_date_mark (out_itt) < today_date_mark;

	if (out_itt->is_date && !is_overdue)
		return NULL;

	if (is_overdue) {
		struct tm tm;

		tm = icaltimetype_to_tm (out_itt);

		res = e_datetime_format_format_tm ("calendar", "table", out_itt->is_date ? DTFormatKindDate : DTFormatKindDateTime, &tm);
	} else {
		if (use_24hour_format) {
			res = g_strdup_printf ("%d:%02d", out_itt->hour, out_itt->minute);
		} else {
			gint hour = out_itt->hour;
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

			if (!out_itt->minute)
				res = g_strdup_printf ("%d %s", hour, suffix);
			else
				res = g_strdup_printf ("%d:%02d %s", hour, out_itt->minute, suffix);
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
		       icaltimezone *default_zone,
		       const struct icaltimetype *in_itt,
		       const gchar *tzid)
{
	struct icaltimetype itt;
	struct tm tm;

	if (!in_itt)
		return NULL;

	itt = *in_itt;

	etdp_itt_to_zone (&itt, tzid, client, default_zone);

	tm = icaltimetype_to_tm (&itt);

	return e_datetime_format_format_tm ("calendar", "table", itt.is_date ? DTFormatKindDate : DTFormatKindDateTime, &tm);
}

static gboolean
etdp_get_component_data (EToDoPane *to_do_pane,
			 ECalClient *client,
			 ECalComponent *comp,
			 icaltimezone *default_zone,
			 guint today_date_mark,
			 gchar **out_summary,
			 gchar **out_tooltip,
			 gboolean *out_is_task,
			 gboolean *out_is_completed,
			 gchar **out_sort_key,
			 guint *out_date_mark)
{
	icalcomponent *icalcomp;
	ECalComponentDateTime dt = { 0 };
	ECalComponentId *id;
	struct icaltimetype itt = icaltime_null_time ();
	const gchar *prefix, *location, *description;
	GString *tooltip;

	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), FALSE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);
	g_return_val_if_fail (out_summary, FALSE);
	g_return_val_if_fail (out_tooltip, FALSE);
	g_return_val_if_fail (out_is_task, FALSE);
	g_return_val_if_fail (out_is_completed, FALSE);
	g_return_val_if_fail (out_sort_key, FALSE);
	g_return_val_if_fail (out_date_mark, FALSE);

	icalcomp = e_cal_component_get_icalcomponent (comp);
	g_return_val_if_fail (icalcomp != NULL, FALSE);

	location = icalcomponent_get_location (icalcomp);
	if (location && !*location)
		location = NULL;

	tooltip = g_string_sized_new (512);

	etdp_append_to_string_escaped (tooltip, "<b>%s</b>", icalcomponent_get_summary (icalcomp), NULL);

	if (location) {
		g_string_append (tooltip, "\n");
		/* Translators: It will display "Location: LocationOfTheAppointment" */
		etdp_append_to_string_escaped (tooltip, _("Location: %s"), location, NULL);
	}

	*out_is_task = e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO;
	*out_is_completed = FALSE;

	if (*out_is_task) {
		struct icaltimetype *completed = NULL;

		/* Tasks after events */
		prefix = "1";

		e_cal_component_get_due (comp, &dt);
		e_cal_component_get_completed (comp, &completed);

		if (dt.value) {
			gchar *tmp;

			tmp = etdp_format_date_time (client, default_zone, dt.value, dt.tzid);

			g_string_append (tooltip, "\n");
			/* Translators: It will display "Due: DueDateAndTime" */
			etdp_append_to_string_escaped (tooltip, _("Due: %s"), tmp, NULL);

			g_free (tmp);
		}

		if (completed) {
			gchar *tmp;

			tmp = etdp_format_date_time (client, default_zone, completed, NULL);

			g_string_append (tooltip, "\n");
			/* Translators: It will display "Completed: DateAndTimeWhenCompleted" */
			etdp_append_to_string_escaped (tooltip, _("Completed: %s"), tmp, NULL);

			g_free (tmp);

			*out_is_completed = TRUE;
			e_cal_component_free_icaltimetype (completed);
		}
	} else {
		/* Events first */
		prefix = "0";

		e_cal_component_get_dtstart (comp, &dt);

		if (dt.value) {
			ECalComponentDateTime dtend = { 0 };
			struct icaltimetype ittstart, ittend;
			gchar *strstart, *strduration;

			e_cal_component_get_dtend (comp, &dtend);

			ittstart = *dt.value;
			if (dtend.value)
				ittend = *dtend.value;
			else
				ittend = ittstart;

			etdp_itt_to_zone (&ittstart, dt.tzid, client, default_zone);
			etdp_itt_to_zone (&ittend, dtend.value ? dtend.tzid : dt.tzid, client, default_zone);

			strstart = etdp_format_date_time (client, default_zone, &ittstart, NULL);
			strduration = calculate_time (icaltime_as_timet (ittstart), icaltime_as_timet (ittend));

			g_string_append (tooltip, "\n");
			/* Translators: It will display "Time: StartDateAndTime (Duration)" */
			etdp_append_to_string_escaped (tooltip, _("Time: %s %s"), strstart, strduration);

			g_free (strduration);
			g_free (strstart);

			e_cal_component_free_datetime (&dtend);
		}
	}

	*out_summary = NULL;

	if (dt.value) {
		gchar *time_str;

		time_str = etdp_date_time_to_string (&dt, client, default_zone, today_date_mark, *out_is_task,
			to_do_pane->priv->use_24hour_format, &itt);

		if (time_str) {
			*out_summary = g_markup_printf_escaped ("<span size=\"xx-small\">%s</span> %s%s%s%s",
				time_str, icalcomponent_get_summary (icalcomp), location ? " (" : "",
				location ? location : "", location ? ")" : "");
		}

		g_free (time_str);
	}

	if (!*out_summary) {
		*out_summary = g_markup_printf_escaped ("%s%s%s%s", icalcomponent_get_summary (icalcomp),
			location ? " (" : "", location ? location : "", location ? ")" : "");
	}

	if (*out_is_completed) {
		gchar *tmp = *out_summary;

		/* With leading space, to have proper row height in GtkTreeView */
		*out_summary = g_strdup_printf (" <s>%s</s>", *out_summary);

		g_free (tmp);
	} else {
		gchar *tmp = *out_summary;

		/* With leading space, to have proper row height in GtkTreeView */
		*out_summary = g_strconcat (" ", *out_summary, NULL);

		g_free (tmp);
	}

	e_cal_component_free_datetime (&dt);

	id = e_cal_component_get_id (comp);

	*out_sort_key = g_strdup_printf ("%s-%04d%02d%02d%02d%02d%02d-%s-%s",
		prefix, itt.year, itt.month, itt.day, itt.hour, itt.minute, itt.second,
		(id && id->uid) ? id->uid : "", (id && id->rid) ? id->rid : "");

	if (id)
		e_cal_component_free_id (id);

	description = icalcomponent_get_description (icalcomp);
	if (description && *description && g_utf8_validate (description, -1, NULL)) {
		gchar *tmp = NULL;
		glong len;

		len = g_utf8_strlen (description, -1);
		if (len > MAX_TOOLTIP_DESCRIPTION_LEN) {
			GString *str;
			const gchar *end;

			end = g_utf8_offset_to_pointer (description, MAX_TOOLTIP_DESCRIPTION_LEN);
			str = g_string_new_len (description, end - description);
			g_string_append (str, "…");

			tmp = g_string_free (str, FALSE);
		}

		g_string_append (tooltip, "\n\n");
		etdp_append_to_string_escaped (tooltip, "%s", tmp ? tmp : description, NULL);

		g_free (tmp);
	}

	*out_date_mark = etdp_create_date_mark (&itt);
	*out_tooltip = g_string_free (tooltip, FALSE);

	return TRUE;
}

static GdkRGBA
etdp_get_fgcolor_for_bgcolor (const GdkRGBA *bgcolor)
{
	GdkRGBA fgcolor = { 1.0, 1.0, 1.0, 1.0 };

	if (bgcolor) {
		if ((bgcolor->red > 0.7) || (bgcolor->green > 0.7) || (bgcolor->blue > 0.7)) {
			fgcolor.red = 0.0;
			fgcolor.green = 0.0;
			fgcolor.blue = 0.0;
		} else {
			fgcolor.red = 1.0;
			fgcolor.green = 1.0;
			fgcolor.blue = 1.0;
		}
	}

	return fgcolor;
}

static GSList * /* GtkTreePath * */
etdp_get_component_root_paths (EToDoPane *to_do_pane,
			       ECalClient *client,
			       ECalComponent *comp,
			       icaltimezone *default_zone)
{
	ECalComponentDateTime dt;
	struct icaltimetype itt;
	GtkTreePath *first_root_path = NULL;
	GtkTreeModel *model;
	GSList *roots = NULL;
	guint start_date_mark, end_date_mark, prev_date_mark = 0;
	gint ii;

	g_return_val_if_fail (E_IS_TO_DO_PANE (to_do_pane), NULL);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), NULL);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), NULL);

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO) {
		e_cal_component_get_due (comp, &dt);

		if (dt.value) {
			itt = *dt.value;

			etdp_itt_to_zone (&itt, dt.tzid, client, default_zone);
			start_date_mark = etdp_create_date_mark (&itt);
		} else {
			start_date_mark = 0;
		}

		end_date_mark = start_date_mark;

		e_cal_component_free_datetime (&dt);
	} else {
		e_cal_component_get_dtstart (comp, &dt);

		if (dt.value) {
			itt = *dt.value;

			etdp_itt_to_zone (&itt, dt.tzid, client, default_zone);
			start_date_mark = etdp_create_date_mark (&itt);
		} else {
			start_date_mark = 0;
		}

		e_cal_component_free_datetime (&dt);

		e_cal_component_get_dtend (comp, &dt);

		if (dt.value) {
			itt = *dt.value;

			etdp_itt_to_zone (&itt, dt.tzid, client, default_zone);
			end_date_mark = etdp_create_date_mark (&itt);
		} else {
			end_date_mark = start_date_mark;
		}

		e_cal_component_free_datetime (&dt);
	}

	model = GTK_TREE_MODEL (to_do_pane->priv->tree_store);

	for (ii = 0; ii < N_ROOTS; ii++) {
		if (gtk_tree_row_reference_valid (to_do_pane->priv->roots[ii])) {
			GtkTreePath *root_path;
			GtkTreeIter root_iter;
			guint root_date_mark = 0;

			root_path = gtk_tree_row_reference_get_path (to_do_pane->priv->roots[ii]);
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

	if (!roots && first_root_path)
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
			GtkTreeIter parent;
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
	GdkRGBA *bgcolor, fgcolor;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));
	g_return_if_fail (out_bgcolor);
	g_return_if_fail (out_bgcolor_set);
	g_return_if_fail (out_fgcolor);
	g_return_if_fail (out_fgcolor_set);

	*out_bgcolor_set = FALSE;
	*out_fgcolor_set = FALSE;

	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	bgcolor = g_hash_table_lookup (to_do_pane->priv->client_colors, e_client_get_source (E_CLIENT (client)));

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO &&
	    to_do_pane->priv->highlight_overdue &&
	    to_do_pane->priv->overdue_color) {
		ECalComponentDateTime dt = { 0 };

		e_cal_component_get_due (comp, &dt);

		if (dt.value) {
			icaltimezone *default_zone;
			struct icaltimetype itt, now;

			default_zone = e_cal_data_model_get_timezone (to_do_pane->priv->events_data_model);

			itt = *dt.value;
			etdp_itt_to_zone (&itt, dt.tzid, client, default_zone);

			now = icaltime_current_time_with_zone (default_zone);

			if (icaltime_compare (itt, now) <= 0) {
				bgcolor = to_do_pane->priv->overdue_color;
			} else if (out_nearest_due) {
				time_t due_tt;

				due_tt = icaltime_as_timet_with_zone (itt, default_zone);
				if (*out_nearest_due == (time_t) -1 ||
				    *out_nearest_due > due_tt)
					*out_nearest_due = due_tt;
			}
		}

		e_cal_component_free_datetime (&dt);
	}

	fgcolor = etdp_get_fgcolor_for_bgcolor (bgcolor);

	*out_bgcolor_set = bgcolor != NULL;
	if (bgcolor)
		*out_bgcolor = *bgcolor;

	*out_fgcolor_set = *out_bgcolor_set;
	*out_fgcolor = fgcolor;
}

static void
etdp_add_component (EToDoPane *to_do_pane,
		    ECalClient *client,
		    ECalComponent *comp)
{
	ECalComponentId *id;
	ComponentIdent *ident;
	icaltimezone *default_zone;
	GSList *new_root_paths, *new_references, *link;
	GtkTreeModel *model;
	GtkTreeIter iter = { 0 };
	GdkRGBA bgcolor, fgcolor;
	gboolean bgcolor_set = FALSE, fgcolor_set = FALSE;
	gchar *summary = NULL, *tooltip = NULL, *sort_key = NULL;
	gboolean is_task = FALSE, is_completed = FALSE;
	const gchar *icon_name;
	guint date_mark = 0;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	id = e_cal_component_get_id (comp);
	g_return_if_fail (id != NULL);

	default_zone = e_cal_data_model_get_timezone (to_do_pane->priv->events_data_model);

	if (!etdp_get_component_data (to_do_pane, client, comp, default_zone, to_do_pane->priv->last_today,
		&summary, &tooltip, &is_task, &is_completed, &sort_key, &date_mark)) {
		e_cal_component_free_id (id);
		return;
	}

	model = GTK_TREE_MODEL (to_do_pane->priv->tree_store);
	ident = component_ident_new (client, id->uid, id->rid);

	new_root_paths = etdp_get_component_root_paths (to_do_pane, client, comp, default_zone);

	new_references = etdp_merge_with_root_paths (to_do_pane, model, new_root_paths,
		g_hash_table_lookup (to_do_pane->priv->component_refs, ident));

	g_slist_free_full (new_root_paths, (GDestroyNotify) gtk_tree_path_free);

	if (e_cal_component_has_attendees (comp)) {
		if (is_task)
			icon_name = "stock_task-assigned";
		else
			icon_name = "stock_people";
	} else {
		if (is_task)
			icon_name = "stock_task";
		else
			icon_name = "appointment-new";
	}

	etdp_get_comp_colors (to_do_pane, client, comp, &bgcolor, &bgcolor_set, &fgcolor, &fgcolor_set,
		&to_do_pane->priv->nearest_due);

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
					COLUMN_SUMMARY, summary,
					COLUMN_TOOLTIP, tooltip,
					COLUMN_SORTKEY, sort_key,
					COLUMN_DATE_MARK, date_mark,
					COLUMN_CAL_CLIENT, client,
					COLUMN_CAL_COMPONENT, comp,
					-1);
			}

			gtk_tree_path_free (path);
		}
	}

	g_hash_table_insert (to_do_pane->priv->component_refs, component_ident_copy (ident), new_references);

	component_ident_free (ident);
	e_cal_component_free_id (id);
	g_free (summary);
	g_free (tooltip);
	g_free (sort_key);
}

static void
etdp_got_client_cb (GObject *source_object,
		    GAsyncResult *result,
		    gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;
	EClient *client;
	GError *error = NULL;

	client = e_client_cache_get_client_finish (E_CLIENT_CACHE (source_object), result, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&error);
		return;
	}

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

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
		(guint32) -1, to_do_pane->priv->cancellable, etdp_got_client_cb, to_do_pane);
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
	GSList *link;

	g_return_if_fail (E_IS_TO_DO_PANE (subscriber));

	to_do_pane = E_TO_DO_PANE (subscriber);

	ident.client = client;
	ident.uid = (gchar *) uid;
	ident.rid = (gchar *) (rid && *rid ? rid : NULL);

	for (link = g_hash_table_lookup (to_do_pane->priv->component_refs, &ident); link; link = g_slist_next (link)) {
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

	g_hash_table_remove (to_do_pane->priv->component_refs, &ident);
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
etdp_update_all (EToDoPane *to_do_pane)
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
etdp_check_time_changed (EToDoPane *to_do_pane,
			 gboolean force_update)
{
	icaltimetype itt;
	icaltimezone *zone;
	guint new_today;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	zone = e_cal_data_model_get_timezone (to_do_pane->priv->events_data_model);
	itt = icaltime_current_time_with_zone (zone);
	new_today = etdp_create_date_mark (&itt);

	if (force_update || new_today != to_do_pane->priv->last_today) {
		gchar *tasks_filter;
		time_t tt_begin, tt_end;
		gchar *iso_begin_all, *iso_begin, *iso_end;
		gint ii;

		to_do_pane->priv->last_today = new_today;

		tt_begin = icaltime_as_timet_with_zone (itt, zone);
		tt_begin = time_day_begin_with_zone (tt_begin, zone);
		tt_end = time_add_week_with_zone (tt_begin, 1, zone);

		iso_begin_all = isodate_from_time_t (0);
		iso_begin = isodate_from_time_t (tt_begin);
		iso_end = isodate_from_time_t (tt_end);
		if (to_do_pane->priv->show_completed_tasks) {
			tasks_filter = g_strdup_printf (
					"(or"
					" (and"
					 " (not (is-completed?))"
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
					" (due-in-time-range? (make-time \"%s\") (make-time \"%s\"))"
					")",
					iso_begin_all, iso_end);
		}

		/* Re-label the roots */
		for (ii = 0; ii < N_ROOTS; ii++) {
			GtkTreePath *path;
			GtkTreeIter iter;

			if (!gtk_tree_row_reference_valid (to_do_pane->priv->roots[ii]))
				continue;

			path = gtk_tree_row_reference_get_path (to_do_pane->priv->roots[ii]);

			if (gtk_tree_model_get_iter (gtk_tree_row_reference_get_model (to_do_pane->priv->roots[ii]), &iter, path)) {
				struct tm tm;
				gchar *markup;
				guint date_mark;

				tm = icaltimetype_to_tm (&itt);

				icaltime_adjust (&itt, 1, 0, 0, 0);

				date_mark = etdp_create_date_mark (&itt);

				if (ii == 0) {
					markup = g_markup_printf_escaped ("<b>%s</b>", _("Today"));
				} else if (ii == 1) {
					markup = g_markup_printf_escaped ("<b>%s</b>", _("Tomorrow"));
				} else {
					gchar *date;

					date = e_datetime_format_format_tm ("calendar", "table", DTFormatKindDate, &tm);
					markup = g_markup_printf_escaped ("<b>%s</b>", date);
					g_free (date);
				}

				gtk_tree_store_set (to_do_pane->priv->tree_store, &iter,
					COLUMN_SUMMARY, markup,
					COLUMN_DATE_MARK, date_mark,
					-1);

				g_free (markup);
			} else {
				icaltime_adjust (&itt, 1, 0, 0, 0);
			}

			gtk_tree_path_free (path);
		}

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

		etdp_update_all (to_do_pane);
	} else {
		time_t now_tt = icaltime_as_timet_with_zone (itt, zone);

		if (to_do_pane->priv->nearest_due != (time_t) -1 &&
		    to_do_pane->priv->nearest_due <= now_tt)
			etdp_update_colors (to_do_pane, TRUE);
	}
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
	icaltimezone *timezone = NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (g_settings_get_boolean (settings, "use-system-timezone"))
		timezone = e_cal_util_get_system_timezone ();
	else
		location = g_variant_get_string (variant, NULL);

	if (location != NULL && *location != '\0')
		timezone = icaltimezone_get_builtin_timezone (location);

	if (timezone == NULL)
		timezone = icaltimezone_get_utc_timezone ();

	g_value_set_pointer (value, timezone);

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
				struct icaltimetype now;
				gint time_divisions_secs;
				icaltimezone *zone;

				time_divisions_secs = g_settings_get_int (settings, "time-divisions") * 60;
				zone = e_cal_data_model_get_timezone (to_do_pane->priv->events_data_model);
				now = icaltime_current_time_with_zone (zone);

				now.year = date_mark / 10000;
				now.month = (date_mark / 100) % 100;
				now.day = date_mark % 100;

				/* The date_mark is the next day, not the day it belongs to */
				icaltime_adjust (&now, -1, 0, 0, 0);

				dtstart = icaltime_as_timet_with_zone (now, zone);
				if (dtstart > 0 && time_divisions_secs > 0) {
					dtstart = dtstart + time_divisions_secs - (dtstart % time_divisions_secs);
					dtend = dtstart + time_divisions_secs;
				} else {
					dtstart = 0;
				}
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

typedef struct _RemoveOperationData {
	ECalClient *client;
	gchar *uid;
	gchar *rid;
	ECalObjModType mod;
} RemoveOperationData;

static void
remove_operation_data_free (gpointer ptr)
{
	RemoveOperationData *rod = ptr;

	if (rod) {
		g_clear_object (&rod->client);
		g_free (rod->uid);
		g_free (rod->rid);
		g_free (rod);
	}
}

static void
etdp_remove_component_thread (EAlertSinkThreadJobData *job_data,
			      gpointer user_data,
			      GCancellable *cancellable,
			      GError **error)
{
	RemoveOperationData *rod = user_data;

	g_return_if_fail (rod != NULL);

	e_cal_client_remove_object_sync (rod->client, rod->uid, rod->rid, rod->mod, cancellable, error);
}

static void
etdp_delete_common (EToDoPane *to_do_pane,
		    ECalObjModType mod)
{
	ECalClient *client = NULL;
	ECalComponent *comp = NULL;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	if (etdp_get_tree_view_selected_one (to_do_pane, &client, &comp) && client && comp) {
		const gchar *description;
		const gchar *alert_ident;
		gchar *display_name;
		GCancellable *cancellable;
		ESource *source;
		RemoveOperationData *rod;
		ECalComponentId *id;

		id = e_cal_component_get_id (comp);
		g_return_if_fail (id != NULL);

		if (!e_cal_dialogs_delete_component (comp, FALSE, 1, e_cal_component_get_vtype (comp), GTK_WIDGET (to_do_pane))) {
			e_cal_component_free_id (id);
			g_clear_object (&client);
			g_clear_object (&comp);
			return;
		}

		switch (e_cal_client_get_source_type (client)) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
				description = _("Removing an event");
				alert_ident = "calendar:failed-remove-event";
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				description = _("Removing a memo");
				alert_ident = "calendar:failed-remove-memo";
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				description = _("Removing a task");
				alert_ident = "calendar:failed-remove-task";
				break;
			default:
				g_warn_if_reached ();
				return;
		}

		if (!e_cal_component_is_instance (comp))
			mod = E_CAL_OBJ_MOD_ALL;

		rod = g_new0 (RemoveOperationData,1);
		rod->client = g_object_ref (client);
		rod->uid = g_strdup (id->uid);
		rod->rid = g_strdup (id->rid);
		rod->mod = mod;

		source = e_client_get_source (E_CLIENT (client));
		display_name = e_util_get_source_full_name (e_source_registry_watcher_get_registry (to_do_pane->priv->watcher), source);

		/* It doesn't matter which data-model is picked, because it's used
		   only for thread creation and manipulation, not for its content. */
		cancellable = e_cal_data_model_submit_thread_job (to_do_pane->priv->events_data_model, description, alert_ident,
			display_name, etdp_remove_component_thread, rod, remove_operation_data_free);

		e_cal_component_free_id (id);
		g_clear_object (&cancellable);
		g_free (display_name);
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
etdp_delete_series_cb (GtkMenuItem *item,
		       gpointer user_data)
{
	EToDoPane *to_do_pane = user_data;

	g_return_if_fail (E_IS_TO_DO_PANE (to_do_pane));

	etdp_delete_common (to_do_pane, E_CAL_OBJ_MOD_ALL);
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

	item = gtk_image_menu_item_new_with_mnemonic (_("New _Appointment..."));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
		gtk_image_new_from_icon_name ("appointment-new", GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
		G_CALLBACK (etdp_new_appointment_cb), to_do_pane);
	gtk_widget_show (item);
	gtk_menu_shell_append (menu_shell, item);

	item = gtk_image_menu_item_new_with_mnemonic (_("New _Meeting..."));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
		gtk_image_new_from_icon_name ("stock_people", GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
		G_CALLBACK (etdp_new_meeting_cb), to_do_pane);
	gtk_widget_show (item);
	gtk_menu_shell_append (menu_shell, item);

	item = gtk_image_menu_item_new_with_mnemonic (_("New _Task..."));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
		gtk_image_new_from_icon_name ("stock_task", GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
		G_CALLBACK (etdp_new_task_cb), to_do_pane);
	gtk_widget_show (item);
	gtk_menu_shell_append (menu_shell, item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_New Assigned Task..."));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
		gtk_image_new_from_icon_name ("stock_task-assigned", GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
		G_CALLBACK (etdp_new_assigned_task_cb), to_do_pane);
	gtk_widget_show (item);
	gtk_menu_shell_append (menu_shell, item);

	if (client && comp) {
		item = gtk_separator_menu_item_new ();
		gtk_widget_show (item);
		gtk_menu_shell_append (menu_shell, item);

		item = gtk_image_menu_item_new_with_mnemonic (_("_Open..."));
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
			gtk_image_new_from_icon_name ("document-open", GTK_ICON_SIZE_MENU));
		g_signal_connect (item, "activate",
			G_CALLBACK (etdp_open_selected_cb), to_do_pane);
		gtk_widget_show (item);
		gtk_menu_shell_append (menu_shell, item);

		item = gtk_separator_menu_item_new ();
		gtk_widget_show (item);
		gtk_menu_shell_append (menu_shell, item);

		if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_EVENT &&
		    e_cal_component_is_instance (comp)) {
			item = gtk_image_menu_item_new_with_mnemonic (_("_Delete This Instance..."));
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				gtk_image_new_from_icon_name ("edit-delete", GTK_ICON_SIZE_MENU));
			g_signal_connect (item, "activate",
				G_CALLBACK (etdp_delete_selected_cb), to_do_pane);
			gtk_widget_show (item);
			gtk_menu_shell_append (menu_shell, item);

			item = gtk_image_menu_item_new_with_mnemonic (_("D_elete All Instances..."));
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				gtk_image_new_from_icon_name ("edit-delete", GTK_ICON_SIZE_MENU));
			g_signal_connect (item, "activate",
				G_CALLBACK (etdp_delete_series_cb), to_do_pane);
			gtk_widget_show (item);
			gtk_menu_shell_append (menu_shell, item);
		} else {
			item = gtk_image_menu_item_new_with_mnemonic (_("_Delete..."));
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				gtk_image_new_from_icon_name ("edit-delete", GTK_ICON_SIZE_MENU));
			g_signal_connect (item, "activate",
				G_CALLBACK (etdp_delete_series_cb), to_do_pane);
			gtk_widget_show (item);
			gtk_menu_shell_append (menu_shell, item);
		}
	}

	g_clear_object (&client);
	g_clear_object (&comp);
}

static gboolean
etdp_destroy_menu_idle_cb (gpointer user_data)
{
	GtkWidget *widget = user_data;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	gtk_widget_destroy (widget);

	return FALSE;
}

static void
etdp_menu_deactivate_cb (GtkWidget *widget)
{
	g_idle_add (etdp_destroy_menu_idle_cb, widget);
}

static void
etdp_popup_menu (EToDoPane *to_do_pane,
		 GdkEvent *event)
{
	GtkMenu *menu;
	guint button, event_time;

	menu = GTK_MENU (gtk_menu_new ());

	g_signal_connect (menu, "deactivate",
		G_CALLBACK (etdp_menu_deactivate_cb), NULL);

	etdp_fill_popup_menu (to_do_pane, menu);

	if (event) {
		if (!gdk_event_get_button (event, &button))
			button = 0;

		event_time = gdk_event_get_time (event);
	} else {
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	gtk_menu_attach_to_widget (menu, GTK_WIDGET (to_do_pane->priv->tree_view), NULL);
	gtk_menu_popup (menu, NULL, NULL, NULL, NULL, button, event_time);
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
etcp_notify_visible_cb (EToDoPane *to_do_pane,
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

		case PROP_USE_24HOUR_FORMAT:
			e_to_do_pane_set_use_24hour_format (
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

		case PROP_USE_24HOUR_FORMAT:
			g_value_set_boolean (
				value,
				e_to_do_pane_get_use_24hour_format (
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
	GtkGrid *grid;
	GtkWidget *widget;
	GtkCellRenderer *renderer;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeModel *model, *sort_model;
	GtkTreeIter iter;
	GSettings *settings;
	PangoAttrList *bold;
	gint ii;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_to_do_pane_parent_class)->constructed (object);

	shell_view = e_to_do_pane_ref_shell_view (to_do_pane);
	shell = e_shell_backend_get_shell (e_shell_view_get_shell_backend (shell_view));

	to_do_pane->priv->client_cache = g_object_ref (e_shell_get_client_cache (shell));
	to_do_pane->priv->watcher = e_source_registry_watcher_new (e_shell_get_registry (shell), NULL);
	to_do_pane->priv->source_changed_id =
		g_signal_connect (e_source_registry_watcher_get_registry (to_do_pane->priv->watcher), "source-changed",
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

	renderer = gtk_cell_renderer_pixbuf_new ();

	column = gtk_tree_view_column_new_with_attributes ("Text", renderer,
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

	for (ii = 0; ii < N_ROOTS; ii++) {
		GtkTreePath *path;
		gchar *sort_key;

		sort_key = g_strdup_printf ("%c", 'A' + ii);

		gtk_tree_store_append (to_do_pane->priv->tree_store, &iter, NULL);
		gtk_tree_store_set (to_do_pane->priv->tree_store, &iter,
			COLUMN_SORTKEY, sort_key,
			COLUMN_HAS_ICON_NAME, FALSE,
			-1);

		g_free (sort_key);

		path = gtk_tree_model_get_path (model, &iter);

		to_do_pane->priv->roots[ii] = gtk_tree_row_reference_new (model, path);
		g_warn_if_fail (to_do_pane->priv->roots[ii] != NULL);

		gtk_tree_path_free (path);
	}

	gtk_tree_view_set_headers_visible (tree_view, FALSE);
	gtk_tree_view_set_tooltip_column (tree_view, COLUMN_TOOLTIP);

	gtk_widget_show_all (GTK_WIDGET (grid));

	to_do_pane->priv->events_data_model = e_cal_data_model_new (e_to_do_pane_submit_thread_job, G_OBJECT (to_do_pane));
	to_do_pane->priv->tasks_data_model = e_cal_data_model_new (e_to_do_pane_submit_thread_job, G_OBJECT (to_do_pane));
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
		G_CALLBACK (etcp_notify_visible_cb), NULL);

	if (gtk_widget_get_visible (GTK_WIDGET (to_do_pane)))
		e_source_registry_watcher_reclaim (to_do_pane->priv->watcher);
}

static void
e_to_do_pane_dispose (GObject *object)
{
	EToDoPane *to_do_pane = E_TO_DO_PANE (object);
	gint ii;

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

	for (ii = 0; ii < N_ROOTS; ii++) {
		gtk_tree_row_reference_free (to_do_pane->priv->roots[ii]);
		to_do_pane->priv->roots[ii] = NULL;
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

	if (to_do_pane->priv->overdue_color)
		gdk_rgba_free (to_do_pane->priv->overdue_color);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_to_do_pane_parent_class)->finalize (object);
}

static void
e_to_do_pane_init (EToDoPane *to_do_pane)
{
	to_do_pane->priv = G_TYPE_INSTANCE_GET_PRIVATE (to_do_pane, E_TYPE_TO_DO_PANE, EToDoPanePrivate);
	to_do_pane->priv->cancellable = g_cancellable_new ();

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

	g_type_class_add_private (klass, sizeof (EToDoPanePrivate));

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
		PROP_USE_24HOUR_FORMAT,
		g_param_spec_boolean (
			"use-24hour-format",
			"Use 24hour Format",
			NULL,
			FALSE,
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

	if (to_do_pane->priv->overdue_color) {
		gdk_rgba_free (to_do_pane->priv->overdue_color);
		to_do_pane->priv->overdue_color = NULL;
	}

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

	etdp_update_all (to_do_pane);

	g_object_notify (G_OBJECT (to_do_pane), "use-24hour-format");
}
