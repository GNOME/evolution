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

#include "evolution-config.h"

#include <string.h>

#include <webkitdom/webkitdom.h>

#include <camel/camel.h>

#include "web-extensions/e-dom-utils.h"

#include "e-editor-page.h"
#include "e-editor-dom-functions.h"
#include "e-editor-undo-redo-manager.h"

#include "e-composer-dom-functions.h"

static WebKitDOMElement *
prepare_top_signature_spacer (EEditorPage *editor_page)
{
	WebKitDOMElement *element;

	element = e_editor_dom_prepare_paragraph (editor_page, FALSE);
	webkit_dom_element_remove_attribute (element, "id");
	element_add_class (element, "-x-evo-top-signature-spacer");

	return element;
}

static gboolean
add_signature_delimiter (void)
{
	gboolean ret_val;
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	ret_val = !g_settings_get_boolean (settings, "composer-no-signature-delim");
	g_object_unref (settings);

	return ret_val;
}

static gboolean
use_top_signature (void)
{
	gboolean ret_val;
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	ret_val = g_settings_get_boolean (settings, "composer-top-signature");
	g_object_unref (settings);

	return ret_val;
}

static gboolean
start_typing_at_bottom (void)
{
	gboolean ret_val;
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	ret_val = g_settings_get_boolean (settings, "composer-reply-start-bottom");
	g_object_unref (settings);

	return ret_val;
}

static void
move_caret_after_signature_inserted (EEditorPage *editor_page,
				     gboolean start_bottom,
				     gboolean top_signature)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element, *signature;
	WebKitDOMHTMLElement *body;
	WebKitDOMNodeList *paragraphs = NULL;
	gboolean has_paragraphs_in_body = TRUE;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	body = webkit_dom_document_get_body (document);
	e_editor_page_block_selection_changed (editor_page);

	paragraphs = webkit_dom_document_query_selector_all (document, "[data-evo-paragraph]", NULL);
	signature = webkit_dom_document_query_selector (document, ".-x-evo-signature-wrapper", NULL);
	/* Situation when wrapped paragraph is just in signature and not in message body */
	if (webkit_dom_node_list_get_length (paragraphs) == 1) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (paragraphs, 0);

		if (signature && webkit_dom_element_query_selector (signature, "[data-evo-paragraph]", NULL))
			has_paragraphs_in_body = FALSE;

		/* Don't take the credentials into account. */
		if (!webkit_dom_node_get_previous_sibling (node) &&
		    !element_has_id (WEBKIT_DOM_ELEMENT (node), "-x-evo-input-start"))
			has_paragraphs_in_body = FALSE;
	}

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

		spacer = prepare_top_signature_spacer (editor_page);
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			WEBKIT_DOM_NODE (spacer),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (signature)),
			NULL);
	}

	if (webkit_dom_node_list_get_length (paragraphs) == 0)
		has_paragraphs_in_body = FALSE;

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-input-start");
	if (!signature) {
		if (start_bottom) {
			if (!element) {
				element = e_editor_dom_prepare_paragraph (editor_page, FALSE);
				webkit_dom_element_set_id (element, "-x-evo-input-start");
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
		element = e_editor_dom_prepare_paragraph (editor_page, FALSE);
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
			element = e_editor_dom_prepare_paragraph (editor_page, FALSE);
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

 move_caret:
	if (element) {
		WebKitDOMDOMSelection *dom_selection = NULL;
		WebKitDOMDOMWindow *dom_window = NULL;
		WebKitDOMRange *range = NULL;

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
	}

	if (start_bottom)
		e_editor_dom_scroll_to_caret (editor_page);

	g_clear_object (&paragraphs);

	e_editor_dom_force_spell_check_in_viewport (editor_page);
	e_editor_page_unblock_selection_changed (editor_page);
}

gchar *
e_composer_dom_insert_signature (EEditorPage *editor_page,
                                 const gchar *content,
                                 gboolean is_html,
                                 const gchar *id,
                                 gboolean *set_signature_from_message,
                                 gboolean *check_if_signature_is_changed,
				 gboolean *ignore_next_signature_change,
				 gint16 in_start_at_bottom,
				 gint16 in_top_signature)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *signature_to_insert;
	WebKitDOMElement *insert_signature_in = NULL;
	WebKitDOMElement *signature_wrapper = NULL;
	WebKitDOMElement *element, *converted_signature = NULL;
	WebKitDOMHTMLElement *body;
	WebKitDOMHTMLCollection *signatures = NULL;
	gchar *new_signature_id = NULL;
	gchar *signature_text = NULL;
	gboolean top_signature, html_mode;
	gulong ii;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);
	g_return_val_if_fail (set_signature_from_message != NULL, NULL);
	g_return_val_if_fail (check_if_signature_is_changed != NULL, NULL);
	g_return_val_if_fail (ignore_next_signature_change != NULL, NULL);

	document = e_editor_page_get_document (editor_page);
	body = webkit_dom_document_get_body (document);

	/* "Edit as New Message" sets is_message_from_edit_as_new.
	 * Always put the signature at the bottom for that case. */
	if (e_editor_page_get_allow_top_signature (editor_page)) {
		if (in_top_signature == 0 || in_top_signature == 1)
			top_signature = in_top_signature == 1;
		else
			top_signature = use_top_signature ();
	} else {
		top_signature = FALSE;
	}

	html_mode = e_editor_page_get_html_mode (editor_page);

	/* Create the DOM signature that is the same across all types of signatures. */
	signature_to_insert = webkit_dom_document_create_element (document, "span", NULL);
	webkit_dom_element_set_class_name (signature_to_insert, "-x-evo-signature");
	/* The combo box active ID is the signature's ESource UID. */
	webkit_dom_element_set_id (signature_to_insert, id);
	insert_signature_in = signature_to_insert;

	/* The signature has no content usually it means it is set to None. */
	if (!(content && *content))
		goto insert;

	if (!is_html) {
		gchar *html;

		html = camel_text_to_html (content, 0, 0);
		if (html) {
			signature_text = html;
		} else
			signature_text = g_strdup (content);

		insert_signature_in = webkit_dom_document_create_element (document, "pre", NULL);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (signature_to_insert),
			WEBKIT_DOM_NODE (insert_signature_in),
			NULL);
	} else
		signature_text = g_strdup (content);

	/* If inserting HTML signature in the plain text composer we have to convert it. */
	if (is_html && !html_mode && !strstr (signature_text, "data-evo-signature-plain-text-mode")) {
		gchar *inner_text;

		/* Save the converted signature to avoid parsing it later again
		 * while inserting it into the view. */
		converted_signature = webkit_dom_document_create_element (document, "pre", NULL);
		webkit_dom_element_set_inner_html (converted_signature, signature_text, NULL);
		e_editor_dom_convert_element_from_html_to_plain_text (editor_page, converted_signature);
		inner_text = webkit_dom_html_element_get_inner_text (WEBKIT_DOM_HTML_ELEMENT (converted_signature));

		g_free (signature_text);
		signature_text = inner_text ? g_strstrip (inner_text) : g_strdup ("");
		/* because of the -- \n check */
		is_html = FALSE;
	}

	/* The signature dash convention ("-- \n") is specified
	 * in the "Son of RFC 1036", section 4.3.2.
	 * http://www.chemie.fu-berlin.de/outerspace/netnews/son-of-1036.html
	 */
	if (add_signature_delimiter ()) {
		const gchar *delim;
		const gchar *delim_nl;

		if (is_html) {
			delim = "-- <BR>";
			delim_nl = "\n-- <BR>";
		} else {
			delim = "-- \n";
			delim_nl = "\n-- \n";
		}

		/* Skip the delimiter if the signature already has one. */
		if (g_ascii_strncasecmp (signature_text, delim, strlen (delim)) == 0)
			;  /* skip */
		else if (e_util_strstrcase (signature_text, delim_nl) != NULL)
			;  /* skip */
		else {
			WebKitDOMElement *pre_delimiter;

			pre_delimiter = webkit_dom_document_create_element (document, "pre", NULL);
                       /* Always use the HTML delimiter as we are never in anything
                        * like a strict plain text mode. */
			webkit_dom_element_set_inner_html (pre_delimiter, "-- <br>", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (insert_signature_in),
				WEBKIT_DOM_NODE (pre_delimiter),
				NULL);
		}
	}

	if (converted_signature) {
		WebKitDOMNode *node;

		while ((node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (converted_signature))))
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (insert_signature_in), node, NULL);
		remove_node (WEBKIT_DOM_NODE (converted_signature));
	} else
		webkit_dom_element_insert_adjacent_html (
			insert_signature_in,
			"beforeend",
			signature_text,
			NULL);

	element = webkit_dom_element_query_selector (
		insert_signature_in, "[data-evo-signature-plain-text-mode]", NULL);
	if (element)
		webkit_dom_element_remove_attribute (
			element, "data-evo-signature-plain-text-mode");
	g_free (signature_text);

insert:
	/* Remove the old signature and insert the new one. */
	signatures = webkit_dom_document_get_elements_by_class_name_as_html_collection (
		document, "-x-evo-signature-wrapper");
	for (ii = webkit_dom_html_collection_get_length (signatures); ii--;) {
		WebKitDOMNode *wrapper, *signature;

		wrapper = webkit_dom_html_collection_item (signatures, ii);
		signature = webkit_dom_node_get_first_child (wrapper);

		/* Old messages will have the signature id in the name attribute, correct it. */
		element_rename_attribute (WEBKIT_DOM_ELEMENT (signature), "name", "id");

		/* When we are editing a message with signature, we need to unset the
		 * active signature id as if the signature in the message was edited
		 * by the user we would discard these changes. */
		if (*set_signature_from_message && content) {
			if (*check_if_signature_is_changed) {
				/* Normalize the signature that we want to insert as the one in the
				 * message already is normalized. */
				webkit_dom_node_normalize (WEBKIT_DOM_NODE (signature_to_insert));
				if (!webkit_dom_node_is_equal_node (WEBKIT_DOM_NODE (signature_to_insert), signature)) {
					/* Signature in the body is different than the one with the
					 * same id, so set the active signature to None and leave
					 * the signature that is in the body. */
					new_signature_id = g_strdup ("none");
					*ignore_next_signature_change = TRUE;
				}

				*check_if_signature_is_changed = FALSE;
				*set_signature_from_message = FALSE;
			} else {
				/* Load the signature and check if is it the same
				 * as the signature in body or the user previously
				 * changed it. */
				new_signature_id = webkit_dom_element_get_id (WEBKIT_DOM_ELEMENT (signature));
				*check_if_signature_is_changed = TRUE;
			}
			g_clear_object (&signatures);

			return new_signature_id;
		}

		/* If the top signature was set we have to remove the newline
		 * that was inserted after it */
		if (top_signature) {
			WebKitDOMElement *spacer;

			spacer = webkit_dom_document_query_selector (
				document, ".-x-evo-top-signature-spacer", NULL);
			if (spacer)
				remove_node_if_empty (WEBKIT_DOM_NODE (spacer));
		}

		/* Leave just one signature wrapper there as it will be reused. */
		if (ii != 0) {
			remove_node (wrapper);
		} else {
			remove_node (signature);
			signature_wrapper = WEBKIT_DOM_ELEMENT (wrapper);
		}
	}

	if (signature_wrapper) {
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (signature_wrapper),
			WEBKIT_DOM_NODE (signature_to_insert),
			NULL);

		/* Insert a spacer below the top signature */
		if (top_signature && content) {
			WebKitDOMElement *spacer;

			spacer = prepare_top_signature_spacer (editor_page);
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (body),
				WEBKIT_DOM_NODE (spacer),
				webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (signature_wrapper)),
				NULL);
		}
	} else {
		gboolean start_at_bottom;

		signature_wrapper = webkit_dom_document_create_element (document, "div", NULL);
		webkit_dom_element_set_class_name (signature_wrapper, "-x-evo-signature-wrapper");

		if (in_start_at_bottom == 0 || in_start_at_bottom == 1)
			start_at_bottom = in_start_at_bottom == 1;
		else
			start_at_bottom = start_typing_at_bottom ();

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (signature_wrapper),
			WEBKIT_DOM_NODE (signature_to_insert),
			NULL);

		if (top_signature) {
			WebKitDOMNode *child;

			child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

			if (start_at_bottom) {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (signature_wrapper),
					child,
					NULL);
			} else {
				/* When we are using signature on top the caret
				 * should be before the signature */
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (signature_wrapper),
					child,
					NULL);
			}
		} else {
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (body),
				WEBKIT_DOM_NODE (signature_wrapper),
				NULL);
		}

		move_caret_after_signature_inserted (editor_page, start_at_bottom, top_signature);
	}
	g_clear_object (&signatures);

	if (is_html && html_mode)
		e_editor_dom_fix_file_uri_images (editor_page);

	/* Make sure the flag will be unset and won't influence user's choice */
	*set_signature_from_message = FALSE;

	return NULL;
}

gchar *
e_composer_dom_get_active_signature_uid (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	gchar *uid = NULL;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);

	if ((element = webkit_dom_document_query_selector (document, ".-x-evo-signature[id]", NULL)))
		uid = webkit_dom_element_get_id (element);

	return uid;
}

gchar *
e_composer_dom_get_raw_body_content_without_signature (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list = NULL;
	GString* content;
	gulong ii, length;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);

	content = g_string_new (NULL);

	list = webkit_dom_document_query_selector_all (
		document, "body > *:not(.-x-evo-signature-wrapper)", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		if (!WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (node)) {
			gchar *text;

			text = webkit_dom_html_element_get_inner_text (WEBKIT_DOM_HTML_ELEMENT (node));
			g_string_append (content, text);
			g_free (text);

			if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (node))
				g_string_append (content, "\n");
			else
				g_string_append (content, " ");
		}
	}
	g_clear_object (&list);

	return g_string_free (content, FALSE);
}

gchar *
e_composer_dom_get_raw_body_content (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);

	body = webkit_dom_document_get_body (document);

	return  webkit_dom_html_element_get_inner_text (body);
}
