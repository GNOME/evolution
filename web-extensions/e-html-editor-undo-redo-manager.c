/*
 * e-html-editor-undo-redo-manager.c
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

#include "config.h"

#include "e-html-editor-undo-redo-manager.h"
#include "e-html-editor-web-extension.h"
#include "e-html-editor-selection-dom-functions.h"
#include "e-html-editor-view-dom-functions.h"
#include "e-dom-utils.h"

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDocumentFragmentUnstable.h>
#include <webkitdom/WebKitDOMRangeUnstable.h>
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>
#include <webkitdom/WebKitDOMHTMLElementUnstable.h>
#include <webkitdom/WebKitDOMDocumentUnstable.h>

#define E_HTML_EDITOR_UNDO_REDO_MANAGER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_UNDO_REDO_MANAGER, EHTMLEditorUndoRedoManagerPrivate))

struct _EHTMLEditorUndoRedoManagerPrivate {
	WebKitDOMDocument *document;
	GWeakRef extension;

	gboolean can_undo;
	gboolean can_redo;
	gboolean operation_in_progress;

	GList *history;
	guint history_size;
};

enum {
	PROP_0,
	PROP_CAN_REDO,
	PROP_CAN_UNDO,
	PROP_HTML_EDITOR_WEB_EXTENSION
};

#define HISTORY_SIZE_LIMIT 30

#define d(x)

G_DEFINE_TYPE (
	EHTMLEditorUndoRedoManager,
	e_html_editor_undo_redo_manager,
	G_TYPE_OBJECT
);

void
e_html_editor_undo_redo_manager_set_document (EHTMLEditorUndoRedoManager *manager,
                                              WebKitDOMDocument *document)
{
	g_return_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager));

	manager->priv->document = document;

	/* The history is not valid if document is changed. */
	e_html_editor_undo_redo_manager_clean_history (manager);
}

static EHTMLEditorWebExtension *
html_editor_undo_redo_manager_ref_extension (EHTMLEditorUndoRedoManager *manager)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager), NULL);

	return g_weak_ref_get (&manager->priv->extension);
}

static WebKitDOMRange *
get_range_for_point (WebKitDOMDocument *document,
                     EHTMLEditorSelectionPoint point)
{
	glong scroll_left, scroll_top;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *range;

	body = webkit_dom_document_get_body (document);
	scroll_left = webkit_dom_element_get_scroll_left (WEBKIT_DOM_ELEMENT (body));
	scroll_top = webkit_dom_element_get_scroll_top (WEBKIT_DOM_ELEMENT (body));

	range = webkit_dom_document_caret_range_from_point (
		document, point.x - scroll_left, point.y - scroll_top);

	/* The point is outside the viewport, scroll to it. */
	if (!range) {
		WebKitDOMDOMWindow *window;

		window = webkit_dom_document_get_default_view (document);
		webkit_dom_dom_window_scroll_to (window, point.x, point.y);

		scroll_left = webkit_dom_element_get_scroll_left (WEBKIT_DOM_ELEMENT (body));
		scroll_top = webkit_dom_element_get_scroll_top (WEBKIT_DOM_ELEMENT (body));
		range = webkit_dom_document_caret_range_from_point (
			document, point.x - scroll_left, point.y - scroll_top);
	}

	return range;
}

static void
restore_selection_to_history_event_state (WebKitDOMDocument *document,
                                          EHTMLEditorSelection selection_state)
{
	gboolean was_collapsed = FALSE;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMElement *element, *tmp;
	WebKitDOMRange *range;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	/* Restore the selection how it was before the event occured. */
	range = get_range_for_point (document, selection_state.start);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	was_collapsed = selection_state.start.x == selection_state.end.x;
	was_collapsed = was_collapsed && selection_state.start.y == selection_state.end.y;
	if (was_collapsed)
		return;

	dom_selection_save (document);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	remove_node (WEBKIT_DOM_NODE (element));

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	webkit_dom_element_remove_attribute (element, "id");

	range = get_range_for_point (document, selection_state.end);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	dom_selection_save (document);

	tmp = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	remove_node (WEBKIT_DOM_NODE (tmp));

	webkit_dom_element_set_id (
		element, "-x-evo-selection-start-marker");

	dom_selection_restore (document);
}

#if d(1)+0
static void
print_fragment_inner_html (WebKitDOMDocumentFragment *fragment)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *div;
	gchar *inner_html;

	if (!fragment) {
		printf ("\tNone'\n");
		return;
	}
	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (fragment));
	div = webkit_dom_document_create_element (document, "div", NULL);
	webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (div),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (fragment), TRUE),
			NULL);

	inner_html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (div));
	printf ("\t'%s'\n", inner_html);
	remove_node (WEBKIT_DOM_NODE (div));
	g_free (inner_html);
}

static void
print_node_inner_html (WebKitDOMNode *fragment)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *div;
	gchar *inner_html;

	if (!fragment) {
		printf ("\tnone\n");
		return;
	}
	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (fragment));
	div = webkit_dom_document_create_element (document, "div", NULL);
	webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (div),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (fragment), TRUE),
			NULL);

	inner_html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (div));
	remove_node (WEBKIT_DOM_NODE (div));

	printf ("\t'%s'\n", inner_html);

	g_free (inner_html);
}

static void
print_history_event (EHTMLEditorHistoryEvent *event)
{
	printf ("HISTORY EVENT: %d ; \n", event->type);
	printf ("\t before: start_x: %u ; start_y: %u ; end_x: %u ; end_y: %u ;\n", event->before.start.x, event->before.start.y, event->before.end.x, event->before.end.y);
	printf ("\t after: start_x: %u ; start_y: %u ; end_x: %u ; end_y: %u ;\n", event->after.start.x, event->after.start.y, event->after.end.x, event->after.end.y);
	switch (event->type) {
		case HISTORY_DELETE:
		case HISTORY_INPUT:
		case HISTORY_REMOVE_LINK:
		case HISTORY_SMILEY:
		case HISTORY_IMAGE:
		case HISTORY_CITATION_SPLIT:
		case HISTORY_HISTORY_BLOCKQUOTE:
			print_node_inner_html (WEBKIT_DOM_NODE (event->data.fragment));
			break;
		case HISTORY_ALIGNMENT:
		case HISTORY_BLOCK_FORMAT:
		case HISTORY_BOLD:
		case HISTORY_FONT_SIZE:
		case HISTORY_INDENT:
		case HISTORY_ITALIC:
		case HISTORY_MONOSPACE:
		case HISTORY_UNDERLINE:
		case HISTORY_STRIKETHROUGH:
		case HISTORY_WRAP:
			printf (" from %d to %d ;\n", event->data.style.from, event->data.style.to);
			break;
		case HISTORY_PASTE:
		case HISTORY_PASTE_AS_TEXT:
		case HISTORY_PASTE_QUOTED:
		case HISTORY_INSERT_HTML:
			printf (" pasting: '%s' ; \n", event->data.string.to);
			break;
		case HISTORY_HRULE_DIALOG:
		case HISTORY_IMAGE_DIALOG:
		case HISTORY_CELL_DIALOG:
		case HISTORY_TABLE_DIALOG:
		case HISTORY_PAGE_DIALOG:
			print_node_inner_html (event->data.dom.from);
			print_node_inner_html (event->data.dom.to);
			break;
		case HISTORY_FONT_COLOR:
		case HISTORY_REPLACE:
		case HISTORY_REPLACE_ALL:
			printf (" from '%s' to '%s';\n", event->data.string.from, event->data.string.to);
			break;
		case HISTORY_START:
			printf ("HISTORY START\n");
			break;
		default:
			printf ("Unknown history type\n");
	}
}

static void
print_history (EHTMLEditorUndoRedoManager *manager)
{
	if (manager->priv->history) {
		printf ("\n");
		g_list_foreach (
			manager->priv->history, (GFunc) print_history_event, NULL);
		printf ("\n");
	} else {
		printf ("History empty!\n");
	}
}

static void
print_undo_events (EHTMLEditorUndoRedoManager *manager)
{
	GList *item = manager->priv->history;

	printf ("UNDO EVENTS:\n");
	if (!item || !item->next) {
		printf ("EMPTY\n");
		return;
	}

	print_history_event (item->data);
	item = item->next;
	while (item) {
		print_history_event (item->data);
		item = item->next;
	}

	printf ("\n");

}

static void
print_redo_events (EHTMLEditorUndoRedoManager *manager)
{
	GList *item = manager->priv->history;

	printf ("REDO EVENTS:\n");
	if (!item || !item->prev) {
		printf ("EMPTY\n");
		return;
	}

	item = item->prev;
	while (item) {
		print_history_event (item->data);
		item = item->prev;
	}

	printf ("\n");
}
#endif

static void
undo_delete (WebKitDOMDocument *document,
             EHTMLEditorWebExtension *extension,
             EHTMLEditorHistoryEvent *event)
{
	gboolean empty, single_block;
	gchar *content;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *element;
	WebKitDOMNode *first_child, *fragment;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	fragment = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (event->data.fragment),  TRUE);
	first_child = webkit_dom_node_get_first_child (fragment);

	content = webkit_dom_node_get_text_content (fragment);
	empty = content && !*content;
	g_free (content);

	/* Tabulator */
	single_block = event->type == HISTORY_INPUT;
	single_block = single_block && event->before.start.x != 0 && event->before.end.y != 0;

	if (!single_block) {
		/* One block delete */
		if ((single_block = WEBKIT_DOM_IS_ELEMENT (first_child)))
			single_block = element_has_id (WEBKIT_DOM_ELEMENT (first_child), "-x-evo-selection-start-marker");
		else
			single_block = WEBKIT_DOM_IS_TEXT (first_child);
	}

	/* Redoing Return key press */
	if (empty) {
		WebKitDOMNode *node;

		range = get_range_for_point (document, event->before.start);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);

		node = webkit_dom_range_get_start_container (range, NULL);
		if (!node)
			return;

		element = get_parent_block_element (node);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			fragment,
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element)),
			NULL);

		dom_selection_restore (document);
		dom_force_spell_check (document, extension);

		return;
	}

	/* Multi block delete */
	if (WEBKIT_DOM_IS_ELEMENT (first_child) && !single_block) {
		WebKitDOMNode *node, *parent, *last_child;
		WebKitDOMNode *parent_deleted_content;
		WebKitDOMNode *parent_current_block;
		WebKitDOMNode *insert_before;

		range = get_range_for_point (document, event->before.start);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
		dom_selection_save (document);

		element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		/* Get the last block in deleted content. */
		last_child = webkit_dom_node_get_last_child (fragment);
		while (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (last_child))
			last_child = webkit_dom_node_get_last_child (last_child);

		/* All the nodes that are in current block after the caret position
		 * belongs on the end of the deleted content. */
		node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));
		while (node) {
			WebKitDOMNode *next_sibling;

			next_sibling = webkit_dom_node_get_next_sibling (node);
			webkit_dom_node_append_child (last_child, node, NULL);
			node = next_sibling;
		}

		/* Get the first block in deleted content. */
		while (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (first_child))
			first_child = webkit_dom_node_get_first_child (first_child);

		/* All the nodes that are in the first block of the deleted content
		 * belongs to the current block right after the caret position. */
		parent = get_parent_block_node_from_child (WEBKIT_DOM_NODE (element));
		while ((node = webkit_dom_node_get_first_child (first_child)))
			webkit_dom_node_append_child (WEBKIT_DOM_NODE (parent), node, NULL);

		parent_deleted_content = webkit_dom_node_get_parent_node (first_child);
		parent_current_block = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (parent));
		insert_before = webkit_dom_node_get_next_sibling (parent);

		/* Remove the first block from deleted content as its content was already
		 * moved to the right place. */
		remove_node (first_child);

		/* Move the deleted content back to the body. Start from the next sibling
		 * of the first block (if presented) where the delete occured. */
		while (parent_deleted_content) {
			WebKitDOMNode *tmp, *sibling;

			/* Move all the siblings from current level back to the body. */
			sibling = webkit_dom_node_get_first_child (parent_deleted_content);
			while (sibling) {
				WebKitDOMNode *next_sibling;

				next_sibling = webkit_dom_node_get_next_sibling (sibling);
				webkit_dom_node_insert_before (
					parent_current_block, sibling, insert_before, NULL);
				sibling = next_sibling;
			}
			tmp = webkit_dom_node_get_parent_node (parent_deleted_content);
			remove_node (parent_deleted_content);
			parent_deleted_content = tmp;
			insert_before = webkit_dom_node_get_next_sibling (parent_current_block);
			parent_current_block = webkit_dom_node_get_parent_node (parent_current_block);
		}

		dom_selection_restore (document);
		dom_force_spell_check (document, extension);
	} else {
		WebKitDOMNode *nd;

		element = webkit_dom_document_create_element (document, "span", NULL);

		range = get_range_for_point (document, event->after.start);
		/* Create temporary node on the selection where the delete occured. */
		webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (element), NULL);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);

		nd = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
		if (nd && WEBKIT_DOM_IS_TEXT (nd)) {
			gchar *text = webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (nd));
			glong length = webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (nd));

			/* We have to preserve empty paragraphs with just UNICODE_ZERO_WIDTH_SPACE
			 * character as when we will remove it it will collapse */
			if (length > 1) {
				if (g_str_has_prefix (text, UNICODE_ZERO_WIDTH_SPACE))
					webkit_dom_character_data_replace_data (
						WEBKIT_DOM_CHARACTER_DATA (nd), 0, 1, "", NULL);
				else if (g_str_has_suffix (text, UNICODE_ZERO_WIDTH_SPACE))
					webkit_dom_character_data_replace_data (
						WEBKIT_DOM_CHARACTER_DATA (nd), length - 1, 1, "", NULL);
			}
			g_free (text);
		}

		/* Insert the deleted content back to the body. */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			fragment,
			WEBKIT_DOM_NODE (element),
			NULL);

		remove_node (WEBKIT_DOM_NODE (element));

		/* If the selection markers are presented restore the selection,
		 * otherwise the selection was not callapsed so select the deleted
		 * content as it was before the delete occured. */
		if (webkit_dom_document_fragment_query_selector (event->data.fragment, "span#-x-evo-selection-start-marker", NULL))
			dom_selection_restore (document);
		else
			restore_selection_to_history_event_state (document, event->before);

		if (e_html_editor_web_extension_get_magic_smileys_enabled (extension))
			dom_check_magic_smileys (document, extension);
		if (e_html_editor_web_extension_get_magic_links_enabled (extension))
			dom_check_magic_links (document, extension, FALSE);
		dom_force_spell_check_for_current_paragraph (document, extension);
	}
}

static void
redo_delete (WebKitDOMDocument *document,
             EHTMLEditorWebExtension *extension,
             EHTMLEditorHistoryEvent *event)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDocumentFragment *fragment = event->data.fragment;
	WebKitDOMNode *first_child;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	first_child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));

	restore_selection_to_history_event_state (document, event->before);

	if (webkit_dom_document_fragment_query_selector (fragment, "span#-x-evo-selection-start-marker", NULL)) {
		gboolean delete = FALSE;

		/* Check if the event was delete or backspace press. */
		delete = WEBKIT_DOM_IS_ELEMENT (first_child);
		delete = delete && element_has_id (WEBKIT_DOM_ELEMENT (first_child), "-x-evo-selection-start-marker");
		if (delete)
			webkit_dom_dom_selection_modify (dom_selection, "extend", "right", "character");
		else
			webkit_dom_dom_selection_modify (dom_selection, "extend", "left", "character");
	}

	dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);
	dom_force_spell_check_for_current_paragraph (document, extension);
}

typedef void (*SelectionStyleChangeFunc) (WebKitDOMDocument *document, EHTMLEditorWebExtension *extension, gint style);

static void
undo_redo_style_change (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension,
                        EHTMLEditorHistoryEvent *event,
                        gboolean undo)
{
	SelectionStyleChangeFunc func;

	switch (event->type) {
		case HISTORY_ALIGNMENT:
			func = (SelectionStyleChangeFunc) dom_selection_set_alignment;
			break;
		case HISTORY_BOLD:
			func = dom_selection_set_bold;
			break;
		case HISTORY_BLOCK_FORMAT:
			func = (SelectionStyleChangeFunc) dom_selection_set_block_format;
			break;
		case HISTORY_FONT_SIZE:
			func = (SelectionStyleChangeFunc) dom_selection_set_font_size;
			break;
		case HISTORY_ITALIC:
			func = dom_selection_set_italic;
			break;
		case HISTORY_MONOSPACE:
			func = dom_selection_set_monospaced;
			break;
		case HISTORY_STRIKETHROUGH:
			func = dom_selection_set_strikethrough;
			break;
		case HISTORY_UNDERLINE:
			func = dom_selection_set_underline;
			break;
		default:
			return;
	}

	restore_selection_to_history_event_state (document, undo ? event->after : event->before);

	func (document, extension, undo ? event->data.style.from : event->data.style.to);

	restore_selection_to_history_event_state (document, undo ? event->before : event->after);
}

static void
undo_redo_indent (WebKitDOMDocument *document,
                  EHTMLEditorWebExtension *extension,
                  EHTMLEditorHistoryEvent *event,
                  gboolean undo)
{
	gboolean was_indent = FALSE;

	if (undo)
		restore_selection_to_history_event_state (document, event->after);

	was_indent = event->data.style.from && event->data.style.to;

	if ((undo && was_indent) || (!undo && !was_indent))
		dom_selection_unindent (document, extension);
	else
		dom_selection_indent (document, extension);

	if (undo)
		restore_selection_to_history_event_state (document, event->before);
}

static void
undo_redo_font_color (WebKitDOMDocument *document,
                      EHTMLEditorWebExtension *extension,
                      EHTMLEditorHistoryEvent *event,
                      gboolean undo)
{
	if (undo)
		restore_selection_to_history_event_state (document, event->after);

	dom_exec_command (
		document,
		extension,
		E_HTML_EDITOR_VIEW_COMMAND_FORE_COLOR,
		undo ? event->data.string.from : event->data.string.to);

	if (undo)
		restore_selection_to_history_event_state (document, event->before);
}

static void
undo_redo_wrap (WebKitDOMDocument *document,
                EHTMLEditorWebExtension *extension,
                EHTMLEditorHistoryEvent *event,
                gboolean undo)
{
	if (undo)
		restore_selection_to_history_event_state (document, event->after);

	if (undo) {
		WebKitDOMNode *node;
		WebKitDOMElement *element;
		WebKitDOMRange *range;

		range = dom_get_current_range (document);
		node = webkit_dom_range_get_common_ancestor_container (range, NULL);
		element = get_parent_block_element (WEBKIT_DOM_NODE (node));
		dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (element));

		dom_force_spell_check_for_current_paragraph (document, extension);
	} else
		dom_selection_wrap (document, extension);

	if (undo)
		restore_selection_to_history_event_state (document, event->before);
}

static void
undo_redo_page_dialog (WebKitDOMDocument *document,
                       EHTMLEditorWebExtension *extension,
                       EHTMLEditorHistoryEvent *event,
                       gboolean undo)
{
	WebKitDOMHTMLElement *body;
	WebKitDOMNamedNodeMap *attributes, *attributes_history;
	gint length, length_history, ii, jj;

	body = webkit_dom_document_get_body (document);

	if (undo)
		restore_selection_to_history_event_state (document, event->after);

	if (undo) {
		attributes = webkit_dom_element_get_attributes (WEBKIT_DOM_ELEMENT (body));
		attributes_history = webkit_dom_element_get_attributes (
			WEBKIT_DOM_ELEMENT (event->data.dom.from));
	} else {
		attributes_history = webkit_dom_element_get_attributes (WEBKIT_DOM_ELEMENT (body));
		attributes = webkit_dom_element_get_attributes (
			WEBKIT_DOM_ELEMENT (event->data.dom.to));
	}

	length = webkit_dom_named_node_map_get_length (attributes);
	length_history = webkit_dom_named_node_map_get_length (attributes_history);
	for (ii = length - 1; ii >= 0; ii--) {
		gchar *name;
		WebKitDOMNode *attr;
		gboolean replaced = FALSE;

		attr = webkit_dom_named_node_map_item (attributes, ii);
		name = webkit_dom_node_get_local_name (attr);

		for (jj = length_history - 1; jj >= 0; jj--) {
			gchar *name_history;
			WebKitDOMNode *attr_history;

			attr_history = webkit_dom_named_node_map_item (attributes_history, jj);
			name_history = webkit_dom_node_get_local_name (attr_history);
			if (g_strcmp0 (name, name_history) == 0) {
				WebKitDOMNode *attr_clone;

				attr_clone = webkit_dom_node_clone_node (
						undo ? attr_history : attr, TRUE);
				webkit_dom_element_set_attribute_node (
					WEBKIT_DOM_ELEMENT (body),
					WEBKIT_DOM_ATTR (attr_clone),
					NULL);

				/* Link color has to replaced in HEAD as well. */
				if (g_strcmp0 (name, "link") == 0) {
					gchar *value;

					value = webkit_dom_node_get_node_value (attr_clone);
					dom_set_link_color (document, value);
					g_free (value);
				}
				replaced = TRUE;
			}
			g_free (name_history);
			g_object_unref (attr_history);
			if (replaced)
				break;
		}

		if (!replaced) {
			if (undo) {
				webkit_dom_element_remove_attribute_node (
					WEBKIT_DOM_ELEMENT (body),
					WEBKIT_DOM_ATTR (attr),
					NULL);
			} else {
				webkit_dom_element_set_attribute_node (
					WEBKIT_DOM_ELEMENT (body),
					WEBKIT_DOM_ATTR (
						webkit_dom_node_clone_node (attr, TRUE)),
					NULL);
			}
		}
		g_free (name);
		g_object_unref (attr);
	}
	g_object_unref (attributes);
	g_object_unref (attributes_history);

	if (undo)
		restore_selection_to_history_event_state (document, event->before);
}

static void
undo_redo_hrule_dialog (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension,
                        EHTMLEditorHistoryEvent *event,
                        gboolean undo)
{
	WebKitDOMElement *element;

	if (undo)
		restore_selection_to_history_event_state (document, event->after);

	dom_selection_save (document);
	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	if (undo) {
		WebKitDOMNode *node;
		WebKitDOMElement *parent;

		parent = get_parent_block_element (WEBKIT_DOM_NODE (element));
		if (event->data.dom.from)
			node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (parent));
		else
			node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent));

		if (node && WEBKIT_DOM_IS_HTML_HR_ELEMENT (node)) {
			if (!event->data.dom.from)
				remove_node (node);
			else
				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (node),
					event->data.dom.from,
					node,
					NULL);
		}
	} else {
		WebKitDOMNode *node;
		WebKitDOMElement *parent;

		parent = get_parent_block_element (WEBKIT_DOM_NODE (element));

		if (event->data.dom.from) {
			node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent));

			if (node && WEBKIT_DOM_IS_HTML_HR_ELEMENT (node))
				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (node),
					event->data.dom.to,
					node,
					NULL);
		} else {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (parent)),
				event->data.dom.to,
				webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent)),
				NULL);
		}
	}

	if (undo)
		restore_selection_to_history_event_state (document, event->before);
	else
		dom_selection_restore (document);
}

static void
undo_redo_image_dialog (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension,
                        EHTMLEditorHistoryEvent *event,
                        gboolean undo)
{
	WebKitDOMElement *element;
	WebKitDOMNode *sibling, *image = NULL;

	if (undo)
		restore_selection_to_history_event_state (document, event->after);

	dom_selection_save (document);
	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
	if (sibling && WEBKIT_DOM_IS_ELEMENT (sibling)) {
		if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (sibling))
			image = sibling;
		else if (element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-resizable-wrapper"))
			image = webkit_dom_node_get_first_child (sibling);
	}

	if (!image) {
		element = WEBKIT_DOM_ELEMENT (webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element)));
		sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));
		if (sibling && WEBKIT_DOM_IS_ELEMENT (sibling)) {
			if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (sibling))
				image = sibling;
			else if (element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-resizable-wrapper"))
				image = webkit_dom_node_get_first_child (sibling);
		}
	}

	if (!image)
		return;

	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (image),
		undo ? event->data.dom.from : event->data.dom.to,
		image,
		NULL);

	if (undo)
		restore_selection_to_history_event_state (document, event->before);
	else
		dom_selection_restore (document);
}

static void
undo_redo_table_dialog (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension,
                        EHTMLEditorHistoryEvent *event,
                        gboolean undo)
{
	WebKitDOMElement *table, *element;

	if (undo)
		restore_selection_to_history_event_state (document, event->after);

	dom_selection_save (document);
	element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker");
	if (!element)
		return;

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "TABLE");

	if (!table) {
		if ((!event->data.dom.to && undo) || (!event->data.dom.from && !undo)) {
			WebKitDOMElement *parent;

			parent = get_parent_block_element (WEBKIT_DOM_NODE (element));
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (parent)),
				undo ? event->data.dom.from : event->data.dom.to,
				WEBKIT_DOM_NODE (parent),
				NULL);
			restore_selection_to_history_event_state (document, event->before);
			return;
		} else
			return;
	}

	if (undo) {
		if (!event->data.dom.from)
			remove_node (WEBKIT_DOM_NODE (table));
		else
			webkit_dom_node_replace_child (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (table)),
				event->data.dom.from,
				WEBKIT_DOM_NODE (table),
				NULL);
	} else {
		if (!event->data.dom.to)
			remove_node (WEBKIT_DOM_NODE (table));
		else
			webkit_dom_node_replace_child (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (table)),
				event->data.dom.to,
				WEBKIT_DOM_NODE (table),
				NULL);
	}

	if (undo)
		restore_selection_to_history_event_state (document, event->before);
	else
		dom_selection_restore (document);
}

static void
undo_redo_paste (WebKitDOMDocument *document,
                 EHTMLEditorWebExtension *extension,
                 EHTMLEditorHistoryEvent *event,
                 gboolean undo)
{
	if (undo) {
		if (event->type == HISTORY_PASTE_QUOTED) {
			WebKitDOMElement *tmp;
			WebKitDOMNode *parent;
			WebKitDOMNode *sibling;

			restore_selection_to_history_event_state (document, event->after);

			dom_selection_save (document);
			tmp = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			if (!tmp)
				return;

			parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (tmp));
			while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (parent)))
				parent = webkit_dom_node_get_parent_node (parent);

			sibling = webkit_dom_node_get_previous_sibling (parent);
			if (sibling) {
				dom_add_selection_markers_into_element_end (document, WEBKIT_DOM_ELEMENT (sibling), NULL, NULL);

				remove_node (parent);
			} else {
				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (parent),
					WEBKIT_DOM_NODE (dom_prepare_paragraph (document, extension, TRUE)),
					parent,
					NULL);
			}
			dom_selection_restore (document);
		} else {
			WebKitDOMDOMWindow *dom_window;
			WebKitDOMDOMSelection *dom_selection;
			WebKitDOMElement *element, *tmp;
			WebKitDOMRange *range;

			dom_window = webkit_dom_document_get_default_view (document);
			dom_selection = webkit_dom_dom_window_get_selection (dom_window);

			/* Restore the selection how it was before the event occured. */
			range = get_range_for_point (document, event->before.start);
			webkit_dom_dom_selection_remove_all_ranges (dom_selection);
			webkit_dom_dom_selection_add_range (dom_selection, range);

			dom_selection_save (document);

			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			remove_node (WEBKIT_DOM_NODE (element));

			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");

			webkit_dom_element_remove_attribute (element, "id");

			range = get_range_for_point (document, event->after.start);
			webkit_dom_dom_selection_remove_all_ranges (dom_selection);
			webkit_dom_dom_selection_add_range (dom_selection, range);

			dom_selection_save (document);

			tmp = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");

			remove_node (WEBKIT_DOM_NODE (tmp));

			webkit_dom_element_set_id (
				element, "-x-evo-selection-start-marker");

			dom_selection_restore (document);

			dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);

			dom_force_spell_check_for_current_paragraph (document, extension);
		}
	} else {
		restore_selection_to_history_event_state (document, event->before);

		if (event->type == HISTORY_PASTE)
			dom_convert_and_insert_html_into_selection (document, extension, event->data.string.to, FALSE);
		else if (event->type == HISTORY_PASTE_QUOTED)
			dom_quote_and_insert_text_into_selection (document, extension, event->data.string.to);
		else if (event->type == HISTORY_INSERT_HTML)
			dom_insert_html (document, extension, event->data.string.to);
		else
			dom_convert_and_insert_html_into_selection (document, extension, event->data.string.to, FALSE);
			//e_html_editor_selection_insert_as_text (selection, event->data.string.to);
	}
}

static void
undo_redo_image (WebKitDOMDocument *document,
                 EHTMLEditorWebExtension *extension,
                 EHTMLEditorHistoryEvent *event,
                 gboolean undo)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	if (undo) {
		WebKitDOMElement *element;
		WebKitDOMNode *node;

		range = get_range_for_point (document, event->before.start);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);

		dom_selection_save (document);
		element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		node = webkit_dom_node_get_next_sibling  (WEBKIT_DOM_NODE (element));

		if (WEBKIT_DOM_IS_ELEMENT (node))
			if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-resizable-wrapper") ||
			    element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-smiley-wrapper"))
				remove_node (node);
		dom_selection_restore (document);
	} else {
		WebKitDOMElement *element;

		range = get_range_for_point (document, event->before.start);
		/* Create temporary node on the selection where the delete occured. */
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);

		dom_selection_save (document);
		element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		/* Insert the deleted content back to the body. */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (event->data.fragment), TRUE),
			WEBKIT_DOM_NODE (element),
			NULL);

		dom_selection_restore (document);
		dom_force_spell_check_for_current_paragraph (document, extension);
	}
}

static void
undo_redo_replace (WebKitDOMDocument *document,
                   EHTMLEditorWebExtension *extension,
                   EHTMLEditorHistoryEvent *event,
                   gboolean undo)
{
	restore_selection_to_history_event_state (document, undo ? event->after : event->before);

	if (undo) {
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);

		webkit_dom_dom_selection_modify (dom_selection, "extend", "left", "word");
	}

	dom_exec_command (
		document,
		extension,
		E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT,
		undo ? event->data.string.from : event->data.string.to);

	dom_force_spell_check_for_current_paragraph (document, extension);

	restore_selection_to_history_event_state (document, undo ? event->before : event->after);
}

static void
undo_redo_replace_all (EHTMLEditorUndoRedoManager *manager,
                       WebKitDOMDocument *document,
                       EHTMLEditorWebExtension *extension,
                       EHTMLEditorHistoryEvent *event,
                       gboolean undo)
{
	if (undo) {
		if (event->type == HISTORY_REPLACE) {
			undo_redo_replace (document, extension, event, undo);
			return;
		} else {
			EHTMLEditorHistoryEvent *next_event;
			GList *next_item;
			WebKitDOMDOMWindow *dom_window;
			WebKitDOMDOMSelection *dom_selection;

			next_item = manager->priv->history->next;

			while (next_item) {
				next_event = next_item->data;

				if (next_event->type != HISTORY_REPLACE)
					break;

				if (g_strcmp0 (next_event->data.string.from, event->data.string.from) != 0)
					break;

				if (g_strcmp0 (next_event->data.string.to, event->data.string.to) != 0)
					break;

				undo_redo_replace (document, extension, next_event, undo);

				next_item = next_item->next;
			}

			manager->priv->history = next_item->prev;

			dom_window = webkit_dom_document_get_default_view (document);
			dom_selection = webkit_dom_dom_window_get_selection (dom_window);
			webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);
		}
	} else {
		/* Find if this history item is part of HISTORY_REPLACE_ALL. */
		EHTMLEditorHistoryEvent *prev_event;
		GList *prev_item;
		gboolean replace_all = FALSE;

		prev_item = manager->priv->history->prev;
		while (prev_item) {
			prev_event = prev_item->data;

			if (prev_event->type == HISTORY_REPLACE)
				prev_item = prev_item->prev;
			else if (prev_event->type == HISTORY_REPLACE_ALL) {
				replace_all = TRUE;
				break;
			} else
				break;
		}

		if (!replace_all) {
			undo_redo_replace (document, extension, event, undo);
			return;
		}

		prev_item = manager->priv->history->prev;
		while (prev_item) {
			prev_event = prev_item->data;

			if (prev_event->type == HISTORY_REPLACE) {
				undo_redo_replace (document, extension, prev_event, undo);
				prev_item = prev_item->prev;
			} else
				break;
		}

		manager->priv->history = prev_item->next;
	}
}

static void
undo_redo_remove_link (WebKitDOMDocument *document,
                       EHTMLEditorWebExtension *extension,
                       EHTMLEditorHistoryEvent *event,
                       gboolean undo)
{
	if (undo)
		restore_selection_to_history_event_state (document, event->after);

	if (undo) {
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;
		WebKitDOMElement *element;
		WebKitDOMRange *range;

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		/* Select the anchor. */
		webkit_dom_dom_selection_modify (dom_selection, "move", "left", "word");
		webkit_dom_dom_selection_modify (dom_selection, "extend", "right", "word");

		range = dom_get_current_range (document);
		element = webkit_dom_document_create_element (document, "SPAN", NULL);
		webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (element), NULL);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (event->data.fragment), TRUE),
			WEBKIT_DOM_NODE (element),
			NULL);
		remove_node (WEBKIT_DOM_NODE (element));
	} else
		dom_selection_unlink (document, extension);

	if (undo)
		restore_selection_to_history_event_state (document, event->before);
}

static void
undo_input (EHTMLEditorUndoRedoManager *manager,
            WebKitDOMDocument *document,
            EHTMLEditorWebExtension *extension,
            EHTMLEditorHistoryEvent *event)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	restore_selection_to_history_event_state (document, event->after);

	webkit_dom_dom_selection_modify (dom_selection, "extend", "left", "character");
	if (dom_selection_is_citation (document)) {
		/* Post processing of quoted text in body_input_event_cb needs to be called. */
		manager->priv->operation_in_progress = FALSE;
		e_html_editor_web_extension_set_dont_save_history_in_body_input (extension, TRUE);
	}
	dom_exec_command (document, extension, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);
}

static void
undo_redo_citation_split (WebKitDOMDocument *document,
                          EHTMLEditorWebExtension *extension,
                          EHTMLEditorHistoryEvent *event,
                          gboolean undo)
{
	if (undo) {
		gint citation_level = 1, length, word_wrap_length;
		WebKitDOMElement *selection_start, *parent;
		WebKitDOMNode *citation_before, *citation_after, *child, *last_child, *tmp;

		restore_selection_to_history_event_state (document, event->after);

		dom_selection_save (document);
		selection_start = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (!selection_start)
			return;

		parent = get_parent_block_element (WEBKIT_DOM_NODE (selection_start));

		citation_before = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (parent));
		if (!dom_node_is_citation_node (citation_before))
			return;

		citation_after = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent));
		if (!dom_node_is_citation_node (citation_after))
			return;

		/* Get first block in next citation. */
		child = webkit_dom_node_get_first_child (citation_after);
		while (child && dom_node_is_citation_node (child)) {
			citation_level++;
			child = webkit_dom_node_get_first_child (child);
		}

		dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (child));
		dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (child));

		/* Get last block in previous citation. */
		last_child = webkit_dom_node_get_last_child (citation_before);
		while (last_child && dom_node_is_citation_node (last_child))
			last_child = webkit_dom_node_get_last_child (last_child);

		dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (last_child));
		dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (last_child));

		/* Copy the content of the first block to the last block to get
		 * to the state how the block looked like before it was split. */
		while ((tmp = webkit_dom_node_get_first_child (child)))
			webkit_dom_node_append_child (last_child, tmp, NULL);

		word_wrap_length = e_html_editor_web_extension_get_word_wrap_length (extension);
		length = word_wrap_length - 2 * citation_level;

		/* We need to re-wrap and re-quote the block. */
		last_child = WEBKIT_DOM_NODE (dom_wrap_paragraph_length (
			document, extension, WEBKIT_DOM_ELEMENT (last_child), length));
		dom_quote_plain_text_element_after_wrapping (
			document, WEBKIT_DOM_ELEMENT (last_child), citation_level);

		remove_node (child);

		/* Move all the block from next citation to the previous one. */
		while ((child = webkit_dom_node_get_first_child (citation_after)))
			webkit_dom_node_append_child (citation_before, child, NULL);

		remove_node (WEBKIT_DOM_NODE (parent));
		remove_node (WEBKIT_DOM_NODE (citation_after));

		/* If enter was pressed when some text was selected, restore it. */
		if (event->data.fragment != NULL)
			undo_delete (document, extension, event);

		restore_selection_to_history_event_state (document, event->before);

		dom_force_spell_check (document, extension);
	} else {
		dom_insert_new_line_into_citation (document, extension, "");
	}
}

static void
undo_redo_blockquote (WebKitDOMDocument *document,
                      EHTMLEditorWebExtension *extension,
                      EHTMLEditorHistoryEvent *event,
                      gboolean undo)
{
	WebKitDOMElement *element;

	if (undo)
		restore_selection_to_history_event_state (document, event->after);

	dom_selection_save (document);
	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	if (undo) {
		WebKitDOMNode *node;
		WebKitDOMElement *parent;

		parent = get_parent_block_element (WEBKIT_DOM_NODE (element));
		node = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (parent));

		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (event->data.fragment),
			node,
			NULL);
	} else {
		dom_selection_set_block_format (
			document, extension, E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE);
	}

	if (undo)
		restore_selection_to_history_event_state (document, event->before);
	else
		dom_selection_restore (document);
}

gboolean
e_html_editor_undo_redo_manager_is_operation_in_progress (EHTMLEditorUndoRedoManager *manager)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager), FALSE);

	return manager->priv->operation_in_progress;
}

void
e_html_editor_undo_redo_manager_set_operation_in_progress (EHTMLEditorUndoRedoManager *manager,
                                                           gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager));

	manager->priv->operation_in_progress = value;
}

static void
free_history_event_content (EHTMLEditorHistoryEvent *event)
{
	switch (event->type) {
		case HISTORY_INPUT:
		case HISTORY_DELETE:
		case HISTORY_CITATION_SPLIT:
		case HISTORY_IMAGE:
		case HISTORY_SMILEY:
		case HISTORY_REMOVE_LINK:
		case HISTORY_BLOCKQUOTE:
			if (event->data.fragment != NULL)
				g_object_unref (event->data.fragment);
			break;
		case HISTORY_FONT_COLOR:
		case HISTORY_PASTE:
		case HISTORY_PASTE_AS_TEXT:
		case HISTORY_PASTE_QUOTED:
		case HISTORY_INSERT_HTML:
		case HISTORY_REPLACE:
		case HISTORY_REPLACE_ALL:
			if (event->data.string.from != NULL)
				g_free (event->data.string.from);
			if (event->data.string.to != NULL)
				g_free (event->data.string.to);
			break;
		case HISTORY_HRULE_DIALOG:
		case HISTORY_IMAGE_DIALOG:
		case HISTORY_CELL_DIALOG:
		case HISTORY_TABLE_DIALOG:
		case HISTORY_PAGE_DIALOG:
			if (event->data.dom.from != NULL)
				g_object_unref (event->data.dom.from);
			if (event->data.dom.to != NULL)
				g_object_unref (event->data.dom.to);
			break;
		default:
			break;
	}
}

static void
free_history_event (EHTMLEditorHistoryEvent *event)
{
	if (event == NULL)
		return;

	free_history_event_content (event);

	g_free (event);
}

static void
remove_history_event (EHTMLEditorUndoRedoManager *manager,
                      GList *item)
{
	free_history_event_content (item->data);

	manager->priv->history = g_list_delete_link (manager->priv->history, item);
	manager->priv->history_size--;
}

static void
remove_forward_redo_history_events_if_needed (EHTMLEditorUndoRedoManager *manager)
{
	GList *history = manager->priv->history;
	GList *item;

	if (!history || !history->prev)
		return;

	item = history->prev;
	while (item) {
		GList *prev_item = item->prev;

		remove_history_event (manager, item);
		item = prev_item;
	}
}

void
e_html_editor_undo_redo_manager_insert_history_event (EHTMLEditorUndoRedoManager *manager,
                                                      EHTMLEditorHistoryEvent *event)
{
	g_return_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager));

	if (manager->priv->operation_in_progress)
		return;

	d (print_history_event (event));

	remove_forward_redo_history_events_if_needed (manager);

	if (manager->priv->history_size >= HISTORY_SIZE_LIMIT)
		remove_history_event (manager, g_list_last (manager->priv->history)->prev);

	manager->priv->history = g_list_prepend (manager->priv->history, event);
	manager->priv->history_size++;
	manager->priv->can_undo = TRUE;

	d (print_undo_events (manager));
	d (print_redo_events (manager));

	g_object_notify (G_OBJECT (manager), "can-undo");
}

EHTMLEditorHistoryEvent *
e_html_editor_undo_redo_manager_get_current_history_event (EHTMLEditorUndoRedoManager *manager)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager), NULL);

	if (manager->priv->history)
		return manager->priv->history->data;

	return NULL;
}

void
e_html_editor_undo_redo_manager_insert_dash_history_event (EHTMLEditorUndoRedoManager *manager)
{
	EHTMLEditorHistoryEvent *event, *last;
	GList *history;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager));

	event = g_new0 (EHTMLEditorHistoryEvent, 1);
	event->type = HISTORY_INPUT;

	document = manager->priv->document;
	fragment = webkit_dom_document_create_document_fragment (document);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (fragment),
		WEBKIT_DOM_NODE (
			webkit_dom_document_create_text_node (document, "-")),
		NULL);
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
	event->data.fragment = fragment;

	last = e_html_editor_undo_redo_manager_get_current_history_event (manager);
	/* The dash event needs to have the same coordinates as the character
	 * that is right after it. */
	event->after.start.x = last->after.start.x;
	event->after.start.y = last->after.start.y;
	event->after.end.x = last->after.end.x;
	event->after.end.y = last->after.end.y;

	history = manager->priv->history;
	while (history) {
		EHTMLEditorHistoryEvent *item;
		WebKitDOMNode *first_child;

		item = history->data;

		if (item->type != HISTORY_INPUT)
			break;

		first_child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (item->data.fragment));
		if (WEBKIT_DOM_IS_TEXT (first_child)) {
			gchar *text;

			text = webkit_dom_node_get_text_content (first_child);
			if (text && *text == ':') {
				guint diff;

				diff = event->after.start.x - item->after.start.x;

				/* We need to move the coordinater of the last
				 * event by one character. */
				last->after.start.x += diff;
				last->after.end.x += diff;

				manager->priv->history = g_list_insert_before (
					manager->priv->history, history, event);
			}
			g_free (text);
			break;
		}
		history = history->next;
	}
}

gboolean
e_html_editor_undo_redo_manager_can_undo (EHTMLEditorUndoRedoManager *manager)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager), FALSE);

	if (manager->priv->history) {
		EHTMLEditorHistoryEvent *event;

		event = manager->priv->history->data;

		return (event->type != HISTORY_START);
	} else
		return FALSE;
}

static gboolean
event_selection_was_collapsed (EHTMLEditorHistoryEvent *ev)
{
	return (ev->before.start.x == ev->before.end.x) && (ev->before.start.y == ev->before.end.y);
}

void
e_html_editor_undo_redo_manager_undo (EHTMLEditorUndoRedoManager *manager)
{
	EHTMLEditorHistoryEvent *event;
	EHTMLEditorWebExtension *extension;
	GList *history;
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager));

	if (!e_html_editor_undo_redo_manager_can_undo (manager))
		return;

	history = manager->priv->history;
	event = history->data;

	d (print_history_event (event));

	manager->priv->operation_in_progress = TRUE;

	document = manager->priv->document;
	extension = html_editor_undo_redo_manager_ref_extension (manager);
	g_return_if_fail (extension != NULL);

	switch (event->type) {
		case HISTORY_BOLD:
		case HISTORY_ITALIC:
		case HISTORY_STRIKETHROUGH:
		case HISTORY_UNDERLINE:
		case HISTORY_FONT_SIZE:
			if (event_selection_was_collapsed (event)) {
				if (history->next) {
					manager->priv->history = history->next;
					e_html_editor_undo_redo_manager_undo (manager);
				}
				manager->priv->operation_in_progress = FALSE;
				g_object_unref (extension);
				return;
			}
		case HISTORY_ALIGNMENT:
		case HISTORY_BLOCK_FORMAT:
		case HISTORY_MONOSPACE:
			undo_redo_style_change (document, extension, event, TRUE);
			break;
		case HISTORY_DELETE:
			undo_delete (document, extension, event);
			break;
		case HISTORY_INDENT:
			undo_redo_indent (document, extension, event, TRUE);
			break;
		case HISTORY_INPUT:
			undo_input (manager, document, extension, event);
			break;
		case HISTORY_REMOVE_LINK:
			undo_redo_remove_link (document, extension, event, TRUE);
			break;
		case HISTORY_FONT_COLOR:
			undo_redo_font_color (document, extension, event, TRUE);
			break;
		case HISTORY_CITATION_SPLIT:
			undo_redo_citation_split (document, extension, event, TRUE);
			break;
		case HISTORY_PASTE:
		case HISTORY_PASTE_AS_TEXT:
		case HISTORY_PASTE_QUOTED:
		case HISTORY_INSERT_HTML:
			undo_redo_paste (document, extension, event, TRUE);
			break;
		case HISTORY_IMAGE:
		case HISTORY_SMILEY:
			undo_redo_image (document, extension, event, TRUE);
			break;
		case HISTORY_WRAP:
			undo_redo_wrap (document, extension, event, TRUE);
			break;
		case HISTORY_IMAGE_DIALOG:
			undo_redo_image_dialog (document, extension, event, TRUE);
			break;
		case HISTORY_TABLE_DIALOG:
			undo_redo_table_dialog (document, extension, event, TRUE);
			break;
		case HISTORY_PAGE_DIALOG:
			undo_redo_page_dialog (document, extension, event, TRUE);
			break;
		case HISTORY_HRULE_DIALOG:
			undo_redo_hrule_dialog (document, extension, event, TRUE);
			break;
		case HISTORY_REPLACE:
		case HISTORY_REPLACE_ALL:
			undo_redo_replace_all (manager, document, extension, event, TRUE);
			break;
		case HISTORY_BLOCKQUOTE:
			undo_redo_blockquote (document, extension, event, TRUE);
			break;
		default:
			g_object_unref (extension);
			return;
	}

	if (history->next)
		manager->priv->history = manager->priv->history->next;

	d (print_history (manager));
/* FIXME WK2
	html_editor_view_user_changed_contents_cb (view);*/

	manager->priv->operation_in_progress = FALSE;

	g_object_unref (extension);
}

gboolean
e_html_editor_undo_redo_manager_can_redo (EHTMLEditorUndoRedoManager *manager)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager), FALSE);

	if (manager->priv->history && manager->priv->history->prev)
		return TRUE;
	else
		return FALSE;
}

void
e_html_editor_undo_redo_manager_redo (EHTMLEditorUndoRedoManager *manager)
{
	EHTMLEditorWebExtension *extension;
	EHTMLEditorHistoryEvent *event;
	GList *history;
	WebKitDOMDocument *document;

	if (!e_html_editor_undo_redo_manager_can_redo (manager))
		return;

	history = manager->priv->history;
	event = history->prev->data;

	d (print_history_event (event));

	document = manager->priv->document;
	extension = html_editor_undo_redo_manager_ref_extension (manager);
	g_return_if_fail (extension != NULL);

	manager->priv->operation_in_progress = TRUE;

	switch (event->type) {
		case HISTORY_BOLD:
		case HISTORY_MONOSPACE:
		case HISTORY_STRIKETHROUGH:
		case HISTORY_UNDERLINE:
		case HISTORY_ALIGNMENT:
		case HISTORY_BLOCK_FORMAT:
		case HISTORY_FONT_SIZE:
		case HISTORY_ITALIC:
			undo_redo_style_change (document, extension, event, FALSE);
			break;
		case HISTORY_DELETE:
			redo_delete (document, extension, event);
			break;
		case HISTORY_INDENT:
			undo_redo_indent (document, extension, event, FALSE);
			break;
		case HISTORY_INPUT:
			undo_delete (document, extension, event);
			break;
		case HISTORY_REMOVE_LINK:
			undo_redo_remove_link (document, extension, event, FALSE);
			break;
		case HISTORY_FONT_COLOR:
			undo_redo_font_color (document, extension, event, FALSE);
			break;
		case HISTORY_CITATION_SPLIT:
			undo_redo_citation_split (document, extension, event, FALSE);
			break;
		case HISTORY_PASTE:
		case HISTORY_PASTE_AS_TEXT:
		case HISTORY_PASTE_QUOTED:
		case HISTORY_INSERT_HTML:
			undo_redo_paste (document, extension, event, FALSE);
			break;
		case HISTORY_IMAGE:
		case HISTORY_SMILEY:
			undo_redo_image (document, extension, event, FALSE);
			break;
		case HISTORY_WRAP:
			undo_redo_wrap (document, extension, event, FALSE);
			break;
		case HISTORY_IMAGE_DIALOG:
			undo_redo_image_dialog (document, extension, event, FALSE);
			break;
		case HISTORY_TABLE_DIALOG:
			undo_redo_table_dialog (document, extension, event, FALSE);
			break;
		case HISTORY_PAGE_DIALOG:
			undo_redo_page_dialog (document, extension, event, FALSE);
			break;
		case HISTORY_HRULE_DIALOG:
			undo_redo_hrule_dialog (document, extension, event, FALSE);
			break;
		case HISTORY_REPLACE:
		case HISTORY_REPLACE_ALL:
			undo_redo_replace_all (manager, document, extension, event, FALSE);
			break;
		case HISTORY_BLOCKQUOTE:
			undo_redo_blockquote (document, extension, event, FALSE);
			break;
		default:
			g_object_unref (extension);
			return;
	}

	manager->priv->history = manager->priv->history->prev;

	d (print_history (manager));
/* FIXME WK2
	html_editor_view_user_changed_contents_cb (view);*/

	manager->priv->operation_in_progress = FALSE;

	g_object_unref (extension);
}

void
e_html_editor_undo_redo_manager_clean_history (EHTMLEditorUndoRedoManager *manager)
{
	EHTMLEditorWebExtension *extension;
	EHTMLEditorHistoryEvent *ev;

	g_return_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager));

	if (manager->priv->history != NULL) {
		g_list_free_full (manager->priv->history, (GDestroyNotify) free_history_event);
		manager->priv->history = NULL;
	}

	manager->priv->history_size = 0;
	extension = html_editor_undo_redo_manager_ref_extension (manager);
	g_return_if_fail (extension != NULL);
	e_html_editor_web_extension_set_dont_save_history_in_body_input (extension, FALSE);
	g_object_unref (extension);
	manager->priv->operation_in_progress = FALSE;

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	ev->type = HISTORY_START;
	manager->priv->history = g_list_append (manager->priv->history, ev);

	manager->priv->can_undo = FALSE;
	g_object_notify (G_OBJECT (manager), "can-undo");
	manager->priv->can_redo = FALSE;
	g_object_notify (G_OBJECT (manager), "can-redo");
}

static void
html_editor_undo_redo_manager_set_extension (EHTMLEditorUndoRedoManager *manager,
                                             EHTMLEditorWebExtension *extension)
{
	g_return_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension));

	g_weak_ref_set (&manager->priv->extension, extension);
}

static void
html_editor_undo_redo_manager_dispose (GObject *object)
{
	EHTMLEditorUndoRedoManagerPrivate *priv;
	EHTMLEditorWebExtension *extension;

	priv = E_HTML_EDITOR_UNDO_REDO_MANAGER_GET_PRIVATE (object);

	if (priv->history != NULL) {
		g_list_free_full (priv->history, (GDestroyNotify) free_history_event);
		priv->history = NULL;
	}

	extension = g_weak_ref_get (&priv->extension);
	g_clear_object (&extension);
	g_weak_ref_set (&priv->extension, NULL);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_html_editor_undo_redo_manager_parent_class)->dispose (object);
}

static void
html_editor_undo_redo_manager_get_property (GObject *object,
                                            guint property_id,
                                            GValue *value,
                                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_REDO:
			g_value_set_boolean (
				value, e_html_editor_undo_redo_manager_can_redo (
				E_HTML_EDITOR_UNDO_REDO_MANAGER (object)));
			return;

		case PROP_CAN_UNDO:
			g_value_set_boolean (
				value, e_html_editor_undo_redo_manager_can_undo (
				E_HTML_EDITOR_UNDO_REDO_MANAGER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
html_editor_undo_redo_manager_set_property (GObject *object,
                                            guint property_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HTML_EDITOR_WEB_EXTENSION:
			html_editor_undo_redo_manager_set_extension (
				E_HTML_EDITOR_UNDO_REDO_MANAGER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_html_editor_undo_redo_manager_class_init (EHTMLEditorUndoRedoManagerClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorUndoRedoManagerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = html_editor_undo_redo_manager_dispose;
	object_class->get_property = html_editor_undo_redo_manager_get_property;
	object_class->set_property = html_editor_undo_redo_manager_set_property;

	/**
	 * EHTMLEditorUndoRedoManager:can-redo
	 *
	 * Determines whether it's possible to redo previous action. The action
	 * is usually disabled when there is no action to redo.
	 */
	g_object_class_install_property (
		object_class,
		PROP_CAN_REDO,
		g_param_spec_boolean (
			"can-redo",
			"Can Redo",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorUndoRedoManager:can-undo
	 *
	 * Determines whether it's possible to undo last action. The action
	 * is usually disabled when there is no previous action to undo.
	 */
	g_object_class_install_property (
		object_class,
		PROP_CAN_UNDO,
		g_param_spec_boolean (
			"can-undo",
			"Can Undo",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_HTML_EDITOR_WEB_EXTENSION,
		g_param_spec_object (
			"html-editor-web-extension",
			NULL,
			NULL,
			E_TYPE_HTML_EDITOR_WEB_EXTENSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_html_editor_undo_redo_manager_init (EHTMLEditorUndoRedoManager *manager)
{
	manager->priv = E_HTML_EDITOR_UNDO_REDO_MANAGER_GET_PRIVATE (manager);

	manager->priv->operation_in_progress = FALSE;
	manager->priv->history = NULL;
	e_html_editor_undo_redo_manager_clean_history (manager);
}
