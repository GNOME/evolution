/*
 * e-html-editor-web-extension-main.c
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

#include "evolution-config.h"

#include <camel/camel.h>

#define E_UTIL_INCLUDE_WITHOUT_WEBKIT
#include <e-util/e-util.h>
#undef E_UTIL_INCLUDE_WITHOUT_WEBKIT

#include "e-editor-web-extension.h"

static void
connected_to_server_cb (GObject *source_object,
			GAsyncResult *result,
			gpointer user_data)
{
	EEditorWebExtension *extension = user_data;
	GDBusConnection *connection;
	GError *error = NULL;

	g_return_if_fail (E_IS_EDITOR_WEB_EXTENSION (extension));

	connection = e_web_extension_container_utils_connect_to_server_finish (result, &error);
	if (!connection) {
		g_warning ("%d %s: Failed to connect to the UI D-Bus server: %s", getpid (), G_STRFUNC,
			error ? error->message : "Unknown error");
		g_clear_error (&error);
		return;
	}

	e_editor_web_extension_dbus_register (extension, connection);

	g_object_unref (connection);
	g_object_unref (extension);
}

/* Forward declaration */
G_MODULE_EXPORT void webkit_web_extension_initialize_with_user_data (WebKitWebExtension *wk_extension,
								     GVariant *user_data);

G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *wk_extension,
						GVariant *user_data)
{
	EEditorWebExtension *extension;
	const gchar *guid = NULL, *server_address = NULL;

	g_return_if_fail (user_data != NULL);

	g_variant_get (user_data, "(&s&s)", &guid, &server_address);

	if (!server_address) {
		g_warning ("%d %s: The UI process didn't provide server address", getpid (), G_STRFUNC);
		return;
	}

	camel_debug_init ();

	extension = e_editor_web_extension_get_default ();
	e_editor_web_extension_initialize (extension, wk_extension);

	e_web_extension_container_utils_connect_to_server (server_address, NULL, connected_to_server_cb, g_object_ref (extension));
}
