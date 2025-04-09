/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "comp-util.h"
#include "e-cal-data-model.h"

#define LOCK_PROPS() g_rec_mutex_lock (&data_model->priv->props_lock)
#define UNLOCK_PROPS() g_rec_mutex_unlock (&data_model->priv->props_lock)

struct _ECalDataModelPrivate {
	GThread *main_thread;
	ESourceRegistry *registry;
	ECalDataModelSubmitThreadJobFunc submit_thread_job_func;
	GWeakRef *submit_thread_job_responder;
	GThreadPool *thread_pool;

	GRecMutex props_lock;	/* to guard all the below members */

	gboolean disposing;
	gboolean expand_recurrences;
	gboolean skip_cancelled;
	gchar *filter;
	gchar *full_filter;	/* to be used with views */
	ICalTimezone *zone;
	time_t range_start;
	time_t range_end;

	GHashTable *clients;	/* ESource::uid ~> ECalClient */
	GHashTable *views;	/* ECalClient ~> ViewData */
	GSList *subscribers;	/* ~> SubscriberData */

	guint32 views_update_freeze;
	gboolean views_update_required;
};

enum {
	PROP_0,
	PROP_EXPAND_RECURRENCES,
	PROP_TIMEZONE,
	PROP_SKIP_CANCELLED,
	PROP_REGISTRY
};

enum {
	VIEW_STATE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (ECalDataModel, e_cal_data_model, G_TYPE_OBJECT)

typedef struct _ComponentData {
	ECalComponent *component;
	time_t instance_start;
	time_t instance_end;
	gboolean is_detached;
} ComponentData;

typedef struct _ViewData {
	gint ref_count;
	GRecMutex lock;
	gboolean is_used;

	ECalClient *client;
	ECalClientView *view;
	gulong objects_added_id;
	gulong objects_modified_id;
	gulong objects_removed_id;
	gulong progress_id;
	gulong complete_id;

	GHashTable *components; /* ECalComponentId ~> ComponentData */
	GHashTable *lost_components; /* ECalComponentId ~> ComponentData; when re-running view, valid till 'complete' is received */
	gboolean received_complete;
	GSList *to_expand_recurrences; /* ICalComponent */
	GSList *expanded_recurrences; /* ComponentData */
	gint pending_expand_recurrences; /* how many is waiting to be processed */

	GCancellable *cancellable;
} ViewData;

typedef struct _SubscriberData {
	ECalDataModelSubscriber *subscriber;
	time_t range_start;
	time_t range_end;
} SubscriberData;

static ComponentData *
component_data_new (ECalComponent *comp,
		    time_t instance_start,
		    time_t instance_end,
		    gboolean is_detached)
{
	ComponentData *comp_data;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), NULL);

	comp_data = g_new0 (ComponentData, 1);
	comp_data->component = g_object_ref (comp);
	comp_data->instance_start = instance_start;
	comp_data->instance_end = instance_end;
	comp_data->is_detached = is_detached;

	return comp_data;
}

static void
component_data_free (gpointer ptr)
{
	ComponentData *comp_data = ptr;

	if (comp_data) {
		g_object_unref (comp_data->component);
		g_free (comp_data);
	}
}

static gboolean
component_data_equal (ComponentData *comp_data1,
		      ComponentData *comp_data2)
{
	ICalComponent *icomp1, *icomp2;
	ICalTime *tt1, *tt2;
	gchar *as_str1, *as_str2;
	gboolean equal;

	if (comp_data1 == comp_data2)
		return TRUE;

	if (!comp_data1 || !comp_data2 || !comp_data1->component || !comp_data2->component)
		return FALSE;

	if (comp_data1->instance_start != comp_data2->instance_start ||
	    comp_data1->instance_end != comp_data2->instance_end)
		return FALSE;

	icomp1 = e_cal_component_get_icalcomponent (comp_data1->component);
	icomp2 = e_cal_component_get_icalcomponent (comp_data2->component);

	if (!icomp1 || !icomp2 ||
	    i_cal_component_get_sequence (icomp1) != i_cal_component_get_sequence (icomp2) ||
	    g_strcmp0 (i_cal_component_get_uid (icomp1), i_cal_component_get_uid (icomp2)) != 0)
		return FALSE;

	tt1 = i_cal_component_get_recurrenceid (icomp1);
	tt2 = i_cal_component_get_recurrenceid (icomp2);
	if (((!tt1 || i_cal_time_is_valid_time (tt1)) ? 1 : 0) != ((!tt2 || i_cal_time_is_valid_time (tt2)) ? 1 : 0) ||
	    ((!tt1 || i_cal_time_is_null_time (tt1)) ? 1 : 0) != ((!tt2 || i_cal_time_is_null_time (tt2)) ? 1 : 0) ||
	    i_cal_time_compare (tt1, tt2) != 0) {
		g_clear_object (&tt1);
		g_clear_object (&tt2);
		return FALSE;
	}

	g_clear_object (&tt1);
	g_clear_object (&tt2);

	tt1 = i_cal_component_get_dtstamp (icomp1);
	tt2 = i_cal_component_get_dtstamp (icomp2);
	if (((!tt1 || i_cal_time_is_valid_time (tt1)) ? 1 : 0) != ((!tt2 || i_cal_time_is_valid_time (tt2)) ? 1 : 0) ||
	    ((!tt1 || i_cal_time_is_null_time (tt1)) ? 1 : 0) != ((!tt2 || i_cal_time_is_null_time (tt2)) ? 1 : 0) ||
	    i_cal_time_compare (tt1, tt2) != 0) {
		g_clear_object (&tt1);
		g_clear_object (&tt2);
		return FALSE;
	}

	g_clear_object (&tt1);
	g_clear_object (&tt2);

	/* Maybe not so effective compare, but might be still more effective
	   than updating whole UI with false notifications */
	as_str1 = i_cal_component_as_ical_string (icomp1);
	as_str2 = i_cal_component_as_ical_string (icomp2);

	equal = g_strcmp0 (as_str1, as_str2) == 0;

	g_free (as_str1);
	g_free (as_str2);

	return equal;
}

static ViewData *
view_data_new (ECalClient *client)
{
	ViewData *view_data;

	g_return_val_if_fail (E_IS_CAL_CLIENT (client), NULL);

	view_data = g_new0 (ViewData, 1);
	view_data->ref_count = 1;
	g_rec_mutex_init (&view_data->lock);
	view_data->is_used = TRUE;
	view_data->client = g_object_ref (client);
	view_data->components = g_hash_table_new_full (
		e_cal_component_id_hash, e_cal_component_id_equal,
		e_cal_component_id_free, component_data_free);

	return view_data;
}

static void
view_data_disconnect_view (ViewData *view_data)
{
	if (view_data && view_data->view) {
		#define disconnect(x) G_STMT_START { \
			if (view_data->x) { \
				g_signal_handler_disconnect (view_data->view, view_data->x); \
				view_data->x = 0; \
			} \
		} G_STMT_END

		disconnect (objects_added_id);
		disconnect (objects_modified_id);
		disconnect (objects_removed_id);
		disconnect (progress_id);
		disconnect (complete_id);

		#undef disconnect
	}
}

static ViewData *
view_data_ref (ViewData *view_data)
{
	g_return_val_if_fail (view_data != NULL, NULL);

	g_atomic_int_inc (&view_data->ref_count);

	return view_data;
}

static void
view_data_unref (gpointer ptr)
{
	ViewData *view_data = ptr;

	if (view_data) {
		if (g_atomic_int_dec_and_test  (&view_data->ref_count)) {
			view_data_disconnect_view (view_data);
			if (view_data->cancellable)
				g_cancellable_cancel (view_data->cancellable);
			g_clear_object (&view_data->cancellable);
			g_clear_object (&view_data->client);
			g_clear_object (&view_data->view);
			g_hash_table_destroy (view_data->components);
			if (view_data->lost_components)
				g_hash_table_destroy (view_data->lost_components);
			g_slist_free_full (view_data->to_expand_recurrences, g_object_unref);
			g_slist_free_full (view_data->expanded_recurrences, component_data_free);
			g_rec_mutex_clear (&view_data->lock);
			g_free (view_data);
		}
	}
}

static void
view_data_lock (ViewData *view_data)
{
	g_return_if_fail (view_data != NULL);

	g_rec_mutex_lock (&view_data->lock);
}

static void
view_data_unlock (ViewData *view_data)
{
	g_return_if_fail (view_data != NULL);

	g_rec_mutex_unlock (&view_data->lock);
}

static SubscriberData *
subscriber_data_new (ECalDataModelSubscriber *subscriber,
		     time_t range_start,
		     time_t range_end)
{
	SubscriberData *subs_data;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL_SUBSCRIBER (subscriber), NULL);

	subs_data = g_new0 (SubscriberData, 1);
	subs_data->subscriber = g_object_ref (subscriber);
	subs_data->range_start = range_start;
	subs_data->range_end = range_end;

	return subs_data;
}

static void
subscriber_data_free (gpointer ptr)
{
	SubscriberData *subs_data = ptr;

	if (subs_data) {
		g_clear_object (&subs_data->subscriber);
		g_free (subs_data);
	}
}

typedef struct _ViewStateChangedData {
	ECalDataModel *data_model;
	ECalClientView *view;
	ECalDataModelViewState state;
	guint percent;
	gchar *message;
	GError *error;
} ViewStateChangedData;

static void
view_state_changed_data_free (gpointer ptr)
{
	ViewStateChangedData *vscd = ptr;

	if (vscd) {
		g_clear_object (&vscd->data_model);
		g_clear_object (&vscd->view);
		g_clear_error (&vscd->error);
		g_free (vscd->message);
		g_slice_free (ViewStateChangedData, vscd);
	}
}

static gboolean
cal_data_model_emit_view_state_changed_timeout_cb (gpointer user_data)
{
	ViewStateChangedData *vscd = user_data;

	g_return_val_if_fail (vscd != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (vscd->data_model), FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT_VIEW (vscd->view), FALSE);

	g_signal_emit (vscd->data_model, signals[VIEW_STATE_CHANGED], 0,
		vscd->view, vscd->state, vscd->percent, vscd->message, vscd->error);

	return FALSE;
}

static void
cal_data_model_emit_view_state_changed (ECalDataModel *data_model,
					ECalClientView *view,
					ECalDataModelViewState state,
					guint percent,
					const gchar *message,
					const GError *error)
{
	ViewStateChangedData *vscd;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (E_IS_CAL_CLIENT_VIEW (view));

	if (e_cal_data_model_get_disposing (data_model))
		return;

	vscd = g_slice_new0 (ViewStateChangedData);
	vscd->data_model = g_object_ref (data_model);
	vscd->view = g_object_ref (view);
	vscd->state = state;
	vscd->percent = percent;
	vscd->message = g_strdup (message);
	vscd->error = error ? g_error_copy (error) : NULL;

	g_timeout_add_full (G_PRIORITY_DEFAULT, 1,
		cal_data_model_emit_view_state_changed_timeout_cb,
		vscd, view_state_changed_data_free);
}

typedef void (* InternalThreadJobFunc) (ECalDataModel *data_model, gpointer user_data);

typedef struct _InternalThreadJobData {
	ECalDataModel *data_model;
	InternalThreadJobFunc func;
	gpointer user_data;
} InternalThreadJobData;

static void
cal_data_model_internal_thread_job_func (gpointer data,
					 gpointer user_data)
{
	InternalThreadJobData *job_data = data;

	g_return_if_fail (job_data != NULL);
	g_return_if_fail (job_data->func != NULL);

	job_data->func (job_data->data_model, job_data->user_data);

	g_object_unref (job_data->data_model);
	g_slice_free (InternalThreadJobData, job_data);
}

static void
cal_data_model_submit_internal_thread_job (ECalDataModel *data_model,
					   InternalThreadJobFunc func,
					   gpointer user_data)
{
	InternalThreadJobData *job_data;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (func != NULL);

	job_data = g_slice_new0 (InternalThreadJobData);
	job_data->data_model = g_object_ref (data_model);
	job_data->func = func;
	job_data->user_data = user_data;

	g_thread_pool_push (data_model->priv->thread_pool, job_data, NULL);
}

typedef struct _SubmitThreadJobData {
	ECalDataModel *data_model;
	const gchar *description;
	const gchar *alert_ident;
	const gchar *alert_arg_0;
	EAlertSinkThreadJobFunc func;
	gpointer user_data;
	GDestroyNotify free_user_data;

	GCancellable *cancellable;
	gboolean finished;
	GMutex mutex;
	GCond cond;
} SubmitThreadJobData;

static gboolean
cal_data_model_call_submit_thread_job (gpointer user_data)
{
	SubmitThreadJobData *stj_data = user_data;
	GObject *responder;

	g_return_val_if_fail (stj_data != NULL, FALSE);

	g_mutex_lock (&stj_data->mutex);
	responder = g_weak_ref_get (stj_data->data_model->priv->submit_thread_job_responder);

	stj_data->cancellable = stj_data->data_model->priv->submit_thread_job_func (
		responder, stj_data->description, stj_data->alert_ident, stj_data->alert_arg_0,
		stj_data->func, stj_data->user_data, stj_data->free_user_data);

	g_clear_object (&responder);

	stj_data->finished = TRUE;
	g_cond_signal (&stj_data->cond);
	g_mutex_unlock (&stj_data->mutex);

	return FALSE;
}

/**
 * e_cal_data_model_submit_thread_job:
 * @data_model: an #ECalDataModel
 * @description: user-friendly description of the job, to be shown in UI
 * @alert_ident: in case of an error, this alert identificator is used
 *    for EAlert construction
 * @alert_arg_0: (allow-none): in case of an error, use this string as
 *    the first argument to the EAlert construction; the second argument
 *    is the actual error message; can be #NULL, in which case only
 *    the error message is passed to the EAlert construction
 * @func: function to be run in a dedicated thread
 * @user_data: (allow-none): custom data passed into @func; can be #NULL
 * @free_user_data: (allow-none): function to be called on @user_data,
 *   when the job is over; can be #NULL
 *
 * Runs the @func in a dedicated thread. Any error is propagated to UI.
 * The cancellable passed into the @func is a #CamelOperation, thus
 * the caller can overwrite progress and description message on it.
 *
 * Returns: (transfer full): Newly created #GCancellable on success.
 *   The caller is responsible to g_object_unref() it when done with it.
 *
 * Note: The @free_user_data, if set, is called in the main thread.
 *
 * Note: This is a blocking call, it waits until the thread job is submitted.
 *
 * Since: 3.16
 **/
GCancellable *
e_cal_data_model_submit_thread_job (ECalDataModel *data_model,
				    const gchar *description,
				    const gchar *alert_ident,
				    const gchar *alert_arg_0,
				    EAlertSinkThreadJobFunc func,
				    gpointer user_data,
				    GDestroyNotify free_user_data)
{
	SubmitThreadJobData stj_data;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), NULL);
	g_return_val_if_fail (data_model->priv->submit_thread_job_func != NULL, NULL);

	if (g_thread_self () == data_model->priv->main_thread) {
		GCancellable *cancellable;
		GObject *responder;

		responder = g_weak_ref_get (data_model->priv->submit_thread_job_responder);
		cancellable = data_model->priv->submit_thread_job_func (
			responder, description, alert_ident, alert_arg_0,
			func, user_data, free_user_data);
		g_clear_object (&responder);

		return cancellable;
	}

	stj_data.data_model = data_model;
	stj_data.description = description;
	stj_data.alert_ident = alert_ident;
	stj_data.alert_arg_0 = alert_arg_0;
	stj_data.func = func;
	stj_data.user_data = user_data;
	stj_data.free_user_data = free_user_data;
	stj_data.cancellable = NULL;
	stj_data.finished = FALSE;
	g_mutex_init (&stj_data.mutex);
	g_cond_init (&stj_data.cond);

	g_timeout_add (1, cal_data_model_call_submit_thread_job, &stj_data);

	g_mutex_lock (&stj_data.mutex);
	while (!stj_data.finished) {
		g_cond_wait (&stj_data.cond, &stj_data.mutex);
	}
	g_mutex_unlock (&stj_data.mutex);

	g_cond_clear (&stj_data.cond);
	g_mutex_clear (&stj_data.mutex);

	return stj_data.cancellable;
}

typedef void (* ECalDataModelForeachSubscriberFunc) (ECalDataModel *data_model,
						     ECalClient *client,
						     ECalDataModelSubscriber *subscriber,
						     gpointer user_data);

static void
cal_data_model_foreach_subscriber_in_range (ECalDataModel *data_model,
					    ECalClient *client,
					    time_t in_range_start,
					    time_t in_range_end,
					    ECalDataModelForeachSubscriberFunc func,
					    gpointer user_data)
{
	GSList *link;

	g_return_if_fail (func != NULL);

	LOCK_PROPS ();

	if (in_range_end == (time_t) 0) {
		in_range_end = in_range_start;
	}

	for (link = data_model->priv->subscribers; link; link = g_slist_next (link)) {
		SubscriberData *subs_data = link->data;

		if ((in_range_start == (time_t) 0 && in_range_end == (time_t) 0) ||
		    (subs_data->range_start == (time_t) 0 && subs_data->range_end == (time_t) 0) ||
		    (subs_data->range_start <= in_range_end && subs_data->range_end >= in_range_start))
			func (data_model, client, subs_data->subscriber, user_data);
	}

	UNLOCK_PROPS ();
}

static void
cal_data_model_foreach_subscriber (ECalDataModel *data_model,
				   ECalClient *client,
				   ECalDataModelForeachSubscriberFunc func,
				   gpointer user_data)
{
	g_return_if_fail (func != NULL);

	cal_data_model_foreach_subscriber_in_range (data_model, client, (time_t) 0, (time_t) 0, func, user_data);
}

static void
cal_data_model_freeze_subscriber_cb (ECalDataModel *data_model,
				     ECalClient *client,
				     ECalDataModelSubscriber *subscriber,
				     gpointer user_data)
{
	e_cal_data_model_subscriber_freeze (subscriber);
}

static void
cal_data_model_thaw_subscriber_cb (ECalDataModel *data_model,
				   ECalClient *client,
				   ECalDataModelSubscriber *subscriber,
				   gpointer user_data)
{
	e_cal_data_model_subscriber_thaw (subscriber);
}

static void
cal_data_model_freeze_all_subscribers (ECalDataModel *data_model)
{
	cal_data_model_foreach_subscriber (data_model, NULL, cal_data_model_freeze_subscriber_cb, NULL);
}

static void
cal_data_model_thaw_all_subscribers (ECalDataModel *data_model)
{
	cal_data_model_foreach_subscriber (data_model, NULL, cal_data_model_thaw_subscriber_cb, NULL);
}

static void
cal_data_model_gather_subscribers_cb (ECalDataModel *data_model,
				      ECalClient *client,
				      ECalDataModelSubscriber *subscriber,
				      gpointer user_data)
{
	GHashTable *subscribers = user_data;

	g_return_if_fail (subscribers != NULL);

	g_hash_table_insert (subscribers, g_object_ref (subscriber), NULL);
}

static void
cal_data_model_add_component_cb (ECalDataModel *data_model,
				 ECalClient *client,
				 ECalDataModelSubscriber *subscriber,
				 gpointer user_data)
{
	ECalComponent *comp = user_data;

	g_return_if_fail (comp != NULL);

	e_cal_data_model_subscriber_component_added (subscriber, client, comp);
}

static void
cal_data_model_modify_component_cb (ECalDataModel *data_model,
				    ECalClient *client,
				    ECalDataModelSubscriber *subscriber,
				    gpointer user_data)
{
	ECalComponent *comp = user_data;

	g_return_if_fail (comp != NULL);

	e_cal_data_model_subscriber_component_modified (subscriber, client, comp);
}

static void
cal_data_model_remove_one_view_component_cb (ECalDataModel *data_model,
					     ECalClient *client,
					     ECalDataModelSubscriber *subscriber,
					     gpointer user_data)
{
	const ECalComponentId *id = user_data;

	g_return_if_fail (id != NULL);

	e_cal_data_model_subscriber_component_removed (subscriber, client,
		e_cal_component_id_get_uid (id),
		e_cal_component_id_get_rid (id));
}

static void
cal_data_model_remove_components (ECalDataModel *data_model,
				  ECalClient *client,
				  GHashTable *components,
				  GHashTable *also_remove_from)
{
	GList *ids, *ilink;

	g_return_if_fail (data_model != NULL);
	g_return_if_fail (components != NULL);

	cal_data_model_freeze_all_subscribers (data_model);

	ids = g_hash_table_get_keys (components);

	for (ilink = ids; ilink; ilink = g_list_next (ilink)) {
		ECalComponentId *id = ilink->data;
		ComponentData *comp_data;
		time_t instance_start = (time_t) 0, instance_end = (time_t) 0;

		if (!id)
			continue;

		/* Try to limit which subscribers will be notified about removal */
		comp_data = g_hash_table_lookup (components, id);
		if (comp_data) {
			instance_start = comp_data->instance_start;
			instance_end = comp_data->instance_end;
		}

		cal_data_model_foreach_subscriber_in_range (data_model, client,
			instance_start, instance_end,
			cal_data_model_remove_one_view_component_cb, id);

		if (also_remove_from)
			g_hash_table_remove (also_remove_from, id);
	}

	g_list_free (ids);

	cal_data_model_thaw_all_subscribers (data_model);
}

static void
cal_data_model_calc_range (ECalDataModel *data_model,
			   time_t *range_start,
			   time_t *range_end)
{
	GSList *link;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (range_start != NULL);
	g_return_if_fail (range_end != NULL);

	*range_start = (time_t) 0;
	*range_end = (time_t) 0;

	LOCK_PROPS ();

	for (link = data_model->priv->subscribers; link; link = g_slist_next (link)) {
		SubscriberData *subs_data = link->data;

		if (!subs_data)
			continue;

		if (subs_data->range_start == (time_t) 0 && subs_data->range_end == (time_t) 0) {
			*range_start = (time_t) 0;
			*range_end = (time_t) 0;
			break;
		}

		if (link == data_model->priv->subscribers) {
			*range_start = subs_data->range_start;
			*range_end = subs_data->range_end;
		} else {
			if (*range_start > subs_data->range_start)
				*range_start = subs_data->range_start;
			if (*range_end < subs_data->range_end)
				*range_end = subs_data->range_end;
		}
	}

	UNLOCK_PROPS ();
}

static gboolean
cal_data_model_update_full_filter (ECalDataModel *data_model)
{
	gchar *filter;
	time_t range_start, range_end;
	gboolean changed;

	LOCK_PROPS ();

	cal_data_model_calc_range (data_model, &range_start, &range_end);

	if (range_start != (time_t) 0 || range_end != (time_t) 0) {
		gchar *iso_start, *iso_end;
		const gchar *default_tzloc = NULL;

		iso_start = isodate_from_time_t (range_start);
		iso_end = isodate_from_time_t (range_end);

		if (data_model->priv->zone && data_model->priv->zone != i_cal_timezone_get_utc_timezone ())
			default_tzloc = i_cal_timezone_get_location (data_model->priv->zone);
		if (!default_tzloc)
			default_tzloc = "";

		if (data_model->priv->filter) {
			filter = g_strdup_printf (
				"(and (occur-in-time-range? (make-time \"%s\") (make-time \"%s\") \"%s\") %s)",
				iso_start, iso_end, default_tzloc, data_model->priv->filter);
		} else {
			filter = g_strdup_printf (
				"(occur-in-time-range? (make-time \"%s\") (make-time \"%s\") \"%s\")",
				iso_start, iso_end, default_tzloc);
		}

		g_free (iso_start);
		g_free (iso_end);
	} else if (data_model->priv->filter) {
		filter = g_strdup (data_model->priv->filter);
	} else {
		filter = g_strdup ("#t");
	}

	changed = g_strcmp0 (data_model->priv->full_filter, filter) != 0;

	if (changed) {
		g_free (data_model->priv->full_filter);
		data_model->priv->full_filter = filter;
	} else {
		g_free (filter);
	}

	UNLOCK_PROPS ();

	return changed;
}

/* This consumes the comp_data - not so nice, but simpler
   than adding reference counter for the structure */
static void
cal_data_model_process_added_component (ECalDataModel *data_model,
					ViewData *view_data,
					ComponentData *comp_data,
					GHashTable *known_instances)
{
	ECalComponentId *id, *old_id = NULL;
	ComponentData *old_comp_data = NULL;
	time_t old_instance_start = (time_t) 0, old_instance_end = (time_t) 0;
	gboolean comp_data_equal;

	g_return_if_fail (data_model != NULL);
	g_return_if_fail (view_data != NULL);
	g_return_if_fail (comp_data != NULL);

	id = e_cal_component_get_id (comp_data->component);
	g_return_if_fail (id != NULL);

	view_data_lock (view_data);

	if (!old_comp_data && view_data->lost_components)
		old_comp_data = g_hash_table_lookup (view_data->lost_components, id);

	if (!old_comp_data && known_instances)
		old_comp_data = g_hash_table_lookup (known_instances, id);

	if (!old_comp_data)
		old_comp_data = g_hash_table_lookup (view_data->components, id);

	if (old_comp_data) {
		/* It can be a previously added detached instance received
		   during recurrences expand */
		if (!comp_data->is_detached)
			comp_data->is_detached = old_comp_data->is_detached;
	}

	comp_data_equal = component_data_equal (comp_data, old_comp_data);

	if (old_comp_data) {
		old_id = e_cal_component_get_id (old_comp_data->component);
		old_instance_start = old_comp_data->instance_start;
		old_instance_end = old_comp_data->instance_end;
	}

	if (view_data->lost_components)
		g_hash_table_remove (view_data->lost_components, id);

	if (known_instances)
		g_hash_table_remove (known_instances, id);

	/* Note: old_comp_data is freed or NULL now */

	/* 'id' is stolen by view_data->components */
	g_hash_table_insert (view_data->components, id, comp_data);

	if (!comp_data_equal) {
		if (!old_comp_data) {
			cal_data_model_foreach_subscriber_in_range (data_model, view_data->client,
				comp_data->instance_start, comp_data->instance_end,
				cal_data_model_add_component_cb, comp_data->component);
		} else if (comp_data->instance_start != old_instance_start ||
			   comp_data->instance_end != old_instance_end) {
			/* Component moved to a different time; some subscribers may lose it,
			   some may just modify it, some may have it added. */
			GHashTable *old_subscribers, *new_subscribers;
			GHashTableIter iter;
			gpointer key;

			old_subscribers = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
			new_subscribers = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);

			cal_data_model_foreach_subscriber_in_range (data_model, view_data->client,
				old_instance_start, old_instance_end,
				cal_data_model_gather_subscribers_cb, old_subscribers);

			cal_data_model_foreach_subscriber_in_range (data_model, view_data->client,
				comp_data->instance_start, comp_data->instance_end,
				cal_data_model_gather_subscribers_cb, new_subscribers);

			g_hash_table_iter_init (&iter, old_subscribers);
			while (g_hash_table_iter_next (&iter, &key, NULL)) {
				ECalDataModelSubscriber *subscriber = key;

				/* If in both hashes, then the subscriber can be notified with 'modified',
				   otherwise the component had been 'removed' for it. */
				if (g_hash_table_remove (new_subscribers, subscriber))
					e_cal_data_model_subscriber_component_modified (subscriber, view_data->client, comp_data->component);
				else if (old_id)
					e_cal_data_model_subscriber_component_removed (subscriber, view_data->client,
						e_cal_component_id_get_uid (old_id),
						e_cal_component_id_get_rid (old_id));
			}

			/* Those which left in the new_subscribers have the component added. */
			g_hash_table_iter_init (&iter, new_subscribers);
			while (g_hash_table_iter_next (&iter, &key, NULL)) {
				ECalDataModelSubscriber *subscriber = key;

				e_cal_data_model_subscriber_component_added (subscriber, view_data->client, comp_data->component);
			}

			g_hash_table_destroy (old_subscribers);
			g_hash_table_destroy (new_subscribers);
		} else {
			cal_data_model_foreach_subscriber_in_range (data_model, view_data->client,
				comp_data->instance_start, comp_data->instance_end,
				cal_data_model_modify_component_cb, comp_data->component);
		}
	}

	view_data_unlock (view_data);

	e_cal_component_id_free (old_id);
}

typedef struct _GatherComponentsData {
	const gchar *uid;
	GList **pcomponent_ids; /* ECalComponentId, can be owned by the hash table */
	GHashTable *component_ids_hash;
	gboolean copy_ids;
	gboolean all_instances; /* FALSE to get only nondetached component instances */
} GatherComponentsData;

static void
cal_data_model_gather_components (gpointer key,
				  gpointer value,
				  gpointer user_data)
{
	ECalComponentId *id = key;
	ComponentData *comp_data = value;
	GatherComponentsData *gather_data = user_data;

	g_return_if_fail (id != NULL);
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (gather_data != NULL);
	g_return_if_fail (gather_data->pcomponent_ids != NULL || gather_data->component_ids_hash != NULL);
	g_return_if_fail (gather_data->pcomponent_ids == NULL || gather_data->component_ids_hash == NULL);

	if ((gather_data->all_instances || !comp_data->is_detached) && g_strcmp0 (e_cal_component_id_get_uid (id), gather_data->uid) == 0) {
		if (gather_data->component_ids_hash) {
			ComponentData *comp_data_copy;

			comp_data_copy = component_data_new (comp_data->component,
				comp_data->instance_start, comp_data->instance_end,
				comp_data->is_detached);

			if (gather_data->copy_ids) {
				g_hash_table_insert (gather_data->component_ids_hash,
					e_cal_component_id_copy (id), comp_data_copy);
			} else {
				g_hash_table_insert (gather_data->component_ids_hash, id, comp_data_copy);
			}
		} else if (gather_data->copy_ids) {
			*gather_data->pcomponent_ids = g_list_prepend (*gather_data->pcomponent_ids,
				e_cal_component_id_copy (id));
		} else {
			*gather_data->pcomponent_ids = g_list_prepend (*gather_data->pcomponent_ids, id);
		}
	}
}

typedef struct _NotifyRecurrencesData {
	ECalDataModel *data_model;
	ECalClient *client;
} NotifyRecurrencesData;

static gboolean
cal_data_model_notify_recurrences_cb (gpointer user_data)
{
	NotifyRecurrencesData *notif_data = user_data;
	ECalDataModel *data_model;
	ViewData *view_data;

	g_return_val_if_fail (notif_data != NULL, FALSE);

	data_model = notif_data->data_model;

	LOCK_PROPS ();

	view_data = g_hash_table_lookup (data_model->priv->views, notif_data->client);
	if (view_data)
		view_data_ref (view_data);

	UNLOCK_PROPS ();

	if (view_data) {
		GHashTable *gathered_uids;
		GHashTable *known_instances;
		GSList *expanded_recurrences, *link;

		view_data_lock (view_data);
		expanded_recurrences = view_data->expanded_recurrences;
		view_data->expanded_recurrences = NULL;

		cal_data_model_freeze_all_subscribers (data_model);

		gathered_uids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		known_instances = g_hash_table_new_full (
			(GHashFunc) e_cal_component_id_hash, (GEqualFunc) e_cal_component_id_equal,
			(GDestroyNotify) e_cal_component_id_free, component_data_free);

		for (link = expanded_recurrences; link && view_data->is_used; link = g_slist_next (link)) {
			ComponentData *comp_data = link->data;
			ICalComponent *icomp;
			const gchar *uid;

			if (!comp_data)
				continue;

			icomp = e_cal_component_get_icalcomponent (comp_data->component);
			if (!icomp || !i_cal_component_get_uid (icomp))
				continue;

			uid = i_cal_component_get_uid (icomp);

			if (!g_hash_table_contains (gathered_uids, uid)) {
				GatherComponentsData gather_data;

				gather_data.uid = uid;
				gather_data.pcomponent_ids = NULL;
				gather_data.component_ids_hash = known_instances;
				gather_data.copy_ids = TRUE;
				gather_data.all_instances = FALSE;

				g_hash_table_foreach (view_data->components,
					cal_data_model_gather_components, &gather_data);

				g_hash_table_insert (gathered_uids, g_strdup (uid), GINT_TO_POINTER (1));
			}

			/* Steal the comp_data */
			link->data = NULL;

			cal_data_model_process_added_component (data_model, view_data, comp_data, known_instances);
		}

		if (view_data->is_used && g_hash_table_size (known_instances) > 0) {
			cal_data_model_remove_components (data_model, view_data->client, known_instances, view_data->components);
			g_hash_table_remove_all (known_instances);
		}

		if (g_atomic_int_dec_and_test (&view_data->pending_expand_recurrences) &&
		    view_data->is_used && view_data->lost_components && view_data->received_complete) {
			cal_data_model_remove_components (data_model, view_data->client, view_data->lost_components, NULL);
			g_hash_table_destroy (view_data->lost_components);
			view_data->lost_components = NULL;
		}

		g_hash_table_destroy (gathered_uids);
		g_hash_table_destroy (known_instances);

		view_data_unlock (view_data);

		cal_data_model_thaw_all_subscribers (data_model);

		view_data_unref (view_data);

		g_slist_free_full (expanded_recurrences, component_data_free);
	}

	g_clear_object (&notif_data->client);
	g_clear_object (&notif_data->data_model);
	g_slice_free (NotifyRecurrencesData, notif_data);

	return FALSE;
}

typedef struct
{
	ECalClient *client;
	ICalTimezone *zone;
	GSList **pexpanded_recurrences;
	gboolean skip_cancelled;
} GenerateInstancesData;

static gboolean
cal_data_model_instance_generated (ICalComponent *icomp,
				   ICalTime *instance_start,
				   ICalTime *instance_end,
				   gpointer user_data,
				   GCancellable *cancellable,
				   GError **error)
{
	GenerateInstancesData *gid = user_data;
	ComponentData *comp_data;
	ECalComponent *comp_copy;
	ICalTime *tt = NULL, *tt2 = NULL;
	time_t start_tt, end_tt;

	g_return_val_if_fail (gid != NULL, FALSE);

	if (gid->skip_cancelled) {
		ICalProperty *prop;

		prop = i_cal_component_get_first_property (icomp, I_CAL_STATUS_PROPERTY);
		if (prop && i_cal_property_get_status (prop) == I_CAL_STATUS_CANCELLED) {
			g_object_unref (prop);
			return TRUE;
		}

		g_clear_object (&prop);
	}

	comp_copy = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
	g_return_val_if_fail (comp_copy != NULL, FALSE);

	cal_comp_get_instance_times (gid->client, e_cal_component_get_icalcomponent (comp_copy),
		gid->zone, &tt, &tt2, cancellable);

	start_tt = i_cal_time_as_timet_with_zone (tt, i_cal_time_get_timezone (tt));
	end_tt = i_cal_time_as_timet_with_zone (tt2, i_cal_time_get_timezone (tt2));

	g_clear_object (&tt);
	g_clear_object (&tt2);

	if (end_tt > start_tt)
		end_tt--;

	comp_data = component_data_new (comp_copy, start_tt, end_tt, FALSE);
	*gid->pexpanded_recurrences = g_slist_prepend (*gid->pexpanded_recurrences, comp_data);

	g_object_unref (comp_copy);

	return TRUE;
}

static void
cal_data_model_expand_recurrences_thread (ECalDataModel *data_model,
					  gpointer user_data)
{
	ECalClient *client = user_data;
	GSList *to_expand_recurrences, *link;
	GSList *expanded_recurrences = NULL;
	GCancellable *cancellable;
	time_t range_start, range_end;
	ViewData *view_data;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	view_data = g_hash_table_lookup (data_model->priv->views, client);
	if (view_data)
		view_data_ref (view_data);

	range_start = data_model->priv->range_start;
	range_end = data_model->priv->range_end;

	UNLOCK_PROPS ();

	if (!view_data) {
		g_object_unref (client);
		return;
	}

	view_data_lock (view_data);

	if (!view_data->is_used) {
		view_data_unlock (view_data);
		view_data_unref (view_data);
		g_object_unref (client);
		return;
	}

	to_expand_recurrences = view_data->to_expand_recurrences;
	view_data->to_expand_recurrences = NULL;

	cancellable = view_data->cancellable ? g_object_ref (view_data->cancellable) : NULL;

	view_data_unlock (view_data);

	for (link = to_expand_recurrences; link && view_data->is_used && !g_cancellable_is_cancelled (cancellable); link = g_slist_next (link)) {
		ICalComponent *icomp = link->data;
		GenerateInstancesData gid;

		if (!icomp)
			continue;

		gid.client = client;
		gid.pexpanded_recurrences = &expanded_recurrences;
		gid.zone = g_object_ref (data_model->priv->zone);
		gid.skip_cancelled = data_model->priv->skip_cancelled;

		e_cal_client_generate_instances_for_object_sync (client, icomp, range_start, range_end, cancellable,
			cal_data_model_instance_generated, &gid);

		g_clear_object (&gid.zone);
	}

	g_slist_free_full (to_expand_recurrences, g_object_unref);

	view_data_lock (view_data);
	if (expanded_recurrences)
		view_data->expanded_recurrences = g_slist_concat (view_data->expanded_recurrences, expanded_recurrences);
	if (view_data->is_used) {
		NotifyRecurrencesData *notif_data;

		notif_data = g_slice_new0 (NotifyRecurrencesData);
		notif_data->data_model = g_object_ref (data_model);
		notif_data->client = g_object_ref (client);

		g_timeout_add (1, cal_data_model_notify_recurrences_cb, notif_data);
	}

	view_data_unlock (view_data);
	view_data_unref (view_data);
	g_object_unref (client);
	g_clear_object (&cancellable);
}

static void
cal_data_model_process_modified_or_added_objects (ECalClientView *view,
						  const GSList *objects,
						  ECalDataModel *data_model,
						  gboolean is_add)
{
	ViewData *view_data;
	ECalClient *client;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	client = e_cal_client_view_ref_client (view);
	if (!client) {
		UNLOCK_PROPS ();
		return;
	}

	view_data = g_hash_table_lookup (data_model->priv->views, client);
	if (view_data) {
		view_data_ref (view_data);
		g_warn_if_fail (view_data->view == view);
	}

	UNLOCK_PROPS ();

	if (!view_data) {
		g_clear_object (&client);
		return;
	}

	view_data_lock (view_data);

	if (view_data->is_used) {
		const GSList *link;
		GSList *to_expand_recurrences = NULL;

		if (!is_add) {
			/* Received a modify before the view was claimed as being complete,
			   aka fully populated, thus drop any previously known components,
			   because there is no hope for a merge. */
			if (view_data->lost_components) {
				cal_data_model_remove_components (data_model, client, view_data->lost_components, NULL);
				g_hash_table_destroy (view_data->lost_components);
				view_data->lost_components = NULL;
			}
		}

		cal_data_model_freeze_all_subscribers (data_model);

		for (link = objects; link; link = g_slist_next (link)) {
			ICalComponent *icomp = link->data;

			if (!icomp || !i_cal_component_get_uid (icomp))
				continue;

			if (data_model->priv->expand_recurrences &&
			    !e_cal_util_component_is_instance (icomp) &&
			    e_cal_util_component_has_recurrences (icomp)) {
				/* This component requires an expand of recurrences, which
				   will be done in a dedicated thread, thus remember it */
				to_expand_recurrences = g_slist_prepend (to_expand_recurrences,
					i_cal_component_clone (icomp));
			} else {
				/* Single or detached instance, the simple case */
				ECalComponent *comp;
				ComponentData *comp_data;
				ICalTime *start_tt = NULL, *end_tt = NULL;
				time_t instance_start, instance_end;

				if (data_model->priv->skip_cancelled &&
				    i_cal_component_get_status (icomp) == I_CAL_STATUS_CANCELLED)
					continue;

				comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
				if (!comp)
					continue;

				cal_comp_get_instance_times (client, icomp, data_model->priv->zone, &start_tt, &end_tt, NULL);

				instance_start = i_cal_time_as_timet_with_zone (start_tt, i_cal_time_get_timezone (start_tt));
				instance_end = i_cal_time_as_timet_with_zone (end_tt, i_cal_time_get_timezone (end_tt));

				g_clear_object (&start_tt);
				g_clear_object (&end_tt);

				if (instance_end > instance_start)
					instance_end--;

				comp_data = component_data_new (comp, instance_start, instance_end,
					e_cal_util_component_is_instance (icomp));

				cal_data_model_process_added_component (data_model, view_data, comp_data, NULL);

				g_object_unref (comp);
			}
		}

		cal_data_model_thaw_all_subscribers (data_model);

		if (to_expand_recurrences) {
			view_data_lock (view_data);
			view_data->to_expand_recurrences = g_slist_concat (
				view_data->to_expand_recurrences, to_expand_recurrences);
			g_atomic_int_inc (&view_data->pending_expand_recurrences);
			view_data_unlock (view_data);

			cal_data_model_submit_internal_thread_job (data_model,
				cal_data_model_expand_recurrences_thread, g_object_ref (client));
		}
	}

	view_data_unlock (view_data);
	view_data_unref (view_data);

	g_clear_object (&client);
}

static void
cal_data_model_view_objects_added (ECalClientView *view,
				   const GSList *objects,
				   ECalDataModel *data_model)
{
	cal_data_model_process_modified_or_added_objects (view, objects, data_model, TRUE);
}

static void
cal_data_model_view_objects_modified (ECalClientView *view,
				      const GSList *objects,
				      ECalDataModel *data_model)
{
	cal_data_model_process_modified_or_added_objects (view, objects, data_model, FALSE);
}

static void
cal_data_model_view_objects_removed (ECalClientView *view,
				     const GSList *uids,
				     ECalDataModel *data_model)
{
	ViewData *view_data;
	ECalClient *client;
	const GSList *link;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	client = e_cal_client_view_ref_client (view);
	if (!client) {
		UNLOCK_PROPS ();
		return;
	}

	view_data = g_hash_table_lookup (data_model->priv->views, client);

	g_clear_object (&client);

	if (view_data) {
		view_data_ref (view_data);
		g_warn_if_fail (view_data->view == view);
	}

	UNLOCK_PROPS ();

	if (!view_data)
		return;

	view_data_lock (view_data);
	if (view_data->is_used) {
		GHashTable *gathered_uids;
		GList *removed = NULL, *rlink;

		gathered_uids = g_hash_table_new (g_str_hash, g_str_equal);

		for (link = uids; link; link = g_slist_next (link)) {
			const ECalComponentId *id = link->data;

			if (id) {
				if (!e_cal_component_id_get_rid (id)) {
					if (!g_hash_table_contains (gathered_uids, e_cal_component_id_get_uid (id))) {
						GatherComponentsData gather_data;

						gather_data.uid = e_cal_component_id_get_uid (id);
						gather_data.pcomponent_ids = &removed;
						gather_data.component_ids_hash = NULL;
						gather_data.copy_ids = TRUE;
						gather_data.all_instances = TRUE;

						g_hash_table_foreach (view_data->components,
							cal_data_model_gather_components, &gather_data);
						if (view_data->lost_components)
							g_hash_table_foreach (view_data->lost_components,
								cal_data_model_gather_components, &gather_data);

						g_hash_table_insert (gathered_uids, (gpointer) e_cal_component_id_get_uid (id), GINT_TO_POINTER (1));
					}
				} else {
					removed = g_list_prepend (removed, e_cal_component_id_copy (id));
				}
			}
		}

		cal_data_model_freeze_all_subscribers (data_model);

		for (rlink = removed; rlink; rlink = g_list_next (rlink)) {
			ECalComponentId *id = rlink->data;

			if (id) {
				ComponentData *comp_data;
				time_t instance_start = (time_t) 0, instance_end = (time_t) 0;

				/* Try to limit which subscribers will be notified about removal */
				comp_data = g_hash_table_lookup (view_data->components, id);
				if (comp_data) {
					instance_start = comp_data->instance_start;
					instance_end = comp_data->instance_end;
				} else if (view_data->lost_components) {
					comp_data = g_hash_table_lookup (view_data->lost_components, id);
					if (comp_data) {
						instance_start = comp_data->instance_start;
						instance_end = comp_data->instance_end;
					}
				}

				g_hash_table_remove (view_data->components, id);
				if (view_data->lost_components)
					g_hash_table_remove (view_data->lost_components, id);

				cal_data_model_foreach_subscriber_in_range (data_model, view_data->client,
					instance_start, instance_end,
					cal_data_model_remove_one_view_component_cb, id);
			}
		}

		cal_data_model_thaw_all_subscribers (data_model);

		g_list_free_full (removed, (GDestroyNotify) e_cal_component_id_free);
		g_hash_table_destroy (gathered_uids);
	}
	view_data_unlock (view_data);
	view_data_unref (view_data);
}

static void
cal_data_model_view_progress (ECalClientView *view,
			      guint percent,
			      const gchar *message,
			      ECalDataModel *data_model)
{
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	cal_data_model_emit_view_state_changed (data_model, view, E_CAL_DATA_MODEL_VIEW_STATE_PROGRESS, percent, message, NULL);
}

static void
cal_data_model_view_complete (ECalClientView *view,
			      const GError *error,
			      ECalDataModel *data_model)
{
	ViewData *view_data;
	ECalClient *client;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	client = e_cal_client_view_ref_client (view);
	if (!client) {
		UNLOCK_PROPS ();
		return;
	}

	view_data = g_hash_table_lookup (data_model->priv->views, client);

	g_clear_object (&client);

	if (view_data) {
		view_data_ref (view_data);
		g_warn_if_fail (view_data->view == view);
	}

	UNLOCK_PROPS ();

	if (!view_data)
		return;

	view_data_lock (view_data);

	view_data->received_complete = TRUE;
	if (view_data->is_used &&
	    view_data->lost_components &&
	    !view_data->pending_expand_recurrences) {
		cal_data_model_remove_components (data_model, view_data->client, view_data->lost_components, NULL);
		g_hash_table_destroy (view_data->lost_components);
		view_data->lost_components = NULL;
	}

	cal_data_model_emit_view_state_changed (data_model, view, E_CAL_DATA_MODEL_VIEW_STATE_COMPLETE, 0, NULL, error);

	view_data_unlock (view_data);
	view_data_unref (view_data);
}

typedef struct _CreateViewData {
	ECalDataModel *data_model;
	ECalClient *client;
} CreateViewData;

static void
create_view_data_free (gpointer ptr)
{
	CreateViewData *cv_data = ptr;

	if (cv_data) {
		g_clear_object (&cv_data->data_model);
		g_clear_object (&cv_data->client);
		g_slice_free (CreateViewData, cv_data);
	}
}

static void
cal_data_model_create_view_thread (EAlertSinkThreadJobData *job_data,
				   gpointer user_data,
				   GCancellable *cancellable,
				   GError **error)
{
	CreateViewData *cv_data = user_data;
	ViewData *view_data;
	ECalDataModel *data_model;
	ECalClient *client;
	ECalClientView *view;
	gchar *filter;

	g_return_if_fail (cv_data != NULL);

	data_model = cv_data->data_model;
	client = cv_data->client;
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (E_IS_CAL_CLIENT (client));

	LOCK_PROPS ();

	if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
		UNLOCK_PROPS ();
		return;
	}

	view_data = g_hash_table_lookup (data_model->priv->views, client);
	if (!view_data) {
		UNLOCK_PROPS ();
		g_warn_if_reached ();
		return;
	}

	filter = g_strdup (data_model->priv->full_filter);

	view_data_ref (view_data);
	UNLOCK_PROPS ();

	view_data_lock (view_data);
	g_warn_if_fail (view_data->view == NULL);

	if (!e_cal_client_get_view_sync (client, filter, &view_data->view, cancellable, error)) {
		view_data_unlock (view_data);
		view_data_unref (view_data);
		g_free (filter);
		return;
	}

	g_warn_if_fail (view_data->view != NULL);

	view_data->objects_added_id = g_signal_connect (view_data->view, "objects-added",
		G_CALLBACK (cal_data_model_view_objects_added), data_model);
	view_data->objects_modified_id = g_signal_connect (view_data->view, "objects-modified",
		G_CALLBACK (cal_data_model_view_objects_modified), data_model);
	view_data->objects_removed_id = g_signal_connect (view_data->view, "objects-removed",
		G_CALLBACK (cal_data_model_view_objects_removed), data_model);
	view_data->progress_id = g_signal_connect (view_data->view, "progress",
		G_CALLBACK (cal_data_model_view_progress), data_model);
	view_data->complete_id = g_signal_connect (view_data->view, "complete",
		G_CALLBACK (cal_data_model_view_complete), data_model);

	view = g_object_ref (view_data->view);

	view_data_unlock (view_data);
	view_data_unref (view_data);

	g_free (filter);

	if (!g_cancellable_is_cancelled (cancellable)) {
		cal_data_model_emit_view_state_changed (data_model, view, E_CAL_DATA_MODEL_VIEW_STATE_START, 0, NULL, NULL);
		e_cal_client_view_start (view, error);
	}

	g_clear_object (&view);
}

typedef struct _NotifyRemoveComponentsData {
	ECalDataModel *data_model;
	ECalClient *client;
} NotifyRemoveComponentsData;

static void
cal_data_model_notify_remove_components_cb (gpointer key,
					    gpointer value,
					    gpointer user_data)
{
	ECalComponentId *id = key;
	ComponentData *comp_data = value;
	NotifyRemoveComponentsData *nrc_data = user_data;

	g_return_if_fail (id != NULL);
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (nrc_data != NULL);

	cal_data_model_foreach_subscriber_in_range (nrc_data->data_model, nrc_data->client,
		comp_data->instance_start, comp_data->instance_end,
		cal_data_model_remove_one_view_component_cb, id);
}

static void
cal_data_model_update_client_view (ECalDataModel *data_model,
				   ECalClient *client)
{
	ESource *source;
	ViewData *view_data;
	CreateViewData *cv_data;
	const gchar *alert_ident = NULL;
	gchar *description = NULL;

	LOCK_PROPS ();

	view_data = g_hash_table_lookup (data_model->priv->views, client);
	if (!view_data) {
		view_data = view_data_new (client);
		g_hash_table_insert (data_model->priv->views, client, view_data);
	}

	view_data_lock (view_data);

	if (view_data->cancellable)
		g_cancellable_cancel (view_data->cancellable);
	g_clear_object (&view_data->cancellable);

	if (view_data->view) {
		view_data_disconnect_view (view_data);
		cal_data_model_emit_view_state_changed (data_model, view_data->view, E_CAL_DATA_MODEL_VIEW_STATE_STOP, 0, NULL, NULL);
		g_clear_object (&view_data->view);
	}

	if (!view_data->received_complete) {
		NotifyRemoveComponentsData nrc_data;

		nrc_data.data_model = data_model;
		nrc_data.client = client;

		cal_data_model_freeze_all_subscribers (data_model);

		g_hash_table_foreach (view_data->components,
			cal_data_model_notify_remove_components_cb, &nrc_data);

		g_hash_table_remove_all (view_data->components);
		if (view_data->lost_components) {
			g_hash_table_foreach (view_data->lost_components,
				cal_data_model_notify_remove_components_cb, &nrc_data);

			g_hash_table_destroy (view_data->lost_components);
			view_data->lost_components = NULL;
		}

		cal_data_model_thaw_all_subscribers (data_model);
	} else {
		if (view_data->lost_components) {
			NotifyRemoveComponentsData nrc_data;

			nrc_data.data_model = data_model;
			nrc_data.client = client;

			cal_data_model_freeze_all_subscribers (data_model);
			g_hash_table_foreach (view_data->lost_components,
				cal_data_model_notify_remove_components_cb, &nrc_data);
			cal_data_model_thaw_all_subscribers (data_model);

			g_hash_table_destroy (view_data->lost_components);
			view_data->lost_components = NULL;
		}

		view_data->lost_components = view_data->components;
		view_data->components = g_hash_table_new_full (
			(GHashFunc) e_cal_component_id_hash, (GEqualFunc) e_cal_component_id_equal,
			(GDestroyNotify) e_cal_component_id_free, component_data_free);
	}

	view_data_unlock (view_data);

	if (!data_model->priv->full_filter) {
		UNLOCK_PROPS ();
		return;
	}

	source = e_client_get_source (E_CLIENT (client));

	switch (e_cal_client_get_source_type (client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			alert_ident = "calendar:failed-create-view-calendar";
			description = g_strdup_printf (_("Creating view for calendar “%s”"), e_source_get_display_name (source));
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			alert_ident = "calendar:failed-create-view-tasks";
			description = g_strdup_printf (_("Creating view for task list “%s”"), e_source_get_display_name (source));
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			alert_ident = "calendar:failed-create-view-memos";
			description = g_strdup_printf (_("Creating view for memo list “%s”"), e_source_get_display_name (source));
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_LAST:
			g_warn_if_reached ();
			UNLOCK_PROPS ();
			return;
	}

	cv_data = g_slice_new0 (CreateViewData);
	cv_data->data_model = g_object_ref (data_model);
	cv_data->client = g_object_ref (client);

	view_data->received_complete = FALSE;
	view_data->cancellable = e_cal_data_model_submit_thread_job (data_model,
		description, alert_ident, e_source_get_display_name (source),
		cal_data_model_create_view_thread, cv_data, create_view_data_free);

	g_free (description);

	UNLOCK_PROPS ();
}

static void
cal_data_model_remove_client_view (ECalDataModel *data_model,
				   ECalClient *client)
{
	ViewData *view_data;

	LOCK_PROPS ();

	view_data = g_hash_table_lookup (data_model->priv->views, client);

	if (view_data) {
		NotifyRemoveComponentsData nrc_data;

		view_data_lock (view_data);

		nrc_data.data_model = data_model;
		nrc_data.client = client;

		cal_data_model_freeze_all_subscribers (data_model);

		g_hash_table_foreach (view_data->components,
			cal_data_model_notify_remove_components_cb, &nrc_data);
		g_hash_table_remove_all (view_data->components);

		if (view_data->lost_components) {
			g_hash_table_foreach (view_data->lost_components,
				cal_data_model_notify_remove_components_cb, &nrc_data);
			g_hash_table_remove_all (view_data->lost_components);
		}

		cal_data_model_thaw_all_subscribers (data_model);

		if (view_data->view)
			cal_data_model_emit_view_state_changed (data_model, view_data->view, E_CAL_DATA_MODEL_VIEW_STATE_STOP, 0, NULL, NULL);

		view_data->is_used = FALSE;
		view_data_unlock (view_data);

		g_hash_table_remove (data_model->priv->views, client);
	}

	UNLOCK_PROPS ();
}

static gboolean
cal_data_model_add_to_subscriber (ECalDataModel *data_model,
				  ECalClient *client,
				  const ECalComponentId *id,
				  ECalComponent *component,
				  time_t instance_start,
				  time_t instance_end,
				  gpointer user_data)
{
	ECalDataModelSubscriber *subscriber = user_data;

	g_return_val_if_fail (subscriber != NULL, FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	e_cal_data_model_subscriber_component_added (subscriber, client, component);

	return TRUE;
}

static gboolean
cal_data_model_add_to_subscriber_except_its_range (ECalDataModel *data_model,
						   ECalClient *client,
						   const ECalComponentId *id,
						   ECalComponent *component,
						   time_t instance_start,
						   time_t instance_end,
						   gpointer user_data)
{
	SubscriberData *subs_data = user_data;

	g_return_val_if_fail (subs_data != NULL, FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	/* subs_data should have set the old time range, which
	   means only components which didn't fit into the old
	   time range will be added */
	if (!(instance_start <= subs_data->range_end &&
	    instance_end >= subs_data->range_start))
		e_cal_data_model_subscriber_component_added (subs_data->subscriber, client, component);

	return TRUE;
}

static gboolean
cal_data_model_remove_from_subscriber_except_its_range (ECalDataModel *data_model,
							ECalClient *client,
							const ECalComponentId *id,
							ECalComponent *component,
							time_t instance_start,
							time_t instance_end,
							gpointer user_data)
{
	SubscriberData *subs_data = user_data;

	g_return_val_if_fail (subs_data != NULL, FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	/* subs_data should have set the new time range, which
	   means only components which don't fit into this new
	   time range will be removed */
	if (!(instance_start <= subs_data->range_end &&
	    instance_end >= subs_data->range_start))
		e_cal_data_model_subscriber_component_removed (subs_data->subscriber, client,
			e_cal_component_id_get_uid (id),
			e_cal_component_id_get_rid (id));

	return TRUE;
}

static void
cal_data_model_set_client_default_zone_cb (gpointer key,
					   gpointer value,
					   gpointer user_data)
{
	ECalClient *client = value;
	ICalTimezone *zone = user_data;

	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (zone != NULL);

	e_cal_client_set_default_timezone (client, zone);
}

static void
cal_data_model_rebuild_everything (ECalDataModel *data_model,
				   gboolean complete_rebuild)
{
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	if (data_model->priv->views_update_freeze > 0) {
		data_model->priv->views_update_required = TRUE;
		UNLOCK_PROPS ();
		return;
	}

	data_model->priv->views_update_required = FALSE;

	g_hash_table_iter_init (&iter, data_model->priv->clients);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ECalClient *client = value;

		if (complete_rebuild)
			cal_data_model_remove_client_view (data_model, client);
		cal_data_model_update_client_view (data_model, client);
	}

	UNLOCK_PROPS ();
}

static void
cal_data_model_update_time_range (ECalDataModel *data_model)
{
	time_t range_start, range_end;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	if (data_model->priv->disposing) {
		UNLOCK_PROPS ();

		return;
	}

	range_start = data_model->priv->range_start;
	range_end = data_model->priv->range_end;

	cal_data_model_calc_range (data_model, &range_start, &range_end);

	if (data_model->priv->range_start != range_start ||
	    data_model->priv->range_end != range_end) {
		data_model->priv->range_start = range_start;
		data_model->priv->range_end = range_end;

		if (cal_data_model_update_full_filter (data_model))
			cal_data_model_rebuild_everything (data_model, FALSE);
	}

	UNLOCK_PROPS ();
}

static void
cal_data_model_set_registry (ECalDataModel *self,
			     ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));

	if (self->priv->registry == registry)
		return;

	g_clear_object (&self->priv->registry);
	self->priv->registry = g_object_ref (registry);
}

static void
cal_data_model_set_property (GObject *object,
			     guint property_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXPAND_RECURRENCES:
			e_cal_data_model_set_expand_recurrences (
				E_CAL_DATA_MODEL (object),
				g_value_get_boolean (value));
			return;

		case PROP_TIMEZONE:
			e_cal_data_model_set_timezone (
				E_CAL_DATA_MODEL (object),
				g_value_get_object (value));
			return;

		case PROP_SKIP_CANCELLED:
			e_cal_data_model_set_skip_cancelled (
				E_CAL_DATA_MODEL (object),
				g_value_get_boolean (value));
			return;

		case PROP_REGISTRY:
			cal_data_model_set_registry (
				E_CAL_DATA_MODEL (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_data_model_get_property (GObject *object,
			     guint property_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXPAND_RECURRENCES:
			g_value_set_boolean (
				value,
				e_cal_data_model_get_expand_recurrences (
				E_CAL_DATA_MODEL (object)));
			return;

		case PROP_TIMEZONE:
			g_value_set_object (
				value,
				e_cal_data_model_get_timezone (
				E_CAL_DATA_MODEL (object)));
			return;

		case PROP_SKIP_CANCELLED:
			g_value_set_boolean (
				value,
				e_cal_data_model_get_skip_cancelled (
				E_CAL_DATA_MODEL (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_cal_data_model_get_registry (
				E_CAL_DATA_MODEL (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_data_model_dispose (GObject *object)
{
	ECalDataModel *data_model = E_CAL_DATA_MODEL (object);

	data_model->priv->disposing = TRUE;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_cal_data_model_parent_class)->dispose (object);
}

static void
cal_data_model_finalize (GObject *object)
{
	ECalDataModel *data_model = E_CAL_DATA_MODEL (object);

	g_thread_pool_free (data_model->priv->thread_pool, TRUE, FALSE);
	g_hash_table_destroy (data_model->priv->clients);
	g_hash_table_destroy (data_model->priv->views);
	g_slist_free_full (data_model->priv->subscribers, subscriber_data_free);
	g_free (data_model->priv->filter);
	g_free (data_model->priv->full_filter);
	g_clear_object (&data_model->priv->zone);
	g_clear_object (&data_model->priv->registry);

	e_weak_ref_free (data_model->priv->submit_thread_job_responder);
	g_rec_mutex_clear (&data_model->priv->props_lock);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_cal_data_model_parent_class)->finalize (object);
}

static void
e_cal_data_model_class_init (ECalDataModelClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cal_data_model_set_property;
	object_class->get_property = cal_data_model_get_property;
	object_class->dispose = cal_data_model_dispose;
	object_class->finalize = cal_data_model_finalize;

	g_object_class_install_property (
		object_class,
		PROP_EXPAND_RECURRENCES,
		g_param_spec_boolean (
			"expand-recurrences",
			"Expand Recurrences",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TIMEZONE,
		g_param_spec_object (
			"timezone",
			"Time Zone",
			NULL,
			I_CAL_TYPE_TIMEZONE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SKIP_CANCELLED,
		g_param_spec_boolean (
			"skip-cancelled",
			"Skip Cancelled",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			NULL,
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	signals[VIEW_STATE_CHANGED] = g_signal_new (
		"view-state-changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalDataModelClass, view_state_changed),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 5, E_TYPE_CAL_CLIENT_VIEW, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_ERROR);
}

static void
e_cal_data_model_init (ECalDataModel *data_model)
{
	data_model->priv = e_cal_data_model_get_instance_private (data_model);

	/* Suppose the data_model is always created in the main/UI thread */
	data_model->priv->main_thread = g_thread_self ();
	data_model->priv->thread_pool = g_thread_pool_new (
		cal_data_model_internal_thread_job_func, data_model, 5, FALSE, NULL);

	data_model->priv->clients = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	data_model->priv->views = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, view_data_unref);
	data_model->priv->subscribers = NULL;

	data_model->priv->disposing = FALSE;
	data_model->priv->expand_recurrences = FALSE;
	data_model->priv->skip_cancelled = FALSE;
	data_model->priv->zone = g_object_ref (i_cal_timezone_get_utc_timezone ());

	data_model->priv->views_update_freeze = 0;
	data_model->priv->views_update_required = FALSE;

	g_rec_mutex_init (&data_model->priv->props_lock);
}

/**
 * e_cal_data_model_new:
 * @registry: an #ESourceRegistry instance
 * @func: a function to be called when the data model needs to create
 *    a thread job within UI
 * @func_responder: (allow none): a responder for @func, which is passed
 *    as the first paramter; can be #NULL
 *
 * Creates a new instance of #ECalDataModel. The @func is mandatory, because
 * it is used to create new thread jobs with UI feedback.
 *
 * Returns: (transfer full): A new #ECalDataModel instance
 *
 * Since: 3.16
 **/
ECalDataModel *
e_cal_data_model_new (ESourceRegistry *registry,
		      ECalDataModelSubmitThreadJobFunc func,
		      GObject *func_responder)
{
	ECalDataModel *data_model;

	g_return_val_if_fail (func != NULL, NULL);

	data_model = g_object_new (E_TYPE_CAL_DATA_MODEL, "registry", registry, NULL);
	data_model->priv->submit_thread_job_func = func;
	data_model->priv->submit_thread_job_responder = e_weak_ref_new (func_responder);

	return data_model;
}

/**
 * e_cal_data_model_new_clone:
 * @src_data_model: an #ECalDataModel to clone
 *
 * Creates a clone of @src_data_model, which means a copy with the same clients, filter and
 * other properties set (not the subscribers).
 *
 * Returns: (transfer full): A new #ECalDataModel instance deriving settings from @src_data_model
 *
 * Since: 3.16
 **/
ECalDataModel *
e_cal_data_model_new_clone (ECalDataModel *src_data_model)
{
	ECalDataModel *clone;
	GObject *func_responder;
	GList *clients, *link;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (src_data_model), NULL);

	func_responder = g_weak_ref_get (src_data_model->priv->submit_thread_job_responder);
	g_return_val_if_fail (func_responder != NULL, NULL);

	clone = e_cal_data_model_new (src_data_model->priv->registry, src_data_model->priv->submit_thread_job_func, func_responder);

	g_clear_object (&func_responder);

	e_cal_data_model_set_expand_recurrences (clone, e_cal_data_model_get_expand_recurrences (src_data_model));
	e_cal_data_model_set_skip_cancelled (clone, e_cal_data_model_get_skip_cancelled (src_data_model));
	e_cal_data_model_set_timezone (clone, e_cal_data_model_get_timezone (src_data_model));
	e_cal_data_model_set_filter (clone, src_data_model->priv->filter);

	clients = e_cal_data_model_get_clients (src_data_model);
	for (link = clients; link; link = g_list_next (link)) {
		ECalClient *client = link->data;

		e_cal_data_model_add_client (clone, client);
	}

	g_list_free_full (clients, g_object_unref);

	return clone;
}

/**
 * e_cal_data_model_get_registry:
 * @data_model: an #ECalDataModel
 *
 * Returns an #ESourceRegistry instance the @data_model was created with.
 *
 * Returns: (transfer none): an #ESourceRegistry instance
 *
 * Since: 3.56
 **/
ESourceRegistry *
e_cal_data_model_get_registry (ECalDataModel *data_model)
{
	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), FALSE);

	return data_model->priv->registry;
}

/**
 * e_cal_data_model_get_disposing:
 * @data_model: an #EDataModel instance
 *
 * Obtains whether the @data_model is disposing and will be freed (soon).
 *
 * Returns: Whether the @data_model is disposing.
 *
 * Since: 3.16
 **/
gboolean
e_cal_data_model_get_disposing (ECalDataModel *data_model)
{
	gboolean disposing;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), FALSE);

	LOCK_PROPS ();

	disposing = data_model->priv->disposing;

	UNLOCK_PROPS ();

	return disposing;
}

/**
 * e_cal_data_model_set_disposing:
 * @data_model: an #EDataModel instance
 * @disposing: whether the object is disposing
 *
 * Sets whether the @data_model is disposing itself (soon).
 * If set to %TRUE, then no updates are done on changes
 * which would otherwise trigger view and subscriber updates.
 *
 * Since: 3.16
 **/
void
e_cal_data_model_set_disposing (ECalDataModel *data_model,
				gboolean disposing)
{
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	if ((data_model->priv->disposing ? 1 : 0) == (disposing ? 1 : 0)) {
		UNLOCK_PROPS ();
		return;
	}

	data_model->priv->disposing = disposing;

	UNLOCK_PROPS ();
}

/**
 * e_cal_data_model_get_expand_recurrences:
 * @data_model: an #EDataModel instance
 *
 * Obtains whether the @data_model expands recurrences of recurring
 * components by default. The default value is #FALSE, to not expand
 * recurrences.
 *
 * Returns: Whether the @data_model expands recurrences of recurring
 *    components.
 *
 * Since: 3.16
 **/
gboolean
e_cal_data_model_get_expand_recurrences (ECalDataModel *data_model)
{
	gboolean expand_recurrences;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), FALSE);

	LOCK_PROPS ();

	expand_recurrences = data_model->priv->expand_recurrences;

	UNLOCK_PROPS ();

	return expand_recurrences;
}

/**
 * e_cal_data_model_set_expand_recurrences:
 * @data_model: an #EDataModel instance
 * @expand_recurrences: whether to expand recurrences
 *
 * Sets whether the @data_model should expand recurrences of recurring
 * components by default.
 *
 * Since: 3.16
 **/
void
e_cal_data_model_set_expand_recurrences (ECalDataModel *data_model,
					 gboolean expand_recurrences)
{
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	if ((data_model->priv->expand_recurrences ? 1 : 0) == (expand_recurrences ? 1 : 0)) {
		UNLOCK_PROPS ();
		return;
	}

	data_model->priv->expand_recurrences = expand_recurrences;

	cal_data_model_rebuild_everything (data_model, TRUE);

	UNLOCK_PROPS ();
}

/**
 * e_cal_data_model_get_skip_cancelled:
 * @data_model: an #EDataModel instance
 *
 * Obtains whether the @data_model skips cancelled components.
 * The default value is #FALSE, to not skip cancelled components.
 *
 * Returns: Whether the @data_model skips cancelled components.
 *
 * Since: 3.32
 **/
gboolean
e_cal_data_model_get_skip_cancelled (ECalDataModel *data_model)
{
	gboolean skip_cancelled;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), FALSE);

	LOCK_PROPS ();

	skip_cancelled = data_model->priv->skip_cancelled;

	UNLOCK_PROPS ();

	return skip_cancelled;
}

/**
 * e_cal_data_model_set_skip_cancelled:
 * @data_model: an #EDataModel instance
 * @skip_cancelled: whether to skip cancelled components
 *
 * Sets whether the @data_model should skip cancelled components.
 *
 * Since: 3.32
 **/
void
e_cal_data_model_set_skip_cancelled (ECalDataModel *data_model,
				     gboolean skip_cancelled)
{
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	if ((data_model->priv->skip_cancelled ? 1 : 0) == (skip_cancelled ? 1 : 0)) {
		UNLOCK_PROPS ();
		return;
	}

	data_model->priv->skip_cancelled = skip_cancelled;

	cal_data_model_rebuild_everything (data_model, TRUE);

	UNLOCK_PROPS ();
}

/**
 * e_cal_data_model_get_timezone:
 * @data_model: an #EDataModel instance
 *
 * Obtains a timezone being used for calendar views. The returned
 * timezone is owned by the @data_model.
 *
 * Returns: (transfer none): An #ICalTimezone being used for calendar views.
 *
 * Since: 3.16
 **/
ICalTimezone *
e_cal_data_model_get_timezone (ECalDataModel *data_model)
{
	ICalTimezone *zone;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), NULL);

	LOCK_PROPS ();

	zone = data_model->priv->zone;

	UNLOCK_PROPS ();

	return zone;
}

/**
 * e_cal_data_model_set_timezone:
 * @data_model: an #EDataModel instance
 * @zone: an #ICalTimezone
 *
 * Sets a trimezone to be used for calendar views. This change
 * regenerates all views.
 *
 * Since: 3.16
 **/
void
e_cal_data_model_set_timezone (ECalDataModel *data_model,
			       ICalTimezone *zone)
{
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (zone != NULL);

	LOCK_PROPS ();

	if (data_model->priv->zone != zone) {
		g_clear_object (&data_model->priv->zone);
		data_model->priv->zone = g_object_ref (zone);

		g_hash_table_foreach (data_model->priv->clients, cal_data_model_set_client_default_zone_cb, zone);

		if (cal_data_model_update_full_filter (data_model))
			cal_data_model_rebuild_everything (data_model, TRUE);
	}

	UNLOCK_PROPS ();
}

/**
 * e_cal_data_model_set_filter:
 * @data_model: an #EDataModel instance
 * @sexp: an expression defining a filter
 *
 * Sets an additional filter for the views. The filter should not
 * contain time constraints, these are meant to be defined by
 * subscribers.
 *
 * Since: 3.16
 **/
void
e_cal_data_model_set_filter (ECalDataModel *data_model,
			     const gchar *sexp)
{
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (sexp != NULL);

	LOCK_PROPS ();

	if (sexp && !*sexp)
		sexp = NULL;

	if (g_strcmp0 (data_model->priv->filter, sexp) != 0) {
		g_free (data_model->priv->filter);
		data_model->priv->filter = g_strdup (sexp);

		if (cal_data_model_update_full_filter (data_model))
			cal_data_model_rebuild_everything (data_model, TRUE);
	}

	UNLOCK_PROPS ();
}

/**
 * e_cal_data_model_dup_filter:
 * @data_model: an #EDataModel instance
 *
 * Obtains currently used filter (an expression) for the views.
 *
 * Returns: (transfer full): A copy of the currently used
 *   filter for views. Free it with g_free() when done with it.
 *   Returns #NULL when there is no extra filter set.
 *
 * Since: 3.16
 **/
gchar *
e_cal_data_model_dup_filter (ECalDataModel *data_model)
{
	gchar *filter;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), NULL);

	LOCK_PROPS ();

	filter = g_strdup (data_model->priv->filter);

	UNLOCK_PROPS ();

	return filter;
}

/**
 * e_cal_data_model_add_client:
 * @data_model: an #EDataModel instance
 * @client: an #ECalClient
 *
 * Adds a new @client into the set of clients which should be used
 * to populate data for subscribers. Adding the same client multiple
 * times does nothing.
 *
 * Since: 3.16
 **/
void
e_cal_data_model_add_client (ECalDataModel *data_model,
			     ECalClient *client)
{
	ESource *source;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (E_IS_CAL_CLIENT (client));

	source = e_client_get_source (E_CLIENT (client));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (e_source_get_uid (source) != NULL);

	LOCK_PROPS ();

	if (g_hash_table_contains (data_model->priv->clients, e_source_get_uid (source))) {
		UNLOCK_PROPS ();
		return;
	}

	g_hash_table_insert (data_model->priv->clients, e_source_dup_uid (source), g_object_ref (client));

	e_cal_client_set_default_timezone (client, data_model->priv->zone);

	cal_data_model_update_client_view (data_model, client);

	UNLOCK_PROPS ();
}

/**
 * e_cal_data_model_remove_client:
 * @uid: a UID of a client to remove
 *
 * Removes a client identified by @uid from a set of clients
 * which populate the data for subscribers. Removing the client
 * which is not used in the @data_model does nothing.
 *
 * Since: 3.16
 **/
void
e_cal_data_model_remove_client (ECalDataModel *data_model,
				const gchar *uid)
{
	ECalClient *client;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (uid != NULL);

	LOCK_PROPS ();

	client = g_hash_table_lookup (data_model->priv->clients, uid);
	if (!client) {
		UNLOCK_PROPS ();
		return;
	}

	cal_data_model_remove_client_view (data_model, client);
	g_hash_table_remove (data_model->priv->clients, uid);

	UNLOCK_PROPS ();
}

static gboolean
cal_data_model_remove_client_cb (gpointer key,
				 gpointer value,
				 gpointer user_data)
{
	ECalDataModel *data_model = user_data;

	cal_data_model_remove_client_view (data_model, value);

	return TRUE;
}

/**
 * e_cal_data_model_remove_all_clients:
 * @data_model: an #ECalDataModel
 *
 * Removes all clients from the @data_model.
 *
 * Since: 3.40
 **/
void
e_cal_data_model_remove_all_clients (ECalDataModel *data_model)
{
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	g_hash_table_foreach_remove (data_model->priv->clients,
		cal_data_model_remove_client_cb, data_model);

	UNLOCK_PROPS ();
}

/**
 * e_cal_data_model_ref_client:
 * @data_model: an #EDataModel instance
 * @uid: a UID of a client to return
 *
 * Obtains an #ECalClient with given @uid from the set of clients
 * being used by the @data_modal. Returns #NULL, if no such client
 * is used by the @data_model.
 *
 * Returns: (tranfer full): An #ECalClient with given @uid being
 *    used by @data_model, or NULL, when no such is used by
 *    the @data_model. Unref returned (non-NULL) client with
 *    g_object_unref() when done with it.
 *
 * Since: 3.16
 **/
ECalClient *
e_cal_data_model_ref_client (ECalDataModel *data_model,
			     const gchar *uid)
{
	ECalClient *client;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), NULL);

	LOCK_PROPS ();

	client = g_hash_table_lookup (data_model->priv->clients, uid);
	if (client)
		g_object_ref (client);

	UNLOCK_PROPS ();

	return client;
}

/**
 * e_cal_data_model_get_clients:
 * @data_model: an #EDataModel instance
 *
 * Obtains a list of all clients being used by the @data_model.
 * Each client in the returned list is referenced and the list
 * itself is also newly allocated, thus free it with
 * g_list_free_full (list, g_object_unref); when done with it.
 *
 * Returns: (transfer full): A list of currently used #ECalClient-s.
 *
 * Since: 3.16
 **/
GList *
e_cal_data_model_get_clients (ECalDataModel *data_model)
{
	GList *clients;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), NULL);

	LOCK_PROPS ();

	clients = g_hash_table_get_values (data_model->priv->clients);
	g_list_foreach (clients, (GFunc) g_object_ref, NULL);

	UNLOCK_PROPS ();

	return clients;
}

static gboolean
cal_data_model_prepend_component (ECalDataModel *data_model,
				  ECalClient *client,
				  const ECalComponentId *id,
				  ECalComponent *comp,
				  time_t instance_start,
				  time_t instance_end,
				  gpointer user_data)
{
	GSList **components = user_data;

	g_return_val_if_fail (components != NULL, FALSE);
	g_return_val_if_fail (comp != NULL, FALSE);

	*components = g_slist_prepend (*components, g_object_ref (comp));

	return TRUE;
}

/**
 * e_cal_data_model_get_components:
 * @data_model: an #EDataModel instance
 * @in_range_start: Start of the time range
 * @in_range_end: End of the time range
 *
 * Obtains a list of components from the given time range. The time range is
 * clamp by the actual time range defined by subscribers (if there is no
 * subscriber, or all subscribers define times out of the given time range,
 * then no components are returned).
 *
 * Returns: (transfer full): A #GSList of #ECalComponent-s known for the given
 *    time range in the time of the call. The #GSList, togher with the components,
 *    is owned by the caller, which should free it with
 *    g_slist_free_full (list, g_object_unref); when done with it.
 *
 * Note: A special case when both @in_range_start and @in_range_end are zero
 *    is treated as a request for all known components.
 *
 * Since: 3.16
 **/
GSList *
e_cal_data_model_get_components (ECalDataModel *data_model,
				 time_t in_range_start,
				 time_t in_range_end)
{
	GSList *components = NULL;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), NULL);

	e_cal_data_model_foreach_component (data_model, in_range_start, in_range_end,
		cal_data_model_prepend_component, &components);

	return g_slist_reverse (components);
}

static gboolean
cal_data_model_foreach_component (ECalDataModel *data_model,
				  time_t in_range_start,
				  time_t in_range_end,
				  ECalDataModelForeachFunc func,
				  gpointer user_data,
				  gboolean include_lost_components)
{
	GHashTableIter viter;
	gpointer key, value;
	gboolean checked_all = TRUE;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	LOCK_PROPS ();

	/* Is the given time range in the currently used time range? */
	if (!(in_range_start == in_range_end && in_range_start == (time_t) 0) &&
	    (in_range_start >= data_model->priv->range_end ||
	    in_range_end <= data_model->priv->range_start)) {
		UNLOCK_PROPS ();
		return checked_all;
	}

	g_hash_table_iter_init (&viter, data_model->priv->views);
	while (checked_all && g_hash_table_iter_next (&viter, &key, &value)) {
		ViewData *view_data = value;
		GHashTableIter citer;

		if (!view_data)
			continue;

		view_data_lock (view_data);

		g_hash_table_iter_init (&citer, view_data->components);
		while (checked_all && g_hash_table_iter_next (&citer, &key, &value)) {
			ECalComponentId *id = key;
			ComponentData *comp_data = value;

			if (!comp_data)
				continue;

			if ((in_range_start == in_range_end && in_range_start == (time_t) 0) ||
			    (comp_data->instance_start < in_range_end && comp_data->instance_end > in_range_start) ||
			    (comp_data->instance_start == comp_data->instance_end && comp_data->instance_end == in_range_start)) {
				if (!func (data_model, view_data->client, id, comp_data->component,
					   comp_data->instance_start, comp_data->instance_end, user_data))
					checked_all = FALSE;
			}
		}

		if (include_lost_components && view_data->lost_components) {
			g_hash_table_iter_init (&citer, view_data->lost_components);
			while (checked_all && g_hash_table_iter_next (&citer, &key, &value)) {
				ECalComponentId *id = key;
				ComponentData *comp_data = value;

				if (!comp_data)
					continue;

				if ((in_range_start == in_range_end && in_range_start == (time_t) 0) ||
				    (comp_data->instance_start < in_range_end && comp_data->instance_end > in_range_start) ||
				    (comp_data->instance_start == comp_data->instance_end && comp_data->instance_end == in_range_start)) {
					if (!func (data_model, view_data->client, id, comp_data->component,
						   comp_data->instance_start, comp_data->instance_end, user_data))
						checked_all = FALSE;
				}
			}
		}

		view_data_unlock (view_data);
	}

	UNLOCK_PROPS ();

	return checked_all;
}

/**
 * e_cal_data_model_foreach_component:
 * @data_model: an #EDataModel instance
 * @in_range_start: Start of the time range
 * @in_range_end: End of the time range
 * @func: a function to be called for each component in the given time range
 * @user_data: user data being passed into the @func
 *
 * Calls @func for each component in the given time range. The time range is
 * clamp by the actual time range defined by subscribers (if there is no
 * subscriber, or all subscribers define times out of the given time range,
 * then the function is not called at all and a #FALSE is returned).
 *
 * The @func returns #TRUE to continue the traversal. If it wants to stop
 * the traversal earlier, then it returns #FALSE.
 *
 * Returns: Whether all the components were checked. The returned value is
 *    usually #TRUE, unless the @func stops traversal earlier.
 *
 * Note: A special case when both @in_range_start and @in_range_end are zero
 *    is treated as a request for all known components.
 *
 * Since: 3.16
 **/
gboolean
e_cal_data_model_foreach_component (ECalDataModel *data_model,
				    time_t in_range_start,
				    time_t in_range_end,
				    ECalDataModelForeachFunc func,
				    gpointer user_data)
{
	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	return cal_data_model_foreach_component (data_model, in_range_start, in_range_end, func, user_data, FALSE);
}

/**
 * e_cal_data_model_subscribe:
 * @data_model: an #EDataModel instance
 * @subscriber: an #ECalDataModelSubscriber instance
 * @range_start: Start of the time range used by the @subscriber
 * @range_end: End of the time range used by the @subscriber
 *
 * Either adds a new @subscriber to the set of subscribers for this
 * @data_model, or changes a time range used by the @subscriber,
 * in case it was added to the @data_model earlier.
 *
 * Reference count of the @subscriber is increased by one, in case
 * it is newly added. The reference count is decreased by one
 * when e_cal_data_model_unsubscribe() is called.
 *
 * Note: A special case when both @range_start and @range_end are zero
 *    is treated as a request with no time constraint. This limits
 *    the result only to those components which satisfy given filter.
 *
 * Since: 3.16
 **/
void
e_cal_data_model_subscribe (ECalDataModel *data_model,
			    ECalDataModelSubscriber *subscriber,
			    time_t range_start,
			    time_t range_end)
{
	SubscriberData *subs_data = NULL;
	GSList *link;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (E_IS_CAL_DATA_MODEL_SUBSCRIBER (subscriber));

	LOCK_PROPS ();

	for (link = data_model->priv->subscribers; link; link = g_slist_next (link)) {
		SubscriberData *tmp_subs_data = link->data;

		if (!tmp_subs_data)
			continue;

		if (tmp_subs_data->subscriber == subscriber)
			break;
	}

	if (link != NULL) {
		time_t new_range_start = range_start, new_range_end = range_end;
		time_t old_range_start, old_range_end;

		/* The subscriber updates its time range (it is already known) */
		subs_data = link->data;

		/* No range change */
		if (range_start == subs_data->range_start &&
		    range_end == subs_data->range_end) {
			UNLOCK_PROPS ();
			return;
		}

		old_range_start = subs_data->range_start;
		old_range_end = subs_data->range_end;

		if (new_range_start == (time_t) 0 && new_range_end == (time_t) 0) {
			new_range_start = data_model->priv->range_start;
			new_range_end = data_model->priv->range_end;
		}

		if (new_range_start == (time_t) 0 && new_range_end == (time_t) 0) {
			/* The subscriber is looking for everything and the data_model has everything too */
			e_cal_data_model_subscriber_freeze (subs_data->subscriber);
			cal_data_model_foreach_component (data_model,
				new_range_start, old_range_start,
				cal_data_model_add_to_subscriber_except_its_range, subs_data, TRUE);
			e_cal_data_model_subscriber_thaw (subs_data->subscriber);
		} else {
			e_cal_data_model_subscriber_freeze (subs_data->subscriber);

			if (new_range_start >= old_range_end ||
			    new_range_end <= old_range_start) {
				subs_data->range_start = range_start;
				subs_data->range_end = range_end;

				/* Completely new range, not overlapping with the former range,
				   everything previously added can be removed... */
				cal_data_model_foreach_component (data_model,
					old_range_start, old_range_end,
					cal_data_model_remove_from_subscriber_except_its_range, subs_data, TRUE);

				subs_data->range_start = old_range_start;
				subs_data->range_end = old_range_end;

				/* ...and components from the new range can be added */
				cal_data_model_foreach_component (data_model,
					new_range_start, new_range_end,
					cal_data_model_add_to_subscriber_except_its_range, subs_data, TRUE);
			} else {
				if (new_range_start < old_range_start) {
					/* Add those known in the new extended range from the start */
					cal_data_model_foreach_component (data_model,
						new_range_start, old_range_start,
						cal_data_model_add_to_subscriber_except_its_range, subs_data, TRUE);
				} else if (new_range_start > old_range_start) {
					subs_data->range_start = range_start;
					subs_data->range_end = range_end;

					/* Remove those out of the new range from the start */
					cal_data_model_foreach_component (data_model,
						old_range_start, new_range_start,
						cal_data_model_remove_from_subscriber_except_its_range, subs_data, TRUE);

					subs_data->range_start = old_range_start;
					subs_data->range_end = old_range_end;
				}

				if (new_range_end > old_range_end) {
					/* Add those known in the new extended range from the end */
					cal_data_model_foreach_component (data_model,
						old_range_end, new_range_end,
						cal_data_model_add_to_subscriber_except_its_range, subs_data, TRUE);
				} else if (new_range_end < old_range_end) {
					subs_data->range_start = range_start;
					subs_data->range_end = range_end;

					/* Remove those out of the new range from the end */
					cal_data_model_foreach_component (data_model,
						new_range_end, old_range_end,
						cal_data_model_remove_from_subscriber_except_its_range, subs_data, TRUE);

					subs_data->range_start = old_range_start;
					subs_data->range_end = old_range_end;
				}
			}

			e_cal_data_model_subscriber_thaw (subs_data->subscriber);
		}

		subs_data->range_start = range_start;
		subs_data->range_end = range_end;
	} else {
		subs_data = subscriber_data_new (subscriber, range_start, range_end);

		data_model->priv->subscribers = g_slist_prepend (data_model->priv->subscribers, subs_data);

		e_cal_data_model_subscriber_freeze (subscriber);
		cal_data_model_foreach_component (data_model, range_start, range_end,
			cal_data_model_add_to_subscriber, subscriber, TRUE);
		e_cal_data_model_subscriber_thaw (subscriber);
	}

	cal_data_model_update_time_range (data_model);

	UNLOCK_PROPS ();
}

/**
 * e_cal_data_model_unsubscribe:
 * @data_model: an #EDataModel instance
 * @subscriber: an #ECalDataModelSubscriber instance
 *
 * Removes the @subscriber from the set of subscribers for the @data_model.
 * Remove of the @subscriber, which is not in the set of subscribers for
 * the @data_model does nothing.
 *
 * Note: The @subscriber is not notified about a removal of the components
 *   which could be added previously, while it was subscribed for the change
 *   notifications.
 *
 * Since: 3.16
 **/
void
e_cal_data_model_unsubscribe (ECalDataModel *data_model,
			      ECalDataModelSubscriber *subscriber)
{
	GSList *link;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (E_IS_CAL_DATA_MODEL_SUBSCRIBER (subscriber));

	LOCK_PROPS ();

	for (link = data_model->priv->subscribers; link; link = g_slist_next (link)) {
		SubscriberData *subs_data = link->data;

		if (!subs_data)
			continue;

		if (subs_data->subscriber == subscriber) {
			data_model->priv->subscribers = g_slist_remove (data_model->priv->subscribers, subs_data);
			subscriber_data_free (subs_data);
			break;
		}
	}

	cal_data_model_update_time_range (data_model);

	UNLOCK_PROPS ();
}

/**
 * e_cal_data_model_get_subscriber_range:
 * @data_model: an #EDataModel instance
 * @subscriber: an #ECalDataModelSubscriber instance
 * @range_start: (out): time range start for the @subscriber
 * @range_end: (out): time range end for the @subscriber
 *
 * Obtains currently set time range for the @subscriber. In case
 * the subscriber is not found returns #FALSE and both @range_start
 * and @range_end are left untouched.
 *
 * Returns: Whether the @subscriber was found and the @range_start with
 *    the @range_end were set to its current time range it uses.
 *
 * Since: 3.16
 **/
gboolean
e_cal_data_model_get_subscriber_range (ECalDataModel *data_model,
				       ECalDataModelSubscriber *subscriber,
				       time_t *range_start,
				       time_t *range_end)
{
	GSList *link;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), FALSE);
	g_return_val_if_fail (E_IS_CAL_DATA_MODEL_SUBSCRIBER (subscriber), FALSE);
	g_return_val_if_fail (range_start, FALSE);
	g_return_val_if_fail (range_end, FALSE);

	LOCK_PROPS ();

	for (link = data_model->priv->subscribers; link; link = g_slist_next (link)) {
		SubscriberData *subs_data = link->data;

		if (!subs_data)
			continue;

		if (subs_data->subscriber == subscriber) {
			*range_start = subs_data->range_start;
			*range_end = subs_data->range_end;
			break;
		}
	}

	UNLOCK_PROPS ();

	return link != NULL;
}

/**
 * e_cal_data_model_freeze_views_update:
 * @data_model: an #EDataModel instance
 *
 * Freezes any views updates until e_cal_data_model_thaw_views_update() is
 * called. This can be called nested, then the same count of the calls of
 * e_cal_data_model_thaw_views_update() is expected to unlock the views update.
 *
 * Since: 3.16
 **/
void
e_cal_data_model_freeze_views_update (ECalDataModel *data_model)
{
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	data_model->priv->views_update_freeze++;

	UNLOCK_PROPS ();
}

/**
 * e_cal_data_model_thaw_views_update:
 * @data_model: an #EDataModel instance
 *
 * A pair function for e_cal_data_model_freeze_views_update(), to unlock
 * views update.
 *
 * Since: 3.16
 **/
void
e_cal_data_model_thaw_views_update (ECalDataModel *data_model)
{
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));

	LOCK_PROPS ();

	if (!data_model->priv->views_update_freeze) {
		UNLOCK_PROPS ();
		g_warn_if_reached ();
		return;
	}

	data_model->priv->views_update_freeze--;
	if (data_model->priv->views_update_freeze == 0 &&
	    data_model->priv->views_update_required)
		cal_data_model_rebuild_everything (data_model, TRUE);

	UNLOCK_PROPS ();
}

/**
 * e_cal_data_model_is_views_update_frozen:
 * @data_model: an #EDataModel instance
 *
 * Check whether any views updates are currently frozen. This is influenced by
 * e_cal_data_model_freeze_views_update() and e_cal_data_model_thaw_views_update().
 *
 * Returns: Whether any views updates are currently frozen.
 *
 * Since: 3.16
 **/
gboolean
e_cal_data_model_is_views_update_frozen (ECalDataModel *data_model)
{
	gboolean is_frozen;

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), FALSE);

	LOCK_PROPS ();

	is_frozen = data_model->priv->views_update_freeze > 0;

	UNLOCK_PROPS ();

	return is_frozen;
}
