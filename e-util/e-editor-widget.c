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

#define UNICODE_HIDDEN_SPACE "\xe2\x80\x8b"

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

	GSettings *font_settings;
	GSettings *aliasing_settings;

	GQueue *postreload_operations;
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

static gboolean
editor_widget_should_show_delete_interface_for_element (EEditorWidget *widget,
                                                        WebKitDOMHTMLElement *element)
{
	return FALSE;
}

static void
editor_widget_load_status_changed (EEditorWidget *widget)
{
	WebKitLoadStatus status;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;

	status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (widget));
	if (status != WEBKIT_LOAD_FINISHED) {
		return;
	}

	widget->priv->reload_in_progress = FALSE;

 	/* After reload, there is no selection in the document. Fix it. */
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);

	range = webkit_dom_document_create_range (document);
	webkit_dom_range_set_start (
		range, WEBKIT_DOM_NODE (webkit_dom_document_get_body (document)),
		0, NULL);
	webkit_dom_range_set_end (
		range, WEBKIT_DOM_NODE (webkit_dom_document_get_body (document)),
		0, NULL);

	webkit_dom_dom_selection_add_range (selection, range);

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
				 gboolean include_space)
{
	gchar *node_text;
	gchar **urls;
	GRegex *regex = NULL;
	GMatchInfo *match_info;
	gint start_pos_url, end_pos_url;
	WebKitDOMNode *node;

	node = webkit_dom_range_get_end_container (range, NULL);

	if (!WEBKIT_DOM_IS_TEXT (node))
		return;

	node_text = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (node));
	if (!node_text || !g_utf8_validate (node_text, -1, NULL))
		return;

	regex = g_regex_new (include_space ? URL_PATTERN_SPACE : URL_PATTERN, 0, 0, NULL);

	if (!regex) {
		g_free (node_text);
		return;
	}

	g_regex_match_all (regex, node_text, G_REGEX_MATCH_NOTEMPTY, &match_info);
	urls = g_match_info_fetch_all (match_info);

	if (urls) {
		gchar *html, *url, *final_url, *url_end_raw;
		glong url_start, url_end, url_length;
		WebKitDOMDocument *document;
		WebKitDOMDOMWindow *window;
		WebKitDOMDOMSelection *selection;

		g_match_info_fetch_pos (match_info, 0, &start_pos_url, &end_pos_url);

		/* Get start and end position of url in node's text because positions
		 * that we get from g_match_info_fetch_pos are not UTF-8 aware */
		url_end_raw = g_strndup(node_text, end_pos_url);
		url_end = g_utf8_strlen (url_end_raw, -1);

		url_length = g_utf8_strlen (urls[0], -1);
		url_start = url_end - url_length;

		/* Remove space on end */
		url = g_utf8_substring (urls[0], 0, include_space ? url_length - 1 : url_length);

		/* Select the link and put it inside <A> */
		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
		window = webkit_dom_document_get_default_view (document);
		selection = webkit_dom_dom_window_get_selection (window);

		webkit_dom_dom_selection_set_base_and_extent (
			selection, webkit_dom_range_get_end_container (range, NULL),
			include_space ? url_end - 1 : url_end, webkit_dom_range_get_end_container (range, NULL),
			url_start, NULL);

		if (g_str_has_prefix (url, "www"))
			final_url = g_strconcat ("http://", url, NULL);
		else
			final_url = g_strdup (url);

		html = g_strdup_printf ("<a href=\"%s\">%s</a>", final_url, url);

		e_editor_selection_insert_html (widget->priv->selection, html);

		webkit_dom_dom_selection_modify (selection, "move", "right", "character");

		g_free (url_end_raw);
		g_free (html);
		g_free (url);
		g_free (final_url);
	}
	else {
		gchar *href, *text, *url;
		gint diff;
		WebKitDOMElement *parent;

		parent = webkit_dom_node_get_parent_element (node);
		/* if parent is href => we're editing the link */
		if (!WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent)) {
			g_match_info_free (match_info);
			g_regex_unref (regex);
			g_free (node_text);
			return;
		}

		/* edit only if href and description are the same */
		href = webkit_dom_html_anchor_element_get_href (
				WEBKIT_DOM_HTML_ANCHOR_ELEMENT (parent));
		text = webkit_dom_html_element_get_inner_text (
				WEBKIT_DOM_HTML_ELEMENT (parent));

		if (strstr (href, "://") && !strstr (text, "://")) {
			url = strstr (href, "://") + 3;
			diff = strlen (text) - strlen (url);

			if (text [strlen (text) - 1] != '/')
				diff++;

			if (g_strcmp0 (url, text) != 0 && ABS (diff) == 1) {
				gchar *inner_html, *protocol, *new_href;

				protocol = g_strndup (href, strstr (href, "://") - href + 3);
				inner_html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (parent));
				new_href = g_strconcat (protocol, inner_html, NULL);

				webkit_dom_html_anchor_element_set_href (WEBKIT_DOM_HTML_ANCHOR_ELEMENT (parent), new_href);

				g_free (new_href);
				g_free (protocol);
				g_free (inner_html);
			}
		}
		else {
			diff = strlen (text) - strlen (href);
			if (text [strlen (text) - 1] != '/')
				diff++;

			if (g_strcmp0 (href, text) != 0 && ABS (diff) == 1) {
				gchar *inner_html;
				gchar *new_href;

				inner_html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (parent));
				new_href = g_strconcat (inner_html, NULL);

				webkit_dom_html_anchor_element_set_href (WEBKIT_DOM_HTML_ANCHOR_ELEMENT (parent), new_href);

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
	if (!webkit_dom_node_get_node_type (node) == 3) {
		return;
	}

	if (!WEBKIT_DOM_IS_TEXT (node))
		return;

	node_text = webkit_dom_text_get_whole_text ((WebKitDOMText *) node);
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
		GtkIconInfo *icon_info;
		const gchar *filename;
		gchar *filename_uri, *html;
		WebKitDOMDocument *document;
		WebKitDOMDOMWindow *window;
		WebKitDOMDOMSelection *selection;
		const EEmoticon *emoticon;

		if (pos > 0) {
			uc = g_utf8_get_char (g_utf8_offset_to_pointer (node_text, pos - 1));
			if (uc != ' ' && uc != '\t') {
				g_free (node_text);
				return;
			}
		}

		/* Select the text-smiley and replace it by <img> */
		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
		window = webkit_dom_document_get_default_view (document);
		selection = webkit_dom_dom_window_get_selection (window);
		webkit_dom_dom_selection_set_base_and_extent (
			selection, webkit_dom_range_get_end_container (range, NULL),
			pos, webkit_dom_range_get_end_container (range, NULL),
			start + 1, NULL);

		emoticon = e_emoticon_chooser_lookup_emoticon (
				emoticons_icon_names[-state - 1]);

		/* Convert a named icon to a file URI. */
		icon_info = gtk_icon_theme_lookup_icon (
			gtk_icon_theme_get_default (),
			emoticons_icon_names[-state - 1], 16, 0);
		g_return_if_fail (icon_info != NULL);
		filename = gtk_icon_info_get_filename (icon_info);
		g_return_if_fail (filename != NULL);
		filename_uri = g_filename_to_uri (filename, NULL, NULL);

		html = g_strdup_printf (
			"<img src=\"%s\" alt=\"%s\" x-evo-smiley=\"%s\" />",
			filename_uri, emoticon ? emoticon->text_face : "",
			emoticons_icon_names[-state - 1]);

		e_editor_selection_insert_html (
			widget->priv->selection, html);

		g_free (html);
		g_free (filename_uri);
		gtk_icon_info_free (icon_info);
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
		webkit_dom_html_element_set_id (
			WEBKIT_DOM_HTML_ELEMENT (style), "--evolution-editor-style-a");
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
	gchar *html, *escaped_text;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	/* This is a trick to escape any HTML characters (like <, > or &).
	 * <textarea> automatically replaces all these unsafe characters
	 * by &lt;, &gt; etc. */
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	element = webkit_dom_document_create_element (document, "TEXTAREA", NULL);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element), text, NULL);
	escaped_text = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element));
	g_object_unref (element);

	html = g_strconcat (
		"<blockquote type=\"cite\"><pre>",
		escaped_text, "</pre></blockquote>", NULL);
	e_editor_selection_insert_html (widget->priv->selection, html);

	g_free (escaped_text);
	g_free (html);
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

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_editor_widget_parent_class)->dispose (object);
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
		}

		return TRUE;
	}

	/* Chain up to parent's button_release_event() method. */
	return GTK_WIDGET_CLASS (e_editor_widget_parent_class)->
		button_release_event (widget, event);
}

static gboolean
editor_widget_key_press_event (GtkWidget *widget,
                               GdkEventKey *event)
{
	EEditorWidget *editor = E_EDITOR_WIDGET (widget);

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

static gboolean
editor_widget_key_release_event (GtkWidget *widget,
                                 GdkEventKey *event)
{
	WebKitDOMRange *range;
	EEditorWidget *editor_widget;

	editor_widget = E_EDITOR_WIDGET (widget);
	range = editor_widget_get_dom_range (editor_widget);

	if (editor_widget->priv->magic_smileys &&
	    editor_widget->priv->html_mode) {
		editor_widget_check_magic_smileys (editor_widget, range);
	}

	if (event->keyval == GDK_KEY_space) {
		editor_widget_check_magic_links (editor_widget, range, TRUE);
	}

	if ((event->keyval == GDK_KEY_Return) ||
	    (event->keyval == GDK_KEY_Linefeed) ||
	    (event->keyval == GDK_KEY_KP_Enter)) {

		editor_widget_check_magic_links (editor_widget, range, FALSE);
	}

	if ((event->keyval == GDK_KEY_Control_L) ||
	    (event->keyval == GDK_KEY_Control_R)) {

		editor_widget_set_links_active (editor_widget, FALSE);
	}

	if ((event->keyval == GDK_KEY_space) ||
	    (event->keyval == GDK_KEY_Delete) ||
	    (event->keyval == GDK_KEY_Return) ||
	    (event->keyval == GDK_KEY_Linefeed) ||
	    (event->keyval == GDK_KEY_KP_Enter) ||
	    (event->keyval == GDK_KEY_BackSpace)) {

		EEditorSelection *selection;
		selection = e_editor_widget_get_selection (editor_widget);
		if (e_editor_selection_get_block_format (selection) == E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH)
			e_editor_selection_wrap_lines (selection, TRUE, event);
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

	e_editor_widget_update_fonts (editor);

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
	gboolean changing = FALSE;

	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	/* If toggling from HTML to plain text mode, ask user first */
	if (widget->priv->html_mode && !html_mode && widget->priv->changed) {
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

		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_CANCEL) {
			gtk_widget_destroy (dialog);
			/* Nothing has changed, but notify anyway */
			g_object_notify (G_OBJECT (widget), "html-mode");
			return;
		} else {
			changing = TRUE;
		}

		gtk_widget_destroy (dialog);
	}

	if (html_mode)
		changing = TRUE;

	if (html_mode == widget->priv->html_mode)
		return;

	widget->priv->html_mode = html_mode;

	/* Update fonts - in plain text we only want monospace */
	e_editor_widget_update_fonts (widget);

	if (widget->priv->html_mode) {
		gchar *plain_text, *html;

		plain_text = e_editor_widget_get_text_plain (widget);

		/* FIXME WEBKIT: This does not process smileys! */
		html = camel_text_to_html (
			plain_text,
			CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
			CAMEL_MIME_FILTER_TOHTML_MARK_CITATION |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES |
			CAMEL_MIME_FILTER_TOHTML_FORMAT_FLOWED,
			0);

		if (changing) {
			gchar *tmp;
			tmp = g_strconcat (html, "<span id=\"-x-evo-changing-mode\"></span>", NULL);
			g_free (html);
			html = tmp;
		}

		e_editor_widget_set_text_html (widget, html);

		g_free (plain_text);
		g_free (html);
	} else {
		gchar *plain;

		plain = e_editor_widget_get_text_plain (widget);

		if (*plain) {
			if (changing) {
				gchar *tmp;
				tmp = g_strconcat (plain, UNICODE_HIDDEN_SPACE, NULL);
				g_free (plain);
				plain = tmp;
			}

			e_editor_widget_set_text_plain (widget, plain);
		}

		g_free (plain);
	}

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
 * Returns HTML content of the editor document.
 *
 * Returns: A newly allocated string
 */
gchar *
e_editor_widget_get_text_html (EEditorWidget *widget)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	element = webkit_dom_document_get_document_element (document);
	return webkit_dom_html_element_get_outer_html (
			WEBKIT_DOM_HTML_ELEMENT (element));
}

static void
process_elements (WebKitDOMNode *node,
                  GString *buffer)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMNodeList *nodes;
	WebKitDOMCSSStyleDeclaration *style;
	gchar *display, *tagname;
	gulong ii, length;
	GRegex *regex, *regex_hidden_space;

	document = webkit_dom_node_get_owner_document (node);
	window = webkit_dom_document_get_default_view (document);

	/* Is this a block element? */
	style = webkit_dom_dom_window_get_computed_style (
		window, WEBKIT_DOM_ELEMENT (node), "");
	display = webkit_dom_css_style_declaration_get_property_value (
		style, "display");

	tagname = webkit_dom_element_get_tag_name (WEBKIT_DOM_ELEMENT (node));

	/* Replace images with smileys by their text representation */
	if (g_ascii_strncasecmp (tagname, "IMG", 3) == 0) {
		if (webkit_dom_element_has_attribute (
				WEBKIT_DOM_ELEMENT (node), "x-evo-smiley")) {

			gchar *smiley_name;
			const EEmoticon *emoticon;

			smiley_name = webkit_dom_element_get_attribute (
				WEBKIT_DOM_ELEMENT (node), "x-evo-smiley");
			emoticon = e_emoticon_chooser_lookup_emoticon (smiley_name);
			if (emoticon != NULL)
				g_string_append_printf (
					buffer, " %s ", emoticon->text_face);

			g_free (smiley_name);
			g_free (display);

			/* IMG can't have child elements, so we return now */
			return;
		}
	}

	/* Skip signature */
	if (g_strcmp0 (webkit_dom_element_get_class_name (WEBKIT_DOM_ELEMENT (node)), "-x-evolution-signature") == 0)
		return;

	nodes = webkit_dom_node_get_child_nodes (node);
	length = webkit_dom_node_list_get_length (nodes);
	regex = g_regex_new ("\x9", 0, 0, NULL);
	regex_hidden_space = g_regex_new (UNICODE_HIDDEN_SPACE, 0, 0, NULL);

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *child;

		child = webkit_dom_node_list_item (nodes, ii);
		if (webkit_dom_node_get_node_type (child) == 3) {
			gchar *content, *tmp;

			tmp = webkit_dom_node_get_text_content (child);

			/* Replace tabs with 4 whitespaces, otherwise they got
			 * replaced by single whitespace */
			content = g_regex_replace (
					regex, tmp, -1, 0, "    ",
					0, NULL);

			content = g_regex_replace (
					regex_hidden_space, content, -1, 0, "", 0, NULL);

			g_string_append (buffer, content);
			g_free (tmp);
			g_free (content);
		} else {
			if (g_strcmp0 (webkit_dom_node_get_local_name (child), "br") == 0) {
				g_string_append (buffer, "\n");
			}
		}

		if (webkit_dom_node_has_child_nodes (child)) {
			process_elements (child, buffer);
		}
	}

	g_free (display);
	g_regex_unref (regex);
	g_regex_unref (regex_hidden_space);
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
	WebKitDOMNode *body;
	GString *plain_text;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));

	plain_text = g_string_sized_new (1024);
	process_elements (body, plain_text);

	/* Return text content between <body> and </body> */
	return g_string_free (plain_text, FALSE);
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

	webkit_web_view_load_html_string (
		WEBKIT_WEB_VIEW (widget), text, "file://");
}

static void
do_set_text_plain (EEditorWidget *widget,
                   gpointer data)
{
	gboolean changing = FALSE;
	const gchar *ptr;
	gchar *data_copy = NULL;

	if (strstr (data, UNICODE_HIDDEN_SPACE)) {
		data_copy = g_new (gchar, strlen (data));
		g_utf8_strncpy (data_copy, data, g_utf8_strlen (data, -1) - 1);
		ptr = data_copy;
		changing = TRUE;
	} else {
		ptr = data;
	}

	e_editor_widget_exec_command (
		widget, E_EDITOR_WIDGET_COMMAND_INSERT_TEXT, ptr);

	if (changing) {
		WebKitDOMDocument *document;
		WebKitDOMNode *body;
		WebKitDOMElement *element;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
		body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));

		element = webkit_dom_document_create_element (document, "SPAN", NULL);
		webkit_dom_html_element_set_id (WEBKIT_DOM_HTML_ELEMENT (element), "-x-evo-changing-mode");

		webkit_dom_node_append_child (
			body,
			WEBKIT_DOM_NODE (element),
			NULL);

		g_free (data_copy);
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

	webkit_web_view_load_html_string (
		WEBKIT_WEB_VIEW (widget), "", "file://");

	/* webkit_web_view_load_html_string() is actually performed
	 * when this functions returns, so the operation below would get
	 * overwritten. Instead queue the insert operation and insert the
	 * actual text when the webview is reloaded */
	editor_widget_queue_postreload_operation (
		widget, do_set_text_plain, g_strdup (text), g_free);
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
	g_string_append_printf (
		stylesheet,
		"p,pre,code,address {\n"
		"  margin: 0;\n"
		"}\n"
		"h1,h2,h3,h4,h5,h6 {\n"
		"  margin-top: 0.2em;\n"
		"  margin-bottom: 0.2em;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite] {\n"
		"  padding: 0.4ex 1ex;\n"
		"  margin: 1ex;\n"
		"  border-width: 0px 2px 0px 2px;\n"
		"  border-style: none solid none solid;\n"
		"  border-radius: 2px;\n"
		"}\n");

	/* Block quote border colors are borrowed from Thunderbird. */

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite] "
		"{\n"
		"  border-color: rgb(114,159,207);\n"  /* Sky Blue 1 */
		"}\n");

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"{\n"
		"  border-color: rgb(173,127,168);\n"  /* Plum 1 */
		"}\n");

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"{\n"
		"  border-color: rgb(138,226,52);\n"  /* Chameleon 1 */
		"}\n");

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"{\n"
		"  border-color: rgb(252,175,62);\n"  /* Orange 1 */
		"}\n");

	g_string_append_printf (
		stylesheet,
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
		"blockquote[type=cite] "
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
	editor_widget_check_magic_links (widget, range, include_space);
}

