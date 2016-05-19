/*
 * module-itip-formatter-web-extension.c
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

#include "module-itip-formatter-web-extension.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit2/webkit-web-extension.h>

#include <web-extensions/e-dom-utils.h>

#include "module-itip-formatter-dom-utils.h"

/* FIXME Clean it */
static GDBusConnection *dbus_connection;

static const char introspection_xml[] =
"<node>"
"  <interface name='" MODULE_ITIP_FORMATTER_WEB_EXTENSION_INTERFACE "'>"
"    <signal name='RecurToggled'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='s' name='part_id' direction='out'/>"
"    </signal>"
"    <signal name='SourceChanged'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='s' name='part_id' direction='out'/>"
"    </signal>"
"    <method name='CreateDOMBindings'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"    </method>"
"    <method name='ShowButton'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='button_id' direction='in'/>"
"    </method>"
"    <method name='ElementSetInnerHTML'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='inner_html' direction='in'/>"
"    </method>"
"    <method name='RemoveElement'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='ElementRemoveChildNodes'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='EnableButton'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='button_id' direction='in'/>"
"      <arg type='b' name='enable' direction='in'/>"
"    </method>"
"    <method name='ElementIsHidden'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='is_hidden' direction='out'/>"
"    </method>"
"    <method name='HideElement'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='hide' direction='in'/>"
"    </method>"
"    <method name='InputSetChecked'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='input_id' direction='in'/>"
"      <arg type='b' name='checked' direction='in'/>"
"    </method>"
"    <method name='InputIsChecked'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='input_id' direction='in'/>"
"      <arg type='b' name='checked' direction='out'/>"
"    </method>"
"    <method name='ShowCheckbox'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='id' direction='in'/>"
"      <arg type='b' name='show' direction='in'/>"
"      <arg type='b' name='update_second' direction='in'/>"
"    </method>"
"    <method name='SetButtonsSensitive'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='b' name='sensitive' direction='in'/>"
"    </method>"
"    <method name='SetAreaText'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='id' direction='in'/>"
"      <arg type='s' name='text' direction='in'/>"
"    </method>"
"    <method name='ElementSetAccessKey'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='access_key' direction='in'/>"
"    </method>"
"    <method name='ElementHideChildNodes'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='EnableSelect'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='b' name='enable' direction='in'/>"
"    </method>"
"    <method name='SelectIsEnabled'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='b' name='enable' direction='out'/>"
"    </method>"
"    <method name='SelectGetValue'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='SelectSetSelected'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='s' name='option' direction='in'/>"
"    </method>"
"    <method name='UpdateTimes'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='header' direction='in'/>"
"      <arg type='s' name='label' direction='in'/>"
"    </method>"
"    <method name='AppendInfoItemRow'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='table_id' direction='in'/>"
"      <arg type='s' name='row_id' direction='in'/>"
"      <arg type='s' name='icon_name' direction='in'/>"
"      <arg type='s' name='message' direction='in'/>"
"    </method>"
"    <method name='EnableTextArea'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='area_id' direction='in'/>"
"      <arg type='b' name='enable' direction='in'/>"
"    </method>"
"    <method name='TextAreaSetValue'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='area_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='TextAreaGetValue'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='part_id' direction='in'/>"
"      <arg type='s' name='area_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='RebuildSourceList'>"
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
	WebKitDOMDocument *document;
	const gchar *part_id = NULL;
	guint64 page_id;

	if (g_strcmp0 (interface_name, MODULE_ITIP_FORMATTER_WEB_EXTENSION_INTERFACE) != 0)
		return;

	if (g_strcmp0 (method_name, "CreateDOMBindings") == 0) {
		g_variant_get (parameters, "(t&s)", &page_id, &part_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_create_dom_bindings (document, page_id, part_id, connection);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ShowButton") == 0) {
		const gchar *button_id;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &button_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_show_button (document, button_id);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "EnableButton") == 0) {
		const gchar *button_id;
		gboolean enable;

		g_variant_get (parameters, "(t&s&sb)", &page_id, &part_id, &button_id, &enable);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_enable_button (document, button_id, enable);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ElementSetInnerHTML") == 0) {
		const gchar *element_id, *inner_html;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &part_id, &element_id, &inner_html);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_dom_utils_element_set_inner_html (document, element_id, inner_html);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "RemoveElement") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &element_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_dom_utils_remove_element (document, element_id);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ElementRemoveChildNodes") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &element_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_dom_utils_element_remove_child_nodes (document, element_id);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "HideElement") == 0) {
		const gchar *element_id;
		gboolean hide;

		g_variant_get (parameters, "(t&s&sb)", &page_id, &part_id, &element_id, &hide);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			e_dom_utils_hide_element (document, element_id, hide);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ElementIsHidden") == 0) {
		const gchar *element_id;
		gboolean hidden;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &element_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			hidden = e_dom_utils_element_is_hidden (document, element_id);
			g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", hidden));
		}
	} else if (g_strcmp0 (method_name, "InputSetChecked") == 0) {
		const gchar *input_id;
		gboolean checked;

		g_variant_get (parameters, "(t&s&sb)", &page_id, &part_id, &input_id, &checked);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_input_set_checked (document, input_id, checked);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "InputIsChecked") == 0) {
		const gchar *input_id;
		gboolean checked;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &input_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			checked = module_itip_formatter_dom_utils_input_is_checked (document, input_id);
			g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", checked));
		}
	} else if (g_strcmp0 (method_name, "ShowCheckbox") == 0) {
		const gchar *id;
		gboolean show, update_second;

		g_variant_get (parameters, "(t&s&sbb)", &page_id, &part_id, &id, &show, &update_second);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_show_checkbox (document, id, show, update_second);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "SetButtonsSensitive") == 0) {
		gboolean sensitive;

		g_variant_get (parameters, "(t&sb)", &page_id, &part_id, &sensitive);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_set_buttons_sensitive (document, sensitive);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "SetAreaText") == 0) {
		const gchar *id, *text;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &part_id, &id, &text);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_set_area_text (document, id, text);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ElementSetAccessKey") == 0) {
		const gchar *element_id, *access_key;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &part_id, &element_id, &access_key);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_element_set_access_key (document, element_id, access_key);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "ElementHideChildNodes") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &element_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_element_hide_child_nodes (document, element_id);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "EnableSelect") == 0) {
		const gchar *select_id;
		gboolean enable;

		g_variant_get (parameters, "(t&s&sb)", &page_id, &part_id, &select_id, &enable);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_enable_select (document, select_id, enable);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "SelectIsEnabled") == 0) {
		const gchar *select_id;
		gboolean enabled;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &select_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			enabled = module_itip_formatter_dom_utils_select_is_enabled (document, select_id);
			g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", enabled));
		}
	} else if (g_strcmp0 (method_name, "SelectGetValue") == 0) {
		const gchar *select_id;
		gchar *value;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &select_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			value = module_itip_formatter_dom_utils_select_get_value (document, select_id);
			g_dbus_method_invocation_return_value (invocation,
				g_variant_new (
					"(@s)",
					g_variant_new_take_string (value ? value : g_strdup (""))));
		}
	} else if (g_strcmp0 (method_name, "SelectSetSelected") == 0) {
		const gchar *select_id, *option;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &part_id, &select_id, &option);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_select_set_selected (document, select_id, option);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "UpdateTimes") == 0) {
		const gchar *element_id, *header, *label;

		g_variant_get (parameters, "(t&s&s&s&s)", &page_id, &part_id, &element_id, &header, &label);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_update_times (document, element_id, header, label);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "AppendInfoItemRow") == 0) {
		const gchar *table_id, *row_id, *icon_name, *message;

		g_variant_get (parameters, "(t&s&s&s&s&s)", &page_id, &part_id, &table_id, &row_id, &icon_name, &message);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_append_info_item_row (document, table_id, row_id, icon_name, message);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "EnableTextArea") == 0) {
		const gchar *area_id;
		gboolean enable;

		g_variant_get (parameters, "(t&s&sb)", &page_id, &part_id, &area_id, &enable);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_enable_text_area (document, area_id, enable);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "TextAreaSetValue") == 0) {
		const gchar *area_id, *value;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &part_id, &area_id, &value);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_text_area_set_value (document, area_id, value);
			g_dbus_method_invocation_return_value (invocation, NULL);
		}
	} else if (g_strcmp0 (method_name, "TextAreaGetValue") == 0) {
		const gchar *area_id;
		gchar *value;

		g_variant_get (parameters, "(t&s&s)", &page_id, &part_id, &area_id);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			value = module_itip_formatter_dom_utils_text_area_get_value (document, area_id);
			g_dbus_method_invocation_return_value (invocation,
				g_variant_new (
					"(@s)",
					g_variant_new_take_string (value ? value : g_strdup (""))));
		}
	} else if (g_strcmp0 (method_name, "RebuildSourceList") == 0) {
		const gchar *optgroup_id, *optgroup_label, *option_id, *option_label;
		gboolean writable;

		g_variant_get (parameters,"(t&s&s&s&s&sb)", &page_id, &part_id, &optgroup_id, &optgroup_label, &option_id, &option_label, &writable);

		document = get_webkit_document_or_return_dbus_error (invocation, web_extension, page_id);
		if (document)
			document = find_webkit_document_for_partid_or_return_dbus_error (invocation, document, part_id);
		if (document) {
			module_itip_formatter_dom_utils_rebuild_source_list (
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
			MODULE_ITIP_FORMATTER_WEB_EXTENSION_OBJECT_PATH,
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
		MODULE_ITIP_FORMATTER_WEB_EXTENSION_SERVICE_NAME,
		G_BUS_NAME_OWNER_FLAGS_NONE,
		bus_acquired_cb,
		NULL, NULL,
		g_object_ref (extension),
		g_object_unref);
}
