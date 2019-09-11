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

#include "evolution-config.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <libedataserver/libedataserver.h>

#include "e-web-extension-container.h"

/**
 * SECTION: e-web-extension-container
 * @include: e-util/e-util.h
 * @short_description: Container of WebKitGTK+ Web Extensions
 *
 * #EWebExtensionContainer is a container of WebKitGTK+ Web Extensions, which
 * runs a D-Bus server to which the Web Extensions can connect. This object
 * is meant for the client side. It also takes care of all the connections
 * and corresponding #GDBusProxy objects.
 *
 * The Web Extension part can use e_web_extension_container_utils_connect_to_server()
 * to connect to it.
 **/

struct _EWebExtensionContainerPrivate {
	gchar *object_path;
	gchar *interface_name;
	GDBusServer *dbus_server;
	GHashTable *proxy_by_page; /* ProxyIdentData ~> ProxyPageData */
	GSList *proxy_instances; /* ProxyInstanceData */

	volatile gint current_stamp;
	GHashTable *used_stamps; /* GINT_TO_POINTER(stamp) ~> NULL */
};

enum {
	PROP_0,
	PROP_INTERFACE_NAME,
	PROP_OBJECT_PATH
};

enum {
	PAGE_PROXY_CHANGED,
	LAST_SIGNAL
};

static gulong signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EWebExtensionContainer, e_web_extension_container, G_TYPE_OBJECT)

static gboolean
authorize_authenticated_peer_cb (GDBusAuthObserver *observer,
				 GIOStream *stream,
				 GCredentials *credentials,
				 gpointer user_data)
{
	GCredentials *my_credentials;
	GError *error = NULL;
	gboolean res;

	if (!credentials)
		return FALSE;

	my_credentials = g_credentials_new ();

	res = g_credentials_is_same_user (credentials, my_credentials, &error);

	if (error) {
		g_warning ("Failed to authrorize credentials: %s", error->message);
		g_clear_error (&error);
	}

	g_clear_object (&my_credentials);

	return res;
}

static GDBusServer *
ewec_new_server_sync (const gchar *object_path,
		      GCancellable *cancellable,
		      GError **error)
{
	GDBusAuthObserver *observer;
	GDBusServer *server;
	gchar *path_part, *path, *tmpdir, *address, *guid;
	gint ii, fd;

	path_part = g_strconcat ("evolution-",
		object_path ? object_path : "",
		object_path ? "-" : "",
		g_get_user_name (),
		"-XXXXXX", NULL);

	for (ii = 0; path_part[ii]; ii++) {
		if (path_part[ii] == '/' ||
		    path_part[ii] == '\\' ||
		    path_part[ii] == ':' ||
		    path_part[ii] == '?' ||
		    path_part[ii] == '*')
			path_part[ii] = '_';
	}

	path = g_build_filename (g_get_tmp_dir (), path_part, NULL);

	fd = g_mkstemp (path);
	if (fd == -1) {
		tmpdir = NULL;
	} else {
		close (fd);
		tmpdir = path;
		g_unlink (tmpdir);
	}

	g_free (path_part);

	if (!tmpdir) {
		g_free (path);

		g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno), "%s", g_strerror (errno));
		return NULL;
	}

	address = g_strconcat ("unix:path=", path, NULL);

	g_free (path);

	guid = g_dbus_generate_guid ();
	observer = g_dbus_auth_observer_new ();

	g_signal_connect (observer, "authorize-authenticated-peer",
		G_CALLBACK (authorize_authenticated_peer_cb), NULL);

	server = g_dbus_server_new_sync (address, G_DBUS_SERVER_FLAGS_NONE, guid, observer, cancellable, error);

	g_object_unref (observer);
	g_free (address);
	g_free (guid);

	if (server)
		g_dbus_server_start (server);

	return server;
}

typedef struct _CallSimpleData {
	gchar *method_name;
	GVariant *params;
} CallSimpleData;

static CallSimpleData *
call_simple_data_new (const gchar *method_name,
		      GVariant *params)
{
	CallSimpleData *csd;

	csd = g_new0 (CallSimpleData, 1);
	csd->method_name = g_strdup (method_name);
	csd->params = params ? g_variant_ref_sink (params) : NULL;

	return csd;
}

static void
call_simple_data_free (gpointer ptr)
{
	CallSimpleData *csd = ptr;

	if (csd) {
		g_free (csd->method_name);
		if (csd->params)
			g_variant_unref (csd->params);
		g_free (csd);
	}
}

typedef struct _ProxyDataIdent {
	guint64 page_id;
	gint stamp;
} ProxyDataIdent;

static ProxyDataIdent *
proxy_data_ident_new (guint64 page_id,
		      gint stamp)
{
	ProxyDataIdent *pdi;

	pdi = g_new0 (ProxyDataIdent, 1);
	pdi->page_id = page_id;
	pdi->stamp = stamp;

	return pdi;
}

static void
proxy_data_ident_free (gpointer ptr)
{
	ProxyDataIdent *pdi = ptr;

	if (pdi) {
		g_free (pdi);
	}
}

static guint
proxy_data_ident_hash (gconstpointer ptr)
{
	const ProxyDataIdent *pdi = ptr;
	gint64 signed_page_id;

	if (!pdi)
		return 0;

	signed_page_id = pdi->page_id;

	return g_int64_hash (&signed_page_id) ^ g_direct_hash (GINT_TO_POINTER (pdi->stamp));
}

static gboolean
proxy_data_ident_equal (gconstpointer aa,
			gconstpointer bb)
{
	const ProxyDataIdent *pdi1 = aa, *pdi2 = bb;

	if (pdi1 == pdi2)
		return TRUE;

	if (!pdi1 || !pdi2)
		return FALSE;

	return pdi1->page_id == pdi2->page_id && pdi1->stamp == pdi2->stamp;
}

typedef struct _ProxyPageData {
	GDBusProxy *proxy;
	GSList *pending_calls; /* CallSimpleData * */
} ProxyPageData;

static ProxyPageData *
proxy_page_data_new (GDBusProxy *proxy) /* can be NULL, adds its own reference if not */
{
	ProxyPageData *ppd;

	ppd = g_new0 (ProxyPageData, 1);
	ppd->proxy = proxy;
	ppd->pending_calls = NULL;

	if (proxy)
		g_object_ref (proxy);

	return ppd;
}

static void
proxy_page_data_free (gpointer ptr)
{
	ProxyPageData *ppd = ptr;

	if (ppd) {
		g_clear_object (&ppd->proxy);
		g_slist_free_full (ppd->pending_calls, call_simple_data_free);
		g_free (ppd);
	}
}

typedef struct _ProxyInstanceData {
	GDBusConnection *connection;
	GDBusProxy *proxy;
	guint extension_object_ready_signal_id;
	guint extension_handles_page_signal_id;
	gulong connection_closed_signal_id;
} ProxyInstanceData;

static void
proxy_instance_gone_cb (gpointer user_data,
			GObject *obj)
{
	ProxyInstanceData *pid = user_data;

	g_return_if_fail (pid != NULL);
	g_return_if_fail (pid->proxy == (GDBusProxy *) obj);

	pid->proxy = NULL;
}

static ProxyInstanceData *
proxy_instance_data_new (GDBusProxy *proxy) /* Takes ownership */
{
	ProxyInstanceData *pid;

	g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);

	pid = g_new0 (ProxyInstanceData, 1);
	pid->connection = g_object_ref (g_dbus_proxy_get_connection (proxy));
	pid->proxy = proxy;

	g_object_weak_ref (G_OBJECT (proxy), proxy_instance_gone_cb, pid);

	return pid;
}

static void
proxy_instance_data_free (gpointer ptr)
{
	ProxyInstanceData *pid = ptr;

	if (pid) {
		if (pid->proxy)
			g_object_weak_unref (G_OBJECT (pid->proxy), proxy_instance_gone_cb, pid);

		if (pid->connection) {
			if (pid->extension_object_ready_signal_id) {
				g_dbus_connection_signal_unsubscribe (pid->connection, pid->extension_object_ready_signal_id);
				pid->extension_object_ready_signal_id = 0;
			}

			if (pid->extension_handles_page_signal_id) {
				g_dbus_connection_signal_unsubscribe (pid->connection, pid->extension_handles_page_signal_id);
				pid->extension_handles_page_signal_id = 0;
			}

			if (pid->connection_closed_signal_id) {
				g_signal_handler_disconnect (pid->connection, pid->connection_closed_signal_id);
				pid->connection_closed_signal_id = 0;
			}
		}

		g_clear_object (&pid->connection);
		g_clear_object (&pid->proxy);
		g_free (pid);
	}
}

static void
ewec_simple_call_finished_cb (GObject *source_object,
			      GAsyncResult *async_result,
			      gpointer user_data)
{
	GVariant *result;
	GError *error = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), async_result, &error);

	if (error) {
		if (!g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_DISCONNECTED) &&
		    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CLOSED) &&
		    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED))
			g_warning ("%s: D-Bus call failed: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else if (result) {
		g_variant_unref (result);
	}
}

static void
ewec_clain_proxy_handles_page (EWebExtensionContainer *container,
			       GDBusProxy *proxy,
			       guint64 page_id,
			       gint stamp)
{
	ProxyDataIdent data_ident;
	ProxyPageData *ppd;

	data_ident.page_id = page_id;
	data_ident.stamp = stamp;

	ppd = g_hash_table_lookup (container->priv->proxy_by_page, &data_ident);

	if (ppd && ppd->proxy != proxy) {
		g_clear_object (&ppd->proxy);
		ppd->proxy = g_object_ref (proxy);

		if (ppd->pending_calls) {
			GSList *pending_calls, *link;

			pending_calls = g_slist_reverse (ppd->pending_calls);
			ppd->pending_calls = NULL;

			for (link = pending_calls; link; link = g_slist_next (link)) {
				CallSimpleData *csd = link->data;

				if (csd && !g_dbus_connection_is_closed (g_dbus_proxy_get_connection (ppd->proxy))) {
					g_dbus_proxy_call (ppd->proxy, csd->method_name, csd->params,
						G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, ewec_simple_call_finished_cb, NULL);
				}
			}

			g_slist_free_full (pending_calls, call_simple_data_free);
		}
	} else if (!ppd) {
		ProxyDataIdent *pdi;

		pdi = proxy_data_ident_new (page_id, stamp);
		ppd = proxy_page_data_new (proxy);

		g_hash_table_insert (container->priv->proxy_by_page, pdi, ppd);
	}

	g_signal_emit (container, signals[PAGE_PROXY_CHANGED], 0, page_id, stamp, proxy);
}

static void
ewec_get_extension_handles_pages_cb (GObject *sender_object,
				     GAsyncResult *async_result,
				     gpointer user_data)
{
	GWeakRef *container_weak_ref = user_data;
	GDBusProxy *proxy;
	GVariant *result;
	GError *error = NULL;

	g_return_if_fail (container_weak_ref != NULL);
	g_return_if_fail (G_IS_DBUS_PROXY (sender_object));

	proxy = G_DBUS_PROXY (sender_object);
	result = g_dbus_proxy_call_finish (proxy, async_result, &error);

	if (error) {
		/* This can happen when the object is not registered on the connection yet */
		if (!g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD) &&
		    !g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_DISCONNECTED) &&
		    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CLOSED) &&
		    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED))
			g_warning ("Failed to call GetExtensionHandlesPages: %s", error->message);
		g_clear_error (&error);
	} else if (result) {
		EWebExtensionContainer *container;

		container = g_weak_ref_get (container_weak_ref);

		if (container) {
			GVariantIter *iter = NULL;
			guint64 page_id;
			guint64 stamp;

			g_variant_get (result, "(at)", &iter);

			while (g_variant_iter_loop (iter, "t", &page_id) &&
			       g_variant_iter_loop (iter, "t", &stamp)) {
				ewec_clain_proxy_handles_page (container, proxy, page_id, (gint) stamp);
			}

			g_variant_iter_free (iter);
			g_object_unref (container);
		}

		g_variant_unref (result);
	}

	e_weak_ref_free (container_weak_ref);
}

static void
ewec_extension_handle_signal_cb (GDBusConnection *connection,
				 const gchar *sender_name,
				 const gchar *object_path,
				 const gchar *interface_name,
				 const gchar *signal_name,
				 GVariant *parameters,
				 gpointer user_data)
{
	EWebExtensionContainer *container = user_data;
	GSList *link;
	GDBusProxy *proxy = NULL;

	g_return_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container));

	for (link = container->priv->proxy_instances; link; link = g_slist_next (link)) {
		ProxyInstanceData *pid = link->data;

		if (pid && pid->proxy &&
		    g_dbus_proxy_get_connection (pid->proxy) == connection) {
			proxy = pid->proxy;
			break;
		}
	}

	if (g_strcmp0 (signal_name, "ExtensionObjectReady") == 0) {
		g_return_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container));

		if (proxy && !g_dbus_connection_is_closed (g_dbus_proxy_get_connection (proxy))) {
			g_dbus_proxy_call (proxy, "GetExtensionHandlesPages", NULL, G_DBUS_CALL_FLAGS_NO_AUTO_START,
				-1, NULL, ewec_get_extension_handles_pages_cb, e_weak_ref_new (container));
		}
	} else if (g_strcmp0 (signal_name, "ExtensionHandlesPage") == 0) {
		guint64 page_id = 0;
		gint stamp = 0;

		if (!parameters)
			return;

		g_variant_get (parameters, "(ti)", &page_id, &stamp);

		if (proxy)
			ewec_clain_proxy_handles_page (container, proxy, page_id, stamp);
	}
}

static void
ewec_connection_closed_cb (GDBusConnection *connection,
			   gboolean remote_peer_vanished,
			   GError *error,
			   gpointer user_data)
{
	EWebExtensionContainer *container = user_data;
	GSList *link, *prev;

	g_return_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container));

	prev = NULL;
	for (link = container->priv->proxy_instances; link; prev = link, link = g_slist_next (link)) {
		ProxyInstanceData *pid = link->data;

		if (pid && pid->proxy && g_dbus_proxy_get_connection (pid->proxy) == connection) {
			GHashTableIter iter;
			gpointer key, value;
			GSList *to_remove = NULL, *trlink;

			link = prev;
			container->priv->proxy_instances = g_slist_remove (container->priv->proxy_instances, pid);

			if (!link)
				link = container->priv->proxy_instances;

			g_hash_table_iter_init (&iter, container->priv->proxy_by_page);

			while (g_hash_table_iter_next (&iter, &key, &value)) {
				ProxyDataIdent *pdi = key;
				ProxyPageData *ppd = value;

				if (ppd && ppd->proxy == pid->proxy)
					to_remove = g_slist_prepend (to_remove, pdi);
			}

			for (trlink = to_remove; trlink; trlink = g_slist_next (trlink)) {
				ProxyDataIdent *pdi = trlink->data;
				guint64 page_id;
				gint stamp;

				page_id = pdi->page_id;
				stamp = pdi->stamp;

				if (g_hash_table_remove (container->priv->proxy_by_page, pdi)) {
					g_signal_emit (container, signals[PAGE_PROXY_CHANGED], 0, page_id, stamp, NULL);
				}
			}

			proxy_instance_data_free (pid);
			g_slist_free (to_remove);
		}
	}
}

static void
e_web_extension_container_proxy_created_cb (GObject *source_object,
					    GAsyncResult *result,
					    gpointer user_data)
{
	EWebExtensionContainer *container;
	GWeakRef *container_weak_ref = user_data;
	GDBusProxy *proxy;
	ProxyInstanceData *pid;
	GError *error = NULL;

	g_return_if_fail (container_weak_ref != NULL);

	container = g_weak_ref_get (container_weak_ref);
	if (!container) {
		e_weak_ref_free (container_weak_ref);
		return;
	}

	proxy = g_dbus_proxy_new_finish (result, &error);
	if (!proxy) {
		g_warning ("Error creating web extension proxy: %s", error ? error->message : "Unknown error");

		e_weak_ref_free (container_weak_ref);
		g_object_unref (container);
		g_error_free (error);
		return;
	}

	pid = proxy_instance_data_new (proxy);
	if (pid) {
		container->priv->proxy_instances = g_slist_prepend (container->priv->proxy_instances, pid);

		pid->extension_object_ready_signal_id =
			g_dbus_connection_signal_subscribe (
				g_dbus_proxy_get_connection (proxy),
				g_dbus_proxy_get_name (proxy),
				container->priv->interface_name,
				"ExtensionObjectReady",
				container->priv->object_path,
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				ewec_extension_handle_signal_cb,
				container,
				NULL);

		pid->extension_handles_page_signal_id =
			g_dbus_connection_signal_subscribe (
				g_dbus_proxy_get_connection (proxy),
				g_dbus_proxy_get_name (proxy),
				container->priv->interface_name,
				"ExtensionHandlesPage",
				container->priv->object_path,
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				ewec_extension_handle_signal_cb,
				container,
				NULL);

		pid->connection_closed_signal_id =
			g_signal_connect (g_dbus_proxy_get_connection (proxy), "closed",
				G_CALLBACK (ewec_connection_closed_cb), container);

		g_dbus_proxy_call (proxy, "GetExtensionHandlesPages", NULL, G_DBUS_CALL_FLAGS_NO_AUTO_START,
			-1, NULL, ewec_get_extension_handles_pages_cb, container_weak_ref);
	} else {
		e_weak_ref_free (container_weak_ref);
	}

	g_object_unref (container);
}

static gboolean
e_web_extension_container_new_connection_cb (GDBusServer *server,
					     GDBusConnection *connection,
					     gpointer user_data)
{
	EWebExtensionContainer *container = user_data;

	g_return_val_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container), FALSE);

	g_dbus_proxy_new (
		connection,
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
		G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL,
		NULL,
		container->priv->object_path,
		container->priv->interface_name,
		NULL,
		e_web_extension_container_proxy_created_cb,
		e_weak_ref_new (container));

	return TRUE;
}

static void
web_extension_container_set_object_path (EWebExtensionContainer *container,
					 const gchar *object_path)
{
	g_return_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container));
	g_return_if_fail (object_path && *object_path);

	if (g_strcmp0 (container->priv->object_path, object_path)) {
		g_free (container->priv->object_path);
		container->priv->object_path = g_strdup (object_path);
	}
}

static void
web_extension_container_set_interface_name (EWebExtensionContainer *container,
					    const gchar *interface_name)
{
	g_return_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container));
	g_return_if_fail (interface_name && *interface_name);

	if (g_strcmp0 (container->priv->interface_name, interface_name)) {
		g_free (container->priv->interface_name);
		container->priv->interface_name = g_strdup (interface_name);
	}
}

static void
web_extension_container_set_property (GObject *object,
				      guint property_id,
				      const GValue *value,
				      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_INTERFACE_NAME:
			web_extension_container_set_interface_name (
				E_WEB_EXTENSION_CONTAINER (object),
				g_value_get_string (value));
			return;

		case PROP_OBJECT_PATH:
			web_extension_container_set_object_path (
				E_WEB_EXTENSION_CONTAINER (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
web_extension_container_get_property (GObject *object,
				      guint property_id,
				      GValue *value,
				      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_INTERFACE_NAME:
			g_value_set_string (
				value, e_web_extension_container_get_interface_name (
				E_WEB_EXTENSION_CONTAINER (object)));
			return;

		case PROP_OBJECT_PATH:
			g_value_set_string (
				value, e_web_extension_container_get_object_path (
				E_WEB_EXTENSION_CONTAINER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
web_extension_container_instance_gone_cb (gpointer user_data,
					  GObject *instance)
{
	GHashTable **pinstances = user_data;
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (pinstances != NULL);
	g_return_if_fail (*pinstances != NULL);

	g_hash_table_iter_init (&iter, *pinstances);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GObject *stored_instance = value;

		if (stored_instance == instance) {
			g_hash_table_remove (*pinstances, key);
			break;
		}
	}

	if (!g_hash_table_size (*pinstances)) {
		g_hash_table_destroy (*pinstances);
		*pinstances = NULL;
	}
}

static GObject *
web_extension_container_constructor (GType type,
				     guint n_construct_params,
				     GObjectConstructParam *construct_params)
{
	static GHashTable *instances = NULL;
	const gchar *object_path = NULL;
	GObject *instance;
	guint ii;

	for (ii = 0; ii < n_construct_params; ii++) {
		if (construct_params[ii].pspec &&
		    g_strcmp0 (construct_params[ii].pspec->name, "object-path") == 0) {
			object_path = g_value_get_string (construct_params[ii].value);
			break;
		}
	}

	if (!object_path)
		object_path = "";

	if (instances) {
		instance = g_hash_table_lookup (instances, object_path);

		if (instance)
			return g_object_ref (instance);
	}

	instance = G_OBJECT_CLASS (e_web_extension_container_parent_class)->constructor (type, n_construct_params, construct_params);

	if (instance) {
		if (!instances)
			instances = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

		g_hash_table_insert (instances, g_strdup (object_path), instance);

		g_object_weak_ref (instance, web_extension_container_instance_gone_cb, &instances);
	}

	return instance;
}

static void
web_extension_container_constructed (GObject *object)
{
	EWebExtensionContainer *container = E_WEB_EXTENSION_CONTAINER (object);
	GError *error = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_web_extension_container_parent_class)->constructed (object);

	container->priv->dbus_server = ewec_new_server_sync (container->priv->object_path, NULL, &error);

	if (container->priv->dbus_server) {
		g_signal_connect_object (container->priv->dbus_server, "new-connection",
			G_CALLBACK (e_web_extension_container_new_connection_cb), container, 0);
	} else {
		g_warning ("%s: Failed to create D-Bus server for object_path '%s': %s", G_STRFUNC,
			container->priv->object_path ? container->priv->object_path : "[null]",
			error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
}

static void
web_extension_container_dispose (GObject *object)
{
	EWebExtensionContainer *container = E_WEB_EXTENSION_CONTAINER (object);
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, container->priv->proxy_by_page);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ProxyDataIdent *pdi = key;
		ProxyPageData *ppd = value;

		if (pdi && ppd && ppd->proxy)
			g_signal_emit (container, signals[PAGE_PROXY_CHANGED], 0, pdi->page_id, pdi->stamp, NULL);
	}

	g_hash_table_remove_all (container->priv->proxy_by_page);
	g_hash_table_remove_all (container->priv->used_stamps);
	g_slist_free_full (container->priv->proxy_instances, proxy_instance_data_free);
	container->priv->proxy_instances = NULL;
	g_clear_object (&container->priv->dbus_server);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_web_extension_container_parent_class)->dispose (object);
}

static void
web_extension_container_finalize (GObject *object)
{
	EWebExtensionContainer *container = E_WEB_EXTENSION_CONTAINER (object);

	g_clear_pointer (&container->priv->interface_name, g_free);
	g_clear_pointer (&container->priv->object_path, g_free);
	g_hash_table_destroy (container->priv->proxy_by_page);
	g_hash_table_destroy (container->priv->used_stamps);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_web_extension_container_parent_class)->finalize (object);
}

static void
e_web_extension_container_class_init (EWebExtensionContainerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = web_extension_container_set_property;
	object_class->get_property = web_extension_container_get_property;
	object_class->constructor = web_extension_container_constructor;
	object_class->constructed = web_extension_container_constructed;
	object_class->dispose = web_extension_container_dispose;
	object_class->finalize = web_extension_container_finalize;

	g_object_class_install_property (
		object_class,
		PROP_OBJECT_PATH,
		g_param_spec_string (
			"object-path",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_INTERFACE_NAME,
		g_param_spec_string (
			"interface-name",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EWebExtensionContainer::page-proxy-changed:
	 * @page_id: a page ID
	 * @stamp: a stamp
	 * @proxy: (nullable): a #GDBusProxy or %NULL
	 *
	 * A signal emitted whenever a proxy for the given @page_id and
	 * @stamp changes. The @proxy can be %NULL, which means the previous
	 * proxy disappeared.
	 *
	 * Since: 3.34.1
	 **/
	signals[PAGE_PROXY_CHANGED] = g_signal_new (
		"page-proxy-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EWebExtensionContainerClass, page_proxy_changed),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 3,
		G_TYPE_UINT64,
		G_TYPE_INT,
		G_TYPE_DBUS_PROXY);
}

static void
e_web_extension_container_init (EWebExtensionContainer *container)
{
	container->priv = e_web_extension_container_get_instance_private (container);
	container->priv->proxy_by_page = g_hash_table_new_full (proxy_data_ident_hash, proxy_data_ident_equal, proxy_data_ident_free, proxy_page_data_free);
	container->priv->used_stamps = g_hash_table_new (g_direct_hash, g_direct_equal);
	container->priv->current_stamp = 0;
}

/**
 * e_web_extension_container_new:
 * @object_path: a D-Bus object path
 * @interface_name: a D-Bus interface name
 *
 * Creates a new #EWebExtensionContainer, which opens a new D-Bus server
 * for the given @object_path. If any such already exists, then it is returned
 * instead.
 *
 * The @interface_name is used for the e_web_extension_container_call_simple()
 * and for the #GDBusProxy instances managed by this object.
 *
 * Returns: (transfer full): a new #EWebExtensionContainer. Free it with
 *    g_object_unref(), when no longer needed.
 *
 * Since: 3.34.1
 **/
EWebExtensionContainer *
e_web_extension_container_new (const gchar *object_path,
			       const gchar *interface_name)
{
	g_return_val_if_fail (object_path && *object_path, NULL);
	g_return_val_if_fail (interface_name && *interface_name, NULL);

	return g_object_new (E_TYPE_WEB_EXTENSION_CONTAINER,
		"object-path", object_path,
		"interface-name", interface_name,
		NULL);
}

/**
 * e_web_extension_container_get_object_path:
 * @container: an #EWebExtensionContainer
 *
 * Returns: the object path the @container had been created with
 *
 * Since: 3.34.1
 **/
const gchar *
e_web_extension_container_get_object_path (EWebExtensionContainer *container)
{
	g_return_val_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container), NULL);

	return container->priv->object_path;
}

/**
 * e_web_extension_container_get_interface_name:
 * @container: an #EWebExtensionContainer
 *
 * Returns: the interface name the @container had been created with
 *
 * Since: 3.34.1
 **/
const gchar *
e_web_extension_container_get_interface_name (EWebExtensionContainer *container)
{
	g_return_val_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container), NULL);

	return container->priv->interface_name;
}

/**
 * e_web_extension_container_get_server_guid:
 * @container: an #EWebExtensionContainer
 *
 * Returns: (nullable): a GUID of the D-Bus server this @container created,
 *    or %NULL, when no server is running.
 *
 * Since: 3.34.1
 **/
const gchar *
e_web_extension_container_get_server_guid (EWebExtensionContainer *container)
{
	g_return_val_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container), NULL);

	if (container->priv->dbus_server)
		return g_dbus_server_get_guid (container->priv->dbus_server);

	return NULL;
}

/**
 * e_web_extension_container_get_server_address:
 * @container: an #EWebExtensionContainer
 *
 * Returns an address of the created D-Bus server. There can be used
 * e_web_extension_container_utils_connect_to_server() to connect to it.
 *
 * Returns: (nullable): an address of the D-Bus server this @container created,
 *    or %NULL, when no server is running.
 *
 * Since: 3.34.1
 **/
const gchar *
e_web_extension_container_get_server_address (EWebExtensionContainer *container)
{
	g_return_val_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container), NULL);

	if (container->priv->dbus_server)
		return g_dbus_server_get_client_address (container->priv->dbus_server);

	return NULL;
}

/**
 * e_web_extension_container_reserve_stamp:
 * @container: an #EWebExtensionContainer
 *
 * Returns: a new stamp, which is used to distinguish between instances
 *    of the Web Extensions.
 *
 * Since: 3.34.1
 **/
gint
e_web_extension_container_reserve_stamp (EWebExtensionContainer *container)
{
	gint stamp, start = 0;

	g_return_val_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container), 0);

	do {
		stamp = g_atomic_int_add (&container->priv->current_stamp, 1);
		if (!stamp) {
			g_atomic_int_add (&container->priv->current_stamp, 1);
		} else if (stamp < 0) {
			g_atomic_int_add (&container->priv->current_stamp, -stamp);

			stamp = g_atomic_int_add (&container->priv->current_stamp, 1);
			if (!stamp)
				g_atomic_int_add (&container->priv->current_stamp, 1);
		}

		if (!start) {
			start = stamp;
		} else if (stamp == start) {
			g_warn_if_reached ();
			stamp = 0;
			break;
		}
	} while (stamp <= 0 || g_hash_table_contains (container->priv->used_stamps, GINT_TO_POINTER (stamp)));

	if (stamp > 0)
		g_hash_table_insert (container->priv->used_stamps, GINT_TO_POINTER (stamp), NULL);

	return stamp;
}

/**
 * e_web_extension_container_ref_proxy:
 * @container: an #EWebExtensionContainer
 * @page_id: a page ID, as returned by webkit_web_view_get_page_id()
 * @stamp: a stamp of a Web Extension to look for
 *
 * Tries to search for a #GDBusProxy, which currently handles a Web Extension
 * with @page_id and @stamp. The owner should listen to page-proxy-changed signal,
 * to know when the proxy for the page changes.
 *
 * Free the returned proxy, if not NULL, with g_object_unref(),
 * when no longer needed.
 *
 * Returns: (transfer full) (nullable): a #GDBusProxy for @page_id and @stamp,
 *    or %NULL, when not found.
 *
 * Since: 3.34.1
 **/
GDBusProxy *
e_web_extension_container_ref_proxy (EWebExtensionContainer *container,
				     guint64 page_id,
				     gint stamp)
{
	ProxyDataIdent pdi;
	ProxyPageData *ppd;

	g_return_val_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container), NULL);

	pdi.page_id = page_id;
	pdi.stamp = stamp;

	ppd = g_hash_table_lookup (container->priv->proxy_by_page, &pdi);

	if (!ppd || !ppd->proxy)
		return NULL;

	return g_object_ref (ppd->proxy);
}

/**
 * e_web_extension_container_forget_stamp:
 * @container: an #EWebExtensionContainer
 * @stamp: a stamp of the Web Extension
 *
 * The owner of @container can call this function to free any resources
 * related to @stamp, indicating that this @stamp is no longer used.
 *
 * Since: 3.34.1
 **/
void
e_web_extension_container_forget_stamp (EWebExtensionContainer *container,
					gint stamp)
{
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container));

	g_hash_table_iter_init (&iter, container->priv->proxy_by_page);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ProxyDataIdent *pdi = key;
		ProxyPageData *ppd = value;

		if (pdi && ppd && pdi->stamp == stamp) {
			if (ppd->proxy)
				g_signal_emit (container, signals[PAGE_PROXY_CHANGED], 0, pdi->page_id, pdi->stamp, NULL);
			g_hash_table_remove (container->priv->proxy_by_page, pdi);
			break;
		}
	}

	g_hash_table_remove (container->priv->used_stamps, GINT_TO_POINTER (stamp));
}

/**
 * e_web_extension_container_call_simple:
 * @container: an #EWebExtensionContainer
 * @page_id: a page ID, as returned by webkit_web_view_get_page_id()
 * @stamp: a stamp of a Web Extension to look for
 * @method_name: a D-Bus method name
 * @params: (nullable): optional parameters for the D-Bus method
 *
 * Either calls asynchronously the D-Bus method named @method_name with
 * parameters @params immediately, when there exists a #GDBusProxy for
 * the given @page_id and @stamp, or remembers this call and calls
 * it as soon as the #GDBusProxy is created. The @method_name should
 * be part of the interface this @container had been created for.
 *
 * The D-Bus call is made asynchronously. If there are more pending calls,
 * then they are made in the order they had been added by this function.
 *
 * Since: 3.34.1
 **/
void
e_web_extension_container_call_simple (EWebExtensionContainer *container,
				       guint64 page_id,
				       gint stamp,
				       const gchar *method_name,
				       GVariant *params)
{
	ProxyDataIdent pdi;
	ProxyPageData *ppd;

	g_return_if_fail (E_IS_WEB_EXTENSION_CONTAINER (container));
	g_return_if_fail (method_name != NULL);

	pdi.page_id = page_id;
	pdi.stamp = stamp;

	ppd = g_hash_table_lookup (container->priv->proxy_by_page, &pdi);
	if (!ppd) {
		ppd = proxy_page_data_new (NULL);

		g_hash_table_insert (container->priv->proxy_by_page, proxy_data_ident_new (page_id, stamp), ppd);
	}

	if (ppd->proxy) {
		if (!g_dbus_connection_is_closed (g_dbus_proxy_get_connection (ppd->proxy))) {
			g_dbus_proxy_call (ppd->proxy, method_name, params,
				G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, ewec_simple_call_finished_cb, NULL);
		}
	} else {
		ppd->pending_calls = g_slist_prepend (ppd->pending_calls, call_simple_data_new (method_name, params));
	}
}

/**
 * e_web_extension_container_utils_connect_to_server:
 * @server_address: a D-Bus server address to connect to
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the user data to pass to @callback
 *
 * This is only a wrapper around g_dbus_connection_new_for_address(),
 * setting a #GDBusObserver, which will authorize a peer the same
 * way the preview or composer server expects it.
 *
 * Call e_web_extension_container_utils_connect_to_server_finish() to finish
 * the request from the @callback.
 *
 * Since: 3.34.1
 **/
void
e_web_extension_container_utils_connect_to_server (const gchar *server_address,
						   GCancellable *cancellable,
						   GAsyncReadyCallback callback,
						   gpointer user_data)
{
	GDBusAuthObserver *observer;

	g_return_if_fail (server_address != NULL);
	g_return_if_fail (callback != NULL);

	observer = g_dbus_auth_observer_new ();

	g_signal_connect (observer, "authorize-authenticated-peer",
		G_CALLBACK (authorize_authenticated_peer_cb), NULL);

	g_dbus_connection_new_for_address (server_address,
		G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
		observer, cancellable,
		callback, user_data);

	g_object_unref (observer);
}

/**
 * e_web_extension_container_utils_connect_to_server_finish:
 * @result: a #GAsyncResult obtained from #GAsyncReadyCallback passed
 *    to e_web_extension_container_utils_connect_to_server()
 * @error: return location for a #GError, or %NULL
 *
 * Finishes call of e_web_extension_container_utils_connect_to_server().
 *
 * Free the returned connection, if not %NULL, with g_object_unref(),
 * when no longer needed.
 *
 * Returns: (transfer full) (nullable): a newly allocated #GDBusConnection, or %NULL on error
 *
 * Since: 3.34.1
 **/
GDBusConnection *
e_web_extension_container_utils_connect_to_server_finish (GAsyncResult *result,
							  GError **error)
{
	return g_dbus_connection_new_for_address_finish (result, error);
}
