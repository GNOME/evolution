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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SIMPLE_ASYNC_RESULT_H
#define E_SIMPLE_ASYNC_RESULT_H

#include <gio/gio.h>

/* Standard GObject macros */
#define E_TYPE_SIMPLE_ASYNC_RESULT \
	(e_simple_async_result_get_type ())
#define E_SIMPLE_ASYNC_RESULT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SIMPLE_ASYNC_RESULT, ESimpleAsyncResult))
#define E_SIMPLE_ASYNC_RESULT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SIMPLE_ASYNC_RESULT, ESimpleAsyncResultClass))
#define E_IS_SIMPLE_ASYNC_RESULT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SIMPLE_ASYNC_RESULT))
#define E_IS_SIMPLE_ASYNC_RESULT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SIMPLE_ASYNC_RESULT))
#define E_SIMPLE_ASYNC_RESULT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SIMPLE_ASYNC_RESULT, ESimpleAsyncResultClass))

G_BEGIN_DECLS

typedef struct _ESimpleAsyncResult ESimpleAsyncResult;
typedef struct _ESimpleAsyncResultClass ESimpleAsyncResultClass;
typedef struct _ESimpleAsyncResultPrivate ESimpleAsyncResultPrivate;

typedef void (* ESimpleAsyncResultThreadFunc)	(ESimpleAsyncResult *result,
						 gpointer source_object,
						 GCancellable *cancellable);

/**
 * ESimpleAsyncResult:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _ESimpleAsyncResult {
	GObject parent;

	ESimpleAsyncResultPrivate *priv;
};

struct _ESimpleAsyncResultClass {
	GObjectClass parent_class;
};

GType		e_simple_async_result_get_type	(void) G_GNUC_CONST;
ESimpleAsyncResult *
		e_simple_async_result_new	(GObject *source_object,
						 GAsyncReadyCallback callback,
						 gpointer user_data,
						 gpointer source_tag);
gboolean	e_simple_async_result_is_valid	(GAsyncResult *result,
						 GObject *source,
						 gpointer source_tag);
void		e_simple_async_result_set_user_data
						(ESimpleAsyncResult *result,
						 gpointer user_data,
						 GDestroyNotify destroy_user_data);
gpointer	e_simple_async_result_get_user_data
						(ESimpleAsyncResult *result);
gpointer	e_simple_async_result_steal_user_data
						(ESimpleAsyncResult *result);
void		e_simple_async_result_set_op_pointer
						(ESimpleAsyncResult *result,
						 gpointer ptr,
						 GDestroyNotify destroy_ptr);
gpointer	e_simple_async_result_get_op_pointer
						(ESimpleAsyncResult *result);
void		e_simple_async_result_run_in_thread
						(ESimpleAsyncResult *result,
						 gint io_priority,
						 ESimpleAsyncResultThreadFunc func,
						 GCancellable *cancellable);
void		e_simple_async_result_complete	(ESimpleAsyncResult *result);
void		e_simple_async_result_complete_idle
						(ESimpleAsyncResult *result);
void		e_simple_async_result_complete_idle_take
						(ESimpleAsyncResult *result);
void		e_simple_async_result_take_error
						(ESimpleAsyncResult *result,
						 GError *error);
gboolean	e_simple_async_result_propagate_error
						(ESimpleAsyncResult *result,
						 GError **error);
void		e_simple_async_result_set_check_cancellable
						(ESimpleAsyncResult *result,
						 GCancellable *cancellable);
void		e_simple_async_result_free_global_memory
						(void);

G_END_DECLS

#endif /* E_SIMPLE_ASYNC_RESULT_H */
