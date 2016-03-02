/*
 * e-html-editor-view-dom-functions.c
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

#include "e-html-editor-view-dom-functions.h"

#include "e-html-editor-selection-dom-functions.h"

#include <e-util/e-misc-utils.h>
#include <e-util/e-emoticon-chooser.h>
#include <web-extensions/e-dom-utils.h>

#include <string.h>

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDocumentUnstable.h>
#include <webkitdom/WebKitDOMDocumentFragmentUnstable.h>
#include <webkitdom/WebKitDOMRangeUnstable.h>
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>
#include <webkitdom/WebKitDOMHTMLElementUnstable.h>

#define HTML_KEY_CODE_BACKSPACE 8
#define HTML_KEY_CODE_RETURN 13
#define HTML_KEY_CODE_CONTROL 17
#define HTML_KEY_CODE_SPACE 32
#define HTML_KEY_CODE_DELETE 46

#define TRY_TO_PRESERVE_BLOCKS 0

/**
 * e_html_editor_view_dom_exec_command:
 * @document: a #WebKitDOMDocument
 * @command: an #EHTMLEditorViewCommand to execute
 * @value: value of the command (or @NULL if the command does not require value)
 *
 * The function will fail when @value is @NULL or empty but the current @command
 * requires a value to be passed. The @value is ignored when the @command does
 * not expect any value.
 *
 * Returns: @TRUE when the command was succesfully executed, @FALSE otherwise.
 */
gboolean
dom_exec_command (WebKitDOMDocument *document,
                  EHTMLEditorWebExtension *extension,
                  EHTMLEditorViewCommand command,
                  const gchar *value)
{
	const gchar *cmd_str = 0;
	gboolean has_value = FALSE;

#define CHECK_COMMAND(cmd,str,val) case cmd:\
	if (val) {\
		g_return_val_if_fail (value && *value, FALSE);\
	}\
	has_value = val; \
	cmd_str = str;\
	break;

	switch (command) {
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_BACKGROUND_COLOR, "BackColor", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_BOLD, "Bold", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_COPY, "Copy", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_CREATE_LINK, "CreateLink", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_CUT, "Cut", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR, "DefaultParagraphSeparator", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_DELETE, "Delete", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FIND_STRING, "FindString", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FONT_NAME, "FontName", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE, "FontSize", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE_DELTA, "FontSizeDelta", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FORE_COLOR, "ForeColor", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FORMAT_BLOCK, "FormatBlock", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FORWARD_DELETE, "ForwardDelete", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_HILITE_COLOR, "HiliteColor", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INDENT, "Indent", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_HORIZONTAL_RULE, "InsertHorizontalRule", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML, "InsertHTML", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_IMAGE, "InsertImage", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_LINE_BREAK, "InsertLineBreak", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, "InsertNewlineInQuotedContent", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_ORDERED_LIST, "InsertOrderedList", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_PARAGRAPH, "InsertParagraph", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, "InsertText", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_UNORDERED_LIST, "InsertUnorderedList", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_ITALIC, "Italic", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_CENTER, "JustifyCenter", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_FULL, "JustifyFull", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_LEFT, "JustifyLeft", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_NONE, "JustifyNone", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_RIGHT, "JustifyRight", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_OUTDENT, "Outdent", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_PASTE, "Paste", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_PASTE_AND_MATCH_STYLE, "PasteAndMatchStyle", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_PASTE_AS_PLAIN_TEXT, "PasteAsPlainText", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_PRINT, "Print", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_REDO, "Redo", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_REMOVE_FORMAT, "RemoveFormat", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_SELECT_ALL, "SelectAll", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH, "Strikethrough", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_STYLE_WITH_CSS, "StyleWithCSS", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_SUBSCRIPT, "Subscript", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_SUPERSCRIPT, "Superscript", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_TRANSPOSE, "Transpose", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE, "Underline", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_UNDO, "Undo", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_UNLINK, "Unlink", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_UNSELECT, "Unselect", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_USE_CSS, "UseCSS", TRUE)
	}

	e_html_editor_web_extension_set_dont_save_history_in_body_input (extension, TRUE);

	return webkit_dom_document_exec_command (
		document, cmd_str, FALSE, has_value ? value : "" );
}

static void
perform_spell_check (WebKitDOMDOMSelection *dom_selection,
                     WebKitDOMRange *start_range,
                     WebKitDOMRange *end_range)
{
	WebKitDOMRange *actual = start_range;

	/* Go through all words to spellcheck them. To avoid this we have to wait for
	 * http://www.w3.org/html/wg/drafts/html/master/editing.html#dom-forcespellcheck */
	/* We are moving forward word by word until we hit the text on the end. */
	while (actual && webkit_dom_range_compare_boundary_points (end_range, 2, actual, NULL) != 0) {
		g_object_unref (actual);
		webkit_dom_dom_selection_modify (
			dom_selection, "move", "forward", "word");
		actual = webkit_dom_dom_selection_get_range_at (
			dom_selection, 0, NULL);
	}
	g_clear_object (&actual);
}

void
dom_force_spell_check_for_current_paragraph (WebKitDOMDocument *document,
                                             EHTMLEditorWebExtension *extension)
{
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMElement *parent, *element;
	WebKitDOMRange *end_range, *actual;
	WebKitDOMText *text;

	if (!e_html_editor_web_extension_get_inline_spelling_enabled (extension))
		return;

	element = webkit_dom_document_query_selector (
		document, "body[spellcheck=true]", NULL);

	if (!element)
		return;

	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)))
		return;

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return;

	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EHTMLEditorSelection and here as well
	 * when we are moving with caret */
	e_html_editor_web_extension_block_selection_changed_callback (extension);

	parent = get_parent_block_element (WEBKIT_DOM_NODE (selection_end_marker));

	/* Append some text on the end of the element */
	text = webkit_dom_document_create_text_node (document, "-x-evo-end");
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (parent),
		WEBKIT_DOM_NODE (text),
		NULL);

	parent = get_parent_block_element (WEBKIT_DOM_NODE (selection_start_marker));

	/* Create range that's pointing on the end of this text */
	end_range = webkit_dom_document_create_range (document);
	webkit_dom_range_select_node_contents (
		end_range, WEBKIT_DOM_NODE (text), NULL);
	webkit_dom_range_collapse (end_range, FALSE, NULL);

	/* Move on the beginning of the paragraph */
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	actual = webkit_dom_document_create_range (document);
	webkit_dom_range_select_node_contents (
		actual, WEBKIT_DOM_NODE (parent), NULL);
	webkit_dom_range_collapse (actual, TRUE, NULL);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, actual);

	actual = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	perform_spell_check (dom_selection, actual, end_range);

	g_object_unref (dom_selection);
	g_object_unref (dom_window);
	g_object_unref (end_range);

	/* Remove the text that we inserted on the end of the paragraph */
	remove_node (WEBKIT_DOM_NODE (text));

	/* Unblock the callbacks */
	e_html_editor_web_extension_unblock_selection_changed_callback (extension);

	dom_selection_restore (document);
}

static void
refresh_spell_check (WebKitDOMDocument *document,
                     EHTMLEditorWebExtension *extension,
                     gboolean enable_spell_check)
{
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *end_range, *actual;
	WebKitDOMText *text;

	body = webkit_dom_document_get_body (document);

	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)))
		return;

	/* Enable/Disable spellcheck in composer */
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body),
		"spellcheck",
		enable_spell_check ? "true" : "false",
		NULL);

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	/* Sometimes the web view is not focused, so we have to save the selection
	 * manually into the body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMNode *child;

		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
		if (!WEBKIT_DOM_IS_HTML_ELEMENT (child))
			return;

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EHTMLEditorSelection and here as well
	 * when we are moving with caret */
	e_html_editor_web_extension_block_selection_changed_callback (extension);

	/* Append some text on the end of the body */
	text = webkit_dom_document_create_text_node (document, "-x-evo-end");
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (body), WEBKIT_DOM_NODE (text), NULL);

	/* Create range that's pointing on the end of this text */
	end_range = webkit_dom_document_create_range (document);
	webkit_dom_range_select_node_contents (
		end_range, WEBKIT_DOM_NODE (text), NULL);
	webkit_dom_range_collapse (end_range, FALSE, NULL);

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	/* Move on the beginning of the document */
	webkit_dom_dom_selection_modify (
		dom_selection, "move", "backward", "documentboundary");

	actual = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	perform_spell_check (dom_selection, actual, end_range);

	g_object_unref (dom_selection);
	g_object_unref (dom_window);
	g_object_unref (end_range);

	/* Remove the text that we inserted on the end of the body */
	remove_node (WEBKIT_DOM_NODE (text));

	/* Unblock the callbacks */
	e_html_editor_web_extension_unblock_selection_changed_callback (extension);

	dom_selection_restore (document);
}

void
dom_turn_spell_check_off (WebKitDOMDocument *document,
                          EHTMLEditorWebExtension *extension)
{
	refresh_spell_check (document, extension, FALSE);
}

void
dom_force_spell_check_in_viewport (WebKitDOMDocument *document,
                                   EHTMLEditorWebExtension *extension)
{
	glong viewport_height;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMElement *last_element;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *end_range, *actual;
	WebKitDOMText *text;

	if (!e_html_editor_web_extension_get_inline_spelling_enabled (extension))
		return;

	body = WEBKIT_DOM_HTML_ELEMENT (webkit_dom_document_query_selector (
		document, "body[spellcheck=true]", NULL));

	if (!body) {
		body = webkit_dom_document_get_body (document);
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body), "spellcheck", "true", NULL);
	}

	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)))
		return;

	dom_selection_save (document);

	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EHTMLEditorSelection and here as well
	 * when we are moving with caret */
	e_html_editor_web_extension_block_selection_changed_callback (extension);

	/* We have to add 10 px offset as otherwise just the HTML element will be returned */
	actual = webkit_dom_document_caret_range_from_point (document, 10, 10);
	if (!actual)
		return;

	/* Append some text on the end of the body */
	text = webkit_dom_document_create_text_node (document, "-x-evo-end");

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	/* We have to add 10 px offset as otherwise just the HTML element will be returned */
	viewport_height = webkit_dom_dom_window_get_inner_height (dom_window);
	last_element = webkit_dom_document_element_from_point (document, 10, viewport_height - 10);
	if (last_element && !WEBKIT_DOM_IS_HTML_HTML_ELEMENT (last_element)) {
		WebKitDOMElement *parent;

		parent = get_parent_block_element (WEBKIT_DOM_NODE (last_element));
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (parent), WEBKIT_DOM_NODE (text), NULL);
	} else
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (body), WEBKIT_DOM_NODE (text), NULL);

	/* Create range that's pointing on the end of viewport */
	end_range = webkit_dom_document_create_range (document);
	webkit_dom_range_select_node_contents (
		end_range, WEBKIT_DOM_NODE (text), NULL);
	webkit_dom_range_collapse (end_range, FALSE, NULL);

	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, actual);
	perform_spell_check (dom_selection, actual, end_range);

	g_object_unref (dom_selection);
	g_object_unref (dom_window);
	g_object_unref (end_range);

	/* Remove the text that we inserted on the end of the body */
	remove_node (WEBKIT_DOM_NODE (text));

	/* Unblock the callbacks */
	e_html_editor_web_extension_unblock_selection_changed_callback (extension);

	dom_selection_restore (document);
}

void
dom_force_spell_check (WebKitDOMDocument *document,
                       EHTMLEditorWebExtension *extension)
{
	if (e_html_editor_web_extension_get_inline_spelling_enabled (extension))
		refresh_spell_check (document, extension, TRUE);
}

gboolean
dom_node_is_citation_node (WebKitDOMNode *node)
{
	gchar *value;

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

gint
get_citation_level (WebKitDOMNode *node,
                    gboolean set_plaintext_quoted)
{
	WebKitDOMNode *parent = node;
	gint level = 0;

	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent) &&
		    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "type")) {
			level++;

			if (set_plaintext_quoted) {
				element_add_class (
					WEBKIT_DOM_ELEMENT (parent),
					"-x-evo-plaintext-quoted");
			}
		}

		parent = webkit_dom_node_get_parent_node (parent);
	}

	return level;
}

static gchar *
get_quotation_for_level (gint quote_level)
{
	gint ii;
	GString *output = g_string_new ("");

	for (ii = 0; ii < quote_level; ii++) {
		g_string_append (output, "<span class=\"-x-evo-quote-character\">");
		g_string_append (output, QUOTE_SYMBOL);
		g_string_append (output, " ");
		g_string_append (output, "</span>");
	}

	return g_string_free (output, FALSE);
}

void
dom_quote_plain_text_element_after_wrapping (WebKitDOMDocument *document,
                                             WebKitDOMElement *element,
                                             gint quote_level)
{
	WebKitDOMNodeList *list;
	WebKitDOMNode *quoted_node;
	gint length, ii;
	gchar *quotation;

	quoted_node = WEBKIT_DOM_NODE (
		webkit_dom_document_create_element (document, "SPAN", NULL));
	webkit_dom_element_set_class_name (
		WEBKIT_DOM_ELEMENT (quoted_node), "-x-evo-quoted");
	quotation = get_quotation_for_level (quote_level);
	webkit_dom_element_set_inner_html (
		WEBKIT_DOM_ELEMENT (quoted_node), quotation, NULL);

	list = webkit_dom_element_query_selector_all (
		element, "br.-x-evo-wrap-br", NULL);
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (element),
		quoted_node,
		webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)),
		NULL);

	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *br = webkit_dom_node_list_item (list, ii);

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (br),
			webkit_dom_node_clone_node (quoted_node, TRUE),
			webkit_dom_node_get_next_sibling (br),
			NULL);
		g_object_unref (br);
	}

	g_object_unref (list);
	g_free (quotation);
}

static gboolean
return_pressed_in_empty_line (WebKitDOMDocument *document)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	range = dom_get_current_range (document);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_start_container (range, NULL);
	if (!WEBKIT_DOM_IS_TEXT (node)) {
		WebKitDOMNode *first_child;

		first_child = webkit_dom_node_get_first_child (node);
		if (first_child && WEBKIT_DOM_IS_ELEMENT (first_child) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (first_child), "-x-evo-quoted")) {
			WebKitDOMNode *prev_sibling;

			prev_sibling = webkit_dom_node_get_previous_sibling (node);
			if (!prev_sibling) {
				gboolean collapsed;

				collapsed = webkit_dom_range_get_collapsed (range, NULL);
				g_object_unref (range);
				return collapsed;
			}
		}
	}

	g_object_unref (range);

	return FALSE;
}

WebKitDOMNode *
get_parent_block_node_from_child (WebKitDOMNode *node)
{
	WebKitDOMNode *parent = node;

	if (!WEBKIT_DOM_IS_ELEMENT (parent) ||
	    dom_is_selection_position_node (parent))
		parent = webkit_dom_node_get_parent_node (parent);

	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-temp-text-wrapper") ||
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-quoted") ||
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-quote-character") ||
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-signature") ||
	    WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent) ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "b") ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "i") ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "u"))
		parent = webkit_dom_node_get_parent_node (parent);

	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-quoted"))
		parent = webkit_dom_node_get_parent_node (parent);

	return parent;
}

WebKitDOMElement *
wrap_and_quote_element (WebKitDOMDocument *document,
			EHTMLEditorWebExtension *extension,
			WebKitDOMElement *element)
{
	gint citation_level;
	WebKitDOMElement *tmp_element = element;

	g_return_val_if_fail (WEBKIT_DOM_IS_ELEMENT (element), element);

	if (e_html_editor_web_extension_get_html_mode (extension))
		return element;

	citation_level = get_citation_level (WEBKIT_DOM_NODE (element), FALSE);

	dom_remove_quoting_from_element (element);
	dom_remove_wrapping_from_element (element);

	if (element_has_class (element, "-x-evo-paragraph")) {
		gint word_wrap_length, length;

		word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);
		length = word_wrap_length - 2 * citation_level;
		tmp_element = dom_wrap_paragraph_length (
			document, extension, element, length);
	}

	if (citation_level > 0) {

		webkit_dom_node_normalize (WEBKIT_DOM_NODE (tmp_element));
		dom_quote_plain_text_element_after_wrapping (
			document, tmp_element, citation_level);
	}

	return tmp_element;
}

WebKitDOMElement *
dom_insert_new_line_into_citation (WebKitDOMDocument *document,
                                   EHTMLEditorWebExtension *extension,
                                   const gchar *html_to_insert)
{
	gboolean html_mode = FALSE, ret_val, avoid_editor_call;
	WebKitDOMElement *element, *paragraph = NULL;
	WebKitDOMNode *last_block;

	html_mode = e_html_editor_web_extension_get_html_mode (extension);

	avoid_editor_call = return_pressed_in_empty_line (document);

	if (avoid_editor_call) {
		WebKitDOMElement *selection_start_marker;
		WebKitDOMNode *current_block, *parent, *parent_block, *block_clone;

		dom_selection_save (document);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		current_block = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		block_clone = webkit_dom_node_clone_node (current_block, TRUE);
		/* Find selection start marker and restore it after the new line
		 * is inserted */
		selection_start_marker = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block_clone), "#-x-evo-selection-start-marker", NULL);

		/* Find parent node that is immediate child of the BODY */
		/* Build the same structure of parent nodes of the current block */
		parent_block = current_block;
		parent = webkit_dom_node_get_parent_node (parent_block);
		while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
			WebKitDOMNode *node;

			parent_block = parent;
			node = webkit_dom_node_clone_node (parent_block, FALSE);
			webkit_dom_node_append_child (node, block_clone, NULL);
			block_clone = node;
			parent = webkit_dom_node_get_parent_node (parent_block);
		}

		paragraph = dom_get_paragraph_element (document, extension, -1, 0);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (paragraph),
			WEBKIT_DOM_NODE (
				webkit_dom_document_create_element (document, "BR", NULL)),
			NULL);

		/* Insert the selection markers to right place */
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (paragraph),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_start_marker)),
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (paragraph)),
			NULL);
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (paragraph),
			WEBKIT_DOM_NODE (selection_start_marker),
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (paragraph)),
			NULL);

		/* Insert the cloned nodes before the BODY parent node */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent_block),
			block_clone,
			parent_block,
			NULL);

		/* Insert the new empty paragraph before the BODY parent node */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent_block),
			WEBKIT_DOM_NODE (paragraph),
			parent_block,
			NULL);

		/* Remove the old block (its copy was moved to the right place) */
		remove_node (current_block);

		dom_selection_restore (document);

		return NULL;
	} else {
		ret_val = dom_exec_command (
			document, extension, E_HTML_EDITOR_VIEW_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, NULL);

		if (!ret_val)
			return NULL;

		element = webkit_dom_document_query_selector (
			document, "body>br", NULL);

		if (!element)
			return NULL;
	}

	last_block = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
	while (last_block && dom_node_is_citation_node (last_block))
		last_block = webkit_dom_node_get_last_child (last_block);

	if (last_block) {
		WebKitDOMNode *last_child;

		if ((last_child = webkit_dom_node_get_last_child (last_block))) {
			if (WEBKIT_DOM_IS_ELEMENT (last_child) &&
			    element_has_class (WEBKIT_DOM_ELEMENT (last_child), "-x-evo-quoted"))
				webkit_dom_node_append_child (
					last_block,
					WEBKIT_DOM_NODE (
						webkit_dom_document_create_element (
							document, "br", NULL)),
					NULL);
		}
	}

	if (!html_mode) {
		WebKitDOMNode *sibling;

		sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));

		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (sibling)) {
			WebKitDOMNode *node;

			node = webkit_dom_node_get_first_child (sibling);
			while (node && dom_node_is_citation_node (node))
				node = webkit_dom_node_get_first_child (node);

			/* Rewrap and requote nodes that were created by split. */
			if (WEBKIT_DOM_IS_ELEMENT (node))
				wrap_and_quote_element (document, extension, WEBKIT_DOM_ELEMENT (node));

			if (WEBKIT_DOM_IS_ELEMENT (last_block))
				wrap_and_quote_element (document, extension, WEBKIT_DOM_ELEMENT (last_block));

			dom_force_spell_check_in_viewport (document, extension);
		}
	}

	if (html_to_insert && *html_to_insert) {
		paragraph = dom_prepare_paragraph (document, extension, FALSE);
		webkit_dom_element_set_inner_html (
			paragraph, html_to_insert, NULL);
		if (!webkit_dom_element_query_selector (paragraph, "#-x-evo-selection-start-marker", NULL))
			dom_add_selection_markers_into_element_end (
				document, paragraph, NULL, NULL);
	} else
		paragraph = dom_prepare_paragraph (document, extension, TRUE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
		WEBKIT_DOM_NODE (paragraph),
		WEBKIT_DOM_NODE (element),
		NULL);

	remove_node (WEBKIT_DOM_NODE (element));

	dom_selection_restore (document);

	return paragraph;
}

/* For purpose of this function see e-mail-formatter-quote.c */
static void
put_body_in_citation (WebKitDOMDocument *document)
{
	WebKitDOMElement *cite_body = webkit_dom_document_query_selector (
		document, "span.-x-evo-cite-body", NULL);

	if (cite_body) {
		WebKitDOMHTMLElement *body = webkit_dom_document_get_body (document);
		WebKitDOMNode *citation;
		WebKitDOMNode *sibling;

		citation = WEBKIT_DOM_NODE (
			webkit_dom_document_create_element (document, "blockquote", NULL));
		webkit_dom_element_set_id (WEBKIT_DOM_ELEMENT (citation), "-x-evo-main-cite");
		webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (citation), "type", "cite", NULL);

		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			citation,
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)),
			NULL);

		while ((sibling = webkit_dom_node_get_next_sibling (citation)))
			webkit_dom_node_append_child (citation, sibling, NULL);

		remove_node (WEBKIT_DOM_NODE (cite_body));
	}
}

/* For purpose of this function see e-mail-formatter-quote.c */
static void
move_elements_to_body (WebKitDOMDocument *document,
		       EHTMLEditorWebExtension *extension)
{
	WebKitDOMHTMLElement *body = webkit_dom_document_get_body (document);
	WebKitDOMNodeList *list;
	gint ii;

	list = webkit_dom_document_query_selector_all (
		document, "div[data-headers]", NULL);
	for (ii = webkit_dom_node_list_get_length (list) - 1; ii >= 0; ii--) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (node), "data-headers");
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			node,
			webkit_dom_node_get_first_child (
				WEBKIT_DOM_NODE (body)),
			NULL);

		g_object_unref (node);
	}
	g_object_unref (list);

	list = webkit_dom_document_query_selector_all (
		document, "span.-x-evo-to-body[data-credits]", NULL);
	for (ii = webkit_dom_node_list_get_length (list) - 1; ii >= 0; ii--) {
		char *credits;
		WebKitDOMElement *element;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		element = dom_get_paragraph_element (document, extension, -1, 0);
		credits = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "data-credits");
		webkit_dom_html_element_set_inner_text (WEBKIT_DOM_HTML_ELEMENT (element), credits, NULL);
		g_free (credits);

		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			WEBKIT_DOM_NODE (element),
			webkit_dom_node_get_first_child (
				WEBKIT_DOM_NODE (body)),
			NULL);

		remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);
}

static void
repair_gmail_blockquotes (WebKitDOMDocument *document)
{
	WebKitDOMNodeList *list;
	gint ii, length;

	list = webkit_dom_document_query_selector_all (
		document, "blockquote.gmail_quote", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "class");
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "style");
		webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (node), "type", "cite", NULL);

		if (!WEBKIT_DOM_IS_HTML_BR_ELEMENT (webkit_dom_node_get_last_child (node)))
			webkit_dom_node_append_child (
				node,
				WEBKIT_DOM_NODE (
					webkit_dom_document_create_element (
						document, "br", NULL)),
				NULL);
		g_object_unref (node);
	}
	g_object_unref (list);
}

void
dom_check_magic_links (WebKitDOMDocument *document,
                       EHTMLEditorWebExtension *extension,
                       gboolean include_space_by_user)
{
	gchar *node_text;
	gchar **urls;
	gboolean include_space = FALSE;
	gboolean is_email_address = FALSE;
	gboolean return_key_pressed =
		e_html_editor_web_extension_get_return_key_pressed (extension);
	GRegex *regex = NULL;
	GMatchInfo *match_info;
	gint start_pos_url, end_pos_url;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	if (!e_html_editor_web_extension_get_magic_links_enabled (extension))
		return;

	if (include_space_by_user == TRUE)
		include_space = TRUE;
	else
		include_space = e_html_editor_web_extension_get_return_key_pressed (extension);

	range = dom_get_current_range (document);
	node = webkit_dom_range_get_end_container (range, NULL);
	g_object_unref (range);

	if (return_key_pressed) {
		WebKitDOMNode* block;

		node = webkit_dom_range_get_end_container (range, NULL);
		block = get_parent_block_node_from_child (node);
		/* Get previous block */
		block = webkit_dom_node_get_previous_sibling (block);
		/* If block is quoted content, get the last block there */
		while (block && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (block))
			block = webkit_dom_node_get_last_child (block);

		/* Get the last non-empty node */
		node = webkit_dom_node_get_last_child (block);
		if (WEBKIT_DOM_IS_CHARACTER_DATA (node) &&
		    webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node)) == 0)
			node = webkit_dom_node_get_previous_sibling (node);
	} else {
		dom_selection_save (document);
		node = webkit_dom_range_get_end_container (range, NULL);
	}

	if (!node)
		goto out;

	if (!WEBKIT_DOM_IS_TEXT (node)) {
		if (webkit_dom_node_has_child_nodes (node))
			node = webkit_dom_node_get_first_child (node);
		if (!WEBKIT_DOM_IS_TEXT (node))
			goto out;
	}

	node_text = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (node));
	if (!(node_text && *node_text) || !g_utf8_validate (node_text, -1, NULL)) {
		g_free (node_text);
		goto out;
	}

	if (strstr (node_text, "@") && !strstr (node_text, "://")) {
		is_email_address = TRUE;
		regex = g_regex_new (include_space ? E_MAIL_PATTERN_SPACE : E_MAIL_PATTERN, 0, 0, NULL);
	} else
		regex = g_regex_new (include_space ? URL_PATTERN_SPACE : URL_PATTERN, 0, 0, NULL);

	if (!regex) {
		g_free (node_text);
		goto out;
	}

	g_regex_match_all (regex, node_text, G_REGEX_MATCH_NOTEMPTY, &match_info);
	urls = g_match_info_fetch_all (match_info);

	if (urls) {
		const gchar *end_of_match = NULL;
		gchar *final_url, *url_end_raw, *url_text;
		glong url_start, url_end, url_length;
		WebKitDOMNode *url_text_node;
		WebKitDOMElement *anchor;

		g_match_info_fetch_pos (match_info, 0, &start_pos_url, &end_pos_url);

		/* Get start and end position of url in node's text because positions
		 * that we get from g_match_info_fetch_pos are not UTF-8 aware */
		url_end_raw = g_strndup(node_text, end_pos_url);
		url_end = g_utf8_strlen (url_end_raw, -1);
		url_length = g_utf8_strlen (urls[0], -1);

		end_of_match = url_end_raw + end_pos_url - (include_space ? 3 : 2);
		/* URLs are extremely unlikely to end with any punctuation, so
		 * strip any trailing punctuation off from link and put it after
		 * the link. Do the same for any closing double-quotes as well. */
		while (end_of_match && end_of_match != url_end_raw && strchr (URL_INVALID_TRAILING_CHARS, *end_of_match)) {
			url_length--;
			url_end--;
			end_of_match--;
		}

		url_start = url_end - url_length;

		webkit_dom_text_split_text (
			WEBKIT_DOM_TEXT (node),
			include_space ? url_end - 1 : url_end,
			NULL);

		webkit_dom_text_split_text (
			WEBKIT_DOM_TEXT (node), url_start, NULL);
		url_text_node = webkit_dom_node_get_next_sibling (node);
		url_text = webkit_dom_character_data_get_data (
			WEBKIT_DOM_CHARACTER_DATA (url_text_node));

		if (g_str_has_prefix (url_text, "www."))
			final_url = g_strconcat ("http://" , url_text, NULL);
		else if (is_email_address)
			final_url = g_strconcat ("mailto:" , url_text, NULL);
		else
			final_url = g_strdup (url_text);

		/* Create and prepare new anchor element */
		anchor = webkit_dom_document_create_element (document, "A", NULL);

		webkit_dom_element_set_inner_html (anchor, url_text, NULL);

		webkit_dom_html_anchor_element_set_href (
			WEBKIT_DOM_HTML_ANCHOR_ELEMENT (anchor),
			final_url);

		/* Insert new anchor element into document */
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (anchor),
			WEBKIT_DOM_NODE (url_text_node),
			NULL);

		g_free (url_end_raw);
		g_free (final_url);
		g_free (url_text);
	} else {
		gboolean appending_to_link = FALSE;
		gchar *href, *text, *url, *text_to_append = NULL;
		gint diff;
		WebKitDOMElement *parent;
		WebKitDOMNode *prev_sibling;

		parent = webkit_dom_node_get_parent_element (node);
		prev_sibling = webkit_dom_node_get_previous_sibling (node);

		/* If previous sibling is ANCHOR and actual text node is not beginning with
		 * space => we're appending to link */
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling)) {
			text_to_append = webkit_dom_node_get_text_content (node);
			if (text_to_append && *text_to_append &&
			    !strstr (text_to_append, " ") &&
			    !strchr (URL_INVALID_TRAILING_CHARS, *text_to_append) &&
			    !g_str_has_prefix (text_to_append, UNICODE_NBSP)) {

				appending_to_link = TRUE;
				parent = WEBKIT_DOM_ELEMENT (prev_sibling);
				/* If the node(text) contains the some of unwanted characters
				 * split it into two nodes and select the right one. */
				if (g_str_has_suffix (text_to_append, UNICODE_NBSP) ||
				    g_str_has_suffix (text_to_append, UNICODE_ZERO_WIDTH_SPACE)) {
					webkit_dom_text_split_text (
						WEBKIT_DOM_TEXT (node),
						g_utf8_strlen (text_to_append, -1) - 1,
						NULL);
					g_free (text_to_append);
					text_to_append = webkit_dom_node_get_text_content (node);
				}
			}
		}

		/* If parent is ANCHOR => we're editing the link */
		if ((!WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent) && !appending_to_link) || !text_to_append) {
			g_match_info_free (match_info);
			g_regex_unref (regex);
			g_free (node_text);
			g_free (text_to_append);
			goto out;
		}

		/* edit only if href and description are the same */
		href = webkit_dom_html_anchor_element_get_href (
			WEBKIT_DOM_HTML_ANCHOR_ELEMENT (parent));

		if (appending_to_link) {
			gchar *inner_text;

			inner_text =
				webkit_dom_html_element_get_inner_text (
					WEBKIT_DOM_HTML_ELEMENT (parent)),

			text = g_strconcat (inner_text, text_to_append, NULL);
			g_free (inner_text);
		} else
			text = webkit_dom_html_element_get_inner_text (
					WEBKIT_DOM_HTML_ELEMENT (parent));

		element_remove_class (parent, "-x-evo-visited-link");

		if (strstr (href, "://") && !strstr (text, "://")) {
			url = strstr (href, "://") + 3;
			diff = strlen (text) - strlen (url);

			if (text [strlen (text) - 1] != '/')
				diff++;

			if ((g_strcmp0 (url, text) != 0 && ABS (diff) == 1) || appending_to_link) {
				gchar *inner_html, *protocol, *new_href;

				protocol = g_strndup (href, strstr (href, "://") - href + 3);
				inner_html = webkit_dom_element_get_inner_html (parent);
				new_href = g_strconcat (
					protocol, inner_html, appending_to_link ? text_to_append : "", NULL);

				webkit_dom_html_anchor_element_set_href (
					WEBKIT_DOM_HTML_ANCHOR_ELEMENT (parent),
					new_href);

				if (appending_to_link) {
					webkit_dom_html_element_insert_adjacent_html (
						WEBKIT_DOM_HTML_ELEMENT (parent),
						"beforeend",
						text_to_append,
						NULL);

					remove_node (node);
				}

				g_free (new_href);
				g_free (protocol);
				g_free (inner_html);
			}
		} else {
			diff = strlen (text) - strlen (href);
			if (text [strlen (text) - 1] != '/')
				diff++;

			if ((g_strcmp0 (href, text) != 0 && ABS (diff) == 1) || appending_to_link) {
				gchar *inner_html;
				gchar *new_href;

				inner_html = webkit_dom_element_get_inner_html (parent);
				new_href = g_strconcat (
						inner_html,
						appending_to_link ? text_to_append : "",
						NULL);

				webkit_dom_html_anchor_element_set_href (
					WEBKIT_DOM_HTML_ANCHOR_ELEMENT (parent),
					new_href);

				if (appending_to_link) {
					webkit_dom_html_element_insert_adjacent_html (
						WEBKIT_DOM_HTML_ELEMENT (parent),
						"beforeend",
						text_to_append,
						NULL);

					remove_node (node);
				}

				g_free (new_href);
				g_free (inner_html);
			}

		}
		g_free (text_to_append);
		g_free (text);
		g_free (href);
	}

	g_match_info_free (match_info);
	g_regex_unref (regex);
	g_free (node_text);

 out:
	if (!return_key_pressed)
		dom_selection_restore (document);
}

void
dom_embed_style_sheet (WebKitDOMDocument *document,
                       const gchar *style_sheet_content)
{
	WebKitDOMElement *sheet;

	e_dom_utils_create_and_add_css_style_sheet (document, "-x-evo-composer-sheet");

	sheet = webkit_dom_document_get_element_by_id (document, "-x-evo-composer-sheet");
	webkit_dom_element_set_attribute (
		sheet,
		"type",
		"text/css",
		NULL);

	webkit_dom_element_set_inner_html (sheet, style_sheet_content, NULL);
}

void
dom_remove_embed_style_sheet (WebKitDOMDocument *document)
{
	WebKitDOMElement *sheet;

	sheet = webkit_dom_document_get_element_by_id (
		document, "-x-evo-composer-sheet");

	remove_node (WEBKIT_DOM_NODE (sheet));
}

static void
insert_delete_event (WebKitDOMDocument *document,
		     EHTMLEditorWebExtension *extension,
                     WebKitDOMRange *range)
{
	EHTMLEditorHistoryEvent *ev;
	WebKitDOMDocumentFragment *fragment;
	EHTMLEditorUndoRedoManager *manager;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

	if (e_html_editor_undo_redo_manager_is_operation_in_progress (manager))
		return;

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	ev->type = HISTORY_DELETE;

	fragment = webkit_dom_range_clone_contents (range, NULL);
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
}

/* Based on original use_pictograms() from GtkHTML */
static const gchar *emoticons_chars =
	/*  0 */ "DO)(|/PQ*!"
	/* 10 */ "S\0:-\0:\0:-\0"
	/* 20 */ ":\0:;=-\"\0:;"
	/* 30 */ "B\"|\0:-'\0:X"
	/* 40 */ "\0:\0:-\0:\0:-"
	/* 50 */ "\0:\0:-\0:\0:-"
	/* 60 */ "\0:\0:\0:-\0:\0"
	/* 70 */ ":-\0:\0:-\0:\0";
static gint emoticons_states[] = {
	/*  0 */  12,  17,  22,  34,  43,  48,  53,  58,  65,  70,
	/* 10 */  75,   0, -15,  15,   0, -15,   0, -17,  20,   0,
	/* 20 */ -17,   0, -14, -20, -14,  28,  63,   0, -14, -20,
	/* 30 */  -3,  63, -18,   0, -12,  38,  41,   0, -12,  -2,
	/* 40 */   0,  -4,   0, -10,  46,   0, -10,   0, -19,  51,
	/* 50 */   0, -19,   0, -11,  56,   0, -11,   0, -13,  61,
	/* 60 */   0, -13,   0,  -6,   0,  68,  -7,   0,  -7,   0,
	/* 70 */ -16,  73,   0, -16,   0, -21,  78,   0, -21,   0 };
static const gchar *emoticons_icon_names[] = {
	"face-angel",
	"face-angry",
	"face-cool",
	"face-crying",
	"face-devilish",
	"face-embarrassed",
	"face-kiss",
	"face-laugh",		/* not used */
	"face-monkey",		/* not used */
	"face-plain",
	"face-raspberry",
	"face-sad",
	"face-sick",
	"face-smile",
	"face-smile-big",
	"face-smirk",
	"face-surprise",
	"face-tired",
	"face-uncertain",
	"face-wink",
	"face-worried"
};

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	EEmoticon *emoticon;
	EHTMLEditorWebExtension *extension;
	gchar *content_type;
	gchar *name;
	WebKitDOMDocument *document;
};

static LoadContext *
emoticon_load_context_new (WebKitDOMDocument *document,
                           EHTMLEditorWebExtension *extension,
                           EEmoticon *emoticon)
{
	LoadContext *load_context;

	load_context = g_slice_new0 (LoadContext);
	load_context->emoticon = emoticon;
	load_context->extension = extension;
	load_context->document = document;

	return load_context;
}

static void
emoticon_load_context_free (LoadContext *load_context)
{
	g_free (load_context->content_type);
	g_free (load_context->name);
	g_slice_free (LoadContext, load_context);
}

static void
emoticon_insert_span (EEmoticon *emoticon,
                      LoadContext *load_context,
                      WebKitDOMElement *span)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	EHTMLEditorWebExtension *extension = load_context->extension;
	gboolean misplaced_selection = FALSE, smiley_written;
	gchar *node_text = NULL;
	const gchar *emoticon_start;
	WebKitDOMDocument *document = load_context->document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *node, *insert_before, *prev_sibling, *next_sibling;
	WebKitDOMNode *selection_end_marker_parent;
	WebKitDOMRange *range;

	smiley_written = e_html_editor_web_extension_get_is_smiley_written (extension);
	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

	if (dom_selection_is_collapsed (document)) {
		dom_selection_save (document);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		if (!smiley_written) {
			if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
				ev = g_new0 (EHTMLEditorHistoryEvent, 1);
				ev->type = HISTORY_SMILEY;

				dom_selection_get_coordinates (
					document,
					&ev->before.start.x,
					&ev->before.start.y,
					&ev->before.end.x,
					&ev->before.end.y);
			}
		}
	} else {
		WebKitDOMRange *tmp_range;

		tmp_range = dom_get_current_range (document);
		insert_delete_event (document, extension, tmp_range);
		g_object_unref (tmp_range);

		dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);

		if (!smiley_written) {
			if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
				ev = g_new0 (EHTMLEditorHistoryEvent, 1);
				ev->type = HISTORY_SMILEY;

				dom_selection_get_coordinates (
					document,
					&ev->before.start.x,
					&ev->before.start.y,
					&ev->before.end.x,
					&ev->before.end.y);
			}
		}

		dom_selection_save (document);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
	}

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

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
	}

	/* Sometimes selection end marker is in body. Move it into next sibling */
	selection_end_marker_parent = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_end_marker));
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (selection_end_marker_parent)) {
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (selection_start_marker)),
			WEBKIT_DOM_NODE (selection_end_marker),
			WEBKIT_DOM_NODE (selection_start_marker),
			NULL);
		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
	}
	selection_end_marker_parent = webkit_dom_node_get_parent_node (
		WEBKIT_DOM_NODE (selection_end_marker));

	/* Determine before what node we have to insert the smiley */
	insert_before = WEBKIT_DOM_NODE (selection_start_marker);
	prev_sibling = webkit_dom_node_get_previous_sibling (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (prev_sibling) {
		if (webkit_dom_node_is_same_node (
			prev_sibling, WEBKIT_DOM_NODE (selection_end_marker))) {
			insert_before = WEBKIT_DOM_NODE (selection_end_marker);
		} else {
			prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);
			if (prev_sibling &&
			    webkit_dom_node_is_same_node (
				prev_sibling, WEBKIT_DOM_NODE (selection_end_marker))) {
				insert_before = WEBKIT_DOM_NODE (selection_end_marker);
			}
		}
	} else
		insert_before = WEBKIT_DOM_NODE (selection_start_marker);

	/* Look if selection is misplaced - that means that the selection was
	 * restored before the previously inserted smiley in situations when we
	 * are writing more smileys in a row */
	next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker));
	if (next_sibling && WEBKIT_DOM_IS_ELEMENT (next_sibling))
		if (element_has_class (WEBKIT_DOM_ELEMENT (next_sibling), "-x-evo-smiley-wrapper"))
			misplaced_selection = TRUE;

	range = dom_get_current_range (document);
	node = webkit_dom_range_get_end_container (range, NULL);
	g_object_unref (range);
	if (WEBKIT_DOM_IS_TEXT (node))
		node_text = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (node));

	if (misplaced_selection) {
		/* Insert smiley and selection markers after it */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (insert_before),
			WEBKIT_DOM_NODE (selection_start_marker),
			webkit_dom_node_get_next_sibling (next_sibling),
			NULL);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (insert_before),
			WEBKIT_DOM_NODE (selection_end_marker),
			webkit_dom_node_get_next_sibling (next_sibling),
			NULL);
		span = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (insert_before),
				WEBKIT_DOM_NODE (span),
				webkit_dom_node_get_next_sibling (next_sibling),
				NULL));
	} else {
		span = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (insert_before),
				WEBKIT_DOM_NODE (span),
				insert_before,
				NULL));
	}

	/* &#8203 == UNICODE_ZERO_WIDTH_SPACE */
	webkit_dom_html_element_insert_adjacent_html (
		WEBKIT_DOM_HTML_ELEMENT (span), "afterend", "&#8203;", NULL);

	if (ev) {
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMNode *node;

		fragment = webkit_dom_document_create_document_fragment (document);
		node = webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (span), TRUE),
			NULL);
		webkit_dom_html_element_insert_adjacent_html (
			WEBKIT_DOM_HTML_ELEMENT (node), "afterend", "&#8203;", NULL);
		ev->data.fragment = fragment;
	}

	/* Remove the text that represents the text version of smiley that was
	 * written into the composer. */
	if (node_text && smiley_written) {
		emoticon_start = g_utf8_strrchr (
			node_text, -1, g_utf8_get_char (emoticon->text_face));
		/* Check if the written smiley is really the one that we inserted. */
		if (emoticon_start) {
			/* The written smiley is the same as text version. */
			if (g_str_has_prefix (emoticon_start, emoticon->text_face)) {
				webkit_dom_character_data_delete_data (
					WEBKIT_DOM_CHARACTER_DATA (node),
					g_utf8_strlen (node_text, -1) - strlen (emoticon_start),
					strlen (emoticon->text_face),
					NULL);
			} else if (strstr (emoticon->text_face, "-")) {
				gboolean same = TRUE, compensate = FALSE;
				gint ii = 0, jj = 0;

				/* Try to recognize smileys without the dash e.g. :). */
				while (emoticon_start[ii] && emoticon->text_face[jj]) {
					if (emoticon_start[ii] == emoticon->text_face[jj]) {
						if (emoticon->text_face[jj+1] && emoticon->text_face[jj+1] == '-') {
							ii++;
							jj+=2;
							compensate = TRUE;
						} else {
							ii++;
							jj++;
						}
					} else {
						same = FALSE;
						break;
					}
				}

				if (same) {
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (node),
						g_utf8_strlen (node_text, -1) - strlen (emoticon_start),
						ii,
						NULL);
				}
				/* If we recognize smiley without dash, but we inserted
				 * the text version with dash we need it insert new
				 * history input event with that dash. */
				if (compensate)
					e_html_editor_undo_redo_manager_insert_dash_history_event (manager);
			}
		}

		e_html_editor_web_extension_set_is_smiley_written (extension, FALSE);
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

	e_html_editor_web_extension_set_content_changed (extension);

	g_free (node_text);
}

static void
emoticon_read_async_cb (GFile *file,
                        GAsyncResult *result,
                        LoadContext *load_context)
{
	EEmoticon *emoticon = load_context->emoticon;
	EHTMLEditorWebExtension *extension = load_context->extension;
	GError *error = NULL;
	gboolean html_mode;
	gchar *mime_type;
	gchar *base64_encoded, *output, *data;
	GFileInputStream *input_stream;
	GOutputStream *output_stream;
	gssize size;
	WebKitDOMElement *wrapper, *image, *smiley_text;
	WebKitDOMDocument *document;

	input_stream = g_file_read_finish (file, result, &error);
	g_return_if_fail (!error && input_stream);

	output_stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

	size = g_output_stream_splice (
		output_stream, G_INPUT_STREAM (input_stream),
		G_OUTPUT_STREAM_SPLICE_NONE, NULL, &error);

	if (error || (size == -1))
		goto out;

	mime_type = g_content_type_get_mime_type (load_context->content_type);

	data = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output_stream));
	base64_encoded = g_base64_encode ((const guchar *) data, size);
	output = g_strconcat ("data:", mime_type, ";base64,", base64_encoded, NULL);

	html_mode = e_html_editor_web_extension_get_html_mode (extension);
	document = load_context->document;

	/* Insert span with image representation and another one with text
	 * represetation and hide/show them dependant on active composer mode */
	wrapper = webkit_dom_document_create_element (document, "SPAN", NULL);
	if (html_mode)
		webkit_dom_element_set_attribute (
			wrapper, "class", "-x-evo-smiley-wrapper -x-evo-resizable-wrapper", NULL);
	else
		webkit_dom_element_set_attribute (
			wrapper, "class", "-x-evo-smiley-wrapper", NULL);

	image = webkit_dom_document_create_element (document, "IMG", NULL);
	webkit_dom_element_set_attribute (image, "src", output, NULL);
	webkit_dom_element_set_attribute (image, "data-inline", "", NULL);
	webkit_dom_element_set_attribute (image, "data-name", load_context->name, NULL);
	webkit_dom_element_set_attribute (image, "alt", emoticon->text_face, NULL);
	webkit_dom_element_set_attribute (image, "class", "-x-evo-smiley-img", NULL);
	if (!html_mode)
		webkit_dom_element_set_attribute (image, "style", "display: none;", NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (wrapper), WEBKIT_DOM_NODE (image), NULL);

	smiley_text = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_attribute (smiley_text, "class", "-x-evo-smiley-text", NULL);
	if (html_mode)
		webkit_dom_element_set_attribute (smiley_text, "style", "display: none;", NULL);
	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (smiley_text), emoticon->text_face, NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (wrapper), WEBKIT_DOM_NODE (smiley_text), NULL);

	emoticon_insert_span (emoticon, load_context, wrapper);

	g_free (base64_encoded);
	g_free (output);
	g_free (mime_type);
	g_object_unref (output_stream);
 out:
	emoticon_load_context_free (load_context);
}

static void
emoticon_query_info_async_cb (GFile *file,
                              GAsyncResult *result,
                              LoadContext *load_context)
{
	GError *error = NULL;
	GFileInfo *info;

	info = g_file_query_info_finish (file, result, &error);
	g_return_if_fail (!error && info);

	load_context->content_type = g_strdup (g_file_info_get_content_type (info));
	load_context->name = g_strdup (g_file_info_get_name (info));

	g_file_read_async (
		file, G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) emoticon_read_async_cb, load_context);

	g_object_unref (info);
}

void
dom_insert_smiley (WebKitDOMDocument *document,
                   EHTMLEditorWebExtension *extension,
                   EEmoticon *emoticon)
{
	GFile *file;
	gchar *filename_uri;
	LoadContext *load_context;

	if (e_html_editor_web_extension_get_unicode_smileys_enabled (extension)) {
		WebKitDOMElement *wrapper;

		wrapper = webkit_dom_document_create_element (document, "SPAN", NULL);
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (wrapper), emoticon->unicode_character, NULL);

		load_context = emoticon_load_context_new (document, extension, emoticon);
		emoticon_insert_span (emoticon, load_context, wrapper);
		emoticon_load_context_free (load_context);
	} else {
		filename_uri = e_emoticon_get_uri (emoticon);
		g_return_if_fail (filename_uri != NULL);

		load_context = emoticon_load_context_new (document, extension, emoticon);

		file = g_file_new_for_uri (filename_uri);
		g_file_query_info_async (
			file,  "standard::*", G_FILE_QUERY_INFO_NONE,
			G_PRIORITY_DEFAULT, NULL,
			(GAsyncReadyCallback) emoticon_query_info_async_cb, load_context);

		g_free (filename_uri);
		g_object_unref (file);
	}
}

void
dom_insert_smiley_by_name (WebKitDOMDocument *document,
                           EHTMLEditorWebExtension *extension,
                           const gchar *name)
{
	const EEmoticon *emoticon;

	emoticon = e_emoticon_chooser_lookup_emoticon (name);
	e_html_editor_web_extension_set_is_smiley_written (extension, FALSE);
	dom_insert_smiley (document, extension, (EEmoticon *) emoticon);
}

void
dom_check_magic_smileys (WebKitDOMDocument *document,
                         EHTMLEditorWebExtension *extension)
{
	gint pos, state, relative, start;
	gchar *node_text;
	gunichar uc;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	if (!e_html_editor_web_extension_get_magic_smileys_enabled (extension))
		return;

	range = dom_get_current_range (document);
	node = webkit_dom_range_get_end_container (range, NULL);
	if (!WEBKIT_DOM_IS_TEXT (node))
		return;

	node_text = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (node));
	if (node_text == NULL)
		return;

	start = webkit_dom_range_get_end_offset (range, NULL) - 1;
	pos = start;
	state = 0;
	while (pos >= 0) {
		uc = g_utf8_get_char (g_utf8_offset_to_pointer (node_text, pos));
		relative = 0;
		while (emoticons_chars[state + relative]) {
			if (emoticons_chars[state + relative] == uc)
				break;
			relative++;
		}
		state = emoticons_states[state + relative];
		/* 0 .. not found, -n .. found n-th */
		if (state <= 0)
			break;
		pos--;
	}

	/* Special case needed to recognize angel and devilish. */
	if (pos > 0 && state == -14) {
		uc = g_utf8_get_char (g_utf8_offset_to_pointer (node_text, pos - 1));
		if (uc == 'O') {
			state = -1;
			pos--;
		} else if (uc == '>') {
			state = -5;
			pos--;
		}
	}

	if (state < 0) {
		const EEmoticon *emoticon;

		if (pos > 0) {
			uc = g_utf8_get_char (g_utf8_offset_to_pointer (node_text, pos - 1));
			if (!g_unichar_isspace (uc)) {
				g_free (node_text);
				return;
			}
		}

		emoticon = e_emoticon_chooser_lookup_emoticon (
			emoticons_icon_names[-state - 1]);
		e_html_editor_web_extension_set_is_smiley_written (extension, TRUE);
		dom_insert_smiley (document, extension, (EEmoticon *) emoticon);
	}

	g_free (node_text);
}

static void
dom_set_links_active (WebKitDOMDocument *document,
                      gboolean active)
{
	WebKitDOMElement *style;

	if (active) {
		style = webkit_dom_document_get_element_by_id (
			document, "-x-evo-style-a");
		if (style)
			remove_node (WEBKIT_DOM_NODE (style));
	} else {
		WebKitDOMHTMLHeadElement *head;
		head = webkit_dom_document_get_head (document);

		style = webkit_dom_document_create_element (document, "STYLE", NULL);
		webkit_dom_element_set_id (style, "-x-evo-style-a");
		webkit_dom_element_set_attribute (style, "type", "text/css", NULL);
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (style), "a { cursor: text; }", NULL);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (head), WEBKIT_DOM_NODE (style), NULL);
	}
}

static void
fix_paragraph_structure_after_pressing_enter_after_smiley (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_query_selector (
		document, "span.-x-evo-smiley-wrapper > br", NULL);

	if (element) {
		WebKitDOMNode *parent;

		parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
		webkit_dom_element_set_inner_html (
			webkit_dom_node_get_parent_element (parent),
			UNICODE_ZERO_WIDTH_SPACE,
			NULL);
	}
}

static gboolean
fix_paragraph_structure_after_pressing_enter (WebKitDOMDocument *document,
					      EHTMLEditorWebExtension *extension)
{
	gboolean prev_is_heading = FALSE;
	gint ii, length;
	WebKitDOMNodeList *list;

	/* When pressing Enter on empty line in the list (or after heading elements)
	 * WebKit will end thatlist and inserts <div><br></div> so mark it for wrapping. */
	list = webkit_dom_document_query_selector_all (
		document, "body > div:not(.-x-evo-paragraph) > br", NULL);

	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *prev_sibling;
		WebKitDOMNode *node = webkit_dom_node_get_parent_node (
			webkit_dom_node_list_item (list, ii));

		prev_sibling = webkit_dom_node_get_previous_sibling (node);
		if (prev_sibling && WEBKIT_DOM_IS_HTML_HEADING_ELEMENT (prev_sibling))
			prev_is_heading = TRUE;
		dom_set_paragraph_style (document, extension, WEBKIT_DOM_ELEMENT (node), -1, 0, "");
		g_object_unref (node);
	}
	g_object_unref (list);

	return prev_is_heading;
}

static gboolean
surround_text_with_paragraph_if_needed (WebKitDOMDocument *document,
                                        EHTMLEditorWebExtension *extension,
                                        WebKitDOMNode *node)
{
	WebKitDOMNode *next_sibling = webkit_dom_node_get_next_sibling (node);
	WebKitDOMNode *prev_sibling = webkit_dom_node_get_previous_sibling (node);
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);
	WebKitDOMElement *element;

	/* All text in composer has to be written in div elements, so if
	 * we are writing something straight to the body, surround it with
	 * paragraph */
	if (WEBKIT_DOM_IS_TEXT (node) &&
	    (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent) ||
	     WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (parent))) {
		element = dom_put_node_into_paragraph (document, extension, node, TRUE);
		if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (parent))
			webkit_dom_element_remove_attribute (element, "style");

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling))
			remove_node (next_sibling);

		/* Tab character */
		if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "Apple-tab-span")) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (element),
				prev_sibling,
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (element)),
				NULL);
		}

		return TRUE;
	}

	return FALSE;
}

static void
body_keydown_event_cb (WebKitDOMElement *element,
                       WebKitDOMUIEvent *event,
                       EHTMLEditorWebExtension *extension)
{
	glong key_code;

	key_code = webkit_dom_ui_event_get_key_code (event);
	if (key_code == HTML_KEY_CODE_CONTROL)
		dom_set_links_active (
			webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element)), TRUE);
	else if (key_code == HTML_KEY_CODE_DELETE ||
		 key_code == HTML_KEY_CODE_BACKSPACE)
		e_html_editor_web_extension_set_dont_save_history_in_body_input (extension, TRUE);
}

static gboolean
save_history_before_event_in_table (WebKitDOMDocument *document,
                                    EHTMLEditorWebExtension *extension,
                                    WebKitDOMRange *range)
{
	WebKitDOMNode *node;
	WebKitDOMElement *block;

	node = webkit_dom_range_get_start_container (range, NULL);
	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node))
		block = WEBKIT_DOM_ELEMENT (node);
	else
		block = get_parent_block_element (node);

	if (block && WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (block)) {
		EHTMLEditorUndoRedoManager *manager;
		EHTMLEditorHistoryEvent *ev;

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_TABLE_INPUT;

		if (block) {
			dom_selection_save (document);
			ev->data.dom.from = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (block), TRUE);
			dom_selection_restore (document);
		} else
			ev->data.dom.from = NULL;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);

		return TRUE;
	}

	return FALSE;
}

static void
body_keypress_event_cb (WebKitDOMElement *element,
                        WebKitDOMUIEvent *event,
                        EHTMLEditorWebExtension *extension)
{
	glong key_code;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	e_html_editor_web_extension_set_return_key_pressed (extension, FALSE);
	e_html_editor_web_extension_set_space_key_pressed (extension, FALSE);

	key_code = webkit_dom_ui_event_get_key_code (event);
	if (key_code == HTML_KEY_CODE_RETURN)
		e_html_editor_web_extension_set_return_key_pressed (extension, TRUE);
	else if (key_code == HTML_KEY_CODE_SPACE)
		e_html_editor_web_extension_set_space_key_pressed (extension, TRUE);

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	if (save_history_before_event_in_table (document, extension, range)) {
		g_object_unref (range);
		g_object_unref (dom_selection);
		return;
	}

	if (!webkit_dom_range_get_collapsed (range, NULL))
		insert_delete_event (document, extension, range);

	if (e_html_editor_web_extension_get_return_key_pressed (extension)) {
		EHTMLEditorHistoryEvent *ev;
		EHTMLEditorUndoRedoManager *manager;

		/* Insert new history event for Return to have the right coordinates.
		 * The fragment will be added later. */
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_INPUT;

		manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	g_object_unref (range);
	g_object_unref (dom_selection);
}

static gboolean
save_history_after_event_in_table (WebKitDOMDocument *document,
                                   EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev;
	EHTMLEditorUndoRedoManager *manager;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection)) {
		g_object_unref (dom_selection);
		return FALSE;
	}
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	/* Find if writing into table. */
	node = webkit_dom_range_get_start_container (range, NULL);
	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = get_parent_block_element (node);

	g_object_unref (dom_selection);
	g_object_unref (range);

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	/* If writing to table we have to create different history event. */
	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (element)) {
		ev = e_html_editor_undo_redo_manager_get_current_history_event (manager);
		if (ev->type != HISTORY_TABLE_INPUT)
			return FALSE;
	} else
		return FALSE;

	dom_selection_save (document);

	dom_selection_get_coordinates (
		document,
		&ev->after.start.x,
		&ev->after.start.y,
		&ev->after.end.x,
		&ev->after.end.y);

	ev->data.dom.to = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (element), TRUE);

	dom_selection_restore (document);

	return TRUE;
}

static void
save_history_for_input (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev;
	EHTMLEditorUndoRedoManager *manager;
	glong offset;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range, *range_clone;
	WebKitDOMNode *start_container;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection)) {
		g_object_unref (dom_selection);
		return;
	}

	if (e_html_editor_web_extension_get_return_key_pressed (extension)) {
		ev = e_html_editor_undo_redo_manager_get_current_history_event (manager);
		if (ev->type != HISTORY_INPUT) {
			g_object_unref (dom_selection);
			return;
		}
	} else {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_INPUT;
	}

	e_html_editor_web_extension_block_selection_changed_callback (extension);

	dom_selection_get_coordinates (
		document,
		&ev->after.start.x,
		&ev->after.start.y,
		&ev->after.end.x,
		&ev->after.end.y);

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	range_clone = webkit_dom_range_clone_range (range, NULL);
	offset = webkit_dom_range_get_start_offset (range_clone, NULL);
	start_container = webkit_dom_range_get_start_container (range_clone, NULL);
	if (offset > 0)
		webkit_dom_range_set_start (
			range_clone,
			start_container,
			offset - 1,
			NULL);
	fragment = webkit_dom_range_clone_contents (range_clone, NULL);
	/* We have to specialy handle Return key press */
	if (e_html_editor_web_extension_get_return_key_pressed (extension)) {
		WebKitDOMElement *element_start, *element_end;
		WebKitDOMNode *parent_start, *parent_end, *node;

		element_start = webkit_dom_document_create_element (document, "span", NULL);
		webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (element_start), NULL);
		webkit_dom_dom_selection_modify (dom_selection, "move", "left", "character");
		g_object_unref (range);
		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		element_end = webkit_dom_document_create_element (document, "span", NULL);
		webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (element_end), NULL);

		parent_start = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element_start));
		parent_end = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element_end));

		while (parent_start && parent_end && !webkit_dom_node_is_same_node (parent_start, parent_end)) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (fragment),
				webkit_dom_node_clone_node (parent_start, FALSE),
				webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
				NULL);
			parent_start = webkit_dom_node_get_parent_node (parent_start);
			parent_end = webkit_dom_node_get_parent_node (parent_end);
		}

		node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
		while (webkit_dom_node_get_next_sibling (node)) {
			WebKitDOMNode *last_child;

			last_child = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (fragment));
			webkit_dom_node_append_child (
				webkit_dom_node_get_previous_sibling (last_child),
				last_child,
				NULL);
		}

		node = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (fragment));
		while (webkit_dom_node_get_last_child (node)) {
			node = webkit_dom_node_get_last_child (node);
		}

		webkit_dom_node_append_child (
			node,
			WEBKIT_DOM_NODE (
				webkit_dom_document_create_element (document, "br", NULL)),
			NULL);
		webkit_dom_node_append_child (
			node,
			WEBKIT_DOM_NODE (
				dom_create_selection_marker (document, TRUE)),
			NULL);
		webkit_dom_node_append_child (
			node,
			WEBKIT_DOM_NODE (
				dom_create_selection_marker (document, FALSE)),
			NULL);

		remove_node (WEBKIT_DOM_NODE (element_start));
		remove_node (WEBKIT_DOM_NODE (element_end));

		g_object_set_data (
			G_OBJECT (fragment), "-x-evo-return-key", GINT_TO_POINTER (1));


		webkit_dom_dom_selection_modify (dom_selection, "move", "right", "character");
	} else {
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			WEBKIT_DOM_NODE (
				dom_create_selection_marker (document, TRUE)),
			NULL);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			WEBKIT_DOM_NODE (
				dom_create_selection_marker (document, FALSE)),
			NULL);
	}

	g_object_unref (dom_selection);
	g_object_unref (range);
	g_object_unref (range_clone);

	e_html_editor_web_extension_unblock_selection_changed_callback (extension);

	ev->data.fragment = fragment;

	if (!e_html_editor_web_extension_get_return_key_pressed (extension))
		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
}

typedef struct _TimeoutContext TimeoutContext;

struct _TimeoutContext {
	EHTMLEditorWebExtension *extension;
	WebKitDOMDocument *document;
};

static void
timeout_context_free (TimeoutContext *context)
{
	g_slice_free (TimeoutContext, context);
}

static gboolean
force_spell_check_on_timeout (TimeoutContext *context)
{
	dom_force_spell_check_in_viewport (context->document, context->extension);
	e_html_editor_web_extension_set_spell_check_on_scroll_event_source_id (context->extension, 0);
	return FALSE;
}

static void
body_scroll_event_cb (WebKitDOMElement *element,
                      WebKitDOMEvent *event,
                      EHTMLEditorWebExtension *extension)
{
	TimeoutContext *context;
	guint id;
	WebKitDOMDocument *document;

	if (!e_html_editor_web_extension_get_inline_spelling_enabled (extension))
		return;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));

	context = g_slice_new0 (TimeoutContext);
	context->extension = extension;
	context->document = document;

	id = e_html_editor_web_extension_get_spell_check_on_scroll_event_source_id (extension);
	if (id > 0)
		g_source_remove (id);

	id = g_timeout_add_seconds_full (
		1,
		G_PRIORITY_DEFAULT,
		(GSourceFunc)force_spell_check_on_timeout,
		context,
		(GDestroyNotify)timeout_context_free);

	e_html_editor_web_extension_set_spell_check_on_scroll_event_source_id (extension, id);
}

void
body_input_event_process (WebKitDOMDocument *document,
			  EHTMLEditorWebExtension *extension,
			  WebKitDOMEvent *event)
{
	EHTMLEditorUndoRedoManager *manager;
	gboolean do_spell_check = FALSE;
	gboolean html_mode;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	range = dom_get_current_range (document);

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

	html_mode = e_html_editor_web_extension_get_html_mode (extension);
	e_html_editor_web_extension_set_content_changed (extension);

	if (e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		e_html_editor_undo_redo_manager_set_operation_in_progress (manager, FALSE);
		e_html_editor_web_extension_set_dont_save_history_in_body_input (extension, FALSE);
		do_spell_check = TRUE;
		goto out;
	}

	if (!save_history_after_event_in_table (document, extension)) {
		if (!e_html_editor_web_extension_get_dont_save_history_in_body_input (extension))
			save_history_for_input (document, extension);
		else
			do_spell_check = TRUE;
	}

	/* Don't try to look for smileys if we are deleting text. */
	if (!e_html_editor_web_extension_get_dont_save_history_in_body_input (extension))
		dom_check_magic_smileys (document, extension);

	e_html_editor_web_extension_set_dont_save_history_in_body_input (extension, FALSE);

	if (e_html_editor_web_extension_get_return_key_pressed (extension) ||
	    e_html_editor_web_extension_get_space_key_pressed (extension)) {
		dom_check_magic_links (document, extension, FALSE);
		if (e_html_editor_web_extension_get_return_key_pressed (extension)) {
			if (fix_paragraph_structure_after_pressing_enter (document, extension) &&
			    html_mode) {
				/* When the return is pressed in a H1-6 element, WebKit doesn't
				 * continue with the same element, but creates normal paragraph,
				 * so we have to unset the bold font. */
				e_html_editor_undo_redo_manager_set_operation_in_progress (manager, TRUE);
				dom_selection_set_bold (document, extension, FALSE);
				e_html_editor_undo_redo_manager_set_operation_in_progress (manager, FALSE);
			}

			fix_paragraph_structure_after_pressing_enter_after_smiley (document);

			do_spell_check = TRUE;
		}
	} else {
		WebKitDOMNode *node;

		node = webkit_dom_range_get_end_container (range, NULL);

		if (surround_text_with_paragraph_if_needed (document, extension, node)) {
			WebKitDOMElement *element;

			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
			dom_selection_restore (document);
		}

		if (WEBKIT_DOM_IS_TEXT (node)) {
			gchar *text;

			text = webkit_dom_node_get_text_content (node);

			if (text && *text && !strstr (text, " ") &&
			    !strchr (URL_INVALID_TRAILING_CHARS, *text)) {
				WebKitDOMNode *prev_sibling;

				prev_sibling = webkit_dom_node_get_previous_sibling (node);

				if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling))
					dom_check_magic_links (document, extension, FALSE);
			}
			g_free (text);
		}
	}

	node = webkit_dom_range_get_end_container (range, NULL);

	/* After toggling monospaced format, we are using UNICODE_ZERO_WIDTH_SPACE
	 * to move caret into right space. When this callback is called it is not
	 * necessary anymore so remove it */
	if (html_mode) {
		WebKitDOMElement *parent = webkit_dom_node_get_parent_element (node);

		if (parent) {
			WebKitDOMNode *prev_sibling;

			prev_sibling = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (parent));

			if (prev_sibling && WEBKIT_DOM_IS_TEXT (prev_sibling)) {
				gchar *text = webkit_dom_node_get_text_content (
					prev_sibling);

				if (g_strcmp0 (text, UNICODE_ZERO_WIDTH_SPACE) == 0)
					remove_node (prev_sibling);

				g_free (text);
			}

		}
	}

	/* If text before caret includes UNICODE_ZERO_WIDTH_SPACE character, remove it */
	if (WEBKIT_DOM_IS_TEXT (node)) {
		gchar *text = webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (node));
		glong length = webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node));
		WebKitDOMNode *parent;

		/* We have to preserve empty paragraphs with just UNICODE_ZERO_WIDTH_SPACE
		 * character as when we will remove it it will collapse */
		if (length > 1) {
			if (g_str_has_prefix (text, UNICODE_ZERO_WIDTH_SPACE))
				webkit_dom_character_data_replace_data (
					WEBKIT_DOM_CHARACTER_DATA (node), 0, 1, "", NULL);
			else if (g_str_has_suffix (text, UNICODE_ZERO_WIDTH_SPACE))
				webkit_dom_character_data_replace_data (
					WEBKIT_DOM_CHARACTER_DATA (node), length - 1, 1, "", NULL);
		}
		g_free (text);

		parent = webkit_dom_node_get_parent_node (node);
		if ((WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT (parent) ||
		    WEBKIT_DOM_IS_HTML_DIV_ELEMENT (parent)) &&
		    !element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-paragraph")) {
			if (html_mode)
				element_add_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-paragraph");
			else
				dom_set_paragraph_style (
					document, extension, WEBKIT_DOM_ELEMENT (parent), -1, 0, "");
		}

		/* When new smiley is added we have to use UNICODE_HIDDEN_SPACE to set the
		 * caret position to right place. It is removed when user starts typing. But
		 * when the user will press left arrow he will move the caret into
		 * smiley wrapper. If he will start to write there we have to move the written
		 * text out of the wrapper and move caret to right place */
		if (WEBKIT_DOM_IS_ELEMENT (parent) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-smiley-text")) {
			gchar *text;
			WebKitDOMCharacterData *data;
			WebKitDOMText *text_node;

			/* Split out the newly written character to its own text node, */
			data = WEBKIT_DOM_CHARACTER_DATA (node);
			parent = webkit_dom_node_get_parent_node (parent);
			text = webkit_dom_character_data_substring_data (
				data,
				webkit_dom_character_data_get_length (data) - 1,
				1,
				NULL);
			webkit_dom_character_data_delete_data (
				data,
				webkit_dom_character_data_get_length (data) - 1,
				1,
				NULL);
			text_node = webkit_dom_document_create_text_node (document, text);
			g_free (text);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (
					dom_create_selection_marker (document, FALSE)),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (
					dom_create_selection_marker (document, TRUE)),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
			/* Move the text node outside of smiley. */
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (text_node),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
			dom_selection_restore (document);
		}
	}

	/* Writing into quoted content */
	if (html_mode) {
		gint citation_level;
		WebKitDOMElement *selection_start_marker, *selection_end_marker;
		WebKitDOMNode *node, *parent;

		node = webkit_dom_range_get_end_container (range, NULL);

		citation_level = get_citation_level (node, FALSE);
		if (citation_level == 0)
			goto out;

		selection_start_marker = webkit_dom_document_query_selector (
			document, "span#-x-evo-selection-start-marker", NULL);
		if (selection_start_marker)
			goto out;

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

		/* We have to process elements only inside normal block */
		parent = WEBKIT_DOM_NODE (get_parent_block_element (
			WEBKIT_DOM_NODE (selection_start_marker)));
		if (WEBKIT_DOM_IS_HTML_PRE_ELEMENT (parent)) {
			dom_selection_restore (document);
			goto out;
		}

		if (selection_start_marker) {
			gchar *content;
			gint text_length, word_wrap_length, length;
			WebKitDOMElement *block;
			gboolean remove_quoting = FALSE;

			word_wrap_length =
				e_html_editor_web_extension_get_word_wrap_length (extension);
			length = word_wrap_length - 2 * citation_level;

			block = WEBKIT_DOM_ELEMENT (parent);
			if (webkit_dom_element_query_selector (
				WEBKIT_DOM_ELEMENT (block), ".-x-evo-quoted", NULL)) {
				WebKitDOMNode *prev_sibling;

				prev_sibling = webkit_dom_node_get_previous_sibling (
					WEBKIT_DOM_NODE (selection_end_marker));

				if (WEBKIT_DOM_IS_ELEMENT (prev_sibling))
					remove_quoting = element_has_class (
						WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-quoted");
			}

			content = webkit_dom_node_get_text_content (WEBKIT_DOM_NODE (block));
			text_length = g_utf8_strlen (content, -1);
			g_free (content);

			/* Wrap and quote the line */
			if (!remove_quoting && text_length >= word_wrap_length) {
				dom_remove_quoting_from_element (block);

				block = dom_wrap_paragraph_length (document, extension, block, length);
				webkit_dom_node_normalize (WEBKIT_DOM_NODE (block));
				dom_quote_plain_text_element_after_wrapping (
					document, WEBKIT_DOM_ELEMENT (block), citation_level);
				selection_start_marker = webkit_dom_document_query_selector (
					document, "span#-x-evo-selection-start-marker", NULL);
				if (!selection_start_marker)
					dom_add_selection_markers_into_element_end (
						document,
						WEBKIT_DOM_ELEMENT (block),
						NULL,
						NULL);

				dom_selection_restore (document);
				do_spell_check = TRUE;
				goto out;
			}
		}
		dom_selection_restore (document);
	}
 out:
	if (do_spell_check)
		dom_force_spell_check_for_current_paragraph (document, extension);

	g_object_unref (range);
}

static void
body_input_event_cb (WebKitDOMElement *element,
                     WebKitDOMEvent *event,
                     EHTMLEditorWebExtension *extension)
{
	WebKitDOMDocument *document;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));

	body_input_event_process (document, extension, event);
}

void
dom_remove_input_event_listener_from_body (WebKitDOMDocument *document,
                                           EHTMLEditorWebExtension *extension)
{
	if (!e_html_editor_web_extension_get_body_input_event_removed (extension)) {
		webkit_dom_event_target_remove_event_listener (
			WEBKIT_DOM_EVENT_TARGET (
				webkit_dom_document_get_body (document)),
			"input",
			G_CALLBACK (body_input_event_cb),
			FALSE);

		e_html_editor_web_extension_set_body_input_event_removed (extension, TRUE);
	}
}

void
dom_register_input_event_listener_on_body (WebKitDOMDocument *document,
                                           EHTMLEditorWebExtension *extension)
{
	if (e_html_editor_web_extension_get_body_input_event_removed (extension)) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (
				webkit_dom_document_get_body (document)),
			"input",
			G_CALLBACK (body_input_event_cb),
			FALSE,
			extension);

		e_html_editor_web_extension_set_body_input_event_removed (extension, FALSE);
	}
}

static void
remove_empty_blocks (WebKitDOMDocument *document)
{
	gint ii, length;
	WebKitDOMNodeList *list;

	list = webkit_dom_document_query_selector_all (
	document, "blockquote[type=cite] > :empty", NULL);

	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		remove_node (node);
		g_object_unref (node);
	}

	list = webkit_dom_document_query_selector_all (
		document, "blockquote[type=cite]:empty", NULL);

	length = webkit_dom_node_list_get_length (list);
	for  (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		remove_node (node);
		g_object_unref (node);
	}

	g_object_unref (list);
}

/* Following two functions are used when deleting the selection inside
 * the quoted content. The thing is that normally the quote marks are not
 * selectable by user. But this caused a lof of problems for WebKit when removing
 * the selection. This will avoid it as when the delete or backspace key is pressed
 * we will make the quote marks user selectable so they will act as any other text.
 * On HTML keyup event callback we will make them again non-selectable. */
void
dom_disable_quote_marks_select (WebKitDOMDocument *document)
{
	WebKitDOMHTMLHeadElement *head;
	WebKitDOMElement *style_element;

	head = webkit_dom_document_get_head (document);

	if (!webkit_dom_document_get_element_by_id (document, "-x-evo-quote-style")) {
		style_element = webkit_dom_document_create_element (document, "style", NULL);
		webkit_dom_element_set_id (style_element, "-x-evo-quote-style");
		webkit_dom_element_set_attribute (style_element, "type", "text/css", NULL);
		webkit_dom_element_set_inner_html (
			style_element,
			".-x-evo-quoted { -webkit-user-select: none; }",
			NULL);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (head), WEBKIT_DOM_NODE (style_element), NULL);
	}
}

static void
enable_quote_marks_select (WebKitDOMDocument *document)
{
	WebKitDOMElement *style_element;

	if ((style_element = webkit_dom_document_get_element_by_id (document, "-x-evo-quote-style")))
		remove_node (WEBKIT_DOM_NODE (style_element));
}

void
dom_remove_node_and_parents_if_empty (WebKitDOMNode *node)
{
	WebKitDOMNode *parent;

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (node));

	remove_node (WEBKIT_DOM_NODE (node));

	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		WebKitDOMNode *prev_sibling, *next_sibling;

		prev_sibling = webkit_dom_node_get_previous_sibling (parent);
		next_sibling = webkit_dom_node_get_next_sibling (parent);
		/* Empty or BR as sibling, but no sibling after it. */
		if ((!prev_sibling ||
		     (WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling) &&
		      !webkit_dom_node_get_previous_sibling (prev_sibling))) &&
		    (!next_sibling ||
		     (WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling) &&
		      !webkit_dom_node_get_next_sibling (next_sibling)))) {
			WebKitDOMNode *tmp;

			tmp = webkit_dom_node_get_parent_node (parent);
			remove_node (parent);
			parent = tmp;
		} else {
			if (!webkit_dom_node_get_first_child (parent))
				remove_node (parent);
			return;
		}
	}
}

void
dom_merge_siblings_if_necessarry (WebKitDOMDocument *document,
				  WebKitDOMDocumentFragment *deleted_content)
{
	gboolean equal_nodes;
	WebKitDOMElement *element, *prev_element;
	WebKitDOMNode *child;

	element = webkit_dom_document_query_selector (document, "blockquote + blockquote", NULL);
	if (!element)
		goto signature;
 repeat:
	prev_element = WEBKIT_DOM_ELEMENT (webkit_dom_node_get_previous_sibling (
		WEBKIT_DOM_NODE (element)));
	equal_nodes = webkit_dom_node_is_equal_node (
		webkit_dom_node_clone_node (WEBKIT_DOM_NODE (element), FALSE),
		webkit_dom_node_clone_node (WEBKIT_DOM_NODE (prev_element), FALSE));

	if (equal_nodes) {
		if (webkit_dom_element_get_child_element_count (element) >
		    webkit_dom_element_get_child_element_count (prev_element)) {
			while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element))))
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (prev_element), child, NULL);
			remove_node (WEBKIT_DOM_NODE (element));
		} else {
			while ((child = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (prev_element))))
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (element),
					child,
					webkit_dom_node_get_first_child (
						WEBKIT_DOM_NODE (element)),
					NULL);
			remove_node (WEBKIT_DOM_NODE (prev_element));
		}
	}

	element = webkit_dom_document_query_selector (document, "blockquote + blockquote", NULL);
	if (element)
		goto repeat;

 signature:
	if (!deleted_content)
		return;

	/* Replace the corrupted signatures with the right one. */
	element = webkit_dom_document_query_selector (
		document, ".-x-evo-signature-wrapper + .-x-evo-signature-wrapper", NULL);
	if (element) {
		WebKitDOMElement *right_signature;

		right_signature = webkit_dom_document_fragment_query_selector (
			deleted_content, ".-x-evo-signature-wrapper", NULL);
		remove_node (webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element)));
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (right_signature), TRUE),
			WEBKIT_DOM_NODE (element),
			NULL);
	}
}

/* This will fix the structure after the situations where some text
 * inside the quoted content is selected and afterwards deleted with
 * BackSpace or Delete. */
void
body_key_up_event_process_backspace_or_delete (WebKitDOMDocument *document,
					       EHTMLEditorWebExtension *extension,
                                               gboolean delete)
{
	gint level;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMElement *tmp_element;
	WebKitDOMNode *parent, *node;

	if (e_html_editor_web_extension_get_html_mode (extension))
		return;

	dom_disable_quote_marks_select (document);
	/* Remove empty blocks if presented. */
	remove_empty_blocks (document);

	dom_selection_save (document);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	/* If we deleted a selection the caret will be inside the quote marks, fix it. */
	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-quote-character")) {
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (
				webkit_dom_node_get_parent_node (parent)),
			WEBKIT_DOM_NODE (selection_end_marker),
			webkit_dom_node_get_next_sibling (
				webkit_dom_node_get_parent_node (parent)),
			NULL);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (
				webkit_dom_node_get_parent_node (parent)),
			WEBKIT_DOM_NODE (selection_start_marker),
			webkit_dom_node_get_next_sibling (
				webkit_dom_node_get_parent_node (parent)),
			NULL);
	}

	/* Under some circumstances we will end with block inside the citation
	 * that has the quote marks removed and we have to reinsert them back. */
	level = get_citation_level (WEBKIT_DOM_NODE (selection_start_marker), FALSE);
	node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker));
	if (level > 0 && node && !WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
		WebKitDOMElement *block;

		block = WEBKIT_DOM_ELEMENT (get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker)));

		dom_remove_quoting_from_element (block);
		if (element_has_class (block, "-x-evo-paragraph")) {
			gint length, word_wrap_length;

			word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);
			length =  word_wrap_length - 2 * level;
			block = dom_wrap_paragraph_length (
				document, extension, block, length);
			webkit_dom_node_normalize (WEBKIT_DOM_NODE (block));
		}
		dom_quote_plain_text_element_after_wrapping (
			document, block, level);
	} else if (level > 0 && !node) {
		WebKitDOMNode *prev_sibling;

		prev_sibling = webkit_dom_node_get_previous_sibling (
			WEBKIT_DOM_NODE (selection_start_marker));
		if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-quoted") &&
		    !webkit_dom_node_get_previous_sibling (prev_sibling))
			webkit_dom_node_append_child (
				parent,
				WEBKIT_DOM_NODE (webkit_dom_document_create_element (document, "br", NULL)),
				NULL);
	}

	/* Situation where the start of the selection was in the beginning
	 * of the block in quoted content and the end in the beginning of
	 * content that is after the citation or the selection end was in
	 * the end of the quoted content (showed by ^). The correct structure
	 * in these cases is to have empty block after the citation.
	 *
	 * > |xxx
	 * > xxx^
	 * |xxx
	 * */
	tmp_element = webkit_dom_document_get_element_by_id (document, "-x-evo-tmp-block");
	if (tmp_element) {
		dom_remove_wrapping_from_element (tmp_element);
		dom_remove_quoting_from_element (tmp_element);

		/* Append the BR element if the block is empty, but the
		 * selection is there to be able to move to the block
		 * with caret later. */
		if (!webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker)) &&
		    !webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker)))
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (tmp_element),
				WEBKIT_DOM_NODE (webkit_dom_document_create_element (
					document, "br", NULL)),
				NULL);

		webkit_dom_element_remove_attribute (tmp_element, "id");

		parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (tmp_element));
		while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (parent)))
			parent = webkit_dom_node_get_parent_node (parent);

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			WEBKIT_DOM_NODE (tmp_element),
			webkit_dom_node_get_next_sibling (parent),
			NULL);
	}

	dom_merge_siblings_if_necessarry (document, NULL);

	dom_selection_restore (document);
	dom_force_spell_check_for_current_paragraph (document, extension);
}

void
body_key_up_event_process_return_key (WebKitDOMDocument *document,
				      EHTMLEditorWebExtension *extension)
{
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *parent;

	/* If the return is pressed in an unordered list in plain text mode
	 * the caret is moved to the "*" character before the newly inserted
	 * item. It looks like it is not enough that the item has BR element
	 * inside, but we have to again use the zero width space character
	 * to fix the situation. */
	if (e_html_editor_web_extension_get_html_mode (extension))
		return;

	/* FIXME WK2 this is called twice */
	/* dom_selection_save (document); */

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	if (!WEBKIT_DOM_IS_HTML_LI_ELEMENT (parent) ||
	    !WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (webkit_dom_node_get_parent_node (parent))) {
		dom_selection_restore (document);
		return;
	}

	if (!webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker)) &&
	    (!webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker)) ||
	     WEBKIT_DOM_IS_HTML_BR_ELEMENT (webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker)))))
		webkit_dom_html_element_insert_adjacent_text (
			WEBKIT_DOM_HTML_ELEMENT (parent),
			"afterbegin",
			UNICODE_ZERO_WIDTH_SPACE,
			NULL);

	dom_selection_restore (document);
}

static void
body_keyup_event_cb (WebKitDOMElement *element,
                     WebKitDOMUIEvent *event,
                     EHTMLEditorWebExtension *extension)
{
	WebKitDOMDocument *document;
	glong key_code;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));
	if (!e_html_editor_web_extension_is_composition_in_progress (extension))
		dom_register_input_event_listener_on_body (document, extension);

	if (!dom_selection_is_collapsed (document))
		return;

	key_code = webkit_dom_ui_event_get_key_code (event);
	if (key_code == HTML_KEY_CODE_BACKSPACE || key_code == HTML_KEY_CODE_DELETE) {
		body_key_up_event_process_backspace_or_delete (document, extension, key_code == HTML_KEY_CODE_DELETE);

		/* The content was wrapped and the coordinates
		 * of caret could be changed, so renew them. But
		 * only do that when we are not redoing a history
		 * event, otherwise it would modify the history. */
		if (e_html_editor_web_extension_get_renew_history_after_coordinates (extension)) {
			EHTMLEditorHistoryEvent *ev = NULL;
			EHTMLEditorUndoRedoManager *manager;

			manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
			ev = e_html_editor_undo_redo_manager_get_current_history_event (manager);
			dom_selection_get_coordinates (
				document,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);
		}
	} else if (key_code == HTML_KEY_CODE_CONTROL)
		dom_set_links_active (document, FALSE);
	else if (key_code == HTML_KEY_CODE_RETURN)
		body_key_up_event_process_return_key (document, extension);
}

static void
fix_structure_after_pasting_multiline_content (WebKitDOMNode *node)
{
	WebKitDOMNode *first_child, *parent;

	/* When pasting content that does not contain just the
	 * one line text WebKit inserts all the content after the
	 * first line into one element. So we have to take it out
	 * of this element and insert it after that element. */
	parent = webkit_dom_node_get_parent_node (node);
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent))
		return;
	first_child = webkit_dom_node_get_first_child (parent);
	while (first_child) {
		WebKitDOMNode *next_child =
			webkit_dom_node_get_next_sibling  (first_child);
		if (webkit_dom_node_has_child_nodes (first_child))
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				first_child,
				parent,
				NULL);
		first_child = next_child;
	}
}

static gboolean
delete_hidden_space (WebKitDOMDocument *document,
		     EHTMLEditorWebExtension *extension)
{
	gint citation_level;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *block;

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return FALSE;

	block = WEBKIT_DOM_ELEMENT (get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker)));

	citation_level = get_citation_level (
		WEBKIT_DOM_NODE (selection_start_marker), FALSE);

	if (selection_start_marker && citation_level > 0) {
		EHTMLEditorUndoRedoManager *manager;
		EHTMLEditorHistoryEvent *ev = NULL;
		WebKitDOMNode *node;
		WebKitDOMDocumentFragment *fragment;

		manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

		node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker));
		if (!(WEBKIT_DOM_IS_ELEMENT (node) &&
		      element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-quoted")))
			return FALSE;

		node = webkit_dom_node_get_previous_sibling (node);
		if (!(WEBKIT_DOM_IS_ELEMENT (node) &&
		      element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br")))
			return FALSE;

		node = webkit_dom_node_get_previous_sibling (node);
		if (!(WEBKIT_DOM_IS_ELEMENT (node) &&
		      webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (node), "data-hidden-space")))
			return FALSE;

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_DELETE;

		dom_selection_get_coordinates (
			document, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);

		remove_node (node);

		wrap_and_quote_element (document, extension, block);

		fragment = webkit_dom_document_create_document_fragment (document);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			WEBKIT_DOM_NODE (
				webkit_dom_document_create_text_node (document, " ")),
			NULL);
		ev->data.fragment = fragment;

		dom_selection_get_coordinates (
			document, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);

		return TRUE;
	}

	return FALSE;
}

gboolean
dom_change_quoted_block_to_normal (WebKitDOMDocument *document,
                                   EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	gboolean html_mode;
	gint citation_level, success = FALSE;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;
	EHTMLEditorUndoRedoManager *manager;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	html_mode = e_html_editor_web_extension_get_html_mode (extension);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return FALSE;

	block = get_parent_block_node_from_child (WEBKIT_DOM_NODE (selection_start_marker));

	citation_level = get_citation_level (
		WEBKIT_DOM_NODE (selection_start_marker), FALSE);

	if (selection_start_marker && citation_level > 0) {
		if (webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), ".-x-evo-quoted", NULL)) {

			WebKitDOMNode *prev_sibling;

			webkit_dom_node_normalize (block);

			prev_sibling = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (selection_start_marker));

			if (!prev_sibling) {
				WebKitDOMNode *parent;

				parent = webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_start_marker));
				if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent))
					prev_sibling = webkit_dom_node_get_previous_sibling (parent);
			}

			if (WEBKIT_DOM_IS_ELEMENT (prev_sibling))
				success = element_has_class (
					WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-quoted");

			/* We really have to be in the beginning of paragraph and
			 * not on the beginning of some line in the paragraph */
			if (success && webkit_dom_node_get_previous_sibling (prev_sibling))
				success = FALSE;
		}

		if (html_mode)
			success = WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (
				webkit_dom_node_get_parent_element (block));
	}

	if (!success)
		return FALSE;

	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_UNQUOTE;

		dom_selection_get_coordinates (document, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		ev->data.dom.from = webkit_dom_node_clone_node (block, TRUE);
	}

	if (citation_level == 1) {
		gchar *inner_html;
		WebKitDOMElement *paragraph, *element;

		inner_html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (block));
		webkit_dom_element_set_id (WEBKIT_DOM_ELEMENT (block), "-x-evo-to-remove");

		paragraph = dom_insert_new_line_into_citation (document, extension, inner_html);
		g_free (inner_html);

		if (paragraph) {
			if (html_mode) {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (paragraph),
					WEBKIT_DOM_NODE (selection_start_marker),
					webkit_dom_node_get_first_child (
						WEBKIT_DOM_NODE (paragraph)),
					NULL);
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (paragraph),
					WEBKIT_DOM_NODE (selection_end_marker),
					webkit_dom_node_get_first_child (
						WEBKIT_DOM_NODE (paragraph)),
					NULL);

			}

			dom_remove_quoting_from_element (paragraph);
			dom_remove_wrapping_from_element (paragraph);
		}

		if (block)
			remove_node (block);

		while ((element = webkit_dom_document_get_element_by_id (document, "-x-evo-to-remove")))
			remove_node (WEBKIT_DOM_NODE (element));

		if (paragraph)
			remove_node_if_empty (
				webkit_dom_node_get_next_sibling (
					WEBKIT_DOM_NODE (paragraph)));
	}

	if (citation_level > 1) {
		WebKitDOMNode *parent;

		if (html_mode) {
			webkit_dom_node_insert_before (
				block,
				WEBKIT_DOM_NODE (selection_start_marker),
				webkit_dom_node_get_first_child (block),
				NULL);
			webkit_dom_node_insert_before (
				block,
				WEBKIT_DOM_NODE (selection_end_marker),
				webkit_dom_node_get_first_child (block),
				NULL);

		}

		dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));
		dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));

		parent = webkit_dom_node_get_parent_node (block);

		if (!webkit_dom_node_get_previous_sibling (block)) {
			/* Currect block is in the beginning of citation, just move it
			 * before the citation where already is */
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				block,
				parent,
				NULL);
		} else if (!webkit_dom_node_get_next_sibling (block)) {
			/* Currect block is at the end of the citation, just move it
			 * after the citation where already is */
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				block,
				webkit_dom_node_get_next_sibling (parent),
				NULL);
		} else {
			/* Current block is somewhere in the middle of the citation
			 * so we need to split the citation and insert the block into
			 * the citation that is one level lower */
			WebKitDOMNode *clone, *child;

			clone = webkit_dom_node_clone_node (parent, FALSE);

			/* Move nodes that are after the currect block into the
			 * new blockquote */
			child = webkit_dom_node_get_next_sibling (block);
			while (child) {
				WebKitDOMNode *next = webkit_dom_node_get_next_sibling (child);
				webkit_dom_node_append_child (clone, child, NULL);
				child = next;
			}

			clone = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				clone,
				webkit_dom_node_get_next_sibling (parent),
				NULL);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				block,
				clone,
				NULL);
		}

		wrap_and_quote_element (document, extension, WEBKIT_DOM_ELEMENT (block));
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

	return success;
}

static gboolean
prevent_from_deleting_last_element_in_body (WebKitDOMDocument *document)
{
	gboolean ret_val = FALSE;
	WebKitDOMHTMLElement *body;
	WebKitDOMNode *node;

	body = webkit_dom_document_get_body (document);

	node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
	if (!node || (
	    webkit_dom_node_get_next_sibling (node) &&
	    !webkit_dom_node_get_next_sibling (webkit_dom_node_get_next_sibling (node)))) {
		gchar *content;

		content = webkit_dom_node_get_text_content (WEBKIT_DOM_NODE (body));

		if (!*content)
			ret_val = TRUE;

		g_free (content);

		if (webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (body), "img", NULL))
			ret_val = FALSE;
	}

	return ret_val;
}

static void
insert_quote_symbols (WebKitDOMElement *element,
                      gint quote_level,
                      gboolean skip_first,
                      gboolean insert_newline)
{
	gchar *text;
	gint ii;
	GString *output;
	gchar *quotation;

	if (!WEBKIT_DOM_IS_ELEMENT (element))
		return;

	text = webkit_dom_element_get_inner_html (element);
	output = g_string_new ("");
	quotation = get_quotation_for_level (quote_level);

	if (g_strcmp0 (text, "\n") == 0) {
		g_string_append (output, "<span class=\"-x-evo-quoted\">");
		g_string_append (output, quotation);
		g_string_append (output, "</span>");
		g_string_append (output, "\n");
	} else {
		gchar **lines;

		lines = g_strsplit (text, "\n", 0);

		for (ii = 0; lines[ii]; ii++) {
			if (ii == 0 && skip_first) {
				if (g_strv_length (lines) == 1) {
					g_strfreev (lines);
					goto exit;
				}
				g_string_append (output, lines[ii]);
				g_string_append (output, "\n");
			}

			g_string_append (output, "<span class=\"-x-evo-quoted\">");
			g_string_append (output, quotation);
			g_string_append (output, "</span>");

			/* Insert line of text */
			g_string_append (output, lines[ii]);
			if ((ii == g_strv_length (lines) - 1) &&
			    !g_str_has_suffix (text, "\n") && !insert_newline) {
				/* If we are on last line and node's text doesn't
				 * end with \n, don't insert it */
				break;
			}
			g_string_append (output, "\n");
		}

		g_strfreev (lines);
	}

	webkit_dom_element_set_inner_html (element, output->str, NULL);
 exit:
	g_free (quotation);
	g_free (text);
	g_string_free (output, TRUE);
}

static void
quote_node (WebKitDOMDocument *document,
            WebKitDOMNode *node,
            gint quote_level)
{
	gboolean skip_first = FALSE;
	gboolean insert_newline = FALSE;
	gboolean is_html_node = FALSE;
	WebKitDOMElement *wrapper;
	WebKitDOMNode *node_clone, *prev_sibling, *next_sibling;

	/* Don't quote when we are not in citation */
	if (quote_level == 0)
		return;

	if (WEBKIT_DOM_IS_COMMENT (node))
		return;

	if (WEBKIT_DOM_IS_ELEMENT (node)) {
		insert_quote_symbols (WEBKIT_DOM_ELEMENT (node), quote_level, FALSE, FALSE);
		return;
	}

	prev_sibling = webkit_dom_node_get_previous_sibling (node);
	next_sibling = webkit_dom_node_get_next_sibling (node);

	is_html_node =
		!WEBKIT_DOM_IS_TEXT (prev_sibling) &&
		!WEBKIT_DOM_IS_COMMENT (prev_sibling) && (
		WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling) ||
		element_has_tag (WEBKIT_DOM_ELEMENT (prev_sibling), "b") ||
		element_has_tag (WEBKIT_DOM_ELEMENT (prev_sibling), "i") ||
		element_has_tag (WEBKIT_DOM_ELEMENT (prev_sibling), "u") ||
		element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "Apple-tab-span"));

	if (prev_sibling && is_html_node)
		skip_first = TRUE;

	/* Skip the BR between first blockquote and pre */
	if (quote_level == 1 && next_sibling && WEBKIT_DOM_IS_HTML_PRE_ELEMENT (next_sibling))
		return;

	/* Do temporary wrapper */
	wrapper = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_class_name (wrapper, "-x-evo-temp-text-wrapper");

	node_clone = webkit_dom_node_clone_node (node, TRUE);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (wrapper),
		node_clone,
		NULL);

	insert_quote_symbols (
		wrapper,
		quote_level,
		skip_first,
		insert_newline);

	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (node),
		WEBKIT_DOM_NODE (wrapper),
		node,
		NULL);
}

static void
insert_quote_symbols_before_node (WebKitDOMDocument *document,
                                  WebKitDOMNode *node,
                                  gint quote_level,
                                  gboolean is_html_node)
{
	gboolean skip, wrap_br;
	gchar *quotation;
	WebKitDOMElement *element;

	quotation = get_quotation_for_level (quote_level);
	element = webkit_dom_document_create_element (document, "SPAN", NULL);
	element_add_class (element, "-x-evo-quoted");
	webkit_dom_element_set_inner_html (element, quotation, NULL);

	/* Don't insert temporary BR before BR that is used for wrapping */
	skip = WEBKIT_DOM_IS_HTML_BR_ELEMENT (node);
	wrap_br = element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br");
	skip = skip && wrap_br;

	if (is_html_node && !skip) {
		WebKitDOMElement *new_br;

		new_br = webkit_dom_document_create_element (document, "br", NULL);
		element_add_class (new_br, "-x-evo-temp-br");

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (new_br),
			node,
			NULL);
	}

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (node),
		WEBKIT_DOM_NODE (element),
		node,
		NULL);

	if (is_html_node && !wrap_br)
		remove_node (node);

	g_free (quotation);
}

static gboolean
check_if_suppress_next_node (WebKitDOMNode *node)
{
	if (!node)
		return FALSE;

	if (node && WEBKIT_DOM_IS_ELEMENT (node))
		if (dom_is_selection_position_node (node))
			if (!webkit_dom_node_get_previous_sibling (node))
				return FALSE;

	return TRUE;
}

static void
quote_br_node (WebKitDOMNode *node,
               gint quote_level)
{
	gchar *quotation, *content;

	quotation = get_quotation_for_level (quote_level);

	content = g_strconcat (
		"<span class=\"-x-evo-quoted\">",
		quotation,
		"</span><br class=\"-x-evo-temp-br\">",
		NULL);

	webkit_dom_element_set_outer_html (
		WEBKIT_DOM_ELEMENT (node),
		content,
		NULL);

	g_free (content);
	g_free (quotation);
}

static void
quote_plain_text_recursive (WebKitDOMDocument *document,
                            WebKitDOMNode *node,
                            WebKitDOMNode *start_node,
                            gint quote_level)
{
	gboolean skip_node = FALSE;
	gboolean move_next = FALSE;
	gboolean suppress_next = FALSE;
	gboolean is_html_node = FALSE;
	gboolean next = FALSE;
	WebKitDOMNode *next_sibling, *prev_sibling;

	node = webkit_dom_node_get_first_child (node);

	while (node) {
		gchar *text_content;

		skip_node = FALSE;
		move_next = FALSE;
		is_html_node = FALSE;

		if (WEBKIT_DOM_IS_COMMENT (node) ||
		    WEBKIT_DOM_IS_HTML_META_ELEMENT (node) ||
		    WEBKIT_DOM_IS_HTML_STYLE_ELEMENT (node) ||
		    WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (node)) {

			move_next = TRUE;
			goto next_node;
		}

		prev_sibling = webkit_dom_node_get_previous_sibling (node);
		next_sibling = webkit_dom_node_get_next_sibling (node);

		if (WEBKIT_DOM_IS_TEXT (node)) {
			/* Start quoting after we are in blockquote */
			if (quote_level > 0 && !suppress_next) {
				/* When quoting text node, we are wrappering it and
				 * afterwards replacing it with that wrapper, thus asking
				 * for next_sibling after quoting will return NULL bacause
				 * that node don't exist anymore */
				quote_node (document, node, quote_level);
				node = next_sibling;
				skip_node = TRUE;
			}

			goto next_node;
		}

		if (!(WEBKIT_DOM_IS_ELEMENT (node) || WEBKIT_DOM_IS_HTML_ELEMENT (node)))
			goto next_node;

		if (dom_is_selection_position_node (node)) {
			/* If there is collapsed selection in the beginning of line
			 * we cannot suppress first text that is after the end of
			 * selection */
			suppress_next = check_if_suppress_next_node (prev_sibling);
			if (suppress_next)
				next = FALSE;
			move_next = TRUE;
			goto next_node;
		}

		if (!WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node) &&
		    webkit_dom_element_get_child_element_count (WEBKIT_DOM_ELEMENT (node)) != 0)
			goto with_children;

		/* Even in plain text mode we can have some basic html element
		 * like anchor and others. When Forwaring e-mail as Quoted EMFormat
		 * generates header that contatains <b> tags (bold font).
		 * We have to treat these elements separately to avoid
		 * modifications of theirs inner texts */
		is_html_node =
			WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node) ||
			element_has_tag (WEBKIT_DOM_ELEMENT (node), "b") ||
			element_has_tag (WEBKIT_DOM_ELEMENT (node), "i") ||
			element_has_tag (WEBKIT_DOM_ELEMENT (node), "u") ||
			element_has_class (WEBKIT_DOM_ELEMENT (node), "Apple-tab-span");

		if (is_html_node) {
			gboolean wrap_br;

			wrap_br =
				prev_sibling &&
				WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling) &&
				element_has_class (
					WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-wrap-br");

			if (!prev_sibling || wrap_br) {
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);
				if (!prev_sibling && next_sibling && WEBKIT_DOM_IS_TEXT (next_sibling))
					suppress_next = TRUE;
			}

			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling) && !wrap_br)
				insert_quote_symbols_before_node (
					document, prev_sibling, quote_level, TRUE);

			move_next = TRUE;
			goto next_node;
		}

		/* If element doesn't have children, we can quote it */
		if (dom_node_is_citation_node (node)) {
			/* Citation with just text inside */
			quote_node (document, node, quote_level + 1);
			/* Set citation as quoted */
			element_add_class (
				WEBKIT_DOM_ELEMENT (node),
				"-x-evo-plaintext-quoted");

			move_next = TRUE;
			goto next_node;
		}

		if (!WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling)) {
				move_next = TRUE;
				goto next_node;
			}
			goto not_br;
		} else if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-first-br") ||
			   element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-last-br")) {
			quote_br_node (node, quote_level);
			node = next_sibling;
			skip_node = TRUE;
			goto next_node;
		}

		if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (next_sibling) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-temp-text-wrapper")) {
			/* Situation when anchors are alone on line */
			text_content = webkit_dom_node_get_text_content (prev_sibling);

			if (g_str_has_suffix (text_content, "\n")) {
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);
				remove_node (node);
				g_free (text_content);
				node = next_sibling;
				skip_node = TRUE;
				goto next_node;
			}
			g_free (text_content);
		}

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling)) {
			quote_br_node (prev_sibling, quote_level);
			node = next_sibling;
			skip_node = TRUE;
			goto next_node;
		}

		if (!prev_sibling && !next_sibling) {
			WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);

			if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (parent) ||
			    WEBKIT_DOM_IS_HTML_PRE_ELEMENT (parent) ||
			    (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent) &&
			     !dom_node_is_citation_node (parent))) {
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);

				goto next_node;
			}
		}

		if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-temp-text-wrapper")) {
			text_content = webkit_dom_node_get_text_content (prev_sibling);
			if (text_content && !*text_content) {
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);

				g_free (text_content);
				goto next_node;

			}

			g_free (text_content);
		}

		if (dom_node_is_citation_node (prev_sibling)) {
			insert_quote_symbols_before_node (
				document, node, quote_level, FALSE);
			goto next_node;
		}

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (node) &&
		    !next_sibling && WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    dom_is_selection_position_node (prev_sibling)) {
			insert_quote_symbols_before_node (
				document, node, quote_level, FALSE);
			goto next_node;
		}

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
			move_next = TRUE;
			goto next_node;
		}

 not_br:
		text_content = webkit_dom_node_get_text_content (node);
		if (text_content && !*text_content) {
			g_free (text_content);
			move_next = TRUE;
			goto next_node;
		}
		g_free (text_content);

		quote_node (document, node, quote_level);

		move_next = TRUE;
		goto next_node;

 with_children:
		if (dom_node_is_citation_node (node)) {
			/* Go deeper and increase level */
			quote_plain_text_recursive (
				document, node, start_node, quote_level + 1);
			/* set citation as quoted */
			element_add_class (
				WEBKIT_DOM_ELEMENT (node),
				"-x-evo-plaintext-quoted");
			move_next = TRUE;
		} else {
			quote_plain_text_recursive (
				document, node, start_node, quote_level);
			move_next = TRUE;
		}
 next_node:
		if (next) {
			suppress_next = FALSE;
			next = FALSE;
		}

		if (suppress_next)
			next = TRUE;

		if (!skip_node) {
			/* Move to next node */
			if (!move_next && webkit_dom_node_has_child_nodes (node)) {
				node = webkit_dom_node_get_first_child (node);
			} else if (webkit_dom_node_get_next_sibling (node)) {
				node = webkit_dom_node_get_next_sibling (node);
			} else {
				return;
			}
		}
	}
}

WebKitDOMElement *
dom_quote_plain_text_element (WebKitDOMDocument *document,
                              WebKitDOMElement *element)
{
	WebKitDOMNode *element_clone;
	WebKitDOMNodeList *list;
	gint ii, length, level;

	element_clone = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (element), TRUE);
	level = get_citation_level (WEBKIT_DOM_NODE (element), TRUE);

	/* Remove old quote characters if the exists */
	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (element_clone), "span.-x-evo-quoted", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);

	webkit_dom_node_normalize (element_clone);
	quote_plain_text_recursive (
		document, element_clone, element_clone, level);

	/* Set citation as quoted */
	if (dom_node_is_citation_node (element_clone))
		element_add_class (
			WEBKIT_DOM_ELEMENT (element_clone),
			"-x-evo-plaintext-quoted");

	/* Replace old element with one, that is quoted */
	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
		element_clone,
		WEBKIT_DOM_NODE (element),
		NULL);

	return WEBKIT_DOM_ELEMENT (element_clone);
}

/**
 * e_html_editor_view_quote_plain_text:
 * @view: an #EHTMLEditorView
 *
 * Quote text inside citation blockquotes in plain text mode.
 *
 * As this function is cloning and replacing all citation blockquotes keep on
 * mind that any pointers to nodes inside these blockquotes will be invalidated.
 */
static WebKitDOMElement *
dom_quote_plain_text (WebKitDOMDocument *document)
{
	WebKitDOMHTMLElement *body;
	WebKitDOMNode *body_clone;
	WebKitDOMNamedNodeMap *attributes;
	WebKitDOMNodeList *list;
	WebKitDOMElement *element;
	gint ii, length;
	gulong attributes_length;

	/* Check if the document is already quoted */
	element = webkit_dom_document_query_selector (
		document, ".-x-evo-plaintext-quoted", NULL);
	if (element)
		return NULL;

	body = webkit_dom_document_get_body (document);
	body_clone = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (body), TRUE);

	/* Clean unwanted spaces before and after blockquotes */
	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (body_clone), "blockquote[type|=cite]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *blockquote = webkit_dom_node_list_item (list, ii);
		WebKitDOMNode *prev_sibling = webkit_dom_node_get_previous_sibling (blockquote);
		WebKitDOMNode *next_sibling = webkit_dom_node_get_next_sibling (blockquote);

		if (prev_sibling && WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling))
			remove_node (prev_sibling);

		if (next_sibling && WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling))
			remove_node (next_sibling);

		if (webkit_dom_node_has_child_nodes (blockquote)) {
			WebKitDOMNode *child = webkit_dom_node_get_first_child (blockquote);
			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (child))
				remove_node (child);
		}
		g_object_unref (blockquote);
	}
	g_object_unref (list);

	webkit_dom_node_normalize (body_clone);
	quote_plain_text_recursive (document, body_clone, body_clone, 0);

	/* Copy attributes */
	attributes = webkit_dom_element_get_attributes (WEBKIT_DOM_ELEMENT (body));
	attributes_length = webkit_dom_named_node_map_get_length (attributes);
	for (ii = 0; ii < attributes_length; ii++) {
		gchar *name, *value;
		WebKitDOMNode *node = webkit_dom_named_node_map_item (attributes, ii);

		name = webkit_dom_node_get_local_name (node);
		value = webkit_dom_node_get_node_value (node);

		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body_clone), name, value, NULL);

		g_object_unref (node);
		g_free (name);
		g_free (value);
	}
	g_object_unref (attributes);

	/* Replace old BODY with one, that is quoted */
	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (body)),
		body_clone,
		WEBKIT_DOM_NODE (body),
		NULL);

	return WEBKIT_DOM_ELEMENT (body_clone);
}

/**
 * e_html_editor_view_dequote_plain_text:
 * @view: an #EHTMLEditorView
 *
 * Dequote already quoted plain text in editor.
 * Editor have to be quoted with e_html_editor_view_quote_plain_text otherwise
 * it's not working.
 */
static void
dom_dequote_plain_text (WebKitDOMDocument *document)
{
	WebKitDOMNodeList *paragraphs;
	gint length, ii;

	paragraphs = webkit_dom_document_query_selector_all (
		document, "blockquote.-x-evo-plaintext-quoted", NULL);
	length = webkit_dom_node_list_get_length (paragraphs);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMElement *element;

		element = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (paragraphs, ii));

		if (dom_node_is_citation_node (WEBKIT_DOM_NODE (element))) {
			element_remove_class (element, "-x-evo-plaintext-quoted");
			dom_remove_quoting_from_element (element);
		}
		g_object_unref (element);
	}
	g_object_unref (paragraphs);
}

static gboolean
create_anchor_for_link (const GMatchInfo *info,
                        GString *res,
                        gpointer data)
{
	gint offset = 0, truncate_from_end = 0;
	const gchar *end_of_match = NULL;
	gchar *match;
	gboolean link_surrounded;

	match = g_match_info_fetch (info, 0);

	if (g_str_has_prefix (match, "&nbsp;"))
		offset += 6;

	end_of_match = match + strlen (match) - 1;
	/* Taken from camel-url-scanner.c */
	/* URLs are extremely unlikely to end with any punctuation, so
	 * strip any trailing punctuation off from link and put it after
	 * the link. Do the same for any closing double-quotes as well. */
	while (end_of_match && end_of_match != match && strchr (URL_INVALID_TRAILING_CHARS, *end_of_match)) {
		truncate_from_end++;
		end_of_match--;
	}
	end_of_match++;

	link_surrounded =
		g_str_has_suffix (res->str, "&lt;");

	if (link_surrounded) {
		if (end_of_match && *end_of_match && strlen (match) > strlen (end_of_match) + 3)
			link_surrounded = link_surrounded && g_str_has_prefix (end_of_match - 3, "&gt;");
		else
			link_surrounded = link_surrounded && g_str_has_suffix (match, "&gt;");

		if (link_surrounded) {
			/* ";" is already counted by code above */
			truncate_from_end += 3;
			end_of_match -= 3;
		}
	}

	g_string_append (res, "<a href=\"");
	if (strstr (match, "@") && !strstr (match, "://"))
		g_string_append (res, "mailto:");
	g_string_append (res, match + offset);
	if (truncate_from_end > 0)
		g_string_truncate (res, res->len - truncate_from_end);

	g_string_append (res, "\">");
	g_string_append (res, match + offset);
	if (truncate_from_end > 0)
		g_string_truncate (res, res->len - truncate_from_end);

	g_string_append (res, "</a>");

	if (truncate_from_end > 0)
		g_string_append (res, end_of_match);

	g_free (match);

	return FALSE;
}

static gboolean
replace_to_nbsp (const GMatchInfo *info,
                 GString *res)
{
	gchar *match;
	gint ii = 0;

	match = g_match_info_fetch (info, 0);

	while (match[ii] != '\0') {
		if (match[ii] == ' ') {
			/* Alone spaces or spaces before/after tabulator. */
			g_string_append (res, "&nbsp;");
		} else if (match[ii] == '\t') {
			/* Replace tabs with their WebKit HTML representation. */
			g_string_append (res, "<span class=\"Apple-tab-span\" style=\"white-space:pre\">\t</span>");
		}

		ii++;
	}

	g_free (match);

	return FALSE;
}

static gboolean
surround_links_with_anchor (const gchar *text)
{
	return (strstr (text, "http") || strstr (text, "ftp") ||
		strstr (text, "www") || strstr (text, "@"));
}

static void
append_new_block (WebKitDOMElement *parent,
                  WebKitDOMElement **block)
{
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (parent),
		WEBKIT_DOM_NODE (*block),
		NULL);

	*block = NULL;
}

static WebKitDOMElement *
create_and_append_new_block (WebKitDOMDocument *document,
			     EHTMLEditorWebExtension *extension,
                             WebKitDOMElement *parent,
                             WebKitDOMElement *block_template,
                             const gchar *content)
{
	WebKitDOMElement *block;

	if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (block_template))
		block = dom_get_paragraph_element (document, extension, -1, 0);
	else
		block = WEBKIT_DOM_ELEMENT (webkit_dom_node_clone_node (
			WEBKIT_DOM_NODE (block_template), FALSE));

	webkit_dom_element_set_inner_html (block, content, NULL);

	append_new_block (parent, &block);

	return block;
}

static void
append_citation_mark (WebKitDOMDocument *document,
                      WebKitDOMElement *parent,
                      const gchar *citation_mark_text)
{
	WebKitDOMText *text;

	text = webkit_dom_document_create_text_node (document, citation_mark_text);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (parent),
		WEBKIT_DOM_NODE (text),
		NULL);
}

static glong
get_decoded_line_length (WebKitDOMDocument *document,
                         const gchar *line_text)
{
	glong total_length = 0, length = 0;
	WebKitDOMElement *decode;
	WebKitDOMNode *node;

	decode = webkit_dom_document_create_element (document, "DIV", NULL);
	webkit_dom_element_set_inner_html (decode, line_text, NULL);

	node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (decode));
	while (node) {
		if (WEBKIT_DOM_IS_TEXT (node)) {
			gulong text_length = 0;

			text_length = webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node));
			total_length += text_length;
			length += text_length;
		} else if (WEBKIT_DOM_IS_ELEMENT (node)) {
			if (element_has_class (WEBKIT_DOM_ELEMENT (node), "Apple-tab-span")) {
				total_length += TAB_LENGTH - length % TAB_LENGTH;
				length = 0;
			}
		}
		node = webkit_dom_node_get_next_sibling (node);
	}

	g_object_unref (decode);

	return total_length;
}

static gboolean
check_if_end_block (const gchar *input,
                    glong length,
                    gboolean preserve_next_line)
{
	const gchar *next_space;

	next_space = strstr (input, " ");
	if (next_space) {
		const gchar *next_br;
		glong length_next_word =
			next_space - input - 4;

		if (g_str_has_prefix (input + 4, "<br>"))
			length_next_word = 0;

		if (length_next_word > 0)
			next_br = strstr (input + 4, "<br>");

		if (length_next_word > 0 && next_br < next_space)
			length_next_word = 0;

		if (length_next_word + length < 72)
			return TRUE;
	} else {
		/* If the current text to insert doesn't contain space we
		 * have to look on the previous line if we were preserving
		 * the block or not */
		return !preserve_next_line;
	}

	return FALSE;
}

/* This parses the HTML code (that contains just text, &nbsp; and BR elements)
 * into blocks.
 * HTML code in that format we can get by taking innerText from some element,
 * setting it to another one and finally getting innerHTML from it */
static void
parse_html_into_blocks (WebKitDOMDocument *document,
			EHTMLEditorWebExtension *extension,
                        WebKitDOMElement *parent,
                        WebKitDOMElement *passed_block_template,
                        const gchar *html)
{
	gboolean ignore_next_br = FALSE;
	gboolean first_element = TRUE;
	gboolean citation_was_first_element = FALSE;
	gboolean preserve_next_line = FALSE;
	gboolean has_citation = FALSE;
	gboolean previously_had_empty_citation_start = FALSE;
	const gchar *prev_br, *next_br;
	GRegex *regex_nbsp = NULL, *regex_link = NULL, *regex_email = NULL;
	WebKitDOMElement *block = NULL, *block_template = passed_block_template;

	webkit_dom_element_set_inner_html (parent, "", NULL);

	if (!block_template) {
		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent)) {
			gboolean use_paragraphs;
			GSettings *settings;

			settings = e_util_ref_settings ("org.gnome.evolution.mail");

			use_paragraphs = g_settings_get_boolean (
				settings, "composer-wrap-quoted-text-in-replies");

			if (use_paragraphs)
				block_template = dom_get_paragraph_element (
					document, extension, -1, 0);
			else
				block_template = webkit_dom_document_create_element (document, "pre", NULL);

			g_object_unref (settings);
		} else
			block_template = dom_get_paragraph_element (
				document, extension, -1, 0);
	}

	prev_br = html;
	next_br = strstr (prev_br, "<br>");

	/* Replace single spaces on the beginning of line, 2+ spaces and
	 * tabulators with non breaking spaces */
	regex_nbsp = g_regex_new ("^\\s{1}|\\s{2,}|\x9", 0, 0, NULL);

	while (next_br) {
		gboolean local_ignore_next_br = ignore_next_br;
		gboolean local_preserve_next_line = preserve_next_line;
		gboolean preserve_block = TRY_TO_PRESERVE_BLOCKS;
		const gchar *citation = NULL, *citation_end = NULL;
		const gchar *rest = NULL, *with_br = NULL;
		gchar *to_insert = NULL;

		ignore_next_br = FALSE;
		preserve_next_line = TRUE;

		to_insert = g_utf8_substring (
			prev_br, 0, g_utf8_pointer_to_offset (prev_br, next_br));

		with_br = strstr (to_insert, "<br>");
		citation = strstr (to_insert, "##CITATION_");
		if (citation) {
			gchar *citation_mark;

			has_citation = TRUE;
			if (strstr (citation, "END##")) {
				ignore_next_br = TRUE;
				if (block)
					append_new_block (parent, &block);
			} else {
				previously_had_empty_citation_start = TRUE;
			}

			citation_end = strstr (citation + 2, "##");
			if (citation_end)
				rest = citation_end + 2;

			if (rest && *rest)
				previously_had_empty_citation_start = FALSE;

			if (first_element)
				citation_was_first_element = TRUE;

			if (block)
				append_new_block (parent, &block);
			else if (with_br && rest && !*rest && previously_had_empty_citation_start && ignore_next_br) {
				/* Insert an empty block for an empty blockquote */
				block = create_and_append_new_block (
					document, extension, parent, block_template, "<br>");
				previously_had_empty_citation_start = FALSE;
			}

			citation_mark = g_utf8_substring (
				citation, 0, g_utf8_pointer_to_offset (citation, rest));

			append_citation_mark (document, parent, citation_mark);

			g_free (citation_mark);
		} else {
			rest = with_br ?
				to_insert + 4 + (with_br - to_insert) : to_insert;
			previously_had_empty_citation_start = FALSE;
		}

		if (!rest) {
			preserve_next_line = FALSE;
			goto next;
		}

		if (*rest) {
			gboolean empty = FALSE;
			gchar *truncated = g_strdup (rest);
			gchar *rest_to_insert;

			empty = !*truncated && strlen (rest) > 0;

			if (strchr (" +-@*=\t;#", *rest))
				preserve_block = FALSE;

			rest_to_insert = g_regex_replace_eval (
				regex_nbsp,
				empty ? rest : truncated,
				-1,
				0,
				0,
				(GRegexEvalCallback) replace_to_nbsp,
				NULL,
				NULL);
			g_free (truncated);

			if (surround_links_with_anchor (rest_to_insert)) {
				gboolean is_email_address =
					strstr (rest_to_insert, "@") &&
					!strstr (rest_to_insert, "://");

				if (is_email_address && !regex_email)
					regex_email = g_regex_new (E_MAIL_PATTERN, 0, 0, NULL);
				if (!is_email_address && !regex_link)
					regex_link = g_regex_new (URL_PATTERN, 0, 0, NULL);

				truncated = g_regex_replace_eval (
					is_email_address ? regex_email : regex_link,
					rest_to_insert,
					-1,
					0,
					0,
					create_anchor_for_link,
					NULL,
					NULL);

				g_free (rest_to_insert);
				rest_to_insert = truncated;
			}

			if (g_strcmp0 (rest_to_insert, UNICODE_ZERO_WIDTH_SPACE) == 0) {
				if (block)
					append_new_block (parent, &block);

				block = create_and_append_new_block (
					document, extension, parent, block_template, "<br>");
			} else if (preserve_block) {
				gchar *html;
				gchar *content_to_append;

			       if (!block) {
				      if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (block_template))
					       block = dom_get_paragraph_element (document, extension, -1, 0);
				       else
					       block = WEBKIT_DOM_ELEMENT (webkit_dom_node_clone_node (WEBKIT_DOM_NODE (block_template), FALSE));
			       }

				html = webkit_dom_element_get_inner_html (block);

				content_to_append = g_strconcat (
					html && *html ? " " : "",
					rest_to_insert ? rest_to_insert : "<br>",
					NULL),

				webkit_dom_html_element_insert_adjacent_html (
					WEBKIT_DOM_HTML_ELEMENT (block),
					"beforeend",
					content_to_append,
					NULL);

				g_free (html);
				g_free (content_to_append);
			} else {
				if (block)
					append_new_block (parent, &block);

				block = create_and_append_new_block (
					document, extension, parent, block_template, rest_to_insert);
			}

			if (rest_to_insert && *rest_to_insert && preserve_block && block) {
				glong length = 0;

				/* If the line contains some encoded chracters (i.e. &gt;)
				 * we can't use the strlen functions. */
				if (strstr (rest_to_insert, "&"))
					length = get_decoded_line_length (document, rest_to_insert);
				else
					length = g_utf8_strlen (rest_to_insert, -1);

				/* End the block if there is line with less that 62 characters. */
				/* The shorter line can also mean that there is a long word on next
				 * line (and the line was wrapped). So look at it and decide what to do. */
				if (length < 62 && check_if_end_block (next_br, length, local_preserve_next_line)) {
					append_new_block (parent, &block);
					preserve_next_line = FALSE;
				}

				if (length > 72) {
					append_new_block (parent, &block);
					preserve_next_line = FALSE;
				}
			}

			citation_was_first_element = FALSE;

			g_free (rest_to_insert);
		} else if (with_br) {
			if (!citation && (!local_ignore_next_br || citation_was_first_element)) {
				if (block)
					append_new_block (parent, &block);

				block = create_and_append_new_block (
					document, extension, parent, block_template, "<br>");

				citation_was_first_element = FALSE;
			} else if (first_element && !citation_was_first_element) {
				block = create_and_append_new_block (
					document,
					extension,
					parent,
					block_template,
					"<br class=\"-x-evo-first-br\">");
			} else
				preserve_next_line = FALSE;
		} else if (first_element && !citation_was_first_element) {
			block = create_and_append_new_block (
				document,
				extension,
				parent,
				block_template,
				"<br class=\"-x-evo-first-br\">");
		} else
			preserve_next_line = FALSE;
 next:
		first_element = FALSE;
		prev_br = next_br;
		next_br = strstr (prev_br + 4, "<br>");
		g_free (to_insert);
	}

	if (block)
		append_new_block (parent, &block);

	if (g_utf8_strlen (prev_br, -1) > 0) {
		gchar *rest_to_insert;
		gchar *truncated = g_strdup (
			g_str_has_prefix (prev_br, "<br>") ? prev_br + 4 : prev_br);

		/* On the end on the HTML there is always an extra BR element,
		 * so skip it and if there was another BR element before it mark it. */
		if (truncated && !*truncated) {
			WebKitDOMNode *child;

			child = webkit_dom_node_get_last_child (
				WEBKIT_DOM_NODE (parent));
			if (child) {
				child = webkit_dom_node_get_first_child (child);
				if (child && WEBKIT_DOM_IS_HTML_BR_ELEMENT (child)) {
					/* If the processed HTML contained just
					 * the BR don't overwrite its class. */
					if (!element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-first-br"))
						element_add_class (
							WEBKIT_DOM_ELEMENT (child),
							"-x-evo-last-br");
				} else if (!e_html_editor_web_extension_is_editting_message (extension))
					create_and_append_new_block (
						document, extension, parent, block_template, "<br>");
			} else
				create_and_append_new_block (
					document, extension, parent, block_template, "<br>");
			g_free (truncated);
			goto end;
		}

		if (g_ascii_strncasecmp (truncated, "##CITATION_END##", 16) == 0) {
			append_citation_mark (document, parent, truncated);
			g_free (truncated);
			goto end;
		}

		rest_to_insert = g_regex_replace_eval (
			regex_nbsp,
			truncated,
			-1,
			0,
			0,
			(GRegexEvalCallback) replace_to_nbsp,
			NULL,
			NULL);
		g_free (truncated);

		if (surround_links_with_anchor (rest_to_insert)) {
			gboolean is_email_address =
				strstr (rest_to_insert, "@") &&
				!strstr (rest_to_insert, "://");

			if (is_email_address && !regex_email)
				regex_email = g_regex_new (E_MAIL_PATTERN, 0, 0, NULL);
			if (!is_email_address && !regex_link)
				regex_link = g_regex_new (URL_PATTERN, 0, 0, NULL);

			truncated = g_regex_replace_eval (
				is_email_address ? regex_email : regex_link,
				rest_to_insert,
				-1,
				0,
				0,
				create_anchor_for_link,
				NULL,
				NULL);

			g_free (rest_to_insert);
			rest_to_insert = truncated;
		}

		if (g_strcmp0 (rest_to_insert, UNICODE_ZERO_WIDTH_SPACE) == 0)
			create_and_append_new_block (
				document, extension, parent, block_template, "<br>");
		else
			create_and_append_new_block (
				document, extension, parent, block_template, rest_to_insert);

		g_free (rest_to_insert);
	}

 end:
	if (has_citation) {
		gchar *inner_html;
		GString *start, *end;

		/* Replace text markers with actual HTML blockquotes */
		inner_html = webkit_dom_element_get_inner_html (parent);
		start = e_str_replace_string (
			inner_html, "##CITATION_START##","<blockquote type=\"cite\">");
		end = e_str_replace_string (
			start->str, "##CITATION_END##", "</blockquote>");
		webkit_dom_element_set_inner_html (parent, end->str, NULL);

		g_free (inner_html);
		g_string_free (start, TRUE);
		g_string_free (end, TRUE);
	}

	if (regex_email != NULL)
		g_regex_unref (regex_email);
	if (regex_link != NULL)
		g_regex_unref (regex_link);
	g_regex_unref (regex_nbsp);
}

void
dom_quote_and_insert_text_into_selection (WebKitDOMDocument *document,
                                          EHTMLEditorWebExtension *extension,
                                          const gchar *text)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	gchar *escaped_text, *inner_html;
	WebKitDOMElement *blockquote, *element, *selection_start;
	WebKitDOMNode *sibling;

	if (!text || !*text)
		return;

	/* This is a trick to escape any HTML characters (like <, > or &).
	 * <textarea> automatically replaces all these unsafe characters
	 * by &lt;, &gt; etc. */
	element = webkit_dom_document_create_element (document, "textarea", NULL);
	webkit_dom_element_set_inner_html (element, text, NULL);
	escaped_text = webkit_dom_element_get_inner_html (element);

	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (element), escaped_text, NULL);

	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element));

	dom_selection_save (document);

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_PASTE_QUOTED;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.string.from = NULL;
		ev->data.string.to = g_strdup (text);
	}

	blockquote = webkit_dom_document_create_element (document, "blockquote", NULL);
	webkit_dom_element_set_attribute (blockquote, "type", "cite", NULL);

	selection_start = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start));
	/* Check if block is empty. If so, replace it otherwise insert the quoted
	 * content after current block. */
	if (!sibling || WEBKIT_DOM_IS_HTML_BR_ELEMENT (sibling)) {
		sibling = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (selection_start));
		sibling = webkit_dom_node_get_next_sibling (sibling);
		if (!sibling || WEBKIT_DOM_IS_HTML_BR_ELEMENT (sibling)) {
			webkit_dom_node_replace_child (
				webkit_dom_node_get_parent_node (
					webkit_dom_node_get_parent_node (
						WEBKIT_DOM_NODE (selection_start))),
				WEBKIT_DOM_NODE (blockquote),
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start)),
				NULL);
		}
	} else {
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (webkit_dom_document_get_body (document)),
			WEBKIT_DOM_NODE (blockquote),
			webkit_dom_node_get_next_sibling (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_start))),
			NULL);
	}

	parse_html_into_blocks (document, extension, blockquote, NULL, inner_html);

	if (!e_html_editor_web_extension_get_html_mode (extension)) {
		WebKitDOMNode *node;
		gint word_wrap_length;

		element_add_class (blockquote, "-x-evo-plaintext-quoted");

		word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);
		node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (blockquote));
		while (node) {
			WebKitDOMNode *next_sibling;

			node = WEBKIT_DOM_NODE (dom_wrap_paragraph_length (
				document, extension, WEBKIT_DOM_ELEMENT (node), word_wrap_length - 2));

			webkit_dom_node_normalize (node);
			dom_quote_plain_text_element_after_wrapping (
				document, WEBKIT_DOM_ELEMENT (node), 1);

			next_sibling = webkit_dom_node_get_next_sibling (node);
			if (!next_sibling)
				dom_add_selection_markers_into_element_end (
					document, WEBKIT_DOM_ELEMENT (node), NULL, NULL);
			node = next_sibling;
		}
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

	e_html_editor_web_extension_set_content_changed (extension);

	g_free (escaped_text);
	g_free (inner_html);
}

static void
mark_citation (WebKitDOMElement *citation)
{
	gchar *inner_html, *surrounded;

	inner_html = webkit_dom_element_get_inner_html (citation);

	surrounded = g_strconcat (
		"<span>##CITATION_START##</span>", inner_html,
		"<span>##CITATION_END##</span>", NULL);

	webkit_dom_element_set_inner_html (citation, surrounded, NULL);

	element_add_class (citation, "marked");

	g_free (inner_html);
	g_free (surrounded);
}

static gint
create_text_markers_for_citations_in_document (WebKitDOMDocument *document)
{
	gint count = 0;
	WebKitDOMElement *citation;

	citation = webkit_dom_document_query_selector (
		document, "blockquote[type=cite]:not(.marked)", NULL);

	while (citation) {
		mark_citation (citation);
		count++;

		citation = webkit_dom_document_query_selector (
			document, "blockquote[type=cite]:not(.marked)", NULL);
	}

	return count;
}

static gint
create_text_markers_for_citations_in_element (WebKitDOMElement *element)
{
	gint count = 0;
	WebKitDOMElement *citation;

	citation = webkit_dom_element_query_selector (
		element, "blockquote[type=cite]:not(.marked)", NULL);

	while (citation) {
		mark_citation (citation);
		count ++;

		citation = webkit_dom_element_query_selector (
			element, "blockquote[type=cite]:not(.marked)", NULL);
	}

	return count;
}

static void
quote_plain_text_elements_after_wrapping_in_document (WebKitDOMDocument *document)
{
	gint length, ii;
	WebKitDOMNodeList *list;

	/* Also quote the PRE elements as well. */
	list = webkit_dom_document_query_selector_all (
		document, "blockquote[type=cite] > div.-x-evo-paragraph, blockquote[type=cite] > pre", NULL);

	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		gint citation_level;
		WebKitDOMNode *child;

		child = webkit_dom_node_list_item (list, ii);
		citation_level = get_citation_level (child, TRUE);
		dom_quote_plain_text_element_after_wrapping (
			document, WEBKIT_DOM_ELEMENT (child), citation_level);
		g_object_unref (child);
	}
	g_object_unref (list);
}

static void
clear_attributes (WebKitDOMDocument *document)
{
	gint length, ii;
	WebKitDOMNamedNodeMap *attributes;
	WebKitDOMHTMLElement *body = webkit_dom_document_get_body (document);
	WebKitDOMHTMLHeadElement *head = webkit_dom_document_get_head (document);
	WebKitDOMElement *document_element =
		webkit_dom_document_get_document_element (document);

	/* Remove all attributes from HTML element */
	attributes = webkit_dom_element_get_attributes (document_element);
	length = webkit_dom_named_node_map_get_length (attributes);
	for (ii = length - 1; ii >= 0; ii--) {
		WebKitDOMNode *node = webkit_dom_named_node_map_item (attributes, ii);

		webkit_dom_element_remove_attribute_node (
			document_element, WEBKIT_DOM_ATTR (node), NULL);
		g_object_unref (node);
	}
	g_object_unref (attributes);

	/* Remove everything from HEAD element */
	while (webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (head)))
		remove_node (webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (head)));

	/* Make the quote marks non-selectable. */
	dom_disable_quote_marks_select (document);

	/* Remove non Evolution attributes from BODY element */
	attributes = webkit_dom_element_get_attributes (WEBKIT_DOM_ELEMENT (body));
	length = webkit_dom_named_node_map_get_length (attributes);
	for (ii = length - 1; ii >= 0; ii--) {
		gchar *name;
		WebKitDOMNode *node = webkit_dom_named_node_map_item (attributes, ii);

		name = webkit_dom_node_get_local_name (node);

		if (!g_str_has_prefix (name, "data-") && (g_strcmp0 (name, "spellcheck") != 0))
			webkit_dom_element_remove_attribute_node (
				WEBKIT_DOM_ELEMENT (body),
				WEBKIT_DOM_ATTR (node),
				NULL);

		g_object_unref (node);
		g_free (name);
	}
	g_object_unref (attributes);
}

static void
body_compositionstart_event_cb (WebKitDOMElement *element,
                                WebKitDOMUIEvent *event,
                                EHTMLEditorWebExtension *extension)
{
	WebKitDOMDocument *document;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));
	e_html_editor_web_extension_set_composition_in_progress (extension, TRUE);
	dom_remove_input_event_listener_from_body (document, extension);
}

static void
body_compositionend_event_cb (WebKitDOMElement *element,
                              WebKitDOMUIEvent *event,
                              EHTMLEditorWebExtension *extension)
{
	WebKitDOMDocument *document;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));
	e_html_editor_web_extension_set_composition_in_progress (extension, FALSE);
	dom_remove_input_event_listener_from_body (document, extension);
}

static void
register_html_events_handlers (WebKitDOMHTMLElement *body,
                               EHTMLEditorWebExtension *extension)
{
	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"keydown",
		G_CALLBACK (body_keydown_event_cb),
		FALSE,
		extension);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"keypress",
		G_CALLBACK (body_keypress_event_cb),
		FALSE,
		extension);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"keyup",
		G_CALLBACK (body_keyup_event_cb),
		FALSE,
		extension);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"compositionstart",
		G_CALLBACK (body_compositionstart_event_cb),
		FALSE,
		extension);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"compositionend",
		G_CALLBACK (body_compositionend_event_cb),
		FALSE,
		extension);
}


static void
rename_attribute (WebKitDOMElement *element,
                  const gchar *from,
                  const gchar *to)
{
	gchar *value;

	value = webkit_dom_element_get_attribute (element, from);
	if (value)
		webkit_dom_element_set_attribute (element, to, value, NULL);
	webkit_dom_element_remove_attribute (element, from);
	g_free (value);
}

static void
set_monospace_font_family_on_body (WebKitDOMElement *body,
                                   gboolean html_mode)
{
	/* If copying some content in view, WebKit adds various information about
	 * the content's style (such as color, font size, ..) to the resulting HTML
	 * to correctly apply the style when pasting the content later. The thing
	 * is that in plain text mode the only font allowed is the monospaced one,
	 * but we are forcing it through user style sheet in WebKitWebSettings and
	 * sadly WebKit doesn't count with it, so when the content is pasted,
	 * WebKit wraps it inside SPANs and sets the font-family style on them.
	 * The problem is that when we switch to the HTML mode, the pasted content
	 * will have the monospaced font set. To avoid it we need to set the
	 * font-family style to the body, so WebKit will know about it and will
	 * avoid the described behaviour. */
	if (!html_mode) {
		rename_attribute (WEBKIT_DOM_ELEMENT (body), "data-style", "style");
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body),
			"style",
			"font-family: Monospace;",
			NULL);
	} else {
		rename_attribute (WEBKIT_DOM_ELEMENT (body), "style", "data-style");
	}
}

void
dom_convert_content (WebKitDOMDocument *document,
                      EHTMLEditorWebExtension *extension,
                      const gchar *preferred_text)
{
	gboolean start_bottom, empty = FALSE;
	gchar *inner_html;
	gint ii, length;
	GSettings *settings;
	WebKitDOMElement *paragraph, *content_wrapper, *top_signature;
	WebKitDOMElement *cite_body, *signature, *wrapper;
	WebKitDOMHTMLElement *body;
	WebKitDOMNodeList *list;
	WebKitDOMNode *node;
	WebKitDOMDOMWindow *dom_window;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	start_bottom = g_settings_get_boolean (settings, "composer-reply-start-bottom");
	g_object_unref (settings);

	dom_window = webkit_dom_document_get_default_view (document);
	body = webkit_dom_document_get_body (document);
	/* Wrapper that will represent the new body. */
	wrapper = webkit_dom_document_create_element (document, "div", NULL);

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-converted", "", NULL);

	cite_body = webkit_dom_document_query_selector (
		document, "span.-x-evo-cite-body", NULL);

	/* content_wrapper when the processed text will be placed. */
	content_wrapper = webkit_dom_document_create_element (
		document, cite_body ? "blockquote" : "div", NULL);
	if (cite_body) {
		webkit_dom_element_set_attribute (content_wrapper, "type", "cite", NULL);
		webkit_dom_element_set_attribute (content_wrapper, "id", "-x-evo-main-cite", NULL);
	}

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (wrapper), WEBKIT_DOM_NODE (content_wrapper), NULL);

	/* Remove all previously inserted paragraphs. */
	list = webkit_dom_document_query_selector_all (
		document, ".-x-evo-paragraph:not([data-headers])", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);

	/* Insert the paragraph where the caret will be. */
	paragraph = dom_prepare_paragraph (document, extension, TRUE);
	webkit_dom_element_set_id (paragraph, "-x-evo-input-start");
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (wrapper),
		WEBKIT_DOM_NODE (paragraph),
		start_bottom ?
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (content_wrapper)) :
			WEBKIT_DOM_NODE (content_wrapper),
		NULL);

	/* Insert signature (if presented) to the right position. */
	top_signature = webkit_dom_document_query_selector (
		document, ".-x-evo-top-signature", NULL);
	signature = webkit_dom_document_query_selector (
		document, ".-x-evo-signature-content_wrapper", NULL);
	if (signature) {
		if (top_signature) {
			WebKitDOMElement *spacer;

			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (wrapper),
				WEBKIT_DOM_NODE (signature),
				start_bottom ?
					WEBKIT_DOM_NODE (content_wrapper) :
					webkit_dom_node_get_next_sibling (
						WEBKIT_DOM_NODE (paragraph)),
				NULL);
			/* Insert NL after the signature */
			spacer = dom_prepare_paragraph (document, extension, FALSE);
			element_add_class (spacer, "-x-evo-top-signature-spacer");
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (wrapper),
				WEBKIT_DOM_NODE (spacer),
				webkit_dom_node_get_next_sibling (
					WEBKIT_DOM_NODE (signature)),
				NULL);
		} else {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (wrapper),
				WEBKIT_DOM_NODE (signature),
				webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (
					start_bottom ? paragraph : content_wrapper)),
				NULL);
		}
	}

	/* Move credits to the body */
	list = webkit_dom_document_query_selector_all (
		document, "span.-x-evo-to-body[data-credits]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		char *credits;
		WebKitDOMElement *element;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		element = dom_get_paragraph_element (document, extension, -1, 0);
		credits = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "data-credits");
		webkit_dom_html_element_set_inner_text (WEBKIT_DOM_HTML_ELEMENT (element), credits, NULL);
		g_free (credits);

		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (wrapper),
			WEBKIT_DOM_NODE (element),
			WEBKIT_DOM_NODE (content_wrapper),
			NULL);

		remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);

	/* Move headers to body */
	list = webkit_dom_document_query_selector_all (
		document, "div[data-headers]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (list, ii);
		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (node), "data-headers");
		dom_set_paragraph_style (
			document, extension, WEBKIT_DOM_ELEMENT (node), -1, 0, "");
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (wrapper),
			node,
			WEBKIT_DOM_NODE (content_wrapper),
			NULL);

		g_object_unref (node);
	}
	g_object_unref (list);

	repair_gmail_blockquotes (document);
	create_text_markers_for_citations_in_document (document);

	if (preferred_text && *preferred_text)
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (content_wrapper), preferred_text, NULL);
	else {
		gchar *inner_text;

		inner_text = webkit_dom_html_element_get_inner_text (body);
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (content_wrapper), inner_text, NULL);

		g_free (inner_text);
	}

	inner_html = webkit_dom_element_get_inner_html (content_wrapper);

	/* Replace the old body with the new one. */
	node = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (body), FALSE);
	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (body)),
		node,
		WEBKIT_DOM_NODE (body),
		NULL);
	body = WEBKIT_DOM_HTML_ELEMENT (node);

	/* Copy all to nodes to the new body. */
	while ((node = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (wrapper)))) {
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			WEBKIT_DOM_NODE (node),
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)),
			NULL);
	}
	remove_node (WEBKIT_DOM_NODE (wrapper));

	if (inner_html && !*inner_html)
		empty = TRUE;

	length = webkit_dom_element_get_child_element_count (WEBKIT_DOM_ELEMENT (body));
	if (length <= 1) {
		empty = TRUE;
		if (length == 1) {
			WebKitDOMNode *child;

			child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
			empty = child && WEBKIT_DOM_IS_HTML_BR_ELEMENT (child);
		}
	}

	if (preferred_text && *preferred_text)
		empty = FALSE;

	if (!empty)
		parse_html_into_blocks (document, extension, content_wrapper, NULL, inner_html);
	else
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (content_wrapper),
			WEBKIT_DOM_NODE (dom_prepare_paragraph (document, extension, FALSE)),
			NULL);

	if (!cite_body) {
		if (!empty) {
			WebKitDOMNode *child;

			while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (content_wrapper)))) {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (content_wrapper)),
					child,
					WEBKIT_DOM_NODE (content_wrapper),
					NULL);
			}
		}

		remove_node (WEBKIT_DOM_NODE (content_wrapper));
	}

	/* If not editting a message, don't add any new block and just place
	 * the carret in the beginning of content. We want to have the same
	 * behaviour when editting message as new or we start replying on top. */
	if (e_html_editor_web_extension_is_message_from_edit_as_new (extension) ||
	    !e_html_editor_web_extension_is_editting_message (extension) ||
            !start_bottom) {
		WebKitDOMNode *child;

		remove_node (WEBKIT_DOM_NODE (paragraph));
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
		if (child)
			dom_add_selection_markers_into_element_start (
				document, WEBKIT_DOM_ELEMENT (child), NULL, NULL);
	}

	paragraph = webkit_dom_document_query_selector (document, "br.-x-evo-last-br", NULL);
	if (paragraph)
		webkit_dom_element_remove_attribute (paragraph, "class");
	paragraph = webkit_dom_document_query_selector (document, "br.-x-evo-first-br", NULL);
	if (paragraph)
		webkit_dom_element_remove_attribute (paragraph, "class");

	if (!e_html_editor_web_extension_get_html_mode (extension)) {
		dom_wrap_paragraphs_in_document (document, extension);

		quote_plain_text_elements_after_wrapping_in_document (document);
	}

	clear_attributes (document);

	dom_selection_restore (document);
	dom_force_spell_check_in_viewport (document, extension);

	/* Register on input event that is called when the content (body) is modified */
	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"input",
		G_CALLBACK (body_input_event_cb),
		FALSE,
		extension);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (dom_window),
		"scroll",
		G_CALLBACK (body_scroll_event_cb),
		FALSE,
		extension);

	register_html_events_handlers (body, extension);
	set_monospace_font_family_on_body (WEBKIT_DOM_ELEMENT (body), e_html_editor_web_extension_get_html_mode (extension));

	g_free (inner_html);
}

void
dom_convert_and_insert_html_into_selection (WebKitDOMDocument *document,
                                            EHTMLEditorWebExtension *extension,
                                            const gchar *html,
                                            gboolean is_html)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	gboolean has_selection;
	gchar *inner_html;
	gint citation_level;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *element;
	WebKitDOMNode *node, *current_block;

	dom_remove_input_event_listener_from_body (document, extension);

	dom_selection_save (document);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");
	current_block = get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (current_block))
		current_block = NULL;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		gboolean collapsed;

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_PASTE;
/* FIXME WK2
		ev->type = HISTORY_PASTE_AS_TEXT;*/

		collapsed = dom_selection_is_collapsed (document);
		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		if (!collapsed) {
			ev->before.end.x = ev->before.start.x;
			ev->before.end.y = ev->before.start.y;
		}

		ev->data.string.from = NULL;
		ev->data.string.to = g_strdup (html);
	}

	element = webkit_dom_document_create_element (document, "div", NULL);
	if (is_html) {
		gchar *inner_text;

		if (strstr (html, "\n")) {
			GRegex *regex;
			gchar *tmp;

			/* Strip new lines between tags to avoid unwanted line breaks. */
			regex = g_regex_new ("\\>[\\s]+\\<", 0, 0, NULL);
			tmp = g_regex_replace (
				regex, html, -1, 0, "> <", 0, NULL);
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (element), tmp, NULL);
			g_free (tmp);
			g_regex_unref (regex);
		} else {
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (element), html, NULL);
		}

		inner_text = webkit_dom_html_element_get_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (element));
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (element), inner_text, NULL);

		g_free (inner_text);
	} else
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (element), html, NULL);

	inner_html = webkit_dom_element_get_inner_html (element);
	parse_html_into_blocks (document, extension, element, WEBKIT_DOM_ELEMENT (current_block), inner_html);

	g_free (inner_html);

	has_selection = !dom_selection_is_collapsed (document);
	if (has_selection) {
		WebKitDOMRange *range;

		range = dom_get_current_range (document);
		insert_delete_event (document, extension, range);
		g_object_unref (range);
	}

	citation_level = get_citation_level (WEBKIT_DOM_NODE (selection_end_marker), FALSE);
	/* Pasting into the citation */
	if (citation_level > 0) {
		gint length;
		gint word_wrap_length;
		WebKitDOMElement *br;
		WebKitDOMNode *first_paragraph, *last_paragraph;
		WebKitDOMNode *child, *parent;

		first_paragraph = webkit_dom_node_get_first_child (
			WEBKIT_DOM_NODE (element));
		last_paragraph = webkit_dom_node_get_last_child (
			WEBKIT_DOM_NODE (element));

		word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);
		length = word_wrap_length - 2 * citation_level;

		/* Pasting text that was parsed just into one paragraph */
		if (webkit_dom_node_is_same_node (first_paragraph, last_paragraph)) {
			WebKitDOMNode *child, *parent;

			parent = get_parent_block_node_from_child (
				WEBKIT_DOM_NODE (selection_start_marker));

			dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (parent));
			dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (parent));

			while ((child = webkit_dom_node_get_first_child (first_paragraph)))
				webkit_dom_node_insert_before (
					parent,
					child,
					WEBKIT_DOM_NODE (selection_start_marker),
					NULL);

			parent = WEBKIT_DOM_NODE (
				dom_wrap_paragraph_length (
					document, extension, WEBKIT_DOM_ELEMENT (parent), length));
			webkit_dom_node_normalize (parent);
			dom_quote_plain_text_element_after_wrapping (
				document, WEBKIT_DOM_ELEMENT (parent), citation_level);

			goto delete;
		}

		/* Pasting content parsed into the multiple paragraphs */
		parent = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (parent));
		dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (parent));

		/* Move the elements from the first paragraph before the selection start element */
		while ((child = webkit_dom_node_get_first_child (first_paragraph)))
			webkit_dom_node_insert_before (
				parent,
				child,
				WEBKIT_DOM_NODE (selection_start_marker),
				NULL);

		remove_node (first_paragraph);

		/* If the BR element is on the last position, remove it as we don't need it */
		child = webkit_dom_node_get_last_child (parent);
		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (child))
			remove_node (child);

		parent = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_end_marker));

		child = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (selection_end_marker));
		/* Move the elements that are in the same paragraph as the selection end
		 * on the end of pasted text, but avoid BR on the end of paragraph */
		while (child) {
			WebKitDOMNode *next_child =
				webkit_dom_node_get_next_sibling  (child);
			if (!(!next_child && WEBKIT_DOM_IS_HTML_BR_ELEMENT (child)))
				webkit_dom_node_append_child (last_paragraph, child, NULL);
			child = next_child;
		}

		/* Caret will be restored on the end of pasted text */
		webkit_dom_node_append_child (
			last_paragraph,
			WEBKIT_DOM_NODE (dom_create_selection_marker (document, TRUE)),
			NULL);

		webkit_dom_node_append_child (
			last_paragraph,
			WEBKIT_DOM_NODE (dom_create_selection_marker (document, FALSE)),
			NULL);

		/* Insert the paragraph with the end of the pasted text after
		 * the paragraph that contains the selection end */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			last_paragraph,
			webkit_dom_node_get_next_sibling (parent),
			NULL);

		/* Wrap, quote and move all paragraphs from pasted text into the body */
		while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)))) {
			child = WEBKIT_DOM_NODE (dom_wrap_paragraph_length (
				document, extension, WEBKIT_DOM_ELEMENT (child), length));
			dom_quote_plain_text_element_after_wrapping (
				document, WEBKIT_DOM_ELEMENT (child), citation_level);
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (last_paragraph),
				child,
				last_paragraph,
				NULL);
		}

		webkit_dom_node_normalize (last_paragraph);

		last_paragraph = WEBKIT_DOM_NODE (
			dom_wrap_paragraph_length (
				document, extension, WEBKIT_DOM_ELEMENT (last_paragraph), length));
		dom_quote_plain_text_element_after_wrapping (
			document, WEBKIT_DOM_ELEMENT (last_paragraph), citation_level);

		dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (parent));
		dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (parent));

		parent = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		parent = WEBKIT_DOM_NODE (dom_wrap_paragraph_length (
			document, extension, WEBKIT_DOM_ELEMENT (parent), length));
		dom_quote_plain_text_element_after_wrapping (
			document, WEBKIT_DOM_ELEMENT (parent), citation_level);

		/* If the pasted text begun or ended with a new line we have to
		 * quote these paragraphs as well */
		br = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (last_paragraph), "br.-x-evo-last-br", NULL);
		if (br) {
			WebKitDOMNode *parent;

			parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (br));
			quote_plain_text_recursive (document, parent, parent, citation_level);
			webkit_dom_element_remove_attribute (br, "class");
		}

		br = webkit_dom_document_query_selector (
			document, "* > br.-x-evo-first-br", NULL);
		if (br) {
			WebKitDOMNode *parent;

			parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (br));
			quote_plain_text_recursive (document, parent, parent, citation_level);
			webkit_dom_element_remove_attribute (br, "class");
		}
 delete:
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
		/* Remove the text that was meant to be replaced by the pasted text */
		if (has_selection)
			dom_exec_command (
				document, extension, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);

		g_object_unref (element);
		goto out;
	}

	remove_node (WEBKIT_DOM_NODE (selection_start_marker));
	remove_node (WEBKIT_DOM_NODE (selection_end_marker));

	/* If the text to insert was converted just to one block, pass just its
	 * text to WebKit otherwise WebKit will insert unwanted block with
	 * extra new line. */
	if (!webkit_dom_node_get_next_sibling (webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element))))
		inner_html = webkit_dom_html_element_get_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element))));
	else
		inner_html = webkit_dom_html_element_get_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (element));

	dom_exec_command (
		document, extension, E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML, inner_html);

	if (g_str_has_suffix (inner_html, " "))
		dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, " ");

	g_free (inner_html);

	g_object_unref (element);
	dom_selection_save (document);

	element = webkit_dom_document_query_selector (
		document, "* > br.-x-evo-first-br", NULL);
	if (element) {
		WebKitDOMNode *sibling;
		WebKitDOMNode *parent;

		parent = webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (element));

		sibling = webkit_dom_node_get_previous_sibling (parent);
		if (sibling)
			remove_node (WEBKIT_DOM_NODE (parent));
		else
			webkit_dom_element_remove_attribute (element, "class");
	}

	element = webkit_dom_document_query_selector (
		document, "* > br.-x-evo-last-br", NULL);
	if (element) {
		WebKitDOMNode *parent;
		WebKitDOMNode *child;

		parent = webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (element));

		node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent));
		if (node) {
			node = webkit_dom_node_get_first_child (node);
			if (node) {
				inner_html = webkit_dom_node_get_text_content (node);
				if (g_str_has_prefix (inner_html, UNICODE_NBSP))
					webkit_dom_character_data_replace_data (
						WEBKIT_DOM_CHARACTER_DATA (node), 0, 1, "", NULL);
				g_free (inner_html);
			}
		}

		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		if (has_selection) {
			/* Everything after the selection end marker have to be in separate
			 * paragraph */
			child = webkit_dom_node_get_next_sibling (
				WEBKIT_DOM_NODE (selection_end_marker));
			/* Move the elements that are in the same paragraph as the selection end
			 * on the end of pasted text, but avoid BR on the end of paragraph */
			while (child) {
				WebKitDOMNode *next_child =
					webkit_dom_node_get_next_sibling  (child);
				if (!(!next_child && WEBKIT_DOM_IS_HTML_BR_ELEMENT (child)))
					webkit_dom_node_append_child (parent, child, NULL);
				child = next_child;
			}

			remove_node (WEBKIT_DOM_NODE (element));

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (
					webkit_dom_node_get_parent_node (
						WEBKIT_DOM_NODE (selection_end_marker))),
				parent,
				webkit_dom_node_get_next_sibling (
					webkit_dom_node_get_parent_node (
						WEBKIT_DOM_NODE (selection_end_marker))),
				NULL);
			node = parent;
		} else {
			node = webkit_dom_node_get_next_sibling (parent);
			if (!node) {
				fix_structure_after_pasting_multiline_content (parent);
				if (!webkit_dom_node_get_first_child (parent))
					remove_node (parent);
			}
		}

		if (node) {
			/* Restore caret on the end of pasted text */
			webkit_dom_node_insert_before (
				node,
				WEBKIT_DOM_NODE (selection_end_marker),
				webkit_dom_node_get_first_child (node),
				NULL);

			selection_start_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			webkit_dom_node_insert_before (
				node,
				WEBKIT_DOM_NODE (selection_start_marker),
				webkit_dom_node_get_first_child (node),
				NULL);
		}

		if (element)
			webkit_dom_element_remove_attribute (element, "class");

		if (webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent)) && !has_selection)
			remove_node (parent);
	} else {
		/* When pasting the content that was copied from the composer, WebKit
		 * restores the selection wrongly, thus is saved wrongly and we have
		 * to fix it */
		WebKitDOMNode *block, *parent, *clone1, *clone2;

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		block = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		parent = webkit_dom_node_get_parent_node (block);
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (parent), "id");

		/* Check if WebKit created wrong structure */
		clone1 = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (block), FALSE);
		clone2 = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (parent), FALSE);
		if (webkit_dom_node_is_equal_node (clone1, clone2) ||
		    (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (clone1) && WEBKIT_DOM_IS_HTML_DIV_ELEMENT (clone2))) {
			fix_structure_after_pasting_multiline_content (block);
			if (g_strcmp0 (html, "\n") == 0) {
				WebKitDOMElement *br;

				br = webkit_dom_document_create_element (document, "br", NULL);
				webkit_dom_node_append_child (
					parent, WEBKIT_DOM_NODE (br), NULL);

				webkit_dom_node_insert_before (
					parent,
					WEBKIT_DOM_NODE (selection_start_marker),
					webkit_dom_node_get_last_child (parent),
					NULL);
			} else if (!webkit_dom_node_get_first_child (parent))
				remove_node (parent);
		}

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (selection_start_marker)),
			WEBKIT_DOM_NODE (selection_end_marker),
			webkit_dom_node_get_next_sibling (
				WEBKIT_DOM_NODE (selection_start_marker)),
			NULL);
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
 out:
	dom_force_spell_check_in_viewport (document, extension);
	dom_scroll_to_caret (document);

	dom_register_input_event_listener_on_body (document, extension);
}

static gint
get_indentation_level (WebKitDOMElement *element)
{
	WebKitDOMElement *parent;
	gint level = 1;

	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (element));
	/* Count level of indentation */
	while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (element_has_class (parent, "-x-evo-indented"))
			level++;

		parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (parent));
	}

	return level;
}

static void
process_blockquote (WebKitDOMElement *blockquote,
		    gboolean replace_indentation_with_spaces)
{
	WebKitDOMNodeList *list;
	int jj, length;

	/* First replace wrappers */
	list = webkit_dom_element_query_selector_all (
		blockquote, "span.-x-evo-temp-text-wrapper", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (jj = 0; jj < length; jj++) {
		WebKitDOMNode *quoted_node;
		gchar *text_content;

		quoted_node = webkit_dom_node_list_item (list, jj);
		text_content = webkit_dom_node_get_text_content (quoted_node);
		webkit_dom_element_set_outer_html (
			WEBKIT_DOM_ELEMENT (quoted_node), text_content, NULL);

		g_free (text_content);
		g_object_unref (quoted_node);
	}
	g_object_unref (list);

	/* Afterwards replace quote nodes with symbols */
	list = webkit_dom_element_query_selector_all (
		blockquote, "span.-x-evo-quoted", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (jj = 0; jj < length; jj++) {
		WebKitDOMNode *quoted_node;
		gchar *text_content;

		quoted_node = webkit_dom_node_list_item (list, jj);
		text_content = webkit_dom_node_get_text_content (quoted_node);
		webkit_dom_element_set_outer_html (
			WEBKIT_DOM_ELEMENT (quoted_node), text_content, NULL);

		g_free (text_content);
		g_object_unref (quoted_node);
	}
	g_object_unref (list);

	if (element_has_class (blockquote, "-x-evo-indented") && replace_indentation_with_spaces) {
		WebKitDOMNode *child;
		gchar *spaces;

		spaces = g_strnfill (4 * get_indentation_level (blockquote), ' ');

		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (blockquote));
		while (child) {
			/* If next sibling is indented blockqoute skip it,
			 * it will be processed afterwards */
			if (WEBKIT_DOM_IS_ELEMENT (child) &&
			    element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-indented"))
				child = webkit_dom_node_get_next_sibling (child);

			if (WEBKIT_DOM_IS_TEXT (child)) {
				gchar *text_content;
				gchar *indented_text;

				text_content = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (child));
				indented_text = g_strconcat (spaces, text_content, NULL);

				webkit_dom_text_replace_whole_text (
					WEBKIT_DOM_TEXT (child),
					indented_text,
					NULL);

				g_free (text_content);
				g_free (indented_text);
			}

			if (!child)
				break;

			/* Move to next node */
			if (webkit_dom_node_has_child_nodes (child))
				child = webkit_dom_node_get_first_child (child);
			else if (webkit_dom_node_get_next_sibling (child))
				child = webkit_dom_node_get_next_sibling (child);
			else {
				if (webkit_dom_node_is_equal_node (WEBKIT_DOM_NODE (blockquote), child))
					break;

				child = webkit_dom_node_get_parent_node (child);
				if (child)
					child = webkit_dom_node_get_next_sibling (child);
			}
		}
		g_free (spaces);

		webkit_dom_element_remove_attribute (blockquote, "style");
	}
}

/* Taken from GtkHTML */
static gchar *
get_alpha_value (gint value,
                 gboolean lower)
{
	GString *str;
	gchar *rv;
	gint add = lower ? 'a' : 'A';

	str = g_string_new (". ");

	do {
		g_string_prepend_c (str, ((value - 1) % 26) + add);
		value = (value - 1) / 26;
	} while (value);

	rv = str->str;
	g_string_free (str, FALSE);

	return rv;
}

/* Taken from GtkHTML */
static gchar *
get_roman_value (gint value,
                 gboolean lower)
{
	GString *str;
	const gchar *base = "IVXLCDM";
	gchar *rv;
	gint b, r, add = lower ? 'a' - 'A' : 0;

	if (value > 3999)
		return g_strdup ("?. ");

	str = g_string_new (". ");

	for (b = 0; value > 0 && b < 7 - 1; b += 2, value /= 10) {
		r = value % 10;
		if (r != 0) {
			if (r < 4) {
				for (; r; r--)
					g_string_prepend_c (str, base[b] + add);
			} else if (r == 4) {
				g_string_prepend_c (str, base[b + 1] + add);
				g_string_prepend_c (str, base[b] + add);
			} else if (r == 5) {
				g_string_prepend_c (str, base[b + 1] + add);
			} else if (r < 9) {
				for (; r > 5; r--)
					g_string_prepend_c (str, base[b] + add);
				g_string_prepend_c (str, base[b + 1] + add);
			} else if (r == 9) {
				g_string_prepend_c (str, base[b + 2] + add);
				g_string_prepend_c (str, base[b] + add);
			}
		}
	}

	rv = str->str;
	g_string_free (str, FALSE);

	return rv;
}

static void
process_list_to_plain_text (EHTMLEditorWebExtension *extension,
                            WebKitDOMElement *element,
                            gint level,
                            GString *output)
{
	EHTMLEditorSelectionBlockFormat format;
	EHTMLEditorSelectionAlignment alignment;
	gint counter = 1;
	gchar *indent_per_level = g_strnfill (SPACES_PER_LIST_LEVEL, ' ');
	WebKitDOMNode *item;
	gint word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);

	format = dom_get_list_format_from_node (
		WEBKIT_DOM_NODE (element));

	/* Process list items to plain text */
	item = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element));
	while (item) {
		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (item))
			g_string_append (output, "\n");

		if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
			gchar *space, *item_str = NULL;
			gint ii = 0;
			WebKitDOMElement *wrapped;
			GString *item_value = g_string_new ("");

			alignment = dom_get_list_alignment_from_node (
				WEBKIT_DOM_NODE (item));

			wrapped = webkit_dom_element_query_selector (
				WEBKIT_DOM_ELEMENT (item), ".-x-evo-wrap-br", NULL);
			/* Wrapped text */
			if (wrapped) {
				WebKitDOMNode *node = webkit_dom_node_get_first_child (item);
				GString *line = g_string_new ("");
				while (node) {
					if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (node) &&
					    element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br")) {
						g_string_append (line, "\n");
						/* put spaces before line characters -> wordwraplength - indentation */
						for (ii = 0; ii < level; ii++)
							g_string_append (line, indent_per_level);
						if (WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (element))
							g_string_append (line, indent_per_level);
						g_string_append (item_value, line->str);
						g_string_erase (line, 0, -1);
					} else {
						/* append text from node to line */
						gchar *text_content;
						text_content = webkit_dom_node_get_text_content (node);
						g_string_append (line, text_content);
						g_free (text_content);
					}
					node = webkit_dom_node_get_next_sibling (node);
				}

				if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT)
					g_string_append (item_value, line->str);

				if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER) {
					gchar *fill = NULL;
					gint fill_length;

					fill_length = word_wrap_length - g_utf8_strlen (line->str, -1);
				        fill_length -= ii * SPACES_PER_LIST_LEVEL;
					if (WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (element))
						fill_length += SPACES_PER_LIST_LEVEL;
					fill_length /= 2;

					if (fill_length < 0)
						fill_length = 0;

					fill = g_strnfill (fill_length, ' ');

					g_string_append (item_value, fill);
					g_string_append (item_value, line->str);
					g_free (fill);
				}

				if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT) {
					gchar *fill = NULL;
					gint fill_length;

					fill_length = word_wrap_length - g_utf8_strlen (line->str, -1);
				        fill_length -= ii * SPACES_PER_LIST_LEVEL;

					if (fill_length < 0)
						fill_length = 0;

					fill = g_strnfill (fill_length, ' ');

					g_string_append (item_value, fill);
					g_string_append (item_value, line->str);
					g_free (fill);
				}
				g_string_free (line, TRUE);
				/* that same here */
			} else {
				gchar *text_content =
					webkit_dom_node_get_text_content (item);
				g_string_append (item_value, text_content);
				g_free (text_content);
			}

			if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST) {
				space = g_strnfill (SPACES_PER_LIST_LEVEL - 2, ' ');
				item_str = g_strdup_printf (
					"%s* %s", space, item_value->str);
				g_free (space);
			}

			if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST) {
				gint length = 1, tmp = counter;

				while ((tmp = tmp / 10) > 1)
					length++;

				if (tmp == 1)
					length++;

				space = g_strnfill (SPACES_ORDERED_LIST_FIRST_LEVEL - 2 - length, ' ');
				item_str = g_strdup_printf (
					"%s%d. %s", space, counter, item_value->str);
				g_free (space);
			}

			if (format > E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST) {
				gchar *value;

				if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA)
					value = get_alpha_value (counter, FALSE);
				else
					value = get_roman_value (counter, FALSE);

				space = g_strnfill (SPACES_ORDERED_LIST_FIRST_LEVEL - strlen (value), ' ');
				item_str = g_strdup_printf (
					"%s%s%s", space, value, item_value->str);
				g_free (space);
				g_free (value);
			}

			if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT) {
				for (ii = 0; ii < level - 1; ii++) {
					g_string_append (output, indent_per_level);
				}
				if (WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (element))
					if (dom_node_find_parent_element (item, "OL"))
						g_string_append (output, indent_per_level);
				g_string_append (output, item_str);
			}

			if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT) {
				if (!wrapped) {
					gchar *fill = NULL;
					gint fill_length;

					fill_length = word_wrap_length - g_utf8_strlen (item_str, -1);
				        fill_length -= ii * SPACES_PER_LIST_LEVEL;

					if (fill_length < 0)
						fill_length = 0;

					if (g_str_has_suffix (item_str, " "))
						fill_length++;

					fill = g_strnfill (fill_length, ' ');

					g_string_append (output, fill);
					g_free (fill);
				}
				if (g_str_has_suffix (item_str, " "))
					g_string_append_len (output, item_str, g_utf8_strlen (item_str, -1) - 1);
				else
					g_string_append (output, item_str);
			}

			if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER) {
				if (!wrapped) {
					gchar *fill = NULL;
					gint fill_length = 0;

					for (ii = 0; ii < level - 1; ii++)
						g_string_append (output, indent_per_level);

					fill_length = word_wrap_length - g_utf8_strlen (item_str, -1);
				        fill_length -= ii * SPACES_PER_LIST_LEVEL;
					if (WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (element))
						fill_length += SPACES_PER_LIST_LEVEL;
					fill_length /= 2;

					if (fill_length < 0)
						fill_length = 0;

					if (g_str_has_suffix (item_str, " "))
						fill_length++;

					fill = g_strnfill (fill_length, ' ');

					g_string_append (output, fill);
					g_free (fill);
				}
				if (g_str_has_suffix (item_str, " "))
					g_string_append_len (output, item_str, g_utf8_strlen (item_str, -1) - 1);
				else
					g_string_append (output, item_str);
			}

			counter++;
			item = webkit_dom_node_get_next_sibling (item);
			if (item)
				g_string_append (output, "\n");

			g_free (item_str);
			g_string_free (item_value, TRUE);
		} else if (node_is_list (item)) {
			process_list_to_plain_text (
				extension, WEBKIT_DOM_ELEMENT (item), level + 1, output);
			item = webkit_dom_node_get_next_sibling (item);
		} else {
			item = webkit_dom_node_get_next_sibling (item);
		}
	}

	if (webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element)))
		g_string_append (output, "\n");

	g_free (indent_per_level);
}

static void
remove_base_attributes (WebKitDOMElement *element)
{
	webkit_dom_element_remove_attribute (element, "class");
	webkit_dom_element_remove_attribute (element, "id");
	webkit_dom_element_remove_attribute (element, "name");
}

static void
remove_evolution_attributes (WebKitDOMElement *element)
{
	webkit_dom_element_remove_attribute (element, "data-converted");
	webkit_dom_element_remove_attribute (element, "data-edit-as-new");
	webkit_dom_element_remove_attribute (element, "data-evo-draft");
	webkit_dom_element_remove_attribute (element, "data-inline");
	webkit_dom_element_remove_attribute (element, "data-uri");
	webkit_dom_element_remove_attribute (element, "data-message");
	webkit_dom_element_remove_attribute (element, "data-name");
	webkit_dom_element_remove_attribute (element, "data-new-message");
	webkit_dom_element_remove_attribute (element, "data-user-wrapped");
	webkit_dom_element_remove_attribute (element, "data-evo-plain-text");
	webkit_dom_element_remove_attribute (element, "spellcheck");
}

static void
process_elements (EHTMLEditorWebExtension *extension,
                  WebKitDOMNode *node,
                  gboolean changing_mode,
                  gboolean to_plain_text,
                  GString *buffer)
{
	WebKitDOMNodeList *nodes;
	gulong ii, length;
	gchar *content;
	gboolean skip_nl = FALSE;

	if (to_plain_text && !buffer)
		return;

	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node)) {
		if (changing_mode && to_plain_text) {
			WebKitDOMNamedNodeMap *attributes;
			gulong attributes_length;

			/* Copy attributes */
			g_string_append (buffer, "<html><head></head><body ");
			attributes = webkit_dom_element_get_attributes (
				WEBKIT_DOM_ELEMENT (node));
			attributes_length =
				webkit_dom_named_node_map_get_length (attributes);

			for (ii = 0; ii < attributes_length; ii++) {
				gchar *name;
				WebKitDOMNode *node =
					webkit_dom_named_node_map_item (
						attributes, ii);

				name = webkit_dom_node_get_local_name (node);
				if (g_strcmp0 (name, "bgcolor") != 0 &&
				    g_strcmp0 (name, "text") != 0 &&
				    g_strcmp0 (name, "vlink") != 0 &&
				    g_strcmp0 (name, "link") != 0) {
					gchar *value;

					value = webkit_dom_node_get_node_value (node);

					g_string_append (buffer, name);
					g_string_append (buffer, "=\"");
					g_string_append (buffer, value);
					g_string_append (buffer, "\" ");

					g_free (value);
				}
				g_free (name);
				g_object_unref (node);
			}
			g_string_append (buffer, ">");
			g_object_unref (attributes);
		}
		if (!to_plain_text)
			remove_evolution_attributes (WEBKIT_DOM_ELEMENT (node));
	}

	nodes = webkit_dom_node_get_child_nodes (node);
	length = webkit_dom_node_list_get_length (nodes);

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *child;
		gboolean skip_node = FALSE;

		child = webkit_dom_node_list_item (nodes, ii);

		if (WEBKIT_DOM_IS_TEXT (child)) {
			gchar *content, *tmp;
			GRegex *regex;

			content = webkit_dom_node_get_text_content (child);
			if (strstr (content, UNICODE_ZERO_WIDTH_SPACE)) {
				regex = g_regex_new (UNICODE_ZERO_WIDTH_SPACE, 0, 0, NULL);
				tmp = g_regex_replace (
					regex, content, -1, 0, "", 0, NULL);
				webkit_dom_node_set_text_content (child, tmp, NULL);
				g_free (tmp);
				g_free (content);
				content = webkit_dom_node_get_text_content (child);
				g_regex_unref (regex);
			}

			if (to_plain_text && !changing_mode) {
				gchar *class;
				const gchar *css_align;

				class = webkit_dom_element_get_class_name (WEBKIT_DOM_ELEMENT (node));
				if ((css_align = strstr (class, "-x-evo-align-"))) {
					gchar *align;
					gchar *content_with_align;
					gint length;
					gint word_wrap_length =
						e_html_editor_web_extension_get_word_wrap_length (extension);

					if (!g_str_has_prefix (css_align + 13, "left")) {
						if (g_str_has_prefix (css_align + 13, "center"))
							length = (word_wrap_length - g_utf8_strlen (content, -1)) / 2;
						else
							length = word_wrap_length - g_utf8_strlen (content, -1);

						if (length < 0)
							length = 0;

						if (g_str_has_suffix (content, " ")) {
							char *tmp;

							length++;
							align = g_strnfill (length, ' ');

							tmp = g_strndup (content, g_utf8_strlen (content, -1) -1);

							content_with_align = g_strconcat (
								align, tmp, NULL);
							g_free (tmp);
						} else {
							align = g_strnfill (length, ' ');

							content_with_align = g_strconcat (
								align, content, NULL);
						}

						g_free (content);
						g_free (align);
						content = content_with_align;
					}
				}

				g_free (class);
			}

			if (to_plain_text || changing_mode)
				g_string_append (buffer, content);

			g_free (content);

			goto next;
		}

		if (WEBKIT_DOM_IS_COMMENT (child) || !WEBKIT_DOM_IS_ELEMENT (child))
			goto next;

		if (element_has_class (WEBKIT_DOM_ELEMENT (child), "Apple-tab-span")) {
			if (!changing_mode) {
				if (to_plain_text) {
					content = webkit_dom_node_get_text_content (child);
					g_string_append (buffer, content);
					g_free (content);
				} else
					element_remove_class (
						WEBKIT_DOM_ELEMENT (child),
						"Applet-tab-span");
			}
			skip_node = TRUE;
			goto next;
		}

		/* Leave blockquotes as they are */
		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (child)) {
			if (changing_mode && to_plain_text) {
				content = webkit_dom_element_get_outer_html (
					WEBKIT_DOM_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
				goto next;
			} else {
				process_blockquote (WEBKIT_DOM_ELEMENT (child), FALSE);
				if (!to_plain_text)
					remove_base_attributes (WEBKIT_DOM_ELEMENT (child));
			}
		}

		if (!to_plain_text && !changing_mode) {
			gchar *class;
			const gchar *css_align;

			class = webkit_dom_element_get_class_name (WEBKIT_DOM_ELEMENT (child));
			if ((css_align = strstr (class, "-x-evo-align-"))) {
				if (!g_str_has_prefix (css_align + 13, "left")) {
					if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (child))
						webkit_dom_element_set_attribute (
							WEBKIT_DOM_ELEMENT (child),
							"style",
							g_str_has_prefix (css_align + 13, "center") ?
								"list-style-position: inside; text-align: center" :
								"list-style-position: inside; text-align: right",
							NULL);
					else
						webkit_dom_element_set_attribute (
							WEBKIT_DOM_ELEMENT (child),
							"style",
							g_str_has_prefix (css_align + 13, "center") ?
								"text-align: center" :
								"text-align: right",
							NULL);
				}
			}
			element_remove_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-align-left");
			element_remove_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-align-center");
			element_remove_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-align-right");
			g_free (class);
		}

		if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (child) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-indented")) {
			if (!to_plain_text && !changing_mode) {
				process_blockquote (WEBKIT_DOM_ELEMENT (child), FALSE);
				element_remove_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-indented");
			} else
				process_blockquote (WEBKIT_DOM_ELEMENT (child), TRUE);

		}

		if (node_is_list (child)) {
			if (to_plain_text) {
				if (changing_mode) {
					content = webkit_dom_element_get_outer_html (
						WEBKIT_DOM_ELEMENT (child));
					g_string_append (buffer, content);
					g_free (content);
				} else {
					process_list_to_plain_text (
						extension, WEBKIT_DOM_ELEMENT (child), 1, buffer);
				}
				skip_node = TRUE;
				goto next;
			}
		}

		if (element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-resizable-wrapper") &&
		    !element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-smiley-wrapper")) {
			WebKitDOMNode *image =
				webkit_dom_node_get_first_child (child);

			if (!to_plain_text && WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (image)) {
				remove_evolution_attributes (
					WEBKIT_DOM_ELEMENT (image));

				webkit_dom_node_replace_child (
					node, image, child, NULL);
			}

			skip_node = TRUE;
			goto next;
		}

		/* Leave paragraphs as they are */
		if (element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-paragraph")) {
			if (changing_mode && to_plain_text) {
				content = webkit_dom_element_get_outer_html (
					WEBKIT_DOM_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
				goto next;
			}
			if (!to_plain_text) {
				remove_base_attributes (WEBKIT_DOM_ELEMENT (child));
				remove_evolution_attributes (WEBKIT_DOM_ELEMENT (child));
			}
		}

		/* Signature */
		if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (child) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-signature-wrapper")) {
			WebKitDOMNode *first_child;

			first_child = webkit_dom_node_get_first_child (child);

			if (!to_plain_text) {
				remove_base_attributes (
					WEBKIT_DOM_ELEMENT (first_child));
				remove_evolution_attributes (
					WEBKIT_DOM_ELEMENT (first_child));
			}
			if (to_plain_text && !changing_mode) {
				WebKitDOMDocument *document;
				WebKitDOMNodeList *list_pre;
				gint jj, pre_count;

				g_string_append (buffer, "\n");

				/* If the user edited the signature or added more
				 * content after it, WebKit just duplicated the DOM
				 * structure and left us with multiple PRE elements
				 * that don't have the BR elements on their ends.
				 * The content is rendered fine (every pre has its
				 * own line), but when we below try to get a plain text
				 * version of the signature we will get the text from
				 * these PRE elements on one line. As a solution we need
				 * to insert the BR elements on the end of each PRE
				 * element (if not presented) to get the correct text
				 * from signature. */
				document = webkit_dom_node_get_owner_document (child);
				list_pre = webkit_dom_element_query_selector_all (
					WEBKIT_DOM_ELEMENT (first_child), "pre", NULL);
				pre_count = webkit_dom_node_list_get_length (list_pre);
				for (jj = 0; jj < pre_count; jj++) {
					WebKitDOMNode *last_pre_child, *pre_node;

					pre_node = webkit_dom_node_list_item (list_pre, jj);
					last_pre_child = webkit_dom_node_get_last_child (pre_node);

					if (last_pre_child && !WEBKIT_DOM_IS_HTML_BR_ELEMENT (last_pre_child)) {
						WebKitDOMElement *br;

						br = webkit_dom_document_create_element (document, "br", NULL);
						webkit_dom_node_append_child (
							pre_node, WEBKIT_DOM_NODE (br), NULL);
					}
					g_object_unref (pre_node);
				}
				g_object_unref (list_pre);

				content = webkit_dom_html_element_get_inner_text (
					WEBKIT_DOM_HTML_ELEMENT (first_child));
				g_string_append (buffer, content);
				g_free (content);
				skip_nl = TRUE;
			}
			if (to_plain_text && changing_mode) {
				content = webkit_dom_element_get_outer_html (
					WEBKIT_DOM_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
			}
			skip_node = TRUE;
			goto next;
		}

		/* Replace smileys with their text representation */
		if (element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-smiley-wrapper")) {
			if (to_plain_text && !changing_mode) {
				WebKitDOMNode *text_version;

				text_version = webkit_dom_node_get_last_child (child);
				content = webkit_dom_html_element_get_inner_text (
					WEBKIT_DOM_HTML_ELEMENT (text_version));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
				goto next;
			}
			if (!to_plain_text) {
				WebKitDOMElement *img;

				img = WEBKIT_DOM_ELEMENT (
					webkit_dom_node_get_first_child (child)),

				remove_evolution_attributes (img);
				remove_base_attributes (img);

				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (child),
					WEBKIT_DOM_NODE (img),
					child,
					NULL);
				remove_node (child);
				skip_node = TRUE;
				goto next;
			}
		}

		/* Leave PRE elements untouched */
		if (WEBKIT_DOM_IS_HTML_PRE_ELEMENT (child)) {
			if (changing_mode && to_plain_text) {
				content = webkit_dom_element_get_outer_html (
					WEBKIT_DOM_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
			}
			if (!to_plain_text)
				remove_evolution_attributes (WEBKIT_DOM_ELEMENT (child));
		}

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (child)) {
			if (to_plain_text) {
				if (element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-wrap-br")) {
					g_string_append (buffer, changing_mode ? "<br>" : "\n");
					goto next;
				}

				/* Insert new line when we hit the BR element that is
				 * not the last element in the block */
				if (!webkit_dom_node_is_same_node (
					child, webkit_dom_node_get_last_child (node))) {
					g_string_append (buffer, changing_mode ? "<br>" : "\n");
				} else {
					/* In citations in the empty lines the BR element
					 * is on the end and we have to put NL there */
					WebKitDOMNode *parent;

					parent = webkit_dom_node_get_parent_node (child);
					if (webkit_dom_node_get_next_sibling (parent)) {
						parent = webkit_dom_node_get_parent_node (parent);

						if (dom_node_is_citation_node (parent))
							g_string_append (buffer, changing_mode ? "<br>" : "\n");
					}
				}
			}
		}

		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (child)) {
			if (changing_mode && to_plain_text) {
				content = webkit_dom_element_get_outer_html (
					WEBKIT_DOM_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
			}
			if (!changing_mode) {
				if (to_plain_text) {
					content = webkit_dom_html_element_get_inner_text (
						WEBKIT_DOM_HTML_ELEMENT (child));
					g_string_append (buffer, content);
					g_free (content);
					skip_node = TRUE;
				} else
					remove_base_attributes (WEBKIT_DOM_ELEMENT (child));
			}
		}
 next:
		if (webkit_dom_node_has_child_nodes (child) && !skip_node)
			process_elements (
				extension, child, changing_mode, to_plain_text, buffer);
		g_object_unref (child);
	}

	if (to_plain_text && (
	    WEBKIT_DOM_IS_HTML_DIV_ELEMENT (node) ||
	    WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT (node) ||
	    WEBKIT_DOM_IS_HTML_PRE_ELEMENT (node) ||
	    WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (node))) {

		gboolean add_br = TRUE;
		WebKitDOMNode *next_sibling = webkit_dom_node_get_next_sibling (node);
		WebKitDOMNode *last_child = webkit_dom_node_get_last_child (node);

		if (last_child && WEBKIT_DOM_IS_HTML_BR_ELEMENT (last_child))
			if (webkit_dom_node_get_previous_sibling (last_child))
				add_br = FALSE;

		/* If we don't have next sibling (last element in body) or next element is
		 * signature we are not adding the BR element */
		if (!next_sibling)
			add_br = FALSE;
		else if (next_sibling && WEBKIT_DOM_IS_HTML_DIV_ELEMENT (next_sibling)) {
			if (webkit_dom_element_query_selector (
				WEBKIT_DOM_ELEMENT (next_sibling),
				"span.-x-evo-signature", NULL)) {

				add_br = FALSE;
			}
		}

		if (add_br && !skip_nl)
			g_string_append (buffer, changing_mode ? "<br>" : "\n");
	}

	g_object_unref (nodes);
}

static void
remove_image_attributes_from_element (WebKitDOMElement *element)
{
	webkit_dom_element_remove_attribute (element, "background");
	webkit_dom_element_remove_attribute (element, "data-uri");
	webkit_dom_element_remove_attribute (element, "data-inline");
	webkit_dom_element_remove_attribute (element, "data-name");
}

static void
remove_background_images_in_document (WebKitDOMDocument *document)
{
	gint length, ii;
	WebKitDOMNodeList *elements;

	elements = webkit_dom_document_query_selector_all (
		document, "[background][data-inline]", NULL);

	length = webkit_dom_node_list_get_length (elements);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMElement *element = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_list_item (elements, ii));

		remove_image_attributes_from_element (element);
		g_object_unref (element);
	}

	g_object_unref (elements);
}

static void
remove_images_in_element (WebKitDOMElement *element)
{
	gint length, ii;
	WebKitDOMNodeList *images;

	images = webkit_dom_element_query_selector_all (
		element, "img:not(.-x-evo-smiley-img)", NULL);

	length = webkit_dom_node_list_get_length (images);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (images, ii);
		remove_node (node);
		g_object_unref (node);
	}

	g_object_unref (images);
}

static void
remove_images (WebKitDOMDocument *document)
{
	remove_images_in_element (
		WEBKIT_DOM_ELEMENT (webkit_dom_document_get_body (document)));
}

static void
toggle_smileys (WebKitDOMDocument *document,
                EHTMLEditorWebExtension *extension)
{
	gboolean html_mode;
	gint length;
	gint ii;
	WebKitDOMNodeList *smileys;

	html_mode = e_html_editor_web_extension_get_html_mode (extension);

	smileys = webkit_dom_document_query_selector_all (
		document, "img.-x-evo-smiley-img", NULL);

	length = webkit_dom_node_list_get_length (smileys);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *img = webkit_dom_node_list_item (smileys, ii);
		WebKitDOMNode *text = webkit_dom_node_get_next_sibling (img);
		WebKitDOMElement *parent = webkit_dom_node_get_parent_element (img);

		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (html_mode ? text : img),
			"style",
			"display: none",
			NULL);

		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (html_mode ? img : text), "style");

		if (html_mode)
			element_add_class (parent, "-x-evo-resizable-wrapper");
		else
			element_remove_class (parent, "-x-evo-resizable-wrapper");
		g_object_unref (img);
	}

	g_object_unref (smileys);
}

static void
toggle_paragraphs_style_in_element (WebKitDOMDocument *document,
                                    EHTMLEditorWebExtension *extension,
                                    WebKitDOMElement *element,
				    gboolean html_mode)
{
	gint ii, length;
	WebKitDOMNodeList *paragraphs;

	paragraphs = webkit_dom_element_query_selector_all (
		element, ":not(td) > .-x-evo-paragraph", NULL);

	length = webkit_dom_node_list_get_length (paragraphs);

	for (ii = 0; ii < length; ii++) {
		gchar *style;
		const gchar *css_align;
		WebKitDOMNode *node = webkit_dom_node_list_item (paragraphs, ii);

		if (html_mode) {
			style = webkit_dom_element_get_attribute (
				WEBKIT_DOM_ELEMENT (node), "style");

			if ((css_align = strstr (style, "text-align: "))) {
				webkit_dom_element_set_attribute (
					WEBKIT_DOM_ELEMENT (node),
					"style",
					g_str_has_prefix (css_align + 12, "center") ?
						"text-align: center" :
						"text-align: right",
					NULL);
			} else {
				/* In HTML mode the paragraphs don't have width limit */
				webkit_dom_element_remove_attribute (
					WEBKIT_DOM_ELEMENT (node), "style");
			}
			g_free (style);
		} else {
			WebKitDOMNode *parent;

			parent = webkit_dom_node_get_parent_node (node);
			/* If the paragraph is inside indented paragraph don't set
			 * the style as it will be inherited */
			if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent) && node_is_list (node)) {
				gint offset;

				offset = WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (node) ?
					SPACES_PER_LIST_LEVEL : SPACES_ORDERED_LIST_FIRST_LEVEL;
				/* In plain text mode the paragraphs have width limit */
				dom_set_paragraph_style (
					document, extension, WEBKIT_DOM_ELEMENT (node), -1, -offset, "");
			} else if (!element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-indented")) {
				const gchar *style_to_add = "";
				style = webkit_dom_element_get_attribute (
					WEBKIT_DOM_ELEMENT (node), "style");

				if ((css_align = strstr (style, "text-align: "))) {
					style_to_add = g_str_has_prefix (
						css_align + 12, "center") ?
							"text-align: center;" :
							"text-align: right;";
				}

				/* In plain text mode the paragraphs have width limit */
				dom_set_paragraph_style (
					document, extension, WEBKIT_DOM_ELEMENT (node), -1, 0, style_to_add);

				g_free (style);
			}
		}
		g_object_unref (node);
	}
	g_object_unref (paragraphs);
}

static void
toggle_paragraphs_style (WebKitDOMDocument *document,
                         EHTMLEditorWebExtension *extension)
{
	toggle_paragraphs_style_in_element (
		document,
		extension,
		WEBKIT_DOM_ELEMENT (webkit_dom_document_get_body (document)),
		e_html_editor_web_extension_get_html_mode (extension));
}

gchar *
dom_process_content_for_draft (WebKitDOMDocument *document)
{
	gchar *content;
	gint ii, length;
	WebKitDOMHTMLElement *body;
	WebKitDOMElement *document_element;
	WebKitDOMNodeList *list;
	WebKitDOMNode *document_element_clone;

	body = webkit_dom_document_get_body (document);

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-evo-draft", "", NULL);

	document_element = webkit_dom_document_get_document_element (document);

	document_element_clone = webkit_dom_node_clone_node (
		WEBKIT_DOM_NODE (document_element), TRUE);

	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (document_element_clone), "a.-x-evo-visited-link", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *anchor;

		anchor = webkit_dom_node_list_item (list, ii);
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (anchor), "class");
		g_object_unref (anchor);
	}
	g_object_unref (list);

	content = webkit_dom_element_get_outer_html (WEBKIT_DOM_ELEMENT (document_element_clone));

	webkit_dom_element_remove_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-evo-draft");

	return content;
}

static gchar *
process_content_for_mode_change (WebKitDOMDocument *document,
                                 EHTMLEditorWebExtension *extension)
{
	WebKitDOMNode *body;
	GString *plain_text;

	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));

	plain_text = g_string_sized_new (1024);

	webkit_dom_element_remove_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-user-colors");

	process_elements (extension, body, TRUE, TRUE, plain_text);

	g_string_append (plain_text, "</body></html>");

	return g_string_free (plain_text, FALSE);
}

static void
convert_element_from_html_to_plain_text (WebKitDOMDocument *document,
                                         EHTMLEditorWebExtension *extension,
                                         WebKitDOMElement *element,
                                         gboolean *wrap,
                                         gboolean *quote)
{
	gint blockquotes_count;
	gchar *inner_text, *inner_html;
	gboolean restore = TRUE;
	WebKitDOMElement *top_signature, *signature, *blockquote, *main_blockquote;
	WebKitDOMNode *signature_clone, *from;

	top_signature = webkit_dom_element_query_selector (
		element, ".-x-evo-top-signature", NULL);
	signature = webkit_dom_element_query_selector (
		element, "span.-x-evo-signature", NULL);
	main_blockquote = webkit_dom_element_query_selector (
		element, "#-x-evo-main-cite", NULL);

	blockquote = webkit_dom_document_create_element (
		document, "blockquote", NULL);

	if (main_blockquote) {
		WebKitDOMElement *input_start;

		webkit_dom_element_set_attribute (
			blockquote, "type", "cite", NULL);

		input_start = webkit_dom_element_query_selector (
			element, "#-x-evo-input-start", NULL);

		restore = input_start ? TRUE : FALSE;

		if (input_start)
			dom_add_selection_markers_into_element_start (
				document, WEBKIT_DOM_ELEMENT (input_start), NULL, NULL);
		from = WEBKIT_DOM_NODE (main_blockquote);
	} else {
		if (signature) {
			WebKitDOMNode *parent = webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (signature));
			signature_clone = webkit_dom_node_clone_node (parent, TRUE);
			remove_node (parent);
		}
		from = WEBKIT_DOM_NODE (element);
	}

	blockquotes_count = create_text_markers_for_citations_in_element (
		WEBKIT_DOM_ELEMENT (from));

	inner_text = webkit_dom_html_element_get_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (from));

	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (blockquote), inner_text, NULL);

	inner_html = webkit_dom_element_get_inner_html (blockquote);

	parse_html_into_blocks (
		document,
		extension,
		main_blockquote ? blockquote : WEBKIT_DOM_ELEMENT (element),
		NULL,
		inner_html);

	if (main_blockquote) {
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (main_blockquote)),
			WEBKIT_DOM_NODE (blockquote),
			WEBKIT_DOM_NODE (main_blockquote),
			NULL);

		remove_evolution_attributes (WEBKIT_DOM_ELEMENT (element));
	} else {
		WebKitDOMNode *first_child;

		if (signature) {
			if (!top_signature) {
				signature_clone = webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (element),
					signature_clone,
					NULL);
			} else {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (element),
					signature_clone,
					webkit_dom_node_get_first_child (
						WEBKIT_DOM_NODE (element)),
					NULL);
			}
		}

		first_child = webkit_dom_node_get_first_child (
			WEBKIT_DOM_NODE (element));
		if (first_child) {
			if (!webkit_dom_node_has_child_nodes (first_child)) {
				webkit_dom_element_set_inner_html (
					WEBKIT_DOM_ELEMENT (first_child),
					"<br>",
					NULL);
			}
			dom_add_selection_markers_into_element_start (
				document, WEBKIT_DOM_ELEMENT (first_child), NULL, NULL);
		}
	}

	*wrap = TRUE;
	*quote = main_blockquote || blockquotes_count > 0;

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-converted", "", NULL);

	g_free (inner_text);
	g_free (inner_html);

	if (restore)
		dom_selection_restore (document);
}

gchar *
dom_process_content_for_plain_text (WebKitDOMDocument *document,
                                    EHTMLEditorWebExtension *extension)
{
	gboolean wrap = FALSE, quote = FALSE, clean = FALSE;
	gboolean converted, is_from_new_message;
	gint length, ii;
	GString *plain_text;
	WebKitDOMNode *body, *source;
	WebKitDOMNodeList *paragraphs;

	plain_text = g_string_sized_new (1024);

	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));
	converted = webkit_dom_element_has_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-converted");
	is_from_new_message = webkit_dom_element_has_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-new-message");
	source = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (body), TRUE);

	/* If composer is in HTML mode we have to move the content to plain version */
	if (e_html_editor_web_extension_get_html_mode (extension)) {
		if (converted || is_from_new_message) {
			toggle_paragraphs_style_in_element (
				document, extension, WEBKIT_DOM_ELEMENT (source), FALSE);
			remove_images_in_element (
				WEBKIT_DOM_ELEMENT (source));
			remove_background_images_in_document (document);
		} else {
			gchar *inner_html;
			WebKitDOMElement *div;

			inner_html = webkit_dom_element_get_inner_html (
				WEBKIT_DOM_ELEMENT (body));

			div = webkit_dom_document_create_element (
				document, "div", NULL);

			webkit_dom_element_set_inner_html (div, inner_html, NULL);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (body),
				WEBKIT_DOM_NODE (div),
				NULL);

			paragraphs = webkit_dom_element_query_selector_all (
				div, "#-x-evo-input-start", NULL);

			length = webkit_dom_node_list_get_length (paragraphs);
			for (ii = 0; ii < length; ii++) {
				WebKitDOMNode *paragraph;

				paragraph = webkit_dom_node_list_item (paragraphs, ii);

				webkit_dom_element_remove_attribute (
					WEBKIT_DOM_ELEMENT (paragraph), "id");
			}
			g_object_unref (paragraphs);

			convert_element_from_html_to_plain_text (
				document, extension, div, &wrap, &quote);

			g_object_unref (source);

			source = WEBKIT_DOM_NODE (div);

			clean = TRUE;
		}
	}

	paragraphs = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (source), ".-x-evo-paragraph", NULL);

	length = webkit_dom_node_list_get_length (paragraphs);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *paragraph;

		paragraph = webkit_dom_node_list_item (paragraphs, ii);

		if (node_is_list (paragraph)) {
			WebKitDOMNode *item = webkit_dom_node_get_first_child (paragraph);

			while (item) {
				WebKitDOMNode *next_item =
					webkit_dom_node_get_next_sibling (item);

				if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
					dom_wrap_paragraph (
						document, extension, WEBKIT_DOM_ELEMENT (item));
				}
				item = next_item;
			}
		} else if (!webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (paragraph), ".-x-evo-wrap-br,.-x-evo-quoted", NULL)) {
			/* Dont't try to wrap the already wrapped content. */
			dom_wrap_paragraph (
				document, extension, WEBKIT_DOM_ELEMENT (paragraph));
		}
		g_object_unref (paragraph);
	}
	g_object_unref (paragraphs);

	paragraphs = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (source), "span[id^=\"-x-evo-selection-\"]", NULL);

	length = webkit_dom_node_list_get_length (paragraphs);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (paragraphs, ii);
		WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);

		remove_node (node);
		g_object_unref (node);
		webkit_dom_node_normalize (parent);
	}
	g_object_unref (paragraphs);

	if (quote)
		quote_plain_text_recursive (document, source, source, 0);
	else if (e_html_editor_web_extension_get_html_mode (extension)) {
		WebKitDOMElement *citation;

		citation = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (source), "blockquote[type=cite]", NULL);
		if (citation)
			quote_plain_text_recursive (document, source, source, 0);
	}

	process_elements (extension, source, FALSE, TRUE, plain_text);

	if (clean)
		remove_node (source);
	else
		g_object_unref (source);

	/* Return text content between <body> and </body> */
	return g_string_free (plain_text, FALSE);
}

static void
restore_image (WebKitDOMDocument *document,
               const gchar *id,
               const gchar *element_src)
{
	gchar *selector;
	gint length, ii;
	WebKitDOMNodeList *list;

	selector = g_strconcat ("[data-inline][background=\"cid:", id, "\"]", NULL);
	list = webkit_dom_document_query_selector_all (document, selector, NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMElement *element = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_list_item (list, ii));

		webkit_dom_element_set_attribute (element, "background", element_src, NULL);
		g_object_unref (element);
	}
	g_free (selector);
	g_object_unref (list);

	selector = g_strconcat ("[data-inline][src=\"cid:", id, "\"]", NULL);
	list = webkit_dom_document_query_selector_all (document, selector, NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMElement *element = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_list_item (list, ii));

		webkit_dom_element_set_attribute (element, "src", element_src, NULL);
		g_object_unref (element);
	}
	g_free (selector);
	g_object_unref (list);
}

static void
dom_restore_images (WebKitDOMDocument *document,
                    GVariant *inline_images_to_restore)
{
	const gchar *element_src, *name, *id;
	GVariantIter *iter;

	g_variant_get (inline_images_to_restore, "asss", &iter);
	while (g_variant_iter_loop (iter, "&s&s&s", &element_src, &name, &id))
		restore_image (document, id, element_src);
}

gchar *
dom_process_content_for_html (WebKitDOMDocument *document,
                              EHTMLEditorWebExtension *extension,
                              const gchar *from_domain)
{
	gint ii, length;
	GVariant *inline_images_to_restore = NULL;
	gchar *html_content;
	WebKitDOMElement *marker;
	WebKitDOMNode *node, *document_clone;
	WebKitDOMNodeList *list;
	GSettings *settings;
	gboolean send_editor_colors = FALSE;

	if (from_domain != NULL)
		inline_images_to_restore = dom_get_inline_images_data (document, extension, from_domain);

	document_clone = webkit_dom_node_clone_node (
		WEBKIT_DOM_NODE (webkit_dom_document_get_document_element (document)), TRUE);
	node = WEBKIT_DOM_NODE (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-quote-style", NULL));
	if (node)
		remove_node (node);
	node = WEBKIT_DOM_NODE (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-a-color-style", NULL));
	if (node)
		remove_node (node);
	node = WEBKIT_DOM_NODE (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-a-color-style-visited", NULL));
	if (node)
		remove_node (node);
	/* When the Ctrl + Enter is pressed for sending, the links are activated. */
	node = WEBKIT_DOM_NODE (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-style-a", NULL));
	if (node)
		remove_node (node);
	node = WEBKIT_DOM_NODE (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "body", NULL));
	marker = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (node), "#-x-evo-selection-start-marker", NULL);
	if (marker)
		remove_node (WEBKIT_DOM_NODE (marker));
	marker = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (node), "#-x-evo-selection-end-marker", NULL);
	if (marker)
		remove_node (WEBKIT_DOM_NODE (marker));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	send_editor_colors = g_settings_get_boolean (settings, "composer-inherit-theme-colors");
	g_object_unref (settings);

	if (webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (node), "data-user-colors")) {
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "data-user-colors");
	} else if (!send_editor_colors) {
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "bgcolor");
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "text");
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "link");
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "vlink");
	}

	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (node), "span[data-hidden-space]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *hidden_space_node;

		hidden_space_node = webkit_dom_node_list_item (list, ii);
		remove_node (hidden_space_node);
		g_object_unref (hidden_space_node);
	}
	g_object_unref (list);

	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (node), "[data-style]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *data_style_node;

		data_style_node = webkit_dom_node_list_item (list, ii);

		rename_attribute (WEBKIT_DOM_ELEMENT (data_style_node), "data-style", "style");
		g_object_unref (data_style_node);
	}
	g_object_unref (list);

	process_elements (extension, node, FALSE, FALSE, NULL);

	html_content = webkit_dom_element_get_outer_html (
		WEBKIT_DOM_ELEMENT (document_clone));

	g_object_unref (document_clone);

	if (from_domain && inline_images_to_restore) {
		dom_restore_images (document, inline_images_to_restore);
		g_object_unref (inline_images_to_restore);
	}

	return html_content;
}

void
dom_convert_when_changing_composer_mode (WebKitDOMDocument *document,
                                         EHTMLEditorWebExtension *extension)
{
	gboolean quote = FALSE, wrap = FALSE;
	WebKitDOMHTMLElement *body;

	body = webkit_dom_document_get_body (document);

	convert_element_from_html_to_plain_text (
		document, extension, WEBKIT_DOM_ELEMENT (body), &wrap, &quote);

	if (wrap)
		dom_wrap_paragraphs_in_document (document, extension);

	if (quote) {
		dom_selection_save (document);
		if (wrap)
			quote_plain_text_elements_after_wrapping_in_document (
				document);
		else
			body = WEBKIT_DOM_HTML_ELEMENT (dom_quote_plain_text (document));
		dom_selection_restore (document);
	}

	toggle_paragraphs_style (document, extension);
	toggle_smileys (document, extension);
	remove_images (document);
	remove_background_images_in_document (document);

	clear_attributes (document);

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-converted", "", NULL);

	dom_force_spell_check_in_viewport (document, extension);
	dom_scroll_to_caret (document);
}

static void
wrap_paragraphs_in_quoted_content (WebKitDOMDocument *document,
                                   EHTMLEditorWebExtension *extension)
{
	gint ii, length;
	WebKitDOMNodeList *paragraphs;

	paragraphs = webkit_dom_document_query_selector_all (
		document, "blockquote[type=cite] > .-x-evo-paragraph", NULL);

	length = webkit_dom_node_list_get_length (paragraphs);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *paragraph;

		paragraph = webkit_dom_node_list_item (paragraphs, ii);

		dom_wrap_paragraph (document, extension, WEBKIT_DOM_ELEMENT (paragraph));

		g_object_unref (paragraph);
	}
	g_object_unref (paragraphs);
}

static void
set_base64_to_element_attribute (GHashTable *inline_images,
                                 WebKitDOMElement *element,
                                 const gchar *attribute)
{
	gchar *attribute_value;
	const gchar *base64_src;

	attribute_value = webkit_dom_element_get_attribute (element, attribute);

	if (attribute_value && (base64_src = g_hash_table_lookup (inline_images, attribute_value)) != NULL) {
		const gchar *base64_data = strstr (base64_src, ";") + 1;
		gchar *name;
		glong name_length;

		name_length =
			g_utf8_strlen (base64_src, -1) -
			g_utf8_strlen (base64_data, -1) - 1;
		name = g_strndup (base64_src, name_length);

		webkit_dom_element_set_attribute (element, "data-inline", "", NULL);
		webkit_dom_element_set_attribute (element, "data-name", name, NULL);
		webkit_dom_element_set_attribute (element, attribute, base64_data, NULL);

		g_free (name);
	}
	g_free (attribute_value);
}

static void
change_cid_images_src_to_base64 (WebKitDOMDocument *document,
                                 EHTMLEditorWebExtension *extension)
{
	GHashTable *inline_images;
	gint ii, length;
	WebKitDOMElement *document_element;
	WebKitDOMNamedNodeMap *attributes;
	WebKitDOMNodeList *list;

	inline_images = e_html_editor_web_extension_get_inline_images (extension);

	document_element = webkit_dom_document_get_document_element (document);

	list = webkit_dom_document_query_selector_all (document, "img[src^=\"cid:\"]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		set_base64_to_element_attribute (inline_images, WEBKIT_DOM_ELEMENT (node), "src");
		g_object_unref (node);
	}
	g_object_unref (list);

	/* Namespaces */
	attributes = webkit_dom_element_get_attributes (document_element);
	length = webkit_dom_named_node_map_get_length (attributes);
	for (ii = 0; ii < length; ii++) {
		gchar *name;
		WebKitDOMNode *node = webkit_dom_named_node_map_item (attributes, ii);

		name = webkit_dom_node_get_local_name (node);

		if (g_str_has_prefix (name, "xmlns:")) {
			const gchar *ns = name + 6;
			gchar *attribute_ns = g_strconcat (ns, ":src", NULL);
			gchar *selector = g_strconcat ("img[", ns, "\\:src^=\"cid:\"]", NULL);
			gint ns_length, jj;

			list = webkit_dom_document_query_selector_all (
				document, selector, NULL);
			ns_length = webkit_dom_node_list_get_length (list);
			for (jj = 0; jj < ns_length; jj++) {
				WebKitDOMNode *node = webkit_dom_node_list_item (list, jj);

				set_base64_to_element_attribute (
					inline_images, WEBKIT_DOM_ELEMENT (node), attribute_ns);
				g_object_unref (node);
			}

			g_object_unref (list);
			g_free (attribute_ns);
			g_free (selector);
		}
		g_object_unref (node);
		g_free (name);
	}
	g_object_unref (attributes);

	list = webkit_dom_document_query_selector_all (
		document, "[background^=\"cid:\"]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		set_base64_to_element_attribute (
			inline_images, WEBKIT_DOM_ELEMENT (node), "background");
		g_object_unref (node);
	}
	g_object_unref (list);
	g_hash_table_remove_all (inline_images);
}

void
dom_process_content_after_load (WebKitDOMDocument *document,
                                EHTMLEditorWebExtension *extension)
{
	WebKitDOMHTMLElement *body;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMNodeList *list;
	gint ii, length;

	/* Don't use CSS when possible to preserve compatibility with older
	 * versions of Evolution or other MUAs */
	dom_exec_command (
		document, extension, E_HTML_EDITOR_VIEW_COMMAND_STYLE_WITH_CSS, "false");

	body = webkit_dom_document_get_body (document);

	webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (body), "style");
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-message", "", NULL);

	/* Workaround for https://bugzilla.gnome.org/show_bug.cgi?id=752997 where
	* WebKit (2.4.9) crashes when it is trying to move or remove an anchor
	* element that has an image element inside and this image element has
	* the CSS float property set in the style attribute. To workaround it we
	* will rename the style attribute and rename it back when we will send
	* the message. It is unfortunate that we can change the formatting with
	* this, but this is definitely better than crashing. This could be
	* removed once Evolution switches to WebKit2 as the WebKit2 is unaffected
	* (tested on 2.8.4). */
	list = webkit_dom_document_query_selector_all (document, "a img[style]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;
		gchar *style_value;

		node = webkit_dom_node_list_item (list, ii);
		style_value = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "style");
		if (camel_strstrcase (style_value, "float"))
			rename_attribute (WEBKIT_DOM_ELEMENT (node), "style", "data-style");
		g_free (style_value);

		g_object_unref (node);
	}
	g_object_unref (list);

	if (e_html_editor_web_extension_get_convert_in_situ (extension)) {
		dom_convert_content (document, extension, NULL);
		/* Make the quote marks non-selectable. */
		dom_disable_quote_marks_select (document);
		dom_set_links_active (document, FALSE);
		e_html_editor_web_extension_set_convert_in_situ (extension, FALSE);

		dom_register_input_event_listener_on_body (document, extension);
		register_html_events_handlers (body, extension);

		return;
	}

	/* Make the quote marks non-selectable. */
	dom_disable_quote_marks_select (document);
	dom_set_links_active (document, FALSE);
	put_body_in_citation (document);
	move_elements_to_body (document, extension);
	repair_gmail_blockquotes (document);

	if (webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (body), "data-evo-draft")) {
		/* Restore the selection how it was when the draft was saved */
		dom_move_caret_into_element (document, WEBKIT_DOM_ELEMENT (body), FALSE);
		dom_selection_restore (document);
		dom_remove_embed_style_sheet (document);
	}

	/* The composer body could be empty in some case (loading an empty string
	 * or empty HTML. In that case create the initial paragraph. */
	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body))) {
		WebKitDOMElement *paragraph;

		paragraph = dom_prepare_paragraph (document, extension, TRUE);
		webkit_dom_element_set_id (paragraph, "-x-evo-input-start");
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (body), WEBKIT_DOM_NODE (paragraph), NULL);
		dom_selection_restore (document);
	}

	/* Register on input event that is called when the content (body) is modified */
	dom_register_input_event_listener_on_body (document, extension);
	register_html_events_handlers (body, extension);

	if (e_html_editor_web_extension_get_html_mode (extension))
		change_cid_images_src_to_base64 (document, extension);

	if (e_html_editor_web_extension_get_inline_spelling_enabled (extension))
		dom_force_spell_check (document, extension);
	else
		dom_turn_spell_check_off (document, extension);

	set_monospace_font_family_on_body (WEBKIT_DOM_ELEMENT (body), e_html_editor_web_extension_get_html_mode (extension));

	dom_window = webkit_dom_document_get_default_view (document);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (dom_window),
		"scroll",
		G_CALLBACK (body_scroll_event_cb),
		FALSE,
		extension);
}

GVariant *
dom_get_inline_images_data (WebKitDOMDocument *document,
                            EHTMLEditorWebExtension *extension,
                            const gchar *uid_domain)
{
	GVariant *result;
	GVariantBuilder *builder = NULL;
	GHashTable *added;
	gint length, ii;
	WebKitDOMNodeList *list;

	list = webkit_dom_document_query_selector_all (document, "img[data-inline]", NULL);

	length = webkit_dom_node_list_get_length (list);
	if (length == 0) {
		g_object_unref (list);
		goto background;
	}

	builder = g_variant_builder_new (G_VARIANT_TYPE ("asss"));

	added = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	for (ii = 0; ii < length; ii++) {
		const gchar *id;
		gchar *cid;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "src");

		if ((id = g_hash_table_lookup (added, src)) != NULL) {
			cid = g_strdup_printf ("cid:%s", id);
			g_free (src);
		} else {
			gchar *data_name = webkit_dom_element_get_attribute (
				WEBKIT_DOM_ELEMENT (node), "data-name");
			gchar *new_id;

			new_id = camel_header_msgid_generate (uid_domain);
			g_variant_builder_add (
				builder, "sss", src, data_name, new_id);
			cid = g_strdup_printf ("cid:%s", new_id);

			g_hash_table_insert (added, src, new_id);
			g_free (new_id);
		}
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (node), "src", cid, NULL);
		g_object_unref (node);
		g_free (cid);
	}
	g_object_unref (list);

 background:
	list = webkit_dom_document_query_selector_all (
		document, "[data-inline][background]", NULL);
	length = webkit_dom_node_list_get_length (list);
	if (length == 0)
		goto out;
	if (!builder)
		builder = g_variant_builder_new (G_VARIANT_TYPE ("asss"));

	for (ii = 0; ii < length; ii++) {
		const gchar *id;
		gchar *cid = NULL;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "background");

		if ((id = g_hash_table_lookup (added, src)) != NULL) {
			cid = g_strdup_printf ("cid:%s", id);
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (node), "background", cid, NULL);
			g_free (src);
		} else {
			gchar *data_name = webkit_dom_element_get_attribute (
				WEBKIT_DOM_ELEMENT (node), "data-name");
			gchar *new_id;

			new_id = camel_header_msgid_generate (uid_domain);
			g_variant_builder_add (
				builder, "sss", src, data_name, new_id);
			cid = g_strdup_printf ("cid:%s", new_id);

			g_hash_table_insert (added, src, new_id);
			g_free (new_id);

			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (node), "background", cid, NULL);
		}
		g_free (cid);
		g_object_unref (node);
	}
 out:
	g_object_unref (list);
	g_hash_table_destroy (added);

	result = g_variant_new ("asss", builder);
	g_variant_builder_unref (builder);

	return result;
}

static gboolean
pasting_quoted_content (const gchar *content)
{
	/* Check if the content we are pasting is a quoted content from composer.
	 * If it is, we can't use WebKit to paste it as it would leave the formatting
	 * on the content. */
	return g_str_has_prefix (
		content,
		"<meta http-equiv=\"content-type\" content=\"text/html; "
		"charset=utf-8\"><blockquote type=\"cite\"") &&
		strstr (content, "\"-x-evo-");
}

/**
 * e_html_editor_selection_insert_html:
 * @selection: an #EHTMLEditorSelection
 * @html_text: an HTML code to insert
 *
 * Insert @html_text into document at current cursor position. When a text range
 * is selected, it will be replaced by @html_text.
 */
void
dom_insert_html (WebKitDOMDocument *document,
                 EHTMLEditorWebExtension *extension,
                 const gchar *html_text)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	gboolean html_mode;

	g_return_if_fail (html_text != NULL);

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		gboolean collapsed;

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_INSERT_HTML;

		collapsed = dom_selection_is_collapsed (document);
		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		if (!collapsed) {
			ev->before.end.x = ev->before.start.x;
			ev->before.end.y = ev->before.start.y;
		}

		ev->data.string.from = NULL;
		ev->data.string.to = g_strdup (html_text);
	}

	html_mode = e_html_editor_web_extension_get_html_mode (extension);
	if (html_mode ||
	    (e_html_editor_web_extension_is_pasting_content_from_itself (extension) &&
	    !pasting_quoted_content (html_text))) {
		if (!dom_selection_is_collapsed (document)) {
			EHTMLEditorHistoryEvent *event;
			WebKitDOMDocumentFragment *fragment;
			WebKitDOMRange *range;

			event = g_new0 (EHTMLEditorHistoryEvent, 1);
			event->type = HISTORY_DELETE;

			range = dom_get_current_range (document);
			fragment = webkit_dom_range_clone_contents (range, NULL);
			g_object_unref (range);
			event->data.fragment = fragment;

			dom_selection_get_coordinates (
				document,
				&event->before.start.x,
				&event->before.start.y,
				&event->before.end.x,
				&event->before.end.y);

			event->after.start.x = event->before.start.x;
			event->after.start.y = event->before.start.y;
			event->after.end.x = event->before.start.x;
			event->after.end.y = event->before.start.y;

			e_html_editor_undo_redo_manager_insert_history_event (manager, event);

			event = g_new0 (EHTMLEditorHistoryEvent, 1);
			event->type = HISTORY_AND;

			e_html_editor_undo_redo_manager_insert_history_event (manager, event);
		}

		dom_exec_command (
			document, extension, E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML, html_text);
		if (strstr (html_text, "id=\"-x-evo-selection-start-marker\""))
			dom_selection_restore (document);

		if (!html_mode) {
			WebKitDOMNodeList *list;
			gint ii, length;

			list = webkit_dom_document_query_selector_all (
				document, "span[style^=font-family]", NULL);
			length = webkit_dom_node_list_get_length (list);
			if (length > 0)
				dom_selection_save (document);

			for (ii = 0; ii < length; ii++) {
				WebKitDOMNode *span, *child;

				span = webkit_dom_node_list_item (list, ii);
				while ((child = webkit_dom_node_get_first_child (span)))
					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (span),
						child,
						span,
						NULL);

				remove_node (span);
				g_object_unref (span);
			}
			g_object_unref (list);

			if (length > 0)
				dom_selection_restore (document);
		}

		dom_check_magic_links (document, extension, FALSE);
		dom_force_spell_check (document, extension);
		dom_scroll_to_caret (document);
	} else
		dom_convert_and_insert_html_into_selection (document, extension, html_text, TRUE);

	if (ev) {
		dom_selection_get_coordinates (
			document,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}
}

static void
save_history_for_delete_or_backspace (WebKitDOMDocument *document,
                                      EHTMLEditorWebExtension *extension,
                                      gboolean delete_key,
                                      gboolean control_key)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	WebKitDOMDocumentFragment *fragment = NULL;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection)) {
		g_object_unref (dom_selection);
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	if (save_history_before_event_in_table (document, extension, range)) {
		g_object_unref (range);
		g_object_unref (dom_selection);
		return;
	}

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	ev->type = HISTORY_DELETE;

	dom_selection_get_coordinates (
		document, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
	g_object_unref (range);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	if (webkit_dom_range_get_collapsed (range, NULL)) {
		gboolean removing_from_anchor = FALSE;
		WebKitDOMRange *range_clone;
		WebKitDOMNode *node, *next_block = NULL;

		e_html_editor_web_extension_block_selection_changed_callback (extension);

		range_clone = webkit_dom_range_clone_range (range, NULL);
		if (control_key) {
			WebKitDOMRange *tmp_range;

			/* Control + Delete/Backspace deletes previous/next word. */
			webkit_dom_dom_selection_modify (
				dom_selection, "move", delete_key ? "right" : "left", "word");
			tmp_range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
			if (delete_key)
				webkit_dom_range_set_end (
					range_clone,
					webkit_dom_range_get_end_container (tmp_range, NULL),
					webkit_dom_range_get_end_offset (tmp_range, NULL),
					NULL);
			else
				webkit_dom_range_set_start (
					range_clone,
					webkit_dom_range_get_start_container (tmp_range, NULL),
					webkit_dom_range_get_start_offset (tmp_range, NULL),
					NULL);
			g_object_unref (tmp_range);
		} else {
			typedef WebKitDOMNode * (*GetSibling)(WebKitDOMNode *node);
			WebKitDOMNode *container, *sibling;
			WebKitDOMElement *selection_marker;

			GetSibling get_sibling = delete_key ?
				webkit_dom_node_get_next_sibling :
				webkit_dom_node_get_previous_sibling;

			container = webkit_dom_range_get_end_container (range_clone, NULL);
			sibling = get_sibling (container);

			selection_marker = webkit_dom_document_get_element_by_id (
				document,
				delete_key ?
					"-x-evo-selection-end-marker" :
					"-x-evo-selection-start-marker");

			if (selection_marker) {
				WebKitDOMNode *tmp_sibling;

				tmp_sibling = get_sibling (WEBKIT_DOM_NODE (selection_marker));
				if (!tmp_sibling || (WEBKIT_DOM_IS_HTML_BR_ELEMENT (tmp_sibling) &&
				    !element_has_class (WEBKIT_DOM_ELEMENT (tmp_sibling), "-x-evo-wrap-br")))
					sibling = WEBKIT_DOM_NODE (selection_marker);
			}

			if (dom_is_selection_position_node (sibling)) {
				if ((node = get_sibling (sibling)))
					node = get_sibling (node);
				if (node) {
					if (WEBKIT_DOM_IS_ELEMENT (node) &&
					    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (node), "data-hidden-space")) {
						fragment = webkit_dom_document_create_document_fragment (document);
						webkit_dom_node_append_child (
							WEBKIT_DOM_NODE (fragment),
							WEBKIT_DOM_NODE (
								webkit_dom_document_create_text_node (document, " ")),
							NULL);
					} else if (delete_key) {
						webkit_dom_range_set_start (
							range_clone, node, 0, NULL);
						webkit_dom_range_set_end (
							range_clone, node, 1, NULL);
					}
				} else {
					WebKitDOMRange *tmp_range, *actual_range;

					actual_range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

					webkit_dom_dom_selection_modify (
						dom_selection, "move", delete_key ? "right" : "left", "character");

					tmp_range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
					if (webkit_dom_range_compare_boundary_points (tmp_range, 2, actual_range, NULL) != 0) {
						WebKitDOMNode *actual_block;
						WebKitDOMNode *tmp_block;

						actual_block = get_parent_block_node_from_child (container);

						tmp_block = delete_key ?
							webkit_dom_range_get_end_container (tmp_range, NULL) :
							webkit_dom_range_get_start_container (tmp_range, NULL);
						tmp_block = get_parent_block_node_from_child (tmp_block);

						webkit_dom_dom_selection_modify (
							dom_selection, "move", delete_key ? "left" : "right", "character");

						if (tmp_block) {
							fragment = webkit_dom_document_create_document_fragment (document);
							if (delete_key) {
								webkit_dom_node_append_child (
									WEBKIT_DOM_NODE (fragment),
									webkit_dom_node_clone_node (actual_block, TRUE),
									NULL);
								webkit_dom_node_append_child (
									WEBKIT_DOM_NODE (fragment),
									webkit_dom_node_clone_node (tmp_block, TRUE),
									NULL);
								if (delete_key)
									next_block = tmp_block;
							} else {
								webkit_dom_node_append_child (
									WEBKIT_DOM_NODE (fragment),
									webkit_dom_node_clone_node (tmp_block, TRUE),
									NULL);
								webkit_dom_node_append_child (
									WEBKIT_DOM_NODE (fragment),
									webkit_dom_node_clone_node (actual_block, TRUE),
									NULL);
							}
							g_object_set_data (
								G_OBJECT (fragment),
								"-x-evo-fragment",
								GINT_TO_POINTER (1));
						}
					}
					g_object_unref (tmp_range);
					g_object_unref (actual_range);
				}
			} else {
				glong offset;

				offset = webkit_dom_range_get_start_offset (range_clone, NULL);

				if (delete_key)
					webkit_dom_range_set_end (
						range_clone, container, offset + 1, NULL);
				else
					webkit_dom_range_set_start (
						range_clone, container, offset - 1, NULL);

				removing_from_anchor = WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (
					webkit_dom_node_get_parent_node (container));
			}
		}


		if (!fragment)
			fragment = webkit_dom_range_clone_contents (range_clone, NULL);
		if (removing_from_anchor)
			g_object_set_data (G_OBJECT (fragment), "-x-evo-removing-from-anchor", GINT_TO_POINTER (1));
		node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
		if (!node) {
			g_free (ev);
			e_html_editor_web_extension_unblock_selection_changed_callback (extension);
			g_object_unref (range);
			g_object_unref (range_clone);
			g_object_unref (dom_selection);
			g_warning ("History event was not saved for %s key", delete_key ? "Delete" : "Backspace");
			return;
		}

		if (control_key) {
			if (delete_key) {
				ev->after.start.x = ev->before.start.x;
				ev->after.start.y = ev->before.start.y;
				ev->after.end.x = ev->before.end.x;
				ev->after.end.y = ev->before.end.y;

				webkit_dom_range_collapse (range_clone, TRUE, NULL);
				webkit_dom_dom_selection_remove_all_ranges (dom_selection);
				webkit_dom_dom_selection_add_range (dom_selection, range_clone);
			} else {
				gboolean selection_saved = FALSE;
				WebKitDOMRange *tmp_range;

				if (webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker"))
					selection_saved = TRUE;

				if (selection_saved)
					dom_selection_restore (document);

				tmp_range = webkit_dom_range_clone_range (range_clone, NULL);
				/* Prepare the selection to the right position after
				 * delete and save it. */
				webkit_dom_range_collapse (range_clone, TRUE, NULL);
				webkit_dom_dom_selection_remove_all_ranges (dom_selection);
				webkit_dom_dom_selection_add_range (dom_selection, range_clone);
				dom_selection_get_coordinates (
					document, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
				/* Restore the selection where it was before the
				 * history event was saved. */
				webkit_dom_range_collapse (tmp_range, FALSE, NULL);
				webkit_dom_dom_selection_remove_all_ranges (dom_selection);
				webkit_dom_dom_selection_add_range (dom_selection, tmp_range);
				g_object_unref (tmp_range);

				if (selection_saved)
					dom_selection_save (document);
			}
		} else {
			gboolean selection_saved = FALSE;

			if (webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker"))
				selection_saved = TRUE;

			if (selection_saved)
				dom_selection_restore (document);

			if (delete_key) {
				dom_selection_get_coordinates (
					document, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
			} else {
				webkit_dom_dom_selection_modify (dom_selection, "move", "left", "character");
				dom_selection_get_coordinates (
					document, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
				webkit_dom_dom_selection_modify (dom_selection, "move", "right", "character");

				ev->after.end.x = ev->after.start.x;
				ev->after.end.y = ev->after.start.y;
			}

			if (selection_saved)
				dom_selection_save (document);
		}

		g_object_unref (range_clone);

		if (delete_key) {
			if (!WEBKIT_DOM_IS_ELEMENT (node)) {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						dom_create_selection_marker (document, FALSE)),
					webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
					NULL);
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						dom_create_selection_marker (document, TRUE)),
					webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
					NULL);
			}
		} else {
			if (!WEBKIT_DOM_IS_ELEMENT (node)) {
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						dom_create_selection_marker (document, TRUE)),
					NULL);
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						dom_create_selection_marker (document, FALSE)),
					NULL);
			}
		}

		/* If concatenating two blocks with pressing Delete on the end
		 * of the previous one and the next node contain content that
		 * is wrapped on multiple lines, the last line will by separated
		 * by WebKit to the separate block. To avoid it let's remove
		 * all quoting and wrapping from the next paragraph. */
		if (next_block) {
			dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (next_block));
			dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (next_block));
		}

		e_html_editor_web_extension_unblock_selection_changed_callback (extension);
	} else {
		WebKitDOMElement *tmp_element;
		WebKitDOMNode *sibling;

		ev->after.start.x = ev->before.start.x;
		ev->after.start.y = ev->before.start.y;
		ev->after.end.x = ev->before.start.x;
		ev->after.end.y = ev->before.start.y;

		fragment = webkit_dom_range_clone_contents (range, NULL);

		tmp_element = webkit_dom_document_fragment_query_selector (
			fragment, "#-x-evo-selection-start-marker", NULL);
		if (tmp_element)
			remove_node (WEBKIT_DOM_NODE (tmp_element));

		tmp_element = webkit_dom_document_fragment_query_selector (
			fragment, "#-x-evo-selection-end-marker", NULL);
		if (tmp_element)
			remove_node (WEBKIT_DOM_NODE (tmp_element));

		/* If any empty blockquote is presented, remove it. */
		tmp_element = webkit_dom_document_query_selector (
			document, "blockquote[type=cite]:empty", NULL);
		if (tmp_element)
			remove_node (WEBKIT_DOM_NODE (tmp_element));

		/* Selection starts in the beginning of blockquote. */
		tmp_element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (tmp_element));
		if (sibling && WEBKIT_DOM_IS_ELEMENT (sibling) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-quoted")) {
			WebKitDOMNode *child;

			tmp_element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			/* If there is no text after the selection end it means that
			 * the block will be replaced with block that is body's descendant
			 * and not the blockquote's one. Also if the selection started
			 * in the beginning of blockquote we have to insert the quote
			 * characters into the deleted content to correctly restore
			 * them during undo/redo operations. */
			if (!(tmp_element && webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (tmp_element)))) {
				child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
				while (child && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (child))
					child = webkit_dom_node_get_first_child (child);

				child = webkit_dom_node_get_first_child (child);
				if (child && (WEBKIT_DOM_IS_TEXT (child) ||
				    (WEBKIT_DOM_IS_ELEMENT (child) &&
				     !element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-quoted")))) {
					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (child),
						webkit_dom_node_clone_node (sibling, TRUE),
						child,
						NULL);
				}
			}
		}

		/* When we were cloning the range above and the range contained
		 * quoted content there will still be blockquote missing in the
		 * final range. Let's modify the fragment and add it there. */
		tmp_element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		if (tmp_element) {
			WebKitDOMNode *node;

			node = WEBKIT_DOM_NODE (tmp_element);
			while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (node))) {
				if (webkit_dom_node_get_next_sibling (node))
					break;
				node = webkit_dom_node_get_parent_node (node);
			}

			if (node && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (node)) {
				WebKitDOMNode *last_child;

				last_child = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (fragment));

				if (last_child && !WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (last_child)) {
					WebKitDOMDocumentFragment *tmp_fragment;
					WebKitDOMNode *clone;

					tmp_fragment = webkit_dom_document_create_document_fragment (document);
					clone = webkit_dom_node_clone_node (node, FALSE);
					clone = webkit_dom_node_append_child (
						WEBKIT_DOM_NODE (tmp_fragment), clone, NULL);
					webkit_dom_node_append_child (clone, WEBKIT_DOM_NODE (fragment), NULL);
					fragment = tmp_fragment;
				}
			}
		}

		/* FIXME Ugly hack */
		/* If the deleted selection contained the signature (or at least its
		 * part) replace it with the unchanged signature to correctly perform
		 * undo operation. */
		tmp_element = webkit_dom_document_fragment_query_selector (fragment, ".-x-evo-signature-wrapper", NULL);
		if (tmp_element) {
			WebKitDOMElement *signature;

			signature = webkit_dom_document_query_selector (document, ".-x-evo-signature-wrapper", NULL);
			if (signature) {
				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (tmp_element)),
					webkit_dom_node_clone_node (WEBKIT_DOM_NODE (signature), TRUE),
					WEBKIT_DOM_NODE (tmp_element),
					NULL);
			}
		}
	}

	g_object_unref (range);
	g_object_unref (dom_selection);

	g_object_set_data (G_OBJECT (fragment), "-x-evo-delete-key", GINT_TO_POINTER (delete_key));
	g_object_set_data (G_OBJECT (fragment), "-x-evo-control-key", GINT_TO_POINTER (control_key));

	ev->data.fragment = fragment;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
}

gboolean
dom_fix_structure_after_delete_before_quoted_content (WebKitDOMDocument *document,
						      EHTMLEditorWebExtension *extension,
						      guint key_val,
						      guint state,
						      gboolean delete_key)
{
	gboolean collapsed = FALSE;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block, *node;

	collapsed = dom_selection_is_collapsed (document);

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return FALSE;

	if (collapsed) {
		WebKitDOMNode *next_block;

		block = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		next_block = webkit_dom_node_get_next_sibling (block);

		/* Next block is quoted content */
		if (!WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (next_block))
			goto restore;

		/* Delete was pressed in block without any content */
		if (webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker)))
			goto restore;

		/* If there is just BR element go ahead */
		node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker));
		if (node && !WEBKIT_DOM_IS_HTML_BR_ELEMENT (node))
			goto restore;
		else {
			if (key_val != ~0)
				save_history_for_delete_or_backspace (
					document, extension, key_val == GDK_KEY_Delete, (state & GDK_CONTROL_MASK) != 0);

			/* Remove the empty block and move caret to the right place. */
			remove_node (block);

			if (delete_key) {
				/* To the beginning of the next block. */
				dom_move_caret_into_element (
					document, WEBKIT_DOM_ELEMENT (next_block), TRUE);
			} else {
				WebKitDOMNode *prev_block;

				/* On the end of previous block. */
				prev_block = webkit_dom_node_get_previous_sibling (next_block);
				while (prev_block && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (prev_block))
					prev_block = webkit_dom_node_get_last_child (prev_block);

				if (prev_block)
					dom_move_caret_into_element (
						document,
						WEBKIT_DOM_ELEMENT (prev_block),
						FALSE);
			}

			return TRUE;
		}
	} else {
		WebKitDOMNode *end_block, *parent;

		/* Let the quote marks be selectable to nearly correctly remove the
		 * selection. Corrections after are done in body_keyup_event_cb. */
		enable_quote_marks_select (document);

		parent = webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (selection_start_marker));
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent) ||
		    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "b") ||
		    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "i") ||
		    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "u"))
			node = webkit_dom_node_get_previous_sibling (parent);
		else
			node = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (selection_start_marker));

		if (!node || !WEBKIT_DOM_IS_ELEMENT (node))
			goto restore;

		if (!element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-quoted"))
			goto restore;

		block = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		end_block = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_end_marker));

		/* Situation where the start of the selection is in the beginning
		+ * of the block in quoted content and the end in the beginning of
		+ * content that is after the citation or the selection end is in
		+ * the end of the quoted content (showed by ^). We have to
		+ * mark the start block to correctly restore the structure
		+ * afterwards.
		*
		* > |xxx
		* > xxx^
		* |xxx
		*/
		if (get_citation_level (end_block, FALSE) > 0) {
			WebKitDOMNode *parent;

			if (webkit_dom_node_get_next_sibling (end_block))
				goto restore;

			parent = webkit_dom_node_get_parent_node (end_block);
			while (parent && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent)) {
				WebKitDOMNode *next_parent = webkit_dom_node_get_parent_node (parent);

				if (webkit_dom_node_get_next_sibling (parent) &&
				    !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (next_parent))
					goto restore;

				parent = next_parent;
			}
		}

		node = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (selection_end_marker));
		if (!node || WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
			webkit_dom_element_set_id (
				WEBKIT_DOM_ELEMENT (block), "-x-evo-tmp-block");
		}
	}
 restore:
	if (key_val != ~0)
		save_history_for_delete_or_backspace (
			document, extension, key_val == GDK_KEY_Delete, (state & GDK_CONTROL_MASK) != 0);

	dom_selection_restore (document);

	return FALSE;
}

static gboolean
split_citation (WebKitDOMDocument *document,
                EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	WebKitDOMElement *element;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_CITATION_SPLIT;

		dom_selection_get_coordinates (
			document, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);

		if (!dom_selection_is_collapsed (document)) {
			WebKitDOMDocumentFragment *fragment;
			WebKitDOMDOMWindow *dom_window;
			WebKitDOMDOMSelection *dom_selection;
			WebKitDOMRange *range;

			dom_window = webkit_dom_document_get_default_view (document);
			dom_selection = webkit_dom_dom_window_get_selection (dom_window);
			g_object_unref (dom_window);

			if (!webkit_dom_dom_selection_get_range_count (dom_selection)) {
				g_object_unref (dom_selection);
				return FALSE;
			}

			range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
			fragment = webkit_dom_range_clone_contents (range, NULL);

			g_object_unref (range);
			g_object_unref (dom_selection);

			ev->data.fragment = fragment;
		} else {
			WebKitDOMElement *selection_end;
			WebKitDOMNode *sibling;

			dom_selection_save (document);

			selection_end = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end));
			if (!sibling || (WEBKIT_DOM_IS_HTML_BR_ELEMENT (sibling) &&
			    !element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-wrap-br"))) {
				WebKitDOMDocumentFragment *fragment;

				fragment = webkit_dom_document_create_document_fragment (document);
				ev->data.fragment = fragment;
			} else {
				ev->data.fragment = NULL;
			}

			dom_selection_restore (document);
		}
	}

	element = dom_insert_new_line_into_citation (document, extension, "");

	if (ev) {
		dom_selection_get_coordinates (
			document, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	return element != NULL;
}

static gboolean
selection_is_in_table (WebKitDOMDocument *document,
                       gboolean *first_cell,
                       WebKitDOMNode **table_node)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMNode *node, *parent;
	WebKitDOMRange *range;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (first_cell != NULL)
		*first_cell = FALSE;

	if (table_node != NULL)
		*table_node = NULL;

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_object_unref (dom_selection);
		return FALSE;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	node = webkit_dom_range_get_start_container (range, NULL);
	g_object_unref (range);
	g_object_unref (dom_selection);

	parent = node;
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (parent)) {
			if (first_cell != NULL) {
				if (!webkit_dom_node_get_previous_sibling (parent)) {
					gboolean on_start = TRUE;
					WebKitDOMNode *tmp;

					tmp = webkit_dom_node_get_previous_sibling (node);
					if (!tmp && WEBKIT_DOM_IS_TEXT (node))
						on_start = webkit_dom_range_get_start_offset (range, NULL) == 0;
					else if (tmp)
						on_start = FALSE;

					if (on_start) {
						node = webkit_dom_node_get_parent_node (parent);
						if (node && WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (node))
							if (!webkit_dom_node_get_previous_sibling (node))
								*first_cell = TRUE;
					}
				}
			} else
				return TRUE;
		}
		if (WEBKIT_DOM_IS_HTML_TABLE_ELEMENT (parent)) {
			if (table_node != NULL)
				*table_node = parent;
			else
				return TRUE;
		}
		parent = webkit_dom_node_get_parent_node (parent);
	}

	if (table_node == NULL)
		return FALSE;

	return *table_node != NULL;
}

static gboolean
jump_to_next_table_cell (WebKitDOMDocument *document,
                         gboolean jump_back)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMNode *node, *cell;
	WebKitDOMRange *range;

	if (!selection_is_in_table (document, NULL, NULL))
		return FALSE;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	node = webkit_dom_range_get_start_container (range, NULL);

	cell = node;
	while (cell && !WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (cell)) {
		cell = webkit_dom_node_get_parent_node (cell);
	}

	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (cell)) {
		g_object_unref (range);
		g_object_unref (dom_selection);
		return FALSE;
	}

	if (jump_back) {
		/* Get previous cell */
		node = webkit_dom_node_get_previous_sibling (cell);
		if (!node || !WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node)) {
			/* No cell, go one row up. */
			node = webkit_dom_node_get_parent_node (cell);
			node = webkit_dom_node_get_previous_sibling (node);
			if (node && WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT (node)) {
				node = webkit_dom_node_get_last_child (node);
			} else {
				/* No row above, move to the block before table. */
				node = webkit_dom_node_get_parent_node (cell);
				while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (node)))
					node = webkit_dom_node_get_parent_node (node);

				node = webkit_dom_node_get_previous_sibling (node);
			}
		}
	} else {
		/* Get next cell */
		node = webkit_dom_node_get_next_sibling (cell);
		if (!node || !WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node)) {
			/* No cell, go one row below. */
			node = webkit_dom_node_get_parent_node (cell);
			node = webkit_dom_node_get_next_sibling (node);
			if (node && WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT (node)) {
				node = webkit_dom_node_get_first_child (node);
			} else {
				/* No row below, move to the block after table. */
				node = webkit_dom_node_get_parent_node (cell);
				while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (node)))
					node = webkit_dom_node_get_parent_node (node);

				node = webkit_dom_node_get_next_sibling (node);
			}
		}
	}

	if (!node)
		return FALSE;

	webkit_dom_range_select_node_contents (range, node, NULL);
	webkit_dom_range_collapse (range, TRUE, NULL);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_object_unref (range);
	g_object_unref (dom_selection);

	return TRUE;
}

gboolean
dom_delete_character_from_quoted_line_start (WebKitDOMDocument *document,
					     EHTMLEditorWebExtension *extension,
					     guint key_val,
					     guint state)
{
	gboolean success = FALSE;
	WebKitDOMElement *element;
	WebKitDOMNode *node, *beginning, *next_sibling;

	/* We have to be in quoted content. */
	if (!dom_selection_is_citation (document))
		return FALSE;

	/* Selection is just caret. */
	if (!dom_selection_is_collapsed (document))
		return FALSE;

	dom_selection_save (document);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	/* selection end marker */
	node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));

	/* We have to be on the end of line. */
	next_sibling = webkit_dom_node_get_next_sibling (node);
	if (next_sibling && (!WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling) || webkit_dom_node_get_next_sibling (next_sibling)))
		goto out;

	/* Before the caret is just text. */
	node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
	if (!(node && WEBKIT_DOM_IS_TEXT (node)))
		goto out;

	/* There is just one character. */
	if (webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node)) != 1)
		goto out;

	beginning = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (node));
	if (!(beginning && WEBKIT_DOM_IS_ELEMENT (beginning)))
		goto out;

	/* Before the text is the beginning of line. */
	if (!(element_has_class (WEBKIT_DOM_ELEMENT (beginning), "-x-evo-quoted")))
		goto out;

	/* If we are just on the beginning of the line and not on the beginning of
	 * the block we need to remove the last character ourselves as well, otherwise
	 * WebKit will put the caret to wrong position. */
	if (webkit_dom_node_get_previous_sibling (beginning)) {
		EHTMLEditorHistoryEvent *ev = NULL;
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMNode *prev_sibling;

		if (key_val != ~0) {
			ev = g_new0 (EHTMLEditorHistoryEvent, 1);
			ev->type = HISTORY_DELETE;

			dom_selection_get_coordinates (
				document,
				&ev->before.start.x,
				&ev->before.start.y,
				&ev->before.end.x,
				&ev->before.end.y);

			fragment = webkit_dom_document_create_document_fragment (document);
		}

		prev_sibling = webkit_dom_node_get_previous_sibling (beginning);
		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling)) {
			if (key_val != ~0)
				webkit_dom_node_append_child (WEBKIT_DOM_NODE (fragment), prev_sibling, NULL);
			else
				remove_node (prev_sibling);
		}

		prev_sibling = webkit_dom_node_get_previous_sibling (beginning);
		if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (prev_sibling), "data-hidden-space")) {
			if (key_val != ~0)
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (fragment),
					prev_sibling,
					webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
					NULL);
			else
				remove_node (prev_sibling);
		}

		if (key_val != ~0)
			webkit_dom_node_append_child (WEBKIT_DOM_NODE (fragment), beginning, NULL);
		else
			remove_node (beginning);

		if (key_val != ~0) {
			EHTMLEditorUndoRedoManager *manager;

			webkit_dom_node_append_child (WEBKIT_DOM_NODE (fragment), node, NULL);

			dom_selection_get_coordinates (
				document,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);

			ev->data.fragment = fragment;

			manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
			e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
		} else
			remove_node (node);

		dom_selection_restore (document);

		return TRUE;
	}

	if (key_val != ~0)
		save_history_for_delete_or_backspace (
			document, extension, key_val == GDK_KEY_Delete, (state & GDK_CONTROL_MASK) != 0);

	element = webkit_dom_node_get_parent_element (beginning);
	remove_node (WEBKIT_DOM_NODE (element));

	success = TRUE;
 out:
	dom_selection_restore (document);

	if (success)
		dom_insert_new_line_into_citation (document, extension, NULL);

	return success;
}

static gboolean
is_return_key (guint key_val)
{
	return (
	    (key_val == GDK_KEY_Return) ||
	    (key_val == GDK_KEY_Linefeed) ||
	    (key_val == GDK_KEY_KP_Enter));
}

static gboolean
insert_tabulator (WebKitDOMDocument *document,
                  EHTMLEditorWebExtension *extension)
{
	EHTMLEditorUndoRedoManager *manager;
	EHTMLEditorHistoryEvent *ev = NULL;
	gboolean success;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_INPUT;

		if (!dom_selection_is_collapsed (document)) {
			WebKitDOMRange *tmp_range;

			tmp_range = dom_get_current_range (document);
			insert_delete_event (document, extension, tmp_range);
			g_object_unref (tmp_range);
		}

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->before.end.x = ev->before.start.x;
		ev->before.end.y = ev->before.start.y;
	}

	success = dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, "\t");

	if (ev) {
		if (success) {
			WebKitDOMElement *element;
			WebKitDOMDocumentFragment *fragment;

			dom_selection_get_coordinates (
				document,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);

			fragment = webkit_dom_document_create_document_fragment (document);
			element = webkit_dom_document_create_element (document, "span", NULL);
			webkit_dom_html_element_set_inner_text (
				WEBKIT_DOM_HTML_ELEMENT (element), "\t", NULL);
			webkit_dom_element_set_attribute (
				element, "class", "Apple-tab-span", NULL);
			webkit_dom_element_set_attribute (
				element, "style", "white-space:pre", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment), WEBKIT_DOM_NODE (element), NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				WEBKIT_DOM_NODE (dom_create_selection_marker (document, TRUE)),
				NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				WEBKIT_DOM_NODE (dom_create_selection_marker (document, FALSE)),
				NULL);
			ev->data.fragment = fragment;

			e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
			e_html_editor_web_extension_set_content_changed (extension);
		} else {
			e_html_editor_undo_redo_manager_remove_current_history_event (manager);
			e_html_editor_undo_redo_manager_remove_current_history_event (manager);
			g_free (ev);
		}
	}

	return success;
}

gboolean
return_pressed_in_empty_list_item (WebKitDOMDocument *document,
				   EHTMLEditorWebExtension *extension,
				   gboolean save_history)
{
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *parent, *node;

	if (!dom_selection_is_collapsed (document))
		return FALSE;

	dom_selection_save (document);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	if (!WEBKIT_DOM_IS_HTML_LI_ELEMENT (parent)) {
		dom_selection_restore (document);
		return FALSE;
	}

	node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker));
	/* Check if return was pressed inside an empty list item. */
	if (!webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker)) &&
	    (!node || (node && WEBKIT_DOM_IS_HTML_BR_ELEMENT (node) && !webkit_dom_node_get_next_sibling (node)))) {
		EHTMLEditorHistoryEvent *ev;
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMElement *paragraph;
		WebKitDOMNode *list;

		if (save_history) {
			/* Insert new history event for Return to have the right coordinates.
			 * The fragment will be added later. */
			ev = g_new0 (EHTMLEditorHistoryEvent, 1);
			ev->type = HISTORY_INPUT;

			dom_selection_get_coordinates (
				document,
				&ev->before.start.x,
				&ev->before.start.y,
				&ev->before.end.x,
				&ev->before.end.y);

			fragment = webkit_dom_document_create_document_fragment (document);
		}

		list = split_list_into_two (parent, -1);

		if (save_history) {
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				parent,
				NULL);
		} else {
			remove_node (parent);
		}

		paragraph = dom_prepare_paragraph (document, extension, TRUE);

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (list),
			WEBKIT_DOM_NODE (paragraph),
			list,
			NULL);

		dom_selection_restore (document);

		if (save_history) {
			EHTMLEditorUndoRedoManager *manager;

			dom_selection_get_coordinates (
				document,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);

			ev->data.fragment = fragment;

			manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

			e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
		}

		e_html_editor_web_extension_set_content_changed (extension);

		return TRUE;
	}

	dom_selection_restore (document);

	return FALSE;
}

static void
change_smiley_to_plain_text (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;
	WebKitDOMNode *parent;
	gboolean in_smiley = FALSE;

	dom_selection_save (document);
	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
	if (WEBKIT_DOM_IS_ELEMENT (parent) &&
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-smiley-text"))
		in_smiley = TRUE;
	else {
		if (dom_selection_is_collapsed (document)) {
			WebKitDOMNode *prev_sibling;

			prev_sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
			if (prev_sibling && WEBKIT_DOM_IS_TEXT (prev_sibling)) {
				gchar *text = webkit_dom_character_data_get_data (
					WEBKIT_DOM_CHARACTER_DATA (prev_sibling));

				if (g_strcmp0 (text, UNICODE_ZERO_WIDTH_SPACE) == 0) {
					remove_node (prev_sibling);
					in_smiley = TRUE;
					prev_sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
					if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
					    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-smiley-wrapper"))
						parent = webkit_dom_node_get_last_child (prev_sibling);
				}

				g_free (text);
			}
		} else {
			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
			if (WEBKIT_DOM_IS_ELEMENT (parent) &&
			    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-smiley-text"))
				in_smiley = TRUE;
		}
	}

	if (in_smiley) {
		WebKitDOMNode *wrapper, *child;

		wrapper = webkit_dom_node_get_parent_node (parent);
		while ((child = webkit_dom_node_get_first_child (parent)))
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (wrapper),
				child,
				wrapper,
				NULL);
		remove_node (wrapper);
	}

	dom_selection_restore (document);
}

gboolean
key_press_event_process_return_key (WebKitDOMDocument *document,
				    EHTMLEditorWebExtension *extension)
{
	gboolean first_cell = FALSE;
	WebKitDOMNode *table = NULL;

	/* Return pressed in the beginning of the first cell will insert
	 * new block before the table (and move the caret there) if none
	 * is already there, otherwise it will act as normal return. */
	if (selection_is_in_table (document, &first_cell, &table) && first_cell) {
		WebKitDOMNode *node;

		node = webkit_dom_node_get_previous_sibling (table);
		if (!node) {
			node = webkit_dom_node_get_next_sibling (table);
			node = webkit_dom_node_clone_node (node, FALSE);
			webkit_dom_node_append_child (
				node,
				WEBKIT_DOM_NODE (webkit_dom_document_create_element (
					document, "br", NULL)),
				NULL);
			dom_add_selection_markers_into_element_start (
				document, WEBKIT_DOM_ELEMENT (node), NULL, NULL);
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (table),
				node,
				table,
				NULL);
			dom_selection_restore (document);
			e_html_editor_web_extension_set_content_changed (extension);
			return TRUE;
		}
	}

	/* When user presses ENTER in a citation block, WebKit does
	 * not break the citation automatically, so we need to use
	 * the special command to do it. */
	if (dom_selection_is_citation (document)) {
		dom_remove_input_event_listener_from_body (document, extension);
		if (split_citation (document, extension)) {
			e_html_editor_web_extension_set_return_key_pressed (extension, TRUE);
			dom_check_magic_links (document, extension, FALSE);
			e_html_editor_web_extension_set_return_key_pressed (extension, FALSE);
			e_html_editor_web_extension_set_content_changed (extension);

			return TRUE;
		}
		return FALSE;
	}

	/* If the ENTER key is pressed inside an empty list item then the list
	 * is broken into two and empty paragraph is inserted between lists. */
	if (return_pressed_in_empty_list_item (document, extension, TRUE))
		return TRUE;

	return FALSE;
}

gboolean
key_press_event_process_backspace_key (WebKitDOMDocument *document,
				       EHTMLEditorWebExtension *extension)
{
	/* BackSpace pressed in the beginning of quoted content changes
	 * format to normal and inserts text into body */
	if (dom_selection_is_collapsed (document)) {
		dom_selection_save (document);
		if (dom_change_quoted_block_to_normal (document, extension) || delete_hidden_space (document, extension)) {
			dom_selection_restore (document);
			dom_force_spell_check_for_current_paragraph (document, extension);
			e_html_editor_web_extension_set_content_changed (extension);
			return TRUE;
		}
		dom_selection_restore (document);
	}

	/* BackSpace in indented block decrease indent level by one */
	if (dom_selection_is_indented (document) &&
	    dom_selection_is_collapsed (document)) {
		WebKitDOMElement *selection_start;
		WebKitDOMNode *prev_sibling;

		dom_selection_save (document);
		selection_start = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		/* Empty text node before caret */
		prev_sibling = webkit_dom_node_get_previous_sibling (
			WEBKIT_DOM_NODE (selection_start));
		if (prev_sibling && WEBKIT_DOM_IS_TEXT (prev_sibling))
			if (webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (prev_sibling)) == 0)
				prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);

		dom_selection_restore (document);
		if (!prev_sibling) {
			dom_selection_unindent (document, extension);
			e_html_editor_web_extension_set_content_changed (extension);
			return TRUE;
		}
	}

	if (prevent_from_deleting_last_element_in_body (document))
		return TRUE;

	return FALSE;
}

gboolean
key_press_event_process_delete_or_backspace_key (WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 guint key_val,
						 guint state,
						 gboolean delete)
{
	gboolean html_mode;
	gboolean local_delete;

	html_mode = e_html_editor_web_extension_get_html_mode (extension);
	local_delete = (key_val == GDK_KEY_Delete) || delete;

	if (!html_mode && e_html_editor_web_extension_get_magic_links_enabled (extension)) {
		/* If deleting something in a smiley it won't be a smiley
		 * anymore (at least from Evolution' POV), so remove all
		 * the elements that are hidden in the wrapper and leave
		 * just the text. Also this ensures that when a smiley is
		 * recognized and we press the BackSpace key we won't delete
		 * the UNICODE_HIDDEN_SPACE, but we will correctly delete
		 * the last character of smiley. */
		change_smiley_to_plain_text (document);
	}

	if (!local_delete && !html_mode &&
	    dom_delete_character_from_quoted_line_start (document, extension, key_val, state))
		goto out;

	if (dom_fix_structure_after_delete_before_quoted_content (document, extension, key_val, state, FALSE))
		goto out;

	if (local_delete) {
		WebKitDOMElement *selection_start_marker;
		WebKitDOMNode *sibling, *block, *next_block;

		/* This needs to be performed just in plain text mode
		 * and when the selection is collapsed. */
		if (html_mode || !dom_selection_is_collapsed (document))
			return FALSE;

		dom_selection_save (document);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		sibling = webkit_dom_node_get_previous_sibling (
			WEBKIT_DOM_NODE (selection_start_marker));
		/* Check if the key was pressed in the beginning of block. */
		if (!(sibling && WEBKIT_DOM_IS_ELEMENT (sibling) &&
		      element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-quoted"))) {
			dom_selection_restore (document);
			return FALSE;
		}

		sibling = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (selection_start_marker));
		sibling = webkit_dom_node_get_next_sibling (sibling);

		/* And also the current block was empty. */
		if (!(!sibling || (sibling && WEBKIT_DOM_IS_HTML_BR_ELEMENT (sibling) &&
		      !element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-wrap-br")))) {
			dom_selection_restore (document);
			return FALSE;
		}

		block = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		next_block = webkit_dom_node_get_next_sibling (block);

		remove_node (block);

		dom_move_caret_into_element (
			document, WEBKIT_DOM_ELEMENT (next_block), TRUE);

		goto out;
	} else {
		/* Concatenating a non-quoted block with Backspace key to the
		 * previous block that is inside a quoted content. */
		WebKitDOMElement *selection_start_marker;
		WebKitDOMNode *node, *block, *prev_block, *last_child, *child;

		if (html_mode || !dom_selection_is_collapsed (document) ||
		    dom_selection_is_citation (document))
			return FALSE;

		dom_selection_save (document);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker));
		if (node) {
			dom_selection_restore (document);
			return FALSE;
		}

		remove_empty_blocks (document);

		block = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		prev_block = webkit_dom_node_get_previous_sibling (block);
		if (!prev_block || !WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (prev_block)) {
			dom_selection_restore (document);
			return FALSE;
		}

		last_child = webkit_dom_node_get_last_child (prev_block);
		while (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (last_child))
			last_child = webkit_dom_node_get_last_child (last_child);

		dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (last_child));
		dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (last_child));

		node = webkit_dom_node_get_last_child (last_child);
		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (node))
			remove_node (node);

		while ((child = webkit_dom_node_get_first_child (block)))
			webkit_dom_node_append_child (last_child, child, NULL);

		remove_node (block);

		if (WEBKIT_DOM_IS_ELEMENT (last_child))
			wrap_and_quote_element (document, extension, WEBKIT_DOM_ELEMENT (last_child));

		dom_selection_restore (document);

		goto out;
	}

	return FALSE;
 out:
	dom_force_spell_check_for_current_paragraph (document, extension);
	e_html_editor_web_extension_set_content_changed (extension);

	return TRUE;
}

gboolean
dom_process_on_key_press (WebKitDOMDocument *document,
                          EHTMLEditorWebExtension *extension,
                          guint key_val,
			  guint state)
{
	e_html_editor_web_extension_set_dont_save_history_in_body_input (extension, FALSE);

	if (key_val == GDK_KEY_Tab || key_val == GDK_KEY_ISO_Left_Tab) {
		if (jump_to_next_table_cell (document, key_val == GDK_KEY_ISO_Left_Tab))
			return TRUE;

		if (key_val == GDK_KEY_Tab)
			return insert_tabulator (document, extension);
		else
			return FALSE;
	}

	if (is_return_key (key_val) && key_press_event_process_return_key (document, extension))
		return TRUE;

	if (key_val == GDK_KEY_BackSpace &&
	    key_press_event_process_backspace_key (document, extension)) {
		return TRUE;
	}

	if ((key_val == GDK_KEY_Delete || key_val == GDK_KEY_BackSpace) &&
	    key_press_event_process_delete_or_backspace_key (document, extension, key_val, state, key_val == GDK_KEY_Delete)) {
		return TRUE;
	}

	return FALSE;
}

gboolean
dom_check_if_conversion_needed (WebKitDOMDocument *document)
{
	gboolean is_from_new_message, converted, edit_as_new, message, convert;
	gboolean reply, hide;
	WebKitDOMHTMLElement *body;

	body = webkit_dom_document_get_body (document);

	is_from_new_message = webkit_dom_element_has_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-new-message");
	converted = webkit_dom_element_has_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-converted");
	edit_as_new = webkit_dom_element_has_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-edit-as-new");
	message = webkit_dom_element_has_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-message");

	reply = !is_from_new_message && !edit_as_new && message;
	hide = !reply && !converted;

	convert = message && ((!hide && reply && !converted) || (edit_as_new && !converted));
	convert = convert && !is_from_new_message;

	return convert;
}

static void
toggle_tables (WebKitDOMDocument *document,
               gboolean html_mode)
{
	WebKitDOMNodeList *list;
	gint ii, length;

	list = webkit_dom_document_query_selector_all (document, "table", NULL);
	length = webkit_dom_node_list_get_length (list);

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *table = webkit_dom_node_list_item (list, ii);

		if (html_mode) {
			element_remove_class (WEBKIT_DOM_ELEMENT (table), "-x-evo-plaintext-table");
			rename_attribute (WEBKIT_DOM_ELEMENT (table), "data-width", "width");
			rename_attribute (WEBKIT_DOM_ELEMENT (table), "data-cellspacing", "cellspacing");
			rename_attribute (WEBKIT_DOM_ELEMENT (table), "data-cellpadding", "cellpadding");
			rename_attribute (WEBKIT_DOM_ELEMENT (table), "data-border", "border");
		} else {
			element_add_class (WEBKIT_DOM_ELEMENT (table), "-x-evo-plaintext-table");
			rename_attribute (WEBKIT_DOM_ELEMENT (table), "width", "data-width");
			rename_attribute (WEBKIT_DOM_ELEMENT (table), "cellspacing", "data-cellspacing");
			webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (table), "cellspacing", "0", NULL);
			rename_attribute (WEBKIT_DOM_ELEMENT (table), "cellpadding", "data-cellpadding");
			webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (table), "cellpadding", "0", NULL);
			rename_attribute (WEBKIT_DOM_ELEMENT (table), "border", "data-border");
			webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (table), "border", "0", NULL);
		}
		g_object_unref (table);
	}
	g_object_unref (list);
}

static void
toggle_unordered_lists (WebKitDOMDocument *document,
			gboolean html_mode)
{
	WebKitDOMNodeList *list;
	gint ii, length;

	list = webkit_dom_document_query_selector_all (document, "ul", NULL);
	length = webkit_dom_node_list_get_length (list);

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		if (html_mode) {
			webkit_dom_element_remove_attribute (
				WEBKIT_DOM_ELEMENT (node), "data-evo-plain-text");
		} else {
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (node), "data-evo-plain-text", "", NULL);
		}
		g_object_unref (node);
	}
	g_object_unref (list);
}

void
dom_process_content_after_mode_change (WebKitDOMDocument *document,
                                       EHTMLEditorWebExtension *extension)
{
	EHTMLEditorUndoRedoManager *manager;
	gboolean html_mode;
	WebKitDOMElement *blockquote;

	html_mode = e_html_editor_web_extension_get_html_mode (extension);

	blockquote = webkit_dom_document_query_selector (
		document, "blockquote[type|=cite]", NULL);

	if (html_mode) {
		WebKitDOMHTMLElement *body;

		if (blockquote)
			dom_dequote_plain_text (document);

		toggle_paragraphs_style (document, extension);
		toggle_smileys (document, extension);
		toggle_tables (document, html_mode);
		toggle_unordered_lists (document, html_mode);

		body = webkit_dom_document_get_body (document);

		dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (body));
	} else {
		gchar *plain;

		dom_selection_save (document);

		if (blockquote) {
			wrap_paragraphs_in_quoted_content (document, extension);
			quote_plain_text_elements_after_wrapping_in_document (
				document);
		}

		toggle_paragraphs_style (document, extension);
		toggle_smileys (document, extension);
		toggle_tables (document, html_mode);
		toggle_unordered_lists (document, html_mode);
		remove_images (document);
		remove_background_images_in_document (document);

		plain = process_content_for_mode_change (document, extension);

		if (*plain) {
			webkit_dom_element_set_outer_html (
				WEBKIT_DOM_ELEMENT (
					webkit_dom_document_get_document_element (document)),
				plain,
				NULL);
			dom_selection_restore (document);
			dom_force_spell_check_in_viewport (document, extension);
		}

		g_free (plain);
	}

	set_monospace_font_family_on_body (WEBKIT_DOM_ELEMENT (webkit_dom_document_get_body (document)), html_mode);

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	e_html_editor_undo_redo_manager_clean_history (manager);
}

gint
dom_get_caret_offset (WebKitDOMDocument *document,
		      EHTMLEditorWebExtension *extension)
{
	gint ret_val;
	gchar *text;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMNode *anchor;
	WebKitDOMRange *range;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_object_unref (dom_selection);
		return 0;
	}

	webkit_dom_dom_selection_collapse_to_start (dom_selection, NULL);
	/* Select the text from the current caret position to the beginning of the line. */
	webkit_dom_dom_selection_modify (dom_selection, "extend", "left", "lineBoundary");

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	anchor = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	text = webkit_dom_range_to_string (range, NULL);
	ret_val = strlen (text);
	g_free (text);

	webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);

	/* In the plain text mode we need to increase the return value by 2 per
	 * citation level because of "> ". */
	if (!e_html_editor_web_extension_get_html_mode (extension)) {
		WebKitDOMNode *parent = anchor;

		while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
			if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent) &&
			    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-plaintext-quoted"))
				ret_val += 2;

			parent = webkit_dom_node_get_parent_node (parent);
		}
	}

	g_object_unref (range);
	g_object_unref (dom_selection);

	return ret_val;
}

gint
dom_get_caret_position (WebKitDOMDocument *document)
{
	gint ret_val;
	gchar *text;
	WebKitDOMHTMLElement *body;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range, *range_clone;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_object_unref (dom_selection);
		return 0;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	range_clone = webkit_dom_range_clone_range (range, NULL);

	body = webkit_dom_document_get_body (document);
	/* Select the text from the beginning of the body to the current caret. */
	webkit_dom_range_set_start_before (
		range_clone, webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)), NULL);

	/* This is returning a text without new lines! */
	text = webkit_dom_range_to_string (range_clone, NULL);
	ret_val = strlen (text);
	g_free (text);

	g_object_unref (range_clone);
	g_object_unref (range);
	g_object_unref (dom_selection);

	return ret_val;
}

void
dom_drag_and_drop_end (WebKitDOMDocument *document,
                       EHTMLEditorWebExtension *extension)
{
	gint ii, length;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMNodeList *list;

	/* When the image is DnD inside the view WebKit removes the wrapper that
	 * is used for resizing the image, so we have to recreate it again. */
	list = webkit_dom_document_query_selector_all (document, ":not(span) > img[data-inline]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMElement *element;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		element = webkit_dom_document_create_element (document, "span", NULL);
		webkit_dom_element_set_class_name (element, "-x-evo-resizable-wrapper");

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (element),
			node,
			NULL);

		webkit_dom_node_append_child (WEBKIT_DOM_NODE (element), node, NULL);
		g_object_unref (node);
	}
	g_object_unref (list);

	/* When the image is moved the new selection is created after after it, so
	 * lets collapse the selection to have the caret right after the image. */
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	if (length > 0)
		webkit_dom_dom_selection_collapse_to_start (dom_selection, NULL);
	else
		webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);

	dom_force_spell_check_in_viewport (document, extension);

	g_object_unref (dom_selection);
	g_object_unref (dom_window);
}

static void
dom_set_link_color_in_document (WebKitDOMDocument *document,
				const gchar *color,
				gboolean visited)
{
	gchar *color_str = NULL;
	const gchar *style_id;
	WebKitDOMHTMLHeadElement *head;
	WebKitDOMElement *style_element;
	WebKitDOMHTMLElement *body;

	g_return_if_fail (color != NULL);

	style_id = visited ? "-x-evo-a-color-style-visited" : "-x-evo-a-color-style";

	head = webkit_dom_document_get_head (document);
	body = webkit_dom_document_get_body (document);

	style_element = webkit_dom_document_get_element_by_id (document, style_id);
	if (!style_element) {
		style_element = webkit_dom_document_create_element (document, "style", NULL);
		webkit_dom_element_set_id (style_element, style_id);
		webkit_dom_element_set_attribute (style_element, "type", "text/css", NULL);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (head), WEBKIT_DOM_NODE (style_element), NULL);
	}

	color_str = g_strdup_printf (
		visited ? "a.-x-evo-visited-link { color: %s; }" : "a { color: %s; }", color);
	webkit_dom_element_set_inner_html (style_element, color_str, NULL);
	g_free (color_str);

	if (visited)
		webkit_dom_html_body_element_set_v_link (
			WEBKIT_DOM_HTML_BODY_ELEMENT (body), color);
	else
		webkit_dom_html_body_element_set_link (
			WEBKIT_DOM_HTML_BODY_ELEMENT (body), color);
}

void
dom_set_link_color (WebKitDOMDocument *document,
		    const gchar *color)
{
	dom_set_link_color_in_document (document, color, FALSE);
}

void
dom_set_visited_link_color (WebKitDOMDocument *document,
			    const gchar *color)
{
	dom_set_link_color_in_document (document, color, TRUE);
}
