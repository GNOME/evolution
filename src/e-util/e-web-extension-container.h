/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2019 Red Hat (www.redhat.com)
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

#ifndef E_WEB_EXTENSION_CONTAINER_H
#define E_WEB_EXTENSION_CONTAINER_H

/* Standard GObject macros */
#define E_TYPE_WEB_EXTENSION_CONTAINER \
	(e_web_extension_container_get_type ())
#define E_WEB_EXTENSION_CONTAINER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEB_EXTENSION_CONTAINER, EWebExtensionContainer))
#define E_WEB_EXTENSION_CONTAINER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEB_EXTENSION_CONTAINER, EWebExtensionContainerClass))
#define E_IS_WEB_EXTENSION_CONTAINER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEB_EXTENSION_CONTAINER))
#define E_IS_WEB_EXTENSION_CONTAINER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEB_EXTENSION_CONTAINER))
#define E_WEB_EXTENSION_CONTAINER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEB_EXTENSION_CONTAINER, EWebExtensionContainerClass))

G_BEGIN_DECLS

#include <gio/gio.h>

typedef struct _EWebExtensionContainer EWebExtensionContainer;
typedef struct _EWebExtensionContainerClass EWebExtensionContainerClass;
typedef struct _EWebExtensionContainerPrivate EWebExtensionContainerPrivate;

struct _EWebExtensionContainer {
	GObject parent;
	EWebExtensionContainerPrivate *priv;
};

struct _EWebExtensionContainerClass {
	GObjectClass parent_class;

	/* Signals */
	void		(* page_proxy_changed)		(EWebExtensionContainer *container,
							 guint64 page_id,
							 gint stamp,
							 GDBusProxy *proxy);
};

GType		e_web_extension_container_get_type	(void) G_GNUC_CONST;
EWebExtensionContainer *
		e_web_extension_container_new		(const gchar *object_path,
							 const gchar *interface_name);
const gchar *	e_web_extension_container_get_object_path
							(EWebExtensionContainer *container);
const gchar *	e_web_extension_container_get_interface_name
							(EWebExtensionContainer *container);
const gchar *	e_web_extension_container_get_server_guid
							(EWebExtensionContainer *container);
const gchar *	e_web_extension_container_get_server_address
							(EWebExtensionContainer *container);
gint		e_web_extension_container_reserve_stamp	(EWebExtensionContainer *container);
GDBusProxy *	e_web_extension_container_ref_proxy	(EWebExtensionContainer *container,
							 guint64 page_id,
							 gint stamp);
void		e_web_extension_container_forget_stamp	(EWebExtensionContainer *container,
							 gint stamp);
void		e_web_extension_container_call_simple	(EWebExtensionContainer *container,
							 guint64 page_id,
							 gint stamp,
							 const gchar *method_name,
							 GVariant *params);

void		e_web_extension_container_utils_connect_to_server
							(const gchar *server_address,
							 GCancellable *cancellable,
							 GAsyncReadyCallback callback,
							 gpointer user_data);
GDBusConnection *
		e_web_extension_container_utils_connect_to_server_finish
							(GAsyncResult *result,
							 GError **error);

G_END_DECLS

#endif /* E_WEB_EXTENSION_CONTAINER_H */
