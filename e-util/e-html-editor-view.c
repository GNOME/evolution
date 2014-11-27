/*
 * e-html-editor-view.c
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

#include "e-html-editor-view.h"
#include "e-html-editor.h"
#include "e-emoticon-chooser.h"
#include "e-misc-utils.h"

#include "web-extensions/e-html-editor-web-extension-names.h"

#include <e-util/e-util.h>
#include <e-util/e-marshal.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#define E_HTML_EDITOR_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_VIEW, EHTMLEditorViewPrivate))

#define UNICODE_ZERO_WIDTH_SPACE "\xe2\x80\x8b"
#define UNICODE_NBSP "\xc2\xa0"

#define URL_PATTERN \
	"((([A-Za-z]{3,9}:(?:\\/\\/)?)(?:[\\-;:&=\\+\\$,\\w]+@)?" \
	"[A-Za-z0-9\\.\\-]+|(?:www\\.|[\\-;:&=\\+\\$,\\w]+@)" \
	"[A-Za-z0-9\\.\\-]+)((?:\\/[\\+~%\\/\\.\\w\\-]*)?\\?" \
	"?(?:[\\-\\+=&;%@\\.\\w]*)#?(?:[\\.\\!\\/\\\\w]*))?)"

#define URL_PATTERN_SPACE URL_PATTERN "\\s"

#define QUOTE_SYMBOL ">"

/* Keep synchronized with the same value in EHTMLEditorSelection */
#define SPACES_PER_LIST_LEVEL 8
#define TAB_LENGTH 8

/**
 * EHTMLEditorView:
 *
 * The #EHTMLEditorView is a WebKit-based rich text editor. The view itself
 * only provides means to configure global behavior of the editor. To work
 * with the actual content, current cursor position or current selection,
 * use #EHTMLEditorSelection object.
 */

struct _EHTMLEditorViewPrivate {
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

	EHTMLEditorSelection *selection;

	WebKitDOMElement *element_under_mouse;

	GHashTable *inline_images;

	GSettings *mail_settings;
	GSettings *font_settings;
	GSettings *aliasing_settings;

	gboolean convert_in_situ;
	gboolean body_input_event_removed;
	gboolean is_message_from_draft;
	gboolean is_message_from_edit_as_new;
	gboolean is_message_from_selection;
	gboolean remove_initial_input_line;

	GDBusProxy *web_extension;
	guint web_extension_watch_name_id;

	GHashTable *old_settings;

	GdkEventKey *key_event;

	GQueue *post_reload_operations;
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
	PASTE_PRIMARY_CLIPBOARD,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef void (*PostReloadOperationFunc) (EHTMLEditorView *view, gpointer data);

typedef struct {
	PostReloadOperationFunc func;
	gpointer data;
	GDestroyNotify data_free_func;
} PostReloadOperation;

G_DEFINE_TYPE_WITH_CODE (
	EHTMLEditorView,
	e_html_editor_view,
	WEBKIT_TYPE_WEB_VIEW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static void
html_editor_view_queue_post_reload_operation (EHTMLEditorView *view,
                                            PostReloadOperationFunc func,
                                            gpointer data,
                                            GDestroyNotify data_free_func)
{
	PostReloadOperation *op;

	g_return_if_fail (func != NULL);

	if (view->priv->post_reload_operations == NULL)
		view->priv->post_reload_operations = g_queue_new ();

	op = g_new0 (PostReloadOperation, 1);
	op->func = func;
	op->data = data;
	op->data_free_func = data_free_func;

	g_queue_push_head (view->priv->post_reload_operations, op);
}

static void
html_editor_view_can_redo_cb (WebKitWebView *webkit_web_view,
                              GAsyncResult *result,
                              EHTMLEditorView *view)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	if (view->priv->can_redo != value) {
		view->priv->can_redo = value;
		g_object_notify (G_OBJECT (view), "can-redo");
	}
}

static gboolean
html_editor_view_can_redo (EHTMLEditorView *view)
{
	return view->priv->can_redo;
}

static void
html_editor_view_can_undo_cb (WebKitWebView *webkit_web_view,
                              GAsyncResult *result,
                              EHTMLEditorView *view)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	if (view->priv->can_undo != value) {
		view->priv->can_undo = value;
		g_object_notify (G_OBJECT (view), "can-undo");
	}
}

static gboolean
html_editor_view_can_undo (EHTMLEditorView *view)
{
	return view->priv->can_undo;
}

static void
html_editor_view_user_changed_contents_cb (EHTMLEditorView *view)
{
	WebKitWebView *web_view;

	web_view = WEBKIT_WEB_VIEW (view);

	e_html_editor_view_set_changed (view, TRUE);

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_REDO,
		NULL, /* cancellable */
		(GAsyncReadyCallback) html_editor_view_can_redo_cb,
		view);

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_UNDO,
		NULL, /* cancellable */
		(GAsyncReadyCallback) html_editor_view_can_undo_cb,
		view);
}

static void
html_editor_view_can_copy_cb (WebKitWebView *webkit_web_view,
                              GAsyncResult *result,
                              EHTMLEditorView *view)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	if (view->priv->can_copy != value) {
		view->priv->can_copy = value;
		g_object_notify (G_OBJECT (view), "can-copy");
	}
}

static gboolean
html_editor_view_can_copy (EHTMLEditorView *view)
{
	return view->priv->can_copy;
}

static void
html_editor_view_can_paste_cb (WebKitWebView *webkit_web_view,
                              GAsyncResult *result,
                              EHTMLEditorView *view)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	if (view->priv->can_paste != value) {
		view->priv->can_paste = value;
		g_object_notify (G_OBJECT (view), "can-paste");
	}
}

static gboolean
html_editor_view_can_paste (EHTMLEditorView *view)
{
	return view->priv->can_paste;
}

static void
html_editor_view_can_cut_cb (WebKitWebView *webkit_web_view,
                             GAsyncResult *result,
                             EHTMLEditorView *view)
{
	gboolean value;

	value = webkit_web_view_can_execute_editing_command_finish (
		webkit_web_view, result, NULL);

	if (view->priv->can_cut != value) {
		view->priv->can_cut = value;
		g_object_notify (G_OBJECT (view), "can-cut");
	}
}

static gboolean
html_editor_view_can_cut (EHTMLEditorView *view)
{
	return view->priv->can_cut;
}

static void
html_editor_view_selection_changed_cb (EHTMLEditorView *view)
{
	WebKitWebView *web_view;

	web_view = WEBKIT_WEB_VIEW (view);

	/* When the webview is being (re)loaded, the document is in an
	 * inconsistant state and there is no selection, so don't propagate
	 * the signal further to EHTMLEditorSelection and others and wait until
	 * the load is finished. */
	if (view->priv->reload_in_progress) {
		g_signal_stop_emission_by_name (view, "selection-changed");
		return;
	}

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_COPY,
		NULL, /* cancellable */
		(GAsyncReadyCallback) html_editor_view_can_copy_cb,
		view);

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_CUT,
		NULL, /* cancellable */
		(GAsyncReadyCallback) html_editor_view_can_cut_cb,
		view);

	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (web_view),
		WEBKIT_EDITING_COMMAND_PASTE,
		NULL, /* cancellable */
		(GAsyncReadyCallback) html_editor_view_can_paste_cb,
		view);
}

static gboolean
html_editor_view_should_show_delete_interface_for_element (EHTMLEditorView *view,
                                                           WebKitDOMHTMLElement *element)
{
	return FALSE;
}

void
e_html_editor_view_force_spell_check_for_current_paragraph (EHTMLEditorView *view)
{
/* FIXME WK2
	if (!view->priv->inline_spelling)
		return;
*/
	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EHTMLEditorSelection and here as well
	 * when we are moving with caret */
/* FIXME WK2
	g_signal_handlers_block_by_func (
		view, html_editor_view_selection_changed_cb, NULL);
	e_html_editor_selection_block_selection_changed (selection);
*/

	/* Unblock the callbacks */
/* FIXME WK2
	g_signal_handlers_unblock_by_func (
		view, html_editor_view_selection_changed_cb, NULL);
	e_html_editor_selection_unblock_selection_changed (selection);
*/
}

static void
refresh_spell_check (EHTMLEditorView *view,
                     gboolean enable_spell_check)
{
	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EHTMLEditorSelection and here as well
	 * when we are moving with caret */
/* FIXME WK2
	g_signal_handlers_block_by_func (
		view, html_editor_view_selection_changed_cb, NULL);
	e_html_editor_selection_block_selection_changed (selection);
*/
	/* Unblock the callbacks */
/*
	g_signal_handlers_unblock_by_func (
		view, html_editor_view_selection_changed_cb, NULL);
	e_html_editor_selection_unblock_selection_changed (selection);
*/
}

void
e_html_editor_view_turn_spell_check_off (EHTMLEditorView *view)
{
	refresh_spell_check (view, FALSE);
}

void
e_html_editor_view_force_spell_check (EHTMLEditorView *view)
{
	if (view->priv->inline_spelling)
		refresh_spell_check (view, TRUE);
}

static void
set_base64_to_element_attribute (EHTMLEditorView *view,
                                 WebKitDOMElement *element,
                                 const gchar *attribute)
{
	gchar *attribute_value;
	const gchar *base64_src;

	attribute_value = webkit_dom_element_get_attribute (element, attribute);

	if ((base64_src = g_hash_table_lookup (view->priv->inline_images, attribute_value)) != NULL) {
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
change_cid_images_src_to_base64 (EHTMLEditorView *view)
{
	gint ii, length;
	WebKitDOMDocument *document;
	WebKitDOMElement *document_element;
	WebKitDOMNamedNodeMap *attributes;
	WebKitDOMNodeList *list;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	document_element = webkit_dom_document_get_document_element (document);

	list = webkit_dom_document_query_selector_all (document, "img[src^=\"cid:\"]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		set_base64_to_element_attribute (view, WEBKIT_DOM_ELEMENT (node), "src");
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
					view, WEBKIT_DOM_ELEMENT (node), attribute_ns);
			}

			g_object_unref (list);
			g_free (attribute_ns);
			g_free (selector);
		}
		g_free (name);
	}
	g_object_unref (attributes);

	list = webkit_dom_document_query_selector_all (
		document, "[background^=\"cid:\"]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		set_base64_to_element_attribute (
			view, WEBKIT_DOM_ELEMENT (node), "background");
	}
	g_object_unref (list);
	g_hash_table_remove_all (view->priv->inline_images);
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

static gboolean
is_return_key (GdkEventKey *event)
{
	return (
	    (event->keyval == GDK_KEY_Return) ||
	    (event->keyval == GDK_KEY_Linefeed) ||
	    (event->keyval == GDK_KEY_KP_Enter));
}

static void
html_editor_view_check_magic_links (EHTMLEditorView *view,
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
		WebKitDOMDocument *document;
		WebKitDOMNode *url_text_node_clone;
		WebKitDOMText *url_text_node;
		WebKitDOMElement *anchor;
		const gchar* url_text;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

		if (!return_pressed)
			e_html_editor_selection_save_caret_position (
				e_html_editor_view_get_selection (view));

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
			e_html_editor_selection_restore_caret_position (
				e_html_editor_view_get_selection (view));

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

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	EHTMLEditorView *view;
	gchar *content_type;
	gchar *name;
	EEmoticon *emoticon;
};

static LoadContext *
emoticon_load_context_new (EHTMLEditorView *view,
                           EEmoticon *emoticon)
{
	LoadContext *load_context;

	load_context = g_slice_new0 (LoadContext);
	load_context->view = view;
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
	EHTMLEditorView *view = load_context->view;
	EEmoticon *emoticon = load_context->emoticon;
	GError *error = NULL;
	gboolean misplaced_selection = FALSE, empty = FALSE;
	gchar *html, *node_text = NULL, *mime_type, *content;
	gchar *base64_encoded, *output, *data;
	const gchar *emoticon_start;
	GFileInputStream *input_stream;
	GOutputStream *output_stream;
	gssize size;
	WebKitDOMDocument *document;
	WebKitDOMElement *span, *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *node, *insert_before, *prev_sibling, *next_sibling;
	WebKitDOMNode *selection_end_marker_parent;
	WebKitDOMRange *range;

	input_stream = g_file_read_finish (file, result, &error);
	g_return_if_fail (!error && input_stream);

	output_stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

	size = g_output_stream_splice (
		output_stream, G_INPUT_STREAM (input_stream),
		G_OUTPUT_STREAM_SPLICE_NONE, NULL, &error);

	if (error || (size == -1))
		goto out;

	e_html_editor_selection_save (e_html_editor_view_get_selection (view));

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

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

	mime_type = g_content_type_get_mime_type (load_context->content_type);
	range = dom_get_range (view);
	node = webkit_dom_range_get_end_container (range, NULL);
	if (WEBKIT_DOM_IS_TEXT (node))
		node_text = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (node));

	data = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output_stream));
	base64_encoded = g_base64_encode ((const guchar *) data, size);
	output = g_strconcat ("data:", mime_type, ";base64,", base64_encoded, NULL);

	content = webkit_dom_node_get_text_content (selection_end_marker_parent);
	empty = !*content || (g_strcmp0 (content, UNICODE_ZERO_WIDTH_SPACE) == 0);
	g_free (content);

	/* Insert span with image representation and another one with text
	 * represetation and hide/show them dependant on active composer mode */
	/* &#8203 == UNICODE_ZERO_WIDTH_SPACE */
	html = g_strdup_printf (
		"<span class=\"-x-evo-smiley-wrapper -x-evo-resizable-wrapper\">"
		"<img src=\"%s\" alt=\"%s\" x-evo-smiley=\"%s\" "
		"class=\"-x-evo-smiley-img\" data-inline data-name=\"%s\"/>"
		"<span class=\"-x-evo-smiley-text\" style=\"display: none;\">%s"
		"</span></span>%s",
		output, emoticon ? emoticon->text_face : "", emoticon->icon_name,
		load_context->name, emoticon ? emoticon->text_face : "",
		empty ? "&#8203;" : "");

	span = webkit_dom_document_create_element (document, "SPAN", NULL);

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

	webkit_dom_html_element_set_outer_html (
		WEBKIT_DOM_HTML_ELEMENT (span), html, NULL);

	if (node_text) {
		emoticon_start = g_utf8_strrchr (
			node_text, -1, g_utf8_get_char (emoticon->text_face));
		if (emoticon_start) {
			webkit_dom_character_data_delete_data (
				WEBKIT_DOM_CHARACTER_DATA (node),
				g_utf8_strlen (node_text, -1) - strlen (emoticon_start),
				strlen (emoticon->text_face),
				NULL);
		}
	}

	e_html_editor_selection_restore (
		e_html_editor_view_get_selection (view));

	e_html_editor_view_set_changed (view, TRUE);

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
e_html_editor_view_insert_smiley (EHTMLEditorView *view,
                                  EEmoticon *emoticon)
{
	GFile *file;
	gchar *filename_uri;
	LoadContext *load_context;

	filename_uri = e_emoticon_get_uri (emoticon);
	g_return_if_fail (filename_uri != NULL);

	load_context = emoticon_load_context_new (view, emoticon);

	file = g_file_new_for_uri (filename_uri);
	g_file_query_info_async (
		file,  "standard::*", G_FILE_QUERY_INFO_NONE,
		G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) emoticon_query_info_async_cb, load_context);

	g_free (filename_uri);
	g_object_unref (file);
}

static void
html_editor_view_check_magic_smileys (EHTMLEditorView *view,
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
		e_html_editor_view_insert_smiley (view, (EEmoticon *) emoticon);
	}

	g_free (node_text);
}

static void
insert_and_convert_html_into_selection (EHTMLEditorView *view,
                                        const gchar *text,
                                        gboolean is_html)
{
	GDBusProxy *web_extension;

	g_return_if_fail (view != NULL);

	if (!text || !*text)
		return;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"DOMInsertAndConvertHTMLIntoSelection",
		g_variant_new (
			"(tsb)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			text,
			is_html),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
clipboard_text_received_for_paste_as_text (GtkClipboard *clipboard,
                                           const gchar *text,
                                           EHTMLEditorView *view)
{
	insert_and_convert_html_into_selection (view, text, TRUE);
}

static void
clipboard_text_received (GtkClipboard *clipboard,
                         const gchar *text,
                         EHTMLEditorView *view)
{
	GDBusProxy *web_extension;

	g_return_if_fail (view != NULL);

	if (!text || !*text)
		return;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"DOMQuoteAndInsertTextIntoSelection",
		g_variant_new (
			"(tsb)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			text),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
html_editor_view_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHANGED:
			e_html_editor_view_set_changed (
				E_HTML_EDITOR_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_HTML_MODE:
			e_html_editor_view_set_html_mode (
				E_HTML_EDITOR_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_INLINE_SPELLING:
			e_html_editor_view_set_inline_spelling (
				E_HTML_EDITOR_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_LINKS:
			e_html_editor_view_set_magic_links (
				E_HTML_EDITOR_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_SMILEYS:
			e_html_editor_view_set_magic_smileys (
				E_HTML_EDITOR_VIEW (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
html_editor_view_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_COPY:
			g_value_set_boolean (
				value, html_editor_view_can_copy (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_CAN_CUT:
			g_value_set_boolean (
				value, html_editor_view_can_cut (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_CAN_PASTE:
			g_value_set_boolean (
				value, html_editor_view_can_paste (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_CAN_REDO:
			g_value_set_boolean (
				value, html_editor_view_can_redo (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_CAN_UNDO:
			g_value_set_boolean (
				value, html_editor_view_can_undo (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_CHANGED:
			g_value_set_boolean (
				value, e_html_editor_view_get_changed (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_HTML_MODE:
			g_value_set_boolean (
				value, e_html_editor_view_get_html_mode (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_INLINE_SPELLING:
			g_value_set_boolean (
				value, e_html_editor_view_get_inline_spelling (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_MAGIC_LINKS:
			g_value_set_boolean (
				value, e_html_editor_view_get_magic_links (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_MAGIC_SMILEYS:
			g_value_set_boolean (
				value, e_html_editor_view_get_magic_smileys (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_SPELL_CHECKER:
			g_value_set_object (
				value, e_html_editor_view_get_spell_checker (
				E_HTML_EDITOR_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
html_editor_view_dispose (GObject *object)
{
	EHTMLEditorViewPrivate *priv;

	priv = E_HTML_EDITOR_VIEW_GET_PRIVATE (object);

	g_clear_object (&priv->selection);

	if (priv->aliasing_settings != NULL) {
		g_signal_handlers_disconnect_by_data (priv->aliasing_settings, object);
		g_object_unref (priv->aliasing_settings);
		priv->aliasing_settings = NULL;
	}

	if (priv->font_settings != NULL) {
		g_signal_handlers_disconnect_by_data (priv->font_settings, object);
		g_object_unref (priv->font_settings);
		priv->font_settings = NULL;
	}

	if (priv->mail_settings != NULL) {
		g_signal_handlers_disconnect_by_data (priv->mail_settings, object);
		g_object_unref (priv->mail_settings);
		priv->mail_settings = NULL;
	}

	if (priv->web_extension_watch_name_id > 0) {
		g_bus_unwatch_name (priv->web_extension_watch_name_id);
		priv->web_extension_watch_name_id = 0;
	}

	g_clear_object (&priv->selection);
	g_clear_object (&priv->web_extension);

	g_hash_table_remove_all (priv->inline_images);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_html_editor_view_parent_class)->dispose (object);
}

static void
html_editor_view_finalize (GObject *object)
{
	EHTMLEditorViewPrivate *priv;

	priv = E_HTML_EDITOR_VIEW_GET_PRIVATE (object);

	g_hash_table_destroy (priv->inline_images);

	if (priv->old_settings) {
		g_hash_table_destroy (priv->old_settings);
		priv->old_settings = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_html_editor_view_parent_class)->finalize (object);
}

static void
html_editor_view_constructed (GObject *object)
{

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_html_editor_view_parent_class)->constructed (object);
/* FIXME WK2
	WebKitSettings *web_settings;

	web_settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (object));
	g_object_set (
		G_OBJECT (web_settings),
		"enable-dom-paste", TRUE,
		"enable-file-access-from-file-uris", TRUE,
		"enable-spell-checking", TRUE,
		NULL);
*/
}

static void
html_editor_view_save_element_under_mouse_click (GtkWidget *widget)
{
	gint x, y;
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;
	EHTMLEditorView *view;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (widget));

	device_manager = gdk_display_get_device_manager (
		gtk_widget_get_display (GTK_WIDGET (widget)));
	pointer = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (
		gtk_widget_get_window (GTK_WIDGET (widget)), pointer, &x, &y, NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	element = webkit_dom_document_element_from_point (document, x, y);

	view = E_HTML_EDITOR_VIEW (widget);
	view->priv->element_under_mouse = element;
}

static gboolean
html_editor_view_button_press_event (GtkWidget *widget,
                                     GdkEventButton *event)
{
	gboolean event_handled;

	if (event->button == 2) {
		/* Middle click paste */
		g_signal_emit (widget, signals[PASTE_PRIMARY_CLIPBOARD], 0);
		event_handled = TRUE;
	} else if (event->button == 3) {
		html_editor_view_save_element_under_mouse_click (widget);
		g_signal_emit (
			widget, signals[POPUP_EVENT],
			0, event, &event_handled);
	} else {
		event_handled = FALSE;
	}

	if (event_handled)
		return TRUE;

	/* Chain up to parent's button_press_event() method. */
	return GTK_WIDGET_CLASS (e_html_editor_view_parent_class)->
		button_press_event (widget, event);
}

static gboolean
html_editor_view_button_release_event (GtkWidget *widget,
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
	return GTK_WIDGET_CLASS (e_html_editor_view_parent_class)->
		button_release_event (widget, event);
}

static gboolean
html_editor_view_key_press_event (GtkWidget *widget,
                                  GdkEventKey *event)
{
	EHTMLEditorView *view = E_HTML_EDITOR_VIEW (widget);

	view->priv->key_event = event;

	if (event->keyval == GDK_KEY_Menu) {
		gboolean event_handled;

		html_editor_view_save_element_under_mouse_click (widget);
		g_signal_emit (
			widget, signals[POPUP_EVENT],
			0, event, &event_handled);

		return event_handled;
	}

	if (event->keyval == GDK_KEY_Tab)
		return e_html_editor_view_exec_command (
			view, E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, "\t");

	if ((event->keyval == GDK_KEY_Control_L) ||
	    (event->keyval == GDK_KEY_Control_R)) {

		html_editor_view_set_links_active (view, TRUE);
	}

	if (is_return_key (event)) {
		EHTMLEditorSelection *selection;
		EHTMLEditorSelectionBlockFormat format;

		selection = e_html_editor_view_get_selection (view);
		/* When user presses ENTER in a citation block, WebKit does
		 * not break the citation automatically, so we need to use
		 * the special command to do it. */
		if (e_html_editor_selection_is_citation (selection)) {
			remove_input_event_listener_from_body (view);
			return (insert_new_line_into_citation (view, "")) ? TRUE : FALSE;
		}

		/* When the return is pressed in a H1-6 element, WebKit doesn't
		 * continue with the same element, but creates normal paragraph,
		 * so we have to unset the bold font. */
		format = e_html_editor_selection_get_block_format (selection);
		if (format >= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H1 &&
		    format <= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H6)
			e_html_editor_selection_set_bold (selection, FALSE);
	}

	if (event->keyval == GDK_KEY_BackSpace) {
		EHTMLEditorSelection *selection;

		selection = e_html_editor_view_get_selection (view);

		/* BackSpace pressed in the beginning of quoted content changes
		 * format to normal and inserts text into body */
		if (e_html_editor_selection_is_collapsed (selection)) {
			e_html_editor_selection_save (selection);
			if (change_quoted_block_to_normal (view)) {
				e_html_editor_selection_restore (selection);
				e_html_editor_view_force_spell_check_for_current_paragraph (view);
				return TRUE;
			}
			e_html_editor_selection_restore (selection);
		} else
			remove_input_event_listener_from_body (view);

		/* BackSpace in indented block decrease indent level by one */
		if (e_html_editor_selection_is_indented (selection)) {
			WebKitDOMElement *caret;
			WebKitDOMNode *prev_sibling;

			caret = e_html_editor_selection_save_caret_position (selection);

			/* Empty text node before caret */
			prev_sibling = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (caret));
			if (prev_sibling && WEBKIT_DOM_IS_TEXT (prev_sibling)) {
				gchar *content;

				content = webkit_dom_node_get_text_content (prev_sibling);
				if (g_strcmp0 (content, "") == 0)
					prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);
				g_free (content);
			}

			if (!prev_sibling) {
				e_html_editor_selection_clear_caret_position_marker (selection);
				e_html_editor_selection_unindent (selection);
				return TRUE;
			} else
				e_html_editor_selection_clear_caret_position_marker (selection);
		}

		if (prevent_from_deleting_last_element_in_body (view))
			return TRUE;
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_html_editor_view_parent_class)->
		key_press_event (widget, event);
}

static gboolean
html_editor_view_key_release_event (GtkWidget *widget,
                                    GdkEventKey *event)
{
	EHTMLEditorView *view;
	EHTMLEditorSelection *selection;

	view = E_HTML_EDITOR_VIEW (widget);
	register_input_event_listener_on_body (view);

	selection = e_html_editor_view_get_selection (view);
	if (!e_html_editor_selection_is_collapsed (selection))
		goto out;

	/* This will fix the structure after the situations where some text
	 * inside the quoted content is selected and afterwards deleted with
	 * BackSpace or Delete. */
	if ((event->keyval == GDK_KEY_BackSpace) ||
	    (event->keyval == GDK_KEY_Delete)) {
		gint level;
		WebKitDOMElement *selection_start_marker, *selection_end_marker;
		WebKitDOMElement *element;
		WebKitDOMDocument *document;
		WebKitDOMNode *node, *parent;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

		e_html_editor_selection_save (selection);
		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		level = get_citation_level (
			WEBKIT_DOM_NODE (selection_start_marker), FALSE);
		if (level == 0)
			goto restore;

		node = webkit_dom_node_get_previous_sibling (
			WEBKIT_DOM_NODE (selection_start_marker));

		if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (node))
			node = webkit_dom_node_get_previous_sibling (node);

		if (node)
			goto restore;

		parent = get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		node = webkit_dom_node_get_previous_sibling (parent);
		if (!node) {
			/* Situation where the start of the selection was in the
			 * multiple quoted content and that start on the beginning
			 * of the citation.
			 *
			 * >
			 * >> |
			 * >> xx|x
			 * */
			node = webkit_dom_node_get_parent_node (parent);
			if (!WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (node))
				goto restore;
			node = webkit_dom_node_get_previous_sibling (node);
			if (!node)
				goto restore;
			if (!WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (webkit_dom_node_get_parent_node (node)))
				goto restore;
		}

		element = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (node), "span.-x-evo-quote-character > br", NULL);
		if (element) {
			WebKitDOMNode *tmp;

			if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (node)) {
				/* We have to select the right block when the selection
				 * started on the end of the citation that is
				 * inside another citation.
				 *
				 * >>|
				 * > xx|x
				 */
				/* <span class="-x-evo-quote-character"> */
				node = webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (element));
				/* <span class="-x-evo-quoted"> */
				node = webkit_dom_node_get_parent_node (node);
				/* right block */
				node = webkit_dom_node_get_parent_node (node);
			}

			webkit_dom_node_append_child (
				node, WEBKIT_DOM_NODE (selection_start_marker), NULL);

			while ((tmp = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker))))
				webkit_dom_node_append_child (node, tmp, NULL);

			webkit_dom_node_insert_before (
				node,
				WEBKIT_DOM_NODE (selection_end_marker),
				webkit_dom_node_get_next_sibling (
					WEBKIT_DOM_NODE (selection_start_marker)),
				NULL);

			webkit_dom_node_append_child (
				node, WEBKIT_DOM_NODE (element), NULL);
			remove_node (parent);
		}
 restore:
		e_html_editor_selection_restore (selection);
	}

	if ((event->keyval == GDK_KEY_Control_L) ||
	    (event->keyval == GDK_KEY_Control_R)) {

		html_editor_view_set_links_active (view, FALSE);
	}
 out:
	/* Chain up to parent's key_release_event() method. */
	return GTK_WIDGET_CLASS (e_html_editor_view_parent_class)->
		key_release_event (widget, event);
}

static void
html_editor_view_paste_as_text (EHTMLEditorView *view)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get_for_display (
		gdk_display_get_default (),
		GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_text (
		clipboard,
		(GtkClipboardTextReceivedFunc) clipboard_text_received_for_paste_as_text,
		view);
}

static void
html_editor_view_paste_clipboard_quoted (EHTMLEditorView *view)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get_for_display (
		gdk_display_get_default (),
		GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_text (
		clipboard,
		(GtkClipboardTextReceivedFunc) clipboard_text_received,
		view);
}

static void
web_extension_proxy_created_cb (GDBusProxy *proxy,
                                GAsyncResult *result,
                                EHTMLEditorView *view)
{
	GError *error = NULL;

	view->priv->web_extension = g_dbus_proxy_new_finish (result, &error);
	if (!view->priv->web_extension) {
		g_warning ("Error creating web extension proxy: %s\n", error->message);
		g_error_free (error);
	}
}

static void
web_extension_appeared_cb (GDBusConnection *connection,
                           const gchar *name,
                           const gchar *name_owner,
                           EHTMLEditorView *view)
{
	g_dbus_proxy_new (
		connection,
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
		G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL,
		name,
		E_HTML_EDITOR_WEB_EXTENSION_OBJECT_PATH,
		E_HTML_EDITOR_WEB_EXTENSION_INTERFACE,
		NULL,
		(GAsyncReadyCallback) web_extension_proxy_created_cb,
		view);
}

static void
web_extension_vanished_cb (GDBusConnection *connection,
                           const gchar *name,
                           EHTMLEditorView *view)
{
	g_clear_object (&view->priv->web_extension);
}

static void
html_editor_view_watch_web_extension (EHTMLEditorView *view)
{
	view->priv->web_extension_watch_name_id =
		g_bus_watch_name (
			G_BUS_TYPE_SESSION,
			E_HTML_EDITOR_WEB_EXTENSION_SERVICE_NAME,
			G_BUS_NAME_WATCHER_FLAGS_NONE,
			(GBusNameAppearedCallback) web_extension_appeared_cb,
			(GBusNameVanishedCallback) web_extension_vanished_cb,
			view,
			NULL);
}

GDBusProxy *
e_html_editor_view_get_web_extension_proxy (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), NULL);

	return view->priv->web_extension;
}

void
e_html_editor_view_call_simple_extension_function (EHTMLEditorView *view,
                                                   const gchar *function)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));
	g_return_if_fail (function && *function);

	if (!view->priv->web_extension)
		return;

	g_dbus_proxy_call (
		view->priv->web_extension,
		function,
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

GVariant *
e_html_editor_view_get_element_attribute (EHTMLEditorView *view,
                                          const gchar *selector,
                                          const gchar *attribute)
{
	if (!view->priv->web_extension)
		return NULL;

	return g_dbus_proxy_call_sync (
		view->priv->web_extension,
		"ElementGetAttributeBySelector",
		g_variant_new (
			"(tss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			selector,
			attribute),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);
}

void
e_html_editor_view_set_element_attribute (EHTMLEditorView *view,
                                          const gchar *selector,
                                          const gchar *attribute,
					  const gchar *value)
{
	if (!view->priv->web_extension)
		return;

	g_dbus_proxy_call (
		view->priv->web_extension,
		"ElementSetAttributeBySelector",
		g_variant_new (
			"(tsss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			selector,
			attribute,
			value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

void
e_html_editor_view_remove_element_attribute (EHTMLEditorView *view,
                                             const gchar *selector,
                                             const gchar *attribute)
{
	if (!view->priv->web_extension)
		return;

	g_dbus_proxy_call (
		view->priv->web_extension,
		"ElementRemoveAttributeBySelector",
		g_variant_new (
			"(tss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			selector,
			attribute),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static GObjectConstructParam*
find_property (guint n_properties,
               GObjectConstructParam* properties,
               GParamSpec* param_spec)
{
	while (n_properties--) {
		if (properties->pspec == param_spec)
			return properties;
		properties++;
	}

	return NULL;
}

static GObject *
html_editor_view_constructor (GType type,
                              guint n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
	GObjectClass* object_class;
	GParamSpec* param_spec;
	GObjectConstructParam *param = NULL;

	object_class = G_OBJECT_CLASS(g_type_class_ref(type));
	g_return_val_if_fail(object_class != NULL, NULL);

	if (construct_properties && n_construct_properties != 0) {
		param_spec = g_object_class_find_property(object_class, "settings");
		if ((param = find_property(n_construct_properties, construct_properties, param_spec)))
			g_value_take_object (param->value, e_web_view_get_default_webkit_settings ());
		param_spec = g_object_class_find_property(object_class, "user-content-manager");
		if ((param = find_property(n_construct_properties, construct_properties, param_spec)))
			g_value_take_object (param->value, webkit_user_content_manager_new ());
	}

	g_type_class_unref (object_class);

	return G_OBJECT_CLASS (e_html_editor_view_parent_class)->constructor(type, n_construct_properties, construct_properties);
}

static void
html_editor_view_initialize_web_context (void)
{
	WebKitWebContext *web_context = webkit_web_context_get_default ();

	webkit_web_context_set_cache_model (
		web_context, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
}

static void
e_html_editor_view_class_init (EHTMLEditorViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorViewPrivate));

	html_editor_view_initialize_web_context ();

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = html_editor_view_constructor;
	object_class->get_property = html_editor_view_get_property;
	object_class->set_property = html_editor_view_set_property;
	object_class->dispose = html_editor_view_dispose;
	object_class->finalize = html_editor_view_finalize;
	object_class->constructed = html_editor_view_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = html_editor_view_button_press_event;
	widget_class->button_release_event = html_editor_view_button_release_event;
	widget_class->key_press_event = html_editor_view_key_press_event;
	widget_class->key_release_event = html_editor_view_key_release_event;

	class->paste_clipboard_quoted = html_editor_view_paste_clipboard_quoted;

	/**
	 * EHTMLEditorView:can-copy
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
	 * EHTMLEditorView:can-cut
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
	 * EHTMLEditorView:can-paste
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
	 * EHTMLEditorView:can-redo
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
	 * EHTMLEditorView:can-undo
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
	 * EHTMLEditorView:changed
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
	 * EHTMLEditorView:html-mode
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
	 * EHTMLEditorView::inline-spelling
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
	 * EHTMLEditorView:magic-links
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
	 * EHTMLEditorView:magic-smileys
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
	 * EHTMLEditorView:spell-checker:
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
	 * EHTMLEditorView:popup-event
	 *
	 * Emitted whenever a context menu is requested.
	 */
	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EHTMLEditorViewClass, popup_event),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__BOXED,
		G_TYPE_BOOLEAN, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
	/**
	 * EHTMLEditorView:paste-primary-clipboad
	 *
	 * Emitted when user presses middle button on EHTMLEditorView
	 */
	signals[PASTE_PRIMARY_CLIPBOARD] = g_signal_new (
		"paste-primary-clipboard",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EHTMLEditorViewClass, paste_primary_clipboard),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
initialize_web_extensions_cb (WebKitWebContext *web_context)
{
	/* Set the web extensions dir before the process is launched */
	webkit_web_context_set_web_extensions_directory (
		web_context, EVOLUTION_WEB_EXTENSIONS_DIR);
}

static void
e_html_editor_settings_changed_cb (GSettings *settings,
				   const gchar *key,
				   EHTMLEditorView *view)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (view->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (view->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (view->priv->old_settings, key);

		e_html_editor_view_update_fonts (view);
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

/**
 * e_html_editor_view_new:
 *
 * Returns a new instance of the editor.
 *
 * Returns: A newly created #EHTMLEditorView. [transfer-full]
 */
EHTMLEditorView *
e_html_editor_view_new (void)
{
	return g_object_new (E_TYPE_HTML_EDITOR_VIEW, NULL);
}

/**
 * e_html_editor_view_get_selection:
 * @view: an #EHTMLEditorView
 *
 * Returns an #EHTMLEditorSelection object which represents current selection or
 * cursor position within the editor document. The #EHTMLEditorSelection allows
 * programmer to manipulate with formatting, selection, styles etc.
 *
 * Returns: An always valid #EHTMLEditorSelection object. The object is owned by
 * the @view and should never be free'd.
 */
EHTMLEditorSelection *
e_html_editor_view_get_selection (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), NULL);

	return view->priv->selection;
}

/**
 * e_html_editor_view_get_changed:
 * @view: an #EHTMLEditorView
 *
 * Whether content of the editor has been changed.
 *
 * Returns: @TRUE when document was changed, @FALSE otherwise.
 */
gboolean
e_html_editor_view_get_changed (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->changed;
}

/**
 * e_html_editor_view_set_changed:
 * @view: an #EHTMLEditorView
 * @changed: whether document has been changed or not
 *
 * Sets whether document has been changed or not. The editor is tracking changes
 * automatically, but sometimes it's necessary to change the dirty flag to force
 * "Save changes" dialog for example.
 */
void
e_html_editor_view_set_changed (EHTMLEditorView *view,
                                gboolean changed)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->changed == changed)
		return;

	view->priv->changed = changed;

	g_object_notify (G_OBJECT (view), "changed");
}

/**
 * e_html_editor_view_get_html_mode:
 * @view: an #EHTMLEditorView
 *
 * Whether the editor is in HTML mode or plain text mode. In HTML mode,
 * more formatting options are avilable an the email is sent as
 * multipart/alternative.
 *
 * Returns: @TRUE when HTML mode is enabled, @FALSE otherwise.
 */
gboolean
e_html_editor_view_get_html_mode (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->html_mode;
}

static gboolean
show_lose_formatting_dialog (EHTMLEditorView *view)
{
	gint result;
	GtkWidget *toplevel, *dialog;
	GtkWindow *parent = NULL;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));

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
		g_object_notify (G_OBJECT (view), "html-mode");
		return FALSE;
	}

	gtk_widget_destroy (dialog);

	return TRUE;
}

static void
html_editor_view_load_changed (EHTMLEditorView *view,
                               WebKitLoadEvent load_event)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;

	if (load_event != WEBKIT_LOAD_FINISHED)
		return;

	/* Dispatch queued operations - as we are using this just for load
	 * operations load just the latest request and throw away the rest. */
	if (view->priv->post_reload_operations &&
	    !g_queue_is_empty (view->priv->post_reload_operations)) {

		PostReloadOperation *op;

		op = g_queue_pop_head (view->priv->post_reload_operations);

		op->func (view, op->data);

		if (op->data_free_func)
			op->data_free_func (op->data);
		g_free (op);

		g_queue_clear (view->priv->post_reload_operations);

		return;
	}

	view->priv->reload_in_progress = FALSE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

	webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (body), "style");
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-message", "", NULL);

	if (view->priv->convert_in_situ) {
		html_editor_convert_view_content (view, NULL);
		view->priv->convert_in_situ = FALSE;

		return;
	}

	put_body_in_citation (document);
	move_elements_to_body (document);
	repair_gmail_blockquotes (document);

	if (webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (body), "data-evo-draft")) {
		/* Restore the selection how it was when the draft was saved */
		e_html_editor_selection_move_caret_into_element (
			document, WEBKIT_DOM_ELEMENT (body));
		e_html_editor_selection_restore (
			e_html_editor_view_get_selection (view));
	}

	/* Register on input event that is called when the content (body) is modified */
	register_input_event_listener_on_body (view);

	if (view->priv->html_mode)
		change_cid_images_src_to_base64 (view);

	if (view->priv->inline_spelling)
		e_html_editor_view_force_spell_check (view);
	else
		e_html_editor_view_turn_spell_check_off (view);
}

/**
 * e_html_editor_view_set_html_mode:
 * @view: an #EHTMLEditorView
 * @html_mode: @TRUE to enable HTML mode, @FALSE to enable plain text mode
 *
 * When switching from HTML to plain text mode, user will be prompted whether
 * he/she really wants to switch the mode and lose all formatting. When user
 * declines, the property is not changed. When they accept, the all formatting
 * is lost.
 */
void
e_html_editor_view_set_html_mode (EHTMLEditorView *view,
                                  gboolean html_mode)
{
	EHTMLEditorSelection *selection;
	gboolean is_from_new_message, converted, edit_as_new, message, convert;
	gboolean reply, hide;
	WebKitDOMElement *blockquote;
	WebKitDOMHTMLElement *body;
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
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
	if (convert && view->priv->html_mode && !html_mode) {
		if (!show_lose_formatting_dialog (view))
			return;

		view->priv->html_mode = html_mode;

		GDBusProxy *web_extension;

		g_return_if_fail (view != NULL);

		web_extension = e_html_editor_view_get_web_extension_proxy (view);
		if (!web_extension)
			return;

		g_dbus_proxy_call (
			web_extension,
			"ConvertWhenChangingComposerMode",
			g_variant_new (
				"(tsb)",
				webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
				text,
				TRUE),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);

		/* Update fonts - in plain text we only want monospace */
		e_html_editor_view_update_fonts (view);

		e_html_editor_view_force_spell_check (view);

		e_html_editor_selection_scroll_to_caret (selection);

		goto out;
	}

	if (html_mode == view->priv->html_mode)
		return;

	view->priv->html_mode = html_mode;

	/* Update fonts - in plain text we only want monospace */
	e_html_editor_view_update_fonts (view);

	blockquote = webkit_dom_document_query_selector (
		document, "blockquote[type|=cite]", NULL);

	if (view->priv->html_mode) {
		if (blockquote)
			e_html_editor_view_dequote_plain_text (view);

		toggle_paragraphs_style (view);
		toggle_smileys (view);
		remove_wrapping_from_document (view);
	} else {
		gchar *plain;

		e_html_editor_selection_save (selection);

		if (blockquote) {
			wrap_paragraphs_in_quoted_content (selection, document);
			quote_plain_text_elements_after_wrapping_in_document (
				document);
		}

		toggle_paragraphs_style (view);
		toggle_smileys (view);
		remove_images (view);
		remove_background_images_in_document (document);

		plain = process_content_for_mode_change (view);

		if (*plain) {
			webkit_dom_html_element_set_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (
					webkit_dom_document_get_document_element (document)),
				plain,
				NULL);
			e_html_editor_selection_restore (selection);
			e_html_editor_view_force_spell_check (view);
		}

		g_free (plain);
	}

 out:
	g_object_notify (G_OBJECT (view), "html-mode");
}

static void
e_html_editor_view_init (EHTMLEditorView *view)
{
	WebKitSettings *settings;
	GSettings *g_settings;
	GSettingsSchema *settings_schema;
	ESpellChecker *checker;
	gchar **languages;
	gchar *comma_separated;

	view->priv = E_HTML_EDITOR_VIEW_GET_PRIVATE (view);

	webkit_web_view_set_editable (WEBKIT_WEB_VIEW (view), TRUE);
	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view));

	view->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);

	/* Override the spell-checker, use our own */
	checker = e_spell_checker_new ();
	webkit_set_text_checker (G_OBJECT (checker));
	g_object_unref (checker);

	/* Don't use CSS when possible to preserve compatibility with older
	 * versions of Evolution or other MUAs */
	e_html_editor_view_exec_command (
		view, E_HTML_EDITOR_VIEW_COMMAND_STYLE_WITH_CSS, "false");

	g_signal_connect (
		view, "user-changed-contents",
		G_CALLBACK (html_editor_view_user_changed_contents_cb), NULL);
	g_signal_connect (
		view, "selection-changed",
		G_CALLBACK (html_editor_view_selection_changed_cb), NULL);
	g_signal_connect (
		view, "should-show-delete-interface-for-element",
		G_CALLBACK (html_editor_view_should_show_delete_interface_for_element), NULL);
	g_signal_connect (
		view, "load-changed",
		G_CALLBACK (html_editor_view_load_changed), NULL);

	e_signal_connect (
		webkit_web_context_get_default (), "initialize-web-extensions",
		G_CALLBACK (initialize_web_extensions_cb), NULL);

	view->priv->selection = g_object_new (
		E_TYPE_HTML_EDITOR_SELECTION,
		"html-editor-view", view,
		NULL);

	html_editor_view_watch_web_extension (view);

	g_settings = e_util_ref_settings ("org.gnome.desktop.interface");
	g_signal_connect (
		g_settings, "changed::font-name",
		G_CALLBACK (e_html_editor_settings_changed_cb), view);
	g_signal_connect (
		g_settings, "changed::monospace-font-name",
		G_CALLBACK (e_html_editor_settings_changed_cb), view);
	view->priv->font_settings = g_settings;

	g_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	view->priv->mail_settings = g_settings;

	/* This schema is optional.  Use if available. */
	settings_schema = g_settings_schema_source_lookup (
		g_settings_schema_source_get_default (),
		"org.gnome.settings-daemon.plugins.xsettings", FALSE);
	if (settings_schema != NULL) {
		g_settings = e_util_ref_settings ("org.gnome.settings-daemon.plugins.xsettings");
		g_signal_connect (
			settings, "changed::antialiasing",
			G_CALLBACK (e_html_editor_settings_changed_cb), view);
		view->priv->aliasing_settings = g_settings;
	}

	view->priv->inline_images = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	e_html_editor_view_update_fonts (view);

	/* Give spell check languages to WebKit */
	languages = e_spell_checker_list_active_languages (checker, NULL);
	comma_separated = g_strjoinv (",", languages);
	g_strfreev (languages);

	g_object_set (
		G_OBJECT (settings),
		"spell-checking-languages", comma_separated,
		NULL);

	g_free (comma_separated);

	view->priv->body_input_event_removed = TRUE;
	view->priv->is_message_from_draft = FALSE;
	view->priv->is_message_from_selection = FALSE;
	view->priv->is_message_from_edit_as_new = FALSE;
	view->priv->remove_initial_input_line = FALSE;
	view->priv->convert_in_situ = FALSE;

	g_object_set (
		G_OBJECT (settings),
		"enable-scripts", FALSE,
		"enable-plugins", FALSE,
		NULL);

	/* Make WebKit think we are displaying a local file, so that it
	 * does not block loading resources from file:// protocol */
	webkit_web_view_load_html (WEBKIT_WEB_VIEW (view), "", "file://");

	html_editor_view_set_links_active (view, FALSE);

}

/**
 * e_html_editor_view_get_inline_spelling:
 * @view: an #EHTMLEditorView
 *
 * Returns whether automatic spellchecking is enabled or not. When enabled,
 * editor will perform spellchecking as user is typing. Otherwise spellcheck
 * has to be run manually from menu.
 *
 * Returns: @TRUE when automatic spellchecking is enabled, @FALSE otherwise.
 */
gboolean
e_html_editor_view_get_inline_spelling (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->inline_spelling;
}

/**
 * e_html_editor_view_set_inline_spelling:
 * @view: an #EHTMLEditorView
 * @inline_spelling: @TRUE to enable automatic spellchecking, @FALSE otherwise
 *
 * Enables or disables automatic spellchecking.
 */
void
e_html_editor_view_set_inline_spelling (EHTMLEditorView *view,
                                        gboolean inline_spelling)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->inline_spelling == inline_spelling)
		return;

	view->priv->inline_spelling = inline_spelling;

	if (inline_spelling)
		e_html_editor_view_force_spell_check (view);
	else
		e_html_editor_view_turn_spell_check_off (view);

	g_object_notify (G_OBJECT (view), "inline-spelling");
}

/**
 * e_html_editor_view_get_magic_links:
 * @view: an #EHTMLEditorView
 *
 * Returns whether automatic links conversion is enabled. When enabled, the editor
 * will automatically convert any HTTP links into clickable HTML links.
 *
 * Returns: @TRUE when magic links are enabled, @FALSE otherwise.
 */
gboolean
e_html_editor_view_get_magic_links (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->magic_links;
}

/**
 * e_html_editor_view_set_magic_links:
 * @view: an #EHTMLEditorView
 * @magic_links: @TRUE to enable magic links, @FALSE to disable them
 *
 * Enables or disables automatic links conversion.
 */
void
e_html_editor_view_set_magic_links (EHTMLEditorView *view,
                                    gboolean magic_links)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->magic_links == magic_links)
		return;

	view->priv->magic_links = magic_links;

	g_object_notify (G_OBJECT (view), "magic-links");
}

/**
 * e_html_editor_view_get_magic_smileys:
 * @view: an #EHTMLEditorView
 *
 * Returns whether automatic conversion of smileys is enabled or disabled. When
 * enabled, the editor will automatically convert text smileys ( :-), ;-),...)
 * into images.
 *
 * Returns: @TRUE when magic smileys are enabled, @FALSE otherwise.
 */
gboolean
e_html_editor_view_get_magic_smileys (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->magic_smileys;
}

/**
 * e_html_editor_view_set_magic_smileys:
 * @view: an #EHTMLEditorView
 * @magic_smileys: @TRUE to enable magic smileys, @FALSE to disable them
 *
 * Enables or disables magic smileys.
 */
void
e_html_editor_view_set_magic_smileys (EHTMLEditorView *view,
                                      gboolean magic_smileys)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->magic_smileys == magic_smileys)
		return;

	view->priv->magic_smileys = magic_smileys;

	g_object_notify (G_OBJECT (view), "magic-smileys");
}

/**
 * e_html_editor_view_get_spell_checker:
 * @view: an #EHTMLEditorView
 *
 * Returns an #ESpellChecker object that is used to perform spellchecking.
 *
 * Returns: An always-valid #ESpellChecker object
 */
ESpellChecker *
e_html_editor_view_get_spell_checker (EHTMLEditorView *view)
{
	return E_SPELL_CHECKER (webkit_get_text_checker ());
}

static gchar *
process_document (EHTMLEditorView *view,
                  const gchar *function)
{
	GDBusProxy *web_extension;
	GVariant *result;

	g_return_val_if_fail (view != NULL, NULL);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return NULL;

	result = g_dbus_proxy_call_sync (
		web_extension,
		function,
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		gchar *value;
		gsize length = 0;

		value = g_variant_dup_string (result, &length);
		g_variant_unref (result);

		return value;
	}

	return NULL;
}

/**
 * e_html_editor_view_get_text_html:
 * @view: an #EHTMLEditorView:
 *
 * Returns processed HTML content of the editor document (with elements attributes
 * used in Evolution composer)
 *
 * Returns: A newly allocated string
 */
gchar *
e_html_editor_view_get_text_html (EHTMLEditorView *view)
{
	return process_document (view, "ProcessDocumentForHTML");
}

/**
 * e_html_editor_view_get_text_html_for_drafts:
 * @view: an #EHTMLEditorView:
 *
 * Returns HTML content of the editor document (without elements attributes
 * used in Evolution composer)
 *
 * Returns: A newly allocated string
 */
gchar *
e_html_editor_view_get_text_html_for_drafts (EHTMLEditorView *view)
{
	return process_document (view, "ProcessDocumentForDrafts");
}

/**
 * e_html_editor_view_get_text_plain:
 * @view: an #EHTMLEditorView
 *
 * Returns plain text content of the @view. The algorithm removes any
 * formatting or styles from the document and keeps only the text and line
 * breaks.
 *
 * Returns: A newly allocated string with plain text content of the document.
 */
gchar *
e_html_editor_view_get_text_plain (EHTMLEditorView *view)
{
	return process_document (view, "ProcessDocumentForPlainText");
}

void
e_html_editor_view_convert_and_insert_plain_text (EHTMLEditorView *view,
                                                  const gchar *text)
{
	insert_and_convert_html_into_selection (view, text, FALSE);
}

void
e_html_editor_view_convert_and_insert_html_to_plain_text (EHTMLEditorView *view,
                                                          const gchar *html)
{
	insert_and_convert_html_into_selection (view, html, TRUE);
}

/**
 * e_html_editor_view_set_text_html:
 * @view: an #EHTMLEditorView
 * @text: HTML code to load into the editor
 *
 * Loads given @text into the editor, destroying any content already present.
 */
void
e_html_editor_view_set_text_html (EHTMLEditorView *view,
                                  const gchar *text)
{
	WebKitLoadStatus status;

	/* It can happen that the view is not ready yet (it is in the middle of
	 * another load operation) so we have to queue the current operation and
	 * redo it again when the view is ready. This was happening when loading
	 * the stuff in EMailSignatureEditor. */
	status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (view));
	if (status != WEBKIT_LOAD_FINISHED) {
		html_editor_view_queue_post_reload_operation (
			view,
			(PostReloadOperationFunc) e_html_editor_view_set_text_html,
			g_strdup (text),
			g_free);
		return;
	}

	view->priv->reload_in_progress = TRUE;

	if (view->priv->is_message_from_draft) {
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (view), text, "file://");
		return;
	}

	if (view->priv->is_message_from_selection && !view->priv->html_mode) {
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (view), text, "file://");
		view->priv->convert_in_situ = TRUE;
		return;
	}

	/* Only convert messages that are in HTML */
	if (!view->priv->html_mode) {
		if (strstr (text, "<!-- text/html -->")) {
			if (!show_lose_formatting_dialog (view)) {
				e_html_editor_view_set_html_mode (view, TRUE);
				webkit_web_view_load_html (
					WEBKIT_WEB_VIEW (view), text, "file://");
				return;
			}
		}
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (view), text, "file://");
		view->priv->convert_in_situ = TRUE;
	} else
		webkit_web_view_load_html (WEBKIT_WEB_VIEW (view), text, "file://");
}

/**
 * e_html_editor_view_set_text_plain:
 * @view: an #EHTMLEditorView
 * @text: A plain text to load into the editor
 *
 * Loads given @text into the editor, destryoing any content already present.
 */
void
e_html_editor_view_set_text_plain (EHTMLEditorView *view,
                                   const gchar *text)
{
	WebKitLoadStatus status;

	/* It can happen that the view is not ready yet (it is in the middle of
	 * another load operation) so we have to queue the current operation and
	 * redo it again when the view is ready. This was happening when loading
	 * the stuff in EMailSignatureEditor. */
	status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (view));
	if (status != WEBKIT_LOAD_FINISHED) {
		html_editor_view_queue_post_reload_operation (
			view,
			(PostReloadOperationFunc) e_html_editor_view_set_text_plain,
			g_strdup (text),
			g_free);
		return;
	}

	view->priv->reload_in_progress = TRUE;

	html_editor_convert_view_content (view, text);
}

/**
 * e_html_editor_view_paste_as_text:
 * @view: an #EHTMLEditorView
 *
 * Pastes current content of clipboard into the editor without formatting
 */
void
e_html_editor_view_paste_as_text (EHTMLEditorView *view)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	html_editor_view_paste_as_text (view);
}

/**
 * e_html_editor_view_paste_clipboard_quoted:
 * @view: an #EHTMLEditorView
 *
 * Pastes current content of clipboard into the editor as quoted text
 */
void
e_html_editor_view_paste_clipboard_quoted (EHTMLEditorView *view)
{
	EHTMLEditorViewClass *class;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	class = E_HTML_EDITOR_VIEW_GET_CLASS (view);
	g_return_if_fail (class->paste_clipboard_quoted != NULL);

	class->paste_clipboard_quoted (view);
}

void
e_html_editor_view_embed_styles (EHTMLEditorView *view)
{
	GDBusProxy *web_extension;
	gchar *stylesheet_uri;
	gchar *stylesheet_content;
	const gchar *stylesheet;
	gsize length;
	WebKitSettings *settings;

	g_return_if_fail (view != NULL);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view));

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

	g_dbus_proxy_call (
		web_extension,
		"DOMEmbedStyleSheet",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			stylesheet_content),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	g_free (stylesheet_content);
}

void
e_html_editor_view_remove_embed_styles (EHTMLEditorView *view)
{
	e_html_editor_view_call_simple_extension_function (
		view, "DOMRemoveEmbedStyleSheet");
}

static const gchar *
citation_color_level_1 (void)
{
	return "rgb(114,159,207)";  /* Sky Blue 1 */
}

static const gchar *
citation_color_level_2 (void)
{
	return "rgb(173,127,168)";  /* Plum 1 */
}

static const gchar *
citation_color_level_3 (void)
{
	return "rgb(138,226,52)";  /* Chameleon 1 */
}

static const gchar *
citation_color_level_4 (void)
{
	return "rgb(252,175,62)";  /* Orange 1 */
}

static const gchar *
citation_color_level_5 (void)
{
	return "rgb(233,185,110)";  /* Chocolate 1 */
}

/**
 * e_html_editor_view_update_fonts:
 * @view: an #EHTMLEditorView
 *
 * Forces the editor to reload font settings from WebKitSettings and apply
 * it on the content of the editor document.
 */
void
e_html_editor_view_update_fonts (EHTMLEditorView *view)
{
	gboolean mark_citations, use_custom_font;
	GdkColor *link = NULL;
	GdkColor *visited = NULL;
	gchar *base64, *font, *aa = NULL, *citation_color;
	const gchar *styles[] = { "normal", "oblique", "italic" };
	const gchar *smoothing = NULL;
	GString *stylesheet;
	GtkStyleContext *context;
	PangoFontDescription *ms, *vw;
	WebKitSettings *settings;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	use_custom_font = g_settings_get_boolean (
		view->priv->mail_settings, "use-custom-font");

	if (use_custom_font) {
		font = g_settings_get_string (
			view->priv->mail_settings, "monospace-font");
		ms = pango_font_description_from_string (font ? font : "monospace 10");
		g_free (font);
	} else {
		font = g_settings_get_string (
			view->priv->font_settings, "monospace-font-name");
		ms = pango_font_description_from_string (font ? font : "monospace 10");
		g_free (font);
	}

	if (view->priv->html_mode) {
		if (use_custom_font) {
			font = g_settings_get_string (
				view->priv->mail_settings, "variable-width-font");
			vw = pango_font_description_from_string (font ? font : "serif 10");
			g_free (font);
		} else {
			font = g_settings_get_string (
				view->priv->font_settings, "font-name");
			vw = pango_font_description_from_string (font ? font : "serif 10");
			g_free (font);
		}
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

	if (view->priv->aliasing_settings != NULL)
		aa = g_settings_get_string (
			view->priv->aliasing_settings, "antialiasing");

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

	context = gtk_widget_get_style_context (GTK_WIDGET (view));
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

	g_string_append (
		stylesheet,
		"img "
		"{\n"
		"  height: inherit; \n"
		"  width: inherit; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"span.-x-evo-resizable-wrapper:hover "
		"{\n"
		"  outline: 1px dashed red; \n"
		"  resize: both; \n"
		"  overflow: hidden; \n"
		"  display: inline-block; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"ul,ol "
		"{\n"
		"  -webkit-padding-start: 7ch; \n"
		"}\n");

	g_string_append (
		stylesheet,
		".-x-evo-align-left "
		"{\n"
		"  text-align: left; \n"
		"}\n");

	g_string_append (
		stylesheet,
		".-x-evo-align-center "
		"{\n"
		"  text-align: center; \n"
		"}\n");

	g_string_append (
		stylesheet,
		".-x-evo-align-right "
		"{\n"
		"  text-align: right; \n"
		"}\n");

	g_string_append (
		stylesheet,
		".-x-evo-list-item-align-left "
		"{\n"
		"  text-align: left; \n"
		"}\n");

	g_string_append (
		stylesheet,
		".-x-evo-list-item-align-center "
		"{\n"
		"  text-align: center; \n"
		"  -webkit-padding-start: 0ch; \n"
		"  margin-left: -3ch; \n"
		"  margin-right: 1ch; \n"
		"  list-style-position: inside; \n"
		"}\n");

	g_string_append (
		stylesheet,
		".-x-evo-list-item-align-right "
		"{\n"
		"  text-align: right; \n"
		"  -webkit-padding-start: 0ch; \n"
		"  margin-left: -3ch; \n"
		"  margin-right: 1ch; \n"
		"  list-style-position: inside; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"ol,ul "
		"{\n"
		"  -webkit-margin-before: 0em; \n"
		"  -webkit-margin-after: 0em; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"blockquote "
		"{\n"
		"  -webkit-margin-before: 0em; \n"
		"  -webkit-margin-after: 0em; \n"
		"}\n");

	citation_color = g_settings_get_string (
		view->priv->mail_settings, "citation-color");
	mark_citations = g_settings_get_boolean (
		view->priv->mail_settings, "mark-citations");

	g_string_append (
		stylesheet,
		"blockquote[type=cite] "
		"{\n"
		"  padding: 0.0ex 0ex;\n"
		"  margin: 0ex;\n"
		"  -webkit-margin-start: 0em; \n"
		"  -webkit-margin-end : 0em; \n");

	if (mark_citations && citation_color)
		g_string_append_printf (
			stylesheet,
			"  color: %s !important; \n",
			citation_color);

	g_string_append (stylesheet, "}\n");

	g_string_append (
		stylesheet,
		".-x-evo-quoted "
		"{\n"
		"  -webkit-user-select: none;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character "
		"{\n"
		"  color: %s;\n"
		"}\n",
		citation_color_level_1 ());

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character+"
		".-x-evo-quote-character"
		"{\n"
		"  color: %s;\n"
		"}\n",
		citation_color_level_2 ());

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character"
		"{\n"
		"  color: %s;\n"
		"}\n",
		citation_color_level_3 ());

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character"
		"{\n"
		"  color: %s;\n"
		"}\n",
		citation_color_level_4 ());

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character"
		"{\n"
		"  color: %s;\n"
		"}\n",
		citation_color_level_5 ());

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

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		citation_color_level_1 ());

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		citation_color_level_2 ());

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		citation_color_level_3 ());

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		citation_color_level_4 ());

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		citation_color_level_5 ());

	gdk_color_free (link);
	gdk_color_free (visited);

	base64 = g_base64_encode ((guchar *) stylesheet->str, stylesheet->len);
	g_string_free (stylesheet, TRUE);

	stylesheet = g_string_new ("data:text/css;charset=utf-8;base64,");
	g_string_append (stylesheet, base64);
	g_free (base64);

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view));
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
 * e_html_editor_view_get_element_under_mouse_click:
 * @view: an #EHTMLEditorView
 *
 * Returns DOM element, that was clicked on.
 *
 * Returns: DOM element on that was clicked.
 */
WebKitDOMElement *
e_html_editor_view_get_element_under_mouse_click (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), NULL);

	return view->priv->element_under_mouse;
}

/**
 * e_html_editor_view_check_magic_links
 * @view: an #EHTMLEditorView
 * @include_space: If TRUE the pattern for link expects space on end
 *
 * Check if actual selection in given editor is link. If so, it is surrounded
 * with ANCHOR element.
 */
void
e_html_editor_view_check_magic_links (EHTMLEditorView *view,
                                      gboolean include_space)
{
	WebKitDOMRange *range;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	range = dom_get_range (view);
	html_editor_view_check_magic_links (view, range, include_space, NULL);
}

static CamelMimePart *
e_html_editor_view_add_inline_image_from_element (EHTMLEditorView *view,
                                                  WebKitDOMElement *element,
                                                  const gchar *attribute,
						  const gchar *uid_domain)
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

	cid = camel_header_msgid_generate (uid_domain);
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
e_html_editor_view_get_parts_for_inline_images (EHTMLEditorView *view,
						const gchar *uid_domain)
{
	GHashTable *added;
	GList *parts = NULL;
	gint length, ii;
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW  (view));
	list = webkit_dom_document_query_selector_all (document, "img[data-inline]", NULL);

	length = webkit_dom_node_list_get_length (list);
	if (length == 0)
		return parts;

	added = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
	for (ii = 0; ii < length; ii++) {
		const gchar *id;
		gchar *cid;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "src");

		if ((id = g_hash_table_lookup (added, src)) != NULL) {
			cid = g_strdup_printf ("cid:%s", id);
		} else {
			CamelMimePart *part;

			part = e_html_editor_view_add_inline_image_from_element (
				view, WEBKIT_DOM_ELEMENT (node), "src", uid_domain);
			parts = g_list_append (parts, part);

			id = camel_mime_part_get_content_id (part);
			cid = g_strdup_printf ("cid:%s", id);

			g_hash_table_insert (added, src, (gpointer) id);
		}
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (node), "src", cid, NULL);
		g_free (src);
		g_free (cid);
	}
	g_object_unref (list);

	list = webkit_dom_document_query_selector_all (
		document, "[data-inline][background]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		CamelMimePart *part;
		const gchar *id;
		gchar *cid = NULL;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "background");

		if ((id = g_hash_table_lookup (added, src)) != NULL) {
			cid = g_strdup_printf ("cid:%s", id);
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (node), "background", cid, NULL);
		} else {
			part = e_html_editor_view_add_inline_image_from_element (
				view, WEBKIT_DOM_ELEMENT (node), "background", uid_domain);
			if (part) {
				parts = g_list_append (parts, part);
				id = camel_mime_part_get_content_id (part);
				g_hash_table_insert (added, src, (gpointer) id);
				cid = g_strdup_printf ("cid:%s", id);
				webkit_dom_element_set_attribute (
					WEBKIT_DOM_ELEMENT (node), "background", cid, NULL);
			}
		}
		g_free (src);
		g_free (cid);
	}

	g_object_unref (list);
	g_hash_table_destroy (added);

	return parts;
}

/**
 * e_html_editor_view_add_inline_image_from_mime_part:
 * @composer: a composer object
 * @part: a CamelMimePart containing image data
 *
 * This adds the mime part @part to @composer as an inline image.
 **/
void
e_html_editor_view_add_inline_image_from_mime_part (EHTMLEditorView *view,
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

	g_hash_table_insert (view->priv->inline_images, cid_src, src);

	g_free (base64_encoded);
	g_free (mime_type);
	g_object_unref (stream);
}

void
e_html_editor_view_set_is_message_from_draft (EHTMLEditorView *view,
                                              gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	view->priv->is_message_from_draft = value;
}

gboolean
e_html_editor_view_is_message_from_draft (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->is_message_from_draft;
}

void
e_html_editor_view_set_is_message_from_selection (EHTMLEditorView *view,
                                                  gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	view->priv->is_message_from_selection = value;
}

gboolean
e_html_editor_view_is_message_from_edit_as_new (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->is_message_from_edit_as_new;
}
void
e_html_editor_view_set_is_message_from_edit_as_new (EHTMLEditorView *view,
                                                    gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	view->priv->is_message_from_edit_as_new = value;
}

void
e_html_editor_view_set_remove_initial_input_line (EHTMLEditorView *view,
                                                  gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	view->priv->remove_initial_input_line = value;
}
