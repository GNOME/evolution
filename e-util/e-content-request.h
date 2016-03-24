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

#ifndef E_CONTENT_REQUEST_H
#define E_CONTENT_REQUEST_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

/* Standard GObject macros */
#define E_TYPE_CONTENT_REQUEST \
	(e_content_request_get_type ())
#define E_CONTENT_REQUEST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTENT_REQUEST, EContentRequest))
#define E_CONTENT_REQUEST_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTENT_REQUEST, EContentRequestInterface))
#define E_IS_CONTENT_REQUEST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTENT_REQUEST))
#define E_IS_CONTENT_REQUEST_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONTENT_REQUEST))
#define E_CONTENT_REQUEST_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_CONTENT_REQUEST, EContentRequestInterface))

G_BEGIN_DECLS

typedef struct _EContentRequest EContentRequest;
typedef struct _EContentRequestInterface EContentRequestInterface;

struct _EContentRequestInterface {
	GTypeInterface parent_interface;

	gboolean	(* can_process_uri)	(EContentRequest *request,
						 const gchar *uri);
	gboolean	(* process_sync)	(EContentRequest *request,
						 const gchar *uri,
						 GObject *requester,
						 GInputStream **out_stream,
						 gint64 *out_stream_length,
						 gchar **out_mime_type,
						 GCancellable *cancellable,
						 GError **error);
};

GType		e_content_request_get_type		(void);
gboolean	e_content_request_can_process_uri	(EContentRequest *request,
							 const gchar *uri);
gboolean	e_content_request_process_sync		(EContentRequest *request,
							 const gchar *uri,
							 GObject *requester,
							 GInputStream **out_stream,
							 gint64 *out_stream_length,
							 gchar **out_mime_type,
							 GCancellable *cancellable,
							 GError **error);
void		e_content_request_process		(EContentRequest *request,
							 const gchar *uri,
							 GObject *requester,
							 GCancellable *cancellable,
							 GAsyncReadyCallback callback,
							 gpointer user_data);
gboolean	e_content_request_process_finish	(EContentRequest *request,
							 GAsyncResult *result,
							 GInputStream **out_stream,
							 gint64 *out_stream_length,
							 gchar **out_mime_type,
							 GError **error);

G_END_DECLS

#endif /* E_CONTENT_REQUEST_H */
