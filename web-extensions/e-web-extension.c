/*
 * e-web-extension.c
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

#include <string.h>

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include "e-web-extension.h"
#include "e-dom-utils.h"
#include "e-web-extension-names.h"

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

#define E_WEB_EXTENSION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEB_EXTENSION, EWebExtensionPrivate))

struct _EWebExtensionPrivate {
	WebKitWebExtension *wk_extension;

	GDBusConnection *dbus_connection;
	guint registration_id;

	gboolean initialized;

	gboolean need_input;
};

static const char introspection_xml[] =
"<node>"
"  <interface name='" E_WEB_EXTENSION_INTERFACE "'>"
"    <signal name='HeadersCollapsed'>"
"      <arg type='b' name='expanded' direction='out'/>"
"    </signal>"
"    <method name='ReplaceLocalImageLinks'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DocumentHasSelection'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='has_selection' direction='out'/>"
"    </method>"
"    <method name='GetDocumentContentHTML'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='html_content' direction='out'/>"
"    </method>"
"    <method name='GetSelectionContentHTML'>"
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
"      <arg type='s' name='button_id' direction='out'/>"
"    </signal>"
"    <signal name='VCardInlineSaveButtonPressed'>"
"      <arg type='s' name='button_value' direction='out'/>"
"    </signal>"
"    <method name='VCardInlineBindDOM'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='VCardInlineUpdateButton'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='button_id' direction='in'/>"
"      <arg type='s' name='html_label' direction='in'/>"
"      <arg type='s' name='access_key' direction='in'/>"
"    </method>"
"    <method name='VCardInlineSetIFrameSrc'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='button_id' direction='in'/>"
"      <arg type='s' name='src' direction='in'/>"
"    </method>"
"    <method name='GetDocumentURIFromPoint'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='x' direction='in'/>"
"      <arg type='i' name='y' direction='in'/>"
"      <arg type='s' name='document_uri' direction='out'/>"
"    </method>"
"    <method name='SetDocumentIFrameSrc'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='document_uri' direction='in'/>"
"      <arg type='s' name='new_iframe_src' direction='in'/>"
"    </method>"
"    <property type='b' name='NeedInput' access='readwrite'/>"
"  </interface>"
"</node>";

G_DEFINE_TYPE (EWebExtension, e_web_extension, G_TYPE_OBJECT)

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
	guint64 page_id;
        EWebExtension *extension = E_WEB_EXTENSION (user_data);
	WebKitDOMDocument *document;
	WebKitWebExtension *web_extension = extension->priv->wk_extension;
	WebKitWebPage *web_page;

	if (g_strcmp0 (interface_name, E_WEB_EXTENSION_INTERFACE) != 0)
		return;

	if (camel_debug ("wex"))
		printf ("EWebExtension - %s - %s\n", G_STRFUNC, method_name);
	if (g_strcmp0 (method_name, "ReplaceLocalImageLinks") == 0) {
		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_replace_local_image_links (document);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DocumentHasSelection") == 0) {
		gboolean has_selection;

		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		has_selection = e_dom_utils_document_has_selection (document);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", has_selection));
	} else if (g_strcmp0 (method_name, "GetDocumentContentHTML") == 0) {
		gchar *html_content;

		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		html_content = e_dom_utils_get_document_content_html (document);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					html_content ? html_content : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "GetSelectionContentHTML") == 0) {
		gchar *html_content;

		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		html_content = e_dom_utils_get_selection_content_html (document);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					html_content ? html_content : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "GetSelectionContentText") == 0) {
		gchar *text_content;

		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		text_content = e_dom_utils_get_selection_content_text (document);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_take_string (text_content));
	} else if (g_strcmp0 (method_name, "AddCSSRuleIntoStyleSheet") == 0) {
		const gchar *style_sheet_id, *selector, *style;

		g_variant_get (
			parameters,
			"(t&s&s&s)",
			&page_id, &style_sheet_id, &selector, &style);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_add_css_rule_into_style_sheet (document, style_sheet_id, selector, style);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "CreateAndAddCSSStyleSheet") == 0) {
		const gchar *style_sheet_id;

		g_variant_get (parameters, "(t&s)", &page_id, &style_sheet_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_create_and_add_css_style_sheet (document, style_sheet_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EABContactFormatterBindDOM") == 0) {
		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_eab_contact_formatter_bind_dom (document);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EMailDisplayBindDOM") == 0) {
		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_e_mail_display_bind_dom (document, connection);
		e_dom_utils_bind_focus_on_elements (document, connection);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementExists") == 0) {
		const gchar *element_id;
		gboolean element_exists;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element_exists = e_dom_utils_element_exists (document, element_id);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(bt)", element_exists, page_id));
	} else if (g_strcmp0 (method_name, "GetActiveElementName") == 0) {
		gchar *element_name;

		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
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
	} else if (g_strcmp0 (method_name, "EMailPartHeadersBindDOMElement") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_e_mail_part_headers_bind_dom_element (document, element_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "VCardInlineBindDOM") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_module_vcard_inline_bind_dom (
			document, element_id, connection);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "VCardInlineUpdateButton") == 0) {
		const gchar *button_id, *html_label, *access_key;

		g_variant_get (
			parameters,
			"(t&s&s&s)",
			&page_id, &button_id, &html_label, &access_key);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_module_vcard_inline_update_button (
			document, button_id, html_label, access_key);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "VCardInlineSetIFrameSrc") == 0) {
		const gchar *src, *button_id;

		g_variant_get (parameters, "(t&s&s)", &page_id, &button_id, &src);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_dom_utils_module_vcard_inline_set_iframe_src (document, button_id, src);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "GetDocumentURIFromPoint") == 0) {
		WebKitDOMDocument *document_at_point;
		gchar *document_uri = NULL;
		gint32 xx = 0, yy = 0;

		g_variant_get (parameters, "(tii)", &page_id, &xx, &yy);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		document_at_point = e_dom_utils_get_document_from_point (document, xx, yy);

		if (document_at_point)
			document_uri = webkit_dom_document_get_document_uri (document_at_point);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new ("(@s)", g_variant_new_take_string (document_uri ? document_uri : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "SetDocumentIFrameSrc") == 0) {
		const gchar *document_uri = NULL, *new_iframe_src = NULL;
		WebKitDOMDocument *iframe_document;

		g_variant_get (parameters, "(t&s&s)", &page_id, &document_uri, &new_iframe_src);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		iframe_document = e_dom_utils_find_document_with_uri (document, document_uri);

		if (iframe_document) {
			WebKitDOMDOMWindow *window;
			WebKitDOMElement *frame_element;

			/* Get frame's window and from the window the actual <iframe> element */
			window = webkit_dom_document_get_default_view (iframe_document);
			frame_element = webkit_dom_dom_window_get_frame_element (window);
			webkit_dom_html_iframe_element_set_src (
				WEBKIT_DOM_HTML_IFRAME_ELEMENT (frame_element), new_iframe_src);
		}

		g_dbus_method_invocation_return_value (invocation, NULL);
	}
}

static GVariant *
handle_get_property (GDBusConnection *connection,
                     const gchar *sender,
                     const gchar *object_path,
                     const gchar *interface_name,
                     const gchar *property_name,
                     GError **error,
                     gpointer user_data)
{
	EWebExtension *extension = E_WEB_EXTENSION (user_data);
	GVariant *variant;

	if (g_strcmp0 (property_name, "NeedInput") == 0) {
		variant = g_variant_new_boolean (extension->priv->need_input);
	}

	return variant;
}

static gboolean
handle_set_property (GDBusConnection *connection,
                     const gchar *sender,
                     const gchar *object_path,
                     const gchar *interface_name,
                     const gchar *property_name,
                     GVariant *variant,
                     GError **error,
                     gpointer user_data)
{
	EWebExtension *extension = E_WEB_EXTENSION (user_data);
	GError *local_error = NULL;
	GVariantBuilder *builder;

	builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);

	if (g_strcmp0 (property_name, "NeedInput") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->need_input)
			goto exit;

		extension->priv->need_input = value;

		g_variant_builder_add (builder,
			"{sv}",
			"NeedInput",
			g_variant_new_boolean (value));
	}

	g_dbus_connection_emit_signal (connection,
		NULL,
		object_path,
		"org.freedesktop.DBus.Properties",
		"PropertiesChanged",
		g_variant_new (
			"(sa{sv}as)",
			interface_name,
			builder,
			NULL),
		&local_error);

	g_assert_no_error (local_error);

 exit:
	g_variant_builder_unref (builder);

	return TRUE;
}

static const GDBusInterfaceVTable interface_vtable = {
	handle_method_call,
	handle_get_property,
	handle_set_property
};

static void
e_web_extension_dispose (GObject *object)
{
	EWebExtension *extension = E_WEB_EXTENSION (object);

	if (extension->priv->dbus_connection) {
		g_dbus_connection_unregister_object (
			extension->priv->dbus_connection,
			extension->priv->registration_id);
		extension->priv->registration_id = 0;
		extension->priv->dbus_connection = NULL;
	}

	g_clear_object (&extension->priv->wk_extension);

	G_OBJECT_CLASS (e_web_extension_parent_class)->dispose (object);
}

static void
e_web_extension_class_init (EWebExtensionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = e_web_extension_dispose;

	g_type_class_add_private (object_class, sizeof(EWebExtensionPrivate));
}

static void
e_web_extension_init (EWebExtension *extension)
{
	extension->priv = G_TYPE_INSTANCE_GET_PRIVATE (extension, E_TYPE_WEB_EXTENSION, EWebExtensionPrivate);

	extension->priv->initialized = FALSE;
	extension->priv->need_input = FALSE;
}

static gpointer
e_web_extension_create_instance(gpointer data)
{
	return g_object_new (E_TYPE_WEB_EXTENSION, NULL);
}

EWebExtension *
e_web_extension_get (void)
{
	static GOnce once_init = G_ONCE_INIT;
	return E_WEB_EXTENSION (g_once (&once_init, e_web_extension_create_instance, NULL));
}

static gboolean
web_page_send_request_cb (WebKitWebPage *web_page,
                          WebKitURIRequest *request,
                          WebKitURIResponse *redirected_response,
                          EWebExtension *extension)
{
	const gchar *request_uri;
	const gchar *page_uri;

	request_uri = webkit_uri_request_get_uri (request);
	page_uri = webkit_web_page_get_uri (web_page);

	/* Always load the main resource. */
	if (g_strcmp0 (request_uri, page_uri) == 0 ||
	    /* Do not influence real pages, like those with eds OAuth sign-in */
	    g_str_has_prefix (page_uri, "http:") ||
	    g_str_has_prefix (page_uri, "https:"))
		return FALSE;

	if (g_str_has_prefix (request_uri, "http:") ||
	    g_str_has_prefix (request_uri, "https:")) {
		gchar *new_uri;

		new_uri = g_strconcat ("evo-", request_uri, NULL);

		webkit_uri_request_set_uri (request, new_uri);

		g_free (new_uri);
	}

	return FALSE;
}

static void
web_page_document_loaded_cb (WebKitWebPage *web_page,
                             gpointer user_data)
{
}

static void
web_page_created_cb (WebKitWebExtension *wk_extension,
                     WebKitWebPage *web_page,
                     EWebExtension *extension)
{
	g_signal_connect_object (
		web_page, "send-request",
		G_CALLBACK (web_page_send_request_cb),
		extension, 0);

	g_signal_connect_object (
		web_page, "document-loaded",
		G_CALLBACK (web_page_document_loaded_cb),
		extension, 0);

}

void
e_web_extension_initialize (EWebExtension *extension,
                            WebKitWebExtension *wk_extension)
{
	g_return_if_fail (E_IS_WEB_EXTENSION (extension));

	if (extension->priv->initialized)
		return;

	extension->priv->initialized = TRUE;

	extension->priv->wk_extension = g_object_ref (wk_extension);

	g_signal_connect (
		wk_extension, "page-created",
		G_CALLBACK (web_page_created_cb), extension);
}

void
e_web_extension_dbus_register (EWebExtension *extension,
                               GDBusConnection *connection)
{
	GError *error = NULL;
	static GDBusNodeInfo *introspection_data = NULL;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));
	g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

	if (!introspection_data) {
		introspection_data =
			g_dbus_node_info_new_for_xml (introspection_xml, NULL);

		extension->priv->registration_id =
			g_dbus_connection_register_object (
				connection,
				E_WEB_EXTENSION_OBJECT_PATH,
				introspection_data->interfaces[0],
				&interface_vtable,
				extension,
				NULL,
				&error);

		if (!extension->priv->registration_id) {
			g_warning ("Failed to register object: %s\n", error->message);
			g_error_free (error);
		} else {
			extension->priv->dbus_connection = connection;
			g_object_add_weak_pointer (
				G_OBJECT (connection),
				(gpointer *)&extension->priv->dbus_connection);
		}
	}
}
