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
#include <gtk/gtksignal.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktooltips.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-sound.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-uidefs.h>
#include <e-util/eggtrayicon.h>
#include <e-util/e-icon-factory.h>
#include <libecal/e-cal-time-util.h>
#include "evolution-calendar.h"
#include "alarm.h"
#include "alarm-notify-dialog.h"
#include "alarm-queue.h"
#include "config-data.h"
#include "util.h"



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
	char *uid;

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

	/* Whether this is a snoozed queued alarm or a normal one */
	guint snooze : 1;
} QueuedAlarm;

/* Alarm ID for the midnight refresh function */
static gpointer midnight_refresh_id = NULL;

static void display_notification (time_t trigger, CompQueuedAlarms *cqa,
				  gpointer alarm_id, gboolean use_description);
static void audio_notification (time_t trigger, CompQueuedAlarms *cqa, gpointer alarm_id);
static void mail_notification (time_t trigger, CompQueuedAlarms *cqa, gpointer alarm_id);
static void procedure_notification (time_t trigger, CompQueuedAlarms *cqa, gpointer alarm_id);

static void query_objects_changed_cb (ECal *client, GList *objects, gpointer data);
static void query_objects_removed_cb (ECal *client, GList *objects, gpointer data);



/* Alarm queue engine */

static void load_alarms_for_today (ClientAlarms *ca);
static void midnight_refresh_cb (gpointer alarm_id, time_t trigger, gpointer data);

/* Queues an alarm trigger for midnight so that we can load the next day's worth
 * of alarms.
 */
static void
queue_midnight_refresh (void)
{
	time_t midnight;
	icaltimezone *zone;

	g_assert (midnight_refresh_id == NULL);

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

	midnight_refresh_id = NULL;
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
		g_hash_table_remove (cqa->parent_client->uid_alarms_hash, cqa->uid);
		g_free (cqa->uid);
		cqa->uid = NULL;
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

	e_cal_component_alarm_get_action (alarm, &action);
	e_cal_component_alarm_free (alarm);

	switch (action) {
	case E_CAL_COMPONENT_ALARM_AUDIO:
		audio_notification (trigger, cqa, alarm_id);
		break;

	case E_CAL_COMPONENT_ALARM_DISPLAY:
		display_notification (trigger, cqa, alarm_id, TRUE);
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
	const char *uid;
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
		qa->snooze = FALSE;

		cqa->queued_alarms = g_slist_prepend (cqa->queued_alarms, qa);
	}

	e_cal_component_get_uid (alarms->comp, &uid);

	/* If we failed to add all the alarms, then we should get rid of the cqa */
	if (cqa->queued_alarms == NULL) {
		g_message ("add_component_alarms(): Could not add any of the alarms "
			   "for the component `%s'; discarding it...", uid);

		e_cal_component_alarms_free (cqa->alarms);
		cqa->alarms = NULL;

		g_free (cqa);
		return;
	}

	cqa->queued_alarms = g_slist_reverse (cqa->queued_alarms);
	cqa->uid = g_strdup (uid);
	g_hash_table_insert (ca->uid_alarms_hash, cqa->uid, cqa);
}

/* Loads the alarms of a client for a given range of time */
static void
load_alarms (ClientAlarms *ca, time_t start, time_t end)
{
	/* create the live query */
	if (!ca->query) {
		/* FIXME: handle errors */
		if (!e_cal_get_query (ca->client, "(has-alarms?)", &ca->query, NULL)) {
			g_warning (G_STRLOC ": Could not get query for client");
			return;
		}

		g_signal_connect (G_OBJECT (ca->query), "objects_added",
				  G_CALLBACK (query_objects_changed_cb), ca);
		g_signal_connect (G_OBJECT (ca->query), "objects_modified",
				  G_CALLBACK (query_objects_changed_cb), ca);
		g_signal_connect (G_OBJECT (ca->query), "objects_removed",
				  G_CALLBACK (query_objects_removed_cb), ca);

		e_cal_view_start (ca->query);
	}
}

/* Loads today's remaining alarms for a client */
static void
load_alarms_for_today (ClientAlarms *ca)
{
	time_t now, day_end;
	icaltimezone *zone;

	now = time (NULL);

	zone = config_data_get_timezone ();

	g_message ("Loading alarms for today");
	day_end = time_day_end_with_zone (now, zone);
	load_alarms (ca, now, day_end);
}

/* Adds any alarms that should have occurred while the alarm daemon was not
 * running.
 */
static void
load_missed_alarms (ClientAlarms *ca)
{
	time_t now;

	now = time (NULL);

	g_assert (saved_notification_time != -1);

	/* We add 1 to the saved_notification_time to make the time ranges
	 * half-open; we do not want to display the "last" displayed alarm
	 * twice, once when it occurs and once when the alarm daemon restarts.
	 */
	load_alarms (ca, saved_notification_time + 1, now);
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
	load_missed_alarms (ca);
}

/* Looks up a component's queued alarm structure in a client alarms structure */
static CompQueuedAlarms *
lookup_comp_queued_alarms (ClientAlarms *ca, const char *uid)
{
	return g_hash_table_lookup (ca->uid_alarms_hash, uid);
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
remove_comp (ClientAlarms *ca, const char *uid)
{
	CompQueuedAlarms *cqa;

	cqa = lookup_comp_queued_alarms (ca, uid);
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
	g_assert (lookup_comp_queued_alarms (ca, uid) == NULL);
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
		const char *uid;
		GSList *sl;

		uid = icalcomponent_get_uid (l->data);
		found = e_cal_get_alarms_for_object (ca->client, uid, from, day_end, &alarms);

		if (!found) {
			g_message ("No alarms found on object");
			remove_comp (ca, uid);
			continue;
		}

		cqa = lookup_comp_queued_alarms (ca, uid);
		if (!cqa) {
			g_message ("No currently queue alarms");
			add_component_alarms (ca, alarms);
			continue;
		}

		g_message ("Already existing alarms");
		/* if the alarms or the alarms list is empty, just remove it */
		if (alarms == NULL || alarms->alarms == NULL) {
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
			
			cqa->queued_alarms = g_slist_prepend (cqa->queued_alarms, qa);
		}
		
		cqa->queued_alarms = g_slist_reverse (cqa->queued_alarms);
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
		g_message (G_STRLOC ": Could not activate the component editor factory");
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
		g_message (G_STRLOC ": Exception while editing the component");

	CORBA_exception_free (&ev);

	/* Get rid of the factory */
	bonobo_object_release_unref (factory, NULL);
}

typedef struct {
	char *message;
	gboolean blink_state;
	gint blink_id;
	time_t trigger;
	CompQueuedAlarms *cqa;
	gpointer alarm_id;
	ECalComponent *comp;
	ECal *client;
	ECalView *query;
	GtkWidget *tray_icon;
	GtkWidget *image;
	GtkWidget *alarm_dialog;
} TrayIconData;

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
			if (tray_data->alarm_dialog)
				alarm_notify_dialog_disable_buttons (tray_data->alarm_dialog);
			tray_data->cqa = NULL;
			tray_data->alarm_id = NULL;

			gtk_widget_destroy (tray_data->tray_icon);
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
		tray_data->cqa = NULL;
		return;

	case ALARM_NOTIFY_EDIT:
		edit_component (tray_data->client, tray_data->comp);
		break;

	case ALARM_NOTIFY_CLOSE:
		/* Do nothing */
		break;

	default:
		g_assert_not_reached ();
	}

	tray_data->alarm_dialog = NULL;
	gtk_widget_destroy (tray_data->tray_icon);
}

static gint
tray_icon_destroyed_cb (GtkWidget *tray, gpointer user_data)
{
	TrayIconData *tray_data = user_data;

	if (tray_data->cqa != NULL)
		remove_queued_alarm (tray_data->cqa, tray_data->alarm_id, TRUE, TRUE);

	if (tray_data->message != NULL) {
		g_free (tray_data->message);
		tray_data->message = NULL;
	}

	g_source_remove (tray_data->blink_id);

	g_object_unref (tray_data->comp);
	g_object_unref (tray_data->client);

	tray_icons_list = g_list_remove (tray_icons_list, tray_data);
	g_free (tray_data);

	return TRUE;
}

/* Callbacks.  */
static void
add_popup_menu_item (GtkMenu *menu, const char *label, const char *pixmap,
		     GCallback callback, gpointer user_data)
{
	GtkWidget *item, *image;

	if (pixmap) {
		item = gtk_image_menu_item_new_with_label (label);

		/* load the image */
		if (g_file_test (pixmap, G_FILE_TEST_EXISTS))
			image = gtk_image_new_from_file (pixmap);
		else
			image = gtk_image_new_from_stock (pixmap, GTK_ICON_SIZE_MENU);

		if (image) {
			gtk_widget_show (image);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		}
	} else {
		item = gtk_menu_item_new_with_label (label);
	}

	if (callback)
		g_signal_connect (G_OBJECT (item), "activate", callback, user_data);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

static gboolean
open_alarm_dialog (TrayIconData *tray_data)
{
	QueuedAlarm *qa;

	if (tray_data->alarm_dialog != NULL)
		return FALSE;

	qa = lookup_queued_alarm (tray_data->cqa, tray_data->alarm_id);
	if (qa) {
		gtk_widget_hide (tray_data->tray_icon);
		tray_data->alarm_dialog = alarm_notify_dialog (
			tray_data->trigger,
			qa->instance->occur_start,
			qa->instance->occur_end,
			e_cal_component_get_vtype (tray_data->comp),
			tray_data->message,
			notify_dialog_cb, tray_data);
	}

	return TRUE;
}

static void
popup_dismiss_cb (GtkWidget *widget, TrayIconData *tray_data)
{
	gtk_widget_destroy (tray_data->tray_icon);
}

static void
popup_dismiss_all_cb (GtkWidget *widget, TrayIconData *tray_data)
{
	while (tray_icons_list != NULL) {
		TrayIconData *tray_data = tray_icons_list->data;

		gtk_widget_destroy (tray_data->tray_icon);

		tray_icons_list = g_list_remove (tray_icons_list, tray_icons_list);
	}
}

static void
popup_open_cb (GtkWidget *widget, TrayIconData *tray_data)
{
	open_alarm_dialog (tray_data);
}

static gint
tray_icon_clicked_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	TrayIconData *tray_data = user_data;

	if (event->type == GDK_BUTTON_PRESS) {
		if (event->button == 1) {
			return open_alarm_dialog (tray_data);
		} else if (event->button == 3) {
			GtkWidget *menu;

			/* display popup menu */
			menu = gtk_menu_new ();
			add_popup_menu_item (GTK_MENU (menu), _("Open"), GTK_STOCK_OPEN,
					     G_CALLBACK (popup_open_cb), tray_data);
			add_popup_menu_item (GTK_MENU (menu), _("Dismiss"), NULL,
					     G_CALLBACK (popup_dismiss_cb), tray_data);
			add_popup_menu_item (GTK_MENU (menu), _("Dismiss All"), NULL,
					     G_CALLBACK (popup_dismiss_all_cb), tray_data);
			gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);

			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
tray_icon_blink_cb (gpointer data)
{
	TrayIconData *tray_data = data;
	GdkPixbuf *pixbuf;

	tray_data->blink_state = tray_data->blink_state == TRUE ? FALSE : TRUE;
	pixbuf = e_icon_factory_get_icon  (tray_data->blink_state == TRUE ?
				 	   "stock_appointment-reminder-excl" :
				 	   "stock_appointment-reminder",
					   E_ICON_SIZE_LARGE_TOOLBAR);
	gtk_image_set_from_pixbuf (GTK_IMAGE (tray_data->image), pixbuf);
	gdk_pixbuf_unref (pixbuf);

	return TRUE;
}

/* Performs notification of a display alarm */
static void
display_notification (time_t trigger, CompQueuedAlarms *cqa,
		      gpointer alarm_id, gboolean use_description)
{
	QueuedAlarm *qa;
	ECalComponent *comp;
	const char *message;
	ECalComponentAlarm *alarm;
	GtkWidget *tray_icon, *image, *ebox;
	GtkTooltips *tooltips;
	TrayIconData *tray_data;
	ECalComponentText text;
	char *str, *start_str, *end_str, *alarm_str;
	icaltimezone *current_zone;
	GdkPixbuf *pixbuf;

	comp = cqa->alarms->comp;
	qa = lookup_queued_alarm (cqa, alarm_id);
	if (!qa)
		return;
	
	/* get a sensible description for the event */
	alarm = e_cal_component_get_alarm (comp, qa->instance->auid);
	g_assert (alarm != NULL);

	e_cal_component_alarm_get_description (alarm, &text);
	e_cal_component_alarm_free (alarm);

	if (text.value)
		message = text.value;
	else {
		e_cal_component_get_summary (comp, &text); 
 		if (text.value)
 			message = text.value;
 		else
 			message = _("No description available.");
	}

	/* create the tray icon */
	tooltips = gtk_tooltips_new ();

	/* FIXME: Use stock image equivalent when it becomes available */
	tray_icon = egg_tray_icon_new (qa->instance->auid);
	pixbuf = e_icon_factory_get_icon  ("stock_appointment-reminder", E_ICON_SIZE_LARGE_TOOLBAR);
	image = gtk_image_new_from_pixbuf (pixbuf);
	gdk_pixbuf_unref (pixbuf);
	ebox = gtk_event_box_new ();

	gtk_widget_show (image);
	gtk_widget_show (ebox);

	current_zone = config_data_get_timezone ();
	alarm_str = timet_to_str_with_zone (trigger, current_zone);
	start_str = timet_to_str_with_zone (qa->instance->occur_start, current_zone);
	end_str = timet_to_str_with_zone (qa->instance->occur_end, current_zone);
	str = g_strdup_printf (_("Alarm on %s\n%s\nStarting at %s\nEnding at %s"),
			       alarm_str, message, start_str, end_str);
	gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips), ebox, str, str);
	g_free (start_str);
	g_free (end_str);
	g_free (alarm_str);
	g_free (str);
	
	g_object_set_data (G_OBJECT (tray_icon), "image", image);
	g_object_set_data (G_OBJECT (tray_icon), "available", GINT_TO_POINTER (1));

	gtk_container_add (GTK_CONTAINER (ebox), image);
	gtk_container_add (GTK_CONTAINER (tray_icon), ebox);

	/* create the private structure */
	tray_data = g_new0 (TrayIconData, 1);
	tray_data->message = g_strdup (message);
	tray_data->trigger = trigger;
	tray_data->cqa = cqa;
	tray_data->alarm_id = alarm_id;
	tray_data->comp = e_cal_component_clone (comp);
	tray_data->client = cqa->parent_client->client;
	tray_data->query = cqa->parent_client->query;
	tray_data->image = image;
	tray_data->blink_state = FALSE;
	g_object_ref (tray_data->client);
	tray_data->tray_icon = tray_icon;

	tray_icons_list = g_list_prepend (tray_icons_list, tray_data);

	g_signal_connect (G_OBJECT (tray_icon), "destroy",
			  G_CALLBACK (tray_icon_destroyed_cb), tray_data);
	g_signal_connect (G_OBJECT (ebox), "button_press_event",
			  G_CALLBACK (tray_icon_clicked_cb), tray_data);
	g_signal_connect (G_OBJECT (tray_data->query), "objects_removed",
			  G_CALLBACK (on_dialog_objs_removed_cb), tray_data);

	tray_data->blink_id = g_timeout_add (500, tray_icon_blink_cb, tray_data);

	if (!config_data_get_notify_with_tray ()) {
		open_alarm_dialog (tray_data);
	} else {
		gtk_widget_show (tray_icon);
	}	
}

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

	/* We present a notification message in addition to playing the sound */
	display_notification (trigger, cqa, alarm_id, FALSE);
}

/* Performs notification of a mail alarm */
static void
mail_notification (time_t trigger, CompQueuedAlarms *cqa, gpointer alarm_id)
{
	GtkWidget *dialog;
	GtkWidget *label;

	/* FIXME */

	display_notification (trigger, cqa, alarm_id, FALSE);

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

	remove_queued_alarm (cqa, alarm_id, TRUE, TRUE);
	return;

 fallback:

	display_notification (trigger, cqa, alarm_id, FALSE);
}



/**
 * alarm_queue_init:
 *
 * Initializes the alarm queueing system.  This should be called near the
 * beginning of the program.
 **/
void
alarm_queue_init (void)
{
	g_return_if_fail (alarm_queue_inited == FALSE);

	client_alarms_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	queue_midnight_refresh ();

	saved_notification_time = config_data_get_last_notification_time ();
	if (saved_notification_time == -1) {
		saved_notification_time = time (NULL);
		config_data_set_last_notification_time (saved_notification_time);
	}

	alarm_queue_inited = TRUE;
}

static void
free_client_alarms_cb (gpointer key, gpointer value, gpointer user_data)
{
	ClientAlarms *ca = value;

	if (ca) {
		g_object_unref (ca->client);
		g_free (ca);
	}
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

	g_hash_table_foreach (client_alarms_hash, (GHFunc) free_client_alarms_cb, NULL);
	g_hash_table_destroy (client_alarms_hash);
	client_alarms_hash = NULL;

	g_assert (midnight_refresh_id != NULL);
	alarm_remove (midnight_refresh_id);
	midnight_refresh_id = NULL;

	alarm_queue_inited = FALSE;
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

	ca->uid_alarms_hash = g_hash_table_new (g_str_hash, g_str_equal);

	if (e_cal_get_load_state (client) != E_CAL_LOAD_LOADED)
		g_signal_connect (client, "cal_opened",
				  G_CALLBACK (cal_opened_cb),
				  ca);

	if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED) {
		load_alarms_for_today (ca);
		load_missed_alarms (ca);
	}
}

/* Called from g_hash_table_foreach(); adds a component UID to a list */
static void
add_uid_cb (gpointer key, gpointer value, gpointer data)
{
	GSList **uids;
	const char *uid;

	uids = data;
	uid = key;

	*uids = g_slist_prepend (*uids, (char *) uid);
}

/* Removes all the alarms queued for a particular calendar client */
static void
remove_client_alarms (ClientAlarms *ca)
{
	GSList *uids;
	GSList *l;

	/* First we build a list of UIDs so that we can remove them one by one */

	uids = NULL;
	g_hash_table_foreach (ca->uid_alarms_hash, add_uid_cb, &uids);

	for (l = uids; l; l = l->next) {
		const char *uid;

		uid = l->data;

		remove_comp (ca, uid);
	}

	g_slist_free (uids);

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

	g_signal_handlers_disconnect_matched (ca->client, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, ca);

	g_object_unref (ca->query);
	ca->query = NULL;
	g_object_unref (ca->client);
	ca->client = NULL;

	g_hash_table_destroy (ca->uid_alarms_hash);
	ca->uid_alarms_hash = NULL;

	g_free (ca);

	g_hash_table_remove (client_alarms_hash, client);
}
