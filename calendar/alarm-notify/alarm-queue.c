/*
 * Alarm queueing engine
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#ifdef HAVE_CANBERRA
#include <canberra-gtk.h>
#endif

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#include "alarm.h"
#include "alarm-notify-dialog.h"
#include "alarm-queue.h"
#include "alarm-notify.h"
#include "config-data.h"
#include "util.h"

#include "calendar/gui/print.h"

/* The dialog with alarm nofications */
static AlarmNotificationsDialog *alarm_notifications_dialog = NULL;

/* Whether the queueing system has been initialized */
static gboolean alarm_queue_inited = FALSE;

/* Clients we are monitoring for alarms */
static GHashTable *client_alarms_hash = NULL;

/* List of tray icons being displayed */
static GList *tray_icons_list = NULL;

/* Top Tray Image */
static GtkStatusIcon *tray_icon = NULL;
static gint tray_blink_id = -1;
static gint tray_blink_countdown = 0;
static AlarmNotify *an;

/* Structure that stores a client we are monitoring */
typedef struct {
	/* Monitored client */
	ECalClient *cal_client;

	/* The live view to the calendar */
	ECalClientView *view;

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

	#ifdef HAVE_LIBNOTIFY
	NotifyNotification *notify;
	#endif

	/* Whether this is a snoozed queued alarm or a normal one */
	guint snooze : 1;
} QueuedAlarm;

/* Alarm ID for the midnight refresh function */
static gpointer midnight_refresh_id = NULL;
static time_t midnight = 0;

static void	remove_client_alarms		(ClientAlarms *ca);
static void	display_notification		(time_t trigger,
						 CompQueuedAlarms *cqa,
						 gpointer alarm_id,
						 gboolean use_description);
static void	audio_notification		(time_t trigger,
						 CompQueuedAlarms *cqa,
						 gpointer alarm_id);
static void	mail_notification		(time_t trigger,
						 CompQueuedAlarms *cqa,
						 gpointer alarm_id);
static void	procedure_notification		(time_t trigger,
						 CompQueuedAlarms *cqa,
						 gpointer alarm_id);
#ifdef HAVE_LIBNOTIFY
static void	popup_notification		(time_t trigger,
						 CompQueuedAlarms *cqa,
						 gpointer alarm_id,
						 gboolean use_description);
#endif
static void	query_objects_modified_cb	(ECalClientView *view,
						 const GSList *objects,
						 gpointer data);
static void	query_objects_removed_cb	(ECalClientView *view,
						 const GSList *uids,
						 gpointer data);

static void	update_cqa			(CompQueuedAlarms *cqa,
						 ECalComponent *comp);
static void	update_qa			(ECalComponentAlarms *alarms,
						 QueuedAlarm *qa);
static void	tray_list_remove_cqa		(CompQueuedAlarms *cqa);
static void	on_dialog_objs_removed_cb	(ECalClientView *view,
						 const GSList *uids,
						 gpointer data);

/* Alarm queue engine */

static void	load_alarms_for_today		(ClientAlarms *ca);
static void	midnight_refresh_cb		(gpointer alarm_id,
						 time_t trigger,
						 gpointer data);

/* Simple asynchronous message dispatcher */

typedef struct _Message Message;
typedef void (*MessageFunc) (Message *msg);

struct _Message {
	MessageFunc func;
};

/*
static void
message_proxy (Message *msg)
{
	g_return_if_fail (msg->func != NULL);
 *
	msg->func (msg);
}
 *
static gpointer
create_thread_pool (void)
{
	return g_thread_pool_new ((GFunc) message_proxy, NULL, 1, FALSE, NULL);
}*/

static void
message_push (Message *msg)
{
	/* This used be pushed through the thread pool. This fix is made to
	 * work-around the crashers in dbus due to threading. The threading
	 * is not completely removed as its better to have alarm daemon
	 * running in a thread rather than blocking main thread.  This is
	 * the reason the creation of thread pool is commented out. */
	msg->func (msg);
}

/*
 * use a static ring-buffer so we can call this twice
 * in a printf without getting nonsense results.
 */
static const gchar *
e_ctime (const time_t *timep)
{
	static gchar *buffer[4] = { 0, };
	static gint next = 0;
	const gchar *ret;

	g_free (buffer[next]);
	ret = buffer[next++] = g_strdup (ctime (timep));
	if (buffer[next - 1] && *buffer[next - 1]) {
		gint len = strlen (buffer[next - 1]);
		while (len > 0 && (buffer[next - 1][len - 1] == '\n' ||
			buffer[next - 1][len - 1] == '\r' ||
			g_ascii_isspace (buffer[next - 1][len - 1])))
			len--;

		buffer[next - 1][len - 1] = 0;
	}

	if (next >= G_N_ELEMENTS (buffer))
		next = 0;

	return ret;
}

/* Queues an alarm trigger for midnight so that we can load the next
 * day's worth of alarms. */
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

	debug (("Refresh at %s", e_ctime (&midnight)));

	midnight_refresh_id = alarm_add (
		midnight, midnight_refresh_cb, NULL, NULL);
	if (!midnight_refresh_id) {
		debug (("Could not setup the midnight refresh alarm"));
		/* FIXME: what to do? */
	}
}

/* Loads a client's alarms; called from g_hash_table_foreach() */
static void
add_client_alarms_cb (gpointer key,
                      gpointer value,
                      gpointer data)
{
	ClientAlarms *ca = (ClientAlarms *) value;

	debug (("Adding %p", ca));

	load_alarms_for_today (ca);
}

struct _midnight_refresh_msg {
	Message header;
	gboolean remove;
};

/* Loads the alarms for the new day every midnight */
static void
midnight_refresh_async (struct _midnight_refresh_msg *msg)
{
	debug (("..."));

	/* Re-load the alarms for all clients */
	g_hash_table_foreach (client_alarms_hash, add_client_alarms_cb, NULL);

	/* Re-schedule the midnight update */
	if (msg->remove && midnight_refresh_id != NULL) {
		debug (("Reschedule the midnight update"));
		alarm_remove (midnight_refresh_id);
		midnight_refresh_id = NULL;
	}

	queue_midnight_refresh ();

	g_slice_free (struct _midnight_refresh_msg, msg);
}

static void
midnight_refresh_cb (gpointer alarm_id,
                     time_t trigger,
                     gpointer data)
{
	struct _midnight_refresh_msg *msg;

	msg = g_slice_new0 (struct _midnight_refresh_msg);
	msg->header.func = (MessageFunc) midnight_refresh_async;
	msg->remove = TRUE;

	message_push ((Message *) msg);
}

/* Looks up a client in the client alarms hash table */
static ClientAlarms *
lookup_client (ECalClient *cal_client)
{
	return g_hash_table_lookup (client_alarms_hash, cal_client);
}

/* Looks up a queued alarm based on its alarm ID */
static QueuedAlarm *
lookup_queued_alarm (CompQueuedAlarms *cqa,
                     gpointer alarm_id)
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

static void
alarm_queue_discard_alarm_cb (GObject *source,
			      GAsyncResult *result,
			      gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (source);
	GError *error = NULL;

	g_return_if_fail (client != NULL);

	if (!e_cal_client_discard_alarm_finish (client, result, &error) &&
	    !g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_NOT_SUPPORTED))
		g_warning ("Failed to discard alarm at '%s': %s",
			e_source_get_display_name (e_client_get_source (E_CLIENT (client))),
			error ? error->message : "Unknown error");

	g_clear_error (&error);
}

/* Removes an alarm from the list of alarms of a component.  If the alarm was
 * the last one listed for the component, it removes the component itself.
 */
static gboolean
remove_queued_alarm (CompQueuedAlarms *cqa,
                     gpointer alarm_id,
                     gboolean free_object,
                     gboolean remove_alarm)
{
	QueuedAlarm *qa = NULL;
	GSList *l;

	debug (("..."));

	for (l = cqa->queued_alarms; l; l = l->next) {
		qa = l->data;
		if (qa->alarm_id == alarm_id)
			break;
	}

	if (!l)
		return FALSE;

	cqa->queued_alarms = g_slist_delete_link (cqa->queued_alarms, l);

	if (remove_alarm && !e_client_is_readonly (E_CLIENT (cqa->parent_client->cal_client))) {
		ECalComponentId *id;

		id = e_cal_component_get_id (cqa->alarms->comp);
		if (id) {
			cqa->expecting_update = TRUE;
			e_cal_client_discard_alarm (
				cqa->parent_client->cal_client, id->uid,
				id->rid, qa->instance->auid, NULL,
				alarm_queue_discard_alarm_cb, NULL);
			cqa->expecting_update = FALSE;

			e_cal_component_free_id (id);
		}
	}

	#ifdef HAVE_LIBNOTIFY
	if (qa->notify) {
		notify_notification_close (qa->notify, NULL);
		g_clear_object (&qa->notify);
	}
	#endif
	g_free (qa);

	/* If this was the last queued alarm for this component, remove the
	 * component itself.
	 */

	if (cqa->queued_alarms != NULL)
		return FALSE;

	debug (("Last Component. Removing CQA- Free=%d", free_object));
	if (free_object) {
		cqa->id = NULL;
		cqa->parent_client = NULL;
		e_cal_component_alarms_free (cqa->alarms);
		g_free (cqa);
	} else {
		e_cal_component_alarms_free (cqa->alarms);
		cqa->alarms = NULL;
	}

	return TRUE;
}

/**
 * has_known_notification:
 * Test for notification method and returns if it knows it or not.
 * @comp: Component with an alarm.
 * @alarm_uid: ID of the alarm in the comp to test.
 *
 * Returns: %TRUE when we know the notification type, %FALSE otherwise.
 */
static gboolean
has_known_notification (ECalComponent *comp,
                        const gchar *alarm_uid)
{
	ECalComponentAlarm *alarm;
	ECalComponentAlarmAction action;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (alarm_uid != NULL, FALSE);

	alarm = e_cal_component_get_alarm (comp, alarm_uid);
	if (!alarm)
		 return FALSE;

	e_cal_component_alarm_get_action (alarm, &action);
	e_cal_component_alarm_free (alarm);

	switch (action) {
		case E_CAL_COMPONENT_ALARM_AUDIO:
		case E_CAL_COMPONENT_ALARM_DISPLAY:
		case E_CAL_COMPONENT_ALARM_EMAIL:
		case E_CAL_COMPONENT_ALARM_PROCEDURE:
			return TRUE;
		default:
			break;
	}

	return FALSE;
}

/* Callback used when an alarm triggers */
static void
alarm_trigger_cb (gpointer alarm_id,
                  time_t trigger,
                  gpointer data)
{
	CompQueuedAlarms *cqa;
	ECalComponent *comp;
	QueuedAlarm *qa;
	ECalComponentAlarm *alarm;
	ECalComponentAlarmAction action;

	cqa = data;
	comp = cqa->alarms->comp;

	config_data_set_last_notification_time (
		cqa->parent_client->cal_client, trigger);
	debug (("Setting Last notification time to %s", e_ctime (&trigger)));

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO) {
		icalproperty_status status = ICAL_STATUS_NONE;

		e_cal_component_get_status (comp, &status);

		if (status == ICAL_STATUS_COMPLETED &&
		    !config_data_get_task_reminder_for_completed ()) {
			return;
		}
	}

	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;

	/* Decide what to do based on the alarm action.  We use the trigger that
	 * is passed to us instead of the one from the instance structure
	 * because this may be a snoozed alarm instead of an original
	 * occurrence.
	 */

	alarm = e_cal_component_get_alarm (comp, qa->instance->auid);
	if (!alarm)
		 return;

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
		display_notification (trigger, cqa, alarm_id, TRUE);
		break;

	case E_CAL_COMPONENT_ALARM_EMAIL:
		mail_notification (trigger, cqa, alarm_id);
		break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE:
		procedure_notification (trigger, cqa, alarm_id);
		break;

	default:
		g_return_if_reached ();
	}
	debug (("Notification sent: %d", action));
}

/* Adds the alarms in a ECalComponentAlarms structure to the alarms queued for a
 * particular client.  Also puts the triggers in the alarm timer queue.
 */
static void
add_component_alarms (ClientAlarms *ca,
                      ECalComponentAlarms *alarms)
{
	ECalComponentId *id;
	CompQueuedAlarms *cqa;
	GSList *l;

	/* No alarms? */
	if (alarms == NULL || alarms->alarms == NULL) {
		debug (("No alarms to add"));
		if (alarms)
			e_cal_component_alarms_free (alarms);
		return;
	}

	cqa = g_new (CompQueuedAlarms, 1);
	cqa->parent_client = ca;
	cqa->alarms = alarms;
	cqa->expecting_update = FALSE;

	cqa->queued_alarms = NULL;
	debug (("Creating CQA %p", cqa));

	for (l = alarms->alarms; l; l = l->next) {
		ECalComponentAlarmInstance *instance;
		gpointer alarm_id;
		QueuedAlarm *qa;

		instance = l->data;

		if (!has_known_notification (cqa->alarms->comp, instance->auid))
			continue;

		alarm_id = alarm_add (
			instance->trigger, alarm_trigger_cb, cqa, NULL);
		if (!alarm_id)
			continue;

		qa = g_new0 (QueuedAlarm, 1);
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
		debug (("Failed to add all : %p", cqa));
		g_free (cqa);
		return;
	}

	cqa->queued_alarms = g_slist_reverse (cqa->queued_alarms);
	cqa->id = id;
	debug (("Alarm added for %s", id->uid));
	g_hash_table_insert (ca->uid_alarms_hash, cqa->id, cqa);
}

/* Loads the alarms of a client for a given range of time */
static void
load_alarms (ClientAlarms *ca,
             time_t start,
             time_t end)
{
	gchar *str_query, *iso_start, *iso_end;
	GError *error = NULL;

	debug (("..."));

	iso_start = isodate_from_time_t (start);
	if (!iso_start)
		return;

	iso_end = isodate_from_time_t (end);
	if (!iso_end) {
		g_free (iso_start);
		return;
	}

	str_query = g_strdup_printf (
		"(has-alarms-in-range? (make-time \"%s\") "
		"(make-time \"%s\"))", iso_start, iso_end);
	g_free (iso_start);
	g_free (iso_end);

	/* create the live query */
	if (ca->view) {
		debug (("Disconnecting old queries"));
		g_signal_handlers_disconnect_matched (
			ca->view, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, ca);
		g_object_unref (ca->view);
		ca->view = NULL;
	}

	e_cal_client_get_view_sync (
		ca->cal_client, str_query, &ca->view, NULL, &error);

	if (error != NULL) {
		g_warning (
			"%s: Could not get query for client: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	} else {
		debug (("Setting Call backs"));

		g_signal_connect (
			ca->view, "objects-added",
			G_CALLBACK (query_objects_modified_cb), ca);
		g_signal_connect (
			ca->view, "objects-modified",
			G_CALLBACK (query_objects_modified_cb), ca);
		g_signal_connect (
			ca->view, "objects-removed",
			G_CALLBACK (query_objects_removed_cb), ca);

		e_cal_client_view_start (ca->view, &error);

		if (error != NULL) {
			g_warning (
				"%s: Failed to start view: %s",
				G_STRFUNC, error->message);
			g_error_free (error);
		}
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
	 * We add 1 to the saved notification time to make the time ranges
	 * half-open; we do not want to display the "last" displayed alarm
	 * twice, once when it occurs and once when the alarm daemon restarts.
	 */
	from = config_data_get_last_notification_time (ca->cal_client) + 1;
	if (from <= 0)
		from = MAX (from, day_start);

	/* Add one hour after midnight, just to cover the delay in 30 minutes
	 * midnight checking. */
	day_end = time_day_end_with_zone (now, zone) + (60 * 60);
	debug (("From %s to %s", e_ctime (&from), e_ctime (&day_end)));
	load_alarms (ca, from, day_end);
}

/* Looks up a component's queued alarm structure in a client alarms structure */
static CompQueuedAlarms *
lookup_comp_queued_alarms (ClientAlarms *ca,
                           const ECalComponentId *id)
{
	return g_hash_table_lookup (ca->uid_alarms_hash, id);
}

static void
remove_alarms (CompQueuedAlarms *cqa,
               gboolean free_object)
{
	GSList *l;

	debug (("Removing for %p", cqa));

	tray_list_remove_cqa (cqa);

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
remove_comp (ClientAlarms *ca,
             ECalComponentId *id)
{
	CompQueuedAlarms *cqa;

	debug (("Removing uid %s", id->uid));

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
	g_return_if_fail (cqa->queued_alarms != NULL);

	debug (("Removing CQA %p", cqa));
	remove_alarms (cqa, TRUE);
}

/* Called when a calendar component changes; we must reload its corresponding
 * alarms.
 */
struct _query_msg {
	Message header;
	GSList *objects;
	gpointer data;
};

static GSList *
duplicate_ical (const GSList *in_list)
{
	const GSList *l;
	GSList *out_list = NULL;
	for (l = in_list; l; l = l->next) {
		out_list = g_slist_prepend (
			out_list, icalcomponent_new_clone (l->data));
	}

	return g_slist_reverse (out_list);
}

static GSList *
duplicate_ecal (const GSList *in_list)
{
	const GSList *l;
	GSList *out_list = NULL;
	for (l = in_list; l; l = l->next) {
		ECalComponentId *id, *old;
		old = l->data;
		id = g_new0 (ECalComponentId, 1);
		id->uid = g_strdup (old->uid);
		id->rid = g_strdup (old->rid);
		out_list = g_slist_prepend (out_list, id);
	}

	return g_slist_reverse (out_list);
}

static gboolean
get_alarms_for_object (ECalClient *cal_client,
                       const ECalComponentId *id,
                       time_t start,
                       time_t end,
                       ECalComponentAlarms **alarms)
{
	icalcomponent *icalcomp = NULL;
	ECalComponent *comp;
	ECalComponentAlarmAction omit[] = {-1};

	g_return_val_if_fail (cal_client != NULL, FALSE);
	g_return_val_if_fail (id != NULL, FALSE);
	g_return_val_if_fail (alarms != NULL, FALSE);
	g_return_val_if_fail (start >= 0 && end >= 0, FALSE);
	g_return_val_if_fail (start <= end, FALSE);

	e_cal_client_get_object_sync (
		cal_client, id->uid, id->rid, &icalcomp, NULL, NULL);

	if (icalcomp == NULL)
		return FALSE;

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		icalcomponent_free (icalcomp);
		g_object_unref (comp);
		return FALSE;
	}

	*alarms = e_cal_util_generate_alarms_for_comp (
		comp, start, end, omit, e_cal_client_resolve_tzid_cb,
		cal_client, config_data_get_timezone ());

	g_object_unref (comp);

	return TRUE;
}

static void
query_objects_changed_async (struct _query_msg *msg)
{
	ClientAlarms *ca;
	time_t from, day_end;
	ECalComponentAlarms *alarms;
	gboolean found;
	icaltimezone *zone;
	CompQueuedAlarms *cqa;
	GSList *l;
	GSList *objects;

	ca = msg->data;
	objects = msg->objects;

	from = config_data_get_last_notification_time (ca->cal_client);
	if (from == -1)
		from = time (NULL);
	else
		from += 1; /* we add 1 to make sure the alarm is not displayed twice */

	zone = config_data_get_timezone ();

	day_end = time_day_end_with_zone (time (NULL), zone);

	for (l = objects; l != NULL; l = l->next) {
		ECalComponentId *id;
		GSList *sl;
		ECalComponent *comp = e_cal_component_new ();

		e_cal_component_set_icalcomponent (comp, l->data);

		id = e_cal_component_get_id (comp);
		found = get_alarms_for_object (ca->cal_client, id, from, day_end, &alarms);

		if (!found) {
			debug (("No Alarm found for client %p", ca->cal_client));
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
			debug (("No currently queued alarms for %s", id->uid));
			add_component_alarms (ca, alarms);
			g_object_unref (comp);
			comp = NULL;
			continue;
		}

		debug (("Alarm Already Exist for %s", id->uid));
		/* If the alarms or the alarms list is empty,
		 * remove it after updating the cqa structure. */
		if (alarms == NULL || alarms->alarms == NULL) {

			/* Update the cqa and its queued alarms
			 * for changes in summary and alarm_uid.  */
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

			if (!has_known_notification (cqa->alarms->comp, instance->auid))
				continue;

			alarm_id = alarm_add (instance->trigger, alarm_trigger_cb, cqa, NULL);
			if (!alarm_id)
				continue;

			qa = g_new0 (QueuedAlarm, 1);
			qa->alarm_id = alarm_id;
			qa->instance = instance;
			qa->snooze = FALSE;
			qa->orig_trigger = instance->trigger;
			cqa->queued_alarms = g_slist_prepend (cqa->queued_alarms, qa);
			debug (("Adding %p to queue", qa));
		}

		cqa->queued_alarms = g_slist_reverse (cqa->queued_alarms);
		g_object_unref (comp);
		comp = NULL;
	}
	g_slist_free (objects);

	g_slice_free (struct _query_msg, msg);
}

static void
query_objects_modified_cb (ECalClientView *view,
                           const GSList *objects,
                           gpointer data)
{
	struct _query_msg *msg;

	msg = g_slice_new0 (struct _query_msg);
	msg->header.func = (MessageFunc) query_objects_changed_async;
	msg->objects = duplicate_ical (objects);
	msg->data = data;

	message_push ((Message *) msg);
}

/* Called when a calendar component is removed; we must delete its corresponding
 * alarms.
 */
static void
query_objects_removed_async (struct _query_msg *msg)
{
	ClientAlarms *ca;
	GSList *l;
	GSList *objects;

	ca = msg->data;
	objects = msg->objects;

	debug (("Removing %d objects", g_slist_length (objects)));

	for (l = objects; l != NULL; l = l->next) {
		/* If the alarm is already triggered remove it. */
		tray_list_remove_cqa (lookup_comp_queued_alarms (ca, l->data));
		remove_comp (ca, l->data);
		g_hash_table_remove (ca->uid_alarms_hash, l->data);
		e_cal_component_free_id (l->data);
	}

	g_slist_free (objects);

	g_slice_free (struct _query_msg, msg);
}

static void
query_objects_removed_cb (ECalClientView *view,
                          const GSList *uids,
                          gpointer data)
{
	struct _query_msg *msg;

	msg = g_slice_new0 (struct _query_msg);
	msg->header.func = (MessageFunc) query_objects_removed_async;
	msg->objects = duplicate_ecal (uids);
	msg->data = data;

	message_push ((Message *) msg);
}

/* Notification functions */

/* Creates a snooze alarm based on an existing one.  The snooze offset is
 * compued with respect to the current time.
 */
static void
create_snooze (CompQueuedAlarms *cqa,
               gpointer alarm_id,
               gint snooze_mins)
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
		debug (("Unable to schedule trigger for %s", e_ctime (&t)));
		return;
	}

	orig_qa->instance->trigger = t;
	orig_qa->alarm_id = new_id;
	orig_qa->snooze = TRUE;
	#ifdef HAVE_LIBNOTIFY
	if (orig_qa->notify) {
		notify_notification_close (orig_qa->notify, NULL);
		g_clear_object (&orig_qa->notify);
	}
	#endif
	debug (("Adding an alarm at %s", e_ctime (&t)));
}

/* Launches a component editor for a component */
static void
edit_component (ECalClient *cal_client,
                ECalComponent *comp)
{
	ESource *source;
	gchar *command_line;
	const gchar *scheme;
	const gchar *comp_uid;
	const gchar *source_uid;
	GError *error = NULL;

	/* XXX Don't we have a function to construct these URIs?
	 *     How are other apps expected to know this stuff? */

	source = e_client_get_source (E_CLIENT (cal_client));
	source_uid = e_source_get_uid (source);

	e_cal_component_get_uid (comp, &comp_uid);

	switch (e_cal_client_get_source_type (cal_client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			scheme = "calendar:";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			scheme = "task:";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			scheme = "memo:";
			break;
		default:
			g_return_if_reached ();
	}

	command_line = g_strdup_printf (
		"%s %s///?source-uid=%s&comp-uid=%s",
		PACKAGE, scheme, source_uid, comp_uid);

	if (!g_spawn_command_line_async (command_line, &error)) {
		g_critical ("%s", error->message);
		g_error_free (error);
	}

	g_free (command_line);
}

static void
print_component (ECalClient *cal_client,
                 ECalComponent *comp)
{
	print_comp (
		comp,
		cal_client,
		config_data_get_timezone (),
		config_data_get_24_hour_format (),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

typedef struct {
	gchar *summary;
	gchar *description;
	gchar *location;
	gboolean blink_state;
	gboolean snooze_set;
	gint blink_id;
	time_t trigger;
	CompQueuedAlarms *cqa;
	gpointer alarm_id;
	ECalComponent *comp;
	ECalClient *cal_client;
	ECalClientView *view;
	GdkPixbuf *image;
	GtkTreeIter iter;
	gboolean is_in_tree;
} TrayIconData;

static void
free_tray_icon_data (TrayIconData *tray_data)
{
	g_return_if_fail (tray_data != NULL);

	if (tray_data->summary) {
		g_free (tray_data->summary);
		tray_data->summary = NULL;
	}

	if (tray_data->description) {
		g_free (tray_data->description);
		tray_data->description = NULL;
	}

	if (tray_data->location) {
		g_free (tray_data->location);
		tray_data->location = NULL;
	}

	g_object_unref (tray_data->cal_client);
	tray_data->cal_client = NULL;

	g_signal_handlers_disconnect_matched (
		tray_data->view, G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_dialog_objs_removed_cb, NULL);
	g_object_unref (tray_data->view);
	tray_data->view = NULL;

	g_object_unref (tray_data->comp);
	tray_data->comp = NULL;

	tray_data->cqa = NULL;
	tray_data->alarm_id = NULL;
	tray_data->image = NULL;

	g_free (tray_data);
}

static void
on_dialog_objs_removed_async (struct _query_msg *msg)
{
	TrayIconData *tray_data;
	GSList *l, *objects;
	ECalComponentId *our_id;

	debug (("..."));

	tray_data = msg->data;
	objects = msg->objects;

	our_id = e_cal_component_get_id (tray_data->comp);
	g_return_if_fail (our_id);

	for (l = objects; l != NULL; l = l->next) {
		ECalComponentId *id = l->data;

		if (!id)
			continue;

		if (tray_data &&
		    g_strcmp0 (id->uid, our_id->uid) == 0 &&
		    g_strcmp0 (id->rid, our_id->rid) == 0) {
			tray_data->cqa = NULL;
			tray_data->alarm_id = NULL;
			tray_icons_list = g_list_remove (
				tray_icons_list, tray_data);
			tray_data = NULL;
		}

		e_cal_component_free_id (id);
	}

	e_cal_component_free_id (our_id);
	g_slist_free (objects);
	g_slice_free (struct _query_msg, msg);
}

static void
on_dialog_objs_removed_cb (ECalClientView *view,
                           const GSList *uids,
                           gpointer data)
{
	struct _query_msg *msg;

	msg = g_slice_new0 (struct _query_msg);
	msg->header.func = (MessageFunc) on_dialog_objs_removed_async;
	msg->objects = duplicate_ecal (uids);
	msg->data = data;

	message_push ((Message *) msg);
}

struct _tray_cqa_msg {
	Message header;
	CompQueuedAlarms *cqa;
};

static void
tray_list_remove_cqa_async (struct _tray_cqa_msg *msg)
{
	CompQueuedAlarms *cqa = msg->cqa;
	GList *list = tray_icons_list;

	debug (("Removing CQA %p from tray list", cqa));

	while (list) {
		TrayIconData *tray_data = list->data;
		GList *tmp = list;
		GtkTreeModel *model;

		list = list->next;
		if (tray_data->cqa == cqa) {
			debug (("Found"));
			tray_icons_list = g_list_delete_link (tray_icons_list, tmp);
			if (alarm_notifications_dialog && tray_data->is_in_tree) {
				model = gtk_tree_view_get_model (
					GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
				gtk_list_store_remove (GTK_LIST_STORE (model), &(tray_data->iter));
				tray_data->is_in_tree = FALSE;
			}
			free_tray_icon_data (tray_data);
		}
	}

	debug (("%d alarms left", g_list_length (tray_icons_list)));

	if (alarm_notifications_dialog) {
		if (!g_list_length (tray_icons_list)) {
			gtk_widget_destroy (alarm_notifications_dialog->dialog);
			g_free (alarm_notifications_dialog);
			alarm_notifications_dialog = NULL;
		} else {
			GtkTreeIter iter;
			GtkTreeModel *model;
			GtkTreeSelection *sel;

			model = gtk_tree_view_get_model (
				GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
			if (gtk_tree_model_get_iter_first (model, &iter)) {
				sel = gtk_tree_view_get_selection (
					GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
				gtk_tree_selection_select_iter (sel, &iter);
			}
		}
	}

	g_slice_free (struct _tray_cqa_msg, msg);
}

static void
tray_list_remove_cqa (CompQueuedAlarms *cqa)
{
	struct _tray_cqa_msg *msg;

	msg = g_slice_new0 (struct _tray_cqa_msg);
	msg->header.func = (MessageFunc) tray_list_remove_cqa_async;
	msg->cqa = cqa;

	message_push ((Message *) msg);
}

/* Callback used from the alarm notify dialog */
static void
tray_list_remove_async (Message *msg)
{
	GList *list = tray_icons_list;

	debug (("Removing %d alarms", g_list_length (list)));
	while (list != NULL) {

		TrayIconData *tray_data = list->data;

		if (!tray_data->snooze_set) {
			GList *temp = list->next;
			gboolean status;

			tray_icons_list = g_list_remove_link (tray_icons_list, list);
			if (alarm_notifications_dialog && tray_data->is_in_tree) {
				GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
				gtk_list_store_remove (GTK_LIST_STORE (model), &tray_data->iter);
				tray_data->is_in_tree = FALSE;
			}
			status = remove_queued_alarm (
				tray_data->cqa,
				tray_data->alarm_id, FALSE, TRUE);
			if (status) {
				g_hash_table_remove (
					tray_data->cqa->parent_client->uid_alarms_hash,
					tray_data->cqa->id);
				e_cal_component_free_id (tray_data->cqa->id);
				g_free (tray_data->cqa);
			}
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

	g_slice_free (Message, msg);
}

static void
tray_list_remove_icons (void)
{
	Message *msg;

	msg = g_slice_new0 (Message);
	msg->func = tray_list_remove_async;

	message_push (msg);
}

struct _tray_msg {
	Message header;
	TrayIconData *data;
};

static void
tray_list_remove_data_async (struct _tray_msg *msg)
{
	TrayIconData *tray_data = msg->data;

	debug (("Removing %p from tray list", tray_data));

	tray_icons_list = g_list_remove_all (tray_icons_list, tray_data);
	free_tray_icon_data (tray_data);
	tray_data = NULL;

	g_slice_free (struct _tray_msg, msg);
}

static void
tray_list_remove_data (TrayIconData *data)
{
	struct _tray_msg *msg;

	msg = g_slice_new0 (struct _tray_msg);
	msg->header.func = (MessageFunc) tray_list_remove_data_async;
	msg->data = data;

	message_push ((Message *) msg);
}

static void
notify_dialog_cb (AlarmNotifyResult result,
                  gint snooze_mins,
                  gpointer data)
{
	TrayIconData *tray_data = data;

	debug (("Received from dialog"));

	g_signal_handlers_disconnect_matched (
		tray_data->view, G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_dialog_objs_removed_cb, NULL);

	switch (result) {
	case ALARM_NOTIFY_SNOOZE:
		debug (("Creating a snooze"));
		create_snooze (tray_data->cqa, tray_data->alarm_id, snooze_mins);
		tray_data->snooze_set = TRUE;
		if (alarm_notifications_dialog) {
			GtkTreeSelection *selection =
				gtk_tree_view_get_selection (
					GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
			GtkTreeIter iter;
			GtkTreeModel *model = NULL;

			/* We can also use tray_data->iter */
			if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				tray_data->is_in_tree = FALSE;
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
		tray_list_remove_data (tray_data);

		break;

	case ALARM_NOTIFY_EDIT:
		edit_component (tray_data->cal_client, tray_data->comp);

		break;

	case ALARM_NOTIFY_PRINT:
		print_component (tray_data->cal_client, tray_data->comp);

		break;

	case ALARM_NOTIFY_DISMISS:
		if (alarm_notifications_dialog && tray_data->is_in_tree) {
			GtkTreeModel *model;

			model = gtk_tree_view_get_model (
				GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
			gtk_list_store_remove (GTK_LIST_STORE (model), &tray_data->iter);
			tray_data->is_in_tree = FALSE;
		}
		break;

	case ALARM_NOTIFY_CLOSE:
		debug (("Dialog close"));
		if (alarm_notifications_dialog) {
			GtkTreeIter iter;
			GtkTreeModel *model =
				gtk_tree_view_get_model (
					GTK_TREE_VIEW (alarm_notifications_dialog->treeview));
			gboolean valid = gtk_tree_model_get_iter_first (model, &iter);

			/* Maybe we should warn about this first? */
			while (valid) {
				valid = gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
			}

			gtk_widget_destroy (alarm_notifications_dialog->dialog);
			g_free (alarm_notifications_dialog);
			alarm_notifications_dialog = NULL;

			/* Task to remove the tray icons */
			tray_list_remove_icons ();
		}

		break;

	default:
		g_return_if_reached ();
	}

	return;
}

static void
remove_tray_icon (void)
{
	if (tray_blink_id > -1)
		g_source_remove (tray_blink_id);
	tray_blink_id = -1;

	if (tray_icon) {
		gtk_status_icon_set_visible (tray_icon, FALSE);
		g_object_unref (tray_icon);
		tray_icon = NULL;
	}
}

/* Callbacks.  */
static gboolean
open_alarm_dialog (TrayIconData *tray_data)
{
	QueuedAlarm *qa;

	debug (("..."));
	qa = lookup_queued_alarm (tray_data->cqa, tray_data->alarm_id);
	if (qa) {
		gboolean is_first = !alarm_notifications_dialog;

		remove_tray_icon ();

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

			tray_data->is_in_tree = TRUE;

			if (is_first)
				gtk_tree_selection_select_iter (selection, &tray_data->iter);

			gtk_window_present (GTK_WINDOW (alarm_notifications_dialog->dialog));
		}

	} else {
		remove_tray_icon ();
	}

	return TRUE;
}

static gint
tray_icon_clicked_cb (GtkWidget *widget,
                      GdkEvent *event,
                      gpointer user_data)
{
	if (event->type == GDK_BUTTON_PRESS) {
		guint event_button = 0;

		debug (("left click and %d alarms", g_list_length (tray_icons_list)));

		gdk_event_get_button (event, &event_button);
		if (event_button == 1 && g_list_length (tray_icons_list) > 0) {
			GList *tmp;
			for (tmp = tray_icons_list; tmp; tmp = tmp->next) {
				open_alarm_dialog (tmp->data);
			}

			return TRUE;
		} else if (event_button == 3) {
			debug (("right click"));

			remove_tray_icon ();
			return TRUE;
		}
	}

	return FALSE;
}

static void
icon_activated (GtkStatusIcon *icon)
{
	GdkEvent event;

	event.type = GDK_BUTTON_PRESS;
	event.button.button = 1;
	event.button.time = gtk_get_current_event_time ();

	tray_icon_clicked_cb (NULL, &event, NULL);
}

static void
popup_menu (GtkStatusIcon *icon,
            guint button,
            guint activate_time)
{
	if (button == 3) {
		/* right click */
		GdkEvent event;

		event.type = GDK_BUTTON_PRESS;
		event.button.button = 3;
		event.button.time = gtk_get_current_event_time ();

		tray_icon_clicked_cb (NULL, &event, NULL);
	}
}

static gboolean
tray_icon_blink_cb (gpointer data)
{
	static gboolean tray_blink_state = FALSE;
	const gchar *icon_name;

	tray_blink_countdown--;
	tray_blink_state = !tray_blink_state;

	if (tray_blink_state || tray_blink_countdown <= 0)
		icon_name = "appointment-missed";
	else
		icon_name = "appointment-soon";

	if (tray_icon)
		gtk_status_icon_set_from_icon_name (tray_icon, icon_name);

	if (tray_blink_countdown <= 0)
		tray_blink_id = -1;

	return tray_blink_countdown > 0;
}

/* Add a new data to tray list */

static void
tray_list_add_async (struct _tray_msg *msg)
{
	tray_icons_list = g_list_prepend (tray_icons_list, msg->data);

	g_slice_free (struct _tray_msg, msg);
}

static void
tray_list_add_new (TrayIconData *data)
{
	struct _tray_msg *msg;

	msg = g_slice_new0 (struct _tray_msg);
	msg->header.func = (MessageFunc) tray_list_add_async;
	msg->data = data;

	message_push ((Message *) msg);
}

static gchar *
alarm_queue_get_alarm_summary (ECalClient *cal_client,
                               ECalComponent *comp,
                               const ECalComponentAlarmInstance *instance)
{
	ECalComponentAlarm *alarm = NULL;
	ECalComponentText summary_text, alarm_text;
	gchar *alarm_summary;

	g_return_val_if_fail (comp != NULL, NULL);
	g_return_val_if_fail (instance != NULL, NULL);
	g_return_val_if_fail (instance->auid != NULL, NULL);

	summary_text.value = NULL;
	alarm_text.value = NULL;

	e_cal_component_get_summary (comp, &summary_text);

	if (e_client_check_capability (E_CLIENT (cal_client), CAL_STATIC_CAPABILITY_ALARM_DESCRIPTION)) {
		alarm = e_cal_component_get_alarm (comp, instance->auid);
		if (alarm) {
			e_cal_component_alarm_get_description (alarm, &alarm_text);
			if (!alarm_text.value || !*alarm_text.value)
				alarm_text.value = NULL;
		}
	}

	if (alarm_text.value && summary_text.value &&
	    e_util_utf8_strcasecmp (alarm_text.value, summary_text.value) == 0)
		alarm_text.value = NULL;

	if (summary_text.value && *summary_text.value &&
	    alarm_text.value && *alarm_text.value)
		alarm_summary = g_strconcat (summary_text.value, "\n", alarm_text.value, NULL);
	else if (summary_text.value && *summary_text.value)
		alarm_summary = g_strdup (summary_text.value);
	else if (alarm_text.value && *alarm_text.value)
		alarm_summary = g_strdup (alarm_text.value);
	else
		alarm_summary = NULL;

	if (alarm)
		e_cal_component_alarm_free (alarm);

	return alarm_summary;
}

/* Performs notification of a display alarm */
static void
display_notification (time_t trigger,
                      CompQueuedAlarms *cqa,
                      gpointer alarm_id,
                      gboolean use_description)
{
	QueuedAlarm *qa;
	ECalComponent *comp;
	const gchar *summary, *description, *location;
	gchar *alarm_summary;
	TrayIconData *tray_data;
	ECalComponentText text;
	GSList *text_list;
	gchar *str, *start_str, *end_str, *alarm_str, *time_str;
	icaltimezone *current_zone;
	ECalComponentOrganizer organiser;

	debug (("..."));

	comp = cqa->alarms->comp;
	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;

	/* get a sensible description for the event */
	alarm_summary = alarm_queue_get_alarm_summary (cqa->parent_client->cal_client, comp, qa->instance);
	e_cal_component_get_organizer (comp, &organiser);

	if (alarm_summary && *alarm_summary)
		summary = alarm_summary;
	else
		summary = _("No summary available.");

	e_cal_component_get_description_list (comp, &text_list);

	if (text_list) {
		text = *((ECalComponentText *) text_list->data);
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
	if (!tray_icon && !e_util_is_running_gnome ()) {
		tray_icon = gtk_status_icon_new ();
		gtk_status_icon_set_title (tray_icon, _("Evolution Reminders"));
		gtk_status_icon_set_from_icon_name (
			tray_icon, "appointment-soon");
		g_signal_connect (
			tray_icon, "activate",
			G_CALLBACK (icon_activated), NULL);
		g_signal_connect (
			tray_icon, "popup-menu",
			G_CALLBACK (popup_menu), NULL);
	}

	current_zone = config_data_get_timezone ();
	alarm_str = timet_to_str_with_zone (trigger, current_zone);
	start_str = timet_to_str_with_zone (qa->instance->occur_start, current_zone);
	end_str = timet_to_str_with_zone (qa->instance->occur_end, current_zone);
	time_str = calculate_time (qa->instance->occur_start, qa->instance->occur_end);

	str = g_strdup_printf (
		"%s\n%s %s",
		summary, start_str, time_str);

	/* create the private structure */
	tray_data = g_new0 (TrayIconData, 1);
	tray_data->summary = g_strdup (summary);
	tray_data->description = g_strdup (description);
	tray_data->location = g_strdup (location);
	tray_data->trigger = trigger;
	tray_data->cqa = cqa;
	tray_data->alarm_id = alarm_id;
	tray_data->comp = e_cal_component_clone (comp);
	tray_data->cal_client = cqa->parent_client->cal_client;
	tray_data->view = g_object_ref (cqa->parent_client->view);
	tray_data->blink_state = FALSE;
	tray_data->snooze_set = FALSE;
	tray_data->is_in_tree = FALSE;
	g_object_ref (tray_data->cal_client);

	/* Task to add tray_data to the global tray_icon_list */
	tray_list_add_new (tray_data);

	if (g_list_length (tray_icons_list) > 1) {
		gchar *tip;

		tip = g_strdup_printf (ngettext (
			"You have %d reminder", "You have %d reminders",
			g_list_length (tray_icons_list)),
			g_list_length (tray_icons_list));
		if (tray_icon)
			gtk_status_icon_set_tooltip_text (tray_icon, tip);
	} else if (tray_icon) {
		gtk_status_icon_set_tooltip_text (tray_icon, str);
	}

	g_free (alarm_summary);
	g_free (start_str);
	g_free (end_str);
	g_free (alarm_str);
	g_free (time_str);
	g_free (str);

	g_signal_connect (
		tray_data->view, "objects_removed",
		G_CALLBACK (on_dialog_objs_removed_cb), tray_data);

	/* FIXME: We should remove this check */
	if (!config_data_get_notify_with_tray ()) {
		tray_blink_id = -1;
		open_alarm_dialog (tray_data);
		if (alarm_notifications_dialog)
			gtk_window_stick (GTK_WINDOW (
				alarm_notifications_dialog->dialog));
	} else {
		if (tray_blink_id == -1 && tray_icon) {
			tray_blink_countdown = 30;
			tray_blink_id = e_named_timeout_add (
				500, tray_icon_blink_cb, tray_data);
		}
	}
}

#ifdef HAVE_LIBNOTIFY

static void
notify_open_appointments_cb (NotifyNotification *notification,
			     gchar *action,
			     gpointer user_data)
{
	GdkEvent event;

	notify_notification_close (notification, NULL);

	event.type = GDK_BUTTON_PRESS;
	event.button.button = 1;
	event.button.time = gtk_get_current_event_time ();

	tray_icon_clicked_cb (NULL, &event, NULL);
}

static void
popup_notification (time_t trigger,
                    CompQueuedAlarms *cqa,
                    gpointer alarm_id,
                    gboolean use_description)
{
	QueuedAlarm *qa;
	ECalComponent *comp;
	const gchar *summary, *location;
	gchar *alarm_summary;
	gchar *str, *start_str, *end_str, *alarm_str, *time_str;
	icaltimezone *current_zone;
	ECalComponentOrganizer organiser;
	gchar *body;
	GError *error = NULL;

	debug (("..."));

	comp = cqa->alarms->comp;
	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;
	if (!notify_is_initted ())
		notify_init (_("Evolution Reminders"));

	/* get a sensible description for the event */
	alarm_summary = alarm_queue_get_alarm_summary (cqa->parent_client->cal_client, comp, qa->instance);
	e_cal_component_get_organizer (comp, &organiser);

	if (alarm_summary && *alarm_summary)
		summary = alarm_summary;
	else
		summary = _("No summary available.");

	e_cal_component_get_location (comp, &location);

	/* create the tray icon */

	current_zone = config_data_get_timezone ();
	alarm_str = timet_to_str_with_zone (trigger, current_zone);
	start_str = timet_to_str_with_zone (qa->instance->occur_start, current_zone);
	end_str = timet_to_str_with_zone (qa->instance->occur_end, current_zone);
	time_str = calculate_time (qa->instance->occur_start, qa->instance->occur_end);

	str = g_strdup_printf (
		"%s %s",
		start_str, time_str);

	if (organiser.cn) {
		if (location)
			body = g_strdup_printf (
				"<b>%s</b>\n%s %s\n%s %s",
				organiser.cn, _("Location:"),
				location, start_str, time_str);
		else
			body = g_strdup_printf (
				"<b>%s</b>\n%s %s",
				organiser.cn, start_str, time_str);
	}
	else {
		if (location)
			body = g_strdup_printf (
				"%s %s\n%s %s", _("Location:"),
				location, start_str, time_str);
		else
			body = g_strdup_printf (
				"%s %s", start_str, time_str);
	}

	if (qa->notify) {
		notify_notification_close (qa->notify, NULL);
		g_clear_object (&qa->notify);
	}

	qa->notify = notify_notification_new (summary, body, "appointment-soon");

	/* If the user wants Evolution notifications suppressed, honor
	 * it even though evolution-alarm-notify is a separate process
	 * with its own .desktop file. */
	notify_notification_set_hint (
		qa->notify, "desktop-entry",
		g_variant_new_string (PACKAGE));

	notify_notification_set_hint (
		qa->notify, "sound-name",
		g_variant_new_string ("alarm-clock-elapsed"));

	notify_notification_add_action (
		qa->notify, "open-appointments", _("Appointments"),
		notify_open_appointments_cb, NULL, NULL);

	if (!notify_notification_show (qa->notify, &error))
		g_warning ("Could not send notification to daemon: %s\n", error ? error->message : "Unknown error");

	g_clear_error (&error);
	g_free (alarm_summary);
	g_free (start_str);
	g_free (end_str);
	g_free (alarm_str);
	g_free (time_str);
	g_free (str);

}
#endif

/* Performs notification of an audio alarm */
static void
audio_notification (time_t trigger,
                    CompQueuedAlarms *cqa,
                    gpointer alarm_id)
{
	QueuedAlarm *qa;
	ECalComponent *comp;
	ECalComponentAlarm *alarm;
	icalattach *attach;
	gint	flag = 0;

	debug (("..."));

	comp = cqa->alarms->comp;
	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;

	alarm = e_cal_component_get_alarm (comp, qa->instance->auid);
	g_return_if_fail (alarm != NULL);

	e_cal_component_alarm_get_attach (alarm, &attach);
	e_cal_component_alarm_free (alarm);

	if (attach && icalattach_get_is_url (attach)) {
		const gchar *url;

		url = icalattach_get_url (attach);
		if (url && *url) {
			gchar *filename;
			GError *error = NULL;

			filename = g_filename_from_uri (url, NULL, &error);

			if (error != NULL) {
				g_warning ("%s: %s", G_STRFUNC, error->message);
				g_error_free (error);
			} else if (filename && g_file_test (filename, G_FILE_TEST_EXISTS)) {
#ifdef HAVE_CANBERRA
				flag = 1;
				ca_context_play (
					ca_gtk_context_get (), 0,
					CA_PROP_MEDIA_FILENAME, filename, NULL);
#endif
			}

			g_free (filename);
		}
	}

	if (!flag)
		gdk_beep ();

	if (attach)
		icalattach_unref (attach);

}

/* Performs notification of a mail alarm */
static void
mail_notification (time_t trigger,
                   CompQueuedAlarms *cqa,
                   gpointer alarm_id)
{
	if (!e_client_check_capability (
		E_CLIENT (cqa->parent_client->cal_client),
		CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS))
		return;

	/* FIXME Implement this. */
}

/* Performs notification of a procedure alarm */
static gboolean
procedure_notification_dialog (const gchar *cmd,
                               const gchar *url)
{
	GtkWidget *container;
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *checkbox;
	gchar *str;
	gint btn;

	debug (("..."));

	if (config_data_is_blessed_program (url))
		return TRUE;

	dialog = gtk_dialog_new_with_buttons (
		_("Warning"), NULL, 0,
		_("_No"), GTK_RESPONSE_CANCEL,
		_("_Yes"), GTK_RESPONSE_OK,
		NULL);

	str = g_strdup_printf (
		_("An Evolution Calendar reminder is about to trigger. "
		"This reminder is configured to run the following program:\n\n"
		"        %s\n\n"
		"Are you sure you want to run this program?"),
		cmd);
	label = gtk_label_new (str);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (label);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (container), label, TRUE, TRUE, 4);
	g_free (str);

	checkbox = gtk_check_button_new_with_label
		(_("Do not ask me about this program again."));
	gtk_widget_show (checkbox);
	gtk_box_pack_start (GTK_BOX (container), checkbox, TRUE, TRUE, 4);

	/* Run the dialog */
	btn = gtk_dialog_run (GTK_DIALOG (dialog));
	if (btn == GTK_RESPONSE_OK &&
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox)))
		config_data_save_blessed_program (url);
	gtk_widget_destroy (dialog);

	return (btn == GTK_RESPONSE_OK);
}

static void
procedure_notification (time_t trigger,
                        CompQueuedAlarms *cqa,
                        gpointer alarm_id)
{
	QueuedAlarm *qa;
	ECalComponent *comp;
	ECalComponentAlarm *alarm;
	ECalComponentText description;
	icalattach *attach;
	const gchar *url;
	gchar *cmd;
	gboolean result = TRUE;

	debug (("..."));

	comp = cqa->alarms->comp;
	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;

	alarm = e_cal_component_get_alarm (comp, qa->instance->auid);
	g_return_if_fail (alarm != NULL);

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
	g_return_if_fail (url != NULL);

	/* Ask for confirmation before executing the stuff */
	if (description.value)
		cmd = g_strconcat (url, " ", description.value, NULL);
	else
		cmd = (gchar *) url;

	if (procedure_notification_dialog (cmd, url))
		result = g_spawn_command_line_async (cmd, NULL);

	if (cmd != (gchar *) url)
		g_free (cmd);

	icalattach_unref (attach);

	/* Fall back to display notification if we got an error */
	if (result == FALSE)
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

	debug (("..."));

	zone = config_data_get_timezone ();
	new_midnight = time_day_end_with_zone (time (NULL), zone);

	if (new_midnight > midnight) {
		struct _midnight_refresh_msg *msg;

		msg = g_slice_new0 (struct _midnight_refresh_msg);
		msg->header.func = (MessageFunc) midnight_refresh_async;
		msg->remove = FALSE;

		message_push ((Message *) msg);
	}

	return TRUE;
}

static gboolean
check_wall_clock_time_changed (gpointer user_data)
{
	static gint64 expected_wall_clock_time = 0;
	gint64 wall_clock_time;

	#define ADD_SECONDS(to, secs) ((to) + ((secs) * 1000000))

	wall_clock_time = g_get_real_time ();

	/* use one second margin */
	if (wall_clock_time > ADD_SECONDS (expected_wall_clock_time, 1) ||
	    wall_clock_time < ADD_SECONDS (expected_wall_clock_time, -1)) {
		debug (("Current wall-clock time differs from expected, rescheduling alarms"));
		check_midnight_refresh (NULL);
		alarm_reschedule_timeout ();
	}

	expected_wall_clock_time = ADD_SECONDS (wall_clock_time, 60);

	#undef ADD_SECONDS

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

	debug (("..."));

	client_alarms_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	queue_midnight_refresh ();

	if (config_data_get_last_notification_time (NULL) == -1) {
		time_t tmval = time_day_begin (time (NULL));
		debug (("Setting last notification time to %s", e_ctime (&tmval)));
		config_data_set_last_notification_time (NULL, tmval);
	}

	/* Install timeout handler (every 30 mins) for not missing the
	 * midnight refresh. */
	e_named_timeout_add_seconds (1800, check_midnight_refresh, NULL);

	/* Monotonic time doesn't change during hibernation, while the
	 * wall clock time does, thus check for wall clock time changes
	 * and reschedule alarms when it changes. */
	e_named_timeout_add_seconds (60, check_wall_clock_time_changed, NULL);

#ifdef HAVE_LIBNOTIFY
	notify_init (_("Evolution Reminders"));
#endif

	alarm_queue_inited = TRUE;
}

static gboolean
free_client_alarms_cb (gpointer key,
                       gpointer value,
                       gpointer user_data)
{
	ClientAlarms *ca = value;

	debug (("ca=%p", ca));

	if (ca) {
		remove_client_alarms (ca);
		if (ca->cal_client) {
			debug (("Disconnecting Client"));

			g_signal_handlers_disconnect_matched (
				ca->cal_client, G_SIGNAL_MATCH_DATA,
				0, 0, NULL, NULL, ca);
			g_object_unref (ca->cal_client);
		}

		if (ca->view) {
			debug (("Disconnecting Query"));

			g_signal_handlers_disconnect_matched (
				ca->view, G_SIGNAL_MATCH_DATA,
				0, 0, NULL, NULL, ca);
			g_object_unref (ca->view);
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

	debug (("..."));

	g_hash_table_foreach_remove (
		client_alarms_hash, (GHRFunc) free_client_alarms_cb, NULL);
	g_hash_table_destroy (client_alarms_hash);
	client_alarms_hash = NULL;

	if (midnight_refresh_id != NULL) {
		alarm_remove (midnight_refresh_id);
		midnight_refresh_id = NULL;
	}

	alarm_queue_inited = FALSE;
}

static gboolean
compare_ids (gpointer a,
             gpointer b)
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
	Message header;
	ECalClient *cal_client;
};

static void
alarm_queue_add_async (struct _alarm_client_msg *msg)
{
	ClientAlarms *ca;
	ECalClient *cal_client = msg->cal_client;

	g_return_if_fail (alarm_queue_inited);
	g_return_if_fail (cal_client != NULL);
	g_return_if_fail (E_IS_CAL_CLIENT (cal_client));

	ca = lookup_client (cal_client);
	if (ca) {
		/* We already have it. Unref the passed one*/
		g_object_unref (cal_client);
		return;
	}

	debug (("client=%p", cal_client));

	ca = g_new (ClientAlarms, 1);

	ca->cal_client = cal_client;
	ca->view = NULL;

	g_hash_table_insert (client_alarms_hash, cal_client, ca);

	ca->uid_alarms_hash = g_hash_table_new (
		(GHashFunc) hash_ids, (GEqualFunc) compare_ids);

	load_alarms_for_today (ca);

	g_slice_free (struct _alarm_client_msg, msg);
}

/**
 * alarm_queue_add_client:
 * @cal_client: A calendar client.
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
alarm_queue_add_client (ECalClient *cal_client)
{
	struct _alarm_client_msg *msg;

	msg = g_slice_new0 (struct _alarm_client_msg);
	msg->header.func = (MessageFunc) alarm_queue_add_async;
	msg->cal_client = g_object_ref (cal_client);

	message_push ((Message *) msg);
}

/* Removes a component an its alarms */
static void
remove_cqa (ClientAlarms *ca,
            ECalComponentId *id,
            CompQueuedAlarms *cqa)
{

	/* If a component is present, then it means we must have alarms queued
	 * for it.
	 */
	g_return_if_fail (cqa->queued_alarms != NULL);

	debug (("removing %d alarms", g_slist_length (cqa->queued_alarms)));
	remove_alarms (cqa, TRUE);
}

static gboolean
remove_comp_by_id (gpointer key,
                   gpointer value,
                   gpointer userdata)
{

	ClientAlarms *ca = (ClientAlarms *) userdata;

	debug (("..."));

/*	if (!g_hash_table_size (ca->uid_alarms_hash)) */
/*		return; */

	remove_cqa (ca, (ECalComponentId *) key, (CompQueuedAlarms *) value);

	return TRUE;
}

/* Removes all the alarms queued for a particular calendar client */
static void
remove_client_alarms (ClientAlarms *ca)
{
	debug (("size %d", g_hash_table_size (ca->uid_alarms_hash)));

	g_hash_table_foreach_remove  (
		ca->uid_alarms_hash, (GHRFunc) remove_comp_by_id, ca);

	/* The hash table should be empty now */
	g_return_if_fail (g_hash_table_size (ca->uid_alarms_hash) == 0);
}

/**
 * alarm_queue_remove_client:
 * @client: A calendar client.
 *
 * Removes a calendar client from the alarm queueing system.
 **/
static void
alarm_queue_remove_async (struct _alarm_client_msg *msg)
{
	ClientAlarms *ca;
	ECalClient *cal_client = msg->cal_client;

	g_return_if_fail (alarm_queue_inited);
	g_return_if_fail (cal_client != NULL);
	g_return_if_fail (E_IS_CAL_CLIENT (cal_client));

	ca = lookup_client (cal_client);
	g_return_if_fail (ca != NULL);

	debug (("..."));
	remove_client_alarms (ca);

	/* Clean up */
	if (ca->cal_client) {
		debug (("Disconnecting Client"));

		g_signal_handlers_disconnect_matched (
			ca->cal_client, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, ca);
		g_object_unref (ca->cal_client);
		ca->cal_client = NULL;
	}

	if (ca->view) {
		debug (("Disconnecting Query"));

		g_signal_handlers_disconnect_matched (
			ca->view, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, ca);
		g_object_unref (ca->view);
		ca->view = NULL;
	}

	g_hash_table_destroy (ca->uid_alarms_hash);
	ca->uid_alarms_hash = NULL;

	g_free (ca);

	g_hash_table_remove (client_alarms_hash, cal_client);

	g_slice_free (struct _alarm_client_msg, msg);
}

/** alarm_queue_remove_client
 *
 * asynchronously remove client from alarm queue.
 * @cal_client: Client to remove.
 * @immediately: Indicates whether use thread or do it right now.
 */

void
alarm_queue_remove_client (ECalClient *cal_client,
                           gboolean immediately)
{
	struct _alarm_client_msg *msg;

	msg = g_slice_new0 (struct _alarm_client_msg);
	msg->header.func = (MessageFunc) alarm_queue_remove_async;
	msg->cal_client = cal_client;

	if (immediately) {
		alarm_queue_remove_async (msg);
	} else
		message_push ((Message *) msg);
}

/* Update non-time related variables for various structures on modification
 * of an existing component to be called only from query_objects_changed_cb */
static void
update_cqa (CompQueuedAlarms *cqa,
            ECalComponent *newcomp)
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

	debug (("Generating alarms between %s and %s", e_ctime (&from), e_ctime (&to)));
	alarms = e_cal_util_generate_alarms_for_comp (
		newcomp, from, to, omit, e_cal_client_resolve_tzid_cb,
		cqa->parent_client->cal_client, zone);

	/* Update auids in Queued Alarms*/
	for (qa_list = cqa->queued_alarms; qa_list; qa_list = qa_list->next) {
		QueuedAlarm *qa = qa_list->data;
		gchar *check_auid = (gchar *) qa->instance->auid;
		ECalComponentAlarm *alarm;

		alarm = e_cal_component_get_alarm (newcomp, check_auid);
		if (alarm) {
			e_cal_component_alarm_free (alarm);
			continue;
		} else {
			alarm = e_cal_component_get_alarm (oldcomp, check_auid);
			if (alarm) { /* Need to update QueuedAlarms */
				e_cal_component_alarm_free (alarm);
				if (alarms == NULL) {
					debug (("No alarms found in the modified component"));
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
	if (alarms != NULL)
		e_cal_component_alarms_free (alarms);
}

static void
update_qa (ECalComponentAlarms *alarms,
           QueuedAlarm *qa)
{
	ECalComponentAlarmInstance *al_inst;
	GSList *instance_list;

	debug (("..."));
	for (instance_list = alarms->alarms;
	     instance_list;
	     instance_list = instance_list->next) {
		al_inst = instance_list->data;
		/* FIXME If two or more alarm instances (audio, note)
		 *       for same component have same trigger... */
		if (al_inst->trigger == qa->orig_trigger) {
			g_free ((gchar *) qa->instance->auid);
			qa->instance->auid = g_strdup (al_inst->auid);
			break;
		}
	}
}
