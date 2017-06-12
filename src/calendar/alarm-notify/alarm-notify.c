/*
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

#include "evolution-config.h"

#include <string.h>
#include <camel/camel.h>

#include "e-util/e-util.h"

#include "alarm.h"
#include "alarm-notify.h"
#include "alarm-queue.h"
#include "config-data.h"

#define ALARM_NOTIFY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_ALARM_NOTIFY, AlarmNotifyPrivate))

#define APPLICATION_ID "org.gnome.EvolutionAlarmNotify"

struct _AlarmNotifyPrivate {
	ESourceRegistry *registry;
	ESourceRegistryWatcher *watcher;
	GHashTable *clients;
	GMutex mutex;
};

/* Forward Declarations */
static void	alarm_notify_initable_init	(GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (
	AlarmNotify, alarm_notify, GTK_TYPE_APPLICATION,
	G_IMPLEMENT_INTERFACE (
		G_TYPE_INITABLE, alarm_notify_initable_init))

static gboolean
an_watcher_filter_cb (ESourceRegistryWatcher *watcher,
		      ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	if (!e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR) &&
	    !e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST) &&
	    !e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
		return FALSE;

	return !e_source_has_extension (source, E_SOURCE_EXTENSION_ALARMS) ||
	    e_source_alarms_get_include_me (e_source_get_extension (source, E_SOURCE_EXTENSION_ALARMS));
}

static void
alarm_notify_dispose (GObject *object)
{
	AlarmNotifyPrivate *priv;
	GHashTableIter iter;
	gpointer client;

	priv = ALARM_NOTIFY_GET_PRIVATE (object);

	g_clear_object (&priv->registry);
	g_clear_object (&priv->watcher);

	g_hash_table_iter_init (&iter, priv->clients);
	while (g_hash_table_iter_next (&iter, NULL, &client))
		alarm_queue_remove_client (client, TRUE);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (alarm_notify_parent_class)->dispose (object);
}

static void
alarm_notify_finalize (GObject *object)
{
	AlarmNotifyPrivate *priv;

	priv = ALARM_NOTIFY_GET_PRIVATE (object);

	g_hash_table_destroy (priv->clients);

	alarm_queue_done ();
	alarm_done ();

	g_mutex_clear (&priv->mutex);

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
	AlarmNotify *an = ALARM_NOTIFY (application);

	if (g_application_get_is_remote (application)) {
		g_application_quit (application);
		return;
	}

	if (an->priv->registry != NULL) {
		an->priv->watcher = e_source_registry_watcher_new (an->priv->registry, NULL);

		g_signal_connect (an->priv->watcher, "filter",
			G_CALLBACK (an_watcher_filter_cb), an);

		g_signal_connect_swapped (an->priv->watcher, "appeared",
			G_CALLBACK (alarm_notify_add_calendar), an);

		g_signal_connect_swapped (an->priv->watcher, "disappeared",
			G_CALLBACK (alarm_notify_remove_calendar), an);

		e_source_registry_watcher_reclaim (an->priv->watcher);
	}
}

static gboolean
alarm_notify_initable (GInitable *initable,
                       GCancellable *cancellable,
                       GError **error)
{
	AlarmNotify *an = ALARM_NOTIFY (initable);

	an->priv->registry = e_source_registry_new_sync (cancellable, error);

	return (an->priv->registry != NULL);
}

static void
alarm_notify_class_init (AlarmNotifyClass *class)
{
	GObjectClass *object_class;
	GApplicationClass *application_class;

	g_type_class_add_private (class, sizeof (AlarmNotifyPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = alarm_notify_dispose;
	object_class->finalize = alarm_notify_finalize;

	application_class = G_APPLICATION_CLASS (class);
	application_class->startup = alarm_notify_startup;
	application_class->activate = alarm_notify_activate;
}

static void
alarm_notify_initable_init (GInitableIface *iface)
{
	/* XXX Awkward name since we're missing an 'E' prefix. */
	iface->init = alarm_notify_initable;
}

static void
alarm_notify_init (AlarmNotify *an)
{
	an->priv = ALARM_NOTIFY_GET_PRIVATE (an);
	g_mutex_init (&an->priv->mutex);

	an->priv->clients = g_hash_table_new_full (
		(GHashFunc) e_source_hash,
		(GEqualFunc) e_source_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) g_object_unref);

	alarm_queue_init (an);
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

static void
client_connect_cb (GObject *source_object,
                   GAsyncResult *result,
                   gpointer user_data)
{
	AlarmNotify *an = ALARM_NOTIFY (user_data);
	EClient *client;
	ESource *source;
	ECalClient *cal_client;
	GError *error = NULL;

	client = e_cal_client_connect_finish (result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		debug (("%s", error->message));
		g_error_free (error);
		return;
	}

	source = e_client_get_source (client);

	g_hash_table_insert (
		an->priv->clients,
		g_object_ref (source), client);

	cal_client = E_CAL_CLIENT (client);

	/* to resolve floating DATE-TIME properly */
	e_cal_client_set_default_timezone (
		cal_client, config_data_get_timezone ());

	alarm_queue_add_client (cal_client);
}

/**
 * alarm_notify_add_calendar:
 * @an: an #AlarmNotify
 * @source: the #ESource to create an #ECal from
 *
 * Tells the alarm notification service to load a calendar and start
 * monitoring its alarms.
 **/
void
alarm_notify_add_calendar (AlarmNotify *an,
                           ESource *source)
{
	ECalClientSourceType source_type;
	const gchar *extension_name;

	g_return_if_fail (IS_ALARM_NOTIFY (an));

	g_mutex_lock (&an->priv->mutex);

	/* Check if we already know about this ESource. */
	if (g_hash_table_lookup (an->priv->clients, source) != NULL) {
		g_mutex_unlock (&an->priv->mutex);
		return;
	}

	/* Skip disabled sources. */
	if (!e_source_registry_check_enabled (an->priv->registry, source)) {
		debug (("Skipping disabled source '%s' (%s)", e_source_get_display_name (source), e_source_get_uid (source)));
		g_mutex_unlock (&an->priv->mutex);
		return;
	}

	/* Check if this is an ESource we're interested in. */
	if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
		source_type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
	else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST))
		source_type = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
	else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
		source_type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
	else {
		g_mutex_unlock (&an->priv->mutex);
		return;
	}

	/* Check if alarms are even wanted on this ESource. */
	extension_name = E_SOURCE_EXTENSION_ALARMS;
	if (e_source_has_extension (source, extension_name)) {
		ESourceAlarms *extension;
		extension = e_source_get_extension (source, extension_name);
		if (!e_source_alarms_get_include_me (extension)) {
			g_mutex_unlock (&an->priv->mutex);
			return;
		}
	}

	debug (("Opening '%s' (%s) as %s client", e_source_get_display_name (source), e_source_get_uid (source),
		source_type == E_CAL_CLIENT_SOURCE_TYPE_TASKS ? "tasks" :
		source_type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS ? "memos" :
		source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS ? "events" : "???"));

	e_cal_client_connect (source, source_type, 30, NULL, client_connect_cb, an);

	g_mutex_unlock (&an->priv->mutex);
}

static gboolean
remove_that_or_collection_children_sources_cb (gpointer key,
					       gpointer value,
					       gpointer user_data)
{
	ESource *stored_source = key;
	ECalClient *cal_client = value;
	ESource *removing_source = user_data;
	gboolean remove;

	remove = stored_source == removing_source ||
		 e_source_equal (stored_source, removing_source) ||
		 (!e_source_get_enabled (removing_source) && e_source_has_extension (removing_source, E_SOURCE_EXTENSION_COLLECTION) &&
		 g_strcmp0 (e_source_get_uid (removing_source), e_source_get_parent (stored_source)) == 0);

	if (remove)
		alarm_queue_remove_client (cal_client, FALSE);

	return remove;
}

void
alarm_notify_remove_calendar (AlarmNotify *an,
                              ESource *source)
{
	g_return_if_fail (IS_ALARM_NOTIFY (an));
	g_return_if_fail (E_IS_SOURCE (source));

	g_object_ref (source);

	g_hash_table_foreach_remove (an->priv->clients, remove_that_or_collection_children_sources_cb, source);

	g_object_unref (source);
}
