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

#include "evolution-config.h"

#include <string.h>

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include "e-web-extension.h"
#include "e-dom-utils.h"
#include "e-itip-formatter-dom-utils.h"
#include "e-web-extension-names.h"

#include <webkitdom/webkitdom.h>

#define WEB_EXTENSION_PAGE_ID_KEY "web-extension-page-id"

#define E_WEB_EXTENSION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEB_EXTENSION, EWebExtensionPrivate))

typedef struct _EWebPageData {
	WebKitWebPage *web_page; /* not referenced */
	gint stamp;
	gboolean need_input;
	guint32 clipboard_flags;
} EWebPageData;

struct _EWebExtensionPrivate {
	WebKitWebExtension *wk_extension;

	GDBusConnection *dbus_connection;
	guint registration_id;

	gboolean initialized;

	GSList *pages; /* EWebPageData * */
};

enum {
	REGISTER_DBUS_CONNECTION,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static const char introspection_xml[] =
"<node>"
"  <interface name='" E_WEB_EXTENSION_INTERFACE "'>"
"    <signal name='ExtensionObjectReady'>"
"    </signal>"
"    <method name='GetExtensionHandlesPages'>"
"      <arg type='at' name='array' direction='out'/>"
"    </method>"
"    <signal name='ExtensionHandlesPage'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='i' name='stamp' direction='out'/>"
"    </signal>"
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
"    <method name='EWebViewEnsureBodyClass'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='body_class' direction='in'/>"
"    </method>"
"    <signal name='NeedInputChanged'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='b' name='need_input' direction='out'/>"
"    </signal>"
"    <signal name='ClipboardFlagsChanged'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='u' name='flags' direction='out'/>"
"    </signal>"
"    <signal name='MailPartAppeared'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='s' name='part_id' direction='out'/>"
"    </signal>"
"    <signal name='ItipRecurToggled'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='s' name='part_id' direction='out'/>"
"    </signal>"
"    <signal name='ItipSourceChanged'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='s' name='part_id' direction='out'/>"
"    </signal>"
"    <method name='ItipCreateDOMBindings'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"    </method>"
"    <method name='ItipShowButton'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='button_id' direction='in'/>"
"    </method>"
"    <method name='ItipElementSetInnerHTML'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='inner_html' direction='in'/>"
"    </method>"
"    <method name='ItipRemoveElement'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='ItipElementRemoveChildNodes'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='ItipEnableButton'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='button_id' direction='in'/>"
"      <arg type='b' name='enable' direction='in'/>"
"    </method>"
"    <method name='ItipElementIsHidden'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='is_hidden' direction='out'/>"
"    </method>"
"    <method name='ItipHideElement'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='hide' direction='in'/>"
"    </method>"
"    <method name='ItipInputSetChecked'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='input_id' direction='in'/>"
"      <arg type='b' name='checked' direction='in'/>"
"    </method>"
"    <method name='ItipInputIsChecked'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='input_id' direction='in'/>"
"      <arg type='b' name='checked' direction='out'/>"
"    </method>"
"    <method name='ItipShowCheckbox'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='id' direction='in'/>"
"      <arg type='b' name='show' direction='in'/>"
"      <arg type='b' name='update_second' direction='in'/>"
"    </method>"
"    <method name='ItipSetButtonsSensitive'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='b' name='sensitive' direction='in'/>"
"    </method>"
"    <method name='ItipSetAreaText'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='id' direction='in'/>"
"      <arg type='s' name='text' direction='in'/>"
"    </method>"
"    <method name='ItipElementSetAccessKey'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='access_key' direction='in'/>"
"    </method>"
"    <method name='ItipElementHideChildNodes'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='ItipEnableSelect'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='b' name='enable' direction='in'/>"
"    </method>"
"    <method name='ItipSelectIsEnabled'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='b' name='enable' direction='out'/>"
"    </method>"
"    <method name='ItipSelectGetValue'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='ItipSelectSetSelected'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='s' name='option' direction='in'/>"
"    </method>"
"    <method name='ItipUpdateTimes'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='header' direction='in'/>"
"      <arg type='s' name='label' direction='in'/>"
"    </method>"
"    <method name='ItipAppendInfoItemRow'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='table_id' direction='in'/>"
"      <arg type='s' name='row_id' direction='in'/>"
"      <arg type='s' name='icon_name' direction='in'/>"
"      <arg type='s' name='message' direction='in'/>"
"    </method>"
"    <method name='ItipEnableTextArea'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='area_id' direction='in'/>"
"      <arg type='b' name='enable' direction='in'/>"
"    </method>"
"    <method name='ItipTextAreaSetValue'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='area_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='ItipTextAreaGetValue'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='area_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='ItipRebuildSourceList'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='optgroup_id' direction='in'/>"
"      <arg type='s' name='optgroup_label' direction='in'/>"
"      <arg type='s' name='option_id' direction='in'/>"
"      <arg type='s' name='option_label' direction='in'/>"
"      <arg type='b' name='writable' direction='in'/>"
"    </method>"
"  </interface>"
"</node>";

G_DEFINE_TYPE_WITH_CODE (EWebExtension, e_web_extension, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

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

static WebKitDOMDocument *
get_webkit_document_or_return_dbus_error (GDBusMethodInvocation *invocation,
                                          WebKitWebExtension *web_extension,
                                          guint64 page_id)
{
	WebKitDOMDocument *document;
	WebKitWebPage *web_page;

	web_page = webkit_web_extension_get_page (web_extension, page_id);
	if (!web_page) {
		g_dbus_method_invocation_return_error (
			invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"Invalid page ID: %" G_GUINT64_FORMAT, page_id);
		return NULL;
	}

	document = webkit_web_page_get_dom_document (web_page);
	if (!document) {
		g_dbus_method_invocation_return_error (
			invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"No document for page ID: %" G_GUINT64_FORMAT, page_id);
		return NULL;
	}

	return document;
}

static WebKitDOMDocument *
find_webkit_document_for_partid_or_return_dbus_error (GDBusMethodInvocation *invocation,
						      WebKitDOMDocument *owner,
						      const gchar *part_id)
{
	WebKitDOMElement *element;

	g_return_val_if_fail (G_IS_DBUS_METHOD_INVOCATION (invocation), NULL);
	g_return_val_if_fail (WEBKIT_DOM_IS_DOCUMENT (owner), NULL);
	g_return_val_if_fail (part_id && *part_id, NULL);

	element = e_dom_utils_find_element_by_id (owner, part_id);
	if (element && WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element)) {
		WebKitDOMDocument *document = webkit_dom_html_iframe_element_get_content_document (WEBKIT_DOM_HTML_IFRAME_ELEMENT (element));
		return document;
	}

	if (element)
		g_dbus_method_invocation_return_error (
			invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"Part ID '%s' is not IFRAME, but %s", part_id, G_OBJECT_TYPE_NAME (element));
	else
		g_dbus_method_invocation_return_error (
			invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"Part ID '%s' not found", part_id);
	return NULL;
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
	while (WEBKIT_DOM_IS_DOM_WINDOW (dom_window)) {
		WebKitDOMDOMWindow *parent_dom_window = webkit_dom_dom_window_get_parent (dom_window);
		WebKitDOMElement *frame_element;
		glong scrll_x = 0, scrll_y = 0;

		frame_element = webkit_dom_dom_window_get_frame_element (dom_window);

		if (parent_dom_window != dom_window && frame_element) {
			with_parents_left += webkit_dom_element_get_client_left (frame_element);
			with_parents_top += webkit_dom_element_get_client_top (frame_element);
		}

		while (frame_element) {
			with_parents_left += webkit_dom_element_get_offset_left (frame_element);
			with_parents_top += webkit_dom_element_get_offset_top (frame_element);

			frame_element = webkit_dom_element_get_offset_parent (frame_element);
		}

		g_object_get (G_OBJECT (dom_window),
			"scroll-x", &scrll_x,
			"scroll-y", &scrll_y,
			NULL);

		scroll_x += scrll_x;
		scroll_y += scrll_y;

		if (parent_dom_window == dom_window) {
			g_clear_object (&parent_dom_window);
			break;
		}

		g_object_unref (dom_window);
		dom_window = parent_dom_window;
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

static guint64
e_web_extension_find_page_id_from_document (WebKitDOMDocument *document)
{
	guint64 *ppage_id;

	g_return_val_if_fail (WEBKIT_DOM_IS_DOCUMENT (document), 0);

	while (document) {
		WebKitDOMDocument *prev_document = document;

		ppage_id = g_object_get_data (G_OBJECT (document), WEB_EXTENSION_PAGE_ID_KEY);
		if (ppage_id)
			return *ppage_id;

		document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (document));
		if (prev_document == document)
			break;
	}

	return 0;
}

static EWebPageData *
e_web_extension_get_page_data (EWebExtension *extension,
			       guint64 page_id)
{
	GSList *link;

	for (link = extension->priv->pages; link; link = g_slist_next (link)) {
		EWebPageData *page_data = link->data;

		if (page_data && webkit_web_page_get_id (page_data->web_page) == page_id)
			return page_data;
	}

	return NULL;
}

static void
e_web_extension_set_need_input (EWebExtension *extension,
				guint64 page_id,
				gboolean need_input)
{
	EWebPageData *page_data;
	GError *error = NULL;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));
	g_return_if_fail (page_id != 0);

	page_data = e_web_extension_get_page_data (extension, page_id);

	if (!page_data || (!page_data->need_input) == (!need_input))
		return;

	page_data->need_input = need_input;

	g_dbus_connection_emit_signal (
		extension->priv->dbus_connection,
		NULL,
		E_WEB_EXTENSION_OBJECT_PATH,
		E_WEB_EXTENSION_INTERFACE,
		"NeedInputChanged",
		g_variant_new ("(tb)", page_id, need_input),
		&error);

	if (error) {
		g_warning ("Error emitting signal NeedInputChanged: %s\n", error->message);
		g_error_free (error);
	}
}

static void
element_focus_cb (WebKitDOMElement *element,
		  WebKitDOMEvent *event,
		  EWebExtension *extension)
{
	guint64 *ppage_id;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));

	ppage_id = g_object_get_data (G_OBJECT (element), WEB_EXTENSION_PAGE_ID_KEY);
	g_return_if_fail (ppage_id != NULL);

	e_web_extension_set_need_input (extension, *ppage_id, TRUE);
}

static void
element_blur_cb (WebKitDOMElement *element,
		 WebKitDOMEvent *event,
		 EWebExtension *extension)
{
	guint64 *ppage_id;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));

	ppage_id = g_object_get_data (G_OBJECT (element), WEB_EXTENSION_PAGE_ID_KEY);
	g_return_if_fail (ppage_id != NULL);

	e_web_extension_set_need_input (extension, *ppage_id, FALSE);
}

static void
e_web_extension_bind_focus_and_blur_recursively (EWebExtension *extension,
						 WebKitDOMDocument *document,
						 const gchar *selector,
						 guint64 page_id)
{
	WebKitDOMNodeList *nodes = NULL;
	WebKitDOMHTMLCollection *frames = NULL;
	gulong ii, length;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));

	nodes = webkit_dom_document_query_selector_all (document, selector, NULL);

	length = webkit_dom_node_list_get_length (nodes);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;
		guint64 *ppage_id;

		node = webkit_dom_node_list_item (nodes, ii);

		ppage_id = g_new (guint64, 1);
		*ppage_id = page_id;

		g_object_set_data_full (G_OBJECT (node), WEB_EXTENSION_PAGE_ID_KEY, ppage_id, g_free);

		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (node), "focus",
			G_CALLBACK (element_focus_cb), FALSE, extension);

		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (node), "blur",
			G_CALLBACK (element_blur_cb), FALSE, extension);
	}
	g_clear_object (&nodes);

	frames = webkit_dom_document_get_elements_by_tag_name_as_html_collection (document, "iframe");
	length = webkit_dom_html_collection_get_length (frames);

	/* Add rules to every sub document */
	for (ii = 0; ii < length; ii++) {
		WebKitDOMDocument *content_document = NULL;
		WebKitDOMNode *node;

		node = webkit_dom_html_collection_item (frames, ii);
		content_document =
			webkit_dom_html_iframe_element_get_content_document (
				WEBKIT_DOM_HTML_IFRAME_ELEMENT (node));

		if (!content_document)
			continue;

		e_web_extension_bind_focus_and_blur_recursively (
			extension,
			content_document,
			selector,
			page_id);
	}
	g_clear_object (&frames);
}

static void
e_web_extension_bind_focus_on_elements (EWebExtension *extension,
					WebKitDOMDocument *document)
{
	const gchar *elements = "input, textarea, select, button, label";
	guint64 page_id;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));
	g_return_if_fail (WEBKIT_DOM_IS_DOCUMENT (document));

	page_id = e_web_extension_find_page_id_from_document (document);
	g_return_if_fail (page_id != 0);

	e_web_extension_bind_focus_and_blur_recursively (
		extension,
		document,
		elements,
		page_id);
}

typedef struct _MailPartAppearedData {
	GWeakRef *dbus_connection;
	GWeakRef *web_page;
	gchar *element_id;
	GVariant *params;
} MailPartAppearedData;

static void
mail_part_appeared_data_free (gpointer ptr)
{
	MailPartAppearedData *mpad = ptr;

	if (mpad) {
		e_weak_ref_free (mpad->dbus_connection);
		e_weak_ref_free (mpad->web_page);
		g_free (mpad->element_id);
		if (mpad->params)
			g_variant_unref (mpad->params);
		g_free (mpad);
	}
}

static gboolean
web_extension_can_emit_mail_part_appeared (WebKitWebPage *web_page,
					   const gchar *element_id,
					   gboolean *out_abort_wait)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMElement *iframe;
	WebKitDOMDocument *iframe_document;
	WebKitDOMHTMLElement *iframe_body;

	g_return_val_if_fail (out_abort_wait != NULL, FALSE);

	*out_abort_wait = TRUE;

	if (!web_page)
		return FALSE;

	if (!element_id || !*element_id)
		return FALSE;

	document = webkit_web_page_get_dom_document (web_page);
	if (!document)
		return FALSE;

	element = e_dom_utils_find_element_by_id (document, element_id);

	if (!WEBKIT_DOM_IS_HTML_ELEMENT (element))
		return FALSE;

	iframe = webkit_dom_element_query_selector (element, "iframe", NULL);
	if (!iframe)
		return FALSE;

	iframe_document = webkit_dom_html_iframe_element_get_content_document (WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe));
	if (!iframe_document)
		return FALSE;

	iframe_body = webkit_dom_document_get_body (iframe_document);
	if (!iframe_body)
		return FALSE;

	*out_abort_wait = FALSE;

	return webkit_dom_element_get_first_element_child (WEBKIT_DOM_ELEMENT (iframe_body)) != NULL;
}

static gboolean
web_extension_emit_mail_part_appeared_cb (gpointer user_data)
{
	MailPartAppearedData *mpad = user_data;
	GDBusConnection *dbus_connection;
	WebKitWebPage *web_page;
	gboolean abort_wait = TRUE;

	g_return_val_if_fail (mpad != NULL, FALSE);

	dbus_connection = g_weak_ref_get (mpad->dbus_connection);
	web_page = g_weak_ref_get (mpad->web_page);

	if (dbus_connection && web_page &&
	    web_extension_can_emit_mail_part_appeared (web_page, mpad->element_id, &abort_wait)) {
		GError *error = NULL;

		g_dbus_connection_emit_signal (
			dbus_connection,
			NULL,
			E_WEB_EXTENSION_OBJECT_PATH,
			E_WEB_EXTENSION_INTERFACE,
			"MailPartAppeared",
			mpad->params,
			&error);

		if (error) {
			g_warning ("Error emitting signal MailPartAppeared: %s", error->message);
			g_error_free (error);
		}

		abort_wait = TRUE;
		mpad->params = NULL;
	}

	if (abort_wait)
		mail_part_appeared_data_free (mpad);

	g_clear_object (&dbus_connection);
	g_clear_object (&web_page);

	return !abort_wait;
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

	if (camel_debug ("webkit:preview"))
		printf ("EWebExtension - %s - %s\n", G_STRFUNC, method_name);

	if (g_strcmp0 (method_name, "GetExtensionHandlesPages") == 0) {
		GVariantBuilder *builder;
		GSList *link;

		builder = g_variant_builder_new (G_VARIANT_TYPE ("at"));

		for (link = extension->priv->pages; link; link = g_slist_next (link)) {
			EWebPageData *page_data = link->data;

			if (page_data) {
				g_variant_builder_add (builder, "t", webkit_web_page_get_id (page_data->web_page));
				g_variant_builder_add (builder, "t", (guint64) page_data->stamp);
			}
		}

		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(at)", builder));

		g_variant_builder_unref (builder);
	} else if (g_strcmp0 (method_name, "RegisterElementClicked") == 0) {
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
			gboolean expand_inner_data = FALSE;

			document = webkit_web_page_get_dom_document (web_page);
			/* A secret short-cut, to not have two functions for basically the same thing ("hide attachment" and "hide element") */
			if (!hidden && g_str_has_prefix (element_id, "attachment-wrapper-")) {
				WebKitDOMElement *element;

				element = e_dom_utils_find_element_by_id (document, element_id);

				if (WEBKIT_DOM_IS_HTML_ELEMENT (element) &&
				    webkit_dom_element_get_child_element_count (element) == 0) {
					gchar *inner_html_data;

					expand_inner_data = TRUE;

					inner_html_data = webkit_dom_element_get_attribute (element, "inner-html-data");
					if (inner_html_data && *inner_html_data) {
						gchar *related_part_id;

						webkit_dom_element_set_inner_html (element, inner_html_data, NULL);
						webkit_dom_element_remove_attribute (element, "inner-html-data");

						related_part_id = webkit_dom_element_get_attribute (element, "related-part-id");
						webkit_dom_element_remove_attribute (element, "related-part-id");

						if (related_part_id && *related_part_id) {
							GVariant *params = g_variant_new ("(ts)", page_id, related_part_id);
							WebKitDOMElement *iframe;

							iframe = webkit_dom_element_query_selector (element, "iframe", NULL);
							if (iframe) {
								WebKitDOMDocument *iframe_document;

								iframe_document = webkit_dom_html_iframe_element_get_content_document (WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe));
								if (iframe_document) {
									WebKitDOMHTMLElement *iframe_body;

									iframe_body = webkit_dom_document_get_body (iframe_document);
									if (iframe_body && !webkit_dom_element_get_first_element_child (WEBKIT_DOM_ELEMENT (iframe_body))) {
										/* The iframe document is still empty, wait until it's loaded;
										   wish being there something better than this busy-wait... */
										MailPartAppearedData *mpad;

										mpad = g_new0 (MailPartAppearedData, 1);
										mpad->dbus_connection = e_weak_ref_new (extension->priv->dbus_connection);
										mpad->web_page = e_weak_ref_new (web_page);
										mpad->element_id = g_strdup (element_id);
										mpad->params = params;

										/* Try 10 times per second */
										g_timeout_add (100, web_extension_emit_mail_part_appeared_cb, mpad);

										/* To not emit the signal below */
										params = NULL;
									}
								}
							}

							if (params) {
								GError *error = NULL;

								g_dbus_connection_emit_signal (
									extension->priv->dbus_connection,
									NULL,
									E_WEB_EXTENSION_OBJECT_PATH,
									E_WEB_EXTENSION_INTERFACE,
									"MailPartAppeared",
									params,
									&error);

								if (error) {
									g_warning ("Error emitting signal MailPartAppeared: %s", error->message);
									g_error_free (error);
								}
							}
						}

						g_free (related_part_id);
					}

					g_free (inner_html_data);
				}
			}

			e_dom_utils_hide_element (document, element_id, hidden);

			if (expand_inner_data)
				e_dom_resize_document_content_to_preview_width (document);
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
		e_dom_utils_e_mail_display_unstyle_blockquotes (document);
		e_dom_utils_e_mail_display_bind_dom (document, connection);
		e_web_extension_bind_focus_on_elements (extension, document);

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
	} else if (g_strcmp0 (method_name, "EWebViewEnsureBodyClass") == 0) {
		const gchar *body_class = NULL;
		WebKitDOMHTMLElement *body;

		g_variant_get (parameters, "(t&s)", &page_id, &body_class);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);

		body = webkit_dom_document_get_body (document);
		if (body && !webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (body), "class"))
			webkit_dom_element_set_class_name (WEBKIT_DOM_ELEMENT (body), body_class);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ItipCreateDOMBindings") == 0) {
		const gchar *part_id = NULL;

		g_variant_get (parameters, "(t&s)", &page_id, &part_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_create_dom_bindings (document, page_id, part_id, connection);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipShowButton") == 0) {
		const gchar *button_id, *part_id = NULL;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &button_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_show_button (document, button_id);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipEnableButton") == 0) {
		const gchar *button_id, *part_id = NULL;
		gboolean enable;

		g_variant_get (parameters, "(t&s&sb)", &page_id, &part_id, &button_id, &enable);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_enable_button (document, button_id, enable);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipElementSetInnerHTML") == 0) {
		const gchar *element_id, *inner_html, *part_id = NULL;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &part_id, &element_id, &inner_html);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_dom_utils_element_set_inner_html (document, element_id, inner_html);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipRemoveElement") == 0) {
		const gchar *element_id, *part_id = NULL;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &element_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_dom_utils_remove_element (document, element_id);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipElementRemoveChildNodes") == 0) {
		const gchar *element_id, *part_id = NULL;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &element_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_dom_utils_element_remove_child_nodes (document, element_id);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipHideElement") == 0) {
		const gchar *element_id, *part_id = NULL;
		gboolean hide;

		g_variant_get (parameters, "(t&s&sb)", &page_id, &part_id, &element_id, &hide);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_dom_utils_hide_element (document, element_id, hide);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipElementIsHidden") == 0) {
		const gchar *element_id, *part_id = NULL;
		gboolean hidden;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &element_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			hidden = e_dom_utils_element_is_hidden (document, element_id);
			g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", hidden));
		}
	} else if (g_strcmp0 (method_name, "ItipInputSetChecked") == 0) {
		const gchar *input_id, *part_id = NULL;
		gboolean checked;

		g_variant_get (parameters, "(t&s&sb)", &page_id, &part_id, &input_id, &checked);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_input_set_checked (document, input_id, checked);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipInputIsChecked") == 0) {
		const gchar *input_id, *part_id = NULL;
		gboolean checked;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &input_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			checked = e_itip_formatter_dom_utils_input_is_checked (document, input_id);
			g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", checked));
		}
	} else if (g_strcmp0 (method_name, "ItipShowCheckbox") == 0) {
		const gchar *id, *part_id = NULL;
		gboolean show, update_second;

		g_variant_get (parameters, "(t&s&sbb)", &page_id, &part_id, &id, &show, &update_second);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_show_checkbox (document, id, show, update_second);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipSetButtonsSensitive") == 0) {
		const gchar *part_id = NULL;
		gboolean sensitive;

		g_variant_get (parameters, "(t&sb)", &page_id, &part_id, &sensitive);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_set_buttons_sensitive (document, sensitive);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipSetAreaText") == 0) {
		const gchar *id, *text, *part_id = NULL;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &part_id, &id, &text);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_set_area_text (document, id, text);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipElementSetAccessKey") == 0) {
		const gchar *element_id, *access_key, *part_id = NULL;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &part_id, &element_id, &access_key);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_element_set_access_key (document, element_id, access_key);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipElementHideChildNodes") == 0) {
		const gchar *element_id, *part_id = NULL;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &element_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_element_hide_child_nodes (document, element_id);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipEnableSelect") == 0) {
		const gchar *select_id, *part_id = NULL;
		gboolean enable;

		g_variant_get (parameters, "(t&s&sb)", &page_id, &part_id, &select_id, &enable);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_enable_select (document, select_id, enable);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipSelectIsEnabled") == 0) {
		const gchar *select_id, *part_id = NULL;
		gboolean enabled;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &select_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			enabled = e_itip_formatter_dom_utils_select_is_enabled (document, select_id);
			g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", enabled));
		}
	} else if (g_strcmp0 (method_name, "ItipSelectGetValue") == 0) {
		const gchar *select_id, *part_id = NULL;
		gchar *value;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &select_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			value = e_itip_formatter_dom_utils_select_get_value (document, select_id);
			g_dbus_method_invocation_return_value (invocation,
				g_variant_new (
					"(@s)",
					g_variant_new_take_string (value ? value : g_strdup (""))));
		}
	} else if (g_strcmp0 (method_name, "ItipSelectSetSelected") == 0) {
		const gchar *select_id, *option, *part_id = NULL;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &part_id, &select_id, &option);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_select_set_selected (document, select_id, option);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipUpdateTimes") == 0) {
		const gchar *element_id, *header, *label, *part_id = NULL;

		g_variant_get (parameters, "(t&s&s&s&s)", &page_id, &part_id, &element_id, &header, &label);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_update_times (document, element_id, header, label);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipAppendInfoItemRow") == 0) {
		const gchar *table_id, *row_id, *icon_name, *message, *part_id = NULL;

		g_variant_get (parameters, "(t&s&s&s&s&s)", &page_id, &part_id, &table_id, &row_id, &icon_name, &message);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_append_info_item_row (document, table_id, row_id, icon_name, message);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipEnableTextArea") == 0) {
		const gchar *area_id, *part_id = NULL;
		gboolean enable;

		g_variant_get (parameters, "(t&s&sb)", &page_id, &part_id, &area_id, &enable);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_enable_text_area (document, area_id, enable);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipTextAreaSetValue") == 0) {
		const gchar *area_id, *value, *part_id = NULL;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &part_id, &area_id, &value);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_text_area_set_value (document, area_id, value);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ItipTextAreaGetValue") == 0) {
		const gchar *area_id, *part_id = NULL;
		gchar *value;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &area_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			value = e_itip_formatter_dom_utils_text_area_get_value (document, area_id);
			g_dbus_method_invocation_return_value (invocation,
				g_variant_new (
					"(@s)",
					g_variant_new_take_string (value ? value : g_strdup (""))));
		}
	} else if (g_strcmp0 (method_name, "ItipRebuildSourceList") == 0) {
		const gchar *optgroup_id, *optgroup_label, *option_id, *option_label, *part_id = NULL;
		gboolean writable;

		g_variant_get (parameters,"(t&s&s&s&s&sb)", &page_id, &part_id, &optgroup_id, &optgroup_label, &option_id, &option_label, &writable);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_itip_formatter_dom_utils_rebuild_source_list (
				document,
				optgroup_id,
				optgroup_label,
				option_id,
				option_label,
				writable);

			g_dbus_method_invocation_return_value (invocation, NULL);
		}
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
	/* EWebExtension *extension = E_WEB_EXTENSION (user_data); */
	GVariant *variant = NULL;

	g_warn_if_reached ();

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
	/* EWebExtension *extension = E_WEB_EXTENSION (user_data); */

	g_warn_if_reached ();

	return TRUE;
}

static const GDBusInterfaceVTable interface_vtable = {
	handle_method_call,
	handle_get_property,
	handle_set_property
};

static void
web_page_gone_cb (gpointer user_data,
                  GObject *gone_web_page)
{
	EWebExtension *extension = user_data;
	GSList *link;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));

	for (link = extension->priv->pages; link; link = g_slist_next (link)) {
		EWebPageData *page_data = link->data;

		if (page_data && page_data->web_page == (gpointer) gone_web_page) {
			extension->priv->pages = g_slist_remove (extension->priv->pages, page_data);
			g_free (page_data);
			break;
		}
	}
}

static void
e_web_extension_constructed (GObject *object)
{
	G_OBJECT_CLASS (e_web_extension_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
e_web_extension_dispose (GObject *object)
{
	EWebExtension *extension = E_WEB_EXTENSION (object);

	if (extension->priv->dbus_connection) {
		g_dbus_connection_unregister_object (
			extension->priv->dbus_connection,
			extension->priv->registration_id);
		extension->priv->registration_id = 0;
		g_clear_object (&extension->priv->dbus_connection);
	}

	g_slist_free_full (extension->priv->pages, g_free);
	extension->priv->pages = NULL;

	g_clear_object (&extension->priv->wk_extension);

	G_OBJECT_CLASS (e_web_extension_parent_class)->dispose (object);
}

static void
e_web_extension_class_init (EWebExtensionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	g_type_class_add_private (object_class, sizeof (EWebExtensionPrivate));

	object_class->constructed = e_web_extension_constructed;
	object_class->dispose = e_web_extension_dispose;

	signals[REGISTER_DBUS_CONNECTION] = g_signal_new (
		"register-dbus-connection",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 1, G_TYPE_DBUS_CONNECTION);
}

static void
e_web_extension_init (EWebExtension *extension)
{
	extension->priv = G_TYPE_INSTANCE_GET_PRIVATE (extension, E_TYPE_WEB_EXTENSION, EWebExtensionPrivate);

	extension->priv->initialized = FALSE;
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
e_web_extension_store_page_id_on_document (WebKitWebPage *web_page)
{
	WebKitDOMDocument *document;
	guint64 *ppage_id;

	g_return_if_fail (WEBKIT_IS_WEB_PAGE (web_page));

	ppage_id = g_new (guint64, 1);
	*ppage_id = webkit_web_page_get_id (web_page);

	document = webkit_web_page_get_dom_document (web_page);

	g_object_set_data_full (G_OBJECT (document), WEB_EXTENSION_PAGE_ID_KEY, ppage_id, g_free);
}

static void
web_page_document_loaded_cb (WebKitWebPage *web_page,
                             gpointer user_data)
{
	WebKitDOMDocument *document;

	e_web_extension_store_page_id_on_document (web_page);

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
e_web_extension_set_clipboard_flags (EWebExtension *extension,
				     WebKitDOMDocument *document,
				     guint32 clipboard_flags)
{
	EWebPageData *page_data = NULL;
	guint64 page_id;
	GError *error = NULL;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));
	g_return_if_fail (WEBKIT_DOM_IS_DOCUMENT (document));

	page_id = e_web_extension_find_page_id_from_document (document);
	g_return_if_fail (page_id != 0);

	page_data = e_web_extension_get_page_data (extension, page_id);

	if (!page_data || page_data->clipboard_flags == clipboard_flags)
		return;

	page_data->clipboard_flags = clipboard_flags;

	g_dbus_connection_emit_signal (
		extension->priv->dbus_connection,
		NULL,
		E_WEB_EXTENSION_OBJECT_PATH,
		E_WEB_EXTENSION_INTERFACE,
		"ClipboardFlagsChanged",
		g_variant_new ("(tu)", page_id, clipboard_flags),
		&error);

	if (error) {
		g_warning ("Error emitting signal ClipboardFlagsChanged: %s\n", error->message);
		g_error_free (error);
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

	e_web_extension_set_clipboard_flags (extension, document, clipboard_flags);
}

static void
web_page_notify_uri_cb (GObject *object,
			GParamSpec *param,
			gpointer user_data)
{
	EWebExtension *extension = user_data;
	WebKitWebPage *web_page;
	GSList *link;
	const gchar *uri;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));

	web_page = WEBKIT_WEB_PAGE (object);
	uri = webkit_web_page_get_uri (web_page);

	for (link = extension->priv->pages; link; link = g_slist_next (link)) {
		EWebPageData *page_data = link->data;

		if (page_data && page_data->web_page == web_page) {
			gint new_stamp = 0;

			if (uri && *uri) {
				SoupURI *suri;

				suri = soup_uri_new (uri);
				if (suri) {
					if (soup_uri_get_query (suri)) {
						GHashTable *form;

						form = soup_form_decode (soup_uri_get_query (suri));
						if (form) {
							const gchar *evo_stamp;

							evo_stamp = g_hash_table_lookup (form, "evo-stamp");
							if (evo_stamp)
								new_stamp = (gint) g_ascii_strtoll (evo_stamp, NULL, 10);

							g_hash_table_destroy (form);
						}
					}

					soup_uri_free (suri);
				}
			}

			if (extension->priv->dbus_connection) {
				GError *error = NULL;

				g_dbus_connection_emit_signal (
					extension->priv->dbus_connection,
					NULL,
					E_WEB_EXTENSION_OBJECT_PATH,
					E_WEB_EXTENSION_INTERFACE,
					"ExtensionHandlesPage",
					g_variant_new ("(ti)", webkit_web_page_get_id (web_page), new_stamp),
					&error);

				if (error) {
					g_warning ("Error emitting signal ExtensionHandlesPage: %s", error->message);
					g_error_free (error);
				}
			}

			page_data->stamp = new_stamp;
			return;
		}
	}

	g_warning ("%s: Cannot find web_page %p\n", G_STRFUNC, web_page);
}

static void
web_page_created_cb (WebKitWebExtension *wk_extension,
                     WebKitWebPage *web_page,
                     EWebExtension *extension)
{
	EWebPageData *page_data;

	page_data = g_new0 (EWebPageData, 1);
	page_data->web_page = web_page;
	page_data->need_input = FALSE;
	page_data->clipboard_flags = 0;
	page_data->stamp = 0;

	e_web_extension_store_page_id_on_document (web_page);

	extension->priv->pages = g_slist_prepend (extension->priv->pages, page_data);

	g_object_weak_ref (G_OBJECT (web_page), web_page_gone_cb, extension);

	g_signal_connect_object (
		web_page, "send-request",
		G_CALLBACK (web_page_send_request_cb),
		extension, 0);

	g_signal_connect_object (
		web_page, "document-loaded",
		G_CALLBACK (web_page_document_loaded_cb),
		extension, 0);

	g_signal_connect_object (
		web_page, "notify::uri",
		G_CALLBACK (web_page_notify_uri_cb),
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
			extension->priv->dbus_connection = g_object_ref (connection);

			g_signal_emit (extension, signals[REGISTER_DBUS_CONNECTION], 0, connection);

			g_dbus_connection_emit_signal (
				extension->priv->dbus_connection,
				NULL,
				E_WEB_EXTENSION_OBJECT_PATH,
				E_WEB_EXTENSION_INTERFACE,
				"ExtensionObjectReady",
				NULL,
				&error);

			if (error) {
				g_warning ("Error emitting signal ExtensionObjectReady: %s", error->message);
				g_error_free (error);
			}
		}
	}
}

WebKitWebExtension *
e_web_extension_get_webkit_extension (EWebExtension *extension)
{
	g_return_val_if_fail (E_IS_WEB_EXTENSION (extension), NULL);

	return extension->priv->wk_extension;
}

GDBusConnection *
e_web_extension_get_dbus_connection (EWebExtension *extension)
{
	g_return_val_if_fail (E_IS_WEB_EXTENSION (extension), NULL);

	return extension->priv->dbus_connection;
}
