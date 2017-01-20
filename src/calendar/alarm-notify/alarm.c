/*
 * Evolution calendar - Low-level alarm timer mechanism
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
 *		Miguel de Icaza <miguel@ximian.com>
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <gdk/gdk.h>
#include "alarm.h"
#include "config-data.h"

/* Our glib timeout */
static guint timeout_id;

/* The list of pending alarms */
static GList *alarms = NULL;

/* A queued alarm structure */
typedef struct {
	time_t             trigger;
	AlarmFunction      alarm_fn;
	gpointer           data;
	AlarmDestroyNotify destroy_notify_fn;
} AlarmRecord;

static void setup_timeout (void);

/* Removes the head alarm from the queue.  Does not touch the timeout_id. */
static void
pop_alarm (void)
{
	AlarmRecord *ar;
	GList *l;

	if (!alarms) {
		g_warning ("Nothing to pop from the alarm queue");
		return;
	}

	ar = alarms->data;

	l = alarms;
	alarms = g_list_delete_link (alarms, l);

	g_free (ar);
}

/* Callback from the alarm timeout */
static gboolean
alarm_ready_cb (gpointer data)
{
	time_t now;

	if (!alarms) {
		g_warning ("Alarm triggered, but no alarm present\n");
		return FALSE;
	}

	timeout_id = 0;

	now = time (NULL);

	debug (("Alarm callback!"));
	while (alarms) {
		AlarmRecord *notify_id, *ar;
		AlarmRecord ar_copy;

		ar = alarms->data;

		if (ar->trigger > now)
			break;

		debug (("Process alarm with trigger %" G_GINT64_FORMAT, (gint64) ar->trigger));
		notify_id = ar;

		ar_copy = *ar;
		ar = &ar_copy;

		/* This will free the original AlarmRecord;
		 * that's why we copy it. */
		pop_alarm ();

		(* ar->alarm_fn) (notify_id, ar->trigger, ar->data);

		if (ar->destroy_notify_fn)
			(* ar->destroy_notify_fn) (notify_id, ar->data);
	}

	/* We need this check because one of the alarm_fn above may have
	 * re-entered and added an alarm of its own, so the timer will
	 * already be set up.
	 */
	if (alarms)
		setup_timeout ();

	return FALSE;
}

/* Sets up a timeout for the next minute.  We do not need to be concerned with
 * timezones here, as this is just a periodic check on the alarm queue.
 */
static void
setup_timeout (void)
{
	const AlarmRecord *ar;
	guint diff;
	time_t now;

	if (!alarms) {
		g_warning ("No alarm to setup\n");
		return;
	}

	ar = alarms->data;

	/* Remove the existing time out */
	if (timeout_id != 0) {
		g_source_remove (timeout_id);
		timeout_id = 0;
	}

	/* Ensure that if the trigger managed to get behind the
	 * current time we timeout immediately */
	diff = MAX (0, ar->trigger - time (NULL));
	now = time (NULL);

	/* Add the time out */
	debug (
		("Setting timeout for %d.%2d (from now) %" G_GINT64_FORMAT " %" G_GINT64_FORMAT,
		diff / 60, diff % 60, (gint64) ar->trigger, (gint64) now));
	debug ((" %s", ctime (&ar->trigger)));
	debug ((" %s", ctime (&now)));
	timeout_id = e_named_timeout_add_seconds (diff, alarm_ready_cb, NULL);
}

/* Used from g_list_insert_sorted(); compares the
 * trigger times of two AlarmRecord structures. */
static gint
compare_alarm_by_time (gconstpointer a,
                       gconstpointer b)
{
	const AlarmRecord *ara = a;
	const AlarmRecord *arb = b;
	time_t diff;

	diff = ara->trigger - arb->trigger;
	return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
}

/* Adds an alarm to the queue and sets up the timer */
static void
queue_alarm (AlarmRecord *ar)
{
	GList *old_head;

	/* Track the current head of the list in case there are changes */
	old_head = alarms;

	/* Insert the new alarm in order if the alarm's trigger time is
	 * after the current time */
	alarms = g_list_insert_sorted (alarms, ar, compare_alarm_by_time);

	/* If there first item on the list didn't change, the time out is fine */
	if (old_head == alarms)
		return;

	/* Set the timer for removal upon activation */
	setup_timeout ();
}

/**
 * alarm_add:
 * @trigger: Time at which alarm will trigger.
 * @alarm_fn: Callback for trigger.
 * @data: Closure data for callback.
 * @destroy_notify_fn: destroy notification callback.
 *
 * Adds an alarm to trigger at the specified time.  The @alarm_fn will be called
 * with the provided data and the alarm will be removed from the trigger list.
 *
 * Return value: An identifier for this alarm; it can be used to remove the
 * alarm later with alarm_remove().  If the trigger time occurs in the past, then
 * the alarm will not be queued and the function will return NULL.
 **/
gpointer
alarm_add (time_t trigger,
           AlarmFunction alarm_fn,
           gpointer data,
           AlarmDestroyNotify destroy_notify_fn)
{
	AlarmRecord *ar;

	g_return_val_if_fail (trigger != -1, NULL);
	g_return_val_if_fail (alarm_fn != NULL, NULL);

	ar = g_new (AlarmRecord, 1);
	ar->trigger = trigger;
	ar->alarm_fn = alarm_fn;
	ar->data = data;
	ar->destroy_notify_fn = destroy_notify_fn;

	queue_alarm (ar);

	return ar;
}

/**
 * alarm_remove:
 * @alarm: A queued alarm identifier.
 *
 * Removes an alarm from the alarm queue.
 **/
void
alarm_remove (gpointer alarm)
{
	AlarmRecord *notify_id, *ar;
	AlarmRecord ar_copy;
	AlarmRecord *old_head;
	GList *l;

	g_return_if_fail (alarm != NULL);

	ar = alarm;

	l = g_list_find (alarms, ar);
	if (!l) {
		g_warning (G_STRLOC ": Requested removal of nonexistent alarm!");
		return;
	}

	old_head = alarms->data;

	notify_id = ar;

	if (old_head == ar) {
		ar_copy = *ar;
		ar = &ar_copy;

		/* This will free the original AlarmRecord;
		 * that's why we copy it. */
		pop_alarm ();
	} else {
		alarms = g_list_delete_link (alarms, l);
	}

	/* Reset the timeout */
	if (!alarms) {
		g_source_remove (timeout_id);
		timeout_id = 0;
	}

	/* Notify about destructiono of the alarm */

	if (ar->destroy_notify_fn)
		(* ar->destroy_notify_fn) (notify_id, ar->data);

}

/**
 * alarm_done:
 *
 * Terminates the alarm timer mechanism.  This should be called at the end of
 * the program.
 **/
void
alarm_done (void)
{
	GList *l;

	if (timeout_id == 0) {
		if (alarms)
			g_warning ("No timeout, but queue is not NULL\n");
		return;
	}

	g_source_remove (timeout_id);
	timeout_id = 0;

	if (!alarms) {
		g_warning ("timeout present, freed, but no alarms active\n");
		return;
	}

	for (l = alarms; l; l = l->next) {
		AlarmRecord *ar;

		ar = l->data;

		if (ar->destroy_notify_fn)
			(* ar->destroy_notify_fn) (ar, ar->data);

		g_free (ar);
	}

	g_list_free (alarms);
	alarms = NULL;
}

/**
 * alarm_reschedule_timeout:
 *
 * Re-sets timeout for alarms, if any.
 **/
void
alarm_reschedule_timeout (void)
{
	if (alarms)
		setup_timeout ();
}
