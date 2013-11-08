/*
 * evolution-web-extension.c
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

#include "evolution-web-extension.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit2/webkit-web-extension.h>

#include <libemail-engine/e-mail-enums.h>

#include <string.h>

#include "../e-util/e-dom-utils.h"

/* FIXME Clean it */
static GDBusConnection *dbus_connection;

static const char introspection_xml[] =
"<node>"
"  <interface name='org.gnome.Evolution.WebExtension'>"
"    <signal name='HeadersCollapsed'>"
"      <arg type='b' name='expanded' direction='out'/>"
"    </signal>"
"    <method name='ReplaceLocalImageLinks'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='GetDocumentContentHtml'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='html_content' direction='out'/>"
"    </method>"
"    <method name='GetSelectionContentHtml'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='html_content' direction='out'/>"
"    </method>"
"    <method name='GetSelectionContentText'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='text_content' direction='out'/>"
"    </method>"
"    <method name='CreateAndAddCSSStyleSheet'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='style_sheet_id' direction='in'/>"
"    </method>"
"    <method name='AddCSSRuleIntoStyleSheet'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='style_sheet_id' direction='in'/>"
"      <arg type='s' name='selector' direction='in'/>"
"      <arg type='s' name='style' direction='in'/>"
"    </method>"
"    <method name='EABContactFormatterBindDOM'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EMailDisplayBindDOM'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='ElementExists'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='element_exists' direction='out'/>"
"      <arg type='t' name='page_id' direction='out'/>"
"    </method>"
"    <method name='GetActiveElementName'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_name' direction='out'/>"
"    </method>"
"    <method name='EMailPartHeadersBindDOMElement'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <signal name='VCardInlineDisplayModeToggled'>"
"      <arg type='s' name='button_class' direction='out'/>"
"    </signal>"
"    <signal name='VCardInlineSaveButtonPressed'>"
"      <arg type='s' name='button_class' direction='out'/>"
"    </signal>"
"    <method name='VCardInlineBindDOM'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='VCardInlineUpdateButton'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='button_value' direction='in'/>"
"      <arg type='s' name='html_label' direction='in'/>"
"      <arg type='s' name='access_key' direction='in'/>"
"    </method>"
"    <method name='VCardInlineSetIFrameSrc'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='src' direction='in'/>"
"    </method>"
"  </interface>"
"</node>";

static gboolean
web_page_send_request (WebKitWebPage *web_page,
                       WebKitURIRequest *request,
                       WebKitURIResponse *redirected_response,
                       gpointer user_data)
{
	const char *request_uri;
	const char *page_uri;

	request_uri = webkit_uri_request_get_uri (request);
	page_uri = webkit_web_page_get_uri (web_page);

	/* Always load the main resource. */
	if (g_strcmp0 (request_uri, page_uri) == 0)
		return FALSE;

	/* Redirect http(s) request to evo-http(s) protocol. */
	if (strstr (request_uri, "http")) {
		GSettings *settings;
		EMailImageLoadingPolicy image_policy;
		gchar *new_uri;

		settings = g_settings_new ("org.gnome.evolution.mail");
		image_policy = g_settings_get_enum (settings, "image-loading-policy");
		g_object_unref (settings);

		if (image_policy == E_MAIL_IMAGE_LOADING_POLICY_ALWAYS)
			return FALSE;

		new_uri = g_strconcat ("evo-", request_uri, NULL);
		webkit_uri_request_set_uri (request, new_uri);

		g_free (new_uri);
	}

	return FALSE;
}

static void
web_page_document_loaded (WebKitWebPage *web_page,
                          gpointer user_data)
{

}

static void
web_page_created_callback (WebKitWebExtension *extension,
                           WebKitWebPage *web_page,
                           gpointer user_data)
{
	g_signal_connect_object (
		web_page, "send-request",
		G_CALLBACK (web_page_send_request),
		NULL, 0);

	g_signal_connect_object (
		web_page, "document-loaded",
		G_CALLBACK (web_page_document_loaded),
		NULL, 0);

}

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

	if (g_strcmp0 (interface_name, EVOLUTION_WEB_EXTENSION_INTERFACE) != 0)
		return;

	if (g_strcmp0 (method_name, "ReplaceLocalImageLinks") == 0) {
		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_replace_local_image_links (document);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "GetDocumentContentHtml") == 0) {
		gchar *html_content;

		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		html_content = e_dom_utils_get_document_content_html (document);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(s)", html_content));

		g_free (html_content);
	} else if (g_strcmp0 (method_name, "GetSelectionContentHtml") == 0) {
		gchar *html_content;

		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		html_content = e_dom_utils_get_selection_content_html (document);

		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(s)", html_content));

		g_free (html_content);
	} else if (g_strcmp0 (method_name, "GetSelectionContentText") == 0) {
		gchar *text_content;

		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		text_content = e_dom_utils_get_selection_content_html (document);

		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(s)", text_content));

		g_free (text_content);
	} else if (g_strcmp0 (method_name, "AddCSSRuleIntoStyleSheet") == 0) {
		const gchar *style_sheet_id, *selector, *style;

		g_variant_get (
			parameters,
			"(t&s&s&s)",
			&page_id, &style_sheet_id, &selector, &style);

		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_add_css_rule_into_style_sheet (document, style_sheet_id, selector, style);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "CreateAndAddCSSStyleSheet") == 0) {
		const gchar *style_sheet_id;

		g_variant_get (parameters, "(t&s)", &page_id, &style_sheet_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_create_and_add_css_style_sheet (document, style_sheet_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EABContactFormatterBindDOM") == 0) {
		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_eab_contact_formatter_bind_dom (document);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EMailDisplayBindDOM") == 0) {
		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_e_mail_display_bind_dom (document, connection);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementExists") == 0) {
		const gchar *element_id;
		gboolean element_exists;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element_exists = e_dom_utils_element_exists (document, element_id);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(bt)", element_exists, page_id));
	} else if (g_strcmp0 (method_name, "GetActiveElementName") == 0) {
		gchar *element_name;

		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element_name = e_dom_utils_get_active_element_name (document);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(s)", element_name));

		g_free (element_name);
	} else if (g_strcmp0 (method_name, "EMailPartHeadersBindDOMElement") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_e_mail_part_headers_bind_dom_element (document, element_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "VCardInlineBindDOM") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_module_vcard_inline_bind_dom (
			document, element_id, connection);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "VCardInlineUpdateButton") == 0) {
		const gchar *button_value, *html_label, *access_key;

		g_variant_get (
			parameters,
			"(t&s&s&s)",
			&page_id, &button_value, &html_label, &access_key);

		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_module_vcard_inline_update_button (
			document, button_value, html_label, access_key);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "VCardInlineSetIFrameSrc") == 0) {
		const gchar *src;

		g_variant_get (parameters, "(t&s)", &page_id, &src);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_module_vcard_inline_set_iframe_src (document, src);

		g_dbus_method_invocation_return_value (invocation, NULL);
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
			EVOLUTION_WEB_EXTENSION_OBJECT_PATH,
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
	g_signal_connect (
		extension, "page-created",
		G_CALLBACK (web_page_created_callback),
		NULL);

	g_bus_own_name (
		G_BUS_TYPE_SESSION,
		EVOLUTION_WEB_EXTENSION_SERVICE_NAME,
		G_BUS_NAME_OWNER_FLAGS_NONE,
		bus_acquired_cb,
		NULL, NULL,
		g_object_ref (extension),
		g_object_unref);
}
