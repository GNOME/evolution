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



#if 0

/* Sends a mail notification of an alarm trigger */
static void
mail_notification (char *mail_address, char *text, time_t app_time)
{
	pid_t pid;
	int   p [2];
	char *command;

	pipe (p);
	pid = fork ();
	if (pid == 0){
		int dev_null;

		dev_null = open ("/dev/null", O_RDWR);
		dup2 (p [0], 0);
		dup2 (dev_null, 1);
		dup2 (dev_null, 2);
		execl ("/usr/lib/sendmail", "/usr/lib/sendmail",
		       mail_address, NULL);
		_exit (127);
	}
	command = g_strconcat ("To: ", mail_address, "\n",
			       "Subject: ", _("Reminder of your appointment at "),
			       ctime (&app_time), "\n\n", text, "\n", NULL);
	write (p [1], command, strlen (command));
 	close (p [1]);
	close (p [0]);
	g_free (command);
}

static int
max_open_files (void)
{
        static int files;

        if (files)
                return files;

        files = sysconf (_SC_OPEN_MAX);
        if (files != -1)
                return files;
#ifdef OPEN_MAX
        return files = OPEN_MAX;
#else
        return files = 256;
#endif
}

/* Executes a program as a notification of an alarm trigger */
static void
program_notification (char *command, int close_standard)
{
	struct sigaction ignore, save_intr, save_quit;
	int status = 0, i;
	pid_t pid;

	ignore.sa_handler = SIG_IGN;
	sigemptyset (&ignore.sa_mask);
	ignore.sa_flags = 0;

	sigaction (SIGINT, &ignore, &save_intr);
	sigaction (SIGQUIT, &ignore, &save_quit);

	if ((pid = fork ()) < 0){
		fprintf (stderr, "\n\nfork () = -1\n");
		return;
	}
	if (pid == 0){
		pid = fork ();
		if (pid == 0){
			const int top = max_open_files ();
			sigaction (SIGINT,  &save_intr, NULL);
			sigaction (SIGQUIT, &save_quit, NULL);

			for (i = (close_standard ? 0 : 3); i < top; i++)
				close (i);

			/* FIXME: As an excercise to the reader, copy the
			 * code from mc to setup shell properly instead of
			 * /bin/sh.  Yes, this comment is larger than a cut and paste.
			 */
			execl ("/bin/sh", "/bin/sh", "-c", command, (char *) 0);

			_exit (127);
		} else {
			_exit (127);
		}
	}
	wait (&status);
	sigaction (SIGINT,  &save_intr, NULL);
	sigaction (SIGQUIT, &save_quit, NULL);
}

/* Queues a snooze alarm */
static void
snooze (GnomeCalendar *gcal, CalComponent *comp, time_t occur, int snooze_mins, gboolean audio)
{
	time_t now, trigger;
	struct tm tm;
	CalAlarmInstance ai;

	now = time (NULL);
	tm = *localtime (&now);
	tm.tm_min += snooze_mins;

	trigger = mktime (&tm);
	if (trigger == -1) {
		g_message ("snooze(): produced invalid time_t; not queueing alarm!");
		return;
	}

#if 0
	cal_component_get_uid (comp, &ai.uid);
	ai.type = audio ? ALARM_AUDIO : ALARM_DISPLAY;
#endif
	ai.trigger = trigger;
	ai.occur = occur;

	setup_alarm (gcal, &ai);
}

struct alarm_notify_closure {
	GnomeCalendar *gcal;
	CalComponent *comp;
	time_t occur;
};

/* Callback used for the result of the alarm notification dialog */
static void
display_notification_cb (AlarmNotifyResult result, int snooze_mins, gpointer data)
{
	struct alarm_notify_closure *c;

	c = data;

	switch (result) {
	case ALARM_NOTIFY_CLOSE:
		break;

	case ALARM_NOTIFY_SNOOZE:
		snooze (c->gcal, c->comp, c->occur, snooze_mins, FALSE);
		break;

	case ALARM_NOTIFY_EDIT:
		gnome_calendar_edit_object (c->gcal, c->comp);
		break;

	default:
		g_assert_not_reached ();
	}

	gtk_object_unref (GTK_OBJECT (c->comp));
	g_free (c);
}

/* Present a display notification of an alarm trigger */
static void
display_notification (time_t trigger, time_t occur, CalComponent *comp, GnomeCalendar *gcal)
{
	gboolean result;
	struct alarm_notify_closure *c;

	gtk_object_ref (GTK_OBJECT (comp));

	c = g_new (struct alarm_notify_closure, 1);
	c->gcal = gcal;
	c->comp = comp;
	c->occur = occur;

	result = alarm_notify_dialog (trigger, occur, comp, display_notification_cb, c);
	if (!result) {
		g_message ("display_notification(): could not display the alarm notification dialog");
		g_free (c);
		gtk_object_unref (GTK_OBJECT (comp));
	}
}

/* Present an audible notification of an alarm trigger */
static void
audio_notification (time_t trigger, time_t occur, CalComponent *comp, GnomeCalendar *gcal)
{
	g_message ("AUDIO NOTIFICATION!");
	/* FIXME */
}

/* Callback function used when an alarm is triggered */
static void
trigger_alarm_cb (gpointer alarm_id, time_t trigger, gpointer data)
{
	struct trigger_alarm_closure *c;
	GnomeCalendarPrivate *priv;
	CalComponent *comp;
	CalClientGetStatus status;
	const char *uid;
	ObjectAlarms *oa;
   	GList *l;

	c = data;
	priv = c->gcal->priv;

	/* Fetch the object */

	status = cal_client_get_object (priv->client, c->uid, &comp);

	switch (status) {
	case CAL_CLIENT_GET_SUCCESS:
		/* Go on */
		break;
	case CAL_CLIENT_GET_SYNTAX_ERROR:
	case CAL_CLIENT_GET_NOT_FOUND:
		g_message ("trigger_alarm_cb(): syntax error in fetched object");
		return;
	}

	g_assert (comp != NULL);

	/* Present notification */

	switch (c->type) {
	case CAL_COMPONENT_ALARM_EMAIL:
#if 0
		g_assert (ico->malarm.enabled);
		mail_notification (ico->malarm.data, ico->summary, c->occur);
#endif
		break;

	case CAL_COMPONENT_ALARM_PROCEDURE:
#if 0
		g_assert (ico->palarm.enabled);
		program_notification (ico->palarm.data, FALSE);
#endif
		break;

	case CAL_COMPONENT_ALARM_DISPLAY:
#if 0
		g_assert (ico->dalarm.enabled);
#endif
		display_notification (trigger, c->occur, comp, c->gcal);
		break;

	case CAL_COMPONENT_ALARM_AUDIO:
#if 0
		g_assert (ico->aalarm.enabled);
#endif
		audio_notification (trigger, c->occur, comp, c->gcal);
		break;

	default:
		break;
	}

	/* Remove the alarm from the hash table */
	cal_component_get_uid (comp, &uid);
	oa = g_hash_table_lookup (priv->alarms, uid);
	g_assert (oa != NULL);

	l = g_list_find (oa->alarm_ids, alarm_id);
	g_assert (l != NULL);

	oa->alarm_ids = g_list_remove_link (oa->alarm_ids, l);
	g_list_free_1 (l);

	if (!oa->alarm_ids) {
		g_hash_table_remove (priv->alarms, uid);
		g_free (oa->uid);
		g_free (oa);
	}

	gtk_object_unref (GTK_OBJECT (comp));
}

#endif

#if 0

static void
stop_beeping (GtkObject* object, gpointer data)
{
	guint timer_tag, beep_tag;
	timer_tag = GPOINTER_TO_INT (gtk_object_get_data (object, "timer_tag"));
	beep_tag  = GPOINTER_TO_INT (gtk_object_get_data (object, "beep_tag"));

	if (beep_tag > 0) {
		gtk_timeout_remove (beep_tag);
		gtk_object_set_data (object, "beep_tag", GINT_TO_POINTER (0));
	}
	if (timer_tag > 0) {
		gtk_timeout_remove (timer_tag);
		gtk_object_set_data (object, "timer_tag", GINT_TO_POINTER (0));
	}
}

static gint
start_beeping (gpointer data)
{
	gdk_beep ();

	return TRUE;
}

static gint
timeout_beep (gpointer data)
{
	stop_beeping (data, NULL);
	return FALSE;
}

void
calendar_notify (time_t activation_time, CalendarAlarm *which, void *data)
{
	iCalObject *ico = data;
	guint beep_tag, timer_tag;
	int ret;
	gchar* snooze_button = (enable_snooze ? _("Snooze") : NULL);
	time_t now, diff;

	if (&ico->aalarm == which){
		time_t app = ico->aalarm.trigger + ico->aalarm.offset;
		GtkWidget *w;
		char *msg;

		msg = g_strconcat (_("Reminder of your appointment at "),
					ctime (&app), "`",
					ico->summary, "'", NULL);

		/* Idea: we need Snooze option :-) */
		w = gnome_message_box_new (msg, GNOME_MESSAGE_BOX_INFO, _("Ok"), snooze_button, NULL);
		beep_tag = gtk_timeout_add (1000, start_beeping, NULL);
		if (enable_aalarm_timeout)
			timer_tag = gtk_timeout_add (audio_alarm_timeout*1000,
						     timeout_beep, w);
		else
			timer_tag = 0;
		gtk_object_set_data (GTK_OBJECT (w), "timer_tag",
				     GINT_TO_POINTER (timer_tag));
		gtk_object_set_data (GTK_OBJECT (w), "beep_tag",
				     GINT_TO_POINTER (beep_tag));
		gtk_widget_ref (w);
		gtk_window_set_modal (GTK_WINDOW (w), FALSE);
		ret = gnome_dialog_run (GNOME_DIALOG (w));
		switch (ret) {
		case 1:
			stop_beeping (GTK_OBJECT (w), NULL);
			now = time (NULL);
			diff = now - which->trigger;
			which->trigger = which->trigger + diff + snooze_secs;
			which->offset  = which->offset - diff - snooze_secs;
			alarm_add (which, &calendar_notify, data);
			break;
		default:
			stop_beeping (GTK_OBJECT (w), NULL);
			break;
		}

		gtk_widget_unref (w);
		return;
	}

        if (&ico->palarm == which){
		execute (ico->palarm.data, 0);
		return;
	}

	if (&ico->malarm == which){
		time_t app = ico->malarm.trigger + ico->malarm.offset;

		mail_notify (ico->malarm.data, ico->summary, app);
		return;
	}

	if (&ico->dalarm == which){
		time_t app = ico->dalarm.trigger + ico->dalarm.offset;
		GtkWidget *w;
		char *msg;

		if (beep_on_display)
			gdk_beep ();
		msg = g_strconcat (_("Reminder of your appointment at "),
					ctime (&app), "`",
					ico->summary, "'", NULL);
		w = gnome_message_box_new (msg, GNOME_MESSAGE_BOX_INFO,
					   _("Ok"), snooze_button, NULL);
		gtk_window_set_modal (GTK_WINDOW (w), FALSE);
		ret = gnome_dialog_run (GNOME_DIALOG (w));
		switch (ret) {
		case 1:
			now = time (NULL);
			diff = now - which->trigger;
			which->trigger = which->trigger + diff + snooze_secs;
			which->offset  = which->offset - diff - snooze_secs;
			alarm_add (which, &calendar_notify, data);
			break;
		default:
			break;
		}

		return;
	}
}

#endif
