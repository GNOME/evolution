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
static GtkWidget *tray_image = NULL;
static GtkWidget *tray_event_box = NULL;
#else
static GtkStatusIcon *tray_icon = NULL;
#endif
static int tray_blink_id = -1;
static int tray_blink_state = FALSE;
static AlarmNotify *an;

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

	midnight_refresh_id = alarm_add (midnight, midnight_refresh_cb, NULL, NULL);
	if (!midnight_refresh_id) {
		g_message ("queue_midnight_refresh(): Could not set up the midnight refresh alarm!");
		/* FIXME: what to do? */
	}
}

/* Loads a client's alarms; called from g_hash_table_foreach() */
static void
add_client_alarms_cb (gpointer key, gpointer value, gpointer data)
{
	ClientAlarms *ca;

	ca = value;
	load_alarms_for_today (ca);
}

/* Loads the alarms for the new day every midnight */
static void
midnight_refresh_cb (gpointer alarm_id, time_t trigger, gpointer data)
{
	/* Re-load the alarms for all clients */

	g_hash_table_foreach (client_alarms_hash, add_client_alarms_cb, NULL);

	/* Re-schedule the midnight update */

	if (midnight_refresh_id != NULL) {
		alarm_remove (midnight_refresh_id);
		midnight_refresh_id = NULL;
	}

	queue_midnight_refresh ();
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
	QueuedAlarm *qa;
	GSList *l;

	qa = NULL;

	for (l = cqa->queued_alarms; l; l = l->next) {
		qa = l->data;
		if (qa->alarm_id == alarm_id)
			break;
	}

	if (!l)
		return;

	cqa->queued_alarms = g_slist_remove_link (cqa->queued_alarms, l);
	g_slist_free_1 (l);

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

	if (free_object) {
		g_hash_table_remove (cqa->parent_client->uid_alarms_hash, cqa->id);
		e_cal_component_free_id (cqa->id);
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
		g_message ("No alarms to add");
		if (alarms)
			e_cal_component_alarms_free (alarms);
		return;
	}

	cqa = g_new (CompQueuedAlarms, 1);
	cqa->parent_client = ca;
	cqa->alarms = alarms;
	cqa->expecting_update = FALSE;

	cqa->queued_alarms = NULL;

	for (l = alarms->alarms; l; l = l->next) {
		ECalComponentAlarmInstance *instance;
		gpointer alarm_id;
		QueuedAlarm *qa;

		instance = l->data;

		g_message ("Adding alarm at %lu (%lu)", instance->trigger, time (NULL));
		alarm_id = alarm_add (instance->trigger, alarm_trigger_cb, cqa, NULL);
		if (!alarm_id) {
			g_message ("add_component_alarms(): Could not schedule a trigger for "
				   "%ld, discarding...", (long) instance->trigger);
			continue;
		}

		qa = g_new (QueuedAlarm, 1);
		qa->alarm_id = alarm_id;
		qa->instance = instance;
		qa->orig_trigger = instance->trigger;
		qa->snooze = FALSE;

		cqa->queued_alarms = g_slist_prepend (cqa->queued_alarms, qa);
	}

	id = e_cal_component_get_id (alarms->comp);

	/* If we failed to add all the alarms, then we should get rid of the cqa */
	if (cqa->queued_alarms == NULL) {
		e_cal_component_alarms_free (cqa->alarms);
		cqa->alarms = NULL;

		g_free (cqa);
		return;
	}

	cqa->queued_alarms = g_slist_reverse (cqa->queued_alarms);
	cqa->id = id;
	g_hash_table_insert (ca->uid_alarms_hash, cqa->id, cqa);
}

/* Loads the alarms of a client for a given range of time */
static void
load_alarms (ClientAlarms *ca, time_t start, time_t end)
{
	char *str_query, *iso_start, *iso_end;

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
		g_signal_handlers_disconnect_matched (ca->query, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, ca);
		g_object_unref (ca->query);
		ca->query = NULL;
	}

	/* FIXME: handle errors */
	if (!e_cal_get_query (ca->client, str_query, &ca->query, NULL)) {
		g_warning (G_STRLOC ": Could not get query for client");
	} else {
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

	g_message ("Loading alarms for today");
	day_end = time_day_end_with_zone (now, zone);
	load_alarms (ca, from, day_end);
}

/* Called when a calendar client finished loading; we load its alarms */
static void
cal_opened_cb (ECal *client, ECalendarStatus status, gpointer data)
{
	ClientAlarms *ca;

	ca = data;

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
remove_comp (ClientAlarms *ca, const ECalComponentId *id)
{
	CompQueuedAlarms *cqa;

	cqa = lookup_comp_queued_alarms (ca, id);
	if (!cqa) 
		return;

	/* If a component is present, then it means we must have alarms queued
	 * for it.
	 */
	g_assert (cqa->queued_alarms != NULL);

	remove_alarms (cqa, TRUE);

	/* The list should be empty now, and thus the queued component alarms
	 * structure should have been freed and removed from the hash table.
	 */
	g_assert (lookup_comp_queued_alarms (ca, id) == NULL);
}

/* Called when a calendar component changes; we must reload its corresponding
 * alarms.
 */
static void
query_objects_changed_cb (ECal *client, GList *objects, gpointer data)
{
	ClientAlarms *ca;
	time_t from, day_end;
	ECalComponentAlarms *alarms;
	gboolean found;
	icaltimezone *zone;
	CompQueuedAlarms *cqa;
	GList *l;

	ca = data;

	from = config_data_get_last_notification_time ();
	if (from == -1)
		from = time (NULL);
	else
		from += 1; /* we add 1 to make sure the alarm is not displayed twice */

	zone = config_data_get_timezone ();

	day_end = time_day_end_with_zone (time (NULL), zone);
	g_message ("Query response for alarms");
	for (l = objects; l != NULL; l = l->next) {
		ECalComponentId *id;
		GSList *sl;
		ECalComponent *comp = e_cal_component_new ();

		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (l->data));

		id = e_cal_component_get_id (comp);
		found = e_cal_get_alarms_for_object (ca->client, id, from, day_end, &alarms);

		if (!found) {
			g_message ("No alarms found on object");
			remove_comp (ca, id);
			e_cal_component_free_id (id);
			g_object_unref (comp);
			comp = NULL;
			continue;
		}

		cqa = lookup_comp_queued_alarms (ca, id);
		if (!cqa) {
			g_message ("No currently queue alarms");
			add_component_alarms (ca, alarms);
			g_object_unref (comp);
			comp = NULL;
			continue;
		}

		g_message ("Already existing alarms");

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
				g_message (G_STRLOC ": Could not schedule a trigger for "
					   "%ld, discarding...", (long) instance->trigger);
				continue;
			}

			qa = g_new (QueuedAlarm, 1);
			qa->alarm_id = alarm_id;
			qa->instance = instance;
			qa->snooze = FALSE;
			qa->orig_trigger = instance->trigger;	
			cqa->queued_alarms = g_slist_prepend (cqa->queued_alarms, qa);
		}
		
		cqa->queued_alarms = g_slist_reverse (cqa->queued_alarms);
		g_object_unref (comp);
		comp = NULL;
	}
}

/* Called when a calendar component is removed; we must delete its corresponding
 * alarms.
 */
static void
query_objects_removed_cb (ECal *client, GList *objects, gpointer data)
{
	ClientAlarms *ca;
	GList *l;

	ca = data;

	for (l = objects; l != NULL; l = l->next)
		remove_comp (ca, l->data);
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
		g_message ("create_snooze(): Could not schedule a trigger for "
			   "%ld, discarding...", (long) t);
		return;
	}

	orig_qa->instance->trigger = t;
	orig_qa->alarm_id = new_id;
	orig_qa->snooze = TRUE;
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
on_dialog_objs_removed_cb (ECal *client, GList *objects, gpointer data)
{
	const char *our_uid;
	GList *l;
	TrayIconData *tray_data = data;

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

/* Callback used from the alarm notify dialog */
static void
notify_dialog_cb (AlarmNotifyResult result, int snooze_mins, gpointer data)
{
	TrayIconData *tray_data = data;
	
	g_signal_handlers_disconnect_matched (tray_data->query, G_SIGNAL_MATCH_FUNC,
					      0, 0, NULL, on_dialog_objs_removed_cb, NULL);

	switch (result) {
	case ALARM_NOTIFY_SNOOZE:
		create_snooze (tray_data->cqa, tray_data->alarm_id, snooze_mins);
		tray_data->snooze_set = TRUE;
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
		
			/* FIXME tray_icons_list is a global data structure - make this thread safe */

			list = tray_icons_list;
			while (list != NULL) {

				tray_data = list->data;
	
				if (!tray_data->snooze_set){
					GList *temp = list->next;
					tray_icons_list = g_list_remove_link (tray_icons_list, list);
					remove_queued_alarm (tray_data->cqa, tray_data->alarm_id, TRUE, TRUE);
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
	
	qa = lookup_queued_alarm (tray_data->cqa, tray_data->alarm_id);
	if (qa) {
		GdkPixbuf *pixbuf;
		GtkTooltips *tooltips = gtk_tooltips_new ();
		
		pixbuf = e_icon_factory_get_icon ("stock_appointment-reminder", E_ICON_SIZE_LARGE_TOOLBAR);

#ifndef USE_GTK_STATUS_ICON
		gtk_image_set_from_pixbuf (GTK_IMAGE (tray_image), pixbuf);
#else
		gtk_status_icon_set_from_pixbuf (tray_icon, pixbuf);
#endif
		g_object_unref (pixbuf);	

		if (tray_blink_id > -1)
			g_source_remove (tray_blink_id);
		tray_blink_id = -1;
		
#ifndef USE_GTK_STATUS_ICON
		gtk_tooltips_set_tip (tooltips, tray_event_box, NULL, NULL);
#else
		gtk_status_icon_set_tooltip (tray_icon, NULL);
#endif
		if (!alarm_notifications_dialog)
			alarm_notifications_dialog = notified_alarms_dialog_new ();
		
		if (alarm_notifications_dialog) {

			GtkTreeSelection *selection = NULL;
			GtkTreeModel *model = NULL;
			
			selection = gtk_tree_view_get_selection (
				GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
			model = gtk_tree_view_get_model (
				GTK_TREE_VIEW(alarm_notifications_dialog->treeview));
		
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

static void
alarm_quit (EPopup *ep, EPopupItem *pitem, void *data)
{
	bonobo_main_quit ();
}

static void
menu_item_toggle_callback (GtkToggleButton *item, void *data)
{
	gboolean state = gtk_toggle_button_get_active (item);
	ESource *source = e_source_copy ((ESource *) data);
	GSList *groups, *sel_groups, *p;

	if (e_source_get_uri ((ESource *)data)) {
		g_free (e_source_get_uri (source));
		e_source_set_absolute_uri (source, g_strdup (e_source_get_uri ((ESource *)data)));
	}

	if (state) {
		const char *uid = e_source_peek_uid (source);
		ESourceList *selected_cal = alarm_notify_get_selected_calendars (an);
		ESourceList *all_cal;
		ESourceGroup *group = NULL, *sel_group = NULL;
		const char *grp_uid = NULL;
		char * check_grp_uid = NULL;
		ESource *source_got = NULL;		
		gboolean found_grp = FALSE;

		e_cal_get_sources (&all_cal, E_CAL_SOURCE_TYPE_EVENT, NULL);

		alarm_notify_add_calendar (an, E_CAL_SOURCE_TYPE_EVENT, source, FALSE);

		/* Browse the list of all calendars for the newly added calendar*/
		groups = e_source_list_peek_groups (all_cal);
		for (p = groups; p != NULL; p = p->next) {
			group = E_SOURCE_GROUP (p->data);
			source_got = e_source_group_peek_source_by_uid (group, uid);

			if (source_got) {	/* You have got the group */
				break;
			}
		}

		/* Ensure that the source is under some source group in all calendar list */
		if (group == NULL){
			g_warning ("Source Group selected is *NOT* in all calendar list");
			g_object_unref (all_cal);
			return;
		}

		/* Get the group id from the above */
		grp_uid =  e_source_group_peek_uid (group);

		/* Look for the particular group in the original selected calendars list */
		sel_groups = e_source_list_peek_groups (selected_cal);
		for (p = sel_groups; p != NULL; p = p->next) {
			sel_group = E_SOURCE_GROUP (p->data);
			check_grp_uid = g_strdup ((const char *)e_source_group_peek_uid (sel_group));
			if (!strcmp (grp_uid, check_grp_uid)) {
				g_free (check_grp_uid);
				found_grp = TRUE;
				break;
			}	
			g_free (check_grp_uid);
		}

		if (found_grp != TRUE) {
			g_warning ("Did not find the source group to add the source in the selected calendars");
			g_object_unref (all_cal);
			return;
		}

		e_source_group_add_source (sel_group, source, -1);

		g_object_unref (all_cal);

	} else {
		const char *uid = e_source_peek_uid (source);
		ESourceList *selected_cal = alarm_notify_get_selected_calendars (an);
		alarm_notify_remove_calendar (an, E_CAL_SOURCE_TYPE_EVENT, e_source_get_uri (source));
		
		/* Browse the calendar for alarms and remove the source */
		groups = e_source_list_peek_groups (selected_cal);
		for (p = groups; p != NULL; p = p->next) {
			ESourceGroup *group = E_SOURCE_GROUP (p->data);
			ESource *del_source;
		
			del_source = e_source_group_peek_source_by_uid (group, uid);
			if (del_source) {
				e_source_group_remove_source_by_uid (group, uid);
				break;
			}
		}
	}
		
}

static GtkWidget *
populate ()
{
	GtkWidget *frame = gtk_frame_new (NULL);
	GtkWidget *label1 = gtk_label_new (NULL);
	GtkWidget *box = gtk_vbox_new(FALSE, 0);
	ESourceList *selected_cal = alarm_notify_get_selected_calendars (an);	
	GSList *groups;
	GSList *p;
	ESourceList *source_list;
	
	gtk_label_set_markup (GTK_LABEL(label1), _("<b>Calendars</b>"));
	gtk_frame_set_label_widget (GTK_FRAME(frame), label1);
	
	if (!e_cal_get_sources (&source_list, E_CAL_SOURCE_TYPE_EVENT, NULL)) {
		g_message (G_STRLOC ": Could not get the list of sources to load");

		return NULL;
	}
		
	groups = e_source_list_peek_groups (source_list);

	for (p = groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);
		char *txt = g_strdup_printf ("<b>%s</b>", e_source_group_peek_name (group));
		GtkWidget *item = gtk_label_new (NULL);
		GSList *q;
		GtkWidget *hbox, *image;

		hbox = gtk_hbox_new (FALSE, 0);
		image = e_icon_factory_get_image  ("stock_appointment-reminder", E_ICON_SIZE_SMALL_TOOLBAR);

		gtk_box_pack_start ((GtkBox *)hbox, image, FALSE, FALSE, 2);
		gtk_box_pack_start ((GtkBox *)hbox, item, FALSE, FALSE, 2);
		
		gtk_label_set_markup (GTK_LABEL(item), txt);
		gtk_label_set_justify (GTK_LABEL(item), GTK_JUSTIFY_LEFT);
		g_free (txt);

		gtk_box_pack_start (GTK_BOX(box), hbox, TRUE, TRUE, 4);
		gtk_widget_show_all (hbox);

		for (q = e_source_group_peek_sources (group); q != NULL; q = q->next) {
			ESource *source = E_SOURCE (q->data);
			GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
			GtkWidget *item = gtk_check_button_new_with_label (e_source_peek_name (source));

			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(item), e_source_list_peek_source_by_uid (selected_cal, e_source_peek_uid (source)) ? TRUE:FALSE);
			
			gtk_box_pack_start ((GtkBox *)hbox, item, FALSE, FALSE, 24);
			gtk_object_set_data_full (GTK_OBJECT (item), "ESourceMenu", source,
						  (GtkDestroyNotify) g_object_unref);

			g_signal_connect (item, "toggled", G_CALLBACK (menu_item_toggle_callback), source);

			gtk_box_pack_start (GTK_BOX(box), hbox, FALSE, FALSE, 2);
			gtk_widget_show_all (hbox);
		}
	}

	gtk_container_add (GTK_CONTAINER(frame), box);
	gtk_container_set_border_width (GTK_CONTAINER(frame), 6);
	return frame;
}

static void
alarm_pref_response (GtkWidget *widget, int response, gpointer dummy)
{
	gtk_widget_destroy (widget);	
}

static void
alarms_configure (EPopup *ep, EPopupItem *pitem, void *data)
{
	GtkWidget *box = populate ();
	GtkWidget *dialog;

	dialog = gtk_dialog_new_with_buttons (_("Preferences"), 
						NULL,0,
						GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
						GTK_STOCK_HELP, GTK_RESPONSE_HELP,
						NULL);

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), box);
	gtk_dialog_set_has_separator (GTK_DIALOG(dialog), FALSE);
	g_signal_connect (dialog, "response", G_CALLBACK (alarm_pref_response), NULL);
	gtk_widget_show_all (dialog);
}

static EPopupItem tray_items[] = {
	{ E_POPUP_ITEM, "00.configure", N_("_Configure Alarms"), alarms_configure, NULL, GTK_STOCK_PREFERENCES },
	{ E_POPUP_BAR , "10.bar" },
	{ E_POPUP_ITEM, "10.quit", N_("_Quit"), alarm_quit, NULL, GTK_STOCK_QUIT },
};

static void
tray_popup_free(EPopup *ep, GSList *items, void *data)
{
	g_slist_free(items);
}

static gint
tray_icon_clicked_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	TrayIconData *tray_data = user_data;

	if (event->type == GDK_BUTTON_PRESS) {
		if (event->button == 1 && g_list_length (tray_icons_list) > 0) {
			GList *tmp;
			for (tmp = tray_icons_list; tmp; tmp = tmp->next) {
				open_alarm_dialog (tmp->data);
			}
			
			return TRUE;
		} else if (event->button == 3) {
			GtkMenu *menu;
			GSList *menus = NULL;
			EPopup *ep;
			int i;
			GdkPixbuf *pixbuf;
			GtkTooltips *tooltips = gtk_tooltips_new ();
			
			tray_blink_state = FALSE;
			pixbuf = e_icon_factory_get_icon  (tray_blink_state == TRUE ?
							   "stock_appointment-reminder-excl" :
							   "stock_appointment-reminder",
							   E_ICON_SIZE_LARGE_TOOLBAR);
	
#ifndef USE_GTK_STATUS_ICON
			gtk_image_set_from_pixbuf (GTK_IMAGE (tray_image), pixbuf);
#else
			gtk_status_icon_set_from_pixbuf (tray_icon, pixbuf);
#endif
			g_object_unref (pixbuf);	

			if (tray_blink_id > -1)
				g_source_remove (tray_blink_id);
			tray_blink_id = -1;

#ifndef USE_GTK_STATUS_ICON
			gtk_tooltips_set_tip (tooltips, tray_event_box, NULL, NULL);
#else
			gtk_status_icon_set_tooltip (tray_icon, NULL);
#endif

			ep = e_popup_new("org.gnome.evolution.alarmNotify.popup");
			for (i=0;i<sizeof(tray_items)/sizeof(tray_items[0]);i++)
				menus = g_slist_prepend(menus, &tray_items[i]);
			e_popup_add_items(ep, menus, NULL, tray_popup_free, tray_data);
			menu = e_popup_create_menu_once(ep, NULL, 0);
			gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);

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

static void 
popup_menu (GtkStatusIcon *icon,
	    guint          button,
	    guint32        activate_time)
{
  GdkEventButton event;

  event.type = GDK_BUTTON_PRESS;
  event.button = button;
  event.time = activate_time;

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
	gtk_image_set_from_pixbuf (GTK_IMAGE (tray_image), pixbuf);
#else
	gtk_status_icon_set_from_pixbuf (tray_icon, pixbuf);
#endif
	g_object_unref (pixbuf);

	return TRUE;
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
	GtkWidget *tray_icon=NULL, *image=NULL;
	GtkTooltips *tooltips;
#endif
	TrayIconData *tray_data;
	ECalComponentText text;
	GSList *text_list;
	char *str, *start_str, *end_str, *alarm_str, *time_str;
	icaltimezone *current_zone;
	ECalComponentOrganizer organiser;

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
	tooltips = gtk_tooltips_new ();
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

	tray_icons_list = g_list_prepend (tray_icons_list, tray_data);

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
		gtk_status_icon_set_tooltip (tray_icon, NULL);
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
	char *filename;
	char *body;
	
	comp = cqa->alarms->comp;
	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;
	if (!notify_is_initted ())
		notify_init("Evolution Alarm Notify");
	GdkPixbuf *icon = e_icon_factory_get_icon("stock_appointment-reminder", E_ICON_SIZE_DIALOG);
	g_free (filename);
	
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

	NotifyNotification *n = notify_notification_new (summary, body, "", NULL);
	notify_notification_set_icon_data_from_pixbuf (n, icon);
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

	zone = config_data_get_timezone ();
	new_midnight = time_day_end_with_zone (time (NULL), zone);

	if (new_midnight > midnight) {
		/* Re-load the alarms for all clients */
		g_hash_table_foreach (client_alarms_hash, add_client_alarms_cb, NULL);

		queue_midnight_refresh ();
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
#ifndef USE_GTK_STATUS_ICON
	GtkWidget *tray_icon;
#endif
	an = data;
	g_return_if_fail (alarm_queue_inited == FALSE);

	client_alarms_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	queue_midnight_refresh ();

	saved_notification_time = config_data_get_last_notification_time ();
	if (saved_notification_time == -1) {
		saved_notification_time = time (NULL);
		config_data_set_last_notification_time (saved_notification_time);
	}

	/* install timeout handler (every 30 mins) for not missing the midnight refresh */
	g_timeout_add (1800000, (GSourceFunc) check_midnight_refresh, NULL);

#ifndef USE_GTK_STATUS_ICON
	tray_icon = GTK_WIDGET (egg_tray_icon_new ("Evolution Alarm"));
	tray_image = e_icon_factory_get_image  ("stock_appointment-reminder", E_ICON_SIZE_LARGE_TOOLBAR);
	tray_event_box = gtk_event_box_new ();
	gtk_container_add (GTK_CONTAINER (tray_event_box), tray_image);
	gtk_container_add (GTK_CONTAINER (tray_icon), tray_event_box);
	g_signal_connect (G_OBJECT (tray_event_box), "button_press_event",
			  G_CALLBACK (tray_icon_clicked_cb), NULL);
	gtk_widget_show_all (tray_icon);
#else
	tray_icon = gtk_status_icon_new ();
	gtk_status_icon_set_from_pixbuf (tray_icon, e_icon_factory_get_icon ("stock_appointment-reminder", E_ICON_SIZE_LARGE_TOOLBAR));
	g_signal_connect (G_OBJECT (tray_icon), "activate",
			  G_CALLBACK (icon_activated), NULL);
	g_signal_connect (G_OBJECT (tray_icon), "popup-menu",
			  G_CALLBACK (popup_menu), NULL);
#endif

#ifdef HAVE_LIBNOTIFY
	notify_init("Evolution Alarms");
#endif

	alarm_queue_inited = TRUE;
}

static gboolean
free_client_alarms_cb (gpointer key, gpointer value, gpointer user_data)
{
	ClientAlarms *ca = value;

	if (ca) {
		remove_client_alarms (ca);
		if (ca->client) {
			g_signal_handlers_disconnect_matched (ca->client, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, ca);
			g_object_unref (ca->client);
		}

		if (ca->query) {
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
	ClientAlarms *ca;

	g_return_if_fail (alarm_queue_inited);
	g_return_if_fail (client != NULL);
	g_return_if_fail (E_IS_CAL (client));

	ca = lookup_client (client);
	if (ca) {
		return;
	}

	ca = g_new (ClientAlarms, 1);

	ca->client = client;
	ca->query = NULL;
	g_object_ref (ca->client);

	g_hash_table_insert (client_alarms_hash, client, ca);

	ca->uid_alarms_hash = g_hash_table_new (g_direct_hash, (GEqualFunc) compare_ids);

	if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED) {
		load_alarms_for_today (ca);
	} else {
		g_signal_connect (client, "cal_opened",
				  G_CALLBACK (cal_opened_cb),
				  ca);
	}
}

static void
remove_comp_by_id (gpointer key, gpointer value, gpointer userdata) {

	ClientAlarms *ca = (ClientAlarms *)userdata;
	remove_comp (ca, (ECalComponentId *)key);
}


/* Removes all the alarms queued for a particular calendar client */
static void
remove_client_alarms (ClientAlarms *ca)
{
	g_hash_table_foreach (ca->uid_alarms_hash, (GHFunc)remove_comp_by_id, ca);
	/* The hash table should be empty now */
	g_assert (g_hash_table_size (ca->uid_alarms_hash) == 0);
}

/**
 * alarm_queue_remove_client:
 * @client: A calendar client.
 *
 * Removes a calendar client from the alarm queueing system.
 **/
void
alarm_queue_remove_client (ECal *client)
{
	ClientAlarms *ca;

	g_return_if_fail (alarm_queue_inited);
	g_return_if_fail (client != NULL);
	g_return_if_fail (E_IS_CAL (client));

	ca = lookup_client (client);
	g_return_if_fail (ca != NULL);

	remove_client_alarms (ca);

	/* Clean up */

	if (ca->client) {
		g_signal_handlers_disconnect_matched (ca->client, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, ca);
		g_object_unref (ca->client);
		ca->client = NULL;
	}

	if (ca->query) {
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
					g_warning ("No alarms found on the modified component\n");
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
	
	for (instance_list = alarms->alarms; instance_list; instance_list = instance_list->next) {
		al_inst = instance_list->data;
		if (al_inst->trigger == qa->orig_trigger) {  /* FIXME if two or more alarm instances (audio, note) 									  for same component have same trigger */
			g_free ((char *) qa->instance->auid);
			qa->instance->auid = g_strdup (al_inst->auid);
			break;
		}
	}
}	
