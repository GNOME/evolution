/*
 * evolution-connman.c
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

#include <shell/e-shell.h>
#include <e-util/e-extension.h>

#define CM_DBUS_SERVICE   "net.connman"
#define CM_DBUS_INTERFACE "net.connman.Manager"
#define CM_DBUS_PATH      "/"

/* Standard GObject macros */
#define E_TYPE_CONNMAN \
	(e_connman_get_type ())
#define E_CONNMAN(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONNMAN, EConnMan))

typedef struct {
	EExtension parent;
	GDBusConnection *connection;
} EConnMan;
typedef EExtensionClass EConnManClass;

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_connman_get_type (void);
static gboolean network_manager_connect (EConnMan *extension);

G_DEFINE_DYNAMIC_TYPE (EConnMan, e_connman, E_TYPE_EXTENSION)

static void
extension_set_state (EConnMan *extension, const gchar *state)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));
	g_return_if_fail (E_IS_SHELL (extensible));

	e_shell_set_network_available (E_SHELL (extensible), !g_strcmp0 (state, "online"));
}

static void
cm_connection_closed_cb (GDBusConnection *pconnection, gboolean remote_peer_vanished, GError *error, gpointer user_data)
{
	EConnMan *extension = user_data;

	g_object_unref (extension->connection);
	extension->connection = NULL;

	g_timeout_add_seconds (
		3, (GSourceFunc) network_manager_connect, extension);
}

static void
conn_manager_signal_cb (GDBusConnection *connection,
	const gchar *sender_name,
	const gchar *object_path,
	const gchar *interface_name,
	const gchar *signal_name,
	GVariant *parameters,
	gpointer user_data)
{
	EConnMan *extension = user_data;
	gchar *state = NULL;

	if (g_strcmp0 (interface_name, CM_DBUS_INTERFACE) != 0
	    || g_strcmp0 (object_path, CM_DBUS_PATH) != 0
	    || g_strcmp0 (signal_name, "StateChanged") != 0)
		return;

	g_variant_get (parameters, "(s)", &state);
	extension_set_state (extension, state);
	g_free (state);
}

static void
connman_check_initial_state (EConnMan *extension)
{
	GDBusMessage *message = NULL;
	GDBusMessage *response = NULL;
	GError *error = NULL;

	message = g_dbus_message_new_method_call (
		CM_DBUS_SERVICE, CM_DBUS_PATH, CM_DBUS_INTERFACE, "GetState");

	/* XXX Assuming this should be safe to call synchronously. */
	response = g_dbus_connection_send_message_with_reply_sync (
		extension->connection, message, G_DBUS_SEND_MESSAGE_FLAGS_NONE, 100, NULL, NULL, &error);

	if (response != NULL && !g_dbus_message_to_gerror (response, &error)) {
		gchar *state = NULL;
		GVariant *body = g_dbus_message_get_body (response);

		g_variant_get (body, "(s)", &state);
		extension_set_state (extension, state);
		g_free (state);
	} else {
		g_warning ("%s: %s", G_STRFUNC, error ? error->message : "Unknown error");
		if (error)
			g_error_free (error);
		if (response)
			g_object_unref (response);
		g_object_unref (message);
		return;
	}

	g_object_unref (message);
	g_object_unref (response);
}

static gboolean
network_manager_connect (EConnMan *extension)
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
		CM_DBUS_SERVICE,
		CM_DBUS_INTERFACE,
		NULL,
		CM_DBUS_PATH,
		NULL,
		G_DBUS_SIGNAL_FLAGS_NONE,
		conn_manager_signal_cb,
		extension,
		NULL)) {
		g_warning ("%s: Failed to subscribe for a signal", G_STRFUNC);
		goto fail;
	}

	g_signal_connect (extension->connection, "closed", G_CALLBACK (cm_connection_closed_cb), extension);

	connman_check_initial_state (extension);

	return FALSE;

fail:
	g_object_unref (extension->connection);
	extension->connection = NULL;

	return TRUE;
}

static void
network_manager_constructed (GObject *object)
{
	network_manager_connect (E_CONNMAN (object));
}

static void
e_connman_class_init (EConnManClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = network_manager_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL;
}

static void
e_connman_class_finalize (EConnManClass *class)
{
}

static void
e_connman_init (EConnMan *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_connman_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
