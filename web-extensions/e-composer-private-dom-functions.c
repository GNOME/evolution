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
	gulong list_length, ii;
	WebKitDOMNodeList *signatures;

	g_return_val_if_fail (WEBKIT_DOM_IS_HTML_DOCUMENT (document), NULL);
	g_return_val_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension), NULL);

	signatures = webkit_dom_document_get_elements_by_class_name (
		document, "-x-evo-signature-wrapper");
	list_length = webkit_dom_node_list_get_length (signatures);
	for (ii = 0; ii < list_length; ii++) {
		WebKitDOMNode *wrapper, *signature;
		gchar *id;

		wrapper = webkit_dom_node_list_item (signatures, ii);
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

			g_free (id);
			break;
		}

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

	element = prepare_paragraph (document, extension, FALSE);
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
	EHTMLEditorSelection *editor_selection;
	gboolean is_message_from_draft;
	gboolean is_message_from_edit_as_new;
	gboolean is_from_new_message;
	gboolean has_paragraphs_in_body = TRUE;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMElement *element, *signature;
	WebKitDOMHTMLElement *body;
	WebKitDOMNodeList *list;
	WebKitDOMRange *new_range;

	is_message_from_draft = e_html_editor_web_extension_is_message_from_draft (extension);
	is_message_from_edit_as_new =
		e_html_editor_web_extension_is_message_from_edit_as_new (extension);
	is_from_new_message = e_html_editor_web_extension_is_from_new_message (extension);

	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	body = webkit_dom_document_get_body (document);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-message", "", NULL);
	new_range = webkit_dom_document_create_range (document);

	/* If editing message as new don't handle with caret */
	if (is_message_from_edit_as_new || is_message_from_draft) {
		if (is_message_from_edit_as_new)
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (body),
				"data-edit-as-new",
				"",
				NULL);

		if (is_message_from_edit_as_new) {
			element = WEBKIT_DOM_ELEMENT (body);
/* FIXME WK2
			e_html_editor_selection_block_selection_changed (editor_selection);*/
			goto move_caret;
		} else
			dom_scroll_to_caret (document);

		return;
	}
/* FIXME WK2
	e_html_editor_selection_block_selection_changed (editor_selection);*/

	/* When the new message is written from the beginning - note it into body */
	if (is_from_new_message)
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body), "data-new-message", "", NULL);

	list = webkit_dom_document_get_elements_by_class_name (document, "-x-evo-paragraph");
	signature = webkit_dom_document_query_selector (document, ".-x-evo-signature-wrapper", NULL);
	/* Situation when wrapped paragraph is just in signature and not in message body */
	if (webkit_dom_node_list_get_length (list) == 1)
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

		spacer = prepare_top_signature_spacer (editor_selection, document);
		webkit_dom_element_set_id (element, "-x-evo-input-start");
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			WEBKIT_DOM_NODE (spacer),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (signature)),
			NULL);
	}

	if (webkit_dom_node_list_get_length (list) == 0)
		has_paragraphs_in_body = FALSE;

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-input-start");
	if (!signature) {
		if (start_bottom) {
			if (!element) {
				element = prepare_paragraph (editor_selection, document);
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (element),
					NULL);
			}
		} else
			element = WEBKIT_DOM_ELEMENT (body);

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
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (body),
				WEBKIT_DOM_NODE (element),
				WEBKIT_DOM_NODE (signature),
				NULL);
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
		}
	}

	g_object_unref (list);
 move_caret:
	if (element) {
		webkit_dom_range_select_node_contents (
		new_range, WEBKIT_DOM_NODE (element), NULL);
		webkit_dom_range_collapse (new_range, TRUE, NULL);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, new_range);
	}

	dom_force_spell_check (document, extension);
/* FIXME WK2
	e_html_editor_selection_unblock_selection_changed (editor_selection);*/
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

	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element), signature_html, NULL);

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

