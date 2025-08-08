/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <gio/gio.h>

#include "e-simple-async-result.h"

struct _ESimpleAsyncResultPrivate {
	GObject *source_object;
	GAsyncReadyCallback callback;
	gpointer callback_user_data;
	gpointer source_tag;

	gpointer user_data;
	GDestroyNotify destroy_user_data;

	gpointer op_pointer;
	GDestroyNotify destroy_op_pointer;

	GCancellable *cancellable;
	GError *error;
};

static void e_simple_async_result_iface_init (GAsyncResultIface *iface);

G_DEFINE_TYPE_WITH_CODE (ESimpleAsyncResult, e_simple_async_result, G_TYPE_OBJECT,
	G_ADD_PRIVATE (ESimpleAsyncResult)
	G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT, e_simple_async_result_iface_init))

static gpointer
e_simple_async_result_iface_get_user_data (GAsyncResult *result)
{
	ESimpleAsyncResult *eresult;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	return eresult->priv->callback_user_data;
}

static GObject *
e_simple_async_result_iface_get_source_object (GAsyncResult *result)
{
	ESimpleAsyncResult *eresult;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	return eresult->priv->source_object;
}

static gboolean
e_simple_async_result_iface_is_tagged (GAsyncResult *result,
				       gpointer source_tag)
{
	ESimpleAsyncResult *eresult;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	return eresult && eresult->priv->source_tag == source_tag;
}

static void
e_simple_async_result_iface_init (GAsyncResultIface *iface)
{
	iface->get_user_data = e_simple_async_result_iface_get_user_data;
	iface->get_source_object = e_simple_async_result_iface_get_source_object;
	iface->is_tagged = e_simple_async_result_iface_is_tagged;
}

static void
e_simple_async_result_finalize (GObject *object)
{
	ESimpleAsyncResult *result = E_SIMPLE_ASYNC_RESULT (object);

	if (result->priv->user_data && result->priv->destroy_user_data)
		result->priv->destroy_user_data (result->priv->user_data);

	result->priv->destroy_user_data = NULL;
	result->priv->user_data = NULL;

	if (result->priv->op_pointer && result->priv->destroy_op_pointer)
		result->priv->destroy_op_pointer (result->priv->op_pointer);

	result->priv->destroy_op_pointer = NULL;
	result->priv->op_pointer = NULL;

	g_clear_object (&result->priv->source_object);
	g_clear_object (&result->priv->cancellable);
	g_clear_error (&result->priv->error);

	/* Chain up to parent's method */
	G_OBJECT_CLASS (e_simple_async_result_parent_class)->finalize (object);
}

static void
e_simple_async_result_class_init (ESimpleAsyncResultClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_simple_async_result_finalize;
}

static void
e_simple_async_result_init (ESimpleAsyncResult *result)
{
	result->priv = e_simple_async_result_get_instance_private (result);
}

ESimpleAsyncResult *
e_simple_async_result_new (GObject *source_object,
			   GAsyncReadyCallback callback,
			   gpointer user_data,
			   gpointer source_tag)
{
	ESimpleAsyncResult *result;

	if (source_object)
		g_return_val_if_fail (G_IS_OBJECT (source_object), NULL);

	result = g_object_new (E_TYPE_SIMPLE_ASYNC_RESULT, NULL);

	result->priv->source_object = source_object ? g_object_ref (source_object) : NULL;
	result->priv->callback = callback;
	result->priv->callback_user_data = user_data;
	result->priv->source_tag = source_tag;

	return result;
}

gboolean
e_simple_async_result_is_valid (GAsyncResult *result,
				GObject *source,
				gpointer source_tag)
{
	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

	return g_async_result_get_source_object (result) == source &&
	       g_async_result_is_tagged (result, source_tag);
}

void
e_simple_async_result_set_user_data (ESimpleAsyncResult *result,
				     gpointer user_data,
				     GDestroyNotify destroy_user_data)
{
	ESimpleAsyncResult *eresult;

	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	if (eresult->priv->user_data == user_data)
		return;

	if (eresult->priv->user_data && eresult->priv->destroy_user_data)
		eresult->priv->destroy_user_data (eresult->priv->user_data);

	eresult->priv->user_data = user_data;
	eresult->priv->destroy_user_data = destroy_user_data;
}

gpointer
e_simple_async_result_get_user_data (ESimpleAsyncResult *result)
{
	ESimpleAsyncResult *eresult;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	return eresult->priv->user_data;
}

gpointer
e_simple_async_result_steal_user_data (ESimpleAsyncResult *result)
{
	ESimpleAsyncResult *eresult;
	gpointer user_data;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	user_data = eresult->priv->user_data;

	eresult->priv->user_data = NULL;
	eresult->priv->destroy_user_data = NULL;

	return user_data;
}

void
e_simple_async_result_set_op_pointer (ESimpleAsyncResult *result,
				      gpointer ptr,
				      GDestroyNotify destroy_ptr)
{
	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	if (result->priv->op_pointer == ptr)
		return;

	if (result->priv->op_pointer && result->priv->destroy_op_pointer)
		result->priv->destroy_op_pointer (result->priv->op_pointer);

	result->priv->op_pointer = ptr;
	result->priv->destroy_op_pointer = destroy_ptr;
}

gpointer
e_simple_async_result_get_op_pointer (ESimpleAsyncResult *result)
{
	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	return result->priv->op_pointer;
}

#define DEFAULT_N_THREADS 10
#define MAX_N_THREADS 30

static void
update_thread_pool_threads_locked (GThreadPool *pool,
				   gint n_pending)
{
	gint set_max = 0;

	if (!pool)
		return;

	if (n_pending > g_thread_pool_get_max_threads (pool) && g_thread_pool_get_max_threads (pool) < MAX_N_THREADS)
		set_max = MIN (n_pending, MAX_N_THREADS);
	else if (n_pending <= DEFAULT_N_THREADS && g_thread_pool_get_max_threads (pool) > DEFAULT_N_THREADS)
		set_max = DEFAULT_N_THREADS;

	if (set_max != 0)
		g_thread_pool_set_max_threads (pool, set_max, NULL);
}

static GThreadPool *thread_pool = NULL;
static GThreadPool *low_prio_thread_pool = NULL;
static gint normal_n_pending = 0;
static gint low_prio_n_pending = 0;
static guint update_thread_pool_threads_id = 0;
G_LOCK_DEFINE_STATIC (thread_pool);

static gboolean
update_thread_pool_threads_cb (gpointer user_data)
{
	G_LOCK (thread_pool);
	update_thread_pool_threads_locked (thread_pool, normal_n_pending);
	update_thread_pool_threads_locked (low_prio_thread_pool, low_prio_n_pending);
	update_thread_pool_threads_id = 0;
	G_UNLOCK (thread_pool);

	return FALSE;
}

typedef struct _ThreadData {
	ESimpleAsyncResult *result;
	gint io_priority;
	ESimpleAsyncResultThreadFunc func;
	GCancellable *cancellable;
	gint *p_n_pending;
} ThreadData;

static gint
e_simple_async_result_thread_pool_sort_func (gconstpointer ptra,
					     gconstpointer ptrb,
					     gpointer user_data)
{
	const ThreadData *tda = ptra, *tdb = ptrb;

	if (!tda || !tdb)
		return 0;

	return tda->io_priority < tdb->io_priority ? -1 :
	       tda->io_priority > tdb->io_priority ? 1 : 0;
}

static void
e_simple_async_result_thread (gpointer data,
			      gpointer user_data)
{
	ThreadData *td = data;

	GError *error = NULL;

	g_return_if_fail (td != NULL);
	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (td->result));
	g_return_if_fail (td->func != NULL);

	if (td->result->priv->cancellable &&
	    g_cancellable_set_error_if_cancelled (td->result->priv->cancellable, &error)) {
		e_simple_async_result_take_error (td->result, error);
	} else {
		td->func (td->result,
			g_async_result_get_source_object (G_ASYNC_RESULT (td->result)),
			td->cancellable);
	}

	e_simple_async_result_complete_idle_take (td->result);

	if (g_atomic_int_add (td->p_n_pending, -1) <= DEFAULT_N_THREADS) {
		G_LOCK (thread_pool);
		if (!update_thread_pool_threads_id && (
		    (thread_pool &&
		    g_thread_pool_get_max_threads (thread_pool) > DEFAULT_N_THREADS &&
		    normal_n_pending < g_thread_pool_get_max_threads (thread_pool)) ||
		    (low_prio_thread_pool &&
		    g_thread_pool_get_max_threads (low_prio_thread_pool) > DEFAULT_N_THREADS &&
		    low_prio_n_pending < g_thread_pool_get_max_threads (low_prio_thread_pool)))) {
			update_thread_pool_threads_id = g_timeout_add_seconds (2, update_thread_pool_threads_cb, NULL);
		}
		G_UNLOCK (thread_pool);
	}
	g_clear_object (&td->cancellable);
	g_slice_free (ThreadData, td);
}

void
e_simple_async_result_run_in_thread (ESimpleAsyncResult *result,
				     gint io_priority,
				     ESimpleAsyncResultThreadFunc func,
				     GCancellable *cancellable)
{
	ThreadData *td;
	GThreadPool *use_thread_pool;
	GError *error = NULL;

	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));
	g_return_if_fail (func != NULL);

	if (g_cancellable_set_error_if_cancelled (result->priv->cancellable, &error) ||
	    g_cancellable_set_error_if_cancelled (cancellable, &error)) {
		e_simple_async_result_take_error (result, error);
		e_simple_async_result_complete_idle (result);
		return;
	}

	td = g_slice_new0 (ThreadData);
	td->result = g_object_ref (result);
	td->io_priority = io_priority;
	td->func = func;
	td->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

	G_LOCK (thread_pool);

	if (!thread_pool) {
		thread_pool = g_thread_pool_new (e_simple_async_result_thread, NULL, DEFAULT_N_THREADS, FALSE, NULL);
		g_thread_pool_set_sort_function (thread_pool, e_simple_async_result_thread_pool_sort_func, NULL);

		low_prio_thread_pool = g_thread_pool_new (e_simple_async_result_thread, NULL, DEFAULT_N_THREADS, FALSE, NULL);
		g_thread_pool_set_sort_function (low_prio_thread_pool, e_simple_async_result_thread_pool_sort_func, NULL);
	}

	if (io_priority >= G_PRIORITY_LOW) {
		td->p_n_pending = &low_prio_n_pending;
		use_thread_pool = low_prio_thread_pool;
	} else {
		td->p_n_pending = &normal_n_pending;
		use_thread_pool = thread_pool;
	}

	g_atomic_int_add (td->p_n_pending, 1);

	if (!update_thread_pool_threads_id &&
	    *td->p_n_pending > g_thread_pool_get_max_threads (use_thread_pool)) {
		update_thread_pool_threads_id = g_timeout_add_seconds (2, update_thread_pool_threads_cb, NULL);
	}

	g_thread_pool_push (use_thread_pool, td, NULL);

	G_UNLOCK (thread_pool);
}

void
e_simple_async_result_complete (ESimpleAsyncResult *result)
{
	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	g_object_ref (result);

	if (result->priv->callback)
		result->priv->callback (result->priv->source_object, G_ASYNC_RESULT (result), result->priv->callback_user_data);

	g_object_unref (result);
}

static gboolean
result_complete_idle_cb (gpointer user_data)
{
	ESimpleAsyncResult *result = user_data;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

	e_simple_async_result_complete (result);
	g_object_unref (result);

	return FALSE;
}

void
e_simple_async_result_complete_idle (ESimpleAsyncResult *result)
{
	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	e_simple_async_result_complete_idle_take (g_object_ref (result));
}

/* The same as e_simple_async_result_complete_idle(), but assumes ownership
   of the 'result' argument. */
void
e_simple_async_result_complete_idle_take (ESimpleAsyncResult *result)
{
	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	g_idle_add (result_complete_idle_cb, result);
}

void
e_simple_async_result_take_error (ESimpleAsyncResult *result,
				  GError *error)
{
	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	if (error != result->priv->error) {
		g_clear_error (&result->priv->error);
		result->priv->error = error;
	}
}

gboolean
e_simple_async_result_propagate_error (ESimpleAsyncResult *result,
				       GError **error)
{
	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

	if (!result->priv->error)
		return FALSE;

	if (error)
		g_propagate_error (error, g_error_copy (result->priv->error));

	return TRUE;
}

void
e_simple_async_result_set_check_cancellable (ESimpleAsyncResult *result,
					     GCancellable *cancellable)
{
	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	if (result->priv->cancellable != cancellable) {
		g_clear_object (&result->priv->cancellable);
		if (cancellable)
			result->priv->cancellable = g_object_ref (cancellable);
	}
}

void
e_simple_async_result_free_global_memory (void)
{
	G_LOCK (thread_pool);

	if (thread_pool) {
		g_thread_pool_free (thread_pool, TRUE, FALSE);
		thread_pool = NULL;
	}

	if (low_prio_thread_pool) {
		g_thread_pool_free (low_prio_thread_pool, TRUE, FALSE);
		low_prio_thread_pool = NULL;
	}

	G_UNLOCK (thread_pool);
}
