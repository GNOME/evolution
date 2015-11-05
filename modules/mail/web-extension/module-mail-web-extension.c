/*
 * module-mail-web-extension.c
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

#include "module-mail-web-extension.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit2/webkit-web-extension.h>

#include <web-extensions/e-dom-utils.h>

/* FIXME Clean it */
static GDBusConnection *dbus_connection;

static const char introspection_xml[] =
"<node>"
"  <interface name='"MODULE_MAIL_WEB_EXTENSION_INTERFACE"'>"
"    <method name='GetActiveElementName'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_name' direction='out'/>"
"    </method>"
"  </interface>"
"</node>";

static WebKitWebPage *
get_webkit_web_page_or_return_dbus_error (GDBusMethodInvocation *invocation,
                                          WebKitWebExtension *web_extension,
                                          guint64 page_id)
{
	WebKitWebPage *web_page = webkit_web_extension_get_page (web_extension, page_id);
	if (!web_page) {
		g_dbus_method_invocation_return_error (
			invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"Invalid page ID: %"G_GUINT64_FORMAT, page_id);
	}
	return web_page;
}

static void
handle_method_call (GDBusConnection *connection,
                    const char *sender,
                    const char *object_path,
                    const char *interface_name,
                    const char *method_name,
                    GVariant *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer user_data)
{
	WebKitWebExtension *web_extension = WEBKIT_WEB_EXTENSION (user_data);
	WebKitWebPage *web_page;
	WebKitDOMDocument *document;
	guint64 page_id;

	if (g_strcmp0 (interface_name, MODULE_MAIL_WEB_EXTENSION_INTERFACE) != 0)
		return;

	if (g_strcmp0 (method_name, "GetActiveElementName") == 0) {
		gchar *element_name;

		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element_name = e_dom_utils_get_active_element_name (document);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					element_name ? element_name : g_strdup (""))));
	}
}

static const GDBusInterfaceVTable interface_vtable = {
	handle_method_call,
	NULL,
	NULL
};

static void
bus_acquired_cb (GDBusConnection *connection,
                 const char *name,
                 gpointer user_data)
{
	guint registration_id;
	GError *error = NULL;
	static GDBusNodeInfo *introspection_data = NULL;

	if (!introspection_data)
		introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

	registration_id =
		g_dbus_connection_register_object (
			connection,
			MODULE_MAIL_WEB_EXTENSION_OBJECT_PATH,
			introspection_data->interfaces[0],
			&interface_vtable,
			g_object_ref (user_data),
			g_object_unref,
			&error);

	if (!registration_id) {
		g_warning ("Failed to register object: %s\n", error->message);
		g_error_free (error);
	} else {
		dbus_connection = connection;
		g_object_add_weak_pointer (G_OBJECT (connection), (gpointer *)&dbus_connection);
	}
}

/* Forward declaration */
G_MODULE_EXPORT void webkit_web_extension_initialize (WebKitWebExtension *extension);

G_MODULE_EXPORT void
webkit_web_extension_initialize (WebKitWebExtension *extension)
{
	g_bus_own_name (
		G_BUS_TYPE_SESSION,
		MODULE_MAIL_WEB_EXTENSION_SERVICE_NAME,
		G_BUS_NAME_OWNER_FLAGS_NONE,
		bus_acquired_cb,
		NULL, NULL,
		g_object_ref (extension),
		g_object_unref);
}
