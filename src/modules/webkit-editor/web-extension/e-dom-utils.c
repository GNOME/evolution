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

#include "evolution-config.h"

#include <string.h>

#include <webkitdom/webkitdom.h>

#include "e-dom-utils.h"

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
	}
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

	element_tag = webkit_dom_element_get_tag_name (element);

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
	gchar *element_class, *final_class;
	GRegex *regex;
	gchar *pattern = NULL;

	if (!WEBKIT_DOM_IS_ELEMENT (element))
		return;

	if (!element_has_class (element, class))
		return;

	element_class = webkit_dom_element_get_class_name (element);

	pattern = g_strconcat ("[\\s]*", class, "[\\s]*", NULL);
	regex = g_regex_new (pattern, 0, 0, NULL);
	final_class = g_regex_replace (regex, element_class, -1, 0, " ", 0, NULL);

	if (g_strcmp0 (final_class, " ") != 0)
		webkit_dom_element_set_class_name (element, final_class);
	else
		webkit_dom_element_remove_attribute (element, "class");

	g_free (element_class);
	g_free (final_class);
	g_free (pattern);
	g_regex_unref (regex);
}

void
element_rename_attribute (WebKitDOMElement *element,
                      const gchar *from,
                      const gchar *to)
{
	gchar *value;

	if (!webkit_dom_element_has_attribute (element, from))
		return;

	value = webkit_dom_element_get_attribute (element, from);
	webkit_dom_element_set_attribute (element, to, (value && *value) ? value : "", NULL);
	webkit_dom_element_remove_attribute (element, from);
	g_free (value);
}

void
remove_node (WebKitDOMNode *node)
{
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);

	if (parent)
		webkit_dom_node_remove_child (parent, node, NULL);
}

void
remove_node_if_empty (WebKitDOMNode *node)
{
	WebKitDOMNode *child;

	if (!WEBKIT_DOM_IS_NODE (node))
		return;

	if ((child = webkit_dom_node_get_first_child (node))) {
		WebKitDOMNode *prev_sibling, *next_sibling;

		prev_sibling = webkit_dom_node_get_previous_sibling (child);
		next_sibling = webkit_dom_node_get_next_sibling (child);
		/* Empty or BR as sibling, but no sibling after it. */
		if (!webkit_dom_node_get_first_child (child) &&
		    !WEBKIT_DOM_IS_TEXT (child) &&
		    (!prev_sibling ||
		     (WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling) &&
		      !webkit_dom_node_get_previous_sibling (prev_sibling))) &&
		    (!next_sibling ||
		     (WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling) &&
		      !webkit_dom_node_get_next_sibling (next_sibling)))) {

			remove_node (node);
		} else {
			gchar *text_content;

			text_content = webkit_dom_node_get_text_content (node);
			if (!text_content)
				remove_node (node);

			if (text_content && !*text_content)
				remove_node (node);

			if (g_strcmp0 (text_content, UNICODE_ZERO_WIDTH_SPACE) == 0)
				remove_node (node);

			g_free (text_content);
		}
	} else
		remove_node (node);
}

WebKitDOMNode *
split_list_into_two (WebKitDOMNode *item,
		     gint level)
{
	gint current_level = 1;
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMNode *parent, *prev_parent = NULL, *tmp;

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

		while (tmp && (sibling = webkit_dom_node_get_next_sibling (tmp)))
			webkit_dom_node_append_child (clone, sibling, NULL);

		if (tmp)
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
		prev_parent ? webkit_dom_node_get_next_sibling (prev_parent) : NULL,
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
 * Returns: #EContentEditorBlockFormat
 */
EContentEditorBlockFormat
dom_get_list_format_from_node (WebKitDOMNode *node)
{
	EContentEditorBlockFormat format =
		E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST;

	if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (node))
		return E_CONTENT_EDITOR_BLOCK_FORMAT_NONE;

	if (WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (node))
		return format;

	if (WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (node)) {
		gchar *type_value = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "type");

		if (!type_value)
			return E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST;

		if (!*type_value)
			format = E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST;
		else if (g_ascii_strcasecmp (type_value, "A") == 0)
			format = E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA;
		else if (g_ascii_strcasecmp (type_value, "I") == 0)
			format = E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ROMAN;
		g_free (type_value);

		return format;
	}

	return E_CONTENT_EDITOR_BLOCK_FORMAT_NONE;
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
	EContentEditorBlockFormat format, prev, next;
	gint ii, length;
	WebKitDOMNode *prev_sibling, *next_sibling;
	WebKitDOMNodeList *lists = NULL;

	prev_sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (list));
	next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (list));

	format = dom_get_list_format_from_node (list),
	prev = dom_get_list_format_from_node (prev_sibling);
	next = dom_get_list_format_from_node (next_sibling);

	if (format != E_CONTENT_EDITOR_BLOCK_FORMAT_NONE) {
		if (format == prev && prev != E_CONTENT_EDITOR_BLOCK_FORMAT_NONE)
			merge_list_into_list (prev_sibling, list, TRUE);

		if (format == next && next != E_CONTENT_EDITOR_BLOCK_FORMAT_NONE)
			merge_list_into_list (next_sibling, list, FALSE);
	}

	lists = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (list), "ol + ol, ul + ul", NULL);
	length = webkit_dom_node_list_get_length (lists);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (lists, ii);
		merge_lists_if_possible (node);
	}
	g_clear_object (&lists);
}

WebKitDOMElement *
get_parent_block_element (WebKitDOMNode *node)
{
	WebKitDOMElement *parent = webkit_dom_node_get_parent_element (node);

	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent))
		return WEBKIT_DOM_IS_ELEMENT (node) ? WEBKIT_DOM_ELEMENT (node) : NULL;

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

void
dom_element_swap_attributes (WebKitDOMElement *element,
                             const gchar *from,
                             const gchar *to)
{
	gchar *value_from, *value_to;

	if (!webkit_dom_element_has_attribute (element, from) ||
	    !webkit_dom_element_has_attribute (element, to))
		return;

	value_from = webkit_dom_element_get_attribute (element, from);
	value_to = webkit_dom_element_get_attribute (element, to);
	webkit_dom_element_set_attribute (element, to, (value_from && *value_from) ? value_from : "", NULL);
	webkit_dom_element_set_attribute (element, from, (value_to && *value_to) ? value_to : "", NULL);
	g_free (value_from);
	g_free (value_to);
}
