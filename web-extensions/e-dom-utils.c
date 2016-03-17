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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>
#include <webkitdom/WebKitDOMHTMLElementUnstable.h>

#include "e-web-extension.h"
#include "e-web-extension-names.h"

#include "e-dom-utils.h"

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

gboolean
e_dom_utils_document_has_selection (WebKitDOMDocument *document)
{
	gboolean ret_val = FALSE;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;

	if (!document)
		return FALSE;

	dom_window = webkit_dom_document_get_default_view (document);
	if (!dom_window)
		goto out;

	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	if (!WEBKIT_DOM_IS_DOM_SELECTION (dom_selection))
		goto out;

	if (webkit_dom_dom_selection_get_is_collapsed (dom_selection))
		goto out;

	ret_val = TRUE;
 out:
	g_clear_object (&dom_window);
	g_clear_object (&dom_selection);

	if (!ret_val) {
		WebKitDOMHTMLCollection *frames;
		gulong ii, length;

		frames = webkit_dom_document_get_elements_by_tag_name_as_html_collection (document, "iframe");
		length = webkit_dom_html_collection_get_length (frames);
		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *node;
			WebKitDOMDocument *content_document;

			node = webkit_dom_html_collection_item (frames, ii);
			content_document = webkit_dom_html_iframe_element_get_content_document (
				WEBKIT_DOM_HTML_IFRAME_ELEMENT (node));

			if ((ret_val = e_dom_utils_document_has_selection (content_document))) {
				g_object_unref (node);
				break;
			}

			g_object_unref (node);
		}

		g_object_unref (frames);
	}

	return ret_val;
}


gchar *
e_dom_utils_get_document_content_html (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_document_element (document);

	return webkit_dom_element_get_outer_html (element);
}

static gboolean
element_is_in_pre_tag (WebKitDOMNode *node)
{
	WebKitDOMElement *element;

	if (!node)
		return FALSE;

	while (element = webkit_dom_node_get_parent_element (node), element) {
		node = WEBKIT_DOM_NODE (element);

		if (WEBKIT_DOM_IS_HTML_PRE_ELEMENT (element)) {
			return TRUE;
		} else if (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element)) {
			break;
		}
	}

	return FALSE;
}

static gchar *
get_frame_selection_html (WebKitDOMElement *iframe)
{
	WebKitDOMDocument *content_document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMHTMLCollection *frames;
	gulong ii, length;

	content_document = webkit_dom_html_iframe_element_get_content_document (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe));

	if (!content_document)
		return NULL;

	dom_window = webkit_dom_document_get_default_view (content_document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);
	if (dom_selection && (webkit_dom_dom_selection_get_range_count (dom_selection) > 0)) {
		WebKitDOMRange *range;
		WebKitDOMElement *element;
		WebKitDOMDocumentFragment *fragment;

		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		if (range != NULL) {
			gchar *inner_html;
			WebKitDOMNode *node;

			fragment = webkit_dom_range_clone_contents (
				range, NULL);

			element = webkit_dom_document_create_element (
				content_document, "DIV", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element),
				WEBKIT_DOM_NODE (fragment), NULL);

			inner_html = webkit_dom_element_get_inner_html (element);

			node = webkit_dom_range_get_start_container (range, NULL);
			if (element_is_in_pre_tag (node)) {
				gchar *tmp = inner_html;
				inner_html = g_strconcat ("<pre>", tmp, "</pre>", NULL);
				g_free (tmp);
			}

			g_object_unref (range);
			g_object_unref (dom_selection);
			return inner_html;
		}
	}

	g_object_unref (dom_selection);

	frames = webkit_dom_document_get_elements_by_tag_name_as_html_collection (content_document, "iframe");
	length = webkit_dom_html_collection_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;
		gchar *text;

		node = webkit_dom_html_collection_item (frames, ii);

		text = get_frame_selection_html (
			WEBKIT_DOM_ELEMENT (node));

		g_object_unref (node);
		if (text != NULL) {
			g_object_unref (frames);
			return text;
		}
	}

	g_object_unref (frames);

	return NULL;
}

gchar *
e_dom_utils_get_selection_content_html (WebKitDOMDocument *document)
{
	WebKitDOMHTMLCollection *frames;
	gulong ii, length;

	if (!e_dom_utils_document_has_selection (document))
		return NULL;

	frames = webkit_dom_document_get_elements_by_tag_name_as_html_collection (document, "iframe");
	length = webkit_dom_html_collection_get_length (frames);

	for (ii = 0; ii < length; ii++) {
		gchar *text;
		WebKitDOMNode *node;

		node = webkit_dom_html_collection_item (frames, ii);

		text = get_frame_selection_html (
			WEBKIT_DOM_ELEMENT (node));

		g_object_unref (node);
		if (text != NULL) {
			g_object_unref (frames);
			return text;
		}
	}

	g_object_unref (frames);
	return NULL;
}

static gchar *
get_frame_selection_content_text (WebKitDOMElement *iframe)
{
	WebKitDOMDocument *content_document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMHTMLCollection *frames;
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

	frames = webkit_dom_document_get_elements_by_tag_name_as_html_collection (content_document, "iframe");
	length = webkit_dom_html_collection_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;
		gchar *text;

		node = webkit_dom_html_collection_item (frames, ii);

		text = get_frame_selection_content_text (
			WEBKIT_DOM_ELEMENT (node));

		g_object_unref (node);
		if (text != NULL) {
			g_object_unref (frames);
			return text;
		}
	}

	g_object_unref (frames);
	return NULL;
}

gchar *
e_dom_utils_get_selection_content_text (WebKitDOMDocument *document)
{
	WebKitDOMHTMLCollection *frames;
	gulong ii, length;

	frames = webkit_dom_document_get_elements_by_tag_name_as_html_collection (document, "iframe");
	length = webkit_dom_html_collection_get_length (frames);

	for (ii = 0; ii < length; ii++) {
		gchar *text;
		WebKitDOMNode *node;

		node = webkit_dom_html_collection_item (frames, ii);

		text = get_frame_selection_content_text (
			WEBKIT_DOM_ELEMENT (node));

		g_object_unref (node);
		if (text != NULL) {
			g_object_unref (frames);
			return text;
		}
	}

	g_object_unref (frames);
	return NULL;
}

void
e_dom_utils_create_and_add_css_style_sheet (WebKitDOMDocument *document,
                                            const gchar *style_sheet_id)
{
	WebKitDOMElement *style_element;

	style_element = webkit_dom_document_get_element_by_id (document, style_sheet_id);

	if (!style_element) {
		WebKitDOMText *dom_text;
		WebKitDOMHTMLHeadElement *head;

		dom_text = webkit_dom_document_create_text_node (document, "");

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
			WEBKIT_DOM_NODE (dom_text),
			NULL);

		head = webkit_dom_document_get_head (document);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (head),
			WEBKIT_DOM_NODE (style_element),
			NULL);

		g_object_unref (head);
		g_object_unref (dom_text);
		g_object_unref (style_element);
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
	gint length, ii, selector_length;
	gboolean removed = FALSE;

	g_return_if_fail (selector != NULL);

	selector_length = strlen (selector);
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
	for (ii = 0; ii < length && !removed; ii++) {
		WebKitDOMCSSRule *rule;
		gchar *rule_text = NULL;

		rule = webkit_dom_css_rule_list_item (rules_list, ii);

		g_return_if_fail (WEBKIT_DOM_IS_CSS_RULE (rule));

		rule_text = webkit_dom_css_rule_get_css_text (rule);

		/* Find the start of the style => end of the selector */
		if (rule_text && selector && g_str_has_prefix (rule_text, selector) &&
		    rule_text[selector_length] == ' ' && rule_text[selector_length + 1] == '{') {
			/* If exists remove it */
			webkit_dom_css_style_sheet_remove_rule (
				WEBKIT_DOM_CSS_STYLE_SHEET (sheet),
				ii, NULL);
			length--;
			removed = TRUE;
		}

		g_free (rule_text);
		g_object_unref (rule);
	}

	g_object_unref (rules_list);

	/* Insert the rule at the end, so it will override previously inserted */
	webkit_dom_css_style_sheet_add_rule (
		WEBKIT_DOM_CSS_STYLE_SHEET (sheet), selector, style, length, NULL);

	g_object_unref (sheet);
	g_object_unref (style_element);
}

static void
add_css_rule_into_style_sheet_recursive (WebKitDOMDocument *document,
                                         const gchar *style_sheet_id,
                                         const gchar *selector,
                                         const gchar *style)
{
	WebKitDOMHTMLCollection *frames;
	gint ii, length;

	/* Add rule to document */
	add_css_rule_into_style_sheet (
		document,
		style_sheet_id,
		selector,
		style);

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

		add_css_rule_into_style_sheet_recursive (
			content_document,
			style_sheet_id,
			selector,
			style);
		g_object_unref (node);
	}
	g_object_unref (frames);
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

	if (!id)
		return;

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
	WebKitDOMElement *short_headers = NULL, *full_headers = NULL;
	WebKitDOMCSSStyleDeclaration *css_short = NULL, *css_full = NULL;
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
		goto clean;

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
 clean:
	g_clear_object (&short_headers);
	g_clear_object (&css_short);
	g_clear_object (&full_headers);
	g_clear_object (&css_full);
}

static void
toggle_address_visibility (WebKitDOMElement *button,
                           WebKitDOMEvent *event,
                           GDBusConnection *connection)
{
	WebKitDOMElement *full_addr = NULL, *ellipsis = NULL;
	WebKitDOMElement *parent = NULL, *bold = NULL;
	WebKitDOMCSSStyleDeclaration *css_full = NULL, *css_ellipsis = NULL;
	const gchar *path;
	gchar *property_value;
	gboolean expanded;
	GError *error = NULL;

	/* <b> element */
	bold = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (button));
	/* <td> element */
	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (bold));
	g_object_unref (bold);

	full_addr = webkit_dom_element_query_selector (parent, "#__evo-moreaddr", NULL);

	if (!full_addr)
		goto clean;

	css_full = webkit_dom_element_get_style (full_addr);

	ellipsis = webkit_dom_element_query_selector (parent, "#__evo-moreaddr-ellipsis", NULL);

	if (!ellipsis)
		goto clean;

	css_ellipsis = webkit_dom_element_get_style (ellipsis);

	property_value = webkit_dom_css_style_declaration_get_property_value (css_full, "display");
	expanded = g_strcmp0 (property_value, "inline") == 0;
	g_free (property_value);

	webkit_dom_css_style_declaration_set_property (
		css_full, "display", (expanded ? "none" : "inline"), "", NULL);
	webkit_dom_css_style_declaration_set_property (
		css_ellipsis, "display", (expanded ? "inline" : "none"), "", NULL);

	if (expanded)
		path = "evo-file://" EVOLUTION_IMAGESDIR "/plus.png";
	else
		path = "evo-file://" EVOLUTION_IMAGESDIR "/minus.png";

	if (!WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (button)) {
		WebKitDOMElement *element;

		element = webkit_dom_element_query_selector (parent, "#__evo-moreaddr-img", NULL);
		if (!element)
			goto clean;

		webkit_dom_html_image_element_set_src (WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), path);

		g_object_unref (element);
	} else
		webkit_dom_html_image_element_set_src (WEBKIT_DOM_HTML_IMAGE_ELEMENT (button), path);

	g_dbus_connection_emit_signal (
		connection,
		NULL,
		E_WEB_EXTENSION_OBJECT_PATH,
		E_WEB_EXTENSION_INTERFACE,
		"HeadersCollapsed",
		g_variant_new ("(b)", expanded),
		&error);

	if (error) {
		g_warning ("Error emitting signal HeadersCollapsed: %s\n", error->message);
		g_error_free (error);
	}

 clean:
	g_clear_object (&css_full);
	g_clear_object (&css_ellipsis);
	g_clear_object (&full_addr);
	g_clear_object (&ellipsis);
	g_clear_object (&parent);
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
	g_object_unref (nodes);
}

static void
e_dom_utils_bind_elements_recursively (WebKitDOMDocument *document,
                                       const gchar *selector,
                                       const gchar *event,
                                       gpointer callback,
                                       gpointer user_data)
{
	WebKitDOMNodeList *nodes;
	WebKitDOMHTMLCollection *frames;
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
	g_object_unref (nodes);

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

		e_dom_utils_bind_elements_recursively (
			content_document,
			selector,
			event,
			callback,
			user_data);
	}
	g_object_unref (frames);
}

static void
element_focus_cb (WebKitDOMElement *element,
                  WebKitDOMEvent *event,
		  GDBusConnection *connection)
{
	g_dbus_connection_call (
		connection,
		E_WEB_EXTENSION_SERVICE_NAME,
		E_WEB_EXTENSION_OBJECT_PATH,
		E_WEB_EXTENSION_INTERFACE,
		"Set",
		g_variant_new (
			"(ssv)",
			E_WEB_EXTENSION_INTERFACE,
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
		E_WEB_EXTENSION_SERVICE_NAME,
		E_WEB_EXTENSION_OBJECT_PATH,
		E_WEB_EXTENSION_INTERFACE,
		"Set",
		g_variant_new (
			"(ssv)",
			E_WEB_EXTENSION_INTERFACE,
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
	WebKitDOMHTMLCollection *frames;
	WebKitDOMElement *element;
	gulong ii, length;

	/* Try to look up the element in this DOM document */
	element = webkit_dom_document_query_selector (document, selector, NULL);
	if (element != NULL)
		return element;

	/* If the element is not here then recursively scan all frames */
	frames = webkit_dom_document_get_elements_by_tag_name_as_html_collection (document, "iframe");
	length = webkit_dom_html_collection_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMHTMLIFrameElement *iframe;
		WebKitDOMDocument *content_document;

		iframe = WEBKIT_DOM_HTML_IFRAME_ELEMENT (
			webkit_dom_html_collection_item (frames, ii));

		content_document = webkit_dom_html_iframe_element_get_content_document (iframe);
		if (!content_document)
			continue;

		element = e_dom_utils_find_element_by_id (content_document, selector);

		g_object_unref (iframe);
		if (element != NULL)
			break;
	}

	g_object_unref (frames);
	return element;
}

/* ! This function can be called only from WK2 web-extension ! */
WebKitDOMElement *
e_dom_utils_find_element_by_id (WebKitDOMDocument *document,
                                const gchar *id)
{
	WebKitDOMHTMLCollection *frames;
	WebKitDOMElement *element;
	gulong ii, length;

	/* Try to look up the element in this DOM document */
	element = webkit_dom_document_get_element_by_id (document, id);
	if (element != NULL)
		return element;

	/* If the element is not here then recursively scan all frames */
	frames = webkit_dom_document_get_elements_by_tag_name_as_html_collection (document, "iframe");
	length = webkit_dom_html_collection_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMHTMLIFrameElement *iframe;
		WebKitDOMDocument *content_document;

		iframe = WEBKIT_DOM_HTML_IFRAME_ELEMENT (
			webkit_dom_html_collection_item (frames, ii));

		content_document = webkit_dom_html_iframe_element_get_content_document (iframe);
		if (!content_document)
			continue;

		element = e_dom_utils_find_element_by_id (content_document, id);

		g_object_unref (iframe);
		if (element != NULL)
			break;
	}

	g_object_unref (frames);
	return element;
}

gboolean
e_dom_utils_element_exists (WebKitDOMDocument *document,
                            const gchar *element_id)
{
	WebKitDOMHTMLCollection *frames;
	gboolean element_exists = FALSE;
	gulong ii, length;

	/* Try to look up the element in this DOM document */
	if (webkit_dom_document_get_element_by_id (document, element_id))
		return TRUE;

	/* If the element is not here then recursively scan all frames */
	frames = webkit_dom_document_get_elements_by_tag_name_as_html_collection (document, "iframe");
	length = webkit_dom_html_collection_get_length (frames);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMHTMLIFrameElement *iframe;
		WebKitDOMDocument *content_document;

		iframe = WEBKIT_DOM_HTML_IFRAME_ELEMENT (
			webkit_dom_html_collection_item (frames, ii));

		content_document = webkit_dom_html_iframe_element_get_content_document (iframe);
		if (!content_document)
			continue;

		element_exists = e_dom_utils_element_exists (content_document, element_id);

		if (element_exists) {
			g_object_unref (frames);
			return TRUE;
		}
	}

	g_object_unref (frames);
	return FALSE;
}

gchar *
e_dom_utils_get_active_element_name (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_active_element (document);

	if (!element)
		return NULL;

	while (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element)) {
		WebKitDOMDocument *content_document;

		content_document =
			webkit_dom_html_iframe_element_get_content_document (
				WEBKIT_DOM_HTML_IFRAME_ELEMENT (element));

		if (!content_document)
			break;

		element = webkit_dom_document_get_active_element (content_document);
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
	gchar *addr;

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
	if (addr) {
		gchar *uri;

		uri = g_strdup_printf ("mail://contact-photo?mailaddr=%s", addr);

		webkit_dom_html_image_element_set_src (
			WEBKIT_DOM_HTML_IMAGE_ELEMENT (photo), uri);

		g_free (uri);
	}

	g_free (addr);
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

	webkit_dom_element_set_inner_html (element, inner_html, NULL);
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
		element = webkit_dom_document_get_active_element (document);
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
		E_WEB_EXTENSION_OBJECT_PATH,
		E_WEB_EXTENSION_INTERFACE,
		"VCardInlineDisplayModeToggled",
		g_variant_new ("(s)", element_id ? element_id : ""),
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
		E_WEB_EXTENSION_OBJECT_PATH,
		E_WEB_EXTENSION_INTERFACE,
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

	webkit_dom_element_set_inner_html (element, html_label, NULL);

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

/**
 * e_html_editor_dom_node_find_parent_element:
 * @node: Start node
 * @tagname: Tag name of element to search
 *
 * Recursively searches for first occurance of element with given @tagname
 * that is parent of given @node.
 *
 * Returns: A #WebKitDOMElement with @tagname representing parent of @node or
 * @NULL when @node has no parent with given @tagname. When @node matches @tagname,
 * then the @node is returned.
 */
WebKitDOMElement *
dom_node_find_parent_element (WebKitDOMNode *node,
                              const gchar *tagname)
{
	WebKitDOMNode *tmp_node = node;
	gint taglen = strlen (tagname);

	while (tmp_node) {
		if (WEBKIT_DOM_IS_ELEMENT (tmp_node)) {
			gchar *node_tagname;

			node_tagname = webkit_dom_element_get_tag_name (
				WEBKIT_DOM_ELEMENT (tmp_node));

			if (node_tagname &&
			    (strlen (node_tagname) == taglen) &&
			    (g_ascii_strncasecmp (node_tagname, tagname, taglen) == 0)) {
				g_free (node_tagname);
				return WEBKIT_DOM_ELEMENT (tmp_node);
			}

			g_free (node_tagname);
		}

		tmp_node = webkit_dom_node_get_parent_node (tmp_node);
	}

	return NULL;
}

/**
 * e_html_editor_dom_node_find_child_element:
 * @node: Start node
 * @tagname: Tag name of element to search.
 *
 * Recursively searches for first occurrence of element with given @tagname that
 * is a child of @node.
 *
 * Returns: A #WebKitDOMElement with @tagname representing a child of @node or
 * @NULL when @node has no child with given @tagname. When @node matches @tagname,
 * then the @node is returned.
 */
WebKitDOMElement *
dom_node_find_child_element (WebKitDOMNode *node,
                             const gchar *tagname)
{
	WebKitDOMNode *start_node = node;
	gint taglen = strlen (tagname);

	do {
		if (WEBKIT_DOM_IS_ELEMENT (node)) {
			gchar *node_tagname;

			node_tagname = webkit_dom_element_get_tag_name (
					WEBKIT_DOM_ELEMENT (node));

			if (node_tagname &&
			    (strlen (node_tagname) == taglen) &&
			    (g_ascii_strncasecmp (node_tagname, tagname, taglen) == 0)) {
				g_free (node_tagname);
				return WEBKIT_DOM_ELEMENT (node);
			}

			g_free (node_tagname);
		}

		if (webkit_dom_node_has_child_nodes (node)) {
			node = webkit_dom_node_get_first_child (node);
		} else if (webkit_dom_node_get_next_sibling (node)) {
			node = webkit_dom_node_get_next_sibling (node);
		} else {
			node = webkit_dom_node_get_parent_node (node);
		}
	} while (!webkit_dom_node_is_same_node (node, start_node));

	return NULL;
}

gboolean
element_has_id (WebKitDOMElement *element,
                const gchar* id)
{
	gchar *element_id;

	if (!element)
		return FALSE;

	if (!WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	element_id = webkit_dom_element_get_id (element);

	if (element_id && g_ascii_strcasecmp (element_id, id) == 0) {
		g_free (element_id);
		return TRUE;
	}
	g_free (element_id);

	return FALSE;
}

gboolean
element_has_tag (WebKitDOMElement *element,
                 const gchar* tag)
{
	gchar *element_tag;

	if (!WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	element_tag = webkit_dom_node_get_local_name (WEBKIT_DOM_NODE (element));

	if (g_ascii_strcasecmp (element_tag, tag) != 0) {
		g_free (element_tag);
		return FALSE;
	}
	g_free (element_tag);

	return TRUE;
}

gboolean
element_has_class (WebKitDOMElement *element,
                   const gchar* class)
{
	gchar *element_class;

	if (!element)
		return FALSE;

	if (!WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	element_class = webkit_dom_element_get_class_name (element);

	if (element_class && g_strstr_len (element_class, -1, class)) {
		g_free (element_class);
		return TRUE;
	}
	g_free (element_class);

	return FALSE;
}

void
element_add_class (WebKitDOMElement *element,
                   const gchar* class)
{
	gchar *element_class;
	gchar *new_class;

	if (!WEBKIT_DOM_IS_ELEMENT (element))
		return;

	if (element_has_class (element, class))
		return;

	element_class = webkit_dom_element_get_class_name (element);

	if (!element_class)
		new_class = g_strdup (class);
	else
		new_class = g_strconcat (element_class, " ", class, NULL);

	webkit_dom_element_set_class_name (element, new_class);

	g_free (element_class);
	g_free (new_class);
}

void
element_remove_class (WebKitDOMElement *element,
                      const gchar* class)
{
	gchar *element_class;
	GString *result;

	if (!WEBKIT_DOM_IS_ELEMENT (element))
		return;

	if (!element_has_class (element, class))
		return;

	element_class = webkit_dom_element_get_class_name (element);

	if (g_strcmp0 (element_class, class) == 0) {
		webkit_dom_element_remove_attribute (element, "class");
		g_free (element_class);
		return;
	}

	result = e_str_replace_string (element_class, class, "");
	if (result) {
		webkit_dom_element_set_class_name (element, result->str);
		g_string_free (result, TRUE);
	}

	g_free (element_class);
}

void
remove_node (WebKitDOMNode *node)
{
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);

	/* Check if the parent exists, if so it means that the node is still
	 * in the DOM or at least the parent is. If it doesn't exists it is not
	 * in the DOM and we can free it. */
	if (parent)
		webkit_dom_node_remove_child (parent, node, NULL);
	else
		g_object_unref (node);
}

void
remove_node_if_empty (WebKitDOMNode *node)
{
	if (!WEBKIT_DOM_IS_NODE (node))
		return;

	if (!webkit_dom_node_get_first_child (node)) {
		remove_node (node);
	} else {
		gchar *text_content;

		text_content = webkit_dom_node_get_text_content (node);
		if (!text_content)
			remove_node (node);

		if (text_content && !*text_content)
			remove_node (node);

		g_free (text_content);
	}
}

WebKitDOMNode *
split_list_into_two (WebKitDOMNode *item,
		     gint level)
{
	gint current_level = 1;
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMNode *parent, *prev_parent, *tmp;

	document = webkit_dom_node_get_owner_document (item);
	fragment = webkit_dom_document_create_document_fragment (document);

	tmp = item;
	parent = webkit_dom_node_get_parent_node (item);
	while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		WebKitDOMNode *clone, *first_child, *insert_before = NULL, *sibling;

		first_child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
		clone = webkit_dom_node_clone_node_with_error (parent, FALSE, NULL);
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (fragment), clone, first_child, NULL);

		if (first_child)
			insert_before = webkit_dom_node_get_first_child (first_child);

		while (first_child && (sibling = webkit_dom_node_get_next_sibling (first_child)))
			webkit_dom_node_insert_before (first_child, sibling, insert_before, NULL);

		while ((sibling = webkit_dom_node_get_next_sibling (tmp)))
			webkit_dom_node_append_child (clone, sibling, NULL);

		webkit_dom_node_insert_before (
			clone, tmp, webkit_dom_node_get_first_child (clone), NULL);

		prev_parent = parent;
		tmp = webkit_dom_node_get_next_sibling (parent);
		parent = webkit_dom_node_get_parent_node (parent);
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
			first_child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
			insert_before = webkit_dom_node_get_first_child (first_child);
			while (first_child && (sibling = webkit_dom_node_get_next_sibling (first_child))) {
				webkit_dom_node_insert_before (
					first_child, sibling, insert_before, NULL);
			}
		}

		if (current_level >= level && level >= 0)
			break;

		current_level++;
	}

	tmp = webkit_dom_node_insert_before (
		parent,
		webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
		webkit_dom_node_get_next_sibling (prev_parent),
		NULL);
	remove_node_if_empty (prev_parent);

	return tmp;
}

WebKitDOMElement *
dom_create_selection_marker (WebKitDOMDocument *document,
                             gboolean selection_start_marker)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (
		document, "SPAN", NULL);
	webkit_dom_element_set_id (
		element,
		selection_start_marker ?
			"-x-evo-selection-start-marker" :
			"-x-evo-selection-end-marker");

	return element;
}

void
dom_remove_selection_markers (WebKitDOMDocument *document)
{
	WebKitDOMElement *marker;

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (marker)
		remove_node (WEBKIT_DOM_NODE (marker));
	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");
	if (marker)
		remove_node (WEBKIT_DOM_NODE (marker));
}

void
dom_add_selection_markers_into_element_start (WebKitDOMDocument *document,
                                              WebKitDOMElement *element,
                                              WebKitDOMElement **selection_start_marker,
                                              WebKitDOMElement **selection_end_marker)
{
	WebKitDOMElement *marker;

	dom_remove_selection_markers (document);
	marker = dom_create_selection_marker (document, FALSE);
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (element),
		WEBKIT_DOM_NODE (marker),
		webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)),
		NULL);
	if (selection_end_marker)
		*selection_end_marker = marker;

	marker = dom_create_selection_marker (document, TRUE);
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (element),
		WEBKIT_DOM_NODE (marker),
		webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)),
		NULL);
	if (selection_start_marker)
		*selection_start_marker = marker;
}

void
dom_add_selection_markers_into_element_end (WebKitDOMDocument *document,
                                            WebKitDOMElement *element,
                                            WebKitDOMElement **selection_start_marker,
                                            WebKitDOMElement **selection_end_marker)
{
	WebKitDOMElement *marker;

	dom_remove_selection_markers (document);
	marker = dom_create_selection_marker (document, TRUE);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (element), WEBKIT_DOM_NODE (marker), NULL);
	if (selection_start_marker)
		*selection_start_marker = marker;

	marker = dom_create_selection_marker (document, FALSE);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (element), WEBKIT_DOM_NODE (marker), NULL);
	if (selection_end_marker)
		*selection_end_marker = marker;
}

gboolean
node_is_list_or_item (WebKitDOMNode *node)
{
	return node && (
		WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (node) ||
		WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (node) ||
		WEBKIT_DOM_IS_HTML_LI_ELEMENT (node));
}

gboolean
node_is_list (WebKitDOMNode *node)
{
	return node && (
		WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (node) ||
		WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (node));
}

/**
 * e_html_editor_selection_get_list_format_from_node:
 * @node: an #WebKitDOMNode
 *
 * Returns block format of given list.
 *
 * Returns: #EHTMLEditorSelectionBlockFormat
 */
EHTMLEditorSelectionBlockFormat
dom_get_list_format_from_node (WebKitDOMNode *node)
{
	EHTMLEditorSelectionBlockFormat format =
		E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;

	if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (node))
		return -1;

	if (WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (node))
		return format;

	if (WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (node)) {
		gchar *type_value = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "type");

		if (!type_value)
			return E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST;

		if (!*type_value)
			format = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST;
		else if (g_ascii_strcasecmp (type_value, "A") == 0)
			format = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA;
		else if (g_ascii_strcasecmp (type_value, "I") == 0)
			format = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN;
		g_free (type_value);

		return format;
	}

	return -1;
}

void
merge_list_into_list (WebKitDOMNode *from,
                      WebKitDOMNode *to,
                      gboolean insert_before)
{
	WebKitDOMNode *item, *insert_before_node;

	if (!(to && from))
		return;

	insert_before_node = webkit_dom_node_get_first_child (to);
	while ((item = webkit_dom_node_get_first_child (from)) != NULL) {
		if (insert_before)
			webkit_dom_node_insert_before (
				to, item, insert_before_node, NULL);
		else
			webkit_dom_node_append_child (to, item, NULL);
	}

	if (!webkit_dom_node_has_child_nodes (from))
		remove_node (from);

}

void
merge_lists_if_possible (WebKitDOMNode *list)
{
	EHTMLEditorSelectionBlockFormat format, prev, next;
	gint ii, length;
	WebKitDOMNode *prev_sibling, *next_sibling;
	WebKitDOMNodeList *lists;

	prev_sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (list));
	next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (list));

	format = dom_get_list_format_from_node (list),
	prev = dom_get_list_format_from_node (prev_sibling);
	next = dom_get_list_format_from_node (next_sibling);

	if (format == prev && format != -1 && prev != -1)
		merge_list_into_list (prev_sibling, list, TRUE);

	if (format == next && format != -1 && next != -1)
		merge_list_into_list (next_sibling, list, FALSE);

	lists = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (list), "ol + ol, ul + ul", NULL);
	length = webkit_dom_node_list_get_length (lists);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (lists, ii);
		merge_lists_if_possible (node);
		g_object_unref (node);
	}
	g_object_unref (lists);
}

WebKitDOMElement *
get_parent_block_element (WebKitDOMNode *node)
{
	WebKitDOMElement *parent = webkit_dom_node_get_parent_element (node);

	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent))
		return WEBKIT_DOM_ELEMENT (node);

	while (parent &&
	       !WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_DIV_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_PRE_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_HEADING_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (parent) &&
	       !element_has_tag (parent, "address")) {
		parent = webkit_dom_node_get_parent_element (
			WEBKIT_DOM_NODE (parent));
	}

	return parent;
}

gchar *
dom_get_node_inner_html (WebKitDOMNode *node)
{
	gchar *inner_html;
	WebKitDOMDocument *document;
	WebKitDOMElement *div;

	document = webkit_dom_node_get_owner_document (node);
	div = webkit_dom_document_create_element (document, "div", NULL);
	webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (div),
			webkit_dom_node_clone_node_with_error (node, TRUE, NULL),
			NULL);

	inner_html = webkit_dom_element_get_inner_html (div);
	remove_node (WEBKIT_DOM_NODE (div));

	return inner_html;
}

WebKitDOMDocument *
e_dom_utils_find_document_with_uri (WebKitDOMDocument *root_document,
				    const gchar *find_document_uri)
{
	WebKitDOMDocument *res_document = NULL;
	GSList *todo;

	g_return_val_if_fail (WEBKIT_DOM_IS_DOCUMENT (root_document), NULL);
	g_return_val_if_fail (find_document_uri != NULL, NULL);

	todo = g_slist_append (NULL, root_document);

	while (todo) {
		WebKitDOMDocument *document;
		WebKitDOMHTMLCollection *frames;
		gchar *document_uri;
		gint ii, length;

		document = todo->data;
		todo = g_slist_remove (todo, document);

		document_uri = webkit_dom_document_get_document_uri (document);
		if (g_strcmp0 (document_uri, find_document_uri) == 0) {
			g_free (document_uri);
			res_document = document;
			break;
		}

		g_free (document_uri);

		frames = webkit_dom_document_get_elements_by_tag_name_as_html_collection (document, "iframe");
		length = webkit_dom_html_collection_get_length (frames);

		/* Add rules to every sub document */
		for (ii = 0; ii < length; ii++) {
			WebKitDOMDocument *content_document;
			WebKitDOMNode *node;

			node = webkit_dom_html_collection_item (frames, ii);
			content_document =
				webkit_dom_html_iframe_element_get_content_document (
					WEBKIT_DOM_HTML_IFRAME_ELEMENT (node));

			if (!content_document)
				continue;

			todo = g_slist_prepend (todo, content_document);

			g_object_unref (node);
		}

		g_object_unref (frames);
	}

	g_slist_free (todo);

	return res_document;
}
