/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* calendar-summary.c
 *
 * Authors: Iain Holmes <iain@ximian.com>
 *
 * Copyright (C) 2000  Helix Code, Inc.
 * Copyright (C) 2000  Ximian, Inc.
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

#include <bonobo/bonobo-persist-stream.h>
#include <bonobo/bonobo-property-control.h>
#include <bonobo/bonobo-stream-client.h>
#include <liboaf/liboaf.h>

#include <evolution-services/executive-summary-component.h>
#include <evolution-services/executive-summary-html-view.h>
#include <gal/widgets/e-unicode.h>

#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include <cal-util/cal-component.h>
#include <cal-util/timeutil.h>
#include "alarm-notify/alarm.h"
#include "calendar-model.h"
#include "calendar-summary.h"

typedef struct {
	ExecutiveSummaryComponent *component;
	ExecutiveSummaryHtmlView *view;
	BonoboPropertyControl *property_control;
	CalClient *client;

	GtkWidget *show_appointments;
	GtkWidget *show_tasks;

	gboolean appointments;
	gboolean tasks;

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
	CalSummary *summary = user_data;
	CalClientGetStatus status;
	CalComponentDateTime start_a, start_b;

	/* a after b then return > 0 */

	status = cal_client_get_object (summary->client, a, &comp_a);
	if (status != CAL_CLIENT_GET_SUCCESS)
		return -1;

	status = cal_client_get_object (summary->client, b, &comp_b);
	if (status != CAL_CLIENT_GET_SUCCESS)
		return 1;

	cal_component_get_dtstart (comp_a, &start_a);
	cal_component_get_dtstart (comp_b, &start_b);

	return icaltime_compare (*start_a.value, *start_b.value);
}

static gboolean
generate_html_summary (gpointer data)
{
	CalSummary *summary;	
	time_t t, day_begin, day_end;
	struct tm *timeptr;
	GList *uids, *l;
	char *ret_html, *datestr;
	char *tmp, *tmp2;

	summary = data;
	
	t = time (NULL);
	day_begin = time_day_begin (t);
	day_end = time_day_end (t);

	datestr = g_new (char, 256);
	timeptr = localtime (&t);
	strftime (datestr, 255, _("%A, %e %B %Y"),
		  timeptr);
	tmp = g_strdup_printf ("<b>%s</b>", datestr);
	ret_html = e_utf8_from_locale_string (tmp);
	g_free (tmp);
	g_free (datestr);

	if (summary->appointments) {
		tmp = ret_html;
		tmp2 = e_utf8_from_locale_string (_("Appointments"));
		ret_html = g_strconcat (tmp, "<p align=\"center\">",
					tmp2, "</p><hr><ul>", NULL);
		g_free (tmp);
		g_free (tmp2);
		
		uids = cal_client_get_objects_in_range (summary->client, 
							CALOBJ_TYPE_EVENT, day_begin,
							day_end);
		uids = cal_list_sort (uids, sort_uids, summary);

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
		ret_html = g_strconcat (ret_html, "</ul>", NULL);
		g_free (tmp);
	}

	if (summary->tasks) {
		tmp = ret_html;
		tmp2 = e_utf8_from_locale_string (_("Tasks"));
		ret_html = g_strconcat (tmp, "<p align=\"center\">",
					tmp2, "</p><hr><ul>", NULL);
		g_free (tmp);
		g_free (tmp2);
		
		/* Generate a list of tasks */
		uids = cal_client_get_uids (summary->client, CALOBJ_TYPE_TODO);
		for (l = uids; l; l = l->next){
			CalComponent *comp;
			CalComponentText text;
			CalClientGetStatus status;
			struct icaltimetype *completed;
			char *uid;
			
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
	}

	executive_summary_html_view_set_html (summary->view, ret_html);
	g_free (ret_html);
	
	summary->idle = 0;
	return FALSE;
}

static void
get_property (BonoboPropertyBag *bag,
	      BonoboArg *arg,
	      guint arg_id,
	      CORBA_Environment *ev,
	      gpointer data)
{
	CalSummary *summary = (CalSummary *) data;

	switch (arg_id) {
	case PROPERTY_TITLE:
		g_warning ("Get property: %s", summary->title);
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
set_property (BonoboPropertyBag *bag,
	      const BonoboArg *arg,
	      guint arg_id,
	      CORBA_Environment *ev,
	      gpointer user_data)
{
	CalSummary *summary = (CalSummary *) user_data;

	switch (arg_id) {
	case PROPERTY_TITLE:
		if (summary->title)
			g_free (summary->title);

		summary->title = g_strdup (BONOBO_ARG_GET_STRING (arg));
		bonobo_property_bag_notify_listeners (bag, "window_title",
						      arg, NULL);
		break;

	case PROPERTY_ICON:
		if (summary->icon)
			g_free (summary->icon);

		summary->icon = g_strdup (BONOBO_ARG_GET_STRING (arg));
		bonobo_property_bag_notify_listeners (bag, "window_icon",
						      arg, NULL);
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
cal_opened_cb (CalClient *client,
	       CalClientOpenStatus status,
	       CalSummary *summary)
{
	switch (status) {
	case CAL_CLIENT_OPEN_SUCCESS:
		if (summary->idle != 0)
			return;

		summary->idle = g_idle_add (generate_html_summary, summary);
		break;

	case CAL_CLIENT_OPEN_ERROR:
		executive_summary_html_view_set_html (summary->view,
						      _("<b>Error loading calendar</b>"));
		break;

	case CAL_CLIENT_OPEN_NOT_FOUND:
		/* We did not use only_if_exists when opening the calendar, so
		 * this should not happen.
		 */
		g_assert_not_reached ();
		break;

	case CAL_CLIENT_OPEN_METHOD_NOT_SUPPORTED:
		executive_summary_html_view_set_html (summary->view,
						      _("<b>Error loading calendar:<br>Method not supported"));
		break;

	default:
		break;
	}
}

static void
alarm_fn (gpointer alarm_id,
	  time_t trigger,
	  gpointer data)
{
	CalSummary *summary;
	time_t t, day_end;

	summary = data;

	t = time (NULL);
	day_end = time_day_end (t);
	summary->alarm = alarm_add (day_end, alarm_fn, summary, NULL);

	/* Now redraw the summary */
	generate_html_summary (summary);
}

/* PersistStream callbacks */
static void
load_from_stream (BonoboPersistStream *ps,
		  Bonobo_Stream stream,
		  Bonobo_Persist_ContentType type,
		  gpointer data,
		  CORBA_Environment *ev)
{
	CalSummary *summary = (CalSummary *) data;
	char *str;
	xmlChar *xml_str;
	xmlDocPtr doc;
	xmlNodePtr root, children;

	if (*type && g_strcasecmp (type, "application/x-evolution-calendar-summary") != 0) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, 
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	bonobo_stream_client_read_string (stream, &str, ev);
	if (ev->_major != CORBA_NO_EXCEPTION || str == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	doc = xmlParseDoc ((xmlChar *) str);

	if (doc == NULL) {
		g_warning ("Bad data: %s!", str);
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		g_free (str);
		return;
	}

	g_free (str);
	root = doc->root;
	children = root->childs;
	while (children) {
		if (strcasecmp (children->name, "showappointments") == 0) {
			xml_str = xmlNodeListGetString (doc, children->childs, 1);
			if (strcmp (xml_str, "TRUE") == 0)
				summary->appointments = TRUE;
			else 
				summary->appointments = FALSE;
			xmlFree (xml_str);

			children = children->next;
			continue;
		}

		if (strcasecmp (children->name, "showtasks") == 0) {
			xml_str = xmlNodeListGetString (doc, children->childs, 1);
			if (strcmp (xml_str, "TRUE") == 0)
				summary->tasks = TRUE;
			else
				summary->tasks = FALSE;
			xmlFree (xml_str);

			children = children->next;
			continue;
		}

		g_print ("Unknown name: %s\n", children->name);
		children = children->next;
	}
	xmlFreeDoc (doc);

	summary->idle = g_idle_add (generate_html_summary, summary);
}

static char *
summary_to_string (CalSummary *summary)
{
	xmlChar *out_str;
	int out_len = 0;
	xmlDocPtr doc;
	xmlNsPtr ns;

	doc = xmlNewDoc ("1.0");
	ns = xmlNewGlobalNs (doc, "http://www.ximian.com", "calendar-summary");
	doc->root = xmlNewDocNode (doc, ns, "calendar-summary", NULL);

	xmlNewChild (doc->root, ns, "showappointments", 
		     summary->appointments ? "TRUE" : "FALSE");
	xmlNewChild (doc->root, ns, "showtasks", summary->tasks ? "TRUE" : "FALSE");
	
	xmlDocDumpMemory (doc, &out_str, &out_len);
	return out_str;
}

static void
save_to_stream (BonoboPersistStream *ps,
		const Bonobo_Stream stream,
		Bonobo_Persist_ContentType type,
		gpointer data,
		CORBA_Environment *ev)
{
	CalSummary *summary = (CalSummary *) data;
	char *str;

	if (*type && g_strcasecmp (type, "application/x-evolution-calendar-summary") != 0) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	str = summary_to_string (summary);
	if (str)
		bonobo_stream_client_printf (stream, TRUE, ev, str);
	xmlFree (str);

	return;
}

static Bonobo_Persist_ContentTypeList *
content_types (BonoboPersistStream *ps,
	       void *closure,
	       CORBA_Environment *ev)
{
	return bonobo_persist_generate_content_types (1, "application/x-evolution-calendar-summary");
}

static void
property_dialog_changed (GtkWidget *widget,
			 CalSummary *summary)
{
	bonobo_property_control_changed (summary->property_control, NULL);
}

static BonoboControl *
property_dialog (BonoboPropertyControl *property_control,
		 int page_num,
		 void *user_data)
{
	BonoboControl *control;
	CalSummary *summary = (CalSummary *) user_data;
	GtkWidget *container, *vbox;

	container = gtk_frame_new (_("Display"));
	gtk_container_set_border_width (GTK_CONTAINER (container), 2);
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (container), vbox);
	
	summary->show_appointments = gtk_check_button_new_with_label (_("Show appointments"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (summary->show_appointments), 
				      summary->appointments);
	gtk_signal_connect (GTK_OBJECT (summary->show_appointments), "toggled",
			    GTK_SIGNAL_FUNC (property_dialog_changed), summary);
	gtk_box_pack_start (GTK_BOX (vbox), summary->show_appointments, 
			    TRUE, TRUE, 0);
	
	summary->show_tasks = gtk_check_button_new_with_label (_("Show tasks"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (summary->show_tasks),
				      summary->tasks);
	gtk_signal_connect (GTK_OBJECT (summary->show_tasks), "toggled",
			    GTK_SIGNAL_FUNC (property_dialog_changed), summary);
	gtk_box_pack_start (GTK_BOX (vbox), summary->show_tasks, TRUE, TRUE, 0);
	gtk_widget_show_all (container);

	control = bonobo_control_new (container);
	return control;
}

static void
property_action (GtkObject *property_control,
		 int page_num,
		 Bonobo_PropertyControl_Action action,
		 CalSummary *summary)
{
	switch (action) {
	case Bonobo_PropertyControl_APPLY:
		summary->appointments = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (summary->show_appointments));
		summary->tasks = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (summary->show_tasks));
		summary->idle = g_idle_add (generate_html_summary, summary);
		break;

	case Bonobo_PropertyControl_HELP:
		g_print ("HELP\n");
		break;

	default:
		break;
	}
}

BonoboObject *
create_summary_view (ExecutiveSummaryComponentFactory *_factory,
		     void *closure)
{
	BonoboObject *component, *view;
	BonoboPersistStream *stream;
	BonoboPropertyBag *bag;
	BonoboPropertyControl *property_control;
	BonoboEventSource *event_source;
	CalSummary *summary;
	char *file;
	time_t t, day_end;

	file = g_concat_dir_and_file (evolution_dir, "local/Calendar/calendar.ics");

	/* Create the component object */
	component = executive_summary_component_new ();

	summary = g_new (CalSummary, 1);
	summary->component = EXECUTIVE_SUMMARY_COMPONENT (component);
	summary->icon = g_strdup ("evolution-calendar.png");
	summary->title = e_utf8_from_locale_string (_("Things to do"));
	summary->client = cal_client_new ();
	summary->idle = 0;
	summary->appointments = TRUE;
	summary->tasks = TRUE;

	t = time (NULL);
	day_end = time_day_end (t);
	summary->alarm = alarm_add (day_end, alarm_fn, summary, NULL);

	/* Load calendar */
	cal_client_open_calendar (summary->client, file, FALSE);
	g_free (file);

	gtk_signal_connect (GTK_OBJECT (summary->client), "cal-opened",
			    GTK_SIGNAL_FUNC (cal_opened_cb), summary);
	gtk_signal_connect (GTK_OBJECT (summary->client), "obj-updated",
			    GTK_SIGNAL_FUNC (obj_updated_cb), summary);
	gtk_signal_connect (GTK_OBJECT (summary->client), "obj-removed",
			    GTK_SIGNAL_FUNC (obj_removed_cb), summary);
		
	gtk_signal_connect (GTK_OBJECT (component), "destroy",
			    GTK_SIGNAL_FUNC (component_destroyed), summary);

	event_source = bonobo_event_source_new ();

	/* HTML view */
	view = executive_summary_html_view_new_full (event_source);
	summary->view = EXECUTIVE_SUMMARY_HTML_VIEW (view);

	executive_summary_html_view_set_html (EXECUTIVE_SUMMARY_HTML_VIEW (view),
					      _("Loading Calendar"));
	bonobo_object_add_interface (component, view);

	/* BonoboPropertyBag */
	bag = bonobo_property_bag_new_full (get_property, set_property, 
					    event_source, summary);
	bonobo_property_bag_add (bag, "window_title", PROPERTY_TITLE,
				 BONOBO_ARG_STRING, NULL, 
				 "The title of this component's window", 0);
	bonobo_property_bag_add (bag, "window_icon", PROPERTY_ICON,
				 BONOBO_ARG_STRING, NULL, 
				 "The icon for this component's window", 0);
	bonobo_object_add_interface (component, BONOBO_OBJECT (bag));

	property_control = bonobo_property_control_new_full (property_dialog, 
							     1, event_source,
							     summary);
	summary->property_control = property_control;
	gtk_signal_connect (GTK_OBJECT (property_control), "action",
			    GTK_SIGNAL_FUNC (property_action), summary);
	bonobo_object_add_interface (component, BONOBO_OBJECT (property_control));
			    
	stream = bonobo_persist_stream_new (load_from_stream, save_to_stream,
					    NULL, content_types, summary);
	bonobo_object_add_interface (component, BONOBO_OBJECT (stream));

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
		g_warning ("Cannot initialize calendar summary factory");
		return NULL;
	}

	return factory;
}
