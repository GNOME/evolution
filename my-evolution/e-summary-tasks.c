/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-tasks.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Iain Holmes
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <gal/widgets/e-unicode.h>

#include "e-summary-tasks.h"
#include "e-summary.h"

#include <cal-client/cal-client.h>
#include <cal-util/timeutil.h>

#include <bonobo/bonobo-event-source.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-listener.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-object.h>

#include <gconf/gconf-client.h>


#define MAX_RELOAD_TRIES 10

struct _ESummaryTasks {
	CalClient *client;

	char *html;
	char *due_today_colour;
	char *overdue_colour;

	char *default_uri;

	GConfClient *gconf_client;
	int gconf_value_changed_handler_id;

	int cal_open_reload_timeout_id;
	int reload_count;
};

const char *
e_summary_tasks_get_html (ESummary *summary)
{
	if (summary == NULL) {
		return NULL;
	}
	
	if (summary->tasks == NULL) {
		return NULL;
	}

	return summary->tasks->html;
}

/* list_sort_merge, and list_sort are copied from GNOME-VFS.
   Author: Sven Oliver <sven.over@ob.kamp.net>
   Modified by Ettore Perazzoli <ettore@comm2000.it> to let the compare
   functions get an additional gpointer parameter.  

   Included here as using gnome-vfs for 1 20 line function 
   seems a bit of overkill.
*/

typedef gint (* CalSummaryListCompareFunc) (gconstpointer a, 
					    gconstpointer b,
					    gpointer data);
static GList *
cal_list_sort_merge (GList *l1,
		     GList *l2,
		     CalSummaryListCompareFunc compare_func,
		     gpointer data)
{
	GList list, *l, *lprev;

	l = &list;
	lprev = NULL;

	while (l1 && l2) {
		if (compare_func (l1->data, l2->data, data) < 0) {
			l->next = l1;
			l = l->next;
			l->prev = lprev;
			lprev = l;
			l1 = l1->next;
		} else {
			l->next = l2;
			l = l->next;
			l->prev = lprev;
			lprev = l;
			l2 = l2->next;
		}
	}

	l->next = l1 ? l1 : l2;
	l->next->prev = l;

	return list.next;
}

static GList *
cal_list_sort (GList *list,
	       CalSummaryListCompareFunc compare_func,
	       gpointer data)
{
	GList *l1, *l2;

	if (!list)
		return NULL;
	if (!list->next)
		return list;

	l1 = list;
	l2 = list->next;

	while ((l2 = l2->next) != NULL) {
		if ((l2 = l2->next) == NULL)
			break;
		l1 = l1->next;
	}

	l2 = l1->next;
	l1->next = NULL;
	
	return cal_list_sort_merge (cal_list_sort (list, compare_func, data),
				    cal_list_sort (l2, compare_func, data),
				    compare_func, data);
}
	
static int
sort_uids (gconstpointer a,
	   gconstpointer b,
	   gpointer user_data)
{
	icalcomponent *icalcomp_a, *icalcomp_b;
	CalComponent *comp_a, *comp_b;
	CalClient *client = user_data;
	CalClientGetStatus status;
	/* let undefined priorities be lowest ones */
	int lowest = 10, rv;
	int *pri_a, *pri_b;

	/* a after b then return > 0 */

	status = cal_client_get_object (client, a, NULL, &icalcomp_a);
	if (status != CAL_CLIENT_GET_SUCCESS)
		return -1;

	status = cal_client_get_object (client, b, NULL, &icalcomp_b);
	if (status != CAL_CLIENT_GET_SUCCESS)
		return 1;

	comp_a = cal_component_new ();
	cal_component_set_icalcomponent (comp_a, icalcomp_a);
	comp_b = cal_component_new ();
	cal_component_set_icalcomponent (comp_b, icalcomp_b);

	cal_component_get_priority (comp_a, &pri_a);
	cal_component_get_priority (comp_b, &pri_b);

	if (pri_a == NULL)
		pri_a = &lowest;
	if (pri_b == NULL)
		pri_b = &lowest;

	if (*pri_a == 0) {
		*pri_a = lowest;
	}

	if (*pri_b == 0) {
		*pri_b = lowest;
	}
	
	rv = *pri_a - *pri_b;

	if (pri_a != &lowest)
		cal_component_free_priority (pri_a);
	if (pri_b != &lowest)
		cal_component_free_priority (pri_b);

	g_object_unref (comp_a);
	g_object_unref (comp_b);

	return rv;
}

static GList *
get_todays_uids (ESummary *summary,
		 CalClient *client,
		 GList *uids)
{
	GList *today = NULL, *p;
	time_t todays_end, todays_start, t;

	t = time (NULL);
	todays_start = time_day_begin_with_zone (t, summary->tz);
	todays_end = time_day_end_with_zone (t, summary->tz);

	for (p = uids; p; p = p->next) {
		char *uid;
		icalcomponent *icalcomp;
		CalComponent *comp;
		CalClientGetStatus status;
		CalComponentDateTime due;
		icaltimezone *zone;
		time_t endt;

		uid = p->data;
		status = cal_client_get_object (client, uid, NULL, &icalcomp);
		if (status != CAL_CLIENT_GET_SUCCESS) {
			continue;
		}

		comp = cal_component_new ();
		cal_component_set_icalcomponent (comp, icalcomp);
	
		cal_component_get_due (comp, &due);

		cal_client_get_timezone (client, due.tzid, &zone);
		if (due.value != 0) {
			icaltimezone_convert_time (due.value, zone, summary->tz);
			endt = icaltime_as_timet (*due.value);

			if (endt <= todays_end) {
				today = g_list_append (today, g_strdup (uid));
			}
		}
		
		cal_component_free_datetime (&due);
		g_object_unref (comp);
	}

	if (today == NULL) {
		return NULL;
	}

	today = cal_list_sort (today, sort_uids, client);
	return today;
}

static const char *
get_task_colour (ESummary *summary,
		 CalClient *client,
		 const char *uid)
{
	icalcomponent *icalcomp;
	CalComponent *comp;
	CalClientGetStatus status;
	CalComponentDateTime due;
	icaltimezone *zone;
	char *ret;
	time_t end_t, t, todays_start, todays_end;

	t = time (NULL);
	todays_start = time_day_begin_with_zone (t, summary->tz);
	todays_end = time_day_end_with_zone (t, summary->tz);

	status = cal_client_get_object (client, uid, NULL, &icalcomp);
	if (status != CAL_CLIENT_GET_SUCCESS) {
		return "black";
	}

	comp = cal_component_new ();
	cal_component_set_icalcomponent (comp, icalcomp);

	cal_component_get_due (comp, &due);

	cal_client_get_timezone (client, due.tzid, &zone);
	if (due.value != 0) {
		icaltimezone_convert_time (due.value, zone, summary->tz);
		end_t = icaltime_as_timet (*due.value);

		if (end_t >= todays_start && end_t <= todays_end) {
			ret = summary->tasks->due_today_colour;
		} else if (end_t < t) {
			ret = summary->tasks->overdue_colour;
		} else {
			ret = "black";
		}
	} else {
		ret = "black";
	}

	cal_component_free_datetime (&due);
	g_object_unref (comp);

	return (const char *)ret;
}

static gboolean
generate_html (gpointer data)
{
	ESummary *summary = data;
	ESummaryTasks *tasks = summary->tasks;
	GList *uids, *l;
	GString *string;
	char *tmp;
	time_t t;

	if (cal_client_get_load_state (tasks->client) != CAL_CLIENT_LOAD_LOADED)
		return FALSE;

	/* Set the default timezone on the server. */
	if (summary->tz) {
		cal_client_set_default_timezone (tasks->client,
						 summary->tz);
	}

	t = time (NULL);

	uids = cal_client_get_uids (tasks->client, CALOBJ_TYPE_TODO);
	if (summary->preferences->show_tasks == E_SUMMARY_CALENDAR_TODAYS_TASKS && uids != NULL) {
		GList *tmp;
		
		tmp = get_todays_uids (summary, tasks->client, uids);
		cal_obj_uid_list_free (uids);

		uids = tmp;
	}

	if (uids == NULL) {
		g_free (tasks->html);
		tasks->html = g_strconcat ("<dl><dt><img src=\"myevo-post-it.png\" align=\"middle\" "
					   "alt=\"\" width=\"48\" height=\"48\"> "
					   "<b><a href=\"", tasks->default_uri, "\">",
					   _("Tasks"),
					   "</a></b></dt><dd><b>", _("No tasks"), "</b></dd></dl>",
					   NULL);
		return FALSE;
	} else {
		uids = cal_list_sort (uids, sort_uids, tasks->client);
		string = g_string_new (NULL);
		g_string_sprintf (string, "<dl><dt><img src=\"myevo-post-it.png\" align=\"middle\" "
				  "alt=\"\" width=\"48\" height=\"48\"> <b><a href=\"%s\">", tasks->default_uri);
		g_string_append (string, _("Tasks"));
		g_string_append (string, "</a></b></dt><dd>");

		for (l = uids; l; l = l->next) {
			char *uid;
			icalcomponent *icalcomp;
			CalComponent *comp;
			CalComponentText text;
			CalClientGetStatus status;
			struct icaltimetype *completed;
			const char *colour;
			
			uid = l->data;
			status = cal_client_get_object (tasks->client, uid, NULL, &icalcomp);
			if (status != CAL_CLIENT_GET_SUCCESS) {
				continue;
			}

			comp = cal_component_new ();
			cal_component_set_icalcomponent (comp, icalcomp);

			cal_component_get_summary (comp, &text);
			cal_component_get_completed (comp, &completed);

			colour = get_task_colour (summary, tasks->client, uid);

			if (completed == NULL) {
				tmp = g_strdup_printf ("<img align=\"middle\" src=\"task.png\" "
						       "alt=\"\" width=\"16\" height=\"16\">  &#160; "
						       "<a href=\"tasks:/%s\"><font size=\"-1\" color=\"%s\">%s</font></a><br>", 
						       uid, colour, text.value ? text.value : _("(No Description)"));
			} else {
#if 0
				tmp = g_strdup_printf ("<img align=\"middle\" src=\"task.xpm\" "
						       "alt=\"\" width=\"16\" height=\"16\">  &#160; "
						       "<font size=\"-1\"><strike><a href=\"evolution:/local/Tasks\">%s</a></strike></font><br>",
						       text.value);
#endif
				cal_component_free_icaltimetype (completed);
				g_object_unref (comp);
				continue;
			}

			g_object_unref (comp);
			g_string_append (string, tmp);
			g_free (tmp);
		}

		cal_obj_uid_list_free (uids);
		g_string_append (string, "</dd></dl>");
	}

	if (tasks->html) {
		g_free (tasks->html);
	}
	tasks->html = string->str;
	g_string_free (string, FALSE);

  	e_summary_draw (summary);
	return FALSE;
}

static gboolean
cal_open_reload_timeout (void *data)
{
	ESummary *summary = (ESummary *) data;

	summary->tasks->cal_open_reload_timeout_id = 0;

	if (++ summary->tasks->reload_count >= MAX_RELOAD_TRIES) {
		summary->tasks->reload_count = 0;
		return FALSE;
	}

	cal_client_open_default_tasks (summary->tasks->client, FALSE);
	return FALSE;
}

static void
cal_opened_cb (CalClient *client,
	       CalClientOpenStatus status,
	       ESummary *summary)
{
	if (status == CAL_CLIENT_OPEN_SUCCESS)
		g_idle_add (generate_html, summary);
	else
		summary->tasks->cal_open_reload_timeout_id = g_timeout_add (1000,
									    cal_open_reload_timeout,
									    summary);
}

static void
obj_changed_cb (CalClient *client,
		const char *uid,
		gpointer data)
{
	g_idle_add (generate_html, data);
}

static void
e_summary_tasks_protocol (ESummary *summary,
			  const char *uri,
			  void *closure)
{
	ESummaryTasks *tasks;
	CORBA_Environment ev;
	const char *comp_uri;
	GNOME_Evolution_Calendar_CompEditorFactory factory;

	tasks = (ESummaryTasks *) closure;

	comp_uri = cal_client_get_uri (tasks->client);

	/* Get the factory */
	CORBA_exception_init (&ev);
	factory = bonobo_activation_activate_from_id ("OAFIID:GNOME_Evolution_Calendar_CompEditorFactory", 0, NULL, &ev);
	if (BONOBO_EX (&ev)) {
		g_message ("%s: Could not activate the component editor factory (%s)",
			   G_GNUC_FUNCTION, CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return;
	}
	
	GNOME_Evolution_Calendar_CompEditorFactory_editExisting (factory, comp_uri, (char *)uri + 7, &ev);
	
	if (BONOBO_EX (&ev)) {
		g_message ("%s: Execption while editing the component (%s)",
			   G_GNUC_FUNCTION, CORBA_exception_id (&ev));
	}
	
	CORBA_exception_free (&ev);
	bonobo_object_release_unref (factory, NULL);
}

static void
setup_task_folder (ESummary *summary)
{
	ESummaryTasks *tasks;

	tasks = summary->tasks;
	g_assert (tasks != NULL);
	g_assert (tasks->gconf_client != NULL);

	if (tasks->cal_open_reload_timeout_id != 0) {
		g_source_remove (tasks->cal_open_reload_timeout_id);
		tasks->cal_open_reload_timeout_id = 0;
		tasks->reload_count = 0;
	}

	g_free (tasks->due_today_colour);
	g_free (tasks->overdue_colour);
	g_free (tasks->default_uri);
	
	tasks->due_today_colour = gconf_client_get_string (tasks->gconf_client,
							   "/apps/evolution/calendar/tasks/colors/due_today", NULL);
	if (!tasks->due_today_colour)
		tasks->due_today_colour = g_strdup ("blue");

	tasks->overdue_colour = gconf_client_get_string (tasks->gconf_client,
							 "/apps/evolution/calendar/tasks/colors/overdue", NULL);
	if (!tasks->overdue_colour)
		tasks->overdue_colour = g_strdup ("red");

	tasks->default_uri = gconf_client_get_string (tasks->gconf_client,
						      "/apps/evolution/shell/default_folders/tasks_path", NULL);

	if (tasks->client != NULL)
		g_object_unref (tasks->client);
	
	tasks->client = cal_client_new ();
	if (tasks->client == NULL) {
		g_warning ("Error making the client");
		return;
	}

	g_signal_connect (tasks->client, "cal-opened", G_CALLBACK (cal_opened_cb), summary);
	g_signal_connect (tasks->client, "obj-updated", G_CALLBACK (obj_changed_cb), summary);
	g_signal_connect (tasks->client, "obj-removed", G_CALLBACK (obj_changed_cb), summary);

	if (! cal_client_open_default_tasks (tasks->client, FALSE))
		g_message ("Open tasks failed");
}

static void
gconf_client_value_changed_cb (GConfClient *gconf_client,
			       const char *key,
			       GConfValue *value,
			       void *user_data)
{
	setup_task_folder (E_SUMMARY (user_data));
	generate_html (user_data);
}

static void
setup_gconf_client (ESummary *summary)
{
	ESummaryTasks *tasks;

	tasks = summary->tasks;
	g_assert (tasks != NULL);

	tasks->gconf_client = gconf_client_get_default ();

	gconf_client_add_dir (tasks->gconf_client, "/apps/evolution/calendar/tasks/colors", FALSE, NULL);
	gconf_client_add_dir (tasks->gconf_client, "/apps/evolution/shell/default_folders", FALSE, NULL);

	tasks->gconf_value_changed_handler_id
		= g_signal_connect (tasks->gconf_client, "value_changed",
				    G_CALLBACK (gconf_client_value_changed_cb), summary);
}

void
e_summary_tasks_init (ESummary *summary)
{
	ESummaryTasks *tasks;

	g_return_if_fail (summary != NULL);

	tasks = g_new0 (ESummaryTasks, 1);

	summary->tasks = tasks;

	setup_gconf_client (summary);
	setup_task_folder (summary);

	e_summary_add_protocol_listener (summary, "tasks", e_summary_tasks_protocol, tasks);
}

void
e_summary_tasks_reconfigure (ESummary *summary)
{
	setup_task_folder (summary);
	generate_html (summary);
}

void
e_summary_tasks_free (ESummary *summary)
{
	ESummaryTasks *tasks;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	tasks = summary->tasks;

	if (tasks->cal_open_reload_timeout_id != 0)
		g_source_remove (tasks->cal_open_reload_timeout_id);

	g_object_unref (tasks->client);
	g_free (tasks->html);
	g_free (tasks->due_today_colour);
	g_free (tasks->overdue_colour);
	g_free (tasks->default_uri);

	if (tasks->gconf_value_changed_handler_id != 0)
		g_signal_handler_disconnect (tasks->gconf_client,
					     tasks->gconf_value_changed_handler_id);
	g_object_unref (tasks->gconf_client);

	g_free (tasks);
	summary->tasks = NULL;
}
