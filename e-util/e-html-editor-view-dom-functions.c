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

#include "e-dom-utils.h"
#include "e-html-editor-selection-dom-functions.h"

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
                  EHTMLEditorViewCommand command,
                  const gchar *value)
{
	const gchar *cmd_str = 0;
	gboolean has_value;

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
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, "InsertText", TRUE)
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

	return webkit_dom_document_exec_command (
		document, cmd_str, FALSE, has_value ? value : "" );
}

static WebKitDOMElement *
get_parent_block_element (WebKitDOMNode *node)
{
	WebKitDOMElement *parent = webkit_dom_node_get_parent_element (node);

	while (parent &&
	       !WEBKIT_DOM_IS_HTML_DIV_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_PRE_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_HEADING_ELEMENT (parent) &&
	       !element_has_tag (parent, "address")) {
		parent = webkit_dom_node_get_parent_element (
			WEBKIT_DOM_NODE (parent));
	}

	return parent;
}

void
dom_force_spell_check_for_current_paragraph (WebKitDOMDocument *document)
{
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *window;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMElement *parent, *element;
	WebKitDOMRange *end_range, *actual;
	WebKitDOMText *text;

	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	element = webkit_dom_document_query_selector (
		document, "body[spellcheck=true]", NULL);

	if (!element)
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
/* FIXME WK2
	g_signal_handlers_block_by_func (
		view, html_editor_view_selection_changed_cb, NULL);
	e_html_editor_selection_block_selection_changed (selection);
*/
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
	actual = webkit_dom_document_create_range (document);
	webkit_dom_range_select_node_contents (
		actual, WEBKIT_DOM_NODE (parent), NULL);
	webkit_dom_range_collapse (actual, TRUE, NULL);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, actual);

	/* Go through all words to spellcheck them. To avoid this we have to wait for
	 * http://www.w3.org/html/wg/drafts/html/master/editing.html#dom-forcespellcheck */
	actual = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	/* We are moving forward word by word until we hit the text on the end of
	 * the paragraph that we previously inserted there */
	while (actual && webkit_dom_range_compare_boundary_points (end_range, 2, actual, NULL) != 0) {
		webkit_dom_dom_selection_modify (
			dom_selection, "move", "forward", "word");
		actual = webkit_dom_dom_selection_get_range_at (
			dom_selection, 0, NULL);
	}

	/* Remove the text that we inserted on the end of the paragraph */
	remove_node (WEBKIT_DOM_NODE (text));

	/* Unblock the callbacks */
/* FIXME WK2
	g_signal_handlers_unblock_by_func (
		view, html_editor_view_selection_changed_cb, NULL);
	e_html_editor_selection_unblock_selection_changed (selection);
*/
	do_selection_restore (document);
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

static void
add_selection_markers_into_element_end (WebKitDOMDocument *document,
                                        WebKitDOMElement *element,
                                        WebKitDOMElement **selection_start_marker,
                                        WebKitDOMElement **selection_end_marker)
{
	WebKitDOMElement *marker;

	marker = create_selection_marker (document, TRUE);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (element), WEBKIT_DOM_NODE (marker), NULL);
	if (selection_start_marker)
		*selection_start_marker = marker;

	marker = create_selection_marker (document, FALSE);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (element), WEBKIT_DOM_NODE (marker), NULL);
	if (selection_end_marker)
		*selection_end_marker = marker;
}

static void
refresh_spell_check (WebKitDOMDocument *document,
                     gboolean enable_spell_check)
{
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *window;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *end_range, *actual;
	WebKitDOMText *text;

	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	/* Enable/Disable spellcheck in composer */
	body = webkit_dom_document_get_body (document);
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
		if (!child)
			return;

		add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EHTMLEditorSelection and here as well
	 * when we are moving with caret */
/* FIXME WK2
	g_signal_handlers_block_by_func (
		view, html_editor_view_selection_changed_cb, NULL);
	e_html_editor_selection_block_selection_changed (selection);
*/
	/* Append some text on the end of the body */
	text = webkit_dom_document_create_text_node (document, "-x-evo-end");
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (body), WEBKIT_DOM_NODE (text), NULL);

	/* Create range that's pointing on the end of this text */
	end_range = webkit_dom_document_create_range (document);
	webkit_dom_range_select_node_contents (
		end_range, WEBKIT_DOM_NODE (text), NULL);
	webkit_dom_range_collapse (end_range, FALSE, NULL);

	/* Move on the beginning of the document */
	webkit_dom_dom_selection_modify (
		dom_selection, "move", "backward", "documentboundary");

	/* Go through all words to spellcheck them. To avoid this we have to wait for
	 * http://www.w3.org/html/wg/drafts/html/master/editing.html#dom-forcespellcheck */
	actual = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	/* We are moving forward word by word until we hit the text on the end of
	 * the body that we previously inserted there */
	while (actual && webkit_dom_range_compare_boundary_points (end_range, 2, actual, NULL) != 0) {
		webkit_dom_dom_selection_modify (
			dom_selection, "move", "forward", "word");
		actual = webkit_dom_dom_selection_get_range_at (
			dom_selection, 0, NULL);
	}

	/* Remove the text that we inserted on the end of the body */
	remove_node (WEBKIT_DOM_NODE (text));

	/* Unblock the callbacks */
/* FIXME WK2
	g_signal_handlers_unblock_by_func (
		view, html_editor_view_selection_changed_cb, NULL);
	e_html_editor_selection_unblock_selection_changed (selection);
*/
	dom_selection_restore (document);
}

void
dom_turn_spell_check_off (WebKitDOMDocument *document)
{
	refresh_spell_check (document, FALSE);
}

void
dom_force_spell_check (EHTMLEditorWebExtension *extension,
                       WebKitDOMDocument *document)
{
	if (extension->priv->inline_spelling)
		refresh_spell_check (document, TRUE);
}

static gint
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

static void
quote_plain_text_element_after_wrapping (WebKitDOMDocument *document,
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
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (quoted_node), quotation, NULL);

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
	}

	g_object_unref (list);
	g_free (quotation);
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
			if (!prev_sibling)
				return webkit_dom_range_get_collapsed (range, NULL);
		}
	}

	return FALSE;
}

static WebKitDOMNode *
get_parent_block_node_from_child (WebKitDOMNode *node)
{
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);

	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-temp-text-wrapper") ||
	    WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent) ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "b") ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "i") ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "u"))
		parent = webkit_dom_node_get_parent_node (parent);

	return parent;
}

static WebKitDOMElement *
prepare_paragraph (WebKitDOMDocument *document,
                   gboolean with_selection)
{
	WebKitDOMElement *element, *paragraph;

	paragraph = dom_get_paragraph_element (document, -1, 0);

	if (with_selection)
		add_selection_markers_into_element_start (
			document, paragraph, NULL, NULL);

	element = webkit_dom_document_create_element (document, "BR", NULL);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (paragraph), WEBKIT_DOM_NODE (element), NULL);

	return paragraph;
}

static WebKitDOMElement *
insert_new_line_into_citation (EHTMLEditorWebExtension *extension,
                               WebKitDOMDocument *document,
                               const gchar *html_to_insert)
{
	gboolean html_mode = FALSE, ret_val, avoid_editor_call;
	WebKitDOMElement *element, *paragraph = NULL;

	html_mode = is_in_html_mode (extension);

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

		paragraph = dom_get_paragraph_element (document, -1, 0);

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
			document, E_HTML_EDITOR_VIEW_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, NULL);

		if (!ret_val)
			return NULL;

		element = webkit_dom_document_query_selector (
			document, "body>br", NULL);

		if (!element)
			return NULL;
	}

	if (!html_mode) {
		WebKitDOMNode *next_sibling;

		next_sibling = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (element));

		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (next_sibling)) {
			gint citation_level, length;
			gint word_wrap_length = extension->priv->word_wrap_length;
			WebKitDOMNode *node;

			node = webkit_dom_node_get_first_child (next_sibling);
			while (node && is_citation_node (node))
				node = webkit_dom_node_get_first_child (node);

			citation_level = get_citation_level (node, FALSE);
			length = word_wrap_length - 2 * citation_level;

			/* Rewrap and requote first block after the newly inserted line */
			if (node && WEBKIT_DOM_IS_ELEMENT (node)) {
				remove_quoting_from_element (WEBKIT_DOM_ELEMENT (node));
				remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (node));
				node = WEBKIT_DOM_NODE (dom_wrap_paragraph_length (
					document, WEBKIT_DOM_ELEMENT (node), length));
				quote_plain_text_element_after_wrapping (
					document, WEBKIT_DOM_ELEMENT (node), citation_level);
			}

			dom_force_spell_check (document);
		}
	}

	if (html_to_insert && *html_to_insert) {
		paragraph = prepare_paragraph (document, FALSE);
		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (paragraph),
			html_to_insert,
			NULL);
		add_selection_markers_into_element_end (
			document, paragraph, NULL, NULL);
	} else
		paragraph = prepare_paragraph (document, TRUE);

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
	}
}

/* For purpose of this function see e-mail-formatter-quote.c */
static void
move_elements_to_body (WebKitDOMDocument *document)
{
	WebKitDOMHTMLElement *body = webkit_dom_document_get_body (document);
	WebKitDOMNodeList *list;
	gint ii;

	list = webkit_dom_document_query_selector_all (
		document, "span.-x-evo-to-body[data-headers]", NULL);
	for (ii = webkit_dom_node_list_get_length (list) - 1; ii >= 0; ii--) {
		WebKitDOMNode *child;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		while ((child = webkit_dom_node_get_first_child (node))) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (body),
				child,
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (body)),
				NULL);
		}

		remove_node (node);
	}
	g_object_unref (list);

	list = webkit_dom_document_query_selector_all (
		document, "span.-x-evo-to-body[data-credits]", NULL);
	for (ii = webkit_dom_node_list_get_length (list) - 1; ii >= 0; ii--) {
		WebKitDOMNode *child;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		while ((child = webkit_dom_node_get_first_child (node))) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (body),
				child,
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (body)),
				NULL);
		}

		remove_node (node);
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
	}
	g_object_unref (list);
}

/* FIXME WK2 */
static void
dom_check_magic_links (WebKitDOMDocument *document,
                       WebKitDOMRange *range,
                       gboolean include_space_by_user,
		       gboolean return_pressed,
		       gboolean space_pressed)
{
	gchar *node_text;
	gchar **urls;
	GRegex *regex = NULL;
	GMatchInfo *match_info;
	gint start_pos_url, end_pos_url;
	WebKitDOMNode *node;
	gboolean include_space = FALSE;
	gboolean return_pressed = FALSE;

	if (event != NULL) {
		return_pressed = is_return_key (event);
		include_space = (event->keyval == GDK_KEY_space);
	} else {
		include_space = include_space_by_user;
	}

	node = webkit_dom_range_get_end_container (range, NULL);

	if (return_pressed)
		node = webkit_dom_node_get_previous_sibling (node);

	if (!node)
		return;

	if (!WEBKIT_DOM_IS_TEXT (node)) {
		if (webkit_dom_node_has_child_nodes (node))
			node = webkit_dom_node_get_first_child (node);
		if (!WEBKIT_DOM_IS_TEXT (node))
			return;
	}

	node_text = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (node));
	if (!node_text || !(*node_text) || !g_utf8_validate (node_text, -1, NULL))
		return;

	regex = g_regex_new (include_space ? URL_PATTERN_SPACE : URL_PATTERN, 0, 0, NULL);

	if (!regex) {
		g_free (node_text);
		return;
	}

	g_regex_match_all (regex, node_text, G_REGEX_MATCH_NOTEMPTY, &match_info);
	urls = g_match_info_fetch_all (match_info);

	if (urls) {
		gchar *final_url, *url_end_raw;
		glong url_start, url_end, url_length;
		WebKitDOMNode *url_text_node_clone;
		WebKitDOMText *url_text_node;
		WebKitDOMElement *anchor;
		const gchar* url_text;

		if (!return_pressed)
			dom_save_caret_position (document);

		g_match_info_fetch_pos (match_info, 0, &start_pos_url, &end_pos_url);

		/* Get start and end position of url in node's text because positions
		 * that we get from g_match_info_fetch_pos are not UTF-8 aware */
		url_end_raw = g_strndup(node_text, end_pos_url);
		url_end = g_utf8_strlen (url_end_raw, -1);

		url_length = g_utf8_strlen (urls[0], -1);
		url_start = url_end - url_length;

		webkit_dom_text_split_text (
			WEBKIT_DOM_TEXT (node),
			include_space ? url_end - 1 : url_end,
			NULL);

		url_text_node = webkit_dom_text_split_text (
			WEBKIT_DOM_TEXT (node), url_start, NULL);
		url_text_node_clone = webkit_dom_node_clone_node (
			WEBKIT_DOM_NODE (url_text_node), TRUE);
		url_text = webkit_dom_text_get_whole_text (
			WEBKIT_DOM_TEXT (url_text_node_clone));

		final_url = g_strconcat (
			g_str_has_prefix (url_text, "www") ? "http://" : "", url_text, NULL);

		/* Create and prepare new anchor element */
		anchor = webkit_dom_document_create_element (document, "A", NULL);

		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (anchor),
			url_text,
			NULL);

		webkit_dom_html_anchor_element_set_href (
			WEBKIT_DOM_HTML_ANCHOR_ELEMENT (anchor),
			final_url);

		/* Insert new anchor element into document */
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (anchor),
			WEBKIT_DOM_NODE (url_text_node),
			NULL);

		if (!return_pressed)
			dom_restore_caret_position (document);

		g_free (url_end_raw);
		g_free (final_url);
	} else {
		WebKitDOMElement *parent;
		WebKitDOMNode *prev_sibling;
		gchar *href, *text, *url;
		gint diff;
		const char* text_to_append;
		gboolean appending_to_link = FALSE;

		parent = webkit_dom_node_get_parent_element (node);
		prev_sibling = webkit_dom_node_get_previous_sibling (node);

		/* If previous sibling is ANCHOR and actual text node is not beginning with
		 * space => we're appending to link */
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling)) {
			text_to_append = webkit_dom_node_get_text_content (node);
			if (g_strcmp0 (text_to_append, "") != 0 &&
				!g_unichar_isspace (g_utf8_get_char (text_to_append))) {

				appending_to_link = TRUE;
				parent = WEBKIT_DOM_ELEMENT (prev_sibling);
			}
		}

		/* If parent is ANCHOR => we're editing the link */
		if (!WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent) && !appending_to_link) {
			g_match_info_free (match_info);
			g_regex_unref (regex);
			g_free (node_text);
			return;
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

		if (strstr (href, "://") && !strstr (text, "://")) {
			url = strstr (href, "://") + 3;
			diff = strlen (text) - strlen (url);

			if (text [strlen (text) - 1] != '/')
				diff++;

			if ((g_strcmp0 (url, text) != 0 && ABS (diff) == 1) || appending_to_link) {
				gchar *inner_html, *protocol, *new_href;

				protocol = g_strndup (href, strstr (href, "://") - href + 3);
				inner_html = webkit_dom_html_element_get_inner_html (
					WEBKIT_DOM_HTML_ELEMENT (parent));
				new_href = g_strconcat (
					protocol, inner_html, appending_to_link ? text_to_append : "", NULL);

				webkit_dom_html_anchor_element_set_href (
					WEBKIT_DOM_HTML_ANCHOR_ELEMENT (parent),
					new_href);

				if (appending_to_link) {
					gchar *tmp;

					tmp = g_strconcat (inner_html, text_to_append, NULL);
					webkit_dom_html_element_set_inner_html (
						WEBKIT_DOM_HTML_ELEMENT (parent),
						tmp,
						NULL);

					remove_node (node);

					g_free (tmp);
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

				inner_html = webkit_dom_html_element_get_inner_html (
					WEBKIT_DOM_HTML_ELEMENT (parent));
				new_href = g_strconcat (
						inner_html,
						appending_to_link ? text_to_append : "",
						NULL);

				webkit_dom_html_anchor_element_set_href (
					WEBKIT_DOM_HTML_ANCHOR_ELEMENT (parent),
					new_href);

				if (appending_to_link) {
					gchar *tmp;

					tmp = g_strconcat (inner_html, text_to_append, NULL);
					webkit_dom_html_element_set_inner_html (
						WEBKIT_DOM_HTML_ELEMENT (parent),
						tmp,
						NULL);

					remove_node (node);

					g_free (tmp);
				}

				g_free (new_href);
				g_free (inner_html);
			}

		}
		g_free (text);
		g_free (href);
	}

	g_match_info_free (match_info);
	g_regex_unref (regex);
	g_free (node_text);
}

void
dom_embed_style_sheet (WebKitDOMDocument *document,
                       const gchar *style_sheet_content)
{
	WebKitDOMElement *sheet;

	e_web_view_create_and_add_css_style_sheet (document, "-x-evo-composer-sheet");

	sheet = webkit_dom_document_get_element_by_id (document, "-x-evo-composer-sheet");
	webkit_dom_element_set_attribute (
		sheet,
		"type",
		"text/css",
		NULL);

	webkit_dom_html_element_set_inner_html (WEBKIT_DOM_HTML_ELEMENT (sheet), stylesheet_content, NULL);
}

void
dom_remove_embed_style_sheet (WebKitDOMDocument *document)
{
	WebKitDOMElement *sheet;

	sheet = webkit_dom_document_get_element_by_id (
		document, "-x-evo-composer-sheet");

	remove_node (WEBKIT_DOM_NODE (sheet));
}

void
dom_set_links_active (WebKitDOMDocument *document,
                      gboolean active)
{
	WebKitDOMElement *style;

	if (active) {
		style = webkit_dom_document_get_element_by_id (
				document, "--evolution-editor-style-a");
		if (style)
			remove_node (WEBKIT_DOM_NODE (style));
	} else {
		WebKitDOMHTMLHeadElement *head;
		head = webkit_dom_document_get_head (document);

		style = webkit_dom_document_create_element (document, "STYLE", NULL);
		webkit_dom_element_set_id (style, "--evolution-editor-style-a");
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
		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (
				webkit_dom_node_get_parent_node (parent)),
			UNICODE_ZERO_WIDTH_SPACE,
			NULL);
	}
}

static void
mark_node_as_paragraph_after_ending_list (WebKitDOMDocument *document)
{
	gint ii, length;
	WebKitDOMNodeList *list;

	/* When pressing Enter on empty line in the list WebKit will end that
	 * list and inserts <div><br></div> so mark it for wrapping */
	list = webkit_dom_document_query_selector_all (
		document, "body > div:not(.-x-evo-paragraph) > br", NULL);

	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_get_parent_node (
			webkit_dom_node_list_item (list, ii));

		dom_set_paragraph_style (WEBKIT_DOM_ELEMENT (node), -1, 0, "");
	}
	g_object_unref (list);
}

static gboolean
surround_text_with_paragraph_if_needed (WebKitDOMDocument *document,
                                        WebKitDOMNode *node)
{
	WebKitDOMNode *next_sibling = webkit_dom_node_get_next_sibling (node);
	WebKitDOMNode *prev_sibling = webkit_dom_node_get_previous_sibling (node);
	WebKitDOMElement *element;

	/* All text in composer has to be written in div elements, so if
	 * we are writing something straight to the body, surround it with
	 * paragraph */
	if (WEBKIT_DOM_IS_TEXT (node) &&
	    WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (node))) {
		element = dom_put_node_into_paragraph (
			document, node, dom_caret_position_node (document));

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
body_keypress_event_cb (WebKitDOMElement *element,
                        WebKitDOMUIEvent *event,
                        EHTMLEditorWebExtension *extension)
{
	glong key_code;

	e_html_editor_web_extension_set_return_key_pressed (extension, FALSE);
	e_html_editor_web_extension_set_space_key_pressed (extension, FALSE);

	key_code = webkit_dom_ui_event_get_key_code (event);
	if (key_code == 13)
		e_html_editor_web_extension_set_return_key_pressed (extension, TRUE);
	else if (key_code == 32)
		e_html_editor_web_extension_set_space_key_pressed (extension, TRUE);
}

static void
body_input_event_cb (WebKitDOMElement *element,
                     WebKitDOMEvent *event,
                     EHTMLEditorWebExtension *extension)
{
	gboolean html_mode;
	WebKitDOMDocument *document;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));
	range = dom_get_range (document);

	html_mode = is_in_html_mode (extension)
	set_content_changed (extension);

	if (extension->priv->magic_smileys && html_mode)
		html_editor_view_check_magic_smileys (view, range);

	if (e_html_editor_web_extension_get_return_key_pressed (extension) ||
	    e_html_editor_web_extension_get_space_key_pressed (extension)) {
		html_editor_view_check_magic_links (view, range, FALSE);
		mark_node_as_paragraph_after_ending_list (document);
		if (html_mode)
			fix_paragraph_structure_after_pressing_enter_after_smiley (document);
	} else {
		WebKitDOMNode *node;

		node = webkit_dom_range_get_end_container (range, NULL);

		if (surround_text_with_paragraph_if_needed (document, node)) {
			dom_restore_caret_position (document);
			node = webkit_dom_range_get_end_container (range, NULL);
			range = dom_get_range (document);
		}

		if (WEBKIT_DOM_IS_TEXT (node)) {
			gchar *text;

			text = webkit_dom_node_get_text_content (node);

			if (g_strcmp0 (text, "") != 0 && !g_unichar_isspace (g_utf8_get_char (text))) {
				WebKitDOMNode *prev_sibling;

				prev_sibling = webkit_dom_node_get_previous_sibling (node);

				if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling))
					html_editor_view_check_magic_links (view, range, FALSE);
			}
			g_free (text);
		}
	}

	node = webkit_dom_range_get_end_container (range, NULL);

	/* After toggling monospaced format, we are using UNICODE_ZERO_WIDTH_SPACE
	 * to move caret into right space. When this callback is called it is not
	 * necassary anymore so remove it */
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
		glong length = g_utf8_strlen (text, -1);
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
					selection, WEBKIT_DOM_ELEMENT (parent), -1, 0, "");
		}

		/* When new smiley is added we have to use UNICODE_HIDDEN_SPACE to set the
		 * caret position to right place. It is removed when user starts typing. But
		 * when the user will press left arrow he will move the caret into
		 * smiley wrapper. If he will start to write there we have to move the written
		 * text out of the wrapper and move caret to right place */
		if (WEBKIT_DOM_IS_ELEMENT (parent) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-smiley-wrapper")) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				dom_crate_caret_position_node (document),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				node,
				webkit_dom_node_get_next_sibling (parent),
				NULL);
			dom_restore_caret_position (document);
		}
	}

	/* Writing into quoted content */
	if (html_mode) {
		gint citation_level;
		WebKitDOMElement *selection_start_marker, *selection_end_marker;
		WebKitDOMNode *node, *parent;
		WebKitDOMRange *range;

		range = dom_get_range (document);
		node = webkit_dom_range_get_end_container (range, NULL);

		citation_level = get_citation_level (node, FALSE);
		if (citation_level == 0)
			return;

		selection_start_marker = webkit_dom_document_query_selector (
			document, "span#-x-evo-selection-start-marker", NULL);
		if (selection_start_marker)
			return;

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

		/* We have to process elements only inside normal block */
		parent = WEBKIT_DOM_NODE (get_parent_block_element (
			WEBKIT_DOM_NODE (selection_start_marker)));
		if (WEBKIT_DOM_IS_HTML_PRE_ELEMENT (parent)) {
			dom_selection_restore (document);
			return;
		}

		if (selection_start_marker) {
			gchar *content;
			gint text_length, word_wrap_length, length;
			WebKitDOMElement *block;
			gboolean remove_quoting = FALSE;

			word_wrap_length = extension->priv->word_wrap_length;
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
				remove_quoting_from_element (block);

				block = dom_wrap_paragraph_length (document, block, length);
				webkit_dom_node_normalize (WEBKIT_DOM_NODE (block));
				quote_plain_text_element_after_wrapping (
					document, WEBKIT_DOM_ELEMENT (block), citation_level);
				selection_start_marker = webkit_dom_document_query_selector (
					document, "span#-x-evo-selection-start-marker", NULL);
				if (!selection_start_marker)
					add_selection_markers_into_element_end (
						document,
						WEBKIT_DOM_ELEMENT (block),
						NULL,
						NULL);

				dom_selection_restore (document);
				dom_force_spell_check_for_current_paragraph (document);
				return;
			}
		}
		dom_selection_restore (document);
	}
}

static void
remove_input_event_listener_from_body (EHTMLEditorWebExtension *extension,
                                       WebKitDOMDocument *document)
{
	if (!extension->priv->body_input_event_removed) {
		webkit_dom_event_target_remove_event_listener (
			WEBKIT_DOM_EVENT_TARGET (
				webkit_dom_document_get_body (document)),
			"input",
			G_CALLBACK (body_input_event_cb),
			FALSE);

		extension->priv->body_input_event_removed = TRUE;
	}
}

static void
register_input_event_listener_on_body (EHTMLEditorWebExtension *extension,
                                       WebKitDOMDocument *document)
{
	if (extension->priv->body_input_event_removed) {
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (
				webkit_dom_document_get_body (document)),
			"input",
			G_CALLBACK (body_input_event_cb),
			FALSE,
			extension);

		extension->priv->body_input_event_removed = FALSE;
	}
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
	remove_node (parent);
}

static void
dom_convert_and_insert_html_into_selection (WebKitDOMDocument *document,
                                            EHTMLEditorWebExtension *extension,
                                            const gchar *html,
                                            gboolean is_html)
{
	gboolean has_selection;
	gchar *inner_html;
	gint citation_level;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	remove_input_event_listener_from_body (document);

	element = webkit_dom_document_create_element (document, "div", NULL);
	if (is_html) {
		gchar *inner_text;

		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (element), html, NULL);
		inner_text = webkit_dom_html_element_get_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (element));
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (element), inner_text, NULL);

		g_free (inner_text);
	} else
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (element), html, NULL);

	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element));
	parse_html_into_paragraphs (document, element, inner_html);

	g_free (inner_html);

	has_selection = !dom_selection_is_collapsed (document);

	range = dom_get_range (document);
	node = webkit_dom_range_get_end_container (range, NULL);
	citation_level = get_citation_level (node, FALSE);
	/* Pasting into the citation */
	if (citation_level > 0) {
		gint length;
		gint word_wrap_length = extension->priv->word_wrap_length;
		WebKitDOMElement *selection_start_marker, *selection_end_marker;
		WebKitDOMElement *br;
		WebKitDOMNode *first_paragraph, *last_paragraph;
		WebKitDOMNode *child, *parent;

		dom_selection_save (selection);
		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		first_paragraph = webkit_dom_node_get_first_child (
			WEBKIT_DOM_NODE (element));
		last_paragraph = webkit_dom_node_get_last_child (
			WEBKIT_DOM_NODE (element));

		length = word_wrap_length - 2 * citation_level;

		/* Pasting text that was parsed just into one paragraph */
		if (webkit_dom_node_is_same_node (first_paragraph, last_paragraph)) {
			WebKitDOMNode *child, *parent;

			parent = get_parent_block_node_from_child (
				WEBKIT_DOM_NODE (selection_start_marker));

			remove_quoting_from_element (WEBKIT_DOM_ELEMENT (parent));
			remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (parent));

			while ((child = webkit_dom_node_get_first_child (first_paragraph)))
				webkit_dom_node_insert_before (
					parent,
					child,
					WEBKIT_DOM_NODE (selection_start_marker),
					NULL);

			parent = WEBKIT_DOM_NODE (
				dom_wrap_paragraph_length (
					document, WEBKIT_DOM_ELEMENT (parent), length));
			webkit_dom_node_normalize (parent);
			quote_plain_text_element_after_wrapping (
				document, WEBKIT_DOM_ELEMENT (parent), citation_level);

			goto delete;
		}

		/* Pasting content parsed into the multiple paragraphs */
		parent = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		remove_quoting_from_element (WEBKIT_DOM_ELEMENT (parent));
		remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (parent));

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
			WEBKIT_DOM_NODE (selection_end_marker)),

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
			dom_create_caret_position_node (document),
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
				document, WEBKIT_DOM_ELEMENT (child), length));
			quote_plain_text_element_after_wrapping (
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
				document, WEBKIT_DOM_ELEMENT (last_paragraph), length));
		quote_plain_text_element_after_wrapping (
			document, WEBKIT_DOM_ELEMENT (last_paragraph), citation_level);

		remove_quoting_from_element (WEBKIT_DOM_ELEMENT (parent));
		remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (parent));

		parent = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		parent = WEBKIT_DOM_NODE (dom_wrap_paragraph_length (
			document, WEBKIT_DOM_ELEMENT (parent), length));
		quote_plain_text_element_after_wrapping (
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
		dom_selection_restore (document);
		/* Remove the text that was meant to be replaced by the pasted text */
		if (has_selection)
			dom_exec_command (
				document, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);

		dom_restore_caret_position (document);
		goto out;
	}

	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element));
	dom_exec_command (
		document, E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML, inner_html);
	g_free (inner_html);

	dom_selection_save (document);

	element = webkit_dom_document_query_selector (
		document, "* > br.-x-evo-first-br", NULL);
	if (element) {
		WebKitDOMNode *next_sibling;
		WebKitDOMNode *parent;

		parent = webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (element));

		next_sibling = webkit_dom_node_get_next_sibling (parent);
		if (next_sibling)
			remove_node (WEBKIT_DOM_NODE (parent));
		else
			webkit_dom_element_remove_attribute (element, "class");
	}

	element = webkit_dom_document_query_selector (
		document, "* > br.-x-evo-last-br", NULL);
	if (element) {
		WebKitDOMNode *parent;
		WebKitDOMNode *child;
		WebKitDOMElement *selection_marker;

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

		selection_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		if (has_selection) {
			/* Everything after the selection end marker have to be in separate
			 * paragraph */
			child = webkit_dom_node_get_next_sibling (
				WEBKIT_DOM_NODE (selection_marker));
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
						WEBKIT_DOM_NODE (selection_marker))),
				parent,
				webkit_dom_node_get_next_sibling (
					webkit_dom_node_get_parent_node (
						WEBKIT_DOM_NODE (selection_marker))),
				NULL);
			node = parent;
		} else {
			node = webkit_dom_node_get_next_sibling (parent);
			if (!node)
				fix_structure_after_pasting_multiline_content (parent);
		}

		if (node) {
			/* Restore caret on the end of pasted text */
			webkit_dom_node_insert_before (
				node,
				WEBKIT_DOM_NODE (selection_marker),
				webkit_dom_node_get_first_child (node),
				NULL);

			selection_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			webkit_dom_node_insert_before (
				node,
				WEBKIT_DOM_NODE (selection_marker),
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
		WebKitDOMElement *selection_start_marker, *selection_end_marker;
		WebKitDOMNode *paragraph, *parent;

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		paragraph = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		parent = webkit_dom_node_get_parent_node (paragraph);
		if (element_has_class (WEBKIT_DOM_ELEMENT (paragraph), "-x-evo-paragraph") &&
		    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-paragraph"))
			fix_structure_after_pasting_multiline_content (paragraph);

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (selection_start_marker)),
			WEBKIT_DOM_NODE (selection_end_marker),
			webkit_dom_node_get_next_sibling (
				WEBKIT_DOM_NODE (selection_start_marker)),
			NULL);
	}

	dom_selection_restore (document);
 out:
	dom_force_spell_check (document);
	dom_scroll_to_caret (document);

	register_input_event_listener_on_body (document);
}

void
dom_quote_and_insert_text_into_selection (EHTMLEditorWebExtension *extension,
                                          WebKitDOMDocument *document,
                                          const gchar *text)
{
	gchar *escaped_text;
	WebKitDOMElement *blockquote, *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	if (!text || !*text)
		return;

	/* This is a trick to escape any HTML characters (like <, > or &).
	 * <textarea> automatically replaces all these unsafe characters
	 * by &lt;, &gt; etc. */
	element = webkit_dom_document_create_element (document, "textarea", NULL);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element), text, NULL);
	escaped_text = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element));

	element = webkit_dom_document_create_element (document, "pre", NULL);

	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (element), escaped_text, NULL);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (element),
		dom_create_caret_position_node (document),
		NULL);

	blockquote = webkit_dom_document_create_element (document, "blockquote", NULL);
	webkit_dom_element_set_attribute (blockquote, "type", "cite", NULL);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (blockquote), WEBKIT_DOM_NODE (element), NULL);

	if (!is_in_html_mode (extension))
		dom_quote_plain_text_element (document, element);

	range = dom_get_range (document);
	node = webkit_dom_range_get_end_container (range, NULL);

	webkit_dom_node_append_child (
		webkit_dom_node_get_parent_node (node),
		WEBKIT_DOM_NODE (blockquote),
		NULL);

	dom_restore_caret_position (document);

	dom_force_spell_check_for_current_paragraph (document);

	g_free (escaped_text);
}

gboolean
dom_change_quoted_block_to_normal (EHTMLEditorWebExtension *extension,
                                   WebKitDOMDocument *document)
{
	gboolean html_mode;
	gint citation_level, success = FALSE;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *block;

	html_mode = is_in_html_mode (extension);

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
		if (webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), ".-x-evo-quoted", NULL)) {

			WebKitDOMNode *prev_sibling;

			webkit_dom_node_normalize (WEBKIT_DOM_NODE (block));

			prev_sibling = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (selection_start_marker));

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
				webkit_dom_node_get_parent_element (
					WEBKIT_DOM_NODE (block)));
	}

	if (!success)
		return FALSE;

	if (citation_level == 1) {
		gchar *inner_html;
		WebKitDOMElement *paragraph;

		inner_html = webkit_dom_html_element_get_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (block));
		webkit_dom_element_set_id (
			WEBKIT_DOM_ELEMENT (block), "-x-evo-to-remove");

		paragraph = insert_new_line_into_citation (document, inner_html);
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

			remove_quoting_from_element (paragraph);
			remove_wrapping_from_element (paragraph);
		}

		if (block)
			remove_node (WEBKIT_DOM_NODE (block));
		block = webkit_dom_document_get_element_by_id (
			document, "-x-evo-to-remove");
		if (block)
			remove_node (WEBKIT_DOM_NODE (block));

		if (paragraph)
			remove_node_if_empty (
				webkit_dom_node_get_next_sibling (
					WEBKIT_DOM_NODE (paragraph)));
	}

	if (citation_level > 1) {
		gint length, word_wrap_length;
		WebKitDOMNode *parent;

		word_wrap_length = extension->priv->word_wrap_length;
		length =  word_wrap_length - 2 * (citation_level - 1);

		if (html_mode) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (block),
				WEBKIT_DOM_NODE (selection_start_marker),
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (block)),
				NULL);
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (block),
				WEBKIT_DOM_NODE (selection_end_marker),
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (block)),
				NULL);

		}

		remove_quoting_from_element (block);
		remove_wrapping_from_element (block);

		block = dom_wrap_paragraph_length (document, block, length);
		webkit_dom_node_normalize (WEBKIT_DOM_NODE (block));
		quote_plain_text_element_after_wrapping (document, block, citation_level - 1);

		parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (block));

		if (!webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (block))) {
			/* Currect block is in the beginning of citation, just move it
			 * before the citation where already is */
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (block),
				parent,
				NULL);
		} else if (!webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (block))) {
			/* Currect block is at the end of the citation, just move it
			 * after the citation where already is */
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (block),
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
			child = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (block));
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
				WEBKIT_DOM_NODE (block),
				clone,
				NULL);
		}
	}

	return success;
}

static gboolean
prevent_from_deleting_last_element_in_body (WebKitDOMDocument *document)
{
	gboolean ret_val = FALSE;
	WebKitDOMHTMLElement *body;
	WebKitDOMNodeList *list;

	body = webkit_dom_document_get_body (document);

	list = webkit_dom_node_get_child_nodes (WEBKIT_DOM_NODE (body));

	if (webkit_dom_node_list_get_length (list) <= 1) {
		gchar *content;

		content = webkit_dom_node_get_text_content (WEBKIT_DOM_NODE (body));

		if (!*content)
			ret_val = TRUE;

		g_free (content);

		if (webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (body), "img", NULL))
			ret_val = FALSE;
	}
	g_object_unref (list);

	return ret_val;
}

static void
insert_quote_symbols (WebKitDOMHTMLElement *element,
                      gint quote_level,
                      gboolean skip_first,
                      gboolean insert_newline)
{
	gchar *text;
	gint ii;
	GString *output;
	gchar *quotation;

	if (!WEBKIT_DOM_IS_HTML_ELEMENT (element))
		return;

	text = webkit_dom_html_element_get_inner_html (element);
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

	webkit_dom_html_element_set_inner_html (element, output->str, NULL);
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

	if (WEBKIT_DOM_IS_HTML_ELEMENT (node)) {
		insert_quote_symbols (
			WEBKIT_DOM_HTML_ELEMENT (node), quote_level, FALSE, FALSE);
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
		element_has_tag (WEBKIT_DOM_ELEMENT (prev_sibling), "u"));

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
		WEBKIT_DOM_HTML_ELEMENT (wrapper),
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
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element), quotation, NULL);

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
element_is_selection_marker (WebKitDOMElement *element)
{
	gboolean is_marker = FALSE;

	is_marker =
		element_has_id (element, "-x-evo-selection-start-marker") ||
		element_has_id (element, "-x-evo-selection-end-marker");

	return is_marker;
}

static gboolean
check_if_suppress_next_node (WebKitDOMNode *node)
{
	if (!node)
		return FALSE;

	if (node && WEBKIT_DOM_IS_ELEMENT (node))
		if (element_is_selection_marker (WEBKIT_DOM_ELEMENT (node)))
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

	webkit_dom_html_element_set_outer_html (
		WEBKIT_DOM_HTML_ELEMENT (node),
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

		if (element_has_id (WEBKIT_DOM_ELEMENT (node), "-x-evo-caret-position")) {
			if (quote_level > 0)
				element_add_class (
					WEBKIT_DOM_ELEMENT (node), "-x-evo-caret-quoting");

			move_next = TRUE;
			suppress_next = TRUE;
			next = FALSE;
			goto next_node;
		}

		if (element_is_selection_marker (WEBKIT_DOM_ELEMENT (node))) {
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
			element_has_tag (WEBKIT_DOM_ELEMENT (node), "u");

		if (is_html_node) {
			gboolean wrap_br;

			wrap_br =
				prev_sibling &&
				WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling) &&
				element_has_class (
					WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-wrap-br");

			if (!prev_sibling || wrap_br)
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);

			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling) && !wrap_br)
				insert_quote_symbols_before_node (
					document, prev_sibling, quote_level, TRUE);

			move_next = TRUE;
			goto next_node;
		}

		/* If element doesn't have children, we can quote it */
		if (is_citation_node (node)) {
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
			     !is_citation_node (parent))) {
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

		if (is_citation_node (prev_sibling)) {
			insert_quote_symbols_before_node (
				document, node, quote_level, FALSE);
			goto next_node;
		}

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (node) &&
		    !next_sibling &&
		    element_is_selection_marker (WEBKIT_DOM_ELEMENT (prev_sibling))) {
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
		if (is_citation_node (node)) {
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
	for  (ii = 0; ii < length; ii++)
		remove_node (webkit_dom_node_list_item (list, ii));

	webkit_dom_node_normalize (element_clone);
	quote_plain_text_recursive (
		document, element_clone, element_clone, level);

	/* Set citation as quoted */
	if (is_citation_node (element_clone))
		element_add_class (
			WEBKIT_DOM_ELEMENT (element_clone),
			"-x-evo-plaintext-quoted");

	/* Replace old element with one, that is quoted */
	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
		element_clone,
		WEBKIT_DOM_NODE (element),
		NULL);

	g_object_unref (list);
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
WebKitDOMElement *
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
void
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

		if (is_citation_node (WEBKIT_DOM_NODE (element))) {
			element_remove_class (element, "-x-evo-plaintext-quoted");
			remove_quoting_from_element (element);
		}
	}
	g_object_unref (paragraphs);
}

static gboolean
create_anchor_for_link (const GMatchInfo *info,
                        GString *res,
                        gpointer data)
{
	gint offset = 0;
	gchar *match;
	gboolean address_surrounded;

	match = g_match_info_fetch (info, 0);

	address_surrounded =
		strstr (match, "@") &&
		g_str_has_prefix (match, "&lt;") &&
		g_str_has_suffix (match, "&gt;");

	if (address_surrounded)
		offset += 4;

	if (g_str_has_prefix (match, "&nbsp;"))
		offset += 6;

	if (address_surrounded)
		g_string_append (res, "&lt;");

	g_string_append (res, "<a href=\"");
	if (strstr (match, "@")) {
		g_string_append (res, "mailto:");
		g_string_append (res, match + offset);
		if (address_surrounded)
			g_string_truncate (res, res->len - 4);

		g_string_append (res, "\">");
		g_string_append (res, match + offset);
		if (address_surrounded)
			g_string_truncate (res, res->len - 4);
	} else {
		g_string_append (res, match + offset);
		g_string_append (res, "\">");
		g_string_append (res, match + offset);
	}
	g_string_append (res, "</a>");

	if (address_surrounded)
		g_string_append (res, "&gt;");

	g_free (match);

	return FALSE;
}

static gboolean
replace_to_nbsp (const GMatchInfo *info,
                 GString *res,
                 gboolean use_nbsp)
{
	gchar *match;
	const gchar *string, *previous_tab;
	gint ii, length = 0, start = 0;

	match = g_match_info_fetch (info, 0);
	g_match_info_fetch_pos (info, 0, &start, NULL);
	string = g_match_info_get_string (info);

	if (start > 0) {
		previous_tab = g_strrstr_len (string, start, "\x9");
		if (previous_tab && *previous_tab) {
			const char *act_tab = NULL;
			act_tab = strstr (previous_tab + 1, "\x9");

			if (act_tab && *act_tab) {
				length = act_tab - previous_tab - 1;
				length = TAB_LENGTH - length;
			}
		}
	}

	if (length == 0) {
		if (strstr (match, "\x9")) {
			gint tab_count = strlen (match);
			length = TAB_LENGTH - (start %  TAB_LENGTH);
			length += (tab_count - 1) * TAB_LENGTH;
		} else
			length = strlen (match);
	}

	for (ii = 0; ii < length; ii++)
		g_string_append (res, "&nbsp;");

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
append_new_paragraph (WebKitDOMElement *parent,
                      WebKitDOMElement **paragraph)
{
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (parent),
		WEBKIT_DOM_NODE (*paragraph),
		NULL);

	*paragraph = NULL;
}

static WebKitDOMElement *
create_and_append_new_paragraph (WebKitDOMDocument *document,
                                 WebKitDOMElement *parent,
                                 const gchar *content)
{
	WebKitDOMElement *paragraph;

	paragraph = dom_get_paragraph_element (document, -1, 0);

	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (paragraph),
		content,
		NULL);

	append_new_paragraph (parent, &paragraph);

	return paragraph;
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
	gchar *decoded_text;
	glong length = 0;
	WebKitDOMElement *decode;

	decode = webkit_dom_document_create_element (document, "DIV", NULL);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (decode), line_text, NULL);

	decoded_text = webkit_dom_html_element_get_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (decode));
	length = g_utf8_strlen (decoded_text, -1);

	g_free (decoded_text);
	g_object_unref (decode);

	return length;
}

static gboolean
check_if_end_paragraph (const gchar *input,
                        glong length)
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
	}

	return FALSE;
}

/* This parses the HTML code (that contains just text, &nbsp; and BR elements)
 * into paragraphs.
 * HTML code in that format we can get by taking innerText from some element,
 * setting it to another one and finally getting innerHTML from it */
static void
parse_html_into_paragraphs (WebKitDOMDocument *document,
                            WebKitDOMElement *blockquote,
                            const gchar *html)
{
	gboolean ignore_next_br = FALSE;
	gboolean first_element = TRUE;
	gboolean citation_was_first_element = FALSE;
	const gchar *prev_br, *next_br;
	gchar *inner_html;
	GRegex *regex_nbsp = NULL, *regex_links = NULL;
	GString *start, *end;
	WebKitDOMElement *paragraph = NULL;

	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (blockquote), "", NULL);

	prev_br = html;
	next_br = strstr (prev_br, "<br>");

	/* Replace single spaces on the beginning of line, 2+ spaces and
	 * tabulators with non breaking spaces */
	regex_nbsp = g_regex_new ("^\\s{1}|\\s{2,}|\x9", 0, 0, NULL);
	regex_links = g_regex_new (URL_PATTERN, 0, 0, NULL);

	while (next_br) {
		gboolean local_ignore_next_br = ignore_next_br;
		gboolean prevent_block = TRUE;
		const gchar *citation = NULL, *citation_end = NULL;
		const gchar *rest = NULL, *with_br = NULL;
		gchar *to_insert = NULL;

		ignore_next_br = FALSE;

		to_insert = g_utf8_substring (
			prev_br, 0, g_utf8_pointer_to_offset (prev_br, next_br));

		with_br = strstr (to_insert, "<br>");
		citation = strstr (to_insert, "##CITATION_");
		if (citation) {
			gchar *citation_mark;

			if (strstr (citation, "END##")) {
				ignore_next_br = TRUE;
				if (paragraph)
					append_new_paragraph (blockquote, &paragraph);
			}

			citation_end = strstr (citation + 2, "##");
			if (citation_end)
				rest = citation_end + 2;

			if (first_element)
				citation_was_first_element = TRUE;

			if (paragraph)
				append_new_paragraph (blockquote, &paragraph);

			citation_mark = g_utf8_substring (
				citation, 0, g_utf8_pointer_to_offset (citation, rest));

			append_citation_mark (document, blockquote, citation_mark);

			g_free (citation_mark);
		} else
			rest = with_br ?
				to_insert + 4 + (with_br - to_insert) : to_insert;

		if (!rest)
			goto next;

		if (*rest) {
			gboolean empty = FALSE;
			gchar *truncated = g_strdup (rest);
			gchar *rest_to_insert;

			g_strchomp (truncated);
			empty = !*truncated && strlen (rest) > 0;

			if (strchr (" +-@*=", *rest))
				prevent_block = FALSE;

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
				truncated = g_regex_replace_eval (
					regex_links,
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
				paragraph = create_and_append_new_paragraph (
					document, blockquote, "<br>");
			} else if (prevent_block) {
				gchar *html;
				gchar *new_content;

				if (!paragraph)
					paragraph = dom_get_paragraph_element (
						document, -1, 0);

				html = webkit_dom_html_element_get_inner_html (
					WEBKIT_DOM_HTML_ELEMENT (paragraph));

				new_content = g_strconcat (
					html && *html ? html : "",
					html && *html ? " " : "",
					rest_to_insert ? rest_to_insert : "<br>",
					NULL),

				webkit_dom_html_element_set_inner_html (
					WEBKIT_DOM_HTML_ELEMENT (paragraph),
					new_content,
					NULL);

				g_free (html);
				g_free (new_content);
			} else
				paragraph = create_and_append_new_paragraph (
					document, blockquote, rest_to_insert);

			if (rest_to_insert && *rest_to_insert && prevent_block && paragraph) {
				glong length = 0;

				if (strstr (rest, "&"))
					length = get_decoded_line_length (document, rest);
				else
					length = g_utf8_strlen (rest, -1);

				/* End the block if there is line with less that 62 characters. */
				/* The shorter line can also mean that there is a long word on next
				 * line (and the line was wrapped). So look at it and decide what to do. */
				if (length < 62 && check_if_end_paragraph (next_br, length))
					append_new_paragraph (blockquote, &paragraph);

				if (length > 72)
					append_new_paragraph (blockquote, &paragraph);
			}

			citation_was_first_element = FALSE;

			g_free (rest_to_insert);
		} else if (with_br) {
			if (!citation && (!local_ignore_next_br || citation_was_first_element)) {
				if (paragraph)
					append_new_paragraph (blockquote, &paragraph);

				paragraph = create_and_append_new_paragraph (
					document, blockquote, "<br>");

				citation_was_first_element = FALSE;
			} else if (first_element && !citation_was_first_element) {
				paragraph = create_and_append_new_paragraph (
					document,
					blockquote,
					"<br class=\"-x-evo-first-br\">");
			}
		}
 next:
		first_element = FALSE;
		prev_br = next_br;
		next_br = strstr (prev_br + 4, "<br>");
		g_free (to_insert);
	}

	if (paragraph)
		append_new_paragraph (blockquote, &paragraph);

	if (g_utf8_strlen (prev_br, -1) > 0) {
		gchar *rest_to_insert;
		gchar *truncated = g_strdup (
			g_str_has_prefix (prev_br, "<br>") ? prev_br + 4 : prev_br);

		/* On the end on the HTML there is always an extra BR element,
		 * so skip it and if there was another BR element before it mark it. */
		if (truncated && !*truncated) {
			WebKitDOMNode *child;

			child = webkit_dom_node_get_last_child (
				WEBKIT_DOM_NODE (blockquote));
			if (child) {
				child = webkit_dom_node_get_first_child (child);
				if (child && WEBKIT_DOM_IS_HTML_BR_ELEMENT (child)) {
					element_add_class (
						WEBKIT_DOM_ELEMENT (child),
						"-x-evo-last-br");
				}
			}
			g_free (truncated);
			goto end;
		}

		if (g_ascii_strncasecmp (truncated, "##CITATION_END##", 16) == 0) {
			append_citation_mark (document, blockquote, truncated);
			g_free (truncated);
			goto end;
		}

		g_strchomp (truncated);

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
			truncated = g_regex_replace_eval (
				regex_links,
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
			create_and_append_new_paragraph (
				document, blockquote, "<br>");
		else
			create_and_append_new_paragraph (
				document, blockquote, rest_to_insert);

		g_free (rest_to_insert);
	}

 end:
	/* Replace text markers with actual HTML blockquotes */
	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (blockquote));
	start = e_str_replace_string (
		inner_html, "##CITATION_START##","<blockquote type=\"cite\">");
	end = e_str_replace_string (
		start->str, "##CITATION_END##", "</blockquote>");
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (blockquote), end->str, NULL);

	g_regex_unref (regex_nbsp);
	g_regex_unref (regex_links);
	g_free (inner_html);
	g_string_free (start, TRUE);
	g_string_free (end, TRUE);
}

static void
mark_citation (WebKitDOMElement *citation)
{
	gchar *inner_html, *surrounded;

	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (citation));

	surrounded = g_strconcat (
		"<span>##CITATION_START##</span>", inner_html,
		"<span>##CITATION_END##</span>", NULL);

	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (citation), surrounded, NULL);

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
		count ++;

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

	list = webkit_dom_document_query_selector_all (
		document, "blockquote[type=cite] > div.-x-evo-paragraph", NULL);

	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		gint citation_level;
		WebKitDOMNode *child;

		child = webkit_dom_node_list_item (list, ii);
		citation_level = get_citation_level (child, TRUE);
		quote_plain_text_element_after_wrapping (
			document, WEBKIT_DOM_ELEMENT (child), citation_level);
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
	}
	g_object_unref (attributes);

	/* Remove everything from HEAD element */
	while (webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (head)))
		remove_node (webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (head)));

	/* Remove non Evolution attributes from BODY element */
	attributes = webkit_dom_element_get_attributes (WEBKIT_DOM_ELEMENT (body));
	length = webkit_dom_named_node_map_get_length (attributes);
	for (ii = length - 1; ii >= 0; ii--) {
		gchar *name;
		WebKitDOMNode *node = webkit_dom_named_node_map_item (attributes, ii);

		name = webkit_dom_node_get_local_name (node);

		if (!g_str_has_prefix (name, "data-"))
			webkit_dom_element_remove_attribute_node (
				WEBKIT_DOM_ELEMENT (body),
				WEBKIT_DOM_ATTR (node),
				NULL);

		g_free (name);
	}
	g_object_unref (attributes);
}

static void
html_editor_convert_view_content (EHTMLEditorWebExtension *extension,
                                  WebKitDOMDocument *document,
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

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	start_bottom = g_settings_get_boolean (settings, "composer-reply-start-bottom");
	g_object_unref (settings);

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
		document, ".-x-evo-paragraph", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++)
		remove_node (webkit_dom_node_list_item (list, ii));
	g_object_unref (list);

	/* Insert the paragraph where the caret will be. */
	paragraph = prepare_paragraph (document, TRUE);
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
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (wrapper),
				WEBKIT_DOM_NODE (signature),
				start_bottom ?
					WEBKIT_DOM_NODE (content_wrapper) :
					webkit_dom_node_get_next_sibling (
						WEBKIT_DOM_NODE (paragraph)),
				NULL);
			/* Insert NL after the signature */
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (wrapper),
				WEBKIT_DOM_NODE (prepare_paragraph (
					selection, document, FALSE)),
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
		WebKitDOMNode *node, *child;

		node = webkit_dom_node_list_item (list, ii);
		while ((child = webkit_dom_node_get_first_child (node))) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (wrapper),
				child,
				WEBKIT_DOM_NODE (content_wrapper),
				NULL);
		}

		remove_node (node);
	}
	g_object_unref (list);

	/* Move headers to body */
	list = webkit_dom_document_query_selector_all (
		document, "span.-x-evo-to-body[data-headers]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node, *child;

		node = webkit_dom_node_list_item (list, ii);
		while ((child = webkit_dom_node_get_first_child (node))) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (wrapper),
				child,
				WEBKIT_DOM_NODE (content_wrapper),
				NULL);
		}

		remove_node (node);
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

	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (content_wrapper));

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

	/* FIXME XXX */
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
		parse_html_into_paragraphs (document, content_wrapper, inner_html);

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

	if (e_html_editor_web_extension_is_message_from_edit_as_new (extension) ||
	    e_html_editor_web_extension_get_remove_initial_input_line (extension)) {
		WebKitDOMNode *child;

		remove_node (WEBKIT_DOM_NODE (paragraph));
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
		if (child)
			add_selection_markers_into_element_start (
				document, WEBKIT_DOM_ELEMENT (child), NULL, NULL);
	}

	paragraph = webkit_dom_document_query_selector (document, "br.-x-evo-last-br", NULL);
	if (paragraph)
		webkit_dom_element_remove_attribute (paragraph, "class");
	paragraph = webkit_dom_document_query_selector (document, "br.-x-evo-first-br", NULL);
	if (paragraph)
		webkit_dom_element_remove_attribute (paragraph, "class");

	if (!is_in_html_mode (extension)) {
		dom_wrap_paragraphs_in_document (document);

		quote_plain_text_elements_after_wrapping_in_document (document);
	}

	clear_attributes (document);

	dom_selection_restore (selection);
	dom_force_spell_check (document);

	/* Register on input event that is called when the content (body) is modified */
	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"input",
		G_CALLBACK (body_input_event_cb),
		FALSE,
		document);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"keypress",
		G_CALLBACK (body_keypress_event_cb),
		FALSE,
		extension);


	g_free (inner_html);
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
process_blockquote (WebKitDOMElement *blockquote)
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
		webkit_dom_html_element_set_outer_html (
			WEBKIT_DOM_HTML_ELEMENT (quoted_node), text_content, NULL);

		g_free (text_content);
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
		webkit_dom_html_element_set_outer_html (
			WEBKIT_DOM_HTML_ELEMENT (quoted_node), text_content, NULL);

		g_free (text_content);
	}
	g_object_unref (list);

	if (element_has_class (blockquote, "-x-evo-indented")) {
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
					if (WEBKIT_DOM_IS_TEXT (node)) {
						/* append text from line */
						gchar *text_content;
						text_content = webkit_dom_node_get_text_content (node);
						g_string_append (line, text_content);
						g_free (text_content);
					}
					if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (node) &&
					    element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br")) {
						g_string_append (line, "\n");
						/* put spaces before line characters -> wordwraplength - indentation */
						for (ii = 0; ii < level; ii++)
							g_string_append (line, indent_per_level);
						g_string_append (item_value, line->str);
						g_string_erase (line, 0, -1);
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

				space = g_strnfill (SPACES_PER_LIST_LEVEL - 2 - length, ' ');
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

				/* Value already containes dot and space */
				space = g_strnfill (SPACES_PER_LIST_LEVEL - strlen (value), ' ');
				item_str = g_strdup_printf (
					"%s%s%s", space, value, item_value->str);
				g_free (space);
				g_free (value);
			}

			if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT) {
				for (ii = 0; ii < level - 1; ii++) {
					g_string_append (output, indent_per_level);
				}
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
		} else if (WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (item) ||
			   WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (item)) {
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
	webkit_dom_element_remove_attribute (element, "x-evo-smiley");
	webkit_dom_element_remove_attribute (element, "data-converted");
	webkit_dom_element_remove_attribute (element, "data-edit-as-new");
	webkit_dom_element_remove_attribute (element, "data-evo-draft");
	webkit_dom_element_remove_attribute (element, "data-inline");
	webkit_dom_element_remove_attribute (element, "data-uri");
	webkit_dom_element_remove_attribute (element, "data-message");
	webkit_dom_element_remove_attribute (element, "data-name");
	webkit_dom_element_remove_attribute (element, "data-new-message");
	webkit_dom_element_remove_attribute (element, "spellcheck");
}
/*
static void
remove_style_attributes (WebKitDOMElement *element)
{
	webkit_dom_element_remove_attribute (element, "bgcolor");
	webkit_dom_element_remove_attribute (element, "background");
	webkit_dom_element_remove_attribute (element, "style");
}
*/
static gboolean
replace_to_whitespaces (const GMatchInfo *info,
                        GString *res,
                        gpointer data)
{
	gint ii, length = 0;
	gint chars_count = GPOINTER_TO_INT (data);

	length = TAB_LENGTH - (chars_count %  TAB_LENGTH);

	for (ii = 0; ii < length; ii++)
		g_string_append (res, " ");

	return FALSE;
}

static void
process_elements (EHTMLEditorWebExtension *extension,
                  WebKitDOMNode *node,
                  gboolean to_html,
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
				gchar *name, *value;
				WebKitDOMNode *node =
					webkit_dom_named_node_map_item (
						attributes, ii);

				name = webkit_dom_node_get_local_name (node);
				value = webkit_dom_node_get_node_value (node);

				g_string_append (buffer, name);
				g_string_append (buffer, "=\"");
				g_string_append (buffer, value);
				g_string_append (buffer, "\" ");

				g_free (name);
				g_free (value);
			}
			g_string_append (buffer, ">");
			g_object_unref (attributes);
		}
		if (to_html)
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
			gint char_count = 0;

			content = webkit_dom_node_get_text_content (child);
			if (!changing_mode && to_plain_text) {
				/* Replace tabs with 8 whitespaces, otherwise they got
				 * replaced by single whitespace */
				if (strstr (content, "\x9")) {
					if (buffer->str && *buffer->str) {
						gchar *start_of_line = g_strrstr_len (
							buffer->str, -1, "\n") + 1;

						if (start_of_line && *start_of_line)
								char_count = strlen (start_of_line);
					} else
						char_count = 0;

					regex = g_regex_new ("\x9", 0, 0, NULL);
					tmp = g_regex_replace_eval (
						regex,
						content,
						-1,
						0,
						0,
						(GRegexEvalCallback) replace_to_whitespaces,
						GINT_TO_POINTER (char_count),
						NULL);

					g_string_append (buffer, tmp);
					g_free (tmp);
					g_free (content);
					content = webkit_dom_node_get_text_content (child);
					g_regex_unref (regex);
				}
			}

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

				if (strstr (content, UNICODE_NBSP)) {
					GString *nbsp_free;

					nbsp_free = e_str_replace_string (
						content, UNICODE_NBSP, " ");

					g_free (content);
					content = g_string_free (nbsp_free, FALSE);
				}

				class = webkit_dom_element_get_class_name (WEBKIT_DOM_ELEMENT (node));
				if ((css_align = strstr (class, "-x-evo-align-"))) {
					gchar *align;
					gchar *content_with_align;
					gint length;
					gint word_wrap_length = extension->priv->word_wrap_length;

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

		/* Leave caret position untouched */
		if (element_has_id (WEBKIT_DOM_ELEMENT (child), "-x-evo-caret-position")) {
			if (changing_mode && to_plain_text) {
				content = webkit_dom_html_element_get_outer_html (
					WEBKIT_DOM_HTML_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
			}
			if (to_html)
				remove_node (child);

			skip_node = TRUE;
			goto next;
		}

		if (element_has_class (WEBKIT_DOM_ELEMENT (child), "Apple-tab-span")) {
			if (!changing_mode && to_plain_text) {
				gchar *content, *tmp;
				GRegex *regex;
				gint char_count = 0;

				content = webkit_dom_node_get_text_content (child);
				/* Replace tabs with 8 whitespaces, otherwise they got
				 * replaced by single whitespace */
				if (strstr (content, "\x9")) {
					if (buffer->str && *buffer->str) {
						gchar *start_of_line = g_strrstr_len (
							buffer->str, -1, "\n") + 1;

						if (start_of_line && *start_of_line)
							char_count = strlen (start_of_line);
					} else
						char_count = 0;

					regex = g_regex_new ("\x9", 0, 0, NULL);
					tmp = g_regex_replace_eval (
						regex,
						content,
						-1,
						0,
						0,
						(GRegexEvalCallback) replace_to_whitespaces,
						GINT_TO_POINTER (char_count),
						NULL);

					g_string_append (buffer, tmp);
					g_free (tmp);
					g_regex_unref (regex);
				} else if (content && *content) {
					/* Some it happens that some text is written inside
					 * the tab span element, so save it. */
					g_string_append (buffer, content);
				}
				g_free (content);
			}
			if (to_html) {
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
				content = webkit_dom_html_element_get_outer_html (
					WEBKIT_DOM_HTML_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
				goto next;
			} else {
				if (!changing_mode && to_plain_text) {
					if (get_citation_level (child, FALSE) == 0) {
						gchar *value = webkit_dom_element_get_attribute (
							WEBKIT_DOM_ELEMENT (child), "type");

						if (value && g_strcmp0 (value, "cite") == 0)
							g_string_append (buffer, "\n");
						g_free (value);
					}
				}
				process_blockquote (WEBKIT_DOM_ELEMENT (child));
				if (to_html)
					remove_base_attributes (WEBKIT_DOM_ELEMENT (child));
			}
		}

		if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (child) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-indented"))
			process_blockquote (WEBKIT_DOM_ELEMENT (child));

		if (WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (child) ||
		    WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (child)) {
			if (to_plain_text) {
				if (changing_mode) {
					content = webkit_dom_html_element_get_outer_html (
						WEBKIT_DOM_HTML_ELEMENT (child));
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

			if (to_html && WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (image)) {
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
				content = webkit_dom_html_element_get_outer_html (
					WEBKIT_DOM_HTML_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
				goto next;
			}
			if (to_html) {
				remove_base_attributes (WEBKIT_DOM_ELEMENT (child));
				remove_evolution_attributes (WEBKIT_DOM_ELEMENT (child));
			}
			if (!changing_mode && to_plain_text)
				if (!webkit_dom_node_has_child_nodes (child))
					g_string_append (buffer, "\n");
		}

		/* Signature */
		if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (child) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-signature-wrapper")) {
			WebKitDOMNode *first_child;

			first_child = webkit_dom_node_get_first_child (child);

			if (to_html) {
				remove_base_attributes (
					WEBKIT_DOM_ELEMENT (first_child));
				remove_evolution_attributes (
					WEBKIT_DOM_ELEMENT (first_child));
			}
			if (to_plain_text && !changing_mode) {
				g_string_append (buffer, "\n");
				content = webkit_dom_html_element_get_inner_text (
					WEBKIT_DOM_HTML_ELEMENT (first_child));
				g_string_append (buffer, content);
				g_free (content);
				skip_nl = TRUE;
			}
			if (to_plain_text && changing_mode) {
				content = webkit_dom_html_element_get_outer_html (
					WEBKIT_DOM_HTML_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
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
			if (to_html) {
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
				content = webkit_dom_html_element_get_outer_html (
					WEBKIT_DOM_HTML_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
			}
			if (to_html)
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

						if (is_citation_node (parent))
							g_string_append (buffer, changing_mode ? "<br>" : "\n");
					}
				}
			}
		}

		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (child)) {
			if (changing_mode && to_plain_text) {
				content = webkit_dom_html_element_get_outer_html (
					WEBKIT_DOM_HTML_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
			}
			if (!changing_mode && to_plain_text) {
				content = webkit_dom_html_element_get_inner_text (
					WEBKIT_DOM_HTML_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
			}
		}
 next:
		if (webkit_dom_node_has_child_nodes (child) && !skip_node)
			process_elements (
				extension, child, to_html, changing_mode, to_plain_text, buffer);
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
remove_wrapping_from_document (WebKitDOMDocument *document)
{
	gint length;
	gint ii;
	WebKitDOMNodeList *list;

	list = webkit_dom_document_query_selector_all (document, "br.-x-evo-wrap-br", NULL);

	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++)
		remove_node (webkit_dom_node_list_item (list, ii));

	g_object_unref (list);
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
	for (ii = 0; ii < length; ii++)
		remove_node (webkit_dom_node_list_item (images, ii));

	g_object_unref (images);
}

static void
remove_images (WebKitDOMDocument *document)
{
	remove_images_in_element (
		WEBKIT_DOM_ELEMENT (webkit_dom_document_get_body (document)));
}

static void
toggle_smileys (EHTMLEditorWebExtension *extension,
                WebKitDOMDocument *document)
{
	gboolean html_mode;
	gint length;
	gint ii;
	WebKitDOMNodeList *smileys;

	html_mode = is_in_html_mode (extension);

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
	}

	g_object_unref (smileys);
}

static void
toggle_paragraphs_style_in_element (WebKitDOMDocument *document,
                                    WebKitDOMElement *element,
				    gboolean html_mode)
{
	gint ii, length;
	WebKitDOMNodeList *paragraphs;

	paragraphs = webkit_dom_element_query_selector_all (
		element, ".-x-evo-paragraph", NULL);

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
			if (!element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-indented")) {
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
					document, WEBKIT_DOM_ELEMENT (node), -1, 0, style_to_add);

				g_free (style);
			}
		}
	}
	g_object_unref (paragraphs);
}

static void
toggle_paragraphs_style (EHTMLEditorWebExtension *extension,
                         WebKitDOMDocument *document)
{
	toggle_paragraphs_style_in_element (
		document,
		WEBKIT_DOM_ELEMENT (webkit_dom_document_get_body (document)),
		is_in_html_mode (extension));
}

static gchar *
process_content_for_saving_as_draft (WebKitDOMDocument *document)
{
	WebKitDOMHTMLElement *body;
	WebKitDOMElement *document_element;
	gchar *content;

	body = webkit_dom_document_get_body (document);

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-evo-draft", "", NULL);

	document_element = webkit_dom_document_get_document_element (document);
	content = webkit_dom_html_element_get_outer_html (
		WEBKIT_DOM_HTML_ELEMENT (document_element));

	webkit_dom_element_remove_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-evo-draft");

	return content;
}

static gchar *
process_content_for_mode_change (WebKitDOMDocument *document)
{
	WebKitDOMNode *body;
	GString *plain_text;

	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));

	plain_text = g_string_sized_new (1024);

	process_elements (extension, body, FALSE, TRUE, TRUE, plain_text);

	g_string_append (plain_text, "</body></html>");

	return g_string_free (plain_text, FALSE);
}

static void
convert_element_from_html_to_plain_text (WebKitDOMDocument *document,
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
			add_selection_markers_into_element_start (
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

	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (blockquote));

	parse_html_into_paragraphs (
		document,
		main_blockquote ? blockquote : WEBKIT_DOM_ELEMENT (element),
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
				webkit_dom_html_element_set_inner_html (
					WEBKIT_DOM_HTML_ELEMENT (first_child),
					"<br>",
					NULL);
			}
			add_selection_markers_into_element_start (
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

static gchar *
process_content_for_plain_text (EHTMLEditorWebExtension *extension,
                                WebKitDOMDocument *document)
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
	if (is_in_html_mode (extension)) {
		if (converted || is_from_new_message) {
			toggle_paragraphs_style_in_element (
				document, WEBKIT_DOM_ELEMENT (source), FALSE);
			remove_images_in_element (
				document, WEBKIT_DOM_ELEMENT (source));
			remove_background_images_in_document (document);
		} else {
			gchar *inner_html;
			WebKitDOMElement *div;

			inner_html = webkit_dom_html_element_get_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (body));

			div = webkit_dom_document_create_element (
				document, "div", NULL);

			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (div), inner_html, NULL);

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
				document, div, &wrap, &quote);

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

		if (WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (paragraph) ||
		    WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (paragraph)) {
			WebKitDOMNode *item = webkit_dom_node_get_first_child (paragraph);

			while (item) {
				WebKitDOMNode *next_item =
					webkit_dom_node_get_next_sibling (item);

				if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
					dom_wrap_paragraph (
						document, WEBKIT_DOM_ELEMENT (item));
				}
				item = next_item;
			}
		} else {
			dom_wrap_paragraph (
				document, WEBKIT_DOM_ELEMENT (paragraph));
		}
	}
	g_object_unref (paragraphs);

	paragraphs = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (source),
		"span[id^=\"-x-evo-selection-\"], span#-x-evo-caret-position",
		NULL);

	length = webkit_dom_node_list_get_length (paragraphs);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (paragraphs, ii);
		WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);

		remove_node (node);
		webkit_dom_node_normalize (parent);
	}
	g_object_unref (paragraphs);

	if (is_in_html_mode (extension) || quote)
		quote_plain_text_recursive (document, source, source, 0);

	process_elements (extension, source, FALSE, FALSE, TRUE, plain_text);

	if (clean)
		remove_node (source);
	else
		g_object_unref (source);

	/* Return text content between <body> and </body> */
	return g_string_free (plain_text, FALSE);
}

static gchar *
process_content_for_html (WebKitDOMDocument *document)
{
	WebKitDOMNode *body, *document_clone;
	gchar *html_content;

	document_clone = webkit_dom_node_clone_node (
		WEBKIT_DOM_NODE (webkit_dom_document_get_document_element (document)), TRUE);
	body = WEBKIT_DOM_NODE (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "body", NULL));
	process_elements (extension, body, TRUE, FALSE, FALSE, NULL);

	html_content = webkit_dom_html_element_get_outer_html (
		WEBKIT_DOM_HTML_ELEMENT (document_clone));

	g_object_unref (document_clone);

	return html_content;
}

void
convert_when_changing_composer_mode (WebKitDOMDocument *document)
{
	gboolean quote = FALSE, wrap = FALSE;
	WebKitDOMHTMLElement *body;

	body = webkit_dom_document_get_body (document);

	convert_element_from_html_to_plain_text (
		document, WEBKIT_DOM_ELEMENT (body), &wrap, &quote);

	if (wrap)
		dom_wrap_paragraphs_in_document (document);

	if (quote) {
		dom_selection_save (document);
		if (wrap)
			quote_plain_text_elements_after_wrapping_in_document (
				document);
		else
			body = WEBKIT_DOM_HTML_ELEMENT (dom_quote_plain_text (document));
		dom_selection_restore (document);
	}

	toggle_paragraphs_style (document);
	toggle_smileys (document);
	remove_images (document);
	remove_background_images_in_document (document);

	clear_attributes (document);

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-converted", "", NULL);
}

static void
wrap_paragraphs_in_quoted_content (WebKitDOMDocument *document)
{
	gint ii, length;
	WebKitDOMNodeList *paragraphs;

	paragraphs = webkit_dom_document_query_selector_all (
		document, "blockquote[type=cite] > .-x-evo-paragraph", NULL);

	length = webkit_dom_node_list_get_length (paragraphs);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *paragraph;

		paragraph = webkit_dom_node_list_item (paragraphs, ii);

		dom_wrap_paragraph (document, WEBKIT_DOM_ELEMENT (paragraph));
	}
	g_object_unref (paragraphs);
}

