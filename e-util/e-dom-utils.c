/*
 * e-dom-utils.c
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

#include "e-dom-utils.h"

#include "../web-extensions/evolution-web-extension.h"

#include <config.h>

static void
replace_local_image_links (WebKitDOMElement *element)
{
	WebKitDOMElement *child;

	if (element == NULL)
		return;

	if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (element)) {
		WebKitDOMHTMLImageElement *img;
		gchar *src;

		img = WEBKIT_DOM_HTML_IMAGE_ELEMENT (element);
		src = webkit_dom_html_image_element_get_src (img);
		if (src && g_ascii_strncasecmp (src, "file://", 7) == 0) {
			gchar *new_src;

			/* this forms "evo-file://", which can be loaded,
			 * while "file://" cannot be, due to webkit policy */
			new_src = g_strconcat ("evo-", src, NULL);
			webkit_dom_html_image_element_set_src (img, new_src);
			g_free (new_src);
		}

		g_free (src);
	}

	if (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element)) {
		WebKitDOMDocument *content_document;

		content_document =
			webkit_dom_html_iframe_element_get_content_document (
				WEBKIT_DOM_HTML_IFRAME_ELEMENT (element));

		if (!content_document)
			return;

		replace_local_image_links (WEBKIT_DOM_ELEMENT (content_document));
	}

	child = webkit_dom_element_get_first_element_child (element);
	replace_local_image_links (child);

	do {
		element = webkit_dom_element_get_next_element_sibling (element);
		replace_local_image_links (element);
	} while (element != NULL);
}

void
e_dom_utils_replace_local_image_links (WebKitDOMDocument *document)
{
	WebKitDOMNode *node;

	for (node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (document));
	     node;
	     node = webkit_dom_node_get_next_sibling (node)) {
		if (WEBKIT_DOM_IS_ELEMENT (node))
			replace_local_image_links (WEBKIT_DOM_ELEMENT (node));
	}
}

static gboolean
document_has_selection (WebKitDOMDocument *document)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;

	dom_window = webkit_dom_document_get_default_view (document);
	if (!dom_window)
		return FALSE;

	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	if (!WEBKIT_DOM_IS_DOM_SELECTION (dom_selection))
		return FALSE;

	if (webkit_dom_dom_selection_get_range_count (dom_selection) == 0)
		return FALSE;

	if (webkit_dom_dom_selection_get_is_collapsed (dom_selection))
		return FALSE;

	return TRUE;
}

gchar *
e_dom_utils_get_document_content_html (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_document_element (document);

	return webkit_dom_html_element_get_outer_html (WEBKIT_DOM_HTML_ELEMENT (element));
}

static gchar *
get_frame_selection_html (WebKitDOMElement *iframe)
{
	WebKitDOMDocument *content_document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMNodeList *frames;
	gulong ii, length;

	content_document = webkit_dom_html_iframe_element_get_content_document (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe));

	if (!content_document)
		return NULL;

	window = webkit_dom_document_get_default_view (content_document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (selection && (webkit_dom_dom_selection_get_range_count (selection) > 0)) {
		WebKitDOMRange *range;
		WebKitDOMElement *element;
		WebKitDOMDocumentFragment *fragment;

		range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
		if (range != NULL) {
			fragment = webkit_dom_range_clone_contents (
				range, NULL);

			element = webkit_dom_document_create_element (
				content_document, "DIV", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element),
				WEBKIT_DOM_NODE (fragment), NULL);

			return webkit_dom_html_element_get_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (element));
		}
	}

	frames = webkit_dom_document_get_elements_by_tag_name (
		content_document, "IFRAME");
	length = webkit_dom_node_list_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;
		gchar *text;

		node = webkit_dom_node_list_item (frames, ii);

		text = get_frame_selection_html (
			WEBKIT_DOM_ELEMENT (node));

		if (text != NULL)
			return text;
	}

	return NULL;
}

gchar *
e_dom_utils_get_selection_content_html (WebKitDOMDocument *document)
{
	WebKitDOMNodeList *frames;
	gulong ii, length;

	if (!document_has_selection (document))
		return NULL;

	frames = webkit_dom_document_get_elements_by_tag_name (document, "IFRAME");
	length = webkit_dom_node_list_get_length (frames);

	for (ii = 0; ii < length; ii++) {
		gchar *text;
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (frames, ii);

		text = get_frame_selection_html (
			WEBKIT_DOM_ELEMENT (node));

		if (text != NULL)
			return text;
	}

	return NULL;
}

static gchar *
get_frame_selection_content_text (WebKitDOMElement *iframe)
{
	WebKitDOMDocument *content_document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMNodeList *frames;
	gulong ii, length;

	content_document = webkit_dom_html_iframe_element_get_content_document (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe));

	if (!content_document)
		return NULL;

	window = webkit_dom_document_get_default_view (content_document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (selection && (webkit_dom_dom_selection_get_range_count (selection) > 0)) {
		WebKitDOMRange *range;

		range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
		if (range != NULL)
			return webkit_dom_range_to_string (range, NULL);
	}

	frames = webkit_dom_document_get_elements_by_tag_name (
		content_document, "IFRAME");
	length = webkit_dom_node_list_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;
		gchar *text;

		node = webkit_dom_node_list_item (frames, ii);

		text = get_frame_selection_content_text (
			WEBKIT_DOM_ELEMENT (node));

		if (text != NULL)
			return text;
	}

	return NULL;
}

gchar *
e_dom_utils_get_selection_content_text (WebKitDOMDocument *document)
{
	WebKitDOMNodeList *frames;
	gulong ii, length;

	frames = webkit_dom_document_get_elements_by_tag_name (document, "IFRAME");
	length = webkit_dom_node_list_get_length (frames);

	for (ii = 0; ii < length; ii++) {
		gchar *text;
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (frames, ii);

		text = get_frame_selection_content_text (
			WEBKIT_DOM_ELEMENT (node));

		if (text != NULL)
			return text;
	}

	return NULL;
}

void
e_dom_utils_create_and_add_css_style_sheet (WebKitDOMDocument *document,
                                            const gchar *style_sheet_id)
{
	WebKitDOMElement *style_element;

	style_element = webkit_dom_document_get_element_by_id (document, style_sheet_id);

	if (!style_element) {
		/* Create new <style> element */
		style_element = webkit_dom_document_create_element (document, "style", NULL);
		webkit_dom_element_set_id (
			style_element,
			style_sheet_id);
		webkit_dom_html_style_element_set_media (
			WEBKIT_DOM_HTML_STYLE_ELEMENT (style_element),
			"screen");
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (style_element),
			/* WebKit hack - we have to insert empty TextNode into style element */
			WEBKIT_DOM_NODE (webkit_dom_document_create_text_node (document, "")),
			NULL);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (webkit_dom_document_get_head (document)),
			WEBKIT_DOM_NODE (style_element),
			NULL);
	}
}

static void
add_css_rule_into_style_sheet (WebKitDOMDocument *document,
                               const gchar *style_sheet_id,
                               const gchar *selector,
                               const gchar *style)
{
	WebKitDOMElement *style_element;
	WebKitDOMStyleSheet *sheet;
	WebKitDOMCSSRuleList *rules_list;
	gint length, ii;

	style_element = webkit_dom_document_get_element_by_id (document, style_sheet_id);

	if (!style_element) {
		e_dom_utils_create_and_add_css_style_sheet (document, style_sheet_id);
		style_element = webkit_dom_document_get_element_by_id (document, style_sheet_id);
	}

	/* Get sheet that is associated with style element */
	sheet = webkit_dom_html_style_element_get_sheet (WEBKIT_DOM_HTML_STYLE_ELEMENT (style_element));

	rules_list = webkit_dom_css_style_sheet_get_css_rules (WEBKIT_DOM_CSS_STYLE_SHEET (sheet));
	length = webkit_dom_css_rule_list_get_length (rules_list);

	/* Check if rule exists */
	for (ii = 0; ii < length; ii++) {
		WebKitDOMCSSRule *rule;
		gchar *rule_text;
		gchar *rule_selector, *selector_end;

		rule = webkit_dom_css_rule_list_item (rules_list, ii);

		if (!WEBKIT_DOM_IS_CSS_RULE (rule))
			continue;

		rule_text = webkit_dom_css_rule_get_css_text (rule);

		/* Find the start of the style => end of the selector */
		selector_end = g_strstr_len (rule_text, -1, " {");
		if (!selector_end) {
			g_free (rule_text);
			continue;
		}

		rule_selector =
			g_utf8_substring (
				rule_text,
				0,
				g_utf8_pointer_to_offset (rule_text, selector_end));

		if (g_strcmp0 (rule_selector, selector) == 0) {
			/* If exists remove it */
			webkit_dom_css_style_sheet_remove_rule (
				WEBKIT_DOM_CSS_STYLE_SHEET (sheet),
				ii, NULL);
		}

		g_free (rule_selector);
		g_free (rule_text);
	}

	/* Insert the rule at the end, so it will override previously inserted */
	webkit_dom_css_style_sheet_add_rule (
		WEBKIT_DOM_CSS_STYLE_SHEET (sheet),
		selector,
		style,
		webkit_dom_css_rule_list_get_length (
			webkit_dom_css_style_sheet_get_css_rules (
				WEBKIT_DOM_CSS_STYLE_SHEET (sheet))), /* Index */
		NULL);
}

static void
add_css_rule_into_style_sheet_recursive (WebKitDOMDocument *document,
                                         const gchar *style_sheet_id,
                                         const gchar *selector,
                                         const gchar *style)
{
	WebKitDOMNodeList *frames;
	gint ii, length;

	/* Add rule to document */
	add_css_rule_into_style_sheet (
		document,
		style_sheet_id,
		selector,
		style);

	frames = webkit_dom_document_query_selector_all (document, "iframe", NULL);
	length = webkit_dom_node_list_get_length (frames);

	/* Add rules to every sub document */
	for (ii = 0; ii < length; ii++) {
		WebKitDOMDocument *content_document = NULL;
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (frames, ii);
		content_document =
			webkit_dom_html_iframe_element_get_content_document (
				WEBKIT_DOM_HTML_IFRAME_ELEMENT (node));

		if (!content_document)
			continue;

		add_css_rule_into_style_sheet_recursive (
			content_document,
			style_sheet_id,
			selector,
			style);
	}
}

void
e_dom_utils_add_css_rule_into_style_sheet (WebKitDOMDocument *document,
                                           const gchar *style_sheet_id,
                                           const gchar *selector,
                                           const gchar *style)
{
	g_return_if_fail (style_sheet_id && *style_sheet_id);
	g_return_if_fail (selector && *selector);
	g_return_if_fail (style && *style);

	add_css_rule_into_style_sheet_recursive (
		document,
		style_sheet_id,
		selector,
		style);
}

static void
collapse_contacts_list (WebKitDOMEventTarget *event_target,
                        WebKitDOMEvent *event,
                        gpointer user_data)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *list;
	gchar *id, *list_id;
	gchar *imagesdir, *src;
	gboolean hidden;

	document = user_data;
	id = webkit_dom_element_get_id (WEBKIT_DOM_ELEMENT (event_target));

	list_id = g_strconcat ("list-", id, NULL);
	list = webkit_dom_document_get_element_by_id (document, list_id);
	g_free (id);
	g_free (list_id);

	if (list == NULL)
		return;

	imagesdir = g_filename_to_uri (EVOLUTION_IMAGESDIR, NULL, NULL);
	hidden = webkit_dom_html_element_get_hidden (WEBKIT_DOM_HTML_ELEMENT (list));

	if (hidden)
		src = g_strdup_printf ("evo-file://%s/minus.png", imagesdir);
	else
		src = g_strdup_printf ("evo-file://%s/plus.png", imagesdir);

	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (list), !hidden);
	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (event_target), src);

	g_free (src);
	g_free (imagesdir);
}

static void
toggle_headers_visibility (WebKitDOMElement *button,
                           WebKitDOMEvent *event,
                           WebKitDOMDocument *document)
{
	WebKitDOMElement *short_headers, *full_headers;
	WebKitDOMCSSStyleDeclaration *css_short, *css_full;
	gboolean expanded;
	const gchar *path;
	gchar *css_value;

	short_headers = webkit_dom_document_get_element_by_id (
		document, "__evo-short-headers");
	if (short_headers == NULL)
		return;

	css_short = webkit_dom_element_get_style (short_headers);

	full_headers = webkit_dom_document_get_element_by_id (
		document, "__evo-full-headers");
	if (full_headers == NULL)
		return;

	css_full = webkit_dom_element_get_style (full_headers);
	css_value = webkit_dom_css_style_declaration_get_property_value (
		css_full, "display");
	expanded = (g_strcmp0 (css_value, "table") == 0);
	g_free (css_value);

	webkit_dom_css_style_declaration_set_property (
		css_full, "display",
		expanded ? "none" : "table", "", NULL);
	webkit_dom_css_style_declaration_set_property (
		css_short, "display",
		expanded ? "table" : "none", "", NULL);

	if (expanded)
		path = "evo-file://" EVOLUTION_IMAGESDIR "/plus.png";
	else
		path = "evo-file://" EVOLUTION_IMAGESDIR "/minus.png";

	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (button), path);
}

static void
toggle_address_visibility (WebKitDOMElement *button,
                           WebKitDOMEvent *event,
                           GDBusConnection *connection)
{
	WebKitDOMElement *full_addr, *ellipsis;
	WebKitDOMElement *parent;
	WebKitDOMCSSStyleDeclaration *css_full, *css_ellipsis;
	const gchar *path;
	gboolean expanded;
	GError *error = NULL;

	/* <b> element */
	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (button));
	/* <td> element */
	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (parent));

	full_addr = webkit_dom_element_query_selector (parent, "#__evo-moreaddr", NULL);

	if (!full_addr)
		return;

	css_full = webkit_dom_element_get_style (full_addr);

	ellipsis = webkit_dom_element_query_selector (parent, "#__evo-moreaddr-ellipsis", NULL);

	if (!ellipsis)
		return;

	css_ellipsis = webkit_dom_element_get_style (ellipsis);

	expanded = (g_strcmp0 (
		webkit_dom_css_style_declaration_get_property_value (
		css_full, "display"), "inline") == 0);

	webkit_dom_css_style_declaration_set_property (
		css_full, "display", (expanded ? "none" : "inline"), "", NULL);
	webkit_dom_css_style_declaration_set_property (
		css_ellipsis, "display", (expanded ? "inline" : "none"), "", NULL);

	if (expanded)
		path = "evo-file://" EVOLUTION_IMAGESDIR "/plus.png";
	else
		path = "evo-file://" EVOLUTION_IMAGESDIR "/minus.png";

	if (!WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (button)) {
		button = webkit_dom_element_query_selector (parent, "#__evo-moreaddr-img", NULL);

		if (!button)
			return;
	}

	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (button), path);

	g_dbus_connection_emit_signal (
		connection,
		NULL,
		EVOLUTION_WEB_EXTENSION_OBJECT_PATH,
		EVOLUTION_WEB_EXTENSION_INTERFACE,
		"HeadersCollapsed",
		g_variant_new ("(b)", expanded),
		&error);

	if (error) {
		g_warning ("Error emitting signal HeadersCollapsed: %s\n", error->message);
		g_error_free (error);
	}
}

static void
e_dom_utils_bind_dom (WebKitDOMDocument *document,
                      const gchar *selector,
                      const gchar *event,
                      gpointer callback,
                      gpointer user_data)
{
	WebKitDOMNodeList *nodes;
	gulong ii, length;

	nodes = webkit_dom_document_query_selector_all (
			document, selector, NULL);

	length = webkit_dom_node_list_get_length (nodes);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (nodes, ii);
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (node), event,
			G_CALLBACK (callback), FALSE, user_data);
	}
}

static void
e_dom_utils_bind_elements_recursively (WebKitDOMDocument *document,
                                       const gchar *selector,
                                       const gchar *event,
                                       gpointer callback,
                                       gpointer user_data)
{
	WebKitDOMNodeList *nodes;
	gulong ii, length;

	nodes = webkit_dom_document_query_selector_all (
			document, selector, NULL);

	length = webkit_dom_node_list_get_length (nodes);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (nodes, ii);
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (node), event,
			G_CALLBACK (callback), FALSE, user_data);
	}

	nodes = webkit_dom_document_query_selector_all (document, "iframe", NULL);
	length = webkit_dom_node_list_get_length (nodes);

	/* Add rules to every sub document */
	for (ii = 0; ii < length; ii++) {
		WebKitDOMDocument *content_document = NULL;
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (nodes, ii);
		content_document =
			webkit_dom_html_iframe_element_get_content_document (
				WEBKIT_DOM_HTML_IFRAME_ELEMENT (node));

		if (!content_document)
			continue;

		e_dom_utils_bind_elements_recursively (
			content_document,
			selector,
			event,
			callback,
			user_data);
	}
}

static void
element_focus_cb (WebKitDOMElement *element,
                  WebKitDOMEvent *event,
		  GDBusConnection *connection)
{
	g_dbus_connection_call (
		connection,
		"org.gnome.Evolution.WebExtension",
		"/org/gnome/Evolution/WebExtension",
		"org.freedesktop.DBus.Properties",
		"Set",
		g_variant_new (
			"(ssv)",
			"org.gnome.Evolution.WebExtension",
			"NeedInput",
			g_variant_new_boolean (TRUE)),
		NULL,
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

}

static void
element_blur_cb (WebKitDOMElement *element,
                 WebKitDOMEvent *event,
		 GDBusConnection *connection)
{
	g_dbus_connection_call (
		connection,
		"org.gnome.Evolution.WebExtension",
		"/org/gnome/Evolution/WebExtension",
		"org.freedesktop.DBus.Properties",
		"Set",
		g_variant_new (
			"(ssv)",
			"org.gnome.Evolution.WebExtension",
			"NeedInput",
			g_variant_new_boolean (FALSE)),
		NULL,
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}


void
e_dom_utils_bind_focus_on_elements (WebKitDOMDocument *document,
                                    GDBusConnection *connection)
{
	const gchar *elements = "input, textarea, select, button, label";

	e_dom_utils_bind_elements_recursively (
		document,
		elements,
		"focus",
		element_focus_cb,
		connection);

	e_dom_utils_bind_elements_recursively (
		document,
		elements,
		"blur",
		element_blur_cb,
		connection);
}

void
e_dom_utils_e_mail_display_bind_dom (WebKitDOMDocument *document,
                                     GDBusConnection *connection)
{
	e_dom_utils_bind_dom (
		document,
		"#__evo-collapse-headers-img",
		"click",
		toggle_headers_visibility,
		document);

	e_dom_utils_bind_dom (
		document,
		"*[id^=__evo-moreaddr-]",
		"click",
		toggle_address_visibility,
		connection);
}

void
e_dom_utils_eab_contact_formatter_bind_dom (WebKitDOMDocument *document)
{
	e_dom_utils_bind_dom (
		document,
		"._evo_collapse_button",
		"click",
		collapse_contacts_list,
		document);
}

/* ! This function can be called only from WK2 web-extension ! */
WebKitDOMElement *
e_dom_utils_find_element_by_selector (WebKitDOMDocument *document,
                                      const gchar *selector)
{
	WebKitDOMNodeList *frames;
	WebKitDOMElement *element;
	gulong ii, length;

	/* Try to look up the element in this DOM document */
	element = webkit_dom_document_query_selector (document, selector, NULL);
	if (element != NULL)
		return element;

	/* If the element is not here then recursively scan all frames */
	frames = webkit_dom_document_get_elements_by_tag_name (document, "iframe");
	length = webkit_dom_node_list_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMHTMLIFrameElement *iframe;
		WebKitDOMDocument *content_document;
		WebKitDOMElement *element;

		iframe = WEBKIT_DOM_HTML_IFRAME_ELEMENT (
			webkit_dom_node_list_item (frames, ii));

		content_document = webkit_dom_html_iframe_element_get_content_document (iframe);
		if (!content_document)
			continue;

		element = e_dom_utils_find_element_by_id (content_document, selector);

		if (element != NULL)
			return element;
	}

	return NULL;
}

/* ! This function can be called only from WK2 web-extension ! */
WebKitDOMElement *
e_dom_utils_find_element_by_id (WebKitDOMDocument *document,
                                const gchar *id)
{
	WebKitDOMNodeList *frames;
	WebKitDOMElement *element;
	gulong ii, length;

	/* Try to look up the element in this DOM document */
	element = webkit_dom_document_get_element_by_id (document, id);
	if (element != NULL)
		return element;

	/* If the element is not here then recursively scan all frames */
	frames = webkit_dom_document_get_elements_by_tag_name (
		document, "iframe");
	length = webkit_dom_node_list_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMHTMLIFrameElement *iframe;
		WebKitDOMDocument *content_document;
		WebKitDOMElement *element;

		iframe = WEBKIT_DOM_HTML_IFRAME_ELEMENT (
			webkit_dom_node_list_item (frames, ii));

		content_document = webkit_dom_html_iframe_element_get_content_document (iframe);
		if (!content_document)
			continue;

		element = e_dom_utils_find_element_by_id (content_document, id);

		if (element != NULL)
			return element;
	}

	return NULL;
}

gboolean
e_dom_utils_element_exists (WebKitDOMDocument *document,
                            const gchar *element_id)
{
	WebKitDOMNodeList *frames;
	gboolean element_exists = FALSE;
	gulong ii, length;

	/* Try to look up the element in this DOM document */
	if (webkit_dom_document_get_element_by_id (document, element_id))
		return TRUE;

	/* If the element is not here then recursively scan all frames */
	frames = webkit_dom_document_get_elements_by_tag_name (
		document, "iframe");
	length = webkit_dom_node_list_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMHTMLIFrameElement *iframe;
		WebKitDOMDocument *content_document;

		iframe = WEBKIT_DOM_HTML_IFRAME_ELEMENT (
			webkit_dom_node_list_item (frames, ii));

		content_document = webkit_dom_html_iframe_element_get_content_document (iframe);
		if (!content_document)
			continue;

		element_exists = e_dom_utils_element_exists (content_document, element_id);

		if (element_exists)
			return TRUE;
	}

	return FALSE;
}

gchar *
e_dom_utils_get_active_element_name (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;

	element = webkit_dom_html_document_get_active_element (
		WEBKIT_DOM_HTML_DOCUMENT (document));

	if (!element)
		return NULL;

	while (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element)) {
		WebKitDOMDocument *content_document;

		content_document =
			webkit_dom_html_iframe_element_get_content_document (
				WEBKIT_DOM_HTML_IFRAME_ELEMENT (element));

		if (!content_document)
			break;

		element = webkit_dom_html_document_get_active_element (
			WEBKIT_DOM_HTML_DOCUMENT (content_document));
	}

	return webkit_dom_node_get_local_name (WEBKIT_DOM_NODE (element));
}

void
e_dom_utils_e_mail_part_headers_bind_dom_element (WebKitDOMDocument *document,
                                                  const gchar *element_id)
{
	WebKitDOMDocument *element_document;
	WebKitDOMElement *element;
	WebKitDOMElement *photo;
	gchar *addr, *uri;

	element = e_dom_utils_find_element_by_id (document, element_id);
	if (!element)
		return;

	element_document = webkit_dom_node_get_owner_document (
		WEBKIT_DOM_NODE (element));
	photo = webkit_dom_document_get_element_by_id (
		element_document, "__evo-contact-photo");

	/* Contact photos disabled, the <img> tag is not there. */
	if (!photo)
		return;

	addr = webkit_dom_element_get_attribute (photo, "data-mailaddr");
	uri = g_strdup_printf ("mail://contact-photo?mailaddr=%s", addr);

	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (photo), uri);

	g_free (addr);
	g_free (uri);
}

void
e_dom_utils_element_set_inner_html (WebKitDOMDocument *document,
                                    const gchar *element_id,
                                    const gchar *inner_html)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (document, element_id);

	if (!element)
		return;

	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element), inner_html, NULL);
}

void
e_dom_utils_remove_element (WebKitDOMDocument *document,
                            const gchar *element_id)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (document, element_id);

	if (!element)
		return;

	webkit_dom_node_remove_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
		WEBKIT_DOM_NODE (element),
		NULL);
}

void
e_dom_utils_element_remove_child_nodes (WebKitDOMDocument *document,
                                        const gchar *element_id)
{
	WebKitDOMNode *node;

	node = WEBKIT_DOM_NODE (webkit_dom_document_get_element_by_id (document, element_id));

	if (!node)
		return;

	while (webkit_dom_node_has_child_nodes (node)) {
		webkit_dom_node_remove_child (
			node,
			webkit_dom_node_get_last_child (node),
			NULL);
	}
}

void
e_dom_utils_hide_element (WebKitDOMDocument *document,
                          const gchar *element_id,
                          gboolean hide)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (document, element_id);

	if (!element)
		return;

	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (element), hide);
}

gboolean
e_dom_utils_element_is_hidden (WebKitDOMDocument *document,
                               const gchar *element_id)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (document, element_id);

	if (!element)
		return FALSE;

	return webkit_dom_html_element_get_hidden (
		WEBKIT_DOM_HTML_ELEMENT (element));
}

static void
get_total_offsets (WebKitDOMElement *element,
                   glong *left,
                   glong *top)
{
	WebKitDOMElement *offset_parent;

	if (left)
		*left = 0;

	if (top)
		*top = 0;

	offset_parent = element;
	do {
		if (left) {
			*left += webkit_dom_element_get_offset_left (offset_parent);
			*left -= webkit_dom_element_get_scroll_left (offset_parent);
		}
		if (top) {
			*top += webkit_dom_element_get_offset_top (offset_parent);
			*top -= webkit_dom_element_get_scroll_top (offset_parent);
		}
		offset_parent = webkit_dom_element_get_offset_parent (offset_parent);
	} while (offset_parent);
}

static WebKitDOMElement *
find_element_from_point (WebKitDOMDocument *document,
                         gint32 x,
                         gint32 y,
                         WebKitDOMElement *element_on_point)
{
	WebKitDOMDocument *content_document;
	WebKitDOMElement *element;

	if (!element_on_point)
		element = webkit_dom_document_element_from_point (document, x, y);
	else {
		glong left, top;
		get_total_offsets (element_on_point, &left, &top);

		element = webkit_dom_document_element_from_point (
			document, x - left, y - top);
	}

	if (!element)
		return element_on_point;

	if (element_on_point && webkit_dom_node_is_equal_node (
	    	WEBKIT_DOM_NODE (element),
	        WEBKIT_DOM_NODE (element_on_point))) {
		return element_on_point;
	}

	if (!WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element))
		return element_on_point;

	content_document =
		webkit_dom_html_iframe_element_get_content_document (
			WEBKIT_DOM_HTML_IFRAME_ELEMENT (element));

	if (!content_document)
		return element_on_point;

	return find_element_from_point (content_document, x, y, element);
}

/* ! This function can be called only from WK2 web-extension ! */
WebKitDOMElement *
e_dom_utils_get_element_from_point (WebKitDOMDocument *document,
                                    gint32 x,
                                    gint32 y)
{
	return find_element_from_point (document, x, y, NULL);
}

/* ! This function can be called only from WK2 web-extension ! */
WebKitDOMDocument *
e_dom_utils_get_document_from_point (WebKitDOMDocument *document,
                                     gint32 x,
                                     gint32 y)
{
	WebKitDOMElement *element;

	if (x == 0 && y == 0)
		element = webkit_dom_html_document_get_active_element (WEBKIT_DOM_HTML_DOCUMENT (document));
	else
		element = find_element_from_point (document, x, y, NULL);

	if (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element))
		return webkit_dom_html_iframe_element_get_content_document (
			WEBKIT_DOM_HTML_IFRAME_ELEMENT (element));
	else
		return webkit_dom_node_get_owner_document (
			WEBKIT_DOM_NODE (element));
}

/* VCard Inline Module DOM functions */

static void
display_mode_toggle_button_cb (WebKitDOMElement *button,
                               WebKitDOMEvent *event,
                               GDBusConnection *connection)
{
	GError *error = NULL;
	gchar *element_id;

	element_id = webkit_dom_element_get_id (button);

	g_dbus_connection_emit_signal (
		connection,
		NULL,
		EVOLUTION_WEB_EXTENSION_OBJECT_PATH,
		EVOLUTION_WEB_EXTENSION_INTERFACE,
		"VCardInlineDisplayModeToggled",
		g_variant_new ("(s)", element_id),
		&error);

	if (error) {
		g_warning ("Error emitting signal DisplayModeToggled: %s\n", error->message);
		g_error_free (error);
	}

	g_free (element_id);
}

static void
save_vcard_button_cb (WebKitDOMElement *button,
                      WebKitDOMEvent *event,
                      GDBusConnection *connection)
{
	GError *error = NULL;
	gchar *button_value;

	button_value = webkit_dom_html_button_element_get_value (
		WEBKIT_DOM_HTML_BUTTON_ELEMENT (button));

	g_dbus_connection_emit_signal (
		connection,
		NULL,
		EVOLUTION_WEB_EXTENSION_OBJECT_PATH,
		EVOLUTION_WEB_EXTENSION_INTERFACE,
		"VCardInlineSaveButtonPressed",
		g_variant_new ("(s)", button_value),
		&error);

	if (error) {
		g_warning ("Error emitting signal SaveVCardButtonPressed: %s\n", error->message);
		g_error_free (error);
	}

	g_free (button_value);
}

void
e_dom_utils_module_vcard_inline_bind_dom (WebKitDOMDocument *document,
                                          const gchar *element_id,
                                          GDBusConnection *connection)
{
	WebKitDOMElement *element;
	WebKitDOMDocument *element_document;
	gchar *selector;

	element = e_dom_utils_find_element_by_id (document, element_id);
	if (!element)
		return;

	element_document = webkit_dom_node_get_owner_document (
		WEBKIT_DOM_NODE (element));

	selector = g_strconcat ("button[id='", element_id, "']", NULL);
	e_dom_utils_bind_dom (
		element_document,
		selector,
		"click",
		display_mode_toggle_button_cb,
		connection);
	g_free (selector);

	selector = g_strconcat ("button[value='", element_id, "']", NULL);
	e_dom_utils_bind_dom (
		element_document,
		selector,
		"click",
		save_vcard_button_cb,
		connection);
	g_free (selector);

	e_dom_utils_eab_contact_formatter_bind_dom (element_document);
}

void
e_dom_utils_module_vcard_inline_update_button (WebKitDOMDocument *document,
                                               const gchar *button_id,
                                               const gchar *html_label,
                                               const gchar *access_key)
{
	WebKitDOMElement *element;
	gchar *selector;

	selector = g_strconcat ("button[id='", button_id, "']", NULL);
	element = e_dom_utils_find_element_by_selector (document, selector);
	g_free (selector);

	if (!element)
		return;

	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element), html_label, NULL);

	if (access_key) {
		webkit_dom_html_element_set_access_key (
			WEBKIT_DOM_HTML_ELEMENT (element), access_key);
	}
}

void
e_dom_utils_module_vcard_inline_set_iframe_src (WebKitDOMDocument *document,
                                                const gchar *button_id,
                                                const gchar *src)
{
	WebKitDOMElement *element, *parent, *iframe;
	gchar *selector;

	selector = g_strconcat ("button[id='", button_id, "']", NULL);
	element = e_dom_utils_find_element_by_selector (document, selector);
	g_free (selector);

	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (element));
	if (!parent)
		return;

	iframe = webkit_dom_element_query_selector (parent, "iframe", NULL);
	if (!iframe)
		return;

	webkit_dom_html_iframe_element_set_src (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe), src);
}
