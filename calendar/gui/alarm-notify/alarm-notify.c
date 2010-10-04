/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserverui/e-passwords.h>
#include <libecal/e-cal.h>
#include "alarm-notify.h"
#include "alarm-queue.h"
#include "config-data.h"
#include "common/authentication.h"

#define d(x)

#define ALARM_NOTIFY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_ALARM_NOTIFY, AlarmNotifyPrivate))

struct _AlarmNotifyPrivate {
	/* Mapping from EUri's to LoadedClient structures */
	/* FIXME do we need per source type uri hashes? or perhaps we
	   just need to hash based on source */
	GHashTable *uri_client_hash[E_CAL_SOURCE_TYPE_LAST];
        ESourceList *source_lists[E_CAL_SOURCE_TYPE_LAST];
	ESourceList *selected_calendars;
        GMutex *mutex;
};

typedef struct {
	AlarmNotify *an;
	ESourceList *source_list;
	GList *removals;
} ProcessRemovalsData;

static gpointer parent_class;

static void
process_removal_in_hash (const gchar *uri,
                         gpointer value,
                         ProcessRemovalsData *prd)
{
	GSList *groups, *sources, *p, *q;
	gboolean found = FALSE;

	/* search the list of selected calendars */
	groups = e_source_list_peek_groups (prd->source_list);
	for (p = groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);

		sources = e_source_group_peek_sources (group);
		for (q = sources; q != NULL; q = q->next) {
			ESource *source = E_SOURCE (q->data);
			gchar *source_uri;
			const gchar *completion = e_source_get_property (source, "alarm");

			source_uri = e_source_get_uri (source);
			if (strcmp (source_uri, uri) == 0)
				if (!completion || !g_ascii_strcasecmp (completion, "true"))
					found = TRUE;

			g_free (source_uri);

			if (found)
				return;
		}
	}

	/* not found, so list it for removal */
	prd->removals = g_list_prepend (prd->removals, (gpointer) uri);
}

static void
alarm_notify_list_changed_cb (ESourceList *source_list,
                              AlarmNotify *an)
{
	GSList *groups, *sources, *p, *q;
	ECalSourceType source_type = E_CAL_SOURCE_TYPE_LAST;
	ProcessRemovalsData prd;
	GList *l;
	gint i;

	g_signal_handlers_block_by_func (
		source_list, alarm_notify_list_changed_cb, an);

	/* Figure out the source type */
	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		if (source_list == an->priv->source_lists[i]) {
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
			gchar *uri;
			const gchar *completion = e_source_get_property (source, "alarm");

			if (completion  && (!g_ascii_strcasecmp (completion, "false") ||
						!g_ascii_strcasecmp (completion, "never")))
				continue;

			uri = e_source_get_uri (source);
			if (!g_hash_table_lookup (an->priv->uri_client_hash[source_type], uri)) {
				d (printf("%s:%d (list_changed_cb) - Adding Calendar %s\n", __FILE__, __LINE__, uri));
				alarm_notify_add_calendar (an, source_type, source, FALSE);
			}
			g_free (uri);
		}
	}

	/* process the removals */
	prd.an = an;
	prd.source_list = an->priv->source_lists[source_type];
	prd.removals = NULL;
	g_hash_table_foreach (an->priv->uri_client_hash[source_type], (GHFunc) process_removal_in_hash, &prd);

	for (l = prd.removals; l; l = l->next) {
		d (printf("%s:%d (list_changed_cb) - Removing Calendar %s\n", __FILE__, __LINE__, (gchar *)l->data));
		alarm_notify_remove_calendar (an, source_type, l->data);
	}
	g_list_free (prd.removals);
	g_signal_handlers_unblock_by_func (
		source_list, alarm_notify_list_changed_cb, an);

}

static void
alarm_notify_load_calendars (AlarmNotify *an,
                             ECalSourceType source_type)
{
	ESourceList *source_list;
	GSList *groups, *sources, *p, *q;

	if (!e_cal_get_sources (&source_list, source_type, NULL)) {
		d (printf("%s:%d (load_calendars) - Cannont get sources\n ", __FILE__, __LINE__));
		an->priv->source_lists[source_type] = NULL;

		return;
	}

	groups = e_source_list_peek_groups (source_list);
	for (p = groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);

		sources = e_source_group_peek_sources (group);
		for (q = sources; q != NULL; q = q->next) {
			ESource *source = E_SOURCE (q->data);
			gchar *uri;
			const gchar *completion = e_source_get_property (source, "alarm");

			if (completion  && (!g_ascii_strcasecmp (completion, "false") ||
						!g_ascii_strcasecmp (completion, "never")))
				continue;

			uri = e_source_get_uri (source);
			d (printf("%s:%d (load_calendars) - Loading Calendar %s \n", __FILE__, __LINE__, uri));
			alarm_notify_add_calendar (an, source_type, source, FALSE);
			g_free (uri);

		}
	}

	g_signal_connect_object (
		source_list, "changed",
		G_CALLBACK (alarm_notify_list_changed_cb), an, 0);
	an->priv->source_lists[source_type] = source_list;
}

static void
alarm_notify_dequeue_client (gpointer key,
                             ECal *client)
{
	alarm_queue_remove_client (client, TRUE);
}

static void
alarm_notify_finalize (GObject *object)
{
	AlarmNotifyPrivate *priv;
	gint ii;

	priv = ALARM_NOTIFY_GET_PRIVATE (object);

	for (ii = 0; ii < E_CAL_SOURCE_TYPE_LAST; ii++) {
		g_hash_table_foreach (
			priv->uri_client_hash[ii],
			(GHFunc) alarm_notify_dequeue_client, NULL);
		g_hash_table_destroy (priv->uri_client_hash[ii]);
	}

	alarm_queue_done ();

	g_mutex_free (priv->mutex);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
alarm_notify_class_init (AlarmNotifyClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (AlarmNotifyPrivate));

	object_class = (GObjectClass *) class;
	object_class->finalize = alarm_notify_finalize;
}

static void
alarm_notify_init (AlarmNotify *an)
{
	gint ii;

	an->priv = ALARM_NOTIFY_GET_PRIVATE (an);
	an->priv->mutex = g_mutex_new ();
	an->priv->selected_calendars = config_data_get_calendars (
		"/apps/evolution/calendar/sources");

	for (ii = 0; ii < E_CAL_SOURCE_TYPE_LAST; ii++)
		an->priv->uri_client_hash[ii] = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) g_object_unref);

	alarm_queue_init (an);

	for (ii = 0; ii < E_CAL_SOURCE_TYPE_LAST; ii++)
		alarm_notify_load_calendars (an, ii);
}

ESourceList *
alarm_notify_get_selected_calendars (AlarmNotify *an)
{
	return an->priv->selected_calendars;
}

GType
alarm_notify_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (AlarmNotifyClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) alarm_notify_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (AlarmNotify),
			0,     /* n_preallocs */
			(GInstanceInitFunc) alarm_notify_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "AlarmNotify", &type_info, 0);
	}

	return type;
}

/**
 * alarm_notify_new:
 *
 * Creates a new #AlarmNotify object.
 *
 * Returns: a newly-created #AlarmNotify
 **/
AlarmNotify *
alarm_notify_new (void)
{
	return g_object_new (TYPE_ALARM_NOTIFY, NULL);
}

static void
cal_opened_cb (ECal *client, const GError *error, gpointer user_data)
{
	AlarmNotifyPrivate *priv;
	AlarmNotify *an = ALARM_NOTIFY (user_data);

	priv = an->priv;

	d (printf("%s:%d (cal_opened_cb) %s - Calendar Status %d%s%s%s\n", __FILE__, __LINE__, e_cal_get_uri (client), error ? error->code : 0, error ? " (" : "", error ? error->message : "", error ? ")" : ""));

	if (!error)
		alarm_queue_add_client (client);
	else {
		g_hash_table_remove (priv->uri_client_hash[e_cal_get_source_type (client)],
				     e_cal_get_uri (client));
		g_signal_handlers_disconnect_matched (G_OBJECT (client), G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, an);
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
alarm_notify_add_calendar (AlarmNotify *an, ECalSourceType source_type,  ESource *source, gboolean load_afterwards)
{
	AlarmNotifyPrivate *priv;
	ECal *client;
	EUri *e_uri;
	gchar *str_uri;
	gchar *pass_key;
	g_return_if_fail (an != NULL);
	g_return_if_fail (IS_ALARM_NOTIFY (an));

	/* Make sure the key used in for getting password is properly generated for all types of backends */
	priv = an->priv;
	str_uri = e_source_get_uri (source);
	e_uri = e_uri_new (str_uri);
	if (e_source_get_property (source, "auth-type"))
		pass_key = e_uri_to_string (e_uri, FALSE);
	else
		pass_key = g_strdup (str_uri);
	e_uri_free (e_uri);

	g_mutex_lock (an->priv->mutex);
	/* See if we already know about this uri */
	if (g_hash_table_lookup (priv->uri_client_hash[source_type], str_uri)) {
		g_mutex_unlock (an->priv->mutex);
		g_free (str_uri);
		g_free (pass_key);
		return;
	}
	/* if loading of this requires password and password is not currently availble in e-password
	   session skip this source loading. we do not really want to prompt for auth from alarm dameon*/

	if (e_source_get_property (source, "auth")) {
		const gchar *name = e_source_get_property (source, "auth-domain");

		if (!name)
			name = "Calendar";

		if (!e_passwords_get_password (name, pass_key)) {
			g_mutex_unlock (an->priv->mutex);
			g_free (str_uri);
			g_free (pass_key);

			return;
		}
	}

	client = e_auth_new_cal_from_source (source, source_type);

	if (client) {
		d (printf("%s:%d (alarm_notify_add_calendar) %s - Calendar Open Async... %p\n", __FILE__, __LINE__, str_uri, client));
		g_hash_table_insert (priv->uri_client_hash[source_type], g_strdup (str_uri), client);
		g_signal_connect (G_OBJECT (client), "cal_opened_ex", G_CALLBACK (cal_opened_cb), an);
		/* to resolve floating DATE-TIME properly */
		e_cal_set_default_timezone (client, config_data_get_timezone (), NULL);
		e_cal_open_async (client, FALSE);
	}

	g_free (str_uri);
	g_free (pass_key);
	g_mutex_unlock (an->priv->mutex);
}

void
alarm_notify_remove_calendar (AlarmNotify *an, ECalSourceType source_type, const gchar *str_uri)
{
	AlarmNotifyPrivate *priv;
	ECal *client;

	priv = an->priv;

	client = g_hash_table_lookup (priv->uri_client_hash[source_type], str_uri);
	if (client) {
		d (printf("%s:%d (alarm_notify_remove_calendar) - Removing Client %p\n", __FILE__, __LINE__, client));
		alarm_queue_remove_client (client, FALSE);
		g_hash_table_remove (priv->uri_client_hash[source_type], str_uri);
	}
}
