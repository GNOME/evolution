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
		WebKitDOMDocument *frame_document;

		frame_document =
			webkit_dom_html_iframe_element_get_content_document (
				WEBKIT_DOM_HTML_IFRAME_ELEMENT (element));
		replace_local_image_links (WEBKIT_DOM_ELEMENT (frame_document));
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
	if (!WEBKIT_DOM_IS_DOM_SELECTION (dom_selection)
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
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMNodeList *frames;
	gulong ii, length;

	document = webkit_dom_html_iframe_element_get_content_document (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe));
	window = webkit_dom_document_get_default_view (document);
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
				document, "DIV", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element),
				WEBKIT_DOM_NODE (fragment), NULL);

			return webkit_dom_html_element_get_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (element));
		}
	}

	frames = webkit_dom_document_get_elements_by_tag_name (
		document, "IFRAME");
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
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMNodeList *frames;
	gulong ii, length;

	document = webkit_dom_html_iframe_element_get_content_document (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (iframe));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (selection && (webkit_dom_dom_selection_get_range_count (selection) > 0)) {
		WebKitDOMRange *range;

		range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
		if (range != NULL)
			return webkit_dom_range_to_string (range, NULL);
	}

	frames = webkit_dom_document_get_elements_by_tag_name (
		document, "IFRAME");
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
		webkit_dom_html_element_set_id (
			WEBKIT_DOM_HTML_ELEMENT (style_element),
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
		WebKitDOMDocument *iframe_document;
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (frames, ii);
		iframe_document = webkit_dom_html_iframe_element_get_content_document (
			WEBKIT_DOM_HTML_IFRAME_ELEMENT (node));

		add_css_rule_into_style_sheet_recursive (
			iframe_document,
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
	id = webkit_dom_html_element_get_id (WEBKIT_DOM_HTML_ELEMENT (event_target));

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

void
e_dom_utils_eab_contact_formatter_bind_dom (WebKitDOMDocument *document)
{
	WebKitDOMNodeList *nodes;
	gulong ii, length;

	nodes = webkit_dom_document_get_elements_by_class_name (
		document, "_evo_collapse_button");

	length = webkit_dom_node_list_get_length (nodes);
	for (ii = 0; ii < length; ii++) {

		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (nodes, ii);
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (node), "click",
			G_CALLBACK (collapse_contacts_list), FALSE, document);
	}
}

