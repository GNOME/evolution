/*
 * e-summary-calendar.c:
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * Authors:  Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "e-summary-calendar.h"
#include "e-summary.h"
#include <cal-client/cal-client.h>
#include <cal-util/timeutil.h>

struct _ESummaryCalendar {
	CalClient *client;

	char *html;
};

const char *
e_summary_calendar_get_html (ESummary *summary)
{
	if (summary->calendar == NULL) {
		return NULL;
	}

	return summary->calendar->html;
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
	ESummaryCalendar *calendar = summary->calendar;
	CalClientGetStatus status;
	CalComponentDateTime start_a, start_b;

	/* a after b then return > 0 */

	status = cal_client_get_object (calendar->client, a, &comp_a);
	if (status != CAL_CLIENT_GET_SUCCESS)
		return -1;

	status = cal_client_get_object (calendar->client, b, &comp_b);
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
	ESummaryCalendar *calendar = summary->calendar;
	GList *uids, *l;
	GString *string;
	char *tmp;
	time_t t, day_begin, day_end;

	t = time (NULL);
	day_begin = time_day_begin (t);
	day_end = time_day_end (t);

	uids = cal_client_get_objects_in_range (calendar->client, 
						CALOBJ_TYPE_EVENT, day_begin,
						day_end);
	if (uids == NULL) {
		if (calendar->html) {
			g_free (calendar->html);
		}
		calendar->html = g_strdup ("<dl><dt><img src=\"ico-calendar.png\" align=\"middle\" "
					   "alt=\"\" width=\"48\" height=\"48\"> <b><a href=\"evolution:/local/Calendar\">Appointments</a>"
					   "</b></dt><dd><b>No appointments</b></dd></dl>");

		e_summary_draw (summary);
		return FALSE;
	} else {
		uids = cal_list_sort (uids, sort_uids, summary);

		string = g_string_new ("<dl><dt><img src=\"ico-calendar.png\" align=\"middle\" "
				       "alt=\"\" width=\"48\" height=\"48\"> <b><a href=\"evolution:/local/Calendar\">Appointments</a>"
				       "</b></dt><dd>");
		for (l = uids; l; l = l->next) {
			char *uid, *start_str;
			CalComponent *comp;
			CalComponentText text;
			CalClientGetStatus status;
			CalComponentDateTime start, end;
			time_t start_t;
			struct tm *start_tm;

			uid = l->data;
			status = cal_client_get_object (calendar->client, uid, &comp);
			if (status != CAL_CLIENT_GET_SUCCESS) {
				continue;
			}

			cal_component_get_summary (comp, &text);
			cal_component_get_dtstart (comp, &start);
			cal_component_get_dtend (comp, &end);
			
			start_t = icaltime_as_timet (*start.value);

			start_str = g_new (char, 20);
			start_tm = localtime (&start_t);
			strftime (start_str, 19, _("%I:%M%p"), start_tm);

#if 0
			end_str = g_new (char, 20);
			end_t = icaltime_as_timet (*end.value);
			end_tm = localtime (&end_t);
			strftime (end_str, 19, _("%I:%M%p"), end_tm);
#endif
			tmp = g_strdup_printf ("<img align=\"middle\" src=\"es-appointments.png\" "
					       "alt=\"\" width=\"16\" height=\"16\">  &#160; "
					       "<font size=\"-1\"><a href=\"#\">%s, %s</a></font><br>", 
					       start_str, text.value);
			g_free (start_str);
			
			g_string_append (string, tmp);
			g_free (tmp);
		}

		cal_obj_uid_list_free (uids);
		g_string_append (string, "</ul>");
	}

	if (calendar->html) {
		g_free (calendar->html);
	}
	calendar->html = string->str;
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
e_summary_calendar_protocol (ESummary *summary,
			     const char *uri,
			     void *closure)
{

}

void
e_summary_calendar_init (ESummary *summary)
{
	ESummaryCalendar *calendar;
	gboolean result;
	char *uri;

	g_return_if_fail (summary != NULL);

	calendar = g_new (ESummaryCalendar, 1);
	summary->calendar = calendar;
	calendar->html = NULL;

	calendar->client = cal_client_new ();
	if (calendar->client == NULL) {
		g_warning ("Error making the client");
		return;
	}

	gtk_signal_connect (GTK_OBJECT (calendar->client), "cal-opened",
			    GTK_SIGNAL_FUNC (cal_opened_cb), summary);
	gtk_signal_connect (GTK_OBJECT (calendar->client), "obj-updated",
			    GTK_SIGNAL_FUNC (obj_changed_cb), summary);
	gtk_signal_connect (GTK_OBJECT (calendar->client), "obj-removed",
			    GTK_SIGNAL_FUNC (obj_changed_cb), summary);

	uri = gnome_util_prepend_user_home ("evolution/local/Calendar/calendar.ics");
	result = cal_client_open_calendar (calendar->client, uri, FALSE);
	g_free (uri);
	if (result == FALSE) {
		g_message ("Open calendar failed");
	}

	e_summary_add_protocol_listener (summary, "calendar", e_summary_calendar_protocol, calendar);
}
