/*
 * e-editor-widget.c
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-editor-widget.h"
#include "e-editor.h"
#include "e-emoticon-chooser.h"

#include <e-util/e-util.h>
#include <e-util/e-marshal.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#define E_EDITOR_WIDGET_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_EDITOR_WIDGET, EEditorWidgetPrivate))

#define UNICODE_ZERO_WIDTH_SPACE "\xe2\x80\x8b"

#define URL_PATTERN \
	"((([A-Za-z]{3,9}:(?:\\/\\/)?)(?:[\\-;:&=\\+\\$,\\w]+@)?" \
	"[A-Za-z0-9\\.\\-]+|(?:www\\.|[\\-;:&=\\+\\$,\\w]+@)" \
	"[A-Za-z0-9\\.\\-]+)((?:\\/[\\+~%\\/\\.\\w\\-]*)?\\?" \
	"?(?:[\\-\\+=&;%@\\.\\w]*)#?(?:[\\.\\!\\/\\\\w]*))?)"

#define URL_PATTERN_SPACE URL_PATTERN "\\s"

/**
 * EEditorWidget:
 *
 * The #EEditorWidget is a WebKit-based rich text editor. The widget itself
 * only provides means to configure global behavior of the editor. To work
 * with the actual content, current cursor position or current selection,
 * use #EEditorSelection object.
 */

struct _EEditorWidgetPrivate {
	gint changed		: 1;
	gint inline_spelling	: 1;
	gint magic_links	: 1;
	gint magic_smileys	: 1;
	gint can_copy		: 1;
	gint can_cut		: 1;
	gint can_paste		: 1;
	gint can_redo		: 1;
	gint can_undo		: 1;
	gint reload_in_progress : 1;
	gint html_mode		: 1;

	EEditorSelection *selection;

	WebKitDOMElement *element_under_mouse;

	GHashTable *inline_images;

	GSettings *font_settings;
	GSettings *aliasing_settings;

	GQueue *postreload_operations;

	WebKitWebView *convertor_web_view;
};

enum {
	PROP_0,
	PROP_CAN_COPY,
	PROP_CAN_CUT,
	PROP_CAN_PASTE,
	PROP_CAN_REDO,
	PROP_CAN_UNDO,
	PROP_CHANGED,
	PROP_HTML_MODE,
	PROP_INLINE_SPELLING,
	PROP_MAGIC_LINKS,
	PROP_MAGIC_SMILEYS,
	PROP_SPELL_CHECKER
};

enum {
	POPUP_EVENT,
	LAST_SIGNAL
};

typedef void (*PostReloadOperationFunc)	(EEditorWidget *widget,
					 gpointer data);

typedef struct  {
	PostReloadOperationFunc	func;
	gpointer		data;
	GDestroyNotify		data_free_func;
} PostReloadOperation;

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (
	EEditorWidget,
	e_editor_widget,
	WEBKIT_TYPE_WEB_VIEW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static void
editor_widget_queue_postreload_operation (EEditorWidget *widget,
                                          PostReloadOperationFunc func,
                                          gpointer data,
                                          GDestroyNotify data_free_func)
{
	PostReloadOperation *op;

	g_return_if_fail (func != NULL);

	if (widget->priv->postreload_operations == NULL)
		widget->priv->postreload_operations = g_queue_new ();

	op = g_new0 (PostReloadOperation, 1);
	op->func = func;
	op->data = data;
	op->data_free_func = data_free_func;

	g_queue_push_head (widget->priv->postreload_operations, op);
}

static WebKitDOMRange *
editor_widget_get_dom_range (EEditorWidget *widget)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);

	if (webkit_dom_dom_selection_get_range_count (selection) < 1) {
		return NULL;
	}

	return webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
}

static void
editor_widget_user_changed_contents_cb (EEditorWidget *widget,
                                        gpointer user_data)
{
	WebKitWebView *web_view;
	gboolean can_redo, can_undo;

	web_view = WEBKIT_WEB_VIEW (widget);

	e_editor_widget_set_changed (widget, TRUE);

	can_redo = webkit_web_view_can_redo (web_view);
	if (widget->priv->can_redo != can_redo) {
		widget->priv->can_redo = can_redo;
		g_object_notify (G_OBJECT (widget), "can-redo");
	}

	can_undo = webkit_web_view_can_undo (web_view);
	if (widget->priv->can_undo != can_undo) {
		widget->priv->can_undo = can_undo;
		g_object_notify (G_OBJECT (widget), "can-undo");
	}
}

static void
editor_widget_selection_changed_cb (EEditorWidget *widget,
                                    gpointer user_data)
{
	WebKitWebView *web_view;
	gboolean can_copy, can_cut, can_paste;

	web_view = WEBKIT_WEB_VIEW (widget);

	/* When the webview is being (re)loaded, the document is in an
	 * inconsistant state and there is no selection, so don't propagate
	 * the signal further to EEditorSelection and others and wait until
	 * the load is finished. */
	if (widget->priv->reload_in_progress) {
		g_signal_stop_emission_by_name (widget, "selection-changed");
		return;
	}

	can_copy = webkit_web_view_can_copy_clipboard (web_view);
	if (widget->priv->can_copy != can_copy) {
		widget->priv->can_copy = can_copy;
		g_object_notify (G_OBJECT (widget), "can-copy");
	}

	can_cut = webkit_web_view_can_cut_clipboard (web_view);
	if (widget->priv->can_cut != can_cut) {
		widget->priv->can_cut = can_cut;
		g_object_notify (G_OBJECT (widget), "can-cut");
	}

	can_paste = webkit_web_view_can_paste_clipboard (web_view);
	if (widget->priv->can_paste != can_paste) {
		widget->priv->can_paste = can_paste;
		g_object_notify (G_OBJECT (widget), "can-paste");
	}
}

static void
move_caret_into_element (WebKitDOMDocument *document,
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

static gboolean
editor_widget_should_show_delete_interface_for_element (EEditorWidget *widget,
                                                        WebKitDOMHTMLElement *element)
{
	return FALSE;
}

void
e_editor_widget_force_spellcheck (EEditorWidget *widget)
{
	EEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *window;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *end_range, *actual;
	WebKitDOMText *text;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	/* Enable spellcheck in composer */
	body = webkit_dom_document_get_body (document);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "spellcheck", "true", NULL);

	selection = e_editor_widget_get_selection (widget);
	e_editor_selection_save_caret_position (selection);

	if (!webkit_dom_document_get_element_by_id (document, "-x-evo-caret-position")) {
		move_caret_into_element (
			document,
			WEBKIT_DOM_ELEMENT (webkit_dom_document_get_body (document)));
		e_editor_selection_save_caret_position (selection);
	}

	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EEditorSelection and here as well
	 * when we are moving with caret */
	g_signal_handlers_block_by_func (widget, editor_widget_selection_changed_cb, NULL);
	e_editor_selection_block_selection_changed (selection);

	/* Append some text on the end of the body */
	text = webkit_dom_document_create_text_node (document, "-x-evo-end");
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (body), WEBKIT_DOM_NODE (text), NULL);

	/* Create range that's pointing on the end of this text */
	end_range = webkit_dom_document_create_range (document);
	webkit_dom_range_select_node_contents (end_range, WEBKIT_DOM_NODE (text), NULL);
	webkit_dom_range_collapse (end_range, FALSE, NULL);

	/* Move on the beginning of the document */
	webkit_dom_dom_selection_modify (dom_selection, "move", "backward", "documentboundary");

	/* Go through all words to spellcheck them. To avoid this we have to wait for
	 * http://www.w3.org/html/wg/drafts/html/master/editing.html#dom-forcespellcheck */
	actual = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	/* We are moving forward word by word until we hit the text on the end of
	 * the body that we previously inserted there */
	while (actual && webkit_dom_range_compare_boundary_points (end_range, 2, actual, NULL) != 0) {
		webkit_dom_dom_selection_modify (dom_selection, "move", "forward", "word");
		actual = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	}

	/* Remove the text that we inserted on the end of the body */
	webkit_dom_node_remove_child (
		WEBKIT_DOM_NODE (body), WEBKIT_DOM_NODE (text), NULL);

	/* Unblock the callbacks */
	g_signal_handlers_unblock_by_func (widget, editor_widget_selection_changed_cb, NULL);
	e_editor_selection_unblock_selection_changed (selection);

	e_editor_selection_restore_caret_position (selection);
}

static void
body_input_event_cb (WebKitDOMElement *element,
                     WebKitDOMEvent *event,
                     EEditorWidget *editor_widget)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range = editor_widget_get_dom_range (editor_widget);

	/* If text before caret includes UNICODE_ZERO_WIDTH_SPACE character, remove it */
	node = webkit_dom_range_get_end_container (range, NULL);
	if (WEBKIT_DOM_IS_TEXT (node)) {
		gchar *text = webkit_dom_node_get_text_content (node);
		glong length = g_utf8_strlen (text, -1);
		/* We have to preserve empty paragraphs with just UNICODE_ZERO_WIDTH_SPACE
		 * character as when we will remove it it will collapse */
		if (length > 1 && strstr (text, UNICODE_ZERO_WIDTH_SPACE)) {
			WebKitDOMDocument *document;
			WebKitDOMDOMWindow *window;
			WebKitDOMDOMSelection *selection;
			GString *res = e_str_replace_string (text, UNICODE_ZERO_WIDTH_SPACE, "");

			webkit_dom_node_set_text_content (node, res->str, NULL);

			document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (editor_widget));
			window = webkit_dom_document_get_default_view (document);
			selection = webkit_dom_dom_window_get_selection (window);

			webkit_dom_dom_selection_modify (selection, "move", "right", "character");

			g_string_free (res, TRUE);
		}
		g_free (text);
	}
}

static void
change_images_http_src_to_evo_http (EEditorWidget *widget,
                                    gboolean to_evo_http)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list;
	gint length, ii;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	list = webkit_dom_document_query_selector_all (
		document, to_evo_http ? "img[src^=http]" : "img[src^=evo-http]", NULL);
	length = webkit_dom_node_list_get_length (list);

	for (ii = 0; ii < length; ii++) {
		WebKitDOMHTMLImageElement *img;
		gchar *src;

		img = WEBKIT_DOM_HTML_IMAGE_ELEMENT (webkit_dom_node_list_item (list, ii));
		src = webkit_dom_html_image_element_get_src (img);

		if (to_evo_http) {
			gchar *new_src = g_strconcat ("evo-http", src + 4, NULL);
			webkit_dom_html_image_element_set_src (img, new_src);
			g_free (new_src);
		} else
			webkit_dom_html_image_element_set_src (img, src + 4);

		g_free (src);
	}
}

static void
set_base64_to_element_attribute (EEditorWidget *widget,
                                 WebKitDOMElement *element,
                                 const gchar *attribute)
{
	gchar *attribute_value;
	const gchar *base64_src;

	attribute_value = webkit_dom_element_get_attribute (element, attribute);

	if ((base64_src = g_hash_table_lookup (widget->priv->inline_images, attribute_value)) != NULL) {
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
}

static void
change_cid_images_src_to_base64 (EEditorWidget *widget)
{
	gint ii, length;
	WebKitDOMDocument *document;
	WebKitDOMElement *document_element;
	WebKitDOMNamedNodeMap *attributes;
	WebKitDOMNodeList *list;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	document_element = webkit_dom_document_get_document_element (document);

	list = webkit_dom_document_query_selector_all (document, "img[src^=\"cid:\"]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		set_base64_to_element_attribute (widget, WEBKIT_DOM_ELEMENT (node), "src");
	}

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
					widget, WEBKIT_DOM_ELEMENT (node), attribute_ns);
			}

			g_free (attribute_ns);
			g_free (selector);
		}
		g_free (name);
	}

	list = webkit_dom_document_query_selector_all (
		document, "[background^=\"cid:\"]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		set_base64_to_element_attribute (
			widget, WEBKIT_DOM_ELEMENT (node), "background");
	}
	g_hash_table_remove_all (widget->priv->inline_images);
}

/* For purpose of this function see e-mail-formatter-quote.c */
static void
put_body_in_citation (WebKitDOMDocument *document)
{
	WebKitDOMElement *cite_body = webkit_dom_document_query_selector (
		document, "span.-x-evo-cite-body", NULL);

	if (cite_body) {
		WebKitDOMHTMLElement *body = webkit_dom_document_get_body (document);
		gchar *inner_html, *with_citation;

		webkit_dom_node_remove_child (
			WEBKIT_DOM_NODE (body),
			WEBKIT_DOM_NODE (cite_body),
			NULL);

		inner_html = webkit_dom_html_element_get_inner_html (body);
		with_citation = g_strconcat (
			"<blockquote type=\"cite\" id=\"-x-evo-main-cite\">",
			inner_html, "</span>", NULL);
		webkit_dom_html_element_set_inner_html (body, with_citation, NULL);
		g_free (inner_html);
		g_free (with_citation);
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
		document, "span.-x-evo-to-body", NULL);
	for (ii = webkit_dom_node_list_get_length (list) - 1; ii >= 0; ii--) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		while (webkit_dom_node_has_child_nodes (node)) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (body),
				webkit_dom_node_get_first_child (node),
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (body)),
				NULL);
		}

		webkit_dom_node_remove_child (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (node),
			NULL);
	}
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
}

static void
editor_widget_load_status_changed (EEditorWidget *widget)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitLoadStatus status;

	status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (widget));
	if (status != WEBKIT_LOAD_FINISHED)
		return;

	widget->priv->reload_in_progress = FALSE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	body = webkit_dom_document_get_body (document);

	webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (body), "style");
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-message", "", NULL);

	put_body_in_citation (document);
	move_elements_to_body (document);
	repair_gmail_blockquotes (document);

	/* Register on input event that is called when the content (body) is modified */
	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"input",
		G_CALLBACK (body_input_event_cb),
		FALSE,
		widget);

	if (widget->priv->html_mode) {
		change_images_http_src_to_evo_http (widget, TRUE);
		change_cid_images_src_to_base64 (widget);
	}

	/* Dispatch queued operations */
	while (widget->priv->postreload_operations &&
	       !g_queue_is_empty (widget->priv->postreload_operations)) {

		PostReloadOperation *op;

		op = g_queue_pop_head (widget->priv->postreload_operations);

		op->func (widget, op->data);

		if (op->data_free_func)
			op->data_free_func (op->data);
		g_free (op);
	}
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

static void
editor_widget_check_magic_links (EEditorWidget *widget,
				 WebKitDOMRange *range,
				 gboolean include_space_by_user,
				 GdkEventKey *event)
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
		if ((event->keyval == GDK_KEY_Return) ||
		    (event->keyval == GDK_KEY_Linefeed) ||
		    (event->keyval == GDK_KEY_KP_Enter)) {

			return_pressed = TRUE;
		}

		if (event->keyval == GDK_KEY_space)
			include_space = TRUE;
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
		WebKitDOMDocument *document;
		WebKitDOMNode *url_text_node_clone;
		WebKitDOMText *url_text_node;
		WebKitDOMElement *anchor;
		const gchar* url_text;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

		if (!return_pressed)
			e_editor_selection_save_caret_position (e_editor_widget_get_selection (widget));

		g_match_info_fetch_pos (match_info, 0, &start_pos_url, &end_pos_url);

		/* Get start and end position of url in node's text because positions
		 * that we get from g_match_info_fetch_pos are not UTF-8 aware */
		url_end_raw = g_strndup(node_text, end_pos_url);
		url_end = g_utf8_strlen (url_end_raw, -1);

		url_length = g_utf8_strlen (urls[0], -1);
		url_start = url_end - url_length;

		webkit_dom_text_split_text (WEBKIT_DOM_TEXT (node), include_space ? url_end - 1 : url_end, NULL);

		url_text_node = webkit_dom_text_split_text (WEBKIT_DOM_TEXT (node), url_start, NULL);
		url_text_node_clone = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (url_text_node), TRUE);

		url_text = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (url_text_node_clone));

		final_url = g_strconcat (g_str_has_prefix (url_text, "www") ? "http://" : "",
					 url_text, NULL);

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
			e_editor_selection_restore_caret_position (e_editor_widget_get_selection (widget));

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
				inner_html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (parent));
				new_href = g_strconcat (protocol, inner_html,
							appending_to_link ? text_to_append : "", NULL);

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

					webkit_dom_node_remove_child (
						webkit_dom_node_get_parent_node (node),
						node, NULL);

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

				inner_html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (parent));
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

					webkit_dom_node_remove_child (
						webkit_dom_node_get_parent_node (node),
						node, NULL);

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

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	EEditorWidget *widget;
	gchar *content_type;
	gchar *name;
	EEmoticon *emoticon;
};

static LoadContext *
emoticon_load_context_new (EEditorWidget *widget,
                           EEmoticon *emoticon)
{
	LoadContext *load_context;

	load_context = g_slice_new0 (LoadContext);
	load_context->widget = widget;
	load_context->emoticon = emoticon;

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
emoticon_read_async_cb (GFile *file,
                        GAsyncResult *result,
                        LoadContext *load_context)
{
	EEditorWidget *widget = load_context->widget;
	EEmoticon *emoticon = load_context->emoticon;
	GError *error = NULL;
	gchar *html, *node_text, *mime_type;
	gchar *base64_encoded, *output, *data;
	const gchar *emoticon_start;
	GFileInputStream *input_stream;
	GOutputStream *output_stream;
	gssize size;
	WebKitDOMDocument *document;
	WebKitDOMElement *span, *caret_position;
	WebKitDOMNode *node;
	WebKitDOMNode *parent;
	WebKitDOMRange *range;

	input_stream = g_file_read_finish (file, result, &error);
	g_return_if_fail (!error && input_stream);

	output_stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

	size = g_output_stream_splice (
		output_stream, G_INPUT_STREAM (input_stream),
		G_OUTPUT_STREAM_SPLICE_NONE, NULL, &error);

	if (error || (size == -1))
		goto out;

	e_editor_selection_save_caret_position (
		e_editor_widget_get_selection (widget));

	mime_type = g_content_type_get_mime_type (load_context->content_type);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	range = editor_widget_get_dom_range (widget);
	node = webkit_dom_range_get_end_container (range, NULL);
	node_text = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (node));
	parent = webkit_dom_node_get_parent_node (node);
	span = webkit_dom_document_create_element (document, "SPAN", NULL);

	data = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output_stream));

	base64_encoded = g_base64_encode ((const guchar *) data, size);
	output = g_strconcat ("data:", mime_type, ";base64,", base64_encoded, NULL);

	/* Insert span with image representation and another one with text
	 * represetation and hide/show them dependant on active composer mode */
	/* &#8203 == UNICODE_ZERO_WIDTH_SPACE */
	html = g_strdup_printf (
		"<span class=\"-x-evo-smiley-wrapper\"><img src=\"%s\" alt=\"%s\" "
		"x-evo-smiley=\"%s\" class=\"-x-evo-smiley-img\" data-inline "
		"data-name=\"%s\"/><span class=\"-x-evo-smiley-text\" "
		"style=\"display: none;\">%s</span></span>&#8203;",
		output, emoticon ? emoticon->text_face : "", emoticon->icon_name,
		load_context->name, emoticon ? emoticon->text_face : "");

	caret_position = webkit_dom_document_get_element_by_id (
		document, "-x-evo-caret-position");
	span = WEBKIT_DOM_ELEMENT (webkit_dom_node_insert_before (
		parent,
		WEBKIT_DOM_NODE (span),
		WEBKIT_DOM_NODE (caret_position),
		NULL));

	webkit_dom_html_element_set_outer_html (
		WEBKIT_DOM_HTML_ELEMENT (span), html, NULL);

	emoticon_start = g_utf8_strrchr (
		node_text, -1, g_utf8_get_char (emoticon->text_face));
	if (emoticon_start) {
		webkit_dom_character_data_delete_data (
			WEBKIT_DOM_CHARACTER_DATA (node),
			g_utf8_strlen (node_text, -1) - strlen (emoticon_start),
			strlen (emoticon->text_face),
			NULL);
	}

	e_editor_selection_restore_caret_position (
		e_editor_widget_get_selection (widget));

	g_free (html);
	g_free (node_text);
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
e_editor_widget_insert_smiley (EEditorWidget *widget,
                               EEmoticon *emoticon)
{
	GFile *file;
	gchar *filename_uri;
	LoadContext *load_context;

	filename_uri = e_emoticon_get_uri (emoticon);
	g_return_if_fail (filename_uri != NULL);

	load_context = emoticon_load_context_new (widget, emoticon);

	file = g_file_new_for_uri (filename_uri);
	g_file_query_info_async (
		file,  "standard::*", G_FILE_QUERY_INFO_NONE,
		G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) emoticon_query_info_async_cb, load_context);

	g_free (filename_uri);
	g_object_unref (file);
}

static void
editor_widget_check_magic_smileys (EEditorWidget *widget,
                                   WebKitDOMRange *range)
{
	gint pos;
	gint state;
	gint relative;
	gint start;
	gchar *node_text;
	gunichar uc;
	WebKitDOMNode *node;

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

		emoticon = (e_emoticon_chooser_lookup_emoticon (
			emoticons_icon_names[-state - 1]));
		e_editor_widget_insert_smiley (widget, (EEmoticon *) emoticon);
	}

	g_free (node_text);
}

static void
editor_widget_set_links_active (EEditorWidget *widget,
                                gboolean active)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *style;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

	if (active) {
		style = webkit_dom_document_get_element_by_id (
				document, "--evolution-editor-style-a");
		if (style) {
			webkit_dom_node_remove_child (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (style)),
				WEBKIT_DOM_NODE (style), NULL);
		}
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
clipboard_text_received (GtkClipboard *clipboard,
                         const gchar *text,
                         EEditorWidget *widget)
{
	EEditorSelection *selection;
	gchar *escaped_text;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMElement *blockquote, *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	if (!text || !*text)
		return;

	selection = e_editor_widget_get_selection (widget);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

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
		e_editor_selection_get_caret_position_node (document),
		NULL);

	blockquote = webkit_dom_document_create_element (document, "blockquote", NULL);
	webkit_dom_element_set_attribute (blockquote, "type", "cite", NULL);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (blockquote), WEBKIT_DOM_NODE (element), NULL);

	if (!e_editor_widget_get_html_mode (widget))
		e_editor_widget_quote_plain_text_element (widget, element);

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	node = webkit_dom_range_get_end_container (range, NULL);

	webkit_dom_node_append_child (
		webkit_dom_node_get_parent_node (node),
		WEBKIT_DOM_NODE (blockquote),
		NULL);

	e_editor_selection_restore_caret_position (selection);

	g_free (escaped_text);
}

static void
editor_widget_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHANGED:
			e_editor_widget_set_changed (
				E_EDITOR_WIDGET (object),
				g_value_get_boolean (value));
			return;

		case PROP_HTML_MODE:
			e_editor_widget_set_html_mode (
				E_EDITOR_WIDGET (object),
				g_value_get_boolean (value));
			return;

		case PROP_INLINE_SPELLING:
			e_editor_widget_set_inline_spelling (
				E_EDITOR_WIDGET (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_LINKS:
			e_editor_widget_set_magic_links (
				E_EDITOR_WIDGET (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_SMILEYS:
			e_editor_widget_set_magic_smileys (
				E_EDITOR_WIDGET (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_widget_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_COPY:
			g_value_set_boolean (
				value, webkit_web_view_can_copy_clipboard (
				WEBKIT_WEB_VIEW (object)));
			return;

		case PROP_CAN_CUT:
			g_value_set_boolean (
				value, webkit_web_view_can_cut_clipboard (
				WEBKIT_WEB_VIEW (object)));
			return;

		case PROP_CAN_PASTE:
			g_value_set_boolean (
				value, webkit_web_view_can_paste_clipboard (
				WEBKIT_WEB_VIEW (object)));
			return;

		case PROP_CAN_REDO:
			g_value_set_boolean (
				value, webkit_web_view_can_redo (
				WEBKIT_WEB_VIEW (object)));
			return;

		case PROP_CAN_UNDO:
			g_value_set_boolean (
				value, webkit_web_view_can_undo (
				WEBKIT_WEB_VIEW (object)));
			return;

		case PROP_CHANGED:
			g_value_set_boolean (
				value, e_editor_widget_get_changed (
				E_EDITOR_WIDGET (object)));
			return;

		case PROP_HTML_MODE:
			g_value_set_boolean (
				value, e_editor_widget_get_html_mode (
				E_EDITOR_WIDGET (object)));
			return;

		case PROP_INLINE_SPELLING:
			g_value_set_boolean (
				value, e_editor_widget_get_inline_spelling (
				E_EDITOR_WIDGET (object)));
			return;

		case PROP_MAGIC_LINKS:
			g_value_set_boolean (
				value, e_editor_widget_get_magic_links (
				E_EDITOR_WIDGET (object)));
			return;

		case PROP_MAGIC_SMILEYS:
			g_value_set_boolean (
				value, e_editor_widget_get_magic_smileys (
				E_EDITOR_WIDGET (object)));
			return;

		case PROP_SPELL_CHECKER:
			g_value_set_object (
				value, e_editor_widget_get_spell_checker (
				E_EDITOR_WIDGET (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_widget_dispose (GObject *object)
{
	EEditorWidgetPrivate *priv;

	priv = E_EDITOR_WIDGET_GET_PRIVATE (object);

	g_clear_object (&priv->selection);

	if (priv->convertor_web_view != NULL) {
		g_object_unref (priv->convertor_web_view);
		priv->convertor_web_view = NULL;
	}

	if (priv->aliasing_settings != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->aliasing_settings, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->aliasing_settings);
		priv->aliasing_settings = NULL;
	}

	if (priv->font_settings != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->font_settings, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->font_settings);
		priv->font_settings = NULL;
	}

	g_hash_table_remove_all (priv->inline_images);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_editor_widget_parent_class)->dispose (object);
}

static void
editor_widget_finalize (GObject *object)
{
	EEditorWidgetPrivate *priv;

	priv = E_EDITOR_WIDGET_GET_PRIVATE (object);

	g_hash_table_destroy (priv->inline_images);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_editor_widget_parent_class)->finalize (object);
}

static void
editor_widget_constructed (GObject *object)
{
	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_editor_widget_parent_class)->constructed (object);
}

static void
editor_widget_save_element_under_mouse_click (GtkWidget *widget)
{
	gint x, y;
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;
	EEditorWidget *editor_widget;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	device_manager = gdk_display_get_device_manager (gtk_widget_get_display (GTK_WIDGET (widget)));
	pointer = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (gtk_widget_get_window (GTK_WIDGET (widget)), pointer, &x, &y, NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	element = webkit_dom_document_element_from_point (document, x, y);

	editor_widget = E_EDITOR_WIDGET (widget);
	editor_widget->priv->element_under_mouse = element;
}

static gboolean
editor_widget_button_press_event (GtkWidget *widget,
                                  GdkEventButton *event)
{
	gboolean event_handled;

	if (event->button != 3) {
		event_handled = FALSE;
	} else {
		editor_widget_save_element_under_mouse_click (widget);
		g_signal_emit (
			widget, signals[POPUP_EVENT],
			0, event, &event_handled);
	}

	if (event_handled)
		return TRUE;

	/* Chain up to parent's button_press_event() method. */
	return GTK_WIDGET_CLASS (e_editor_widget_parent_class)->
		button_press_event (widget, event);
}

static gboolean
editor_widget_button_release_event (GtkWidget *widget,
                                    GdkEventButton *event)
{
	WebKitWebView *webview;
	WebKitHitTestResult *hit_test;
	WebKitHitTestResultContext context;
	gchar *uri;

	webview = WEBKIT_WEB_VIEW (widget);
	hit_test = webkit_web_view_get_hit_test_result (webview, event);

	g_object_get (
		hit_test,
		"context", &context,
		"link-uri", &uri,
		NULL);

	g_object_unref (hit_test);

	/* Left click on a link */
	if ((context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) &&
	    (event->button == 1)) {

		/* Ctrl + Left Click on link opens it, otherwise ignore the
		 * click completely */
		if (event->state & GDK_CONTROL_MASK) {
			GtkWidget *toplevel;
			GdkScreen *screen;

			toplevel = gtk_widget_get_toplevel (widget);
			screen = gtk_window_get_screen (GTK_WINDOW (toplevel));
			gtk_show_uri (screen, uri, event->time, NULL);
			g_free (uri);
		}

		return TRUE;
	}

	g_free (uri);

	/* Chain up to parent's button_release_event() method. */
	return GTK_WIDGET_CLASS (e_editor_widget_parent_class)->
		button_release_event (widget, event);
}

static gboolean
editor_widget_key_press_event (GtkWidget *widget,
                               GdkEventKey *event)
{
	EEditorWidget *editor = E_EDITOR_WIDGET (widget);

	editor->priv->changed = FALSE;

	if (event->keyval == GDK_KEY_Tab)
		return e_editor_widget_exec_command (
			editor,
			E_EDITOR_WIDGET_COMMAND_INSERT_TEXT,
			"\t");

	if ((event->keyval == GDK_KEY_Control_L) ||
	    (event->keyval == GDK_KEY_Control_R)) {

		editor_widget_set_links_active (editor, TRUE);
	}

	if ((event->keyval == GDK_KEY_Return) ||
	    (event->keyval == GDK_KEY_KP_Enter)) {

		/* When user presses ENTER in a citation block, WebKit does
		 * not break the citation automatically, so we need to use
		 * the special command to do it. */
		EEditorSelection *selection;

		selection = e_editor_widget_get_selection (editor);
		if (e_editor_selection_is_citation (selection)) {
			return e_editor_widget_exec_command (
				editor,
				E_EDITOR_WIDGET_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT,
				NULL);
		}
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_editor_widget_parent_class)->
		key_press_event (widget, event);
}

static void
adjust_html_structure_after_ending_list (EEditorSelection *selection,
                                         WebKitDOMDocument *document,
                                         WebKitDOMNode *node)
{
	WebKitDOMNode *prev_sibling;
	WebKitDOMNode *parent;
	WebKitDOMElement *paragraph;

	/* When pressing Enter on empty line in the list WebKit will end that
	 * list and inserts <div><br></div> after it and sets the caret after
	 * the BR element. But that's not what we want, so we will remove the DIV
	 * and BR elements and append new paragraph with caret inside it after
	 * the paragraph that includes the previously inserted list. */
	prev_sibling = webkit_dom_node_get_previous_sibling (node);
	if (!(WEBKIT_DOM_IS_HTMLLI_ELEMENT (prev_sibling) ||
	    WEBKIT_DOM_IS_HTMLO_LIST_ELEMENT (prev_sibling) ||
	    WEBKIT_DOM_IS_HTMLU_LIST_ELEMENT (prev_sibling)))
		return;

	if (!(WEBKIT_DOM_IS_HTML_DIV_ELEMENT (node) &&
	    !element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-paragraph")))
		return;

	parent = webkit_dom_node_get_parent_node (prev_sibling);
	paragraph = e_editor_selection_get_paragraph_element (
		selection, document, -1, 0),

	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (paragraph), UNICODE_ZERO_WIDTH_SPACE, NULL);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (paragraph),
		e_editor_selection_get_caret_position_node (document),
		NULL);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (parent),
		WEBKIT_DOM_NODE (paragraph),
		webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent)),
		NULL);

	webkit_dom_node_remove_child (
		parent, node, NULL);

	e_editor_selection_restore_caret_position (selection);
}

static gboolean
surround_text_with_paragraph_if_needed (EEditorSelection *selection,
                                        WebKitDOMDocument *document,
                                        WebKitDOMNode *node)
{
	WebKitDOMNode *next_sibling = webkit_dom_node_get_next_sibling (node);

	/* All text in composer has to be written in div elements, so if
	 * we are writing something straight to the body, surround it with
	 * paragraph */
	if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling) && WEBKIT_DOM_IS_TEXT (node) &&
	    WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (node))) {
		e_editor_selection_put_node_into_paragraph (
			selection,
			document,
			node,
			e_editor_selection_get_caret_position_node (document));

		webkit_dom_node_remove_child (
			webkit_dom_node_get_parent_node (next_sibling),
			next_sibling,
			NULL);

		return TRUE;
	}

	return FALSE;
}

static gboolean
editor_widget_key_release_event (GtkWidget *widget,
                                 GdkEventKey *event)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	EEditorWidget *editor_widget;
	EEditorSelection *selection;

	editor_widget = E_EDITOR_WIDGET (widget);
	range = editor_widget_get_dom_range (editor_widget);
	selection = e_editor_widget_get_selection (editor_widget);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

	if (editor_widget->priv->magic_smileys &&
	    editor_widget->priv->html_mode) {
		editor_widget_check_magic_smileys (editor_widget, range);
	}

	if ((event->keyval == GDK_KEY_Return) ||
	    (event->keyval == GDK_KEY_Linefeed) ||
	    (event->keyval == GDK_KEY_KP_Enter) ||
	    (event->keyval == GDK_KEY_space)) {

		editor_widget_check_magic_links (editor_widget, range, FALSE, event);
		adjust_html_structure_after_ending_list (
			selection,
			document,
			webkit_dom_range_get_end_container (range, NULL));
	} else {
		WebKitDOMNode *node;

		node = webkit_dom_range_get_end_container (range, NULL);

		if (surround_text_with_paragraph_if_needed (selection, document, node)) {
			e_editor_selection_restore_caret_position (selection);
			node = webkit_dom_range_get_end_container (range, NULL);
			range = editor_widget_get_dom_range (editor_widget);
		}

		if (WEBKIT_DOM_IS_TEXT (node)) {
			gchar *text;

			text = webkit_dom_node_get_text_content (node);

			if (g_strcmp0 (text, "") != 0 && !g_unichar_isspace (g_utf8_get_char (text))) {
				WebKitDOMNode *prev_sibling;

				prev_sibling = webkit_dom_node_get_previous_sibling (node);

				if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling))
					editor_widget_check_magic_links (editor_widget, range, FALSE, event);
			}
			g_free (text);
		}
	}

	if ((event->keyval == GDK_KEY_Control_L) ||
	    (event->keyval == GDK_KEY_Control_R)) {

		editor_widget_set_links_active (editor_widget, FALSE);
	}

	/* Chain up to parent's key_release_event() method. */
	return GTK_WIDGET_CLASS (e_editor_widget_parent_class)->
		key_release_event (widget, event);
}

static void
editor_widget_paste_clipboard_quoted (EEditorWidget *widget)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get_for_display (
		gdk_display_get_default (),
		GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_text (
		clipboard,
		(GtkClipboardTextReceivedFunc) clipboard_text_received,
		widget);
}

static void
e_editor_widget_class_init (EEditorWidgetClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EEditorWidgetPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = editor_widget_get_property;
	object_class->set_property = editor_widget_set_property;
	object_class->dispose = editor_widget_dispose;
	object_class->finalize = editor_widget_finalize;
	object_class->constructed = editor_widget_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = editor_widget_button_press_event;
	widget_class->button_release_event = editor_widget_button_release_event;
	widget_class->key_press_event = editor_widget_key_press_event;
	widget_class->key_release_event = editor_widget_key_release_event;

	class->paste_clipboard_quoted = editor_widget_paste_clipboard_quoted;

	/**
	 * EEditorWidget:can-copy
	 *
	 * Determines whether it's possible to copy to clipboard. The action
	 * is usually disabled when there is no selection to copy.
	 */
	g_object_class_install_property (
		object_class,
		PROP_CAN_COPY,
		g_param_spec_boolean (
			"can-copy",
			"Can Copy",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EEditorWidget:can-cut
	 *
	 * Determines whether it's possible to cut to clipboard. The action
	 * is usually disabled when there is no selection to cut.
	 */
	g_object_class_install_property (
		object_class,
		PROP_CAN_CUT,
		g_param_spec_boolean (
			"can-cut",
			"Can Cut",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EEditorWidget:can-paste
	 *
	 * Determines whether it's possible to paste from clipboard. The action
	 * is usually disabled when there is no valid content in clipboard to
	 * paste.
	 */
	g_object_class_install_property (
		object_class,
		PROP_CAN_PASTE,
		g_param_spec_boolean (
			"can-paste",
			"Can Paste",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EEditorWidget:can-redo
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
	 * EEditorWidget:can-undo
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

	/**
	 * EEditorWidget:changed
	 *
	 * Determines whether document has been modified
	 */
	g_object_class_install_property (
		object_class,
		PROP_CHANGED,
		g_param_spec_boolean (
			"changed",
			_("Changed property"),
			_("Whether editor changed"),
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EEditorWidget:html-mode
	 *
	 * Determines whether HTML or plain text mode is enabled.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_HTML_MODE,
		g_param_spec_boolean (
			"html-mode",
			"HTML Mode",
			"Edit HTML or plain text",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EEditorWidget::inline-spelling
	 *
	 * Determines whether automatic spellchecking is enabled.
	 */
	g_object_class_install_property (
		object_class,
		PROP_INLINE_SPELLING,
		g_param_spec_boolean (
			"inline-spelling",
			"Inline Spelling",
			"Check your spelling as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EEditorWidget:magic-links
	 *
	 * Determines whether automatic conversion of text links into
	 * HTML links is enabled.
	 */
	g_object_class_install_property (
		object_class,
		PROP_MAGIC_LINKS,
		g_param_spec_boolean (
			"magic-links",
			"Magic Links",
			"Make URIs clickable as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EEditorWidget:magic-smileys
	 *
	 * Determines whether automatic conversion of text smileys into
	 * images is enabled.
	 */
	g_object_class_install_property (
		object_class,
		PROP_MAGIC_SMILEYS,
		g_param_spec_boolean (
			"magic-smileys",
			"Magic Smileys",
			"Convert emoticons to images as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EEditorWidget:spell-checker:
	 *
	 * The #ESpellChecker used for spell checking.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SPELL_CHECKER,
		g_param_spec_object (
			"spell-checker",
			"Spell Checker",
			"The spell checker",
			E_TYPE_SPELL_CHECKER,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EEditorWidget:popup-event
	 *
	 * Emitted whenever a context menu is requested.
	 */
	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EEditorWidgetClass, popup_event),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__BOXED,
		G_TYPE_BOOLEAN, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

/* This parses the HTML code (that contains just text, &nbsp; and BR elements)
 * into paragraphs.
 * HTML code in that format we can get by taking innerText from some element,
 * setting it to another one and finally getting innerHTML from it */
static void
parse_html_into_paragraphs (EEditorWidget *widget,
                            WebKitDOMDocument *document,
                            WebKitDOMElement *blockquote,
                            const gchar *html,
                            gboolean use_pre)
{
	const gchar *prev_br, *next_br;
	gchar *inner_html;
	gint citation_level = 0;
	GString *start, *end;

	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (blockquote), "", NULL);

	prev_br = html;
	next_br = strstr (prev_br, "<br>");

	while (next_br) {
		gboolean with_br = FALSE;
		const gchar *citation = NULL, *citation_end = NULL, *rest = NULL;
		gchar *to_insert = NULL;
		WebKitDOMElement *paragraph;

		to_insert = g_utf8_substring (
			prev_br, 0, g_utf8_pointer_to_offset (prev_br, next_br));

		with_br = g_str_has_prefix (to_insert, "<br>");

		citation = strstr (to_insert, "##CITATION_");
		if (citation) {
			if (strstr (to_insert, "##CITATION_START##"))
				citation_level++;
			else
				citation_level--;

			citation_end = strstr (citation + 2, "##");
			if (citation_end)
				rest = citation_end + 2;
		} else {
			rest = with_br ? to_insert + 4 : to_insert;
		}

		if (use_pre || citation_level > 0) {
			paragraph = webkit_dom_document_create_element (
				document, "pre", NULL);
		} else {
			paragraph = e_editor_selection_get_paragraph_element (
				e_editor_widget_get_selection (widget),
				document, -1, citation_level);
		}

		if (with_br) {
			WebKitDOMNode *paragraph_clone;

			paragraph_clone = webkit_dom_node_clone_node (
				WEBKIT_DOM_NODE (paragraph), TRUE);

			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (paragraph_clone),
				"&nbsp;",
				NULL);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (blockquote),
				paragraph_clone,
				NULL);
		}

		if (citation) {
			WebKitDOMText *text;
			gchar *citation_mark;

			citation_mark = g_utf8_substring (
				citation, 0,
				g_utf8_pointer_to_offset (
					citation, citation_end + 2));

			text = webkit_dom_document_create_text_node (
				document, citation_mark);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (blockquote),
				WEBKIT_DOM_NODE (text),
				NULL);

			g_free (citation_mark);
		}

		if (rest) {
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (paragraph),
				rest, NULL);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (blockquote),
				WEBKIT_DOM_NODE (paragraph),
				NULL);
		}

		prev_br = next_br + 4;
		next_br = strstr (prev_br + 4, "<br>");
		g_free (to_insert);
	}

	if (g_utf8_strlen (prev_br, -1) > 0) {
		WebKitDOMElement *paragraph;

		if (use_pre || citation_level > 0) {
			paragraph = webkit_dom_document_create_element (
				document, "pre", NULL);
		} else {
			paragraph = e_editor_selection_get_paragraph_element (
				e_editor_widget_get_selection (widget),
				document, -1, citation_level);
		}

		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (paragraph),
			prev_br, NULL);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (blockquote),
			WEBKIT_DOM_NODE (paragraph),
			NULL);
	}

	/* Replace text markers with actual HTML blockquotes */
	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (blockquote));
	start = e_str_replace_string (
		inner_html, "##CITATION_START##","<blockquote type=\"cite\">");
	end = e_str_replace_string (
		start->str, "##CITATION_END##", "</blockquote>");
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (blockquote), end->str, NULL);

	g_free (inner_html);
	g_string_free (start, TRUE);
	g_string_free (end, TRUE);
}

static void
create_text_markers_for_citations (WebKitDOMNodeList *citations)
{
	gint ii, length = webkit_dom_node_list_get_length (citations);

	for (ii = 0; ii < length; ii++) {
		gchar *inner_html, *surrounded;
		WebKitDOMNode *node = webkit_dom_node_list_item (citations, ii);

		inner_html = webkit_dom_html_element_get_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (node));

		surrounded = g_strconcat (
			"<span>##CITATION_START##</span>", inner_html,
			"<span>##CITATION_END##</span>", NULL);

		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (node), surrounded, NULL);

		g_free (inner_html);
		g_free (surrounded);
	}
}

static void
html_plain_text_convertor_load_status_changed (WebKitWebView *web_view,
                                               GParamSpec *pspec,
                                               EEditorWidget *widget)
{
	EEditorSelection *selection = e_editor_widget_get_selection (widget);
	gboolean start_bottom;
	gchar *inner_text, *inner_html;
	gint ii;
	GSettings *settings;
	WebKitLoadStatus status;
	WebKitDOMDocument *document_convertor, *document;
	WebKitDOMElement *paragraph, *new_blockquote, *top_signature;
	WebKitDOMElement *cite_body, *signature;
	WebKitDOMHTMLElement *body, *convertor_body;
	WebKitDOMNodeList *list;

	status = webkit_web_view_get_load_status (web_view);
	if (status != WEBKIT_LOAD_FINISHED)
		return;

	document_convertor = webkit_web_view_get_dom_document (web_view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

	settings = g_settings_new ("org.gnome.evolution.mail");
	start_bottom = g_settings_get_boolean (settings, "composer-reply-start-bottom");
	g_object_unref (settings);

	body = webkit_dom_document_get_body (document);
	convertor_body = webkit_dom_document_get_body (document_convertor);

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-converted", "", NULL);

	paragraph = webkit_dom_document_get_element_by_id (document, "-x-evo-input-start");
	if (!paragraph) {
		WebKitDOMElement *element;

		element = webkit_dom_document_create_element (document, "div", NULL);
		element_add_class (element, "-x-evo-paragraph");
		webkit_dom_element_set_id (element, "-x-evo-input-start");
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (element), UNICODE_ZERO_WIDTH_SPACE, NULL);
		paragraph = WEBKIT_DOM_ELEMENT (webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (webkit_dom_document_get_body (document)),
			WEBKIT_DOM_NODE (element),
			NULL));
	}

	list = webkit_dom_document_query_selector_all (
		document_convertor, "span.-x-evo-to-body", NULL);
	for (ii = webkit_dom_node_list_get_length (list) - 1; ii >= 0; ii--) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (list, ii);
		while (webkit_dom_node_has_child_nodes (node)) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (body),
				webkit_dom_node_clone_node (
					webkit_dom_node_get_first_child (node), TRUE),
				webkit_dom_node_get_next_sibling (
					WEBKIT_DOM_NODE (paragraph)),
				NULL);

			webkit_dom_node_remove_child (
				node, webkit_dom_node_get_first_child (node), NULL);
		}

		webkit_dom_node_remove_child (
			WEBKIT_DOM_NODE (convertor_body),
			WEBKIT_DOM_NODE (node),
			NULL);
	}

	repair_gmail_blockquotes (document_convertor);

	list = webkit_dom_document_query_selector_all (
		document_convertor, "blockquote[type=cite]", NULL);

	create_text_markers_for_citations (list);

	/* Get innertText from convertor */
	inner_text = webkit_dom_html_element_get_inner_text (
		webkit_dom_document_get_body (document_convertor));

	cite_body = webkit_dom_document_query_selector (
		document_convertor, "span.-x-evo-cite-body", NULL);

	if (cite_body) {
		e_editor_selection_save_caret_position (selection);
	} else {
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (paragraph),
			WEBKIT_DOM_NODE (
				e_editor_selection_get_caret_position_node (
					document)),
			NULL);
	}

	top_signature = webkit_dom_document_query_selector (
		document, ".-x-evo-top-signature", NULL);
	signature = webkit_dom_document_query_selector (
		document, "span.-x-evolution-signature", NULL);

	new_blockquote = webkit_dom_document_create_element (
		document, "blockquote", NULL);
	webkit_dom_element_set_attribute (
		new_blockquote, "type", "cite", NULL);

	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (new_blockquote), inner_text, NULL);
	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (new_blockquote));

	if (cite_body) {
		webkit_dom_element_set_attribute (
			new_blockquote, "id", "-x-evo-main-cite", NULL);

		parse_html_into_paragraphs (
			widget, document, new_blockquote, inner_html, TRUE);

		if (signature) {
			WebKitDOMNode *parent =
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (signature));

			if (!top_signature) {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (new_blockquote),
					parent,
					NULL);

				if (start_bottom) {
					webkit_dom_node_insert_before (
						WEBKIT_DOM_NODE (body),
						WEBKIT_DOM_NODE (paragraph),
						WEBKIT_DOM_NODE (parent),
						NULL);
				}
			} else {
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (new_blockquote),
					NULL);
			}
		} else {
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (body),
				WEBKIT_DOM_NODE (new_blockquote),
				NULL);
		}
	} else {
		WebKitDOMNode *signature_clone, *first_child;

		if (signature) {
			signature_clone = webkit_dom_node_clone_node (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (signature)),
				TRUE);
		}

		parse_html_into_paragraphs (
			widget, document, WEBKIT_DOM_ELEMENT (body),
			inner_html, FALSE);

		if (signature) {
			if (!top_signature) {
				signature_clone = webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (body),
					signature_clone,
					NULL);
			} else {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (body),
					signature_clone,
					webkit_dom_node_get_first_child (
						WEBKIT_DOM_NODE (body)),
					NULL);
			}
		}

		first_child = webkit_dom_node_get_first_child (
			WEBKIT_DOM_NODE (body));

		webkit_dom_node_insert_before (
			first_child,
			e_editor_selection_get_caret_position_node (
				document),
			webkit_dom_node_get_first_child (first_child),
			NULL);
	}

	e_editor_widget_quote_plain_text (widget);
	e_editor_selection_restore_caret_position (selection);
	e_editor_widget_force_spellcheck (widget);

	g_free (inner_html);
	g_free (inner_text);
}

static void
e_editor_widget_init (EEditorWidget *editor)
{
	WebKitWebSettings *settings;
	GSettings *g_settings;
	GSettingsSchema *settings_schema;
	ESpellChecker *checker;

	editor->priv = E_EDITOR_WIDGET_GET_PRIVATE (editor);

	webkit_web_view_set_editable (WEBKIT_WEB_VIEW (editor), TRUE);
	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (editor));

	g_object_set (
		G_OBJECT (settings),
		"enable-developer-extras", TRUE,
		"enable-dom-paste", TRUE,
		"enable-file-access-from-file-uris", TRUE,
		"enable-plugins", FALSE,
		"enable-scripts", FALSE,
		"enable-spell-checking", TRUE,
		"respect-image-orientation", TRUE,
		NULL);

	webkit_web_view_set_settings (WEBKIT_WEB_VIEW (editor), settings);

	/* Override the spell-checker, use our own */
	checker = e_spell_checker_new ();
	webkit_set_text_checker (G_OBJECT (checker));
	g_object_unref (checker);

	/* Don't use CSS when possible to preserve compatibility with older
	 * versions of Evolution or other MUAs */
	e_editor_widget_exec_command (
		editor, E_EDITOR_WIDGET_COMMAND_STYLE_WITH_CSS, "false");

	g_signal_connect (
		editor, "user-changed-contents",
		G_CALLBACK (editor_widget_user_changed_contents_cb), NULL);
	g_signal_connect (
		editor, "selection-changed",
		G_CALLBACK (editor_widget_selection_changed_cb), NULL);
	g_signal_connect (
		editor, "should-show-delete-interface-for-element",
		G_CALLBACK (editor_widget_should_show_delete_interface_for_element), NULL);
	g_signal_connect (
		editor, "notify::load-status",
		G_CALLBACK (editor_widget_load_status_changed), NULL);

	editor->priv->selection = g_object_new (
		E_TYPE_EDITOR_SELECTION,
		"editor-widget", editor,
		NULL);

	g_settings = g_settings_new ("org.gnome.desktop.interface");
	g_signal_connect_swapped (
		g_settings, "changed::font-name",
		G_CALLBACK (e_editor_widget_update_fonts), editor);
	g_signal_connect_swapped (
		g_settings, "changed::monospace-font-name",
		G_CALLBACK (e_editor_widget_update_fonts), editor);
	editor->priv->font_settings = g_settings;

	/* This schema is optional.  Use if available. */
	settings_schema = g_settings_schema_source_lookup (
		g_settings_schema_source_get_default (),
		"org.gnome.settings-daemon.plugins.xsettings", FALSE);
	if (settings_schema != NULL) {
		g_settings = g_settings_new ("org.gnome.settings-daemon.plugins.xsettings");
		g_signal_connect_swapped (
			settings, "changed::antialiasing",
			G_CALLBACK (e_editor_widget_update_fonts), editor);
		editor->priv->aliasing_settings = g_settings;
	}

	editor->priv->inline_images = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	e_editor_widget_update_fonts (editor);

	editor->priv->convertor_web_view =
		g_object_ref_sink (WEBKIT_WEB_VIEW (webkit_web_view_new ()));
	settings = webkit_web_view_get_settings (editor->priv->convertor_web_view);

	g_object_set (
		G_OBJECT (settings),
		"enable-scripts", FALSE,
		"enable-plugins", FALSE,
		NULL);

	g_signal_connect (
		editor->priv->convertor_web_view, "notify::load-status",
		G_CALLBACK (html_plain_text_convertor_load_status_changed), editor);

	/* Make WebKit think we are displaying a local file, so that it
	 * does not block loading resources from file:// protocol */
	webkit_web_view_load_string (
		WEBKIT_WEB_VIEW (editor), "", "text/html", "UTF-8", "file://");

	editor_widget_set_links_active (editor, FALSE);
}

/**
 * e_editor_widget_new:
 *
 * Returns a new instance of the editor.
 *
 * Returns: A newly created #EEditorWidget. [transfer-full]
 */
EEditorWidget *
e_editor_widget_new (void)
{
	return g_object_new (E_TYPE_EDITOR_WIDGET, NULL);
}

/**
 * e_editor_widget_get_selection:
 * @widget: an #EEditorWidget
 *
 * Returns an #EEditorSelection object which represents current selection or
 * cursor position within the editor document. The #EEditorSelection allows
 * programmer to manipulate with formatting, selection, styles etc.
 *
 * Returns: An always valid #EEditorSelection object. The object is owned by
 * the @widget and should never be free'd.
 */
EEditorSelection *
e_editor_widget_get_selection (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), NULL);

	return widget->priv->selection;
}

/**
 * e_editor_widget_exec_command:
 * @widget: an #EEditorWidget
 * @command: an #EEditorWidgetCommand to execute
 * @value: value of the command (or @NULL if the command does not require value)
 *
 * The function will fail when @value is @NULL or empty but the current @command
 * requires a value to be passed. The @value is ignored when the @command does
 * not expect any value.
 *
 * Returns: @TRUE when the command was succesfully executed, @FALSE otherwise.
 */
gboolean
e_editor_widget_exec_command (EEditorWidget *widget,
                              EEditorWidgetCommand command,
                              const gchar *value)
{
	WebKitDOMDocument *document;
	const gchar *cmd_str = 0;
	gboolean has_value;

	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

#define CHECK_COMMAND(cmd,str,val) case cmd:\
	if (val) {\
		g_return_val_if_fail (value && *value, FALSE);\
	}\
	has_value = val; \
	cmd_str = str;\
	break;

	switch (command) {
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_BACKGROUND_COLOR, "BackColor", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_BOLD, "Bold", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_COPY, "Copy", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_CREATE_LINK, "CreateLink", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_CUT, "Cut", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR, "DefaultParagraphSeparator", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_DELETE, "Delete", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_FIND_STRING, "FindString", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_FONT_NAME, "FontName", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_FONT_SIZE, "FontSize", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_FONT_SIZE_DELTA, "FontSizeDelta", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_FORE_COLOR, "ForeColor", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK, "FormatBlock", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_FORWARD_DELETE, "ForwardDelete", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_HILITE_COLOR, "HiliteColor", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_INDENT, "Indent", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_INSERT_HORIZONTAL_RULE, "InsertHorizontalRule", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_INSERT_HTML, "InsertHTML", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_INSERT_IMAGE, "InsertImage", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_INSERT_LINE_BREAK, "InsertLineBreak", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, "InsertNewlineInQuotedContent", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_INSERT_ORDERED_LIST, "InsertOrderedList", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_INSERT_PARAGRAPH, "InsertParagraph", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_INSERT_TEXT, "InsertText", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_INSERT_UNORDERED_LIST, "InsertUnorderedList", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_ITALIC, "Italic", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_JUSTIFY_CENTER, "JustifyCenter", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_JUSTIFY_FULL, "JustifyFull", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_JUSTIFY_LEFT, "JustifyLeft", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_JUSTIFY_NONE, "JustifyNone", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_JUSTIFY_RIGHT, "JustifyRight", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_OUTDENT, "Outdent", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_PASTE, "Paste", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_PASTE_AND_MATCH_STYLE, "PasteAndMatchStyle", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_PASTE_AS_PLAIN_TEXT, "PasteAsPlainText", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_PRINT, "Print", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_REDO, "Redo", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_REMOVE_FORMAT, "RemoveFormat", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_SELECT_ALL, "SelectAll", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_STRIKETHROUGH, "Strikethrough", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_STYLE_WITH_CSS, "StyleWithCSS", TRUE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_SUBSCRIPT, "Subscript", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_SUPERSCRIPT, "Superscript", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_TRANSPOSE, "Transpose", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_UNDERLINE, "Underline", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_UNDO, "Undo", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_UNLINK, "Unlink", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_UNSELECT, "Unselect", FALSE)
		CHECK_COMMAND (E_EDITOR_WIDGET_COMMAND_USE_CSS, "UseCSS", TRUE)
	}

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	return webkit_dom_document_exec_command (
		document, cmd_str, FALSE, has_value ? value : "" );
}

/**
 * e_editor_widget_get_changed:
 * @widget: an #EEditorWidget
 *
 * Whether content of the editor has been changed.
 *
 * Returns: @TRUE when document was changed, @FALSE otherwise.
 */
gboolean
e_editor_widget_get_changed (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->changed;
}

/**
 * e_editor_widget_set_changed:
 * @widget: an #EEditorWidget
 * @changed: whether document has been changed or not
 *
 * Sets whether document has been changed or not. The editor is tracking changes
 * automatically, but sometimes it's necessary to change the dirty flag to force
 * "Save changes" dialog for example.
 */
void
e_editor_widget_set_changed (EEditorWidget *widget,
                             gboolean changed)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if (widget->priv->changed == changed)
		return;

	widget->priv->changed = changed;

	g_object_notify (G_OBJECT (widget), "changed");
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

static void
insert_quote_symbols (WebKitDOMHTMLElement *element,
                      gint quote_level,
                      gboolean skip_first,
                      gboolean insert_newline)
{
	gchar *text;
	gint ii;
	GString *output;
	gchar *indent;

	if (!WEBKIT_DOM_IS_HTML_ELEMENT (element))
		return;

	text = webkit_dom_html_element_get_inner_html (element);
	output = g_string_new ("");
	indent = g_strnfill (quote_level, '>');

	if (g_strcmp0 (text, "\n") == 0) {
		g_string_append (output, "<span class=\"-x-evo-quoted\">");
		g_string_append (output, indent);
		g_string_append (output, " </span>");
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
			if (!(ii == 0 && (g_strcmp0 (lines[ii], "&gt;") == 0) &&
			    g_str_has_prefix (text, "&gt;"))) {
				g_string_append (output, "<span class=\"-x-evo-quoted\">");
				g_string_append (output, indent);
				g_string_append (output, " </span>");
			}
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
	g_free (indent);
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

	if (next_sibling && WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling) &&
	    WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (webkit_dom_node_get_next_sibling (next_sibling))) {
		insert_newline = TRUE;
	}

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
	gchar *indent;
	gchar *content;
	WebKitDOMElement *element;

	indent = g_strnfill (quote_level, '>');
	element = webkit_dom_document_create_element (document, "SPAN", NULL);
	element_add_class (element, "-x-evo-quoted");
	content = g_strconcat (indent, " ", NULL);
	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (element),
		content,
		NULL);

	if (is_html_node) {
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

	if (is_html_node) {
		webkit_dom_node_remove_child (
			webkit_dom_node_get_parent_node (node),
			node,
			NULL);
	}

	g_free (indent);
	g_free (content);
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
	WebKitDOMNode *next_sibling, *prev_sibling;

	node = webkit_dom_node_get_first_child (node);

	while (node) {
		skip_node = FALSE;
		move_next = FALSE;
		is_html_node = FALSE;

		if (WEBKIT_DOM_IS_COMMENT (node))
			goto next_node;

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
			} else
				suppress_next = FALSE;

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
			goto next_node;
		}

		if (WEBKIT_DOM_IS_HTML_META_ELEMENT (node)) {
			goto next_node;
		}
		if (WEBKIT_DOM_IS_HTML_STYLE_ELEMENT (node)) {
			move_next = TRUE;
			goto next_node;
		}
		if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (node)) {
			move_next = TRUE;
			goto next_node;
		}

		if (webkit_dom_element_get_child_element_count (WEBKIT_DOM_ELEMENT (node)) != 0)
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
			if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (prev_sibling))
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

		if (!WEBKIT_DOM_IS_HTMLBR_ELEMENT (node))
			goto not_br;

		if (!prev_sibling) {
			WebKitDOMNode *parent;

			parent = webkit_dom_node_get_parent_node (node);

			/* BR in the beginning of the citation */
			if (WEBKIT_DOM_IS_HTML_PRE_ELEMENT (parent))
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);
		}

		if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (next_sibling) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-temp-text-wrapper")) {
			/* Situation when anchors are alone on line */
			gchar *text_content;

			text_content = webkit_dom_node_get_text_content (prev_sibling);

			if (g_str_has_suffix (text_content, "\n")) {
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);
				webkit_dom_node_remove_child (
					webkit_dom_node_get_parent_node (node),
					node,
					NULL);
				g_free (text_content);
				node = next_sibling;
				skip_node = TRUE;
				goto next_node;
			}
			g_free (text_content);
		}

		if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (prev_sibling)) {
			gchar *indent;
			gchar *content;

			indent = g_strnfill (quote_level, '>');

			content = g_strconcat (
				"<span class=\"-x-evo-quoted\">",
				indent,
				" </span><br class=\"-x-evo-temp-br\">",
				NULL);

			webkit_dom_html_element_set_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (node),
				content,
				NULL);

			g_free (content);
			g_free (indent);

			node = next_sibling;
			skip_node = TRUE;
			goto next_node;
		}

		if (!prev_sibling && !next_sibling) {
			WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);

			if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (parent)) {
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);
			}
		}

		if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-temp-text-wrapper")) {
			gchar *text_content;

			text_content = webkit_dom_node_get_text_content (prev_sibling);
			if (g_strcmp0 (text_content, "") == 0)
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);

			g_free (text_content);
		}

		if (is_citation_node (prev_sibling)) {
			insert_quote_symbols_before_node (
				document, node, quote_level, FALSE);
		}
 not_br:
		if (g_strcmp0 (webkit_dom_node_get_text_content (node), "") == 0) {
			move_next = TRUE;
			goto next_node;
		}

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

static gint
get_citation_level (WebKitDOMNode *node,
                    gboolean set_plaintext_quoted)
{
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);
	gint level = 0;

	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent)) {
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

WebKitDOMElement *
e_editor_widget_quote_plain_text_element (EEditorWidget *widget,
                                          WebKitDOMElement *element)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *element_clone;
	gint level;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));

	element_clone = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (element), TRUE);
	level = get_citation_level (WEBKIT_DOM_NODE (element), TRUE);

	quote_plain_text_recursive (
		document, element_clone, element_clone, level);

	/* Replace old element with one, that is quoted */
	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
		element_clone,
		WEBKIT_DOM_NODE (element),
		NULL);

	return WEBKIT_DOM_ELEMENT (element_clone);
}

/**
 * e_editor_widget_quote_plain_text:
 * @widget: an #EEditorWidget
 *
 * Quote text inside citation blockquotes in plain text mode.
 *
 * As this function is cloning and replacing all citation blockquotes keep on
 * mind that any pointers to nodes inside these blockquotes will be invalidated.
 */
WebKitDOMElement *
e_editor_widget_quote_plain_text (EEditorWidget *widget)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitDOMNode *body_clone;
	WebKitDOMNamedNodeMap *attributes;
	WebKitDOMNodeList *list;
	WebKitDOMElement *element;
	gint ii, length;
	gulong attributes_length;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

	/* Check if the document is already quoted */
	element = webkit_dom_document_query_selector (
		document, ".-x-evo-plaintext-quoted", NULL);
	if (element)
		return;

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

		if (prev_sibling && WEBKIT_DOM_IS_HTMLBR_ELEMENT (prev_sibling)) {
			webkit_dom_node_remove_child (
				webkit_dom_node_get_parent_node (prev_sibling),
				prev_sibling,
				NULL);
		}
		if (next_sibling && WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling)) {
			webkit_dom_node_remove_child (
				webkit_dom_node_get_parent_node (next_sibling),
				next_sibling,
				NULL);
		}
		if (webkit_dom_node_has_child_nodes (blockquote)) {
			WebKitDOMNode *child = webkit_dom_node_get_first_child (blockquote);
			if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (child)) {
				webkit_dom_node_remove_child (
					blockquote,
					child,
					NULL);
			}
		}
	}

	quote_plain_text_recursive (
		document, body_clone, body_clone, 0);

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

	/* Replace old BODY with one, that is quoted */
	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (body)),
		body_clone,
		WEBKIT_DOM_NODE (body),
		NULL);

	return WEBKIT_DOM_ELEMENT (body_clone);
}

/**
 * e_editor_widget_dequote_plain_text:
 * @widget: an #EEditorWidget
 *
 * Dequote already quoted plain text in editor.
 * Editor have to be quoted with e_editor_widget_quote_plain_text otherwise
 * it's not working.
 */
void
e_editor_widget_dequote_plain_text (EEditorWidget *widget)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *paragraphs;
	gint length, ii;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

	paragraphs = webkit_dom_document_query_selector_all (
		document, "blockquote.-x-evo-plaintext-quoted", NULL);
	length = webkit_dom_node_list_get_length (paragraphs);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNodeList *list;
		WebKitDOMElement *element;
		gint jj, list_length;

		element = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (paragraphs, ii));

		if (is_citation_node (WEBKIT_DOM_NODE (element))) {
			element_remove_class (element, "-x-evo-plaintext-quoted");

			list = webkit_dom_element_query_selector_all (
				element, "span.-x-evo-quoted", NULL);
			list_length = webkit_dom_node_list_get_length (list);
			for (jj = 0; jj < list_length; jj++) {
				WebKitDOMNode *node = webkit_dom_node_list_item (list, jj);

				webkit_dom_node_remove_child (
					webkit_dom_node_get_parent_node (node),
					node,
					NULL);
			}
			list = webkit_dom_element_query_selector_all (
				element, "span.-x-evo-temp-text-wrapper", NULL);
			list_length = webkit_dom_node_list_get_length (list);
			for (jj = 0; jj < list_length; jj++) {
				WebKitDOMNode *node = webkit_dom_node_list_item (list, jj);

				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (node),
					webkit_dom_node_get_first_child (node),
					node,
					NULL);
			}
		}
	}
}

/**
 * e_editor_widget_get_html_mode:
 * @widget: an #EEditorWidget
 *
 * Whether the editor is in HTML mode or plain text mode. In HTML mode,
 * more formatting options are avilable an the email is sent as
 * multipart/alternative.
 *
 * Returns: @TRUE when HTML mode is enabled, @FALSE otherwise.
 */
gboolean
e_editor_widget_get_html_mode (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->html_mode;
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
			WEBKIT_DOM_HTML_ELEMENT (quoted_node),
			text_content,
			NULL);
		g_free (text_content);
	}

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
			WEBKIT_DOM_HTML_ELEMENT (quoted_node),
			text_content,
			NULL);
		g_free (text_content);
	}

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

static void
remove_attributes (WebKitDOMElement *element)
{
	webkit_dom_element_remove_attribute (element, "class");
	webkit_dom_element_remove_attribute (element, "id");
	webkit_dom_element_remove_attribute (element, "name");
	webkit_dom_element_remove_attribute (element, "x-evo-smiley");
	webkit_dom_element_remove_attribute (element, "data-name");
	webkit_dom_element_remove_attribute (element, "data-inline");
	webkit_dom_element_remove_attribute (element, "bgcolor");
	webkit_dom_element_remove_attribute (element, "background");
	webkit_dom_element_remove_attribute (element, "style");
}

static void
process_elements (WebKitDOMNode *node,
                  gboolean to_html,
                  gboolean changing_mode,
                  gboolean to_plain_text,
                  GString *buffer)
{
	WebKitDOMNodeList *nodes;
	gulong ii, length;
	gchar *content;

	if (to_plain_text && !buffer)
		return;

	/* Skip signature */
	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evolution-signature")) {
		if (to_plain_text && !changing_mode)
			g_string_append (buffer, "\n");
		if (to_html)
			remove_attributes (WEBKIT_DOM_ELEMENT (node));
		if (changing_mode)
			return;
	}

	if (changing_mode && to_plain_text) {
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node)) {
			WebKitDOMNamedNodeMap *attributes;
			gulong attributes_length;

			/* Copy attributes */
			g_string_append (buffer, "<html><head></head><body ");
			attributes = webkit_dom_element_get_attributes (WEBKIT_DOM_ELEMENT (node));
			attributes_length = webkit_dom_named_node_map_get_length (attributes);
			for (ii = 0; ii < attributes_length; ii++) {
				gchar *name, *value;
				WebKitDOMNode *node = webkit_dom_named_node_map_item (attributes, ii);

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
		}
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
			/* Replace tabs with 4 whitespaces, otherwise they got
			 * replaced by single whitespace */
			if (strstr (content, "\x9")) {
				regex = g_regex_new ("\x9", 0, 0, NULL);
				tmp = g_regex_replace (
					regex, content, -1, 0, "    ", 0, NULL);
				webkit_dom_node_set_text_content (child, tmp, NULL);
				g_free (content);
				g_free (tmp);
				content = webkit_dom_node_get_text_content (child);
				g_regex_unref (regex);
			}

			if (strstr (content, UNICODE_ZERO_WIDTH_SPACE)) {
				regex = g_regex_new (UNICODE_ZERO_WIDTH_SPACE, 0, 0, NULL);
				tmp = g_regex_replace (
					regex, content, -1, 0, "", 0, NULL);
				g_free (content);
				g_free (tmp);
				content = webkit_dom_node_get_text_content (child);
				g_regex_unref (regex);
			}

			if (to_plain_text || changing_mode)
				g_string_append (buffer, content);

			g_free (content);
		} else if (!WEBKIT_DOM_IS_COMMENT (child)) {
			/* Leave caret position untouched */
			if (element_has_id (WEBKIT_DOM_ELEMENT (child), "-x-evo-caret-position")) {
				if (changing_mode && to_plain_text) {
					content = webkit_dom_html_element_get_outer_html (
						WEBKIT_DOM_HTML_ELEMENT (child));
					g_string_append (buffer, content);
					g_free (content);
				}
				if (to_html) {
					webkit_dom_node_remove_child (
						webkit_dom_node_get_parent_node (child),
						child,
						NULL);
				}

				skip_node = TRUE;
			}

			/* Leave blockquotes as they are */
			if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (child)) {
				if (changing_mode && to_plain_text) {
					content = webkit_dom_html_element_get_outer_html (
						WEBKIT_DOM_HTML_ELEMENT (child));
					g_string_append (buffer, content);
					g_free (content);
					skip_node = TRUE;
				} else
					process_blockquote (WEBKIT_DOM_ELEMENT (child));
			}

			if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (child)) {
				if (to_html)
					remove_attributes (WEBKIT_DOM_ELEMENT (child));
			}

			/* Leave paragraphs as they are */
			if (element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-paragraph")) {
				if (changing_mode && to_plain_text) {
					content = webkit_dom_html_element_get_outer_html (
						WEBKIT_DOM_HTML_ELEMENT (child));
					g_string_append (buffer, content);
					g_free (content);
					skip_node = TRUE;
				}
				if (to_html)
					remove_attributes (WEBKIT_DOM_ELEMENT (child));
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
				}
				if (to_html) {
					WebKitDOMElement *img;

					img = WEBKIT_DOM_ELEMENT (
						webkit_dom_node_get_first_child (
							WEBKIT_DOM_NODE (child)));

					remove_attributes (img);

					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (
							child),
						WEBKIT_DOM_NODE (img),
						child,
						NULL);
					webkit_dom_node_remove_child (
						webkit_dom_node_get_parent_node (
							child),
						child,
						NULL);
					skip_node = TRUE;
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
					remove_attributes (WEBKIT_DOM_ELEMENT (child));
			}

			if (to_plain_text) {
				/* Insert new line when we hit BR element */
				if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (child))
					g_string_append (buffer, changing_mode ? "<br>" : "\n");
			}
		}

		if (webkit_dom_node_has_child_nodes (child) && !skip_node)
			process_elements (child, to_html, changing_mode, to_plain_text, buffer);
	}

	if (to_plain_text && (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (node) ||
	    WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT (node))) {
		gboolean add_br = TRUE;
		WebKitDOMNode *next_sibling = webkit_dom_node_get_next_sibling (node);

		/* If we don't have next sibling (last element in body) or next element is
		 * signature we are not adding the BR element */
		if (!next_sibling)
			add_br = FALSE;

		if (element_has_class (webkit_dom_node_get_parent_element (node), "-x-evo-indented"))
			add_br = TRUE;

		if (next_sibling && WEBKIT_DOM_IS_HTML_DIV_ELEMENT (next_sibling)) {
			if (webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (next_sibling), "span.-x-evolution-signature", NULL))
				add_br = FALSE;
		}

		content = webkit_dom_node_get_text_content (node);
		if (add_br && g_utf8_strlen (content, -1) > 0)
			g_string_append (buffer, changing_mode ? "<br>" : "\n");

		g_free (content);
	}

}

static void
remove_wrapping (EEditorWidget *widget)
{
	gint length;
	gint ii;
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	list = webkit_dom_document_query_selector_all (document, "br.-x-evo-wrap-br", NULL);

	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		webkit_dom_node_remove_child (
			webkit_dom_node_get_parent_node (node), node, NULL);
	}
}

static void
remove_images (EEditorWidget *widget)
{
	gint length;
	gint ii;
	WebKitDOMDocument *document;
	WebKitDOMNodeList *images;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	images = webkit_dom_document_query_selector_all (document, "img", NULL);

	length = webkit_dom_node_list_get_length (images);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *img = webkit_dom_node_list_item (images, ii);

		webkit_dom_node_remove_child (
			webkit_dom_node_get_parent_node (img), img, NULL);
	}
}

static void
toggle_smileys (EEditorWidget *widget)
{
	gboolean html_mode;
	gint length;
	gint ii;
	WebKitDOMDocument *document;
	WebKitDOMNodeList *smileys;

	html_mode = e_editor_widget_get_html_mode (widget);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	smileys = webkit_dom_document_query_selector_all (
		document, "img.-x-evo-smiley-img", NULL);

	length = webkit_dom_node_list_get_length (smileys);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *img = webkit_dom_node_list_item (smileys, ii);
		WebKitDOMNode *text = webkit_dom_node_get_next_sibling (img);

		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (html_mode ? text : img),
			"style",
			"display: none",
			NULL);

		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (html_mode ? img : text), "style");
	}
}

static void
toggle_paragraphs_style (EEditorWidget *widget)
{
	EEditorSelection *selection;
	gboolean html_mode;
	gint length;
	gint ii;
	WebKitDOMDocument *document;
	WebKitDOMNodeList *paragraphs;

	html_mode = e_editor_widget_get_html_mode (widget);
	selection = e_editor_widget_get_selection (widget);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	paragraphs = webkit_dom_document_query_selector_all (
		document, ".-x-evo-paragraph", NULL);

	length = webkit_dom_node_list_get_length (paragraphs);

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (paragraphs, ii);

		if (html_mode)
			/* In HTML mode the paragraphs don't have width limit */
			webkit_dom_element_remove_attribute (
				WEBKIT_DOM_ELEMENT (node), "style");
		else {
			WebKitDOMNode *parent;

			parent = webkit_dom_node_get_parent_node (node);
			/* If the paragraph is inside indented paragraph don't set
			 * the style as it will be inherited */
			if (!element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-indented"))
				/* In HTML mode the paragraphs have width limit */
				e_editor_selection_set_paragraph_style (
					selection, WEBKIT_DOM_ELEMENT (node), -1, 0);
		}
	}
}

static gchar *
process_dom_document_for_saving_as_draft (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-caret-position");

	if (element)
		webkit_dom_element_set_attribute (
			element, "style", "display: none; color: red;", NULL);

	element = webkit_dom_document_get_document_element (document);
	return webkit_dom_html_element_get_outer_html (
		WEBKIT_DOM_HTML_ELEMENT (element));
}

static gchar *
process_dom_document_for_mode_change (WebKitDOMDocument *document)
{
	WebKitDOMNode *body;
	GString *plain_text;

	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));

	plain_text = g_string_sized_new (1024);

	process_elements (body, FALSE, TRUE, TRUE, plain_text);

	g_string_append (plain_text, "</body></html>");

	return g_string_free (plain_text, FALSE);
}

static gchar *
process_dom_document_for_plain_text (EEditorWidget *widget,
                                     WebKitDOMDocument *document)
{
	WebKitDOMNode *body;
	WebKitDOMNode *body_clone;
	WebKitDOMNodeList *paragraphs;
	gint length, ii;
	GString *plain_text;

	plain_text = g_string_sized_new (1024);

	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));
	body_clone = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (body), TRUE);

	paragraphs = webkit_dom_element_query_selector_all (
			WEBKIT_DOM_ELEMENT (body_clone),
			".-x-evo-paragraph",
			NULL);

	length = webkit_dom_node_list_get_length (paragraphs);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *paragraph;

		paragraph = webkit_dom_node_list_item (paragraphs, ii);

		e_editor_selection_wrap_paragraph (
			e_editor_widget_get_selection (widget),
			WEBKIT_DOM_ELEMENT (paragraph));
	}

	process_elements (body_clone, FALSE, FALSE, TRUE, plain_text);

	/* Return text content between <body> and </body> */
	return g_string_free (plain_text, FALSE);
}

static gchar *
process_dom_document_for_html (WebKitDOMDocument *document)
{
	WebKitDOMNode *body;
	WebKitDOMElement *element;

	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));
	process_elements (body, TRUE, FALSE, FALSE, NULL);
	element = webkit_dom_document_get_document_element (document);
	return webkit_dom_html_element_get_outer_html (
		WEBKIT_DOM_HTML_ELEMENT (element));
}

static gboolean
show_lose_formatting_dialog (EEditorWidget *widget)
{
	gint result;
	GtkWidget *toplevel, *dialog;
	GtkWindow *parent = NULL;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (widget));

	if (GTK_IS_WINDOW (toplevel))
		parent = GTK_WINDOW (toplevel);

	dialog = gtk_message_dialog_new (
		parent,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_WARNING,
		GTK_BUTTONS_NONE,
		_("Turning HTML mode off will cause the text "
		"to lose all formatting. Do you want to continue?"));
	gtk_dialog_add_buttons (
		GTK_DIALOG (dialog),
		_("_Don't lose formatting"), GTK_RESPONSE_CANCEL,
		_("_Lose formatting"), GTK_RESPONSE_OK,
		NULL);

	result = gtk_dialog_run (GTK_DIALOG (dialog));

	if (result != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		/* Nothing has changed, but notify anyway */
		g_object_notify (G_OBJECT (widget), "html-mode");
		return FALSE;
	}

	gtk_widget_destroy (dialog);

	return TRUE;
}

static void
convert_when_changing_composer_mode (EEditorWidget *widget)
{
	EEditorSelection *selection;
	gchar *inner_text, *inner_html;
	gboolean restore = TRUE;
	WebKitDOMDocument *document;
	WebKitDOMElement *top_signature, *signature, *element, *blockquote;
	WebKitDOMHTMLElement *body;
	WebKitDOMNode *signature_clone, *from;
	WebKitDOMNodeList *blockquotes;

	selection = e_editor_widget_get_selection (widget);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	body = webkit_dom_document_get_body (document);

	top_signature = webkit_dom_document_query_selector (
		document, ".-x-evo-top-signature", NULL);
	signature = webkit_dom_document_query_selector (
		document, "span.-x-evolution-signature", NULL);
	blockquote = webkit_dom_document_get_element_by_id (
		document, "-x-evo-main-cite");

	element = webkit_dom_document_create_element (
		document, "blockquote", NULL);

	if (blockquote) {
		WebKitDOMElement *input_start;

		webkit_dom_element_set_attribute (
			element, "type", "cite", NULL);

		input_start = webkit_dom_document_get_element_by_id (
			document, "-x-evo-input-start");

		restore = input_start ? TRUE : FALSE;

		if (input_start) {
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (input_start),
				e_editor_selection_get_caret_position_node (
					document),
				NULL);
		}
		from = WEBKIT_DOM_NODE (blockquote);
	} else {
		if (signature) {
			signature_clone = webkit_dom_node_clone_node (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (signature)),
				TRUE);

			webkit_dom_node_remove_child (
				WEBKIT_DOM_NODE (body),
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (signature)),
				NULL);
		}
		from = WEBKIT_DOM_NODE (body);
	}

	blockquotes = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (from),
		"blockquote[type=cite]",
		NULL);

	create_text_markers_for_citations (blockquotes);

	inner_text = webkit_dom_html_element_get_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (from));

	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (element), inner_text, NULL);

	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element));

	parse_html_into_paragraphs (
		widget, document,
		blockquote ? element : WEBKIT_DOM_ELEMENT (body),
		inner_html,
		blockquote ? TRUE : FALSE);

	if (blockquote) {
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (blockquote)),
			WEBKIT_DOM_NODE (element),
			WEBKIT_DOM_NODE (blockquote),
			NULL);

		remove_attributes (WEBKIT_DOM_ELEMENT (body));
	} else {
		WebKitDOMNode *first_child;

		if (signature) {
			if (!top_signature) {
				signature_clone = webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (body),
					signature_clone,
					NULL);
			} else {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (body),
					signature_clone,
					webkit_dom_node_get_first_child (
						WEBKIT_DOM_NODE (body)),
					NULL);
			}
		}

		first_child = webkit_dom_node_get_first_child (
			WEBKIT_DOM_NODE (body));

		webkit_dom_node_insert_before (
			first_child,
			e_editor_selection_get_caret_position_node (
				document),
			webkit_dom_node_get_first_child (first_child),
			NULL);

		e_editor_selection_wrap_paragraphs_in_document (
			selection, document);

		/* Update fonts - in plain text we only want monospace */
		e_editor_widget_update_fonts (widget);
	}

	if (blockquote || webkit_dom_node_list_get_length (blockquotes) > 0)
		body = WEBKIT_DOM_HTML_ELEMENT (
			e_editor_widget_quote_plain_text (widget));

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-converted", "", NULL);

	g_free (inner_text);
	g_free (inner_html);

	/* Update fonts - in plain text we only want monospace */
	e_editor_widget_update_fonts (widget);

	if (restore)
		e_editor_selection_restore_caret_position (selection);

	e_editor_widget_force_spellcheck (widget);

	if (restore) {
		WebKitDOMDOMWindow *window;
		WebKitDOMDOMSelection *dom_selection;

		window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (window);
		webkit_dom_dom_selection_modify (
			dom_selection, "move", "backward", "character");
	}
}

/**
 * e_editor_widget_set_html_mode:
 * @widget: an #EEditorWidget
 * @html_mode: @TRUE to enable HTML mode, @FALSE to enable plain text mode
 *
 * When switching from HTML to plain text mode, user will be prompted whether
 * he/she really wants to switch the mode and lose all formatting. When user
 * declines, the property is not changed. When they accept, the all formatting
 * is lost.
 */
void
e_editor_widget_set_html_mode (EEditorWidget *widget,
                               gboolean html_mode)
{
	EEditorSelection *selection;
	gboolean is_from_new_message, converted, edit_as_new, message, convert;
	gboolean reply, hide;
	WebKitDOMElement *blockquote;
	WebKitDOMHTMLElement *body;
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	selection = e_editor_widget_get_selection (widget);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
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

	/* If toggling from HTML to plain text mode, ask user first */
	if (convert && widget->priv->html_mode && !html_mode) {
		if (!show_lose_formatting_dialog (widget))
			return;

		widget->priv->html_mode = html_mode;

		convert_when_changing_composer_mode (widget);

		goto out;
	}

	if (html_mode == widget->priv->html_mode)
		return;

	widget->priv->html_mode = html_mode;

	/* Update fonts - in plain text we only want monospace */
	e_editor_widget_update_fonts (widget);

	blockquote = webkit_dom_document_query_selector (
		document, "blockquote[type|=cite]", NULL);

	if (widget->priv->html_mode) {
		if (blockquote)
			e_editor_widget_dequote_plain_text (widget);

		toggle_paragraphs_style (widget);
		toggle_smileys (widget);
		remove_wrapping (widget);

	} else {
		gchar *plain;

		/* Save caret position -> it will be restored in e-composer-private.c */
		e_editor_selection_save_caret_position (selection);

		if (blockquote) {
			e_editor_selection_wrap_paragraphs_in_document (
				selection, document);
			e_editor_widget_quote_plain_text (widget);
		}

		toggle_paragraphs_style (widget);
		toggle_smileys (widget);
		remove_images (widget);

		plain = process_dom_document_for_mode_change (document);

		if (*plain)
			webkit_web_view_load_string (
				WEBKIT_WEB_VIEW (widget), plain, NULL, NULL, "file://");

		g_free (plain);
	}

 out:
	g_object_notify (G_OBJECT (widget), "html-mode");
}

/**
 * e_editor_widget_get_inline_spelling:
 * @widget: an #EEditorWidget
 *
 * Returns whether automatic spellchecking is enabled or not. When enabled,
 * editor will perform spellchecking as user is typing. Otherwise spellcheck
 * has to be run manually from menu.
 *
 * Returns: @TRUE when automatic spellchecking is enabled, @FALSE otherwise.
 */
gboolean
e_editor_widget_get_inline_spelling (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->inline_spelling;
}

/**
 * e_editor_widget_set_inline_spelling:
 * @widget: an #EEditorWidget
 * @inline_spelling: @TRUE to enable automatic spellchecking, @FALSE otherwise
 *
 * Enables or disables automatic spellchecking.
 */
void
e_editor_widget_set_inline_spelling (EEditorWidget *widget,
                                     gboolean inline_spelling)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if (widget->priv->inline_spelling == inline_spelling)
		return;

	widget->priv->inline_spelling = inline_spelling;

	g_object_notify (G_OBJECT (widget), "inline-spelling");
}

/**
 * e_editor_widget_get_magic_links:
 * @widget: an #EEditorWidget
 *
 * Returns whether automatic links conversion is enabled. When enabled, the editor
 * will automatically convert any HTTP links into clickable HTML links.
 *
 * Returns: @TRUE when magic links are enabled, @FALSE otherwise.
 */
gboolean
e_editor_widget_get_magic_links (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->magic_links;
}

/**
 * e_editor_widget_set_magic_links:
 * @widget: an #EEditorWidget
 * @magic_links: @TRUE to enable magic links, @FALSE to disable them
 *
 * Enables or disables automatic links conversion.
 */
void
e_editor_widget_set_magic_links (EEditorWidget *widget,
                                 gboolean magic_links)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if (widget->priv->magic_links == magic_links)
		return;

	widget->priv->magic_links = magic_links;

	g_object_notify (G_OBJECT (widget), "magic-links");
}

/**
 * e_editor_widget_get_magic_smileys:
 * @widget: an #EEditorWidget
 *
 * Returns whether automatic conversion of smileys is enabled or disabled. When
 * enabled, the editor will automatically convert text smileys ( :-), ;-),...)
 * into images.
 *
 * Returns: @TRUE when magic smileys are enabled, @FALSE otherwise.
 */
gboolean
e_editor_widget_get_magic_smileys (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->magic_smileys;
}

/**
 * e_editor_widget_set_magic_smileys:
 * @widget: an #EEditorWidget
 * @magic_smileys: @TRUE to enable magic smileys, @FALSE to disable them
 *
 * Enables or disables magic smileys.
 */
void
e_editor_widget_set_magic_smileys (EEditorWidget *widget,
                                   gboolean magic_smileys)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if (widget->priv->magic_smileys == magic_smileys)
		return;

	widget->priv->magic_smileys = magic_smileys;

	g_object_notify (G_OBJECT (widget), "magic-smileys");
}

/**
 * e_editor_widget_get_spell_checker:
 * @widget: an #EEditorWidget
 *
 * Returns an #ESpellChecker object that is used to perform spellchecking.
 *
 * Returns: An always-valid #ESpellChecker object
 */
ESpellChecker *
e_editor_widget_get_spell_checker (EEditorWidget *widget)
{
	return E_SPELL_CHECKER (webkit_get_text_checker ());
}

/**
 * e_editor_widget_get_text_html:
 * @widget: an #EEditorWidget:
 *
 * Returns processed HTML content of the editor document (with elements attributes
 * used in Evolution composer)
 *
 * Returns: A newly allocated string
 */
gchar *
e_editor_widget_get_text_html (EEditorWidget *widget)
{
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	change_images_http_src_to_evo_http (widget, FALSE);
	return process_dom_document_for_html (document);
}

/**
 * e_editor_widget_get_text_html_for_drafts:
 * @widget: an #EEditorWidget:
 *
 * Returns HTML content of the editor document (without elements attributes
 * used in Evolution composer)
 *
 * Returns: A newly allocated string
 */
gchar *
e_editor_widget_get_text_html_for_drafts (EEditorWidget *widget)
{
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	return process_dom_document_for_saving_as_draft (document);
}

/**
 * e_editor_widget_get_text_plain:
 * @widget: an #EEditorWidget
 *
 * Returns plain text content of the @widget. The algorithm removes any
 * formatting or styles from the document and keeps only the text and line
 * breaks.
 *
 * Returns: A newly allocated string with plain text content of the document.
 */
gchar *
e_editor_widget_get_text_plain (EEditorWidget *widget)
{
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

	return process_dom_document_for_plain_text (widget, document);
}

static void
convert_html_to_plain_text (EEditorWidget *widget,
                            const gchar *html)
{
	webkit_web_view_load_string (
		widget->priv->convertor_web_view, html, NULL, NULL, "file://");
}

/**
 * e_editor_widget_set_text_html:
 * @widget: an #EEditorWidget
 * @text: HTML code to load into the editor
 *
 * Loads given @text into the editor, destroying any content already present.
 */
void
e_editor_widget_set_text_html (EEditorWidget *widget,
                               const gchar *text)
{
	widget->priv->reload_in_progress = TRUE;

	/* Only convert messages that are in HTML */
	if (!widget->priv->html_mode) {
		if (strstr (text, "<!-- text/html -->")) {
			if (!show_lose_formatting_dialog (widget)) {
				e_editor_widget_set_html_mode (widget, TRUE);
				webkit_web_view_load_string (
					WEBKIT_WEB_VIEW (widget), text, NULL, NULL, "file://");
				return;
			}
			convert_html_to_plain_text (widget, text);
		} else {
			convert_html_to_plain_text (widget, text);
		}
	} else {
		webkit_web_view_load_string (
			WEBKIT_WEB_VIEW (widget), text, NULL, NULL, "file://");
	}
}

/**
 * e_editor_widget_set_text_plain:
 * @widget: an #EEditorWidget
 * @text: A plain text to load into the editor
 *
 * Loads given @text into the editor, destryoing any content already present.
 */
void
e_editor_widget_set_text_plain (EEditorWidget *widget,
                                const gchar *text)
{
	widget->priv->reload_in_progress = TRUE;

	webkit_web_view_load_string (
		WEBKIT_WEB_VIEW (widget), text, "text/plain", NULL, "file://");
}

/**
 * e_editor_widget_paste_clipboard_quoted:
 * @widget: an #EEditorWidget
 *
 * Pastes current content of clipboard into the editor as quoted text
 */
void
e_editor_widget_paste_clipboard_quoted (EEditorWidget *widget)
{
	EEditorWidgetClass *class;

	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	class = E_EDITOR_WIDGET_GET_CLASS (widget);
	g_return_if_fail (class->paste_clipboard_quoted != NULL);

	class->paste_clipboard_quoted (widget);
}

void
e_editor_widget_embed_styles (EEditorWidget *widget)
{
	WebKitWebSettings *settings;
	WebKitDOMDocument *document;
	WebKitDOMElement *sheet;
	gchar *stylesheet_uri;
	gchar *stylesheet_content;
	const gchar *stylesheet;
	gsize length;

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (widget));
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

	g_object_get (
		G_OBJECT (settings),
		"user-stylesheet-uri", &stylesheet_uri,
		NULL);

	stylesheet = strstr (stylesheet_uri, ",");
	stylesheet_content = (gchar *) g_base64_decode (stylesheet, &length);
	g_free (stylesheet_uri);

	if (length == 0) {
		g_free (stylesheet_content);
		return;
	}

	e_web_view_create_and_add_css_style_sheet (document, "-x-evo-composer-sheet");

	sheet = webkit_dom_document_get_element_by_id (document, "-x-evo-composer-sheet");
	webkit_dom_element_set_attribute (
		sheet,
		"type",
		"text/css",
		NULL);

	webkit_dom_html_element_set_inner_html (WEBKIT_DOM_HTML_ELEMENT (sheet), stylesheet_content, NULL);

	g_free (stylesheet_content);
}

void
e_editor_widget_remove_embed_styles (EEditorWidget *widget)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *sheet;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	sheet = webkit_dom_document_get_element_by_id (
			document, "-x-evo-composer-sheet");

	webkit_dom_node_remove_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (sheet)),
		WEBKIT_DOM_NODE (sheet),
		NULL);
}

/**
 * e_editor_widget_update_fonts:
 * @widget: an #EEditorWidget
 *
 * Forces the editor to reload font settings from WebKitWebSettings and apply
 * it on the content of the editor document.
 */
void
e_editor_widget_update_fonts (EEditorWidget *widget)
{
	GString *stylesheet;
	gchar *base64;
	gchar *aa = NULL;
	WebKitWebSettings *settings;
	PangoFontDescription *ms, *vw;
	const gchar *styles[] = { "normal", "oblique", "italic" };
	const gchar *smoothing = NULL;
	GtkStyleContext *context;
	GdkColor *link = NULL;
	GdkColor *visited = NULL;
	gchar *font;

	font = g_settings_get_string (
		widget->priv->font_settings,
		"monospace-font-name");
	ms = pango_font_description_from_string (
		font ? font : "monospace 10");
	g_free (font);

	if (widget->priv->html_mode) {
		font = g_settings_get_string (
				widget->priv->font_settings,
				"font-name");
		vw = pango_font_description_from_string (
				font ? font : "serif 10");
		g_free (font);
	} else {
		/* When in plain text mode, force monospace font */
		vw = pango_font_description_copy (ms);
	}

	stylesheet = g_string_new ("");
	g_string_append_printf (
		stylesheet,
		"body {\n"
		"  font-family: '%s';\n"
		"  font-size: %dpt;\n"
		"  font-weight: %d;\n"
		"  font-style: %s;\n",
		pango_font_description_get_family (vw),
		pango_font_description_get_size (vw) / PANGO_SCALE,
		pango_font_description_get_weight (vw),
		styles[pango_font_description_get_style (vw)]);

	if (widget->priv->aliasing_settings != NULL)
		aa = g_settings_get_string (
			widget->priv->aliasing_settings, "antialiasing");

	if (g_strcmp0 (aa, "none") == 0)
		smoothing = "none";
	else if (g_strcmp0 (aa, "grayscale") == 0)
		smoothing = "antialiased";
	else if (g_strcmp0 (aa, "rgba") == 0)
		smoothing = "subpixel-antialiased";

	if (smoothing != NULL)
		g_string_append_printf (
			stylesheet,
			" -webkit-font-smoothing: %s;\n",
			smoothing);

	g_free (aa);

	g_string_append (stylesheet, "}\n");

	g_string_append_printf (
		stylesheet,
		"pre,code,.pre {\n"
		"  font-family: '%s';\n"
		"  font-size: %dpt;\n"
		"  font-weight: %d;\n"
		"  font-style: %s;\n"
		"}",
		pango_font_description_get_family (ms),
		pango_font_description_get_size (ms) / PANGO_SCALE,
		pango_font_description_get_weight (ms),
		styles[pango_font_description_get_style (ms)]);

	context = gtk_widget_get_style_context (GTK_WIDGET (widget));
	gtk_style_context_get_style (
		context,
		"link-color", &link,
		"visited-link-color", &visited,
		NULL);

	if (link == NULL) {
		link = g_slice_new0 (GdkColor);
		link->blue = G_MAXINT16;
	}

	if (visited == NULL) {
		visited = g_slice_new0 (GdkColor);
		visited->red = G_MAXINT16;
	}

	g_string_append_printf (
		stylesheet,
		"a {\n"
		"  color: #%06x;\n"
		"}\n"
		"a:visited {\n"
		"  color: #%06x;\n"
		"}\n",
		e_color_to_value (link),
		e_color_to_value (visited));

	/* See bug #689777 for details */
	g_string_append (
		stylesheet,
		"p,pre,code,address {\n"
		"  margin: 0;\n"
		"}\n"
		"h1,h2,h3,h4,h5,h6 {\n"
		"  margin-top: 0.2em;\n"
		"  margin-bottom: 0.2em;\n"
		"}\n");

	g_string_append(
		stylesheet,
		"ol,ul "
		"{\n"
		"  -webkit-margin-before: 0em; \n"
		"  -webkit-margin-after: 0em; \n"
		"}\n");

	g_string_append(
		stylesheet,
		"blockquote "
		"{\n"
		"  -webkit-margin-before: 0em; \n"
		"  -webkit-margin-after: 0em; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"blockquote[type=cite] "
		"{\n"
		"  padding: 0.0ex 0ex;\n"
		"  margin: 0ex;\n"
		"  -webkit-margin-start: 0em; \n"
		"  -webkit-margin-end : 0em; \n"
		"  color: #737373 !important;\n"
		"}\n");

	g_string_append (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  padding: 0.4ex 1ex;\n"
		"  margin: 1ex;\n"
		"  border-width: 0px 2px 0px 2px;\n"
		"  border-style: none solid none solid;\n"
		"  border-radius: 2px;\n"
		"}\n");

	/* Block quote border colors are borrowed from Thunderbird. */

	g_string_append (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: rgb(114,159,207);\n"  /* Sky Blue 1 */
		"}\n");

	g_string_append (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: rgb(173,127,168);\n"  /* Plum 1 */
		"}\n");

	g_string_append (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: rgb(138,226,52);\n"  /* Chameleon 1 */
		"}\n");

	g_string_append (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: rgb(252,175,62);\n"  /* Orange 1 */
		"}\n");

	g_string_append (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: rgb(233,185,110);\n"  /* Chocolate 1 */
		"}\n");

	gdk_color_free (link);
	gdk_color_free (visited);

	base64 = g_base64_encode ((guchar *) stylesheet->str, stylesheet->len);
	g_string_free (stylesheet, TRUE);

	stylesheet = g_string_new ("data:text/css;charset=utf-8;base64,");
	g_string_append (stylesheet, base64);
	g_free (base64);

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (widget));
	g_object_set (
		G_OBJECT (settings),
		"default-font-size", pango_font_description_get_size (vw) / PANGO_SCALE,
		"default-font-family", pango_font_description_get_family (vw),
		"monospace-font-family", pango_font_description_get_family (ms),
		"default-monospace-font-size", (pango_font_description_get_size (ms) / PANGO_SCALE),
		"user-stylesheet-uri", stylesheet->str,
		NULL);

	g_string_free (stylesheet, TRUE);

	pango_font_description_free (ms);
	pango_font_description_free (vw);
}

/**
 * e_editor_widget_get_element_under_mouse_click:
 * @widget: an #EEditorWidget
 *
 * Returns DOM element, that was clicked on.
 *
 * Returns: DOM element on that was clicked.
 */
WebKitDOMElement *
e_editor_widget_get_element_under_mouse_click (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), NULL);

	return widget->priv->element_under_mouse;
}

/**
 * e_editor_widget_check_magic_links
 * @widget: an #EEditorWidget
 * @include_space: If TRUE the pattern for link expects space on end
 *
 * Check if actual selection in given editor is link. If so, it is surrounded
 * with ANCHOR element.
 */
void
e_editor_widget_check_magic_links (EEditorWidget *widget,
				   gboolean include_space)
{
	WebKitDOMRange *range;

	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	range = editor_widget_get_dom_range (widget);
	editor_widget_check_magic_links (widget, range, include_space, NULL);
}

static CamelMimePart *
e_editor_widget_add_inline_image_from_element (EEditorWidget *widget,
                                               WebKitDOMElement *element,
                                               const gchar *attribute)
{
	CamelStream *stream;
	CamelDataWrapper *wrapper;
	CamelMimePart *part = NULL;
	gsize decoded_size;
	gssize size;
	gchar *mime_type = NULL;
	gchar *element_src, *cid, *name;
	const gchar *base64_encoded_data;
	guchar *base64_decoded_data;

	if (!WEBKIT_DOM_IS_ELEMENT (element)) {
		return NULL;
	}

	element_src = webkit_dom_element_get_attribute (
		WEBKIT_DOM_ELEMENT (element), attribute);

	base64_encoded_data = strstr (element_src, ";base64,");
	if (!base64_encoded_data)
		goto out;

	mime_type = g_strndup (
		element_src + 5,
		base64_encoded_data - (strstr (element_src, "data:") + 5));

	/* Move to actual data */
	base64_encoded_data += 8;

	base64_decoded_data = g_base64_decode (base64_encoded_data, &decoded_size);

	stream = camel_stream_mem_new ();
	size = camel_stream_write (
		stream, (gchar *) base64_decoded_data, decoded_size, NULL, NULL);

	if (size == -1)
		goto out;

	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream_sync (
		wrapper, stream, NULL, NULL);
	g_object_unref (stream);

	camel_data_wrapper_set_mime_type (wrapper, mime_type);

	part = camel_mime_part_new ();
	camel_medium_set_content (CAMEL_MEDIUM (part), wrapper);
	g_object_unref (wrapper);

	cid = camel_header_msgid_generate ();
	camel_mime_part_set_content_id (part, cid);
	name = webkit_dom_element_get_attribute (element, "data-name");
	camel_mime_part_set_filename (part, name);
	g_free (name);
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_BASE64);
out:
	g_free (mime_type);
	g_free (element_src);
	g_free (base64_decoded_data);

	return part;
}

GList *
e_editor_widget_get_parts_for_inline_images (EEditorWidget *widget)
{
	GHashTable *added;
	GList *parts = NULL;
	gint length, ii;
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW  (widget));
	list = webkit_dom_document_query_selector_all (document, "img[data-inline]", NULL);

	length = webkit_dom_node_list_get_length (list);
	if (length == 0)
		return parts;

	added = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
	for (ii = 0; ii < length; ii++) {
		CamelMimePart *part;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "src");

		if (!g_hash_table_lookup (added, src)) {
			part = e_editor_widget_add_inline_image_from_element (
				widget, WEBKIT_DOM_ELEMENT (node), "src");
			parts = g_list_append (parts, part);
			g_hash_table_insert (
				added, src, (gpointer) camel_mime_part_get_content_id (part));
		}
		g_free (src);
	}

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		const gchar *id;
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "src");

		if ((id = g_hash_table_lookup (added, src)) != NULL) {
			gchar *cid = g_strdup_printf ("cid:%s", id);
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (node), "src", cid, NULL);
			g_free (cid);
		}
		g_free (src);
	}

	list = webkit_dom_document_query_selector_all (
		document, "[data-inline][background]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		CamelMimePart *part;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "background");

		if (!g_hash_table_lookup (added, src)) {
			part = e_editor_widget_add_inline_image_from_element (
				widget, WEBKIT_DOM_ELEMENT (node), "background");
			if (part) {
				parts = g_list_append (parts, part);
				g_hash_table_insert (
					added, src,
					(gpointer) camel_mime_part_get_content_id (part));
			}
		}
		g_free (src);
	}

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "background");
		const gchar *id;

		if ((id = g_hash_table_lookup (added, src)) != NULL) {
			gchar *cid = g_strdup_printf ("cid:%s", id);
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (node), "background", cid, NULL);
			g_free (cid);
		}
		g_free (src);
	}

	g_hash_table_destroy (added);

	return parts;
}

/**
 * e_editor_widget_add_inline_image_from_mime_part:
 * @composer: a composer object
 * @part: a CamelMimePart containing image data
 *
 * This adds the mime part @part to @composer as an inline image.
 **/
void
e_editor_widget_add_inline_image_from_mime_part (EEditorWidget *widget,
                                                 CamelMimePart *part)
{
	CamelDataWrapper *dw;
	CamelStream *stream;
	GByteArray *byte_array;
	gchar *src, *base64_encoded, *mime_type, *cid_src;
	const gchar *cid, *name;

	stream = camel_stream_mem_new ();
	dw = camel_medium_get_content (CAMEL_MEDIUM (part));
	g_return_if_fail (dw);

	mime_type = camel_data_wrapper_get_mime_type (dw);
	camel_data_wrapper_decode_to_stream_sync (dw, stream, NULL, NULL);
	camel_stream_close (stream, NULL, NULL);

	byte_array = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (stream));

	if (!byte_array->data)
		return;

	base64_encoded = g_base64_encode ((const guchar *) byte_array->data, byte_array->len);

	name = camel_mime_part_get_filename (part);
	/* Insert file name before new src */
	src = g_strconcat (name, ";data:", mime_type, ";base64,", base64_encoded, NULL);

	cid = camel_mime_part_get_content_id (part);
	if (!cid) {
		camel_mime_part_set_content_id (part, NULL);
		cid = camel_mime_part_get_content_id (part);
	}
	cid_src = g_strdup_printf ("cid:%s", cid);

	g_hash_table_insert (widget->priv->inline_images, cid_src, src);

	g_free (base64_encoded);
	g_free (mime_type);
	g_object_unref (stream);
}
