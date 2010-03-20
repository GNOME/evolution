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

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
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
	DBusConnection *connection;
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

static DBusHandlerResult
network_manager_monitor (DBusConnection *connection G_GNUC_UNUSED,
                         DBusMessage *message,
                         gpointer user_data)
{
	ENetworkManager *extension = user_data;
	EShell *shell;
	const gchar *path;
	guint32 state;
	DBusError error = DBUS_ERROR_INIT;

	shell = network_manager_get_shell (extension);

	path = dbus_message_get_path (message);

	if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected") &&
		g_strcmp0 (path, DBUS_PATH_LOCAL) == 0) {
		dbus_connection_unref (extension->connection);
		extension->connection = NULL;

		g_timeout_add_seconds (
			3, (GSourceFunc) network_manager_connect, extension);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!dbus_message_is_signal (message, NM_DBUS_INTERFACE, "StateChanged"))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_get_args (
		message, &error,
		DBUS_TYPE_UINT32, &state,
		DBUS_TYPE_INVALID);

	if (dbus_error_is_set (&error)) {
		g_warning ("%s", error.message);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

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

	return DBUS_HANDLER_RESULT_HANDLED;
}

static void
network_manager_check_initial_state (ENetworkManager *extension)
{
	EShell *shell;
	DBusMessage *message = NULL;
	DBusMessage *response = NULL;
	guint32 state = -1;
	DBusError error = DBUS_ERROR_INIT;

	shell = network_manager_get_shell (extension);

	message = dbus_message_new_method_call (
		NM_DBUS_SERVICE, NM_DBUS_PATH, NM_DBUS_INTERFACE, "state");

	/* XXX Assuming this should be safe to call synchronously. */
	response = dbus_connection_send_with_reply_and_block (
		extension->connection, message, 100, &error);

	if (response != NULL) {
		dbus_message_get_args (
			response, &error, DBUS_TYPE_UINT32,
			&state, DBUS_TYPE_INVALID);
	} else {
		g_warning ("%s", error.message);
		dbus_error_free (&error);
		return;
	}

	/* Update the state only in the absence of a network connection,
	 * otherwise let the old state prevail. */
	if (state == NM_STATE_ASLEEP || state == NM_STATE_DISCONNECTED)
		e_shell_set_network_available (shell, FALSE);

	dbus_message_unref (message);
	dbus_message_unref (response);
}

static gboolean
network_manager_connect (ENetworkManager *extension)
{
	DBusError error = DBUS_ERROR_INIT;

	/* This is a timeout callback, so the return value denotes
	 * whether to reschedule, not whether we're successful. */

	if (extension->connection != NULL)
		return FALSE;

	extension->connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (extension->connection == NULL) {
		g_warning ("%s", error.message);
		dbus_error_free (&error);
		return TRUE;
	}

	dbus_connection_setup_with_g_main (extension->connection, NULL);
	dbus_connection_set_exit_on_disconnect (extension->connection, FALSE);

	if (!dbus_connection_add_filter (
		extension->connection,
		network_manager_monitor, extension, NULL))
		goto fail;

	network_manager_check_initial_state (extension);

	dbus_bus_add_match (
		extension->connection,
		"type='signal',"
		"interface='" NM_DBUS_INTERFACE "',"
		"sender='" NM_DBUS_SERVICE "',"
		"path='" NM_DBUS_PATH "'",
		&error);
	if (dbus_error_is_set (&error)) {
		g_warning ("%s", error.message);
		dbus_error_free (&error);
		goto fail;
	}

	return FALSE;

fail:
	dbus_connection_unref (extension->connection);
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
