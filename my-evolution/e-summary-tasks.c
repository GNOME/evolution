/*
 * e-summary-tasks.c:
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * Authors:  Iain Holmes  <iain@ximian.com>
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

struct _ESummaryTasks {
	CalClient *client;

	char *html;
};

const char *
e_summary_tasks_get_html (ESummary *summary)
{
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
	CalComponent *comp_a, *comp_b;
	ESummary *summary = user_data;
	ESummaryTasks *tasks = summary->tasks;
	CalClientGetStatus status;
	CalComponentDateTime start_a, start_b;

	/* a after b then return > 0 */

	status = cal_client_get_object (tasks->client, a, &comp_a);
	if (status != CAL_CLIENT_GET_SUCCESS)
		return -1;

	status = cal_client_get_object (tasks->client, b, &comp_b);
	if (status != CAL_CLIENT_GET_SUCCESS)
		return 1;

	cal_component_get_dtstart (comp_a, &start_a);
	cal_component_get_dtstart (comp_b, &start_b);

	return icaltime_compare (*start_a.value, *start_b.value);
}

static gboolean
generate_html (gpointer data)
{
	ESummary *summary = data;
	ESummaryTasks *tasks = summary->tasks;
	GList *uids, *l;
	GString *string;
	char *tmp;
	time_t t, day_begin, day_end;

	t = time (NULL);
	day_begin = time_day_begin (t);
	day_end = time_day_end (t);

	uids = cal_client_get_uids (tasks->client, CALOBJ_TYPE_TODO);
	if (uids == NULL) {
		char *s1, *s2;

		s1 = e_utf8_from_locale_string (_("Tasks"));
		s2 = e_utf8_from_locale_string (_("No tasks"));
		g_free (tasks->html);
		tasks->html = g_strconcat ("<dl><dt><img src=\"ico-calendar.png\" align=\"middle\" "
		                              "alt=\"\" width=\"48\" height=\"48\"> <b><a href=\"evolution:/local/Tasks\">",
		                              s1, "</a></b></dt><dd><b>", s2, "</b></dd></dl>", NULL);
		g_free (s1);
		g_free (s2);

		e_summary_draw (summary);
		return FALSE;
	} else {
		char *s;

		string = g_string_new ("<dl><dt><img src=\"ico-calendar.png\" align=\"middle\" "
		                       "alt=\"\" width=\"48\" height=\"48\"> <b><a href=\"evolution:/local/Tasks\">");
		s = e_utf8_from_locale_string (_("Tasks"));
		g_string_append (string, s);
		g_free (s);
		g_string_append (string, "</a></b></dt><dd>");
		for (l = uids; l; l = l->next) {
			char *uid;
			CalComponent *comp;
			CalComponentText text;
			CalClientGetStatus status;
			struct icaltimetype *completed;

			uid = l->data;
			status = cal_client_get_object (tasks->client, uid, &comp);
			if (status != CAL_CLIENT_GET_SUCCESS) {
				continue;
			}

			cal_component_get_summary (comp, &text);
			cal_component_get_completed (comp, &completed);
			
			if (completed == NULL) {
				tmp = g_strdup_printf ("<img align=\"middle\" src=\"es-appointments.png\" "
						       "alt=\"\" width=\"16\" height=\"16\">  &#160; "
						       "<font size=\"-1\"><a href=\"evolution:/local/Tasks\">%s</a></font><br>", 
						       text.value);
			} else {
				tmp = g_strdup_printf ("<img align=\"middle\" src=\"es-appointments.png\" "
						       "alt=\"\" width=\"16\" height=\"16\">  &#160; "
						       "<font size=\"-1\"><strike><a href=\"evolution:/local/Tasks\">%s</a></strike></font><br>",
						       text.value);
			}

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

static void
cal_opened_cb (CalClient *client,
	       CalClientOpenStatus status,
	       ESummary *summary)
{
	if (status == CAL_CLIENT_OPEN_SUCCESS) {
		g_idle_add (generate_html, summary);
	} else {
		/* Need to work out what to do if there's an error */
	}
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

}

void
e_summary_tasks_init (ESummary *summary)
{
	ESummaryTasks *tasks;
	gboolean result;
	char *uri;

	g_return_if_fail (summary != NULL);

	tasks = g_new (ESummaryTasks, 1);
	summary->tasks = tasks;
	tasks->html = NULL;

	tasks->client = cal_client_new ();
	if (tasks->client == NULL) {
		g_warning ("Error making the client");
		return;
	}

	gtk_signal_connect (GTK_OBJECT (tasks->client), "cal-opened",
			    GTK_SIGNAL_FUNC (cal_opened_cb), summary);
	gtk_signal_connect (GTK_OBJECT (tasks->client), "obj-updated",
			    GTK_SIGNAL_FUNC (obj_changed_cb), summary);
	gtk_signal_connect (GTK_OBJECT (tasks->client), "obj-removed",
			    GTK_SIGNAL_FUNC (obj_changed_cb), summary);

	uri = gnome_util_prepend_user_home ("evolution/local/Tasks/tasks.ics");
	result = cal_client_open_calendar (tasks->client, uri, FALSE);
	g_free (uri);
	if (result == FALSE) {
		g_message ("Open tasks failed");
	}

	e_summary_add_protocol_listener (summary, "tasks", e_summary_tasks_protocol, tasks);
}

void
e_summary_tasks_reconfigure (ESummary *summary)
{

}

void
e_summary_tasks_free (ESummary *summary)
{
	ESummaryTasks *tasks;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	tasks = summary->tasks;
	gtk_object_unref (GTK_OBJECT (tasks->client));
	g_free (tasks->html);

	g_free (tasks);
	summary->tasks = NULL;
}
