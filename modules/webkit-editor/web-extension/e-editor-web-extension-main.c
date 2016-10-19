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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <camel/camel.h>

#include "e-editor-web-extension.h"

static void
bus_acquired_cb (GDBusConnection *connection,
                 const gchar *name,
                 EEditorWebExtension *extension)
{
	e_editor_web_extension_dbus_register (extension, connection);
}

/* Forward declaration */
G_MODULE_EXPORT void webkit_web_extension_initialize_with_user_data (WebKitWebExtension *wk_extension,
								     GVariant *user_data);

G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *wk_extension,
						GVariant *user_data)
{
	EEditorWebExtension *extension;
	const gchar *service_name;

	g_return_if_fail (user_data != NULL);

	service_name = g_variant_get_string (user_data, NULL);

	camel_debug_init ();

	extension = e_editor_web_extension_get_default ();
	e_editor_web_extension_initialize (extension, wk_extension);

	g_bus_own_name (
		G_BUS_TYPE_SESSION,
		service_name,
		G_BUS_NAME_OWNER_FLAGS_NONE,
		(GBusAcquiredCallback) bus_acquired_cb,
		NULL, /* GBusNameAcquiredCallback */
		NULL, /* GBusNameLostCallback */
		g_object_ref (extension),
		(GDestroyNotify) g_object_unref);
}
