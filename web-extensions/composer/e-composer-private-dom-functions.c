/*
 * e-composer-private-dom-functions.c
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

#include "e-composer-private-dom-functions.h"

#include "e-html-editor-web-extension.h"
#include "e-html-editor-selection-dom-functions.h"
#include "e-html-editor-view-dom-functions.h"

#include <string.h>

#include <web-extensions/e-dom-utils.h>
#include <e-util/e-misc-utils.h>

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

gchar *
dom_remove_signatures (WebKitDOMDocument *document,
                       EHTMLEditorWebExtension *extension,
                       gboolean top_signature)
{
	gchar *ret_val = NULL;
	gulong length, ii;
	WebKitDOMHTMLCollection *signatures;

	g_return_val_if_fail (WEBKIT_DOM_IS_HTML_DOCUMENT (document), NULL);
	g_return_val_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension), NULL);

	signatures = webkit_dom_document_get_elements_by_class_name_as_html_collection (
		document, "-x-evo-signature-wrapper");
	length = webkit_dom_html_collection_get_length (signatures);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *wrapper, *signature;
		gchar *id;

		wrapper = webkit_dom_html_collection_item (signatures, ii);
		signature = webkit_dom_node_get_first_child (wrapper);
		id = webkit_dom_element_get_id (WEBKIT_DOM_ELEMENT (signature));

		/* When we are editing a message with signature we need to set active
		 * signature id in signature combo box otherwise no signature will be
		 * added but we have to do it just once when the composer opens */
		if (ret_val)
			g_free (ret_val);
		ret_val = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (signature), "name");

		if (id && (strlen (id) == 1) && (*id == '1')) {
			/* If the top signature was set we have to remove the NL
			 * that was inserted after it */
			if (top_signature) {
				WebKitDOMElement *spacer;

				spacer = webkit_dom_document_query_selector (
				document, ".-x-evo-top-signature-spacer", NULL);
				if (spacer)
					remove_node_if_empty (WEBKIT_DOM_NODE (spacer));
			}
			/* We have to remove the div containing the span with signature */
			remove_node (wrapper);
			g_object_unref (wrapper);

			g_free (id);
			break;
		}

		g_object_unref (wrapper);
		g_free (id);
	}
	g_object_unref (signatures);

	return ret_val;
}

static WebKitDOMElement *
prepare_top_signature_spacer (WebKitDOMDocument *document,
                              EHTMLEditorWebExtension *extension)
{
	WebKitDOMElement *element;

	element = dom_prepare_paragraph (document, extension, FALSE);
	webkit_dom_element_remove_attribute (element, "id");
	element_add_class (element, "-x-evo-top-signature-spacer");

	return element;
}

static void
composer_move_caret (WebKitDOMDocument *document,
                     EHTMLEditorWebExtension *extension,
                     gboolean top_signature,
		     gboolean start_bottom)
{
	gboolean is_message_from_draft;
	gboolean is_message_from_edit_as_new;
	gboolean is_from_new_message;
	gboolean has_paragraphs_in_body = TRUE;
	WebKitDOMElement *element, *signature;
	WebKitDOMHTMLElement *body;
	WebKitDOMHTMLCollection *paragraphs;

	is_message_from_draft = e_html_editor_web_extension_is_message_from_draft (extension);
	is_message_from_edit_as_new =
		e_html_editor_web_extension_is_message_from_edit_as_new (extension);
	is_from_new_message = e_html_editor_web_extension_is_from_new_message (extension);

	body = webkit_dom_document_get_body (document);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-message", "", NULL);

	/* If editing message as new don't handle with caret */
	if (is_message_from_edit_as_new || is_message_from_draft) {
		if (is_message_from_edit_as_new)
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (body),
				"data-edit-as-new",
				"",
				NULL);

		if (is_message_from_edit_as_new && !is_message_from_draft) {
			element = WEBKIT_DOM_ELEMENT (body);
			e_html_editor_web_extension_block_selection_changed_callback (extension);
			goto move_caret;
		} else
			dom_scroll_to_caret (document);

		return;
	}
	e_html_editor_web_extension_block_selection_changed_callback (extension);

	/* When the new message is written from the beginning - note it into body */
	if (is_from_new_message)
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body), "data-new-message", "", NULL);

	paragraphs = webkit_dom_document_get_elements_by_class_name_as_html_collection (document, "-x-evo-paragraph");
	signature = webkit_dom_document_query_selector (document, ".-x-evo-signature-wrapper", NULL);
	/* Situation when wrapped paragraph is just in signature and not in message body */
	if (webkit_dom_html_collection_get_length (paragraphs) == 1)
		if (signature && webkit_dom_element_query_selector (signature, ".-x-evo-paragraph", NULL))
			has_paragraphs_in_body = FALSE;

	/*
	 *
	 * Keeping Signatures in the beginning of composer
	 * ------------------------------------------------
	 *
	 * Purists are gonna blast me for this.
	 * But there are so many people (read Outlook users) who want this.
	 * And Evo is an exchange-client, Outlook-replacement etc.
	 * So Here it goes :(
	 *
	 * -- Sankar
	 *
	 */
	if (signature && top_signature) {
		WebKitDOMElement *spacer;

		spacer = prepare_top_signature_spacer (document, extension);
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			WEBKIT_DOM_NODE (spacer),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (signature)),
			NULL);
	}

	if (webkit_dom_html_collection_get_length (paragraphs) == 0)
		has_paragraphs_in_body = FALSE;

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-input-start");
	if (!signature) {
		if (start_bottom) {
			if (!element) {
				element = dom_prepare_paragraph (document, extension, FALSE);
				webkit_dom_element_set_id (element, "-x-evo-input-start");
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (element),
					NULL);
			}
		} else
			element = WEBKIT_DOM_ELEMENT (body);

		g_object_unref (paragraphs);
		goto move_caret;
	}

	/* When there is an option composer-reply-start-bottom set we have
	 * to move the caret between reply and signature. */
	if (!has_paragraphs_in_body) {
		element = dom_prepare_paragraph (document, extension, FALSE);
		webkit_dom_element_set_id (element, "-x-evo-input-start");
		if (top_signature) {
			if (start_bottom) {
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (element),
					NULL);
			} else {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (element),
					WEBKIT_DOM_NODE (signature),
					NULL);
			}
		} else {
			if (start_bottom)
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (element),
					WEBKIT_DOM_NODE (signature),
					NULL);
			else
				element = WEBKIT_DOM_ELEMENT (body);
		}
	} else {
		if (!element && top_signature) {
			element = dom_prepare_paragraph (document, extension, FALSE);
			webkit_dom_element_set_id (element, "-x-evo-input-start");
			if (start_bottom) {
					webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (element),
					NULL);
			} else {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (element),
					WEBKIT_DOM_NODE (signature),
					NULL);
			}
		} else if (element && top_signature && !start_bottom) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (body),
				WEBKIT_DOM_NODE (element),
				WEBKIT_DOM_NODE (signature),
				NULL);
		} else if (element && start_bottom) {
			/* Leave it how it is */
		} else
			element = WEBKIT_DOM_ELEMENT (body);
	}

	g_object_unref (paragraphs);
 move_caret:
	if (element) {
		WebKitDOMDOMSelection *dom_selection;
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMRange *range;

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		range = webkit_dom_document_create_range (document);

		webkit_dom_range_select_node_contents (
			range, WEBKIT_DOM_NODE (element), NULL);
		webkit_dom_range_collapse (range, TRUE, NULL);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);

		g_clear_object (&dom_selection);
		g_clear_object (&dom_window);
		g_clear_object (&range);

		if (start_bottom)
			dom_scroll_to_caret (document);
	}

	dom_force_spell_check_in_viewport (document, extension);
	e_html_editor_web_extension_unblock_selection_changed_callback (extension);
}

void
dom_insert_signature (WebKitDOMDocument *document,
                      EHTMLEditorWebExtension *extension,
                      const gchar *signature_html,
                      gboolean top_signature,
		      gboolean start_bottom)
{
	WebKitDOMElement *element;
	WebKitDOMHTMLElement *body;

	g_return_if_fail (WEBKIT_DOM_IS_HTML_DOCUMENT (document));
	g_return_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension));
	g_return_if_fail (signature_html && *signature_html);

	body = webkit_dom_document_get_body (document);
	element = webkit_dom_document_create_element (document, "DIV", NULL);
	webkit_dom_element_set_class_name (element, "-x-evo-signature-wrapper");

	webkit_dom_element_set_inner_html (element, signature_html, NULL);

	if (top_signature) {
		WebKitDOMNode *child =
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		if (start_bottom) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (body),
				WEBKIT_DOM_NODE (element),
				child,
				NULL);
		} else {
			/* When we are using signature on top the caret
			 * should be before the signature */
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (body),
				WEBKIT_DOM_NODE (element),
				child,
				NULL);
		}
	} else {
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (body),
			WEBKIT_DOM_NODE (element),
			NULL);
	}

	composer_move_caret (document, extension, top_signature, start_bottom);
}

static void
insert_nbsp_history_event (WebKitDOMDocument *document,
			   EHTMLEditorUndoRedoManager *manager,
                           gboolean delete,
                           guint x,
                           guint y)
{
	EHTMLEditorHistoryEvent *event;
	WebKitDOMDocumentFragment *fragment;

	event = g_new0 (EHTMLEditorHistoryEvent, 1);
	event->type = HISTORY_AND;
	e_html_editor_undo_redo_manager_insert_history_event (manager, event);

	fragment = webkit_dom_document_create_document_fragment (document);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (fragment),
		WEBKIT_DOM_NODE (
			webkit_dom_document_create_text_node (document, UNICODE_NBSP)),
		NULL);

	event = g_new0 (EHTMLEditorHistoryEvent, 1);
	event->type = HISTORY_DELETE;

	if (delete)
		g_object_set_data (G_OBJECT (event), "history-delete-key", GINT_TO_POINTER (1));

	event->data.fragment = fragment;

	event->before.start.x = x;
	event->before.start.y = y;
	event->before.end.x = x;
	event->before.end.y = y;

	event->after.start.x = x;
	event->after.start.y = y;
	event->after.end.x = x;
	event->after.end.y = y;

	e_html_editor_undo_redo_manager_insert_history_event (manager, event);
}

void
dom_save_drag_and_drop_history (WebKitDOMDocument *document,
				EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *event;
	EHTMLEditorUndoRedoManager *manager;
	gboolean start_to_start, end_to_end;
	gchar *range_text;
	guint x, y;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMRange *beginning_of_line = NULL;
	WebKitDOMRange *range = NULL, *range_clone = NULL;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);

	if (!(dom_window = webkit_dom_document_get_default_view (document)))
		return;

	if (!(dom_selection = webkit_dom_dom_window_get_selection (dom_window))) {
		g_object_unref (dom_window);
		return;
	}

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_object_unref (dom_selection);
		g_object_unref (dom_window);
		return;
	}

	/* Obtain the dragged content. */
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	range_clone = webkit_dom_range_clone_range (range, NULL);

	/* Create the history event for the content that will
	 * be removed by DnD. */
	event = g_new0 (EHTMLEditorHistoryEvent, 1);
	event->type = HISTORY_DELETE;

	dom_selection_get_coordinates (
		document,
		&event->before.start.x,
		&event->before.start.y,
		&event->before.end.x,
		&event->before.end.y);

	x = event->before.start.x;
	y = event->before.start.y;

	event->after.start.x = x;
	event->after.start.y = y;
	event->after.end.x = x;
	event->after.end.y = y;

	/* Save the content that will be removed. */
	fragment = webkit_dom_range_clone_contents (range_clone, NULL);

	/* Extend the cloned range to point one character after
	 * the selection ends to later check if there is a whitespace
	 * after it. */
	webkit_dom_range_set_end (
		range_clone,
		webkit_dom_range_get_end_container (range_clone, NULL),
		webkit_dom_range_get_end_offset (range_clone, NULL) + 1,
		NULL);
	range_text = webkit_dom_range_get_text (range_clone);

	/* Check if the current selection starts on the beginning
	 * of line. */
	webkit_dom_dom_selection_modify (
		dom_selection, "extend", "left", "lineboundary");
	beginning_of_line = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	start_to_start = webkit_dom_range_compare_boundary_points (
		beginning_of_line, 0 /* START_TO_START */, range, NULL) == 0;

	/* Restore the selection to state before the check. */
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_object_unref (beginning_of_line);

	/* Check if the current selection end on the end of the line. */
	webkit_dom_dom_selection_modify (
		dom_selection, "extend", "right", "lineboundary");
	beginning_of_line = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	end_to_end = webkit_dom_range_compare_boundary_points (
		beginning_of_line, 2 /* END_TO_END */, range, NULL) == 0;

	/* Dragging the whole line. */
	if (start_to_start && end_to_end) {
		WebKitDOMNode *container, *actual_block, *tmp_block;

		/* Select the whole line (to the beginning of the next
		 * one so we can reuse the undo code while undoing this.
		 * Because of this we need to special mark the event
		 * with history-drag-and-drop to correct the selection
		 * after undoing it (otherwise the beginning of the next
		 * line will be selected as well. */
		webkit_dom_dom_selection_modify (
			dom_selection, "extend", "right", "character");
		g_object_unref (beginning_of_line);
		beginning_of_line = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

		container = webkit_dom_range_get_end_container (range, NULL);
		actual_block = get_parent_block_node_from_child (container);

		tmp_block = webkit_dom_range_get_end_container (beginning_of_line, NULL);
		if ((tmp_block = get_parent_block_node_from_child (tmp_block))) {
			dom_selection_get_coordinates (
				document,
				&event->before.start.x,
				&event->before.start.y,
				&event->before.end.x,
				&event->before.end.y);

			/* Create the right content for the history event. */
			fragment = webkit_dom_document_create_document_fragment (document);
			/* The removed line. */
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				webkit_dom_node_clone_node (actual_block, TRUE),
				NULL);
			/* The following block, but empty. */
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				webkit_dom_node_clone_node (tmp_block, FALSE),
				NULL);
			g_object_set_data (
				G_OBJECT (event),
				"history-drag-and-drop",
				GINT_TO_POINTER (1));
			/* It should act as a Delete key press. */
			g_object_set_data (
				G_OBJECT (event),
				"history-delete-key",
				GINT_TO_POINTER (1));
		}
	}

	event->data.fragment = fragment;
	e_html_editor_undo_redo_manager_insert_history_event (manager, event);

	/* Selection is ending on the end of the line, check if
	 * there is a space before the selection start. If so, it
	 * will be removed and we need create the history event
	 * for it. */
	if (end_to_end) {
		gchar *range_text_start;
		glong start_offset;

		start_offset = webkit_dom_range_get_start_offset (range_clone, NULL);
		webkit_dom_range_set_start (
			range_clone,
			webkit_dom_range_get_start_container (range_clone, NULL),
			start_offset > 0 ? start_offset - 1 : 0,
			NULL);

		range_text_start = webkit_dom_range_get_text (range_clone);
		if (g_str_has_prefix (range_text_start, " ") ||
		    g_str_has_prefix (range_text_start, UNICODE_NBSP))
			insert_nbsp_history_event (document, manager, FALSE, x, y);

		g_free (range_text_start);
	}

	/* WebKit removes the space (if presented) after selection and
	 * we need to create a new history event for it. */
	if (g_str_has_suffix (range_text, " ") ||
	    g_str_has_suffix (range_text, UNICODE_NBSP))
		insert_nbsp_history_event (document, manager, TRUE, x, y);

	g_free (range_text);

	/* Restore the selection to original state. */
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_object_unref (beginning_of_line);

	/* All the things above were about removing the content,
	 * create an AND event to continue later with inserting
	 * the dropped content. */
	event = g_new0 (EHTMLEditorHistoryEvent, 1);
	event->type = HISTORY_AND;
	e_html_editor_undo_redo_manager_insert_history_event (manager, event);

	g_object_unref (dom_selection);
	g_object_unref (dom_window);

	g_object_unref (range);
	g_object_unref (range_clone);
}

void
dom_clean_after_drag_and_drop (WebKitDOMDocument *document,
                               EHTMLEditorWebExtension *extension)
{
	dom_save_history_for_drop (document, extension);
	dom_check_magic_links (document, extension, FALSE);
}
