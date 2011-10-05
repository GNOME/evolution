/*
 * evolution-network-manager.c
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <libebackend/e-extension.h>
#include <NetworkManager/NetworkManager.h>

#if !defined(NM_CHECK_VERSION)
#define NM_CHECK_VERSION(x,y,z) 0
#endif

#include <shell/e-shell.h>

/* Standard GObject macros */
#define E_TYPE_NETWORK_MANAGER \
	(e_network_manager_get_type ())
#define E_NETWORK_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_NETWORK_MANAGER, ENetworkManager))

typedef struct _ENetworkManager ENetworkManager;
typedef struct _ENetworkManagerClass ENetworkManagerClass;

struct _ENetworkManager {
	EExtension parent;
	GDBusConnection *connection;
};

struct _ENetworkManagerClass {
	EExtensionClass parent_class;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_network_manager_get_type (void);
static gboolean network_manager_connect (ENetworkManager *extension);

G_DEFINE_DYNAMIC_TYPE (ENetworkManager, e_network_manager, E_TYPE_EXTENSION)

static EShell *
network_manager_get_shell (ENetworkManager *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_SHELL (extensible);
}

static void
nm_connection_closed_cb (GDBusConnection *connection,
                         gboolean remote_peer_vanished,
                         GError *error,
                         ENetworkManager *extension)
{
	gboolean try_again;

	g_object_unref (extension->connection);
	extension->connection = NULL;

	/* Try connecting to the session bus immediately, and then
	 * keep trying at 3 second intervals until we're back on. */

	try_again = network_manager_connect (extension);

	if (try_again)
		g_timeout_add_seconds (
			3, (GSourceFunc) network_manager_connect, extension);
}

static void
network_manager_handle_state (EShell *shell,
                              guint32 state)
{
	switch (state) {
#if NM_CHECK_VERSION(0,8,992)
		case NM_STATE_CONNECTED_LOCAL:
		case NM_STATE_CONNECTED_SITE:
		case NM_STATE_CONNECTED_GLOBAL:
#else
		case NM_STATE_CONNECTED:
#endif
			e_shell_set_network_available (shell, TRUE);
			break;
		case NM_STATE_ASLEEP:
		case NM_STATE_DISCONNECTED:
#if NM_CHECK_VERSION(0,8,992)
		case NM_STATE_DISCONNECTING:
#endif
			e_shell_set_network_available (shell, FALSE);
			break;
		default:
			break;
	}
}

static void
network_manager_signal_cb (GDBusConnection *connection,
                           const gchar *sender_name,
                           const gchar *object_path,
                           const gchar *interface_name,
                           const gchar *signal_name,
                           GVariant *parameters,
                           gpointer user_data)
{
	ENetworkManager *extension = user_data;
	EShell *shell;
	guint32 state;

	shell = network_manager_get_shell (extension);

	if (g_strcmp0 (interface_name, NM_DBUS_INTERFACE) != 0
	    || g_strcmp0 (signal_name, "StateChanged") != 0)
		return;

	g_variant_get (parameters, "(u)", &state);
	network_manager_handle_state (shell, state);
}

static void
network_manager_query_state (ENetworkManager *extension)
{
	EShell *shell;
	GDBusMessage *message = NULL;
	GDBusMessage *response = NULL;
	GVariant *body;
	guint32 state = -1;
	GError *error = NULL;

	shell = network_manager_get_shell (extension);

	message = g_dbus_message_new_method_call (
		NM_DBUS_SERVICE, NM_DBUS_PATH, NM_DBUS_INTERFACE, "state");

	/* XXX Assuming this should be safe to call synchronously. */
	response = g_dbus_connection_send_message_with_reply_sync (
		extension->connection, message,
		G_DBUS_SEND_MESSAGE_FLAGS_NONE, 100, NULL, NULL, &error);

	if (response != NULL) {
		if (g_dbus_message_to_gerror (response, &error)) {
			g_object_unref (response);
			response = NULL;
		}
	}

	g_object_unref (message);

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		return;
	}

	g_return_if_fail (G_IS_DBUS_MESSAGE (response));

	body = g_dbus_message_get_body (response);
	g_variant_get (body, "(u)", &state);
	network_manager_handle_state (shell, state);

	g_object_unref (response);
}

static gboolean
network_manager_connect (ENetworkManager *extension)
{
	GError *error = NULL;

	/* This is a timeout callback, so the return value denotes
	 * whether to reschedule, not whether we're successful. */

	if (extension->connection != NULL)
		return FALSE;

	extension->connection =
		g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		return TRUE;
	}

	g_return_val_if_fail (
		G_IS_DBUS_CONNECTION (extension->connection), FALSE);

	g_dbus_connection_set_exit_on_close (extension->connection, FALSE);

	if (!g_dbus_connection_signal_subscribe (
		extension->connection,
		NM_DBUS_SERVICE,
		NM_DBUS_INTERFACE,
		NULL,
		NM_DBUS_PATH,
		NULL,
		G_DBUS_SIGNAL_FLAGS_NONE,
		network_manager_signal_cb,
		extension,
		NULL)) {
		g_warning ("%s: Failed to subscribe for a signal", G_STRFUNC);
		goto fail;
	}

	g_signal_connect (
		extension->connection, "closed",
		G_CALLBACK (nm_connection_closed_cb), extension);

	network_manager_query_state (extension);

	return FALSE;

fail:
	g_object_unref (extension->connection);
	extension->connection = NULL;

	return TRUE;
}

static void
network_manager_constructed (GObject *object)
{
	network_manager_connect (E_NETWORK_MANAGER (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_network_manager_parent_class)->constructed (object);
}

static void
e_network_manager_class_init (ENetworkManagerClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = network_manager_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL;
}

static void
e_network_manager_class_finalize (ENetworkManagerClass *class)
{
}

static void
e_network_manager_init (ENetworkManager *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_network_manager_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
