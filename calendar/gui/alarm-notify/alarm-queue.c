/* Evolution calendar - Alarm notification engine
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <gtk/gtksignal.h>
#include <cal-util/timeutil.h>
#include "alarm.h"
#include "alarm-notify.h"



/* Whether the notification system has been initialized */
static gboolean alarm_notify_inited;

/* Clients we are monitoring for alarms */
static GHashTable *client_alarms_hash = NULL;

/* Structure that stores a client we are monitoring */
typedef struct {
	/* Monitored client */
	CalClient *client;

	/* Number of times this client has been registered */
	int refcount;

	/* Hash table of component UID -> CompQueuedAlarms.  If an element is
	 * present here, then it means its cqa->queued_alarms contains at least
	 * one queued alarm.  When all the alarms for a component have been
	 * dequeued, the CompQueuedAlarms structure is removed from the hash
	 * table.  Thus a CQA exists <=> it has queued alarms.
	 */
	GHashTable *uid_alarms_hash;
} ClientAlarms;

/* Pair of a CalComponentAlarms and the mapping from queued alarm IDs to the
 * actual alarm instance structures.
 */
typedef struct {
	/* The parent client alarms structure */
	ClientAlarms *parent_client;

	/* The actual component and its alarm instances */
	CalComponentAlarms *alarms;

	/* List of QueuedAlarm structures */
	GSList *queued_alarms;
} CompQueuedAlarms;

/* Pair of a queued alarm ID and the alarm trigger instance it refers to */
typedef struct {
	/* Alarm ID from alarm.h */
	gpointer alarm_id;

	/* Instance from our parent CompAlarms->alarms list */
	CalAlarmInstance *instance;
} QueuedAlarm;

/* Alarm ID for the midnight refresh function */
static gpointer midnight_refresh_id = NULL;



static void load_alarms (ClientAlarms *ca);
static void midnight_refresh_cb (gpointer alarm_id, time_t trigger, gpointer data);

/* Queues an alarm trigger for midnight so that we can load the next day's worth
 * of alarms.
 */
static void
queue_midnight_refresh (void)
{
	time_t midnight;

	g_assert (midnight_refresh_id == NULL);

	midnight = time_day_end (time (NULL));

	midnight_refresh_id = alarm_add (midnight, midnight_refresh_cb, NULL, NULL);
	if (!midnight_refresh_id) {
		g_message ("alarm_notify_init(): Could not set up the midnight refresh alarm!");
		/* FIXME: what to do? */
	}
}

/* Loads a client's alarms; called from g_hash_table_foreach() */
static void
add_client_alarms_cb (gpointer key, gpointer value, gpointer data)
{
	ClientAlarms *ca;

	ca = value;
	load_alarms (ca);
}

/* Loads the alarms for the new day every midnight */
static void
midnight_refresh_cb (gpointer alarm_id, time_t trigger, gpointer data)
{
	/* Re-load the alarms for all clients */

	g_hash_table_foreach (client_alarms_hash, add_client_alarms_cb, NULL);

	/* Re-schedule the midnight update */

	midnight_refresh_id = NULL;
	queue_midnight_refresh ();
}

/* Looks up a client in the client alarms hash table */
static ClientAlarms *
lookup_client (CalClient *client)
{
	return g_hash_table_lookup (client_alarms_hash, client);
}

/* Callback used when an alarm triggers */
static void
alarm_trigger_cb (gpointer alarm_id, time_t trigger, gpointer data)
{
	CompQueuedAlarms *cqa;

	cqa = data;

	/* FIXME */

	g_message ("alarm_trigger_cb(): Triggered!");
}

/* Callback used when an alarm must be destroyed */
static void
alarm_destroy_cb (gpointer alarm_id, gpointer data)
{
	CompQueuedAlarms *cqa;
	GSList *l;
	QueuedAlarm *qa;
	const char *uid;

	cqa = data;

	qa = NULL; /* Keep GCC happy */

	/* Find the alarm in the queued alarms */

	for (l = cqa->queued_alarms; l; l = l->next) {
		qa = l->data;
		if (qa->alarm_id == alarm_id)
			break;
	}

	g_assert (l != NULL);

	/* Remove it and free it */

	cqa->queued_alarms = g_slist_remove_link (cqa->queued_alarms, l);
	g_slist_free_1 (l);

	g_free (qa);

	/* If this was the last queued alarm for this component, remove the
	 * component itself.
	 */

	if (cqa->queued_alarms != NULL)
		return;

	cal_component_get_uid (cqa->alarms->comp, &uid);
	g_hash_table_remove (cqa->parent_client->uid_alarms_hash, uid);
	cqa->parent_client = NULL;

	cal_component_alarms_free (cqa->alarms);
	cqa->alarms = NULL;

	g_free (cqa);
}

/* Adds the alarms in a CalComponentAlarms structure to the alarms queued for a
 * particular client.  Also puts the triggers in the alarm timer queue.
 */
static void
add_component_alarms (ClientAlarms *ca, CalComponentAlarms *alarms)
{
	const char *uid;
	CompQueuedAlarms *cqa;
	GSList *l;

	cqa = g_new (CompQueuedAlarms, 1);
	cqa->parent_client = ca;
	cqa->alarms = alarms;

	cqa->queued_alarms = NULL;

	for (l = alarms->alarms; l; l = l->next) {
		CalAlarmInstance *instance;
		gpointer alarm_id;
		QueuedAlarm *qa;

		instance = l->data;

		alarm_id = alarm_add (instance->trigger, alarm_trigger_cb, cqa, alarm_destroy_cb);
		if (!alarm_id) {
			g_message ("add_component_alarms(): Could not schedule a trigger for "
				   "%ld, discarding...", (long) instance->trigger);
			continue;
		}

		qa = g_new (QueuedAlarm, 1);
		qa->alarm_id = alarm_id;
		qa->instance = instance;

		cqa->queued_alarms = g_slist_prepend (cqa->queued_alarms, qa);
	}

	cal_component_get_uid (alarms->comp, &uid);

	/* If we failed to add all the alarms, then we should get rid of the cqa */
	if (cqa->queued_alarms == NULL) {
		g_message ("add_component_alarms(): Could not add any of the alarms "
			   "for the component `%s'; discarding it...", uid);

		cal_component_alarms_free (cqa->alarms);
		cqa->alarms = NULL;

		g_free (cqa);
		return;
	}

	cqa->queued_alarms = g_slist_reverse (cqa->queued_alarms);
	g_hash_table_insert (ca->uid_alarms_hash, (char *) uid, cqa);
}

/* Loads today's remaining alarms for a client */
static void
load_alarms (ClientAlarms *ca)
{
	time_t now, day_end;
	GSList *comp_alarms;
	GSList *l;

	now = time (NULL);
	day_end = time_day_end (now);

	comp_alarms = cal_client_get_alarms_in_range (ca->client, now, day_end);

	/* All of the last day's alarms should have already triggered and should
	 * have been removed, so we should have no pending components.
	 */
	g_assert (g_hash_table_size (ca->uid_alarms_hash) == 0);

	for (l = comp_alarms; l; l = l->next) {
		CalComponentAlarms *alarms;

		alarms = l->data;
		add_component_alarms (ca, alarms);
	}

	g_slist_free (comp_alarms);
}

/* Called when a calendar client finished loading; we load its alarms */
static void
cal_loaded_cb (CalClient *client, CalClientLoadStatus status, gpointer data)
{
	ClientAlarms *ca;

	ca = data;

	if (status != CAL_CLIENT_LOAD_SUCCESS)
		return;

	load_alarms (ca);
}

/* Looks up a component's queued alarm structure in a client alarms structure */
static CompQueuedAlarms *
lookup_comp_queued_alarms (ClientAlarms *ca, const char *uid)
{
	return g_hash_table_lookup (ca->uid_alarms_hash, uid);
}

/* Removes a component an its alarms */
static void
remove_comp (ClientAlarms *ca, const char *uid)
{
	CompQueuedAlarms *cqa;
	GSList *l;

	cqa = lookup_comp_queued_alarms (ca, uid);
	if (!cqa)
		return;

	/* If a component is present, then it means we must have alarms queued
	 * for it.
	 */
	g_assert (cqa->queued_alarms != NULL);

	for (l = cqa->queued_alarms; l;) {
		QueuedAlarm *qa;

		qa = l->data;

		/* Get the next element here because the list element will go
		 * away.  Also, we do not free the qa here because it will be
		 * freed by the destroy notification function.
		 */
		l = l->next;

		alarm_remove (qa->alarm_id);
	}

	/* The list should be empty now, and thus the queued component alarms
	 * structure should have been freed and removed from the hash table.
	 */
	g_assert (lookup_comp_queued_alarms (ca, uid) == NULL);
}

/* Called when a calendar component changes; we must reload its corresponding
 * alarms.
 */
static void
obj_updated_cb (CalClient *client, const char *uid, gpointer data)
{
	ClientAlarms *ca;
	time_t now, day_end;
	CalComponentAlarms *alarms;
	gboolean found;

	ca = data;

	remove_comp (ca, uid);

	now = time (NULL);
	day_end = time_day_end (now);

	found = cal_client_get_alarms_for_object (ca->client, uid, now, day_end, &alarms);

	if (!found)
		return;

	add_component_alarms (ca, alarms);
}

/* Called when a calendar component is removed; we must delete its corresponding
 * alarms.
 */
static void
obj_removed_cb (CalClient *client, const char *uid, gpointer data)
{
	ClientAlarms *ca;

	ca = data;

	remove_comp (ca, uid);
}



/**
 * alarm_notify_init:
 *
 * Initializes the alarm notification system.  This should be called near the
 * beginning of the program, after calling alarm_init().
 **/
void
alarm_notify_init (void)
{
	g_return_if_fail (alarm_notify_inited == FALSE);

	client_alarms_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	queue_midnight_refresh ();

	alarm_notify_inited = TRUE;
}

/**
 * alarm_notify_done:
 *
 * Shuts down the alarm notification system.  This should be called near the end
 * of the program.  All the monitored calendar clients should already have been
 * unregistered with alarm_notify_remove_client().
 **/
void
alarm_notify_done (void)
{
	g_return_if_fail (alarm_notify_inited);

	/* All clients must be unregistered by now */
	g_return_if_fail (g_hash_table_size (client_alarms_hash) == 0);

	g_hash_table_destroy (client_alarms_hash);
	client_alarms_hash = NULL;

	g_assert (midnight_refresh_id != NULL);
	alarm_remove (midnight_refresh_id);
	midnight_refresh_id = NULL;

	alarm_notify_inited = FALSE;
}

/**
 * alarm_notify_add_client:
 * @client: A calendar client.
 *
 * Adds a calendar client to the alarm notification system.  Alarm trigger
 * notifications will be presented at the appropriate times.  The client should
 * be removed with alarm_notify_remove_client() when receiving notifications
 * from it is no longer desired.
 *
 * A client can be added any number of times to the alarm notification system,
 * but any single alarm trigger will only be presented once for a particular
 * client.  The client must still be removed the same number of times from the
 * notification system when it is no longer wanted.
 **/
void
alarm_notify_add_client (CalClient *client)
{
	ClientAlarms *ca;

	g_return_if_fail (alarm_notify_inited);
	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_CAL_CLIENT (client));

	ca = lookup_client (client);
	if (ca) {
		ca->refcount++;
		return;
	}

	ca = g_new (ClientAlarms, 1);

	ca->client = client;
	gtk_object_ref (GTK_OBJECT (ca->client));

	ca->refcount = 1;
	g_hash_table_insert (client_alarms_hash, client, ca);

	ca->uid_alarms_hash = g_hash_table_new (g_str_hash, g_str_equal);

	if (!cal_client_is_loaded (client))
		gtk_signal_connect (GTK_OBJECT (client), "cal_loaded",
				    GTK_SIGNAL_FUNC (cal_loaded_cb), ca);

	gtk_signal_connect (GTK_OBJECT (client), "obj_updated",
			    GTK_SIGNAL_FUNC (obj_updated_cb), ca);
	gtk_signal_connect (GTK_OBJECT (client), "obj_removed",
			    GTK_SIGNAL_FUNC (obj_removed_cb), ca);

	if (cal_client_is_loaded (client))
		load_alarms (ca);
}

/**
 * alarm_notify_remove_client:
 * @client: A calendar client.
 *
 * Removes a calendar client from the alarm notification system.
 **/
void
alarm_notify_remove_client (CalClient *client)
{
	ClientAlarms *ca;

	g_return_if_fail (alarm_notify_inited);
	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_CAL_CLIENT (client));

	ca = lookup_client (client);
	g_return_if_fail (ca != NULL);

	g_assert (ca->refcount > 0);
	ca->refcount--;

	if (ca->refcount > 0)
		return;

	/* FIXME: remove alarms */

	/* Clean up */

	gtk_signal_disconnect_by_data (GTK_OBJECT (ca->client), ca);

	gtk_object_unref (GTK_OBJECT (ca->client));
	ca->client = NULL;

	g_hash_table_destroy (ca->uid_alarms_hash);
	ca->uid_alarms_hash = NULL;

	g_free (ca);

	g_hash_table_remove (client_alarms_hash, client);
}
