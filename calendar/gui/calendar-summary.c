/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* calendar-summary.c
 *
 * Authors: Iain Holmes <iain@helixcode.com>
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>

#include <bonobo.h>

#include <gnome.h>
#include <liboaf/liboaf.h>

#include <evolution-services/executive-summary-component.h>
#include <evolution-services/executive-summary-html-view.h>

#include "cal-util/cal-component.h"
#include "cal-util/timeutil.h"
#include "calendar-model.h"

#include "calendar-summary.h"

typedef struct {
	ExecutiveSummaryComponent *component;
	ExecutiveSummaryHtmlView *view;
	CalClient *client;
	gboolean cal_loaded;

	char *title;
	char *icon;
	
	guint32 idle;

	gpointer alarm;
} CalSummary;

enum {
	PROPERTY_TITLE,
	PROPERTY_ICON
};

extern gchar *evolution_dir;

static int running_views = 0;
static BonoboGenericFactory *factory;
#define CALENDAR_SUMMARY_ID "OAFIID:GNOME_Evolution_Calendar_Summary_ComponentFactory"

static gboolean
generate_html_summary (CalSummary *summary)
{
	time_t t, day_begin, day_end;
	struct tm *timeptr;
	GList *uids, *l;
	char *ret_html, *tmp, *datestr;
	
	t = time (NULL);
	day_begin = time_day_begin (t);
	day_end = time_day_end (t);

	datestr = g_new (char, 256);
	timeptr = localtime (&t);
	strftime (datestr, 255, _("%A, %e %B %Y"), timeptr);
	ret_html = g_strdup_printf ("<p align=\"center\">Appointments</p>"
				    "<hr><b>%s</b><br><ul>", datestr);
	g_free (datestr);

	uids = cal_client_get_objects_in_range (summary->client, 
						CALOBJ_TYPE_EVENT, day_begin,
						day_end);
	for (l = uids; l; l = l->next){
		CalComponent *comp;
		CalComponentText text;
		CalClientGetStatus status;
		CalComponentDateTime start, end;
		struct icaltimetype *end_time;
		time_t start_t, end_t;
		struct tm *start_tm, *end_tm;
		char *start_str, *end_str;
		char *uid;
		char *tmp2;
		
		uid = l->data;
		status = cal_client_get_object (summary->client, uid, &comp);
		if (status != CAL_CLIENT_GET_SUCCESS)
			continue;
		
		cal_component_get_summary (comp, &text);
		cal_component_get_dtstart (comp, &start);
		cal_component_get_dtend (comp, &end);

		g_print ("text.value: %s\n", text.value);
		end_time = end.value;

		start_t = icaltime_as_timet (*start.value);

		start_str = g_new (char, 20);
		start_tm = localtime (&start_t);
		strftime (start_str, 19, _("%I:%M%p"), start_tm);

		if (end_time) {
			end_str = g_new (char, 20);
			end_t = icaltime_as_timet (*end_time);
			end_tm = localtime (&end_t);
			strftime (end_str, 19, _("%I:%M%p"), end_tm);
		} else {
			end_str = g_strdup ("...");
		}

		tmp2 = g_strdup_printf ("<li>%s:%s -> %s</li>", text.value, start_str, end_str);
		g_free (start_str);
		g_free (end_str);

		tmp = ret_html;
		ret_html = g_strconcat (ret_html, tmp2, NULL);
		g_free (tmp);
		g_free (tmp2);
	}
	
	cal_obj_uid_list_free (uids);
	
	tmp = ret_html;
	ret_html = g_strconcat (ret_html, 
				"</ul><p align=\"center\">Tasks<hr><ul>", 
				NULL);
	g_free (tmp);

	/* Generate a list of tasks */
	uids = cal_client_get_uids (summary->client, CALOBJ_TYPE_TODO);
	for (l = uids; l; l = l->next){
		CalComponent *comp;
		CalComponentText text;
		CalClientGetStatus status;
		struct icaltimetype *completed;
		char *uid;
		char *tmp2;
		
		uid = l->data;
		status = cal_client_get_object (summary->client, uid, &comp);
		if (status != CAL_CLIENT_GET_SUCCESS)
			continue;
		
		cal_component_get_summary (comp, &text);
		cal_component_get_completed (comp, &completed);

		if (completed == NULL) {
			tmp2 = g_strdup_printf ("<li>%s</li>", text.value);
		} else {
			tmp2 = g_strdup_printf ("<li><strike>%s</strike></li>",
						text.value);
			cal_component_free_icaltimetype (completed);
		}

		tmp = ret_html;
		ret_html = g_strconcat (ret_html, tmp2, NULL);
		g_free (tmp);
		g_free (tmp2);
	}
	
	cal_obj_uid_list_free (uids);
	
	tmp = ret_html;
	ret_html = g_strconcat (ret_html, "</ul>", NULL);
	g_free (tmp);
	
	executive_summary_html_view_set_html (summary->view, ret_html);
	g_free (ret_html);

	summary->idle = 0;
	return FALSE;
}

static void
get_property (BonoboPropertyBag *bag,
	      BonoboArg *arg,
	      guint arg_id,
	      gpointer data)
{
	CalSummary *summary = (CalSummary *) data;

	switch (arg_id) {
	case PROPERTY_TITLE:
		BONOBO_ARG_SET_STRING (arg, summary->title);
		break;

	case PROPERTY_ICON:
		BONOBO_ARG_SET_STRING (arg, summary->icon);
		break;

	default:
		break;
	}
}

static void
component_destroyed (GtkObject *object,
		     gpointer data)
{
	CalSummary *summary = (CalSummary *) data;

	g_free (summary->title);
	g_free (summary->icon);
	gtk_object_destroy (GTK_OBJECT (summary->client));

	g_free (summary);

	running_views--;

	if (running_views <= 0) {
		bonobo_object_unref (BONOBO_OBJECT (factory));
	}
}

static void
obj_updated_cb (CalClient *client,
		const char *uid,
		CalSummary *summary)
{
	/* FIXME: Maybe cache the uid's in the summary and only call this if
	   uid is in this cache??? */

	if (summary->idle != 0) 
		return;

	summary->idle = g_idle_add (generate_html_summary, summary);
}

static void
obj_removed_cb (CalClient *client,
		const char *uid,
		CalSummary *summary)
{
	/* See FIXME: above */
	if (summary->idle != 0)
		return;

	summary->idle = g_idle_add (generate_html_summary, summary);
}
static void
cal_loaded_cb (CalClient *client,
	       CalClientLoadStatus status,
	       CalSummary *summary)
{
	switch (status) {
	case CAL_CLIENT_LOAD_SUCCESS:
		summary->cal_loaded = TRUE;

		if (summary->idle != 0)
			return;

		summary->idle = g_idle_add (generate_html_summary, summary);
		break;

	case CAL_CLIENT_LOAD_ERROR:
		executive_summary_html_view_set_html (summary->view,
						      _("<b>Error loading calendar</b>"));
		break;

	case CAL_CLIENT_LOAD_IN_USE:
		executive_summary_html_view_set_html (summary->view,
						      _("<b>Error loading calendar:<br>Calendar in use."));
		
		break;

	case CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED:
		executive_summary_html_view_set_html (summary->view,
						      _("<b>Error loading calendar:<br>Method not supported"));
		break;

	default:
		break;
	}
}
static void
alarm_fn (gpointer alarm_id,
	  time_t old_t,
	  CalSummary *summary)
{
	time_t t, day_end;

	/* Remove the old alarm, and start a new one for the next midnight */
	alarm_remove (alarm_id);

	t = time (NULL);
	day_end = time_day_end (t);
	summary->alarm = alarm_add (day_end, alarm_fn, summary, NULL);

	/* Now redraw the summary */
	generate_html_summary (summary);
}

BonoboObject *
create_summary_view (ExecutiveSummaryComponentFactory *_factory,
		     void *closure)
{
	BonoboObject *component, *view;
	BonoboPersistStream *stream;
	BonoboPropertyBag *bag;
	BonoboEventSource *event_source;
	CalSummary *summary;
	char *html, *file;
	time_t t, day_end;

	file = g_concat_dir_and_file (evolution_dir, "local/Calendar/calendar.ics");

	/* Create the component object */
	component = executive_summary_component_new ();

	summary = g_new (CalSummary, 1);
	summary->component = component;
	summary->icon = g_strdup ("evolution-calendar.png");
	summary->title = g_strdup ("Things to do");
	summary->client = cal_client_new ();
	summary->cal_loaded = FALSE;
	summary->idle = 0;

	t = time (NULL);
	day_end = time_day_end (t);
	summary->alarm = alarm_add (day_end, alarm_fn, summary, NULL);

	/* Check for calendar files */
	if (!g_file_exists (file)) {
		cal_client_create_calendar (summary->client, file);
	}

	/* Load calendar */
	cal_client_load_calendar (summary->client, file);
	g_free (file);

	gtk_signal_connect (GTK_OBJECT (summary->client), "cal-loaded",
			    GTK_SIGNAL_FUNC (cal_loaded_cb), summary);
	gtk_signal_connect (GTK_OBJECT (summary->client), "obj-updated",
			    GTK_SIGNAL_FUNC (obj_updated_cb), summary);
	gtk_signal_connect (GTK_OBJECT (summary->client), "obj-removed",
			    GTK_SIGNAL_FUNC (obj_removed_cb), summary);
		
	gtk_signal_connect (GTK_OBJECT (component), "destroy",
			    GTK_SIGNAL_FUNC (component_destroyed), summary);

	event_source = bonobo_event_source_new ();

	/* HTML view */
	view = executive_summary_html_view_new_full (event_source);
	summary->view = view;

	executive_summary_html_view_set_html (EXECUTIVE_SUMMARY_HTML_VIEW (view),
					      _("Loading Calendar"));
	bonobo_object_add_interface (component, view);

	/* BonoboPropertyBag */
	bag = bonobo_property_bag_new_full (get_property, NULL, 
					    event_source, summary);
	bonobo_property_bag_add (bag, "window_title", PROPERTY_TITLE,
				 BONOBO_ARG_STRING, NULL, 
				 "The title of this component's window", 0);
	bonobo_property_bag_add (bag, "window_icon", PROPERTY_ICON,
				 BONOBO_ARG_STRING, NULL, 
				 "The icon for this component's window", 0);
	bonobo_object_add_interface (component, BONOBO_OBJECT (bag));

	running_views++;

	return component;
}

static BonoboObject *
factory_fn (BonoboGenericFactory *generic_factory,
	    void *closure)
{
	BonoboObject *_factory;

	_factory = executive_summary_component_factory_new (create_summary_view,
							    NULL);
	return _factory;
}

BonoboGenericFactory *
calendar_summary_factory_init (void)
{
	if (factory != NULL)
		return factory;

	factory = bonobo_generic_factory_new (CALENDAR_SUMMARY_ID, factory_fn,
					      NULL);

	if (factory == NULL) {
		g_warning ("Cannot initialise calendar summary factory");
		return NULL;
	}

	return factory;
}
