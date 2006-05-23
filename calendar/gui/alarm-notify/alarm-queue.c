/* Evolution calendar - Alarm queueing engine
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-main.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkversion.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-sound.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-uidefs.h>

#if GTK_CHECK_VERSION (2, 9, 0)
#include <gtk/gtkstatusicon.h>
#else
#include <e-util/eggtrayicon.h>
#endif

/* Evo's copy of eggtrayicon, for Win32, contains the gtkstatusicon
 * API.
 */
#if GTK_CHECK_VERSION (2, 9, 0) || defined (GDK_WINDOWING_WIN32)
#define USE_GTK_STATUS_ICON
#endif

#include <e-util/e-icon-factory.h>
#include <libecal/e-cal-time-util.h>
#include <libecal/e-cal-component.h>


#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#include "evolution-calendar.h"
#include "alarm.h"
#include "alarm-notify-dialog.h"
#include "alarm-queue.h"
#include "alarm-notify.h"
#include "config-data.h"
#include "util.h"
#include "e-util/e-popup.h"
#include "e-util/e-error.h"



#define d(x) x

/* The dialog with alarm nofications */
static AlarmNotificationsDialog *alarm_notifications_dialog = NULL;

/* Whether the queueing system has been initialized */
static gboolean alarm_queue_inited;

/* When the alarm queue system is inited, this gets set to the last time an
 * alarm notification was issued.  This lets us present any notifications that
 * should have happened while the alarm daemon was not running.
 */
static time_t saved_notification_time;

/* Clients we are monitoring for alarms */
static GHashTable *client_alarms_hash = NULL;

/* List of tray icons being displayed */
static GList *tray_icons_list = NULL;

/* Top Tray Image */
#ifndef USE_GTK_STATUS_ICON
static GtkWidget *tray_icon = NULL;
static GtkWidget *tray_image = NULL;
static GtkWidget *tray_event_box = NULL;
#else
static GtkStatusIcon *tray_icon = NULL;
#endif
static int tray_blink_id = -1;
static int tray_blink_state = FALSE;
static AlarmNotify *an;

/* Main Tasks thread for dealing with the global structures */
extern EThread *alarm_operation_thread;

/* Structure that stores a client we are monitoring */
typedef struct {
	/* Monitored client */
	ECal *client;

	/* The live query to the calendar */
	ECalView *query;

	/* Hash table of component UID -> CompQueuedAlarms.  If an element is
	 * present here, then it means its cqa->queued_alarms contains at least
	 * one queued alarm.  When all the alarms for a component have been
	 * dequeued, the CompQueuedAlarms structure is removed from the hash
	 * table.  Thus a CQA exists <=> it has queued alarms.
	 */
	GHashTable *uid_alarms_hash;
} ClientAlarms;

/* Pair of a ECalComponentAlarms and the mapping from queued alarm IDs to the
 * actual alarm instance structures.
 */
typedef struct {
	/* The parent client alarms structure */
	ClientAlarms *parent_client;

	/* The component's UID */
	ECalComponentId *id;

	/* The actual component and its alarm instances */
	ECalComponentAlarms *alarms;

	/* List of QueuedAlarm structures */
	GSList *queued_alarms;

	/* Flags */
	gboolean expecting_update;
} CompQueuedAlarms;

/* Pair of a queued alarm ID and the alarm trigger instance it refers to */
typedef struct {
	/* Alarm ID from alarm.h */
	gpointer alarm_id;

	/* Instance from our parent CompQueuedAlarms->alarms->alarms list */
	ECalComponentAlarmInstance *instance;

	/* original trigger of the instance from component */
	time_t orig_trigger;

	/* Whether this is a snoozed queued alarm or a normal one */
	guint snooze : 1;
} QueuedAlarm;

/* Alarm ID for the midnight refresh function */
static gpointer midnight_refresh_id = NULL;
static time_t midnight = 0;

static void
remove_client_alarms (ClientAlarms *ca);
static void display_notification (time_t trigger, CompQueuedAlarms *cqa,
				  gpointer alarm_id, gboolean use_description);
static void audio_notification (time_t trigger, CompQueuedAlarms *cqa, gpointer alarm_id);
static void mail_notification (time_t trigger, CompQueuedAlarms *cqa, gpointer alarm_id);
static void procedure_notification (time_t trigger, CompQueuedAlarms *cqa, gpointer alarm_id);
#ifdef HAVE_LIBNOTIFY
static void popup_notification (time_t trigger, CompQueuedAlarms *cqa,
				  gpointer alarm_id, gboolean use_description);
#endif
static void query_objects_changed_cb (ECal *client, GList *objects, gpointer data);
static void query_objects_removed_cb (ECal *client, GList *objects, gpointer data);

static void remove_client_alarms (ClientAlarms *ca);
static void update_cqa (CompQueuedAlarms *cqa, ECalComponent *comp);
static void update_qa (ECalComponentAlarms *alarms, QueuedAlarm *qa); 
static void tray_list_remove_cqa (CompQueuedAlarms *cqa);
static void on_dialog_objs_removed_cb (ECal *client, GList *objects, gpointer data);

/* Alarm queue engine */

static void load_alarms_for_today (ClientAlarms *ca);
static void midnight_refresh_cb (gpointer alarm_id, time_t trigger, gpointer data);

/* Queues an alarm trigger for midnight so that we can load the next day's worth
 * of alarms.
 */
static void
queue_midnight_refresh (void)
{
	icaltimezone *zone;

	if (midnight_refresh_id != NULL) {
		alarm_remove (midnight_refresh_id);
		midnight_refresh_id = NULL;
	}

	zone = config_data_get_timezone ();
	midnight = time_day_end_with_zone (time (NULL), zone);

	d(printf("%s:%d (queue_midnight_refresh) - Refresh at %s \n",__FILE__, __LINE__, ctime(&midnight)));
	
	midnight_refresh_id = alarm_add (midnight, midnight_refresh_cb, NULL, NULL);
	if (!midnight_refresh_id) {
		 d(printf("%s:%d (queue_midnight_refresh)) - Could not setup the midnight refresh alarm\n",__FILE__, __LINE__));
		/* FIXME: what to do? */
	}
}

/* Loads a client's alarms; called from g_hash_table_foreach() */
static void
add_client_alarms_cb (gpointer key, gpointer value, gpointer data)
{
	ClientAlarms *ca;
	
	d(printf("%s:%d (add_client_alarms_cb) - Adding %d\n",__FILE__, __LINE__, ca));
	
	ca = value;
	load_alarms_for_today (ca);
}

struct _midnight_refresh_msg {
	gboolean remove;
};

/* Loads the alarms for the new day every midnight */
static void
midnight_refresh_async (EThread *e, AlarmMsg *msg, void *data)
{
	struct _midnight_refresh_msg *list = msg->data;

	d(printf("%s:%d (midnight_refresh_async) \n",__FILE__, __LINE__));
	
	/* Re-load the alarms for all clients */
	g_hash_table_foreach (client_alarms_hash, add_client_alarms_cb, NULL);

	/* Re-schedule the midnight update */
	if (list->remove && midnight_refresh_id != NULL) {
		d(printf("%s:%d (midnight_refresh_async)) - Reschedule the midnight update \n",__FILE__, __LINE__)); 
		alarm_remove (midnight_refresh_id);
		midnight_refresh_id = NULL;
	}

	queue_midnight_refresh ();
}

static void
midnight_refresh_cb (gpointer alarm_id, time_t trigger, gpointer data)
{
	AlarmMsg *msg;
	struct _midnight_refresh_msg *list;
	
	/* These two structures will be freed by the msg destroy function*/
	msg = malloc (sizeof (AlarmMsg));
	msg->receive_msg = midnight_refresh_async;

	list = malloc (sizeof (struct _midnight_refresh_msg));

	list->remove = TRUE;
	msg->data = list;

	d(printf("%s:%d (midnight_refresh_cb) - Invoking task for midnight refresh\n",__FILE__, __LINE__));
	e_thread_put(alarm_operation_thread, (EMsg *)msg);
}

/* Looks up a client in the client alarms hash table */
static ClientAlarms *
lookup_client (ECal *client)
{
	return g_hash_table_lookup (client_alarms_hash, client);
}

/* Looks up a queued alarm based on its alarm ID */
static QueuedAlarm *
lookup_queued_alarm (CompQueuedAlarms *cqa, gpointer alarm_id)
{
	GSList *l;
	QueuedAlarm *qa;

	qa = NULL;

	for (l = cqa->queued_alarms; l; l = l->next) {
		qa = l->data;
		if (qa->alarm_id == alarm_id)
			return qa;
	}

	/* not found, might have been updated/removed */
	return NULL;
}

/* Removes an alarm from the list of alarms of a component.  If the alarm was
 * the last one listed for the component, it removes the component itself.
 */
static void
remove_queued_alarm (CompQueuedAlarms *cqa, gpointer alarm_id,
		     gboolean free_object, gboolean remove_alarm)
{
	QueuedAlarm *qa=NULL;
	GSList *l;

	d(printf("%s:%d (remove_queued_alarm) \n",__FILE__, __LINE__));

	for (l = cqa->queued_alarms; l; l = l->next) {
		qa = l->data;
		if (qa->alarm_id == alarm_id)
			break;
	}

	if (!l)
		return;

	cqa->queued_alarms = g_slist_delete_link (cqa->queued_alarms, l);

	if (remove_alarm) {
		cqa->expecting_update = TRUE;
		e_cal_discard_alarm (cqa->parent_client->client, cqa->alarms->comp,
				     qa->instance->auid, NULL);
		cqa->expecting_update = FALSE;
	}

	g_free (qa);

	/* If this was the last queued alarm for this component, remove the
	 * component itself.
	 */

	if (cqa->queued_alarms != NULL)
		return;

	d(printf("%s:%d (remove_queued_alarm)) - Last Component. Removing CQA- Free=%d\n",__FILE__, __LINE__, free_object));
	if (free_object) {
		cqa->id = NULL;
		cqa->parent_client = NULL;
		e_cal_component_alarms_free (cqa->alarms);
		g_free (cqa);
	} else {
		e_cal_component_alarms_free (cqa->alarms);
		cqa->alarms = NULL;
	}
}

/* Callback used when an alarm triggers */
static void
alarm_trigger_cb (gpointer alarm_id, time_t trigger, gpointer data)
{
	CompQueuedAlarms *cqa;
	ECalComponent *comp;
	QueuedAlarm *qa;
	ECalComponentAlarm *alarm;
	ECalComponentAlarmAction action;

	cqa = data;
	comp = cqa->alarms->comp;

	config_data_set_last_notification_time (trigger);
	d(printf("%s:%d (alarm_trigger_cb) - Setting Last notification time to %s\n",__FILE__, __LINE__, ctime (&trigger)));
	saved_notification_time = trigger;

	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;

	/* Decide what to do based on the alarm action.  We use the trigger that
	 * is passed to us instead of the one from the instance structure
	 * because this may be a snoozed alarm instead of an original
	 * occurrence.
	 */

	alarm = e_cal_component_get_alarm (comp, qa->instance->auid);
	g_assert (alarm != NULL);

	/* Show it independent of what the notification is?*/
	display_notification (trigger, cqa, alarm_id, TRUE);

	e_cal_component_alarm_get_action (alarm, &action);
	e_cal_component_alarm_free (alarm);

	switch (action) {
	case E_CAL_COMPONENT_ALARM_AUDIO:
		audio_notification (trigger, cqa, alarm_id);
		break;

	case E_CAL_COMPONENT_ALARM_DISPLAY:
#ifdef HAVE_LIBNOTIFY
		popup_notification (trigger, cqa, alarm_id, TRUE);
#endif
		break;

	case E_CAL_COMPONENT_ALARM_EMAIL:
		mail_notification (trigger, cqa, alarm_id);
		break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE:
		procedure_notification (trigger, cqa, alarm_id);
		break;

	default:
		g_assert_not_reached ();
		break;
	}
	d(printf("%s:%d (alarm_trigger_cb) - Notification sent:%d\n",__FILE__, __LINE__, action));
}

/* Adds the alarms in a ECalComponentAlarms structure to the alarms queued for a
 * particular client.  Also puts the triggers in the alarm timer queue.
 */
static void
add_component_alarms (ClientAlarms *ca, ECalComponentAlarms *alarms)
{
	ECalComponentId *id;
	CompQueuedAlarms *cqa;
	GSList *l;

	/* No alarms? */
	if (alarms == NULL || alarms->alarms == NULL) {
		d(printf("%s:%d (add_component_alarms) - No alarms to add\n",__FILE__, __LINE__));
		if (alarms)
			e_cal_component_alarms_free (alarms);
		return;
	}

	cqa = g_new (CompQueuedAlarms, 1);
	cqa->parent_client = ca;
	cqa->alarms = alarms;
	cqa->expecting_update = FALSE;

	cqa->queued_alarms = NULL;
	d(printf("%s:%d (add_component_alarms)) - Creating CQA %d\n",__FILE__, __LINE__, cqa));
	
	for (l = alarms->alarms; l; l = l->next) {
		ECalComponentAlarmInstance *instance;
		gpointer alarm_id;
		QueuedAlarm *qa;
		time_t tnow = time(NULL);
		
		instance = l->data;

		alarm_id = alarm_add (instance->trigger, alarm_trigger_cb, cqa, NULL);
		if (!alarm_id) {
			d(printf("%s:%d (add_component_alarms)) - Could not schedule a trigger for %s. Discarding \n",__FILE__, __LINE__, ctime(&(instance->trigger))));
			continue;
		}

		qa = g_new (QueuedAlarm, 1);
		qa->alarm_id = alarm_id;
		qa->instance = instance;
		qa->orig_trigger = instance->trigger;
		qa->snooze = FALSE;

		cqa->queued_alarms = g_slist_prepend (cqa->queued_alarms, qa);
		d(printf("%s:%d (add_component_alarms)) - Adding alarm %d(%d)at %s (%s)\n",__FILE__, __LINE__, qa, alarm_id, ctime (&(instance->trigger)), ctime(&tnow)));		
	}

	id = e_cal_component_get_id (alarms->comp);

	/* If we failed to add all the alarms, then we should get rid of the cqa */
	if (cqa->queued_alarms == NULL) {
		e_cal_component_alarms_free (cqa->alarms);
		cqa->alarms = NULL;
		d(printf("%s:%d (add_component_alarms)) - Failed to add all : %d\n",__FILE__, __LINE__, cqa));
		g_message ("Failed to add all\n");
		g_free (cqa);
		return;
	}

	cqa->queued_alarms = g_slist_reverse (cqa->queued_alarms);
	cqa->id = id;
	d(printf("%s:%d (add_component_alarms)) - Alarm added for %s\n",__FILE__, __LINE__, id->uid));
	g_hash_table_insert (ca->uid_alarms_hash, cqa->id, cqa);
}

/* Loads the alarms of a client for a given range of time */
static void
load_alarms (ClientAlarms *ca, time_t start, time_t end)
{
	char *str_query, *iso_start, *iso_end;

	d(printf("%s:%d (load_alarms) \n",__FILE__, __LINE__));
	
	iso_start = isodate_from_time_t (start);
	if (!iso_start)
		return;
                                                                               
	iso_end = isodate_from_time_t (end);
	if (!iso_end) {
		g_free (iso_start);
		return;
	}

	str_query = g_strdup_printf ("(has-alarms-in-range? (make-time \"%s\") (make-time \"%s\"))",
				     iso_start, iso_end);
	g_free (iso_start);
	g_free (iso_end);

	/* create the live query */
	if (ca->query) {
		d(printf("%s:%d (load_alarms) - Disconnecting old queries \n",__FILE__, __LINE__));		
		g_signal_handlers_disconnect_matched (ca->query, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, ca);
		g_object_unref (ca->query);
		ca->query = NULL;
	}

	/* FIXME: handle errors */
	if (!e_cal_get_query (ca->client, str_query, &ca->query, NULL)) {
		g_warning (G_STRLOC ": Could not get query for client");
	} else {
		d(printf("%s:%d (load_alarms) - Setting Call backs \n",__FILE__, __LINE__));
		
		g_signal_connect (G_OBJECT (ca->query), "objects_added",
				  G_CALLBACK (query_objects_changed_cb), ca);
		g_signal_connect (G_OBJECT (ca->query), "objects_modified",
				  G_CALLBACK (query_objects_changed_cb), ca);
		g_signal_connect (G_OBJECT (ca->query), "objects_removed",
				  G_CALLBACK (query_objects_removed_cb), ca);

		e_cal_view_start (ca->query);
	}

	g_free (str_query);
}

/* Loads today's remaining alarms for a client */
static void
load_alarms_for_today (ClientAlarms *ca)
{
	time_t now, from, day_end, day_start;
	icaltimezone *zone;

	now = time (NULL);
	zone = config_data_get_timezone ();
	day_start = time_day_begin_with_zone (now, zone);

	/* Make sure we don't miss some events from the last notification.
	 * We add 1 to the saved_notification_time to make the time ranges
	 * half-open; we do not want to display the "last" displayed alarm
	 * twice, once when it occurs and once when the alarm daemon restarts.
	 */
	from = MAX (config_data_get_last_notification_time () + 1, day_start);

	day_end = time_day_end_with_zone (now, zone);
	d(printf("%s:%d (load_alarms_for_today) - From %s to %s\n",__FILE__, __LINE__, ctime (&from), ctime(&day_end)));	
	load_alarms (ca, from, day_end);
}

/* Called when a calendar client finished loading; we load its alarms */
static void
cal_opened_cb (ECal *client, ECalendarStatus status, gpointer data)
{
	ClientAlarms *ca;

	ca = data;

	d(printf("%s:%d (cal_opened_cb)) - Opened Calendar %d (Status %d)\n",__FILE__, __LINE__, client, status==E_CALENDAR_STATUS_OK));
	if (status != E_CALENDAR_STATUS_OK)
		return;

	load_alarms_for_today (ca);
}

/* Looks up a component's queued alarm structure in a client alarms structure */
static CompQueuedAlarms *
lookup_comp_queued_alarms (ClientAlarms *ca, const ECalComponentId *id)
{
	return g_hash_table_lookup (ca->uid_alarms_hash, id);
}

static void
remove_alarms (CompQueuedAlarms *cqa, gboolean free_object)
{
	GSList *l;

	d(printf("%s:%d (remove_alarms) - Removing for %d\n",__FILE__, __LINE__, cqa));
	for (l = cqa->queued_alarms; l;) {
		QueuedAlarm *qa;

		qa = l->data;

		/* Get the next element here because the list element will go
		 * away in remove_queued_alarm().  The qa will be freed there as
		 * well.
		 */
		l = l->next;

		alarm_remove (qa->alarm_id);
		remove_queued_alarm (cqa, qa->alarm_id, free_object, FALSE);
	}

}

/* Removes a component an its alarms */
static void
remove_comp (ClientAlarms *ca, ECalComponentId *id)
{
	CompQueuedAlarms *cqa;

	d(printf("%s:%d (remove_comp) - Removing uid %s\n",__FILE__, __LINE__, id->uid));

	if (id->rid && !(*(id->rid))) {
		g_free (id->rid);
		id->rid = NULL;
	}
	
	cqa = lookup_comp_queued_alarms (ca, id);
	if (!cqa) 
		return;

	/* If a component is present, then it means we must have alarms queued
	 * for it.
	 */
	g_assert (cqa->queued_alarms != NULL);
	
	d(printf("%s:%d (remove_comp) - Removing CQA %d\n",__FILE__, __LINE__, cqa));
	remove_alarms (cqa, TRUE);
}

/* Called when a calendar component changes; we must reload its corresponding
 * alarms.
 */
struct _query_msg {
	ECal *client;
	GList *objects;
	gpointer data;
};

static GList * 
duplicate_ical (GList *in_list)
{
	GList *l, *out_list = NULL;
	for (l = in_list; l; l = l->next) {
		out_list = g_list_prepend (out_list, icalcomponent_new_clone (l->data));
	}
	
	return g_list_reverse (out_list);
}

static GList * 
duplicate_ecal (GList *in_list)
{
	GList *l, *out_list = NULL;
	for (l = in_list; l; l = l->next) {
		ECalComponentId *id, *old;
		old = l->data;
		id = g_new0 (ECalComponentId, 1);
		id->uid = g_strdup (old->uid);
		id->rid = g_strdup (old->rid);
		out_list = g_list_prepend (out_list, id);
	}
	
	return g_list_reverse (out_list);
}

static void
query_objects_changed_async (EThread *e, AlarmMsg *msg, void *data)
{
	ClientAlarms *ca;
	time_t from, day_end;
	ECalComponentAlarms *alarms;
	gboolean found;
	icaltimezone *zone;
	CompQueuedAlarms *cqa;
	GList *l;
	struct _query_msg *list = msg->data;
	ECal *client;
	GList *objects;
		
	client = list->client;
	ca = list->data;
	objects = list->objects;
	
	from = config_data_get_last_notification_time ();
	if (from == -1)
		from = time (NULL);
	else
		from += 1; /* we add 1 to make sure the alarm is not displayed twice */

	zone = config_data_get_timezone ();

	day_end = time_day_end_with_zone (time (NULL), zone);
	
	d(printf("%s:%d (query_objects_changed_async) - Querying for object between %s to %s\n",__FILE__, __LINE__, ctime(&from), ctime(&day_end)));
	
	for (l = objects; l != NULL; l = l->next) {
		ECalComponentId *id;
		GSList *sl;
		ECalComponent *comp = e_cal_component_new ();

		e_cal_component_set_icalcomponent (comp, l->data);

		id = e_cal_component_get_id (comp);
		found = e_cal_get_alarms_for_object (ca->client, id, from, day_end, &alarms);

		if (!found) {
			d(printf("%s:%d (query_objects_changed_async) - No Alarm found for client %d\n",__FILE__, __LINE__, ca->client));
			tray_list_remove_cqa (lookup_comp_queued_alarms (ca, id));
			remove_comp (ca, id);
			g_hash_table_remove (ca->uid_alarms_hash, id);
			e_cal_component_free_id (id);
			g_object_unref (comp);
			comp = NULL;
			continue;
		}

		cqa = lookup_comp_queued_alarms (ca, id);
		if (!cqa) {
			d(printf("%s:%d (query_objects_changed_async) - No currently queued alarms for %s\n",__FILE__, __LINE__, id->uid));
			add_component_alarms (ca, alarms);
			g_object_unref (comp);
			comp = NULL;
			continue;
		}

		d(printf("%s:%d (query_objects_changed_async) - Alarm Already Exist for %s\n",__FILE__, __LINE__, id->uid));
		/* if the alarms or the alarms list is empty remove it after updating the cqa structure */
		if (alarms == NULL || alarms->alarms == NULL) {
		
			/* update the cqa and its queued alarms for changes in summary and alarm_uid  */
			update_cqa (cqa, comp);

			if (alarms)
				e_cal_component_alarms_free (alarms);
			continue;
		}

		/* if already in the list, just update it */
		remove_alarms (cqa, FALSE);
		cqa->alarms = alarms;
		cqa->queued_alarms = NULL;
		
		/* add the new alarms */
		for (sl = cqa->alarms->alarms; sl; sl = sl->next) {
			ECalComponentAlarmInstance *instance;
			gpointer alarm_id;
			QueuedAlarm *qa;

			instance = sl->data;

			alarm_id = alarm_add (instance->trigger, alarm_trigger_cb, cqa, NULL);
			if (!alarm_id) {
				d(printf("%s:%d (query_objects_changed_async) -Unable to schedule trigger for %s \n",__FILE__, __LINE__, ctime(&(instance->trigger)))); 
				continue;
			}

			qa = g_new (QueuedAlarm, 1);
			qa->alarm_id = alarm_id;
			qa->instance = instance;
			qa->snooze = FALSE;
			qa->orig_trigger = instance->trigger;	
			cqa->queued_alarms = g_slist_prepend (cqa->queued_alarms, qa);
			d(printf("%s:%d (query_objects_changed_async) - Adding %d to queue \n",__FILE__, __LINE__, qa));
		}
		
		cqa->queued_alarms = g_slist_reverse (cqa->queued_alarms);
		g_object_unref (comp);
		comp = NULL;
	}
	g_list_free (objects);
}

static void
query_objects_changed_cb (ECal *client, GList *objects, gpointer data)
{
	AlarmMsg *msg;
	struct _query_msg *list;

	/* These two structures will be freed by the msg destroy function*/
	msg = malloc (sizeof (AlarmMsg));
	msg->receive_msg = query_objects_changed_async;
	list = malloc (sizeof (struct _query_msg));
	list->client = client;
	list->objects = duplicate_ical (objects);
	list->data = data;
	msg->data = list;

	d(printf("%s:%d (query_objects_changed_cb) - Posting a task\n",__FILE__, __LINE__));	
	e_thread_put(alarm_operation_thread, (EMsg *)msg);

}

/* Called when a calendar component is removed; we must delete its corresponding
 * alarms.
 */
static void
query_objects_removed_async (EThread *e, AlarmMsg *msg, void *data)
{
	ClientAlarms *ca;
	GList *l;
	struct _query_msg *list = msg->data;
	ECal *client;
	GList *objects;
		
	client = list->client;
	ca = list->data;
	objects = list->objects;
	
	d(printf("%s:%d (query_objects_removed_async) - Removing %d objects\n",__FILE__, __LINE__, g_list_length(objects)));
	
	for (l = objects; l != NULL; l = l->next) { 
		/* If the alarm is already triggered remove it. */
		tray_list_remove_cqa (lookup_comp_queued_alarms (ca, l->data));
		remove_comp (ca, l->data);
		g_hash_table_remove (ca->uid_alarms_hash, l->data);		
		e_cal_component_free_id (l->data);
	}

	g_list_free (objects);
}

static void
query_objects_removed_cb (ECal *client, GList *objects, gpointer data)
{
	AlarmMsg *msg;
	struct _query_msg *list;

	/* These two structures will be freed by the msg destroy function*/
	msg = malloc (sizeof (AlarmMsg));
	msg->receive_msg = query_objects_removed_async;

	list = malloc (sizeof (struct _query_msg));
	list->client = client;
	list->objects = duplicate_ecal (objects);
	list->data = data;	
	msg->data = list;

	d(printf("%s:%d (query_objects_removed_cb) - Posting a task\n",__FILE__, __LINE__));	
	e_thread_put(alarm_operation_thread, (EMsg *)msg);

}


/* Notification functions */

/* Creates a snooze alarm based on an existing one.  The snooze offset is
 * compued with respect to the current time.
 */
static void
create_snooze (CompQueuedAlarms *cqa, gpointer alarm_id, int snooze_mins)
{
	QueuedAlarm *orig_qa;
	time_t t;
	gpointer new_id;

	orig_qa = lookup_queued_alarm (cqa, alarm_id);
	if (!orig_qa)
		return;

	t = time (NULL);
	t += snooze_mins * 60;

	new_id = alarm_add (t, alarm_trigger_cb, cqa, NULL);
	if (!new_id) {
		d(printf("%s:%d (create_snooze) -Unable to schedule trigger for %s \n",__FILE__, __LINE__, ctime(&t)));
		return;
	}

	orig_qa->instance->trigger = t;
	orig_qa->alarm_id = new_id;
	orig_qa->snooze = TRUE;
	d(printf("%s:%d (create_snooze) - Adding a alarm at %s\n",__FILE__, __LINE__, ctime(&t)));
}

/* Launches a component editor for a component */
static void
edit_component (ECal *client, ECalComponent *comp)
{
	const char *uid;
	const char *uri;
	ECalSourceType source_type;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CompEditorFactory factory;
	GNOME_Evolution_Calendar_CompEditorFactory_CompEditorMode corba_type;

	d(printf("%s:%d (edit_component) - Client %d\n",__FILE__, __LINE__, client));
	
	e_cal_component_get_uid (comp, &uid);

	uri = e_cal_get_uri (client);
	source_type = e_cal_get_source_type (client);

	/* Get the factory */
	CORBA_exception_init (&ev);
	factory = bonobo_activation_activate_from_id ("OAFIID:GNOME_Evolution_Calendar_CompEditorFactory:" BASE_VERSION,
						      0, NULL, &ev);

	if (BONOBO_EX (&ev)) {
		e_error_run (NULL, "editor-error", bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	/* Edit the component */
	switch (source_type) {
	case E_CAL_SOURCE_TYPE_TODO:
		corba_type = GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_TODO;
		break;
	default:
		corba_type = GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_EVENT;		
	}
	
	GNOME_Evolution_Calendar_CompEditorFactory_editExisting (factory, uri, (char *) uid, corba_type, &ev);

	if (BONOBO_EX (&ev))
		e_error_run (NULL, "editor-error", bonobo_exception_get_text (&ev));

	CORBA_exception_free (&ev);

	/* Get rid of the factory */
	bonobo_object_release_unref (factory, NULL);
}

typedef struct {
	char *summary;
	char *description;
	char *location;
	gboolean blink_state;
	gboolean snooze_set;
	gint blink_id;
	time_t trigger;
	CompQueuedAlarms *cqa;
	gpointer alarm_id;
	ECalComponent *comp;
	ECal *client;
	ECalView *query;
#ifndef USE_GTK_STATUS_ICON
	GtkWidget *tray_icon;
	GtkWidget *image;
#else
        GtkStatusIcon *tray_icon;
	GdkPixbuf *image;
#endif
	GtkTreeIter iter;
} TrayIconData;

static void
free_tray_icon_data (TrayIconData *tray_data)
{
	g_return_if_fail (tray_data != NULL);

	if (tray_data->summary){
		g_free (tray_data->summary);
		tray_data->summary = NULL;
	}

	if (tray_data->description){
		g_free (tray_data->description);
		tray_data->description = NULL;
	}

	if (tray_data->location){
		g_free (tray_data->description);
		tray_data->location = NULL;
	}

	g_object_unref (tray_data->client);
	tray_data->client = NULL;

	g_signal_handlers_disconnect_matched (tray_data->query, G_SIGNAL_MATCH_FUNC,
					      0, 0, NULL, on_dialog_objs_removed_cb, NULL);
	g_object_unref (tray_data->query);
	tray_data->query = NULL;

	g_object_unref (tray_data->comp);
	tray_data->comp = NULL;
	
	tray_data->cqa = NULL;
	tray_data->alarm_id = NULL;
	tray_data->tray_icon = NULL;
	tray_data->image = NULL;

	g_free (tray_data);
}

static void
on_dialog_objs_removed_async (EThread *e, AlarmMsg *msg, void *data)
{
	const char *our_uid;
	GList *l;
	TrayIconData *tray_data;
	struct _query_msg *list = msg->data;
	ECal *client;
	GList *objects;

	d(printf("%s:%d (on_dialog_objs_removed_async)\n",__FILE__, __LINE__));
	
	client = list->client;
	tray_data = list->data;
	objects = list->objects;
	
	e_cal_component_get_uid (tray_data->comp, &our_uid);
	g_return_if_fail (our_uid && *our_uid);

	for (l = objects; l != NULL; l = l->next) {
		const char *uid = l->data;

		if (!uid)
			continue;

		if (!strcmp (uid, our_uid)) {
			tray_data->cqa = NULL;
			tray_data->alarm_id = NULL;
			tray_icons_list = g_list_remove (tray_icons_list, tray_data);
			tray_data = NULL;
		}
	}
}

static void
on_dialog_objs_removed_cb (ECal *client, GList *objects, gpointer data)
{
	AlarmMsg *msg;
	struct _query_msg *list;

	/* These two structures will be freed by the msg destroy function*/
	msg = malloc (sizeof (AlarmMsg));
	msg->receive_msg = on_dialog_objs_removed_async;

	list = malloc (sizeof (struct _query_msg));
	list->client = client;
	list->objects = objects;
	list->data = data;
	msg->data = list;

	d(printf("%s:%d (on_dialog_objs_removed_cb) - Posting a task \n",__FILE__, __LINE__));
	e_thread_put(alarm_operation_thread, (EMsg *)msg);	
}

struct _tray_cqa_msg {
	CompQueuedAlarms *cqa;
};

static void
tray_list_remove_cqa_async(EThread *e, AlarmMsg *msg, void *data)
{
	struct _tray_cqa_msg *tmsg = msg->data;
	CompQueuedAlarms *cqa = tmsg->cqa;
	GList *list = tray_icons_list;

	d(printf("%s:%d (tray_list_remove_cqa_async) - Removing CQA %d from tray list\n",__FILE__, __LINE__, cqa));
	
	while (list) {
		TrayIconData *tray_data = list->data;
		GList *tmp = list;
		GtkTreeIter iter;
		GtkTreeModel *model;
		
		list = list->next;
		if (tray_data->cqa == cqa) {
			d(printf("%s:%d (tray_list_remove_cqa_async) - Found.\n", __FILE__, __LINE__));
			tray_icons_list = g_list_delete_link (tray_icons_list, tmp);
			if (alarm_notifications_dialog) {
				model = gtk_tree_view_get_model (GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
				gtk_list_store_remove (GTK_LIST_STORE (model), &(tray_data->iter));
			}
			free_tray_icon_data (tray_data);
		}
	}
	
	d(printf("%s:%d (tray_list_remove_cqa_async) - %d alarms left.\n", __FILE__, __LINE__, g_list_length (tray_icons_list)));
	
	if (alarm_notifications_dialog) {
		if (!g_list_length (tray_icons_list)) {
			gtk_widget_destroy (alarm_notifications_dialog->dialog);
			g_free (alarm_notifications_dialog);
			alarm_notifications_dialog = NULL;
		} else {
			GtkTreeIter iter;
			GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
			gboolean valid = gtk_tree_model_get_iter_first (model, &iter);		
			GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
			gtk_tree_selection_select_iter (sel, &iter);
		}
	}
}

static void
tray_list_remove_cqa (CompQueuedAlarms *cqa)
{
	AlarmMsg *msg;
	struct _tray_cqa_msg *list;
	/* These two structures will be freed by the msg destroy function*/
	msg = malloc (sizeof (AlarmMsg));
	msg->receive_msg = tray_list_remove_cqa_async;

	list = malloc (sizeof (struct _tray_cqa_msg));
	list->cqa = cqa;
	msg->data = list;

	d(printf("%s:%d (tray_list_remove_cqa) - Posting a task\n",__FILE__, __LINE__));
	e_thread_put(alarm_operation_thread, (EMsg *)msg);
}

/* Callback used from the alarm notify dialog */
static void
tray_list_remove_async(EThread *e, AlarmMsg *msg, void *data)
{
	GList *list = tray_icons_list;

	d(printf("%s:%d (tray_list_remove_async) - Removing %d alarms\n",__FILE__, __LINE__, g_list_length(list)));
	while (list != NULL) {

		TrayIconData *tray_data = list->data;
	
		if (!tray_data->snooze_set){
			GList *temp = list->next;
			tray_icons_list = g_list_remove_link (tray_icons_list, list);
			remove_queued_alarm (tray_data->cqa, tray_data->alarm_id, FALSE, TRUE);
			g_hash_table_remove (tray_data->cqa->parent_client->uid_alarms_hash, tray_data->cqa->id);
			e_cal_component_free_id (tray_data->cqa->id);
			g_free (tray_data->cqa);			
			free_tray_icon_data (tray_data);
			tray_data = NULL;
			g_list_free_1 (list);
			if (tray_icons_list != list)	/* List head is modified */
				list = tray_icons_list;
			else 
				list = temp;
		} else
			list = list->next;
	}	
}

static void
tray_list_remove_icons ()
{
	AlarmMsg *msg;

	/* These two structures will be freed by the msg destroy function*/
	msg = malloc (sizeof (AlarmMsg));
	msg->receive_msg = tray_list_remove_async;

	msg->data = NULL;

	d(printf("%s:%d (tray_list_remove_icons) - Posting a task\n",__FILE__, __LINE__));
	e_thread_put(alarm_operation_thread, (EMsg *)msg);
}

struct _tray_msg {
	TrayIconData *data;
};

static void
tray_list_remove_data_async(EThread *e, AlarmMsg *msg, void *data)
{
	struct _tray_msg *tmsg = msg->data;
	TrayIconData *tray_data = tmsg->data;

	d(printf("%s:%d (tray_list_remove_data_async) - Removing %d from tray list\n",__FILE__, __LINE__, tray_data));
	
	tray_icons_list = g_list_remove_all (tray_icons_list, tray_data);
	free_tray_icon_data (tray_data);
	tray_data = NULL;
}

static void
tray_list_remove_data (TrayIconData *data)
{
	AlarmMsg *msg;
	struct _tray_msg *list;
	/* These two structures will be freed by the msg destroy function*/
	msg = malloc (sizeof (AlarmMsg));
	msg->receive_msg = tray_list_remove_data_async;

	list = malloc (sizeof (struct _tray_msg));
	list->data = data;
	msg->data = list;

	d(printf("%s:%d (tray_list_remove_data) - Posting a task\n",__FILE__, __LINE__));
	e_thread_put(alarm_operation_thread, (EMsg *)msg);
}

static void
notify_dialog_cb (AlarmNotifyResult result, int snooze_mins, gpointer data)
{
	TrayIconData *tray_data = data;

	d(printf("%s:%d (notify_dialog_cb) - Received from dialog\n",__FILE__, __LINE__));
	
	g_signal_handlers_disconnect_matched (tray_data->query, G_SIGNAL_MATCH_FUNC,
					      0, 0, NULL, on_dialog_objs_removed_cb, NULL);

	switch (result) {
	case ALARM_NOTIFY_SNOOZE:
		d(printf("%s:%d (notify_dialog_cb) - Creating a snooze\n",__FILE__, __LINE__)); 
		create_snooze (tray_data->cqa, tray_data->alarm_id, snooze_mins);
		tray_data->snooze_set = TRUE;
		tray_list_remove_data (tray_data);
		if (alarm_notifications_dialog) {
			GtkTreeSelection *selection = 
				gtk_tree_view_get_selection (
					GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
			GtkTreeIter iter;
			GtkTreeModel *model = NULL;
			
			/* We can` also use tray_data->iter */
			if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				if (!gtk_tree_model_get_iter_first (model, &iter)) {
					/* We removed the last one */
					gtk_widget_destroy (alarm_notifications_dialog->dialog);
					g_free (alarm_notifications_dialog);
					alarm_notifications_dialog = NULL;
				} else {
					/* Select the first */
					gtk_tree_selection_select_iter (selection, &iter);
				}
			}
			
		}
		
		break;

	case ALARM_NOTIFY_EDIT:
		edit_component (tray_data->client, tray_data->comp);

		break;

	case ALARM_NOTIFY_CLOSE:
		d(printf("%s:%d (notify_dialog_cb) - Dialog close\n",__FILE__, __LINE__));
		if (alarm_notifications_dialog) {
			GList *list;
			GtkTreeIter iter;
			GtkTreeModel *model = 
				gtk_tree_view_get_model (
					GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
			gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
		
			/* Maybe we should warn about this first? */			
			while (valid) {
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				valid = gtk_tree_model_iter_next (model, &iter);
			}
			
			gtk_widget_destroy (alarm_notifications_dialog->dialog);
			g_free (alarm_notifications_dialog);
			alarm_notifications_dialog = NULL;
		
			/* Task to remove the tray icons */
			tray_list_remove_icons ();
	}

		break;

	default:
		g_assert_not_reached ();
	}

	return;
}

/* Callbacks.  */
static gboolean
open_alarm_dialog (TrayIconData *tray_data)
{
	QueuedAlarm *qa;

	d(printf("%s:%d (open_alarm_dialog) \n",__FILE__, __LINE__));
	qa = lookup_queued_alarm (tray_data->cqa, tray_data->alarm_id);
	if (qa) {

		if (tray_blink_id > -1)
			g_source_remove (tray_blink_id);
		tray_blink_id = -1;
		
		gtk_widget_destroy (tray_icon);
		tray_icon = NULL;
#ifndef USE_GTK_STATUS_ICON
		tray_image = NULL;
#endif
		if (!alarm_notifications_dialog)
			alarm_notifications_dialog = notified_alarms_dialog_new ();
		
		if (alarm_notifications_dialog) {

			GtkTreeSelection *selection = NULL;
			
			selection = gtk_tree_view_get_selection (
				GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
		
			tray_data->iter = add_alarm_to_notified_alarms_dialog (
								   alarm_notifications_dialog,
								   tray_data->trigger,
							       qa->instance->occur_start,
							       qa->instance->occur_end,
							       e_cal_component_get_vtype (tray_data->comp),
							       tray_data->summary,
							       tray_data->description,
							       tray_data->location,
							       notify_dialog_cb, tray_data);
		
			gtk_tree_selection_select_iter (selection, &tray_data->iter);

		}
		
	}

	return TRUE;
}

static gint
tray_icon_clicked_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	TrayIconData *tray_data = user_data;

	if (event->type == GDK_BUTTON_PRESS) {
			d(printf("%s:%d (tray_icon_clicked_cb) - left click and %d alarms\n",__FILE__, __LINE__, g_list_length (tray_icons_list))); 		 
		if (event->button == 1 && g_list_length (tray_icons_list) > 0) {
			GList *tmp;
			for (tmp = tray_icons_list; tmp; tmp = tmp->next) {
				open_alarm_dialog (tmp->data);
			}
			
			return TRUE;
		} else if (event->button == 3) {
			d(printf("%s:%d (tray_icon_clicked_cb) - right click\n",__FILE__, __LINE__)); 
			if (tray_blink_id > -1)
				g_source_remove (tray_blink_id);
			tray_blink_id = -1;
			
			gtk_widget_destroy (tray_icon);
			tray_icon = NULL;
#ifndef USE_GTK_STATUS_ICON
			tray_image = NULL;
#endif
			return TRUE;
		}
	}

	return FALSE;
}

#ifdef USE_GTK_STATUS_ICON
static void
icon_activated (GtkStatusIcon *icon)
{
  GdkEventButton event;

  event.type = GDK_BUTTON_PRESS;
  event.button = 1;
  event.time = gtk_get_current_event_time ();

  tray_icon_clicked_cb (NULL, &event, NULL);
}
#endif

static gboolean
tray_icon_blink_cb (gpointer data)
{
	GdkPixbuf *pixbuf;

	tray_blink_state = tray_blink_state == TRUE ? FALSE: TRUE;
	
	pixbuf = e_icon_factory_get_icon  (tray_blink_state == TRUE?
					   "stock_appointment-reminder-excl" :
					   "stock_appointment-reminder",
					   E_ICON_SIZE_LARGE_TOOLBAR);

#ifndef USE_GTK_STATUS_ICON
	if (tray_icon && tray_image)
		gtk_image_set_from_pixbuf (GTK_IMAGE (tray_image), pixbuf);
#else
	if (tray_icon)
		gtk_status_icon_set_from_pixbuf (tray_icon, pixbuf);
#endif
	g_object_unref (pixbuf);

	return TRUE;
}


/* Add a new data to tray list */

static void
tray_list_add_async (EThread *e, AlarmMsg *msg, void *data)
{
	struct _tray_msg *list = msg->data;
	d(printf("%s:%d (tray_list_add_async) - Add %d\n",__FILE__, __LINE__, list->data));	
	tray_icons_list = g_list_prepend (tray_icons_list, list->data);
}

static void
tray_list_add_new (TrayIconData *data)
{
	AlarmMsg *msg;
	struct _tray_msg *list;

	/* These two structures will be freed by the msg destroy function*/
	msg = malloc (sizeof (AlarmMsg));
	msg->receive_msg = tray_list_add_async;

	list = malloc (sizeof (struct _tray_msg));
	list->data = data;
	msg->data = list;
	
	d(printf("%s:%d (tray_list_add_new) - Posting a task\n",__FILE__, __LINE__));	
	e_thread_put(alarm_operation_thread, (EMsg *)msg);
}

/* Performs notification of a display alarm */
static void
display_notification (time_t trigger, CompQueuedAlarms *cqa,
		      gpointer alarm_id, gboolean use_description)
{
	QueuedAlarm *qa;
	ECalComponent *comp;
	const char *summary, *description, *location;
#ifndef USE_GTK_STATUS_ICON
	GtkWidget *image=NULL;
	GtkTooltips *tooltips;
#endif
	TrayIconData *tray_data;
	ECalComponentText text;
	GSList *text_list;
	char *str, *start_str, *end_str, *alarm_str, *time_str;
	icaltimezone *current_zone;
	ECalComponentOrganizer organiser;

	d(printf("%s:%d (display_notification)\n",__FILE__, __LINE__));
	
	comp = cqa->alarms->comp;
	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;
	
	/* get a sensible description for the event */
	e_cal_component_get_summary (comp, &text);
	e_cal_component_get_organizer (comp, &organiser); 

	if (text.value)
		summary = text.value;
	else
	        summary = _("No summary available.");

	e_cal_component_get_description_list (comp, &text_list);

	if (text_list) {
		text = *((ECalComponentText *)text_list->data);
		if (text.value)
			description = text.value;
		else
			description = _("No description available.");
	} else {
		description = _("No description available.");
	}

	e_cal_component_free_text_list (text_list);

	e_cal_component_get_location (comp, &location);

	if (!location)
	        location = _("No location information available.");

	/* create the tray icon */
#ifndef USE_GTK_STATUS_ICON
	if (tray_icon == NULL) {
		tray_icon = GTK_WIDGET (egg_tray_icon_new ("Evolution Alarm"));
		tray_image = e_icon_factory_get_image  ("stock_appointment-reminder", E_ICON_SIZE_LARGE_TOOLBAR);
		tray_event_box = gtk_event_box_new ();
		gtk_container_add (GTK_CONTAINER (tray_event_box), tray_image);
		gtk_container_add (GTK_CONTAINER (tray_icon), tray_event_box);
		g_signal_connect (G_OBJECT (tray_event_box), "button_press_event",
				  G_CALLBACK (tray_icon_clicked_cb), NULL);
		gtk_widget_show_all (tray_icon);
	}
	tooltips = gtk_tooltips_new ();
	
#else
	if (tray_icon == NULL) {
		tray_icon = gtk_status_icon_new ();
		gtk_status_icon_set_from_pixbuf (tray_icon, e_icon_factory_get_icon ("stock_appointment-reminder", E_ICON_SIZE_LARGE_TOOLBAR));
		g_signal_connect (G_OBJECT (tray_icon), "activate",
				  G_CALLBACK (icon_activated), NULL);
	}
#endif
	
	current_zone = config_data_get_timezone ();
	alarm_str = timet_to_str_with_zone (trigger, current_zone);
	start_str = timet_to_str_with_zone (qa->instance->occur_start, current_zone);
	end_str = timet_to_str_with_zone (qa->instance->occur_end, current_zone);
	time_str = calculate_time (qa->instance->occur_start, qa->instance->occur_end);

	str = g_strdup_printf ("%s\n%s %s",
			       summary, start_str, time_str);

	/* create the private structure */
	tray_data = g_new0 (TrayIconData, 1);
	tray_data->summary = g_strdup (summary);
	tray_data->description = g_strdup (description);
	tray_data->location = g_strdup (location);
	tray_data->trigger = trigger;
	tray_data->cqa = cqa;
	tray_data->alarm_id = alarm_id;
	tray_data->comp = g_object_ref (e_cal_component_clone (comp));
	tray_data->client = cqa->parent_client->client;
	tray_data->query = g_object_ref (cqa->parent_client->query);
#ifndef USE_GTK_STATUS_ICON
	tray_data->image = image;
#endif
	tray_data->blink_state = FALSE;
	tray_data->snooze_set = FALSE;
	g_object_ref (tray_data->client);
	tray_data->tray_icon = tray_icon;

	/* Task to add tray_data to the global tray_icon_list */
	tray_list_add_new (tray_data);

	if (g_list_length (tray_icons_list) > 1) {
		char *tip;

		tip =  g_strdup_printf (_("You have %d alarms"), g_list_length (tray_icons_list));
#ifndef USE_GTK_STATUS_ICON
		gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips), tray_event_box, tip, NULL);
		g_free (tip);
#else
		gtk_status_icon_set_tooltip (tray_icon, tip);
#endif
	}
	else {
#ifndef USE_GTK_STATUS_ICON
		gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips), tray_event_box, str, NULL);
#else
		gtk_status_icon_set_tooltip (tray_icon, str);
#endif
	}

	g_free (start_str);
	g_free (end_str);
	g_free (alarm_str);
	g_free (time_str);
	g_free (str);

	g_signal_connect (G_OBJECT (tray_data->query), "objects_removed",
			  G_CALLBACK (on_dialog_objs_removed_cb), tray_data);

	// FIXME: We should remove this check
	if (!config_data_get_notify_with_tray ()) {
		tray_blink_id = -1;
		open_alarm_dialog (tray_data);
		gtk_window_stick (GTK_WINDOW (alarm_notifications_dialog->dialog));
	} else {
		if (tray_blink_id == -1)
			tray_blink_id = g_timeout_add (500, tray_icon_blink_cb, tray_data);
	}
}

#ifdef HAVE_LIBNOTIFY
static void
popup_notification (time_t trigger, CompQueuedAlarms *cqa,
	            gpointer alarm_id, gboolean use_description)
{
	QueuedAlarm *qa;
	ECalComponent *comp;
	const char *summary, *location;
	GtkTooltips *tooltips;
	ECalComponentText text;
	char *str, *start_str, *end_str, *alarm_str, *time_str;
	icaltimezone *current_zone;
	ECalComponentOrganizer organiser;
	char *body;

	d(printf("%s:%d (popup_notification)\n",__FILE__, __LINE__));
	
	comp = cqa->alarms->comp;
	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;
	if (!notify_is_initted ())
		notify_init("Evolution Alarm Notify");
	
	/* get a sensible description for the event */
	e_cal_component_get_summary (comp, &text);
	e_cal_component_get_organizer (comp, &organiser); 


	if (text.value)
		summary = text.value;
	else
	        summary = _("No summary available.");

	e_cal_component_get_location (comp, &location);

	/* create the tray icon */
	tooltips = gtk_tooltips_new ();

	current_zone = config_data_get_timezone ();
	alarm_str = timet_to_str_with_zone (trigger, current_zone);
	start_str = timet_to_str_with_zone (qa->instance->occur_start, current_zone);
	end_str = timet_to_str_with_zone (qa->instance->occur_end, current_zone);
	time_str = calculate_time (qa->instance->occur_start, qa->instance->occur_end);

	str = g_strdup_printf ("%s %s",
			       start_str, time_str);

	if (organiser.cn) {
		if (location)
			body = g_strdup_printf ("<b>%s</b>\n%s %s\n%s %s", organiser.cn, _("Location:"), location, start_str, time_str);
		else
			body = g_strdup_printf ("<b>%s</b>\n%s %s", organiser.cn, start_str, time_str);
	}
	else {
		if (location)
			body = g_strdup_printf ("%s %s\n%s %s", _("Location:"), location, start_str, time_str);
		else
			body = g_strdup_printf ("%s %s", start_str, time_str);			
	}

	NotifyNotification *n = notify_notification_new (summary, body, "stock_appointment-reminder", NULL);
	if (!notify_notification_show(n, NULL))
	    g_warning ("Could not send notification to daemon\n");	

	/* create the private structure */
	g_free (start_str);
	g_free (end_str);
	g_free (alarm_str);
	g_free (time_str);
	g_free (str);


}
#endif

/* Performs notification of an audio alarm */
static void
audio_notification (time_t trigger, CompQueuedAlarms *cqa,
		    gpointer alarm_id)
{
	QueuedAlarm *qa;
	ECalComponent *comp;
	ECalComponentAlarm *alarm;
	icalattach *attach;
	int	flag = 0;

	d(printf("%s:%d (audio_notification)\n",__FILE__, __LINE__));
	
	comp = cqa->alarms->comp;
	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;

	alarm = e_cal_component_get_alarm (comp, qa->instance->auid);
	g_assert (alarm != NULL);

	e_cal_component_alarm_get_attach (alarm, &attach);
	e_cal_component_alarm_free (alarm);

	if (attach && icalattach_get_is_url (attach)) {
		const char *url;

		url = icalattach_get_url (attach);

		if (url && *url && g_file_test (url, G_FILE_TEST_EXISTS)) {
			flag = 1;
			gnome_sound_play (url); /* this sucks */
		}
	}

	if (!flag)
		gdk_beep ();

	if (attach)
		icalattach_unref (attach);

}

/* Performs notification of a mail alarm */
static void
mail_notification (time_t trigger, CompQueuedAlarms *cqa, gpointer alarm_id)
{
	GtkWidget *dialog;
	GtkWidget *label;

	/* FIXME */

	d(printf("%s:%d (mail_notification)\n",__FILE__, __LINE__));
	
	dialog = gtk_dialog_new_with_buttons (_("Warning"),
					      NULL, 0,
					      GTK_STOCK_OK, GTK_RESPONSE_CANCEL,
					      NULL);
	label = gtk_label_new (_("Evolution does not support calendar reminders with\n"
				 "email notifications yet, but this reminder was\n"
				 "configured to send an email.  Evolution will display\n"
				 "a normal reminder dialog box instead."));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, TRUE, TRUE, 4);

	gtk_dialog_run (GTK_DIALOG (dialog));
}

/* Performs notification of a procedure alarm */
static gboolean
procedure_notification_dialog (const char *cmd, const char *url) 
{
	GtkWidget *dialog, *label, *checkbox;
	char *str;
	int btn;

	d(printf("%s:%d (procedure_notification_dialog)\n",__FILE__, __LINE__));
	
	if (config_data_is_blessed_program (url))
		return TRUE;
	
	dialog = gtk_dialog_new_with_buttons (_("Warning"),
					      NULL, 0,
					      GTK_STOCK_NO, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_YES, GTK_RESPONSE_OK,
					      NULL);

	str = g_strdup_printf (_("An Evolution Calendar reminder is about to trigger. "
				 "This reminder is configured to run the following program:\n\n"
				 "        %s\n\n"
				 "Are you sure you want to run this program?"),
			       cmd);
	label = gtk_label_new (str);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    label, TRUE, TRUE, 4);
	g_free (str);

	checkbox = gtk_check_button_new_with_label
		(_("Do not ask me about this program again."));
	gtk_widget_show (checkbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), 
			    checkbox, TRUE, TRUE, 4);

	/* Run the dialog */
	btn = gtk_dialog_run (GTK_DIALOG (dialog));
	if (btn == GTK_RESPONSE_OK && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox)))
		config_data_save_blessed_program (url);
	gtk_widget_destroy (dialog);

	return (btn == GTK_RESPONSE_OK);
}

static void
procedure_notification (time_t trigger, CompQueuedAlarms *cqa, gpointer alarm_id)
{
	QueuedAlarm *qa;
	ECalComponent *comp;
	ECalComponentAlarm *alarm;
	ECalComponentText description;
	icalattach *attach;
	const char *url;
	char *cmd;
	int result;

	d(printf("%s:%d (procedure_notification)\n",__FILE__, __LINE__));
	
	comp = cqa->alarms->comp;
	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;

	alarm = e_cal_component_get_alarm (comp, qa->instance->auid);
	g_assert (alarm != NULL);

	e_cal_component_alarm_get_attach (alarm, &attach);
	e_cal_component_alarm_get_description (alarm, &description);
	e_cal_component_alarm_free (alarm);

	/* If the alarm has no attachment, simply display a notification dialog. */
	if (!attach)
		goto fallback;

	if (!icalattach_get_is_url (attach)) {
		icalattach_unref (attach);
		goto fallback;
	}

	url = icalattach_get_url (attach);
	g_assert (url != NULL);

	/* Ask for confirmation before executing the stuff */
	if (description.value)
		cmd = g_strconcat (url, " ", description.value, NULL);
	else
		cmd = (char *) url;

	result = 0;
	if (procedure_notification_dialog (cmd, url))
		result = gnome_execute_shell (NULL, cmd);
	
	if (cmd != (char *) url)
		g_free (cmd);

	icalattach_unref (attach);

	/* Fall back to display notification if we got an error */
	if (result < 0)
		goto fallback;

	return;

 fallback:

	display_notification (trigger, cqa, alarm_id, FALSE);
}



static gboolean
check_midnight_refresh (gpointer user_data)
{
	time_t new_midnight;
	icaltimezone *zone;

	d(printf("%s:%d (check_midnight_refresh)\n",__FILE__, __LINE__));
	
	zone = config_data_get_timezone ();
	new_midnight = time_day_end_with_zone (time (NULL), zone);

	if (new_midnight > midnight) {
		AlarmMsg *msg;
		struct _midnight_refresh_msg *list;
	
		/* These two structures will be freed by the msg destroy function*/
		msg = malloc (sizeof (AlarmMsg));
		msg->receive_msg = midnight_refresh_async;

		list = malloc (sizeof (struct _midnight_refresh_msg));

		list->remove = FALSE;
		/* We dont need it. So set it to NULL */
		msg->data = list;

		d(printf("%s:%d (check_midnight_refresh) - Posting a task to refresh\n",__FILE__, __LINE__));
		e_thread_put(alarm_operation_thread, (EMsg *)msg);		
	}

	return TRUE;
}

/**
 * alarm_queue_init:
 *
 * Initializes the alarm queueing system.  This should be called near the
 * beginning of the program.
 **/
void
alarm_queue_init (gpointer data)
{
	an = data;
	g_return_if_fail (alarm_queue_inited == FALSE);

	d(printf("%s:%d (alarm_queue_init)\n",__FILE__, __LINE__));
	
	client_alarms_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	queue_midnight_refresh ();

	saved_notification_time = config_data_get_last_notification_time ();
	if (saved_notification_time == -1) {
		saved_notification_time = time (NULL);
		d(printf("%s:%d (alarm_queue_init) - Setting last notification time to %s\n",__FILE__, __LINE__, ctime(&saved_notification_time)));
		config_data_set_last_notification_time (saved_notification_time);
	}

	/* install timeout handler (every 30 mins) for not missing the midnight refresh */
	g_timeout_add (1800000, (GSourceFunc) check_midnight_refresh, NULL);

#ifdef HAVE_LIBNOTIFY
	notify_init("Evolution Alarms");
#endif

	alarm_queue_inited = TRUE;
}

static gboolean
free_client_alarms_cb (gpointer key, gpointer value, gpointer user_data)
{
	ClientAlarms *ca = value;

	d(printf("%s:%d (free_client_alarms_cb) - %d \n",__FILE__, __LINE__, ca));
	
	if (ca) {
		remove_client_alarms (ca);
		if (ca->client) {
			d(printf("%s:%d (free_client_alarms_cb) - Disconnecting Client \n",__FILE__, __LINE__));
						
			g_signal_handlers_disconnect_matched (ca->client, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, ca);
			g_object_unref (ca->client);
		}

		if (ca->query) {
			d(printf("%s:%d (free_client_alarms_cb) - Disconnecting Query \n",__FILE__, __LINE__));
			
			g_signal_handlers_disconnect_matched (ca->query, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, ca);
			g_object_unref (ca->query);
		}

		g_hash_table_destroy (ca->uid_alarms_hash);

		g_free (ca);
		return TRUE;
	}

	return FALSE;
}

/**
 * alarm_queue_done:
 *
 * Shuts down the alarm queueing system.  This should be called near the end
 * of the program.  All the monitored calendar clients should already have been
 * unregistered with alarm_queue_remove_client().
 **/
void
alarm_queue_done (void)
{
	g_return_if_fail (alarm_queue_inited);

	/* All clients must be unregistered by now */
	g_return_if_fail (g_hash_table_size (client_alarms_hash) == 0);

	d(printf("%s:%d (alarm_queue_done)\n",__FILE__, __LINE__));
	
	g_hash_table_foreach_remove (client_alarms_hash, (GHRFunc) free_client_alarms_cb, NULL);
	g_hash_table_destroy (client_alarms_hash);
	client_alarms_hash = NULL;

	if (midnight_refresh_id != NULL) {
		alarm_remove (midnight_refresh_id);
		midnight_refresh_id = NULL;
	}

	alarm_queue_inited = FALSE;
}

static gboolean 
compare_ids (gpointer a, gpointer b)
{
	ECalComponentId *id, *id1;

	id = a;
	id1 = b;
	if (id->uid != NULL && id1->uid != NULL) {
		if (g_str_equal (id->uid, id1->uid)) {

			if (id->rid && id1->rid)
				return g_str_equal (id->rid, id1->rid);
			else if (!(id->rid && id1->rid))
				return TRUE;
		} 
	}
	return FALSE;
}

static guint 
hash_ids (gpointer a)
{
	ECalComponentId *id =a;

	return g_str_hash (id->uid);
}

struct _alarm_client_msg {
	ECal *client;
};

static void alarm_queue_add_async (EThread *e, AlarmMsg *msg, void *data)
{
	ClientAlarms *ca;
	struct _alarm_client_msg *list = msg->data;
	ECal *client = list->client;

	g_return_if_fail (alarm_queue_inited);
	g_return_if_fail (client != NULL);
	g_return_if_fail (E_IS_CAL (client));

	ca = lookup_client (client);
	if (ca) {
		/* We already have it. Unref the passed one*/
		g_object_unref(client);
		return;
	}

	d(printf("%s:%d (alarm_queue_add_async) - %d\n",__FILE__, __LINE__, client));
	
	ca = g_new (ClientAlarms, 1);

	ca->client = client;
	ca->query = NULL;

	g_hash_table_insert (client_alarms_hash, client, ca);

	ca->uid_alarms_hash = g_hash_table_new ((GHashFunc) hash_ids, (GEqualFunc) compare_ids);

	if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED) {
		load_alarms_for_today (ca);
	} else {
		g_signal_connect (client, "cal_opened",
				  G_CALLBACK (cal_opened_cb),
				  ca);
	}	
}

/**
 * alarm_queue_add_client:
 * @client: A calendar client.
 *
 * Adds a calendar client to the alarm queueing system.  Alarm trigger
 * notifications will be presented at the appropriate times.  The client should
 * be removed with alarm_queue_remove_client() when receiving notifications
 * from it is no longer desired.
 *
 * A client can be added any number of times to the alarm queueing system,
 * but any single alarm trigger will only be presented once for a particular
 * client.  The client must still be removed the same number of times from the
 * queueing system when it is no longer wanted.
 **/
void
alarm_queue_add_client (ECal *client)
{
	AlarmMsg *msg;
	struct _alarm_client_msg *list;

	/* These two structures will be freed by the msg destroy function*/
	msg = malloc (sizeof (AlarmMsg));
	msg->receive_msg = alarm_queue_add_async;

	list = malloc (sizeof (struct _alarm_client_msg));
	list->client = client;
	g_object_ref (client);
	msg->data = list;

	d(printf("%s:%d (alarm_queue_add_client) - Posting a task\n",__FILE__, __LINE__));
	e_thread_put(alarm_operation_thread, (EMsg *)msg);
}

/* Removes a component an its alarms */
static void
remove_cqa (ClientAlarms *ca, ECalComponentId *id, CompQueuedAlarms *cqa)
{

	/* If a component is present, then it means we must have alarms queued
	 * for it.
	 */
	g_assert (cqa->queued_alarms != NULL);

	d(printf("%s:%d (remove_cqa) - removing %d alarms\n",__FILE__, __LINE__, g_list_length(cqa->queued_alarms)));
	remove_alarms (cqa, TRUE);
}

static gboolean
remove_comp_by_id (gpointer key, gpointer value, gpointer userdata) {

	ClientAlarms *ca = (ClientAlarms *)userdata;
	
	d(printf("%s:%d (remove_comp_by_id)\n",__FILE__, __LINE__));
	
/* 	if (!g_hash_table_size (ca->uid_alarms_hash)) */
/* 		return; */
	
	remove_cqa (ca, (ECalComponentId *)key, (CompQueuedAlarms *) value);

	return TRUE;
}


/* Removes all the alarms queued for a particular calendar client */
static void
remove_client_alarms (ClientAlarms *ca)
{
	d(printf("%s:%d (remove_client_alarms) - size %d \n",__FILE__, __LINE__, g_hash_table_size (ca->uid_alarms_hash))); 

	g_hash_table_foreach_remove  (ca->uid_alarms_hash, (GHFunc)remove_comp_by_id, ca);
	
	/* The hash table should be empty now */
	g_assert (g_hash_table_size (ca->uid_alarms_hash) == 0);
}

/**
 * alarm_queue_remove_client:
 * @client: A calendar client.
 *
 * Removes a calendar client from the alarm queueing system.
 **/
static void
alarm_queue_remove_async (EThread *e, AlarmMsg *msg, void *data)
{
	ClientAlarms *ca;
	struct _alarm_client_msg *list = msg->data;
	ECal *client = list->client;

	g_return_if_fail (alarm_queue_inited);
	g_return_if_fail (client != NULL);
	g_return_if_fail (E_IS_CAL (client));

	ca = lookup_client (client);
	g_return_if_fail (ca != NULL);

	d(printf("%s:%d (alarm_queue_remove_async) \n",__FILE__, __LINE__));
	remove_client_alarms (ca);

	/* Clean up */
	if (ca->client) {
		d(printf("%s:%d (alarm_queue_remove_async) - Disconnecting Client \n",__FILE__, __LINE__));
		
		g_signal_handlers_disconnect_matched (ca->client, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, ca);
		g_object_unref (ca->client);
		ca->client = NULL;
	}

	if (ca->query) {
		d(printf("%s:%d (alarm_queue_remove_async) - Disconnecting Query \n",__FILE__, __LINE__));
			
		g_signal_handlers_disconnect_matched (ca->query, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, ca);
		g_object_unref (ca->query);
		ca->query = NULL;
	}

	g_hash_table_destroy (ca->uid_alarms_hash);
	ca->uid_alarms_hash = NULL;

	g_free (ca);

	g_hash_table_remove (client_alarms_hash, client);
}

void
alarm_queue_remove_client (ECal *client)
{
	AlarmMsg *msg;
	struct _alarm_client_msg *list;

	/* These two structures will be freed by the msg destroy function*/
	msg = malloc (sizeof (AlarmMsg));
	msg->receive_msg = alarm_queue_remove_async;

	list = malloc (sizeof (struct _alarm_client_msg));
	list->client = client;
	msg->data = list;

	d(printf("%s:%d (alarm_queue_remove_client) - Posting a task\n",__FILE__, __LINE__));
	e_thread_put(alarm_operation_thread, (EMsg *)msg);
}

/* Update non-time related variables for various structures on modification of an existing component 
   to be called only from query_objects_changed_cb */
static void 
update_cqa (CompQueuedAlarms *cqa, ECalComponent *newcomp)
{
	ECalComponent *oldcomp;
	ECalComponentAlarms *alarms = NULL;
	GSList *qa_list;	/* List of current QueuedAlarms corresponding to cqa */
	time_t from, to;
	icaltimezone *zone;
	ECalComponentAlarmAction omit[] = {-1};

	oldcomp = cqa->alarms->comp;

	zone = config_data_get_timezone ();
	from = time_day_begin_with_zone (time (NULL), zone);
	to = time_day_end_with_zone (time (NULL), zone);

	d(printf("%s:%d (update_cqa) - Generating alarms between %s and %s\n",__FILE__, __LINE__, ctime(&from), ctime(&to)));
	alarms = e_cal_util_generate_alarms_for_comp (newcomp, from, to, omit, 
					e_cal_resolve_tzid_cb, cqa->parent_client->client, zone);

	/* Update auids in Queued Alarms*/
	for (qa_list = cqa->queued_alarms; qa_list; qa_list = qa_list->next) {
		QueuedAlarm *qa = qa_list->data;
		char *check_auid = (char *) qa->instance->auid;

		if (e_cal_component_get_alarm (newcomp, check_auid))
			continue;
		else {
			if (e_cal_component_get_alarm (oldcomp, check_auid)) { /* Need to update QueuedAlarms */
				if (alarms == NULL) {
					d(printf("%s:%d (update_cqa) - No alarms found in the modified component\n",__FILE__, __LINE__)); 
					break;
				}
				update_qa (alarms, qa);
			}
			else
				g_warning ("Failed in auid lookup for old component also\n");
		}		
	}

	/* Update the actual component stored in CompQueuedAlarms structure */
	g_object_unref (cqa->alarms->comp);
	cqa->alarms->comp = newcomp;
	if (alarms != NULL )
		e_cal_component_alarms_free (alarms);
}

static void
update_qa (ECalComponentAlarms *alarms, QueuedAlarm *qa)
{
	ECalComponentAlarmInstance *al_inst;
	GSList *instance_list;

	d(printf("%s:%d (update_qa)\n",__FILE__, __LINE__));
	for (instance_list = alarms->alarms; instance_list; instance_list = instance_list->next) {
		al_inst = instance_list->data;
		if (al_inst->trigger == qa->orig_trigger) {  /* FIXME if two or more alarm instances (audio, note) 									  for same component have same trigger */
			g_free ((char *) qa->instance->auid);
			qa->instance->auid = g_strdup (al_inst->auid);
			break;
		}
	}
}	
