/* Evolution calendar - Alarm notification service object
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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
#include "config.h"
#endif

#include <string.h>
#include <bonobo/bonobo-main.h>
#include <libecal/e-cal.h>
#include "alarm-notify.h"
#include "alarm-queue.h"
#include "config-data.h"
#include "common/authentication.h"
#include "e-util/e-url.h"



/* Private part of the AlarmNotify structure */
struct _AlarmNotifyPrivate {
	/* Mapping from EUri's to LoadedClient structures */
	/* FIXME do we need per source type uri hashes? or perhaps we
	   just need to hash based on source */
	GHashTable *uri_client_hash;

	ESourceList *source_lists [E_CAL_SOURCE_TYPE_LAST];	
};



static void alarm_notify_class_init (AlarmNotifyClass *klass);
static void alarm_notify_init (AlarmNotify *an, AlarmNotifyClass *klass);
static void alarm_notify_finalize (GObject *object);


static BonoboObjectClass *parent_class;



BONOBO_TYPE_FUNC_FULL(AlarmNotify, GNOME_Evolution_Calendar_AlarmNotify, BONOBO_TYPE_OBJECT, alarm_notify)

/* Class initialization function for the alarm notify service */
static void
alarm_notify_class_init (AlarmNotifyClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = alarm_notify_finalize;
}

typedef struct {
	AlarmNotify *an;
	ESourceList *source_list;
	GList *removals;
} ProcessRemovalsData;

static void
process_removal_in_hash (gpointer key, gpointer value, gpointer data)
{
	char *uri = key;
	ProcessRemovalsData *prd = data;
	GSList *groups, *sources, *p, *q;
	gboolean found = FALSE;

	/* search the list of selected calendars */
	groups = e_source_list_peek_groups (prd->source_list);
	for (p = groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);
		
		sources = e_source_group_peek_sources (group);
		for (q = sources; q != NULL; q = q->next) {
			ESource *source = E_SOURCE (q->data);
			char *source_uri;
			
			source_uri = e_source_get_uri (source);
			if (strcmp (source_uri, uri) == 0)
				found = TRUE;
			g_free (source_uri);

			if (found)
				return;
		}
	}

	/* not found, so list it for removal */
	prd->removals = g_list_prepend (prd->removals, uri);
}

static void
list_changed_cb (ESourceList *source_list, gpointer data)
{
	AlarmNotify *an = data;
	AlarmNotifyPrivate *priv;
	GSList *groups, *sources, *p, *q;
	ECalSourceType source_type = E_CAL_SOURCE_TYPE_LAST;
	ProcessRemovalsData prd;
	GList *l;
	int i;
	
	priv = an->priv;

	/* Figure out the source type */
	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		if (source_list == priv->source_lists[i]) {
			source_type = i;
			break;
		}
	}
	if (source_type == E_CAL_SOURCE_TYPE_LAST)
		return;
	
	/* process the additions */
	groups = e_source_list_peek_groups (source_list);
	for (p = groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);
		
		sources = e_source_group_peek_sources (group);
		for (q = sources; q != NULL; q = q->next) {
			ESource *source = E_SOURCE (q->data);
			char *uri;
			
			uri = e_source_get_uri (source);
			if (!g_hash_table_lookup (priv->uri_client_hash, uri)) {
				g_message ("Adding %s", uri);
				alarm_notify_add_calendar (an, source_type, uri, FALSE);
			}
			g_free (uri);
		}
	}

	/* process the removals */
	prd.an = an;
	prd.source_list = priv->source_lists[source_type];
	prd.removals = NULL;
	g_hash_table_foreach (priv->uri_client_hash, (GHFunc) process_removal_in_hash, &prd);

	for (l = prd.removals; l; l = l->next) {
		g_message ("Removing %s", (char *)l->data);
		alarm_notify_remove_calendar (an, l->data);
	}
	g_list_free (prd.removals);
}

static void
load_calendars (AlarmNotify *an, ECalSourceType source_type)
{
	AlarmNotifyPrivate *priv;
	ESourceList *source_list;
	GSList *groups, *sources, *p, *q;

	priv = an->priv;
	
	if (!e_cal_get_sources (&source_list, source_type, NULL)) {
		g_message (G_STRLOC ": Could not get the list of sources to load");
		priv->source_lists[source_type] = NULL;

		return;
	}

	groups = e_source_list_peek_groups (source_list);
	for (p = groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);
		
		sources = e_source_group_peek_sources (group);
		for (q = sources; q != NULL; q = q->next) {
			ESource *source = E_SOURCE (q->data);
			char *uri;
			
			uri = e_source_get_uri (source);
			g_message ("Loading %s", uri);
			alarm_notify_add_calendar (an, source_type, uri, FALSE);
			g_free (uri);
			
		}
	}

	g_signal_connect_object (source_list, "changed", G_CALLBACK (list_changed_cb), an, 0);
	priv->source_lists[source_type] = source_list;
}

/* Object initialization function for the alarm notify system */
static void
alarm_notify_init (AlarmNotify *an, AlarmNotifyClass *klass)
{
	AlarmNotifyPrivate *priv;
	int i;

	priv = g_new0 (AlarmNotifyPrivate, 1);
	an->priv = priv;

	priv->uri_client_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	alarm_queue_init ();

	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++)
		load_calendars (an, i);
}

static void
dequeue_client (gpointer key, gpointer value, gpointer user_data)
{
	ECal *client = value;

	alarm_queue_remove_client (client);
}

/* Finalize handler for the alarm notify system */
static void
alarm_notify_finalize (GObject *object)
{
	AlarmNotify *an;
	AlarmNotifyPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ALARM_NOTIFY (object));

	an = ALARM_NOTIFY (object);
	priv = an->priv;

	g_hash_table_foreach (priv->uri_client_hash, dequeue_client, NULL);
	g_hash_table_destroy (priv->uri_client_hash);

	g_free (priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}



/**
 * alarm_notify_new:
 * 
 * Creates a new #AlarmNotify object.
 * 
 * Return value: A newly-created #AlarmNotify, or NULL if its corresponding
 * CORBA object could not be created.
 **/
AlarmNotify *
alarm_notify_new (void)
{
	AlarmNotify *an;

	an = g_object_new (TYPE_ALARM_NOTIFY,
			   "poa", bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL),
			   NULL);
	return an;
}

static void
cal_opened_cb (ECal *client, ECalendarStatus status, gpointer user_data)
{
	AlarmNotifyPrivate *priv;
	AlarmNotify *an = ALARM_NOTIFY (user_data);

	priv = an->priv;

	if (status == E_CALENDAR_STATUS_OK)
		alarm_queue_add_client (client);
	else {
		g_hash_table_remove (priv->uri_client_hash,
				     e_cal_get_uri (client));
	}
}

/**
 * alarm_notify_add_calendar:
 * @an: An alarm notification service.
 * @uri: URI of the calendar to load.
 * @load_afterwards: Whether this calendar should be loaded in the future
 * when the alarm daemon starts up.
 * 
 * Tells the alarm notification service to load a calendar and start monitoring
 * its alarms.  It can optionally be made to save the URI of this calendar so
 * that it can be loaded in the future when the alarm daemon starts up.
 **/
void
alarm_notify_add_calendar (AlarmNotify *an, ECalSourceType source_type, const char *str_uri, gboolean load_afterwards)
{
	AlarmNotifyPrivate *priv;
	ECal *client;

	g_return_if_fail (an != NULL);
	g_return_if_fail (IS_ALARM_NOTIFY (an));
	g_return_if_fail (str_uri != NULL);

	priv = an->priv;

	/* See if we already know about this uri */
	if (g_hash_table_lookup (priv->uri_client_hash, str_uri))
		return;

	client = auth_new_cal_from_uri (str_uri, source_type);

	if (client) {
		g_hash_table_insert (priv->uri_client_hash, g_strdup (str_uri), client);
		g_signal_connect (G_OBJECT (client), "cal_opened", G_CALLBACK (cal_opened_cb), an);
		e_cal_open_async (client, FALSE);
	}
}

void
alarm_notify_remove_calendar (AlarmNotify *an, const char *str_uri)
{
	AlarmNotifyPrivate *priv;
	ECal *client;

	priv = an->priv;

	client = g_hash_table_lookup (priv->uri_client_hash, str_uri);
	if (client) {
		alarm_queue_remove_client (client);
		g_hash_table_remove (priv->uri_client_hash, str_uri);
	}
}
