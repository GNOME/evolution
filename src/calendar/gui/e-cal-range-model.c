/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <libedataserver/libedataserver.h>

#include "e-util/e-util.h"
#include "e-cal-data-model.h"
#include "comp-util.h"

#include "e-cal-range-model.h"

/**
 * SECTION: e-cal-range-model
 * @include: calendar/gui/e-cal-range-model.h
 * @short_description: a calendar model gathering components in a specified range
 *
 * The #ECalRangeModel combines #ESourceRegistryWatcher and #ECalDataModel into
 * a single object for easier gathering of the components (events, tasks, memos)
 * in a specified range.
 *
 * The object is not thread-safe, it's meant to be used only from
 * the main/GUI thread.
 *
 * Since: 3.58
 **/

struct _ECalRangeModel {
	ESourceRegistryWatcher parent;

	GWeakRef client_cache_wr; /* EClientCache * */
	GWeakRef alert_sink_wr; /* EAlertSink * */
	ECalRangeModelSourceFilterFunc source_filter_func;
	gpointer source_filter_user_data;

	GCancellable *cancellable;
	ECalDataModel *events;
	ECalDataModel *tasks_memos;
	GHashTable *active_clients; /* gchar *uid ~> ECalClient * */

	time_t start;
	time_t end;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE,
	PROP_ALERT_SINK,
	N_PROPS
};

enum {
	COMPONENT_ADDED,
	COMPONENT_MODIFIED,
	COMPONENT_REMOVED,
	LAST_SIGNAL
};

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL];

static void ecrm_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ECalRangeModel, e_cal_range_model, E_TYPE_SOURCE_REGISTRY_WATCHER,
	G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER, ecrm_cal_data_model_subscriber_init))

static ECalDataModel *
ecrm_get_data_model_for_source (ECalRangeModel *self,
				ESource *source,
				const gchar **out_extension_name)
{
	ECalDataModel *data_model = NULL;
	const gchar *extension_name;

	extension_name = E_SOURCE_EXTENSION_CALENDAR;
	if (e_source_has_extension (source, extension_name)) {
		data_model = self->events;
	} else {
		extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
		if (e_source_has_extension (source, extension_name)) {
			data_model = self->tasks_memos;
		} else {
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			if (e_source_has_extension (source, extension_name)) {
				data_model = self->tasks_memos;
			}
		}
	}

	if (data_model && out_extension_name)
		*out_extension_name = extension_name;

	return data_model;
}

static GCancellable *
e_cal_range_model_submit_thread_job_cb (GObject *responder,
					const gchar *description,
					const gchar *alert_ident,
					const gchar *alert_arg_0,
					EAlertSinkThreadJobFunc func,
					gpointer user_data,
					GDestroyNotify free_user_data)
{
	ECalRangeModel *self = E_CAL_RANGE_MODEL (responder);
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable = NULL;

	alert_sink = e_cal_range_model_ref_alert_sink (self);
	activity = e_alert_sink_submit_thread_job (alert_sink, description, alert_ident, alert_arg_0,
		func, user_data, free_user_data);
	if (activity) {
		g_set_object (&cancellable, e_activity_get_cancellable (activity));
		g_clear_object (&activity);
	}
	g_clear_object (&alert_sink);

	return cancellable;
}

typedef struct _ClientOpenData {
	ECalRangeModel *self; /* not referenced */
	ESource *source; /* is referenced */
	const gchar *extension_name;
} ClientOpenData;

static void
client_open_data_free (gpointer ptr)
{
	ClientOpenData *cod = ptr;

	if (cod) {
		g_clear_object (&cod->source);
		g_free (cod);
	}
}

static void
e_cal_range_model_client_opened_cb (GObject *source_object,
				    GAsyncResult *result,
				    gpointer user_data)
{
	ClientOpenData *cod = user_data;
	EClient *client;
	GError *local_error = NULL;

	client = e_client_cache_get_client_finish (E_CLIENT_CACHE (source_object), result, &local_error);

	if (client) {
		ESource *source = e_client_get_source (client);
		ECalDataModel *data_model;

		if (g_cancellable_is_cancelled (cod->self->cancellable)) {
			g_clear_object (&client);
			client_open_data_free (cod);
			return;
		}

		data_model = ecrm_get_data_model_for_source (cod->self, source, NULL);

		if (data_model) {
			g_hash_table_insert (cod->self->active_clients, (gpointer) e_source_get_uid (e_client_get_source (client)), g_object_ref (client));

			/* only add it when the range is set */
			if (cod->self->start != cod->self->end)
				e_cal_data_model_add_client (data_model, E_CAL_CLIENT (client));
		} else {
			g_warn_if_reached ();
		}

		g_clear_object (&client);
	} else if (!g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED) && local_error) {
		EClientCache *client_cache;
		EAlertSink *alert_sink;
		EAlert *alert;
		const gchar *tag = NULL;
		ESourceRegistry *registry;
		gchar *full_name;

		if (g_strcmp0 (cod->extension_name, E_SOURCE_EXTENSION_CALENDAR) == 0)
			tag = "calendar:failed-open-calendar";
		else if (g_strcmp0 (cod->extension_name, E_SOURCE_EXTENSION_MEMO_LIST) == 0)
			tag = "calendar:failed-open-memos";
		else if (g_strcmp0 (cod->extension_name, E_SOURCE_EXTENSION_TASK_LIST) == 0)
			tag = "calendar:failed-open-tasks";

		alert_sink = e_cal_range_model_ref_alert_sink (cod->self);
		client_cache = e_cal_range_model_ref_client_cache (cod->self);

		if (!tag || !alert_sink || !client_cache) {
			g_warning ("%s: No error tag for extension '%s'; error was: %s", G_STRFUNC,
				cod->extension_name, local_error ? local_error->message : "Unknown error");
			client_open_data_free (cod);
			g_clear_object (&alert_sink);
			g_clear_object (&client_cache);
			return;
		}

		registry = e_client_cache_ref_registry (client_cache);
		full_name = e_util_get_source_full_name (registry, cod->source);

		alert = e_alert_new (tag, full_name, local_error ? local_error->message : _("Unknown error"), NULL);

		e_alert_sink_submit_alert (alert_sink, alert);

		g_free (full_name);
		g_clear_object (&registry);
		g_clear_object (&alert);
		g_clear_object (&alert_sink);
		g_clear_object (&client_cache);
	}

	client_open_data_free (cod);
}

static gboolean
e_cal_range_model_source_filter (ESourceRegistryWatcher *watcher,
				 ESource *source)
{
	ECalRangeModel *self = E_CAL_RANGE_MODEL (watcher);

	if (!self->source_filter_func || !e_source_get_enabled (source))
		return FALSE;

	return self->source_filter_func (source, self->source_filter_user_data);
}

static void
e_cal_range_model_source_appeared (ESourceRegistryWatcher *watcher,
				   ESource *source)
{
	ECalRangeModel *self = E_CAL_RANGE_MODEL (watcher);
	ECalDataModel *data_model;
	EClient *client;
	EClientCache *client_cache;
	const gchar *extension_name = NULL;

	data_model = ecrm_get_data_model_for_source (self, source, &extension_name);
	if (!data_model)
		return;

	client_cache = e_cal_range_model_ref_client_cache (self);
	if (!client_cache)
		return;

	client = e_client_cache_ref_cached_client (client_cache, source, extension_name);
	if (client) {
		if (E_IS_CAL_CLIENT (client)) {
			g_hash_table_insert (self->active_clients, (gpointer) e_source_get_uid (e_client_get_source (client)), g_object_ref (client));

			/* only add it when the range is set */
			if (self->start != self->end)
				e_cal_data_model_add_client (data_model, E_CAL_CLIENT (client));
		}

		g_clear_object (&client);
	} else {
		ClientOpenData *cod = g_new0 (ClientOpenData, 1);

		cod->self = self;
		cod->source = g_object_ref (source);
		cod->extension_name = extension_name;

		e_client_cache_get_client (client_cache, source, extension_name, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS,
			self->cancellable, e_cal_range_model_client_opened_cb, cod);
	}

	g_clear_object (&client_cache);
}

static void
e_cal_range_model_source_disappeared (ESourceRegistryWatcher *watcher,
				      ESource *source)
{
	ECalRangeModel *self = E_CAL_RANGE_MODEL (watcher);
	ECalDataModel *data_model;

	g_hash_table_remove (self->active_clients, e_source_get_uid (source));

	data_model = ecrm_get_data_model_for_source (self, source, NULL);
	if (data_model)
		e_cal_data_model_remove_client (data_model, e_source_get_uid (source));
}

static void
ecrm_data_subscriber_component_added (ECalDataModelSubscriber *subscriber,
				      ECalClient *client,
				      ECalComponent *comp)
{
	g_return_if_fail (E_IS_CAL_RANGE_MODEL (subscriber));

	g_signal_emit (subscriber, signals[COMPONENT_ADDED], 0, client, comp);
}

static void
ecrm_data_subscriber_component_modified (ECalDataModelSubscriber *subscriber,
					 ECalClient *client,
					 ECalComponent *comp)
{
	g_return_if_fail (E_IS_CAL_RANGE_MODEL (subscriber));

	g_signal_emit (subscriber, signals[COMPONENT_MODIFIED], 0, client, comp);
}

static void
ecrm_data_subscriber_component_removed (ECalDataModelSubscriber *subscriber,
					ECalClient *client,
					const gchar *uid,
					const gchar *rid)
{
	g_return_if_fail (E_IS_CAL_RANGE_MODEL (subscriber));

	g_signal_emit (subscriber, signals[COMPONENT_REMOVED], 0, client, uid, rid);
}

static void
ecrm_data_subscriber_freeze (ECalDataModelSubscriber *subscriber)
{
	g_return_if_fail (E_IS_CAL_RANGE_MODEL (subscriber));
}

static void
ecrm_data_subscriber_thaw (ECalDataModelSubscriber *subscriber)
{
	g_return_if_fail (E_IS_CAL_RANGE_MODEL (subscriber));
}

static void
e_cal_range_model_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec)
{
	ECalRangeModel *self = E_CAL_RANGE_MODEL (object);

	switch (prop_id) {
	case PROP_CLIENT_CACHE:
		g_weak_ref_set (&self->client_cache_wr, g_value_get_object (value));
		break;
	case PROP_ALERT_SINK:
		g_weak_ref_set (&self->alert_sink_wr, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_cal_range_model_get_property (GObject *object,
				guint prop_id,
				GValue *value,
				GParamSpec *pspec)
{
	ECalRangeModel *self = E_CAL_RANGE_MODEL (object);

	switch (prop_id) {
	case PROP_CLIENT_CACHE:
		g_value_take_object (value, e_cal_range_model_ref_client_cache (self));
		break;
	case PROP_ALERT_SINK:
		g_value_take_object (value, e_cal_range_model_ref_alert_sink (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_cal_range_model_constructed (GObject *object)
{
	ECalRangeModel *self = E_CAL_RANGE_MODEL (object);
	ESourceRegistry *registry;
	EClientCache *client_cache;

	G_OBJECT_CLASS (e_cal_range_model_parent_class)->constructed (object);

	client_cache = e_cal_range_model_ref_client_cache (self);
	registry = e_client_cache_ref_registry (client_cache);
	g_clear_object (&client_cache);

	self->events = e_cal_data_model_new (registry, e_cal_range_model_submit_thread_job_cb, G_OBJECT (self));
	e_cal_data_model_set_expand_recurrences (self->events, TRUE);

	self->tasks_memos = e_cal_data_model_new (registry, e_cal_range_model_submit_thread_job_cb, G_OBJECT (self));
	e_cal_data_model_set_expand_recurrences (self->tasks_memos, FALSE);

	g_clear_object (&registry);
}

static void
e_cal_range_model_dispose (GObject *object)
{
	ECalRangeModel *self = E_CAL_RANGE_MODEL (object);

	g_cancellable_cancel (self->cancellable);
	g_clear_pointer (&self->active_clients, g_hash_table_unref);

	G_OBJECT_CLASS (e_cal_range_model_parent_class)->dispose (object);
}

static void
e_cal_range_model_finalize (GObject *object)
{
	ECalRangeModel *self = E_CAL_RANGE_MODEL (object);

	g_weak_ref_clear (&self->client_cache_wr);
	g_weak_ref_clear (&self->alert_sink_wr);
	g_clear_object (&self->cancellable);
	g_clear_object (&self->events);
	g_clear_object (&self->tasks_memos);

	G_OBJECT_CLASS (e_cal_range_model_parent_class)->finalize (object);
}

static void
e_cal_range_model_class_init (ECalRangeModelClass *klass)
{
	ESourceRegistryWatcherClass *watcher_class;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_cal_range_model_set_property;
	object_class->get_property = e_cal_range_model_get_property;
	object_class->constructed = e_cal_range_model_constructed;
	object_class->dispose = e_cal_range_model_dispose;
	object_class->finalize = e_cal_range_model_finalize;

	watcher_class = E_SOURCE_REGISTRY_WATCHER_CLASS (klass);
	watcher_class->appeared = e_cal_range_model_source_appeared;
	watcher_class->disappeared = e_cal_range_model_source_disappeared;

	/**
	 * ECalRangeModel:client-cache:
	 *
	 * An #EClientCache to get #ECalClient-s from. It can be set only during construction.
	 *
	 * Since: 3.58
	 **/
	properties[PROP_CLIENT_CACHE] = g_param_spec_object ("client-cache", NULL, NULL,
		E_TYPE_CLIENT_CACHE,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * ECalRangeModel:alert-sink:
	 *
	 * An #EAlertSink to run thread jobs through and to notify about errors.
	 *
	 * Since: 3.58
	 **/
	properties[PROP_ALERT_SINK] = g_param_spec_object ("alert-sink", NULL, NULL,
		E_TYPE_ALERT_SINK,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

	/* void component_added		(ECalRangeModel *range_model,
					 ECalClient *client,
					 ECalComponent *comp,
					 gpointer user_data); */
	signals[COMPONENT_ADDED] = g_signal_new (
		"component-added",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 2, E_TYPE_CAL_CLIENT, E_TYPE_CAL_COMPONENT);

	/* void component_modified	(ECalRangeModel *range_model,
					 ECalClient *client,
					 ECalComponent *comp,
					 gpointer user_data); */
	signals[COMPONENT_MODIFIED] = g_signal_new (
		"component-modified",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 2, E_TYPE_CAL_CLIENT, E_TYPE_CAL_COMPONENT);

	/* void component_removed	(ECalRangeModel *range_model,
					 ECalClient *client,
					 const gchar *uid,
					 const gchar *rid,
					 gpointer user_data); */
	signals[COMPONENT_REMOVED] = g_signal_new (
		"component-removed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 3, E_TYPE_CAL_CLIENT, G_TYPE_STRING, G_TYPE_STRING);
}

static void
ecrm_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface)
{
	iface->component_added = ecrm_data_subscriber_component_added;
	iface->component_modified = ecrm_data_subscriber_component_modified;
	iface->component_removed = ecrm_data_subscriber_component_removed;
	iface->freeze = ecrm_data_subscriber_freeze;
	iface->thaw = ecrm_data_subscriber_thaw;
}

static void
e_cal_range_model_init (ECalRangeModel *self)
{
	self->cancellable = g_cancellable_new ();
	self->active_clients = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
	self->start = 0;
	self->end = 0;

	g_signal_connect (self, "filter",
		G_CALLBACK (e_cal_range_model_source_filter), NULL);
}

/**
 * e_cal_range_model_new:
 * @client_cache: an #EClientCache
 * @alert_sink: an #EAlertSink
 * @source_filter_func: a filter function for the sources to include
 * @source_filter_user_data: optional user data for @source_filter_func
 *
 * Created a new #ECalRangeModel, which gathers components from chosen
 * sources within a set time range. The @client_cache is used to get
 * the allowed clients from. The @alert_sink is used to schedule thread
 * operations and report errors.
 *
 * Returns: (transfer full): a new #ECalRangeModel
 *
 * Since: 3.58
 **/
ECalRangeModel *
e_cal_range_model_new (EClientCache *client_cache,
		       EAlertSink *alert_sink,
		       ECalRangeModelSourceFilterFunc source_filter_func,
		       gpointer source_filter_user_data)
{
	ECalRangeModel *self;
	ESourceRegistry *registry;

	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);
	g_return_val_if_fail (E_IS_ALERT_SINK (alert_sink), NULL);
	g_return_val_if_fail (source_filter_func, NULL);

	registry = e_client_cache_ref_registry (client_cache);

	self = g_object_new (E_TYPE_CAL_RANGE_MODEL,
		"registry", registry,
		"client-cache", client_cache,
		"alert-sink", alert_sink,
		NULL);

	g_clear_object (&registry);

	self->source_filter_func = source_filter_func;
	self->source_filter_user_data = source_filter_user_data;

	e_source_registry_watcher_reclaim (E_SOURCE_REGISTRY_WATCHER (self));

	return self;
}

/**
 * e_cal_range_model_ref_client_cache:
 * @self: an #ECalRangeModel
 *
 * Gets the used #EClientCache.
 *
 * Returns: (transfer full): the referenced #EClientCache the @self was created with
 *
 * Since: 3.58
 **/
EClientCache *
e_cal_range_model_ref_client_cache (ECalRangeModel *self)
{
	g_return_val_if_fail (E_IS_CAL_RANGE_MODEL (self), NULL);

	return g_weak_ref_get (&self->client_cache_wr);
}

/**
 * e_cal_range_model_ref_alert_sink:
 * @self: an #ECalRangeModel
 *
 * Gets the used #EAlertSink.
 *
 * Returns: (transfer full): the referenced #EAlertSink the @self was created with
 *
 * Since: 3.58
 **/
EAlertSink *
e_cal_range_model_ref_alert_sink (ECalRangeModel *self)
{
	g_return_val_if_fail (E_IS_CAL_RANGE_MODEL (self), NULL);

	return g_weak_ref_get (&self->alert_sink_wr);
}

/**
 * e_cal_range_model_set_timezone:
 * @self: an #ECalRangeModel
 * @zone: an #ICalTimezone
 *
 * Sets an #ICalTimezone to be used by the @self for the time calculations.
 *
 * Since: 3.58
 **/
void
e_cal_range_model_set_timezone (ECalRangeModel *self,
				ICalTimezone *zone)
{
	g_return_if_fail (E_IS_CAL_RANGE_MODEL (self));

	e_cal_data_model_set_timezone (self->events, zone);
	e_cal_data_model_set_timezone (self->tasks_memos, zone);
}

/**
 * e_cal_range_model_get_timezone:
 * @self: an #ECalRangeModel
 *
 * Gets an #ICalTimezone, previously set by the e_cal_range_model_set_timezone().
 *
 * Returns: (transfer none): a used #ICalTimezone
 *
 * Since: 3.58
 **/
ICalTimezone *
e_cal_range_model_get_timezone (ECalRangeModel *self)
{
	g_return_val_if_fail (E_IS_CAL_RANGE_MODEL (self), NULL);

	return e_cal_data_model_get_timezone (self->events);
}

/**
 * e_cal_range_model_set_range:
 * @self: an #ECalRangeModel
 * @start: the range start
 * @end: the range end
 *
 * Sets the time range to gather the components for.
 *
 * Since: 3.58
 **/
void
e_cal_range_model_set_range (ECalRangeModel *self,
			     time_t start,
			     time_t end)
{
	g_return_if_fail (E_IS_CAL_RANGE_MODEL (self));

	if (self->start == start && self->end == end)
		return;

	self->start = start;
	self->end = end;

	e_cal_data_model_remove_all_clients (self->events);
	e_cal_data_model_remove_all_clients (self->tasks_memos);

	e_cal_data_model_unsubscribe (self->events, E_CAL_DATA_MODEL_SUBSCRIBER (self));
	e_cal_data_model_unsubscribe (self->tasks_memos, E_CAL_DATA_MODEL_SUBSCRIBER (self));

	if (self->start != self->end) {
		GHashTableIter iter;
		gpointer value;

		e_cal_data_model_subscribe (self->events, E_CAL_DATA_MODEL_SUBSCRIBER (self), start, end);
		e_cal_data_model_subscribe (self->tasks_memos, E_CAL_DATA_MODEL_SUBSCRIBER (self), start, end);

		g_hash_table_iter_init (&iter, self->active_clients);
		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			EClient *client = value;

			if (client) {
				ECalDataModel *data_model;

				data_model = ecrm_get_data_model_for_source (self, e_client_get_source (client), NULL);
				if (data_model)
					e_cal_data_model_add_client (data_model, E_CAL_CLIENT (client));
			}
		}
	}
}

/**
 * e_cal_range_model_get_range:
 * @self: an #ECalRangeModel
 * @out_start: (optional) (out): return location to set the range start to, or %NULL
 * @out_end: (optional) (out): return location to set the range end to, or %NULL
 *
 * Gets what time range the @self gathers the components for.
 *
 * Since: 3.58
 **/
void
e_cal_range_model_get_range (ECalRangeModel *self,
			     time_t *out_start,
			     time_t *out_end)
{
	g_return_if_fail (E_IS_CAL_RANGE_MODEL (self));

	if (out_start)
		*out_start = self->start;
	if (out_end)
		*out_end = self->end;
}

/**
 * e_cal_range_model_get_components:
 * @self: an #ECalRangeModel
 * @start: the range start
 * @end: the range end
 *
 * Gets known #ECalComponent-s for the given time range.
 *
 * Returns: (transfer full) (element-type ECalComponent): a #GSList with an #ECalComponent objects
 *
 * Since: 3.58
 **/
GSList *
e_cal_range_model_get_components (ECalRangeModel *self,
				  time_t start,
				  time_t end)
{
	GSList *events, *tasks_memos;

	g_return_val_if_fail (E_IS_CAL_RANGE_MODEL (self), NULL);

	events = e_cal_data_model_get_components (self->events, start, end);
	tasks_memos = e_cal_data_model_get_components (self->tasks_memos, start, end);

	return g_slist_concat (events, tasks_memos);
}

/**
 * e_cal_range_model_prepare_dispose:
 * @self: an #ECalRangeModel
 *
 * Let the @self know that it will not be used any more. It's needed
 * to be called in the owner's dispose function, because the @self
 * also behaves like subscribed of an internal #ECalDataModel, which
 * references the subscribers, thus it causes a circular dependency,
 * which this function breaks.
 *
 * Since: 3.58
 **/
void
e_cal_range_model_prepare_dispose (ECalRangeModel *self)
{
	g_return_if_fail (E_IS_CAL_RANGE_MODEL (self));

	g_cancellable_cancel (self->cancellable);
	e_cal_data_model_set_disposing (self->events, TRUE);
	e_cal_data_model_set_disposing (self->tasks_memos, TRUE);

	/* the ECalDataModel references the subscriber, which causes a circular
	   dependency between the 'self' and the data model, thus break the circle here */
	e_cal_data_model_unsubscribe (self->events, E_CAL_DATA_MODEL_SUBSCRIBER (self));
	e_cal_data_model_unsubscribe (self->tasks_memos, E_CAL_DATA_MODEL_SUBSCRIBER (self));
}

/**
 * e_cal_range_model_clamp_to_minutes:
 * @self: an #ECalRangeModel
 * @start_tt: start time
 * @duration_minutes: duration in minutes
 * @out_start_minute: (out): location to set the start minute to
 * @out_duration_minutes: (out): location to set the clamped duration minutes to
 *
 * Clamps the @start_tt and @duration_minutes into the range used by the @self
 * and converts is to @out_start_minute as a minute starting after the range
 * start and lasting for @out_duration_minutes, which is minutes up to the range
 * end. In other words, the @out_start_minute and @out_duration_minutes is
 * always within the @self's range, except not in a timezone, but in the minutes
 * from the start of the range.
 *
 * Returns: whether the given @start_tt and @duration_minutes was in the @self's range;
 *    when %FALSE is returned the out arguments are left unchanged
 *
 * Since: 3.58
 **/
gboolean
e_cal_range_model_clamp_to_minutes (ECalRangeModel *self,
				    time_t start_tt,
				    guint duration_minutes,
				    guint *out_start_minute,
				    guint *out_duration_minutes)
{
	time_t clamp_start, clamp_end;

	g_return_val_if_fail (E_IS_CAL_RANGE_MODEL (self), FALSE);
	g_return_val_if_fail (out_start_minute != NULL, FALSE);
	g_return_val_if_fail (out_duration_minutes != NULL, FALSE);

	if (start_tt + (60 * duration_minutes) < self->start || start_tt >= self->end)
		return FALSE;

	if (start_tt < self->start)
		clamp_start = self->start;
	else
		clamp_start = start_tt;

	clamp_end = start_tt + (duration_minutes * 60);

	if (clamp_end > self->end)
		clamp_end = self->end;

	g_warn_if_fail (clamp_end >= clamp_start);

	*out_start_minute = (clamp_start - self->start) / 60;
	*out_duration_minutes = (clamp_end - clamp_start) / 60;

	return TRUE;
}
