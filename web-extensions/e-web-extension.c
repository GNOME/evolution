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

#define WEB_EXTENSION_PAGE_ID_KEY "web-extension-page-id"

#define E_WEB_EXTENSION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEB_EXTENSION, EWebExtensionPrivate))

struct _EWebExtensionPrivate {
	WebKitWebExtension *wk_extension;

	GDBusConnection *dbus_connection;
	guint registration_id;

	gboolean initialized;

	gboolean need_input;
	guint32 clipboard_flags;
};

static const char introspection_xml[] =
"<node>"
"  <interface name='" E_WEB_EXTENSION_INTERFACE "'>"
"    <method name='RegisterElementClicked'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_class' direction='in'/>"
"    </method>"
"    <signal name='ElementClicked'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='s' name='element_class' direction='out'/>"
"      <arg type='s' name='element_value' direction='out'/>"
"      <arg type='i' name='position_left' direction='out'/>"
"      <arg type='i' name='position_top' direction='out'/>"
"      <arg type='i' name='position_width' direction='out'/>"
"      <arg type='i' name='position_height' direction='out'/>"
"    </signal>"
"    <method name='SetElementHidden'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='hidden' direction='in'/>"
"    </method>"
"    <method name='SetElementStyleProperty'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='property_name' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='s' name='priority' direction='in'/>"
"    </method>"
"    <method name='SetElementAttribute'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='namespace_uri' direction='in'/>"
"      <arg type='s' name='qualified_name' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <signal name='HeadersCollapsed'>"
"      <arg type='b' name='expanded' direction='out'/>"
"    </signal>"
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
"    <method name='GetSelectionContentMultipart'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='content' direction='out'/>"
"      <arg type='b' name='is_html' direction='out'/>"
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
"    <method name='ProcessMagicSpacebar'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='towards_bottom' direction='in'/>"
"      <arg type='b' name='processed' direction='out'/>"
"    </method>"
"    <property type='b' name='NeedInput' access='readwrite'/>"
"    <property type='u' name='ClipboardFlags' access='readwrite'/>"
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
			"Invalid page ID: %" G_GUINT64_FORMAT, page_id);
	}
	return web_page;
}

static void
element_clicked_cb (WebKitDOMElement *element,
		    WebKitDOMEvent *event,
		    gpointer user_data)
{
	EWebExtension *extension = user_data;
	WebKitDOMElement *offset_parent;
	WebKitDOMDOMWindow *dom_window = NULL;
	gchar *attr_class, *attr_value;
	const guint64 *ppage_id;
	gdouble with_parents_left, with_parents_top;
	glong scroll_x = 0, scroll_y = 0;
	GError *error = NULL;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));
	g_return_if_fail (G_IS_OBJECT (element));

	ppage_id = g_object_get_data (G_OBJECT (element), WEB_EXTENSION_PAGE_ID_KEY);
	g_return_if_fail (ppage_id != NULL);

	with_parents_left = webkit_dom_element_get_offset_left (element);
	with_parents_top = webkit_dom_element_get_offset_top (element);

	offset_parent = element;
	while (offset_parent = webkit_dom_element_get_offset_parent (offset_parent), offset_parent) {
		with_parents_left += webkit_dom_element_get_offset_left (offset_parent);
		with_parents_top += webkit_dom_element_get_offset_top (offset_parent);
	}

	dom_window = webkit_dom_document_get_default_view (webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element)));
	if (WEBKIT_DOM_IS_DOM_WINDOW (dom_window)) {
		g_object_get (G_OBJECT (dom_window),
			"scroll-x", &scroll_x,
			"scroll-y", &scroll_y,
			NULL);
	}
	g_clear_object (&dom_window);

	attr_class = webkit_dom_element_get_class_name (element);
	attr_value = webkit_dom_element_get_attribute (element, "value");

	g_dbus_connection_emit_signal (
		extension->priv->dbus_connection,
		NULL,
		E_WEB_EXTENSION_OBJECT_PATH,
		E_WEB_EXTENSION_INTERFACE,
		"ElementClicked",
		g_variant_new ("(tssiiii)", *ppage_id, attr_class ? attr_class : "", attr_value ? attr_value : "",
			(gint) (with_parents_left - scroll_x),
			(gint) (with_parents_top - scroll_y),
			(gint) webkit_dom_element_get_offset_width (element),
			(gint) webkit_dom_element_get_offset_height (element)),
		&error);

	if (error) {
		g_warning ("Error emitting signal ElementClicked: %s\n", error->message);
		g_error_free (error);
	}

	g_free (attr_class);
	g_free (attr_value);
}

static void
web_extension_register_element_clicked_in_document (EWebExtension *extension,
						    guint64 page_id,
						    WebKitDOMDocument *document,
						    const gchar *element_class)
{
	WebKitDOMHTMLCollection *collection = NULL;
	gulong ii, len;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));
	g_return_if_fail (WEBKIT_DOM_IS_DOCUMENT (document));
	g_return_if_fail (element_class && *element_class);

	collection = webkit_dom_document_get_elements_by_class_name_as_html_collection (document, element_class);
	if (collection) {
		len = webkit_dom_html_collection_get_length (collection);
		for (ii = 0; ii < len; ii++) {
			WebKitDOMNode *node;

			node = webkit_dom_html_collection_item (collection, ii);
			if (WEBKIT_DOM_IS_EVENT_TARGET (node)) {
				guint64 *ppage_id;

				ppage_id = g_new0 (guint64, 1);
				*ppage_id = page_id;

				g_object_set_data_full (G_OBJECT (node), WEB_EXTENSION_PAGE_ID_KEY, ppage_id, g_free);

				/* Remove first, in case there was a listener already (it's when
				   the page is dynamically filled and not all the elements are
				   available in time of the first call. */
				webkit_dom_event_target_remove_event_listener (
					WEBKIT_DOM_EVENT_TARGET (node), "click",
					G_CALLBACK (element_clicked_cb), FALSE);

				webkit_dom_event_target_add_event_listener (
					WEBKIT_DOM_EVENT_TARGET (node), "click",
					G_CALLBACK (element_clicked_cb), FALSE, extension);
			}
		}
	}
	g_clear_object (&collection);

	/* Traverse also iframe-s */
	collection = webkit_dom_document_get_elements_by_tag_name_as_html_collection (document, "iframe");
	if (collection) {
		len = webkit_dom_html_collection_get_length (collection);
		for (ii = 0; ii < len; ii++) {
			WebKitDOMNode *node;

			node = webkit_dom_html_collection_item (collection, ii);
			if (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (node)) {
				WebKitDOMDocument *content;

				content = webkit_dom_html_iframe_element_get_content_document (WEBKIT_DOM_HTML_IFRAME_ELEMENT (node));
				if (content)
					web_extension_register_element_clicked_in_document (extension, page_id, content, element_class);
			}
		}
	}
	g_clear_object (&collection);
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

	if (camel_debug ("webkit") || camel_debug ("webkit:preview"))
		printf ("EWebExtension - %s - %s\n", G_STRFUNC, method_name);

	if (g_strcmp0 (method_name, "RegisterElementClicked") == 0) {
		const gchar *element_class = NULL;

		g_variant_get (parameters, "(t&s)", &page_id, &element_class);

		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		if (!element_class || !*element_class) {
			g_warn_if_fail (element_class && *element_class);
		} else {
			document = webkit_web_page_get_dom_document (web_page);
			web_extension_register_element_clicked_in_document (extension, page_id, document, element_class);
		}

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetElementHidden") == 0) {
		const gchar *element_id = NULL;
		gboolean hidden = FALSE;

		g_variant_get (parameters, "(t&sb)", &page_id, &element_id, &hidden);

		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		if (!element_id || !*element_id) {
			g_warn_if_fail (element_id && *element_id);
		} else {
			document = webkit_web_page_get_dom_document (web_page);

			/* A secret short-cut, to not have two functions for basically the same thing ("hide attachment" and "hide element") */
			if (!hidden && g_str_has_prefix (element_id, "attachment-wrapper-")) {
				WebKitDOMElement *element;

				element = e_dom_utils_find_element_by_id (document, element_id);

				if (WEBKIT_DOM_IS_HTML_ELEMENT (element) &&
				    webkit_dom_element_get_child_element_count (element) == 0) {
					gchar *inner_html_data;

					inner_html_data = webkit_dom_element_get_attribute (element, "inner-html-data");
					if (inner_html_data && *inner_html_data) {
						webkit_dom_element_set_inner_html (element, inner_html_data, NULL);
						webkit_dom_element_remove_attribute (element, "inner-html-data");
					}

					g_free (inner_html_data);
				}
			}

			e_dom_utils_hide_element (document, element_id, hidden);
		}

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetElementStyleProperty") == 0) {
		const gchar *element_id = NULL, *property_name = NULL, *value = NULL, *priority = NULL;

		g_variant_get (parameters, "(t&s&s&s&s)", &page_id, &element_id, &property_name, &value, &priority);

		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		if (!element_id || !*element_id || !property_name || !*property_name) {
			g_warn_if_fail (element_id && *element_id);
			g_warn_if_fail (property_name && *property_name);
		} else {
			WebKitDOMElement *element;
			gboolean use_child = FALSE;
			gchar *tmp = NULL;

			/* element_id can be also of the form: "id::child", where the change will
			   be done on the first child of it */
			use_child = g_str_has_suffix (element_id, "::child");
			if (use_child) {
				tmp = g_strdup (element_id);
				tmp[strlen (tmp) - 7] = '\0';

				element_id = tmp;
			}

			document = webkit_web_page_get_dom_document (web_page);
			element = e_dom_utils_find_element_by_id (document, element_id);

			if (use_child && element)
				element = webkit_dom_element_get_first_element_child (element);

			if (element) {
				WebKitDOMCSSStyleDeclaration *css;

				css = webkit_dom_element_get_style (element);

				if (value && *value)
					webkit_dom_css_style_declaration_set_property (css, property_name, value, priority, NULL);
				else
					g_free (webkit_dom_css_style_declaration_remove_property (css, property_name, NULL));

				g_clear_object (&css);
			}

			g_free (tmp);
		}

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetElementAttribute") == 0) {
		const gchar *element_id = NULL, *namespace_uri = NULL, *qualified_name = NULL, *value = NULL;

		g_variant_get (parameters, "(t&s&s&s&s)", &page_id, &element_id, &namespace_uri, &qualified_name, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		if (!element_id || !*element_id || !qualified_name || !*qualified_name) {
			g_warn_if_fail (element_id && *element_id);
			g_warn_if_fail (qualified_name && *qualified_name);
		} else {
			WebKitDOMElement *element;
			gboolean use_child = FALSE;
			gchar *tmp = NULL;

			/* element_id can be also of the form: "id::child", where the change will
			   be done on the first child of it */
			use_child = g_str_has_suffix (element_id, "::child");
			if (use_child) {
				tmp = g_strdup (element_id);
				tmp[strlen (tmp) - 7] = '\0';

				element_id = tmp;
			}

			if (namespace_uri && !*namespace_uri)
				namespace_uri = NULL;

			document = webkit_web_page_get_dom_document (web_page);
			element = e_dom_utils_find_element_by_id (document, element_id);

			if (use_child && element)
				element = webkit_dom_element_get_first_element_child (element);

			if (element) {
				if (value && *value)
					webkit_dom_element_set_attribute_ns (element, namespace_uri, qualified_name, value, NULL);
				else
					webkit_dom_element_remove_attribute_ns (element, namespace_uri, qualified_name);
			}

			g_free (tmp);
		}

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
	} else if (g_strcmp0 (method_name, "GetSelectionContentMultipart") == 0) {
		gchar *text_content;
		gboolean is_html = FALSE;

		g_variant_get (parameters, "(t)", &page_id);
		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		text_content = e_dom_utils_get_selection_content_multipart (document, &is_html);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@sb)",
				g_variant_new_take_string (
					text_content ? text_content : g_strdup ("")),
				is_html));
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
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					text_content ? text_content : g_strdup (""))));
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
			WebKitDOMDOMWindow *dom_window;
			WebKitDOMElement *frame_element;

			/* Get frame's window and from the window the actual <iframe> element */
			dom_window = webkit_dom_document_get_default_view (iframe_document);
			frame_element = webkit_dom_dom_window_get_frame_element (dom_window);
			webkit_dom_html_iframe_element_set_src (
				WEBKIT_DOM_HTML_IFRAME_ELEMENT (frame_element), new_iframe_src);
			g_clear_object (&dom_window);
		}

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ProcessMagicSpacebar") == 0) {
		gboolean towards_bottom = FALSE, processed = FALSE;
		WebKitDOMDOMWindow *dom_window;
		glong inner_height = -1, scroll_y_before = -1, scroll_y_after = -1;

		g_variant_get (parameters, "(tb)", &page_id, &towards_bottom);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_window = webkit_dom_document_get_default_view (document);

		g_object_get (G_OBJECT (dom_window),
			"inner-height", &inner_height,
			"scroll-y", &scroll_y_before,
			NULL);

		if (inner_height) {
			webkit_dom_dom_window_scroll_by (dom_window, 0, towards_bottom ? inner_height : -inner_height);

			g_object_get (G_OBJECT (dom_window),
				"scroll-y", &scroll_y_after,
				NULL);

			processed = scroll_y_before != scroll_y_after;
		}

		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", processed));
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
	GVariant *variant = NULL;

	if (g_strcmp0 (property_name, "NeedInput") == 0)
		variant = g_variant_new_boolean (extension->priv->need_input);
	else if (g_strcmp0 (property_name, "ClipboardFlags") == 0)
		variant = g_variant_new_uint32 (extension->priv->clipboard_flags);

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
	} else if (g_strcmp0 (property_name, "ClipboardFlags") == 0) {
		guint32 value = g_variant_get_uint32 (variant);

		if (value == extension->priv->clipboard_flags)
			goto exit;

		extension->priv->clipboard_flags = value;

		g_variant_builder_add (builder,
			"{sv}",
			"ClipboardFlags",
			g_variant_new_uint32 (value));
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
	extension->priv->clipboard_flags = 0;
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
	WebKitDOMDocument *document;

	document = webkit_web_page_get_dom_document (web_page);

	e_dom_utils_replace_local_image_links (document);

	if ((webkit_dom_document_query_selector (
		document, "[data-evo-signature-plain-text-mode]", NULL))) {

		WebKitDOMHTMLElement *body;

		body = webkit_dom_document_get_body (document);

		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body),
			"style",
			"font-family: Monospace;",
			NULL);
	}
}

static void
web_editor_selection_changed_cb (WebKitWebEditor *web_editor,
                                 EWebExtension *extension)
{
	WebKitWebPage *web_page;
	WebKitDOMDocument *document;
	guint32 clipboard_flags = 0;

	web_page = webkit_web_editor_get_page (web_editor);

	document = webkit_web_page_get_dom_document (web_page);

	if (e_dom_utils_document_has_selection (document))
		clipboard_flags |= E_CLIPBOARD_CAN_COPY;

	g_dbus_connection_call (
		extension->priv->dbus_connection,
		E_WEB_EXTENSION_SERVICE_NAME,
		E_WEB_EXTENSION_OBJECT_PATH,
		"org.freedesktop.DBus.Properties",
		"Set",
		g_variant_new (
			"(ssv)",
			E_WEB_EXTENSION_INTERFACE,
			"ClipboardFlags",
			g_variant_new_uint32 (clipboard_flags)),
		NULL,
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
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

	g_signal_connect_object (
		webkit_web_page_get_editor (web_page), "selection-changed",
		G_CALLBACK (web_editor_selection_changed_cb),
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
