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
"  <interface name='"MODULE_ITIP_FORMATTER_WEB_EXTENSION_INTERFACE"'>"
"    <signal name='RecurToggled'>"
"    </signal>"
"    <signal name='SourceChanged'>"
"    </signal>"
"    <signal name='ButtonClicked'>"
"      <arg type='s' name='button_value' direction='out'/>"
"    </signal>"
"    <method name='SaveDocumentFromElement'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='ShowButton'>"
"      <arg type='s' name='button_id' direction='in'/>"
"    </method>"
"    <method name='CreateDOMBindings'>"
"    </method>"
"    <method name='ElementSetInnerHTML'>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='inner_html' direction='in'/>"
"    </method>"
"    <method name='RemoveElement'>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='ElementRemoveChildNodes'>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='EnableButton'>"
"      <arg type='s' name='button_id' direction='in'/>"
"      <arg type='b' name='enable' direction='in'/>"
"    </method>"
"    <method name='ElementIsHidden'>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='is_hidden' direction='out'/>"
"    </method>"
"    <method name='HideElement'>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='hide' direction='in'/>"
"    </method>"
"    <method name='BindSaveButton'>"
"    </method>"
"    <method name='InputSetChecked'>"
"      <arg type='s' name='input_id' direction='in'/>"
"      <arg type='b' name='checked' direction='in'/>"
"    </method>"
"    <method name='InputIsChecked'>"
"      <arg type='s' name='input_id' direction='in'/>"
"      <arg type='b' name='checked' direction='out'/>"
"    </method>"
"    <method name='ShowCheckbox'>"
"      <arg type='s' name='id' direction='in'/>"
"      <arg type='b' name='show' direction='in'/>"
"      <arg type='b' name='update_second' direction='in'/>"
"    </method>"
"    <method name='SetButtonsSensitive'>"
"      <arg type='b' name='sensitive' direction='in'/>"
"    </method>"
"    <method name='SetAreaText'>"
"      <arg type='s' name='id' direction='in'/>"
"      <arg type='s' name='text' direction='in'/>"
"    </method>"
"    <method name='ElementSetAccessKey'>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='access_key' direction='in'/>"
"    </method>"
"    <method name='ElementHideChildNodes'>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='EnableSelect'>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='b' name='enable' direction='in'/>"
"    </method>"
"    <method name='SelectIsEnabled'>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='b' name='enable' direction='out'/>"
"    </method>"
"    <method name='SelectGetValue'>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='SelectSetSelected'>"
"      <arg type='s' name='select_id' direction='in'/>"
"      <arg type='s' name='option' direction='in'/>"
"    </method>"
"    <method name='UpdateTimes'>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='header' direction='in'/>"
"      <arg type='s' name='label' direction='in'/>"
"    </method>"
"    <method name='AppendInfoItemRow'>"
"      <arg type='s' name='table_id' direction='in'/>"
"      <arg type='s' name='row_id' direction='in'/>"
"      <arg type='s' name='icon_name' direction='in'/>"
"      <arg type='s' name='message' direction='in'/>"
"    </method>"
"    <method name='EnableTextArea'>"
"      <arg type='s' name='area_id' direction='in'/>"
"      <arg type='b' name='enable' direction='in'/>"
"    </method>"
"    <method name='TextAreaSetValue'>"
"      <arg type='s' name='area_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='TextAreaGetValue'>"
"      <arg type='s' name='area_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='RebuildSourceList'>"
"      <arg type='s' name='optgroup_id' direction='in'/>"
"      <arg type='s' name='optgroup_label' direction='in'/>"
"      <arg type='s' name='option_id' direction='in'/>"
"      <arg type='s' name='option_label' direction='in'/>"
"      <arg type='b' name='writable' direction='in'/>"
"    </method>"
"  </interface>"
"</node>";

static WebKitDOMDocument *document_saved = NULL;

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

	if (g_strcmp0 (interface_name, MODULE_ITIP_FORMATTER_WEB_EXTENSION_INTERFACE) != 0)
		return;

	if (g_strcmp0 (method_name, "SaveDocumentFromElement") == 0) {
		WebKitDOMElement *element;
		const gchar *element_id;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);
		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		document_saved = document;

		element = e_dom_utils_find_element_by_id (document, element_id);

		if (!WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element))
			element = webkit_dom_element_query_selector (
				element, "iframe", NULL);

		if (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element))
			document_saved =
				webkit_dom_html_iframe_element_get_content_document (
					WEBKIT_DOM_HTML_IFRAME_ELEMENT (element));
		else
			document_saved = document;

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ShowButton") == 0) {
		const gchar *button_id;

		g_variant_get (parameters, "(&s)", &button_id);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_show_button (document_saved, button_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EnableButton") == 0) {
		const gchar *button_id;
		gboolean enable;

		g_variant_get (parameters, "(&sb)", &button_id, &enable);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_enable_button (
			document_saved, button_id, enable);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "CreateDOMBindings") == 0) {

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_create_dom_bindings (
			document_saved, connection);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementSetInnerHTML") == 0) {
		const gchar *element_id, *inner_html;

		g_variant_get (parameters, "(&s&s)", &element_id, &inner_html);
		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		e_dom_utils_element_set_inner_html (
			document_saved, element_id, inner_html);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "RemoveElement") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(&s)", &element_id);
		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		e_dom_utils_remove_element (document_saved, element_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementRemoveChildNodes") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(&s)", &element_id);
		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		e_dom_utils_element_remove_child_nodes (document_saved, element_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "HideElement") == 0) {
		const gchar *element_id;
		gboolean hide;

		g_variant_get (parameters, "(&sb)", &element_id, &hide);
		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		e_dom_utils_hide_element (document_saved, element_id, hide);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementIsHidden") == 0) {
		const gchar *element_id;
		gboolean hidden;

		g_variant_get (parameters, "(&s)", &element_id);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		hidden = e_dom_utils_element_is_hidden (document_saved, element_id);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", hidden));
	} else if (g_strcmp0 (method_name, "BindSaveButton") == 0) {
		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_bind_save_button (
			document_saved, connection);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "InputSetChecked") == 0) {
		const gchar *input_id;
		gboolean checked;

		g_variant_get (parameters, "(&sb)", &input_id, &checked);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_input_set_checked (
			document_saved, input_id, checked);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "InputIsChecked") == 0) {
		const gchar *input_id;
		gboolean checked;

		g_variant_get (parameters, "(&s)", &input_id);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		checked = module_itip_formatter_dom_utils_input_is_checked (
			document_saved, input_id);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", checked));
	} else if (g_strcmp0 (method_name, "ShowCheckbox") == 0) {
		const gchar *id;
		gboolean show, update_second;

		g_variant_get (parameters, "(&sbb)", &id, &show, &update_second);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_show_checkbox (
			document_saved, id, show, update_second);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetButtonsSensitive") == 0) {
		gboolean sensitive;

		g_variant_get (parameters, "(b)", &sensitive);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_set_buttons_sensitive (
			document_saved, sensitive);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetAreaText") == 0) {
		const gchar *id, *text;

		g_variant_get (parameters, "(&s&s)", &id, &text);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_set_area_text (
			document_saved, id, text);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementSetAccessKey") == 0) {
		const gchar *element_id, *access_key;

		g_variant_get (parameters, "(&s&s)", &element_id, &access_key);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_element_set_access_key (
			document_saved, element_id, access_key);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementHideChildNodes") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(&s)", &element_id);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_element_hide_child_nodes (
			document_saved, element_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EnableSelect") == 0) {
		const gchar *select_id;
		gboolean enable;

		g_variant_get (parameters, "(&sb)", &select_id, &enable);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_enable_select (
			document_saved, select_id, enable);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SelectIsEnabled") == 0) {
		const gchar *select_id;
		gboolean enabled;

		g_variant_get (parameters, "(&s)", &select_id);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		enabled = module_itip_formatter_dom_utils_select_is_enabled (
			document_saved, select_id);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", enabled));
	} else if (g_strcmp0 (method_name, "SelectGetValue") == 0) {
		const gchar *select_id;
		gchar *value;

		g_variant_get (parameters, "(&s)", &select_id);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		value = module_itip_formatter_dom_utils_select_get_value (
			document_saved, select_id);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "SelectSetSelected") == 0) {
		const gchar *select_id, *option;

		g_variant_get (parameters, "(&s&s)", &select_id, &option);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_select_set_selected (
			document_saved, select_id, option);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "UpdateTimes") == 0) {
		const gchar *element_id, *header, *label;

		g_variant_get (parameters, "(&s&s&s)", &element_id, &header, &label);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_update_times (
			document_saved, element_id, header, label);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "AppendInfoItemRow") == 0) {
		const gchar *table_id, *row_id, *icon_name, *message;

		g_variant_get (
			parameters,
			"(&s&s&s&s)",
			&table_id, &row_id, &icon_name, &message);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_append_info_item_row (
			document_saved, table_id, row_id, icon_name, message);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EnableTextArea") == 0) {
		const gchar *area_id;
		gboolean enable;

		g_variant_get (parameters, "(&sb)", &area_id, &enable);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_enable_text_area (
			document_saved, area_id, enable);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "TextAreaSetValue") == 0) {
		const gchar *area_id, *value;

		g_variant_get (parameters, "(&s&s)", &area_id, &value);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_text_area_set_value (
			document_saved, area_id, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "TextAreaGetValue") == 0) {
		const gchar *area_id;
		gchar *value;

		g_variant_get (parameters, "(&s)", &area_id);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		value = module_itip_formatter_dom_utils_text_area_get_value (
				document_saved, area_id);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "RebuildSourceList") == 0) {
		const gchar *optgroup_id, *optgroup_label, *option_id, *option_label;
		gboolean writable;

		g_variant_get (
			parameters,
			"(&s&s&s&sb)",
			&optgroup_id, &optgroup_label, &option_id, &option_label, &writable);

		/* FIXME return error */
		if (!document_saved)
			g_dbus_method_invocation_return_value (invocation, NULL);

		module_itip_formatter_dom_utils_rebuild_source_list (
			document_saved,
			optgroup_id,
			optgroup_label,
			option_id,
			option_label,
			writable);

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
