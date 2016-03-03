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

#include <e-util/e-util.h>
#include <e-util/e-marshal.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#define E_HTML_EDITOR_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_VIEW, EHTMLEditorViewPrivate))

/* stephenhay from https://mathiasbynens.be/demo/url-regex */
#define URL_PROTOCOLS "news|telnet|nntp|file|https?|s?ftp|webcal|localhost|ssh"
#define URL_PATTERN_BASE "(?=((?:(?:(?:" URL_PROTOCOLS ")\\:\\/\\/)|(?:www\\.|ftp\\.))[^\\s\\/\\$\\.\\?#].[^\\s]*)"
#define URL_PATTERN_NO_NBSP ")((?:(?!&nbsp;).)*)"
#define URL_PATTERN URL_PATTERN_BASE URL_PATTERN_NO_NBSP
#define URL_PATTERN_SPACE URL_PATTERN_BASE "\\s$" URL_PATTERN_NO_NBSP
/* Taken from camel-url-scanner.c */
#define URL_INVALID_TRAILING_CHARS ",.:;?!-|}])\""

/* http://www.w3.org/TR/html5/forms.html#valid-e-mail-address */
#define E_MAIL_PATTERN \
	"[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}"\
	"[a-zA-Z0-9])?(?:\\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*"

#define E_MAIL_PATTERN_SPACE E_MAIL_PATTERN "\\s"

#define QUOTE_SYMBOL ">"

#define HTML_KEY_CODE_BACKSPACE 8
#define HTML_KEY_CODE_RETURN 13
#define HTML_KEY_CODE_CONTROL 17
#define HTML_KEY_CODE_SPACE 32
#define HTML_KEY_CODE_DELETE 46

#define HISTORY_SIZE_LIMIT 30

#define TRY_TO_PRESERVE_BLOCKS 0

#define d(x)

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
	gint unicode_smileys	: 1;
	gint can_copy		: 1;
	gint can_cut		: 1;
	gint can_paste		: 1;
	gint can_redo		: 1;
	gint can_undo		: 1;
	gint reload_in_progress : 1;
	gint html_mode		: 1;

	EHTMLEditorSelection *selection;

	GHashTable *inline_images;

	GSettings *mail_settings;
	GSettings *font_settings;
	GSettings *aliasing_settings;

	gboolean convert_in_situ;
	gboolean body_input_event_removed;
	gboolean is_editting_message;
	gboolean is_message_from_draft;
	gboolean is_message_from_edit_as_new;
	gboolean is_message_from_selection;
	gboolean return_key_pressed;
	gboolean space_key_pressed;
	gboolean smiley_written;
	gboolean undo_redo_in_progress;
	gboolean dont_save_history_in_body_input;
	gboolean composition_in_progress;
	gboolean style_change_callbacks_blocked;
	gboolean selection_changed_callbacks_blocked;
	gboolean copy_paste_clipboard_in_view;
	gboolean copy_paste_primary_in_view;
	gboolean copy_action_triggered;
	gboolean pasting_primary_clipboard;
	gboolean renew_history_after_coordinates;

	GHashTable *old_settings;

	GQueue *post_reload_operations;
	guint spell_check_on_scroll_event_source_id;

	gulong owner_change_primary_cb_id;
	gulong owner_change_clipboard_cb_id;

	GList *history;
	guint history_size;
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
	PROP_UNICODE_SMILEYS,
	PROP_SPELL_CHECKER
};

enum {
	POPUP_EVENT,
	PASTE_PRIMARY_CLIPBOARD,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static CamelDataCache *emd_global_http_cache = NULL;

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

static WebKitDOMRange *
html_editor_view_get_dom_range (EHTMLEditorView *view)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_object_unref (dom_selection);
		return NULL;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	g_object_unref (dom_selection);
	return range;
}

static gchar *
get_node_inner_html (WebKitDOMNode *node)
{
	gchar *inner_html;
	WebKitDOMDocument *document;
	WebKitDOMElement *div;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (node));
	div = webkit_dom_document_create_element (document, "div", NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (div),
		webkit_dom_node_clone_node (WEBKIT_DOM_NODE (node), TRUE),
		NULL);

	inner_html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (div));
	remove_node (WEBKIT_DOM_NODE (div));

	return inner_html;
}

#if d(1)+0
static void
print_node_inner_html (WebKitDOMNode *node)
{
	gchar *inner_html;

	if (!node) {
		printf ("    none\n");
		return;
	}

	inner_html = get_node_inner_html (node);

	printf ("    '%s'\n", inner_html);

	g_free (inner_html);
}

static void
print_history_event (EHTMLEditorViewHistoryEvent *event)
{
	if (event->type != HISTORY_START && event->type != HISTORY_AND) {
		printf ("  HISTORY EVENT: %d ; \n", event->type);
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
		case HISTORY_BLOCKQUOTE:
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
		case HISTORY_TABLE_INPUT:
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
			printf ("  UNKNOWN HISTORY TYPE\n");
	}
}

static void
print_history (EHTMLEditorView *view)
{
	printf ("-------------------\nWHOLE HISTORY STACK\n");
	if (view->priv->history) {
		g_list_foreach (
			view->priv->history,
			(GFunc) print_history_event,
			NULL);
	}

	printf ("-------------------\n");
}

static void
print_undo_events (EHTMLEditorView *view)
{
	GList *item = view->priv->history;

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
print_redo_events (EHTMLEditorView *view)
{
	GList *item = view->priv->history;

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
#endif

gboolean
e_html_editor_view_can_redo (EHTMLEditorView *view)
{
	if (view->priv->history && view->priv->history->prev)
		return TRUE;
	else
		return FALSE;
}

gboolean
e_html_editor_view_can_undo (EHTMLEditorView *view)
{
	if (view->priv->history) {
		EHTMLEditorViewHistoryEvent *event;

		event = view->priv->history->data;

		return (event->type != HISTORY_START);
	} else
		return FALSE;
}

static void
html_editor_view_user_changed_contents_cb (EHTMLEditorView *view)
{
	gboolean can_redo, can_undo;

	e_html_editor_view_set_changed (view, TRUE);

	can_redo = e_html_editor_view_can_redo (view);
	if (view->priv->can_redo != can_redo) {
		view->priv->can_redo = can_redo;
		g_object_notify (G_OBJECT (view), "can-redo");
	}

	can_undo = e_html_editor_view_can_undo (view);
	if (view->priv->can_undo != can_undo) {
		view->priv->can_undo = can_undo;
		g_object_notify (G_OBJECT (view), "can-undo");
	}
}

static void
html_editor_view_selection_changed_cb (EHTMLEditorView *view,
                                       gpointer user_data)
{
	WebKitWebView *web_view;
	gboolean can_copy, can_cut, can_paste;

	web_view = WEBKIT_WEB_VIEW (view);

	/* When the webview is being (re)loaded, the document is in an
	 * inconsistant state and there is no selection, so don't propagate
	 * the signal further to EHTMLEditorSelection and others and wait until
	 * the load is finished. */
	if (view->priv->reload_in_progress) {
		g_signal_stop_emission_by_name (view, "selection-changed");
		return;
	}

	can_copy = webkit_web_view_can_copy_clipboard (web_view);
	if (view->priv->can_copy != can_copy) {
		view->priv->can_copy = can_copy;
		/* This means that we have an active selection thus the primary
		 * clipboard content is from composer. */
		if (can_copy)
			view->priv->copy_paste_primary_in_view = TRUE;
		g_object_notify (G_OBJECT (view), "can-copy");
	}

	can_cut = webkit_web_view_can_cut_clipboard (web_view);
	if (view->priv->can_cut != can_cut) {
		view->priv->can_cut = can_cut;
		g_object_notify (G_OBJECT (view), "can-cut");
	}

	can_paste = webkit_web_view_can_paste_clipboard (web_view);
	if (view->priv->can_paste != can_paste) {
		view->priv->can_paste = can_paste;
		g_object_notify (G_OBJECT (view), "can-paste");
	}
}

static void
block_selection_changed_callbacks (EHTMLEditorView *view)
{
	e_html_editor_selection_block_selection_changed (view->priv->selection);
	if (!view->priv->selection_changed_callbacks_blocked) {
		g_signal_handlers_block_by_func (view, html_editor_view_selection_changed_cb, NULL);
		view->priv->selection_changed_callbacks_blocked = TRUE;
	}
}

static void
unblock_selection_changed_callbacks (EHTMLEditorView *view)
{
	e_html_editor_selection_unblock_selection_changed (view->priv->selection);
	if (view->priv->selection_changed_callbacks_blocked) {
		g_signal_handlers_unblock_by_func (view, html_editor_view_selection_changed_cb, NULL);
		view->priv->selection_changed_callbacks_blocked = FALSE;
	}
}

static gboolean
html_editor_view_should_show_delete_interface_for_element (EHTMLEditorView *view,
                                                           WebKitDOMHTMLElement *element)
{
	return FALSE;
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
	while (actual && webkit_dom_range_compare_boundary_points (end_range, 2 /* END_TO_END */, actual, NULL) != 0) {
		g_object_unref (actual);
		webkit_dom_dom_selection_modify (
			dom_selection, "move", "forward", "word");
		actual = webkit_dom_dom_selection_get_range_at (
			dom_selection, 0, NULL);
	}
	g_clear_object (&actual);
}

void
e_html_editor_view_force_spell_check_for_current_paragraph (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMElement *parent, *element;
	WebKitDOMRange *end_range, *actual;
	WebKitDOMText *text;

	if (!view->priv->inline_spelling)
		return;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	element = webkit_dom_document_query_selector (
		document, "body[spellcheck=true]", NULL);

	if (!element)
		return;

	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)))
		return;

	selection = e_html_editor_view_get_selection (view);
	e_html_editor_selection_save (selection);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return;

	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EHTMLEditorSelection and here as well
	 * when we are moving with caret */
	block_selection_changed_callbacks (view);

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

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	/* Move on the beginning of the paragraph */
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
	unblock_selection_changed_callbacks (view);

	e_html_editor_selection_restore (selection);
}

static void
refresh_spell_check (EHTMLEditorView *view,
                     gboolean enable_spell_check)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *end_range, *actual;
	WebKitDOMText *text;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)))
		return;

	/* Enable/Disable spellcheck in composer */
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body),
		"spellcheck",
		enable_spell_check ? "true" : "false",
		NULL);

	selection = e_html_editor_view_get_selection (view);
	e_html_editor_selection_save (selection);

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

		add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EHTMLEditorSelection and here as well
	 * when we are moving with caret */
	block_selection_changed_callbacks (view);

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
	unblock_selection_changed_callbacks (view);

	e_html_editor_selection_restore (selection);
}

void
e_html_editor_view_turn_spell_check_off (EHTMLEditorView *view)
{
	refresh_spell_check (view, FALSE);
}

void
e_html_editor_view_force_spell_check_in_viewport (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	glong viewport_height;
	WebKitDOMDocument *document;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMElement *last_element;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *end_range, *actual;
	WebKitDOMText *text;

	if (!view->priv->inline_spelling)
		return;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = WEBKIT_DOM_HTML_ELEMENT (webkit_dom_document_query_selector (
		document, "body[spellcheck=true]", NULL));

	if (!body) {
		body = webkit_dom_document_get_body (document);
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body), "spellcheck", "true", NULL);
	}

	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)))
		return;

	selection = e_html_editor_view_get_selection (view);
	e_html_editor_selection_save (selection);

	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EHTMLEditorSelection and here as well
	 * when we are moving with caret */
	block_selection_changed_callbacks (view);

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
	unblock_selection_changed_callbacks (view);

	e_html_editor_selection_restore (selection);
}

void
e_html_editor_view_force_spell_check (EHTMLEditorView *view)
{
	if (view->priv->inline_spelling)
		refresh_spell_check (view, TRUE);
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

void
e_html_editor_view_quote_plain_text_element_after_wrapping (WebKitDOMDocument *document,
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
		g_object_unref (br);
	}

	g_object_unref (list);
	g_free (quotation);
}

static gboolean
is_citation_node (WebKitDOMNode *node)
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

static gboolean
return_pressed_in_empty_line (EHTMLEditorSelection *selection,
                              WebKitDOMDocument *document)
{
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	if (!range) {
		g_object_unref (dom_selection);
		return FALSE;
	}

	g_object_unref (dom_selection);

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

static WebKitDOMElement *
prepare_paragraph (EHTMLEditorSelection *selection,
                   WebKitDOMDocument *document,
                   gboolean with_selection)
{
	WebKitDOMElement *element, *paragraph;

	paragraph = e_html_editor_selection_get_paragraph_element (
		selection, document, -1, 0);

	if (with_selection)
		add_selection_markers_into_element_start (
			document, paragraph, NULL, NULL);

	element = webkit_dom_document_create_element (document, "BR", NULL);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (paragraph), WEBKIT_DOM_NODE (element), NULL);

	return paragraph;
}

static WebKitDOMElement *
wrap_and_quote_element (EHTMLEditorView *view,
                        WebKitDOMElement *element)
{
	gint citation_level;
	WebKitDOMElement *tmp_element = element;

	g_return_val_if_fail (WEBKIT_DOM_IS_ELEMENT (element), element);

	if (view->priv->html_mode)
		return element;

	citation_level = get_citation_level (WEBKIT_DOM_NODE (element), FALSE);

	remove_quoting_from_element (element);
	remove_wrapping_from_element (element);

	if (element_has_class (element, "-x-evo-paragraph")) {
		gint word_wrap_length, length;

		word_wrap_length = e_html_editor_selection_get_word_wrap_length (
			view->priv->selection);
		length = word_wrap_length - 2 * citation_level;
		tmp_element = e_html_editor_selection_wrap_paragraph_length (
			view->priv->selection, element, length);
	}

	if (citation_level > 0) {
		WebKitDOMDocument *document;

		webkit_dom_node_normalize (WEBKIT_DOM_NODE (tmp_element));
		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		e_html_editor_view_quote_plain_text_element_after_wrapping (
			document, tmp_element, citation_level);
	}

	return tmp_element;
}

static WebKitDOMElement *
insert_new_line_into_citation (EHTMLEditorView *view,
                               const gchar *html_to_insert)
{
	gboolean html_mode, ret_val, avoid_editor_call;
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMElement *element, *paragraph = NULL;
	WebKitDOMNode *last_block;

	html_mode = e_html_editor_view_get_html_mode (view);
	selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	avoid_editor_call =
		return_pressed_in_empty_line (selection, document);

	if (avoid_editor_call) {
		WebKitDOMElement *selection_start_marker;
		WebKitDOMNode *current_block, *parent, *parent_block, *block_clone;

		e_html_editor_selection_save (selection);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		current_block = e_html_editor_get_parent_block_node_from_child (
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

		paragraph = e_html_editor_selection_get_paragraph_element (
			selection, document, -1, 0);

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

		e_html_editor_selection_restore (selection);

		return NULL;
	} else {
		e_html_editor_view_remove_input_event_listener_from_body (view);
		block_selection_changed_callbacks (view);

		ret_val = e_html_editor_view_exec_command (
			view, E_HTML_EDITOR_VIEW_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, NULL);

		unblock_selection_changed_callbacks (view);
		e_html_editor_view_register_input_event_listener_on_body (view);

		if (!ret_val)
			return NULL;

		element = webkit_dom_document_query_selector (
			document, "body>br", NULL);

		if (!element)
			return NULL;
	}

	last_block = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
	while (last_block && is_citation_node (last_block))
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
			while (node && is_citation_node (node))
				node = webkit_dom_node_get_first_child (node);

			/* Rewrap and requote nodes that were created by split. */
			if (WEBKIT_DOM_IS_ELEMENT (node))
				wrap_and_quote_element (view, WEBKIT_DOM_ELEMENT (node));

			if (WEBKIT_DOM_IS_ELEMENT (last_block))
				wrap_and_quote_element (view, WEBKIT_DOM_ELEMENT (last_block));

			e_html_editor_view_force_spell_check_in_viewport (view);
		}
	}

	if (html_to_insert && *html_to_insert) {
		paragraph = prepare_paragraph (selection, document, FALSE);
		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (paragraph),
			html_to_insert,
			NULL);

		if (!webkit_dom_element_query_selector (paragraph, "#-x-evo-selection-start-marker", NULL))
			add_selection_markers_into_element_end (
				document, paragraph, NULL, NULL);
	} else
		paragraph = prepare_paragraph (selection, document, TRUE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
		WEBKIT_DOM_NODE (paragraph),
		WEBKIT_DOM_NODE (element),
		NULL);

	remove_node (WEBKIT_DOM_NODE (element));

	e_html_editor_selection_restore (selection);

	return paragraph;
}

static void
set_base64_to_element_attribute (EHTMLEditorView *view,
                                 WebKitDOMElement *element,
                                 const gchar *attribute)
{
	gchar *attribute_value;
	const gchar *base64_src;

	attribute_value = webkit_dom_element_get_attribute (element, attribute);

	if (attribute_value && (base64_src = g_hash_table_lookup (view->priv->inline_images, attribute_value)) != NULL) {
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
					view, WEBKIT_DOM_ELEMENT (node), attribute_ns);
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
			view, WEBKIT_DOM_ELEMENT (node), "background");
		g_object_unref (node);
	}
	g_object_unref (list);
	g_hash_table_remove_all (view->priv->inline_images);
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
move_elements_to_body (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitDOMNodeList *list;
	gint ii;

	selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

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

		element = e_html_editor_selection_get_paragraph_element (selection, document, -1, 0);
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

		if (!WEBKIT_DOM_IS_HTMLBR_ELEMENT (webkit_dom_node_get_last_child (node)))
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
                                    gboolean include_space_by_user)
{
	gchar *node_text;
	gchar **urls;
	GRegex *regex = NULL;
	GMatchInfo *match_info;
	gint start_pos_url, end_pos_url;
	WebKitDOMNode *node;
	gboolean include_space = FALSE;
	gboolean is_email_address = FALSE;
	gboolean has_selection;

	if (!view->priv->magic_links)
		return;

	if (include_space_by_user)
		include_space = TRUE;
	else
		include_space = view->priv->space_key_pressed;

	node = webkit_dom_range_get_end_container (range, NULL);
	has_selection = !webkit_dom_range_get_collapsed (range, NULL);

	if (view->priv->return_key_pressed) {
		WebKitDOMNode* block;

		block = e_html_editor_get_parent_block_node_from_child (node);
		/* Get previous block */
		if (!(block = webkit_dom_node_get_previous_sibling (block)))
			return;

		/* If block is quoted content, get the last block there */
		while (block && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (block))
			block = webkit_dom_node_get_last_child (block);

		/* Get the last non-empty node */
		node = webkit_dom_node_get_last_child (block);
		if (WEBKIT_DOM_IS_CHARACTER_DATA (node) &&
		    webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node)) == 0)
			node = webkit_dom_node_get_previous_sibling (node);
	} else {
		e_html_editor_selection_save (view->priv->selection);
		if (has_selection) {
			WebKitDOMDocument *document;
			WebKitDOMElement *selection_end_marker;

			document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

			selection_end_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			node = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (selection_end_marker));
		}
	}

	if (!node || WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node))
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
		WebKitDOMDocument *document;
		WebKitDOMNode *url_text_node;
		WebKitDOMElement *anchor;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

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
			    !(strchr (URL_INVALID_TRAILING_CHARS, *text_to_append) &&
			      !(*text_to_append == '?' && strlen(text_to_append) > 1)) &&
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
				inner_html = webkit_dom_html_element_get_inner_html (
					WEBKIT_DOM_HTML_ELEMENT (parent));
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
	if (!view->priv->return_key_pressed)
		e_html_editor_selection_restore (view->priv->selection);
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
insert_dash_history_event (EHTMLEditorView *view)
{
	EHTMLEditorViewHistoryEvent *event, *last;
	GList *history;
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;

	event = g_new0 (EHTMLEditorViewHistoryEvent, 1);
	event->type = HISTORY_INPUT;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	fragment = webkit_dom_document_create_document_fragment (document);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (fragment),
		WEBKIT_DOM_NODE (
			webkit_dom_document_create_text_node (document, "-")),
		NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (fragment),
		WEBKIT_DOM_NODE (
			create_selection_marker (document, TRUE)),
		NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (fragment),
		WEBKIT_DOM_NODE (
			create_selection_marker (document, FALSE)),
		NULL);
	event->data.fragment = fragment;

	last = view->priv->history->data;
	/* The dash event needs to have the same coordinates as the character
	 * that is right after it. */
	event->after.start.x = last->after.start.x;
	event->after.start.y = last->after.start.y;
	event->after.end.x = last->after.end.x;
	event->after.end.y = last->after.end.y;

	history = view->priv->history->next;
	if (history) {
		EHTMLEditorViewHistoryEvent *item;
		WebKitDOMNode *first_child;

		item = history->data;

		if (item->type != HISTORY_INPUT)
			return;

		first_child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (item->data.fragment));
		if (WEBKIT_DOM_IS_TEXT (first_child)) {
			guint diff;

			diff = event->after.start.x - item->after.start.x;

			/* We need to move the coordinate of the last
			 * event by one character. */
			last->after.start.x += diff;
			last->after.end.x += diff;

			view->priv->history = g_list_insert_before (
				view->priv->history, history, event);
		}
	}
}

static void
insert_delete_event (EHTMLEditorView *view,
                     WebKitDOMRange *range)
{
	EHTMLEditorViewHistoryEvent *ev;
	WebKitDOMDocumentFragment *fragment;

	if (view->priv->undo_redo_in_progress)
		return;

	ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
	ev->type = HISTORY_DELETE;

	fragment = webkit_dom_range_clone_contents (range, NULL);
	ev->data.fragment = fragment;

	e_html_editor_selection_get_selection_coordinates (
		view->priv->selection,
		&ev->before.start.x,
		&ev->before.start.y,
		&ev->before.end.x,
		&ev->before.end.y);

	ev->after.start.x = ev->before.start.x;
	ev->after.start.y = ev->before.start.y;
	ev->after.end.x = ev->before.start.x;
	ev->after.end.y = ev->before.start.y;

	e_html_editor_view_insert_new_history_event (view, ev);

	ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
	ev->type = HISTORY_AND;

	e_html_editor_view_insert_new_history_event (view, ev);
}

static void
emoticon_insert_span (EHTMLEditorView *view,
                      EEmoticon *emoticon,
                      WebKitDOMElement *span)
{
	EHTMLEditorSelection *selection;
	EHTMLEditorViewHistoryEvent *ev = NULL;
	gboolean misplaced_selection = FALSE;
	gchar *node_text = NULL;
	const gchar *emoticon_start;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *node, *insert_before, *prev_sibling, *next_sibling;
	WebKitDOMNode *selection_end_marker_parent, *inserted_node;
	WebKitDOMRange *range;

	selection = e_html_editor_view_get_selection (view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	if (e_html_editor_selection_is_collapsed (selection)) {
		e_html_editor_selection_save (selection);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		if (!view->priv->smiley_written) {
			if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
				ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
				if (view->priv->unicode_smileys)
					ev->type = HISTORY_INPUT;
				else {
					ev->type = HISTORY_SMILEY;

					e_html_editor_selection_get_selection_coordinates (
						selection,
						&ev->before.start.x,
						&ev->before.start.y,
						&ev->before.end.x,
						&ev->before.end.y);
				}
			}
		}
	} else {
		WebKitDOMRange *tmp_range;

		tmp_range = html_editor_view_get_dom_range (view);
		insert_delete_event (view, tmp_range);
		g_object_unref (tmp_range);

		e_html_editor_view_exec_command (
			view, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);

		if (!view->priv->smiley_written) {
			if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
				ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
				if (view->priv->unicode_smileys)
					ev->type = HISTORY_INPUT;
				else {
					ev->type = HISTORY_SMILEY;

					e_html_editor_selection_get_selection_coordinates (
						selection,
						&ev->before.start.x,
						&ev->before.start.y,
						&ev->before.end.x,
						&ev->before.end.y);
				}
			}
		}

		e_html_editor_selection_save (selection);

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

		add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);

		if (ev && !view->priv->unicode_smileys)
			e_html_editor_selection_get_selection_coordinates (
				selection,
				&ev->before.start.x,
				&ev->before.start.y,
				&ev->before.end.x,
				&ev->before.end.y);
	}

	/* Sometimes selection end marker is in body. Move it into next sibling */
	selection_end_marker_parent = e_html_editor_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_end_marker));
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (selection_end_marker_parent)) {
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (selection_start_marker)),
			WEBKIT_DOM_NODE (selection_end_marker),
			WEBKIT_DOM_NODE (selection_start_marker),
			NULL);
		if (ev && !view->priv->unicode_smileys)
			e_html_editor_selection_get_selection_coordinates (
				selection,
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

	range = html_editor_view_get_dom_range (view);
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
		if (view->priv->unicode_smileys)
			inserted_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (insert_before),
				webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (span)),
				webkit_dom_node_get_next_sibling (next_sibling),
				NULL);
		else
			inserted_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (insert_before),
				WEBKIT_DOM_NODE (span),
				webkit_dom_node_get_next_sibling (next_sibling),
				NULL);
	} else {
		if (view->priv->unicode_smileys)
			inserted_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (insert_before),
				webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (span)),
				insert_before,
				NULL);
		else
			inserted_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (insert_before),
				WEBKIT_DOM_NODE (span),
				insert_before,
				NULL);
	}

	if (!view->priv->unicode_smileys)
		webkit_dom_html_element_insert_adjacent_html (
			WEBKIT_DOM_HTML_ELEMENT (inserted_node), "afterend", "&#8203;", NULL);

	if (ev) {
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMNode *node;

		fragment = webkit_dom_document_create_document_fragment (document);
		node = webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (inserted_node), TRUE),
			NULL);
		if (view->priv->unicode_smileys) {
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				WEBKIT_DOM_NODE (
					create_selection_marker (document, TRUE)),
				NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				WEBKIT_DOM_NODE (
					create_selection_marker (document, FALSE)),
				NULL);
		} else
			webkit_dom_html_element_insert_adjacent_html (
				WEBKIT_DOM_HTML_ELEMENT (node), "afterend", "&#8203;", NULL);
		ev->data.fragment = fragment;
	}

	/* Remove the text that represents the text version of smiley that was
	 * written into the composer. */
	if (node_text && view->priv->smiley_written) {
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
					insert_dash_history_event (view);
			}
		}
		view->priv->smiley_written = FALSE;
	}

	if (ev) {
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}

	e_html_editor_selection_restore (selection);

	e_html_editor_view_set_changed (view, TRUE);

	g_free (node_text);
}

static void
emoticon_read_async_cb (GFile *file,
                        GAsyncResult *result,
                        LoadContext *load_context)
{
	EHTMLEditorView *view = load_context->view;
	EEmoticon *emoticon = load_context->emoticon;
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

	html_mode = e_html_editor_view_get_html_mode (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
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

	emoticon_insert_span (view, emoticon, wrapper);

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

	if (e_html_editor_view_get_unicode_smileys (view)) {
		WebKitDOMDocument *document;
		WebKitDOMElement *wrapper;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		wrapper = webkit_dom_document_create_element (document, "SPAN", NULL);
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (wrapper), emoticon->unicode_character, NULL);

		emoticon_insert_span (view, emoticon, wrapper);
	} else {
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

	if (!view->priv->magic_smileys)
		return;

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
		view->priv->smiley_written = TRUE;
		e_html_editor_view_insert_smiley (view, (EEmoticon *) emoticon);
	}

	g_free (node_text);
}

static void
html_editor_view_set_links_active (EHTMLEditorView *view,
                                   gboolean active)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *style;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	style = webkit_dom_document_get_element_by_id (document, "-x-evo-style-a");
	if (style)
		remove_node (WEBKIT_DOM_NODE (style));

	if (!active) {
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
fix_paragraph_structure_after_pressing_enter_after_smiley (EHTMLEditorSelection *selection,
                                                           WebKitDOMDocument *document)
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

static gboolean
fix_paragraph_structure_after_pressing_enter (EHTMLEditorSelection *selection,
                                              WebKitDOMDocument *document)
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
		e_html_editor_selection_set_paragraph_style (
			selection, WEBKIT_DOM_ELEMENT (node), -1, 0, "");
		g_object_unref (node);
	}
	g_object_unref (list);

	return prev_is_heading;
}

static gboolean
surround_text_with_paragraph_if_needed (EHTMLEditorSelection *selection,
                                        WebKitDOMDocument *document,
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
		element = e_html_editor_selection_put_node_into_paragraph (
			selection, document, node, TRUE);
		if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (parent))
			webkit_dom_element_remove_attribute (element, "style");

		if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling))
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
                       EHTMLEditorView *view)
{
	glong key_code;

	key_code = webkit_dom_ui_event_get_key_code (event);
	if (key_code == HTML_KEY_CODE_CONTROL)
		html_editor_view_set_links_active (view, TRUE);
	else if (key_code == HTML_KEY_CODE_DELETE ||
		 key_code == HTML_KEY_CODE_BACKSPACE)
		view->priv->dont_save_history_in_body_input = TRUE;
}

static gboolean
save_history_before_event_in_table (EHTMLEditorView *view,
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
		EHTMLEditorViewHistoryEvent *ev;

		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_TABLE_INPUT;

		if (block) {
			e_html_editor_selection_save (view->priv->selection);
			ev->data.dom.from = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (block), TRUE);
			e_html_editor_selection_restore (view->priv->selection);
		} else
			ev->data.dom.from = NULL;

		e_html_editor_selection_get_selection_coordinates (
			view->priv->selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		e_html_editor_view_insert_new_history_event (view, ev);

		return TRUE;
	}

	return FALSE;
}

static void
body_keypress_event_cb (WebKitDOMElement *element,
                        WebKitDOMUIEvent *event,
                        EHTMLEditorView *view)
{
	glong key_code;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	view->priv->return_key_pressed = FALSE;
	view->priv->space_key_pressed = FALSE;

	key_code = webkit_dom_ui_event_get_key_code (event);
	if (key_code == HTML_KEY_CODE_RETURN)
		view->priv->return_key_pressed = TRUE;
	else if (key_code == HTML_KEY_CODE_SPACE)
		view->priv->space_key_pressed = TRUE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	if (save_history_before_event_in_table (view, range)) {
		g_object_unref (range);
		g_object_unref (dom_selection);
		return;
	}

	if (!webkit_dom_range_get_collapsed (range, NULL))
		insert_delete_event (view, range);

	if (view->priv->return_key_pressed) {
		EHTMLEditorViewHistoryEvent *ev;

		/* Insert new history event for Return to have the right coordinates.
		 * The fragment will be added later. */
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_INPUT;

		e_html_editor_selection_get_selection_coordinates (
			view->priv->selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		e_html_editor_view_insert_new_history_event (view, ev);
	}

	g_object_unref (range);
	g_object_unref (dom_selection);
}

static gboolean
save_history_after_event_in_table (EHTMLEditorView *view)
{
	EHTMLEditorViewHistoryEvent *ev;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
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

	/* If writing to table we have to create different history event. */
	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (element)) {
		ev = view->priv->history->data;
		if (ev->type != HISTORY_TABLE_INPUT)
			return FALSE;
	} else
		return FALSE;

	e_html_editor_selection_save (view->priv->selection);

	e_html_editor_selection_get_selection_coordinates (
		view->priv->selection,
		&ev->after.start.x,
		&ev->after.start.y,
		&ev->after.end.x,
		&ev->after.end.y);

	ev->data.dom.to = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (element), TRUE);

	e_html_editor_selection_restore (view->priv->selection);

	return TRUE;
}

static void
save_history_for_input (EHTMLEditorView *view)
{
	EHTMLEditorViewHistoryEvent *ev;
	glong offset;
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range, *range_clone;
	WebKitDOMNode *start_container;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection)) {
		g_object_unref (dom_selection);
		return;
	}

	if (view->priv->return_key_pressed) {
		ev = view->priv->history->data;
		if (ev->type != HISTORY_INPUT) {
			g_object_unref (dom_selection);
			return;
		}
	} else {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_INPUT;
	}

	block_selection_changed_callbacks (view);

	e_html_editor_selection_get_selection_coordinates (
		view->priv->selection,
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
	/* We have to specially handle Return key press */
	if (view->priv->return_key_pressed) {
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
				create_selection_marker (document, TRUE)),
			NULL);
		webkit_dom_node_append_child (
			node,
			WEBKIT_DOM_NODE (
				create_selection_marker (document, FALSE)),
			NULL);

		remove_node (WEBKIT_DOM_NODE (element_start));
		remove_node (WEBKIT_DOM_NODE (element_end));

		g_object_set_data (
			G_OBJECT (fragment), "history-return-key", GINT_TO_POINTER (1));

		webkit_dom_dom_selection_modify (dom_selection, "move", "right", "character");
	} else {
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			WEBKIT_DOM_NODE (
				create_selection_marker (document, TRUE)),
			NULL);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			WEBKIT_DOM_NODE (
				create_selection_marker (document, FALSE)),
			NULL);
	}

	g_object_unref (dom_selection);
	g_object_unref (range);
	g_object_unref (range_clone);

	unblock_selection_changed_callbacks (view);

	ev->data.fragment = fragment;
	if (!view->priv->return_key_pressed)
		e_html_editor_view_insert_new_history_event (view, ev);
}

static gboolean
force_spell_check_on_timeout (EHTMLEditorView *view)
{
	e_html_editor_view_force_spell_check_in_viewport (view);
	view->priv->spell_check_on_scroll_event_source_id = 0;
	return FALSE;
}

static void
body_scroll_event_cb (WebKitDOMElement *element,
                      WebKitDOMEvent *event,
                      EHTMLEditorView *view)
{
	if (!view->priv->inline_spelling)
		return;

	if (view->priv->spell_check_on_scroll_event_source_id > 0)
		g_source_remove (view->priv->spell_check_on_scroll_event_source_id);

	view->priv->spell_check_on_scroll_event_source_id =
		g_timeout_add_seconds (1, (GSourceFunc)force_spell_check_on_timeout, view);
}

static void
body_input_event_cb (WebKitDOMElement *element,
                     WebKitDOMEvent *event,
                     EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	gboolean do_spell_check = FALSE;
	WebKitDOMNode *node;
	WebKitDOMRange *range = html_editor_view_get_dom_range (view);
	WebKitDOMDocument *document;

	selection = e_html_editor_view_get_selection (view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	e_html_editor_view_set_changed (view, TRUE);

	if (view->priv->undo_redo_in_progress) {
		view->priv->undo_redo_in_progress = FALSE;
		view->priv->dont_save_history_in_body_input = FALSE;
		do_spell_check = TRUE;
		goto out;
	}

	/* When the Backspace is pressed in a bulleted list item with just one
	 * character left in it, WebKit will create another BR element in the
	 * item. */
	if (!view->priv->html_mode) {
		WebKitDOMElement *element;

		element = webkit_dom_document_query_selector (
			document, "ul[data-evo-plain-text] > li > br + br", NULL);

		if (element)
			remove_node (WEBKIT_DOM_NODE (element));
	}

	if (!save_history_after_event_in_table (view)) {
		if (!view->priv->dont_save_history_in_body_input)
			save_history_for_input (view);
		else
			do_spell_check = TRUE;
	}

	/* Don't try to look for smileys if we are deleting text. */
	if (!view->priv->dont_save_history_in_body_input)
		html_editor_view_check_magic_smileys (view, range);

	view->priv->dont_save_history_in_body_input = FALSE;

	if (view->priv->return_key_pressed || view->priv->space_key_pressed) {
		html_editor_view_check_magic_links (view, range, FALSE);
		if (view->priv->return_key_pressed) {
			if (fix_paragraph_structure_after_pressing_enter (selection, document) &&
			    view->priv->html_mode) {
				/* When the return is pressed in a H1-6 element, WebKit doesn't
				 * continue with the same element, but creates normal paragraph,
				 * so we have to unset the bold font. */
				view->priv->undo_redo_in_progress = TRUE;
				e_html_editor_selection_set_bold (selection, FALSE);
				view->priv->undo_redo_in_progress = FALSE;
			}

			fix_paragraph_structure_after_pressing_enter_after_smiley (
				selection, document);

			do_spell_check = TRUE;
		}
	} else {
		WebKitDOMNode *node;

		node = webkit_dom_range_get_end_container (range, NULL);

		if (surround_text_with_paragraph_if_needed (selection, document, node)) {
			WebKitDOMElement *element;

			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
			e_html_editor_selection_restore (selection);
		}

		if (WEBKIT_DOM_IS_TEXT (node)) {
			gchar *text;

			text = webkit_dom_node_get_text_content (node);

			if (text && *text && *text != ' ' && !g_str_has_prefix (text, UNICODE_NBSP)) {
				gboolean valid = FALSE;

				if (*text == '?' && strlen (text) > 1)
					valid = TRUE;
				else if (!strchr (URL_INVALID_TRAILING_CHARS, *text))
					valid = TRUE;

				if (valid) {
					WebKitDOMNode *prev_sibling;

					prev_sibling = webkit_dom_node_get_previous_sibling (node);

					if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling))
						html_editor_view_check_magic_links (view, range, FALSE);
				}
			}
			g_free (text);
		}
	}

	node = webkit_dom_range_get_end_container (range, NULL);

	/* After toggling monospaced format, we are using UNICODE_ZERO_WIDTH_SPACE
	 * to move caret into right space. When this callback is called it is not
	 * necessary anymore so remove it */
	if (view->priv->html_mode) {
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
			if (e_html_editor_view_get_html_mode (view)) {
				element_add_class (
					WEBKIT_DOM_ELEMENT (parent), "-x-evo-paragraph");
			} else {
				e_html_editor_selection_set_paragraph_style (
					selection,
					WEBKIT_DOM_ELEMENT (parent),
					-1, 0, "");
			}
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
					create_selection_marker (document, FALSE)),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (
					create_selection_marker (document, TRUE)),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
			/* Move the text node outside of smiley. */
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (text_node),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
			e_html_editor_selection_restore (selection);
		}
	}

	/* Writing into quoted content */
	if (!view->priv->html_mode) {
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

		e_html_editor_selection_save (selection);

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
			e_html_editor_selection_restore (selection);
			goto out;
		}

		if (selection_start_marker) {
			gchar *content;
			gint text_length, word_wrap_length, length;
			WebKitDOMElement *block;
			gboolean remove_quoting = FALSE;

			word_wrap_length =
				e_html_editor_selection_get_word_wrap_length (selection);
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

				block = e_html_editor_selection_wrap_paragraph_length (
					selection, block, length);
				webkit_dom_node_normalize (WEBKIT_DOM_NODE (block));
				e_html_editor_view_quote_plain_text_element_after_wrapping (
					document, WEBKIT_DOM_ELEMENT (block), citation_level);
				selection_start_marker = webkit_dom_document_query_selector (
					document, "span#-x-evo-selection-start-marker", NULL);
				if (!selection_start_marker)
					add_selection_markers_into_element_end (
						document,
						WEBKIT_DOM_ELEMENT (block),
						NULL,
						NULL);

				e_html_editor_selection_restore (selection);
				do_spell_check = TRUE;

				goto out;
			}
		}
		e_html_editor_selection_restore (selection);
	}
 out:
	if (do_spell_check)
		e_html_editor_view_force_spell_check_for_current_paragraph (view);

	g_object_unref (range);
}

static void
remove_empty_blocks (WebKitDOMDocument *document)
{
	gint ii, length;
	WebKitDOMNodeList *list;

	list = webkit_dom_document_query_selector_all (
		document, "blockquote[type=cite] > :empty", NULL);

	length = webkit_dom_node_list_get_length (list);
	for  (ii = 0; ii < length; ii++) {
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
static void
disable_quote_marks_select (WebKitDOMDocument *document)
{
	WebKitDOMHTMLHeadElement *head;
	WebKitDOMElement *style_element;

	head = webkit_dom_document_get_head (document);

	if (!webkit_dom_document_get_element_by_id (document, "-x-evo-quote-style")) {
		style_element = webkit_dom_document_create_element (document, "style", NULL);
		webkit_dom_element_set_id (style_element, "-x-evo-quote-style");
		webkit_dom_element_set_attribute (style_element, "type", "text/css", NULL);
		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (style_element),
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

static void
remove_node_and_parents_if_empty (WebKitDOMNode *node)
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
		     (WEBKIT_DOM_IS_HTMLBR_ELEMENT (prev_sibling) &&
		      !webkit_dom_node_get_previous_sibling (prev_sibling))) &&
		    (!next_sibling ||
		     (WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling) &&
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

static void
merge_siblings_if_necessary (WebKitDOMDocument *document,
                             WebKitDOMDocumentFragment *deleted_content)
{
	gboolean equal_nodes;
	gint ii, length;
	WebKitDOMElement *element, *prev_element;
	WebKitDOMNode *child;
	WebKitDOMNodeList *list;

	if ((element = webkit_dom_document_get_element_by_id (document, "-x-evo-main-cite")))
		webkit_dom_element_remove_attribute (element, "id");

	element = webkit_dom_document_query_selector (document, "blockquote:not([data-evo-query-skip]) + blockquote", NULL);
	if (!element)
		goto signature;
 repeat:
	child = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
	if (WEBKIT_DOM_IS_ELEMENT (child))
		prev_element = WEBKIT_DOM_ELEMENT (child);
	else
		goto signature;

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
	} else
		webkit_dom_element_set_attribute (element, "data-evo-query-skip", "", NULL);

	element = webkit_dom_document_query_selector (document, "blockquote:not([data-evo-query-skip]) + blockquote", NULL);
	if (element)
		goto repeat;

 signature:
	list = webkit_dom_document_query_selector_all (
		document, "blockquote[data-evo-query-skip]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for  (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (node), "data-evo-query-skip");
		g_object_unref (node);
	}
	g_object_unref (list);

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
static void
body_key_up_event_process_backspace_or_delete (EHTMLEditorView *view,
                                               gboolean delete)
{
	EHTMLEditorSelection *selection;
	gint level;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMElement *tmp_element;
	WebKitDOMDocument *document;
	WebKitDOMNode *parent, *node;

	if (view->priv->html_mode)
		return;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	disable_quote_marks_select (document);
	/* Remove empty blocks if presented. */
	remove_empty_blocks (document);

	selection = e_html_editor_view_get_selection (view);
	e_html_editor_selection_save (selection);
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
	if (level > 0 && node && !WEBKIT_DOM_IS_HTMLBR_ELEMENT (node)) {
		WebKitDOMElement *block;

		block = WEBKIT_DOM_ELEMENT (e_html_editor_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker)));

		remove_quoting_from_element (block);
		if (element_has_class (block, "-x-evo-paragraph")) {
			gint length, word_wrap_length;

			word_wrap_length = e_html_editor_selection_get_word_wrap_length (selection);
			length =  word_wrap_length - 2 * level;
			block = e_html_editor_selection_wrap_paragraph_length (
				selection, block, length);
			webkit_dom_node_normalize (WEBKIT_DOM_NODE (block));
		}
		e_html_editor_view_quote_plain_text_element_after_wrapping (
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
		remove_wrapping_from_element (tmp_element);
		remove_quoting_from_element (tmp_element);

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

	merge_siblings_if_necessary (document, NULL);

	e_html_editor_selection_restore (selection);
	e_html_editor_view_force_spell_check_for_current_paragraph (view);
}

static void
body_key_up_event_process_return_key (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *parent;

	/* If the return is pressed in an unordered list in plain text mode
	 * the caret is moved to the "*" character before the newly inserted
	 * item. It looks like it is not enough that the item has BR element
	 * inside, but we have to again use the zero width space character
	 * to fix the situation. */
	if (view->priv->html_mode)
		return;

	selection = e_html_editor_view_get_selection (view);
	e_html_editor_selection_save (selection);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	e_html_editor_selection_save (selection);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	if (!WEBKIT_DOM_IS_HTMLLI_ELEMENT (parent) ||
	    !WEBKIT_DOM_IS_HTMLU_LIST_ELEMENT (webkit_dom_node_get_parent_node (parent))) {
		e_html_editor_selection_restore (selection);
		return;
	}

	if (!webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker)) &&
	    (!webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker)) ||
	     WEBKIT_DOM_IS_HTMLBR_ELEMENT (webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker)))))
		webkit_dom_html_element_insert_adjacent_text (
			WEBKIT_DOM_HTML_ELEMENT (parent),
			"afterbegin",
			UNICODE_ZERO_WIDTH_SPACE,
			NULL);

	e_html_editor_selection_restore (selection);
}

static void
body_keyup_event_cb (WebKitDOMElement *element,
                     WebKitDOMUIEvent *event,
                     EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	glong key_code;

	if (!view->priv->composition_in_progress)
		e_html_editor_view_register_input_event_listener_on_body (view);

	selection = e_html_editor_view_get_selection (view);
	if (!e_html_editor_selection_is_collapsed (selection))
		return;

	key_code = webkit_dom_ui_event_get_key_code (event);
	if (key_code == HTML_KEY_CODE_BACKSPACE || key_code == HTML_KEY_CODE_DELETE) {
		body_key_up_event_process_backspace_or_delete (view, key_code == HTML_KEY_CODE_DELETE);

		/* The content was wrapped and the coordinates
		 * of caret could be changed, so renew them. But
		 * only do that when we are not redoing a history
		 * event, otherwise it would modify the history. */
		if (view->priv->renew_history_after_coordinates) {
			EHTMLEditorViewHistoryEvent *ev = NULL;

			ev = view->priv->history->data;
			e_html_editor_selection_get_selection_coordinates (
				selection,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);
		}
	} else if (key_code == HTML_KEY_CODE_CONTROL)
		html_editor_view_set_links_active (view, FALSE);
	else if (key_code == HTML_KEY_CODE_RETURN)
		body_key_up_event_process_return_key (view);
}

static void
clipboard_text_received_for_paste_as_text (GtkClipboard *clipboard,
                                           const gchar *text,
                                           EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	EHTMLEditorViewHistoryEvent *ev = NULL;

	if (!text || !*text)
		return;

	selection = e_html_editor_view_get_selection (view);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		gboolean collapsed;

		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_PASTE_AS_TEXT;

		collapsed = e_html_editor_selection_is_collapsed (selection);
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
		if (!collapsed) {
			ev->before.end.x = ev->before.start.x;
			ev->before.end.y = ev->before.start.y;
		}
		ev->data.string.from = NULL;
		ev->data.string.to = g_strdup (text);
	}

	e_html_editor_selection_insert_as_text (selection, text);

	if (ev) {
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}
}

static void
clipboard_text_received (GtkClipboard *clipboard,
                         const gchar *text,
                         EHTMLEditorView *view)
{
	e_html_editor_view_insert_quoted_text (view, text);
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

		case PROP_UNICODE_SMILEYS:
			e_html_editor_view_set_unicode_smileys (
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
				value, e_html_editor_view_can_redo (
				E_HTML_EDITOR_VIEW (object)));
			return;

		case PROP_CAN_UNDO:
			g_value_set_boolean (
				value, e_html_editor_view_can_undo (
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

		case PROP_UNICODE_SMILEYS:
			g_value_set_boolean (
				value, e_html_editor_view_get_unicode_smileys (
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
free_history_event_content (EHTMLEditorViewHistoryEvent *event)
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
}

static void
free_history_event (EHTMLEditorViewHistoryEvent *event)
{
	if (event == NULL)
		return;

	free_history_event_content (event);

	g_free (event);
}

static void
html_editor_view_dispose (GObject *object)
{
	EHTMLEditorViewPrivate *priv;

	priv = E_HTML_EDITOR_VIEW_GET_PRIVATE (object);

	g_clear_object (&priv->selection);

	if (priv->spell_check_on_scroll_event_source_id > 0) {
		g_source_remove (priv->spell_check_on_scroll_event_source_id);
		priv->spell_check_on_scroll_event_source_id = 0;
	}

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

	if (priv->history != NULL) {
		g_list_free_full (priv->history, (GDestroyNotify) free_history_event);
		priv->history = NULL;
	}

	if (priv->owner_change_clipboard_cb_id > 0) {
		g_signal_handler_disconnect (
			gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
			priv->owner_change_clipboard_cb_id);
		priv->owner_change_clipboard_cb_id = 0;
	}

	if (priv->owner_change_primary_cb_id > 0) {
		g_signal_handler_disconnect (
			gtk_clipboard_get (GDK_SELECTION_PRIMARY),
			priv->owner_change_primary_cb_id);
		priv->owner_change_primary_cb_id = 0;
	}

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

	if (priv->post_reload_operations) {
		g_warn_if_fail (g_queue_is_empty (priv->post_reload_operations));

		g_queue_free (priv->post_reload_operations);
		priv->post_reload_operations = NULL;
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
}

static void
html_editor_view_move_selection_on_point (GtkWidget *widget)
{
	gint x, y;
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (widget));

	device_manager = gdk_display_get_device_manager (
		gtk_widget_get_display (GTK_WIDGET (widget)));
	pointer = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (
		gtk_widget_get_window (GTK_WIDGET (widget)), pointer, &x, &y, NULL);

	e_html_editor_selection_set_on_point (
		e_html_editor_view_get_selection (E_HTML_EDITOR_VIEW (widget)), x, y);
}

static gboolean
html_editor_view_button_press_event (GtkWidget *widget,
                                     GdkEventButton *event)
{
	EHTMLEditorView *view;
	EHTMLEditorSelection *selection;
	gboolean event_handled, collapsed;

	view = E_HTML_EDITOR_VIEW (widget);
	selection = e_html_editor_view_get_selection (view);
	collapsed = e_html_editor_selection_is_collapsed (selection);

	if (event->button == 2) {
		/* Middle click paste */
		html_editor_view_move_selection_on_point (widget);
		/* Remember, that we are pasting primary clipboard to return
		 * correct value in e_html_editor_view_is_pasting_content_from_itself. */
		view->priv->pasting_primary_clipboard = TRUE;
		g_signal_emit (widget, signals[PASTE_PRIMARY_CLIPBOARD], 0);
		event_handled = TRUE;
	} else if (event->button == 3) {
		if (collapsed)
			html_editor_view_move_selection_on_point (widget);
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
	WebKitDOMNode *node;
	gchar *uri;

	webview = WEBKIT_WEB_VIEW (widget);
	hit_test = webkit_web_view_get_hit_test_result (webview, event);

	g_object_get (
		hit_test,
		"context", &context,
		"link-uri", &uri,
		"inner-node", &node,
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
			WebKitDOMElement *element;

			toplevel = gtk_widget_get_toplevel (widget);
			screen = gtk_window_get_screen (GTK_WINDOW (toplevel));
			gtk_show_uri (screen, uri, event->time, NULL);
			g_free (uri);

			element = e_html_editor_dom_node_find_parent_element (node, "A");
			if (element)
				element_add_class (element, "-x-evo-visited-link");
		}

		return TRUE;
	}

	g_free (uri);

	/* Chain up to parent's button_release_event() method. */
	return GTK_WIDGET_CLASS (e_html_editor_view_parent_class)->
		button_release_event (widget, event);
}

static gboolean
prevent_from_deleting_last_element_in_body (EHTMLEditorView *view)
{
	gboolean ret_val = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitDOMNode *node;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

	node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
	if (!node || !webkit_dom_node_get_next_sibling (node)) {
		gchar *content;

		content = webkit_dom_node_get_text_content (WEBKIT_DOM_NODE (body));

		if (!content || !*content)
			ret_val = TRUE;

		g_free (content);

		if (webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (body), "img", NULL))
			ret_val = FALSE;
	}

	return ret_val;
}

static gboolean
delete_hidden_space (EHTMLEditorView *view)
{
	gint citation_level;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *block;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return FALSE;

	block = WEBKIT_DOM_ELEMENT (e_html_editor_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker)));

	citation_level = get_citation_level (
		WEBKIT_DOM_NODE (selection_start_marker), FALSE);

	if (selection_start_marker && citation_level > 0) {
		EHTMLEditorSelection *selection;
		EHTMLEditorViewHistoryEvent *ev = NULL;
		WebKitDOMNode *node;
		WebKitDOMDocumentFragment *fragment;

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

		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_DELETE;

		selection = e_html_editor_view_get_selection (view);
		e_html_editor_selection_get_selection_coordinates (
			selection, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);

		remove_node (node);

		wrap_and_quote_element (view, block);

		fragment = webkit_dom_document_create_document_fragment (document);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			WEBKIT_DOM_NODE (
				webkit_dom_document_create_text_node (document, " ")),
			NULL);
		ev->data.fragment = fragment;

		e_html_editor_selection_get_selection_coordinates (
			selection, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);

		e_html_editor_view_insert_new_history_event (view, ev);

		return TRUE;
	}

	return FALSE;
}

static gboolean
change_quoted_block_to_normal (EHTMLEditorView *view)
{
	EHTMLEditorViewHistoryEvent *ev = NULL;
	EHTMLEditorSelection *selection;
	gint citation_level, success = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;

	selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return FALSE;

	block = e_html_editor_get_parent_block_node_from_child (WEBKIT_DOM_NODE (selection_start_marker));

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

		if (view->priv->html_mode)
			success = WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (
				webkit_dom_node_get_parent_element (block));
	}

	if (success && !view->priv->undo_redo_in_progress) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_UNQUOTE;

		e_html_editor_selection_get_selection_coordinates (
			selection, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		ev->data.dom.from = webkit_dom_node_clone_node (block, TRUE);
	}

	if (success && citation_level == 1) {
		gchar *inner_html;
		WebKitDOMElement *paragraph, *element;

		inner_html = webkit_dom_html_element_get_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (block));
		webkit_dom_element_set_id (WEBKIT_DOM_ELEMENT (block), "-x-evo-to-remove");

		paragraph = insert_new_line_into_citation (view, inner_html);
		g_free (inner_html);

		if (paragraph) {
			if (view->priv->html_mode) {
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
			remove_node (block);

		while ((element = webkit_dom_document_get_element_by_id (document, "-x-evo-to-remove")))
			remove_node (WEBKIT_DOM_NODE (element));

		if (paragraph)
			remove_node_if_empty (
				webkit_dom_node_get_next_sibling (
					WEBKIT_DOM_NODE (paragraph)));
	}

	if (success && citation_level > 1) {
		WebKitDOMNode *parent;

		if (view->priv->html_mode) {
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

		remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));
		remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));

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

		wrap_and_quote_element (view, WEBKIT_DOM_ELEMENT (block));
	}

	if (ev) {
		e_html_editor_selection_get_selection_coordinates (
			selection, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);

		e_html_editor_view_insert_new_history_event (view, ev);
	}

	return success;
}

static void
save_history_for_delete_or_backspace (EHTMLEditorView *view,
                                      gboolean delete_key,
                                      gboolean control_key)
{
	EHTMLEditorSelection *selection;
	EHTMLEditorViewHistoryEvent *ev = NULL;
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment = NULL;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection)) {
		g_object_unref (dom_selection);
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	/* Check if we can delete something */
	if (webkit_dom_range_get_collapsed (range, NULL)) {
		WebKitDOMRange *tmp_range;

		webkit_dom_dom_selection_modify (
			dom_selection, "move", delete_key ? "right" : "left", "character");

		tmp_range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		if (webkit_dom_range_compare_boundary_points (tmp_range, 2 /* END_TO_END */, range, NULL) == 0) {
			g_object_unref (dom_selection);
			g_object_unref (range);
			g_object_unref (tmp_range);

			return;
		}

		webkit_dom_dom_selection_modify (
			dom_selection, "move", delete_key ? "left" : "right", "character");
	}

	if (save_history_before_event_in_table (view, range)) {
		g_object_unref (range);
		g_object_unref (dom_selection);
		return;
	}

	selection = e_html_editor_view_get_selection (view);

	ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
	ev->type = HISTORY_DELETE;

	e_html_editor_selection_get_selection_coordinates (
		selection, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
	g_object_unref (range);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	if (webkit_dom_range_get_collapsed (range, NULL)) {
		gboolean removing_from_anchor = FALSE;
		WebKitDOMRange *range_clone;
		WebKitDOMNode *node, *next_block = NULL;

		block_selection_changed_callbacks (view);

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
				if (!tmp_sibling || (WEBKIT_DOM_IS_HTMLBR_ELEMENT (tmp_sibling) &&
				    !element_has_class (WEBKIT_DOM_ELEMENT (tmp_sibling), "-x-evo-wrap-br")))
					sibling = WEBKIT_DOM_NODE (selection_marker);
			}

			if (e_html_editor_node_is_selection_position_node (sibling)) {
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
					if (webkit_dom_range_compare_boundary_points (tmp_range, 2 /* END_TO_END */, actual_range, NULL) != 0) {
						WebKitDOMNode *actual_block;
						WebKitDOMNode *tmp_block;

						actual_block = e_html_editor_get_parent_block_node_from_child (container);

						tmp_block = delete_key ?
							webkit_dom_range_get_end_container (tmp_range, NULL) :
							webkit_dom_range_get_start_container (tmp_range, NULL);
						tmp_block = e_html_editor_get_parent_block_node_from_child (tmp_block);

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
								"history-concatenating-blocks",
								GINT_TO_POINTER (1));
						}
					}
					g_object_unref (tmp_range);
					g_object_unref (actual_range);
				}
			} else {
				glong offset;

				/* FIXME This code is wrong for unicode smileys. */
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
			g_object_set_data (
				G_OBJECT (fragment),
				"history-removing-from-anchor",
				GINT_TO_POINTER (1));
		node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
		if (!node) {
			g_free (ev);
			unblock_selection_changed_callbacks (view);
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
					e_html_editor_selection_restore (selection);

				tmp_range = webkit_dom_range_clone_range (range_clone, NULL);
				/* Prepare the selection to the right position after
				 * delete and save it. */
				webkit_dom_range_collapse (range_clone, TRUE, NULL);
				webkit_dom_dom_selection_remove_all_ranges (dom_selection);
				webkit_dom_dom_selection_add_range (dom_selection, range_clone);
				e_html_editor_selection_get_selection_coordinates (
					selection, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
				/* Restore the selection where it was before the
				 * history event was saved. */
				webkit_dom_range_collapse (tmp_range, FALSE, NULL);
				webkit_dom_dom_selection_remove_all_ranges (dom_selection);
				webkit_dom_dom_selection_add_range (dom_selection, tmp_range);
				g_object_unref (tmp_range);

				if (selection_saved)
					e_html_editor_selection_save (selection);
			}
		} else {
			gboolean selection_saved = FALSE;

			if (webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker"))
				selection_saved = TRUE;

			if (selection_saved)
				e_html_editor_selection_restore (selection);

			if (delete_key) {
				e_html_editor_selection_get_selection_coordinates (
					selection, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
			} else {
				webkit_dom_dom_selection_modify (dom_selection, "move", "left", "character");
				e_html_editor_selection_get_selection_coordinates (
					selection, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
				webkit_dom_dom_selection_modify (dom_selection, "move", "right", "character");

				ev->after.end.x = ev->after.start.x;
				ev->after.end.y = ev->after.start.y;
			}

			if (selection_saved)
				e_html_editor_selection_save (selection);
		}

		g_object_unref (range_clone);

		if (delete_key) {
			if (!WEBKIT_DOM_IS_ELEMENT (node)) {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						create_selection_marker (document, FALSE)),
					webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
					NULL);
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						create_selection_marker (document, TRUE)),
					webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
					NULL);
			}
		} else {
			if (!WEBKIT_DOM_IS_ELEMENT (node)) {
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						create_selection_marker (document, TRUE)),
					NULL);
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						create_selection_marker (document, FALSE)),
					NULL);
			}
		}

		/* If concatenating two blocks with pressing Delete on the end
		 * of the previous one and the next node contain content that
		 * is wrapped on multiple lines, the last line will by separated
		 * by WebKit to the separate block. To avoid it let's remove
		 * all quoting and wrapping from the next paragraph. */
		if (next_block) {
			remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (next_block));
			remove_quoting_from_element (WEBKIT_DOM_ELEMENT (next_block));
		}

		unblock_selection_changed_callbacks (view);
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
				WebKitDOMNode *next_sibling;

				next_sibling = webkit_dom_node_get_next_sibling (node);
				if (next_sibling &&
				    (!WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling) ||
				     webkit_dom_node_get_next_sibling (next_sibling)))
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

	g_object_set_data (G_OBJECT (fragment), "history-delete-key", GINT_TO_POINTER (delete_key));
	g_object_set_data (G_OBJECT (fragment), "history-control-key", GINT_TO_POINTER (control_key));

	ev->data.fragment = fragment;
	e_html_editor_view_insert_new_history_event (view, ev);
}

static gboolean
fix_structure_after_delete_before_quoted_content (EHTMLEditorView *view,
                                                  GdkEventKey *event,
                                                  gboolean delete_key)
{
	EHTMLEditorSelection *selection;
	gboolean collapsed = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block, *node;

	selection = e_html_editor_view_get_selection (view);

	collapsed = e_html_editor_selection_is_collapsed (selection);

	e_html_editor_selection_save (selection);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return FALSE;

	if (collapsed) {
		WebKitDOMNode *next_block;

		block = e_html_editor_get_parent_block_node_from_child (
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
		if (node && !WEBKIT_DOM_IS_HTMLBR_ELEMENT (node))
			goto restore;
		else {
			if (event)
				save_history_for_delete_or_backspace (
					view, event->keyval == GDK_KEY_Delete, ((event)->state & GDK_CONTROL_MASK));

			/* Remove the empty block and move caret to the right place. */
			remove_node (block);

			if (delete_key) {
				/* To the beginning of the next block. */
				e_html_editor_selection_move_caret_into_element (
					document, WEBKIT_DOM_ELEMENT (next_block), TRUE);
			} else {
				WebKitDOMNode *prev_block;

				/* On the end of previous block. */
				prev_block = webkit_dom_node_get_previous_sibling (next_block);
				while (prev_block && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (prev_block))
					prev_block = webkit_dom_node_get_last_child (prev_block);

				if (prev_block)
					e_html_editor_selection_move_caret_into_element (
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

		block = e_html_editor_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		end_block = e_html_editor_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_end_marker));

		/* Situation where the start of the selection is in the beginning
		 * of the block in quoted content and the end in the beginning of
		 * content that is after the citation or the selection end is in
		 * the end of the quoted content (showed by ^). We have to
		 * mark the start block to correctly restore the structure
		 * afterwards.
		 *
		 * > |xxx
		 * > xxx^
		 * |xxx
		 * */
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
		if (!node || WEBKIT_DOM_IS_HTMLBR_ELEMENT (node)) {
			webkit_dom_element_set_id (
				WEBKIT_DOM_ELEMENT (block), "-x-evo-tmp-block");
		}
	}
 restore:
	if (event)
		save_history_for_delete_or_backspace (
			view, event->keyval == GDK_KEY_Delete, ((event)->state & GDK_CONTROL_MASK));

	e_html_editor_selection_restore (selection);

	return FALSE;
}

static gboolean
split_citation (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	EHTMLEditorViewHistoryEvent *ev = NULL;
	WebKitDOMElement *element;

	selection = e_html_editor_view_get_selection (view);

	if (!view->priv->undo_redo_in_progress) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_CITATION_SPLIT;

		e_html_editor_selection_get_selection_coordinates (
			selection, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);

		if (!e_html_editor_selection_is_collapsed (selection)) {
			WebKitDOMDocument *document;
			WebKitDOMDocumentFragment *fragment;
			WebKitDOMDOMWindow *dom_window;
			WebKitDOMDOMSelection *dom_selection;
			WebKitDOMRange *range;

			document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
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
			WebKitDOMDocument *document;
			WebKitDOMElement *selection_end;
			WebKitDOMNode *sibling;

			e_html_editor_selection_save (selection);

			document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
			selection_end = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end));
			if (!sibling || (WEBKIT_DOM_IS_HTMLBR_ELEMENT (sibling) &&
			    !element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-wrap-br"))) {
				WebKitDOMDocumentFragment *fragment;

				fragment = webkit_dom_document_create_document_fragment (document);
				ev->data.fragment = fragment;
			} else {
				ev->data.fragment = NULL;
			}

			e_html_editor_selection_restore (selection);
		}
	}

	element = insert_new_line_into_citation (view, "");

	if (ev) {
		e_html_editor_selection_get_selection_coordinates (
			selection, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);

		e_html_editor_view_insert_new_history_event (view, ev);
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
jump_to_next_table_cell (EHTMLEditorView *view,
                         gboolean jump_back)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMNode *node, *cell;
	WebKitDOMRange *range;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
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

static gboolean
delete_last_character_from_previous_line_in_quoted_block (EHTMLEditorView *view,
                                                          GdkEventKey *event)
{
	EHTMLEditorSelection *selection;
	EHTMLEditorViewHistoryEvent *ev = NULL;
	gboolean hidden_space = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment = NULL;
	WebKitDOMElement *element;
	WebKitDOMNode *node, *beginning, *prev_sibling;

	selection = e_html_editor_view_get_selection (view);

	/* We have to be in quoted content. */
	if (!e_html_editor_selection_is_citation (selection))
		return FALSE;

	/* Selection is just caret. */
	if (!e_html_editor_selection_is_collapsed (selection))
		return FALSE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	e_html_editor_selection_save (selection);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	/* Before the caret are just quote characters */
	beginning = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
	if (!(beginning && WEBKIT_DOM_IS_ELEMENT (beginning))) {
		WebKitDOMNode *parent;

		parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent))
			beginning = webkit_dom_node_get_previous_sibling (parent);
		else
			goto out;
	}

	/* Before the text is the beginning of line. */
	if (!(element_has_class (WEBKIT_DOM_ELEMENT (beginning), "-x-evo-quoted")))
		goto out;

	/* If we are just on the beginning of the line and not on the beginning of
	 * the block we need to remove the last character ourselves as well, otherwise
	 * WebKit will put the caret to wrong position. */
	if (!(prev_sibling = webkit_dom_node_get_previous_sibling (beginning)))
		goto out;

	if (event) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_DELETE;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		fragment = webkit_dom_document_create_document_fragment (document);
	}

	if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (prev_sibling)) {
		if (event)
			webkit_dom_node_append_child (WEBKIT_DOM_NODE (fragment), prev_sibling, NULL);
		else
			remove_node (prev_sibling);
	}

	prev_sibling = webkit_dom_node_get_previous_sibling (beginning);
	if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
	    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (prev_sibling), "data-hidden-space")) {
		hidden_space = TRUE;
		if (event)
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (fragment),
				prev_sibling,
				webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
				NULL);
		else
			remove_node (prev_sibling);
	}

	node = webkit_dom_node_get_previous_sibling (beginning);

	if (event)
		webkit_dom_node_append_child (WEBKIT_DOM_NODE (fragment), beginning, NULL);
	else
		remove_node (beginning);

	if (!hidden_space) {
		if (event) {
			gchar *data;

			data = webkit_dom_character_data_substring_data (
				WEBKIT_DOM_CHARACTER_DATA (node),
				webkit_dom_character_data_get_length (
					WEBKIT_DOM_CHARACTER_DATA (node)) -1,
				1,
				NULL);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				WEBKIT_DOM_NODE (
					webkit_dom_document_create_text_node (document, data)),
				NULL);

			g_free (data);
		}

		webkit_dom_character_data_delete_data (
			WEBKIT_DOM_CHARACTER_DATA (node),
			webkit_dom_character_data_get_length (
				WEBKIT_DOM_CHARACTER_DATA (node)) -1,
			1,
			NULL);
	}

	if (event) {
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		ev->data.fragment = fragment;
		e_html_editor_view_insert_new_history_event (view, ev);
	}

	e_html_editor_selection_restore (selection);

	return TRUE;
 out:
	e_html_editor_selection_restore (selection);

	return FALSE;
}

static gboolean
delete_last_character_on_line_in_quoted_block (EHTMLEditorView *view,
                                               GdkEventKey *event)
{
	EHTMLEditorSelection *selection;
	gboolean success = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMNode *node, *beginning, *next_sibling;

	selection = e_html_editor_view_get_selection (view);

	/* We have to be in quoted content. */
	if (!e_html_editor_selection_is_citation (selection))
		return FALSE;

	/* Selection is just caret. */
	if (!e_html_editor_selection_is_collapsed (selection))
		return FALSE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	e_html_editor_selection_save (selection);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	/* selection end marker */
	node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));

	/* We have to be on the end of line. */
	next_sibling = webkit_dom_node_get_next_sibling (node);
	if (next_sibling &&
	    (!WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling) ||
	     webkit_dom_node_get_next_sibling (next_sibling)))
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

	if (!webkit_dom_node_get_previous_sibling (beginning))
		goto out;

	if (event)
		save_history_for_delete_or_backspace (
			view, event->keyval == GDK_KEY_Delete, ((event)->state & GDK_CONTROL_MASK));

	element = webkit_dom_node_get_parent_element (beginning);
	remove_node (WEBKIT_DOM_NODE (element));

	success = TRUE;
 out:
	e_html_editor_selection_restore (selection);

	if (success)
		insert_new_line_into_citation (view, NULL);

	return success;
}

static void
remove_history_event (EHTMLEditorView *view,
		      GList *item)
{
	free_history_event_content (item->data);

	view->priv->history = g_list_delete_link (view->priv->history, item);
	view->priv->history_size--;
}

static gboolean
insert_tabulator (EHTMLEditorView *view)
{
	EHTMLEditorViewHistoryEvent *ev = NULL;
	EHTMLEditorSelection *selection;
	gboolean success;

	selection = e_html_editor_view_get_selection (view);

	if (!view->priv->undo_redo_in_progress) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_INPUT;

		if (!e_html_editor_selection_is_collapsed (selection)) {
			WebKitDOMRange *tmp_range;

			tmp_range = html_editor_view_get_dom_range (view);
			insert_delete_event (view, tmp_range);
			g_object_unref (tmp_range);
		}

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->before.end.x = ev->before.start.x;
		ev->before.end.y = ev->before.start.y;
	}

	success = e_html_editor_view_exec_command (
		view, E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, "\t");

	if (ev) {
		if (success) {
			WebKitDOMDocument *document;
			WebKitDOMElement *element;
			WebKitDOMDocumentFragment *fragment;

			e_html_editor_selection_get_selection_coordinates (
				selection,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);

			document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
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
				WEBKIT_DOM_NODE (create_selection_marker (document, TRUE)),
				NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				WEBKIT_DOM_NODE (create_selection_marker (document, FALSE)),
				NULL);
			ev->data.fragment = fragment;

			e_html_editor_view_insert_new_history_event (view, ev);
			e_html_editor_view_set_changed (view, TRUE);
		} else {
			remove_history_event (view, view->priv->history);
			remove_history_event (view, view->priv->history);
			g_free (ev);
		}
	}

	return success;
}

static gboolean
selection_is_in_empty_list_item (WebKitDOMNode *selection_start_marker)
{
	gchar *text;
	WebKitDOMNode *sibling;

	/* Selection needs to be collapsed. */
	sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_start_marker));
	if (!e_html_editor_node_is_selection_position_node (sibling))
		return FALSE;

	/* After the selection end there could be just the BR element. */
	sibling = webkit_dom_node_get_next_sibling (sibling);
	if (sibling && !WEBKIT_DOM_IS_HTMLBR_ELEMENT (sibling))
	       return FALSE;

	if (sibling && webkit_dom_node_get_next_sibling (sibling))
		return FALSE;

	sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker));

	/* Only text node with the zero width space character is allowed. */
	if (!WEBKIT_DOM_IS_TEXT (sibling))
		return FALSE;

	if (webkit_dom_node_get_previous_sibling (sibling))
		return FALSE;

	if (webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (sibling)) != 1)
		return FALSE;

	text = webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (sibling));
	if (!(text && g_strcmp0 (text, UNICODE_ZERO_WIDTH_SPACE) == 0)) {
		g_free (text);
		return FALSE;
	}

	g_free (text);

	return TRUE;
}

static gboolean
return_pressed_in_image_wrapper (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	EHTMLEditorViewHistoryEvent *ev = NULL;
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMElement *selection_start_marker;
	WebKitDOMNode *parent, *block, *clone;

	selection = e_html_editor_view_get_selection (view);
	if (!e_html_editor_selection_is_collapsed (selection))
		return FALSE;

	e_html_editor_selection_save (selection);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	if (!element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-resizable-wrapper")) {
		e_html_editor_selection_restore (selection);
		return FALSE;
	}

	if (!view->priv->undo_redo_in_progress) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_INPUT;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		fragment = webkit_dom_document_create_document_fragment (document);

		g_object_set_data (
			G_OBJECT (fragment), "history-return-key", GINT_TO_POINTER (1));
	}

	block = e_html_editor_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	clone = webkit_dom_node_clone_node (block, FALSE);
	webkit_dom_node_append_child (
		clone, WEBKIT_DOM_NODE (webkit_dom_document_create_element (document, "br", NULL)), NULL);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (block),
		clone,
		block,
		NULL);

	if (ev) {
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node (clone, TRUE),
			NULL);

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		ev->data.fragment = fragment;

		e_html_editor_view_insert_new_history_event (view, ev);
	}

	e_html_editor_view_set_changed (view, TRUE);

	e_html_editor_selection_restore (selection);

	return TRUE;
}

static gboolean
return_pressed_in_empty_list_item (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker;
	WebKitDOMNode *parent;

	selection = e_html_editor_view_get_selection (view);
	if (!e_html_editor_selection_is_collapsed (selection))
		return FALSE;

	e_html_editor_selection_save (selection);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	if (!WEBKIT_DOM_IS_HTMLLI_ELEMENT (parent)) {
		e_html_editor_selection_restore (selection);
		return FALSE;
	}

	if (selection_is_in_empty_list_item (WEBKIT_DOM_NODE (selection_start_marker))) {
		EHTMLEditorViewHistoryEvent *ev = NULL;
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMElement *paragraph;
		WebKitDOMNode *list;

		if (!view->priv->undo_redo_in_progress) {
			ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
			ev->type = HISTORY_INPUT;

			e_html_editor_selection_get_selection_coordinates (
				selection,
				&ev->before.start.x,
				&ev->before.start.y,
				&ev->before.end.x,
				&ev->before.end.y);

			fragment = webkit_dom_document_create_document_fragment (document);

			g_object_set_data (
				G_OBJECT (fragment), "history-return-key", GINT_TO_POINTER (1));
		}

		list = split_node_into_two (parent, -1);

		if (ev) {
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				parent,
				NULL);
		} else {
			remove_node (parent);
		}

		paragraph = prepare_paragraph (selection, document, TRUE);

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (list),
			WEBKIT_DOM_NODE (paragraph),
			list,
			NULL);

		if (ev) {
			e_html_editor_selection_get_selection_coordinates (
				selection,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);

			ev->data.fragment = fragment;

			e_html_editor_view_insert_new_history_event (view, ev);
		}

		e_html_editor_selection_restore (selection);

		e_html_editor_view_set_changed (view, TRUE);

		return TRUE;
	}

	e_html_editor_selection_restore (selection);

	return FALSE;
}

static void
process_smiley_on_delete_or_backspace (EHTMLEditorView *view)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMNode *parent;
	gboolean in_smiley = FALSE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	e_html_editor_selection_save (view->priv->selection);
	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
	if (WEBKIT_DOM_IS_ELEMENT (parent) &&
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-smiley-text"))
		in_smiley = TRUE;
	else {
		if (e_html_editor_selection_is_collapsed (view->priv->selection)) {
			WebKitDOMNode *prev_sibling;

			prev_sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
			if (prev_sibling && WEBKIT_DOM_IS_TEXT (prev_sibling)) {
				gchar *text = webkit_dom_character_data_get_data (
					WEBKIT_DOM_CHARACTER_DATA (prev_sibling));

				if (g_strcmp0 (text, UNICODE_ZERO_WIDTH_SPACE) == 0) {
					WebKitDOMNode *prev_prev_sibling;

					prev_prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);
					if (WEBKIT_DOM_IS_ELEMENT (prev_prev_sibling) &&
					    element_has_class (WEBKIT_DOM_ELEMENT (prev_prev_sibling), "-x-evo-smiley-wrapper")) {
						remove_node (prev_sibling);
						in_smiley = TRUE;
						parent = webkit_dom_node_get_last_child (prev_prev_sibling);
					}
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
		WebKitDOMNode *wrapper;

		wrapper = webkit_dom_node_get_parent_node (parent);
		if (!view->priv->html_mode) {
			WebKitDOMNode *child;

			while ((child = webkit_dom_node_get_first_child (parent)))
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (wrapper),
					child,
					wrapper,
					NULL);
		}
		/* In the HTML mode the whole smiley will be removed. */
		remove_node (wrapper);
		/* FIXME history will be probably broken here */
	}

	e_html_editor_selection_restore (view->priv->selection);
}

static gboolean
key_press_event_process_return_key (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	gboolean first_cell = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMNode *table = NULL;

	selection = e_html_editor_view_get_selection (view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

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
			add_selection_markers_into_element_start (
				document, WEBKIT_DOM_ELEMENT (node), NULL, NULL);
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (table),
				node,
				table,
				NULL);
			e_html_editor_selection_restore (selection);
			e_html_editor_view_set_changed (view, TRUE);
			return TRUE;
		}
	}

	/* When user presses ENTER in a citation block, WebKit does
	 * not break the citation automatically, so we need to use
	 * the special command to do it. */
	if (e_html_editor_selection_is_citation (selection)) {
		e_html_editor_view_remove_input_event_listener_from_body (view);
		if (split_citation (view)) {
			WebKitDOMRange *range;

			view->priv->return_key_pressed = TRUE;
			range = html_editor_view_get_dom_range (view);
			html_editor_view_check_magic_links (view, range, FALSE);
			view->priv->return_key_pressed = FALSE;
			g_object_unref (range);
			e_html_editor_view_set_changed (view, TRUE);

			return TRUE;
		}
		return FALSE;
	}

	/* If the ENTER key is pressed inside an empty list item then the list
	 * is broken into two and empty paragraph is inserted between lists. */
	if (return_pressed_in_empty_list_item (view))
		return TRUE;

	if (return_pressed_in_image_wrapper (view))
		return TRUE;

	return FALSE;
}

static gboolean
remove_empty_bulleted_list_item (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start;
	WebKitDOMNode *parent;

	selection = e_html_editor_view_get_selection (view);
	e_html_editor_selection_save (selection);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	selection_start = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start));
	while (parent && !node_is_list_or_item (parent))
		parent = webkit_dom_node_get_parent_node (parent);

	if (!parent)
		goto out;

	if (selection_is_in_empty_list_item (WEBKIT_DOM_NODE (selection_start))) {
		EHTMLEditorViewHistoryEvent *ev = NULL;
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMNode *prev_item;

		prev_item = webkit_dom_node_get_previous_sibling (parent);

		if (!view->priv->undo_redo_in_progress) {
			/* Insert new history event for Return to have the right coordinates.
			 * The fragment will be added later. */
			ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
			ev->type = HISTORY_DELETE;

			e_html_editor_selection_get_selection_coordinates (
				selection,
				&ev->before.start.x,
				&ev->before.start.y,
				&ev->before.end.x,
				&ev->before.end.y);

			fragment = webkit_dom_document_create_document_fragment (document);
		}

		if (ev) {
			if (prev_item)
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (fragment),
					webkit_dom_node_clone_node (prev_item, TRUE),
					NULL);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				parent,
				NULL);
		} else
			remove_node (parent);

		if (prev_item)
			add_selection_markers_into_element_end (
				document, WEBKIT_DOM_ELEMENT (prev_item), NULL, NULL);

		if (ev) {
			e_html_editor_selection_get_selection_coordinates (
				selection,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);

			ev->data.fragment = fragment;

			e_html_editor_view_insert_new_history_event (view, ev);
		}

		e_html_editor_view_set_changed (view, TRUE);
		e_html_editor_selection_restore (selection);

		return TRUE;
	}
 out:
	e_html_editor_selection_restore (selection);

	return FALSE;
}

static gboolean
key_press_event_process_backspace_key (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;

	selection = e_html_editor_view_get_selection (view);

	/* BackSpace pressed in the beginning of quoted content changes
	 * format to normal and inserts text into body */
	if (e_html_editor_selection_is_collapsed (selection)) {
		e_html_editor_selection_save (selection);
		if (change_quoted_block_to_normal (view) || delete_hidden_space (view)) {
			e_html_editor_selection_restore (selection);
			e_html_editor_view_force_spell_check_for_current_paragraph (view);
			e_html_editor_view_set_changed (view, TRUE);
			return TRUE;
		}
		e_html_editor_selection_restore (selection);
	}

	/* BackSpace in indented block decrease indent level by one */
	if (e_html_editor_selection_is_indented (selection) &&
	    e_html_editor_selection_is_collapsed (selection)) {
		WebKitDOMDocument *document;
		WebKitDOMElement *selection_start;
		WebKitDOMNode *prev_sibling;

		e_html_editor_selection_save (selection);
		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		selection_start = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		/* Empty text node before caret */
		prev_sibling = webkit_dom_node_get_previous_sibling (
			WEBKIT_DOM_NODE (selection_start));
		if (prev_sibling && WEBKIT_DOM_IS_TEXT (prev_sibling))
			if (webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (prev_sibling)) == 0)
				prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);

		e_html_editor_selection_restore (selection);
		if (!prev_sibling) {
			e_html_editor_selection_unindent (selection);
			e_html_editor_view_set_changed (view, TRUE);
			return TRUE;
		}
	}

	/* BackSpace pressed in an empty item in the bulleted list removes it. */
	if (!view->priv->html_mode && e_html_editor_selection_is_collapsed (selection) &&
	    remove_empty_bulleted_list_item (view))
		return TRUE;


	if (prevent_from_deleting_last_element_in_body (view))
		return TRUE;

	return FALSE;
}

static gboolean
key_press_event_process_delete_or_backspace_key (EHTMLEditorView *view,
                                                 gboolean delete,
                                                 GdkEventKey *event)
{
	gboolean local_delete;

	local_delete = (event && event->keyval == GDK_KEY_Delete) || delete;

	if (view->priv->magic_smileys) {
		/* If deleting something in a smiley it won't be a smiley
		 * anymore (at least from Evolution' POV), so remove all
		 * the elements that are hidden in the wrapper and leave
		 * just the text. Also this ensures that when a smiley is
		 * recognized and we press the BackSpace key we won't delete
		 * the UNICODE_HIDDEN_SPACE, but we will correctly delete
		 * the last character of smiley. */
		process_smiley_on_delete_or_backspace (view);
	}

	if (!local_delete && !view->priv->html_mode &&
	    delete_last_character_on_line_in_quoted_block (view, event))
		goto out;

	if (!local_delete && !view->priv->html_mode &&
	    delete_last_character_from_previous_line_in_quoted_block (view, event))
		goto out;

	if (fix_structure_after_delete_before_quoted_content (view, event, local_delete))
		goto out;

	if (local_delete) {
		EHTMLEditorSelection *selection;
		WebKitDOMDocument *document;
		WebKitDOMElement *selection_start_marker;
		WebKitDOMNode *sibling, *block, *next_block;

		selection = e_html_editor_view_get_selection (view);
		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

		/* This needs to be performed just in plain text mode
		 * and when the selection is collapsed. */
		if (view->priv->html_mode || !e_html_editor_selection_is_collapsed (selection))
			return FALSE;

		e_html_editor_selection_save (selection);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		sibling = webkit_dom_node_get_previous_sibling (
			WEBKIT_DOM_NODE (selection_start_marker));
		/* Check if the key was pressed in the beginning of block. */
		if (!(sibling && WEBKIT_DOM_IS_ELEMENT (sibling) &&
		      element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-quoted"))) {
			e_html_editor_selection_restore (selection);
			return FALSE;
		}

		sibling = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (selection_start_marker));
		sibling = webkit_dom_node_get_next_sibling (sibling);

		/* And also the current block was empty. */
		if (!(!sibling || (sibling && WEBKIT_DOM_IS_HTMLBR_ELEMENT (sibling) &&
		      !element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-wrap-br")))) {
			e_html_editor_selection_restore (selection);
			return FALSE;
		}

		block = e_html_editor_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		next_block = webkit_dom_node_get_next_sibling (block);

		remove_node (block);

		e_html_editor_selection_move_caret_into_element (
			document, WEBKIT_DOM_ELEMENT (next_block), TRUE);

		goto out;
	} else {
		/* Concatenating a non-quoted block with Backspace key to the
		 * previous block that is inside a quoted content. */
		EHTMLEditorSelection *selection;
		WebKitDOMDocument *document;
		WebKitDOMElement *selection_start_marker;
		WebKitDOMNode *node, *block, *prev_block, *last_child, *child;

		selection = e_html_editor_view_get_selection (view);
		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

		if (view->priv->html_mode || !e_html_editor_selection_is_collapsed (selection) ||
		    e_html_editor_selection_is_citation (selection))
			return FALSE;

		e_html_editor_selection_save (selection);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker));
		if (node) {
			e_html_editor_selection_restore (selection);
			return FALSE;
		}

		remove_empty_blocks (document);

		block = e_html_editor_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		prev_block = webkit_dom_node_get_previous_sibling (block);
		if (!prev_block || !WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (prev_block)) {
			e_html_editor_selection_restore (selection);
			return FALSE;
		}

		last_child = webkit_dom_node_get_last_child (prev_block);
		while (last_child && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (last_child))
			last_child = webkit_dom_node_get_last_child (last_child);

		if (!last_child) {
			e_html_editor_selection_restore (selection);
			return FALSE;
		}

		remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (last_child));
		remove_quoting_from_element (WEBKIT_DOM_ELEMENT (last_child));

		node = webkit_dom_node_get_last_child (last_child);
		if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (node))
			remove_node (node);

		while ((child = webkit_dom_node_get_first_child (block)))
			webkit_dom_node_append_child (last_child, child, NULL);

		remove_node (block);

		if (WEBKIT_DOM_IS_ELEMENT (last_child))
			wrap_and_quote_element (view, WEBKIT_DOM_ELEMENT (last_child));

		e_html_editor_selection_restore (selection);

		goto out;
	}

	return FALSE;
 out:
	e_html_editor_view_force_spell_check_for_current_paragraph (view);
	e_html_editor_view_set_changed (view, TRUE);

	return TRUE;
}

static gboolean
html_editor_view_key_press_event (GtkWidget *widget,
                                  GdkEventKey *event)
{
	EHTMLEditorView *view = E_HTML_EDITOR_VIEW (widget);

	view->priv->dont_save_history_in_body_input = FALSE;

	view->priv->return_key_pressed = FALSE;
	view->priv->space_key_pressed = FALSE;

	if (event->keyval == GDK_KEY_Menu) {
		gboolean event_handled;

		g_signal_emit (
			widget, signals[POPUP_EVENT],
			0, event, &event_handled);

		return event_handled;
	}

	if (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab) {
		if (jump_to_next_table_cell (view, event->keyval == GDK_KEY_ISO_Left_Tab))
			return TRUE;

		if (event->keyval == GDK_KEY_Tab)
			return insert_tabulator (view);
		else
			return FALSE;
	}

	if (is_return_key (event) && key_press_event_process_return_key (view))
		return TRUE;

	if (event->keyval == GDK_KEY_BackSpace &&
	    key_press_event_process_backspace_key (view)) {
		return TRUE;
	}

	if ((event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_BackSpace) &&
	     key_press_event_process_delete_or_backspace_key (view, event->keyval == GDK_KEY_Delete, event)) {
		return TRUE;
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_html_editor_view_parent_class)->
		key_press_event (widget, event);
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

static gboolean
html_editor_view_image_exists_in_cache (const gchar *image_uri)
{
	gchar *filename;
	gchar *hash;
	gboolean exists = FALSE;

	if (!emd_global_http_cache)
		return FALSE;

	hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, image_uri, -1);
	filename = camel_data_cache_get_filename (
		emd_global_http_cache, "http", hash);

	if (filename != NULL) {
		exists = g_file_test (filename, G_FILE_TEST_EXISTS);
		g_free (filename);
	}

	g_free (hash);

	return exists;
}

static gchar *
html_editor_view_redirect_uri (EHTMLEditorView *view,
                               const gchar *uri)
{
	EImageLoadingPolicy image_policy;
	GSettings *settings;
	gboolean uri_is_http;

	uri_is_http =
		g_str_has_prefix (uri, "http:") ||
		g_str_has_prefix (uri, "https:") ||
		g_str_has_prefix (uri, "evo-http:") ||
		g_str_has_prefix (uri, "evo-https:");

	/* Redirect http(s) request to evo-http(s) protocol.
	 * See EMailRequest for further details about this. */
	if (uri_is_http) {
		gchar *new_uri;
		SoupURI *soup_uri;
		gboolean image_exists;

		/* Check Evolution's cache */
		image_exists = html_editor_view_image_exists_in_cache (uri);

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		image_policy = g_settings_get_enum (settings, "image-loading-policy");
		g_object_unref (settings);
		/* If the URI is not cached and we are not allowed to load it
		 * then redirect to invalid URI, so that webkit would display
		 * a native placeholder for it. */
		if (!image_exists && (image_policy == E_IMAGE_LOADING_POLICY_NEVER)) {
			return g_strdup ("about:blank");
		}

		new_uri = g_strconcat ("evo-", uri, NULL);
		soup_uri = soup_uri_new (new_uri);
		g_free (new_uri);

		new_uri = soup_uri_to_string (soup_uri, FALSE);

		soup_uri_free (soup_uri);

		return new_uri;
	}

	return g_strdup (uri);
}

static void
html_editor_view_resource_requested (WebKitWebView *web_view,
                                     WebKitWebFrame *frame,
                                     WebKitWebResource *resource,
                                     WebKitNetworkRequest *request,
                                     WebKitNetworkResponse *response,
                                     gpointer user_data)
{
	const gchar *original_uri;

	original_uri = webkit_network_request_get_uri (request);

	if (original_uri != NULL) {
		gchar *redirected_uri;

		redirected_uri = html_editor_view_redirect_uri (
			E_HTML_EDITOR_VIEW (web_view), original_uri);

		webkit_network_request_set_uri (request, redirected_uri);

		g_free (redirected_uri);
	}
}

static void
e_html_editor_view_class_init (EHTMLEditorViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = html_editor_view_get_property;
	object_class->set_property = html_editor_view_set_property;
	object_class->dispose = html_editor_view_dispose;
	object_class->finalize = html_editor_view_finalize;
	object_class->constructed = html_editor_view_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = html_editor_view_button_press_event;
	widget_class->button_release_event = html_editor_view_button_release_event;
	widget_class->key_press_event = html_editor_view_key_press_event;

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
	 * images or Unicode characters is enabled.
	 */
	g_object_class_install_property (
		object_class,
		PROP_MAGIC_SMILEYS,
		g_param_spec_boolean (
			"magic-smileys",
			"Magic Smileys",
			"Convert emoticons to images or Unicode characters as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorView:unicode-smileys
	 *
	 * Determines whether Unicode characters should be used for smileys.
	 */
	g_object_class_install_property (
		object_class,
		PROP_UNICODE_SMILEYS,
		g_param_spec_boolean (
			"unicode-smileys",
			"Unicode Smileys",
			"Use Unicode characters for smileys",
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
	skip = WEBKIT_DOM_IS_HTMLBR_ELEMENT (node);
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
		if (e_html_editor_node_is_selection_position_node (node))
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

		if (e_html_editor_node_is_selection_position_node (node)) {
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
				WEBKIT_DOM_IS_HTMLBR_ELEMENT (prev_sibling) &&
				element_has_class (
					WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-wrap-br");

			if (!prev_sibling || wrap_br) {
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);
				if (!prev_sibling && next_sibling && WEBKIT_DOM_IS_TEXT (next_sibling))
					suppress_next = TRUE;
			}

			if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (prev_sibling) && !wrap_br)
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

		if (!WEBKIT_DOM_IS_HTMLBR_ELEMENT (node)) {
			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling)) {
				move_next = TRUE;
				goto next_node;
			}
			goto not_br;
		} else if (element_has_id (WEBKIT_DOM_ELEMENT (node), "-x-evo-first-br") ||
		           element_has_id (WEBKIT_DOM_ELEMENT (node), "-x-evo-last-br")) {
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

		if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (prev_sibling)) {
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

		if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (node) &&
		    !next_sibling && WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    e_html_editor_node_is_selection_position_node (prev_sibling)) {
			insert_quote_symbols_before_node (
				document, node, quote_level, FALSE);
			goto next_node;
		}

		if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (node)) {
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
e_html_editor_view_quote_plain_text_element (EHTMLEditorView *view,
                                             WebKitDOMElement *element)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *element_clone;
	WebKitDOMNodeList *list;
	gint ii, length, level;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));

	element_clone = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (element), TRUE);
	level = get_citation_level (WEBKIT_DOM_NODE (element), TRUE);

	/* Remove old quote characters if the exists */
	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (element_clone), "span.-x-evo-quoted", NULL);
	length = webkit_dom_node_list_get_length (list);
	for  (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);

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
e_html_editor_view_quote_plain_text (EHTMLEditorView *view)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitDOMNode *body_clone;
	WebKitDOMNamedNodeMap *attributes;
	WebKitDOMNodeList *list;
	WebKitDOMElement *element;
	gint ii, length;
	gulong attributes_length;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

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

		if (prev_sibling && WEBKIT_DOM_IS_HTMLBR_ELEMENT (prev_sibling))
			remove_node (prev_sibling);

		if (next_sibling && WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling))
			remove_node (next_sibling);

		if (webkit_dom_node_has_child_nodes (blockquote)) {
			WebKitDOMNode *child = webkit_dom_node_get_first_child (blockquote);
			if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (child))
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
void
e_html_editor_view_dequote_plain_text (EHTMLEditorView *view)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *paragraphs;
	gint length, ii;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

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
		g_object_unref (element);
	}
	g_object_unref (paragraphs);
}

static gboolean
create_anchor_for_link (const GMatchInfo *info,
                        GString *res,
                        gpointer data)
{
	gboolean link_surrounded, with_nbsp = FALSE;
	gint offset = 0, truncate_from_end = 0;
	gint match_start, match_end;
	gchar *match_with_nbsp, *match_without_nbsp;
	const gchar *end_of_match = NULL;
	const gchar *match, *match_extra_characters;

	match_with_nbsp = g_match_info_fetch (info, 1);
	/* E-mail addresses will be here. */
	match_without_nbsp = g_match_info_fetch (info, 0);

	if (!match_with_nbsp || (strstr (match_with_nbsp, "&nbsp;") && !g_str_has_prefix (match_with_nbsp, "&nbsp;"))) {
		match = match_without_nbsp;
		match_extra_characters = match_with_nbsp;
		g_match_info_fetch_pos (info, 0, &match_start, &match_end);
		with_nbsp = TRUE;
	} else {
		match = match_with_nbsp;
		match_extra_characters = match_without_nbsp;
		g_match_info_fetch_pos (info, 1, &match_start, &match_end);
	}

	if (g_str_has_prefix (match, "&nbsp;"))
		offset += 6;

	end_of_match = match + match_end - match_start - 1;
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

	if (!with_nbsp && match_extra_characters)
		g_string_append (res, match_extra_characters + (match_end - match_start));

	g_free (match_with_nbsp);
	g_free (match_without_nbsp);

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
create_and_append_new_block (EHTMLEditorSelection *selection,
                             WebKitDOMDocument *document,
                             WebKitDOMElement *parent,
                             WebKitDOMElement *block_template,
                             const gchar *content)
{
	WebKitDOMElement *block;

	if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (block_template))
		block = e_html_editor_selection_get_paragraph_element (
			selection, document, -1, 0);
	else
		block = WEBKIT_DOM_ELEMENT (webkit_dom_node_clone_node (
			WEBKIT_DOM_NODE (block_template), FALSE));

	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (block),
		content,
		NULL);

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
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (decode), line_text, NULL);

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

static void
replace_selection_markers (gchar **text)
{
	if (!text)
		return;

	if (strstr (*text, "##SELECTION_START##")) {
		GString *tmp;

		tmp = e_str_replace_string (
			*text,
			"##SELECTION_START##",
			"<span id=\"-x-evo-selection-start-marker\"></span>");

		g_free (*text);
		*text = g_string_free (tmp, FALSE);
	}

	if (strstr (*text, "##SELECTION_END##")) {
		GString *tmp;

		tmp = e_str_replace_string (
			*text,
			"##SELECTION_START##",
			"<span id=\"-x-evo-selection-start-marker\"></span>");

		g_free (*text);
		*text = g_string_free (tmp, FALSE);
	}
}

/* This parses the HTML code (that contains just text, &nbsp; and BR elements)
 * into blocks.
 * HTML code in that format we can get by taking innerText from some element,
 * setting it to another one and finally getting innerHTML from it */
static void
parse_html_into_blocks (EHTMLEditorView *view,
                        WebKitDOMDocument *document,
                        WebKitDOMElement *parent,
                        WebKitDOMElement *passed_block_template,
                        const gchar *html)
{
	EHTMLEditorSelection *selection;
	gboolean ignore_next_br = FALSE;
	gboolean first_element = TRUE;
	gboolean citation_was_first_element = FALSE;
	gboolean preserve_next_line = FALSE;
	gboolean has_citation = FALSE;
	gboolean previously_had_empty_citation_start = FALSE;
	const gchar *prev_br, *next_br;
	GRegex *regex_nbsp = NULL, *regex_link = NULL, *regex_email = NULL;
	WebKitDOMElement *block = NULL, *block_template = passed_block_template;

	selection = e_html_editor_view_get_selection (view);

	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (parent), "", NULL);

	if (!block_template) {
		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent)) {
			gboolean use_paragraphs;
			GSettings *settings;

			settings = e_util_ref_settings ("org.gnome.evolution.mail");

			use_paragraphs = g_settings_get_boolean (
				settings, "composer-wrap-quoted-text-in-replies");

			if (use_paragraphs)
				block_template = e_html_editor_selection_get_paragraph_element (
					selection, document, -1, 0);
			else
				block_template = webkit_dom_document_create_element (document, "pre", NULL);

			g_object_unref (settings);
		} else
			block_template = e_html_editor_selection_get_paragraph_element (
				selection, document, -1, 0);
	}

	prev_br = html;
	next_br = strstr (prev_br, "<br>");

	/* Replace single spaces on the beginning of line, 2+ spaces and
	 * tabulators with non breaking spaces */
	regex_nbsp = g_regex_new ("^\\s{1}|\\s{2,}|\x9|\\s$", 0, 0, NULL);

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
					selection, document, parent, block_template, "<br>");
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

			replace_selection_markers (&rest_to_insert);

			if (strchr (" +-@*=\t;#", *rest))
				preserve_block = FALSE;

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
					G_REGEX_MATCH_NOTEMPTY,
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
					selection, document, parent, block_template, "<br>");
			} else if (preserve_block) {
				gchar *html;
				gchar *content_to_append;

				if (!block) {
					if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (block_template))
						block = e_html_editor_selection_get_paragraph_element (
							selection, document, -1, 0);
					else
						block = WEBKIT_DOM_ELEMENT (webkit_dom_node_clone_node (
							WEBKIT_DOM_NODE (block_template), FALSE));
				}

				html = webkit_dom_html_element_get_inner_html (
					WEBKIT_DOM_HTML_ELEMENT (block));

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
					selection, document, parent, block_template, rest_to_insert);
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
					selection, document, parent, block_template, "<br>");

				citation_was_first_element = FALSE;
			} else if (first_element && !citation_was_first_element) {
				block = create_and_append_new_block (
					selection,
					document,
					parent,
					block_template,
					"<br id=\"-x-evo-first-br\">");
			} else
				preserve_next_line = FALSE;
		} else if (first_element && !citation_was_first_element) {
			block = create_and_append_new_block (
				selection,
				document,
				parent,
				block_template,
				"<br id=\"-x-evo-first-br\">");
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
				if (child && WEBKIT_DOM_IS_HTMLBR_ELEMENT (child)) {
					/* If the processed HTML contained just
					 * the BR don't overwrite its id. */
					if (!element_has_id (WEBKIT_DOM_ELEMENT (child), "-x-evo-first-br"))
						webkit_dom_element_set_id (
							WEBKIT_DOM_ELEMENT (child),
							"-x-evo-last-br");
				} else if (!view->priv->is_editting_message)
					create_and_append_new_block (
						selection, document, parent, block_template, "<br>");
			} else
				create_and_append_new_block (
					selection, document, parent, block_template, "<br>");
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

		replace_selection_markers (&rest_to_insert);

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
				G_REGEX_MATCH_NOTEMPTY,
				create_anchor_for_link,
				NULL,
				NULL);

			g_free (rest_to_insert);
			rest_to_insert = truncated;
		}

		if (g_strcmp0 (rest_to_insert, UNICODE_ZERO_WIDTH_SPACE) == 0)
			create_and_append_new_block (
				selection, document, parent, block_template, "<br>");
		else
			create_and_append_new_block (
				selection, document, parent, block_template, rest_to_insert);

		g_free (rest_to_insert);
	}

 end:
	if (has_citation) {
		gchar *inner_html;
		GString *start, *end;

		/* Replace text markers with actual HTML blockquotes */
		inner_html = webkit_dom_html_element_get_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (parent));
		start = e_str_replace_string (
			inner_html, "##CITATION_START##","<blockquote type=\"cite\">");
		end = e_str_replace_string (
			start->str, "##CITATION_END##", "</blockquote>");
		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (parent), end->str, NULL);

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
e_html_editor_view_insert_quoted_text (EHTMLEditorView *view,
                                       const gchar *text)
{
	EHTMLEditorSelection *selection;
	EHTMLEditorViewHistoryEvent *ev = NULL;
	gchar *escaped_text, *inner_html;
	WebKitDOMDocument *document;
	WebKitDOMElement *blockquote, *element, *selection_start;
	WebKitDOMNode *node;

	if (!text || !*text)
		return;

	selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	/* This is a trick to escape any HTML characters (like <, > or &).
	 * <textarea> automatically replaces all these unsafe characters
	 * by &lt;, &gt; etc. */
	element = webkit_dom_document_create_element (document, "textarea", NULL);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element), text, NULL);
	escaped_text = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element));

	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (element), escaped_text, NULL);

	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element));

	e_html_editor_selection_save (selection);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_PASTE_QUOTED;

		e_html_editor_selection_get_selection_coordinates (
			selection,
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
	node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start));
	/* Check if block is empty. If so, replace it otherwise insert the quoted
	 * content after current block. */
	if (!node || WEBKIT_DOM_IS_HTMLBR_ELEMENT (node)) {
		node = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (selection_start));
		node = webkit_dom_node_get_next_sibling (node);
		if (!node || WEBKIT_DOM_IS_HTMLBR_ELEMENT (node)) {
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

	parse_html_into_blocks (view, document, blockquote, NULL, inner_html);

	if (e_html_editor_view_get_html_mode (view)) {
		node = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (blockquote));
	} else {
		gint word_wrap_length;

		element_add_class (blockquote, "-x-evo-plaintext-quoted");
		word_wrap_length = e_html_editor_selection_get_word_wrap_length (selection);
		node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (blockquote));
		while (node) {
			WebKitDOMNode *next_sibling;

			node = WEBKIT_DOM_NODE (e_html_editor_selection_wrap_paragraph_length (
				selection, WEBKIT_DOM_ELEMENT (node), word_wrap_length - 2));

			webkit_dom_node_normalize (node);
			e_html_editor_view_quote_plain_text_element_after_wrapping (
				document, WEBKIT_DOM_ELEMENT (node), 1);

			next_sibling = webkit_dom_node_get_next_sibling (node);
			if (!next_sibling)
				break;

			node = next_sibling;
		}
	}

	add_selection_markers_into_element_end (
		document, WEBKIT_DOM_ELEMENT (node), NULL, NULL);

	e_html_editor_selection_restore (selection);

	if (ev) {
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}

	e_html_editor_view_force_spell_check_in_viewport (view);

	e_html_editor_view_set_changed (view, TRUE);

	g_free (escaped_text);
	g_free (inner_html);
}

static void
mark_citation (WebKitDOMElement *citation)
{
	webkit_dom_html_element_insert_adjacent_text (
		WEBKIT_DOM_HTML_ELEMENT (citation),
		"beforebegin",
		"##CITATION_START##",
		NULL);

	webkit_dom_html_element_insert_adjacent_text (
		WEBKIT_DOM_HTML_ELEMENT (citation),
		"afterend",
		"##CITATION_END##",
		NULL);

	element_add_class (citation, "marked");
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
create_text_markers_for_selection_in_element (WebKitDOMElement *element)
{
	WebKitDOMElement *selection_marker;

	selection_marker = webkit_dom_element_query_selector (
		element, "#-x-evo-selection-start-marker", NULL);
	if (selection_marker)
		webkit_dom_html_element_insert_adjacent_text (
			WEBKIT_DOM_HTML_ELEMENT (selection_marker),
			"afterend",
			"##SELECTION_START##",
			NULL);

	selection_marker = webkit_dom_element_query_selector (
		element, "#-x-evo-selection-end-marker", NULL);
	if (selection_marker)
		webkit_dom_html_element_insert_adjacent_text (
			WEBKIT_DOM_HTML_ELEMENT (selection_marker),
			"afterend",
			"##SELECTION_END##",
			NULL);
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
		e_html_editor_view_quote_plain_text_element_after_wrapping (
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
	disable_quote_marks_select (document);

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

		g_free (name);
		g_object_unref (node);
	}
	g_object_unref (attributes);
}

static void
body_compositionstart_event_cb (WebKitDOMElement *element,
                                WebKitDOMUIEvent *event,
                                EHTMLEditorView *view)
{
	view->priv->composition_in_progress = TRUE;
	e_html_editor_view_remove_input_event_listener_from_body (view);
}

static void
body_compositionend_event_cb (WebKitDOMElement *element,
                              WebKitDOMUIEvent *event,
                              EHTMLEditorView *view)
{
	view->priv->composition_in_progress = FALSE;
	e_html_editor_view_register_input_event_listener_on_body (view);
}

static void
register_html_events_handlers (EHTMLEditorView *view,
                               WebKitDOMHTMLElement *body)
{
	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"keydown",
		G_CALLBACK (body_keydown_event_cb),
		FALSE,
		view);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"keypress",
		G_CALLBACK (body_keypress_event_cb),
		FALSE,
		view);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"keyup",
		G_CALLBACK (body_keyup_event_cb),
		FALSE,
		view);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"compositionstart",
		G_CALLBACK (body_compositionstart_event_cb),
		FALSE,
		view);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"compositionend",
		G_CALLBACK (body_compositionend_event_cb),
		FALSE,
		view);
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
		dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (body), "data-style", "style");
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body),
			"style",
			"font-family: Monospace;",
			NULL);
	} else {
		dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (body), "style", "data-style");
	}
}

static void
html_editor_convert_view_content (EHTMLEditorView *view,
                                  const gchar *preferred_text)
{
	EHTMLEditorSelection *selection = e_html_editor_view_get_selection (view);
	gboolean start_bottom, empty = FALSE;
	gchar *inner_html;
	gint ii, length;
	GSettings *settings;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMElement *paragraph, *content_wrapper, *top_signature;
	WebKitDOMElement *cite_body, *signature, *wrapper;
	WebKitDOMHTMLElement *body;
	WebKitDOMNodeList *list;
	WebKitDOMNode *node;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	start_bottom = g_settings_get_boolean (settings, "composer-reply-start-bottom");
	g_object_unref (settings);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
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
	paragraph = prepare_paragraph (selection, document, TRUE);
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
			spacer = prepare_paragraph (selection, document, FALSE);
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

		element = e_html_editor_selection_get_paragraph_element (selection, document, -1, 0);
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
		e_html_editor_selection_set_paragraph_style (
			selection, WEBKIT_DOM_ELEMENT (node), -1, 0, "");
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (wrapper),
			node,
			WEBKIT_DOM_NODE (content_wrapper),
			NULL);

		g_object_unref (node);
	}
	g_object_unref (list);

	repair_gmail_blockquotes (document);
	create_text_markers_for_citations_in_element (WEBKIT_DOM_ELEMENT (body));

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

	if (inner_html && !*inner_html)
		empty = TRUE;

	length = webkit_dom_element_get_child_element_count (WEBKIT_DOM_ELEMENT (body));
	if (length <= 1) {
		empty = TRUE;
		if (length == 1) {
			WebKitDOMNode *child;

			child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
			empty = child && WEBKIT_DOM_IS_HTMLBR_ELEMENT (child);
		}
	}

	if (preferred_text && *preferred_text)
		empty = FALSE;

	if (!empty)
		parse_html_into_blocks (view, document, content_wrapper, NULL, inner_html);
	else
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (content_wrapper),
			WEBKIT_DOM_NODE (prepare_paragraph (selection, document, FALSE)),
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
	if (!view->priv->is_editting_message || view->priv->is_message_from_edit_as_new || !start_bottom) {
		WebKitDOMNode *child;

		remove_node (WEBKIT_DOM_NODE (paragraph));
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
		if (child)
			add_selection_markers_into_element_start (
				document, WEBKIT_DOM_ELEMENT (child), NULL, NULL);
	}

	if ((paragraph = webkit_dom_document_get_element_by_id (document, "-x-evo-last-br")))
		webkit_dom_element_remove_attribute (paragraph, "id");
	if ((paragraph = webkit_dom_document_get_element_by_id (document, "-x-evo-first-br")))
		webkit_dom_element_remove_attribute (paragraph, "id");

	merge_siblings_if_necessary (document, NULL);

	if (!e_html_editor_view_get_html_mode (view)) {
		e_html_editor_selection_wrap_paragraphs_in_document (
			selection, document);

		quote_plain_text_elements_after_wrapping_in_document (document);
	}

	clear_attributes (document);

	e_html_editor_selection_restore (selection);
	e_html_editor_view_force_spell_check_in_viewport (view);

	/* Register on input event that is called when the content (body) is modified */
	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"input",
		G_CALLBACK (body_input_event_cb),
		FALSE,
		view);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (dom_window),
		"scroll",
		G_CALLBACK (body_scroll_event_cb),
		FALSE,
		view);

	register_html_events_handlers (view, body);
	set_monospace_font_family_on_body (WEBKIT_DOM_ELEMENT (body), view->priv->html_mode);

	g_free (inner_html);
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

static void
html_editor_view_insert_converted_html_into_selection (EHTMLEditorView *view,
                                                       gboolean is_html,
                                                       const gchar *html)
{
	EHTMLEditorSelection *selection = e_html_editor_view_get_selection (view);
	gboolean has_selection;
	gchar *inner_html;
	gint citation_level;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *element;
	WebKitDOMNode *node;
	WebKitDOMNode *current_block;

	e_html_editor_view_remove_input_event_listener_from_body (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	e_html_editor_selection_save (selection);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");
	current_block = e_html_editor_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (current_block))
		current_block = NULL;

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

	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element));
	parse_html_into_blocks (view, document, element, WEBKIT_DOM_ELEMENT (current_block), inner_html);
	g_free (inner_html);

	has_selection = !e_html_editor_selection_is_collapsed (selection);
	if (has_selection && !view->priv->undo_redo_in_progress) {
		if (!view->priv->undo_redo_in_progress) {
			WebKitDOMRange *range;

			range = html_editor_view_get_dom_range (view);
			insert_delete_event (view, range);
			g_object_unref (range);
		}

		/* Remove the text that was meant to be replaced by the pasted text */
		e_html_editor_view_exec_command (
			view, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);

		e_html_editor_selection_save (selection);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		current_block = e_html_editor_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (current_block))
			current_block = NULL;
	}

	citation_level = get_citation_level (WEBKIT_DOM_NODE (selection_end_marker), FALSE);
	/* Pasting into the citation */
	if (citation_level > 0) {
		gint length;
		gint word_wrap_length = e_html_editor_selection_get_word_wrap_length (selection);
		WebKitDOMElement *br;
		WebKitDOMNode *first_paragraph, *last_paragraph;
		WebKitDOMNode *child, *parent, *current_block;

		first_paragraph = webkit_dom_node_get_first_child (
			WEBKIT_DOM_NODE (element));
		last_paragraph = webkit_dom_node_get_last_child (
			WEBKIT_DOM_NODE (element));

		length = word_wrap_length - 2 * citation_level;

		/* Pasting text that was parsed just into one paragraph */
		if (webkit_dom_node_is_same_node (first_paragraph, last_paragraph)) {
			WebKitDOMNode *child, *parent;

			parent = e_html_editor_get_parent_block_node_from_child (
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
				e_html_editor_selection_wrap_paragraph_length (
					selection, WEBKIT_DOM_ELEMENT (parent), length));
			webkit_dom_node_normalize (parent);
			e_html_editor_view_quote_plain_text_element_after_wrapping (
				document, WEBKIT_DOM_ELEMENT (parent), citation_level);

			e_html_editor_selection_restore (selection);

			g_object_unref (element);
			goto out;
		}

		/* Pasting content parsed into the multiple paragraphs */
		parent = e_html_editor_get_parent_block_node_from_child (
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
		if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (child))
			remove_node (child);

		parent = e_html_editor_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_end_marker));

		child = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (selection_end_marker));
		/* Move the elements that are in the same paragraph as the selection end
		 * on the end of pasted text, but avoid BR on the end of paragraph */
		while (child) {
			WebKitDOMNode *next_child =
				webkit_dom_node_get_next_sibling  (child);
			if (!(!next_child && WEBKIT_DOM_IS_HTMLBR_ELEMENT (child)))
				webkit_dom_node_append_child (last_paragraph, child, NULL);
			child = next_child;
		}

		current_block = e_html_editor_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		remove_selection_markers (document);

		/* Caret will be restored on the end of pasted text */
		webkit_dom_node_append_child (
			last_paragraph,
			WEBKIT_DOM_NODE (create_selection_marker (document, TRUE)),
			NULL);

		webkit_dom_node_append_child (
			last_paragraph,
			WEBKIT_DOM_NODE (create_selection_marker (document, FALSE)),
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
			child = WEBKIT_DOM_NODE (e_html_editor_selection_wrap_paragraph_length (
				selection, WEBKIT_DOM_ELEMENT (child), length));
			e_html_editor_view_quote_plain_text_element_after_wrapping (
				document, WEBKIT_DOM_ELEMENT (child), citation_level);
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (last_paragraph),
				child,
				last_paragraph,
				NULL);
		}

		webkit_dom_node_normalize (last_paragraph);

		last_paragraph = WEBKIT_DOM_NODE (
			e_html_editor_selection_wrap_paragraph_length (
				selection, WEBKIT_DOM_ELEMENT (last_paragraph), length));
		e_html_editor_view_quote_plain_text_element_after_wrapping (
			document, WEBKIT_DOM_ELEMENT (last_paragraph), citation_level);

		remove_quoting_from_element (WEBKIT_DOM_ELEMENT (parent));
		remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (parent));

		current_block = WEBKIT_DOM_NODE (e_html_editor_selection_wrap_paragraph_length (
			selection, WEBKIT_DOM_ELEMENT (current_block), length));
		e_html_editor_view_quote_plain_text_element_after_wrapping (
			document, WEBKIT_DOM_ELEMENT (current_block), citation_level);

		if ((br = webkit_dom_document_get_element_by_id (document, "-x-evo-last-br")))
			webkit_dom_element_remove_attribute (br, "class");

		if ((br = webkit_dom_document_get_element_by_id (document, "-x-evo-first-br")))
			webkit_dom_element_remove_attribute (br, "class");

		e_html_editor_selection_restore (selection);

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

	e_html_editor_view_exec_command (
		view, E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML, inner_html);

	if (g_str_has_suffix (inner_html, " "))
		e_html_editor_view_exec_command (
			view, E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, " ");
	g_free (inner_html);

	g_object_unref (element);
	e_html_editor_selection_save (selection);

	element = webkit_dom_document_query_selector (
		document, "* > br#-x-evo-first-br", NULL);
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
		document, "* > br#-x-evo-last-br", NULL);
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
				if (!(!next_child && WEBKIT_DOM_IS_HTMLBR_ELEMENT (child)))
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

		block = e_html_editor_get_parent_block_node_from_child (
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

		g_object_unref (clone1);
		g_object_unref (clone2);

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (selection_start_marker)),
			WEBKIT_DOM_NODE (selection_end_marker),
			webkit_dom_node_get_next_sibling (
				WEBKIT_DOM_NODE (selection_start_marker)),
			NULL);
	}

	e_html_editor_selection_restore (selection);
 out:
	e_html_editor_view_force_spell_check_in_viewport (view);
	e_html_editor_selection_scroll_to_caret (selection);

	e_html_editor_view_register_input_event_listener_on_body (view);
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
 * e_html_editor_view_exec_command:
 * @view: an #EHTMLEditorView
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
e_html_editor_view_exec_command (EHTMLEditorView *view,
                                 EHTMLEditorViewCommand command,
                                 const gchar *value)
{
	WebKitDOMDocument *document;
	const gchar *cmd_str = 0;
	gboolean has_value = FALSE;

	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

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

	view->priv->dont_save_history_in_body_input = TRUE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	return webkit_dom_document_exec_command (
		document, cmd_str, FALSE, has_value ? value : "" );
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
		webkit_dom_html_element_set_outer_html (
			WEBKIT_DOM_HTML_ELEMENT (quoted_node), text_content, NULL);

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
		webkit_dom_html_element_set_outer_html (
			WEBKIT_DOM_HTML_ELEMENT (quoted_node), text_content, NULL);

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
process_list_to_plain_text (EHTMLEditorView *view,
                            WebKitDOMElement *element,
                            gint level,
                            GString *output)
{
	EHTMLEditorSelectionBlockFormat format;
	EHTMLEditorSelectionAlignment alignment;
	gint counter = 1;
	gchar *indent_per_level = g_strnfill (SPACES_PER_LIST_LEVEL, ' ');
	WebKitDOMNode *item;
	gint word_wrap_length = e_html_editor_selection_get_word_wrap_length (
		e_html_editor_view_get_selection (view));

	format = get_list_format_from_node (WEBKIT_DOM_NODE (element));

	/* Process list items to plain text */
	item = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element));
	while (item) {
		if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (item))
			g_string_append (output, "\n");

		if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (item)) {
			gchar *space, *item_str = NULL;
			gint ii = 0;
			WebKitDOMElement *wrapped;
			GString *item_value = g_string_new ("");

			alignment = e_html_editor_selection_get_list_alignment_from_node (
				WEBKIT_DOM_NODE (item));

			wrapped = webkit_dom_element_query_selector (
				WEBKIT_DOM_ELEMENT (item), ".-x-evo-wrap-br", NULL);
			/* Wrapped text */
			if (wrapped) {
				WebKitDOMNode *node = webkit_dom_node_get_first_child (item);
				GString *line = g_string_new ("");
				while (node) {
					if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (node) &&
					    element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br")) {
						g_string_append (line, "\n");
						/* put spaces before line characters -> wordwraplength - indentation */
						for (ii = 0; ii < level; ii++)
							g_string_append (line, indent_per_level);
						if (WEBKIT_DOM_IS_HTMLO_LIST_ELEMENT (element))
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
					if (WEBKIT_DOM_IS_HTMLO_LIST_ELEMENT (element))
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
				if (WEBKIT_DOM_IS_HTMLU_LIST_ELEMENT (element))
					if (e_html_editor_dom_node_find_parent_element (item, "OL"))
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
					if (WEBKIT_DOM_IS_HTMLO_LIST_ELEMENT (element))
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
				view, WEBKIT_DOM_ELEMENT (item), level + 1, output);
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
	webkit_dom_element_remove_attribute (element, "data-style");
	webkit_dom_element_remove_attribute (element, "spellcheck");
}

static void
convert_element_from_html_to_plain_text (EHTMLEditorView *view,
                                         WebKitDOMElement *element,
                                         gboolean *wrap,
                                         gboolean *quote)
{
	gint blockquotes_count;
	gchar *inner_text, *inner_html;
	WebKitDOMDocument *document;
	WebKitDOMElement *top_signature, *signature, *blockquote, *main_blockquote;
	WebKitDOMNode *signature_clone, *from;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));

	top_signature = webkit_dom_element_query_selector (
		element, ".-x-evo-top-signature", NULL);
	signature = webkit_dom_element_query_selector (
		element, "span.-x-evo-signature", NULL);
	main_blockquote = webkit_dom_element_query_selector (
		element, "#-x-evo-main-cite", NULL);

	blockquote = webkit_dom_document_create_element (
		document, "blockquote", NULL);

	if (main_blockquote) {
		webkit_dom_element_set_attribute (
			blockquote, "type", "cite", NULL);
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

	blockquotes_count = create_text_markers_for_citations_in_element (WEBKIT_DOM_ELEMENT (from));
	create_text_markers_for_selection_in_element (WEBKIT_DOM_ELEMENT (from));

	inner_text = webkit_dom_html_element_get_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (from));

	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (blockquote), inner_text, NULL);

	inner_html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (blockquote));

	parse_html_into_blocks (
		view, document,
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
				webkit_dom_html_element_set_inner_html (
					WEBKIT_DOM_HTML_ELEMENT (first_child),
					"<br>",
					NULL);
			}
			add_selection_markers_into_element_start (
				document, WEBKIT_DOM_ELEMENT (first_child), NULL, NULL);
		}
	}

	if (wrap)
		*wrap = TRUE;
	if (quote)
		*quote = main_blockquote || blockquotes_count > 0;

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-converted", "", NULL);

	g_free (inner_text);
	g_free (inner_html);
}

static void
process_elements (EHTMLEditorView *view,
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
						e_html_editor_selection_get_word_wrap_length (
							e_html_editor_view_get_selection (view));

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
				content = webkit_dom_html_element_get_outer_html (
					WEBKIT_DOM_HTML_ELEMENT (child));
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
					if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (child))
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
					content = webkit_dom_html_element_get_outer_html (
						WEBKIT_DOM_HTML_ELEMENT (child));
					g_string_append (buffer, content);
					g_free (content);
				} else {
					process_list_to_plain_text (
						view, WEBKIT_DOM_ELEMENT (child), 1, buffer);
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
				content = webkit_dom_html_element_get_outer_html (
					WEBKIT_DOM_HTML_ELEMENT (child));
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

			/* Don't generate any text if the signature is set to None. */
			if (!changing_mode) {
				gchar *id;

				id = webkit_dom_element_get_id (WEBKIT_DOM_ELEMENT (first_child));
				if (g_strcmp0 (id, "none") == 0) {
					g_free (id);

					remove_node (child);
					skip_node = TRUE;
					goto next;
				}
				g_free (id);
			}

			if (!to_plain_text) {
				remove_base_attributes (
					WEBKIT_DOM_ELEMENT (child));
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

					if (last_pre_child && !WEBKIT_DOM_IS_HTMLBR_ELEMENT (last_pre_child)) {
						WebKitDOMElement *br;

						br = webkit_dom_document_create_element (document, "br", NULL);
						webkit_dom_node_append_child (
							pre_node, WEBKIT_DOM_NODE (br), NULL);
					}
					g_object_unref (pre_node);
				}
				g_object_unref (list_pre);

				convert_element_from_html_to_plain_text (
					view, WEBKIT_DOM_ELEMENT (first_child), NULL, NULL);
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
			}
			skip_node = TRUE;
			if (!to_plain_text && !changing_mode)
				skip_node = FALSE;
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
				content = webkit_dom_html_element_get_outer_html (
					WEBKIT_DOM_HTML_ELEMENT (child));
				g_string_append (buffer, content);
				g_free (content);
				skip_node = TRUE;
			}
			if (!to_plain_text)
				remove_evolution_attributes (WEBKIT_DOM_ELEMENT (child));
		}

		if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (child)) {
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
				view, child, changing_mode, to_plain_text, buffer);
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

		if (last_child && WEBKIT_DOM_IS_HTMLBR_ELEMENT (last_child))
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

void
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
remove_images_in_element (EHTMLEditorView *view,
                          WebKitDOMElement *element)
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
remove_images (EHTMLEditorView *view)
{
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	remove_images_in_element (
		view, WEBKIT_DOM_ELEMENT (webkit_dom_document_get_body (document)));
}

static void
toggle_smileys (EHTMLEditorView *view)
{
	gboolean html_mode;
	gint length;
	gint ii;
	WebKitDOMDocument *document;
	WebKitDOMNodeList *smileys;

	html_mode = e_html_editor_view_get_html_mode (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
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
toggle_paragraphs_style_in_element (EHTMLEditorView *view,
                                    WebKitDOMElement *element,
				    gboolean html_mode)
{
	EHTMLEditorSelection *selection;
	gint ii, length;
	WebKitDOMNodeList *paragraphs;

	selection = e_html_editor_view_get_selection (view);

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

				offset = WEBKIT_DOM_IS_HTMLU_LIST_ELEMENT (node) ?
					SPACES_PER_LIST_LEVEL : SPACES_ORDERED_LIST_FIRST_LEVEL;
				/* In plain text mode the paragraphs have width limit */
				e_html_editor_selection_set_paragraph_style (
					selection, WEBKIT_DOM_ELEMENT (node), -1, -offset, "");
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
				e_html_editor_selection_set_paragraph_style (
					selection, WEBKIT_DOM_ELEMENT (node),
					-1, 0, style_to_add);

				g_free (style);
			}
		}
		g_object_unref (node);
	}
	g_object_unref (paragraphs);
}

static void
toggle_paragraphs_style (EHTMLEditorView *view)
{
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	toggle_paragraphs_style_in_element (
		view,
		WEBKIT_DOM_ELEMENT (webkit_dom_document_get_body (document)),
		view->priv->html_mode);
}

static gchar *
process_content_for_saving_as_draft (EHTMLEditorView *view,
                                     gboolean only_inner_body)
{
	gchar *content;
	gint ii, length;
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitDOMElement *document_element;
	WebKitDOMNodeList *list;
	WebKitDOMNode *document_element_clone;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
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

	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (document_element_clone), "#-x-evo-input-start", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (list, ii);
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "id");
		g_object_unref (node);
	}

	g_object_unref (list);

	if (only_inner_body) {
		WebKitDOMElement *body;

		body = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (document_element_clone), "body", NULL);
		content = webkit_dom_html_element_get_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (body));
	} else
		content = webkit_dom_html_element_get_outer_html (
			WEBKIT_DOM_HTML_ELEMENT (document_element_clone));

	webkit_dom_element_remove_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-evo-draft");

	return content;
}

static gchar *
process_content_for_mode_change (EHTMLEditorView *view)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *body;
	GString *plain_text;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));

	plain_text = g_string_sized_new (1024);

	webkit_dom_element_remove_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-user-colors");

	process_elements (view, body, TRUE, TRUE, plain_text);

	g_string_append (plain_text, "</body></html>");

	return g_string_free (plain_text, FALSE);
}

static gchar *
process_content_for_plain_text (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	gboolean wrap = FALSE, quote = FALSE, clean = FALSE;
	gboolean converted, is_from_new_message;
	gint length, ii;
	GString *plain_text;
	WebKitDOMDocument *document;
	WebKitDOMNode *body, *source;
	WebKitDOMNodeList *paragraphs;

	plain_text = g_string_sized_new (1024);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));
	converted = webkit_dom_element_has_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-converted");
	is_from_new_message = webkit_dom_element_has_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-new-message");
	source = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (body), TRUE);

	selection = e_html_editor_view_get_selection (view);

	e_html_editor_selection_save (selection);

	/* If composer is in HTML mode we have to move the content to plain version */
	if (view->priv->html_mode) {
		if (converted || is_from_new_message) {
			toggle_paragraphs_style_in_element (
				view, WEBKIT_DOM_ELEMENT (source), FALSE);
			remove_images_in_element (
				view, WEBKIT_DOM_ELEMENT (source));
			remove_background_images_in_document (
				document);
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
				g_object_unref (paragraph);
			}
			g_object_unref (paragraphs);

			convert_element_from_html_to_plain_text (
				view, div, &wrap, &quote);

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

				if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (item)) {
					e_html_editor_selection_wrap_paragraph (
						selection, WEBKIT_DOM_ELEMENT (item));
				}
				item = next_item;
			}
		} else if (!webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (paragraph), ".-x-evo-wrap-br,.-x-evo-quoted", NULL)) {
			/* Dont't try to wrap the already wrapped content. */
			e_html_editor_selection_wrap_paragraph (
				selection, WEBKIT_DOM_ELEMENT (paragraph));
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

	if (quote) {
		quote_plain_text_recursive (document, source, source, 0);
	} else if (view->priv->html_mode) {
		WebKitDOMElement *citation;

		citation = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (source), "blockquote[type=cite]", NULL);
		if (citation)
			quote_plain_text_recursive (document, source, source, 0);
	}

	process_elements (view, source, FALSE, TRUE, plain_text);

	if (clean)
		remove_node (source);
	else
		g_object_unref (source);

	e_html_editor_selection_restore (selection);

	/* Return text content between <body> and </body> */
	return g_string_free (plain_text, FALSE);
}

static gchar *
process_content_for_html (EHTMLEditorView *view)
{
	gint ii, length;
	gchar *html_content;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMNode *node, *document_clone;
	WebKitDOMNodeList *list;
	gboolean send_editor_colors = FALSE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	document_clone = webkit_dom_node_clone_node (
		WEBKIT_DOM_NODE (webkit_dom_document_get_document_element (document)), TRUE);
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-quote-style", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-a-color-style", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-a-color-style-visited", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));
	/* When the Ctrl + Enter is pressed for sending, the links are activated. */
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-style-a", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));
	node = WEBKIT_DOM_NODE (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "body", NULL));
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (node), "#-x-evo-selection-start-marker", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (node), "#-x-evo-selection-end-marker", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));

	send_editor_colors = g_settings_get_boolean (
		view->priv->mail_settings, "composer-inherit-theme-colors");

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

		dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (data_style_node), "data-style", "style");
		g_object_unref (data_style_node);
	}
	g_object_unref (list);

	process_elements (view, node, FALSE, FALSE, NULL);

	html_content = webkit_dom_html_element_get_outer_html (
		WEBKIT_DOM_HTML_ELEMENT (document_clone));

	g_object_unref (document_clone);

	return html_content;
}

static gboolean
show_lose_formatting_dialog (EHTMLEditorView *view)
{
	gboolean lose;
	GtkWidget *toplevel;
	GtkWindow *parent = NULL;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));

	if (GTK_IS_WINDOW (toplevel))
		parent = GTK_WINDOW (toplevel);

	lose = e_util_prompt_user (
		parent, "org.gnome.evolution.mail", "prompt-on-composer-mode-switch",
		"mail-composer:prompt-composer-mode-switch", NULL);

	if (!lose) {
		/* Nothing has changed, but notify anyway */
		g_object_notify (G_OBJECT (view), "html-mode");
		return FALSE;
	}

	return TRUE;
}

static void
convert_when_changing_composer_mode (EHTMLEditorView *view)
{
	EHTMLEditorSelection *selection;
	gboolean quote = FALSE, wrap = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;

	selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

	convert_element_from_html_to_plain_text (
		view, WEBKIT_DOM_ELEMENT (body), &wrap, &quote);

	if (wrap)
		e_html_editor_selection_wrap_paragraphs_in_document (selection, document);

	if (quote) {
		e_html_editor_selection_save (selection);
		if (wrap)
			quote_plain_text_elements_after_wrapping_in_document (
				document);
		else
			body = WEBKIT_DOM_HTML_ELEMENT (e_html_editor_view_quote_plain_text (view));
		e_html_editor_selection_restore (selection);
	}

	toggle_paragraphs_style (view);
	toggle_smileys (view);
	remove_images (view);
	remove_background_images_in_document (document);

	clear_attributes (document);

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-converted", "", NULL);

	/* Update fonts - in plain text we only want monospace */
	e_html_editor_view_update_fonts (view);

	e_html_editor_view_force_spell_check_in_viewport (view);
}

void
e_html_editor_view_embed_styles (EHTMLEditorView *view)
{
	WebKitWebSettings *settings;
	WebKitDOMDocument *document;
	WebKitDOMElement *sheet;
	gchar *stylesheet_uri;
	gchar *stylesheet_content;
	const gchar *stylesheet;
	gsize length;

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view));
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

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
e_html_editor_view_remove_embed_styles (EHTMLEditorView *view)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *sheet;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	sheet = webkit_dom_document_get_element_by_id (
		document, "-x-evo-composer-sheet");

	if (sheet)
		remove_node (WEBKIT_DOM_NODE (sheet));
}

static void
set_link_colors_in_document (WebKitDOMDocument *document,
                             GdkRGBA *color,
                             gboolean visited)
{
	gchar *color_str = NULL;
	const gchar *style_id;
	guint32 color_value;
	WebKitDOMHTMLHeadElement *head;
	WebKitDOMHTMLElement *body;
	WebKitDOMElement *style_element;

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

	color_value = e_rgba_to_value (color);
	color_str = g_strdup_printf (
		visited ? "a.-x-evo-visited-link { color: #%06x; }" : "a { color: #%06x; }", color_value);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (style_element), color_str, NULL);
	g_free (color_str);

	color_str = g_strdup_printf ("#%06x", color_value);
	if (visited)
		webkit_dom_html_body_element_set_v_link (
			WEBKIT_DOM_HTML_BODY_ELEMENT (body), color_str);
	else
		webkit_dom_html_body_element_set_link (
			WEBKIT_DOM_HTML_BODY_ELEMENT (body), color_str);
	g_free (color_str);
}

void
e_html_editor_view_set_link_color (EHTMLEditorView *view,
                                   GdkRGBA *color)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));
	g_return_if_fail (color != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	set_link_colors_in_document (document, color, FALSE);
}

void
e_html_editor_view_set_visited_link_color (EHTMLEditorView *view,
                                           GdkRGBA *color)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));
	g_return_if_fail (color != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	set_link_colors_in_document (document, color, TRUE);
}

static void
get_color_from_context (GtkStyleContext *context,
                        const gchar *name,
                        GdkRGBA *out_color)
{
	GdkColor *color = NULL;

	gtk_style_context_get_style (context, name, &color, NULL);

	if (color == NULL) {
		gboolean is_visited = strstr (name, "visited") != NULL;

		out_color->alpha = 1;
		out_color->red = is_visited ? 1 : 0;
		out_color->green = 0;
		out_color->blue = is_visited ? 0 : 1;

		#if GTK_CHECK_VERSION(3,12,0)
		gtk_style_context_get_color (context, is_visited ? GTK_STATE_FLAG_VISITED : GTK_STATE_FLAG_LINK, out_color);
		#endif
	} else {
		out_color->alpha = 1;
		out_color->red = ((gdouble) color->red) / G_MAXUINT16;
		out_color->green = ((gdouble) color->green) / G_MAXUINT16;
		out_color->blue = ((gdouble) color->blue) / G_MAXUINT16;

		gdk_color_free (color);
	}
}

static void
set_link_colors (EHTMLEditorView *view)
{
	GdkRGBA rgba;
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (GTK_WIDGET (view));

	get_color_from_context (context, "link-color", &rgba);
	e_html_editor_view_set_link_color (view, &rgba);

	get_color_from_context (context, "visited-link-color", &rgba);
	e_html_editor_view_set_visited_link_color (view, &rgba);
}

static void
style_updated_cb (EHTMLEditorView *view)
{
	GdkRGBA color;
	gchar *color_value;
	GtkStateFlags state_flags;
	GtkStyleContext *style_context;
	gboolean backdrop;
	WebKitDOMHTMLElement *body;
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

	if (webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (body), "data-user-colors")) {
		/* If the user set the colors in Page dialog, this callback is useless. */
		return;
	}

	state_flags = gtk_widget_get_state_flags (GTK_WIDGET (view));
	style_context = gtk_widget_get_style_context (GTK_WIDGET (view));
	backdrop = (state_flags & GTK_STATE_FLAG_BACKDROP) != 0;

	if (gtk_style_context_lookup_color (
			style_context,
			backdrop ? "theme_unfocused_base_color" : "theme_base_color",
			&color))
		color_value = g_strdup_printf ("#%06x", e_rgba_to_value (&color));
	else
		color_value = g_strdup (E_UTILS_DEFAULT_THEME_BASE_COLOR);


	webkit_dom_html_body_element_set_bg_color (
		WEBKIT_DOM_HTML_BODY_ELEMENT (body), color_value);

	g_free (color_value);

	if (gtk_style_context_lookup_color (
			style_context,
			backdrop ? "theme_unfocused_fg_color" : "theme_fg_color",
			&color))
		color_value = g_strdup_printf ("#%06x", e_rgba_to_value (&color));
	else
		color_value = g_strdup (E_UTILS_DEFAULT_THEME_FG_COLOR);

	webkit_dom_html_body_element_set_text (
		WEBKIT_DOM_HTML_BODY_ELEMENT (body), color_value);

	g_free (color_value);

	set_link_colors (view);
}

static void
html_editor_view_load_status_changed (EHTMLEditorView *view)
{
	glong ii, length;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMHTMLElement *body;
	WebKitDOMNodeList *list;
	WebKitLoadStatus status;

	status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (view));
	if (status != WEBKIT_LOAD_FINISHED)
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

		while ((op = g_queue_pop_head (view->priv->post_reload_operations))) {
			if (op->data_free_func)
				op->data_free_func (op->data);
			g_free (op);
		}

		g_queue_clear (view->priv->post_reload_operations);

		return;
	}

	view->priv->reload_in_progress = FALSE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
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
			dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (node), "style", "data-style");
		g_free (style_value);

		g_object_unref (node);
	}
	g_object_unref (list);

	if (view->priv->convert_in_situ) {
		html_editor_convert_view_content (view, NULL);
		/* Make the quote marks non-selectable. */
		disable_quote_marks_select (document);
		html_editor_view_set_links_active (view, FALSE);
		style_updated_cb (view);
		view->priv->convert_in_situ = FALSE;

		e_html_editor_view_register_input_event_listener_on_body (view);
		register_html_events_handlers (view, body);

		return;
	}

	/* Make the quote marks non-selectable. */
	disable_quote_marks_select (document);
	style_updated_cb (view);
	html_editor_view_set_links_active (view, FALSE);
	put_body_in_citation (document);
	move_elements_to_body (view);
	repair_gmail_blockquotes (document);

	if (webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (body), "data-evo-draft")) {
		/* Restore the selection how it was when the draft was saved */
		e_html_editor_selection_move_caret_into_element (
			document, WEBKIT_DOM_ELEMENT (body), FALSE);
		e_html_editor_selection_restore (
			e_html_editor_view_get_selection (view));
		e_html_editor_view_remove_embed_styles (view);
	}

	/* The composer body could be empty in some case (loading an empty string
	 * or empty HTML. In that case create the initial paragraph. */
	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body))) {
		EHTMLEditorSelection *selection;
		WebKitDOMElement *paragraph;

		selection = e_html_editor_view_get_selection (view);
		paragraph = prepare_paragraph (selection, document, TRUE);
		webkit_dom_element_set_id (paragraph, "-x-evo-input-start");
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (body), WEBKIT_DOM_NODE (paragraph), NULL);
		e_html_editor_selection_restore (selection);
	}

	/* Register on input event that is called when the content (body) is modified */
	e_html_editor_view_register_input_event_listener_on_body (view);
	register_html_events_handlers (view, body);

	if (view->priv->html_mode)
		change_cid_images_src_to_base64 (view);

	if (view->priv->inline_spelling)
		e_html_editor_view_force_spell_check (view);
	else
		e_html_editor_view_turn_spell_check_off (view);

	set_monospace_font_family_on_body (WEBKIT_DOM_ELEMENT (body), view->priv->html_mode);

	dom_window = webkit_dom_document_get_default_view (document);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (dom_window),
		"scroll",
		G_CALLBACK (body_scroll_event_cb),
		FALSE,
		view);
}

static void
wrap_paragraphs_in_quoted_content (EHTMLEditorSelection *selection,
                                   WebKitDOMDocument *document)
{
	gint ii, length;
	WebKitDOMNodeList *paragraphs;

	paragraphs = webkit_dom_document_query_selector_all (
		document, "blockquote[type=cite] > .-x-evo-paragraph", NULL);

	length = webkit_dom_node_list_get_length (paragraphs);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *paragraph;

		paragraph = webkit_dom_node_list_item (paragraphs, ii);

		e_html_editor_selection_wrap_paragraph (
			selection, WEBKIT_DOM_ELEMENT (paragraph));
		g_object_unref (paragraph);
	}
	g_object_unref (paragraphs);
}

void
e_html_editor_view_clear_history (EHTMLEditorView *view)
{
	EHTMLEditorViewHistoryEvent *ev;

	if (view->priv->history != NULL) {
		g_list_free_full (view->priv->history, (GDestroyNotify) free_history_event);
		view->priv->history = NULL;
	}

	view->priv->history_size = 0;
	view->priv->dont_save_history_in_body_input = FALSE;
	view->priv->undo_redo_in_progress = FALSE;

	ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
	ev->type = HISTORY_START;
	view->priv->history = g_list_append (view->priv->history, ev);

	view->priv->can_undo = FALSE;
	g_object_notify (G_OBJECT (view), "can-undo");
	view->priv->can_redo = FALSE;
	g_object_notify (G_OBJECT (view), "can-redo");
}

static void
toggle_tables (EHTMLEditorView *view)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list;
	gint ii, length;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	list = webkit_dom_document_query_selector_all (document, "table", NULL);
	length = webkit_dom_node_list_get_length (list);

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *table = webkit_dom_node_list_item (list, ii);

		if (view->priv->html_mode) {
			element_remove_class (WEBKIT_DOM_ELEMENT (table), "-x-evo-plaintext-table");
			dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (table), "data-width", "width");
			dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (table), "data-cellspacing", "cellspacing");
			dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (table), "data-cellpadding", "cellpadding");
			dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (table), "data-border", "border");
		} else {
			element_add_class (WEBKIT_DOM_ELEMENT (table), "-x-evo-plaintext-table");
			dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (table), "width", "data-width");
			dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (table), "cellspacing", "data-cellspacing");
			webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (table), "cellspacing", "0", NULL);
			dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (table), "cellpadding", "data-cellpadding");
			webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (table), "cellpadding", "0", NULL);
			dom_element_rename_attribute (WEBKIT_DOM_ELEMENT (table), "border", "data-border");
			webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (table), "border", "0", NULL);
		}
		g_object_unref (table);
	}
	g_object_unref (list);
}

static void
toggle_unordered_lists (EHTMLEditorView *view)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list;
	gint ii, length;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	list = webkit_dom_document_query_selector_all (document, "ul", NULL);
	length = webkit_dom_node_list_get_length (list);

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		if (view->priv->html_mode) {
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
	convert = convert && !is_from_new_message;

	/* If toggling from HTML to plain text mode, ask user first */
	if (convert && view->priv->html_mode && !html_mode) {
		if (!show_lose_formatting_dialog (view))
			return;

		view->priv->html_mode = html_mode;

		convert_when_changing_composer_mode (view);
		style_updated_cb (view);
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
		toggle_tables (view);
		toggle_unordered_lists (view);
		remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (body));

		g_object_notify (G_OBJECT (selection), "font-color");
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
		toggle_tables (view);
		toggle_unordered_lists (view);
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
			e_html_editor_view_force_spell_check_in_viewport (view);
		}

		g_free (plain);
	}

	style_updated_cb (view);
 out:
	set_monospace_font_family_on_body (WEBKIT_DOM_ELEMENT (body), view->priv->html_mode);

	e_html_editor_view_clear_history (view);

	g_object_notify (G_OBJECT (view), "html-mode");
}

void
e_html_editor_view_save_history_for_drop (EHTMLEditorView *view)
{
	EHTMLEditorViewHistoryEvent *event;
	gint ii, length;
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMNodeList *list;
	WebKitDOMRange *range;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

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

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	/* Remove the last inserted history event as this one was inserted in
	 * body_input_event_cb and is wrong as its type is HISTORY_INPUT. */
	/* FIXME we could probably disable the HTML input event callback while
	 * doing DnD within the view */
	if (((EHTMLEditorViewHistoryEvent *) (view->priv->history->data))->type == HISTORY_INPUT)
		remove_history_event (view, view->priv->history);

	event = g_new0 (EHTMLEditorViewHistoryEvent, 1);
	event->type = HISTORY_INSERT_HTML;

	/* Get the dropped content. It's easy as it is selected by WebKit. */
	fragment = webkit_dom_range_clone_contents (range, NULL);
	event->data.string.from = NULL;
	/* Get the HTML content of the dropped content. */
	event->data.string.to = get_node_inner_html (WEBKIT_DOM_NODE (fragment));

	e_html_editor_selection_get_selection_coordinates (
		view->priv->selection,
		&event->before.start.x,
		&event->before.start.y,
		&event->before.end.x,
		&event->before.end.y);

	event->before.end.x = event->before.start.x;
	event->before.end.y = event->before.start.y;

	if (length > 0)
		webkit_dom_dom_selection_collapse_to_start (dom_selection, NULL);
	else
		webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);

	e_html_editor_selection_get_selection_coordinates (
		view->priv->selection,
		&event->after.start.x,
		&event->after.start.y,
		&event->after.end.x,
		&event->after.end.y);

	e_html_editor_view_insert_new_history_event (view, event);

	if (!view->priv->html_mode) {
		WebKitDOMNodeList *list;
		gint ii, length;

		list = webkit_dom_document_query_selector_all (
			document, "span[style^=font-family]", NULL);
		length = webkit_dom_node_list_get_length (list);
		if (length > 0)
			e_html_editor_selection_save (view->priv->selection);

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
			e_html_editor_selection_restore (view->priv->selection);
	}

	e_html_editor_view_force_spell_check_in_viewport (view);

	g_object_unref (range);
	g_object_unref (dom_selection);
	g_object_unref (dom_window);
}

static void
html_editor_view_drag_end_cb (EHTMLEditorView *view,
                              GdkDragContext *context)
{
	e_html_editor_view_save_history_for_drop (view);
}

static void
html_editor_view_owner_change_clipboard_cb (GtkClipboard *clipboard,
                                            GdkEventOwnerChange *event,
                                            EHTMLEditorView *view)
{
	if (!E_IS_HTML_EDITOR_VIEW (view))
		return;

	if (view->priv->copy_action_triggered && event->owner)
		view->priv->copy_paste_clipboard_in_view = TRUE;
	else
		view->priv->copy_paste_clipboard_in_view = FALSE;

	view->priv->copy_action_triggered = FALSE;
}

static void
html_editor_view_owner_change_primary_cb (GtkClipboard *clipboard,
                                          GdkEventOwnerChange *event,
                                          EHTMLEditorView *view)
{
	if (!E_IS_HTML_EDITOR_VIEW (view))
		return;

	if (!event->owner || !view->priv->can_copy)
		view->priv->copy_paste_primary_in_view = FALSE;
}

static void
html_editor_view_copy_cut_clipboard_cb (EHTMLEditorView *view)
{
	view->priv->copy_action_triggered = TRUE;
}

static void
e_html_editor_view_init (EHTMLEditorView *view)
{
	WebKitWebSettings *settings;
	GSettings *g_settings;
	GSettingsSchema *settings_schema;
	ESpellChecker *checker;
	gchar **languages;
	gchar *comma_separated;
	const gchar *user_cache_dir;

	view->priv = E_HTML_EDITOR_VIEW_GET_PRIVATE (view);

	webkit_web_view_set_editable (WEBKIT_WEB_VIEW (view), TRUE);

	/* Override the spell-checker, use our own */
	checker = e_spell_checker_new ();
	webkit_set_text_checker (G_OBJECT (checker));
	g_object_unref (checker);

	/* Give spell check languages to WebKit */
	languages = e_spell_checker_list_active_languages (checker, NULL);
	comma_separated = g_strjoinv (",", languages);
	g_strfreev (languages);

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view));

	g_object_set (
		G_OBJECT (settings),
		"enable-developer-extras", TRUE,
		"enable-dom-paste", TRUE,
		"enable-file-access-from-file-uris", TRUE,
		"enable-plugins", FALSE,
		"enable-scripts", FALSE,
		"enable-spell-checking", TRUE,
		"respect-image-orientation", TRUE,
		"spell-checking-languages", comma_separated,
		NULL);

	g_free (comma_separated);

	webkit_web_view_set_settings (WEBKIT_WEB_VIEW (view), settings);

	view->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);

	/* Don't use CSS when possible to preserve compatibility with older
	 * versions of Evolution or other MUAs */
	e_html_editor_view_exec_command (
		view, E_HTML_EDITOR_VIEW_COMMAND_STYLE_WITH_CSS, "false");

	g_signal_connect (
		view, "drag-end",
		G_CALLBACK (html_editor_view_drag_end_cb), NULL);
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
		view, "resource-request-starting",
		G_CALLBACK (html_editor_view_resource_requested), NULL);
	g_signal_connect (
		view, "notify::load-status",
		G_CALLBACK (html_editor_view_load_status_changed), NULL);

	g_signal_connect (
		view, "style-updated",
		G_CALLBACK (style_updated_cb), NULL);

	g_signal_connect (
		view, "state-flags-changed",
		G_CALLBACK (style_updated_cb), NULL);

	view->priv->selection = g_object_new (
		E_TYPE_HTML_EDITOR_SELECTION,
		"html-editor-view", view,
		NULL);

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
			g_settings, "changed::antialiasing",
			G_CALLBACK (e_html_editor_settings_changed_cb), view);
		view->priv->aliasing_settings = g_settings;
	}

	view->priv->inline_images = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	view->priv->composition_in_progress = FALSE;

	g_signal_connect (
		view, "copy-clipboard",
		G_CALLBACK (html_editor_view_copy_cut_clipboard_cb), NULL);

	g_signal_connect (
		view, "cut-clipboard",
		G_CALLBACK (html_editor_view_copy_cut_clipboard_cb), NULL);

	view->priv->owner_change_primary_cb_id = g_signal_connect (
		gtk_clipboard_get (GDK_SELECTION_PRIMARY), "owner-change",
		G_CALLBACK (html_editor_view_owner_change_primary_cb), view);

	view->priv->owner_change_clipboard_cb_id = g_signal_connect (
		gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), "owner-change",
		G_CALLBACK (html_editor_view_owner_change_clipboard_cb), view);

	view->priv->history = NULL;
	e_html_editor_view_clear_history (view);

	e_html_editor_view_update_fonts (view);
	style_updated_cb (view);

	view->priv->body_input_event_removed = TRUE;
	view->priv->is_editting_message = FALSE;
	view->priv->is_message_from_draft = FALSE;
	view->priv->is_message_from_selection = FALSE;
	view->priv->is_message_from_edit_as_new = FALSE;
	view->priv->convert_in_situ = FALSE;
	view->priv->return_key_pressed = FALSE;
	view->priv->space_key_pressed = FALSE;
	view->priv->smiley_written = FALSE;
	view->priv->undo_redo_in_progress = FALSE;
	view->priv->dont_save_history_in_body_input = FALSE;
	view->priv->style_change_callbacks_blocked = FALSE;
	view->priv->selection_changed_callbacks_blocked = FALSE;
	view->priv->copy_paste_clipboard_in_view = FALSE;
	view->priv->copy_paste_primary_in_view = FALSE;
	view->priv->copy_action_triggered = FALSE;
	view->priv->pasting_primary_clipboard = FALSE;
	view->priv->renew_history_after_coordinates = TRUE;

	view->priv->spell_check_on_scroll_event_source_id = 0;

	/* Make WebKit think we are displaying a local file, so that it
	 * does not block loading resources from file:// protocol */
	webkit_web_view_load_string (
		WEBKIT_WEB_VIEW (view), "", "text/html", "UTF-8", "file://");

	if (emd_global_http_cache == NULL) {
		user_cache_dir = e_get_user_cache_dir ();
		emd_global_http_cache = camel_data_cache_new (user_cache_dir, NULL);

		if (emd_global_http_cache) {
			/* cache expiry - 2 hour access, 1 day max */
			camel_data_cache_set_expire_age (
				emd_global_http_cache, 24 * 60 * 60);
			camel_data_cache_set_expire_access (
				emd_global_http_cache, 2 * 60 * 60);
		}
	}
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
 * into images or Unicode characters.
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
 * e_html_editor_view_get_unicode_smileys:
 * @view: an #EHTMLEditorView
 *
 * Returns whether to use Unicode characters for smileys.
 *
 * Returns: @TRUE when Unicode characters should be used, @FALSE otherwise.
 *
 * Since: 3.16
 */
gboolean
e_html_editor_view_get_unicode_smileys (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->unicode_smileys;
}

/**
 * e_html_editor_view_set_unicode_smileys:
 * @view: an #EHTMLEditorView
 * @unicode_smileys: @TRUE to use Unicode characters, @FALSE to use images
 *
 * Enables or disables the usage of Unicode characters for smileys.
 *
 * Since: 3.16
 */
void
e_html_editor_view_set_unicode_smileys (EHTMLEditorView *view,
                                        gboolean unicode_smileys)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->unicode_smileys == unicode_smileys)
		return;

	view->priv->unicode_smileys = unicode_smileys;

	g_object_notify (G_OBJECT (view), "unicode-smileys");
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
	guchar *base64_decoded_data = NULL;

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
	g_free (cid);
	name = webkit_dom_element_get_attribute (element, "data-name");
	camel_mime_part_set_disposition (part, "inline");
	camel_mime_part_set_filename (part, name);
	g_free (name);
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_BASE64);
out:
	g_free (mime_type);
	g_free (element_src);
	g_free (base64_decoded_data);

	return part;
}

static GList *
html_editor_view_get_parts_for_inline_images (EHTMLEditorView *view,
                                              const gchar *uid_domain,
                                              GHashTable **inline_images)
{
	GList *parts = NULL;
	gint length, ii;
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list;

	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), NULL);
	g_return_val_if_fail (inline_images != NULL, NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW  (view));
	list = webkit_dom_document_query_selector_all (document, "img[data-inline]", NULL);

	length = webkit_dom_node_list_get_length (list);
	if (length == 0)
		goto background;

	*inline_images = g_hash_table_new_full (
		g_str_hash, g_str_equal, g_free, g_free);

	for (ii = 0; ii < length; ii++) {
		const gchar *id;
		gchar *cid;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "src");

		if ((id = g_hash_table_lookup (*inline_images, src)) != NULL) {
			cid = g_strdup_printf ("cid:%s", id);
			g_free (src);
		} else {
			CamelMimePart *part;

			part = e_html_editor_view_add_inline_image_from_element (
				view, WEBKIT_DOM_ELEMENT (node), "src", uid_domain);
			parts = g_list_append (parts, part);

			id = camel_mime_part_get_content_id (part);
			cid = g_strdup_printf ("cid:%s", id);

			g_hash_table_insert (*inline_images, src, g_strdup (id));
		}
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (node), "src", cid, NULL);
		g_free (cid);
		g_object_unref (node);
	}
	g_object_unref (list);

 background:
	list = webkit_dom_document_query_selector_all (
		document, "[data-inline][background]", NULL);

	length = webkit_dom_node_list_get_length (list);
	if (length == 0) {
		g_object_unref (list);
		return parts;
	}
	if (!*inline_images)
		*inline_images = g_hash_table_new_full (
			g_str_hash, g_str_equal, g_free, g_free);

	for (ii = 0; ii < length; ii++) {
		CamelMimePart *part;
		const gchar *id;
		gchar *cid = NULL;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "background");

		if ((id = g_hash_table_lookup (*inline_images, src)) != NULL) {
			cid = g_strdup_printf ("cid:%s", id);
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (node), "background", cid, NULL);
			g_free (src);
		} else {
			part = e_html_editor_view_add_inline_image_from_element (
				view, WEBKIT_DOM_ELEMENT (node), "background", uid_domain);
			if (part) {
				parts = g_list_append (parts, part);
				id = camel_mime_part_get_content_id (part);
				g_hash_table_insert (*inline_images, src, g_strdup (id));
				cid = g_strdup_printf ("cid:%s", id);
				webkit_dom_element_set_attribute (
					WEBKIT_DOM_ELEMENT (node), "background", cid, NULL);
			} else
				g_free (src);
		}
		g_object_unref (node);
		g_free (cid);
	}

	g_object_unref (list);

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

static void
restore_images (gchar *key,
                gchar *value,
                EHTMLEditorView *view)
{
	gchar *selector;
	gint length, ii;
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW  (view));

	selector = g_strconcat ("[data-inline][background=\"cid:", value, "\"]", NULL);
	list = webkit_dom_document_query_selector_all (document, selector, NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMElement *element = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_list_item (list, ii));

		webkit_dom_element_set_attribute (element, "background", key, NULL);
		g_object_unref (element);
	}
	g_free (selector);
	g_object_unref (list);

	selector = g_strconcat ("[data-inline][src=\"cid:", value, "\"]", NULL);
	list = webkit_dom_document_query_selector_all (document, selector, NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMElement *element = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_list_item (list, ii));

		webkit_dom_element_set_attribute (element, "src", key, NULL);
		g_object_unref (element);
	}
	g_free (selector);
	g_object_unref (list);
}

static void
html_editor_view_restore_images (EHTMLEditorView *view,
                                 GHashTable **inline_images)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	g_hash_table_foreach (*inline_images, (GHFunc) restore_images, view);

	/* Remove all hashed images as user can modify them. */
	g_hash_table_remove_all (*inline_images);
	g_hash_table_destroy (*inline_images);
}

/**
 * e_html_editor_view_get_text_html:
 * @view: an #EHTMLEditorView:
 *
 * Returns processed HTML content of the editor document (without elements attributes
 * used in Evolution composer)
 *
 * Returns: A newly allocated string
 */
gchar *
e_html_editor_view_get_text_html (EHTMLEditorView *view,
                                  const gchar *from_domain,
                                  GList **inline_images)
{
	gchar *html = NULL;
	GHashTable *inline_images_to_restore = NULL;

	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), NULL);

	if (inline_images && from_domain)
		*inline_images = html_editor_view_get_parts_for_inline_images (
			view, from_domain, &inline_images_to_restore);

	html = process_content_for_html (view);

	if (inline_images && from_domain && inline_images_to_restore)
		html_editor_view_restore_images (view, &inline_images_to_restore);

	return html;
}

/**
 * e_html_editor_view_get_text_html_for_drafts:
 * @view: an #EHTMLEditorView:
 *
 * Returns HTML content of the editor document (with elements attributes used in
 * Evolution composer)
 *
 * Returns: A newly allocated string
 */
gchar *
e_html_editor_view_get_text_html_for_drafts (EHTMLEditorView *view)
{
	return process_content_for_saving_as_draft (view, FALSE);
}

/**
 * e_html_editor_view_get_text_html_for_drafts:
 * @view: an #EHTMLEditorView:
 *
 * Returns HTML content of the editor document (with elements attributes used in
 * Evolution composer)
 *
 * Returns: A newly allocated string
 */
gchar *
e_html_editor_view_get_body_text_html_for_drafts (EHTMLEditorView *view)
{
	return process_content_for_saving_as_draft (view, TRUE);
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
	return process_content_for_plain_text (view);
}

void
e_html_editor_view_convert_and_insert_plain_text (EHTMLEditorView *view,
                                                  const gchar *text)
{
	html_editor_view_insert_converted_html_into_selection (view, FALSE, text);
}

void
e_html_editor_view_convert_and_insert_html_to_plain_text (EHTMLEditorView *view,
                                                          const gchar *html)
{
	html_editor_view_insert_converted_html_into_selection (view, TRUE, html);
}

void
e_html_editor_view_convert_element_from_html_to_plain_text (EHTMLEditorView *view,
                                                            WebKitDOMElement *element)
{
	convert_element_from_html_to_plain_text (view, element, NULL, NULL);
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
		webkit_web_view_load_string (
			WEBKIT_WEB_VIEW (view), text, NULL, NULL, "file://");
		return;
	}

	if (view->priv->is_message_from_selection && !view->priv->html_mode) {
		if (text && *text)
			view->priv->convert_in_situ = TRUE;
		webkit_web_view_load_string (
			WEBKIT_WEB_VIEW (view), text, NULL, NULL, "file://");
		return;
	}

	/* Only convert messages that are in HTML */
	if (!view->priv->html_mode) {
		if (strstr (text, "<!-- text/html -->")) {
			if (!show_lose_formatting_dialog (view)) {
				e_html_editor_view_set_html_mode (view, TRUE);
				webkit_web_view_load_string (
					WEBKIT_WEB_VIEW (view), text, NULL, NULL, "file://");
				return;
			}
		}
		if (text && *text)
			view->priv->convert_in_situ = TRUE;
		webkit_web_view_load_string (
			WEBKIT_WEB_VIEW (view), text, NULL, NULL, "file://");
	} else
		webkit_web_view_load_string (
			WEBKIT_WEB_VIEW (view), text, NULL, NULL, "file://");
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

/**
 * e_html_editor_view_update_fonts:
 * @view: an #EHTMLEditorView
 *
 * Forces the editor to reload font settings from WebKitWebSettings and apply
 * it on the content of the editor document.
 */
void
e_html_editor_view_update_fonts (EHTMLEditorView *view)
{
	gboolean mark_citations, use_custom_font;
	gchar *base64, *font, *aa = NULL, *citation_color;
	const gchar *styles[] = { "normal", "oblique", "italic" };
	const gchar *smoothing = NULL;
	GString *stylesheet;
	PangoFontDescription *min_size, *ms, *vw;
	WebKitWebSettings *settings;

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
		"  font-style: %s;\n"
		"  -webkit-line-break: after-white-space;\n",
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

	/* When inserting a table into contenteditable element the width of the
	 * cells is nearly zero and the td { min-height } doesn't work so put
	 * unicode zero width space before each cell. */
	g_string_append (
		stylesheet,
		"td:before {\n"
		"  content: \"\xe2\x80\x8b\";"
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
		"td:hover "
		"{\n"
		"  outline: 1px dotted red;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		".-x-evo-plaintext-table "
		"{\n"
		"  border-collapse: collapse;\n"
		"  width: %dch;\n"
		"}\n",
		e_html_editor_selection_get_word_wrap_length (view->priv->selection));

	g_string_append (
		stylesheet,
		".-x-evo-plaintext-table td"
		"{\n"
		"  vertical-align: top;\n"
		"}\n");

	g_string_append (
		stylesheet,
		"td > * "
		"{\n"
		"  display : inline-block;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"ul[data-evo-plain-text]"
		"{\n"
		"  list-style: outside none;\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_PER_LIST_LEVEL);

	g_string_append_printf (
		stylesheet,
		"ul[data-evo-plain-text] > li"
		"{\n"
		"  list-style-position: outside;\n"
		"  text-indent: -%dch;\n"
		"}\n", SPACES_PER_LIST_LEVEL - 1);

	g_string_append (
		stylesheet,
		"ul[data-evo-plain-text] > li::before "
		"{\n"
		"  content: \"*"UNICODE_NBSP"\";\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"ul[data-evo-plain-text].-x-evo-indented "
		"{\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_PER_LIST_LEVEL);

	g_string_append (
		stylesheet,
		"ul:not([data-evo-plain-text]) > li.-x-evo-align-center,ol > li.-x-evo-align-center"
		"{\n"
		"  list-style-position: inside;\n"
		"}\n");

	g_string_append (
		stylesheet,
		"ul:not([data-evo-plain-text]) > li.-x-evo-align-right, ol > li.-x-evo-align-right"
		"{\n"
		"  list-style-position: inside;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"ol"
		"{\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_ORDERED_LIST_FIRST_LEVEL);

	g_string_append_printf (
		stylesheet,
		"ol.-x-evo-indented"
		"{\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_PER_LIST_LEVEL);

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

	g_string_append (
		stylesheet,
		"a "
		"{\n"
		"  word-wrap: break-word; \n"
		"  word-break: break-all; \n"
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

	g_free (citation_color);
	citation_color = NULL;

	g_string_append (stylesheet, "}\n");

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character "
		"{\n"
		"  color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (1));

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character+"
		".-x-evo-quote-character"
		"{\n"
		"  color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (2));

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character"
		"{\n"
		"  color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (3));

	g_string_append_printf (
		stylesheet,
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character+"
		".-x-evo-quote-character"
		"{\n"
		"  color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (4));

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
		e_web_view_get_citation_color_for_level (5));

	g_string_append (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  padding: 0ch 1ch 0ch 1ch;\n"
		"  margin: 0ch;\n"
		"  border-width: 0px 2px 0px 2px;\n"
		"  border-style: none solid none solid;\n"
		"  border-radius: 2px;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (1));

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (2));

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (3));

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"blockquote[type=cite]:not(.-x-evo-plaintext-quoted) "
		"{\n"
		"  border-color: %s;\n"
		"}\n",
		e_web_view_get_citation_color_for_level (4));

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
		e_web_view_get_citation_color_for_level (5));

	base64 = g_base64_encode ((guchar *) stylesheet->str, stylesheet->len);
	g_string_free (stylesheet, TRUE);

	stylesheet = g_string_new ("data:text/css;charset=utf-8;base64,");
	g_string_append (stylesheet, base64);
	g_free (base64);

	if (pango_font_description_get_size (ms) < pango_font_description_get_size (vw) || !view->priv->html_mode)
		min_size = ms;
	else
		min_size = vw;

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view));
	g_object_set (
		G_OBJECT (settings),
		"default-font-size", pango_font_description_get_size (vw) / PANGO_SCALE,
		"default-font-family", pango_font_description_get_family (vw),
		"monospace-font-family", pango_font_description_get_family (ms),
		"default-monospace-font-size", pango_font_description_get_size (ms) / PANGO_SCALE,
		"minimum-font-size", pango_font_description_get_size (min_size) / PANGO_SCALE,
		"user-stylesheet-uri", stylesheet->str,
		NULL);

	g_string_free (stylesheet, TRUE);

	pango_font_description_free (ms);
	pango_font_description_free (vw);
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

	range = html_editor_view_get_dom_range (view);
	html_editor_view_check_magic_links (view, range, include_space);
	g_object_unref (range);
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
e_html_editor_view_set_is_editting_message (EHTMLEditorView *view,
                                            gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	view->priv->is_editting_message = value;
}

void
e_html_editor_view_fix_file_uri_images (EHTMLEditorView *view)
{
	gint ii, length;
	WebKitDOMNodeList *list;
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	list = webkit_dom_document_query_selector_all (
		document, "img[src^=\"file://\"]", NULL);
	length = webkit_dom_node_list_get_length (list);

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;
		gchar *uri;

		node = webkit_dom_node_list_item (list, ii);
		uri = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "src");
		e_html_editor_selection_replace_image_src (
			view->priv->selection, WEBKIT_DOM_ELEMENT (node), uri);
		g_free (uri);
	}

	g_object_unref (list);
}

gboolean
e_html_editor_view_is_undo_redo_in_progress (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	return view->priv->undo_redo_in_progress;
}

void
e_html_editor_view_set_undo_redo_in_progress (EHTMLEditorView *view,
                                              gboolean value)
{
	view->priv->undo_redo_in_progress = value;
}

static void
remove_forward_redo_history_events_if_needed (EHTMLEditorView *view)
{
	GList *history = view->priv->history;
	GList *item;

	if (!history || !history->prev)
		return;

	item = history->prev;
	while (item) {
		GList *prev_item = item->prev;

		remove_history_event (view, item);
		item = prev_item;
	}
}

void
e_html_editor_view_insert_new_history_event (EHTMLEditorView *view,
					     EHTMLEditorViewHistoryEvent *event)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->undo_redo_in_progress)
		return;

	d (printf ("\nINSERTING EVENT:\n"));
	d (print_history_event (event));

	remove_forward_redo_history_events_if_needed (view);

	if (view->priv->history_size >= HISTORY_SIZE_LIMIT) {
		remove_history_event (view, g_list_last (view->priv->history)->prev);
		while (((EHTMLEditorViewHistoryEvent *) (g_list_last (view->priv->history)->prev))->type == HISTORY_AND) {
			remove_history_event (view, g_list_last (view->priv->history)->prev);
			remove_history_event (view, g_list_last (view->priv->history)->prev);
		}

	}

	view->priv->history = g_list_prepend (view->priv->history, event);
	view->priv->history_size++;
	view->priv->can_undo = TRUE;

	d (print_history (view));

	g_object_notify (G_OBJECT (view), "can-undo");
}

static WebKitDOMRange *
get_range_for_point (WebKitDOMDocument *document,
		     EHTMLEditorViewSelectionPoint point)
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
		WebKitDOMDOMWindow *dom_window;

		dom_window = webkit_dom_document_get_default_view (document);
		webkit_dom_dom_window_scroll_to (dom_window, point.x, point.y);

		scroll_left = webkit_dom_element_get_scroll_left (WEBKIT_DOM_ELEMENT (body));
		scroll_top = webkit_dom_element_get_scroll_top (WEBKIT_DOM_ELEMENT (body));
		range = webkit_dom_document_caret_range_from_point (
			document, point.x - scroll_left, point.y - scroll_top);
		g_object_unref (dom_window);
	}

	return range;
}

static void
restore_selection_to_history_event_state (EHTMLEditorView *view,
					  EHTMLEditorViewSelection selection_state)
{
	EHTMLEditorSelection *selection;
	gboolean was_collapsed = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMElement *element, *tmp;
	WebKitDOMRange *range;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	/* Restore the selection how it was before the event occured. */
	selection = e_html_editor_view_get_selection (view);
	range = get_range_for_point (document, selection_state.start);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_object_unref (range);

	was_collapsed = selection_state.start.x == selection_state.end.x;
	was_collapsed = was_collapsed &&  selection_state.start.y == selection_state.end.y;
	if (was_collapsed) {
		g_object_unref (dom_selection);
		return;
	}

	e_html_editor_selection_save (selection);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	remove_node (WEBKIT_DOM_NODE (element));

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	webkit_dom_element_remove_attribute (element, "id");

	range = get_range_for_point (document, selection_state.end);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_object_unref (range);

	e_html_editor_selection_save (selection);

	tmp = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	remove_node (WEBKIT_DOM_NODE (tmp));

	webkit_dom_element_set_id (
		element, "-x-evo-selection-start-marker");

	e_html_editor_selection_restore (selection);

	g_object_unref (dom_selection);
}

static gboolean
event_selection_was_collapsed (EHTMLEditorViewHistoryEvent *ev)
{
	return (ev->before.start.x == ev->before.end.x) && (ev->before.start.y == ev->before.end.y);
}

static void
undo_delete (EHTMLEditorView *view,
	     EHTMLEditorViewHistoryEvent *event)
{
	EHTMLEditorSelection *selection;
	gboolean empty, single_block;
	gchar *content;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *element;
	WebKitDOMNode *first_child, *fragment;

	selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	fragment = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (event->data.fragment), TRUE);
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
		block = e_html_editor_get_parent_block_node_from_child (node);

		if (webkit_dom_document_fragment_query_selector (event->data.fragment, ".-x-evo-quoted", NULL)) {
			while ((node = webkit_dom_node_get_first_child (fragment))) {
				if (WEBKIT_DOM_IS_ELEMENT (node) &&
				    webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (node), ".-x-evo-quoted", NULL))

					if (get_citation_level (block, FALSE) > 0) {
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (block),
							node,
							block,
							NULL);
					} else {
						WebKitDOMNode *next_block;

						next_block = webkit_dom_node_get_next_sibling (block);
						while (next_block && is_citation_node (next_block))
							next_block = webkit_dom_node_get_first_child (next_block);

						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (next_block),
							node,
							next_block,
							NULL);
					}
				else {
					if (get_citation_level (block, FALSE) > 0) {
						WebKitDOMNode *next_node;

						next_node = split_node_into_two (block, -1);
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (next_node),
							node,
							next_node,
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

		remove_node (block);

		restore_selection_to_history_event_state (view, event->before);

		e_html_editor_view_force_spell_check_in_viewport (view);

		return;
	}

	/* Redoing Return key press */
	if (event->type == HISTORY_INPUT && (empty ||
	    g_object_get_data (G_OBJECT (event->data.fragment), "history-return-key"))) {
		if (key_press_event_process_return_key (view)) {
			body_key_up_event_process_return_key (view);
		} else {
			WebKitDOMElement *element;
			WebKitDOMNode *next_sibling;

			range = get_range_for_point (document, event->before.start);
			webkit_dom_dom_selection_remove_all_ranges (dom_selection);
			webkit_dom_dom_selection_add_range (dom_selection, range);
			g_object_unref (dom_selection);
			g_object_unref (range);

			e_html_editor_selection_save (selection);

			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));
			if (next_sibling && !(WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling))) {
				split_node_into_two (WEBKIT_DOM_NODE (element), 1);
			} else {
				WebKitDOMNode *block;

				block = e_html_editor_get_parent_block_node_from_child (
					WEBKIT_DOM_NODE (element));
				remove_selection_markers (document);
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (block),
					fragment,
					webkit_dom_node_get_next_sibling (block),
					NULL);
			}
			e_html_editor_selection_restore (selection);
		}

		view->priv->return_key_pressed = TRUE;
		range = html_editor_view_get_dom_range (view);
		html_editor_view_check_magic_links (view, range, FALSE);
		view->priv->return_key_pressed = FALSE;
		g_object_unref (range);
		e_html_editor_view_force_spell_check_in_viewport (view);

		return;
	}

	if (!single_block) {
		if (WEBKIT_DOM_IS_ELEMENT (first_child) &&
		    !(WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (first_child) ||
		      WEBKIT_DOM_IS_HTML_PRE_ELEMENT (first_child) ||
		      (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (first_child) &&
		       element_has_class (WEBKIT_DOM_ELEMENT (first_child), "-x-evo-paragraph"))))
			single_block = TRUE;
	}

	/* Multi block delete */
	if (WEBKIT_DOM_IS_ELEMENT (first_child) && !single_block) {
		gboolean delete;
		WebKitDOMElement *signature;
		WebKitDOMNode *node, *current_block, *last_child;
		WebKitDOMNode *next_block, *insert_before;
		WebKitDOMNode *parent_deleted_content, *parent_current_block;

		range = get_range_for_point (document, event->after.start);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
		g_object_unref (range);
		e_html_editor_selection_save (selection);

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

		current_block = e_html_editor_get_parent_block_node_from_child (WEBKIT_DOM_NODE (element));

		while (node) {
			WebKitDOMNode *next_sibling, *parent_node;

			next_sibling = webkit_dom_node_get_next_sibling (node);
			parent_node = webkit_dom_node_get_parent_node (node);
			if (!next_sibling && WEBKIT_DOM_IS_HTMLBR_ELEMENT (node)) {
				/* Check if the whole element was deleted. If so replace it and
				 * skip the code down there. */
				if (!webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element))) {
					WebKitDOMNode *tmp_node;
					WebKitDOMElement *tmp_element;

					tmp_node = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
					webkit_dom_node_replace_child (
						webkit_dom_node_get_parent_node (tmp_node),
						fragment,
						tmp_node,
						NULL);

					tmp_element = webkit_dom_document_get_element_by_id (
						document, "-x-evo-tmp-block");
					if (tmp_element) {
						webkit_dom_element_remove_attribute (tmp_element, "id");
						wrap_and_quote_element (view, tmp_element);
					}

					/* Remove empty blockquotes, if presented. */
					tmp_element = webkit_dom_document_query_selector (
						document, "blockquote[type=cite]:empty", NULL);
					if (tmp_element)
						remove_node (WEBKIT_DOM_NODE (tmp_element));

					merge_siblings_if_necessary (document, event->data.fragment);

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

					remove_selection_markers (document);

					restore_selection_to_history_event_state (view, event->before);

					e_html_editor_view_force_spell_check_in_viewport (view);

					g_object_unref (dom_selection);

					return;
				}
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

		parent_deleted_content = webkit_dom_node_get_parent_node (first_child);
		parent_current_block = webkit_dom_node_get_parent_node (current_block);
		insert_before = webkit_dom_node_get_next_sibling (current_block);

		/* Remove the first block from deleted content as its content was already
		 * moved to the right place. */
		remove_node (first_child);

		next_block = webkit_dom_node_get_next_sibling (current_block);
		/* Move the deleted content back to the body. Start from the next sibling
		 * of the first block (if presented) where the delete occurred. */
		while (parent_deleted_content) {
			WebKitDOMNode *tmp, *child;

			/* Move all the siblings from current level back to the body. */
			child = webkit_dom_node_get_first_child (parent_deleted_content);
			while (child) {
				WebKitDOMNode *next_sibling;

				next_sibling = webkit_dom_node_get_next_sibling (child);
				webkit_dom_node_insert_before (
					parent_current_block, child, insert_before, NULL);
				child = next_sibling;
			}
			tmp = webkit_dom_node_get_parent_node (parent_deleted_content);
			remove_node (parent_deleted_content);
			parent_deleted_content = tmp;
			insert_before = webkit_dom_node_get_next_sibling (parent_current_block);

			if (!insert_before && next_block && WEBKIT_DOM_IS_HTML_BODY_ELEMENT (
			    webkit_dom_node_get_parent_node (parent_current_block)))
				insert_before = split_node_into_two (next_block, -1);

			/* Be sure that we don't go above body. */
			if (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent_current_block))
				parent_current_block = webkit_dom_node_get_parent_node (parent_current_block);
		}

		wrap_and_quote_element (view, WEBKIT_DOM_ELEMENT (current_block));

		if (WEBKIT_DOM_IS_ELEMENT (last_child))
			wrap_and_quote_element (view, WEBKIT_DOM_ELEMENT (last_child));

		merge_siblings_if_necessary (document, event->data.fragment);

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

		e_html_editor_selection_restore (selection);
		e_html_editor_view_force_spell_check_in_viewport (view);
	} else {
		gboolean empty_text = FALSE, was_link = FALSE;
		WebKitDOMNode *prev_sibling, *next_sibling, *nd;
		WebKitDOMNode *parent;

		element = webkit_dom_document_create_element (document, "span", NULL);

		/* Create temporary node on the selection where the delete occurred. */
		if (webkit_dom_document_fragment_query_selector (event->data.fragment, ".Apple-tab-span", NULL))
			range = get_range_for_point (document, event->before.start);
		else
			range = get_range_for_point (document, event->after.start);

		webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (element), NULL);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);

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

			clone_prev = webkit_dom_node_clone_node (prev_sibling, FALSE);
			clone_next = webkit_dom_node_clone_node (next_sibling, FALSE);

			if (webkit_dom_node_is_equal_node (clone_prev, clone_next)) {
				WebKitDOMNode *child;

				while ((child = webkit_dom_node_get_first_child (next_sibling)))
					webkit_dom_node_append_child (prev_sibling, child, NULL);

				remove_node (next_sibling);
			}
		}

		remove_node (WEBKIT_DOM_NODE (element));

		if (event->type == HISTORY_DELETE && !view->priv->html_mode) {
			WebKitDOMNode *current_block;

			current_block = e_html_editor_get_parent_block_node_from_child (parent);
			if (get_citation_level (current_block, FALSE) > 0)
				wrap_and_quote_element (view, WEBKIT_DOM_ELEMENT (current_block));
		}

		/* If the selection markers are presented restore the selection,
		 * otherwise the selection was not collapsed so select the deleted
		 * content as it was before the delete occurred. */
		if (webkit_dom_document_fragment_query_selector (event->data.fragment, "span#-x-evo-selection-start-marker", NULL))
			e_html_editor_selection_restore (selection);
		else
			restore_selection_to_history_event_state (view, event->before);

		if (event->type != HISTORY_INPUT) {
			g_object_unref (range);
			range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
			html_editor_view_check_magic_smileys (view, range);
			if (!was_link)
				html_editor_view_check_magic_links (view, range, FALSE);
		}

		g_object_unref (range);
		e_html_editor_view_force_spell_check_for_current_paragraph (view);
	}

	g_object_unref (dom_selection);
}

static void
redo_delete (EHTMLEditorView *view,
	     EHTMLEditorViewHistoryEvent *event)
{
	gboolean delete_key, control_key;
	glong length = 1;
	gint ii;
	WebKitDOMDocumentFragment *fragment = event->data.fragment;
	WebKitDOMNode *node;

	restore_selection_to_history_event_state (view, event->before);

	delete_key = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (event->data.fragment), "history-delete-key"));
	control_key = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (event->data.fragment), "history-control-key"));

	if (!delete_key && key_press_event_process_backspace_key (view))
		goto out;

	if (key_press_event_process_delete_or_backspace_key (view, delete_key, NULL))
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
		WebKitDOMRange *range;

		range = html_editor_view_get_dom_range (view);
		node = webkit_dom_range_get_end_container (range, NULL);
		g_object_unref (range);
		current_block = e_html_editor_get_parent_block_node_from_child (node);
		if (get_citation_level (current_block, FALSE) > 0 &&
		    (next_block = webkit_dom_node_get_next_sibling (current_block))) {
			remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (next_block));
			remove_quoting_from_element (WEBKIT_DOM_ELEMENT (next_block));
		}
	}

	for (ii = 0; ii < length; ii++) {
		e_html_editor_view_exec_command (
			view,
			delete_key ? E_HTML_EDITOR_VIEW_COMMAND_FORWARD_DELETE :
				     E_HTML_EDITOR_VIEW_COMMAND_DELETE,
			NULL);
	}

	/* Really don't know why, but when the selection marker nodes were in
	 * anchors then we need to do an extra delete command otherwise we will
	 * end with two blocks split in half. */
	node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
	while ((node = webkit_dom_node_get_first_child (node))) {
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
			e_html_editor_view_exec_command (
				view,
				E_HTML_EDITOR_VIEW_COMMAND_FORWARD_DELETE,
				NULL);
			break;
		}
	}

	node = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (fragment));
	while ((node = webkit_dom_node_get_last_child (node))) {
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
			e_html_editor_view_exec_command (
				view,
				E_HTML_EDITOR_VIEW_COMMAND_FORWARD_DELETE,
				NULL);
			break;
		}
	}

 out:
	view->priv->dont_save_history_in_body_input = TRUE;
	view->priv->undo_redo_in_progress = FALSE;
	body_input_event_cb (NULL, NULL, view);
	view->priv->dont_save_history_in_body_input = FALSE;
	view->priv->undo_redo_in_progress = TRUE;
	view->priv->renew_history_after_coordinates = FALSE;
	body_key_up_event_process_backspace_or_delete (view, delete_key);
	view->priv->renew_history_after_coordinates = TRUE;

	restore_selection_to_history_event_state (view, event->after);

	e_html_editor_view_force_spell_check_for_current_paragraph (view);
}

typedef void (*SelectionStyleChangeFunc) (EHTMLEditorSelection *selection, gint style);

static void
undo_redo_style_change (EHTMLEditorView *view,
			EHTMLEditorViewHistoryEvent *event,
			gboolean undo)
{
	EHTMLEditorSelection *selection;
	SelectionStyleChangeFunc func;

	selection = e_html_editor_view_get_selection (view);

	switch (event->type) {
		case HISTORY_ALIGNMENT:
			func = (SelectionStyleChangeFunc) e_html_editor_selection_set_alignment;
			break;
		case HISTORY_BOLD:
			func = e_html_editor_selection_set_bold;
			break;
		case HISTORY_BLOCK_FORMAT:
			func = (SelectionStyleChangeFunc) e_html_editor_selection_set_block_format;
			break;
		case HISTORY_FONT_SIZE:
			func = (SelectionStyleChangeFunc) e_html_editor_selection_set_font_size;
			break;
		case HISTORY_ITALIC:
			func = e_html_editor_selection_set_italic;
			break;
		case HISTORY_MONOSPACE:
			func = e_html_editor_selection_set_monospaced;
			break;
		case HISTORY_STRIKETHROUGH:
			func = e_html_editor_selection_set_strikethrough;
			break;
		case HISTORY_UNDERLINE:
			func = e_html_editor_selection_set_underline;
			break;
		default:
			return;
	}

	restore_selection_to_history_event_state (view, undo ? event->after : event->before);

	func (selection, undo ? event->data.style.from : event->data.style.to);

	restore_selection_to_history_event_state (view, undo ? event->before : event->after);
}

static void
undo_redo_indent (EHTMLEditorView *view,
		  EHTMLEditorViewHistoryEvent *event,
		  gboolean undo)
{
	gboolean was_indent = FALSE;
	EHTMLEditorSelection *selection;

	selection = e_html_editor_view_get_selection (view);

	if (undo)
		restore_selection_to_history_event_state (view, event->after);

	was_indent = event->data.style.from && event->data.style.to;

	if (undo) {
		if (was_indent)
			e_html_editor_selection_unindent (selection);
		else
			e_html_editor_selection_indent (selection);
	} else {
		if (was_indent)
			e_html_editor_selection_indent (selection);
		else
			e_html_editor_selection_unindent (selection);
	}

	if (undo)
		restore_selection_to_history_event_state (view, event->before);
}

static void
undo_redo_font_color (EHTMLEditorView *view,
		      EHTMLEditorViewHistoryEvent *event,
		      gboolean undo)
{
	if (undo)
		restore_selection_to_history_event_state (view, event->after);

	e_html_editor_view_exec_command (
		view,
		E_HTML_EDITOR_VIEW_COMMAND_FORE_COLOR,
		undo ? event->data.string.from : event->data.string.to);

	if (undo)
		restore_selection_to_history_event_state (view, event->before);
}

static void
undo_redo_wrap (EHTMLEditorView *view,
		EHTMLEditorViewHistoryEvent *event,
		gboolean undo)
{
	EHTMLEditorSelection *selection;

	selection = e_html_editor_view_get_selection (view);

	if (undo)
		restore_selection_to_history_event_state (view, event->after);

	if (undo) {
		WebKitDOMNode *node;
		WebKitDOMElement *element;
		WebKitDOMRange *range;

		range = html_editor_view_get_dom_range (view);
		node = webkit_dom_range_get_common_ancestor_container (range, NULL);
		g_object_unref (range);
		element = get_parent_block_element (WEBKIT_DOM_NODE (node));
		webkit_dom_element_remove_attribute (element, "data-user-wrapped");
		remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (element));

		e_html_editor_view_force_spell_check_for_current_paragraph (view);
	} else
		e_html_editor_selection_wrap_lines (selection);

	if (undo)
		restore_selection_to_history_event_state (view, event->before);
}

static void
undo_redo_page_dialog (EHTMLEditorView *view,
		       EHTMLEditorViewHistoryEvent *event,
		       gboolean undo)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitDOMNamedNodeMap *attributes, *attributes_history;
	gint length, length_history, ii, jj;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

	if (undo)
		restore_selection_to_history_event_state (view, event->after);

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
					GdkRGBA rgba;

					value = webkit_dom_node_get_node_value (attr_clone);
					if (gdk_rgba_parse (&rgba, value))
						e_html_editor_view_set_link_color (view, &rgba);
					g_free (value);
				}
				if (g_strcmp0 (name, "vlink") == 0) {
					gchar *value;
					GdkRGBA rgba;

					value = webkit_dom_node_get_node_value (attr_clone);
					if (gdk_rgba_parse (&rgba, value))
						e_html_editor_view_set_visited_link_color (view, &rgba);
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
		restore_selection_to_history_event_state (view, event->before);
}

static void
undo_redo_hrule_dialog (EHTMLEditorView *view,
                        EHTMLEditorViewHistoryEvent *event,
                        gboolean undo)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	selection = e_html_editor_view_get_selection (view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	if (undo)
		restore_selection_to_history_event_state (view, event->after);

	e_html_editor_selection_save (selection);
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

		if (node && WEBKIT_DOM_IS_HTMLHR_ELEMENT (node)) {
			if (!event->data.dom.from)
				remove_node (node);
			else
				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (node),
					webkit_dom_node_clone_node (event->data.dom.from, TRUE),
					node,
					NULL);
		}
	} else {
		WebKitDOMNode *node;
		WebKitDOMElement *parent;

		parent = get_parent_block_element (WEBKIT_DOM_NODE (element));

		if (event->data.dom.from) {
			node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent));

			if (node && WEBKIT_DOM_IS_HTMLHR_ELEMENT (node))
				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (node),
					webkit_dom_node_clone_node (event->data.dom.to, TRUE),
					node,
					NULL);
		} else {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (parent)),
				webkit_dom_node_clone_node (event->data.dom.to, TRUE),
				webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent)),
				NULL);
		}
	}

	if (undo)
		restore_selection_to_history_event_state (view, event->before);
	else
		e_html_editor_selection_restore (selection);
}

static void
undo_redo_image_dialog (EHTMLEditorView *view,
                        EHTMLEditorViewHistoryEvent *event,
                        gboolean undo)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMNode *sibling, *image = NULL;

	selection = e_html_editor_view_get_selection (view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	if (undo)
		restore_selection_to_history_event_state (view, event->after);

	e_html_editor_selection_save (selection);
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
		webkit_dom_node_clone_node (undo ? event->data.dom.from : event->data.dom.to, TRUE),
		image,
		NULL);

	if (undo)
		restore_selection_to_history_event_state (view, event->before);
	else
		e_html_editor_selection_restore (selection);
}

static void
undo_redo_link_dialog (EHTMLEditorView *view,
                       EHTMLEditorViewHistoryEvent *event,
                       gboolean undo)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMElement *anchor, *element;

	selection = e_html_editor_view_get_selection (view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	if (undo)
		restore_selection_to_history_event_state (view, event->after);
	else
		restore_selection_to_history_event_state (view, event->before);

	e_html_editor_selection_save (selection);

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker");
	if (!element)
		return;

	anchor = e_html_editor_dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "A");
	if (undo) {
		if (anchor) {
			if (!event->data.dom.from)
				remove_node (WEBKIT_DOM_NODE (anchor));
			else
				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (anchor)),
					webkit_dom_node_clone_node (event->data.dom.from, TRUE),
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
					webkit_dom_node_clone_node (event->data.dom.to, TRUE),
					WEBKIT_DOM_NODE (anchor),
					NULL);
			} else {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
					webkit_dom_node_clone_node (event->data.dom.to, TRUE),
					WEBKIT_DOM_NODE (element),
					NULL);

				if (event->data.dom.from)
					e_html_editor_view_exec_command (
						view, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);
			}
		}
	}

	if (undo)
		restore_selection_to_history_event_state (view, event->before);
	else
		e_html_editor_selection_restore (selection);
}

static void
undo_redo_table_dialog (EHTMLEditorView *view,
                        EHTMLEditorViewHistoryEvent *event,
                        gboolean undo)
{
	EHTMLEditorSelection *selection;

	WebKitDOMDocument *document;
	WebKitDOMElement *table, *element;

	selection = e_html_editor_view_get_selection (view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	if (undo)
		restore_selection_to_history_event_state (view, event->after);

	e_html_editor_selection_save (selection);
	element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker");
	if (!element)
		return;

	table = e_html_editor_dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "TABLE");

	if (!table) {
		if ((!event->data.dom.to && undo) || (!event->data.dom.from && !undo)) {
			WebKitDOMElement *parent;

			parent = get_parent_block_element (WEBKIT_DOM_NODE (element));
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (parent)),
				webkit_dom_node_clone_node (undo ? event->data.dom.from : event->data.dom.to, TRUE),
				WEBKIT_DOM_NODE (parent),
				NULL);
			restore_selection_to_history_event_state (view, event->before);
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
				webkit_dom_node_clone_node (event->data.dom.from, TRUE),
				WEBKIT_DOM_NODE (table),
				NULL);
	} else {
		if (!event->data.dom.to)
			remove_node (WEBKIT_DOM_NODE (table));
		else
			webkit_dom_node_replace_child (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (table)),
				webkit_dom_node_clone_node (event->data.dom.to, TRUE),
				WEBKIT_DOM_NODE (table),
				NULL);
	}

	if (undo)
		restore_selection_to_history_event_state (view, event->before);
	else
		e_html_editor_selection_restore (selection);
}

static void
undo_redo_table_input (EHTMLEditorView *view,
                       EHTMLEditorViewHistoryEvent *event,
                       gboolean undo)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	selection = e_html_editor_view_get_selection (view);

	if (undo)
		restore_selection_to_history_event_state (view, event->after);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection)) {
		g_object_unref (dom_selection);
		return;
	}
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	g_object_unref (dom_selection);

	/* Find if writing into table. */
	node = webkit_dom_range_get_start_container (range, NULL);
	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = get_parent_block_element (node);

	g_object_unref (range);

	/* If writing to table we have to create different history event. */
	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (element))
		return;

	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
		webkit_dom_node_clone_node (undo ? event->data.dom.from : event->data.dom.to, TRUE),
		WEBKIT_DOM_NODE (element),
		NULL);

	e_html_editor_selection_restore (selection);
}

static void
undo_redo_paste (EHTMLEditorView *view,
                 EHTMLEditorViewHistoryEvent *event,
                 gboolean undo)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;

	selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	if (undo) {
		if (event->type == HISTORY_PASTE_QUOTED) {
			WebKitDOMElement *tmp;
			WebKitDOMNode *parent;

			restore_selection_to_history_event_state (view, event->after);

			e_html_editor_selection_save (selection);
			tmp = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			if (!tmp)
				return;

			parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (tmp));
			while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (parent)))
				parent = webkit_dom_node_get_parent_node (parent);

			webkit_dom_node_replace_child (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (prepare_paragraph (selection, document, TRUE)),
				parent,
				NULL);

			e_html_editor_selection_restore (selection);
		} else {
			WebKitDOMDOMWindow *dom_window;
			WebKitDOMDOMSelection *dom_selection;
			WebKitDOMElement *element, *tmp;
			WebKitDOMRange *range;

			dom_window = webkit_dom_document_get_default_view (document);
			dom_selection = webkit_dom_dom_window_get_selection (dom_window);
			g_object_unref (dom_window);

			/* Restore the selection how it was before the event occured. */
			range = get_range_for_point (document, event->before.start);
			webkit_dom_dom_selection_remove_all_ranges (dom_selection);
			webkit_dom_dom_selection_add_range (dom_selection, range);
			g_object_unref (range);

			e_html_editor_selection_save (selection);

			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			remove_node (WEBKIT_DOM_NODE (element));

			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");

			webkit_dom_element_remove_attribute (element, "id");

			range = get_range_for_point (document, event->after.start);
			webkit_dom_dom_selection_remove_all_ranges (dom_selection);
			webkit_dom_dom_selection_add_range (dom_selection, range);
			g_object_unref (range);
			g_object_unref (dom_selection);

			e_html_editor_selection_save (selection);

			tmp = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");

			remove_node (WEBKIT_DOM_NODE (tmp));

			webkit_dom_element_set_id (
				element, "-x-evo-selection-start-marker");

			e_html_editor_selection_restore (selection);

			e_html_editor_view_exec_command (
				view, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);

			e_html_editor_view_force_spell_check_for_current_paragraph (view);
		}
	} else {
		restore_selection_to_history_event_state (view, event->before);

		if (event->type == HISTORY_PASTE)
			e_html_editor_selection_insert_text (selection, event->data.string.to);
		else if (event->type == HISTORY_PASTE_QUOTED)
			e_html_editor_view_insert_quoted_text (view, event->data.string.to);
		else if (event->type == HISTORY_INSERT_HTML)
			e_html_editor_selection_insert_html (selection, event->data.string.to);
		else
			e_html_editor_selection_insert_as_text (selection, event->data.string.to);
	}
}

static void
undo_redo_image (EHTMLEditorView *view,
                 EHTMLEditorViewHistoryEvent *event,
                 gboolean undo)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (undo) {
		WebKitDOMElement *element;
		WebKitDOMNode *node;

		range = get_range_for_point (document, event->before.start);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
		g_object_unref (range);

		e_html_editor_selection_save (selection);
		element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		node = webkit_dom_node_get_next_sibling  (WEBKIT_DOM_NODE (element));

		if (WEBKIT_DOM_IS_ELEMENT (node))
			if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-resizable-wrapper") ||
			    element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-smiley-wrapper"))
				remove_node (node);
		e_html_editor_selection_restore (selection);
	} else {
		WebKitDOMElement *element;

		range = get_range_for_point (document, event->before.start);
		/* Create temporary node on the selection where the delete occured. */
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
		g_object_unref (range);

		e_html_editor_selection_save (selection);
		element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		/* Insert the deleted content back to the body. */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (event->data.fragment), TRUE),
			WEBKIT_DOM_NODE (element),
			NULL);

		e_html_editor_selection_restore (selection);
		e_html_editor_view_force_spell_check_for_current_paragraph (view);
	}

	g_object_unref (dom_selection);
}

static void
undo_redo_replace (EHTMLEditorView *view,
                   EHTMLEditorViewHistoryEvent *event,
                   gboolean undo)
{
	restore_selection_to_history_event_state (view, undo ? event->after : event->before);

	if (undo) {
		WebKitDOMDocument *document;
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		g_object_unref (dom_window);

		webkit_dom_dom_selection_modify (dom_selection, "extend", "left", "word");
		g_object_unref (dom_selection);
	}

	e_html_editor_view_exec_command (
		view,
		E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT,
		undo ? event->data.string.from : event->data.string.to);

	e_html_editor_view_force_spell_check_for_current_paragraph (view);

	restore_selection_to_history_event_state (view, undo ? event->before : event->after);
}

static void
undo_redo_replace_all (EHTMLEditorView *view,
                       EHTMLEditorViewHistoryEvent *event,
                       gboolean undo)
{
	if (undo) {
		if (event->type == HISTORY_REPLACE) {
			undo_redo_replace (view, event, undo);
			return;
		} else {
			EHTMLEditorViewHistoryEvent *next_event;
			GList *next_item;
			WebKitDOMDocument *document;
			WebKitDOMDOMWindow *dom_window;
			WebKitDOMDOMSelection *dom_selection;

			next_item = view->priv->history->next;

			while (next_item) {
				next_event = next_item->data;

				if (next_event->type != HISTORY_REPLACE)
					break;

				if (g_strcmp0 (next_event->data.string.from, event->data.string.from) != 0)
					break;

				if (g_strcmp0 (next_event->data.string.to, event->data.string.to) != 0)
					break;

				undo_redo_replace (view, next_event, undo);

				next_item = next_item->next;
			}

			view->priv->history = next_item->prev;

			document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
			dom_window = webkit_dom_document_get_default_view (document);
			dom_selection = webkit_dom_dom_window_get_selection (dom_window);
			webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);
			g_object_unref (dom_window);
			g_object_unref (dom_selection);
		}
	} else {
		/* Find if this history item is part of HISTORY_REPLACE_ALL. */
		EHTMLEditorViewHistoryEvent *prev_event;
		GList *prev_item;
		gboolean replace_all = FALSE;

		prev_item = view->priv->history->prev;
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
			undo_redo_replace (view, event, undo);
			return;
		}

		prev_item = view->priv->history->prev;
		while (prev_item) {
			prev_event = prev_item->data;

			if (prev_event->type == HISTORY_REPLACE) {
				undo_redo_replace (view, prev_event, undo);
				prev_item = prev_item->prev;
			} else
				break;
		}

		view->priv->history = prev_item->next;
	}
}

static void
undo_redo_remove_link (EHTMLEditorView *view,
                       EHTMLEditorViewHistoryEvent *event,
                       gboolean undo)
{
	EHTMLEditorSelection *selection;

	selection = e_html_editor_view_get_selection (view);

	if (undo)
		restore_selection_to_history_event_state (view, event->after);

	if (undo) {
		WebKitDOMDocument *document;
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;
		WebKitDOMElement *element;
		WebKitDOMRange *range;


		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		/* Select the anchor. */
		webkit_dom_dom_selection_modify (dom_selection, "move", "left", "word");
		webkit_dom_dom_selection_modify (dom_selection, "extend", "right", "word");

		range = html_editor_view_get_dom_range (view);
		element = webkit_dom_document_create_element (document, "SPAN", NULL);
		webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (element), NULL);
		g_object_unref (range);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (event->data.fragment), TRUE),
			WEBKIT_DOM_NODE (element),
			NULL);
		remove_node (WEBKIT_DOM_NODE (element));
		g_object_unref (dom_window);
		g_object_unref (dom_selection);
	} else
		e_html_editor_selection_unlink (selection);

	if (undo)
		restore_selection_to_history_event_state (view, event->before);
}

static void
undo_return_in_empty_list_item (EHTMLEditorView *view,
                                EHTMLEditorViewHistoryEvent *event)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker;
	WebKitDOMNode *parent;

	e_html_editor_selection_save (view->priv->selection);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	selection_start_marker = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker");
	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));

	if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (parent)) {
		WebKitDOMNode *parent_list;

		remove_selection_markers (document);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (event->data.fragment), TRUE),
			webkit_dom_node_get_next_sibling (parent),
			NULL);

		parent_list = parent;
		while (node_is_list_or_item (webkit_dom_node_get_parent_node (parent_list)))
			parent_list = webkit_dom_node_get_parent_node (parent_list);

		merge_lists_if_possible (parent_list);
	}

	e_html_editor_selection_restore (view->priv->selection);
}

static void
undo_input (EHTMLEditorView *view,
            EHTMLEditorViewHistoryEvent *event)
{
	gboolean remove_anchor;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMNode *node, *tmp_node;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	restore_selection_to_history_event_state (view, event->after);

	webkit_dom_dom_selection_modify (dom_selection, "extend", "left", "character");
	if (e_html_editor_selection_is_citation (view->priv->selection)) {
		/* Post processing of quoted text in body_input_event_cb needs to be called. */
		view->priv->undo_redo_in_progress = FALSE;
		view->priv->dont_save_history_in_body_input = TRUE;
	}

	/* If we are undoing the text that was appended to the link we have to
	 * remove the link and make just the plain text from it. */
	node = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	node = webkit_dom_node_get_parent_node (node);
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
	}

	e_html_editor_view_exec_command (
		view, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);

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
	if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (tmp_node) &&
	    WEBKIT_DOM_IS_HTMLBR_ELEMENT (webkit_dom_node_get_last_child (tmp_node)))
		undo_return_in_empty_list_item (view, event);

	g_object_unref (dom_window);
	g_object_unref (dom_selection);
}

static void
undo_redo_citation_split (EHTMLEditorView *view,
                          EHTMLEditorViewHistoryEvent *event,
                          gboolean undo)
{
	gboolean in_situ = FALSE;
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	if (event->before.start.x == event->after.start.x &&
	    event->before.start.y == event->after.start.y &&
	    event->after.end.x == event->after.end.x &&
	    event->after.end.y == event->after.end.y)
		in_situ = TRUE;

	if (undo) {
		EHTMLEditorSelection *selection;
		WebKitDOMElement *selection_start, *parent;
		WebKitDOMNode *citation_before, *citation_after, *child, *last_child, *tmp;

		restore_selection_to_history_event_state (view, event->after);

		selection = e_html_editor_view_get_selection (view);
		e_html_editor_selection_save (selection);
		selection_start = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (!selection_start)
			return;

		parent = get_parent_block_element (WEBKIT_DOM_NODE (selection_start));
		citation_before = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (parent));
		if (!is_citation_node (citation_before)) {
			e_html_editor_selection_restore (selection);
			return;
		}

		citation_after = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent));
		if (!is_citation_node (citation_after)) {
			e_html_editor_selection_restore (selection);
			return;
		}

		/* Get first block in next citation. */
		child = webkit_dom_node_get_first_child (citation_after);
		while (child && is_citation_node (child))
			child = webkit_dom_node_get_first_child (child);

		/* Get last block in previous citation. */
		last_child = webkit_dom_node_get_last_child (citation_before);
		while (last_child && is_citation_node (last_child))
			last_child = webkit_dom_node_get_last_child (last_child);

		/* Before appending any content to the block, check that the
		 * last node is not BR, if it is, remove it. */
		tmp = webkit_dom_node_get_last_child (last_child);
		if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (tmp))
			remove_node (tmp);

		if (in_situ && event->data.fragment) {
			webkit_dom_node_append_child (
				webkit_dom_node_get_parent_node (last_child),
				webkit_dom_node_clone_node (
					WEBKIT_DOM_NODE (event->data.fragment), TRUE),
				NULL);
		} else {
			remove_quoting_from_element (WEBKIT_DOM_ELEMENT (child));
			remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (child));

			remove_quoting_from_element (WEBKIT_DOM_ELEMENT (last_child));
			remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (last_child));

			/* Copy the content of the first block to the last block to get
			 * to the state how the block looked like before it was split. */
			while ((tmp = webkit_dom_node_get_first_child (child)))
				webkit_dom_node_append_child (last_child, tmp, NULL);

			wrap_and_quote_element (view, WEBKIT_DOM_ELEMENT (last_child));

			remove_node (child);
		}

		/* Move all the block from next citation to the previous one. */
		while ((child = webkit_dom_node_get_first_child (citation_after)))
			webkit_dom_node_append_child (citation_before, child, NULL);

		remove_selection_markers (document);

		remove_node (WEBKIT_DOM_NODE (parent));
		remove_node (WEBKIT_DOM_NODE (citation_after));

		/* If enter was pressed when some text was selected, restore it. */
		if (event->data.fragment != NULL && !in_situ)
			undo_delete (view, event);

		merge_siblings_if_necessary (document, NULL);

		restore_selection_to_history_event_state (view, event->before);

		e_html_editor_view_force_spell_check_in_viewport (view);
	} else {
		restore_selection_to_history_event_state (view, event->before);

		if (in_situ) {
			WebKitDOMElement *selection_start_marker;
			WebKitDOMNode *block;

			e_html_editor_selection_save (view->priv->selection);

			selection_start_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");

			block = e_html_editor_get_parent_block_node_from_child (
				WEBKIT_DOM_NODE (selection_start_marker));
			remove_selection_markers (document);

			/* Remove current block (and all of its parents if they
			 * are empty) as it will be replaced by a new block that
			 * will be in the body and not in the blockquote. */
			remove_node_and_parents_if_empty (block);
		}

		insert_new_line_into_citation (view, "");
	}
}

static void
undo_redo_blockquote (EHTMLEditorView *view,
                      EHTMLEditorViewHistoryEvent *event,
                      gboolean undo)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	selection = e_html_editor_view_get_selection (view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	if (undo)
		restore_selection_to_history_event_state (view, event->after);

	e_html_editor_selection_save (selection);
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
		e_html_editor_selection_set_block_format (
			selection, E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE);
	}

	if (undo)
		restore_selection_to_history_event_state (view, event->before);
	else
		e_html_editor_selection_restore (selection);
}

static void
undo_redo_unquote (EHTMLEditorView *view,
                   EHTMLEditorViewHistoryEvent *event,
                   gboolean undo)
{
	EHTMLEditorSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	selection = e_html_editor_view_get_selection (view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	if (undo)
		restore_selection_to_history_event_state (view, event->after);

	e_html_editor_selection_save (selection);
	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	if (undo) {
		WebKitDOMNode *next_sibling, *prev_sibling;
		WebKitDOMElement *block;

		block = get_parent_block_element (WEBKIT_DOM_NODE (element));

		next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (block));
		prev_sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (block));

		if (prev_sibling && is_citation_node (prev_sibling)) {
			webkit_dom_node_append_child (
				prev_sibling,
				webkit_dom_node_clone_node (event->data.dom.from, TRUE),
				NULL);

			if (next_sibling && is_citation_node (next_sibling)) {
				WebKitDOMNode *child;

				while  ((child = webkit_dom_node_get_first_child (next_sibling)))
					webkit_dom_node_append_child (
						prev_sibling, child, NULL);

				remove_node (next_sibling);
			}
		} else if (next_sibling && is_citation_node (next_sibling)) {
			webkit_dom_node_insert_before (
				next_sibling,
				webkit_dom_node_clone_node (event->data.dom.from, TRUE),
				webkit_dom_node_get_first_child (next_sibling),
				NULL);
		}

		remove_node (WEBKIT_DOM_NODE (block));
	} else
		change_quoted_block_to_normal (view);

	if (undo)
		restore_selection_to_history_event_state (view, event->before);
	else
		e_html_editor_selection_restore (selection);

	e_html_editor_view_force_spell_check_for_current_paragraph (view);
}

void
e_html_editor_view_redo (EHTMLEditorView *view)
{
	EHTMLEditorViewHistoryEvent *event;
	GList *history;

	if (!e_html_editor_view_can_redo (view))
		return;

	history = view->priv->history;
	event = history->prev->data;
	d (printf ("\nREDOING EVENT:\n"));
	d (print_history_event (event));

	view->priv->undo_redo_in_progress = TRUE;

	switch (event->type) {
		case HISTORY_BOLD:
		case HISTORY_MONOSPACE:
		case HISTORY_STRIKETHROUGH:
		case HISTORY_UNDERLINE:
		case HISTORY_ALIGNMENT:
		case HISTORY_BLOCK_FORMAT:
		case HISTORY_FONT_SIZE:
		case HISTORY_ITALIC:
			undo_redo_style_change (view, event, FALSE);
			break;
		case HISTORY_DELETE:
			redo_delete (view, event);
			break;
		case HISTORY_INDENT:
			undo_redo_indent (view, event, FALSE);
			break;
		case HISTORY_INPUT:
			undo_delete (view, event);
			{
				gchar *text_content;
				WebKitDOMRange *range;
				WebKitDOMNode *first_child;

				range = html_editor_view_get_dom_range (view);
				html_editor_view_check_magic_smileys (view, range);

				first_child = webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (event->data.fragment));
				text_content = webkit_dom_node_get_text_content (first_child);
				/* Call magic links when the space was pressed. */
				if (g_str_has_prefix (text_content, UNICODE_NBSP)) {
					view->priv->space_key_pressed = TRUE;
					html_editor_view_check_magic_links (view, range, FALSE);
					view->priv->space_key_pressed = FALSE;
				}
				g_free (text_content);
				g_object_unref (range);
			}
			break;
		case HISTORY_REMOVE_LINK:
			undo_redo_remove_link (view, event, FALSE);
			break;
		case HISTORY_FONT_COLOR:
			undo_redo_font_color (view, event, FALSE);
			break;
		case HISTORY_CITATION_SPLIT:
			undo_redo_citation_split (view, event, FALSE);
			break;
		case HISTORY_PASTE:
		case HISTORY_PASTE_AS_TEXT:
		case HISTORY_PASTE_QUOTED:
		case HISTORY_INSERT_HTML:
			undo_redo_paste (view, event, FALSE);
			break;
		case HISTORY_IMAGE:
		case HISTORY_SMILEY:
			undo_redo_image (view, event, FALSE);
			break;
		case HISTORY_WRAP:
			undo_redo_wrap (view, event, FALSE);
			break;
		case HISTORY_IMAGE_DIALOG:
			undo_redo_image_dialog (view, event, FALSE);
			break;
		case HISTORY_LINK_DIALOG:
			undo_redo_link_dialog (view, event, FALSE);
			break;
		case HISTORY_TABLE_DIALOG:
			undo_redo_table_dialog (view, event, FALSE);
			break;
		case HISTORY_TABLE_INPUT:
			undo_redo_table_input (view, event, FALSE);
			break;
		case HISTORY_PAGE_DIALOG:
			undo_redo_page_dialog (view, event, FALSE);
			break;
		case HISTORY_HRULE_DIALOG:
			undo_redo_hrule_dialog (view, event, FALSE);
			break;
		case HISTORY_REPLACE:
		case HISTORY_REPLACE_ALL:
			undo_redo_replace_all (view, event, FALSE);
			break;
		case HISTORY_BLOCKQUOTE:
			undo_redo_blockquote (view, event, FALSE);
			break;
		case HISTORY_UNQUOTE:
			undo_redo_unquote (view, event, FALSE);
			break;
		case HISTORY_AND:
			g_warning ("Unhandled HISTORY_AND event!");
			break;
		default:
			return;
	}

	if (history->prev->prev) {
		event = history->prev->prev->data;
		if (event->type == HISTORY_AND) {
			view->priv->history = history->prev->prev;
			e_html_editor_view_redo (view);
			return;
		}
	}

	view->priv->history = view->priv->history->prev;

	d (print_redo_events (view));

	html_editor_view_user_changed_contents_cb (view);

	view->priv->undo_redo_in_progress = FALSE;
}

void
e_html_editor_view_undo (EHTMLEditorView *view)
{
	EHTMLEditorViewHistoryEvent *event;
	GList *history;

	if (!e_html_editor_view_can_undo (view))
		return;

	history = view->priv->history;
	event = history->data;
	d (printf ("\nUNDOING EVENT:\n"));
	d (print_history_event (event));

	view->priv->undo_redo_in_progress = TRUE;

	switch (event->type) {
		case HISTORY_BOLD:
		case HISTORY_ITALIC:
		case HISTORY_STRIKETHROUGH:
		case HISTORY_UNDERLINE:
		case HISTORY_FONT_SIZE:
			if (event_selection_was_collapsed (event)) {
				if (history->next) {
					view->priv->history = history->next;
					e_html_editor_view_undo (view);
				}
				view->priv->undo_redo_in_progress = FALSE;
				return;
			}
		case HISTORY_ALIGNMENT:
		case HISTORY_BLOCK_FORMAT:
		case HISTORY_MONOSPACE:
			undo_redo_style_change (view, event, TRUE);
			break;
		case HISTORY_DELETE:
			undo_delete (view, event);
			break;
		case HISTORY_INDENT:
			undo_redo_indent (view, event, TRUE);
			break;
		case HISTORY_INPUT:
			undo_input (view, event);
			break;
		case HISTORY_REMOVE_LINK:
			undo_redo_remove_link (view, event, TRUE);
			break;
		case HISTORY_FONT_COLOR:
			undo_redo_font_color (view, event, TRUE);
			break;
		case HISTORY_CITATION_SPLIT:
			undo_redo_citation_split (view, event, TRUE);
			break;
		case HISTORY_PASTE:
		case HISTORY_PASTE_AS_TEXT:
		case HISTORY_PASTE_QUOTED:
		case HISTORY_INSERT_HTML:
			undo_redo_paste (view, event, TRUE);
			break;
		case HISTORY_IMAGE:
		case HISTORY_SMILEY:
			undo_redo_image (view, event, TRUE);
			break;
		case HISTORY_WRAP:
			undo_redo_wrap (view, event, TRUE);
			break;
		case HISTORY_IMAGE_DIALOG:
			undo_redo_image_dialog (view, event, TRUE);
			break;
		case HISTORY_LINK_DIALOG:
			undo_redo_link_dialog (view, event, TRUE);
			break;
		case HISTORY_TABLE_DIALOG:
			undo_redo_table_dialog (view, event, TRUE);
			break;
		case HISTORY_TABLE_INPUT:
			undo_redo_table_input (view, event, TRUE);
			break;
		case HISTORY_PAGE_DIALOG:
			undo_redo_page_dialog (view, event, TRUE);
			break;
		case HISTORY_HRULE_DIALOG:
			undo_redo_hrule_dialog (view, event, TRUE);
			break;
		case HISTORY_REPLACE:
		case HISTORY_REPLACE_ALL:
			undo_redo_replace_all (view, event, TRUE);
			break;
		case HISTORY_BLOCKQUOTE:
			undo_redo_blockquote (view, event, TRUE);
			break;
		case HISTORY_UNQUOTE:
			undo_redo_unquote (view, event, TRUE);
			break;
		case HISTORY_AND:
			g_warning ("Unhandled HISTORY_AND event!");
			break;
		default:
			return;
	}

	event = history->next->data;
	if (event->type == HISTORY_AND) {
		view->priv->history = history->next->next;
		e_html_editor_view_undo (view);
		return;
	}

	if (history->next)
		view->priv->history = view->priv->history->next;

	d (print_undo_events (view));

	html_editor_view_user_changed_contents_cb (view);

	view->priv->undo_redo_in_progress = FALSE;
}

/* Following functions are used by EHTMLEditorPageDialog to get the right colors
 * when view is not focused */
void
e_html_editor_view_block_style_updated_callbacks (EHTMLEditorView *view)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (!view->priv->style_change_callbacks_blocked) {
		g_signal_handlers_block_by_func (view, style_updated_cb, NULL);
		view->priv->style_change_callbacks_blocked = TRUE;
	}
}

void
e_html_editor_view_unblock_style_updated_callbacks (EHTMLEditorView *view)
{
	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	if (view->priv->style_change_callbacks_blocked) {
		g_signal_handlers_unblock_by_func (view, style_updated_cb, NULL);
		view->priv->style_change_callbacks_blocked = FALSE;
	}
}

gboolean
e_html_editor_view_is_pasting_content_from_itself (EHTMLEditorView *view)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_VIEW (view), FALSE);

	if (view->priv->pasting_primary_clipboard)
		return view->priv->copy_paste_primary_in_view;
	else
		return view->priv->copy_paste_clipboard_in_view;
}

void
e_html_editor_view_remove_input_event_listener_from_body (EHTMLEditorView *view)
{
	if (!view->priv->body_input_event_removed) {
		WebKitDOMDocument *document;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

		webkit_dom_event_target_remove_event_listener (
			WEBKIT_DOM_EVENT_TARGET (
				webkit_dom_document_get_body (document)),
			"input",
			G_CALLBACK (body_input_event_cb),
			FALSE);

		view->priv->body_input_event_removed = TRUE;
	}
}

void
e_html_editor_view_register_input_event_listener_on_body (EHTMLEditorView *view)
{
	if (view->priv->body_input_event_removed) {
		WebKitDOMDocument *document;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (
				webkit_dom_document_get_body (document)),
			"input",
			G_CALLBACK (body_input_event_cb),
			FALSE,
			view);

		view->priv->body_input_event_removed = FALSE;
	}
}
