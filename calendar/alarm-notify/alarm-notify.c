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
#include <config.h>
#endif

#include <string.h>
#include <camel/camel.h>
#include <libecal/e-cal-client.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserverui/e-passwords.h>
#include <libedataserverui/e-client-utils.h>

#include "alarm.h"
#include "alarm-notify.h"
#include "alarm-queue.h"
#include "config-data.h"

#define ALARM_NOTIFY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_ALARM_NOTIFY, AlarmNotifyPrivate))

#define APPLICATION_ID "org.gnome.EvolutionAlarmNotify"

struct _AlarmNotifyPrivate {
	/* Mapping from EUri's to LoadedClient structures */
	/* FIXME do we need per source type uri hashes? or perhaps we
	 * just need to hash based on source */
	GHashTable *uri_client_hash[E_CAL_CLIENT_SOURCE_TYPE_LAST];
        ESourceList *source_lists[E_CAL_CLIENT_SOURCE_TYPE_LAST];
	ESourceList *selected_calendars;
        GMutex *mutex;

	GSList *offline_sources;
	guint offline_timeout_id;
};

typedef struct {
	AlarmNotify *an;
	ESourceList *source_list;
	GList *removals;
} ProcessRemovalsData;

/* Forward Declarations */
static void	alarm_notify_initable_init	(GInitableIface *interface);

G_DEFINE_TYPE_WITH_CODE (
	AlarmNotify, alarm_notify, GTK_TYPE_APPLICATION,
	G_IMPLEMENT_INTERFACE (
		G_TYPE_INITABLE, alarm_notify_initable_init))

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

static gint
find_slist_source_uri_cb (gconstpointer a,
                          gconstpointer b)
{
	ESource *asource = (ESource *) a;
	const gchar *buri = b;
	gchar *auri;
	gint res;

	auri = e_source_get_uri (asource);
	res = g_strcmp0 (auri, buri);
	g_free (auri);

	return res;
}

static void
alarm_notify_list_changed_cb (ESourceList *source_list,
                              AlarmNotify *an)
{
	GSList *groups, *sources, *p, *q;
	ECalClientSourceType source_type = E_CAL_CLIENT_SOURCE_TYPE_LAST;
	ProcessRemovalsData prd;
	GList *l;
	gint i;

	g_signal_handlers_block_by_func (
		source_list, alarm_notify_list_changed_cb, an);

	/* Figure out the source type */
	for (i = 0; i < E_CAL_CLIENT_SOURCE_TYPE_LAST; i++) {
		if (source_list == an->priv->source_lists[i]) {
			source_type = i;
			break;
		}
	}
	if (source_type == E_CAL_CLIENT_SOURCE_TYPE_LAST)
		return;

	/* process the additions */
	groups = e_source_list_peek_groups (source_list);
	for (p = groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);

		sources = e_source_group_peek_sources (group);
		for (q = sources; q != NULL; q = q->next) {
			ESource *source = E_SOURCE (q->data);
			gchar *uri;
			const gchar *alarm;

			alarm = e_source_get_property (source, "alarm");

			if (alarm && (!g_ascii_strcasecmp (alarm, "false") ||
			    !g_ascii_strcasecmp (alarm, "never")))
				continue;

			uri = e_source_get_uri (source);
			if (!g_hash_table_lookup (
				an->priv->uri_client_hash[source_type], uri) &&
			    !g_slist_find_custom (
				an->priv->offline_sources, uri,
				find_slist_source_uri_cb)) {
				debug (("Adding Calendar %s", uri));
				alarm_notify_add_calendar (an, source_type, source);
			}
			g_free (uri);
		}
	}

	/* process the removals */
	prd.an = an;
	prd.source_list = an->priv->source_lists[source_type];
	prd.removals = NULL;
	g_hash_table_foreach (
		an->priv->uri_client_hash[source_type],
		(GHFunc) process_removal_in_hash, &prd);

	for (l = prd.removals; l; l = l->next) {
		debug (("Removing Calendar %s", (gchar *)l->data));
		alarm_notify_remove_calendar (an, source_type, l->data);
	}
	g_list_free (prd.removals);
	g_signal_handlers_unblock_by_func (
		source_list, alarm_notify_list_changed_cb, an);

}

static void
alarm_notify_load_calendars (AlarmNotify *an,
                             ECalClientSourceType source_type)
{
	ESourceList *source_list;
	GSList *groups, *sources, *p, *q;

	if (!e_cal_client_get_sources (&source_list, source_type, NULL)) {
		debug (("Cannont get sources"));
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
			const gchar *alarm;

			alarm = e_source_get_property (source, "alarm");

			if (alarm && (
				!g_ascii_strcasecmp (alarm, "false") ||
				!g_ascii_strcasecmp (alarm, "never")))
				continue;

			uri = e_source_get_uri (source);
			debug (("Loading Calendar %s", uri));
			alarm_notify_add_calendar (an, source_type, source);
			g_free (uri);

		}
	}

	e_source_list_sync (source_list, NULL);
	g_signal_connect_object (
		source_list, "changed",
		G_CALLBACK (alarm_notify_list_changed_cb), an, 0);
	an->priv->source_lists[source_type] = source_list;
}

static void
alarm_notify_dequeue_client (gpointer key,
                             ECalClient *client)
{
	alarm_queue_remove_client (client, TRUE);
}

static void
alarm_notify_finalize (GObject *object)
{
	AlarmNotifyPrivate *priv;
	gint ii;

	priv = ALARM_NOTIFY_GET_PRIVATE (object);

	if (priv->offline_timeout_id)
		g_source_remove (priv->offline_timeout_id);
	priv->offline_timeout_id = 0;
	g_slist_free_full (priv->offline_sources, g_object_unref);
	priv->offline_sources = NULL;

	for (ii = 0; ii < E_CAL_CLIENT_SOURCE_TYPE_LAST; ii++) {
		g_hash_table_foreach (
			priv->uri_client_hash[ii],
			(GHFunc) alarm_notify_dequeue_client, NULL);
		g_hash_table_destroy (priv->uri_client_hash[ii]);
	}

	alarm_queue_done ();
	alarm_done ();

	e_passwords_shutdown ();

	g_mutex_free (priv->mutex);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (alarm_notify_parent_class)->finalize (object);
}

static void
alarm_notify_startup (GApplication *application)
{
	GtkIconTheme *icon_theme;

	/* Chain up to parent's startup() method. */
	G_APPLICATION_CLASS (alarm_notify_parent_class)->startup (application);

	/* Keep the application running. */
	g_application_hold (application);

	config_data_init_debugging ();

	/* FIXME Ideally we should not use Camel libraries in calendar,
	 *       though it is the case currently for attachments.  Remove
	 *       this once that is fixed. */

	/* Initialize Camel's type system. */
	camel_object_get_type ();

	icon_theme = gtk_icon_theme_get_default ();
	gtk_icon_theme_append_search_path (icon_theme, EVOLUTION_ICONDIR);
}

static void
alarm_notify_activate (GApplication *application)
{
	/* Disregard.  This is just here to prevent the default
	 * activate method from running, which issues a warning
	 * if there are no handlers connected to this signal. */
}

static gboolean
alarm_notify_initable (GInitable *initable,
                       GCancellable *cancellable,
                       GError **error)
{
	/* XXX Just return TRUE for now.  We'll have use for this soon. */
	return TRUE;
}

static void
alarm_notify_class_init (AlarmNotifyClass *class)
{
	GObjectClass *object_class;
	GApplicationClass *application_class;

	g_type_class_add_private (class, sizeof (AlarmNotifyPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = alarm_notify_finalize;

	application_class = G_APPLICATION_CLASS (class);
	application_class->startup = alarm_notify_startup;
	application_class->activate = alarm_notify_activate;
}

static void
alarm_notify_initable_init (GInitableIface *interface)
{
	/* XXX Awkward name since we're missing an 'E' prefix. */
	interface->init = alarm_notify_initable;
}

static void
alarm_notify_init (AlarmNotify *an)
{
	gint ii;

	an->priv = ALARM_NOTIFY_GET_PRIVATE (an);
	an->priv->mutex = g_mutex_new ();
	an->priv->selected_calendars = config_data_get_calendars (
		"/apps/evolution/calendar/sources");

	for (ii = 0; ii < E_CAL_CLIENT_SOURCE_TYPE_LAST; ii++)
		an->priv->uri_client_hash[ii] = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) g_object_unref);

	alarm_queue_init (an);

	for (ii = 0; ii < E_CAL_CLIENT_SOURCE_TYPE_LAST; ii++)
		alarm_notify_load_calendars (an, ii);
}

ESourceList *
alarm_notify_get_selected_calendars (AlarmNotify *an)
{
	return an->priv->selected_calendars;
}

/**
 * alarm_notify_new:
 *
 * Creates a new #AlarmNotify object.
 *
 * Returns: a newly-created #AlarmNotify
 **/
AlarmNotify *
alarm_notify_new (GCancellable *cancellable,
                  GError **error)
{
	return g_initable_new (
		TYPE_ALARM_NOTIFY, cancellable, error,
		"application-id", APPLICATION_ID, NULL);
}

static gboolean
try_open_offline_timeout_cb (gpointer user_data)
{
	AlarmNotify *an = ALARM_NOTIFY (user_data);
	GSList *sources, *iter;

	g_return_val_if_fail (an != NULL, FALSE);
	g_return_val_if_fail (an->priv != NULL, FALSE);

	sources = an->priv->offline_sources;
	an->priv->offline_sources = NULL;
	an->priv->offline_timeout_id = 0;

	for (iter = sources; iter; iter = iter->next) {
		ESource *source = iter->data;

		alarm_notify_add_calendar (an,
			GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (source), "source-type")),
			source);
	}

	g_slist_free_full (sources, g_object_unref);

	return FALSE;
}

static void
client_opened_cb (GObject *source_object,
                  GAsyncResult *result,
                  gpointer user_data)
{
	ESource *source = E_SOURCE (source_object);
	AlarmNotify *an = ALARM_NOTIFY (user_data);
	EClient *client = NULL;
	ECalClient *cal_client;
	ECalClientSourceType source_type;
	const gchar *uri;
	GError *error = NULL;

	e_client_utils_open_new_finish (source, result, &client, &error);

	if (client == NULL) {
		if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_REPOSITORY_OFFLINE)) {
			if (an->priv->offline_timeout_id)
				g_source_remove (an->priv->offline_timeout_id);
			an->priv->offline_sources = g_slist_append (
				an->priv->offline_sources,
				g_object_ref (source));
			an->priv->offline_timeout_id =
				g_timeout_add_seconds (
				5 * 60, try_open_offline_timeout_cb, an);
		}

		g_clear_error (&error);

		return;
	}

	cal_client = E_CAL_CLIENT (client);
	source_type = e_cal_client_get_source_type (cal_client);
	uri = e_client_get_uri (client);

	g_hash_table_insert (
		an->priv->uri_client_hash[source_type],
		g_strdup (uri), cal_client);

	/* to resolve floating DATE-TIME properly */
	e_cal_client_set_default_timezone (
		cal_client, config_data_get_timezone ());

	alarm_queue_add_client (cal_client);
}

/**
 * alarm_notify_add_calendar:
 * @an: An alarm notification service.
 * @uri: URI of the calendar to load.
 *
 * Tells the alarm notification service to load a calendar and start monitoring
 * its alarms.  It can optionally be made to save the URI of this calendar so
 * that it can be loaded in the future when the alarm daemon starts up.
 **/
void
alarm_notify_add_calendar (AlarmNotify *an,
                           ECalClientSourceType source_type,
                           ESource *source)
{
	AlarmNotifyPrivate *priv;
	EClientSourceType client_source_type;
	EUri *e_uri;
	gchar *str_uri;
	gchar *pass_key;
	g_return_if_fail (an != NULL);
	g_return_if_fail (IS_ALARM_NOTIFY (an));

	/* Make sure the key used in for getting password is
	 * properly generated for all types of backends. */
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

	/* If loading of this requires password and password is not
	 * currently availble in e-password session, skip this source
	 * loading.  We do not really want to prompt for auth from
	 * the alarm dameon. */

	if (e_source_get_property (source, "auth")) {

		if (!e_passwords_get_password (NULL, pass_key)) {
			g_mutex_unlock (an->priv->mutex);
			g_free (str_uri);
			g_free (pass_key);

			return;
		}
	}

	debug (("%s - Calendar Open Async... %p", str_uri, source));

	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			client_source_type = E_CLIENT_SOURCE_TYPE_EVENTS;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			client_source_type = E_CLIENT_SOURCE_TYPE_TASKS;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			client_source_type = E_CLIENT_SOURCE_TYPE_MEMOS;
			break;
		default:
			g_warn_if_reached ();
			client_source_type = E_CLIENT_SOURCE_TYPE_LAST;
	}

	g_object_set_data (
		G_OBJECT (source), "source-type",
		GUINT_TO_POINTER (source_type));

	e_client_utils_open_new (
		source, client_source_type, TRUE, NULL,
		e_client_utils_authenticate_handler, NULL,
		client_opened_cb, an);

	g_free (str_uri);
	g_free (pass_key);
	g_mutex_unlock (an->priv->mutex);
}

void
alarm_notify_remove_calendar (AlarmNotify *an,
                              ECalClientSourceType source_type,
                              const gchar *str_uri)
{
	AlarmNotifyPrivate *priv;
	ECalClient *cal_client;
	GSList *in_offline;

	priv = an->priv;

	cal_client = g_hash_table_lookup (
		priv->uri_client_hash[source_type], str_uri);
	if (cal_client) {
		debug (("Removing Client %p", cal_client));
		alarm_queue_remove_client (cal_client, FALSE);
		g_hash_table_remove (priv->uri_client_hash[source_type], str_uri);
	}

	in_offline = g_slist_find_custom (
		priv->offline_sources, str_uri, find_slist_source_uri_cb);
	if (in_offline) {
		ESource *source = in_offline->data;

		priv->offline_sources = g_slist_remove (priv->offline_sources, source);
		if (!priv->offline_sources && priv->offline_timeout_id) {
			g_source_remove (priv->offline_timeout_id);
			priv->offline_timeout_id = 0;
		}

		g_object_unref (source);
	}
}
