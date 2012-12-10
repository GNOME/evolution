/*
 * e-client-utils.h
 *
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
 * Copyright (C) 2011 Red Hat, Inc. (www.redhat.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CLIENT_UTILS_H
#define E_CLIENT_UTILS_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

G_BEGIN_DECLS

/**
 * EClientSourceType:
 *
 * Since: 3.2
 **/
typedef enum {
	E_CLIENT_SOURCE_TYPE_CONTACTS,
	E_CLIENT_SOURCE_TYPE_EVENTS,
	E_CLIENT_SOURCE_TYPE_MEMOS,
	E_CLIENT_SOURCE_TYPE_TASKS,
	E_CLIENT_SOURCE_TYPE_LAST
} EClientSourceType;

EClient	*	e_client_utils_new		(ESource *source,
						 EClientSourceType source_type,
						 GError **error);

void		e_client_utils_open_new		(ESource *source,
						 EClientSourceType source_type,
						 gboolean only_if_exists,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_client_utils_open_new_finish	(ESource *source,
						 GAsyncResult *result,
						 EClient **client,
						 GError **error);

G_END_DECLS

#endif /* E_CLIENT_UTILS_H */
