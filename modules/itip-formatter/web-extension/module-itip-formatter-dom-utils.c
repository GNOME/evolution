/*
 * module-itip-formatter-dom-utils.c
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

#include "module-itip-formatter-dom-utils.h"

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMHTMLElementUnstable.h>

#include "module-itip-formatter-web-extension.h"
#include "../itip-view-elements-defines.h"

#include <e-util/e-util.h>

#define ITIP_WEB_EXTENSION_PAGE_ID_KEY "itip-web-extension-page-id"
#define ITIP_WEB_EXTENSION_PART_ID_KEY "itip-web-extension-part-id"

static void
recur_toggled_cb (WebKitDOMHTMLInputElement *input,
                  WebKitDOMEvent *event,
                  GDBusConnection *connection)
{
	guint64 *ppage_id;
	const gchar *part_id;
	GError *error = NULL;

	ppage_id = g_object_get_data (G_OBJECT (input), ITIP_WEB_EXTENSION_PAGE_ID_KEY);
	part_id = g_object_get_data (G_OBJECT (input), ITIP_WEB_EXTENSION_PART_ID_KEY);
	if (!ppage_id || !part_id) {
		g_warning ("%s: page_id/part_id not set on %p", G_STRFUNC, input);
		return;
	}

	g_dbus_connection_emit_signal (
		connection,
		NULL,
		MODULE_ITIP_FORMATTER_WEB_EXTENSION_OBJECT_PATH,
		MODULE_ITIP_FORMATTER_WEB_EXTENSION_INTERFACE,
		"RecurToggled",
		g_variant_new ("(ts)", *ppage_id, part_id),
		&error);

	if (error) {
		g_warning ("Error emitting signal RecurToggled: %s\n", error->message);
		g_error_free (error);
	}
}

static void
source_changed_cb (WebKitDOMElement *element,
                   WebKitDOMEvent *event,
                   GDBusConnection *connection)
{
	guint64 *ppage_id;
	const gchar *part_id;
	GError *error = NULL;

	ppage_id = g_object_get_data (G_OBJECT (element), ITIP_WEB_EXTENSION_PAGE_ID_KEY);
	part_id = g_object_get_data (G_OBJECT (element), ITIP_WEB_EXTENSION_PART_ID_KEY);
	if (!ppage_id || !part_id) {
		g_warning ("%s: page_id/part_id not set on %p", G_STRFUNC, element);
		return;
	}

	g_dbus_connection_emit_signal (
		connection,
		NULL,
		MODULE_ITIP_FORMATTER_WEB_EXTENSION_OBJECT_PATH,
		MODULE_ITIP_FORMATTER_WEB_EXTENSION_INTERFACE,
		"SourceChanged",
		g_variant_new ("(ts)", *ppage_id, part_id),
		&error);

	if (error) {
		g_warning ("Error emitting signal SourceChanged: %s\n", error->message);
		g_error_free (error);
	}
}

static void
rsvp_toggled_cb (WebKitDOMHTMLInputElement *input,
                 WebKitDOMEvent *event,
                 GDBusConnection *connection)
{
	WebKitDOMElement *el;
	WebKitDOMDocument *document;
	gboolean rsvp;

       	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (input));
	rsvp = webkit_dom_html_input_element_get_checked (input);
	el = webkit_dom_document_get_element_by_id (
		document, TEXTAREA_RSVP_COMMENT);
	webkit_dom_html_text_area_element_set_disabled (
		WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (el), !rsvp);
}

/**
  alarm_check_toggled_cb
  check1 was changed, so make the second available based on state of the first check.
*/
static void
alarm_check_toggled_cb (WebKitDOMHTMLInputElement *check1,
                        WebKitDOMEvent *event,
                        GDBusConnection *connection)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *check2;
	gchar *id;

       	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (check1));
#if WEBKIT_CHECK_VERSION(2,2,0) /* XXX should really be (2,1,something) */
	id = webkit_dom_element_get_id (WEBKIT_DOM_ELEMENT (check1));
#else
	id = webkit_dom_html_element_get_id (WEBKIT_DOM_HTML_ELEMENT (check1));
#endif

	if (g_strcmp0 (id, CHECKBOX_INHERIT_ALARM)) {
		check2 = webkit_dom_document_get_element_by_id (
			document, CHECKBOX_KEEP_ALARM);
	} else {
		check2 = webkit_dom_document_get_element_by_id (
			document, CHECKBOX_INHERIT_ALARM);
	}

	g_free (id);

	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (check2),
		(webkit_dom_html_element_get_hidden (
				WEBKIT_DOM_HTML_ELEMENT (check1)) &&
			webkit_dom_html_input_element_get_checked (check1)));
}

void
module_itip_formatter_dom_utils_create_dom_bindings (WebKitDOMDocument *document,
						     guint64 page_id,
						     const gchar *part_id,
                                                     GDBusConnection *connection)
{
	WebKitDOMElement *el;

	g_return_if_fail (part_id && *part_id);

	el = webkit_dom_document_get_element_by_id (document, CHECKBOX_RECUR);
	if (el) {
		guint64 *ppage_id;

		ppage_id = g_new0 (guint64, 1);
		*ppage_id = page_id;

		g_object_set_data_full (G_OBJECT (el), ITIP_WEB_EXTENSION_PAGE_ID_KEY, ppage_id, g_free);
		g_object_set_data_full (G_OBJECT (el), ITIP_WEB_EXTENSION_PART_ID_KEY, g_strdup (part_id), g_free);

		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (recur_toggled_cb), FALSE, connection);
	}

	el = webkit_dom_document_get_element_by_id (document, SELECT_ESOURCE);
	if (el) {
		guint64 *ppage_id;

		ppage_id = g_new0 (guint64, 1);
		*ppage_id = page_id;

		g_object_set_data_full (G_OBJECT (el), ITIP_WEB_EXTENSION_PAGE_ID_KEY, ppage_id, g_free);
		g_object_set_data_full (G_OBJECT (el), ITIP_WEB_EXTENSION_PART_ID_KEY, g_strdup (part_id), g_free);

		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "change",
			G_CALLBACK (source_changed_cb), FALSE, connection);
	}

	el = webkit_dom_document_get_element_by_id (document, CHECKBOX_RSVP);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (rsvp_toggled_cb), FALSE, connection);
	}

	el = webkit_dom_document_get_element_by_id (document, CHECKBOX_INHERIT_ALARM);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (alarm_check_toggled_cb), FALSE, connection);
	}

	el = webkit_dom_document_get_element_by_id (document, CHECKBOX_KEEP_ALARM);
	if (el) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (el), "click",
			G_CALLBACK (alarm_check_toggled_cb), FALSE, connection);
	}
}

void
module_itip_formatter_dom_utils_show_button (WebKitDOMDocument *document,
                                             const gchar *button_id)
{
	WebKitDOMElement *button;

	button = webkit_dom_document_get_element_by_id (document, button_id);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (button), FALSE);
}

void
module_itip_formatter_dom_utils_enable_button (WebKitDOMDocument *document,
                                               const gchar *button_id,
                                               gboolean enable)
{
	WebKitDOMElement *el;

	el = webkit_dom_document_get_element_by_id (document, button_id);
	webkit_dom_html_button_element_set_disabled (
		WEBKIT_DOM_HTML_BUTTON_ELEMENT (el), !enable);
}

gboolean
module_itip_formatter_dom_utils_input_is_checked (WebKitDOMDocument *document,
                                                  const gchar *input_id)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (document, input_id);

	if (!element)
		return FALSE;

	return webkit_dom_html_input_element_get_checked (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (element));
}

void
module_itip_formatter_dom_utils_input_set_checked (WebKitDOMDocument *document,
                                                   const gchar *input_id,
                                                   gboolean checked)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (document, input_id);

	if (!element)
		return;

	webkit_dom_html_input_element_set_checked (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (element), checked);
}

void
module_itip_formatter_dom_utils_show_checkbox (WebKitDOMDocument *document,
                                               const gchar *id,
                                               gboolean show,
					       gboolean update_second)
{
	WebKitDOMElement *label;
	WebKitDOMElement *el;
	gchar *row_id;

	el = webkit_dom_document_get_element_by_id (document, id);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (el), !show);

	label = webkit_dom_element_get_next_element_sibling (el);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (label), !show);

	if (!show) {
		webkit_dom_html_input_element_set_checked (
			WEBKIT_DOM_HTML_INPUT_ELEMENT (el), FALSE);
	}

	if (update_second) {
		/* and update state of the second check */
		alarm_check_toggled_cb (
			WEBKIT_DOM_HTML_INPUT_ELEMENT (el),
			NULL, NULL);
	}

	row_id = g_strconcat ("table_row_", id, NULL);
	el = webkit_dom_document_get_element_by_id (document, row_id);
	webkit_dom_html_element_set_hidden (WEBKIT_DOM_HTML_ELEMENT (el), !show);
	g_free (row_id);
}

void
module_itip_formatter_dom_utils_set_buttons_sensitive (WebKitDOMDocument *document,
                                                       gboolean sensitive)
{
	WebKitDOMElement *el, *cell;

	el = webkit_dom_document_get_element_by_id (
		document, CHECKBOX_UPDATE);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		document, CHECKBOX_RECUR);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		document, CHECKBOX_FREE_TIME);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		document, CHECKBOX_KEEP_ALARM);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		document, CHECKBOX_INHERIT_ALARM);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		document, CHECKBOX_RSVP);
	webkit_dom_html_input_element_set_disabled (
		WEBKIT_DOM_HTML_INPUT_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		document, TEXTAREA_RSVP_COMMENT);
	webkit_dom_html_text_area_element_set_disabled (
		WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (el), !sensitive);

	el = webkit_dom_document_get_element_by_id (
		document, TABLE_ROW_BUTTONS);
	cell = webkit_dom_element_get_first_element_child (el);
	do {
		WebKitDOMElement *btn;
		btn = webkit_dom_element_get_first_element_child (cell);
		if (!webkit_dom_html_element_get_hidden (
			WEBKIT_DOM_HTML_ELEMENT (btn))) {
			webkit_dom_html_button_element_set_disabled (
				WEBKIT_DOM_HTML_BUTTON_ELEMENT (btn), !sensitive);
		}
	} while ((cell = webkit_dom_element_get_next_element_sibling (cell)) != NULL);
}

void
module_itip_formatter_dom_utils_set_area_text (WebKitDOMDocument *document,
                                               const gchar *area_id,
                                               const gchar *text)
{
	WebKitDOMElement *row, *col;

	row = webkit_dom_document_get_element_by_id (document, area_id);
	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (row), (g_strcmp0 (text, "") == 0));

	col = webkit_dom_element_get_last_element_child (row);
	webkit_dom_element_set_inner_html (col, text, NULL);
}

void
module_itip_formatter_dom_utils_element_set_access_key (WebKitDOMDocument *document,
                                                        const gchar *element_id,
                                                        const gchar *access_key)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (document, element_id);

	if (!element)
		return;

	webkit_dom_html_element_set_access_key (
		WEBKIT_DOM_HTML_ELEMENT (element), access_key);
}

void
module_itip_formatter_dom_utils_element_hide_child_nodes (WebKitDOMDocument *document,
                                                          const gchar *element_id)
{
	WebKitDOMElement *element, *cell, *button;

	element = webkit_dom_document_get_element_by_id (document, element_id);

	if (!element)
		return;

	element = webkit_dom_document_get_element_by_id (document, element_id);
	cell = webkit_dom_element_get_first_element_child (element);
	do {
		button = webkit_dom_element_get_first_element_child (cell);
		webkit_dom_html_element_set_hidden (
			WEBKIT_DOM_HTML_ELEMENT (button), TRUE);
	} while ((cell = webkit_dom_element_get_next_element_sibling (cell)) != NULL);
}

void
module_itip_formatter_dom_utils_enable_select (WebKitDOMDocument *document,
                                               const gchar *select_id,
                                               gboolean enabled)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (document, select_id);

	if (!element)
		return;

	webkit_dom_html_select_element_set_disabled (
		WEBKIT_DOM_HTML_SELECT_ELEMENT (element), !enabled);
}

gboolean
module_itip_formatter_dom_utils_select_is_enabled (WebKitDOMDocument *document,
                                                   const gchar *select_id)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (document, select_id);

	if (!element)
		return FALSE;

	return !webkit_dom_html_select_element_get_disabled (
		WEBKIT_DOM_HTML_SELECT_ELEMENT (element));
}

gchar *
module_itip_formatter_dom_utils_select_get_value (WebKitDOMDocument *document,
                                                  const gchar *select_id)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (document, select_id);

	if (!element)
		return NULL;

	return webkit_dom_html_select_element_get_value (
		WEBKIT_DOM_HTML_SELECT_ELEMENT (element));
}

void
module_itip_formatter_dom_utils_select_set_selected (WebKitDOMDocument *document,
                                                     const gchar *select_id,
                                                     const gchar *option)
{
	WebKitDOMElement *element;
	gint length, ii;

	element = webkit_dom_document_get_element_by_id (document, select_id);

	if (!element)
		return;

	length = webkit_dom_html_select_element_get_length (
		WEBKIT_DOM_HTML_SELECT_ELEMENT (element));
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;
		WebKitDOMHTMLOptionElement *option_element;
		gchar *value;

		node = webkit_dom_html_select_element_item (
			WEBKIT_DOM_HTML_SELECT_ELEMENT (element), ii);
		option_element = WEBKIT_DOM_HTML_OPTION_ELEMENT (node);

		value = webkit_dom_html_option_element_get_value (option_element);
		if (g_strcmp0 (value, option) == 0) {
			webkit_dom_html_option_element_set_selected (
				option_element, TRUE);

			g_free (value);
			break;
		}

		g_free (value);
	}
}

void
module_itip_formatter_dom_utils_update_times (WebKitDOMDocument *document,
                                              const gchar *element_id,
                                              const gchar *header,
                                              const gchar *label)
{
	WebKitDOMElement *element, *col;

	element = webkit_dom_document_get_element_by_id (document, element_id);

	if (!element)
		return;

	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (element), FALSE);

	col = webkit_dom_element_get_first_element_child (element);
	webkit_dom_element_set_inner_html (col, header, NULL);

	col = webkit_dom_element_get_last_element_child (element);
	webkit_dom_element_set_inner_html (col, label, NULL);
}

void
module_itip_formatter_dom_utils_append_info_item_row (WebKitDOMDocument *document,
                                                      const gchar *table_id,
                                                      const gchar *row_id,
                                                      const gchar *icon_name,
                                                      const gchar *message)
{
	WebKitDOMElement *table;
        WebKitDOMHTMLElement *cell, *row;

	table = webkit_dom_document_get_element_by_id (document, table_id);

	if (!table)
		return;

	table = webkit_dom_document_get_element_by_id (document, table_id);
	row = webkit_dom_html_table_element_insert_row (
		WEBKIT_DOM_HTML_TABLE_ELEMENT (table), -1, NULL);

	webkit_dom_element_set_id (WEBKIT_DOM_ELEMENT (row), row_id);

	cell = webkit_dom_html_table_row_element_insert_cell (
		WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row), -1, NULL);

	if (icon_name) {
		WebKitDOMElement *image;
		gchar *icon_uri;

		image = webkit_dom_document_create_element (
			document, "IMG", NULL);

		icon_uri = g_strdup_printf ("gtk-stock://%s", icon_name);
		webkit_dom_html_image_element_set_src (
			WEBKIT_DOM_HTML_IMAGE_ELEMENT (image), icon_uri);
		g_free (icon_uri);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (cell),
			WEBKIT_DOM_NODE (image),
			NULL);
	}

	cell = webkit_dom_html_table_row_element_insert_cell (
		WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row), -1, NULL);

	webkit_dom_element_set_inner_html (WEBKIT_DOM_ELEMENT (cell), message, NULL);
}

void
module_itip_formatter_dom_utils_enable_text_area (WebKitDOMDocument *document,
                                                  const gchar *area_id,
                                                  gboolean enable)
{
	WebKitDOMElement *el;

	el = webkit_dom_document_get_element_by_id (document, area_id);
	webkit_dom_html_text_area_element_set_disabled (
		WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (el), !enable);
}

void
module_itip_formatter_dom_utils_text_area_set_value (WebKitDOMDocument *document,
                                                     const gchar *area_id,
                                                     const gchar *value)
{
	WebKitDOMElement *el;

	el = webkit_dom_document_get_element_by_id (document, area_id);
	webkit_dom_html_text_area_element_set_value (
		WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (el), value);
}

gchar *
module_itip_formatter_dom_utils_text_area_get_value (WebKitDOMDocument *document,
                                                     const gchar *area_id)
{
	WebKitDOMElement *el;

	el = webkit_dom_document_get_element_by_id (document, area_id);
	return webkit_dom_html_text_area_element_get_value (
		WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (el));
}

void
module_itip_formatter_dom_utils_rebuild_source_list (WebKitDOMDocument *document,
                                                     const gchar *optgroup_id,
                                                     const gchar *optgroup_label,
                                                     const gchar *option_id,
                                                     const gchar *option_label,
						     gboolean writable)
{
	WebKitDOMElement *option;
	WebKitDOMElement *select;
	WebKitDOMHTMLOptGroupElement *optgroup;

	select = webkit_dom_document_get_element_by_id (document, SELECT_ESOURCE);

	if (!select)
		return;

	optgroup = WEBKIT_DOM_HTML_OPT_GROUP_ELEMENT (
			webkit_dom_document_get_element_by_id (
				document, optgroup_id));

	if (!optgroup) {
		optgroup = WEBKIT_DOM_HTML_OPT_GROUP_ELEMENT (
				webkit_dom_document_create_element (
					document, "OPTGROUP", NULL));
		webkit_dom_html_opt_group_element_set_label (
			optgroup, optgroup_label);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (select), WEBKIT_DOM_NODE (optgroup), NULL);
	}

	option = webkit_dom_document_create_element (document, "OPTION", NULL);
	webkit_dom_html_option_element_set_value (
		WEBKIT_DOM_HTML_OPTION_ELEMENT (option), option_id);
	webkit_dom_html_option_element_set_label (
		WEBKIT_DOM_HTML_OPTION_ELEMENT (option), option_label);
	webkit_dom_element_set_inner_html (option, option_label, NULL);

	webkit_dom_element_set_class_name (
		WEBKIT_DOM_ELEMENT (option), "calendar");

	if (!writable) {
		webkit_dom_html_option_element_set_disabled (
			WEBKIT_DOM_HTML_OPTION_ELEMENT (option), TRUE);
	}

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (optgroup),
		WEBKIT_DOM_NODE (option),
		NULL);
}
