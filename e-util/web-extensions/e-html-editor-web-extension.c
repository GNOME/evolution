/*
 * e-html-editor-web-extension.h
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

#include "e-html-editor-web-extension.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit2/webkit-web-extension.h>

#include <string.h>

#include <e-util/e-dom-utils.h>
#include <e-util/e-html-editor-cell-dialog-dom-functions.h>
#include <e-util/e-html-editor-image-dialog-dom-functions.h>
#include <e-util/e-html-editor-link-dialog-dom-functions.h>

/* FIXME Clean it */
static GDBusConnection *dbus_connection;

/* These properties show the actual state of EHTMLEditorView */
static EHTMLEditorSelectionAlignment alignment;
static gchar *background_color = NULL;
static EHTMLEditorSelectionBlockFormat block_format;
static gboolean bold = FALSE;
/* FIXME XXX WK2
static GdkRGBA *font_color = NULL; */
static gchar *font_color = NULL;
static gchar *font_name = NULL;
static gint font_size;
static gboolean indented = FALSE;
static gboolean italic = FALSE;
static gboolean monospaced = FALSE;
static gboolean strikethrough = FALSE;
static gboolean subscript = FALSE;
static gboolean superscript = FALSE;
/* FIXME XXX WK2 is it needed?
static gchar *text = NULL; */
static gboolean underline = FALSE;

static const char introspection_xml[] =
"<node>"
"  <interface name='org.gnome.Evolution.WebExtension'>"
"<!-- ********************************************************* -->"
"<!--                       PROPERTIES                          -->"
"<!-- ********************************************************* -->"
"    <property type='b' name='ForceImageLoad' access='readwrite'/>"
"<!-- ********************************************************* -->"
"<!-- These properties show the actual state of EHTMLEditorView -->"
"<!-- ********************************************************* -->"
"    <property type='b' name='Alignment' access='readwrite'/>"
"    <property type='s' name='BackgroundColor' access='readwrite'/>"
"    <property type='u' name='BlockFormat' access='readwrite'/>"
"    <property type='b' name='Bold' access='readwrite'/>"
"    <property type='s' name='FontColor' access='readwrite'/>"
"    <property type='s' name='FontName' access='readwrite'/>"
"    <property type='u' name='FontSize' access='readwrite'/>"
"    <property type='b' name='Indented' access='readwrite'/>"
"    <property type='b' name='Italic' access='readwrite'/>"
"    <property type='b' name='Monospaced' access='readwrite'/>"
"    <property type='b' name='Strikethrough' access='readwrite'/>"
"    <property type='b' name='Subscript' access='readwrite'/>"
"    <property type='b' name='Superscript' access='readwrite'/>"
"    <property type='b' name='Underline' access='readwrite'/>"
"<!-- ********************************************************* -->"
"<!--                          METHODS                          -->"
"<!-- ********************************************************* -->"
"<!-- ********************************************************* -->"
"<!--                          GENERIC                          -->"
"<!-- ********************************************************* -->"
"    <method name='ElementHasAttribute'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='attribute' direction='in'/>"
"      <arg type='b' name='has_attribute' direction='out'/>"
"    </method>"
"    <method name='ElementGetAttribute'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='attribute' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='ElementRemoveAttribute'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='attribute' direction='in'/>"
"    </method>"
"    <method name='ElementSetAttribute'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='attribute' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='ElementGetTagName'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='tag_name' direction='out'/>"
"    </method>"
"    <method name='TableCellElementGetAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='align' direction='out'/>"
"    </method>"
"    <method name='TableCellElementGetVAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='v_align' direction='out'/>"
"    </method>"
"    <method name='TableCellElementGetNoWrap'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='no_wrap' direction='out'/>"
"    </method>"
"    <method name='TableCellElementGetWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='width' direction='out'/>"
"    </method>"
"    <method name='TableCellElementGetRowSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='row_span' direction='out'/>"
"    </method>"
"    <method name='TableCellElementGetColSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='col_span' direction='out'/>"
"    </method>"
"    <method name='TableCellElementGetBgColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='color' direction='out'/>"
"    </method>"
"    <method name='ImageElementSetAlt'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='ImageElementGetAlt'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementSetWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"    </method>"
"    <method name='ImageElementGetWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementSetHeight'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"    </method>"
"    <method name='ImageElementGetHeight'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementGetNaturalWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementGetNaturalHeight'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementSetAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='ImageElementSetHSpace'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"    </method>"
"    <method name='ImageElementGetHSpace'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementSetVSpace'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"    </method>"
"    <method name='ImageElementGetVSpace'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementSetBorder'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='ImageElementGetBorder'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='BodySetTextColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='BodyGetTextColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='BodySetLinkColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='BodyGetLinkColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='BodySetBgColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='BodyGetBgColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='BodyGetBackground'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='HRElementSetAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='HRElementGetAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='HRElementSetSize'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='HRElementGetSize'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='HRElementSetWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='HRElementGetWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='HRElementSetNoShade'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"    </method>"
"    <method name='HRElementGetNoShade'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='value' direction='out'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorCellDialog      -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorCellDialogMarkCurrentCellElement'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementVAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementNoWrap'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementHeaderStyle'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    <method name='EHTMLEditorCellDialogSetElementColSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementRowSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementBgColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorImageDialog     -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorImageDialogSetElementUrl'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorImageDialogGetElementUrl'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorLinkDialog      -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorLinkDialogOk'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='url' direction='in'/>"
"      <arg type='s' name='inner_text' direction='in'/>"
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

	if (g_strcmp0 (interface_name, EVOLUTION_WEB_EXTENSION_INTERFACE) != 0)
		return;

	if (g_strcmp0 (method_name, "ElementHasAttribute") == 0) {
		gboolean value = FALSE;
		const gchar *element_id, *attribute;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &attribute);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_element_has_attribute (element, attribute);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", value));
	} else if (g_strcmp0 (method_name, "ElementGetAttribute") == 0) {
		const gchar *element_id, *attribute;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &attribute);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_element_get_attribute (element, attribute);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "ElementRemoveAttribute") == 0) {
		const gchar *element_id, *attribute;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &attribute);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_element_remove_attribute (element, attribute);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementSetAttribute") == 0) {
		const gchar *element_id, *attribute, *value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters,
			"(t&s&s&s)",
			&page_id, &element_id, &attribute, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_element_set_attribute (
				element, attribute, value, NULL);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementGetTagName") == 0) {
		const gchar *element_id;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_element_get_tag_name (element);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "TableCellElementGetAlign") == 0) {
		const gchar *element_id;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_align (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "TableCellElementGetVAlign") == 0) {
		const gchar *element_id;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_v_align (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "TableCellElementGetNoWrap") == 0) {
		const gchar *element_id;
		gboolean value = FALSE;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_no_wrap (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_boolean (value));
	} else if (g_strcmp0 (method_name, "TableCellElementGetWidth") == 0) {
		const gchar *element_id;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_width (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "TableCellElementGetRowSpan") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_row_span (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "TableCellElementGetColSpan") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_col_span (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "TableCellElementGetBgColor") == 0) {
		const gchar *element_id;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_bg_color (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "ImageElementGetAlt") == 0) {
		const gchar *element_id, *value;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_alt (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "ImageElementSetAlt") == 0) {
		const gchar *element_id, *value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_image_element_set_alt (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ImageElementSetWidth") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_image_element_set_width (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ImageElementGetWidth") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_width (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "ImageElementSetHeight") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_image_element_set_width (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ImageElementGetHeight") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_height (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "ImageElementGetNaturalWidth") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_natural_width (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "ImageElementGetNaturalHeight") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_natural_height (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "ImageElementSetAlign") == 0) {
		const gchar *element_id, *value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&ss)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_image_element_set_align (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ImageElementSetHSpace") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_image_element_set_hspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ImageElementGetHSpace") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_hspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value);
	} else if (g_strcmp0 (method_name, "ImageElementSetVSpace") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_image_element_set_vspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ImageElementGetVSpace") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_vspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value);
	} else if (g_strcmp0 (method_name, "ImageElementSetBorder") == 0) {
		const gchar *element_id, *value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_image_element_set_border (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ImageElementGetBorder") == 0) {
		const gchar *element_id;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_border (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "BodySetTextColor") == 0) {
		const gchar *value;

		g_variant_get (
			parameters, "(t&s)", &page_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		webkit_dom_html_body_element_set_text (
			webkit_dom_document_get_body (document), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "BodyGetTextColor") == 0) {
		gchar *value = NULL;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = webkit_dom_html_body_element_get_text (
			webkit_dom_document_get_body (document));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "BodySetLinkColor") == 0) {
		const gchar *value;

		g_variant_get (
			parameters, "(t&s)", &page_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		webkit_dom_html_body_element_set_link (
			webkit_dom_document_get_body (document), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "BodyGetLinkColor") == 0) {
		gchar *value = NULL;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = webkit_dom_html_body_element_get_link (
			webkit_dom_document_get_body (document));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "BodySetBgColor") == 0) {
		const gchar *value;

		g_variant_get (
			parameters, "(t&s)", &page_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		webkit_dom_html_body_element_set_bg_color (
			webkit_dom_document_get_body (document), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "BodyGetBgColor") == 0) {
		gchar *value = NULL;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = webkit_dom_html_body_element_get_bg_color (
			webkit_dom_document_get_body (document));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "BodyGetBackground") == 0) {
		gchar *value = NULL;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = webkit_dom_html_body_element_get_background (
			webkit_dom_document_get_body (document));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "HRElementSetAlign") == 0) {
		const gchar *element_id, *value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_hr_element_set_align (
				WEBKIT_DOM_HTML_HR_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "HRElementGetAlign") == 0) {
		const gchar *element_id;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_hr_element_get_align (
				WEBKIT_DOM_HTML_HR_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "HRElementSetSize") == 0) {
		const gchar *element_id, *value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_hr_element_set_size (
				WEBKIT_DOM_HTML_HR_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "HRElementGetSize") == 0) {
		const gchar *element_id;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_hr_element_get_size (
				WEBKIT_DOM_HTML_HR_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "HRElementSetWidth") == 0) {
		const gchar *element_id, *value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_hr_element_set_width (
				WEBKIT_DOM_HTML_HR_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "HRElementGetSize") == 0) {
		const gchar *element_id;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_hr_element_get_width (
				WEBKIT_DOM_HTML_HR_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "HRElementSetNoShade") == 0) {
		gboolean value = FALSE;
		const gchar *element_id;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&sb)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_hr_element_set_no_shade (
				WEBKIT_DOM_HTML_HR_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "HRElementGetNoShade") == 0) {
		gboolean *value = FALSE;
		const gchar *element_id;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_hr_element_get_no_shade (
				WEBKIT_DOM_HTML_HR_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_boolean (value));
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogMarkCurrentCellElement") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_mark_current_cell_element (document, element_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementVAlign") == 0) {
		const gchar *value;
		guint scope;

		g_variant_get (parameters, "(t&su)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_v_align (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementAlign") == 0) {
		const gchar *value;
		guint scope;

		g_variant_get (parameters, "(t&su)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_align (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementNoWrap") == 0) {
		gboolean value;
		guint scope;

		g_variant_get (parameters, "(tbu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_no_wrap (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementHeaderStyle") == 0) {
		gboolean value;
		guint scope;

		g_variant_get (parameters, "(tbu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_header_style (
			document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementWidth") == 0) {
		const gchar *value;
		guint scope;

		g_variant_get (parameters, "(t&su)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_width (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementColSpan") == 0) {
		glong value;
		guint scope;

		g_variant_get (parameters, "(tiu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_col_span (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementRowSpan") == 0) {
		glong value;
		guint scope;

		g_variant_get (parameters, "(tiu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_row_span (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementBgColor") == 0) {
		const gchar *value;
		guint scope;

		g_variant_get (parameters, "(t&su)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_bg_color (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorImageDialogSetElementUrl") == 0) {
		const gchar *value;
		guint scope;

		g_variant_get (parameters, "(t&s)", &page_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_image_dialog_set_element_url (document, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorImageDialogGetElementUrl") == 0) {
		gchar *value;
		guint scope;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = e_html_editor_image_dialog_get_element_url (document);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorLinkDialogOk") == 0) {
		const gchar *url, *inner_text;

		g_variant_get (parameters, "(t&s&s)", &page_id, &url, &inner_text);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_link_dialog_ok (document, url, inner_text);

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
	GVariant *variant;

	if (g_strcmp0 (property_name, "ForceImageLoad") == 0)
		variant = g_variant_new_boolean (force_image_load);
	else if (g_strcmp0 (property_name, "Alignment") == 0)
		variant = g_variant_new_int32 (alignment);
	else if (g_strcmp0 (property_name, "BackgroundColor") == 0)
		variant = g_variant_new_string (background_color);
	else if (g_strcmp0 (property_name, "BlockFormat") == 0)
		variant = g_variant_new_int32 (block_format);
	else if (g_strcmp0 (property_name, "Bold") == 0)
		variant = g_variant_new_boolean (bold);
	else if (g_strcmp0 (property_name, "FontColor") == 0)
		variant = g_variant_new_string (font_color);
	else if (g_strcmp0 (property_name, "FontName") == 0)
		variant = g_variant_new_string (font_name);
	else if (g_strcmp0 (property_name, "FontSize") == 0)
		variant = g_variant_new_int32 (font_size);
	else if (g_strcmp0 (property_name, "Indented") == 0)
		variant = g_variant_new_boolean (indented);
	else if (g_strcmp0 (property_name, "Italic") == 0)
		variant = g_variant_new_boolean (italic);
	else if (g_strcmp0 (property_name, "Monospaced") == 0)
		variant = g_variant_new_boolean (monospaced);
	else if (g_strcmp0 (property_name, "Strikethrough") == 0)
		variant = g_variant_new_boolean (strikethrough);
	else if (g_strcmp0 (property_name, "Subscript") == 0)
		variant = g_variant_new_boolean (subscript);
	else if (g_strcmp0 (property_name, "Superscript") == 0)
		variant = g_variant_new_boolean (superscript);
	else if (g_strcmp0 (property_name, "Superscript") == 0)
		variant = g_variant_new_boolean (superscript);
	else if (g_strcmp0 (property_name, "Underline") == 0)
		variant = g_variant_new_boolean (underline);

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
	GError *local_error = NULL;
	GVariantBuilder *builder;

	builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);

	if (g_strcmp0 (property_name, "ForceImageLoad") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == force_image_load)
			goto exit;

		force_image_load = value;

		g_variant_builder_add (builder,
			"{sv}",
			"ForceImageLoad",
			g_variant_new_boolean (force_image_load));
	} else if (g_strcmp0 (property_name, "Alignment") == 0) {
		gint32 value = g_variant_get_int32 (variant);

		if (value == alignment)
			goto exit;

		alignment = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Alignment",
			g_variant_new_int32 (alignment));
	} else if (g_strcmp0 (property_name, "BackgroundColor") == 0) {
		const gchar *value = g_variant_get_string (variant);

		if (g_strcmp0 (value, background_color) != 0)
			goto exit;

		g_free (background_color);
		background_color = g_strdup (value);

		g_variant_builder_add (builder,
			"{sv}",
			"BackgroundColor",
			g_variant_new_string (background_color));
	} else if (g_strcmp0 (property_name, "BlockFormat") == 0) {
		gint32 value = g_variant_get_int32 (variant);

		if (value == block_format)
			goto exit;

		block_format = value;

		g_variant_builder_add (builder,
			"{sv}",
			"BlockFormat",
			g_variant_new_int32 (block_format));
	} else if (g_strcmp0 (property_name, "Bold") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == bold)
			goto exit;

		bold = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Bold",
			g_variant_new_boolean (bold));
	} else if (g_strcmp0 (property_name, "FontColor") == 0) {
		const gchar *value = g_variant_get_string (variant);

		if (g_strcmp0 (value, font_color) != 0)
			goto exit;

		g_free (font_color);
		font_color = g_strdup (value);

		g_variant_builder_add (builder,
			"{sv}",
			"FontColor",
			g_variant_new_string (font_color));
	} else if (g_strcmp0 (property_name, "FontName") == 0) {
		const gchar *value = g_variant_get_string (variant);

		if (g_strcmp0 (value, font_name) != 0)
			goto exit;

		g_free (font_name);
		font_name = g_strdup (value);

		g_variant_builder_add (builder,
			"{sv}",
			"FontName",
			g_variant_new_string (font_name));
	} else if (g_strcmp0 (property_name, "FontSize") == 0) {
		gint32 value = g_variant_get_int32 (variant);

		if (value == font_size)
			goto exit;

		font_size = value;

		g_variant_builder_add (builder,
			"{sv}",
			"FontSize",
			g_variant_new_int32 (font_size));
	} else if (g_strcmp0 (property_name, "Indented") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == indented)
			goto exit;

		indented = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Indented",
			g_variant_new_boolean (indented));
	} else if (g_strcmp0 (property_name, "Italic") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == italic)
			goto exit;

		italic = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Italic",
			g_variant_new_boolean (italic));
	} else if (g_strcmp0 (property_name, "Monospaced") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == monospaced)
			goto exit;

		monospaced = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Monospaced",
			g_variant_new_boolean (monospaced));
	} else if (g_strcmp0 (property_name, "Strikethrough") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == strikethrough)
			goto exit;

		strikethrough = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Strikethrough",
			g_variant_new_boolean (strikethrough));
	} else if (g_strcmp0 (property_name, "Subscript") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == subscript)
			goto exit;

		subscript = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Subscript",
			g_variant_new_boolean (subscript));
	} else if (g_strcmp0 (property_name, "Superscript") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == superscript)
			goto exit;

		superscript = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Superscript",
			g_variant_new_boolean (superscript));
	} else if (g_strcmp0 (property_name, "Underline") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == underline)
			goto exit;

		underline = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Undeline",
			g_variant_new_boolean (underline));
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
	g_bus_own_name (
		G_BUS_TYPE_SESSION,
		EVOLUTION_WEB_EXTENSION_SERVICE_NAME,
		G_BUS_NAME_OWNER_FLAGS_NONE,
		bus_acquired_cb,
		NULL, NULL,
		g_object_ref (extension),
		g_object_unref);
}
