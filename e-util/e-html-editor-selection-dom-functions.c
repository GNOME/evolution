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

#include "e-util-enums.h"
#include "e-dom-utils.h"
#include "e-html-editor-view-dom-functions.h"

#include <string.h>
#include <stdlib.h>

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDocumentFragmentUnstable.h>
#include <webkitdom/WebKitDOMRangeUnstable.h>
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

#define UNICODE_ZERO_WIDTH_SPACE "\xe2\x80\x8b"
#define UNICODE_NBSP "\xc2\xa0"

#define SPACES_PER_INDENTATION 4
#define SPACES_PER_LIST_LEVEL 8
#define MINIMAL_PARAGRAPH_WIDTH 5

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

/**
 * e_html_editor_selection_clear_caret_position_marker:
 * @selection: an #EHTMLEditorSelection
 *
 * Removes previously set caret position marker from composer.
 */
void
dom_clear_caret_position_marker (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-caret-position");

	if (element)
		remove_node (WEBKIT_DOM_NODE (element));
}

WebKitDOMNode *
dom_create_caret_position_node (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;

	element	= webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_id (element, "-x-evo-caret-position");
	webkit_dom_element_set_attribute (
		element, "style", "color: red", NULL);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element), "*", NULL);

	return WEBKIT_DOM_NODE (element);
}

static WebKitDOMRange *
dom_get_current_range (WebKitDOMDocument *document)
{
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection_dom;
	WebKitDOMRange *range = NULL;

	window = webkit_dom_document_get_default_view (document);
	if (!window)
		return NULL;

	selection_dom = webkit_dom_dom_window_get_selection (window);
	if (!WEBKIT_DOM_IS_DOM_SELECTION (selection_dom))
		return NULL;

	if (webkit_dom_dom_selection_get_range_count (selection_dom) < 1)
		return NULL;

	range = webkit_dom_dom_selection_get_range_at (selection_dom, 0, NULL);

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

	return webkit_dom_range_get_text (range);
}

/**
 * e_html_editor_selection_save_caret_position:
 * @selection: an #EHTMLEditorSelection
 *
 * Saves current caret position in composer.
 *
 * Returns: #WebKitDOMElement that was created on caret position
 */
WebKitDOMElement *
dom_save_caret_position (WebKitDOMDocument *document)
{
	WebKitDOMNode *split_node;
	WebKitDOMNode *start_offset_node;
	WebKitDOMNode *caret_node;
	WebKitDOMRange *range;
	gulong start_offset;

	dom_clear_caret_position_marker (document);

	range = dom_get_current_range (document);
	if (!range)
		return NULL;

	start_offset = webkit_dom_range_get_start_offset (range, NULL);
	start_offset_node = webkit_dom_range_get_end_container (range, NULL);

	caret_node = dom_create_caret_position_node (document);

	if (WEBKIT_DOM_IS_TEXT (start_offset_node) && start_offset != 0) {
		WebKitDOMText *split_text;

		split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (start_offset_node),
				start_offset, NULL);
		split_node = WEBKIT_DOM_NODE (split_text);
	} else {
		split_node = start_offset_node;
	}

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (start_offset_node),
		caret_node,
		split_node,
		NULL);

	return WEBKIT_DOM_ELEMENT (caret_node);
}

static void
fix_quoting_nodes_after_caret_restoration (WebKitDOMDOMSelection *window_selection,
                                           WebKitDOMNode *prev_sibling,
                                           WebKitDOMNode *next_sibling)
{
	WebKitDOMNode *tmp_node;

	if (!element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-temp-text-wrapper"))
		return;

	webkit_dom_dom_selection_modify (
		window_selection, "move", "forward", "character");
	tmp_node = webkit_dom_node_get_next_sibling (
		webkit_dom_node_get_first_child (prev_sibling));

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (prev_sibling),
		tmp_node,
		next_sibling,
		NULL);

	tmp_node = webkit_dom_node_get_first_child (prev_sibling);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (prev_sibling),
		tmp_node,
		webkit_dom_node_get_previous_sibling (next_sibling),
		NULL);

	remove_node (prev_sibling);

	webkit_dom_dom_selection_modify (
		window_selection, "move", "backward", "character");
}

static void
dom_move_caret_into_element (WebKitDOMDocument *document,
                             WebKitDOMElement *element)
{
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *window_selection;
	WebKitDOMRange *new_range;

	if (!element)
		return;

	window = webkit_dom_document_get_default_view (document);
	window_selection = webkit_dom_dom_window_get_selection (window);
	new_range = webkit_dom_document_create_range (document);

	webkit_dom_range_select_node_contents (
		new_range, WEBKIT_DOM_NODE (element), NULL);
	webkit_dom_range_collapse (new_range, FALSE, NULL);
	webkit_dom_dom_selection_remove_all_ranges (window_selection);
	webkit_dom_dom_selection_add_range (window_selection, new_range);
}

/**
 * e_html_editor_selection_restore_caret_position:
 * @selection: an #EHTMLEditorSelection
 *
 * Restores previously saved caret position in composer.
 */
void
dom_restore_caret_position (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;
	gboolean fix_after_quoting;
	gboolean swap_direction = FALSE;
/* FIXME WK2
	e_html_editor_selection_block_selection_changed (selection);
*/
	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-caret-position");
	fix_after_quoting = element_has_class (element, "-x-evo-caret-quoting");

	if (element) {
		WebKitDOMDOMWindow *window;
		WebKitDOMNode *parent_node;
		WebKitDOMDOMSelection *window_selection;
		WebKitDOMNode *prev_sibling;
		WebKitDOMNode *next_sibling;

		if (!webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element)))
			swap_direction = TRUE;

		window = webkit_dom_document_get_default_view (document);
		window_selection = webkit_dom_dom_window_get_selection (window);
		parent_node = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
		/* If parent is BODY element, we try to restore the position on the
		 * element that is next to us */
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent_node)) {
			/* Look if we have DIV on right */
			next_sibling = webkit_dom_node_get_next_sibling (
				WEBKIT_DOM_NODE (element));
			if (!WEBKIT_DOM_IS_ELEMENT (next_sibling)) {
				dom_clear_caret_position_marker (document);
				/* FIXME WK2
					e_html_editor_selection_unblock_selection_changed (selection);
				*/
				return;
			}

			if (element_has_class (WEBKIT_DOM_ELEMENT (next_sibling), "-x-evo-paragraph")) {
				remove_node (WEBKIT_DOM_NODE (element));

				dom_move_caret_into_element (
					document, WEBKIT_DOM_ELEMENT (next_sibling));

				goto out;
			}
		}

		dom_move_caret_into_element (document, element);

		if (fix_after_quoting) {
			prev_sibling = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (element));
			next_sibling = webkit_dom_node_get_next_sibling (
				WEBKIT_DOM_NODE (element));
			if (!next_sibling)
				fix_after_quoting = FALSE;
		}

		remove_node (WEBKIT_DOM_NODE (element));

		if (fix_after_quoting)
			fix_quoting_nodes_after_caret_restoration (
				window_selection, prev_sibling, next_sibling);
 out:
		/* FIXME If caret position is restored and afterwards the
		 * position is saved it is not on the place where it supposed
		 * to be (it is in the beginning of parent's element. It can
		 * be avoided by moving with the caret. */
		if (swap_direction) {
			webkit_dom_dom_selection_modify (
				window_selection, "move", "forward", "character");
			webkit_dom_dom_selection_modify (
				window_selection, "move", "backward", "character");
		} else {
			webkit_dom_dom_selection_modify (
				window_selection, "move", "backward", "character");
			webkit_dom_dom_selection_modify (
				window_selection, "move", "forward", "character");
		}
	}
/* FIXME WK2
	e_html_editor_selection_unblock_selection_changed (selection);
*/
}

static void
dom_insert_base64_image (WebKitDOMDocument *document,
                         const gchar *base64_content,
                         const gchar *filename,
                         const gchar *uri)
{
	WebKitDOMElement *element, *caret_position, *resizable_wrapper;
	WebKitDOMText *text;

	caret_position = dom_save_caret_position (document);

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
			WEBKIT_DOM_NODE (caret_position)),
		WEBKIT_DOM_NODE (resizable_wrapper),
		WEBKIT_DOM_NODE (caret_position),
		NULL);

	/* We have to again use UNICODE_ZERO_WIDTH_SPACE character to restore
	 * caret on right position */
	text = webkit_dom_document_create_text_node (
		document, UNICODE_ZERO_WIDTH_SPACE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (caret_position)),
		WEBKIT_DOM_NODE (text),
		WEBKIT_DOM_NODE (caret_position),
		NULL);

	dom_restore_caret_position (document);
}

/**
 * e_html_editor_selection_unlink:
 * @selection: an #EHTMLEditorSelection
 *
 * Removes any links (&lt;A&gt; elements) from current selection or at current
 * cursor position.
 */
void
dom_unlink (WebKitDOMDocument *document)
{
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection_dom;
	WebKitDOMRange *range;
	WebKitDOMElement *link;

	window = webkit_dom_document_get_default_view (document);
	selection_dom = webkit_dom_dom_window_get_selection (window);

	range = webkit_dom_dom_selection_get_range_at (selection_dom, 0, NULL);
	link = dom_node_find_parent_element (
		webkit_dom_range_get_start_container (range, NULL), "A");

	if (!link) {
		gchar *text;
		/* get element that was clicked on */
		/* FIXME WK2
		link = e_html_editor_view_get_element_under_mouse_click (view); */
		if (!WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (link))
			link = NULL;

		text = webkit_dom_html_element_get_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (link));
		webkit_dom_html_element_set_outer_html (
			WEBKIT_DOM_HTML_ELEMENT (link), text, NULL);
		g_free (text);
	} else {
		dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_UNLINK, NULL);
	}
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
                 const gchar *uri)
{
	g_return_if_fail (uri != NULL && *uri != '\0');

	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_CREATE_LINK, uri);
}

/**
 * e_html_editor_selection_get_list_format_from_node:
 * @node: an #WebKitDOMNode
 *
 * Returns block format of given list.
 *
 * Returns: #EHTMLEditorSelectionBlockFormat
 */
static EHTMLEditorSelectionBlockFormat
get_list_format_from_node (WebKitDOMNode *node)
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
	WebKitDOMDOMWindow *window;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	window = webkit_dom_document_get_default_view (document);
	range = dom_get_current_range (document);
	if (!range)
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;

	node = webkit_dom_range_get_start_container (range, NULL);
	if (!node)
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
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

static void
dom_set_paragraph_style (EHTMLEditorWebExtension *extension,
                         WebKitDOMDocument *document,
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
create_list_element (EHTMLEditorWebExtension *extension,
                     WebKitDOMDocument *document,
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
		dom_set_paragraph_style (extension, document, list, -1, offset, "");

	return list;
}

static WebKitDOMNode *
get_parent_block_node_from_child (WebKitDOMNode *node)
{
	WebKitDOMElement *parent = WEBKIT_DOM_ELEMENT (
		webkit_dom_node_get_parent_node (node));

	 if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent) ||
	     element_has_tag (parent, "b") ||
	     element_has_tag (parent, "i") ||
	     element_has_tag (parent, "u"))
		parent = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (parent)));

	if (element_has_class (parent, "-x-evo-temp-text-wrapper") ||
	    element_has_class (parent, "-x-evo-signature"))
		parent = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (parent)));

	return WEBKIT_DOM_NODE (parent);
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

	format = get_list_format_from_node (list),
	prev = get_list_format_from_node (prev_sibling);
	next = get_list_format_from_node (next_sibling);

	if (format == prev && format != -1 && prev != -1)
		merge_list_into_list (prev_sibling, list, TRUE);

	if (format == next && format != -1 && next != -1)
		merge_list_into_list (next_sibling, list, FALSE);
}

static void
indent_list (EHTMLEditorWebExtension *extension,
             WebKitDOMDocument *document)
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

		format = get_list_format_from_node (source_list);

		list = create_list_element (
			extension, document, format, get_list_level (item), html_mode);

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
dom_set_indented_style (EHTMLEditorWebExtension *extension,
                        WebKitDOMDocument *document,
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
dom_get_indented_element (EHTMLEditorWebExtension *extension,
                          WebKitDOMDocument *document,
                          gint width)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "DIV", NULL);
	dom_set_indented_style (extension, document, element, width);

	return element;
}

static void
indent_block (EHTMLEditorWebExtension *extension,
              WebKitDOMDocument *document,
              WebKitDOMNode *block,
              gint width)
{
	WebKitDOMElement *element;

	element = dom_get_indented_element (extension, document, width);

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
	WebKitDOMDOMWindow *window;

	document = webkit_dom_node_get_owner_document (node);
	window = webkit_dom_document_get_default_view (document);

	style = webkit_dom_dom_window_get_computed_style (
		window, WEBKIT_DOM_ELEMENT (node), NULL);
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

	g_free (value);

	return alignment;
}

static WebKitDOMElement *
create_selection_marker (WebKitDOMDocument *document,
                         gboolean start)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (
		document, "SPAN", NULL);
	webkit_dom_element_set_id (
		element,
		start ? "-x-evo-selection-start-marker" :
			"-x-evo-selection-end-marker");

	return element;
}

static void
add_selection_markers_into_element_start (WebKitDOMDocument *document,
                                          WebKitDOMElement *element,
                                          WebKitDOMElement **selection_start_marker,
                                          WebKitDOMElement **selection_end_marker)
{
	WebKitDOMElement *marker;

	marker = create_selection_marker (document, FALSE);
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (element),
		WEBKIT_DOM_NODE (marker),
		webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)),
		NULL);
	if (selection_end_marker)
		*selection_end_marker = marker;

	marker = create_selection_marker (document, TRUE);
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (element),
		WEBKIT_DOM_NODE (marker),
		webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)),
		NULL);
	if (selection_start_marker)
		*selection_start_marker = marker;
}

/**
 * e_html_editor_selection_indent:
 * @selection: an #EHTMLEditorSelection
 *
 * Indents current paragraph by one level.
 */
static void
dom_indent (EHTMLEditorWebExtension *extension,
            WebKitDOMDocument *document)
{
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

		add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
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
			indent_list (extension, document);
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

			indent_block (extension, document, block, final_width);

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
				if (!after_selection_start)
					continue;
			}

			level = get_indentation_level (
				WEBKIT_DOM_ELEMENT (block_to_process));

			final_width = word_wrap_length - SPACES_PER_INDENTATION * (level + 1);
			if (final_width < MINIMAL_PARAGRAPH_WIDTH &&
			    !e_html_editor_web_extension_get_html_mode (extension))
				continue;

			indent_block (extension, document, block_to_process, final_width);

			if (after_selection_end)
				break;
		}

 next:
		g_object_unref (list);

		if (!after_selection_end)
			block = next_block;
	}

	dom_selection_restore (document);
	dom_force_spell_check_for_current_paragraph (document);

	/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "indented"); */
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
unindent_block (EHTMLEditorWebExtension *extension,
                WebKitDOMDocument *document,
                WebKitDOMNode *block)
{
	gboolean before_node = TRUE;
	gint word_wrap_length, level, width;
	EHTMLEditorSelectionAlignment alignment;
	WebKitDOMElement *element;
	WebKitDOMElement *prev_blockquote = NULL, *next_blockquote = NULL;
	WebKitDOMNode *block_to_process, *node_clone, *child;

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
		prev_blockquote = dom_get_indented_element (extension, document, width);

	/* Look if we have next siblings, if so, we have to
	 * create new blockquote that will include them */
	if (webkit_dom_node_get_next_sibling (block_to_process))
		next_blockquote = dom_get_indented_element (extension, document, width);

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
			extension, document, WEBKIT_DOM_ELEMENT (node_clone), word_wrap_length, 0, "");
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
static void
dom_unindent (EHTMLEditorWebExtension *extension,
              WebKitDOMDocument *document)
{
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

		body = webkit_dom_document_get_body (document);
		selection_start_marker = webkit_dom_document_create_element (
			document, "SPAN", NULL);
		webkit_dom_element_set_id (
			selection_start_marker, "-x-evo-selection-start-marker");
		webkit_dom_node_insert_before (
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)),
			WEBKIT_DOM_NODE (selection_start_marker),
			webkit_dom_node_get_first_child (
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (body))),
			NULL);
		selection_end_marker = webkit_dom_document_create_element (
			document, "SPAN", NULL);
		webkit_dom_element_set_id (
			selection_end_marker, "-x-evo-selection-end-marker");
		webkit_dom_node_insert_before (
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)),
			WEBKIT_DOM_NODE (selection_end_marker),
			webkit_dom_node_get_first_child (
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (body))),
			NULL);
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

			unindent_block (extension, document, block);

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
				if (!after_selection_start)
					continue;
			}

			unindent_block (extension, document, block_to_process);

			if (after_selection_end)
				break;
		}
 next:
		g_object_unref (list);
		block = next_block;
	}

	dom_selection_restore (document);

	dom_force_spell_check_for_current_paragraph (document);

	/* FIXME XXX - Check if the block is still indented */
	set_dbus_property_boolean (extension, "Indented", TRUE);
}

static WebKitDOMNode *
in_empty_block_in_quoted_content (WebKitDOMNode *element)
{
	WebKitDOMNode *first_child, *next_sibling;

	if (!WEBKIT_DOM_IS_HTML_DIV_ELEMENT (element))
		return NULL;

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
	glong offset;
	WebKitDOMRange *range;
	WebKitDOMNode *container, *next_sibling, *marker_node;
	WebKitDOMNode *split_node, *parent_node;
	WebKitDOMElement *marker;

	/* First remove all markers (if present) */
	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (marker != NULL)
		remove_node (WEBKIT_DOM_NODE (marker));

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");
	if (marker != NULL)
		remove_node (WEBKIT_DOM_NODE (marker));

	range = dom_get_current_range (document);

	if (!range)
		return;

	marker = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_id (marker, "-x-evo-selection-start-marker");

	container = webkit_dom_range_get_start_container (range, NULL);
	offset = webkit_dom_range_get_start_offset (range, NULL);

	if (WEBKIT_DOM_IS_TEXT (container)) {
		if (offset != 0) {
			WebKitDOMText *split_text;

			split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (container), offset, NULL);
			split_node = WEBKIT_DOM_NODE (split_text);
		} else {
			marker_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (container),
				WEBKIT_DOM_NODE (marker),
				container,
				NULL);
			goto end_marker;
		}
	} else if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (container)) {
		marker_node = webkit_dom_node_insert_before (
			container,
			WEBKIT_DOM_NODE (marker),
			webkit_dom_node_get_first_child (container),
			NULL);
		goto end_marker;
	} else {
		/* Insert the selection marker on the right position in
		 * an empty paragraph in the quoted content */
		if ((next_sibling = in_empty_block_in_quoted_content (container))) {
			marker_node = webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (marker),
				next_sibling,
				NULL);
			goto end_marker;
		}
		if (!webkit_dom_node_get_previous_sibling (container)) {
			marker_node = webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (marker),
				webkit_dom_node_get_first_child (container),
				NULL);
			goto end_marker;
		} else if (!webkit_dom_node_get_next_sibling (container)) {
			marker_node = webkit_dom_node_append_child (
				container,
				WEBKIT_DOM_NODE (marker),
				NULL);
			goto end_marker;
		} else {
			if (webkit_dom_node_get_first_child (container)) {
				marker_node = webkit_dom_node_insert_before (
					container,
					WEBKIT_DOM_NODE (marker),
					webkit_dom_node_get_first_child (container),
					NULL);
				goto end_marker;
			}
			split_node = container;
		}
	}

	/* Don't save selection straight into body */
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (split_node))
		return;

	if (!split_node) {
		marker_node = webkit_dom_node_insert_before (
			container,
			WEBKIT_DOM_NODE (marker),
			webkit_dom_node_get_first_child (
				WEBKIT_DOM_NODE (container)),
			NULL);
	} else {
		marker_node = WEBKIT_DOM_NODE (marker);
		parent_node = webkit_dom_node_get_parent_node (split_node);

		webkit_dom_node_insert_before (
			parent_node, marker_node, split_node, NULL);
	}

 end_marker:
	marker = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_id (marker, "-x-evo-selection-end-marker");

	if (webkit_dom_range_get_collapsed (range, NULL)) {
		webkit_dom_node_insert_before (
			/* Selection start marker */
			webkit_dom_node_get_parent_node (marker_node),
			WEBKIT_DOM_NODE (marker),
			webkit_dom_node_get_next_sibling (marker_node),
			NULL);
		return;
	}

	container = webkit_dom_range_get_end_container (range, NULL);
	offset = webkit_dom_range_get_end_offset (range, NULL);

	if (WEBKIT_DOM_IS_TEXT (container)) {
		if (offset != 0) {
			WebKitDOMText *split_text;

			split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (container), offset, NULL);
			split_node = WEBKIT_DOM_NODE (split_text);
		} else {
			marker_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (container),
				WEBKIT_DOM_NODE (marker),
				container,
				NULL);
			goto check;

		}
	} else if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (container)) {
		webkit_dom_node_append_child (
			container, WEBKIT_DOM_NODE (marker), NULL);
		return;
	} else {
		/* Insert the selection marker on the right position in
		 * an empty paragraph in the quoted content */
		if ((next_sibling = in_empty_block_in_quoted_content (container))) {
			webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (marker),
				next_sibling,
				NULL);
			return;
		}
		if (!webkit_dom_node_get_previous_sibling (container)) {
			split_node = webkit_dom_node_get_parent_node (container);
		} else if (!webkit_dom_node_get_next_sibling (container)) {
			split_node = webkit_dom_node_get_parent_node (container);
			split_node = webkit_dom_node_get_next_sibling (split_node);
		} else
			split_node = container;
	}

	/* Don't save selection straight into body */
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (split_node)) {
		marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		remove_node (WEBKIT_DOM_NODE (marker));
		return;
	}

	marker_node = WEBKIT_DOM_NODE (marker);

	if (split_node) {
		parent_node = webkit_dom_node_get_parent_node (split_node);

		webkit_dom_node_insert_before (
			parent_node, marker_node, split_node, NULL);
	} else
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (container), marker_node, NULL);

 check:
	if ((next_sibling = webkit_dom_node_get_next_sibling (marker_node))) {
		if (!WEBKIT_DOM_IS_ELEMENT (next_sibling))
			next_sibling = webkit_dom_node_get_next_sibling (next_sibling);
		/* If the selection is collapsed ensure that the selection start marker
		 * is before the end marker */
		if (next_sibling && WEBKIT_DOM_IS_ELEMENT (next_sibling) &&
		    element_has_id (WEBKIT_DOM_ELEMENT (next_sibling), "-x-evo-selection-start-marker")) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (marker_node),
				next_sibling,
				marker_node,
				NULL);
		}
	}
}

static gboolean
is_selection_position_node (WebKitDOMNode *node)
{
	WebKitDOMElement *element;

	if (!node || !WEBKIT_DOM_IS_ELEMENT (node))
		return FALSE;

	element = WEBKIT_DOM_ELEMENT (node);

	return element_has_id (element, "-x-evo-caret-position") ||
	       element_has_id (element, "-x-evo-selection-start-marker") ||
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
	WebKitDOMElement *marker;
	WebKitDOMNode *selection_start_marker, *selection_end_marker;
	WebKitDOMRange *range;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *window;

	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	if (!range)
		return;

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
					remove_node (selection_start_marker);
					remove_node (selection_end_marker);

					return;
				}
			}
		}
	}

	range = webkit_dom_document_create_range (document);
	if (!range)
		return;

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (!marker) {
		marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		if (marker)
			remove_node (WEBKIT_DOM_NODE (marker));
		return;
	}

	webkit_dom_range_set_start_after (range, WEBKIT_DOM_NODE (marker), NULL);
	remove_node (WEBKIT_DOM_NODE (marker));

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");
	if (!marker) {
		marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (marker)
			remove_node (WEBKIT_DOM_NODE (marker));
		return;
	}

	webkit_dom_range_set_end_before (range, WEBKIT_DOM_NODE (marker), NULL);
	remove_node (WEBKIT_DOM_NODE (marker));

	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
}

static gint
find_where_to_break_line (WebKitDOMNode *node,
                          gint max_len,
			  gint word_wrap_length)
{
	gchar *str, *text_start;
	gunichar uc;
	gint pos;
	gint last_space = 0;
	gint length;
	gint ret_val = 0;
	gchar* position;

	text_start =  webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (node));
	length = g_utf8_strlen (text_start, -1);

	pos = 1;
	last_space = 0;
	str = text_start;
	do {
		uc = g_utf8_get_char (str);
		if (!uc) {
			g_free (text_start);
			if (pos <= max_len)
				return pos;
			else
				return last_space;
		}

		/* If last_space is zero then the word is longer than
		 * word_wrap_length characters, so continue until we find
		 * a space */
		if ((pos > max_len) && (last_space > 0)) {
			if (last_space > word_wrap_length) {
				g_free (text_start);
				return last_space - 1;
			}

			if (last_space > max_len) {
				if (g_unichar_isspace (g_utf8_get_char (text_start)))
					ret_val = 1;

				g_free (text_start);
				return ret_val;
			}

			if (last_space == max_len - 1) {
				uc = g_utf8_get_char (str);
				if (g_unichar_isspace (uc) || str[0] == '-')
					last_space++;
			}

			g_free (text_start);
			return last_space;
		}

		if (g_unichar_isspace (uc) || str[0] == '-')
			last_space = pos;

		pos += 1;
		str = g_utf8_next_char (str);
	} while (*str);

	position = g_utf8_offset_to_pointer (text_start, max_len);

	if (g_unichar_isspace (g_utf8_get_char (position))) {
		ret_val = max_len + 1;
	} else {
		if (last_space == 0) {
			/* If word is longer than word_wrap_length, we cannot wrap it */
			ret_val = length;
		} else if (last_space < max_len) {
			ret_val = last_space;
		} else {
			if (length > word_wrap_length)
				ret_val = last_space;
			else
				ret_val = 0;
		}
	}

	g_free (text_start);

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
static gboolean
dom_selection_is_collapsed (WebKitDOMDocument *document)
{
	WebKitDOMRange *range;

	range = dom_get_current_range (document);
	if (!range)
		return TRUE;

	return webkit_dom_range_get_collapsed (range, NULL);
}

static void
dom_scroll_to_caret (WebKitDOMDocument *document)
{
	glong element_top, element_left;
	glong window_top, window_left, window_right, window_bottom;
	WebKitDOMDOMWindow *window;
	WebKitDOMElement *selection_start_marker;

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (!selection_start_marker)
		return;

	window = webkit_dom_document_get_default_view (document);

	window_top = webkit_dom_dom_window_get_scroll_y (window);
	window_left = webkit_dom_dom_window_get_scroll_x (window);
	window_bottom = window_top + webkit_dom_dom_window_get_inner_height (window);
	window_right = window_left + webkit_dom_dom_window_get_inner_width (window);

	element_left = webkit_dom_element_get_offset_left (selection_start_marker);
	element_top = webkit_dom_element_get_offset_top (selection_start_marker);

	/* Check if caret is inside viewport, if not move to it */
	if (!(element_top >= window_top && element_top <= window_bottom &&
	     element_left >= window_left && element_left <= window_right)) {
		webkit_dom_element_scroll_into_view (selection_start_marker, TRUE);
	}

	dom_selection_restore (document);
}

/**
 * e_html_editor_selection_insert_html:
 * @selection: an #EHTMLEditorSelection
 * @html_text: an HTML code to insert
 *
 * Insert @html_text into document at current cursor position. When a text range
 * is selected, it will be replaced by @html_text.
 */
static void
dom_insert_html (EHTMLEditorWebExtension *extension,
                 WebKitDOMDocument *document,
                 const gchar *html_text)
{
	g_return_if_fail (html_text != NULL);

	if (e_html_editor_web_extension_get_html_mode (extension)) {
		dom_exec_command (
			document, E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML, html_text);
/* FIXME WK2
		e_html_editor_view_check_magic_links (view, FALSE);
*/
		dom_force_spell_check (document);
		dom_scroll_to_caret (document);
	} else
		dom_convert_and_insert_html_into_selection (document, extension, html_text, TRUE);
}

static WebKitDOMElement *
wrap_lines (EHTMLEditorWebExtension *extension,
            WebKitDOMDocument *document,
            WebKitDOMNode *paragraph,
	    gboolean remove_all_br,
	    gint word_wrap_length)
{
	WebKitDOMNode *node, *start_node;
	WebKitDOMNode *paragraph_clone;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMElement *element;
	WebKitDOMNodeList *wrap_br;
	gboolean has_selection;
	gint len, ii, br_count;
	gulong length_left;
	glong paragraph_char_count;
	gchar *text_content;

	has_selection = !dom_selection_is_collapsed (document);

	if (has_selection) {
		const gchar *selection_content;

		selection_content = e_html_editor_web_extension_get_selection_text (extension);
		paragraph_char_count = g_utf8_strlen (selection_content, -1);

		fragment = webkit_dom_range_clone_contents (
			dom_get_current_range (document), NULL);

		/* Select all BR elements or just ours that are used for wrapping.
		 * We are not removing user BR elements when this function is activated
		 * from Format->Wrap Lines action */
		wrap_br = webkit_dom_document_fragment_query_selector_all (
			fragment,
			remove_all_br ? "br" : "br.-x-evo-wrap-br",
			NULL);
		br_count = webkit_dom_node_list_get_length (wrap_br);
		/* And remove them */
		for (ii = 0; ii < br_count; ii++)
			remove_node (webkit_dom_node_list_item (wrap_br, ii));
		g_object_unref (wrap_br);
	} else {
		if (!webkit_dom_node_has_child_nodes (paragraph))
			return WEBKIT_DOM_ELEMENT (paragraph);

		paragraph_clone = webkit_dom_node_clone_node (paragraph, TRUE);
		element = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (paragraph_clone),
			"span#-x-evo-caret-position",
			NULL);
		text_content = webkit_dom_node_get_text_content (paragraph_clone);
		paragraph_char_count = g_utf8_strlen (text_content, -1);
		if (element)
			paragraph_char_count--;
		g_free (text_content);

		/* When we wrap, we are wrapping just the text after caret, text
		 * before the caret is already wrapped, so unwrap the text after
		 * the caret position */
		element = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (paragraph_clone),
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
	}

	if (has_selection) {
		node = WEBKIT_DOM_NODE (fragment);
		start_node = node;
	} else {
		webkit_dom_node_normalize (paragraph_clone);
		node = webkit_dom_node_get_first_child (paragraph_clone);
		if (node) {
			text_content = webkit_dom_node_get_text_content (node);
			if (g_strcmp0 ("\n", text_content) == 0)
				node = webkit_dom_node_get_next_sibling (node);
			g_free (text_content);
		}

		/* We have to start from the end of the last wrapped line */
		element = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (paragraph_clone),
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
			start_node = paragraph_clone;
		} else
			start_node = node;
	}

	len = 0;
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

				text_content = webkit_dom_node_get_text_content (node);
				anchor_length = g_utf8_strlen (text_content, -1);
				if (len + anchor_length > word_wrap_length) {
					element = webkit_dom_document_create_element (
						document, "BR", NULL);
					element_add_class (element, "-x-evo-wrap-br");
					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						node,
						NULL);
					len = anchor_length;
				} else
					len += anchor_length;

				g_free (text_content);
				/* If there is space after the anchor don't try to
				 * wrap before it */
				node = webkit_dom_node_get_next_sibling (node);
				if (WEBKIT_DOM_IS_TEXT (node)) {
					text_content = webkit_dom_node_get_text_content (node);
					if (g_strcmp0 (text_content, " ") == 0) {
						node = webkit_dom_node_get_next_sibling (node);
						len++;
					}
					g_free (text_content);
				}
				continue;
			}

			/* When we are not removing user-entered BR elements (lines wrapped by user),
			 * we need to skip those elements */
			if (!remove_all_br && WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
				if (!element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br")) {
					len = 0;
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

		if ((length_left + len) < word_wrap_length) {
			len += length_left;
			goto next_node;
		}

		/* wrap until we have something */
		while ((length_left + len) > word_wrap_length) {
			gint max_length;

			max_length = word_wrap_length - len;
			if (max_length < 0)
				max_length = word_wrap_length;
			/* Find where we can line-break the node so that it
			 * effectively fills the rest of current row */
			offset = find_where_to_break_line (
				node, max_length, word_wrap_length);

			element = webkit_dom_document_create_element (document, "BR", NULL);
			element_add_class (element, "-x-evo-wrap-br");

			if (offset > 0 && offset <= word_wrap_length) {
				if (offset != length_left)
					webkit_dom_text_split_text (
						WEBKIT_DOM_TEXT (node), offset, NULL);

				if (webkit_dom_node_get_next_sibling (node)) {
					gchar *nd_content;
					WebKitDOMNode *nd = webkit_dom_node_get_next_sibling (node);

					nd = webkit_dom_node_get_next_sibling (node);
					nd_content = webkit_dom_node_get_text_content (nd);
					if (nd_content && *nd_content) {
						if (g_str_has_prefix (nd_content, " "))
							webkit_dom_character_data_replace_data (
								WEBKIT_DOM_CHARACTER_DATA (nd), 0, 1, "", NULL);
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
							webkit_dom_character_data_replace_data (
								WEBKIT_DOM_CHARACTER_DATA (nd), 0, 1, "", NULL);
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
				} else {
					webkit_dom_node_append_child (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						NULL);
				}
				len = 0;
				break;
			} else {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (node),
					WEBKIT_DOM_NODE (element),
					node,
					NULL);
			}
			length_left = webkit_dom_character_data_get_length (
				WEBKIT_DOM_CHARACTER_DATA (node));

			len = 0;
		}
		len += length_left - offset;
 next_node:
		if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (node))
			len = 0;

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
		html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (element));

		/* Overwrite the current selection be the processed content */
		dom_insert_html (extension, document, html);

		g_free (html);

		return NULL;
	} else {
		webkit_dom_node_normalize (paragraph_clone);

		node = webkit_dom_node_get_parent_node (paragraph);
		if (node) {
			/* Replace paragraph with wrapped one */
			webkit_dom_node_replace_child (
				node, paragraph_clone, paragraph, NULL);
		}

		return WEBKIT_DOM_ELEMENT (paragraph_clone);
	}
}

static WebKitDOMElement *
dom_get_paragraph_element (EHTMLEditorWebExtension *extension,
                           WebKitDOMDocument *document,
                           gint width,
                           gint offset)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "DIV", NULL);
	dom_set_paragraph_style (extension, document, element, width, offset, "");

	return element;
}

static WebKitDOMElement *
dom_put_node_into_paragraph (EHTMLEditorWebExtension *extension,
                             WebKitDOMDocument *document,
                             WebKitDOMNode *node,
                             WebKitDOMNode *caret_position)
{
	WebKitDOMRange *range;
	WebKitDOMElement *container;

	range = webkit_dom_document_create_range (document);
	container = dom_get_paragraph_element (extension, document, -1, 0);
	webkit_dom_range_select_node (range, node, NULL);
	webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (container), NULL);
	/* We have to move caret position inside this container */
	webkit_dom_node_append_child (WEBKIT_DOM_NODE (container), caret_position, NULL);

	return container;
}

/**
 * e_html_editor_selection_wrap_lines:
 * @selection: an #EHTMLEditorSelection
 *
 * Wraps all lines in current selection to be 71 characters long.
 */

static void
dom_wrap_lines (EHTMLEditorWebExtension *extension,
                WebKitDOMDocument *document)
{
	gint word_wrap_length;
	WebKitDOMRange *range;
	WebKitDOMElement *active_paragraph, *caret;

	word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);

	caret = dom_save_caret_position (document);
	if (dom_selection_is_collapsed (document)) {
		WebKitDOMNode *end_container;
		WebKitDOMNode *parent;
		WebKitDOMNode *paragraph;
		gchar *text_content;

		/* We need to save caret position and restore it after
		 * wrapping the selection, but we need to save it before we
		 * start to modify selection */
		range = dom_get_current_range (document);
		if (!range)
			return;

		end_container = webkit_dom_range_get_common_ancestor_container (range, NULL);

		/* Wrap only text surrounded in DIV and P tags */
		parent = webkit_dom_node_get_parent_node(end_container);
		if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (parent) ||
		    WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT (parent)) {
			element_add_class (
				WEBKIT_DOM_ELEMENT (parent), "-x-evo-paragraph");
			paragraph = parent;
		} else {
			WebKitDOMElement *parent_div =
				dom_node_find_parent_element (parent, "DIV");

			if (element_has_class (parent_div, "-x-evo-paragraph")) {
				paragraph = WEBKIT_DOM_NODE (parent_div);
			} else {
				if (!caret)
					return;

				/* We try to select previous sibling */
				paragraph = webkit_dom_node_get_previous_sibling (
					WEBKIT_DOM_NODE (caret));
				if (paragraph) {
					/* When there is just text without container
					 * we have to surround it with paragraph div */
					if (WEBKIT_DOM_IS_TEXT (paragraph))
						paragraph = WEBKIT_DOM_NODE (
							dom_put_node_into_paragraph (
								extension,
								document,
								paragraph,
								WEBKIT_DOM_NODE (caret)));
				} else {
					/* When some weird element is selected, return */
					dom_clear_caret_position_marker (document);
					return;
				}
			}
		}

		if (!paragraph)
			return;

		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (paragraph), "style");
		webkit_dom_element_set_id (
			WEBKIT_DOM_ELEMENT (paragraph), "-x-evo-active-paragraph");

		text_content = webkit_dom_node_get_text_content (paragraph);
		/* If there is hidden space character in the beginning we remove it */
		if (strstr (text_content, UNICODE_ZERO_WIDTH_SPACE)) {
			if (g_str_has_prefix (text_content, UNICODE_ZERO_WIDTH_SPACE)) {
				WebKitDOMNode *node;

				node = webkit_dom_node_get_first_child (paragraph);

				webkit_dom_character_data_delete_data (
					WEBKIT_DOM_CHARACTER_DATA (node),
					0,
					1,
					NULL);
			}
			if (g_str_has_suffix (text_content, UNICODE_ZERO_WIDTH_SPACE)) {
				WebKitDOMNode *node;

				node = webkit_dom_node_get_last_child (paragraph);

				webkit_dom_character_data_delete_data (
					WEBKIT_DOM_CHARACTER_DATA (node),
					g_utf8_strlen (text_content, -1) -1,
					1,
					NULL);
			}
		}
		g_free (text_content);

		wrap_lines (
			extension, document, paragraph, FALSE, word_wrap_length);

	} else {
		dom_save_caret_position (document);
		/* If we have selection -> wrap it */
		wrap_lines (
			extension, document, NULL, FALSE, word_wrap_length); }

	active_paragraph = webkit_dom_document_get_element_by_id (
		document, "-x-evo-active-paragraph");
	/* We have to move caret on position where it was before modifying the text */
	dom_restore_caret_position (document);

	/* Set paragraph as non-active */
	if (active_paragraph)
		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (active_paragraph), "id");
}

static WebKitDOMElement *
dom_wrap_paragraph_length (EHTMLEditorWebExtension *extension,
                           WebKitDOMDocument *document,
                           WebKitDOMElement *paragraph,
                           gint length)
{
	g_return_val_if_fail (WEBKIT_DOM_IS_ELEMENT (paragraph), NULL);
	g_return_val_if_fail (length >= MINIMAL_PARAGRAPH_WIDTH, NULL);

	return wrap_lines (extension, document, WEBKIT_DOM_NODE (paragraph), FALSE, length);
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

static void
dom_wrap_paragraphs_in_document (EHTMLEditorWebExtension *extension,
                                 WebKitDOMDocument *document)
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
					extension, document, WEBKIT_DOM_ELEMENT (item), word_wrap_length - quote);
				item = webkit_dom_node_get_next_sibling (item);
			}
		} else {
			dom_wrap_paragraph_length (
				extension, document, WEBKIT_DOM_ELEMENT (node), word_wrap_length - quote);
		}
	}
	g_object_unref (list);
}

static WebKitDOMElement *
dom_wrap_paragraph (EHTMLEditorWebExtension *extension,
                    WebKitDOMDocument *document,
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
		extension, document, WEBKIT_DOM_ELEMENT (paragraph), final_width);
}

static void
html_editor_selection_modify (WebKitDOMDocument *document,
                              const gchar *alter,
                              gboolean forward,
                              EHTMLEditorSelectionGranularity granularity)
{
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	const gchar *granularity_str;

	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	switch (granularity) {
		case E_HTML_EDITOR_SELECTION_GRANULARITY_CHARACTER:
			granularity_str = "character";
			break;
		case E_HTML_EDITOR_SELECTION_GRANULARITY_WORD:
			granularity_str = "word";
			break;
	}

	webkit_dom_dom_selection_modify (
		dom_selection, alter,
		forward ? "forward" : "backward",
		granularity_str);
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

	tag_len = strlen (style_tag);
	result = FALSE;
	while (!result && element) {
		gchar *element_tag;
		gboolean accept_citation = FALSE;

		element_tag = webkit_dom_element_get_tag_name (element);

		if (g_ascii_strncasecmp (style_tag, "citation", 8) == 0) {
			accept_citation = TRUE;
			result = ((strlen (element_tag) == 10 /* strlen ("blockquote") */) &&
				(g_ascii_strncasecmp (element_tag, "blockquote", 10) == 0));
			if (element_has_class (element, "-x-evo-indented"))
				result = FALSE;
		} else {
			result = ((tag_len == strlen (element_tag)) &&
				(g_ascii_strncasecmp (element_tag, style_tag, tag_len) == 0));
		}

		/* Special case: <blockquote type=cite> marks quotation, while
		 * just <blockquote> is used for indentation. If the <blockquote>
		 * has type=cite, then ignore it unless style_tag is "citation" */
		if (result && g_ascii_strncasecmp (element_tag, "blockquote", 10) == 0) {
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
	gboolean ret_val;
	gchar *value, *text_content;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	window = webkit_dom_document_get_default_view (document);

	range = dom_get_current_range (document);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set underline property,
	 * otherwise it will be turned off because of no text in composer */
	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return e_html_editor_web_extension_get_underline (extension);
	}
	g_free (text_content);

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-decoration");

	if (g_strstr_len (value, -1, "underline"))
		ret_val = TRUE;
	else
		ret_val = get_has_style (document, "u");

	g_free (value);
	return ret_val;
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

	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE, NULL);

	set_dbus_property_boolean (extension, "Underline", underline);
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
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	range = dom_get_current_range (document);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);

	while (node) {
		gchar *tag_name;

		tag_name = webkit_dom_element_get_tag_name (WEBKIT_DOM_ELEMENT (node));

		if (g_ascii_strncasecmp (tag_name, "sup", 3) == 0) {
			g_free (tag_name);
			break;
		}

		g_free (tag_name);
		node = webkit_dom_node_get_parent_node (node);
	}

	return (node != NULL);
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

	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_SUBSCRIPT, NULL);

/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "subscript");
*/
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
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	range = dom_get_current_range (document);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);

	while (node) {
		gchar *tag_name;

		tag_name = webkit_dom_element_get_tag_name (WEBKIT_DOM_ELEMENT (node));

		if (g_ascii_strncasecmp (tag_name, "sup", 3) == 0) {
			g_free (tag_name);
			break;
		}

		g_free (tag_name);
		node = webkit_dom_node_get_parent_node (node);
	}

	return (node != NULL);
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

	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_SUPERSCRIPT, NULL);

/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "superscript");
*/
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
	gboolean ret_val;
	gchar *value, *text_content;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	window = webkit_dom_document_get_default_view (document);
	range = dom_get_current_range (document);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set strikethrough property,
	 * otherwise it will be turned off because of no text in composer */
	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return e_html_editor_web_extension_get_strikethrough (extension);
	}
	g_free (text_content);

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-decoration");

	if (g_strstr_len (value, -1, "line-through"))
		ret_val = TRUE;
	else
		ret_val = get_has_style (document, "strike");

	g_free (value);
	return ret_val;
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
/* FIXME WK2
	selection->priv->is_strikethrough = strikethrough;
*/

	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH, NULL);
/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "strikethrough");
*/
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
	gboolean ret_val;
	gchar *value, *text_content;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	window = webkit_dom_document_get_default_view (document);

	range = dom_get_current_range (document);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set italic property,
	 * otherwise it will be turned off because of no text in composer */
	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return FALSE;
/* FIXME WK2	return selection->priv->is_monospaced; */
	}
	g_free (text_content);

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "font-family");

	if (g_strstr_len (value, -1, "monospace"))
		ret_val = TRUE;
	else
		ret_val = FALSE;

	g_free (value);
	return ret_val;
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
	guint font_size = 0;
	WebKitDOMRange *range;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *window_selection;

	if (dom_selection_is_monospaced (document, extension) == monospaced)
		return;
/* FIXME WK2
	selection->priv->is_monospaced = monospaced;
*/
	range = dom_get_current_range (document);
	if (!range)
		return;

	font_size = e_html_editor_web_extension_get_font_size (extension);
	if (font_size == 0)
		font_size = E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;

	window = webkit_dom_document_get_default_view (document);
	window_selection = webkit_dom_dom_window_get_selection (window);

	if (monospaced) {
		gchar *font_size_str;
		WebKitDOMElement *monospace;

		monospace = webkit_dom_document_create_element (
			document, "font", NULL);
		webkit_dom_element_set_attribute (
			monospace, "face", "monospace", NULL);
		font_size_str = g_strdup_printf ("%d", font_size);
		webkit_dom_element_set_attribute (
			monospace, "size", font_size_str, NULL);
		g_free (font_size_str);

		if (!dom_selection_is_collapsed (document)) {
			gchar *html, *outer_html;
			WebKitDOMNode *range_clone;

			range_clone = WEBKIT_DOM_NODE (
				webkit_dom_range_clone_contents (range, NULL));

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (monospace), range_clone, NULL);

			outer_html = webkit_dom_html_element_get_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (monospace));

			html = g_strconcat (
				/* Mark selection for restoration */
				"<span id=\"-x-evo-selection-start-marker\"></span>",
				outer_html,
				"<span id=\"-x-evo-selection-end-marker\"></span>",
				NULL),

			dom_insert_html (extension, document, html);

			dom_selection_restore (document);

			g_free (html);
			g_free (outer_html);
		} else {
			/* https://bugs.webkit.org/show_bug.cgi?id=15256 */
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (monospace),
				UNICODE_ZERO_WIDTH_SPACE,
				NULL);
			webkit_dom_range_insert_node (
				range, WEBKIT_DOM_NODE (monospace), NULL);

			dom_move_caret_into_element (document, monospace);
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

			if (!is_monospaced_element (tt_element))
				return;
		}

		/* Save current formatting */
		is_bold = e_html_editor_web_extension_get_bold (extension);
		is_italic = e_html_editor_web_extension_get_italic (extension);
		is_underline = e_html_editor_web_extension_get_underline (extension);
		is_strikethrough = e_html_editor_web_extension_get_strikethrough (extension);

		if (!dom_selection_is_collapsed (document)) {
			gchar *html, *outer_html, *inner_html, *beginning, *end;
			gchar *start_position, *end_position, *font_size_str;
			WebKitDOMElement *wrapper;

			wrapper = webkit_dom_document_create_element (
				document, "SPAN", NULL);
			webkit_dom_element_set_id (wrapper, "-x-evo-remove-tt");
			webkit_dom_range_surround_contents (
				range, WEBKIT_DOM_NODE (wrapper), NULL);

			html = webkit_dom_html_element_get_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (tt_element));

			start_position = g_strstr_len (
				html, -1, "<span id=\"-x-evo-remove-tt\"");
			end_position = g_strstr_len (start_position, -1, "</span>");

			beginning = g_utf8_substring (
				html, 0, g_utf8_pointer_to_offset (html, start_position));
			inner_html = webkit_dom_html_element_get_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (wrapper));
			end = g_utf8_substring (
				html,
				g_utf8_pointer_to_offset (html, end_position) + 7,
				g_utf8_strlen (html, -1)),

			font_size_str = g_strdup_printf ("%d", font_size);

			outer_html =
				g_strconcat (
					/* Beginning */
					beginning,
					/* End the previous FONT tag */
					"</font>",
					/* Mark selection for restoration */
					"<span id=\"-x-evo-selection-start-marker\"></span>",
					/* Inside will be the same */
					inner_html,
					"<span id=\"-x-evo-selection-end-marker\"></span>",
					/* Start the new FONT element */
					"<font face=\"monospace\" size=\"",
					font_size_str,
					"\">",
					/* End - we have to start after </span> */
					end,
					NULL),

			g_free (font_size_str);

			webkit_dom_html_element_set_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (tt_element),
				outer_html,
				NULL);

			dom_selection_restore (document);

			g_free (html);
			g_free (outer_html);
			g_free (inner_html);
			g_free (beginning);
			g_free (end);
		} else {
			WebKitDOMRange *new_range;
			gchar *outer_html;
			gchar *tmp;

			webkit_dom_element_set_id (tt_element, "ev-tt");

		        outer_html = webkit_dom_html_element_get_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (tt_element));
			tmp = g_strconcat (outer_html, UNICODE_ZERO_WIDTH_SPACE, NULL);
			webkit_dom_html_element_set_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (tt_element),
				tmp, NULL);

			/* We need to get that element again */
			tt_element = webkit_dom_document_get_element_by_id (
				document, "ev-tt");
			webkit_dom_element_remove_attribute (
				WEBKIT_DOM_ELEMENT (tt_element), "id");

			new_range = webkit_dom_document_create_range (document);
			webkit_dom_range_set_start_after (
				new_range, WEBKIT_DOM_NODE (tt_element), NULL);
			webkit_dom_range_set_end_after (
				new_range, WEBKIT_DOM_NODE (tt_element), NULL);

			webkit_dom_dom_selection_remove_all_ranges (
				window_selection);
			webkit_dom_dom_selection_add_range (
				window_selection, new_range);

			webkit_dom_dom_selection_modify (
				window_selection, "move", "right", "character");

			g_free (outer_html);
			g_free (tmp);

			dom_force_spell_check_for_current_paragraph (document);
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

		dom_selection_set_font_size (document, extension, font_size);
	}
/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "monospaced");*/
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
	gboolean ret_val;
	gchar *value, *text_content;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	window = webkit_dom_document_get_default_view (document);

	range = dom_get_current_range (document);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set bold property,
	 * otherwise it will be turned off because of no text in composer */
	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return e_html_editor_web_extension_get_bold (extension);
	}
	g_free (text_content);

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "font-weight");

	if (g_strstr_len (value, -1, "normal"))
		ret_val = FALSE;
	else
		ret_val = TRUE;

	g_free (value);
	return ret_val;
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
	if (dom_selection_is_bold (document, extension) == bold)
		return;
/* FIXME WK2
	selection->priv->is_bold = bold; */

	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_BOLD, NULL);
/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "bold");*/
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
	gboolean ret_val;
	gchar *value, *text_content;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	window = webkit_dom_document_get_default_view (document);

	range = dom_get_current_range (document);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set italic property,
	 * otherwise it will be turned off because of no text in composer */
	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return e_html_editor_web_extension_get_italic (extension);
	}
	g_free (text_content);

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "font-style");

	if (g_strstr_len (value, -1, "italic"))
		ret_val = TRUE;
	else
		ret_val = FALSE;

	g_free (value);
	return ret_val;
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
/* FIXME WK2
	selection->priv->is_italic = italic;*/

	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_ITALIC, NULL);
/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "italic");*/
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

	if (dom_selection_is_collapsed (document)) {
		element = get_element_for_inspection (range);
		return element_has_class (element, "-x-evo-indented");
	} else {
		/* If there is a selection search in it and don't look just in
		 * the end container */
		WebKitDOMDocumentFragment *fragment;

		fragment = webkit_dom_range_clone_contents (range, NULL);

		if (fragment) {
			gboolean ret_val = TRUE;

			element = webkit_dom_document_fragment_query_selector (
				fragment, ".-x-evo-indented", NULL);

			if (!element) {
				element = get_element_for_inspection (range);
				ret_val = element_has_class (element, "-x-evo-indented");
			}

			g_object_unref (fragment);
			return ret_val;
		}
	}

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

	if (WEBKIT_DOM_IS_TEXT (node))
		return get_has_style (document, "citation");

	/* If we are changing the format of block we have to re-set bold property,
	 * otherwise it will be turned off because of no text in composer */
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
	if (!size)
		return E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;

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
	gchar *size_str;
/* FIXME WK2
	selection->priv->font_size = font_size; */

	size_str = g_strdup_printf ("%d", font_size);
	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE, size_str);
	g_free (size_str);
/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "font-size"); */
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
	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_FONT_NAME, font_name);
/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "font-name");*/
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
	WebKitDOMNode *node;
	WebKitDOMRange *range;
	WebKitDOMCSSStyleDeclaration *css;

	range = dom_get_current_range (document);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
/* FIXME WK2
	g_free (selection->priv->font_family);
*/
	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (node));
/* FIXME WK2
	selection->priv->font_family =
		webkit_dom_css_style_declaration_get_property_value (css, "fontFamily");
*/
	return webkit_dom_css_style_declaration_get_property_value (css, "fontFamily");
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
/* FIXME WK2
	selection->priv->font_color = g_strdup (color); */
	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_FORE_COLOR, color);

/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "font-color"); */
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

	if (dom_selection_is_collapsed (document)) {
/* FIXME WK2
		color = g_strdup (selection->priv->font_color);*/
	} else {
		color = get_font_property (document, "color");
		if (!color)
			color = g_strdup ("#000000");
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
		result = get_list_format_from_node (WEBKIT_DOM_NODE (element));
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

	return result;
}

static void
remove_wrapping_from_element (WebKitDOMElement *element)
{
	WebKitDOMNodeList *list;
	gint ii, length;

	list = webkit_dom_element_query_selector_all (
		element, "br.-x-evo-wrap-br", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++)
		remove_node (webkit_dom_node_list_item (list, ii));

	webkit_dom_node_normalize (WEBKIT_DOM_NODE (element));

	g_object_unref (list);
}

static void
remove_quoting_from_element (WebKitDOMElement *element)
{
	gint ii, length;
	WebKitDOMNodeList *list;

	list = webkit_dom_element_query_selector_all (
		element, "span.-x-evo-quoted", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++)
		remove_node (webkit_dom_node_list_item (list, ii));
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
	}
	g_object_unref (list);

	list = webkit_dom_element_query_selector_all (
		element, "br.-x-evo-temp-br", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++)
		remove_node (webkit_dom_node_list_item (list, ii));
	g_object_unref (list);

	webkit_dom_node_normalize (WEBKIT_DOM_NODE (element));
}

static gboolean
is_citation_node (WebKitDOMNode *node)
{
	char *value;

	if (!WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (node))
		return FALSE;

	value = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "type");

	/* citation == <blockquote type='cite'> */
	if (g_strcmp0 (value, "cite") == 0) {
		g_free (value);
		return TRUE;
	} else {
		g_free (value);
		return FALSE;
	}
}

static gboolean
process_block_to_block (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension,
                        EHTMLEditorSelectionBlockFormat format,
			const gchar *value,
                        WebKitDOMNode *block,
                        WebKitDOMNode *end_block,
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

		if (is_citation_node (block)) {
			gboolean finished;

			next_block = webkit_dom_node_get_next_sibling (block);
			finished = process_block_to_block (
				document,
				extension,
				format,
				value,
				webkit_dom_node_get_first_child (block),
				end_block,
				html_mode);

			if (finished)
				return TRUE;

			block = next_block;

			continue;
		}

		if (webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), "span.-x-evo-quoted", NULL)) {
			quoted = TRUE;
			remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));
		}

		if (!html_mode)
			remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));

		after_selection_end = webkit_dom_node_is_same_node (block, end_block);

		next_block = webkit_dom_node_get_next_sibling (block);

		if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH)
			element = dom_get_paragraph_element (extension, document, -1, 0);
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

		if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH && !html_mode) {
			gint citation_level;

			citation_level = get_citation_level (WEBKIT_DOM_NODE (element));

			if (citation_level > 0) {
				gint quote, word_wrap_length;

				word_wrap_length =
					e_html_editor_web_extension_get_word_wrap_length (extension);
				quote = citation_level ? citation_level * 2 : 0;

				element = dom_wrap_paragraph_length (
					extension, document, element, word_wrap_length - quote);

			}
		}

		if (quoted)
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
	WebKitDOMNode *block, *end_block;

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

		add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	block = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	end_block = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_end_marker));

	html_mode = e_html_editor_web_extension_get_html_mode (extension);

	/* Process all blocks that are in the selection one by one */
	process_block_to_block (
		document, extension, format, value, block, end_block, html_mode);

	dom_selection_restore (document);
}

static void
format_change_block_to_list (WebKitDOMDocument *document,
                             EHTMLEditorWebExtension *extension,
                             EHTMLEditorSelectionBlockFormat format)
{
	gboolean after_selection_end = FALSE, in_quote = FALSE;
	/* FIXME WK2
	gboolean html_mode = e_html_editor_view_get_html_mode (view);*/
	gboolean html_mode = FALSE;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *item, *list;
	WebKitDOMNode *block, *next_block;

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

		add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	block = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	list = create_list_element (extension, document, format, 0, html_mode);

	if (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (block), "span.-x-evo-quoted", NULL)) {
		WebKitDOMElement *element;

		in_quote = TRUE;

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			dom_create_caret_position_node (document),
			block,
			NULL);

		dom_restore_caret_position (document);

		dom_exec_command (
			document, E_HTML_EDITOR_VIEW_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, NULL);

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

		remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));
		remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));

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

	dom_selection_restore (document);
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

	new_list = create_list_element (extension, document, to, 0, html_mode);

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

	dom_selection_save (document);

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
		goto out;
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
			goto out;
		}
	}

	prev = get_list_format_from_node (prev_list);
	next = get_list_format_from_node (next_list);

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
		goto out;

	format_change_list_from_list (document, extension, format, html_mode);
out:
	dom_selection_restore (document);
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

	dom_selection_save (document);

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
			if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH)
				element = dom_get_paragraph_element (extension, document, -1, 0);
			else
				element = webkit_dom_document_create_element (
					document, value, NULL);

			after_end = webkit_dom_node_contains (next_item, WEBKIT_DOM_NODE (selection_end));

			while (webkit_dom_node_get_first_child (next_item)) {
				WebKitDOMNode *node = webkit_dom_node_get_first_child (next_item);

				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (element), node, NULL);
			}

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

	dom_selection_restore (document);
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

	if (from_list && to_list)
		format_change_list_to_list (document, extension, format, html_mode);

	if (!from_list && !to_list)
		format_change_block_to_block (document, extension, format, value);

	if (from_list && !to_list)
		format_change_list_to_block (document, extension, format, value);

	if (!from_list && to_list)
		format_change_block_to_list (document, extension, format);

	dom_force_spell_check_for_current_paragraph (document);

	/* When changing the format we need to re-set the alignment */
	dom_selection_set_alignment (
		document, extension, e_html_editor_web_extension_get_alignment (extension));

	e_html_editor_web_extension_set_content_changed (extension);
/*
	g_object_notify (G_OBJECT (selection), "block-format");*/
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

	return webkit_dom_css_style_declaration_get_property_value (css, "background-color");
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
	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_BACKGROUND_COLOR, color);
/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "background-color");*/
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
	WebKitDOMDOMWindow *window;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	window = webkit_dom_document_get_default_view (document);
	range = dom_get_current_range (document);
	if (!range)
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;

	node = webkit_dom_range_get_start_container (range, NULL);
	if (!node)
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
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
	gboolean after_selection_end = FALSE;
	const gchar *class = "", *list_class = "";
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;

	if (dom_selection_get_alignment (document, extension) == alignment)
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
/* FIXME WK2
	selection->priv->alignment = alignment;*/

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker)
		return;

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

	dom_selection_restore (document);

	dom_force_spell_check_for_current_paragraph (document);

/* FIXME WK2
	g_object_notify (G_OBJECT (selection), "alignment");*/
}

/**
 * e_html_editor_selection_replace:
 * @selection: an #EHTMLEditorSelection
 * @replacement: a string to replace current selection with
 *
 * Replaces currently selected text with @replacement.
 */
void
dom_selection_replace (EHTMLEditorWebExtension *extension,
                       WebKitDOMDocument *document,
                       const gchar *replacement)
{
	e_html_editor_web_extension_set_content_changed (extension);
	dom_exec_command (document, E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, replacement);
}

/**
 * e_html_editor_selection_replace_caret_word:
 * @selection: an #EHTMLEditorSelection
 * @replacement: a string to replace current caret word with
 *
 * Replaces current word under cursor with @replacement.
 */
void
dom_replace_caret_word (EHTMLEditorWebExtension *extension,
                        WebKitDOMDocument *document,
                        const gchar *replacement)
{
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	e_html_editor_web_extension_set_content_changed (extension);
	range = dom_get_current_range (document);
	webkit_dom_range_expand (range, "word", NULL);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	dom_insert_html (extension, document, replacement);
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
	WebKitDOMRange *range;

	range = dom_get_current_range (document);

	/* Don't operate on the visible selection */
	range = webkit_dom_range_clone_range (range, NULL);
	webkit_dom_range_expand (range, "word", NULL);

	return webkit_dom_range_to_string (range, NULL);
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
	if (WEBKIT_DOM_IS_TEXT (node))
		return TRUE;

	node = webkit_dom_range_get_end_container (range, NULL);
	if (WEBKIT_DOM_IS_TEXT (node))
		return TRUE;

	node = WEBKIT_DOM_NODE (webkit_dom_range_clone_contents (range, NULL));
	while (node) {
		if (WEBKIT_DOM_IS_TEXT (node))
			return TRUE;

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

	return FALSE;
}

