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

#include <gio/gio.h>
#include <NetworkManager/NetworkManager.h>

#include <shell/e-shell.h>
#include <e-util/e-extension.h>

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
nm_connection_closed_cb (GDBusConnection *pconnection,
                         gboolean remote_peer_vanished,
                         GError *error,
                         ENetworkManager *extension)
{
	g_object_unref (extension->connection);
	extension->connection = NULL;

	g_timeout_add_seconds (
		3, (GSourceFunc) network_manager_connect, extension);
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
	switch (state) {
		case NM_STATE_CONNECTED:
			e_shell_set_network_available (shell, TRUE);
			break;
		case NM_STATE_ASLEEP:
		case NM_STATE_DISCONNECTED:
			e_shell_set_network_available (shell, FALSE);
			break;
		default:
			break;
	}
}

static void
network_manager_check_initial_state (ENetworkManager *extension)
{
	EShell *shell;
	GDBusMessage *message = NULL;
	GDBusMessage *response = NULL;
	guint32 state = -1;
	GError *error = NULL;

	shell = network_manager_get_shell (extension);

	message = g_dbus_message_new_method_call (
		NM_DBUS_SERVICE, NM_DBUS_PATH, NM_DBUS_INTERFACE, "state");

	/* XXX Assuming this should be safe to call synchronously. */
	response = g_dbus_connection_send_message_with_reply_sync (
		extension->connection, message,
		G_DBUS_SEND_MESSAGE_FLAGS_NONE, 100, NULL, NULL, &error);

	if (response != NULL && !g_dbus_message_to_gerror (response, &error)) {
		GVariant *body = g_dbus_message_get_body (response);

		g_variant_get (body, "(u)", &state);
	} else {
		g_warning ("%s: %s", G_STRFUNC, error ? error->message : "Unknown error");
		if (error)
			g_error_free (error);
		if (response)
			g_object_unref (response);
		g_object_unref (message);
		return;
	}

	/* Update the state only in the absence of a network connection,
	 * otherwise let the old state prevail. */
	if (state == NM_STATE_ASLEEP || state == NM_STATE_DISCONNECTED)
		e_shell_set_network_available (shell, FALSE);

	g_object_unref (message);
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

	extension->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
	if (extension->connection == NULL) {
		g_warning ("%s: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_error_free (error);

		return TRUE;
	}

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

	network_manager_check_initial_state (extension);

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
