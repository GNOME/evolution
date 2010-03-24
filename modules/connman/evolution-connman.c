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

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <shell/e-shell.h>
#include <e-util/e-extension.h>

#define CM_DBUS_SERVICE   "org.moblin.connman"
#define CM_DBUS_INTERFACE "org.moblin.connman.Manager"
#define CM_DBUS_PATH      "/"

/* Standard GObject macros */
#define E_TYPE_CONNMAN \
	(e_connman_get_type ())
#define E_CONNMAN(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONNMAN, EConnMan))

typedef struct {
	EExtension parent;
	DBusConnection *connection;
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
extension_set_state (EConnMan *extension, const char *state)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));
	g_return_if_fail (E_IS_SHELL (extensible));

	e_shell_set_network_available (E_SHELL (extensible), !g_strcmp0 (state, "online"));
}

static DBusHandlerResult
connman_monitor (DBusConnection *connection G_GNUC_UNUSED,
		 DBusMessage *message,
		 gpointer user_data)
{
	char *value;
	EConnMan *extension = user_data;
	DBusError error = DBUS_ERROR_INIT;
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_get_args (message, &error,
				    DBUS_TYPE_STRING, &value,
				    DBUS_TYPE_INVALID))
		goto err_exit;

	extension_set_state (extension, value);
	ret = DBUS_HANDLER_RESULT_HANDLED;

    err_exit:
	return ret;
}

static void
connman_check_initial_state (EConnMan *extension)
{
	DBusMessage *message = NULL;
	DBusMessage *response = NULL;
	DBusError error = DBUS_ERROR_INIT;

	message = dbus_message_new_method_call (
		CM_DBUS_SERVICE, CM_DBUS_PATH, CM_DBUS_INTERFACE, "GetState");

	/* XXX Assuming this should be safe to call synchronously. */
	response = dbus_connection_send_with_reply_and_block (
		extension->connection, message, 100, &error);

	if (response != NULL) {
		const char *value;
		if (dbus_message_get_args (message, &error,
					   DBUS_TYPE_STRING, &value,
					   DBUS_TYPE_INVALID))
			extension_set_state (extension, value);
	} else {
		g_warning ("%s", error.message);
		dbus_error_free (&error);
		return;
	}

	dbus_message_unref (message);
	dbus_message_unref (response);
}

static gboolean
network_manager_connect (EConnMan *extension)
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
		extension->connection, connman_monitor, extension, NULL))
		goto fail;

	dbus_bus_add_match (
		extension->connection,
		"type='signal',"
		"interface='" CM_DBUS_INTERFACE "',"
		"sender='" CM_DBUS_SERVICE "',"
		"member='StateChanged',"
		"path='" CM_DBUS_PATH "'",
		&error);
	if (dbus_error_is_set (&error)) {
		g_warning ("%s", error.message);
		dbus_error_free (&error);
		goto fail;
	}

	connman_check_initial_state (extension);

	return FALSE;

fail:
	dbus_connection_unref (extension->connection);
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
