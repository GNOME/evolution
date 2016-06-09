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

#include "e-html-editor-web-extension.h"

static void
bus_acquired_cb (GDBusConnection *connection,
                 const gchar *name,
                 EHTMLEditorWebExtension *extension)
{
	e_html_editor_web_extension_dbus_register (extension, connection);
}

/* Forward declaration */
G_MODULE_EXPORT void webkit_web_extension_initialize (WebKitWebExtension *wk_extension);

G_MODULE_EXPORT void
webkit_web_extension_initialize (WebKitWebExtension *wk_extension)
{
	EHTMLEditorWebExtension *extension;

	camel_debug_init ();

	extension = e_html_editor_web_extension_get ();
	e_html_editor_web_extension_initialize (extension, wk_extension);

	g_bus_own_name (
		G_BUS_TYPE_SESSION,
		E_HTML_EDITOR_WEB_EXTENSION_SERVICE_NAME,
		G_BUS_NAME_OWNER_FLAGS_NONE,
		(GBusAcquiredCallback) bus_acquired_cb,
		NULL, /* GBusNameAcquiredCallback */
		NULL, /* GBusNameLostCallback */
		g_object_ref (extension),
		(GDestroyNotify) g_object_unref);
}
