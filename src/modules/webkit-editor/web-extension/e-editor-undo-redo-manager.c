/*
 * e-editor-undo-redo-manager.c
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

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDocumentFragmentUnstable.h>
#include <webkitdom/WebKitDOMRangeUnstable.h>
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>
#include <webkitdom/WebKitDOMHTMLElementUnstable.h>
#include <webkitdom/WebKitDOMDocumentUnstable.h>
#undef WEBKIT_DOM_USE_UNSTABLE_API

#include "web-extensions/e-dom-utils.h"

#include "e-editor-page.h"
#include "e-editor-dom-functions.h"
#include "e-editor-undo-redo-manager.h"

#define E_EDITOR_UNDO_REDO_MANAGER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_EDITOR_UNDO_REDO_MANAGER, EEditorUndoRedoManagerPrivate))

struct _EEditorUndoRedoManagerPrivate {
	GWeakRef editor_page;

	gboolean operation_in_progress;

	GList *history;
	guint history_size;
};

enum {
	PROP_0,
	PROP_CAN_REDO,
	PROP_CAN_UNDO,
	PROP_EDITOR_PAGE
};

const gchar* event_type_string[] = {
	"HISTORY_ALIGNMENT",
	"HISTORY_AND",
	"HISTORY_BLOCK_FORMAT",
	"HISTORY_BOLD",
	"HISTORY_CELL_DIALOG",
	"HISTORY_DELETE",
	"HISTORY_FONT_COLOR",
	"HISTORY_FONT_SIZE",
	"HISTORY_HRULE_DIALOG",
	"HISTORY_INDENT",
	"HISTORY_INPUT",
	"HISTORY_IMAGE",
	"HISTORY_IMAGE_DIALOG",
	"HISTORY_INSERT_HTML",
	"HISTORY_ITALIC",
	"HISTORY_LINK_DIALOG",
	"HISTORY_MONOSPACE",
	"HISTORY_PAGE_DIALOG",
	"HISTORY_PASTE",
	"HISTORY_PASTE_AS_TEXT",
	"HISTORY_PASTE_QUOTED",
	"HISTORY_REMOVE_LINK",
	"HISTORY_REPLACE",
	"HISTORY_REPLACE_ALL",
	"HISTORY_CITATION_SPLIT",
	"HISTORY_SMILEY",
	"HISTORY_START",
	"HISTORY_STRIKETHROUGH",
	"HISTORY_TABLE_DIALOG",
	"HISTORY_TABLE_INPUT",
	"HISTORY_UNDERLINE",
	"HISTORY_WRAP",
	"HISTORY_UNQUOTE"
};

#define HISTORY_SIZE_LIMIT 30

G_DEFINE_TYPE (EEditorUndoRedoManager, e_editor_undo_redo_manager, G_TYPE_OBJECT)

EEditorUndoRedoManager *
e_editor_undo_redo_manager_new (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	return g_object_new (E_TYPE_EDITOR_UNDO_REDO_MANAGER,
		"editor-page", editor_page,
		NULL);
}

static EEditorPage *
editor_undo_redo_manager_ref_editor_page (EEditorUndoRedoManager *manager)
{
	g_return_val_if_fail (E_IS_EDITOR_UNDO_REDO_MANAGER (manager), NULL);

	return g_weak_ref_get (&manager->priv->editor_page);
}

static WebKitDOMRange *
get_range_for_point (WebKitDOMDocument *document,
                     EEditorSelectionPoint point)
{
	glong scroll_left, scroll_top;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *range = NULL;

	body = webkit_dom_document_get_body (document);
	scroll_left = webkit_dom_element_get_scroll_left (WEBKIT_DOM_ELEMENT (body));
	scroll_top = webkit_dom_element_get_scroll_top (WEBKIT_DOM_ELEMENT (body));

	range = webkit_dom_document_caret_range_from_point (
		document, point.x - scroll_left, point.y - scroll_top);

	/* The point is outside the viewport, scroll to it. */
	if (!range) {
		WebKitDOMDOMWindow *dom_window = NULL;

		dom_window = webkit_dom_document_get_default_view (document);
		webkit_dom_dom_window_scroll_to (dom_window, point.x, point.y);

		scroll_left = webkit_dom_element_get_scroll_left (WEBKIT_DOM_ELEMENT (body));
		scroll_top = webkit_dom_element_get_scroll_top (WEBKIT_DOM_ELEMENT (body));
		range = webkit_dom_document_caret_range_from_point (
			document, point.x - scroll_left, point.y - scroll_top);
		g_clear_object (&dom_window);
	}

	return range;
}

static void
restore_selection_to_history_event_state (EEditorPage *editor_page,
                                          EEditorSelection selection_state)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMElement *element, *tmp;
	WebKitDOMRange *range = NULL;
	gboolean was_collapsed = FALSE;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	/* Restore the selection how it was before the event occured. */
	range = get_range_for_point (document, selection_state.start);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_clear_object (&range);

	was_collapsed = selection_state.start.x == selection_state.end.x;
	was_collapsed = was_collapsed && selection_state.start.y == selection_state.end.y;
	if (was_collapsed) {
		g_clear_object (&dom_selection);
		return;
	}

	e_editor_dom_selection_save (editor_page);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	remove_node (WEBKIT_DOM_NODE (element));

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	webkit_dom_element_remove_attribute (element, "id");

	range = get_range_for_point (document, selection_state.end);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_clear_object (&range);

	e_editor_dom_selection_save (editor_page);

	tmp = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	remove_node (WEBKIT_DOM_NODE (tmp));

	webkit_dom_element_set_id (
		element, "-x-evo-selection-start-marker");

	e_editor_dom_selection_restore (editor_page);

	g_clear_object (&dom_selection);
}

static void
print_node_inner_html (WebKitDOMNode *node)
{
	gchar *inner_html;

	if (!node) {
		printf ("    none\n");
		return;
	}

	inner_html = dom_get_node_inner_html (node);

	printf ("    '%s'\n", inner_html);

	g_free (inner_html);
}

static void
print_history_event (EEditorHistoryEvent *event)
{
	if (event->type != HISTORY_START && event->type != HISTORY_AND) {
		printf ("  %s\n", event_type_string[event->type]);
		printf ("    before: start_x: %u ; start_y: %u ; end_x: %u ; end_y: %u ;\n",
			event->before.start.x, event->before.start.y, event->before.end.x, event->before.end.y);
		printf ("    after:  start_x: %u ; start_y: %u ; end_x: %u ; end_y: %u ;\n",
			event->after.start.x, event->after.start.y, event->after.end.x, event->after.end.y);
	}
	switch (event->type) {
		case HISTORY_DELETE:
		case HISTORY_INPUT:
		case HISTORY_REMOVE_LINK:
		case HISTORY_SMILEY:
		case HISTORY_IMAGE:
		case HISTORY_CITATION_SPLIT:
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
			printf ("    from %d to %d ;\n", event->data.style.from, event->data.style.to);
			break;
		case HISTORY_PASTE:
		case HISTORY_PASTE_AS_TEXT:
		case HISTORY_PASTE_QUOTED:
		case HISTORY_INSERT_HTML:
			printf ("    pasting: '%s' ; \n", event->data.string.to);
			break;
		case HISTORY_HRULE_DIALOG:
		case HISTORY_IMAGE_DIALOG:
		case HISTORY_LINK_DIALOG:
		case HISTORY_CELL_DIALOG:
		case HISTORY_TABLE_DIALOG:
		case HISTORY_PAGE_DIALOG:
		case HISTORY_UNQUOTE:
			print_node_inner_html (event->data.dom.from);
			print_node_inner_html (event->data.dom.to);
			break;
		case HISTORY_FONT_COLOR:
		case HISTORY_REPLACE:
		case HISTORY_REPLACE_ALL:
			printf ("    from '%s' to '%s';\n", event->data.string.from, event->data.string.to);
			break;
		case HISTORY_START:
			printf ("  HISTORY START\n");
			break;
		case HISTORY_AND:
			printf ("  HISTORY AND\n");
			break;
		default:
			printf ("  Unknown history type\n");
	}
}

static void
print_history (EEditorUndoRedoManager *manager)
{
	printf ("-------------------\nWHOLE HISTORY STACK\n");
	if (manager->priv->history) {
		g_list_foreach (
			manager->priv->history, (GFunc) print_history_event, NULL);
	}
	printf ("-------------------\n");
}

static void
print_undo_events (EEditorUndoRedoManager *manager)
{
	GList *item = manager->priv->history;

	printf ("------------------\nUNDO HISTORY STACK\n");
	if (!item || !item->next) {
		printf ("------------------\n");
		return;
	}

	print_history_event (item->data);
	item = item->next;
	while (item) {
		print_history_event (item->data);
		item = item->next;
	}

	printf ("------------------\n");
}

static void
print_redo_events (EEditorUndoRedoManager *manager)
{
	GList *item = manager->priv->history;

	printf ("------------------\nREDO HISTORY STACK\n");
	if (!item || !item->prev) {
		printf ("------------------\n");
		return;
	}

	item = item->prev;
	while (item) {
		print_history_event (item->data);
		item = item->prev;
	}

	printf ("------------------\n");
}

static gboolean
event_selection_was_collapsed (EEditorHistoryEvent *ev)
{
	return (ev->before.start.x == ev->before.end.x) && (ev->before.start.y == ev->before.end.y);
}

static WebKitDOMNode *
split_node_into_two (WebKitDOMNode *item,
                     gint level)
{
	gint current_level = 1;
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMNode *parent, *prev_parent = NULL, *tmp = NULL;

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

	if (prev_parent) {
		tmp = webkit_dom_node_insert_before (
			parent,
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
			webkit_dom_node_get_next_sibling (prev_parent),
			NULL);
		remove_node_if_empty (prev_parent);
	}

	return tmp;
}

static void
undo_delete (EEditorPage *editor_page,
             EEditorHistoryEvent *event)
{
	gboolean empty, single_block, delete_key;
	gchar *content;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL;
	WebKitDOMElement *element;
	WebKitDOMNode *first_child, *fragment;

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	delete_key = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (event->data.fragment), "history-delete-key"));

	fragment = webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (event->data.fragment), TRUE, NULL);
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

	/* Delete or BackSpace pressed in the beginning of a block or on its end. */
	if (event->type == HISTORY_DELETE && !single_block &&
	     g_object_get_data (G_OBJECT (event->data.fragment), "history-concatenating-blocks")) {
		WebKitDOMNode *node, *block;

		range = get_range_for_point (document, event->after.start);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);

		node = webkit_dom_range_get_end_container (range, NULL);
		block = e_editor_dom_get_parent_block_node_from_child (node);

		if (webkit_dom_document_fragment_query_selector (event->data.fragment, ".-x-evo-quoted", NULL)) {
			while ((node = webkit_dom_node_get_first_child (fragment))) {
				if (WEBKIT_DOM_IS_ELEMENT (node) &&
				    webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (node), ".-x-evo-quoted", NULL))

					if (e_editor_dom_get_citation_level (block, FALSE) > 0) {
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (block),
							node,
							block,
							NULL);
					} else {
						WebKitDOMNode *next_block;

						next_block = webkit_dom_node_get_next_sibling (block);
						while (next_block && e_editor_dom_node_is_citation_node (next_block))
							next_block = webkit_dom_node_get_first_child (next_block);

						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (next_block),
							node,
							next_block,
							NULL);
					}
				else {
					if (e_editor_dom_get_citation_level (block, FALSE) > 0) {
						WebKitDOMNode *next_node;

						if ((next_node = split_node_into_two (block, -1)))
							webkit_dom_node_insert_before (
								webkit_dom_node_get_parent_node (next_node),
								node,
								next_node,
								NULL);
						else
							webkit_dom_node_insert_before (
								webkit_dom_node_get_parent_node (block),
								node,
								block,
								NULL);
					} else
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (block),
							node,
							block,
							NULL);
				}
			}
		} else {
			while ((node = webkit_dom_node_get_first_child (fragment))) {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (block),
					node,
					block,
					NULL);
			}
		}

		if (!delete_key && (node = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (event->data.fragment))) &&
		    node_is_list_or_item (node))
			remove_node (webkit_dom_node_get_next_sibling (block));

		remove_node (block);

		g_clear_object (&range);
		g_clear_object (&dom_selection);

		restore_selection_to_history_event_state (editor_page, event->before);

		e_editor_dom_force_spell_check_in_viewport (editor_page);

		return;
	}

	/* Redoing Return key press */
	if (event->type == HISTORY_INPUT && (empty ||
	    g_object_get_data (G_OBJECT (event->data.fragment), "history-return-key"))) {
		if (e_editor_dom_key_press_event_process_return_key (editor_page)) {
			e_editor_dom_body_key_up_event_process_return_key (editor_page);
		} else {
			WebKitDOMElement *element;
			WebKitDOMNode *next_sibling;

			range = get_range_for_point (document, event->before.start);
			webkit_dom_dom_selection_remove_all_ranges (dom_selection);
			webkit_dom_dom_selection_add_range (dom_selection, range);
			g_clear_object (&range);

			e_editor_dom_selection_save (editor_page);

			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));
			if (next_sibling && !(WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling))) {
				split_node_into_two (WEBKIT_DOM_NODE (element), 1);
			} else {
				WebKitDOMNode *block;

				block = e_editor_dom_get_parent_block_node_from_child (
					WEBKIT_DOM_NODE (element));
				dom_remove_selection_markers (document);
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (block),
					fragment,
					webkit_dom_node_get_next_sibling (block),
					NULL);
			}
			e_editor_dom_selection_restore (editor_page);
		}

		e_editor_page_set_return_key_pressed (editor_page, TRUE);
		e_editor_dom_check_magic_links (editor_page, FALSE);
		e_editor_page_set_return_key_pressed (editor_page, FALSE);
		e_editor_dom_force_spell_check_in_viewport (editor_page);

		g_clear_object (&dom_selection);

		return;
	}

	if (!single_block) {
		if (WEBKIT_DOM_IS_ELEMENT (first_child) &&
		    !(WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (first_child) ||
		      WEBKIT_DOM_IS_HTML_PRE_ELEMENT (first_child) ||
		      e_editor_dom_node_is_paragraph (first_child) ||
		      WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT (first_child)))
			single_block = TRUE;
	}

	/* Multi block delete */
	if (WEBKIT_DOM_IS_ELEMENT (first_child) && !single_block) {
		gboolean delete;
		WebKitDOMElement *signature;
		WebKitDOMNode *node, *current_block, *last_child;
		WebKitDOMNode *next_block, *insert_before;

		range = get_range_for_point (document, event->after.start);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
		g_clear_object (&range);
		e_editor_dom_selection_save (editor_page);

		if ((element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-end-marker"))) {
			WebKitDOMNode *next_sibling;

			if ((next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element))) &&
			     WEBKIT_DOM_IS_CHARACTER_DATA (next_sibling) &&
			     webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (next_sibling)) == 1) {
				gchar *data;

				data = webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (next_sibling));
				if (data && *data == ' ') {
					WebKitDOMElement *hidden_space;

					hidden_space = webkit_dom_document_create_element (document, "span", NULL);
					webkit_dom_element_set_attribute (
						hidden_space, "data-hidden-space", "", NULL);
					remove_node (next_sibling);
					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
						WEBKIT_DOM_NODE (hidden_space),
						webkit_dom_node_get_previous_sibling (
							WEBKIT_DOM_NODE (element)),
						NULL);
				}
				g_free (data);
			}
		}

		element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker");

		/* Get the last block in deleted content. */
		last_child = webkit_dom_node_get_last_child (fragment);
		while (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (last_child))
			last_child = webkit_dom_node_get_last_child (last_child);

		/* All the nodes that are in current block after the caret position
		 * belongs on the end of the deleted content. */
		node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));

		/* FIXME Ugly hack */
		/* If the selection ended in signature, the structure will be broken
		 * thus we saved the whole signature into deleted fragment and we will
		 * restore the whole signature, but we need to remove the rest of the
		 * signature that's left after delete to avoid duplications. */
		signature = webkit_dom_document_query_selector (document, ".-x-evo-signature-wrapper", NULL);
		delete = signature && webkit_dom_node_contains (WEBKIT_DOM_NODE (signature), WEBKIT_DOM_NODE (element));
		if (!delete) {
			WebKitDOMNode *tmp_node;

			tmp_node = webkit_dom_node_get_last_child (fragment);
			delete = tmp_node && WEBKIT_DOM_IS_ELEMENT (tmp_node) &&
				element_has_class (WEBKIT_DOM_ELEMENT (tmp_node), "-x-evo-signature-wrapper");
		}

		current_block = e_editor_dom_get_parent_block_node_from_child (WEBKIT_DOM_NODE (element));

		while (node) {
			WebKitDOMNode *next_sibling, *parent_node;

			next_sibling = webkit_dom_node_get_next_sibling (node);
			parent_node = webkit_dom_node_get_parent_node (node);
			/* Check if the whole element was deleted. If so replace it. */
			if (!next_sibling && WEBKIT_DOM_IS_HTML_BR_ELEMENT (node) &&
					!webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element))) {
				WebKitDOMNode *tmp_node;
				WebKitDOMElement *tmp_element;

				tmp_node = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (tmp_node),
					fragment,
					tmp_node,
					NULL);

				/* Remove empty blockquotes, if presented. */
				tmp_element = webkit_dom_document_query_selector (
					document, "blockquote[type=cite]:empty", NULL);
				if (tmp_element)
					remove_node (WEBKIT_DOM_NODE (tmp_element));

				e_editor_dom_merge_siblings_if_necessary (editor_page, event->data.fragment);

				tmp_node = webkit_dom_node_get_last_child (last_child);
				if (tmp_node && WEBKIT_DOM_IS_ELEMENT (tmp_node) &&
				    element_has_class (WEBKIT_DOM_ELEMENT (tmp_node), "-x-evo-quoted")) {
					webkit_dom_node_append_child (
						last_child,
						WEBKIT_DOM_NODE (
							webkit_dom_document_create_element (
								document, "br", NULL)),
						NULL);
				}

				dom_remove_selection_markers (document);

				restore_selection_to_history_event_state (editor_page, event->before);

				e_editor_dom_force_spell_check_in_viewport (editor_page);

				g_clear_object (&dom_selection);

				return;
			} else if (!next_sibling && !webkit_dom_node_is_same_node (parent_node, current_block))
				next_sibling = webkit_dom_node_get_next_sibling (parent_node);

			if (delete)
				remove_node (node);
			else
				webkit_dom_node_append_child (last_child, node, NULL);
			node = next_sibling;
		}

		/* Get the first block in deleted content. */
		while (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (first_child))
			first_child = webkit_dom_node_get_first_child (first_child);

		/* All the nodes that are in the first block of the deleted content
		 * belongs to the current block right after the caret position. */
		while ((node = webkit_dom_node_get_first_child (first_child)))
			webkit_dom_node_append_child (current_block, node, NULL);

		next_block = webkit_dom_node_get_next_sibling (current_block);
		insert_before = next_block;

		if (!insert_before) {
			WebKitDOMNode *parent = NULL;

			parent = current_block;
			while ((parent = webkit_dom_node_get_parent_node (parent)) &&
					!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
				insert_before = webkit_dom_node_get_next_sibling (parent);
				if (insert_before)
					break;
			}
		}

		/* Split a BLOCKQUOTE if the deleted content started with BLOCKQUOTE */
		if (insert_before &&
		    WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment))) &&
		    e_editor_dom_get_citation_level (insert_before, FALSE > 0))
			insert_before = split_node_into_two (insert_before, -1);

		/* Remove the first block from deleted content as its content was already
		 * moved to the right place. */
		remove_node (first_child);

		/* Insert the deleted content. */
		if (insert_before)
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (insert_before),
				WEBKIT_DOM_NODE (fragment),
				insert_before,
				NULL);
		else
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (
					webkit_dom_document_get_body (document)),
				WEBKIT_DOM_NODE (fragment),
				NULL);

		e_editor_dom_wrap_and_quote_element (editor_page, WEBKIT_DOM_ELEMENT (current_block));

		if (WEBKIT_DOM_IS_ELEMENT (last_child))
			e_editor_dom_wrap_and_quote_element (editor_page, WEBKIT_DOM_ELEMENT (last_child));

		e_editor_dom_merge_siblings_if_necessary (editor_page, event->data.fragment);

		/* If undoing drag and drop where the whole line was moved we need
		 * to correct the selection. */
		if (g_object_get_data (G_OBJECT (event->data.fragment), "history-drag-and-drop") &&
		    (element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-end-marker"))) {
			WebKitDOMNode *prev_block;

			prev_block = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
			if ((prev_block = webkit_dom_node_get_previous_sibling (prev_block)))
				webkit_dom_node_append_child (
					prev_block, WEBKIT_DOM_NODE (element), NULL);
		}

		e_editor_dom_selection_restore (editor_page);
		e_editor_dom_force_spell_check_in_viewport (editor_page);
	} else {
		gboolean empty_text = FALSE, was_link = FALSE;
		WebKitDOMNode *prev_sibling, *next_sibling, *nd;
		WebKitDOMNode *parent;

		element = webkit_dom_document_create_element (document, "span", NULL);

		/* Create temporary node on the selection where the delete occured. */
		if (webkit_dom_document_fragment_query_selector (event->data.fragment, ".Apple-tab-span", NULL))
			range = get_range_for_point (document, event->before.start);
		else
			range = get_range_for_point (document, event->after.start);

		/* If redoing an INPUT event that was done in the middle of the
		 * text we need to move one character backward as the range is
		 * pointing after the character and not before it - for INPUT
		 * events we don't save the before coordinates. */
		if (event->type == HISTORY_INPUT) {
			glong start_offset;
			WebKitDOMNode *start_container;

			start_offset = webkit_dom_range_get_start_offset (range, NULL);
			start_container = webkit_dom_range_get_start_container (range, NULL);

			if (WEBKIT_DOM_IS_CHARACTER_DATA (start_container) &&
			    start_offset != webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (start_container))) {
				webkit_dom_range_set_start (
					range,
					start_container,
					start_offset > 0 ? start_offset - 1 : 0,
					NULL);
				webkit_dom_range_collapse (range, TRUE, NULL);
			}
		}

		webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (element), NULL);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
		g_clear_object (&range);

		nd = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
		if (nd && WEBKIT_DOM_IS_TEXT (nd)) {
			gchar *text = webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (nd));
			glong length = webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (nd));

			/* We have to preserve empty paragraphs with just UNICODE_ZERO_WIDTH_SPACE
			 * character as when we will remove it paragraph will collapse. */
			if (length > 1) {
				if (g_str_has_prefix (text, UNICODE_ZERO_WIDTH_SPACE))
					webkit_dom_character_data_replace_data (
						WEBKIT_DOM_CHARACTER_DATA (nd), 0, 1, "", NULL);
				else if (g_str_has_suffix (text, UNICODE_ZERO_WIDTH_SPACE))
					webkit_dom_character_data_replace_data (
						WEBKIT_DOM_CHARACTER_DATA (nd), length - 1, 1, "", NULL);
			} else if (length == 0)
				empty_text = TRUE;

			g_free (text);
		}

		parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
		if (!nd || empty_text) {
			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent))
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (parent),
					WEBKIT_DOM_NODE (element),
					parent,
					NULL);
		}

		/* Insert the deleted content back to the body. */
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent)) {
			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (first_child)) {
				WebKitDOMNode *child;

				while ((child = webkit_dom_node_get_first_child (first_child)))
					webkit_dom_node_append_child (parent, child, NULL);

				remove_node (first_child);

				was_link = TRUE;
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (parent),
					fragment,
					webkit_dom_node_get_next_sibling (parent),
					NULL);
			} else {
				if (g_object_get_data (G_OBJECT (event->data.fragment), "history-removing-from-anchor") ||
				    !event_selection_was_collapsed (event)) {
					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
						fragment,
						WEBKIT_DOM_NODE (element),
						NULL);
				} else {
					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (parent),
						fragment,
						webkit_dom_node_get_next_sibling (parent),
						NULL);
				}
			}
		} else {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
				fragment,
				WEBKIT_DOM_NODE (element),
				NULL);
		}

		webkit_dom_node_normalize (parent);
		prev_sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
		next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));
		if (prev_sibling && next_sibling) {
			WebKitDOMNode *clone_prev, *clone_next;

			clone_prev = webkit_dom_node_clone_node_with_error (prev_sibling, FALSE, NULL);
			clone_next = webkit_dom_node_clone_node_with_error (next_sibling, FALSE, NULL);

			if (webkit_dom_node_is_equal_node (clone_prev, clone_next)) {
				WebKitDOMNode *child;

				while ((child = webkit_dom_node_get_first_child (next_sibling)))
					webkit_dom_node_append_child (prev_sibling, child, NULL);

				remove_node (next_sibling);
			}
		}

		if (event->type == HISTORY_INPUT) {
			WebKitDOMNode *sibling;

			sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));
			if (sibling && WEBKIT_DOM_IS_HTML_BR_ELEMENT (sibling) &&
			    !webkit_dom_node_get_next_sibling (sibling)) {
				remove_node (sibling);
			}
		}

		remove_node (WEBKIT_DOM_NODE (element));

		if (event->type == HISTORY_DELETE && !e_editor_page_get_html_mode (editor_page)) {
			WebKitDOMNode *current_block;

			current_block = e_editor_dom_get_parent_block_node_from_child (parent);
			if (e_editor_dom_get_citation_level (current_block, FALSE) > 0)
				e_editor_dom_wrap_and_quote_element (editor_page, WEBKIT_DOM_ELEMENT (current_block));
		}

		/* If the selection markers are presented restore the selection,
		 * otherwise the selection was not collapsed so select the deleted
		 * content as it was before the delete occurred. */
		if (webkit_dom_document_fragment_query_selector (event->data.fragment, "span#-x-evo-selection-start-marker", NULL))
			e_editor_dom_selection_restore (editor_page);
		else
			restore_selection_to_history_event_state (editor_page, event->before);

		if (event->type != HISTORY_INPUT) {
			if (e_editor_page_get_magic_smileys_enabled (editor_page))
				e_editor_dom_check_magic_smileys (editor_page);
			if (!was_link && e_editor_page_get_magic_links_enabled (editor_page))
				e_editor_dom_check_magic_links (editor_page, FALSE);
		}
		e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
	}

	g_clear_object (&dom_selection);
}

static void
redo_delete (EEditorPage *editor_page,
             EEditorHistoryEvent *event)
{
	EEditorUndoRedoManager *manager;
	WebKitDOMDocumentFragment *fragment = event->data.fragment;
	WebKitDOMNode *node;
	gboolean delete_key, control_key;
	glong length = 1;
	gint ii;

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	restore_selection_to_history_event_state (editor_page, event->before);

	delete_key = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (event->data.fragment), "history-delete-key"));
	control_key = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (event->data.fragment), "history-control-key"));

	if (!delete_key && e_editor_dom_key_press_event_process_backspace_key (editor_page))
		goto out;

	if (e_editor_dom_key_press_event_process_delete_or_backspace_key (editor_page, ~0, 0, delete_key))
		goto out;

	if (control_key) {
		gchar *text_content;

		text_content = webkit_dom_node_get_text_content (WEBKIT_DOM_NODE (fragment));
		length = g_utf8_strlen (text_content, -1);
		control_key = length > 1;

		g_free (text_content);
	}

	/* If concatenating two blocks with pressing Delete on the end
	 * of the previous one and the next node contain content that
	 * is wrapped on multiple lines, the last line will by separated
	 * by WebKit to the separate block. To avoid it let's remove
	 * all quoting and wrapping from the next paragraph. */
	if (delete_key &&
	    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (event->data.fragment), "history-concatenating-blocks"))) {
		WebKitDOMNode *current_block, *next_block, *node;
		WebKitDOMRange *range = NULL;

		range = e_editor_dom_get_current_range (editor_page);
		node = webkit_dom_range_get_end_container (range, NULL);
		g_clear_object (&range);
		current_block = e_editor_dom_get_parent_block_node_from_child (node);
		if (e_editor_dom_get_citation_level (current_block, FALSE) > 0 &&
		    (next_block = webkit_dom_node_get_next_sibling (current_block))) {
			e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (next_block));
			e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (next_block));
		}
	}

	for (ii = 0; ii < length; ii++) {
		e_editor_dom_exec_command (editor_page,
			delete_key ? E_CONTENT_EDITOR_COMMAND_FORWARD_DELETE :
				     E_CONTENT_EDITOR_COMMAND_DELETE,
			NULL);
	}

	/* Really don't know why, but when the selection marker nodes were in
	 * anchors then we need to do an extra delete command otherwise we will
	 * end with two blocks split in half. */
	node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
	while ((node = webkit_dom_node_get_first_child (node))) {
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
			e_editor_dom_exec_command (editor_page,
				E_CONTENT_EDITOR_COMMAND_FORWARD_DELETE,
				NULL);
			break;
		}
	}

	node = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (fragment));
	while ((node = webkit_dom_node_get_last_child (node))) {
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
			e_editor_dom_exec_command (editor_page,
				E_CONTENT_EDITOR_COMMAND_FORWARD_DELETE,
				NULL);
			break;
		}
	}

 out:
	e_editor_page_set_dont_save_history_in_body_input (editor_page, TRUE);
	e_editor_undo_redo_manager_set_operation_in_progress (manager, FALSE);
	e_editor_dom_body_input_event_process (editor_page, NULL);
	e_editor_page_set_dont_save_history_in_body_input (editor_page, FALSE);
	e_editor_undo_redo_manager_set_operation_in_progress (manager, TRUE);
	e_editor_page_set_renew_history_after_coordinates (editor_page, FALSE);
	e_editor_dom_body_key_up_event_process_backspace_or_delete (editor_page, delete_key);
	e_editor_page_set_renew_history_after_coordinates (editor_page, TRUE);

	restore_selection_to_history_event_state (editor_page, event->after);

	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
}

typedef void (*SelectionStyleChangeFunc) (EEditorPage *editor_page, gint style);

static void
undo_redo_style_change (EEditorPage *editor_page,
                        EEditorHistoryEvent *event,
                        gboolean undo)
{
	SelectionStyleChangeFunc func;

	switch (event->type) {
		case HISTORY_ALIGNMENT:
			func = (SelectionStyleChangeFunc) e_editor_dom_selection_set_alignment;
			break;
		case HISTORY_BOLD:
			func = e_editor_page_set_bold;
			break;
		case HISTORY_BLOCK_FORMAT:
			func = (SelectionStyleChangeFunc) e_editor_dom_selection_set_block_format;
			break;
		case HISTORY_FONT_SIZE:
			func = (SelectionStyleChangeFunc) e_editor_dom_selection_set_font_size;
			break;
		case HISTORY_ITALIC:
			func = e_editor_page_set_italic;
			break;
		case HISTORY_MONOSPACE:
			func = e_editor_page_set_monospace;
			break;
		case HISTORY_STRIKETHROUGH:
			func = e_editor_page_set_strikethrough;
			break;
		case HISTORY_UNDERLINE:
			func = e_editor_page_set_underline;
			break;
		default:
			return;
	}

	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	func (editor_page, undo ? event->data.style.from : event->data.style.to);

	restore_selection_to_history_event_state (editor_page, undo ? event->before : event->after);
}

static void
undo_redo_indent (EEditorPage *editor_page,
                  EEditorHistoryEvent *event,
                  gboolean undo)
{
	gboolean was_indent = FALSE;

	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	was_indent = event->data.style.from && event->data.style.to;

	if ((undo && was_indent) || (!undo && !was_indent))
		e_editor_dom_selection_unindent (editor_page);
	else
		e_editor_dom_selection_indent (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->before : event->after);
}

static void
undo_redo_font_color (EEditorPage *editor_page,
                      EEditorHistoryEvent *event,
                      gboolean undo)
{
	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	e_editor_dom_exec_command (editor_page,
		E_CONTENT_EDITOR_COMMAND_FORE_COLOR,
		undo ? event->data.string.from : event->data.string.to);

	restore_selection_to_history_event_state (editor_page, undo ? event->before : event->after);
}

static void
undo_redo_wrap (EEditorPage *editor_page,
                EEditorHistoryEvent *event,
                gboolean undo)
{
	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	if (undo) {
		WebKitDOMNode *node;
		WebKitDOMElement *element;
		WebKitDOMRange *range = NULL;

		range = e_editor_dom_get_current_range (editor_page);
		node = webkit_dom_range_get_common_ancestor_container (range, NULL);
		g_clear_object (&range);
		element = get_parent_block_element (WEBKIT_DOM_NODE (node));
		webkit_dom_element_remove_attribute (element, "data-user-wrapped");
		e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (element));

		e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
	} else
		e_editor_dom_selection_wrap (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->before : event->after);
}

static void
undo_redo_page_dialog (EEditorPage *editor_page,
                       EEditorHistoryEvent *event,
                       gboolean undo)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitDOMNamedNodeMap *attributes = NULL, *attributes_history = NULL;
	gint length, length_history, ii, jj;

	document = e_editor_page_get_document (editor_page);
	body = webkit_dom_document_get_body (document);

	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

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
		name = webkit_dom_attr_get_name (WEBKIT_DOM_ATTR (attr));

		for (jj = length_history - 1; jj >= 0; jj--) {
			gchar *name_history;
			WebKitDOMNode *attr_history;

			attr_history = webkit_dom_named_node_map_item (attributes_history, jj);
			name_history = webkit_dom_attr_get_name (WEBKIT_DOM_ATTR (attr_history));
			if (g_strcmp0 (name, name_history) == 0) {
				WebKitDOMNode *attr_clone;

				attr_clone = webkit_dom_node_clone_node_with_error (
						undo ? attr_history : attr, TRUE, NULL);
				webkit_dom_element_set_attribute_node (
					WEBKIT_DOM_ELEMENT (body),
					WEBKIT_DOM_ATTR (attr_clone),
					NULL);

				/* Link color has to replaced in HEAD as well. */
				if (g_strcmp0 (name, "link") == 0) {
					gchar *value;

					value = webkit_dom_node_get_node_value (attr_clone);
					e_editor_dom_set_link_color (editor_page, value);
					g_free (value);
				} else if (g_strcmp0 (name, "vlink") == 0) {
					gchar *value;

					value = webkit_dom_node_get_node_value (attr_clone);
					e_editor_dom_set_visited_link_color (editor_page, value);
					g_free (value);
				}
				replaced = TRUE;
			}
			g_free (name_history);
			g_clear_object (&attr_history);
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
						webkit_dom_node_clone_node_with_error (attr, TRUE, NULL)),
					NULL);
			}
		}
		g_free (name);
	}
	g_clear_object (&attributes);
	g_clear_object (&attributes_history);

	restore_selection_to_history_event_state (editor_page, undo ? event->before : event->after);
}

static void
undo_redo_hrule_dialog (EEditorPage *editor_page,
                        EEditorHistoryEvent *event,
                        gboolean undo)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	document = e_editor_page_get_document (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	e_editor_dom_selection_save (editor_page);
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
					webkit_dom_node_clone_node_with_error (event->data.dom.from, TRUE, NULL),
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
					webkit_dom_node_clone_node_with_error (event->data.dom.to, TRUE, NULL),
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

	if (undo) {
		dom_remove_selection_markers (document);
		restore_selection_to_history_event_state (editor_page, event->before);
	} else
		e_editor_dom_selection_restore (editor_page);
}

static void
undo_redo_image_dialog (EEditorPage *editor_page,
                        EEditorHistoryEvent *event,
                        gboolean undo)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMNode *sibling, *image = NULL;

	document = e_editor_page_get_document (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	e_editor_dom_selection_save (editor_page);
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
		webkit_dom_node_clone_node_with_error (undo ? event->data.dom.from : event->data.dom.to, TRUE, NULL),
		image,
		NULL);

	if (undo)
		restore_selection_to_history_event_state (editor_page, event->before);
	else
		e_editor_dom_selection_restore (editor_page);
}

static void
undo_redo_link_dialog (EEditorPage *editor_page,
                       EEditorHistoryEvent *event,
                       gboolean undo)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *anchor, *element;

	document = e_editor_page_get_document (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	e_editor_dom_selection_save (editor_page);

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker");
	if (!element)
		return;

	anchor = dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "A");
	if (undo) {
		if (anchor) {
			if (!event->data.dom.from)
				remove_node (WEBKIT_DOM_NODE (anchor));
			else
				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (anchor)),
					webkit_dom_node_clone_node_with_error (event->data.dom.from, TRUE, NULL),
					WEBKIT_DOM_NODE (anchor),
					NULL);
		}
	} else {
		if (!event->data.dom.to) {
			if (anchor)
				remove_node (WEBKIT_DOM_NODE (anchor));
		} else {
			if (WEBKIT_DOM_IS_ELEMENT (event->data.dom.from) && anchor) {
				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (anchor)),
					webkit_dom_node_clone_node_with_error (event->data.dom.to, TRUE, NULL),
					WEBKIT_DOM_NODE (anchor),
					NULL);
			} else {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
					webkit_dom_node_clone_node_with_error (event->data.dom.to, TRUE, NULL),
					WEBKIT_DOM_NODE (element),
					NULL);

				if (event->data.dom.from)
					e_editor_dom_exec_command (editor_page,
						E_CONTENT_EDITOR_COMMAND_DELETE, NULL);
			}
		}
	}

	if (undo)
		restore_selection_to_history_event_state (editor_page, event->before);
	else
		e_editor_dom_selection_restore (editor_page);
}

static void
undo_redo_table_dialog (EEditorPage *editor_page,
                        EEditorHistoryEvent *event,
                        gboolean undo)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *table, *element;

	document = e_editor_page_get_document (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	e_editor_dom_selection_save (editor_page);
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
				webkit_dom_node_clone_node_with_error (undo ? event->data.dom.from : event->data.dom.to, TRUE, NULL),
				WEBKIT_DOM_NODE (parent),
				NULL);
			restore_selection_to_history_event_state (editor_page, event->before);
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
				webkit_dom_node_clone_node_with_error (event->data.dom.from, TRUE, NULL),
				WEBKIT_DOM_NODE (table),
				NULL);
	} else {
		if (!event->data.dom.to)
			remove_node (WEBKIT_DOM_NODE (table));
		else
			webkit_dom_node_replace_child (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (table)),
				webkit_dom_node_clone_node_with_error (event->data.dom.to, TRUE, NULL),
				WEBKIT_DOM_NODE (table),
				NULL);
	}

	if (undo)
		restore_selection_to_history_event_state (editor_page, event->before);
	else
		e_editor_dom_selection_restore (editor_page);
}

static void
undo_redo_table_input (EEditorPage *editor_page,
                       EEditorHistoryEvent *event,
                       gboolean undo)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;

	document = e_editor_page_get_document (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection)) {
		g_clear_object (&dom_selection);
		return;
	}
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	g_clear_object (&dom_selection);

	/* Find if writing into table. */
	node = webkit_dom_range_get_start_container (range, NULL);
	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = get_parent_block_element (node);

	g_clear_object (&range);

	/* If writing to table we have to create different history event. */
	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (element))
		return;

	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
		webkit_dom_node_clone_node_with_error (undo ? event->data.dom.from : event->data.dom.to, TRUE, NULL),
		WEBKIT_DOM_NODE (element),
		NULL);

	e_editor_dom_selection_restore (editor_page);
}

static void
undo_redo_paste (EEditorPage *editor_page,
                 EEditorHistoryEvent *event,
                 gboolean undo)
{
	WebKitDOMDocument *document;

	document = e_editor_page_get_document (editor_page);

	if (undo) {
		if (event->type == HISTORY_PASTE_QUOTED) {
			WebKitDOMElement *tmp;
			WebKitDOMNode *parent;

			restore_selection_to_history_event_state (editor_page, event->after);

			e_editor_dom_selection_save (editor_page);
			tmp = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			if (!tmp)
				return;

			parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (tmp));
			while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (parent)))
				parent = webkit_dom_node_get_parent_node (parent);

			webkit_dom_node_replace_child (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (e_editor_dom_prepare_paragraph (editor_page, TRUE)),
				parent,
				NULL);

			e_editor_dom_selection_restore (editor_page);
		} else {
			WebKitDOMDOMWindow *dom_window = NULL;
			WebKitDOMDOMSelection *dom_selection = NULL;
			WebKitDOMElement *element, *tmp;
			WebKitDOMRange *range = NULL;

			dom_window = webkit_dom_document_get_default_view (document);
			dom_selection = webkit_dom_dom_window_get_selection (dom_window);
			g_clear_object (&dom_window);

			/* Restore the selection how it was before the event occured. */
			range = get_range_for_point (document, event->before.start);
			webkit_dom_dom_selection_remove_all_ranges (dom_selection);
			webkit_dom_dom_selection_add_range (dom_selection, range);
			g_clear_object (&range);

			e_editor_dom_selection_save (editor_page);

			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			remove_node (WEBKIT_DOM_NODE (element));

			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");

			webkit_dom_element_remove_attribute (element, "id");

			range = get_range_for_point (document, event->after.start);
			webkit_dom_dom_selection_remove_all_ranges (dom_selection);
			webkit_dom_dom_selection_add_range (dom_selection, range);
			g_clear_object (&range);
			g_clear_object (&dom_selection);

			e_editor_dom_selection_save (editor_page);

			tmp = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");

			remove_node (WEBKIT_DOM_NODE (tmp));

			webkit_dom_element_set_id (
				element, "-x-evo-selection-start-marker");

			e_editor_dom_selection_restore (editor_page);

			e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_DELETE, NULL);

			e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
		}
	} else {
		restore_selection_to_history_event_state (editor_page, event->before);

		if (event->type == HISTORY_PASTE)
			e_editor_dom_convert_and_insert_html_into_selection (editor_page, event->data.string.to, FALSE);
		else if (event->type == HISTORY_PASTE_QUOTED)
			e_editor_dom_quote_and_insert_text_into_selection (editor_page, event->data.string.to, FALSE);
		else if (event->type == HISTORY_INSERT_HTML)
			e_editor_dom_insert_html (editor_page, event->data.string.to);
		else
			e_editor_dom_convert_and_insert_html_into_selection (editor_page, event->data.string.to, FALSE);
			/* e_editor_selection_insert_as_text (selection, event->data.string.to); */
	}
}

static void
undo_redo_image (EEditorPage *editor_page,
                 EEditorHistoryEvent *event,
                 gboolean undo)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL;

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	if (undo) {
		WebKitDOMElement *element;
		WebKitDOMNode *node;

		range = get_range_for_point (document, event->before.start);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
		g_clear_object (&range);

		e_editor_dom_selection_save (editor_page);
		element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		node = webkit_dom_node_get_next_sibling  (WEBKIT_DOM_NODE (element));

		if (WEBKIT_DOM_IS_ELEMENT (node))
			if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-resizable-wrapper") ||
			    element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-smiley-wrapper"))
				remove_node (node);
		e_editor_dom_selection_restore (editor_page);
	} else {
		WebKitDOMElement *element;

		range = get_range_for_point (document, event->before.start);
		/* Create temporary node on the selection where the delete occured. */
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
		g_clear_object (&range);

		e_editor_dom_selection_save (editor_page);
		element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		/* Insert the deleted content back to the body. */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (event->data.fragment), TRUE, NULL),
			WEBKIT_DOM_NODE (element),
			NULL);

		e_editor_dom_selection_restore (editor_page);
		e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
	}

	g_clear_object (&dom_selection);
}

static void
undo_redo_replace (EEditorPage *editor_page,
                   EEditorHistoryEvent *event,
                   gboolean undo)
{
	WebKitDOMDocument *document;

	document = e_editor_page_get_document (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	if (undo) {
		WebKitDOMDOMWindow *dom_window = NULL;
		WebKitDOMDOMSelection *dom_selection = NULL;

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		g_clear_object (&dom_window);

		webkit_dom_dom_selection_modify (dom_selection, "extend", "left", "word");
		g_clear_object (&dom_selection);
	}

	e_editor_dom_exec_command (editor_page,
		E_CONTENT_EDITOR_COMMAND_INSERT_TEXT,
		undo ? event->data.string.from : event->data.string.to);

	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->before : event->after);
}

static void
undo_redo_replace_all (EEditorUndoRedoManager *manager,
                       EEditorPage *editor_page,
                       EEditorHistoryEvent *event,
                       gboolean undo)
{
	WebKitDOMDocument *document;

	document = e_editor_page_get_document (editor_page);

	if (undo) {
		if (event->type == HISTORY_REPLACE) {
			undo_redo_replace (editor_page, event, undo);
			return;
		} else {
			EEditorHistoryEvent *next_event;
			GList *next_item;
			WebKitDOMDOMWindow *dom_window = NULL;
			WebKitDOMDOMSelection *dom_selection = NULL;

			next_item = manager->priv->history->next;

			while (next_item) {
				next_event = next_item->data;

				if (next_event->type != HISTORY_REPLACE)
					break;

				if (g_strcmp0 (next_event->data.string.from, event->data.string.from) != 0)
					break;

				if (g_strcmp0 (next_event->data.string.to, event->data.string.to) != 0)
					break;

				undo_redo_replace (editor_page, next_event, undo);

				next_item = next_item->next;
			}

			manager->priv->history = next_item->prev;

			dom_window = webkit_dom_document_get_default_view (document);
			dom_selection = webkit_dom_dom_window_get_selection (dom_window);
			webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);
			g_clear_object (&dom_window);
			g_clear_object (&dom_selection);
		}
	} else {
		/* Find if this history item is part of HISTORY_REPLACE_ALL. */
		EEditorHistoryEvent *prev_event;
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
			undo_redo_replace (editor_page, event, undo);
			return;
		}

		prev_item = manager->priv->history->prev;
		while (prev_item) {
			prev_event = prev_item->data;

			if (prev_event->type == HISTORY_REPLACE) {
				undo_redo_replace (editor_page, prev_event, undo);
				prev_item = prev_item->prev;
			} else
				break;
		}

		manager->priv->history = prev_item->next;
	}
}

static void
undo_redo_remove_link (EEditorPage *editor_page,
                       EEditorHistoryEvent *event,
                       gboolean undo)
{
	WebKitDOMDocument *document;

	document = e_editor_page_get_document (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	if (undo) {
		WebKitDOMDOMWindow *dom_window = NULL;
		WebKitDOMDOMSelection *dom_selection = NULL;
		WebKitDOMElement *element;
		WebKitDOMRange *range = NULL;

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		/* Select the anchor. */
		webkit_dom_dom_selection_modify (dom_selection, "move", "left", "word");
		webkit_dom_dom_selection_modify (dom_selection, "extend", "right", "word");

		range = e_editor_dom_get_current_range (editor_page);
		element = webkit_dom_document_create_element (document, "SPAN", NULL);
		webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (element), NULL);
		g_clear_object (&range);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (event->data.fragment), TRUE, NULL),
			WEBKIT_DOM_NODE (element),
			NULL);
		remove_node (WEBKIT_DOM_NODE (element));
		g_clear_object (&dom_window);
		g_clear_object (&dom_selection);
	} else
		e_editor_dom_selection_unlink (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->before : event->after);
}

static void
undo_return_in_empty_list_item (EEditorPage *editor_page,
                                EEditorHistoryEvent *event)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker;
	WebKitDOMNode *parent;

	document = e_editor_page_get_document (editor_page);
	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker");
	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));

	if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (parent)) {
		WebKitDOMNode *parent_list;

		dom_remove_selection_markers (document);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (event->data.fragment), TRUE, NULL),
			webkit_dom_node_get_next_sibling (parent),
			NULL);

		parent_list = parent;
		while (node_is_list_or_item (webkit_dom_node_get_parent_node (parent_list)))
			parent_list = webkit_dom_node_get_parent_node (parent_list);

		merge_lists_if_possible (parent_list);
	}

	e_editor_dom_selection_restore (editor_page);
}

static gboolean
undo_return_press_after_h_rule (EEditorPage *editor_page,
                                EEditorHistoryEvent *event)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *block;
	WebKitDOMNode *node;

	document = e_editor_page_get_document (editor_page);

	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	block = get_parent_block_element (WEBKIT_DOM_NODE (selection_start_marker));
	node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE( block));

	if (!webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker)) &&
	     WEBKIT_DOM_IS_HTML_HR_ELEMENT (node)) {

		remove_node_if_empty (WEBKIT_DOM_NODE (block));
		restore_selection_to_history_event_state (editor_page, event->before);

		return TRUE;
	}

	return FALSE;
}

static void
undo_input (EEditorUndoRedoManager *manager,
            EEditorPage *editor_page,
            EEditorHistoryEvent *event)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMNode *node, *anchor_node, *tmp_node;
	gboolean remove_anchor, remove_last_character_from_font_style = FALSE;

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	restore_selection_to_history_event_state (editor_page, event->after);

	/* Undoing Return press after the HR element */
	if (e_editor_page_get_html_mode (editor_page) &&
	    g_object_get_data (G_OBJECT (event->data.fragment), "history-return-key")) {
		if (undo_return_press_after_h_rule (editor_page, event)) {
			g_clear_object (&dom_window);
			g_clear_object (&dom_selection);
			return;
		}
	}

	webkit_dom_dom_selection_modify (dom_selection, "extend", "left", "character");
	if (e_editor_dom_selection_is_citation (editor_page)) {
		/* Post processing of quoted text in body_input_event_cb needs to be called. */
		manager->priv->operation_in_progress = FALSE;
		e_editor_page_set_dont_save_history_in_body_input (editor_page, TRUE);
	}

	/* If we are undoing the text that was appended to the link we have to
	 * remove the link and make just the plain text from it. */
	anchor_node = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	node = webkit_dom_node_get_parent_node (anchor_node);
	remove_anchor = WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node);
	if (remove_anchor) {
		gchar *text_content;

		text_content = webkit_dom_node_get_text_content (node);
		/* Remove the anchor just in case we are undoing the input from
		 * the end of it. */
		remove_anchor =
			g_utf8_strlen (text_content, -1) ==
			webkit_dom_dom_selection_get_anchor_offset (dom_selection);
		g_free (text_content);
	} else if (WEBKIT_DOM_IS_TEXT (anchor_node)) {
		gchar *text_content;
		glong length;

		text_content = webkit_dom_node_get_text_content (anchor_node);
		length = g_utf8_strlen (text_content, -1);
		if (g_strcmp0 (text_content, UNICODE_ZERO_WIDTH_SPACE) == 0) {
			length -= 1;
			webkit_dom_dom_selection_modify (dom_selection, "extend", "left", "character");
		}

		g_free (text_content);

		node = webkit_dom_node_get_parent_node (anchor_node);
		if (length == 1 &&
		    ((element_has_tag (WEBKIT_DOM_ELEMENT (node), "b")) ||
		    (element_has_tag (WEBKIT_DOM_ELEMENT (node), "i")) ||
		    (element_has_tag (WEBKIT_DOM_ELEMENT (node), "u")) ||
		    (element_has_tag (WEBKIT_DOM_ELEMENT (node), "tt")) ||
		    (element_has_tag (WEBKIT_DOM_ELEMENT (node), "strike"))))
			remove_last_character_from_font_style = TRUE;
	}

	if (remove_last_character_from_font_style) {
		WebKitDOMText *text;

		text = webkit_dom_document_create_text_node (document, UNICODE_ZERO_WIDTH_SPACE);
		webkit_dom_node_replace_child (
			node,
			WEBKIT_DOM_NODE (text),
			anchor_node,
			NULL);
	} else
		e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_DELETE, NULL);

	if (remove_anchor) {
		WebKitDOMNode *child;

		/* Don't ask me why, but I got into the situation where the node
		 * that I received above was out of the document, and all the
		 * modifications to it were of course not propagated to it. Let's
		 * get that node again. */
		node = webkit_dom_dom_selection_get_anchor_node (dom_selection);
		node = webkit_dom_node_get_parent_node (node);
		while ((child = webkit_dom_node_get_first_child (node)))
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (node), child, node, NULL);

		remove_node (node);
	}

	tmp_node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (event->data.fragment));
	if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (tmp_node) &&
	    WEBKIT_DOM_IS_HTML_BR_ELEMENT (webkit_dom_node_get_last_child (tmp_node)))
		undo_return_in_empty_list_item (editor_page, event);

	g_clear_object (&dom_window);
	g_clear_object (&dom_selection);
}

static void
undo_redo_citation_split (EEditorPage *editor_page,
                          EEditorHistoryEvent *event,
                          gboolean undo)
{
	WebKitDOMDocument *document;
	gboolean in_situ = FALSE;

	document = e_editor_page_get_document (editor_page);

	if (event->before.start.x == event->after.start.x &&
	    event->before.start.y == event->after.start.y &&
	    event->before.end.x == event->after.end.x &&
	    event->before.end.y == event->after.end.y)
		in_situ = TRUE;

	if (undo) {
		WebKitDOMElement *selection_start, *parent;
		WebKitDOMNode *citation_before, *citation_after, *child, *last_child, *tmp;

		restore_selection_to_history_event_state (editor_page, event->after);

		e_editor_dom_selection_save (editor_page);
		selection_start = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (!selection_start)
			return;

		parent = get_parent_block_element (WEBKIT_DOM_NODE (selection_start));

		if (!in_situ && event->data.fragment &&
		    !webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (event->data.fragment))) {
			remove_node (WEBKIT_DOM_NODE (parent));

			goto out;
		}

		citation_before = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (parent));
		if (!e_editor_dom_node_is_citation_node (citation_before)) {
			e_editor_dom_selection_restore (editor_page);
			return;
		}

		citation_after = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent));
		if (!e_editor_dom_node_is_citation_node (citation_after)) {
			e_editor_dom_selection_restore (editor_page);
			return;
		}

		/* Get first block in next citation. */
		child = webkit_dom_node_get_first_child (citation_after);
		while (child && e_editor_dom_node_is_citation_node (child))
			child = webkit_dom_node_get_first_child (child);

		/* Get last block in previous citation. */
		last_child = webkit_dom_node_get_last_child (citation_before);
		while (last_child && e_editor_dom_node_is_citation_node (last_child))
			last_child = webkit_dom_node_get_last_child (last_child);

		/* Before appending any content to the block, check that the
		 * last node is not BR, if it is, remove it. */
		tmp = webkit_dom_node_get_last_child (last_child);
		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (tmp))
			remove_node (tmp);

		if (in_situ && event->data.fragment) {
			webkit_dom_node_append_child (
				webkit_dom_node_get_parent_node (last_child),
				webkit_dom_node_clone_node_with_error (
					WEBKIT_DOM_NODE (event->data.fragment), TRUE, NULL),
				NULL);
		} else {
			e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (child));
			e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (child));

			e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (last_child));
			e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (last_child));

			/* Copy the content of the first block to the last block to get
			 * to the state how the block looked like before it was split. */
			while ((tmp = webkit_dom_node_get_first_child (child)))
				webkit_dom_node_append_child (last_child, tmp, NULL);

			e_editor_dom_wrap_and_quote_element (editor_page, WEBKIT_DOM_ELEMENT (last_child));

			remove_node (child);
		}

		/* Move all the block from next citation to the previous one. */
		while ((child = webkit_dom_node_get_first_child (citation_after)))
			webkit_dom_node_append_child (citation_before, child, NULL);

		dom_remove_selection_markers (document);

		remove_node (WEBKIT_DOM_NODE (parent));
		remove_node (WEBKIT_DOM_NODE (citation_after));

		/* If enter was pressed when some text was selected, restore it. */
		if (event->data.fragment != NULL && !in_situ)
			undo_delete (editor_page, event);

 out:
		e_editor_dom_merge_siblings_if_necessary (editor_page, NULL);

		restore_selection_to_history_event_state (editor_page, event->before);

		e_editor_dom_force_spell_check_in_viewport (editor_page);
	} else {
		restore_selection_to_history_event_state (editor_page, event->before);

		if (in_situ) {
			WebKitDOMElement *selection_start_marker;
			WebKitDOMNode *block;

			e_editor_dom_selection_save (editor_page);

			selection_start_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");

			block = e_editor_dom_get_parent_block_node_from_child (
				WEBKIT_DOM_NODE (selection_start_marker));
			dom_remove_selection_markers (document);

			/* Remove current block (and all of its parents if they
			 * are empty) as it will be replaced by a new block that
			 * will be in the body and not in the blockquote. */
			e_editor_dom_remove_node_and_parents_if_empty (block);
		}

		e_editor_dom_insert_new_line_into_citation (editor_page, "");
	}
}

static void
undo_redo_unquote (EEditorPage *editor_page,
                   EEditorHistoryEvent *event,
                   gboolean undo)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	document = e_editor_page_get_document (editor_page);

	restore_selection_to_history_event_state (editor_page, undo ? event->after : event->before);

	e_editor_dom_selection_save (editor_page);
	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	if (undo) {
		WebKitDOMNode *next_sibling, *prev_sibling;
		WebKitDOMElement *block;

		block = get_parent_block_element (WEBKIT_DOM_NODE (element));

		next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (block));
		prev_sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (block));

		if (prev_sibling && e_editor_dom_node_is_citation_node (prev_sibling)) {
			webkit_dom_node_append_child (
				prev_sibling,
				webkit_dom_node_clone_node_with_error (event->data.dom.from, TRUE, NULL),
				NULL);

			if (next_sibling && e_editor_dom_node_is_citation_node (next_sibling)) {
				WebKitDOMNode *child;

				while  ((child = webkit_dom_node_get_first_child (next_sibling)))
					webkit_dom_node_append_child (
						prev_sibling, child, NULL);

				remove_node (next_sibling);
			}
		} else if (next_sibling && e_editor_dom_node_is_citation_node (next_sibling)) {
			webkit_dom_node_insert_before (
				next_sibling,
				webkit_dom_node_clone_node_with_error (event->data.dom.from, TRUE, NULL),
				webkit_dom_node_get_first_child (next_sibling),
				NULL);
		}

		remove_node (WEBKIT_DOM_NODE (block));
	} else
		e_editor_dom_move_quoted_block_level_up (editor_page);

	if (undo)
		e_editor_dom_selection_restore (editor_page);
	else
		restore_selection_to_history_event_state (editor_page, event->after);

	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
}

gboolean
e_editor_undo_redo_manager_is_operation_in_progress (EEditorUndoRedoManager *manager)
{
	g_return_val_if_fail (E_IS_EDITOR_UNDO_REDO_MANAGER (manager), FALSE);

	return manager->priv->operation_in_progress;
}

void
e_editor_undo_redo_manager_set_operation_in_progress (EEditorUndoRedoManager *manager,
                                                           gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_UNDO_REDO_MANAGER (manager));

	manager->priv->operation_in_progress = value;
}

static void
free_history_event (EEditorHistoryEvent *event)
{
	if (event == NULL)
		return;

	switch (event->type) {
		case HISTORY_INPUT:
		case HISTORY_DELETE:
		case HISTORY_CITATION_SPLIT:
		case HISTORY_IMAGE:
		case HISTORY_SMILEY:
		case HISTORY_REMOVE_LINK:
			if (event->data.fragment != NULL)
				g_clear_object (&event->data.fragment);
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
		case HISTORY_TABLE_INPUT:
		case HISTORY_PAGE_DIALOG:
		case HISTORY_UNQUOTE:
		case HISTORY_LINK_DIALOG:
			if (event->data.dom.from != NULL)
				g_clear_object (&event->data.dom.from);
			if (event->data.dom.to != NULL)
				g_clear_object (&event->data.dom.to);
			break;
		default:
			break;
	}

	g_free (event);
}

static void
remove_history_event (EEditorUndoRedoManager *manager,
                      GList *item)
{
	free_history_event (item->data);
	manager->priv->history = g_list_delete_link (manager->priv->history, item);
	manager->priv->history_size--;
}

static void
remove_forward_redo_history_events_if_needed (EEditorUndoRedoManager *manager)
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
e_editor_undo_redo_manager_insert_history_event (EEditorUndoRedoManager *manager,
                                                 EEditorHistoryEvent *event)
{
	g_return_if_fail (E_IS_EDITOR_UNDO_REDO_MANAGER (manager));

	if (manager->priv->operation_in_progress)
		return;

	if (camel_debug ("webkit:undo")) {
		printf ("\nINSERTING EVENT:\n");
		print_history_event (event);
	}

	remove_forward_redo_history_events_if_needed (manager);

	if (manager->priv->history_size >= HISTORY_SIZE_LIMIT) {
		EEditorHistoryEvent *prev_event;
		GList *item;

		remove_history_event (manager, g_list_last (manager->priv->history)->prev);
		while ((item = g_list_last (manager->priv->history)) && (item = item->prev) &&
		       (prev_event = item->data) && prev_event->type == HISTORY_AND) {
			remove_history_event (manager, g_list_last (manager->priv->history)->prev);
			remove_history_event (manager, g_list_last (manager->priv->history)->prev);
		}
	}

	manager->priv->history = g_list_prepend (manager->priv->history, event);
	manager->priv->history_size++;

	if (camel_debug ("webkit:undo"))
		print_history (manager);

	g_object_notify (G_OBJECT (manager), "can-undo");
}

EEditorHistoryEvent *
e_editor_undo_redo_manager_get_current_history_event (EEditorUndoRedoManager *manager)
{
	g_return_val_if_fail (E_IS_EDITOR_UNDO_REDO_MANAGER (manager), NULL);

	if (manager->priv->history)
		return manager->priv->history->data;

	return NULL;
}

void
e_editor_undo_redo_manager_remove_current_history_event (EEditorUndoRedoManager *manager)
{
	g_return_if_fail (E_IS_EDITOR_UNDO_REDO_MANAGER (manager));

	if (!manager->priv->history)
		return;

	if (camel_debug ("webkit:undo")) {
		printf ("\nREMOVING EVENT:\n");
		print_history_event (manager->priv->history->data);
	}

	remove_history_event (manager, manager->priv->history);

	if (camel_debug ("webkit:undo"))
		print_history (manager);
}

void
e_editor_undo_redo_manager_insert_dash_history_event (EEditorUndoRedoManager *manager)
{
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	EEditorPage *editor_page;
	EEditorHistoryEvent *event, *last;
	GList *history;

	g_return_if_fail (E_IS_EDITOR_UNDO_REDO_MANAGER (manager));

	editor_page = editor_undo_redo_manager_ref_editor_page (manager);
	g_return_if_fail (editor_page != NULL);

	event = g_new0 (EEditorHistoryEvent, 1);
	event->type = HISTORY_INPUT;

	document = e_editor_page_get_document (editor_page);
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

	last = e_editor_undo_redo_manager_get_current_history_event (manager);
	/* The dash event needs to have the same coordinates as the character
	 * that is right after it. */
	event->after.start.x = last->after.start.x;
	event->after.start.y = last->after.start.y;
	event->after.end.x = last->after.end.x;
	event->after.end.y = last->after.end.y;

	history = manager->priv->history;
	if (history) {
		EEditorHistoryEvent *item;
		WebKitDOMNode *first_child;

		item = history->data;

		if (item->type != HISTORY_INPUT) {
			g_object_unref (editor_page);
			return;
		}

		first_child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (item->data.fragment));
		if (WEBKIT_DOM_IS_TEXT (first_child)) {
			guint diff;

			diff = event->after.start.x - item->after.start.x;

			/* We need to move the coordinate of the last
			 * event by one character. */
			last->after.start.x += diff;
			last->after.end.x += diff;

			manager->priv->history = g_list_insert_before (
				manager->priv->history, history, event);
		}
	}

	g_object_unref (editor_page);
}

gboolean
e_editor_undo_redo_manager_can_undo (EEditorUndoRedoManager *manager)
{
	g_return_val_if_fail (E_IS_EDITOR_UNDO_REDO_MANAGER (manager), FALSE);

	if (manager->priv->history) {
		EEditorHistoryEvent *event;

		event = manager->priv->history->data;

		return (event->type != HISTORY_START);
	} else
		return FALSE;
}

void
e_editor_undo_redo_manager_undo (EEditorUndoRedoManager *manager)
{
	EEditorHistoryEvent *event;
	EEditorPage *editor_page;
	GList *history;

	g_return_if_fail (E_IS_EDITOR_UNDO_REDO_MANAGER (manager));

	if (!e_editor_undo_redo_manager_can_undo (manager))
		return;

	history = manager->priv->history;
	event = history->data;

	if (camel_debug ("webkit:undo")) {
		printf ("\nUNDOING EVENT:\n");
		print_history_event (event);
	}

	manager->priv->operation_in_progress = TRUE;

	editor_page = editor_undo_redo_manager_ref_editor_page (manager);
	g_return_if_fail (editor_page != NULL);

	switch (event->type) {
		case HISTORY_BOLD:
		case HISTORY_ITALIC:
		case HISTORY_STRIKETHROUGH:
		case HISTORY_UNDERLINE:
		case HISTORY_FONT_SIZE:
		case HISTORY_ALIGNMENT:
		case HISTORY_BLOCK_FORMAT:
		case HISTORY_MONOSPACE:
			undo_redo_style_change (editor_page, event, TRUE);
			break;
		case HISTORY_DELETE:
			undo_delete (editor_page, event);
			break;
		case HISTORY_INDENT:
			undo_redo_indent (editor_page, event, TRUE);
			break;
		case HISTORY_INPUT:
			undo_input (manager, editor_page, event);
			break;
		case HISTORY_REMOVE_LINK:
			undo_redo_remove_link (editor_page, event, TRUE);
			break;
		case HISTORY_FONT_COLOR:
			undo_redo_font_color (editor_page, event, TRUE);
			break;
		case HISTORY_CITATION_SPLIT:
			undo_redo_citation_split (editor_page, event, TRUE);
			break;
		case HISTORY_PASTE:
		case HISTORY_PASTE_AS_TEXT:
		case HISTORY_PASTE_QUOTED:
		case HISTORY_INSERT_HTML:
			undo_redo_paste (editor_page, event, TRUE);
			break;
		case HISTORY_IMAGE:
		case HISTORY_SMILEY:
			undo_redo_image (editor_page, event, TRUE);
			break;
		case HISTORY_WRAP:
			undo_redo_wrap (editor_page, event, TRUE);
			break;
		case HISTORY_IMAGE_DIALOG:
			undo_redo_image_dialog (editor_page, event, TRUE);
			break;
		case HISTORY_LINK_DIALOG:
			undo_redo_link_dialog (editor_page, event, TRUE);
			break;
		case HISTORY_TABLE_DIALOG:
			undo_redo_table_dialog (editor_page, event, TRUE);
			break;
		case HISTORY_TABLE_INPUT:
			undo_redo_table_input (editor_page, event, TRUE);
			break;
		case HISTORY_PAGE_DIALOG:
			undo_redo_page_dialog (editor_page, event, TRUE);
			break;
		case HISTORY_HRULE_DIALOG:
			undo_redo_hrule_dialog (editor_page, event, TRUE);
			break;
		case HISTORY_REPLACE:
		case HISTORY_REPLACE_ALL:
			undo_redo_replace_all (manager, editor_page, event, TRUE);
			break;
		case HISTORY_UNQUOTE:
			undo_redo_unquote (editor_page, event, TRUE);
			break;
		case HISTORY_AND:
			g_warning ("Unhandled HISTORY_AND event!");
			break;
		default:
			g_object_unref (editor_page);
			return;
	}

	if (history->next) {
		event = history->next->data;
		if (event->type == HISTORY_AND) {
			manager->priv->history = history->next->next;
			e_editor_undo_redo_manager_undo (manager);
			g_object_unref (editor_page);
			return;
		}

		manager->priv->history = manager->priv->history->next;
	}

	if (camel_debug ("webkit:undo"))
		print_undo_events (manager);

	manager->priv->operation_in_progress = FALSE;

	e_editor_page_emit_selection_changed (editor_page);

	g_object_unref (editor_page);

	g_object_notify (G_OBJECT (manager), "can-undo");
	g_object_notify (G_OBJECT (manager), "can-redo");
}

gboolean
e_editor_undo_redo_manager_can_redo (EEditorUndoRedoManager *manager)
{
	g_return_val_if_fail (E_IS_EDITOR_UNDO_REDO_MANAGER (manager), FALSE);

	if (manager->priv->history && manager->priv->history->prev)
		return TRUE;
	else
		return FALSE;
}

void
e_editor_undo_redo_manager_redo (EEditorUndoRedoManager *manager)
{
	EEditorPage *editor_page;
	EEditorHistoryEvent *event;
	GList *history;

	if (!e_editor_undo_redo_manager_can_redo (manager))
		return;

	history = manager->priv->history;
	event = history->prev->data;

	if (camel_debug ("webkit:undo")) {
		printf ("\nREDOING EVENT:\n");
		print_history_event (event);
	}

	editor_page = editor_undo_redo_manager_ref_editor_page (manager);
	g_return_if_fail (editor_page != NULL);

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
			undo_redo_style_change (editor_page, event, FALSE);
			break;
		case HISTORY_DELETE:
			redo_delete (editor_page, event);
			break;
		case HISTORY_INDENT:
			undo_redo_indent (editor_page, event, FALSE);
			break;
		case HISTORY_INPUT:
			undo_delete (editor_page, event);
			e_editor_dom_check_magic_smileys (editor_page);
			{
				gchar *text_content;
				WebKitDOMNode *first_child;

				first_child = webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (event->data.fragment));
				text_content = webkit_dom_node_get_text_content (first_child);
				/* Call magic links when the space was pressed. */
				if (g_str_has_prefix (text_content, UNICODE_NBSP)) {
					e_editor_page_set_space_key_pressed (editor_page, TRUE);
					e_editor_dom_check_magic_links (editor_page, FALSE);
					e_editor_page_set_space_key_pressed (editor_page, FALSE);
				}
				g_free (text_content);
			}
			break;
		case HISTORY_REMOVE_LINK:
			undo_redo_remove_link (editor_page, event, FALSE);
			break;
		case HISTORY_FONT_COLOR:
			undo_redo_font_color (editor_page, event, FALSE);
			break;
		case HISTORY_CITATION_SPLIT:
			undo_redo_citation_split (editor_page, event, FALSE);
			break;
		case HISTORY_PASTE:
		case HISTORY_PASTE_AS_TEXT:
		case HISTORY_PASTE_QUOTED:
		case HISTORY_INSERT_HTML:
			undo_redo_paste (editor_page, event, FALSE);
			break;
		case HISTORY_IMAGE:
		case HISTORY_SMILEY:
			undo_redo_image (editor_page, event, FALSE);
			break;
		case HISTORY_WRAP:
			undo_redo_wrap (editor_page, event, FALSE);
			break;
		case HISTORY_IMAGE_DIALOG:
			undo_redo_image_dialog (editor_page, event, FALSE);
			break;
		case HISTORY_LINK_DIALOG:
			undo_redo_link_dialog (editor_page, event, FALSE);
			break;
		case HISTORY_TABLE_DIALOG:
			undo_redo_table_dialog (editor_page, event, FALSE);
			break;
		case HISTORY_TABLE_INPUT:
			undo_redo_table_input (editor_page, event, FALSE);
			break;
		case HISTORY_PAGE_DIALOG:
			undo_redo_page_dialog (editor_page, event, FALSE);
			break;
		case HISTORY_HRULE_DIALOG:
			undo_redo_hrule_dialog (editor_page, event, FALSE);
			break;
		case HISTORY_REPLACE:
		case HISTORY_REPLACE_ALL:
			undo_redo_replace_all (manager, editor_page, event, FALSE);
			break;
		case HISTORY_UNQUOTE:
			undo_redo_unquote (editor_page, event, FALSE);
			break;
		case HISTORY_AND:
			g_warning ("Unhandled HISTORY_AND event!");
			break;
		default:
			g_object_unref (editor_page);
			return;
	}

	if (history->prev && history->prev->prev) {
		event = history->prev->prev->data;
		if (event->type == HISTORY_AND) {
			manager->priv->history = manager->priv->history->prev->prev;
			e_editor_undo_redo_manager_redo (manager);
			g_object_unref (editor_page);
			return;
		}
	}

	manager->priv->history = manager->priv->history->prev;

	if (camel_debug ("webkit:undo"))
		print_redo_events (manager);

	manager->priv->operation_in_progress = FALSE;

	e_editor_page_emit_selection_changed (editor_page);

	g_object_unref (editor_page);

	g_object_notify (G_OBJECT (manager), "can-undo");
	g_object_notify (G_OBJECT (manager), "can-redo");
}

void
e_editor_undo_redo_manager_clean_history (EEditorUndoRedoManager *manager)
{
	EEditorPage *editor_page;
	EEditorHistoryEvent *ev;

	g_return_if_fail (E_IS_EDITOR_UNDO_REDO_MANAGER (manager));

	if (manager->priv->history != NULL) {
		g_list_free_full (manager->priv->history, (GDestroyNotify) free_history_event);
		manager->priv->history = NULL;
	}

	manager->priv->history_size = 0;
	editor_page = editor_undo_redo_manager_ref_editor_page (manager);
	g_return_if_fail (editor_page != NULL);
	e_editor_page_set_dont_save_history_in_body_input (editor_page, FALSE);
	g_object_unref (editor_page);
	manager->priv->operation_in_progress = FALSE;

	ev = g_new0 (EEditorHistoryEvent, 1);
	ev->type = HISTORY_START;
	manager->priv->history = g_list_append (manager->priv->history, ev);

	g_object_notify (G_OBJECT (manager), "can-undo");
	g_object_notify (G_OBJECT (manager), "can-redo");
}

static void
editor_undo_redo_manager_set_editor_page (EEditorUndoRedoManager *manager,
                                          EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	g_weak_ref_set (&manager->priv->editor_page, editor_page);
}

static void
editor_undo_redo_manager_dispose (GObject *object)
{
	EEditorUndoRedoManagerPrivate *priv;

	priv = E_EDITOR_UNDO_REDO_MANAGER_GET_PRIVATE (object);

	if (priv->history != NULL) {
		g_list_free_full (priv->history, (GDestroyNotify) free_history_event);
		priv->history = NULL;
	}

	g_weak_ref_set (&priv->editor_page, NULL);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_editor_undo_redo_manager_parent_class)->dispose (object);
}

static void
editor_undo_redo_manager_get_property (GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_REDO:
			g_value_set_boolean (
				value, e_editor_undo_redo_manager_can_redo (
				E_EDITOR_UNDO_REDO_MANAGER (object)));
			return;

		case PROP_CAN_UNDO:
			g_value_set_boolean (
				value, e_editor_undo_redo_manager_can_undo (
				E_EDITOR_UNDO_REDO_MANAGER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_undo_redo_manager_set_property (GObject *object,
                                            guint property_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR_PAGE:
			editor_undo_redo_manager_set_editor_page (
				E_EDITOR_UNDO_REDO_MANAGER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_editor_undo_redo_manager_class_init (EEditorUndoRedoManagerClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EEditorUndoRedoManagerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = editor_undo_redo_manager_dispose;
	object_class->get_property = editor_undo_redo_manager_get_property;
	object_class->set_property = editor_undo_redo_manager_set_property;

	/**
	 * EEditorUndoRedoManager:can-redo
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
	 * EEditorUndoRedoManager:can-undo
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
		PROP_EDITOR_PAGE,
		g_param_spec_object (
			"editor-page",
			NULL,
			NULL,
			E_TYPE_EDITOR_PAGE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_editor_undo_redo_manager_init (EEditorUndoRedoManager *manager)
{
	manager->priv = E_EDITOR_UNDO_REDO_MANAGER_GET_PRIVATE (manager);

	manager->priv->operation_in_progress = FALSE;
	manager->priv->history = NULL;
	manager->priv->history_size = 0;
}
