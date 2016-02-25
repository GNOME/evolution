/*
 * e-html-editor-selection-dom-functions.c
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

#include "e-html-editor-selection-dom-functions.h"

#include "e-html-editor-view-dom-functions.h"
#include "e-html-editor-web-extension.h"

#include <web-extensions/e-dom-utils.h>

#include <string.h>
#include <stdlib.h>

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDocumentFragmentUnstable.h>
#include <webkitdom/WebKitDOMRangeUnstable.h>
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>
#include <webkitdom/WebKitDOMHTMLElementUnstable.h>
#include <webkitdom/WebKitDOMDocumentUnstable.h>

static const GdkRGBA black = { 0, 0, 0, 1 };

void
dom_replace_base64_image_src (WebKitDOMDocument *document,
                              const gchar *selector,
                              const gchar *base64_content,
                              const gchar *filename,
                              const gchar *uri)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_query_selector (document, selector, NULL);

	if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (element))
		webkit_dom_html_image_element_set_src (
			WEBKIT_DOM_HTML_IMAGE_ELEMENT (element),
			base64_content);
	else
		webkit_dom_element_set_attribute (
			element, "background", base64_content, NULL);

	webkit_dom_element_set_attribute (element, "data-uri", uri, NULL);
	webkit_dom_element_set_attribute (element, "data-inline", "", NULL);
	webkit_dom_element_set_attribute (
		element, "data-name", filename ? filename : "", NULL);
}

WebKitDOMRange *
dom_get_current_range (WebKitDOMDocument *document)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range = NULL;

	dom_window = webkit_dom_document_get_default_view (document);
	if (!dom_window)
		return NULL;

	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	if (!WEBKIT_DOM_IS_DOM_SELECTION (dom_selection)) {
		g_object_unref (dom_window);
		return NULL;
	}

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1)
		goto exit;

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
 exit:
	g_object_unref (dom_selection);
	g_object_unref (dom_window);
	return range;
}

/**
 * e_html_editor_selection_get_string:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns currently selected string.
 *
 * Returns: A pointer to content of current selection. The string is owned by
 * #EHTMLEditorSelection and should not be free'd.
 */
gchar *
dom_selection_get_string (WebKitDOMDocument *document,
                          EHTMLEditorWebExtension *extension)
{
	gchar *text;
	WebKitDOMRange *range;

	range = dom_get_current_range (document);
	if (!range)
		return NULL;

	text = webkit_dom_range_get_text (range);
	g_object_unref (range);

	return text;
}

void
dom_move_caret_into_element (WebKitDOMDocument *document,
                             WebKitDOMElement *element,
                             gboolean to_start)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	if (!element)
		return;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	range = webkit_dom_document_create_range (document);

	webkit_dom_range_select_node_contents (
		range, WEBKIT_DOM_NODE (element), NULL);
	webkit_dom_range_collapse (range, to_start, NULL);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	g_object_unref (range);
	g_object_unref (dom_selection);
	g_object_unref (dom_window);
}

void
dom_insert_base64_image (WebKitDOMDocument *document,
                         EHTMLEditorWebExtension *extension,
                         const gchar *filename,
                         const gchar *uri,
                         const gchar *base64_content)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	WebKitDOMElement *element, *selection_start_marker, *resizable_wrapper;
	WebKitDOMText *text;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

	if (!dom_selection_is_collapsed (document)) {
		EHTMLEditorHistoryEvent *ev;
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMRange *range;

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_DELETE;

		range = dom_get_current_range (document);
		fragment = webkit_dom_range_clone_contents (range, NULL);
		g_object_unref (range);
		ev->data.fragment = fragment;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->after.start.x = ev->before.start.x;
		ev->after.start.y = ev->before.start.y;
		ev->after.end.x = ev->before.start.x;
		ev->after.end.y = ev->before.start.y;

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_AND;

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
		dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);
	}

	dom_selection_save (document);
	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);

	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_IMAGE;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
	}

	resizable_wrapper =
		webkit_dom_document_create_element (document, "span", NULL);
	webkit_dom_element_set_attribute (
		resizable_wrapper, "class", "-x-evo-resizable-wrapper", NULL);

	element = webkit_dom_document_create_element (document, "img", NULL);
	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (element),
		base64_content);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-uri", uri, NULL);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-inline", "", NULL);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-name",
		filename ? filename : "", NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (resizable_wrapper),
		WEBKIT_DOM_NODE (element),
		NULL);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (selection_start_marker)),
		WEBKIT_DOM_NODE (resizable_wrapper),
		WEBKIT_DOM_NODE (selection_start_marker),
		NULL);

	/* We have to again use UNICODE_ZERO_WIDTH_SPACE character to restore
	 * caret on right position */
	text = webkit_dom_document_create_text_node (
		document, UNICODE_ZERO_WIDTH_SPACE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (selection_start_marker)),
		WEBKIT_DOM_NODE (text),
		WEBKIT_DOM_NODE (selection_start_marker),
		NULL);

	if (ev) {
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMNode *node;

		fragment = webkit_dom_document_create_document_fragment (document);
		node = webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (resizable_wrapper), TRUE),
			NULL);

		webkit_dom_html_element_insert_adjacent_html (
			WEBKIT_DOM_HTML_ELEMENT (node), "afterend", "&#8203;", NULL);
		ev->data.fragment = fragment;

		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	dom_selection_restore (document);
	dom_force_spell_check_for_current_paragraph (document, extension);
}

/**
 * e_html_editor_selection_unlink:
 * @selection: an #EHTMLEditorSelection
 *
 * Removes any links (&lt;A&gt; elements) from current selection or at current
 * cursor position.
 */
void
dom_selection_unlink (WebKitDOMDocument *document,
                      EHTMLEditorWebExtension *extension)
{
	EHTMLEditorUndoRedoManager *manager;
	gchar *text;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *link;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	link = dom_node_find_parent_element (
		webkit_dom_range_get_start_container (range, NULL), "A");

	g_object_unref (dom_selection);
	g_object_unref (dom_window);

	if (!link) {
		WebKitDOMNode *node;

		/* get element that was clicked on */
		node = webkit_dom_range_get_common_ancestor_container (range, NULL);
		if (node && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
			link = dom_node_find_parent_element (node, "A");
			if (link && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (link)) {
				g_object_unref (range);
				return;
			} else
				link = WEBKIT_DOM_ELEMENT (node);
		}
	} else {
		dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_UNLINK, NULL);
	}

	g_object_unref (range);

	if (!link)
		return;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EHTMLEditorHistoryEvent *ev;
		WebKitDOMDocumentFragment *fragment;

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_REMOVE_LINK;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		fragment = webkit_dom_document_create_document_fragment (document);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (link), TRUE),
			NULL);
		ev->data.fragment = fragment;

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	text = webkit_dom_html_element_get_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (link));
	webkit_dom_element_set_outer_html (link, text, NULL);
	g_free (text);
}

/**
 * e_html_editor_selection_create_link:
 * @document: a @WebKitDOMDocument
 * @uri: destination of the new link
 *
 * Converts current selection into a link pointing to @url.
 */
void
dom_create_link (WebKitDOMDocument *document,
                 EHTMLEditorWebExtension *extension,
                 const gchar *uri)
{
	g_return_if_fail (uri != NULL && *uri != '\0');

	dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_CREATE_LINK, uri);
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

static gboolean
node_is_list_or_item (WebKitDOMNode *node)
{
	return node && (
		WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (node) ||
		WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (node) ||
		WEBKIT_DOM_IS_HTML_LI_ELEMENT (node));
}

static gboolean
node_is_list (WebKitDOMNode *node)
{
	return node && (
		WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (node) ||
		WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (node));
}

static gint
get_list_level (WebKitDOMNode *node)
{
	gint level = 0;

	while (node && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node)) {
		if (node_is_list (node))
			level++;
		node = webkit_dom_node_get_parent_node (node);
	}

	return level;
}

static void
set_ordered_list_type_to_element (WebKitDOMElement *list,
                                  EHTMLEditorSelectionBlockFormat format)
{
	if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST)
		webkit_dom_element_remove_attribute (list, "type");
	else if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA)
		webkit_dom_element_set_attribute (list, "type", "A", NULL);
	else if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN)
		webkit_dom_element_set_attribute (list, "type", "I", NULL);
}

static const gchar *
get_css_alignment_value_class (EHTMLEditorSelectionAlignment alignment)
{
	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT)
		return ""; /* Left is by default on ltr */

	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER)
		return "-x-evo-align-center";

	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT)
		return "-x-evo-align-right";

	return "";
}

/**
 * e_html_editor_selection_get_alignment:
 * @selection: #an EHTMLEditorSelection
 *
 * Returns alignment of current paragraph
 *
 * Returns: #EHTMLEditorSelectionAlignment
 */
static EHTMLEditorSelectionAlignment
dom_get_alignment (WebKitDOMDocument *document)
{
	EHTMLEditorSelectionAlignment alignment;
	gchar *value;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	range = dom_get_current_range (document);
	if (!range)
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;

	node = webkit_dom_range_get_start_container (range, NULL);
	g_object_unref (range);
	if (!node)
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	dom_window = webkit_dom_document_get_default_view (document);
	style = webkit_dom_dom_window_get_computed_style (dom_window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-align");

	if (!value || !*value ||
	    (g_ascii_strncasecmp (value, "left", 4) == 0)) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	} else if (g_ascii_strncasecmp (value, "center", 6) == 0) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER;
	} else if (g_ascii_strncasecmp (value, "right", 5) == 0) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT;
	} else {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	}

	g_object_unref (dom_window);
	g_object_unref (style);
	g_free (value);

	return alignment;
}

static gint
set_word_wrap_length (EHTMLEditorWebExtension *extension,
                      gint user_word_wrap_length)
{
	return (user_word_wrap_length == -1) ?
		e_html_editor_web_extension_get_word_wrap_length (extension) : user_word_wrap_length;
}

void
dom_set_paragraph_style (WebKitDOMDocument *document,
                         EHTMLEditorWebExtension *extension,
                         WebKitDOMElement *element,
                         gint width,
                         gint offset,
                         const gchar *style_to_add)
{
	EHTMLEditorSelectionAlignment alignment;
	char *style = NULL;
	gint word_wrap_length = set_word_wrap_length (extension, width);

	alignment = dom_get_alignment (document);

	element_add_class (element, "-x-evo-paragraph");
	element_add_class (element, get_css_alignment_value_class (alignment));
	if (!e_html_editor_web_extension_get_html_mode (extension)) {
		style = g_strdup_printf (
			"width: %dch; word-wrap: normal; %s",
			(word_wrap_length + offset), style_to_add);
	} else {
		if (*style_to_add)
			style = g_strdup_printf ("%s", style_to_add);
	}
	if (style) {
		webkit_dom_element_set_attribute (element, "style", style, NULL);
		g_free (style);
	}
}

static WebKitDOMElement *
create_list_element (WebKitDOMDocument *document,
                     EHTMLEditorWebExtension *extension,
                     EHTMLEditorSelectionBlockFormat format,
		     gint level,
                     gboolean html_mode)
{
	WebKitDOMElement *list;
	gint offset = -SPACES_PER_LIST_LEVEL;
	gboolean inserting_unordered_list =
		format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;

	list = webkit_dom_document_create_element (
		document, inserting_unordered_list  ? "UL" : "OL", NULL);

	set_ordered_list_type_to_element (list, format);

	if (level >= 0)
		offset = (level + 1) * -SPACES_PER_LIST_LEVEL;

	if (!html_mode)
		dom_set_paragraph_style (document, extension, list, -1, offset, "");

	return list;
}

static void
merge_list_into_list (WebKitDOMNode *from,
                      WebKitDOMNode *to,
                      gboolean insert_before)
{
	WebKitDOMNode *item;

	if (!(to && from))
		return;

	while ((item = webkit_dom_node_get_first_child (from)) != NULL) {
		if (insert_before)
			webkit_dom_node_insert_before (
				to, item, webkit_dom_node_get_last_child (to), NULL);
		else
			webkit_dom_node_append_child (to, item, NULL);
	}

	if (!webkit_dom_node_has_child_nodes (from))
		remove_node (from);

}

static void
merge_lists_if_possible (WebKitDOMNode *list)
{
	EHTMLEditorSelectionBlockFormat format, prev, next;
	WebKitDOMNode *prev_sibling, *next_sibling;

	prev_sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (list));
	next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (list));

	format = dom_get_list_format_from_node (list),
	prev = dom_get_list_format_from_node (prev_sibling);
	next = dom_get_list_format_from_node (next_sibling);

	if (format == prev && format != -1 && prev != -1)
		merge_list_into_list (prev_sibling, list, TRUE);

	if (format == next && format != -1 && next != -1)
		merge_list_into_list (next_sibling, list, FALSE);
}

static void
indent_list (WebKitDOMDocument *document,
             EHTMLEditorWebExtension *extension)
{
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *item, *next_item;
	gboolean after_selection_end = FALSE;

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);

	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	item = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
		gboolean html_mode = e_html_editor_web_extension_get_html_mode (extension);
		WebKitDOMElement *list;
		WebKitDOMNode *source_list = webkit_dom_node_get_parent_node (item);
		EHTMLEditorSelectionBlockFormat format;

		format = dom_get_list_format_from_node (source_list);

		list = create_list_element (
			document, extension, format, get_list_level (item), html_mode);

		element_add_class (list, "-x-evo-indented");

		webkit_dom_node_insert_before (
			source_list, WEBKIT_DOM_NODE (list), item, NULL);

		while (item && !after_selection_end) {
			after_selection_end = webkit_dom_node_contains (
				item, WEBKIT_DOM_NODE (selection_end_marker));

			next_item = webkit_dom_node_get_next_sibling (item);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (list), item, NULL);

			item = next_item;
		}

		merge_lists_if_possible (WEBKIT_DOM_NODE (list));
	}
}

static void
dom_set_indented_style (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension,
                        WebKitDOMElement *element,
                        gint width)
{
	gchar *style;
	gint word_wrap_length = set_word_wrap_length (extension, width);

	webkit_dom_element_set_class_name (element, "-x-evo-indented");

	if (e_html_editor_web_extension_get_html_mode (extension))
		style = g_strdup_printf ("margin-left: %dch;", SPACES_PER_INDENTATION);
	else
		style = g_strdup_printf (
			"margin-left: %dch; word-wrap: normal; width: %dch;",
			SPACES_PER_INDENTATION, word_wrap_length);

	webkit_dom_element_set_attribute (element, "style", style, NULL);
	g_free (style);
}

static WebKitDOMElement *
dom_get_indented_element (WebKitDOMDocument *document,
                          EHTMLEditorWebExtension *extension,
                          gint width)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "DIV", NULL);
	dom_set_indented_style (document, extension, element, width);

	return element;
}

static void
indent_block (WebKitDOMDocument *document,
              EHTMLEditorWebExtension *extension,
              WebKitDOMNode *block,
              gint width)
{
	WebKitDOMElement *element;

	element = dom_get_indented_element (document, extension, width);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (block),
		WEBKIT_DOM_NODE (element),
		block,
		NULL);

	/* Remove style and let the paragraph inherit it from parent */
	if (element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-paragraph"))
		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (block), "style");

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (element),
		block,
		NULL);
}

static gint
get_indentation_level (WebKitDOMElement *element)
{
	WebKitDOMElement *parent;
	gint level = 0;

	if (element_has_class (element, "-x-evo-indented"))
		level++;

	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (element));
	/* Count level of indentation */
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (element_has_class (parent, "-x-evo-indented"))
			level++;

		parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (parent));
	}

	return level;
}

static WebKitDOMNode *
get_parent_indented_block (WebKitDOMNode *node)
{
	WebKitDOMNode *parent, *block = NULL;

	parent = webkit_dom_node_get_parent_node (node);
	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-indented"))
		block = parent;

	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-indented"))
			block = parent;
		parent = webkit_dom_node_get_parent_node (parent);
	}

	return block;
}

static WebKitDOMElement*
get_element_for_inspection (WebKitDOMRange *range)
{
	WebKitDOMNode *node;

	node = webkit_dom_range_get_end_container (range, NULL);
	/* No selection or whole body selected */
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node))
		return NULL;

	return WEBKIT_DOM_ELEMENT (get_parent_indented_block (node));
}

static EHTMLEditorSelectionAlignment
dom_get_alignment_from_node (WebKitDOMNode *node)
{
	EHTMLEditorSelectionAlignment alignment;
	gchar *value;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;

	document = webkit_dom_node_get_owner_document (node);
	dom_window = webkit_dom_document_get_default_view (document);

	style = webkit_dom_dom_window_get_computed_style (
		dom_window, WEBKIT_DOM_ELEMENT (node), NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-align");

	if (!value || !*value ||
	    (g_ascii_strncasecmp (value, "left", 4) == 0)) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	} else if (g_ascii_strncasecmp (value, "center", 6) == 0) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER;
	} else if (g_ascii_strncasecmp (value, "right", 5) == 0) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT;
	} else {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	}

	g_object_unref (dom_window);
	g_object_unref (style);
	g_free (value);

	return alignment;
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

static void
remove_selection_markers (WebKitDOMDocument *document)
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

	remove_selection_markers (document);
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

	remove_selection_markers (document);
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

/**
 * e_html_editor_selection_indent:
 * @selection: an #EHTMLEditorSelection
 *
 * Indents current paragraph by one level.
 */
void
dom_selection_indent (WebKitDOMDocument *document,
                      EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	gboolean after_selection_start = FALSE, after_selection_end = FALSE;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;

	dom_selection_save (document);

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);

	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	/* If the selection was not saved, move it into the first child of body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMHTMLElement *body;
		WebKitDOMNode *child;

		body = webkit_dom_document_get_body (document);
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_INDENT;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.style.from = 1;
		ev->data.style.to = 1;
	}

	block = get_parent_indented_block (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (!block)
		block = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

	while (block && !after_selection_end) {
		gint ii, length, level, word_wrap_length, final_width = 0;
		WebKitDOMNode *next_block;
		WebKitDOMNodeList *list;

		word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);

		next_block = webkit_dom_node_get_next_sibling (block);

		list = webkit_dom_element_query_selector_all (
			WEBKIT_DOM_ELEMENT (block),
			".-x-evo-indented > *:not(.-x-evo-indented):not(li)",
			NULL);

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		length = webkit_dom_node_list_get_length (list);
		if (length == 0 && node_is_list_or_item (block)) {
			indent_list (document, extension);
			goto next;
		}

		if (length == 0) {
			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block, WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start)
					goto next;
			}

			level = get_indentation_level (WEBKIT_DOM_ELEMENT (block));

			final_width = word_wrap_length - SPACES_PER_INDENTATION * (level + 1);
			if (final_width < MINIMAL_PARAGRAPH_WIDTH &&
			    !e_html_editor_web_extension_get_html_mode (extension))
				goto next;

			indent_block (document, extension, block, final_width);

			if (after_selection_end)
				goto next;
		}

		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *block_to_process;

			block_to_process = webkit_dom_node_list_item (list, ii);

			after_selection_end = webkit_dom_node_contains (
				block_to_process, WEBKIT_DOM_NODE (selection_end_marker));

			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block_to_process,
					WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start) {
					g_object_unref (block_to_process);
					continue;
				}
			}

			level = get_indentation_level (
				WEBKIT_DOM_ELEMENT (block_to_process));

			final_width = word_wrap_length - SPACES_PER_INDENTATION * (level + 1);
			if (final_width < MINIMAL_PARAGRAPH_WIDTH &&
			    !e_html_editor_web_extension_get_html_mode (extension)) {
				g_object_unref (block_to_process);
				continue;
			}

			indent_block (document, extension, block_to_process, final_width);

			g_object_unref (block_to_process);
			if (after_selection_end)
				break;
		}

 next:
		g_object_unref (list);

		if (!after_selection_end)
			block = next_block;
	}

	if (ev) {
		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	dom_selection_restore (document);
	dom_force_spell_check_for_current_paragraph (document, extension);

	set_dbus_property_boolean (extension, "Indented", TRUE);
}

static void
unindent_list (WebKitDOMDocument *document)
{
	gboolean after = FALSE;
	WebKitDOMElement *new_list;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *source_list, *source_list_clone, *current_list, *item;
	WebKitDOMNode *prev_item;

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return;

	/* Copy elements from previous block to list */
	item = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));
	source_list = webkit_dom_node_get_parent_node (item);
	new_list = WEBKIT_DOM_ELEMENT (
		webkit_dom_node_clone_node (source_list, FALSE));
	current_list = source_list;
	source_list_clone = webkit_dom_node_clone_node (source_list, FALSE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (source_list),
		WEBKIT_DOM_NODE (source_list_clone),
		webkit_dom_node_get_next_sibling (source_list),
		NULL);

	if (element_has_class (WEBKIT_DOM_ELEMENT (source_list), "-x-evo-indented"))
		element_add_class (WEBKIT_DOM_ELEMENT (new_list), "-x-evo-indented");

	prev_item = source_list;

	while (item) {
		WebKitDOMNode *next_item = webkit_dom_node_get_next_sibling (item);

		if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
			if (after)
				prev_item = webkit_dom_node_append_child (
					source_list_clone, WEBKIT_DOM_NODE (item), NULL);
			else
				prev_item = webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (prev_item),
					item,
					webkit_dom_node_get_next_sibling (prev_item),
					NULL);
		}

		if (webkit_dom_node_contains (item, WEBKIT_DOM_NODE (selection_end_marker)))
			after = TRUE;

		if (!next_item) {
			if (after)
				break;

			current_list = webkit_dom_node_get_next_sibling (current_list);
			next_item = webkit_dom_node_get_first_child (current_list);
		}
		item = next_item;
	}

	remove_node_if_empty (source_list_clone);
	remove_node_if_empty (source_list);
}

static void
unindent_block (WebKitDOMDocument *document,
                EHTMLEditorWebExtension *extension,
                WebKitDOMNode *block)
{
	gboolean before_node = TRUE;
	gint word_wrap_length, level, width;
	EHTMLEditorSelectionAlignment alignment;
	WebKitDOMElement *element;
	WebKitDOMElement *prev_blockquote = NULL, *next_blockquote = NULL;
	WebKitDOMNode *block_to_process, *node_clone = NULL, *child;

	block_to_process = block;

	alignment = dom_get_alignment_from_node (block_to_process);

	element = webkit_dom_node_get_parent_element (block_to_process);

	if (!WEBKIT_DOM_IS_HTML_DIV_ELEMENT (element) &&
	    !element_has_class (element, "-x-evo-indented"))
		return;

	element_add_class (WEBKIT_DOM_ELEMENT (block_to_process), "-x-evo-to-unindent");

	level = get_indentation_level (element);
	word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);
	width = word_wrap_length - SPACES_PER_INDENTATION * level;

	/* Look if we have previous siblings, if so, we have to
	 * create new blockquote that will include them */
	if (webkit_dom_node_get_previous_sibling (block_to_process))
		prev_blockquote = dom_get_indented_element (document, extension, width);

	/* Look if we have next siblings, if so, we have to
	 * create new blockquote that will include them */
	if (webkit_dom_node_get_next_sibling (block_to_process))
		next_blockquote = dom_get_indented_element (document, extension, width);

	/* Copy nodes that are before / after the element that we want to unindent */
	while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)))) {
		if (webkit_dom_node_is_equal_node (child, block_to_process)) {
			before_node = FALSE;
			node_clone = webkit_dom_node_clone_node (child, TRUE);
			remove_node (child);
			continue;
		}

		webkit_dom_node_append_child (
			before_node ?
				WEBKIT_DOM_NODE (prev_blockquote) :
				WEBKIT_DOM_NODE (next_blockquote),
			child,
			NULL);
	}

	if (node_clone) {
		element_remove_class (WEBKIT_DOM_ELEMENT (node_clone), "-x-evo-to-unindent");

		/* Insert blockqoute with nodes that were before the element that we want to unindent */
		if (prev_blockquote) {
			if (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (prev_blockquote))) {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
					WEBKIT_DOM_NODE (prev_blockquote),
					WEBKIT_DOM_NODE (element),
					NULL);
			}
		}

		if (level == 1 && element_has_class (WEBKIT_DOM_ELEMENT (node_clone), "-x-evo-paragraph")) {
			dom_set_paragraph_style (
				document, extension, WEBKIT_DOM_ELEMENT (node_clone), word_wrap_length, 0, "");
			element_add_class (
				WEBKIT_DOM_ELEMENT (node_clone),
				get_css_alignment_value_class (alignment));
		}

		/* Insert the unindented element */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			node_clone,
			WEBKIT_DOM_NODE (element),
			NULL);
	} else {
		g_warn_if_reached ();
	}

	/* Insert blockqoute with nodes that were after the element that we want to unindent */
	if (next_blockquote) {
		if (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (next_blockquote))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
				WEBKIT_DOM_NODE (next_blockquote),
				WEBKIT_DOM_NODE (element),
				NULL);
		}
	}

	/* Remove old blockquote */
	remove_node (WEBKIT_DOM_NODE (element));
}

/**
 * dom_unindent:
 * @selection: an #EHTMLEditorSelection
 *
 * Unindents current paragraph by one level.
 */
void
dom_selection_unindent (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	gboolean after_selection_start = FALSE, after_selection_end = FALSE;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	/* If the selection was not saved, move it into the first child of body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMHTMLElement *body;
		WebKitDOMNode *child;

		body = webkit_dom_document_get_body (document);
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_INDENT;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
	}

	block = get_parent_indented_block (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (!block)
		block = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

	while (block && !after_selection_end) {
		gint ii, length;
		WebKitDOMNode *next_block;
		WebKitDOMNodeList *list;

		next_block = webkit_dom_node_get_next_sibling (block);

		list = webkit_dom_element_query_selector_all (
			WEBKIT_DOM_ELEMENT (block),
			".-x-evo-indented > *:not(.-x-evo-indented):not(li)",
			NULL);

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		length = webkit_dom_node_list_get_length (list);
		if (length == 0 && node_is_list_or_item (block)) {
			unindent_list (document);
			goto next;
		}

		if (length == 0) {
			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block, WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start)
					goto next;
			}

			unindent_block (document, extension, block);

			if (after_selection_end)
				goto next;
		}

		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *block_to_process;

			block_to_process = webkit_dom_node_list_item (list, ii);

			after_selection_end = webkit_dom_node_contains (
				block_to_process,
				WEBKIT_DOM_NODE (selection_end_marker));

			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block_to_process,
					WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start) {
					g_object_unref (block_to_process);
					continue;
				}
			}

			unindent_block (document, extension, block_to_process);

			g_object_unref (block_to_process);
			if (after_selection_end)
				break;
		}
 next:
		g_object_unref (list);
		block = next_block;
	}

	if (ev) {
		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	dom_selection_restore (document);

	dom_force_spell_check_for_current_paragraph (document, extension);

	/* FIXME XXX - Check if the block is still indented */
	set_dbus_property_boolean (extension, "Indented", TRUE);
}

static WebKitDOMNode *
in_empty_block_in_quoted_content (WebKitDOMNode *element)
{
	WebKitDOMNode *first_child, *next_sibling;

	first_child = webkit_dom_node_get_first_child (element);
	if (!WEBKIT_DOM_IS_ELEMENT (first_child))
		return NULL;

	if (!element_has_class (WEBKIT_DOM_ELEMENT (first_child), "-x-evo-quoted"))
		return NULL;

	next_sibling = webkit_dom_node_get_next_sibling (first_child);
	if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling))
		return next_sibling;

	if (!WEBKIT_DOM_IS_ELEMENT (next_sibling))
		return NULL;

	if (!element_has_id (WEBKIT_DOM_ELEMENT (next_sibling), "-x-evo-selection-start-marker"))
		return NULL;

	next_sibling = webkit_dom_node_get_next_sibling (next_sibling);
	if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling))
		return next_sibling;

	return NULL;
}

/**
 * e_html_editor_selection_save:
 * @selection: an #EHTMLEditorSelection
 *
 * Saves current cursor position or current selection range. The selection can
 * be later restored by calling e_html_editor_selection_restore().
 *
 * Note that calling e_html_editor_selection_save() overwrites previously saved
 * position.
 *
 * Note that this method inserts special markings into the HTML code that are
 * used to later restore the selection. It can happen that by deleting some
 * segments of the document some of the markings are deleted too. In that case
 * restoring the selection by e_html_editor_selection_restore() can fail. Also by
 * moving text segments (Cut & Paste) can result in moving the markings
 * elsewhere, thus e_html_editor_selection_restore() will restore the selection
 * incorrectly.
 *
 * It is recommended to use this method only when you are not planning to make
 * bigger changes to content or structure of the document (formatting changes
 * are usually OK).
 */
void
dom_selection_save (WebKitDOMDocument *document)
{
	gboolean collapsed = FALSE;
	glong offset, anchor_offset;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMNode *container, *next_sibling, *marker_node;
	WebKitDOMNode *split_node, *parent_node, *anchor;
	WebKitDOMElement *start_marker = NULL, *end_marker = NULL;

	/* First remove all markers (if present) */
	remove_selection_markers (document);

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_object_unref (dom_selection);
		g_object_unref (dom_window);
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	if (!range) {
		g_object_unref (dom_selection);
		g_object_unref (dom_window);
		return;
	}

	anchor = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);

	collapsed = webkit_dom_range_get_collapsed (range, NULL);
	start_marker = dom_create_selection_marker (document, TRUE);

	container = webkit_dom_range_get_start_container (range, NULL);
	offset = webkit_dom_range_get_start_offset (range, NULL);
	parent_node = webkit_dom_node_get_parent_node (container);

	if (webkit_dom_node_is_same_node (anchor, container) && offset == anchor_offset)
		webkit_dom_element_set_attribute (start_marker, "data-anchor", "", NULL);

	if (element_has_class (WEBKIT_DOM_ELEMENT (parent_node), "-x-evo-quote-character")) {
		WebKitDOMNode *node;

		node = webkit_dom_node_get_parent_node (
		webkit_dom_node_get_parent_node (parent_node));

		if ((next_sibling = in_empty_block_in_quoted_content (node))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (next_sibling),
				WEBKIT_DOM_NODE (start_marker),
				next_sibling,
				NULL);
			goto insert_end_marker;
		}
	} else if (element_has_class (WEBKIT_DOM_ELEMENT (parent_node), "-x-evo-smiley-text")) {
		WebKitDOMNode *node;

		node = webkit_dom_node_get_parent_node (parent_node);
		if (offset == 0) {
			marker_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (node),
				WEBKIT_DOM_NODE (start_marker),
				webkit_dom_node_get_next_sibling (node),
				NULL);
			goto insert_end_marker;
		}
	}

	if (WEBKIT_DOM_IS_TEXT (container)) {
		if (offset != 0) {
			WebKitDOMText *split_text;

			split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (container), offset, NULL);
			split_node = WEBKIT_DOM_NODE (split_text);
		} else {
			marker_node = webkit_dom_node_insert_before (
				parent_node,
				WEBKIT_DOM_NODE (start_marker),
				container,
				NULL);
			goto insert_end_marker;
		}
	} else if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (container)) {
		marker_node = webkit_dom_node_insert_before (
			container,
			WEBKIT_DOM_NODE (start_marker),
			webkit_dom_node_get_first_child (container),
			NULL);
		goto insert_end_marker;
	} else if (element_has_class (WEBKIT_DOM_ELEMENT (container), "-x-evo-resizable-wrapper")) {
		marker_node = webkit_dom_node_insert_before (
			parent_node,
			WEBKIT_DOM_NODE (start_marker),
			webkit_dom_node_get_next_sibling (container),
			NULL);
		goto insert_end_marker;
	} else if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (container)) {
		marker_node = webkit_dom_node_insert_before (
			container,
			WEBKIT_DOM_NODE (start_marker),
			webkit_dom_node_get_first_child (container),
			NULL);
		goto insert_end_marker;
	} else {
		/* Insert the selection marker on the right position in
		 * an empty paragraph in the quoted content */
		if ((next_sibling = in_empty_block_in_quoted_content (container))) {
			marker_node = webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (start_marker),
				next_sibling,
				NULL);
			goto insert_end_marker;
		}
		if (!webkit_dom_node_get_previous_sibling (container)) {
			marker_node = webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (start_marker),
				webkit_dom_node_get_first_child (container),
				NULL);
			goto insert_end_marker;
		} else if (!webkit_dom_node_get_next_sibling (container)) {
			WebKitDOMNode *tmp;

			tmp = webkit_dom_node_get_last_child (container);
			if (tmp && WEBKIT_DOM_IS_HTML_BR_ELEMENT (tmp))
				marker_node = webkit_dom_node_insert_before (
					container,
					WEBKIT_DOM_NODE (start_marker),
					tmp,
					NULL);
			else
				marker_node = webkit_dom_node_append_child (
					container,
					WEBKIT_DOM_NODE (start_marker),
					NULL);
			goto insert_end_marker;
		} else {
			if (webkit_dom_node_get_first_child (container)) {
				marker_node = webkit_dom_node_insert_before (
					container,
					WEBKIT_DOM_NODE (start_marker),
					webkit_dom_node_get_first_child (container),
					NULL);
				goto insert_end_marker;
			}
			split_node = container;
		}
	}

	/* Don't save selection straight into body */
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (split_node)) {
		g_object_unref (range);
		g_object_unref (dom_selection);
		g_object_unref (dom_window);
		return;
	}

	if (!split_node) {
		marker_node = webkit_dom_node_insert_before (
			container,
			WEBKIT_DOM_NODE (start_marker),
			webkit_dom_node_get_first_child (
				WEBKIT_DOM_NODE (container)),
			NULL);
	} else {
		marker_node = WEBKIT_DOM_NODE (start_marker);
		parent_node = webkit_dom_node_get_parent_node (split_node);

		webkit_dom_node_insert_before (
			parent_node, marker_node, split_node, NULL);
	}

	webkit_dom_node_normalize (parent_node);

 insert_end_marker:
	end_marker = dom_create_selection_marker (document, FALSE);

	if (collapsed) {
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (start_marker)),
			WEBKIT_DOM_NODE (end_marker),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (start_marker)),
			NULL);
		goto out;
	}

	container = webkit_dom_range_get_end_container (range, NULL);
	offset = webkit_dom_range_get_end_offset (range, NULL);
	parent_node = webkit_dom_node_get_parent_node (container);

	if (webkit_dom_node_is_same_node (anchor, container) && offset == anchor_offset)
		webkit_dom_element_set_attribute (end_marker, "data-anchor", "", NULL);

	if (element_has_class (WEBKIT_DOM_ELEMENT (parent_node), "-x-evo-quote-character")) {
		WebKitDOMNode *node;

		node = webkit_dom_node_get_parent_node (
		webkit_dom_node_get_parent_node (parent_node));

		if ((next_sibling = in_empty_block_in_quoted_content (node))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (next_sibling),
				WEBKIT_DOM_NODE (end_marker),
				next_sibling,
				NULL);
		} else {
			webkit_dom_node_insert_before (
				node,
				WEBKIT_DOM_NODE (end_marker),
				webkit_dom_node_get_next_sibling (
					webkit_dom_node_get_parent_node (parent_node)),
				NULL);
		}
		goto out;
	}

	if (WEBKIT_DOM_IS_TEXT (container)) {
		if (offset != 0) {
			WebKitDOMText *split_text;

			split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (container), offset, NULL);
			split_node = WEBKIT_DOM_NODE (split_text);
		} else {
			marker_node = webkit_dom_node_insert_before (
				parent_node, WEBKIT_DOM_NODE (end_marker), container, NULL);
			goto check;

		}
	} else if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (container)) {
		webkit_dom_node_append_child (
			container, WEBKIT_DOM_NODE (end_marker), NULL);
		goto out;
	} else {
		/* Insert the selection marker on the right position in
		 * an empty paragraph in the quoted content */
		if ((next_sibling = in_empty_block_in_quoted_content (container))) {
			webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (end_marker),
				next_sibling,
				NULL);
			goto out;
		}
		if (!webkit_dom_node_get_previous_sibling (container)) {
			split_node = parent_node;
		} else if (!webkit_dom_node_get_next_sibling (container)) {
			split_node = parent_node;
			split_node = webkit_dom_node_get_next_sibling (split_node);
		} else
			split_node = container;
	}

	/* Don't save selection straight into body */
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (split_node)) {
		remove_node (WEBKIT_DOM_NODE (start_marker));
		return;
	}

	marker_node = WEBKIT_DOM_NODE (end_marker);

	if (split_node) {
		parent_node = webkit_dom_node_get_parent_node (split_node);

		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent_node)) {
			if (offset == 0)
				webkit_dom_node_insert_before (
					split_node,
					marker_node,
					webkit_dom_node_get_first_child (split_node),
					NULL);
			else
				webkit_dom_node_append_child (
					webkit_dom_node_get_previous_sibling (split_node),
					marker_node,
					NULL);
		} else
			webkit_dom_node_insert_before (
				parent_node, marker_node, split_node, NULL);
	} else {
		 WebKitDOMNode *first_child;

		first_child = webkit_dom_node_get_first_child (container);
		if (offset == 0 && WEBKIT_DOM_IS_TEXT (first_child))
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (container), marker_node, webkit_dom_node_get_first_child (container), NULL);
		else
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (container), marker_node, NULL);
	}

	 webkit_dom_node_normalize (parent_node);

 check:
	if ((next_sibling = webkit_dom_node_get_next_sibling (marker_node))) {
		if (!WEBKIT_DOM_IS_ELEMENT (next_sibling))
			next_sibling = webkit_dom_node_get_next_sibling (next_sibling);
		/* If the selection is collapsed ensure that the selection start marker
		 * is before the end marker */
		if (next_sibling && webkit_dom_node_is_same_node (next_sibling, WEBKIT_DOM_NODE (start_marker))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (marker_node),
				next_sibling,
				marker_node,
				NULL);
		}
	}
 out:
	if (!collapsed) {
		if (start_marker && end_marker) {
			webkit_dom_range_set_start_after (range, WEBKIT_DOM_NODE (start_marker), NULL);
			webkit_dom_range_set_end_before (range, WEBKIT_DOM_NODE (end_marker), NULL);
		} else {
			g_warn_if_reached ();
		}

		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
	}

	g_object_unref (range);
	g_object_unref (dom_selection);
	g_object_unref (dom_window);
}

static gboolean
is_selection_position_node (WebKitDOMNode *node)
{
	WebKitDOMElement *element;

	if (!node || !WEBKIT_DOM_IS_ELEMENT (node))
		return FALSE;

	element = WEBKIT_DOM_ELEMENT (node);

	return element_has_id (element, "-x-evo-selection-start-marker") ||
	       element_has_id (element, "-x-evo-selection-end-marker");
}

/**
 * e_html_editor_selection_restore:
 * @selection: an #EHTMLEditorSelection
 *
 * Restores cursor position or selection range that was saved by
 * e_html_editor_selection_save().
 *
 * Note that calling this function without calling e_html_editor_selection_save()
 * before is a programming error and the behavior is undefined.
 */
void
dom_selection_restore (WebKitDOMDocument *document)
{
	gboolean start_is_anchor = FALSE;
	glong offset;
	WebKitDOMElement *marker;
	WebKitDOMNode *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *parent_start, *parent_end, *anchor;
	WebKitDOMRange *range;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	g_object_unref (dom_window);
	if (!range) {
		WebKitDOMHTMLElement *body;

		range = webkit_dom_document_create_range (document);
		body = webkit_dom_document_get_body (document);

		webkit_dom_range_select_node_contents (range, WEBKIT_DOM_NODE (body), NULL);
		webkit_dom_range_collapse (range, TRUE, NULL);
		webkit_dom_dom_selection_add_range (dom_selection, range);
	}

	selection_start_marker = webkit_dom_range_get_start_container (range, NULL);
	if (selection_start_marker) {
		gboolean ok = FALSE;
		selection_start_marker =
			webkit_dom_node_get_next_sibling (selection_start_marker);

		ok = is_selection_position_node (selection_start_marker);

		if (ok) {
			ok = FALSE;
			if (webkit_dom_range_get_collapsed (range, NULL)) {
				selection_end_marker = webkit_dom_node_get_next_sibling (
					selection_start_marker);

				ok = is_selection_position_node (selection_end_marker);
				if (ok) {
					parent_start = webkit_dom_node_get_parent_node (selection_end_marker);

					remove_node (selection_start_marker);
					remove_node (selection_end_marker);

					webkit_dom_node_normalize (parent_start);
					g_object_unref (range);
					g_object_unref (dom_selection);
					return;
				}
			}
		}
	}

	g_object_unref (range);
	range = webkit_dom_document_create_range (document);
	if (!range) {
		g_object_unref (dom_selection);
		return;
	}

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (!marker) {
		marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		if (marker)
			remove_node (WEBKIT_DOM_NODE (marker));
		g_object_unref (dom_selection);
		g_object_unref (range);
		return;
	}

	start_is_anchor = webkit_dom_element_has_attribute (marker, "data-anchor");
	parent_start = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (marker));

	webkit_dom_range_set_start_after (range, WEBKIT_DOM_NODE (marker), NULL);
	remove_node (WEBKIT_DOM_NODE (marker));

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");
	if (!marker) {
		marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (marker)
			remove_node (WEBKIT_DOM_NODE (marker));
		g_object_unref (dom_selection);
		g_object_unref (range);
		return;
	}

	parent_end = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (marker));

	webkit_dom_range_set_end_before (range, WEBKIT_DOM_NODE (marker), NULL);
	remove_node (WEBKIT_DOM_NODE (marker));

	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	if (webkit_dom_node_is_same_node (parent_start, parent_end))
		webkit_dom_node_normalize (parent_start);
	else {
		webkit_dom_node_normalize (parent_start);
		webkit_dom_node_normalize (parent_end);
	}

	if (start_is_anchor) {
		anchor = webkit_dom_range_get_end_container (range, NULL);
		offset = webkit_dom_range_get_end_offset (range, NULL);

		webkit_dom_range_collapse (range, TRUE, NULL);
	} else {
		anchor = webkit_dom_range_get_start_container (range, NULL);
		offset = webkit_dom_range_get_start_offset (range, NULL);

		webkit_dom_range_collapse (range, FALSE, NULL);
	}
	webkit_dom_dom_selection_add_range (dom_selection, range);
	webkit_dom_dom_selection_extend (dom_selection, anchor, offset, NULL);

	g_object_unref (dom_selection);
	g_object_unref (range);
}

static gint
find_where_to_break_line (WebKitDOMNode *node,
                          gint max_len,
			  gint word_wrap_length)
{
	gchar *str, *text_start;
	gunichar uc;
	gint pos = 1;
	gint last_space = 0;
	gint length;
	gint ret_val = 0;
	gchar* position = NULL;

	text_start =  webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (node));
	length = g_utf8_strlen (text_start, -1);

	str = text_start;
	do {
		uc = g_utf8_get_char (str);
		if (!uc) {
			ret_val = pos <= max_len ? pos : last_space > 0 ? last_space - 1 : 0;
			goto out;
		}

		if (g_unichar_isspace (uc) || str[0] == '-')
			last_space = pos;

		/* If last_space is zero then the word is longer than
		 * word_wrap_length characters, so continue until we find
		 * a space */
		if ((pos > max_len) && (last_space > 0)) {
			if (last_space > word_wrap_length) {
				ret_val = last_space > 0 ? last_space - 1 : 0;
				goto out;
			}

			if (last_space > max_len) {
				if (g_unichar_isspace (g_utf8_get_char (text_start)))
					ret_val = 1;

				goto out;
			}

			if (last_space == max_len - 1) {
				uc = g_utf8_get_char (str);
				if (g_unichar_isspace (uc) || str[0] == '-')
					last_space++;
			}

			ret_val = last_space > 0 ? last_space - 1 : 0;
			goto out;
		}

		pos += 1;
		str = g_utf8_next_char (str);
	} while (*str);

	if (max_len <= length)
		position = g_utf8_offset_to_pointer (text_start, max_len);

	if (position && g_unichar_isspace (g_utf8_get_char (position))) {
		ret_val = max_len;
	} else {
		if (last_space == 0) {
			/* If word is longer than word_wrap_length, we have to
			 * split at maximal given length. */
			ret_val = max_len;
		} else if (last_space < max_len) {
			ret_val = last_space > 0 ? last_space - 1 : 0;
		} else {
			if (length > word_wrap_length)
				ret_val = last_space > 0 ? last_space - 1 : 0;
			else
				ret_val = 0;
		}
	}

 out:
	g_free (text_start);

	/* No space found, split at max_len. */
	if (ret_val == 0)
		ret_val = max_len;

	return ret_val;
}

/**
 * e_html_editor_selection_is_collapsed:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns if selection is collapsed.
 *
 * Returns: Whether the selection is collapsed (just caret) or not (someting is selected).
 */
gboolean
dom_selection_is_collapsed (WebKitDOMDocument *document)
{
	gboolean collapsed;
	WebKitDOMRange *range;

	range = dom_get_current_range (document);
	if (!range)
		return TRUE;

	collapsed = webkit_dom_range_get_collapsed (range, NULL);
	g_object_unref (range);

	return collapsed;
}

void
dom_scroll_to_caret (WebKitDOMDocument *document)
{
	glong element_top, element_left;
	glong window_top, window_left, window_right, window_bottom;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMElement *selection_start_marker;

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (!selection_start_marker)
		return;

	dom_window = webkit_dom_document_get_default_view (document);

	window_top = webkit_dom_dom_window_get_scroll_y (dom_window);
	window_left = webkit_dom_dom_window_get_scroll_x (dom_window);
	window_bottom = window_top + webkit_dom_dom_window_get_inner_height (dom_window);
	window_right = window_left + webkit_dom_dom_window_get_inner_width (dom_window);

	element_left = webkit_dom_element_get_offset_left (selection_start_marker);
	element_top = webkit_dom_element_get_offset_top (selection_start_marker);

	/* Check if caret is inside viewport, if not move to it */
	if (!(element_top >= window_top && element_top <= window_bottom &&
	     element_left >= window_left && element_left <= window_right)) {
		webkit_dom_element_scroll_into_view (selection_start_marker, TRUE);
	}

	dom_selection_restore (document);

	g_object_unref (dom_window);
}

static void
mark_and_remove_leading_space (WebKitDOMDocument *document,
                               WebKitDOMNode *node)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_attribute (element, "data-hidden-space", "", NULL);
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (node),
		WEBKIT_DOM_NODE (element),
		node,
		NULL);
	webkit_dom_character_data_replace_data (
		WEBKIT_DOM_CHARACTER_DATA (node), 0, 1, "", NULL);
}

static WebKitDOMElement *
wrap_lines (WebKitDOMDocument *document,
            EHTMLEditorWebExtension *extension,
            WebKitDOMNode *block,
	    gboolean remove_all_br,
	    gint word_wrap_length)
{
	WebKitDOMNode *node, *start_node, *block_clone;
	WebKitDOMElement *element;
	gboolean has_selection;
	guint line_length;
	gulong length_left;
	gchar *text_content;

	has_selection = !dom_selection_is_collapsed (document);

	if (has_selection) {
		gint ii, length;
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMNodeList *list;
		WebKitDOMRange *range;

		range = dom_get_current_range (document);
		fragment = webkit_dom_range_clone_contents (range, NULL);
		g_object_unref (range);

		/* Select all BR elements or just ours that are used for wrapping.
		 * We are not removing user BR elements when this function is activated
		 * from Format->Wrap Lines action */
		list = webkit_dom_document_fragment_query_selector_all (
			fragment,
			remove_all_br ? "br" : "br.-x-evo-wrap-br",
			NULL);
		length = webkit_dom_node_list_get_length (list);
		/* And remove them */
		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *node = webkit_dom_node_list_item (list, length);
			remove_node (node);
			g_object_unref (node);
		}
		g_object_unref (list);

		list = webkit_dom_document_fragment_query_selector_all (
			fragment, "span[data-hidden-space]", NULL);
		length = webkit_dom_node_list_get_length (list);
		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *hidden_space_node;

			hidden_space_node = webkit_dom_node_list_item (list, ii);
			webkit_dom_html_element_set_outer_text (
				WEBKIT_DOM_HTML_ELEMENT (hidden_space_node), " ", NULL);
			g_object_unref (hidden_space_node);
		}
		g_object_unref (list);

		node = WEBKIT_DOM_NODE (fragment);
		start_node = node;
	} else {
		if (!webkit_dom_node_has_child_nodes (block))
			return WEBKIT_DOM_ELEMENT (block);

		block_clone = webkit_dom_node_clone_node (block, TRUE);
		element = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block_clone),
			"span#-x-evo-caret-position",
			NULL);

		/* When we wrap, we are wrapping just the text after caret, text
		 * before the caret is already wrapped, so unwrap the text after
		 * the caret position */
		element = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block_clone),
			"span#-x-evo-selection-end-marker",
			NULL);

		if (element) {
			WebKitDOMNode *nd = WEBKIT_DOM_NODE (element);

			while (nd) {
				WebKitDOMNode *next_nd = webkit_dom_node_get_next_sibling (nd);
				if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (nd)) {
					if (remove_all_br)
						remove_node (nd);
					else if (element_has_class (WEBKIT_DOM_ELEMENT (nd), "-x-evo-wrap-br"))
						remove_node (nd);
				}
				nd = next_nd;
			}
		}

		webkit_dom_node_normalize (block_clone);
		node = webkit_dom_node_get_first_child (block_clone);
		if (node) {
			text_content = webkit_dom_node_get_text_content (node);
			if (g_strcmp0 ("\n", text_content) == 0)
				node = webkit_dom_node_get_next_sibling (node);
			g_free (text_content);
		}

		if (block_clone && WEBKIT_DOM_IS_ELEMENT (block_clone)) {
			gint ii, length;
			WebKitDOMNodeList *list;

			list = webkit_dom_element_query_selector_all (
				WEBKIT_DOM_ELEMENT (block_clone), "span[data-hidden-space]", NULL);
			length = webkit_dom_node_list_get_length (list);
			for (ii = 0; ii < length; ii++) {
				WebKitDOMNode *hidden_space_node;

				hidden_space_node = webkit_dom_node_list_item (list, ii);
				webkit_dom_html_element_set_outer_text (
					WEBKIT_DOM_HTML_ELEMENT (hidden_space_node), " ", NULL);
				g_object_unref (hidden_space_node);
			}
			g_object_unref (list);
		}

		/* We have to start from the end of the last wrapped line */
		element = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block_clone),
			"span#-x-evo-selection-start-marker",
			NULL);

		if (element) {
			WebKitDOMNode *nd = WEBKIT_DOM_NODE (element);

			while ((nd = webkit_dom_node_get_previous_sibling (nd))) {
				if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (nd)) {
					element = WEBKIT_DOM_ELEMENT (nd);
					break;
				} else
					element = NULL;
			}
		}

		if (element) {
			node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));
			start_node = block_clone;
		} else
			start_node = node;
	}

	line_length = 0;
	while (node) {
		gint offset = 0;

		if (WEBKIT_DOM_IS_TEXT (node)) {
			const gchar *newline;
			WebKitDOMNode *next_sibling;

			/* If there is temporary hidden space we remove it */
			text_content = webkit_dom_node_get_text_content (node);
			if (strstr (text_content, UNICODE_ZERO_WIDTH_SPACE)) {
				if (g_str_has_prefix (text_content, UNICODE_ZERO_WIDTH_SPACE))
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (node),
						0,
						1,
						NULL);
				if (g_str_has_suffix (text_content, UNICODE_ZERO_WIDTH_SPACE))
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (node),
						g_utf8_strlen (text_content, -1) - 1,
						1,
						NULL);
				g_free (text_content);
				text_content = webkit_dom_node_get_text_content (node);
			}
			newline = g_strstr_len (text_content, -1, "\n");

			next_sibling = node;
			while (newline) {
				next_sibling = WEBKIT_DOM_NODE (webkit_dom_text_split_text (
					WEBKIT_DOM_TEXT (next_sibling),
					g_utf8_pointer_to_offset (text_content, newline),
					NULL));

				if (!next_sibling)
					break;

				element = webkit_dom_document_create_element (
					document, "BR", NULL);
				element_add_class (element, "-x-evo-temp-wrap-text-br");

				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (next_sibling),
					WEBKIT_DOM_NODE (element),
					next_sibling,
					NULL);

				g_free (text_content);

				text_content = webkit_dom_node_get_text_content (next_sibling);
				if (g_str_has_prefix (text_content, "\n")) {
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (next_sibling), 0, 1, NULL);
					g_free (text_content);
					text_content =
						webkit_dom_node_get_text_content (next_sibling);
				}
				newline = g_strstr_len (text_content, -1, "\n");
			}
			g_free (text_content);
		} else {
			if (is_selection_position_node (node)) {
				node = webkit_dom_node_get_next_sibling (node);
				continue;
			}

			/* If element is ANCHOR we wrap it separately */
			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
				glong anchor_length;
				WebKitDOMNode *next_sibling;

				text_content = webkit_dom_node_get_text_content (node);
				anchor_length = g_utf8_strlen (text_content, -1);
				g_free (text_content);

				next_sibling = webkit_dom_node_get_next_sibling (node);
				/* If the anchor doesn't fit on the line wrap after it */
				if (anchor_length > word_wrap_length) {
					WebKitDOMNode *inner_node;

					while ((inner_node = webkit_dom_node_get_first_child (node))) {
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (node),
							inner_node,
							next_sibling,
							NULL);
					}
					next_sibling = webkit_dom_node_get_next_sibling (node);

					remove_node (node);
					node = next_sibling;
					continue;
				}

				if (line_length + anchor_length > word_wrap_length) {
					element = webkit_dom_document_create_element (
						document, "BR", NULL);
					element_add_class (element, "-x-evo-wrap-br");
					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						node,
						NULL);
					line_length = anchor_length;
				} else
					line_length += anchor_length;

				node = next_sibling;
				continue;
			}

			if (element_has_class (WEBKIT_DOM_ELEMENT (node), "Apple-tab-span")) {
				WebKitDOMNode *prev_sibling;

				prev_sibling = webkit_dom_node_get_previous_sibling (node);
				if (prev_sibling && WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
				    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "Applet-tab-span"))
					line_length += TAB_LENGTH;
				else
					line_length += TAB_LENGTH - line_length % TAB_LENGTH;
			}
			/* When we are not removing user-entered BR elements (lines wrapped by user),
			 * we need to skip those elements */
			if (!remove_all_br && WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
				if (!element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br")) {
					line_length = 0;
					node = webkit_dom_node_get_next_sibling (node);
					continue;
				}
			}
			goto next_node;
		}

		/* If length of this node + what we already have is still less
		 * then word_wrap_length characters, then just join it and continue to next
		 * node */
		length_left = webkit_dom_character_data_get_length (
			WEBKIT_DOM_CHARACTER_DATA (node));

		if ((length_left + line_length) <= word_wrap_length) {
			line_length += length_left;
			goto next_node;
		}

		/* wrap until we have something */
		while ((length_left + line_length) > word_wrap_length) {
			gint max_length;

			max_length = word_wrap_length - line_length;
			if (max_length < 0)
				max_length = word_wrap_length;
			/* Find where we can line-break the node so that it
			 * effectively fills the rest of current row */
			offset = find_where_to_break_line (
				node, max_length, word_wrap_length);

			element = webkit_dom_document_create_element (document, "BR", NULL);
			element_add_class (element, "-x-evo-wrap-br");

			if (offset > 0 && offset <= word_wrap_length) {
				WebKitDOMNode *nd;

				if (offset != length_left)
					webkit_dom_text_split_text (
						WEBKIT_DOM_TEXT (node), offset, NULL);

				nd = webkit_dom_node_get_next_sibling (node);
				if (nd) {
					gchar *nd_content;

					nd_content = webkit_dom_node_get_text_content (nd);
					if (nd_content && *nd_content) {
						if (g_str_has_prefix (nd_content, " "))
							mark_and_remove_leading_space (document, nd);
						g_free (nd_content);
						nd_content = webkit_dom_node_get_text_content (nd);
						if (g_strcmp0 (nd_content, UNICODE_NBSP) == 0)
							remove_node (nd);
						g_free (nd_content);
					}

					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						nd,
						NULL);

					node = webkit_dom_node_get_next_sibling (
						WEBKIT_DOM_NODE (element));
				} else {
					webkit_dom_node_append_child (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						NULL);
				}
			} else if (offset > word_wrap_length) {
				if (offset != length_left)
					webkit_dom_text_split_text (
						WEBKIT_DOM_TEXT (node), offset + 1, NULL);

				if (webkit_dom_node_get_next_sibling (node)) {
					gchar *nd_content;
					WebKitDOMNode *nd = webkit_dom_node_get_next_sibling (node);

					nd = webkit_dom_node_get_next_sibling (node);
					nd_content = webkit_dom_node_get_text_content (nd);
					if (nd_content && *nd_content) {
						if (g_str_has_prefix (nd_content, " "))
							mark_and_remove_leading_space (document, nd);
						g_free (nd_content);
						nd_content = webkit_dom_node_get_text_content (nd);
						if (g_strcmp0 (nd_content, UNICODE_NBSP) == 0)
							remove_node (nd);
						g_free (nd_content);
					}

					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						nd,
						NULL);

					line_length = 0;
					break;
				} else {
					node = WEBKIT_DOM_NODE (webkit_dom_text_split_text (
						WEBKIT_DOM_TEXT (node), word_wrap_length - line_length, NULL));

					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						node,
						NULL);
				}
			} else {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (node),
					WEBKIT_DOM_NODE (element),
					node,
					NULL);
			}
			if (node)
				length_left = webkit_dom_character_data_get_length (
					WEBKIT_DOM_CHARACTER_DATA (node));

			line_length = 0;
		}
		line_length += length_left - offset;
 next_node:
		if (!node)
			break;

		if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (node))
			line_length = 0;

		/* Move to next node */
		if (webkit_dom_node_has_child_nodes (node)) {
			node = webkit_dom_node_get_first_child (node);
		} else if (webkit_dom_node_get_next_sibling (node)) {
			node = webkit_dom_node_get_next_sibling (node);
		} else {
			if (webkit_dom_node_is_equal_node (node, start_node))
				break;

			node = webkit_dom_node_get_parent_node (node);
			if (node)
				node = webkit_dom_node_get_next_sibling (node);
		}
	}

	if (has_selection) {
		gchar *html;

		/* Create a wrapper DIV and put the processed content into it */
		element = webkit_dom_document_create_element (document, "DIV", NULL);
		element_add_class (element, "-x-evo-paragraph");
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (element),
			WEBKIT_DOM_NODE (start_node),
			NULL);

		webkit_dom_node_normalize (WEBKIT_DOM_NODE (element));
		/* Get HTML code of the processed content */
		html = webkit_dom_element_get_inner_html (element);

		/* Overwrite the current selection by the processed content */
		dom_insert_html (document, extension, html);

		g_free (html);

		return NULL;
	} else {
		webkit_dom_node_normalize (block_clone);

		node = webkit_dom_node_get_parent_node (block);
		if (node) {
			/* Replace block with wrapped one */
			webkit_dom_node_replace_child (
				node, block_clone, block, NULL);
		}

		return WEBKIT_DOM_ELEMENT (block_clone);
	}
}

void
dom_remove_wrapping_from_element (WebKitDOMElement *element)
{
	WebKitDOMNodeList *list;
	gint ii, length;

	list = webkit_dom_element_query_selector_all (
		element, "br.-x-evo-wrap-br", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		WebKitDOMNode *parent;

		parent = get_parent_block_node_from_child (node);
		if (!webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "data-user-wrapped"))
			remove_node (node);
		g_object_unref (node);
	}

	g_object_unref (list);

	list = webkit_dom_element_query_selector_all (
		element, "span[data-hidden-space]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *hidden_space_node;
		WebKitDOMNode *parent;

		hidden_space_node = webkit_dom_node_list_item (list, ii);
		parent = get_parent_block_node_from_child (hidden_space_node);
		if (!webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "data-user-wrapped")) {
			webkit_dom_html_element_set_outer_text (
				WEBKIT_DOM_HTML_ELEMENT (hidden_space_node), " ", NULL);
		}
		g_object_unref (hidden_space_node);
	}
	g_object_unref (list);

	webkit_dom_node_normalize (WEBKIT_DOM_NODE (element));
}

void
dom_remove_quoting_from_element (WebKitDOMElement *element)
{
	gint ii, length;
	WebKitDOMNodeList *list;

	list = webkit_dom_element_query_selector_all (
		element, "span.-x-evo-quoted", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);

	list = webkit_dom_element_query_selector_all (
		element, "span.-x-evo-temp-text-wrapper", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);

		while (webkit_dom_node_get_first_child (node))
			webkit_dom_node_insert_before (
				parent,
				webkit_dom_node_get_first_child (node),
				node,
				NULL);

		remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);

	list = webkit_dom_element_query_selector_all (
		element, "br.-x-evo-temp-br", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);

	webkit_dom_node_normalize (WEBKIT_DOM_NODE (element));
}

WebKitDOMElement *
dom_get_paragraph_element (WebKitDOMDocument *document,
                           EHTMLEditorWebExtension *extension,
                           gint width,
                           gint offset)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "DIV", NULL);
	dom_set_paragraph_style (document, extension, element, width, offset, "");

	return element;
}

WebKitDOMElement *
dom_put_node_into_paragraph (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension,
                             WebKitDOMNode *node,
			     gboolean with_input)
{
	WebKitDOMRange *range;
	WebKitDOMElement *container;

	range = webkit_dom_document_create_range (document);
	container = dom_get_paragraph_element (document, extension, -1, 0);
	webkit_dom_range_select_node (range, node, NULL);
	webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (container), NULL);
	/* We have to move caret position inside this container */
	if (with_input)
		dom_add_selection_markers_into_element_end (document, container, NULL, NULL);

	g_object_unref (range);

	return container;
}

static gint
get_citation_level (WebKitDOMNode *node)
{
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);
	gint level = 0;

	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent) &&
		    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "type"))
			level++;

		parent = webkit_dom_node_get_parent_node (parent);
	}

	return level;
}

WebKitDOMElement *
dom_wrap_paragraph_length (WebKitDOMDocument *document,
                           EHTMLEditorWebExtension *extension,
                           WebKitDOMElement *paragraph,
                           gint length)
{
	g_return_val_if_fail (WEBKIT_DOM_IS_ELEMENT (paragraph), NULL);
	g_return_val_if_fail (length >= MINIMAL_PARAGRAPH_WIDTH, NULL);

	return wrap_lines (document, extension, WEBKIT_DOM_NODE (paragraph), FALSE, length);
}

/**
 * e_html_editor_selection_wrap_lines:
 * @selection: an #EHTMLEditorSelection
 *
 * Wraps all lines in current selection to be 71 characters long.
 */

void
dom_selection_wrap (WebKitDOMDocument *document,
                    EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	gboolean after_selection_end = FALSE, html_mode;
	gint word_wrap_length;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block, *next_block;

	word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);

	dom_selection_save (document);
	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	/* If the selection was not saved, move it into the first child of body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMHTMLElement *body;
		WebKitDOMNode *child;

		body = webkit_dom_document_get_body (document);
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_WRAP;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.style.from = 1;
		ev->data.style.to = 1;
	}

	block = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	html_mode = e_html_editor_web_extension_get_html_mode (extension);

	/* Process all blocks that are in the selection one by one */
	while (block && !after_selection_end) {
		gboolean quoted = FALSE;
		gint citation_level, quote;
		WebKitDOMElement *wrapped_paragraph;

		next_block = webkit_dom_node_get_next_sibling (block);

		/* Don't try to wrap the 'Normal' blocks as they are already wrapped and*/
		/* also skip blocks that we already wrapped with this function. */
		if ((!html_mode && element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-paragraph")) ||
		    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (block), "data-user-wrapped")) {
			block = next_block;
			continue;
		}

		if (webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), "span.-x-evo-quoted", NULL)) {
			quoted = TRUE;
			dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));
		}

		if (!html_mode)
			dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		citation_level = get_citation_level (block);
		quote = citation_level ? citation_level * 2 : 0;

		wrapped_paragraph = dom_wrap_paragraph_length (
			document, extension, WEBKIT_DOM_ELEMENT (block), word_wrap_length - quote);

		webkit_dom_element_set_attribute (
			wrapped_paragraph, "data-user-wrapped", "", NULL);

		if (quoted && !html_mode)
			dom_quote_plain_text_element (document, wrapped_paragraph);

		block = next_block;
	}

	if (ev) {
		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	dom_selection_restore (document);

	dom_force_spell_check_in_viewport (document, extension);
}

void
dom_wrap_paragraphs_in_document (WebKitDOMDocument *document,
                                 EHTMLEditorWebExtension *extension)
{
	WebKitDOMNodeList *list;
	gint ii, length;

	list = webkit_dom_document_query_selector_all (
		document, "div.-x-evo-paragraph:not(#-x-evo-input-start)", NULL);

	length = webkit_dom_node_list_get_length (list);

	for (ii = 0; ii < length; ii++) {
		gint word_wrap_length, quote, citation_level;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		citation_level = get_citation_level (node);
		quote = citation_level ? citation_level * 2 : 0;
		word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);

		if (node_is_list (node)) {
			WebKitDOMNode *item = webkit_dom_node_get_first_child (node);

			while (item && WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
				dom_wrap_paragraph_length (
					document, extension, WEBKIT_DOM_ELEMENT (item), word_wrap_length - quote);
				item = webkit_dom_node_get_next_sibling (item);
			}
		} else {
			dom_wrap_paragraph_length (
				document, extension, WEBKIT_DOM_ELEMENT (node), word_wrap_length - quote);
		}
		g_object_unref (node);
	}
	g_object_unref (list);
}

WebKitDOMElement *
dom_wrap_paragraph (WebKitDOMDocument *document,
                    EHTMLEditorWebExtension *extension,
                    WebKitDOMElement *paragraph)
{
	gint indentation_level, citation_level, quote;
	gint word_wrap_length, final_width, offset = 0;

	g_return_val_if_fail (WEBKIT_DOM_IS_ELEMENT (paragraph), NULL);

	indentation_level = get_indentation_level (paragraph);
	citation_level = get_citation_level (WEBKIT_DOM_NODE (paragraph));

	if (node_is_list_or_item (WEBKIT_DOM_NODE (paragraph))) {
		gint list_level = get_list_level (WEBKIT_DOM_NODE (paragraph));
		indentation_level = 0;

		if (list_level > 0)
			offset = list_level * -SPACES_PER_LIST_LEVEL;
		else
			offset = -SPACES_PER_LIST_LEVEL;
	}

	quote = citation_level ? citation_level * 2 : 0;

	word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);
	final_width = word_wrap_length - quote + offset;
	final_width -= SPACES_PER_INDENTATION * indentation_level;

	return dom_wrap_paragraph_length (
		document, extension, WEBKIT_DOM_ELEMENT (paragraph), final_width);
}

static void
html_editor_selection_modify (WebKitDOMDocument *document,
                              const gchar *alter,
                              gboolean forward,
                              EHTMLEditorSelectionGranularity granularity)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	const gchar *granularity_str = NULL;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	switch (granularity) {
		case E_HTML_EDITOR_SELECTION_GRANULARITY_CHARACTER:
			granularity_str = "character";
			break;
		case E_HTML_EDITOR_SELECTION_GRANULARITY_WORD:
			granularity_str = "word";
			break;
	}

	if (granularity_str) {
		webkit_dom_dom_selection_modify (
			dom_selection, alter,
			forward ? "forward" : "backward",
			granularity_str);
	}

	g_object_unref (dom_selection);
	g_object_unref (dom_window);
}

static gboolean
get_has_style (WebKitDOMDocument *document,
               const gchar *style_tag)
{
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;
	gboolean result;
	gint tag_len;

	range = dom_get_current_range (document);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_start_container (range, NULL);
	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);
	g_object_unref (range);

	tag_len = strlen (style_tag);
	result = FALSE;
	while (!result && element) {
		gchar *element_tag;
		gboolean accept_citation = FALSE;

		element_tag = webkit_dom_element_get_tag_name (element);

		if (g_ascii_strncasecmp (style_tag, "citation", 8) == 0) {
			accept_citation = TRUE;
			result = WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (element);
			if (element_has_class (element, "-x-evo-indented"))
				result = FALSE;
		} else {
			result = ((tag_len == strlen (element_tag)) &&
				(g_ascii_strncasecmp (element_tag, style_tag, tag_len) == 0));
		}

		/* Special case: <blockquote type=cite> marks quotation, while
		 * just <blockquote> is used for indentation. If the <blockquote>
		 * has type=cite, then ignore it unless style_tag is "citation" */
		if (result && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (element)) {
			if (webkit_dom_element_has_attribute (element, "type")) {
				gchar *type;
				type = webkit_dom_element_get_attribute (element, "type");
				if (!accept_citation && (g_ascii_strncasecmp (type, "cite", 4) == 0)) {
					result = FALSE;
				}
				g_free (type);
			} else {
				if (accept_citation)
					result = FALSE;
			}
		}

		g_free (element_tag);

		if (result)
			break;

		element = webkit_dom_node_get_parent_element (
			WEBKIT_DOM_NODE (element));
	}

	return result;
}

typedef gboolean (*IsRightFormatNodeFunc) (WebKitDOMElement *element);

static gboolean
dom_selection_is_font_format (WebKitDOMDocument *document,
			      EHTMLEditorWebExtension *extension,
			      IsRightFormatNodeFunc func,
			      gboolean *previous_value)
{
	gboolean ret_val = FALSE;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMNode *start, *end, *sibling;
	WebKitDOMRange *range = NULL;

	if (!e_html_editor_web_extension_get_html_mode (extension))
		goto out;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection))
		goto out;

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	if (!range)
		goto out;

	if (webkit_dom_range_get_collapsed (range, NULL) && previous_value) {
		WebKitDOMNode *node;
		gchar* text_content;

		node = webkit_dom_range_get_common_ancestor_container (range, NULL);
		/* If we are changing the format of block we have to re-set the
		 * format property, otherwise it will be turned off because of no
		 * text in block. */
		text_content = webkit_dom_node_get_text_content (node);
		if (g_strcmp0 (text_content, "") == 0) {
			g_free (text_content);
			ret_val = *previous_value;
			goto out;
		}
		g_free (text_content);
	}

	/* Range without start or end point is a wrong range. */
	start = webkit_dom_range_get_start_container (range, NULL);
	end = webkit_dom_range_get_end_container (range, NULL);
	if (!start || !end)
		goto out;

	if (WEBKIT_DOM_IS_TEXT (start))
		start = webkit_dom_node_get_parent_node (start);
	while (start && WEBKIT_DOM_IS_ELEMENT (start) && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (start)) {
		/* Find the start point's parent node with given formatting. */
		if (func (WEBKIT_DOM_ELEMENT (start))) {
			ret_val = TRUE;
			break;
		}
		start = webkit_dom_node_get_parent_node (start);
	}

	/* Start point doesn't have the given formatting. */
	if (!ret_val)
		goto out;

	/* If the selection is collapsed, we can return early. */
	if (webkit_dom_range_get_collapsed (range, NULL))
		goto out;

	/* The selection is in the same node and that node is supposed to have
	 * the same formatting (otherwise it is split up with formatting element. */
	if (webkit_dom_node_is_same_node (
		webkit_dom_range_get_start_container (range, NULL),
		webkit_dom_range_get_end_container (range, NULL)))
		goto out;

	ret_val = FALSE;

	if (WEBKIT_DOM_IS_TEXT (end))
		end = webkit_dom_node_get_parent_node (end);
	while (end && WEBKIT_DOM_IS_ELEMENT (end) && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (end)) {
		/* Find the end point's parent node with given formatting. */
		if (func (WEBKIT_DOM_ELEMENT (end))) {
			ret_val = TRUE;
			break;
		}
		end = webkit_dom_node_get_parent_node (end);
	}

	if (!ret_val)
		goto out;

	ret_val = FALSE;

	/* Now go between the end points and check the inner nodes for format validity. */
	sibling = start;
	while ((sibling = webkit_dom_node_get_next_sibling (sibling))) {
		if (webkit_dom_node_is_same_node (sibling, end)) {
			ret_val = TRUE;
			goto out;
		}

		if (WEBKIT_DOM_IS_TEXT (sibling))
			goto out;
		else if (func (WEBKIT_DOM_ELEMENT (sibling)))
			continue;
		else if (webkit_dom_node_get_first_child (sibling)) {
			WebKitDOMNode *first_child;

			first_child = webkit_dom_node_get_first_child (sibling);
			if (!webkit_dom_node_get_next_sibling (first_child))
				if (WEBKIT_DOM_IS_ELEMENT (first_child) && func (WEBKIT_DOM_ELEMENT (first_child)))
					continue;
				else
					goto out;
			else
				goto out;
		} else
			goto out;
	}

	sibling = end;
	while ((sibling = webkit_dom_node_get_previous_sibling (sibling))) {
		if (webkit_dom_node_is_same_node (sibling, start))
			break;

		if (WEBKIT_DOM_IS_TEXT (sibling))
			goto out;
		else if (func (WEBKIT_DOM_ELEMENT (sibling)))
			continue;
		else if (webkit_dom_node_get_first_child (sibling)) {
			WebKitDOMNode *first_child;

			first_child = webkit_dom_node_get_first_child (sibling);
			if (!webkit_dom_node_get_next_sibling (first_child))
				if (WEBKIT_DOM_IS_ELEMENT (first_child) && func (WEBKIT_DOM_ELEMENT (first_child)))
					continue;
				else
					goto out;
			else
				goto out;
		} else
			goto out;
	}

	ret_val = TRUE;
 out:
	g_clear_object (&range);
	g_clear_object (&dom_window);
	g_clear_object (&dom_selection);

	return ret_val;
}

static gboolean
is_underline_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "u");
}

/**
 * e_html_editor_selection_is_underline:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is underlined.
 *
 * Returns @TRUE when selection is underlined, @FALSE otherwise.
 */
gboolean
dom_selection_is_underline (WebKitDOMDocument *document,
                            EHTMLEditorWebExtension *extension)
{
	gboolean is_underline;

	is_underline = e_html_editor_web_extension_get_underline (extension);
	is_underline = dom_selection_is_font_format (
		document, extension, (IsRightFormatNodeFunc) is_underline_element, &is_underline);

	return is_underline;
}

static WebKitDOMElement *
set_font_style (WebKitDOMDocument *document,
                const gchar *element_name,
                gboolean value)
{
	WebKitDOMElement *element;
	WebKitDOMNode *parent;

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-end-marker");
	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
	if (value) {
		WebKitDOMNode *node;
		WebKitDOMElement *el;
		gchar *name;

		el = webkit_dom_document_create_element (document, element_name, NULL);
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (el), UNICODE_ZERO_WIDTH_SPACE, NULL);

		node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (el), node, NULL);
		name = webkit_dom_node_get_local_name (parent);
		if (g_strcmp0 (name, element_name) == 0)
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (el),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
		else
			webkit_dom_node_insert_before (
				parent,
				WEBKIT_DOM_NODE (el),
				WEBKIT_DOM_NODE (element),
				NULL);
		g_free (name);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (el), WEBKIT_DOM_NODE (element), NULL);

		return el;
	} else {
		WebKitDOMNode *node;

		node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));

		/* Turning the formatting in the middle of element. */
		if (webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element))) {
			WebKitDOMNode *clone;
			WebKitDOMNode *sibling;

			clone = webkit_dom_node_clone_node (
				WEBKIT_DOM_NODE (parent), FALSE);

			while ((sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element))))
				webkit_dom_node_insert_before (
					clone,
					sibling,
					webkit_dom_node_get_first_child (clone),
					NULL);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				clone,
				webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent)),
				NULL);
		}

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			WEBKIT_DOM_NODE (element),
			webkit_dom_node_get_next_sibling (parent),
			NULL);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			node,
			webkit_dom_node_get_next_sibling (parent),
			NULL);

		webkit_dom_html_element_insert_adjacent_text (
			WEBKIT_DOM_HTML_ELEMENT (parent),
			"afterend",
			UNICODE_ZERO_WIDTH_SPACE,
			NULL);
	}

	return NULL;
}

static void
selection_set_font_style (WebKitDOMDocument *document,
                          EHTMLEditorWebExtension *extension,
                          EHTMLEditorViewCommand command,
                          gboolean value)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;

	dom_selection_save (document);

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		if (command == E_HTML_EDITOR_VIEW_COMMAND_BOLD)
			ev->type = HISTORY_BOLD;
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_ITALIC)
			ev->type = HISTORY_ITALIC;
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE)
			ev->type = HISTORY_UNDERLINE;
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH)
			ev->type = HISTORY_STRIKETHROUGH;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.style.from = !value;
		ev->data.style.to = value;
	}

	if (dom_selection_is_collapsed (document)) {
		const gchar *element_name = NULL;

		if (command == E_HTML_EDITOR_VIEW_COMMAND_BOLD)
			element_name = "b";
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_ITALIC)
			element_name = "i";
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE)
			element_name = "u";
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH)
			element_name = "strike";

		if (element_name)
			set_font_style (document, element_name, value);
		dom_selection_restore (document);

		goto exit;
	}
	dom_selection_restore (document);

	dom_exec_command (document, extension, command, NULL);
exit:
	if (ev) {
		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	dom_force_spell_check_for_current_paragraph (document, extension);
}

/**
 * e_html_editor_selection_set_underline:
 * @selection: an #EHTMLEditorSelection
 * @underline: @TRUE to enable underline, @FALSE to disable
 *
 * Toggles underline formatting of current selection or letter at current
 * cursor position, depending on whether @underline is @TRUE or @FALSE.
 */
void
dom_selection_set_underline (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension,
                             gboolean underline)
{
	if (dom_selection_is_underline (document, extension) == underline)
		return;

	selection_set_font_style (
		document, extension, E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE, underline);

	set_dbus_property_boolean (extension, "Underline", underline);
}

static gboolean
is_subscript_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "sub");
}

/**
 * e_html_editor_selection_is_subscript:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is in subscript.
 *
 * Returns @TRUE when selection is in subscript, @FALSE otherwise.
 */
gboolean
dom_selection_is_subscript (WebKitDOMDocument *document,
                            EHTMLEditorWebExtension *extension)
{
	return dom_selection_is_font_format (
		document, extension, (IsRightFormatNodeFunc) is_subscript_element, NULL);
}

/**
 * e_html_editor_selection_set_subscript:
 * @selection: an #EHTMLEditorSelection
 * @subscript: @TRUE to enable subscript, @FALSE to disable
 *
 * Toggles subscript of current selection or letter at current cursor position,
 * depending on whether @subscript is @TRUE or @FALSE.
 */
void
dom_selection_set_subscript (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension,
                             gboolean subscript)
{
	if (dom_selection_is_subscript (document, extension) == subscript)
		return;

	dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_SUBSCRIPT, NULL);

	set_dbus_property_boolean (extension, "Subscript", subscript);
}

static gboolean
is_superscript_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "sup");
}

/**
 * e_html_editor_selection_is_superscript:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is in superscript.
 *
 * Returns @TRUE when selection is in superscript, @FALSE otherwise.
 */
gboolean
dom_selection_is_superscript (WebKitDOMDocument *document,
                              EHTMLEditorWebExtension *extension)
{
	return dom_selection_is_font_format (
		document, extension, (IsRightFormatNodeFunc) is_superscript_element, NULL);
}

/**
 * e_html_editor_selection_set_superscript:
 * @selection: an #EHTMLEditorSelection
 * @superscript: @TRUE to enable superscript, @FALSE to disable
 *
 * Toggles superscript of current selection or letter at current cursor position,
 * depending on whether @superscript is @TRUE or @FALSE.
 */
void
dom_selection_set_superscript (WebKitDOMDocument *document,
                               EHTMLEditorWebExtension *extension,
                               gboolean superscript)
{
	if (dom_selection_is_superscript (document, extension) == superscript)
		return;

	dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_SUPERSCRIPT, NULL);

	set_dbus_property_boolean (extension, "Superscript", superscript);
}

static gboolean
is_strikethrough_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "strike");
}

/**
 * e_html_editor_selection_is_strikethrough:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is striked through.
 *
 * Returns @TRUE when selection is striked through, @FALSE otherwise.
 */
gboolean
dom_selection_is_strikethrough (WebKitDOMDocument *document,
                                EHTMLEditorWebExtension *extension)
{
	gboolean is_strikethrough;

	is_strikethrough = e_html_editor_web_extension_get_strikethrough (extension);
	is_strikethrough = dom_selection_is_font_format (
		document, extension, (IsRightFormatNodeFunc) is_strikethrough_element, &is_strikethrough);

	return is_strikethrough;
}

/**
 * e_html_editor_selection_set_strikethrough:
 * @selection: an #EHTMLEditorSelection
 * @strikethrough: @TRUE to enable strikethrough, @FALSE to disable
 *
 * Toggles strike through formatting of current selection or letter at current
 * cursor position, depending on whether @strikethrough is @TRUE or @FALSE.
 */
void
dom_selection_set_strikethrough (WebKitDOMDocument *document,
                                 EHTMLEditorWebExtension *extension,
                                 gboolean strikethrough)
{
	if (dom_selection_is_strikethrough (document, extension) == strikethrough)
		return;

	selection_set_font_style (
		document, extension, E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH, strikethrough);

	set_dbus_property_boolean (extension, "Strikethrough", strikethrough);
}

static gboolean
is_monospaced_element (WebKitDOMElement *element)
{
	gchar *value;
	gboolean ret_val = FALSE;

	if (!element)
		return FALSE;

	if (!WEBKIT_DOM_IS_HTML_FONT_ELEMENT (element))
		return FALSE;

	value = webkit_dom_element_get_attribute (element, "face");

	if (g_strcmp0 (value, "monospace") == 0)
		ret_val = TRUE;

	g_free (value);

	return ret_val;
}

/**
 * e_html_editor_selection_is_monospaced:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is monospaced.
 *
 * Returns @TRUE when selection is monospaced, @FALSE otherwise.
 */
gboolean
dom_selection_is_monospaced (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension)
{
	gboolean is_monospaced;

	is_monospaced = e_html_editor_web_extension_get_monospaced (extension);
	is_monospaced = dom_selection_is_font_format (
		document, extension, (IsRightFormatNodeFunc) is_monospaced_element, &is_monospaced);

	return is_monospaced;
}

static void
monospace_selection (WebKitDOMDocument *document,
                     WebKitDOMElement *monospaced_element)
{
	gboolean selection_end = FALSE;
	gboolean first = TRUE;
	gint length, ii;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *sibling, *node, *monospace, *block;
	WebKitDOMNodeList *list;

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	block = WEBKIT_DOM_NODE (get_parent_block_element (WEBKIT_DOM_NODE (selection_start_marker)));

	monospace = WEBKIT_DOM_NODE (monospaced_element);
	node = WEBKIT_DOM_NODE (selection_start_marker);
	/* Go through first block in selection. */
	while (block && node && !webkit_dom_node_is_same_node (block, node)) {
		if (webkit_dom_node_get_next_sibling (node)) {
			/* Prepare the monospaced element. */
			monospace = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (node),
				first ? monospace : webkit_dom_node_clone_node (monospace, FALSE),
				first ? node : webkit_dom_node_get_next_sibling (node),
				NULL);
		} else
			break;

		/* Move the nodes into monospaced element. */
		while (((sibling = webkit_dom_node_get_next_sibling (monospace)))) {
			webkit_dom_node_append_child (monospace, sibling, NULL);
			if (webkit_dom_node_is_same_node (WEBKIT_DOM_NODE (selection_end_marker), sibling)) {
				selection_end = TRUE;
				break;
			}
		}

		node = webkit_dom_node_get_parent_node (monospace);
		first = FALSE;
	}

	/* Just one block was selected. */
	if (selection_end)
		goto out;

	/* Middle blocks (blocks not containing the end of the selection. */
	block = webkit_dom_node_get_next_sibling (block);
	while (block && !selection_end) {
		WebKitDOMNode *next_block;

		selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		if (selection_end)
			break;

		next_block = webkit_dom_node_get_next_sibling (block);

		monospace = webkit_dom_node_insert_before (
			block,
			webkit_dom_node_clone_node (monospace, FALSE),
			webkit_dom_node_get_first_child (block),
			NULL);

		while (((sibling = webkit_dom_node_get_next_sibling (monospace))))
			webkit_dom_node_append_child (monospace, sibling, NULL);

		block = next_block;
	}

	/* Block containing the end of selection. */
	node = WEBKIT_DOM_NODE (selection_end_marker);
	while (block && node && !webkit_dom_node_is_same_node (block, node)) {
		monospace = webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (node),
			webkit_dom_node_clone_node (monospace, FALSE),
			webkit_dom_node_get_next_sibling (node),
			NULL);

		while (((sibling = webkit_dom_node_get_previous_sibling (monospace)))) {
			webkit_dom_node_insert_before (
				monospace,
				sibling,
				webkit_dom_node_get_first_child (monospace),
				NULL);
		}

		node = webkit_dom_node_get_parent_node (monospace);
	}
 out:
	/* Merge all the monospace elements inside other monospace elements. */
	list = webkit_dom_document_query_selector_all (
		document, "font[face=monospace] > font[face=monospace]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *item;
		WebKitDOMNode *child;

		item = webkit_dom_node_list_item (list, ii);
		while ((child = webkit_dom_node_get_first_child (item))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (item),
				child,
				item,
				NULL);
		}
		remove_node (item);
		g_object_unref (item);
	}
	g_object_unref (list);

	/* Merge all the adjacent monospace elements. */
	list = webkit_dom_document_query_selector_all (
		document, "font[face=monospace] + font[face=monospace]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *item;
		WebKitDOMNode *child;

		item = webkit_dom_node_list_item (list, ii);
		/* The + CSS selector will return some false positives as it doesn't
		 * take text between elements into account so it will return this:
		 * <font face="monospace">xx</font>yy<font face="monospace">zz</font>
		 * as valid, but it isn't so we have to check if previous node
		 * is indeed element or not. */
		if (WEBKIT_DOM_IS_ELEMENT (webkit_dom_node_get_previous_sibling (item))) {
			while ((child = webkit_dom_node_get_first_child (item))) {
				webkit_dom_node_append_child (
					webkit_dom_node_get_previous_sibling (item), child, NULL);
			}
			remove_node (item);
		}
		g_object_unref (item);
	}
	g_object_unref (list);

	dom_selection_restore (document);
}

static void
unmonospace_selection (WebKitDOMDocument *document)
{
	WebKitDOMElement *selection_start_marker;
	WebKitDOMElement *selection_end_marker;
	WebKitDOMElement *selection_start_clone;
	WebKitDOMElement *selection_end_clone;
	WebKitDOMNode *sibling, *node;
	gboolean selection_end = FALSE;
	WebKitDOMNode *block, *clone, *monospace;

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	block = WEBKIT_DOM_NODE (get_parent_block_element (WEBKIT_DOM_NODE (selection_start_marker)));

	node = WEBKIT_DOM_NODE (selection_start_marker);
	monospace = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	while (monospace && !is_monospaced_element (WEBKIT_DOM_ELEMENT (monospace)))
		monospace = webkit_dom_node_get_parent_node (monospace);

	/* No monospaced element was found as a parent of selection start node. */
	if (!monospace)
		goto out;

	/* Make a clone of current monospaced element. */
	clone = webkit_dom_node_clone_node (monospace, TRUE);

	/* First block */
	/* Remove all the nodes that are after the selection start point as they
	 * will be in the cloned node. */
	while (monospace && node && !webkit_dom_node_is_same_node (monospace, node)) {
		WebKitDOMNode *tmp;
		while (((sibling = webkit_dom_node_get_next_sibling (node))))
			remove_node (sibling);

		tmp = webkit_dom_node_get_parent_node (node);
		if (webkit_dom_node_get_next_sibling (node))
			remove_node (node);
		node = tmp;
	}

	selection_start_clone = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (clone), "#-x-evo-selection-start-marker", NULL);
	selection_end_clone = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (clone), "#-x-evo-selection-end-marker", NULL);

	/* No selection start node in the block where it is supposed to be, return. */
	if (!selection_start_clone)
		goto out;

	/* Remove all the nodes until we hit the selection start point as these
	 * nodes will stay monospaced and they are already in original element. */
	node = webkit_dom_node_get_first_child (clone);
	while (node) {
		WebKitDOMNode *next_sibling;

		next_sibling = webkit_dom_node_get_next_sibling (node);
		if (webkit_dom_node_get_first_child (node)) {
			if (webkit_dom_node_contains (node, WEBKIT_DOM_NODE (selection_start_clone))) {
				node = webkit_dom_node_get_first_child (node);
				continue;
			} else
				remove_node (node);
		} else if (webkit_dom_node_is_same_node (node, WEBKIT_DOM_NODE (selection_start_clone)))
			break;
		else
			remove_node (node);

		node = next_sibling;
	}

	/* Insert the clone into the tree. Do it after the previous clean up. If
	 * we would do it the other way the line would contain duplicated text nodes
	 * and the block would be expading and shrinking while we would modify it. */
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (monospace),
		clone,
		webkit_dom_node_get_next_sibling (monospace),
		NULL);

	/* Move selection start point the right place. */
	remove_node (WEBKIT_DOM_NODE (selection_start_marker));
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (clone),
		WEBKIT_DOM_NODE (selection_start_clone),
		clone,
		NULL);

	/* Move all the nodes the are supposed to lose the monospace formatting
	 * out of monospaced element. */
	node = webkit_dom_node_get_first_child (clone);
	while (node) {
		WebKitDOMNode *next_sibling;

		next_sibling = webkit_dom_node_get_next_sibling (node);
		if (webkit_dom_node_get_first_child (node)) {
			if (selection_end_clone &&
			    webkit_dom_node_contains (node, WEBKIT_DOM_NODE (selection_end_clone))) {
				node = webkit_dom_node_get_first_child (node);
				continue;
			} else
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (clone),
					node,
					clone,
					NULL);
		} else if (selection_end_clone &&
			   webkit_dom_node_is_same_node (node, WEBKIT_DOM_NODE (selection_end_clone))) {
			selection_end = TRUE;
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);
			break;
		} else
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);

		node = next_sibling;
	}

	if (!webkit_dom_node_get_first_child (clone))
		remove_node (clone);

	/* Just one block was selected and we hit the selection end point. */
	if (selection_end)
		goto out;

	/* Middle blocks */
	block = webkit_dom_node_get_next_sibling (block);
	while (block && !selection_end) {
		WebKitDOMNode *next_block, *child, *parent;
		WebKitDOMElement *monospaced_element;

		selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		if (selection_end)
			break;

		next_block = webkit_dom_node_get_next_sibling (block);

		/* Find the monospaced element and move all the nodes from it and
		 * finally remove it. */
		monospaced_element = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), "font[face=monospace]", NULL);
		if (!monospaced_element)
			break;

		parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (monospaced_element));
		while  ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (monospaced_element)))) {
			webkit_dom_node_insert_before (
				parent, child, WEBKIT_DOM_NODE (monospaced_element), NULL);
		}

		remove_node (WEBKIT_DOM_NODE (monospaced_element));

		block = next_block;
	}

	/* End block */
	node = WEBKIT_DOM_NODE (selection_end_marker);
	monospace = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_end_marker));
	while (monospace && !is_monospaced_element (WEBKIT_DOM_ELEMENT (monospace)))
		monospace = webkit_dom_node_get_parent_node (monospace);

	/* No monospaced element was found as a parent of selection end node. */
	if (!monospace)
		return;

	clone = WEBKIT_DOM_NODE (monospace);
	node = webkit_dom_node_get_first_child (clone);
	/* Move all the nodes that are supposed to lose the monospaced formatting
	 * out of the monospaced element. */
	while (node) {
		WebKitDOMNode *next_sibling;

		next_sibling = webkit_dom_node_get_next_sibling (node);
		if (webkit_dom_node_get_first_child (node)) {
			if (webkit_dom_node_contains (node, WEBKIT_DOM_NODE (selection_end_marker))) {
				node = webkit_dom_node_get_first_child (node);
				continue;
			} else
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (clone),
					node,
					clone,
					NULL);
		} else if (webkit_dom_node_is_same_node (node, WEBKIT_DOM_NODE (selection_end_marker))) {
			selection_end = TRUE;
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);
			break;
		} else {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);
		}

		node = next_sibling;
	}

	if (!webkit_dom_node_get_first_child (clone))
		remove_node (clone);
 out:
	dom_selection_restore (document);
}

/**
 * e_html_editor_selection_set_monospaced:
 * @selection: an #EHTMLEditorSelection
 * @monospaced: @TRUE to enable monospaced, @FALSE to disable
 *
 * Toggles monospaced formatting of current selection or letter at current cursor
 * position, depending on whether @monospaced is @TRUE or @FALSE.
 */
void
dom_selection_set_monospaced (WebKitDOMDocument *document,
                              EHTMLEditorWebExtension *extension,
                              gboolean monospaced)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	guint font_size = 0;
	WebKitDOMRange *range;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;

	if (e_html_editor_web_extension_get_monospaced (extension) == monospaced)
		return;

	range = dom_get_current_range (document);
	if (!range)
		return;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_MONOSPACE;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.style.from = !monospaced;
		ev->data.style.to = monospaced;
	}

	font_size = e_html_editor_web_extension_get_font_size (extension);
	if (font_size == 0)
		font_size = E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	if (monospaced) {
		WebKitDOMElement *monospace;

		monospace = webkit_dom_document_create_element (
			document, "font", NULL);
		webkit_dom_element_set_attribute (
			monospace, "face", "monospace", NULL);
		if (font_size != 0) {
			gchar *font_size_str;

			font_size_str = g_strdup_printf ("%d", font_size);
			webkit_dom_element_set_attribute (
				monospace, "size", font_size_str, NULL);
			g_free (font_size_str);
		}

		if (!webkit_dom_range_get_collapsed (range, NULL))
			monospace_selection (document, monospace);
		else {
			/* https://bugs.webkit.org/show_bug.cgi?id=15256 */
			webkit_dom_element_set_inner_html (
				monospace,
				UNICODE_ZERO_WIDTH_SPACE,
				NULL);
			webkit_dom_range_insert_node (
				range, WEBKIT_DOM_NODE (monospace), NULL);

			dom_move_caret_into_element (document, monospace, FALSE);
		}
	} else {
		gboolean is_bold = FALSE, is_italic = FALSE;
		gboolean is_underline = FALSE, is_strikethrough = FALSE;
		guint font_size = 0;
		WebKitDOMElement *tt_element;
		WebKitDOMNode *node;

		node = webkit_dom_range_get_end_container (range, NULL);
		if (WEBKIT_DOM_IS_ELEMENT (node) &&
		    is_monospaced_element (WEBKIT_DOM_ELEMENT (node))) {
			tt_element = WEBKIT_DOM_ELEMENT (node);
		} else {
			tt_element = dom_node_find_parent_element (node, "FONT");

			if (!is_monospaced_element (tt_element)) {
				g_object_unref (range);
				g_object_unref (dom_selection);
				g_object_unref (dom_window);
				g_free (ev);
				return;
			}
		}

		/* Save current formatting */
		is_bold = e_html_editor_web_extension_get_bold (extension);
		is_italic = e_html_editor_web_extension_get_italic (extension);
		is_underline = e_html_editor_web_extension_get_underline (extension);
		is_strikethrough = e_html_editor_web_extension_get_strikethrough (extension);

		if (!dom_selection_is_collapsed (document))
			unmonospace_selection (document);
		else {
			dom_selection_save (document);
			set_font_style (document, "", FALSE);
			dom_selection_restore (document);
		}

		/* Re-set formatting */
		if (is_bold)
			dom_selection_set_bold (document, extension, TRUE);
		if (is_italic)
			dom_selection_set_italic (document, extension, TRUE);
		if (is_underline)
			dom_selection_set_underline (document, extension, TRUE);
		if (is_strikethrough)
			dom_selection_set_strikethrough (document, extension, TRUE);

		if (font_size)
			dom_selection_set_font_size (document, extension, font_size);
	}

	if (ev) {
		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	dom_force_spell_check_for_current_paragraph (document, extension);

	g_object_unref (range);
	g_object_unref (dom_selection);
	g_object_unref (dom_window);

	set_dbus_property_boolean (extension, "Monospaced", monospaced);
}

static gboolean
is_bold_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	if (element_has_tag (element, "b"))
		return TRUE;

	/* Headings are bold by default */
	return WEBKIT_DOM_IS_HTML_HEADING_ELEMENT (element);
}

/**
 * e_html_editor_selection_is_bold:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is bold.
 *
 * Returns @TRUE when selection is bold, @FALSE otherwise.
 */
gboolean
dom_selection_is_bold (WebKitDOMDocument *document,
                       EHTMLEditorWebExtension *extension)
{
	gboolean is_bold;

	is_bold = e_html_editor_web_extension_get_bold (extension);

	is_bold = dom_selection_is_font_format (
		document, extension, (IsRightFormatNodeFunc) is_bold_element, &is_bold);

	return is_bold;
}

/**
 * e_html_editor_selection_set_bold:
 * @selection: an #EHTMLEditorSelection
 * @bold: @TRUE to enable bold, @FALSE to disable
 *
 * Toggles bold formatting of current selection or letter at current cursor
 * position, depending on whether @bold is @TRUE or @FALSE.
 */
void
dom_selection_set_bold (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension,
                        gboolean bold)
{
	if (e_html_editor_web_extension_get_bold (extension) == bold)
		return;

	selection_set_font_style (
		document, extension, E_HTML_EDITOR_VIEW_COMMAND_BOLD, bold);

	dom_force_spell_check_for_current_paragraph (document, extension);

	set_dbus_property_boolean (extension, "Bold", bold);
}

static gboolean
is_italic_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "i") || element_has_tag (element, "address");
}

/**
 * e_html_editor_selection_is_italic:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is italic.
 *
 * Returns @TRUE when selection is italic, @FALSE otherwise.
 */
gboolean
dom_selection_is_italic (WebKitDOMDocument *document,
                         EHTMLEditorWebExtension *extension)
{
	gboolean is_italic;

	is_italic = e_html_editor_web_extension_get_italic (extension);
	is_italic = dom_selection_is_font_format (
		document, extension, (IsRightFormatNodeFunc) is_italic_element, &is_italic);

	return is_italic;
}

/**
 * e_html_editor_selection_set_italic:
 * @selection: an #EHTMLEditorSelection
 * @italic: @TRUE to enable italic, @FALSE to disable
 *
 * Toggles italic formatting of current selection or letter at current cursor
 * position, depending on whether @italic is @TRUE or @FALSE.
 */
void
dom_selection_set_italic (WebKitDOMDocument *document,
                          EHTMLEditorWebExtension *extension,
                          gboolean italic)
{
	if (dom_selection_is_italic (document, extension) == italic)
		return;

	selection_set_font_style (
		document, extension, E_HTML_EDITOR_VIEW_COMMAND_ITALIC, italic);

	set_dbus_property_boolean (extension, "Italic", italic);
}

/**
 * e_html_editor_selection_is_indented:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current paragraph is indented. This does not include
 * citations.  To check, whether paragraph is a citation, use
 * e_html_editor_selection_is_citation().
 *
 * Returns: @TRUE when current paragraph is indented, @FALSE otherwise.
 */
gboolean
dom_selection_is_indented (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	range = dom_get_current_range (document);
	if (!range)
		return FALSE;

	if (webkit_dom_range_get_collapsed (range, NULL)) {
		element = get_element_for_inspection (range);
		g_object_unref (range);
		return element_has_class (element, "-x-evo-indented");
	} else {
		WebKitDOMNode *node;
		gboolean ret_val;

		node = webkit_dom_range_get_end_container (range, NULL);
		/* No selection or whole body selected */
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node))
			goto out;

		element = WEBKIT_DOM_ELEMENT (get_parent_indented_block (node));
		ret_val = element_has_class (element, "-x-evo-indented");
		if (!ret_val)
			goto out;

		node = webkit_dom_range_get_start_container (range, NULL);
		/* No selection or whole body selected */
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node))
			goto out;

		element = WEBKIT_DOM_ELEMENT (get_parent_indented_block (node));
		ret_val = element_has_class (element, "-x-evo-indented");

		g_object_unref (range);

		return ret_val;
	}

 out:
	g_object_unref (range);

	return FALSE;
}

/**
 * e_html_editor_selection_is_citation:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current paragraph is a citation.
 *
 * Returns: @TRUE when current paragraph is a citation, @FALSE otherwise.
 */
gboolean
dom_selection_is_citation (WebKitDOMDocument *document)
{
	gboolean ret_val;
	gchar *value, *text_content;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	range = dom_get_current_range (document);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	g_object_unref (range);

	if (WEBKIT_DOM_IS_TEXT (node))
		return get_has_style (document, "citation");

	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return FALSE;
	}
	g_free (text_content);

	value = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "type");

	/* citation == <blockquote type='cite'> */
	if (g_strstr_len (value, -1, "cite"))
		ret_val = TRUE;
	else
		ret_val = get_has_style (document, "citation");

	g_free (value);
	return ret_val;
}

static gchar *
get_font_property (WebKitDOMDocument *document,
                   const gchar *font_property)
{
	WebKitDOMRange *range;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	gchar *value;

	range = dom_get_current_range (document);
	if (!range)
		return NULL;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	g_object_unref (range);
	element = dom_node_find_parent_element (node, "FONT");
	if (!element)
		return NULL;

	g_object_get (G_OBJECT (element), font_property, &value, NULL);

	return value;
}

/**
 * e_editor_Selection_get_font_size:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns point size of current selection or of letter at current cursor position.
 */
guint
dom_selection_get_font_size (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension)
{
	gchar *size;
	guint size_int;

	size = get_font_property (document, "size");
	if (!(size && *size)) {
		g_free (size);
		return E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;
	}

	size_int = atoi (size);
	g_free (size);

	if (size_int == 0)
		return E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;

	return size_int;
}

/**
 * e_html_editor_selection_set_font_size:
 * @selection: an #EHTMLEditorSelection
 * @font_size: point size to apply
 *
 * Sets font size of current selection or of letter at current cursor position
 * to @font_size.
 */
void
dom_selection_set_font_size (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension,
                             guint font_size)
{
	EHTMLEditorUndoRedoManager *manager;
	EHTMLEditorHistoryEvent *ev = NULL;
	gchar *size_str;
	guint current_font_size;

	current_font_size = dom_selection_get_font_size (document, extension);
	if (current_font_size == font_size)
		return;

	dom_selection_save (document);

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_FONT_SIZE;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.style.from = current_font_size;
		ev->data.style.to = font_size;
	}

	size_str = g_strdup_printf ("%d", font_size);

	if (dom_selection_is_collapsed (document)) {
		WebKitDOMElement *font;

		font = set_font_style (document, "font", font_size != 3);
		if (font)
			webkit_dom_element_set_attribute (font, "size", size_str, NULL);
		dom_selection_restore (document);
		goto exit;
	}

	dom_selection_restore (document);

	dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE, size_str);

	/* Text in <font size="3"></font> (size 3 is our default size) is a little
	 * bit smaller than font outsize it. So move it outside of it. */
	if (font_size == E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL) {
		WebKitDOMElement *element;

		element = webkit_dom_document_query_selector (document, "font[size=\"3\"]", NULL);
		if (element) {
			WebKitDOMNode *child;

			while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element))))
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
					child,
					WEBKIT_DOM_NODE (element),
					NULL);

			remove_node (WEBKIT_DOM_NODE (element));
		}
	}

 exit:
	g_free (size_str);

	if (ev) {
		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	set_dbus_property_unsigned (extension, "FontSize", font_size);
}

/**
 * e_html_editor_selection_set_font_name:
 * @selection: an #EHTMLEditorSelection
 * @font_name: a font name to apply
 *
 * Sets font name of current selection or of letter at current cursor position
 * to @font_name.
 */
void
dom_selection_set_font_name (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension,
                             const gchar *font_name)
{
	dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_FONT_NAME, font_name);
/* FIXME WK2
	set_dbus_property_string (extension, "FontName", font_name); */
}

/**
 * e_html_editor_selection_get_font_name:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns name of font used in current selection or at letter at current cursor
 * position.
 *
 * Returns: A string with font name. [transfer-none]
 */
gchar *
dom_selection_get_font_name (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension)
{
	gchar *value;
	WebKitDOMNode *node;
	WebKitDOMRange *range;
	WebKitDOMCSSStyleDeclaration *css;

	range = dom_get_current_range (document);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	g_object_unref (range);

	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (node));
	value = webkit_dom_css_style_declaration_get_property_value (css, "fontFamily");
	g_object_unref (css);

	return value;
}

/**
 * e_html_editor_selection_set_font_color:
 * @selection: an #EHTMLEditorSelection
 * @rgba: a #GdkRGBA
 *
 * Sets font color of current selection or letter at current cursor position to
 * color defined in @rgba.
 */
void
dom_selection_set_font_color (WebKitDOMDocument *document,
                              EHTMLEditorWebExtension *extension,
                              const gchar *color)
{
	EHTMLEditorUndoRedoManager *manager;
	EHTMLEditorHistoryEvent *ev = NULL;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_FONT_COLOR;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.string.from = g_strdup (e_html_editor_web_extension_get_font_color (extension));
		ev->data.string.to = g_strdup (color);
	}

	dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_FORE_COLOR, color);

	if (ev) {
		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	set_dbus_property_string (extension, "FontColor", color);
}

/**
 * e_html_editor_selection_get_font_color:
 * @selection: an #EHTMLEditorSelection
 * @rgba: a #GdkRGBA object to be set to current font color
 *
 * Sets @rgba to contain color of current text selection or letter at current
 * cursor position.
 */
gchar *
dom_selection_get_font_color (WebKitDOMDocument *document,
                              EHTMLEditorWebExtension *extension)
{
	gchar *color;

	color = get_font_property (document, "color");
	if (!(color && *color)) {
		WebKitDOMHTMLElement *body;

		body = webkit_dom_document_get_body (document);
		g_free (color);
		color = webkit_dom_html_body_element_get_text (WEBKIT_DOM_HTML_BODY_ELEMENT (body));
		if (!(color && *color)) {
			g_free (color);
			return g_strdup ("#000000");
		}
	}

	return color;
}

static WebKitDOMNode *
get_block_node (WebKitDOMRange *range)
{
	WebKitDOMNode *node;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	node = get_parent_block_node_from_child (node);

	return node;
}

/**
 * e_html_editor_selection_get_block_format:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns block format of current paragraph.
 *
 * Returns: #EHTMLEditorSelectionBlockFormat
 */
EHTMLEditorSelectionBlockFormat
dom_selection_get_block_format (WebKitDOMDocument *document,
                                EHTMLEditorWebExtension *extension)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range;
	WebKitDOMElement *element;
	EHTMLEditorSelectionBlockFormat result;

	range = dom_get_current_range (document);
	if (!range)
		return E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;

	node = webkit_dom_range_get_start_container (range, NULL);

	if (dom_node_find_parent_element (node, "UL")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;
	} else if ((element = dom_node_find_parent_element (node, "OL")) != NULL) {
		result = dom_get_list_format_from_node (WEBKIT_DOM_NODE (element));
	} else if (dom_node_find_parent_element (node, "PRE")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PRE;
	} else if (dom_node_find_parent_element (node, "ADDRESS")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS;
	} else if (dom_node_find_parent_element (node, "H1")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H1;
	} else if (dom_node_find_parent_element (node, "H2")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H2;
	} else if (dom_node_find_parent_element (node, "H3")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H3;
	} else if (dom_node_find_parent_element (node, "H4")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H4;
	} else if (dom_node_find_parent_element (node, "H5")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H5;
	} else if (dom_node_find_parent_element (node, "H6")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H6;
	} else if ((element = dom_node_find_parent_element (node, "BLOCKQUOTE")) != NULL) {
		if (element_has_class (element, "-x-evo-indented"))
			result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
		else {
			WebKitDOMNode *block = get_block_node (range);

			if (element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-paragraph"))
				result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
			else {
				/* Paragraphs inside quote */
				if ((element = dom_node_find_parent_element (node, "DIV")) != NULL)
					if (element_has_class (element, "-x-evo-paragraph"))
						result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
					else
						result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE;
				else
					result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE;
			}
		}
	} else if (dom_node_find_parent_element (node, "P")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
	} else {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
	}

	g_object_unref (range);

	return result;
}

static gboolean
process_block_to_block (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension,
                        EHTMLEditorSelectionBlockFormat format,
                        const gchar *value,
                        WebKitDOMNode *block,
                        WebKitDOMNode *end_block,
                        WebKitDOMNode *blockquote,
                        gboolean html_mode)
{
	gboolean after_selection_end = FALSE;
	WebKitDOMNode *next_block;

	while (!after_selection_end && block) {
		gboolean quoted = FALSE;
		gboolean empty = FALSE;
		gchar *content;
		WebKitDOMNode *child;
		WebKitDOMElement *element;

		if (dom_node_is_citation_node (block)) {
			gboolean finished;

			next_block = webkit_dom_node_get_next_sibling (block);
			finished = process_block_to_block (
				document,
				extension,
				format,
				value,
				webkit_dom_node_get_first_child (block),
				end_block,
				blockquote,
				html_mode);

			if (finished)
				return TRUE;

			block = next_block;

			continue;
		}

		if (webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), "span.-x-evo-quoted", NULL)) {
			quoted = TRUE;
			dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));
		}

		if (!html_mode)
			dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));

		after_selection_end = webkit_dom_node_is_same_node (block, end_block);

		next_block = webkit_dom_node_get_next_sibling (block);

		if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH ||
		    format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE)
			element = dom_get_paragraph_element (document, extension, -1, 0);
		else
			element = webkit_dom_document_create_element (
				document, value, NULL);

		content = webkit_dom_node_get_text_content (block);

		empty = !*content || (g_strcmp0 (content, UNICODE_ZERO_WIDTH_SPACE) == 0);
		g_free (content);

		while ((child = webkit_dom_node_get_first_child (block))) {
			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (child))
				empty = FALSE;

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element), child, NULL);
		}

		if (empty) {
			WebKitDOMElement *br;

			br = webkit_dom_document_create_element (
				document, "BR", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element), WEBKIT_DOM_NODE (br), NULL);
		}

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			WEBKIT_DOM_NODE (element),
			block,
			NULL);

		remove_node (block);

		if (!next_block && !after_selection_end) {
			gint citation_level;

			citation_level = get_citation_level (WEBKIT_DOM_NODE (element));

			if (citation_level > 0) {
				next_block = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
				next_block = webkit_dom_node_get_next_sibling (next_block);
			}
		}

		block = next_block;

		if (!html_mode &&
		    (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH ||
		     format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE)) {
			gint citation_level;

			if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE)
				citation_level = 1;
			else
				citation_level = get_citation_level (WEBKIT_DOM_NODE (element));

			if (citation_level > 0) {
				gint quote, word_wrap_length;

				word_wrap_length =
					e_html_editor_web_extension_get_word_wrap_length (extension);
				quote = citation_level ? citation_level * 2 : 0;

				element = dom_wrap_paragraph_length (
					document, extension, element, word_wrap_length - quote);

			}
		}

		if (blockquote && format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE) {
			webkit_dom_node_append_child (
				blockquote, WEBKIT_DOM_NODE (element), NULL);
			if (!html_mode)
				dom_quote_plain_text_element_after_wrapping (document, element, 1);
		} else if (!html_mode && quoted)
			dom_quote_plain_text_element (document, element);
	}

	return after_selection_end;
}

static void
format_change_block_to_block (WebKitDOMDocument *document,
                              EHTMLEditorWebExtension *extension,
                              EHTMLEditorSelectionBlockFormat format,
                              const gchar *value)
{
	gboolean html_mode = FALSE;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block, *end_block, *blockquote = NULL;

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	/* If the selection was not saved, move it into the first child of body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMHTMLElement *body;
		WebKitDOMNode *child;

		body = webkit_dom_document_get_body (document);
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	block = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	html_mode = e_html_editor_web_extension_get_html_mode (extension);

	if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE) {
		blockquote = WEBKIT_DOM_NODE (
			webkit_dom_document_create_element (document, "BLOCKQUOTE", NULL));

		webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (blockquote), "type", "cite", NULL);
		if (!html_mode)
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (blockquote), "class", "-x-evo-plaintext-quoted", NULL);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			blockquote,
			block,
			NULL);
	}

	end_block = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_end_marker));

	/* Process all blocks that are in the selection one by one */
	process_block_to_block (
		document, extension, format, value, block, end_block, blockquote, html_mode);
}

static void
format_change_block_to_list (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension,
                             EHTMLEditorSelectionBlockFormat format)
{
	gboolean after_selection_end = FALSE, in_quote = FALSE;
	gboolean html_mode = e_html_editor_web_extension_get_html_mode (extension);
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *item, *list;
	WebKitDOMNode *block, *next_block;

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	/* If the selection was not saved, move it into the first child of body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMHTMLElement *body;
		WebKitDOMNode *child;

		body = webkit_dom_document_get_body (document);
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	block = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	list = create_list_element (document, extension, format, 0, html_mode);

	if (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (block), "span.-x-evo-quoted", NULL)) {
		WebKitDOMElement *element;
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;
		WebKitDOMRange *range;

		in_quote = TRUE;

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		range = webkit_dom_document_create_range (document);

		webkit_dom_range_select_node (range, block, NULL);
		webkit_dom_range_collapse (range, TRUE, NULL);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);

		g_object_unref (range);
		g_object_unref (dom_selection);
		g_object_unref (dom_window);

		dom_exec_command (
			document, extension, E_HTML_EDITOR_VIEW_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, NULL);

		element = webkit_dom_document_query_selector (
			document, "body>br", NULL);

		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			WEBKIT_DOM_NODE (list),
			WEBKIT_DOM_NODE (element),
			NULL);

		block = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
	} else
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			WEBKIT_DOM_NODE (list),
			block,
			NULL);

	/* Process all blocks that are in the selection one by one */
	while (block && !after_selection_end) {
		gboolean empty = FALSE;
		gchar *content;
		WebKitDOMNode *child, *parent;

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		next_block = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (block));

		dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));
		dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));

		item = webkit_dom_document_create_element (document, "LI", NULL);
		content = webkit_dom_node_get_text_content (block);

		empty = !*content || (g_strcmp0 (content, UNICODE_ZERO_WIDTH_SPACE) == 0);
		g_free (content);

		while ((child = webkit_dom_node_get_first_child (block))) {
			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (child))
				empty = FALSE;

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (item), child, NULL);
		}

		/* We have to use again the hidden space to move caret into newly inserted list */
		if (empty) {
			WebKitDOMElement *br;

			br = webkit_dom_document_create_element (
				document, "BR", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (item), WEBKIT_DOM_NODE (br), NULL);
		}

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (list), WEBKIT_DOM_NODE (item), NULL);

		parent = webkit_dom_node_get_parent_node (block);
		remove_node (block);

		if (in_quote) {
			/* Remove all parents if previously removed node was the
			 * only one with text content */
			content = webkit_dom_node_get_text_content (parent);
			while (parent && content && !*content) {
				WebKitDOMNode *tmp = webkit_dom_node_get_parent_node (parent);

				remove_node (parent);
				parent = tmp;

				g_free (content);
				content = webkit_dom_node_get_text_content (parent);
			}
			g_free (content);
		}

		block = next_block;
	}

	merge_lists_if_possible (WEBKIT_DOM_NODE (list));
}

static WebKitDOMNode *
get_list_item_node_from_child (WebKitDOMNode *child)
{
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (child);

	while (parent && !WEBKIT_DOM_IS_HTML_LI_ELEMENT (parent))
		parent = webkit_dom_node_get_parent_node (parent);

	return parent;
}

static WebKitDOMNode *
get_list_node_from_child (WebKitDOMNode *child)
{
	WebKitDOMNode *parent = get_list_item_node_from_child (child);

	return webkit_dom_node_get_parent_node (parent);
}

static void
format_change_list_from_list (WebKitDOMDocument *document,
                              EHTMLEditorWebExtension *extension,
                              EHTMLEditorSelectionBlockFormat to,
                              gboolean html_mode)
{
	gboolean after_selection_end = FALSE;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *new_list;
	WebKitDOMNode *source_list, *source_list_clone, *current_list, *item;

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return;

	new_list = create_list_element (document, extension, to, 0, html_mode);

	/* Copy elements from previous block to list */
	item = get_list_item_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));
	source_list = webkit_dom_node_get_parent_node (item);
	current_list = source_list;
	source_list_clone = webkit_dom_node_clone_node (source_list, FALSE);

	if (element_has_class (WEBKIT_DOM_ELEMENT (source_list), "-x-evo-indented"))
		element_add_class (WEBKIT_DOM_ELEMENT (new_list), "-x-evo-indented");

	while (item) {
		WebKitDOMNode *next_item = webkit_dom_node_get_next_sibling (item);

		if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
			webkit_dom_node_append_child (
				after_selection_end ?
					source_list_clone : WEBKIT_DOM_NODE (new_list),
				WEBKIT_DOM_NODE (item),
				NULL);
		}

		if (webkit_dom_node_contains (item, WEBKIT_DOM_NODE (selection_end_marker))) {
			g_object_unref (source_list_clone);
			source_list_clone = webkit_dom_node_clone_node (current_list, FALSE);
			after_selection_end = TRUE;
		}

		if (!next_item) {
			if (after_selection_end)
				break;
			current_list = webkit_dom_node_get_next_sibling (current_list);
			next_item = webkit_dom_node_get_first_child (current_list);
		}
		item = next_item;
	}

	if (webkit_dom_node_has_child_nodes (source_list_clone))
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (source_list),
			WEBKIT_DOM_NODE (source_list_clone),
			webkit_dom_node_get_next_sibling (source_list), NULL);
	if (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (new_list)))
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (source_list),
			WEBKIT_DOM_NODE (new_list),
			webkit_dom_node_get_next_sibling (source_list), NULL);
	if (!webkit_dom_node_has_child_nodes (source_list))
		remove_node (source_list);
}

static void
format_change_list_to_list (WebKitDOMDocument *document,
                            EHTMLEditorWebExtension *extension,
                            EHTMLEditorSelectionBlockFormat format,
                            gboolean html_mode)
{
	EHTMLEditorSelectionBlockFormat prev = 0, next = 0;
	gboolean done = FALSE, indented = FALSE;
	gboolean selection_starts_in_first_child, selection_ends_in_last_child;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *prev_list, *current_list, *next_list;

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	current_list = get_list_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	prev_list = get_list_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	next_list = get_list_node_from_child (
		WEBKIT_DOM_NODE (selection_end_marker));

	selection_starts_in_first_child =
		webkit_dom_node_contains (
			webkit_dom_node_get_first_child (current_list),
			WEBKIT_DOM_NODE (selection_start_marker));

	selection_ends_in_last_child =
		webkit_dom_node_contains (
			webkit_dom_node_get_last_child (current_list),
			WEBKIT_DOM_NODE (selection_end_marker));

	indented = element_has_class (WEBKIT_DOM_ELEMENT (current_list), "-x-evo-indented");

	if (!prev_list || !next_list || indented) {
		format_change_list_from_list (document, extension, format, html_mode);
		return;
	}

	if (webkit_dom_node_is_same_node (prev_list, next_list)) {
		prev_list = webkit_dom_node_get_previous_sibling (
			webkit_dom_node_get_parent_node (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_start_marker))));
		next_list = webkit_dom_node_get_next_sibling (
			webkit_dom_node_get_parent_node (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_end_marker))));
		if (!prev_list || !next_list) {
			format_change_list_from_list (document, extension, format, html_mode);
			return;
		}
	}

	prev = dom_get_list_format_from_node (prev_list);
	next = dom_get_list_format_from_node (next_list);

	if (format == prev && format != -1 && prev != -1) {
		if (selection_starts_in_first_child && selection_ends_in_last_child) {
			done = TRUE;
			merge_list_into_list (current_list, prev_list, FALSE);
		}
	}

	if (format == next && format != -1 && next != -1) {
		if (selection_starts_in_first_child && selection_ends_in_last_child) {
			done = TRUE;
			merge_list_into_list (next_list, prev_list, FALSE);
		}
	}

	if (done)
		return;

	format_change_list_from_list (document, extension, format, html_mode);
}

static void
format_change_list_to_block (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension,
                             EHTMLEditorSelectionBlockFormat format,
                             const gchar *value)
{
	gboolean after_end = FALSE;
	WebKitDOMElement *selection_start, *element, *selection_end;
	WebKitDOMNode *source_list, *next_item, *item, *source_list_clone;

	selection_start = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	item = get_list_item_node_from_child (
		WEBKIT_DOM_NODE (selection_start));
	source_list = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (item));
	source_list_clone = webkit_dom_node_clone_node (source_list, FALSE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (source_list),
		WEBKIT_DOM_NODE (source_list_clone),
		webkit_dom_node_get_next_sibling (source_list),
		NULL);

	next_item = item;

	/* Process all nodes that are in selection one by one */
	while (next_item) {
		WebKitDOMNode *tmp;

		tmp = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (next_item));

		if (!after_end) {
			WebKitDOMNode *node;

			if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH)
				element = dom_get_paragraph_element (document, extension, -1, 0);
			else
				element = webkit_dom_document_create_element (
					document, value, NULL);

			after_end = webkit_dom_node_contains (next_item, WEBKIT_DOM_NODE (selection_end));

			while ((node = webkit_dom_node_get_first_child (next_item)))
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (element), node, NULL);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (source_list),
				WEBKIT_DOM_NODE (element),
				source_list_clone,
				NULL);

			remove_node (next_item);

			next_item = tmp;
		} else {
			webkit_dom_node_append_child (
				source_list_clone, next_item, NULL);

			next_item = tmp;
		}
	}

	remove_node_if_empty (source_list_clone);
	remove_node_if_empty (source_list);
}

/**
 * e_html_editor_selection_set_block_format:
 * @selection: an #EHTMLEditorSelection
 * @format: an #EHTMLEditorSelectionBlockFormat value
 *
 * Changes block format of current paragraph to @format.
 */
void
dom_selection_set_block_format (WebKitDOMDocument *document,
                                EHTMLEditorWebExtension *extension,
                                EHTMLEditorSelectionBlockFormat format)
{
	EHTMLEditorSelectionBlockFormat current_format;
	EHTMLEditorUndoRedoManager *manager;
	EHTMLEditorHistoryEvent *ev = NULL;
	const gchar *value;
	gboolean from_list = FALSE, to_list = FALSE, html_mode = FALSE;
	WebKitDOMRange *range;

	current_format = dom_selection_get_block_format (document, extension);
	if (current_format == format) {
		return;
	}

	switch (format) {
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE:
			value = "BLOCKQUOTE";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H1:
			value = "H1";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H2:
			value = "H2";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H3:
			value = "H3";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H4:
			value = "H4";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H5:
			value = "H5";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H6:
			value = "H6";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH:
			value = "P";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PRE:
			value = "PRE";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS:
			value = "ADDRESS";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST:
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA:
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN:
			to_list = TRUE;
			value = NULL;
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST:
			to_list = TRUE;
			value = NULL;
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_NONE:
		default:
			value = NULL;
			break;
	}

	/* H1 - H6 have bold font by default */
	if (format >= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H1 &&
	    format <= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H6)
		set_dbus_property_boolean (extension, "Bold", TRUE);

	html_mode = e_html_editor_web_extension_get_html_mode (extension);

	from_list =
		current_format >= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;

	range = dom_get_current_range (document);
	if (!range)
		return;

	dom_selection_save (document);

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		if (format != E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE)
			ev->type = HISTORY_BLOCK_FORMAT;
		else
			ev->type = HISTORY_BLOCKQUOTE;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		if (format != E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE) {
			ev->data.style.from = current_format;
			ev->data.style.to = format;
		} else {
			WebKitDOMDocumentFragment *fragment;
			WebKitDOMElement *selection_start_marker, *selection_end_marker;
			WebKitDOMNode *block, *end_block;

			selection_start_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			selection_end_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");
			block = get_parent_block_node_from_child (
				WEBKIT_DOM_NODE (selection_start_marker));
			end_block = get_parent_block_node_from_child (
				WEBKIT_DOM_NODE (selection_end_marker));
			if (webkit_dom_range_get_collapsed (range, NULL) ||
			    webkit_dom_node_is_same_node (block, end_block)) {
				fragment = webkit_dom_document_create_document_fragment (document);

				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (fragment),
					webkit_dom_node_clone_node (block, TRUE),
					NULL);
			} else {
				fragment = webkit_dom_range_clone_contents (range, NULL);
				webkit_dom_node_replace_child (
					WEBKIT_DOM_NODE (fragment),
					webkit_dom_node_clone_node (block, TRUE),
					webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
					NULL);

				webkit_dom_node_replace_child (
					WEBKIT_DOM_NODE (fragment),
					webkit_dom_node_clone_node (end_block, TRUE),
					webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (fragment)),
					NULL);
			}
			ev->data.fragment = fragment;
		}
	 }

	g_object_unref (range);

	if (from_list && to_list)
		format_change_list_to_list (document, extension, format, html_mode);

	if (!from_list && !to_list)
		format_change_block_to_block (document, extension, format, value);

	if (from_list && !to_list) {
		format_change_list_to_block (document, extension, format, value);

		if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE) {
			dom_selection_restore (document);
			format_change_block_to_block (document, extension, format, value);
		}
	}

	if (!from_list && to_list)
		format_change_block_to_list (document, extension, format);

	dom_selection_restore (document);

	dom_force_spell_check_for_current_paragraph (document, extension);

	/* When changing the format we need to re-set the alignment */
	dom_selection_set_alignment (
		document, extension, e_html_editor_web_extension_get_alignment (extension));

	e_html_editor_web_extension_set_content_changed (extension);

	if (ev) {
		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	set_dbus_property_unsigned (extension, "BlockFormat", format);
}

/**
 * e_html_editor_selection_get_background_color:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns background color of currently selected text or letter at current
 * cursor position.
 *
 * Returns: A string with code of current background color.
 */
gchar *
dom_selection_get_background_color (WebKitDOMDocument *document,
                                    EHTMLEditorWebExtension *extension)
{
	gchar *value;
	WebKitDOMNode *ancestor;
	WebKitDOMRange *range;
	WebKitDOMCSSStyleDeclaration *css;

	range = dom_get_current_range (document);
	ancestor = webkit_dom_range_get_common_ancestor_container (range, NULL);
	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (ancestor));
/* FIXME WK2
	g_free (selection->priv->background_color);
	selection->priv->background_color =
		webkit_dom_css_style_declaration_get_property_value (
			css, "background-color");*/

	value = webkit_dom_css_style_declaration_get_property_value (css, "background-color");

	g_object_unref (css);
	g_object_unref (range);

	return value;
}

/**
 * e_html_editor_selection_set_background_color:
 * @selection: an #EHTMLEditorSelection
 * @color: code of new background color to set
 *
 * Changes background color of current selection or letter at current cursor
 * position to @color.
 */
void
dom_selection_set_background_color (WebKitDOMDocument *document,
                                    EHTMLEditorWebExtension *extension,
                                    const gchar *color)
{
	dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_BACKGROUND_COLOR, color);
/* FIXME WK2
	set_dbus_property_string (extension, "BackgroundColor", color); */
}

/**
 * e_html_editor_selection_get_alignment:
 * @selection: #an EHTMLEditorSelection
 *
 * Returns alignment of current paragraph
 *
 * Returns: #EHTMLEditorSelectionAlignment
 */
EHTMLEditorSelectionAlignment
dom_selection_get_alignment (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension)
{
	EHTMLEditorSelectionAlignment alignment;
	gchar *value;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	range = dom_get_current_range (document);
	if (!range)
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;

	node = webkit_dom_range_get_start_container (range, NULL);
	g_object_unref (range);
	if (!node)
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	dom_window = webkit_dom_document_get_default_view (document);
	style = webkit_dom_dom_window_get_computed_style (dom_window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-align");

	if (!value || !*value ||
	    (g_ascii_strncasecmp (value, "left", 4) == 0)) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	} else if (g_ascii_strncasecmp (value, "center", 6) == 0) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER;
	} else if (g_ascii_strncasecmp (value, "right", 5) == 0) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT;
	} else {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	}

	g_object_unref (style);
	g_object_unref (dom_window);
	g_free (value);

	return alignment;
}

static void
set_block_alignment (WebKitDOMElement *element,
                     const gchar *class)
{
	WebKitDOMElement *parent;

	element_remove_class (element, "-x-evo-align-center");
	element_remove_class (element, "-x-evo-align-right");
	element_add_class (element, class);
	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (element));
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		element_remove_class (parent, "-x-evo-align-center");
		element_remove_class (parent, "-x-evo-align-right");
		parent = webkit_dom_node_get_parent_element (
			WEBKIT_DOM_NODE (parent));
	}
}

/**
 * e_html_editor_selection_set_alignment:
 * @selection: an #EHTMLEditorSelection
 * @alignment: an #EHTMLEditorSelectionAlignment value to apply
 *
 * Sets alignment of current paragraph to give @alignment.
 */
void
dom_selection_set_alignment (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension,
                             EHTMLEditorSelectionAlignment alignment)
{
	EHTMLEditorSelectionAlignment current_alignment;
	EHTMLEditorUndoRedoManager *manager;
	EHTMLEditorHistoryEvent *ev = NULL;
	gboolean after_selection_end = FALSE;
	const gchar *class = "", *list_class = "";
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;

	current_alignment = e_html_editor_web_extension_get_alignment (extension);

	if (current_alignment == alignment)
		return;

	switch (alignment) {
		case E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER:
			class = "-x-evo-align-center";
			list_class = "-x-evo-list-item-align-center";
			break;

		case E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT:
			break;

		case E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT:
			class = "-x-evo-align-right";
			list_class = "-x-evo-list-item-align-right";
			break;
	}

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker)
		return;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_ALIGNMENT;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
		ev->data.style.from = current_alignment;
		ev->data.style.to = alignment;
	 }

	block = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	while (block && !after_selection_end) {
		WebKitDOMNode *next_block;

		next_block = webkit_dom_node_get_next_sibling (block);

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		if (node_is_list (block)) {
			WebKitDOMNode *item = webkit_dom_node_get_first_child (block);

			while (item && WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
				element_remove_class (
					WEBKIT_DOM_ELEMENT (item),
					"-x-evo-list-item-align-center");
				element_remove_class (
					WEBKIT_DOM_ELEMENT (item),
					"-x-evo-list-item-align-right");

				element_add_class (WEBKIT_DOM_ELEMENT (item), list_class);
				after_selection_end = webkit_dom_node_contains (
					item, WEBKIT_DOM_NODE (selection_end_marker));
				if (after_selection_end)
					break;
				item = webkit_dom_node_get_next_sibling (item);
			}
		} else {
			if (element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-indented")) {
				gint ii, length;
				WebKitDOMNodeList *list;

				list = webkit_dom_element_query_selector_all (
					WEBKIT_DOM_ELEMENT (block),
					".-x-evo-indented > *:not(.-x-evo-indented):not(li)",
					NULL);
				length = webkit_dom_node_list_get_length (list);

				for (ii = 0; ii < length; ii++) {
					WebKitDOMNode *item = webkit_dom_node_list_item (list, ii);

					set_block_alignment (WEBKIT_DOM_ELEMENT (item), class);

					after_selection_end = webkit_dom_node_contains (
						item, WEBKIT_DOM_NODE (selection_end_marker));
					g_object_unref (item);
					if (after_selection_end)
						break;
				}

				g_object_unref (list);
			} else {
				set_block_alignment (WEBKIT_DOM_ELEMENT (block), class);
			}
		}

		block = next_block;
	}

	if (ev) {
		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	dom_selection_restore (document);

	dom_force_spell_check_for_current_paragraph (document, extension);

	set_dbus_property_unsigned (extension, "Alignment", alignment);
}

/**
 * e_html_editor_selection_replace:
 * @selection: an #EHTMLEditorSelection
 * @replacement: a string to replace current selection with
 *
 * Replaces currently selected text with @replacement.
 */
void
dom_selection_replace (WebKitDOMDocument *document,
                       EHTMLEditorWebExtension *extension,
                       const gchar *replacement)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;
		WebKitDOMRange *range;

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_REPLACE;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);

		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		ev->data.string.from = webkit_dom_range_get_text (range);
		ev->data.string.to = g_strdup (replacement);

		g_object_unref (range);
		g_object_unref (dom_selection);
		g_object_unref (dom_window);
	}

	dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, replacement);

	if (ev) {
		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	dom_force_spell_check_for_current_paragraph (document, extension);

	e_html_editor_web_extension_set_content_changed (extension);
}

/**
 * e_html_editor_selection_replace_caret_word:
 * @selection: an #EHTMLEditorSelection
 * @replacement: a string to replace current caret word with
 *
 * Replaces current word under cursor with @replacement.
 */
void
dom_replace_caret_word (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension,
                        const gchar *replacement)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	e_html_editor_web_extension_set_content_changed (extension);
	range = dom_get_current_range (document);
	webkit_dom_range_expand (range, "word", NULL);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_object_unref (range);
	g_object_unref (dom_selection);
	g_object_unref (dom_window);

	dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML, replacement);
	dom_force_spell_check_for_current_paragraph (document, extension);
}

/**
 * e_html_editor_selection_get_caret_word:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns word under cursor.
 *
 * Returns: A newly allocated string with current caret word or @NULL when there
 * is no text under cursor or when selection is active. [transfer-full].
 */
gchar *
dom_get_caret_word (WebKitDOMDocument *document)
{
	gchar *word;
	WebKitDOMRange *range;

	range = dom_get_current_range (document);

	/* Don't operate on the visible selection */
	range = webkit_dom_range_clone_range (range, NULL);
	webkit_dom_range_expand (range, "word", NULL);
	word = webkit_dom_range_to_string (range, NULL);

	g_object_unref (range);

	return word;
}

/**
 * e_html_editor_selection_has_text:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection contains any text.
 *
 * Returns: @TRUE when current selection contains text, @FALSE otherwise.
 */
gboolean
dom_selection_has_text (WebKitDOMDocument *document)
{
	WebKitDOMRange *range;
	WebKitDOMNode *node;

	range = dom_get_current_range (document);

	node = webkit_dom_range_get_start_container (range, NULL);
	if (WEBKIT_DOM_IS_TEXT (node)) {
		g_object_unref (range);
		return TRUE;
	}

	node = webkit_dom_range_get_end_container (range, NULL);
	if (WEBKIT_DOM_IS_TEXT (node)) {
		g_object_unref (range);
		return TRUE;
	}

	node = WEBKIT_DOM_NODE (webkit_dom_range_clone_contents (range, NULL));
	while (node) {
		if (WEBKIT_DOM_IS_TEXT (node)) {
			g_object_unref (range);
			return TRUE;
		}

		if (webkit_dom_node_has_child_nodes (node)) {
			node = webkit_dom_node_get_first_child (node);
		} else if (webkit_dom_node_get_next_sibling (node)) {
			node = webkit_dom_node_get_next_sibling (node);
		} else {
			node = webkit_dom_node_get_parent_node (node);
			if (node) {
				node = webkit_dom_node_get_next_sibling (node);
			}
		}
	}

	if (node)
		g_object_unref (node);

	g_object_unref (range);

	return FALSE;
}

/**
 * e_html_editor_selection_get_list_alignment_from_node:
 * @node: #an WebKitDOMNode
 *
 * Returns alignment of given list.
 *
 * Returns: #EHTMLEditorSelectionAlignment
 */
EHTMLEditorSelectionAlignment
dom_get_list_alignment_from_node (WebKitDOMNode *node)
{
	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-list-item-align-left"))
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-list-item-align-center"))
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER;
	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-list-item-align-right"))
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT;

	return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
}

WebKitDOMElement *
dom_prepare_paragraph (WebKitDOMDocument *document,
                       EHTMLEditorWebExtension *extension,
                       gboolean with_selection)
{
	WebKitDOMElement *element, *paragraph;

	paragraph = dom_get_paragraph_element (document, extension, -1, 0);

	if (with_selection)
		dom_add_selection_markers_into_element_start (
			document, paragraph, NULL, NULL);

	element = webkit_dom_document_create_element (document, "BR", NULL);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (paragraph), WEBKIT_DOM_NODE (element), NULL);

	return paragraph;
}

void
dom_selection_set_on_point (WebKitDOMDocument *document,
                            guint x,
                            guint y)
{
	WebKitDOMRange *range;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	range = webkit_dom_document_caret_range_from_point (document, x, y);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	g_object_unref (range);
	g_object_unref (dom_selection);
	g_object_unref (dom_window);
}

void
dom_selection_get_coordinates (WebKitDOMDocument *document,
                               guint *start_x,
                               guint *start_y,
                               guint *end_x,
                               guint *end_y)
{
	gboolean created_selection_markers = FALSE;
	guint local_x = 0, local_y = 0;
	WebKitDOMElement *element, *parent;

	g_return_if_fail (start_x != NULL);
	g_return_if_fail (start_y != NULL);
	g_return_if_fail (end_x != NULL);
	g_return_if_fail (end_y != NULL);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (!element) {
		created_selection_markers = TRUE;
		dom_selection_save (document);
		element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (!element)
			return;
	}

	parent = element;
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		local_x += (guint) webkit_dom_element_get_offset_left (parent);
		local_y += (guint) webkit_dom_element_get_offset_top (parent);
		parent = webkit_dom_element_get_offset_parent (parent);
	}

	if (start_x)
		*start_x = local_x;
	if (start_y)
		*start_y = local_y;

	if (dom_selection_is_collapsed (document)) {
		*end_x = local_x;
		*end_y = local_y;

		if (created_selection_markers)
			dom_selection_restore (document);

		return;
	}

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	local_x = 0;
	local_y = 0;

	parent = element;
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		local_x += (guint) webkit_dom_element_get_offset_left (parent);
		local_y += (guint) webkit_dom_element_get_offset_top (parent);
		parent = webkit_dom_element_get_offset_parent (parent);
	}

	if (end_x)
		*end_x = local_x;
	if (end_y)
		*end_y = local_y;

	if (created_selection_markers)
		dom_selection_restore (document);
}
