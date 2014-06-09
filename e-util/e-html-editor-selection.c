/*
 * e-html-editor-selection.c
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

#include "e-html-editor-selection.h"
#include "e-html-editor-view.h"
#include "e-html-editor.h"
#include "e-html-editor-utils.h"

#include <e-util/e-util.h>

#include <webkit/webkit.h>
#include <webkit/webkitdom.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define E_HTML_EDITOR_SELECTION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_SELECTION, EHTMLEditorSelectionPrivate))

#define UNICODE_ZERO_WIDTH_SPACE "\xe2\x80\x8b"
#define UNICODE_NBSP "\xc2\xa0"

#define SPACES_PER_INDENTATION 4
#define SPACES_PER_LIST_LEVEL 8

/**
 * EHTMLEditorSelection
 *
 * The #EHTMLEditorSelection object represents current position of the cursor
 * with the editor or current text selection within the editor. To obtain
 * valid #EHTMLEditorSelection, call e_html_editor_view_get_selection().
 */

struct _EHTMLEditorSelectionPrivate {

	GWeakRef html_editor_view;
	gulong selection_changed_handler_id;

	gchar *text;

	gboolean is_bold;
	gboolean is_italic;
	gboolean is_underline;
	gboolean is_monospaced;
	gboolean is_strikethrough;

	gchar *background_color;
	gchar *font_color;
	gchar *font_family;

	gulong selection_offset;

	gint word_wrap_length;
	guint font_size;

	EHTMLEditorSelectionAlignment alignment;
};

enum {
	PROP_0,
	PROP_ALIGNMENT,
	PROP_BACKGROUND_COLOR,
	PROP_BLOCK_FORMAT,
	PROP_BOLD,
	PROP_HTML_EDITOR_VIEW,
	PROP_FONT_COLOR,
	PROP_FONT_NAME,
	PROP_FONT_SIZE,
	PROP_INDENTED,
	PROP_ITALIC,
	PROP_MONOSPACED,
	PROP_STRIKETHROUGH,
	PROP_SUBSCRIPT,
	PROP_SUPERSCRIPT,
	PROP_TEXT,
	PROP_UNDERLINE
};

static const GdkRGBA black = { 0 };

G_DEFINE_TYPE (
	EHTMLEditorSelection,
	e_html_editor_selection,
	G_TYPE_OBJECT
);

static WebKitDOMRange *
html_editor_selection_get_current_range (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range = NULL;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, NULL);

	web_view = WEBKIT_WEB_VIEW (view);

	document = webkit_web_view_get_dom_document (web_view);
	window = webkit_dom_document_get_default_view (document);
	if (!window)
		goto exit;

	dom_selection = webkit_dom_dom_window_get_selection (window);
	if (!WEBKIT_DOM_IS_DOM_SELECTION (dom_selection))
		goto exit;

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1)
		goto exit;

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

 exit:
	g_object_unref (view);

	return range;
}

static gboolean
get_has_style (EHTMLEditorSelection *selection,
               const gchar *style_tag)
{
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;
	gboolean result;
	gint tag_len;

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_start_container (range, NULL);
	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	tag_len = strlen (style_tag);
	result = FALSE;
	while (!result && element) {
		gchar *element_tag;
		gboolean accept_citation = FALSE;

		element_tag = webkit_dom_element_get_tag_name (element);

		if (g_ascii_strncasecmp (style_tag, "citation", 8) == 0) {
			accept_citation = TRUE;
			result = ((strlen (element_tag) == 10 /* strlen ("blockquote") */) &&
				(g_ascii_strncasecmp (element_tag, "blockquote", 10) == 0));
			if (element_has_class (element, "-x-evo-indented"))
				result = FALSE;
		} else {
			result = ((tag_len == strlen (element_tag)) &&
				(g_ascii_strncasecmp (element_tag, style_tag, tag_len) == 0));
		}

		/* Special case: <blockquote type=cite> marks quotation, while
		 * just <blockquote> is used for indentation. If the <blockquote>
		 * has type=cite, then ignore it unless style_tag is "citation" */
		if (result && g_ascii_strncasecmp (element_tag, "blockquote", 10) == 0) {
			if (webkit_dom_element_has_attribute (element, "type")) {
				gchar *type;
				type = webkit_dom_element_get_attribute (element, "type");
				if (!accept_citation && (g_ascii_strncasecmp (type, "cite", 4) == 0)) {
					result = FALSE;
				}
				g_free (type);
			} else {
				if (accept_citation)
					result = FALSE;
			}
		}

		g_free (element_tag);

		if (result)
			break;

		element = webkit_dom_node_get_parent_element (
			WEBKIT_DOM_NODE (element));
	}

	return result;
}

static gchar *
get_font_property (EHTMLEditorSelection *selection,
                   const gchar *font_property)
{
	WebKitDOMRange *range;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	gchar *value;

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return NULL;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	element = e_html_editor_dom_node_find_parent_element (node, "FONT");
	if (!element)
		return NULL;

	g_object_get (G_OBJECT (element), font_property, &value, NULL);

	return value;
}

static void
html_editor_selection_selection_changed_cb (WebKitWebView *webview,
                                            EHTMLEditorSelection *selection)
{
	g_object_freeze_notify (G_OBJECT (selection));

	g_object_notify (G_OBJECT (selection), "alignment");
	g_object_notify (G_OBJECT (selection), "background-color");
	g_object_notify (G_OBJECT (selection), "bold");
	g_object_notify (G_OBJECT (selection), "font-name");
	g_object_notify (G_OBJECT (selection), "font-size");
	g_object_notify (G_OBJECT (selection), "font-color");
	g_object_notify (G_OBJECT (selection), "block-format");
	g_object_notify (G_OBJECT (selection), "indented");
	g_object_notify (G_OBJECT (selection), "italic");
	g_object_notify (G_OBJECT (selection), "monospaced");
	g_object_notify (G_OBJECT (selection), "strikethrough");
	g_object_notify (G_OBJECT (selection), "subscript");
	g_object_notify (G_OBJECT (selection), "superscript");
	g_object_notify (G_OBJECT (selection), "text");
	g_object_notify (G_OBJECT (selection), "underline");

	g_object_thaw_notify (G_OBJECT (selection));
}

void
e_html_editor_selection_block_selection_changed (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_signal_handlers_block_by_func (
		view, html_editor_selection_selection_changed_cb, selection);
	g_object_unref (view);
}

void
e_html_editor_selection_unblock_selection_changed (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_signal_handlers_unblock_by_func (
		view, html_editor_selection_selection_changed_cb, selection);
	g_object_unref (view);
}

static void
html_editor_selection_set_html_editor_view (EHTMLEditorSelection *selection,
                                            EHTMLEditorView *view)
{
	gulong handler_id;

	g_return_if_fail (E_IS_HTML_EDITOR_VIEW (view));

	g_weak_ref_set (&selection->priv->html_editor_view, view);

	handler_id = g_signal_connect (
		view, "selection-changed",
		G_CALLBACK (html_editor_selection_selection_changed_cb),
		selection);

	selection->priv->selection_changed_handler_id = handler_id;
}

static void
html_editor_selection_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	GdkRGBA rgba = { 0 };

	switch (property_id) {
		case PROP_ALIGNMENT:
			g_value_set_int (
				value,
				e_html_editor_selection_get_alignment (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_BACKGROUND_COLOR:
			g_value_set_string (
				value,
				e_html_editor_selection_get_background_color (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_BLOCK_FORMAT:
			g_value_set_int (
				value,
				e_html_editor_selection_get_block_format (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_BOLD:
			g_value_set_boolean (
				value,
				e_html_editor_selection_is_bold (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_HTML_EDITOR_VIEW:
			g_value_take_object (
				value,
				e_html_editor_selection_ref_html_editor_view (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_FONT_COLOR:
			e_html_editor_selection_get_font_color (
				E_HTML_EDITOR_SELECTION (object), &rgba);
			g_value_set_boxed (value, &rgba);
			return;

		case PROP_FONT_NAME:
			g_value_set_string (
				value,
				e_html_editor_selection_get_font_name (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_FONT_SIZE:
			g_value_set_int (
				value,
				e_html_editor_selection_get_font_size (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_INDENTED:
			g_value_set_boolean (
				value,
				e_html_editor_selection_is_indented (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_ITALIC:
			g_value_set_boolean (
				value,
				e_html_editor_selection_is_italic (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_MONOSPACED:
			g_value_set_boolean (
				value,
				e_html_editor_selection_is_monospaced (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_STRIKETHROUGH:
			g_value_set_boolean (
				value,
				e_html_editor_selection_is_strikethrough (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_SUBSCRIPT:
			g_value_set_boolean (
				value,
				e_html_editor_selection_is_subscript (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_SUPERSCRIPT:
			g_value_set_boolean (
				value,
				e_html_editor_selection_is_superscript (
				E_HTML_EDITOR_SELECTION (object)));
			return;

		case PROP_TEXT:
			g_value_set_string (
				value,
				e_html_editor_selection_get_string (
				E_HTML_EDITOR_SELECTION (object)));
			break;

		case PROP_UNDERLINE:
			g_value_set_boolean (
				value,
				e_html_editor_selection_is_underline (
				E_HTML_EDITOR_SELECTION (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
html_editor_selection_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALIGNMENT:
			e_html_editor_selection_set_alignment (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_int (value));
			return;

		case PROP_BACKGROUND_COLOR:
			e_html_editor_selection_set_background_color (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_string (value));
			return;

		case PROP_BOLD:
			e_html_editor_selection_set_bold (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_HTML_EDITOR_VIEW:
			html_editor_selection_set_html_editor_view (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_object (value));
			return;

		case PROP_FONT_COLOR:
			e_html_editor_selection_set_font_color (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_boxed (value));
			return;

		case PROP_BLOCK_FORMAT:
			e_html_editor_selection_set_block_format (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_int (value));
			return;

		case PROP_FONT_NAME:
			e_html_editor_selection_set_font_name (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_string (value));
			return;

		case PROP_FONT_SIZE:
			e_html_editor_selection_set_font_size (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_int (value));
			return;

		case PROP_ITALIC:
			e_html_editor_selection_set_italic (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_MONOSPACED:
			e_html_editor_selection_set_monospaced (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_STRIKETHROUGH:
			e_html_editor_selection_set_strikethrough (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_SUBSCRIPT:
			e_html_editor_selection_set_subscript (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_SUPERSCRIPT:
			e_html_editor_selection_set_superscript (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_UNDERLINE:
			e_html_editor_selection_set_underline (
				E_HTML_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
html_editor_selection_dispose (GObject *object)
{
	EHTMLEditorSelectionPrivate *priv;
	EHTMLEditorView *view;

	priv = E_HTML_EDITOR_SELECTION_GET_PRIVATE (object);

	view = g_weak_ref_get (&priv->html_editor_view);
	if (view != NULL) {
		g_signal_handler_disconnect (
			view, priv->selection_changed_handler_id);
		priv->selection_changed_handler_id = 0;
		g_object_unref (view);
	}

	g_weak_ref_set (&priv->html_editor_view, NULL);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_html_editor_selection_parent_class)->dispose (object);
}

static void
html_editor_selection_finalize (GObject *object)
{
	EHTMLEditorSelection *selection = E_HTML_EDITOR_SELECTION (object);

	g_free (selection->priv->text);
	g_free (selection->priv->background_color);
	g_free (selection->priv->font_color);
	g_free (selection->priv->font_family);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_html_editor_selection_parent_class)->finalize (object);
}

static void
e_html_editor_selection_class_init (EHTMLEditorSelectionClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorSelectionPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = html_editor_selection_get_property;
	object_class->set_property = html_editor_selection_set_property;
	object_class->dispose = html_editor_selection_dispose;
	object_class->finalize = html_editor_selection_finalize;

	/**
	 * EHTMLEditorSelectionalignment
	 *
	 * Holds alignment of current paragraph.
	 */
	/* FIXME: Convert the enum to a proper type */
	g_object_class_install_property (
		object_class,
		PROP_ALIGNMENT,
		g_param_spec_int (
			"alignment",
			NULL,
			NULL,
			E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT,
			E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT,
			E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT,
			G_PARAM_READWRITE));

	/**
	 * EHTMLEditorSelectionbackground-color
	 *
	 * Holds background color of current selection or at current cursor
	 * position.
	 */
	g_object_class_install_property (
		object_class,
		PROP_BACKGROUND_COLOR,
		g_param_spec_string (
			"background-color",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	/**
	 * EHTMLEditorSelectionblock-format
	 *
	 * Holds block format of current paragraph. See
	 * #EHTMLEditorSelectionBlockFormat for valid values.
	 */
	/* FIXME Convert the EHTMLEditorSelectionBlockFormat
	 *       enum to a proper type. */
	g_object_class_install_property (
		object_class,
		PROP_BLOCK_FORMAT,
		g_param_spec_int (
			"block-format",
			NULL,
			NULL,
			0,
			G_MAXINT,
			0,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectionbold
	 *
	 * Holds whether current selection or text at current cursor position
	 * is bold.
	 */
	g_object_class_install_property (
		object_class,
		PROP_BOLD,
		g_param_spec_boolean (
			"bold",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_HTML_EDITOR_VIEW,
		g_param_spec_object (
			"html-editor-view",
			NULL,
			NULL,
			E_TYPE_HTML_EDITOR_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectionfont-color
	 *
	 * Holds font color of current selection or at current cursor position.
	 */
	g_object_class_install_property (
		object_class,
		PROP_FONT_COLOR,
		g_param_spec_boxed (
			"font-color",
			NULL,
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectionfont-name
	 *
	 * Holds name of font in current selection or at current cursor
	 * position.
	 */
	g_object_class_install_property (
		object_class,
		PROP_FONT_NAME,
		g_param_spec_string (
			"font-name",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectionfont-size
	 *
	 * Holds point size of current selection or at current cursor position.
	 */
	g_object_class_install_property (
		object_class,
		PROP_FONT_SIZE,
		g_param_spec_int (
			"font-size",
			NULL,
			NULL,
			1,
			7,
			3,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectionindented
	 *
	 * Holds whether current paragraph is indented. This does not include
	 * citations.
	 */
	g_object_class_install_property (
		object_class,
		PROP_INDENTED,
		g_param_spec_boolean (
			"indented",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectionitalic
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is italic.
	 */
	g_object_class_install_property (
		object_class,
		PROP_ITALIC,
		g_param_spec_boolean (
			"italic",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectionmonospaced
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is monospaced.
	 */
	g_object_class_install_property (
		object_class,
		PROP_MONOSPACED,
		g_param_spec_boolean (
			"monospaced",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectionstrikethrough
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is strikethrough.
	 */
	g_object_class_install_property (
		object_class,
		PROP_STRIKETHROUGH,
		g_param_spec_boolean (
			"strikethrough",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectionsuperscript
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is in superscript.
	 */
	g_object_class_install_property (
		object_class,
		PROP_SUPERSCRIPT,
		g_param_spec_boolean (
			"superscript",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectionsubscript
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is in subscript.
	 */
	g_object_class_install_property (
		object_class,
		PROP_SUBSCRIPT,
		g_param_spec_boolean (
			"subscript",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectiontext
	 *
	 * Holds always up-to-date text of current selection.
	 */
	g_object_class_install_property (
		object_class,
		PROP_TEXT,
		g_param_spec_string (
			"text",
			NULL,
			NULL,
			NULL,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EHTMLEditorSelectionunderline
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is underlined.
	 */
	g_object_class_install_property (
		object_class,
		PROP_UNDERLINE,
		g_param_spec_boolean (
			"underline",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_html_editor_selection_init (EHTMLEditorSelection *selection)
{
	GSettings *g_settings;

	selection->priv = E_HTML_EDITOR_SELECTION_GET_PRIVATE (selection);

	g_settings = g_settings_new ("org.gnome.evolution.mail");
	selection->priv->word_wrap_length =
		g_settings_get_int (g_settings, "composer-word-wrap-length");
	g_object_unref (g_settings);
}

gint
e_html_editor_selection_get_word_wrap_length (EHTMLEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), 72);

	return selection->priv->word_wrap_length;
}

/**
 * e_html_editor_selection_ref_html_editor_view:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns a new reference to @selection's #EHTMLEditorView.  Unreference
 * the #EHTMLEditorView with g_object_unref() when finished with it.
 *
 * Returns: an #EHTMLEditorView
 **/
EHTMLEditorView *
e_html_editor_selection_ref_html_editor_view (EHTMLEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);

	return g_weak_ref_get (&selection->priv->html_editor_view);
}

/**
 * e_html_editor_selection_has_text:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection contains any text.
 *
 * Returns: @TRUE when current selection contains text, @FALSE otherwise.
 */
gboolean
e_html_editor_selection_has_text (EHTMLEditorSelection *selection)
{
	WebKitDOMRange *range;
	WebKitDOMNode *node;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	range = html_editor_selection_get_current_range (selection);

	node = webkit_dom_range_get_start_container (range, NULL);
	if (webkit_dom_node_get_node_type (node) == 3)
		return TRUE;

	node = webkit_dom_range_get_end_container (range, NULL);
	if (webkit_dom_node_get_node_type (node) == 3)
		return TRUE;

	node = WEBKIT_DOM_NODE (webkit_dom_range_clone_contents (range, NULL));
	while (node) {
		if (webkit_dom_node_get_node_type (node) == 3)
			return TRUE;

		if (webkit_dom_node_has_child_nodes (node)) {
			node = webkit_dom_node_get_first_child (node);
		} else if (webkit_dom_node_get_next_sibling (node)) {
			node = webkit_dom_node_get_next_sibling (node);
		} else {
			node = webkit_dom_node_get_parent_node (node);
			if (node) {
				node = webkit_dom_node_get_next_sibling (node);
			}
		}
	}

	return FALSE;
}

/**
 * e_html_editor_selection_get_caret_word:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns word under cursor.
 *
 * Returns: A newly allocated string with current caret word or @NULL when there
 * is no text under cursor or when selection is active. [transfer-full].
 */
gchar *
e_html_editor_selection_get_caret_word (EHTMLEditorSelection *selection)
{
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);

	range = html_editor_selection_get_current_range (selection);

	/* Don't operate on the visible selection */
	range = webkit_dom_range_clone_range (range, NULL);
	webkit_dom_range_expand (range, "word", NULL);

	return webkit_dom_range_to_string (range, NULL);
}

/**
 * e_html_editor_selection_replace_caret_word:
 * @selection: an #EHTMLEditorSelection
 * @replacement: a string to replace current caret word with
 *
 * Replaces current word under cursor with @replacement.
 */
void
e_html_editor_selection_replace_caret_word (EHTMLEditorSelection *selection,
                                            const gchar *replacement)
{
	EHTMLEditorView *view;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (replacement != NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	web_view = WEBKIT_WEB_VIEW (view);

	range = html_editor_selection_get_current_range (selection);
	document = webkit_web_view_get_dom_document (web_view);
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	webkit_dom_range_expand (range, "word", NULL);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	e_html_editor_selection_insert_html (selection, replacement);

	g_object_unref (view);
}

/**
 * e_html_editor_selection_get_string:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns currently selected string.
 *
 * Returns: A pointer to content of current selection. The string is owned by
 * #EHTMLEditorSelection and should not be free'd.
 */
const gchar *
e_html_editor_selection_get_string (EHTMLEditorSelection *selection)
{
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return NULL;

	g_free (selection->priv->text);
	selection->priv->text = webkit_dom_range_get_text (range);

	return selection->priv->text;
}

/**
 * e_html_editor_selection_replace:
 * @selection: an #EHTMLEditorSelection
 * @new_string: a string to replace current selection with
 *
 * Replaces currently selected text with @new_string.
 */
void
e_html_editor_selection_replace (EHTMLEditorSelection *selection,
                                 const gchar *new_string)
{
	EHTMLEditorView *view;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	e_html_editor_view_exec_command (
		view, E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, new_string);

	g_object_unref (view);
}

/**
 * e_html_editor_selection_get_list_alignment_from_node:
 * @node: #an WebKitDOMNode
 *
 * Returns alignment of given list.
 *
 * Returns: #EHTMLEditorSelectionAlignment
 */
EHTMLEditorSelectionAlignment
e_html_editor_selection_get_list_alignment_from_node (WebKitDOMNode *node)
{
	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-list-item-align-left"))
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-list-item-align-center"))
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER;
	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-list-item-align-right"))
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT;

	return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
}

/**
 * e_html_editor_selection_get_alignment:
 * @selection: #an EHTMLEditorSelection
 *
 * Returns alignment of current paragraph
 *
 * Returns: #EHTMLEditorSelectionAlignment
 */
EHTMLEditorSelectionAlignment
e_html_editor_selection_get_alignment (EHTMLEditorSelection *selection)
{
	EHTMLEditorSelectionAlignment alignment;
	EHTMLEditorView *view;
	gchar *value;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_val_if_fail (
		E_IS_HTML_EDITOR_SELECTION (selection),
		E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);
	window = webkit_dom_document_get_default_view (document);
	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;

	node = webkit_dom_range_get_start_container (range, NULL);
	if (!node)
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-align");

	if (!value || !*value ||
	    (g_ascii_strncasecmp (value, "left", 4) == 0)) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	} else if (g_ascii_strncasecmp (value, "center", 6) == 0) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER;
	} else if (g_ascii_strncasecmp (value, "right", 5) == 0) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT;
	} else {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	}

	g_free (value);

	return alignment;
}

static void
set_ordered_list_type_to_element (WebKitDOMElement *list,
                                  EHTMLEditorSelectionBlockFormat format)
{
	if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST)
		webkit_dom_element_remove_attribute (list, "type");
	else if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA)
		webkit_dom_element_set_attribute (list, "type", "A", NULL);
	else if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN)
		webkit_dom_element_set_attribute (list, "type", "I", NULL);
}

static WebKitDOMElement *
create_list_element (EHTMLEditorSelection *selection,
                     WebKitDOMDocument *document,
                     EHTMLEditorSelectionBlockFormat format,
		     gint level,
                     gboolean html_mode)
{
	WebKitDOMElement *list;
	gint offset = -SPACES_PER_LIST_LEVEL;
	gboolean inserting_unordered_list =
		format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;

	list = webkit_dom_document_create_element (
		document, inserting_unordered_list  ? "UL" : "OL", NULL);

	set_ordered_list_type_to_element (list, format);

	if (level >= 0)
		offset = (level + 1) * -SPACES_PER_LIST_LEVEL;

	if (!html_mode)
		e_html_editor_selection_set_paragraph_style (
			selection, list, -1, offset, "");

	return list;
}

static void
remove_node (WebKitDOMNode *node)
{
	webkit_dom_node_remove_child (
		webkit_dom_node_get_parent_node (node), node, NULL);
}

static void
format_change_list_from_list (EHTMLEditorSelection *selection,
                              WebKitDOMDocument *document,
                              EHTMLEditorSelectionBlockFormat to,
                              gboolean html_mode)
{
	gboolean after_selection_end = FALSE;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *new_list;
	WebKitDOMNode *source_list, *source_list_clone, *current_list, *item;

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return;

	new_list = create_list_element (selection, document, to, 0, html_mode);

	/* Copy elements from previous block to list */
	item = webkit_dom_node_get_parent_node (
		WEBKIT_DOM_NODE (selection_start_marker));
	source_list = webkit_dom_node_get_parent_node (item);
	current_list = source_list;
	source_list_clone = webkit_dom_node_clone_node (source_list, FALSE);

	if (element_has_class (WEBKIT_DOM_ELEMENT (source_list), "-x-evo-indented"))
		element_add_class (WEBKIT_DOM_ELEMENT (new_list), "-x-evo-indented");

	while (item) {
		WebKitDOMNode *next_item = webkit_dom_node_get_next_sibling (item);

		if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (item)) {
			webkit_dom_node_append_child (
				after_selection_end ?
					source_list_clone : WEBKIT_DOM_NODE (new_list),
				WEBKIT_DOM_NODE (item),
				NULL);
		}

		if (webkit_dom_node_contains (item, WEBKIT_DOM_NODE (selection_end_marker))) {
			source_list_clone = webkit_dom_node_clone_node (current_list, FALSE);
			after_selection_end = TRUE;
		}

		if (!next_item) {
			if (after_selection_end)
				break;
			current_list = webkit_dom_node_get_next_sibling (current_list);
			next_item = webkit_dom_node_get_first_child (current_list);
		}
		item = next_item;
	}

	if (webkit_dom_node_has_child_nodes (source_list_clone))
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (source_list),
			WEBKIT_DOM_NODE (source_list_clone),
			webkit_dom_node_get_next_sibling (source_list), NULL);
	if (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (new_list)))
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (source_list),
			WEBKIT_DOM_NODE (new_list),
			webkit_dom_node_get_next_sibling (source_list), NULL);
	if (!webkit_dom_node_has_child_nodes (source_list))
		remove_node (source_list);
}

/**
 * e_html_editor_selection_set_alignment:
 * @selection: an #EHTMLEditorSelection
 * @alignment: an #EHTMLEditorSelectionAlignment value to apply
 *
 * Sets alignment of current paragraph to give @alignment.
 */
void
e_html_editor_selection_set_alignment (EHTMLEditorSelection *selection,
                                       EHTMLEditorSelectionAlignment alignment)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;
	const gchar *class = "";
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker;
	WebKitDOMNode *parent;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_get_alignment (selection) == alignment)
		return;

	switch (alignment) {
		case E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER:
			command = E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_CENTER;
			class  = "-x-evo-list-item-align-center";
			break;

		case E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT:
			command = E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_LEFT;
			break;

		case E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT:
			command = E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_RIGHT;
			class  = "-x-evo-list-item-align-right";
			break;
	}

	selection->priv->alignment = alignment;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	e_html_editor_selection_save (selection);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);

	if (!selection_start_marker) {
		g_object_unref (view);
		return;
	}

	parent = webkit_dom_node_get_parent_node (
		WEBKIT_DOM_NODE (selection_start_marker));

	if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (parent)) {
		element_remove_class (
			WEBKIT_DOM_ELEMENT (parent),
			"-x-evo-list-item-align-center");
		element_remove_class (
			WEBKIT_DOM_ELEMENT (parent),
			"-x-evo-list-item-align-right");

		element_add_class (WEBKIT_DOM_ELEMENT (parent), class);
	} else {
		e_html_editor_view_exec_command (view, command, NULL);
	}

	e_html_editor_selection_restore (selection);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "alignment");
}

/**
 * e_html_editor_selection_get_background_color:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns background color of currently selected text or letter at current
 * cursor position.
 *
 * Returns: A string with code of current background color.
 */
const gchar *
e_html_editor_selection_get_background_color (EHTMLEditorSelection *selection)
{
	WebKitDOMNode *ancestor;
	WebKitDOMRange *range;
	WebKitDOMCSSStyleDeclaration *css;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);

	range = html_editor_selection_get_current_range (selection);

	ancestor = webkit_dom_range_get_common_ancestor_container (range, NULL);

	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (ancestor));
	selection->priv->background_color =
		webkit_dom_css_style_declaration_get_property_value (
			css, "background-color");

	return selection->priv->background_color;
}

/**
 * e_html_editor_selection_set_background_color:
 * @selection: an #EHTMLEditorSelection
 * @color: code of new background color to set
 *
 * Changes background color of current selection or letter at current cursor
 * position to @color.
 */
void
e_html_editor_selection_set_background_color (EHTMLEditorSelection *selection,
                                              const gchar *color)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (color != NULL && *color != '\0');

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_BACKGROUND_COLOR;
	e_html_editor_view_exec_command (view, command, color);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "background-color");
}

static gint
get_indentation_level (WebKitDOMElement *element)
{
	WebKitDOMElement *parent;
	gint level = 0;

	if (element_has_class (element, "-x-evo-indented"))
		level++;

	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (element));
	/* Count level of indentation */
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (element_has_class (parent, "-x-evo-indented"))
			level++;

		parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (parent));
	}

	return level;
}

static WebKitDOMNode *
get_block_node (WebKitDOMRange *range)
{
	WebKitDOMNode *node;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	if (!WEBKIT_DOM_IS_ELEMENT (node))
		node = WEBKIT_DOM_NODE (webkit_dom_node_get_parent_element (node));

	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-temp-text-wrapper"))
		node = WEBKIT_DOM_NODE (webkit_dom_node_get_parent_element (node));

	return node;
}

/**
 * e_html_editor_selection_get_list_format_from_node:
 * @node: an #WebKitDOMNode
 *
 * Returns block format of given list.
 *
 * Returns: #EHTMLEditorSelectionBlockFormat
 */
EHTMLEditorSelectionBlockFormat
e_html_editor_selection_get_list_format_from_node (WebKitDOMNode *node)
{
	EHTMLEditorSelectionBlockFormat format =
		E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;

	if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (node))
		return -1;

	if (WEBKIT_DOM_IS_HTMLU_LIST_ELEMENT (node))
		return format;

	if (WEBKIT_DOM_IS_ELEMENT (node)) {
		gchar *type_value = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "type");

		if (!type_value)
			return E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST;

		if (!*type_value)
			format = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST;
		else if (g_ascii_strcasecmp (type_value, "A") == 0)
			format = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA;
		else if (g_ascii_strcasecmp (type_value, "I") == 0)
			format = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN;
		g_free (type_value);
	}

	return -1;
}

/**
 * e_html_editor_selection_get_block_format:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns block format of current paragraph.
 *
 * Returns: #EHTMLEditorSelectionBlockFormat
 */
EHTMLEditorSelectionBlockFormat
e_html_editor_selection_get_block_format (EHTMLEditorSelection *selection)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range;
	WebKitDOMElement *element;
	EHTMLEditorSelectionBlockFormat result;

	g_return_val_if_fail (
		E_IS_HTML_EDITOR_SELECTION (selection),
		E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;

	node = webkit_dom_range_get_start_container (range, NULL);

	if (e_html_editor_dom_node_find_parent_element (node, "UL")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;
	} else if ((element = e_html_editor_dom_node_find_parent_element (node, "OL")) != NULL) {
		if (webkit_dom_element_has_attribute (element, "type")) {
			gchar *type;

			type = webkit_dom_element_get_attribute (element, "type");
			if (type && ((*type == 'a') || (*type == 'A'))) {
				result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA;
			} else if (type && ((*type == 'i') || (*type == 'I'))) {
				result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN;
			} else {
				result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST;
			}

			g_free (type);
		} else {
			result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST;
		}
	} else if (e_html_editor_dom_node_find_parent_element (node, "PRE")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PRE;
	} else if (e_html_editor_dom_node_find_parent_element (node, "ADDRESS")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS;
	} else if (e_html_editor_dom_node_find_parent_element (node, "H1")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H1;
	} else if (e_html_editor_dom_node_find_parent_element (node, "H2")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H2;
	} else if (e_html_editor_dom_node_find_parent_element (node, "H3")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H3;
	} else if (e_html_editor_dom_node_find_parent_element (node, "H4")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H4;
	} else if (e_html_editor_dom_node_find_parent_element (node, "H5")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H5;
	} else if (e_html_editor_dom_node_find_parent_element (node, "H6")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H6;
	} else if ((element = e_html_editor_dom_node_find_parent_element (node, "BLOCKQUOTE")) != NULL) {
		if (element_has_class (element, "-x-evo-indented"))
			result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
		else {
			WebKitDOMNode *block = get_block_node (range);

			if (element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-paragraph"))
				result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
			else
				result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE;
		}
	} else if (e_html_editor_dom_node_find_parent_element (node, "P")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
	} else {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
	}

	return result;
}

static gboolean
is_caret_position_node (WebKitDOMNode *node)
{
	return element_has_id (WEBKIT_DOM_ELEMENT (node), "-x-evo-caret-position");
}

static void
merge_list_into_list (WebKitDOMNode *from,
                      WebKitDOMNode *to,
                      gboolean insert_before)
{
	WebKitDOMNode *item;

	if (!(to && from))
		return;

	while ((item = webkit_dom_node_get_first_child (from)) != NULL) {
		if (insert_before)
			webkit_dom_node_insert_before (
				to, item, webkit_dom_node_get_last_child (to), NULL);
		else
			webkit_dom_node_append_child (to, item, NULL);
	}

	if (!webkit_dom_node_has_child_nodes (from))
		remove_node (from);

}

static void
merge_lists_if_possible (WebKitDOMNode *list)
{
	EHTMLEditorSelectionBlockFormat format, prev, next;
	WebKitDOMNode *prev_sibling, *next_sibling;

	prev_sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (list));
	next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (list));

	format = e_html_editor_selection_get_list_format_from_node (list),
	prev = e_html_editor_selection_get_list_format_from_node (prev_sibling);
	next = e_html_editor_selection_get_list_format_from_node (next_sibling);

	if (format == prev && format != -1 && prev != -1)
		merge_list_into_list (prev_sibling, list, TRUE);

	if (format == next && format != -1 && next != -1)
		merge_list_into_list (next_sibling, list, FALSE);
}

static void
remove_wrapping (WebKitDOMElement *element)
{
	WebKitDOMNodeList *list;
	gint ii, length;

	list = webkit_dom_element_query_selector_all (
		element, "br.-x-evo-wrap-br", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++)
		remove_node (webkit_dom_node_list_item (list, ii));
}

static void
remove_quoting (WebKitDOMElement *element)
{
	WebKitDOMNodeList *list;
	gint ii, length;

	list = webkit_dom_element_query_selector_all (
		element, "span.-x-evo-quoted", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		remove_node (webkit_dom_node_list_item (list, ii));
	}

	list = webkit_dom_element_query_selector_all (
		element, "span.-x-evo-temp-text-wrapper", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *nd = webkit_dom_node_list_item (list, ii);

		while (webkit_dom_node_has_child_nodes (nd)) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (nd),
				webkit_dom_node_get_first_child (nd),
				nd,
				NULL);
		}

		remove_node (nd);
	}

	webkit_dom_node_normalize (WEBKIT_DOM_NODE (element));
}

static gboolean
node_is_list (WebKitDOMNode *node)
{
	return node && (
		WEBKIT_DOM_IS_HTMLO_LIST_ELEMENT (node) ||
		WEBKIT_DOM_IS_HTMLU_LIST_ELEMENT (node));
}

static gint
get_citation_level (WebKitDOMNode *node)
{
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);
	gint level = 0;

	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent) &&
		    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "type"))
			level++;

		parent = webkit_dom_node_get_parent_node (parent);
	}

	return level;
}

static void
format_change_block_to_block (EHTMLEditorSelection *selection,
                              EHTMLEditorSelectionBlockFormat format,
                              EHTMLEditorView *view,
                              const gchar *value,
                              WebKitDOMDocument *document)
{
	gboolean after_selection_end, quoted = FALSE;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *element;
	WebKitDOMNode *block, *next_block;

	e_html_editor_selection_save (selection);
	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);
	block = webkit_dom_node_get_parent_node (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-temp-text-wrapper")) {
		block = webkit_dom_node_get_parent_node (block);
		remove_wrapping (WEBKIT_DOM_ELEMENT (block));
		remove_quoting (WEBKIT_DOM_ELEMENT (block));
		quoted = TRUE;
	}

	/* Process all blocks that are in the selection one by one */
	while (block) {
		WebKitDOMNode *child;

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		next_block = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (block));

		if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH)
			element = e_html_editor_selection_get_paragraph_element (
				selection, document, -1, 0);
		else
			element = webkit_dom_document_create_element (
				document, value, NULL);

		while ((child = webkit_dom_node_get_first_child (block)))
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element), child, NULL);

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			WEBKIT_DOM_NODE (element),
			block,
			NULL);

		remove_node (block);

		block = next_block;

		if (after_selection_end)
			break;
	}

	if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH &&
	    !e_html_editor_view_get_html_mode (view)) {
		gint citation_level, quote;

		citation_level = get_citation_level (WEBKIT_DOM_NODE (element));
		quote = citation_level ? citation_level + 1 : 0;

		element = e_html_editor_selection_wrap_paragraph_length (
			selection, element, selection->priv->word_wrap_length - quote);
	}

	if (quoted)
		e_html_editor_view_quote_plain_text_element (view, element);

	e_html_editor_selection_restore (selection);
}

static void
remove_node_if_empty (WebKitDOMNode *node)
{
	if (!WEBKIT_DOM_IS_NODE (node))
		return;

	if (!webkit_dom_node_get_first_child (node)) {
		remove_node (node);
	} else {
		gchar *text_content;

		text_content = webkit_dom_node_get_text_content (node);
		if (!text_content)
			remove_node (node);

		if (text_content && !*text_content)
			remove_node (node);

		g_free (text_content);
	}
}

static void
format_change_block_to_list (EHTMLEditorSelection *selection,
                             EHTMLEditorSelectionBlockFormat format,
                             EHTMLEditorView *view,
                             WebKitDOMDocument *document)
{
	gboolean after_selection_end;
	gboolean html_mode = e_html_editor_view_get_html_mode (view);
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *item, *list;
	WebKitDOMNode *block, *next_block;

	e_html_editor_selection_save (selection);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);
	block = webkit_dom_node_get_parent_node (
		WEBKIT_DOM_NODE (selection_start_marker));

	list = create_list_element (selection, document, format, 0, html_mode);

	if (element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-temp-text-wrapper")) {
		WebKitDOMElement *element;

		block = webkit_dom_node_get_parent_node (block);

		remove_wrapping (WEBKIT_DOM_ELEMENT (block));
		remove_quoting (WEBKIT_DOM_ELEMENT (block));

		e_html_editor_view_exec_command (
			view, E_HTML_EDITOR_VIEW_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, NULL);

		element = webkit_dom_document_query_selector (
			document, "body>br", NULL);

		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			WEBKIT_DOM_NODE (list),
			WEBKIT_DOM_NODE (element),
			NULL);

		selection_start_marker = webkit_dom_document_query_selector (
			document, "span#-x-evo-selection-start-marker", NULL);
		selection_end_marker = webkit_dom_document_query_selector (
			document, "span#-x-evo-selection-end-marker", NULL);
		block = webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (selection_start_marker));
	} else
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			WEBKIT_DOM_NODE (list),
			block,
			NULL);

	/* Process all blocks that are in the selection one by one */
	while (block) {
		gboolean empty = FALSE;
		gchar *content;
		WebKitDOMNode *child;

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		next_block = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (block));

		item = webkit_dom_document_create_element (document, "LI", NULL);
		content = webkit_dom_node_get_text_content (block);

		empty = !*content || (g_strcmp0 (content, UNICODE_ZERO_WIDTH_SPACE) == 0);
		g_free (content);

		/* We have to use again the hidden space to move caret into newly inserted list */
		if (empty) {
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (item),
				UNICODE_ZERO_WIDTH_SPACE,
				NULL);
		}

		while ((child = webkit_dom_node_get_first_child (block)))
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (item), child, NULL);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (list), WEBKIT_DOM_NODE (item), NULL);

		remove_node (block);

		block = next_block;

		if (after_selection_end)
			break;
	}

	merge_lists_if_possible (WEBKIT_DOM_NODE (list));

	e_html_editor_view_force_spell_check (view);
	e_html_editor_selection_restore (selection);
}

static void
format_change_list_to_list (EHTMLEditorSelection *selection,
                            EHTMLEditorSelectionBlockFormat format,
                            WebKitDOMDocument *document,
                            gboolean html_mode)
{
	EHTMLEditorSelectionBlockFormat prev = 0, next = 0;
	gboolean done = FALSE, indented = FALSE;
	gboolean selection_starts_in_first_child, selection_ends_in_last_child;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *prev_list, *current_list, *next_list;

	e_html_editor_selection_save (selection);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	current_list = webkit_dom_node_get_parent_node (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (selection_start_marker)));

	prev_list = webkit_dom_node_get_parent_node (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (selection_start_marker)));

	next_list = webkit_dom_node_get_parent_node (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (selection_end_marker)));

	selection_starts_in_first_child =
		webkit_dom_node_contains (
			webkit_dom_node_get_first_child (current_list),
			WEBKIT_DOM_NODE (selection_start_marker));

	selection_ends_in_last_child =
		webkit_dom_node_contains (
			webkit_dom_node_get_last_child (current_list),
			WEBKIT_DOM_NODE (selection_end_marker));

	indented = element_has_class (WEBKIT_DOM_ELEMENT (current_list), "-x-evo-indented");

	if (!prev_list || !next_list || indented) {
		format_change_list_from_list (selection, document, format, html_mode);
		goto out;
	}

	if (webkit_dom_node_is_same_node (prev_list, next_list)) {
		prev_list = webkit_dom_node_get_previous_sibling (
			webkit_dom_node_get_parent_node (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_start_marker))));
		next_list = webkit_dom_node_get_next_sibling (
			webkit_dom_node_get_parent_node (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_end_marker))));
		if (!prev_list || !next_list) {
			format_change_list_from_list (selection, document, format, html_mode);
			goto out;
		}
	}

	prev = e_html_editor_selection_get_list_format_from_node (prev_list);
	next = e_html_editor_selection_get_list_format_from_node (next_list);

	if (format == prev && format != -1 && prev != -1) {
		if (selection_starts_in_first_child && selection_ends_in_last_child) {
			done = TRUE;
			merge_list_into_list (current_list, prev_list, FALSE);
		}
	}

	if (format == next && format != -1 && next != -1) {
		if (selection_starts_in_first_child && selection_ends_in_last_child) {
			done = TRUE;
			merge_list_into_list (next_list, prev_list, FALSE);
		}
	}

	if (done)
		goto out;

	format_change_list_from_list (selection, document, format, html_mode);
out:
	e_html_editor_selection_restore (selection);
}

static void
format_change_list_to_block (EHTMLEditorSelection *selection,
                             EHTMLEditorSelectionBlockFormat format,
                             WebKitDOMDocument *document)
{
	gboolean after_end = FALSE;
	WebKitDOMElement *selection_start, *element, *selection_end;
	WebKitDOMNode *source_list, *next_item, *item, *source_list_clone;

	e_html_editor_selection_save (selection);

	selection_start = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	item = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start));
	source_list = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (item));
	source_list_clone = webkit_dom_node_clone_node (source_list, FALSE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (source_list),
		WEBKIT_DOM_NODE (source_list_clone),
		webkit_dom_node_get_next_sibling (source_list),
		NULL);

	next_item = item;

	/* Process all nodes that are in selection one by one */
	while (next_item) {
		WebKitDOMNode *tmp;

		tmp = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (next_item));

		if (!after_end) {
			if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH)
				element = e_html_editor_selection_get_paragraph_element (selection, document, -1, 0);
			else if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PRE)
				element = webkit_dom_document_create_element (document, "PRE", NULL);
			else if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE)
				element = webkit_dom_document_create_element (document, "BLOCKQUOTE", NULL);
			else
				element = e_html_editor_selection_get_paragraph_element (selection, document, -1, 0);

			after_end = webkit_dom_node_contains (next_item, WEBKIT_DOM_NODE (selection_end));

			while (webkit_dom_node_get_first_child (next_item)) {
				WebKitDOMNode *node = webkit_dom_node_get_first_child (next_item);

				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (element), node, NULL);
			}

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (source_list),
				WEBKIT_DOM_NODE (element),
				source_list_clone,
				NULL);

			remove_node (next_item);

			next_item = tmp;
		} else {
			webkit_dom_node_append_child (
				source_list_clone, next_item, NULL);

			next_item = tmp;
		}
	}

	remove_node_if_empty (source_list_clone);
	remove_node_if_empty (source_list);

	e_html_editor_selection_restore (selection);
}

/**
 * e_html_editor_selection_set_block_format:
 * @selection: an #EHTMLEditorSelection
 * @format: an #EHTMLEditorSelectionBlockFormat value
 *
 * Changes block format of current paragraph to @format.
 */
void
e_html_editor_selection_set_block_format (EHTMLEditorSelection *selection,
                                          EHTMLEditorSelectionBlockFormat format)
{
	EHTMLEditorView *view;
	EHTMLEditorSelectionBlockFormat current_format;
	const gchar *value;
	gboolean has_selection = FALSE;
	gboolean from_list = FALSE, to_list = FALSE, html_mode;
	WebKitDOMDocument *document;
	WebKitDOMRange *range;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	current_format = e_html_editor_selection_get_block_format (selection);
	if (current_format == format) {
		return;
	}

	switch (format) {
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE:
			value = "BLOCKQUOTE";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H1:
			value = "H1";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H2:
			value = "H2";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H3:
			value = "H3";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H4:
			value = "H4";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H5:
			value = "H5";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H6:
			value = "H6";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH:
			value = "P";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PRE:
			value = "PRE";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS:
			value = "ADDRESS";
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST:
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA:
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN:
			to_list = TRUE;
			value = NULL;
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST:
			to_list = TRUE;
			value = NULL;
			break;
		case E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_NONE:
		default:
			value = NULL;
			break;
	}

	if (g_strcmp0 (e_html_editor_selection_get_string (selection), "") != 0)
		has_selection = TRUE;

	/* H1 - H6 have bold font by default */
	if (format >= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H1 &&
	    format <= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H6)
		selection->priv->is_bold = TRUE;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	html_mode = e_html_editor_view_get_html_mode (view);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	from_list =
		current_format >= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;

	range = html_editor_selection_get_current_range (selection);
	if (!range) {
		g_object_unref (view);
		return;
	}

	if (from_list && to_list)
		format_change_list_to_list (selection, format, document, html_mode);

	if (!from_list && !to_list)
		format_change_block_to_block (selection, format, view, value, document);

	if (from_list && !to_list)
		format_change_list_to_block (selection, format, document);

	if (!from_list && to_list)
		format_change_block_to_list (selection, format, view, document);

	if (!has_selection)
		e_html_editor_view_force_spell_check (view);

	g_object_unref (view);

	/* When changing the format we need to re-set the alignment */
	e_html_editor_selection_set_alignment (selection, selection->priv->alignment);

	g_object_notify (G_OBJECT (selection), "block-format");
}

/**
 * e_html_editor_selection_get_font_color:
 * @selection: an #EHTMLEditorSelection
 * @rgba: a #GdkRGBA object to be set to current font color
 *
 * Sets @rgba to contain color of current text selection or letter at current
 * cursor position.
 */
void
e_html_editor_selection_get_font_color (EHTMLEditorSelection *selection,
                                        GdkRGBA *rgba)
{
	gchar *color;
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (g_strcmp0 (e_html_editor_selection_get_string (selection), "") == 0) {
		color = g_strdup (selection->priv->font_color);
	} else {
		color = get_font_property (selection, "color");
		if (!color) {
			*rgba = black;
			return;
		}
	}

	gdk_rgba_parse (rgba, color);
	g_free (color);
}

/**
 * e_html_editor_selection_set_font_color:
 * @selection: an #EHTMLEditorSelection
 * @rgba: a #GdkRGBA
 *
 * Sets font color of current selection or letter at current cursor position to
 * color defined in @rgba.
 */
void
e_html_editor_selection_set_font_color (EHTMLEditorSelection *selection,
                                        const GdkRGBA *rgba)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;
	guint32 rgba_value;
	gchar *color;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (!rgba)
		rgba = &black;

	rgba_value = e_rgba_to_value ((GdkRGBA *) rgba);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_FORE_COLOR;
	color = g_strdup_printf ("#%06x", rgba_value);
	selection->priv->font_color = g_strdup (color);
	e_html_editor_view_exec_command (view, command, color);
	g_free (color);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "font-color");
}

/**
 * e_html_editor_selection_get_font_name:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns name of font used in current selection or at letter at current cursor
 * position.
 *
 * Returns: A string with font name. [transfer-none]
 */
const gchar *
e_html_editor_selection_get_font_name (EHTMLEditorSelection *selection)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range;
	WebKitDOMCSSStyleDeclaration *css;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);

	range = html_editor_selection_get_current_range (selection);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);

	g_free (selection->priv->font_family);
	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (node));
	selection->priv->font_family =
		webkit_dom_css_style_declaration_get_property_value (css, "fontFamily");

	return selection->priv->font_family;
}

/**
 * e_html_editor_selection_set_font_name:
 * @selection: an #EHTMLEditorSelection
 * @font_name: a font name to apply
 *
 * Sets font name of current selection or of letter at current cursor position
 * to @font_name.
 */
void
e_html_editor_selection_set_font_name (EHTMLEditorSelection *selection,
                                       const gchar *font_name)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_FONT_NAME;
	e_html_editor_view_exec_command (view, command, font_name);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "font-name");
}

/**
 * e_editor_Selection_get_font_size:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns point size of current selection or of letter at current cursor position.
 */
 guint
e_html_editor_selection_get_font_size (EHTMLEditorSelection *selection)
{
	gchar *size;
	guint size_int;

	g_return_val_if_fail (
		E_IS_HTML_EDITOR_SELECTION (selection),
		E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL);

	size = get_font_property (selection, "size");
	if (!size)
		return E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;

	size_int = atoi (size);
	g_free (size);

	if (size_int == 0)
		return E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;

	return size_int;
}

/**
 * e_html_editor_selection_set_font_size:
 * @selection: an #EHTMLEditorSelection
 * @font_size: point size to apply
 *
 * Sets font size of current selection or of letter at current cursor position
 * to @font_size.
 */
void
e_html_editor_selection_set_font_size (EHTMLEditorSelection *selection,
                                       guint font_size)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;
	gchar *size_str;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	selection->priv->font_size = font_size;
	command = E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE;
	size_str = g_strdup_printf ("%d", font_size);
	e_html_editor_view_exec_command (view, command, size_str);
	g_free (size_str);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "font-size");
}

/**
 * e_html_editor_selection_is_citation:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current paragraph is a citation.
 *
 * Returns: @TRUE when current paragraph is a citation, @FALSE otherwise.
 */
gboolean
e_html_editor_selection_is_citation (EHTMLEditorSelection *selection)
{
	gboolean ret_val;
	gchar *value, *text_content;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);

	if (WEBKIT_DOM_IS_TEXT (node))
		return get_has_style (selection, "citation");

	/* If we are changing the format of block we have to re-set bold property,
	 * otherwise it will be turned off because of no text in composer */
	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return FALSE;
	}
	g_free (text_content);

	value = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "type");

	/* citation == <blockquote type='cite'> */
	if (g_strstr_len (value, -1, "cite"))
		ret_val = TRUE;
	else
		ret_val = get_has_style (selection, "citation");

	g_free (value);
	return ret_val;
}

/**
 * e_html_editor_selection_is_indented:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current paragraph is indented. This does not include
 * citations.  To check, whether paragraph is a citation, use
 * e_html_editor_selection_is_citation().
 *
 * Returns: @TRUE when current paragraph is indented, @FALSE otherwise.
 */
gboolean
e_html_editor_selection_is_indented (EHTMLEditorSelection *selection)
{
	WebKitDOMRange *range;
	WebKitDOMNode *node;
	WebKitDOMElement *element;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_end_container (range, NULL);
	if (!WEBKIT_DOM_IS_ELEMENT (node))
		node = WEBKIT_DOM_NODE (webkit_dom_node_get_parent_element (node));

	element = webkit_dom_node_get_parent_element (node);

	return element_has_class (element, "-x-evo-indented");
}

static gboolean
is_in_html_mode (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view = e_html_editor_selection_ref_html_editor_view (selection);
	gboolean ret_val;

	g_return_val_if_fail (view != NULL, FALSE);

	ret_val = e_html_editor_view_get_html_mode (view);

	g_object_unref (view);

	return ret_val;
}

static gint
get_list_level (WebKitDOMNode *node)
{
	gint level = 0;

	while (node && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node)) {
		if (node_is_list (node))
			level++;
		node = webkit_dom_node_get_parent_node (node);
	}

	return level;
}

/**
 * e_html_editor_selection_indent:
 * @selection: an #EHTMLEditorSelection
 *
 * Indents current paragraph by one level.
 */
void
e_html_editor_selection_indent (EHTMLEditorSelection *selection)
{
	gboolean has_selection;
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	has_selection = g_strcmp0 (e_html_editor_selection_get_string (selection), "") != 0;

	if (!has_selection) {
		WebKitDOMDocument *document;
		WebKitDOMRange *range;
		WebKitDOMNode *node, *clone;
		WebKitDOMElement *element, *caret_position;
		gint word_wrap_length = selection->priv->word_wrap_length;
		gint level;
		gint final_width = 0;

		document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (view));

		caret_position = e_html_editor_selection_save_caret_position (selection);

		range = html_editor_selection_get_current_range (selection);
		if (!range) {
			g_object_unref (view);
			return;
		}

		node = webkit_dom_range_get_end_container (range, NULL);
		if (!WEBKIT_DOM_IS_ELEMENT (node))
			node = WEBKIT_DOM_NODE (
				webkit_dom_node_get_parent_element (node));

		if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (node)) {
			gboolean html_mode = e_html_editor_view_get_html_mode (view);
			WebKitDOMElement *list;
			WebKitDOMNode *source_list = webkit_dom_node_get_parent_node (node);
			EHTMLEditorSelectionBlockFormat format;

			format = e_html_editor_selection_get_list_format_from_node (source_list);

			list = create_list_element (
				selection, document, format, get_list_level (node), html_mode);

			element_add_class (list, "-x-evo-indented");
			webkit_dom_node_insert_before (
				source_list, WEBKIT_DOM_NODE (list), node, NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (list), node, NULL);
			if (!webkit_dom_node_contains (node, WEBKIT_DOM_NODE (caret_position))) {
				webkit_dom_node_append_child (
					node, WEBKIT_DOM_NODE (caret_position), NULL);
			}

			merge_lists_if_possible (WEBKIT_DOM_NODE (list));

			e_html_editor_selection_restore_caret_position (selection);

			g_object_unref (view);
			return;
		}

		level = get_indentation_level (WEBKIT_DOM_ELEMENT (node));

		final_width = word_wrap_length - SPACES_PER_INDENTATION * (level + 1);
		if (final_width < 10 && !is_in_html_mode (selection)) {
			e_html_editor_selection_restore_caret_position (selection);
			g_object_unref (view);
			return;
		}

		element = webkit_dom_node_get_parent_element (node);
		clone = webkit_dom_node_clone_node (node, TRUE);

		/* Remove style and let the paragraph inherit it from parent */
		if (element_has_class (WEBKIT_DOM_ELEMENT (clone), "-x-evo-paragraph"))
			webkit_dom_element_remove_attribute (
				WEBKIT_DOM_ELEMENT (clone), "style");

		element = e_html_editor_selection_get_indented_element (
			selection, document, final_width);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (element),
			clone,
			NULL);

		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (element),
			node,
			NULL);

		e_html_editor_selection_restore_caret_position (selection);
	} else {
		command = E_HTML_EDITOR_VIEW_COMMAND_INDENT;
		e_html_editor_selection_save (selection);
		e_html_editor_view_exec_command (view, command, NULL);
	}

	e_html_editor_view_force_spell_check_for_current_paragraph (view);

	if (has_selection)
		e_html_editor_selection_restore (selection);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "indented");
}

static const gchar *
get_css_alignment_value (EHTMLEditorSelectionAlignment alignment)
{
	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT)
		return ""; /* Left is by default on ltr */

	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER)
		return  "text-align: center;";

	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT)
		return "text-align: right;";

	return "";
}

static void
unindent_list (EHTMLEditorSelection *selection,
               WebKitDOMDocument *document)
{
	gboolean after = FALSE;
	WebKitDOMElement *new_list;
	WebKitDOMNode *source_list, *source_list_clone, *current_list, *item;
	WebKitDOMElement *selection_start_marker;
	WebKitDOMElement *selection_end_marker;

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return;

	/* Copy elements from previous block to list */
	item = webkit_dom_node_get_parent_node (
		WEBKIT_DOM_NODE (selection_start_marker));
	source_list = webkit_dom_node_get_parent_node (item);
	new_list = WEBKIT_DOM_ELEMENT (
		webkit_dom_node_clone_node (source_list, FALSE));
	current_list = source_list;
	source_list_clone = webkit_dom_node_clone_node (source_list, FALSE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (source_list),
		WEBKIT_DOM_NODE (source_list_clone),
		webkit_dom_node_get_next_sibling (source_list),
		NULL);

	if (element_has_class (WEBKIT_DOM_ELEMENT (source_list), "-x-evo-indented"))
		element_add_class (WEBKIT_DOM_ELEMENT (new_list), "-x-evo-indented");

	while (item) {
		WebKitDOMNode *next_item = webkit_dom_node_get_next_sibling (item);

		if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (item)) {
			if (after) {
				webkit_dom_node_append_child (
					source_list_clone, WEBKIT_DOM_NODE (item), NULL);
			} else {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (source_list),
					item,
					webkit_dom_node_get_next_sibling (source_list),
					NULL);
			}
		}

		if (webkit_dom_node_contains (item, WEBKIT_DOM_NODE (selection_end_marker)))
			after = TRUE;

		if (!next_item) {
			if (after)
				break;

			current_list = webkit_dom_node_get_next_sibling (current_list);
			next_item = webkit_dom_node_get_first_child (current_list);
		}
		item = next_item;
	}

	remove_node_if_empty (source_list_clone);
	remove_node_if_empty (source_list);
}
/**
 * e_html_editor_selection_unindent:
 * @selection: an #EHTMLEditorSelection
 *
 * Unindents current paragraph by one level.
 */
void
e_html_editor_selection_unindent (EHTMLEditorSelection *selection)
{
	gboolean has_selection;
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	has_selection = g_strcmp0 (e_html_editor_selection_get_string (selection), "") != 0;

	if (!has_selection) {
		EHTMLEditorSelectionAlignment alignment;
		gboolean before_node = TRUE, reinsert_caret_position = FALSE;
		const gchar *align_value;
		gint word_wrap_length = selection->priv->word_wrap_length;
		gint level, width;
		WebKitDOMDocument *document;
		WebKitDOMElement *element;
		WebKitDOMElement *prev_blockquote = NULL, *next_blockquote = NULL;
		WebKitDOMNode *node, *clone, *node_clone, *caret_node;
		WebKitDOMRange *range;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

		e_html_editor_selection_save_caret_position (selection);

		alignment = e_html_editor_selection_get_alignment (selection);
		align_value = get_css_alignment_value (alignment);

		range = html_editor_selection_get_current_range (selection);
		if (!range) {
			g_object_unref (view);
			return;
		}

		node = webkit_dom_range_get_end_container (range, NULL);
		if (!WEBKIT_DOM_IS_ELEMENT (node))
			node = WEBKIT_DOM_NODE (webkit_dom_node_get_parent_element (node));

		element = webkit_dom_node_get_parent_element (node);

		if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (node)) {
			e_html_editor_selection_save (selection);
			e_html_editor_selection_clear_caret_position_marker (selection);

			unindent_list (selection, document);
			e_html_editor_selection_restore (selection);
			g_object_unref (view);
			return;
		}

		if (!WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (element))
			return;

		element_add_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-to-unindent");

		level = get_indentation_level (element);
		width = word_wrap_length - SPACES_PER_INDENTATION * level;
		clone = WEBKIT_DOM_NODE (webkit_dom_node_clone_node (WEBKIT_DOM_NODE (element), TRUE));

		/* Look if we have previous siblings, if so, we have to
		 * create new blockquote that will include them */
		if (webkit_dom_node_get_previous_sibling (node))
			prev_blockquote = e_html_editor_selection_get_indented_element (
				selection, document, width);

		/* Look if we have next siblings, if so, we have to
		 * create new blockquote that will include them */
		if (webkit_dom_node_get_next_sibling (node))
			next_blockquote = e_html_editor_selection_get_indented_element (
				selection, document, width);

		/* Copy nodes that are before / after the element that we want to unindent */
		while (webkit_dom_node_has_child_nodes (clone)) {
			WebKitDOMNode *child;

			child = webkit_dom_node_get_first_child (clone);

			if (is_caret_position_node (child)) {
				reinsert_caret_position = TRUE;
				caret_node = webkit_dom_node_clone_node (child, TRUE);
				remove_node (child);
				continue;
			}

			if (webkit_dom_node_is_equal_node (child, node)) {
				before_node = FALSE;
				node_clone = webkit_dom_node_clone_node (child, TRUE);
				remove_node (child);
				continue;
			}

			webkit_dom_node_append_child (
				before_node ?
					WEBKIT_DOM_NODE (prev_blockquote) :
					WEBKIT_DOM_NODE (next_blockquote),
				child,
				NULL);

			remove_node (child);
		}

		element_remove_class (WEBKIT_DOM_ELEMENT (node_clone), "-x-evo-to-unindent");

		/* Insert blockqoute with nodes that were before the element that we want to unindent */
		if (prev_blockquote) {
			if (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (prev_blockquote))) {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
					WEBKIT_DOM_NODE (prev_blockquote),
					WEBKIT_DOM_NODE (element),
					NULL);
			}
		}

		/* Reinsert the caret position */
		if (reinsert_caret_position) {
			webkit_dom_node_insert_before (
				node_clone,
				caret_node,
				webkit_dom_node_get_first_child (node_clone),
				NULL);
		}

		if (level == 1 && element_has_class (WEBKIT_DOM_ELEMENT (node_clone), "-x-evo-paragraph"))
			e_html_editor_selection_set_paragraph_style (
				selection, WEBKIT_DOM_ELEMENT (node_clone), word_wrap_length, 0, align_value);

		/* Insert the unindented element */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			node_clone,
			WEBKIT_DOM_NODE (element),
			NULL);

		/* Insert blockqoute with nodes that were after the element that we want to unindent */
		if (next_blockquote) {
			if (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (prev_blockquote))) {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
					WEBKIT_DOM_NODE (next_blockquote),
					WEBKIT_DOM_NODE (element),
					NULL);
			}
		}

		/* Remove old blockquote */
		remove_node (WEBKIT_DOM_NODE (element));

		e_html_editor_selection_restore_caret_position (selection);
	} else {
		command = E_HTML_EDITOR_VIEW_COMMAND_OUTDENT;
		e_html_editor_selection_save (selection);
		e_html_editor_view_exec_command (view, command, NULL);
	}

	e_html_editor_view_force_spell_check_for_current_paragraph (view);

	if (has_selection)
		e_html_editor_selection_restore (selection);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "indented");
}

/**
 * e_html_editor_selection_is_bold:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is bold.
 *
 * Returns @TRUE when selection is bold, @FALSE otherwise.
 */
gboolean
e_html_editor_selection_is_bold (EHTMLEditorSelection *selection)
{
	gboolean ret_val;
	gchar *value, *text_content;
	EHTMLEditorView *view;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	if (!e_html_editor_view_get_html_mode (view)) {
		g_object_unref (view);
		return FALSE;
	}

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);
	window = webkit_dom_document_get_default_view (document);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set bold property,
	 * otherwise it will be turned off because of no text in composer */
	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return selection->priv->is_bold;
	}
	g_free (text_content);

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "font-weight");

	if (g_strstr_len (value, -1, "normal"))
		ret_val = FALSE;
	else
		ret_val = TRUE;

	g_free (value);
	return ret_val;
}

/**
 * e_html_editor_selection_set_bold:
 * @selection: an #EHTMLEditorSelection
 * @bold: @TRUE to enable bold, @FALSE to disable
 *
 * Toggles bold formatting of current selection or letter at current cursor
 * position, depending on whether @bold is @TRUE or @FALSE.
 */
void
e_html_editor_selection_set_bold (EHTMLEditorSelection *selection,
                                  gboolean bold)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_bold (selection) == bold)
		return;

	selection->priv->is_bold = bold;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_BOLD;
	e_html_editor_view_exec_command (view, command, NULL);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "bold");
}

/**
 * e_html_editor_selection_is_italic:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is italic.
 *
 * Returns @TRUE when selection is italic, @FALSE otherwise.
 */
gboolean
e_html_editor_selection_is_italic (EHTMLEditorSelection *selection)
{
	gboolean ret_val;
	gchar *value, *text_content;
	EHTMLEditorView *view;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	if (!e_html_editor_view_get_html_mode (view)) {
		g_object_unref (view);
		return FALSE;
	}

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);
	window = webkit_dom_document_get_default_view (document);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set italic property,
	 * otherwise it will be turned off because of no text in composer */
	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return selection->priv->is_italic;
	}
	g_free (text_content);

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "font-style");

	if (g_strstr_len (value, -1, "italic"))
		ret_val = TRUE;
	else
		ret_val = FALSE;

	g_free (value);
	return ret_val;
}

/**
 * e_html_editor_selection_set_italic:
 * @selection: an #EHTMLEditorSelection
 * @italic: @TRUE to enable italic, @FALSE to disable
 *
 * Toggles italic formatting of current selection or letter at current cursor
 * position, depending on whether @italic is @TRUE or @FALSE.
 */
void
e_html_editor_selection_set_italic (EHTMLEditorSelection *selection,
                                    gboolean italic)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_italic (selection) == italic)
		return;

	selection->priv->is_italic = italic;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_ITALIC;
	e_html_editor_view_exec_command (view, command, NULL);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "italic");
}

static gboolean
is_monospaced_element (WebKitDOMElement *element)
{
	gchar *value;
	gboolean ret_val = FALSE;

	if (!element)
		return FALSE;

	if (!WEBKIT_DOM_IS_HTML_FONT_ELEMENT (element))
		return FALSE;

	value = webkit_dom_element_get_attribute (element, "face");

	if (g_strcmp0 (value, "monospace") == 0)
		ret_val = TRUE;

	g_free (value);

	return ret_val;
}

/**
 * e_html_editor_selection_is_monospaced:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is monospaced.
 *
 * Returns @TRUE when selection is monospaced, @FALSE otherwise.
 */
gboolean
e_html_editor_selection_is_monospaced (EHTMLEditorSelection *selection)
{
	gboolean ret_val;
	gchar *value, *text_content;
	EHTMLEditorView *view;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	if (!e_html_editor_view_get_html_mode (view)) {
		g_object_unref (view);
		return FALSE;
	}

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);
	window = webkit_dom_document_get_default_view (document);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set italic property,
	 * otherwise it will be turned off because of no text in composer */
	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return selection->priv->is_monospaced;
	}
	g_free (text_content);

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "font-family");

	if (g_strstr_len (value, -1, "monospace"))
		ret_val = TRUE;
	else
		ret_val = FALSE;

	g_free (value);
	return ret_val;
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

/**
 * e_html_editor_selection_set_monospaced:
 * @selection: an #EHTMLEditorSelection
 * @monospaced: @TRUE to enable monospaced, @FALSE to disable
 *
 * Toggles monospaced formatting of current selection or letter at current cursor
 * position, depending on whether @monospaced is @TRUE or @FALSE.
 */
void
e_html_editor_selection_set_monospaced (EHTMLEditorSelection *selection,
                                        gboolean monospaced)
{
	EHTMLEditorView *view;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *window_selection;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_monospaced (selection) == monospaced)
		return;

	selection->priv->is_monospaced = monospaced;

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	web_view = WEBKIT_WEB_VIEW (view);

	document = webkit_web_view_get_dom_document (web_view);
	window = webkit_dom_document_get_default_view (document);
	window_selection = webkit_dom_dom_window_get_selection (window);

	if (monospaced) {
		gchar *font_size_str;
		guint font_size;
		WebKitDOMElement *monospace;

		monospace = webkit_dom_document_create_element (
			document, "font", NULL);
		webkit_dom_element_set_attribute (
			monospace, "face", "monospace", NULL);

		font_size = selection->priv->font_size;
		if (font_size == 0)
			font_size = E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;
		font_size_str = g_strdup_printf ("%d", font_size);
		webkit_dom_element_set_attribute (
			monospace, "size", font_size_str, NULL);
		g_free (font_size_str);

		if (g_strcmp0 (e_html_editor_selection_get_string (selection), "") != 0) {
			gchar *html, *outer_html;

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (monospace),
				WEBKIT_DOM_NODE (
					webkit_dom_range_clone_contents (range, NULL)),
				NULL);

			outer_html = webkit_dom_html_element_get_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (monospace));

			html = g_strconcat (
				/* Mark selection for restoration */
				"<span id=\"-x-evo-selection-start-marker\"></span>",
				outer_html,
				"<span id=\"-x-evo-selection-end-marker\"></span>",
				NULL),

			e_html_editor_selection_insert_html (selection, html);

			e_html_editor_selection_restore (selection);

			g_free (html);
			g_free (outer_html);
		} else {
			/* https://bugs.webkit.org/show_bug.cgi?id=15256 */
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (monospace),
				UNICODE_ZERO_WIDTH_SPACE,
				NULL);
			webkit_dom_range_insert_node (
				range, WEBKIT_DOM_NODE (monospace), NULL);

			move_caret_into_element (document, monospace);
		}
	} else {
		gboolean is_bold, is_italic, is_underline, is_strikethrough;
		guint font_size;
		WebKitDOMElement *tt_element;
		WebKitDOMNode *node;

		node = webkit_dom_range_get_end_container (range, NULL);
		if (WEBKIT_DOM_IS_ELEMENT (node) &&
		    is_monospaced_element (WEBKIT_DOM_ELEMENT (node))) {
			tt_element = WEBKIT_DOM_ELEMENT (node);
		} else {
			tt_element = e_html_editor_dom_node_find_parent_element (node, "FONT");

			if (!is_monospaced_element (tt_element)) {
				g_object_unref (view);
				return;
			}
		}

		/* Save current formatting */
		is_bold = selection->priv->is_bold;
		is_italic = selection->priv->is_italic;
		is_underline = selection->priv->is_underline;
		is_strikethrough = selection->priv->is_strikethrough;
		font_size = selection->priv->font_size;
		if (font_size == 0)
			font_size = E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;

		if (g_strcmp0 (e_html_editor_selection_get_string (selection), "") != 0) {
			gchar *html, *outer_html, *inner_html, *beginning, *end;
			gchar *start_position, *end_position, *font_size_str;
			WebKitDOMElement *wrapper;

			wrapper = webkit_dom_document_create_element (
				document, "SPAN", NULL);
			webkit_dom_element_set_id (wrapper, "-x-evo-remove-tt");
			webkit_dom_range_surround_contents (
				range, WEBKIT_DOM_NODE (wrapper), NULL);

			html = webkit_dom_html_element_get_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (tt_element));

			start_position = g_strstr_len (
				html, -1, "<span id=\"-x-evo-remove-tt\"");
			end_position = g_strstr_len (start_position, -1, "</span>");

			beginning = g_utf8_substring (
				html, 0, g_utf8_pointer_to_offset (html, start_position));
			inner_html = webkit_dom_html_element_get_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (wrapper));
			end = g_utf8_substring (
				html,
				g_utf8_pointer_to_offset (html, end_position) + 7,
				g_utf8_strlen (html, -1)),

			font_size_str = g_strdup_printf ("%d", font_size);

			outer_html =
				g_strconcat (
					/* Beginning */
					beginning,
					/* End the previous FONT tag */
					"</font>",
					/* Mark selection for restoration */
					"<span id=\"-x-evo-selection-start-marker\"></span>",
					/* Inside will be the same */
					inner_html,
					"<span id=\"-x-evo-selection-end-marker\"></span>",
					/* Start the new FONT element */
					"<font face=\"monospace\" size=\"",
					font_size_str,
					"\">",
					/* End - we have to start after </span> */
					end,
					NULL),

			g_free (font_size_str);

			webkit_dom_html_element_set_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (tt_element),
				outer_html,
				NULL);

			e_html_editor_selection_restore (selection);

			g_free (html);
			g_free (outer_html);
			g_free (inner_html);
			g_free (beginning);
			g_free (end);
		} else {
			WebKitDOMRange *new_range;
			gchar *outer_html;
			gchar *tmp;

			webkit_dom_element_set_id (tt_element, "ev-tt");

		        outer_html = webkit_dom_html_element_get_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (tt_element));
			tmp = g_strconcat (outer_html, UNICODE_ZERO_WIDTH_SPACE, NULL);
			webkit_dom_html_element_set_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (tt_element),
				tmp, NULL);

			/* We need to get that element again */
			tt_element = webkit_dom_document_get_element_by_id (
				document, "ev-tt");
			webkit_dom_element_remove_attribute (
				WEBKIT_DOM_ELEMENT (tt_element), "id");

			new_range = webkit_dom_document_create_range (document);
			webkit_dom_range_set_start_after (
				new_range, WEBKIT_DOM_NODE (tt_element), NULL);
			webkit_dom_range_set_end_after (
				new_range, WEBKIT_DOM_NODE (tt_element), NULL);

			webkit_dom_dom_selection_remove_all_ranges (
				window_selection);
			webkit_dom_dom_selection_add_range (
				window_selection, new_range);

			webkit_dom_dom_selection_modify (
				window_selection, "move", "right", "character");

			g_free (outer_html);
			g_free (tmp);

			e_html_editor_view_force_spell_check_for_current_paragraph (
				view);
		}

		/* Re-set formatting */
		if (is_bold)
			e_html_editor_selection_set_bold (selection, TRUE);
		if (is_italic)
			e_html_editor_selection_set_italic (selection, TRUE);
		if (is_underline)
			e_html_editor_selection_set_underline (selection, TRUE);
		if (is_strikethrough)
			e_html_editor_selection_set_strikethrough (selection, TRUE);

		e_html_editor_selection_set_font_size (selection, font_size);
	}

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "monospaced");
}

/**
 * e_html_editor_selection_is_strikethrough:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is striked through.
 *
 * Returns @TRUE when selection is striked through, @FALSE otherwise.
 */
gboolean
e_html_editor_selection_is_strikethrough (EHTMLEditorSelection *selection)
{
	gboolean ret_val;
	gchar *value, *text_content;
	EHTMLEditorView *view;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	if (!e_html_editor_view_get_html_mode (view)) {
		g_object_unref (view);
		return FALSE;
	}

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);
	window = webkit_dom_document_get_default_view (document);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set strikethrough property,
	 * otherwise it will be turned off because of no text in composer */
	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return selection->priv->is_strikethrough;
	}
	g_free (text_content);

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-decoration");

	if (g_strstr_len (value, -1, "line-through"))
		ret_val = TRUE;
	else
		ret_val = get_has_style (selection, "strike");

	g_free (value);
	return ret_val;
}

/**
 * e_html_editor_selection_set_strikethrough:
 * @selection: an #EHTMLEditorSelection
 * @strikethrough: @TRUE to enable strikethrough, @FALSE to disable
 *
 * Toggles strike through formatting of current selection or letter at current
 * cursor position, depending on whether @strikethrough is @TRUE or @FALSE.
 */
void
e_html_editor_selection_set_strikethrough (EHTMLEditorSelection *selection,
                                           gboolean strikethrough)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_strikethrough (selection) == strikethrough)
		return;

	selection->priv->is_strikethrough = strikethrough;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH;
	e_html_editor_view_exec_command (view, command, NULL);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "strikethrough");
}

/**
 * e_html_editor_selection_is_subscript:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is in subscript.
 *
 * Returns @TRUE when selection is in subscript, @FALSE otherwise.
 */
gboolean
e_html_editor_selection_is_subscript (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	if (!e_html_editor_view_get_html_mode (view)) {
		g_object_unref (view);
		return FALSE;
	}

	g_object_unref (view);

	range = html_editor_selection_get_current_range (selection);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);

	while (node) {
		gchar *tag_name;

		tag_name = webkit_dom_element_get_tag_name (WEBKIT_DOM_ELEMENT (node));

		if (g_ascii_strncasecmp (tag_name, "sub", 3) == 0) {
			g_free (tag_name);
			break;
		}

		g_free (tag_name);
		node = webkit_dom_node_get_parent_node (node);
	}

	return (node != NULL);
}

/**
 * e_html_editor_selection_set_subscript:
 * @selection: an #EHTMLEditorSelection
 * @subscript: @TRUE to enable subscript, @FALSE to disable
 *
 * Toggles subscript of current selection or letter at current cursor position,
 * depending on whether @subscript is @TRUE or @FALSE.
 */
void
e_html_editor_selection_set_subscript (EHTMLEditorSelection *selection,
                                       gboolean subscript)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_subscript (selection) == subscript)
		return;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_SUBSCRIPT;
	e_html_editor_view_exec_command (view, command, NULL);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "subscript");
}

/**
 * e_html_editor_selection_is_superscript:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is in superscript.
 *
 * Returns @TRUE when selection is in superscript, @FALSE otherwise.
 */
gboolean
e_html_editor_selection_is_superscript (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	if (!e_html_editor_view_get_html_mode (view)) {
		g_object_unref (view);
		return FALSE;
	}

	g_object_unref (view);

	range = html_editor_selection_get_current_range (selection);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);

	while (node) {
		gchar *tag_name;

		tag_name = webkit_dom_element_get_tag_name (WEBKIT_DOM_ELEMENT (node));

		if (g_ascii_strncasecmp (tag_name, "sup", 3) == 0) {
			g_free (tag_name);
			break;
		}

		g_free (tag_name);
		node = webkit_dom_node_get_parent_node (node);
	}

	return (node != NULL);
}

/**
 * e_html_editor_selection_set_superscript:
 * @selection: an #EHTMLEditorSelection
 * @superscript: @TRUE to enable superscript, @FALSE to disable
 *
 * Toggles superscript of current selection or letter at current cursor position,
 * depending on whether @superscript is @TRUE or @FALSE.
 */
void
e_html_editor_selection_set_superscript (EHTMLEditorSelection *selection,
                                         gboolean superscript)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_superscript (selection) == superscript)
		return;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_SUPERSCRIPT;
	e_html_editor_view_exec_command (view, command, NULL);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "superscript");
}

/**
 * e_html_editor_selection_is_underline:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is underlined.
 *
 * Returns @TRUE when selection is underlined, @FALSE otherwise.
 */
gboolean
e_html_editor_selection_is_underline (EHTMLEditorSelection *selection)
{
	gboolean ret_val;
	gchar *value, *text_content;
	EHTMLEditorView *view;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	if (!e_html_editor_view_get_html_mode (view)) {
		g_object_unref (view);
		return FALSE;
	}

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);
	window = webkit_dom_document_get_default_view (document);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set underline property,
	 * otherwise it will be turned off because of no text in composer */
	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return selection->priv->is_underline;
	}
	g_free (text_content);

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_dom_window_get_computed_style (window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-decoration");

	if (g_strstr_len (value, -1, "underline"))
		ret_val = TRUE;
	else
		ret_val = get_has_style (selection, "u");

	g_free (value);
	return ret_val;
}

/**
 * e_html_editor_selection_set_underline:
 * @selection: an #EHTMLEditorSelection
 * @underline: @TRUE to enable underline, @FALSE to disable
 *
 * Toggles underline formatting of current selection or letter at current
 * cursor position, depending on whether @underline is @TRUE or @FALSE.
 */
void
e_html_editor_selection_set_underline (EHTMLEditorSelection *selection,
                                       gboolean underline)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_underline (selection) == underline)
		return;

	selection->priv->is_underline = underline;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE;
	e_html_editor_view_exec_command (view, command, NULL);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "underline");
}

/**
 * e_html_editor_selection_unlink:
 * @selection: an #EHTMLEditorSelection
 *
 * Removes any links (&lt;A&gt; elements) from current selection or at current
 * cursor position.
 */
void
e_html_editor_selection_unlink (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *link;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	link = e_html_editor_dom_node_find_parent_element (
			webkit_dom_range_get_start_container (range, NULL), "A");

	if (!link) {
		gchar *text;
		/* get element that was clicked on */
		link = e_html_editor_view_get_element_under_mouse_click (view);
		if (!WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (link))
			link = NULL;

		text = webkit_dom_html_element_get_inner_text (
				WEBKIT_DOM_HTML_ELEMENT (link));
		webkit_dom_html_element_set_outer_html (WEBKIT_DOM_HTML_ELEMENT (link), text, NULL);
		g_free (text);
	} else {
		command = E_HTML_EDITOR_VIEW_COMMAND_UNLINK;
		e_html_editor_view_exec_command (view, command, NULL);
	}
	g_object_unref (view);
}

/**
 * e_html_editor_selection_create_link:
 * @selection: an #EHTMLEditorSelection
 * @uri: destination of the new link
 *
 * Converts current selection into a link pointing to @url.
 */
void
e_html_editor_selection_create_link (EHTMLEditorSelection *selection,
                                     const gchar *uri)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (uri != NULL && *uri != '\0');

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_CREATE_LINK;
	e_html_editor_view_exec_command (view, command, uri);

	g_object_unref (view);
}

/**
 * e_html_editor_selection_insert_text:
 * @selection: an #EHTMLEditorSelection
 * @plain_text: text to insert
 *
 * Inserts @plain_text at current cursor position. When a text range is selected,
 * it will be replaced by @plain_text.
 */
void
e_html_editor_selection_insert_text (EHTMLEditorSelection *selection,
                                     const gchar *plain_text)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (plain_text != NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT;
	e_html_editor_view_exec_command (view, command, plain_text);

	g_object_unref (view);
}

/**
 * e_html_editor_selection_insert_html:
 * @selection: an #EHTMLEditorSelection
 * @html_text: an HTML code to insert
 *
 * Insert @html_text into document at current cursor position. When a text range
 * is selected, it will be replaced by @html_text.
 */
void
e_html_editor_selection_insert_html (EHTMLEditorSelection *selection,
                                     const gchar *html_text)
{
	EHTMLEditorView *view;
	EHTMLEditorViewCommand command;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (html_text != NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	command = E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML;
	if (e_html_editor_view_get_html_mode (view)) {
		e_html_editor_view_exec_command (view, command, html_text);
	} else {
		e_html_editor_view_convert_and_insert_html_to_plain_text (
			view, html_text);
	}

	g_object_unref (view);
}


/************************* image_load_and_insert_async() *************************/

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	EHTMLEditorSelection *selection;
	WebKitDOMElement *element;
	GInputStream *input_stream;
	GOutputStream *output_stream;
	GFile *file;
	GFileInfo *file_info;
	goffset total_num_bytes;
	gssize bytes_read;
	const gchar *content_type;
	const gchar *filename;
	gchar buffer[4096];
};

/* Forward Declaration */
static void
image_load_stream_read_cb (GInputStream *input_stream,
                           GAsyncResult *result,
                           LoadContext *load_context);

static LoadContext *
image_load_context_new (EHTMLEditorSelection *selection)
{
	LoadContext *load_context;

	load_context = g_slice_new0 (LoadContext);
	load_context->selection = selection;

	return load_context;
}

static void
image_load_context_free (LoadContext *load_context)
{
	if (load_context->input_stream != NULL)
		g_object_unref (load_context->input_stream);

	if (load_context->output_stream != NULL)
		g_object_unref (load_context->output_stream);

	if (load_context->file_info != NULL)
		g_object_unref (load_context->file_info);

	if (load_context->file != NULL)
		g_object_unref (load_context->file);

	g_slice_free (LoadContext, load_context);
}

static void
replace_base64_image_src (EHTMLEditorSelection *selection,
                          WebKitDOMElement *element,
                          const gchar *base64_content,
                          const gchar *filename,
                          const gchar *uri)
{
	EHTMLEditorView *view;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	e_html_editor_view_set_changed (view, TRUE);
	g_object_unref (view);

	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (element),
		base64_content);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-uri", uri, NULL);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-inline", "", NULL);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-name",
		filename ? filename : "", NULL);
}

static void
insert_base64_image (EHTMLEditorSelection *selection,
                     const gchar *base64_content,
                     const gchar *filename,
                     const gchar *uri)
{
	EHTMLEditorView *view;
	WebKitDOMDocument *document;
	WebKitDOMElement *element, *caret_position, *resizable_wrapper;
	WebKitDOMText *text;

	caret_position = e_html_editor_selection_save_caret_position (selection);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (
		WEBKIT_WEB_VIEW (view));

	e_html_editor_view_set_changed (view, TRUE);
	g_object_unref (view);

	resizable_wrapper =
		webkit_dom_document_create_element (document, "span", NULL);
	webkit_dom_element_set_attribute (
		resizable_wrapper, "class", "-x-evo-resizable-wrapper", NULL);

	element = webkit_dom_document_create_element (document, "img", NULL);
	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (element),
		base64_content);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-uri", uri, NULL);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-inline", "", NULL);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-name",
		filename ? filename : "", NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (resizable_wrapper),
		WEBKIT_DOM_NODE (element),
		NULL);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (caret_position)),
		WEBKIT_DOM_NODE (resizable_wrapper),
		WEBKIT_DOM_NODE (caret_position),
		NULL);

	/* We have to again use UNICODE_ZERO_WIDTH_SPACE character to restore
	 * caret on right position */
	text = webkit_dom_document_create_text_node (
		document, UNICODE_ZERO_WIDTH_SPACE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (caret_position)),
		WEBKIT_DOM_NODE (text),
		WEBKIT_DOM_NODE (caret_position),
		NULL);

	e_html_editor_selection_restore_caret_position (selection);
}

static void
image_load_finish (LoadContext *load_context)
{
	EHTMLEditorSelection *selection;
	GMemoryOutputStream *output_stream;
	gchar *base64_encoded, *mime_type, *output, *uri;
	gsize size;
	gpointer data;

	output_stream = G_MEMORY_OUTPUT_STREAM (load_context->output_stream);

	selection = load_context->selection;

	mime_type = g_content_type_get_mime_type (load_context->content_type);

	data = g_memory_output_stream_get_data (output_stream);
	size = g_memory_output_stream_get_data_size (output_stream);
	uri = g_file_get_uri (load_context->file);

	base64_encoded = g_base64_encode ((const guchar *) data, size);
	output = g_strconcat ("data:", mime_type, ";base64,", base64_encoded, NULL);
	if (load_context->element)
		replace_base64_image_src (
			selection, load_context->element, output, load_context->filename, uri);
	else
		insert_base64_image (selection, output, load_context->filename, uri);

	g_free (base64_encoded);
	g_free (output);
	g_free (mime_type);
	g_free (uri);

	image_load_context_free (load_context);
}

static void
image_load_write_cb (GOutputStream *output_stream,
                     GAsyncResult *result,
                     LoadContext *load_context)
{
	GInputStream *input_stream;
	gssize bytes_written;
	GError *error = NULL;

	bytes_written = g_output_stream_write_finish (
		output_stream, result, &error);

	if (error) {
		image_load_context_free (load_context);
		return;
	}

	input_stream = load_context->input_stream;

	if (bytes_written < load_context->bytes_read) {
		g_memmove (
			load_context->buffer,
			load_context->buffer + bytes_written,
			load_context->bytes_read - bytes_written);
		load_context->bytes_read -= bytes_written;

		g_output_stream_write_async (
			output_stream,
			load_context->buffer,
			load_context->bytes_read,
			G_PRIORITY_DEFAULT, NULL,
			(GAsyncReadyCallback) image_load_write_cb,
			load_context);
	} else
		g_input_stream_read_async (
			input_stream,
			load_context->buffer,
			sizeof (load_context->buffer),
			G_PRIORITY_DEFAULT, NULL,
			(GAsyncReadyCallback) image_load_stream_read_cb,
			load_context);
}

static void
image_load_stream_read_cb (GInputStream *input_stream,
                           GAsyncResult *result,
                           LoadContext *load_context)
{
	GOutputStream *output_stream;
	gssize bytes_read;
	GError *error = NULL;

	bytes_read = g_input_stream_read_finish (
		input_stream, result, &error);

	if (error) {
		image_load_context_free (load_context);
		return;
	}

	if (bytes_read == 0) {
		image_load_finish (load_context);
		return;
	}

	output_stream = load_context->output_stream;
	load_context->bytes_read = bytes_read;

	g_output_stream_write_async (
		output_stream,
		load_context->buffer,
		load_context->bytes_read,
		G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) image_load_write_cb,
		load_context);
}

static void
image_load_file_read_cb (GFile *file,
                         GAsyncResult *result,
                         LoadContext *load_context)
{
	GFileInputStream *input_stream;
	GOutputStream *output_stream;
	GError *error = NULL;

	/* Input stream might be NULL, so don't use cast macro. */
	input_stream = g_file_read_finish (file, result, &error);
	load_context->input_stream = (GInputStream *) input_stream;

	if (error) {
		image_load_context_free (load_context);
		return;
	}

	/* Load the contents into a GMemoryOutputStream. */
	output_stream = g_memory_output_stream_new (
		NULL, 0, g_realloc, g_free);

	load_context->output_stream = output_stream;

	g_input_stream_read_async (
		load_context->input_stream,
		load_context->buffer,
		sizeof (load_context->buffer),
		G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) image_load_stream_read_cb,
		load_context);
}

static void
image_load_query_info_cb (GFile *file,
                          GAsyncResult *result,
                          LoadContext *load_context)
{
	GFileInfo *file_info;
	GError *error = NULL;

	file_info = g_file_query_info_finish (file, result, &error);
	if (error) {
		image_load_context_free (load_context);
		return;
	}

	load_context->content_type = g_file_info_get_content_type (file_info);
	load_context->total_num_bytes = g_file_info_get_size (file_info);
	load_context->filename = g_file_info_get_name (file_info);

	g_file_read_async (
		file, G_PRIORITY_DEFAULT,
		NULL, (GAsyncReadyCallback)
		image_load_file_read_cb, load_context);
}

static void
image_load_and_insert_async (EHTMLEditorSelection *selection,
                             WebKitDOMElement *element,
                             const gchar *uri)
{
	LoadContext *load_context;
	GFile *file;

	g_return_if_fail (uri && *uri);

	file = g_file_new_for_uri (uri);
	g_return_if_fail (file != NULL);

	load_context = image_load_context_new (selection);
	load_context->file = file;
	load_context->element = element;

	g_file_query_info_async (
		file, "standard::*",
		G_FILE_QUERY_INFO_NONE,G_PRIORITY_DEFAULT,
		NULL, (GAsyncReadyCallback)
		image_load_query_info_cb, load_context);
}

/**
 * e_html_editor_selection_insert_image:
 * @selection: an #EHTMLEditorSelection
 * @image_uri: an URI of the source image
 *
 * Inserts image at current cursor position using @image_uri as source. When a
 * text range is selected, it will be replaced by the image.
 */
void
e_html_editor_selection_insert_image (EHTMLEditorSelection *selection,
                                      const gchar *image_uri)
{
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (image_uri != NULL);

	if (is_in_html_mode (selection)) {
		if (strstr (image_uri, ";base64,")) {
			if (g_str_has_prefix (image_uri, "data:"))
				insert_base64_image (selection, image_uri, "", "");
			if (strstr (image_uri, ";data")) {
				const gchar *base64_data = strstr (image_uri, ";") + 1;
				gchar *filename;
				glong filename_length;

				filename_length =
					g_utf8_strlen (image_uri, -1) -
					g_utf8_strlen (base64_data, -1) - 1;
				filename = g_strndup (image_uri, filename_length);

				insert_base64_image (selection, base64_data, filename, "");
				g_free (filename);
			}
		} else
			image_load_and_insert_async (selection, NULL, image_uri);
	}
}

/**
 * e_html_editor_selection_replace_image_src:
 * @selection: an #EHTMLEditorSelection
 * @image: #WebKitDOMElement representation of image
 * @image_uri: an URI of the source image
 *
 * Replace the src attribute of the given @image with @image_uri.
 */
void
e_html_editor_selection_replace_image_src (EHTMLEditorSelection *selection,
                                           WebKitDOMElement *image,
                                           const gchar *image_uri)
{
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (image_uri != NULL);
	g_return_if_fail (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (image));

	if (strstr (image_uri, ";base64,")) {
		if (g_str_has_prefix (image_uri, "data:"))
			replace_base64_image_src (
				selection, image, image_uri, "", "");
		if (strstr (image_uri, ";data")) {
			const gchar *base64_data = strstr (image_uri, ";") + 1;
			gchar *filename;
			glong filename_length;

			filename_length =
				g_utf8_strlen (image_uri, -1) -
				g_utf8_strlen (base64_data, -1) - 1;
			filename = g_strndup (image_uri, filename_length);

			replace_base64_image_src (
				selection, image, base64_data, filename, "");
			g_free (filename);
		}
	} else
		image_load_and_insert_async (selection, image, image_uri);
}

/**
 * e_html_editor_selection_clear_caret_position_marker:
 * @selection: an #EHTMLEditorSelection
 *
 * Removes previously set caret position marker from composer.
 */
void
e_html_editor_selection_clear_caret_position_marker (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-caret-position");

	if (element)
		remove_node (WEBKIT_DOM_NODE (element));

	g_object_unref (view);
}

WebKitDOMNode *
e_html_editor_selection_get_caret_position_node (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;

	element	= webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_id (element, "-x-evo-caret-position");
	webkit_dom_element_set_attribute (
		element, "style", "color: red", NULL);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element), "*", NULL);

	return WEBKIT_DOM_NODE (element);
}

/**
 * e_html_editor_selection_save_caret_position:
 * @selection: an #EHTMLEditorSelection
 *
 * Saves current caret position in composer.
 *
 * Returns: #WebKitDOMElement that was created on caret position
 */
WebKitDOMElement *
e_html_editor_selection_save_caret_position (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;
	WebKitDOMDocument *document;
	WebKitDOMNode *split_node;
	WebKitDOMNode *start_offset_node;
	WebKitDOMNode *caret_node;
	WebKitDOMRange *range;
	gulong start_offset;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);

	e_html_editor_selection_clear_caret_position_marker (selection);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return NULL;

	start_offset = webkit_dom_range_get_start_offset (range, NULL);
	start_offset_node = webkit_dom_range_get_end_container (range, NULL);

	caret_node = e_html_editor_selection_get_caret_position_node (document);

	if (WEBKIT_DOM_IS_TEXT (start_offset_node) && start_offset != 0) {
		WebKitDOMText *split_text;

		split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (start_offset_node),
				start_offset, NULL);
		split_node = WEBKIT_DOM_NODE (split_text);
	} else {
		split_node = start_offset_node;
	}

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (start_offset_node),
		caret_node,
		split_node,
		NULL);

	return WEBKIT_DOM_ELEMENT (caret_node);
}

static void
fix_quoting_nodes_after_caret_restoration (WebKitDOMDOMSelection *window_selection,
                                           WebKitDOMNode *prev_sibling,
                                           WebKitDOMNode *next_sibling)
{
	WebKitDOMNode *tmp_node;

	if (!element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-temp-text-wrapper"))
		return;

	webkit_dom_dom_selection_modify (
		window_selection, "move", "forward", "character");
	tmp_node = webkit_dom_node_get_next_sibling (
		webkit_dom_node_get_first_child (prev_sibling));

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (prev_sibling),
		tmp_node,
		next_sibling,
		NULL);

	tmp_node = webkit_dom_node_get_first_child (prev_sibling);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (prev_sibling),
		tmp_node,
		webkit_dom_node_get_previous_sibling (next_sibling),
		NULL);

	remove_node (prev_sibling);

	webkit_dom_dom_selection_modify (
		window_selection, "move", "backward", "character");
}

/**
 * e_html_editor_selection_restore_caret_position:
 * @selection: an #EHTMLEditorSelection
 *
 * Restores previously saved caret position in composer.
 */
void
e_html_editor_selection_restore_caret_position (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	gboolean fix_after_quoting;
	gboolean swap_direction = FALSE;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-caret-position");
	fix_after_quoting = element_has_class (element, "-x-evo-caret-quoting");

	if (element) {
		WebKitDOMDOMWindow *window;
		WebKitDOMNode *parent_node;
		WebKitDOMDOMSelection *window_selection;
		WebKitDOMNode *prev_sibling;
		WebKitDOMNode *next_sibling;

		if (!webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element)))
			swap_direction = TRUE;

		window = webkit_dom_document_get_default_view (document);
		window_selection = webkit_dom_dom_window_get_selection (window);
		parent_node = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
		/* If parent is BODY element, we try to restore the position on the 
		 * element that is next to us */
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent_node)) {
			/* Look if we have DIV on right */
			next_sibling = webkit_dom_node_get_next_sibling (
				WEBKIT_DOM_NODE (element));
			if (!WEBKIT_DOM_IS_ELEMENT (next_sibling)) {
				e_html_editor_selection_clear_caret_position_marker (selection);
				return;
			}

			if (element_has_class (WEBKIT_DOM_ELEMENT (next_sibling), "-x-evo-paragraph")) {
				remove_node (WEBKIT_DOM_NODE (element));

				move_caret_into_element (
					document, WEBKIT_DOM_ELEMENT (next_sibling));

				goto out;
			}
		}

		move_caret_into_element (document, element);

		if (fix_after_quoting) {
			prev_sibling = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (element));
			next_sibling = webkit_dom_node_get_next_sibling (
				WEBKIT_DOM_NODE (element));
			if (!next_sibling)
				fix_after_quoting = FALSE;
		}

		remove_node (WEBKIT_DOM_NODE (element));

		if (fix_after_quoting)
			fix_quoting_nodes_after_caret_restoration (
				window_selection, prev_sibling, next_sibling);
 out:
		/* FIXME If caret position is restored and afterwards the
		 * position is saved it is not on the place where it supposed
		 * to be (it is in the beginning of parent's element. It can
		 * be avoided by moving with the caret. */
		if (swap_direction) {
			webkit_dom_dom_selection_modify (
				window_selection, "move", "forward", "character");
			webkit_dom_dom_selection_modify (
				window_selection, "move", "backward", "character");
		} else {
			webkit_dom_dom_selection_modify (
				window_selection, "move", "backward", "character");
			webkit_dom_dom_selection_modify (
				window_selection, "move", "forward", "character");
		}
	}
}

static gint
find_where_to_break_line (WebKitDOMNode *node,
                          gint max_len,
			  gint word_wrap_length)
{
	gchar *str, *text_start;
	gunichar uc;
	gint pos;
	gint last_space = 0;
	gint length;
	gint ret_val = 0;
	gchar* position;

	text_start =  webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (node));
	length = g_utf8_strlen (text_start, -1);

	pos = 1;
	last_space = 0;
	str = text_start;
	do {
		uc = g_utf8_get_char (str);
		if (!uc) {
			g_free (text_start);
			if (pos <= max_len)
				return pos;
			else
				return last_space;
		}

		/* If last_space is zero then the word is longer than
		 * word_wrap_length characters, so continue until we find
		 * a space */
		if ((pos > max_len) && (last_space > 0)) {
			if (last_space > word_wrap_length) {
				g_free (text_start);
				return last_space;
			}

			if (last_space > max_len) {
				if (g_unichar_isspace (g_utf8_get_char (text_start)))
					ret_val = 1;

				g_free (text_start);
				return ret_val;
			}

			if (last_space == max_len - 1) {
				uc = g_utf8_get_char (str);
				if (g_unichar_isspace (uc))
					last_space++;
			}

			g_free (text_start);
			return last_space;
		}

		if (g_unichar_isspace (uc))
			last_space = pos;

		pos += 1;
		str = g_utf8_next_char (str);
	} while (*str);

	position = g_utf8_offset_to_pointer (text_start, max_len);

	if (g_unichar_isspace (g_utf8_get_char (position))) {
		ret_val = max_len + 1;
	} else {
		if (last_space < max_len) {
			ret_val = last_space;
		} else {
			if (length > word_wrap_length)
				ret_val = last_space;
			else
				ret_val = 0;
		}
	}

	g_free (text_start);

	return ret_val;
}

static WebKitDOMElement *
wrap_lines (EHTMLEditorSelection *selection,
	    WebKitDOMNode *paragraph,
	    WebKitDOMDocument *document,
	    gboolean remove_all_br,
	    gint word_wrap_length)
{
	WebKitDOMNode *node, *start_node;
	WebKitDOMNode *paragraph_clone;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMElement *element;
	WebKitDOMNodeList *wrap_br;
	gint len, ii, br_count;
	gulong length_left;
	glong paragraph_char_count;
	gchar *text_content;

	if (selection) {
		paragraph_char_count = g_utf8_strlen (
			e_html_editor_selection_get_string (selection), -1);

		fragment = webkit_dom_range_clone_contents (
			html_editor_selection_get_current_range (selection), NULL);

		/* Select all BR elements or just ours that are used for wrapping.
		 * We are not removing user BR elements when this function is activated
		 * from Format->Wrap Lines action */
		wrap_br = webkit_dom_document_fragment_query_selector_all (
			fragment,
			remove_all_br ? "br" : "br.-x-evo-wrap-br",
			NULL);
	} else {
		WebKitDOMElement *caret_node;

		if (!webkit_dom_node_has_child_nodes (paragraph))
			return WEBKIT_DOM_ELEMENT (paragraph);

		paragraph_clone = webkit_dom_node_clone_node (paragraph, TRUE);
		caret_node = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (paragraph_clone),
			"span#-x-evo-caret-position", NULL);
		text_content = webkit_dom_node_get_text_content (paragraph_clone);
		paragraph_char_count = g_utf8_strlen (text_content, -1);
		if (caret_node)
			paragraph_char_count--;
		g_free (text_content);

		wrap_br = webkit_dom_element_query_selector_all (
			WEBKIT_DOM_ELEMENT (paragraph_clone),
			remove_all_br ? "br" : "br.-x-evo-wrap-br",
			NULL);
	}

	/* And remove them */
	br_count = webkit_dom_node_list_get_length (wrap_br);
	for (ii = 0; ii < br_count; ii++)
		remove_node (webkit_dom_node_list_item (wrap_br, ii));

	if (selection)
		node = WEBKIT_DOM_NODE (fragment);
	else {
		webkit_dom_node_normalize (paragraph_clone);
		node = webkit_dom_node_get_first_child (paragraph_clone);
		if (node) {
			text_content = webkit_dom_node_get_text_content (node);
			if (g_strcmp0 ("\n", text_content) == 0)
				node = webkit_dom_node_get_next_sibling (node);
			g_free (text_content);
		}
	}

	start_node = node;
	len = 0;
	while (node) {
		gint offset = 0;

		if (WEBKIT_DOM_IS_TEXT (node)) {
			const gchar *newline;
			WebKitDOMNode *next_sibling;

			/* If there is temporary hidden space we remove it */
			text_content = webkit_dom_node_get_text_content (node);
			if (strstr (text_content, UNICODE_ZERO_WIDTH_SPACE)) {
				if (g_str_has_prefix (text_content, UNICODE_ZERO_WIDTH_SPACE))
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (node),
						0,
						1,
						NULL);
				if (g_str_has_suffix (text_content, UNICODE_ZERO_WIDTH_SPACE))
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (node),
						g_utf8_strlen (text_content, -1) - 1,
						1,
						NULL);
				g_free (text_content);
				text_content = webkit_dom_node_get_text_content (node);
			}
			newline = g_strstr_len (text_content, -1, "\n");

			next_sibling = node;
			while (newline) {
				WebKitDOMElement *element;

				next_sibling = WEBKIT_DOM_NODE (webkit_dom_text_split_text (
					WEBKIT_DOM_TEXT (next_sibling),
					g_utf8_pointer_to_offset (text_content, newline),
					NULL));

				if (!next_sibling)
					break;

				element = webkit_dom_document_create_element (
					document, "BR", NULL);
				element_add_class (element, "-x-evo-temp-wrap-text-br");

				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (next_sibling),
					WEBKIT_DOM_NODE (element),
					next_sibling,
					NULL);

				g_free (text_content);

				text_content = webkit_dom_node_get_text_content (next_sibling);
				if (g_str_has_prefix (text_content, "\n")) {
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (next_sibling), 0, 1, NULL);
					g_free (text_content);
					text_content =
						webkit_dom_node_get_text_content (next_sibling);
				}
				newline = g_strstr_len (text_content, -1, "\n");
			}
			g_free (text_content);
		} else {
			/* If element is ANCHOR we wrap it separately */
			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
				glong anchor_length;

				text_content = webkit_dom_node_get_text_content (node);
				anchor_length = g_utf8_strlen (text_content, -1);
				if (len + anchor_length > word_wrap_length) {
					element = webkit_dom_document_create_element (
						document, "BR", NULL);
					element_add_class (element, "-x-evo-wrap-br");
					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						node,
						NULL);
					len = anchor_length;
				} else
					len += anchor_length;

				g_free (text_content);
				node = webkit_dom_node_get_next_sibling (node);
				continue;
			}

			if (is_caret_position_node (node)) {
				node = webkit_dom_node_get_next_sibling (node);
				continue;
			}

			/* When we are not removing user-entered BR elements (lines wrapped by user),
			 * we need to skip those elements */
			if (!remove_all_br && WEBKIT_DOM_IS_HTMLBR_ELEMENT (node)) {
				if (!element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br")) {
					len = 0;
					node = webkit_dom_node_get_next_sibling (node);
					continue;
				}
			}
			goto next_node;
		}

		/* If length of this node + what we already have is still less
		 * then word_wrap_length characters, then just join it and continue to next
		 * node */
		length_left = webkit_dom_character_data_get_length (
			WEBKIT_DOM_CHARACTER_DATA (node));

		if ((length_left + len) < word_wrap_length) {
			len += length_left;
			goto next_node;
		}

		/* wrap until we have something */
		while ((length_left + len) > word_wrap_length) {
			/* Find where we can line-break the node so that it
			 * effectively fills the rest of current row */
			offset = find_where_to_break_line (
				node, word_wrap_length - len, word_wrap_length);

			element = webkit_dom_document_create_element (document, "BR", NULL);
			element_add_class (element, "-x-evo-wrap-br");

			if (offset > 0 && offset <= word_wrap_length) {
				if (offset != length_left)
					webkit_dom_text_split_text (
						WEBKIT_DOM_TEXT (node), offset, NULL);

				if (webkit_dom_node_get_next_sibling (node)) {
					gchar *nd_content;
					WebKitDOMNode *nd = webkit_dom_node_get_next_sibling (node);

					nd = webkit_dom_node_get_next_sibling (node);
					nd_content = webkit_dom_node_get_text_content (nd);
					if (nd_content && *nd_content) {
						if (g_str_has_prefix (nd_content, " "))
							webkit_dom_character_data_replace_data (
								WEBKIT_DOM_CHARACTER_DATA (nd), 0, 1, "", NULL);
						g_free (nd_content);
						nd_content = webkit_dom_node_get_text_content (nd);
						if (g_strcmp0 (nd_content, UNICODE_NBSP) == 0)
							remove_node (nd);
						g_free (nd_content);
					}

					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						nd,
						NULL);
				} else {
					webkit_dom_node_append_child (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						NULL);
				}
			} else if (offset > word_wrap_length) {
				if (offset != length_left)
					webkit_dom_text_split_text (
						WEBKIT_DOM_TEXT (node), offset + 1, NULL);

				if (webkit_dom_node_get_next_sibling (node)) {
					gchar *nd_content;
					WebKitDOMNode *nd = webkit_dom_node_get_next_sibling (node);

					nd = webkit_dom_node_get_next_sibling (node);
					nd_content = webkit_dom_node_get_text_content (nd);
					if (nd_content && *nd_content) {
						if (g_str_has_prefix (nd_content, " "))
							webkit_dom_character_data_replace_data (
								WEBKIT_DOM_CHARACTER_DATA (nd), 0, 1, "", NULL);
						g_free (nd_content);
						nd_content = webkit_dom_node_get_text_content (nd);
						if (g_strcmp0 (nd_content, UNICODE_NBSP) == 0)
							remove_node (nd);
						g_free (nd_content);
					}

					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						nd,
						NULL);
				} else {
					webkit_dom_node_append_child (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						NULL);
				}
				len = 0;
				break;
			} else {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (node),
					WEBKIT_DOM_NODE (element),
					node,
					NULL);
			}
			length_left = webkit_dom_character_data_get_length (
				WEBKIT_DOM_CHARACTER_DATA (node));

			len = 0;
		}
		len += length_left - offset;
 next_node:
		if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (node))
			len = 0;

		/* Move to next node */
		if (webkit_dom_node_has_child_nodes (node)) {
			node = webkit_dom_node_get_first_child (node);
		} else if (webkit_dom_node_get_next_sibling (node)) {
			node = webkit_dom_node_get_next_sibling (node);
		} else {
			if (webkit_dom_node_is_equal_node (node, start_node))
				break;

			node = webkit_dom_node_get_parent_node (node);
			if (node)
				node = webkit_dom_node_get_next_sibling (node);
		}
	}

	if (selection) {
		gchar *html;

		/* Create a wrapper DIV and put the processed content into it */
		element = webkit_dom_document_create_element (document, "DIV", NULL);
		element_add_class (element, "-x-evo-paragraph");
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (element),
			WEBKIT_DOM_NODE (start_node),
			NULL);

		webkit_dom_node_normalize (WEBKIT_DOM_NODE (element));
		/* Get HTML code of the processed content */
		html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (element));

		/* Overwrite the current selection be the processed content */
		e_html_editor_selection_insert_html (selection, html);

		g_free (html);

		return NULL;
	} else {
		webkit_dom_node_normalize (paragraph_clone);

		/* Replace paragraph with wrapped one */
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (paragraph),
			paragraph_clone,
			paragraph,
			NULL);

		return WEBKIT_DOM_ELEMENT (paragraph_clone);
	}
}

void
e_html_editor_selection_set_indented_style (EHTMLEditorSelection *selection,
                                            WebKitDOMElement *element,
                                            gint width)
{
	EHTMLEditorSelectionAlignment alignment;
	gchar *style;
	const gchar *align_value;
	gint word_wrap_length = (width == -1) ? selection->priv->word_wrap_length : width;
	gint start = 0, end = 0;

	alignment = e_html_editor_selection_get_alignment (selection);
	align_value = get_css_alignment_value (alignment);

	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT)
		start = SPACES_PER_INDENTATION;

	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER) {
		start = SPACES_PER_INDENTATION;
	}

	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT) {
		start = 0;
		end = SPACES_PER_INDENTATION;
	}

	webkit_dom_element_set_class_name (element, "-x-evo-indented");
	/* We don't want vertical space bellow and above blockquote inserted by
	 * WebKit's User Agent Stylesheet. We have to override it through style attribute. */
	if (is_in_html_mode (selection))
		style = g_strdup_printf (
			"-webkit-margin-start: %dch; -webkit-margin-end : %dch; %s",
			start, end, align_value);
	else
		style = g_strdup_printf (
			"-webkit-margin-start: %dch; -webkit-margin-end : %dch; "
			"word-wrap: normal; width: %dch; %s",
			start, end, word_wrap_length, align_value);

	webkit_dom_element_set_attribute (element, "style", style, NULL);
	g_free (style);
}

WebKitDOMElement *
e_html_editor_selection_get_indented_element (EHTMLEditorSelection *selection,
                                              WebKitDOMDocument *document,
                                              gint width)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "BLOCKQUOTE", NULL);
	e_html_editor_selection_set_indented_style (selection, element, width);

	return element;
}

void
e_html_editor_selection_set_paragraph_style (EHTMLEditorSelection *selection,
                                             WebKitDOMElement *element,
                                             gint width,
                                             gint offset,
                                             const gchar *style_to_add)
{
	EHTMLEditorSelectionAlignment alignment;
	const gchar *align_value = NULL;
	char *style = NULL;
	gint word_wrap_length = (width == -1) ? selection->priv->word_wrap_length : width;

	alignment = e_html_editor_selection_get_alignment (selection);
	align_value = get_css_alignment_value (alignment);

	element_add_class (element, "-x-evo-paragraph");
	if (!is_in_html_mode (selection)) {
		style = g_strdup_printf (
			"width: %dch; word-wrap: normal; %s %s",
			(word_wrap_length + offset), align_value, style_to_add);
	} else {
		if (*align_value || *style_to_add)
			style = g_strdup_printf (
				"%s %s", align_value, style_to_add);
	}
	if (style) {
		webkit_dom_element_set_attribute (element, "style", style, NULL);
		g_free (style);
	}
}

WebKitDOMElement *
e_html_editor_selection_get_paragraph_element (EHTMLEditorSelection *selection,
                                               WebKitDOMDocument *document,
                                               gint width,
                                               gint offset)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "DIV", NULL);
	e_html_editor_selection_set_paragraph_style (selection, element, width, offset, "");

	return element;
}

WebKitDOMElement *
e_html_editor_selection_put_node_into_paragraph (EHTMLEditorSelection *selection,
                                                 WebKitDOMDocument *document,
                                                 WebKitDOMNode *node,
                                                 WebKitDOMNode *caret_position)
{
	WebKitDOMRange *range;
	WebKitDOMElement *container;

	range = webkit_dom_document_create_range (document);
	container = e_html_editor_selection_get_paragraph_element (selection, document, -1, 0);
	webkit_dom_range_select_node (range, node, NULL);
	webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (container), NULL);
	/* We have to move caret position inside this container */
	webkit_dom_node_append_child (WEBKIT_DOM_NODE (container), caret_position, NULL);

	return container;
}

/**
 * e_html_editor_selection_wrap_lines:
 * @selection: an #EHTMLEditorSelection
 *
 * Wraps all lines in current selection to be 71 characters long.
 */
void
e_html_editor_selection_wrap_lines (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;
	WebKitDOMRange *range;
	WebKitDOMDocument *document;
	WebKitDOMElement *active_paragraph, *caret;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);

	caret = e_html_editor_selection_save_caret_position (selection);
	if (g_strcmp0 (e_html_editor_selection_get_string (selection), "") == 0) {
		WebKitDOMNode *end_container;
		WebKitDOMNode *parent;
		WebKitDOMNode *paragraph;
		gchar *text_content;

		/* We need to save caret position and restore it after
		 * wrapping the selection, but we need to save it before we
		 * start to modify selection */
		range = html_editor_selection_get_current_range (selection);
		if (!range)
			return;

		end_container = webkit_dom_range_get_common_ancestor_container (range, NULL);

		/* Wrap only text surrounded in DIV and P tags */
		parent = webkit_dom_node_get_parent_node(end_container);
		if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (parent) ||
		    WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT (parent)) {
			element_add_class (
				WEBKIT_DOM_ELEMENT (parent), "-x-evo-paragraph");
			paragraph = parent;
		} else {
			WebKitDOMElement *parent_div =
				e_html_editor_dom_node_find_parent_element (parent, "DIV");

			if (element_has_class (parent_div, "-x-evo-paragraph")) {
				paragraph = WEBKIT_DOM_NODE (parent_div);
			} else {
				if (!caret)
					return;

				/* We try to select previous sibling */
				paragraph = webkit_dom_node_get_previous_sibling (
					WEBKIT_DOM_NODE (caret));
				if (paragraph) {
					/* When there is just text without container
					 * we have to surround it with paragraph div */
					if (WEBKIT_DOM_IS_TEXT (paragraph))
						paragraph = WEBKIT_DOM_NODE (
							e_html_editor_selection_put_node_into_paragraph (
								selection, document, paragraph,
								WEBKIT_DOM_NODE (caret)));
				} else {
					/* When some weird element is selected, return */
					e_html_editor_selection_clear_caret_position_marker (selection);
					return;
				}
			}
		}

		if (!paragraph)
			return;

		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (paragraph), "style");
		webkit_dom_element_set_id (
			WEBKIT_DOM_ELEMENT (paragraph), "-x-evo-active-paragraph");

		text_content = webkit_dom_node_get_text_content (paragraph);
		/* If there is hidden space character in the beginning we remove it */
		if (strstr (text_content, UNICODE_ZERO_WIDTH_SPACE)) {
			if (g_str_has_prefix (text_content, UNICODE_ZERO_WIDTH_SPACE)) {
				WebKitDOMNode *node;

				node = webkit_dom_node_get_first_child (paragraph);

				webkit_dom_character_data_delete_data (
					WEBKIT_DOM_CHARACTER_DATA (node),
					0,
					1,
					NULL);
			}
			if (g_str_has_suffix (text_content, UNICODE_ZERO_WIDTH_SPACE)) {
				WebKitDOMNode *node;

				node = webkit_dom_node_get_last_child (paragraph);

				webkit_dom_character_data_delete_data (
					WEBKIT_DOM_CHARACTER_DATA (node),
					g_utf8_strlen (text_content, -1) -1,
					1,
					NULL);
			}
		}
		g_free (text_content);

		wrap_lines (
			NULL, paragraph, document, FALSE,
			selection->priv->word_wrap_length);

	} else {
		e_html_editor_selection_save_caret_position (selection);
		/* If we have selection -> wrap it */
		wrap_lines (
			selection, NULL, document, FALSE,
			selection->priv->word_wrap_length);
	}

	active_paragraph = webkit_dom_document_get_element_by_id (
		document, "-x-evo-active-paragraph");
	/* We have to move caret on position where it was before modifying the text */
	e_html_editor_selection_restore_caret_position (selection);

	/* Set paragraph as non-active */
	if (active_paragraph)
		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (active_paragraph), "id");
}

WebKitDOMElement *
e_html_editor_selection_wrap_paragraph_length (EHTMLEditorSelection *selection,
                                               WebKitDOMElement *paragraph,
                                               gint length)
{
	WebKitDOMDocument *document;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);
	g_return_val_if_fail (WEBKIT_DOM_IS_ELEMENT (paragraph), NULL);
	g_return_val_if_fail (length > 10, NULL);

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (paragraph));

	return wrap_lines (
		NULL, WEBKIT_DOM_NODE (paragraph), document, FALSE, length);
}

void
e_html_editor_selection_wrap_paragraphs_in_document (EHTMLEditorSelection *selection,
                                                     WebKitDOMDocument *document)
{
	WebKitDOMNodeList *list;
	gint ii, length;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	list = webkit_dom_document_query_selector_all (
		document, "div.-x-evo-paragraph:not(#-x-evo-input-start)", NULL);

	length = webkit_dom_node_list_get_length (list);

	for (ii = 0; ii < length; ii++) {
		gint quote, citation_level;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		citation_level = get_citation_level (node);
		quote = citation_level ? citation_level + 1 : 0;

		if (node_is_list (node)) {
			WebKitDOMNode *item = webkit_dom_node_get_first_child (node);

			while (item && WEBKIT_DOM_IS_HTMLLI_ELEMENT (item)) {
				e_html_editor_selection_wrap_paragraph_length (
					selection,
					WEBKIT_DOM_ELEMENT (item),
					selection->priv->word_wrap_length - quote);
				item = webkit_dom_node_get_next_sibling (item);
			}
		} else {
			e_html_editor_selection_wrap_paragraph_length (
				selection,
				WEBKIT_DOM_ELEMENT (node),
				selection->priv->word_wrap_length - quote);
		}
	}
}

WebKitDOMElement *
e_html_editor_selection_wrap_paragraph (EHTMLEditorSelection *selection,
                                        WebKitDOMElement *paragraph)
{
	gint indentation_level, citation_level, quote;
	gint final_width, word_wrap_length, offset = 0;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);
	g_return_val_if_fail (WEBKIT_DOM_IS_ELEMENT (paragraph), NULL);

	word_wrap_length = selection->priv->word_wrap_length;
	indentation_level = get_indentation_level (paragraph);
	citation_level = get_citation_level (WEBKIT_DOM_NODE (paragraph));

	if (node_is_list (WEBKIT_DOM_NODE (paragraph)) ||
	    WEBKIT_DOM_IS_HTMLLI_ELEMENT (paragraph)) {

		gint list_level = get_list_level (WEBKIT_DOM_NODE (paragraph));
		indentation_level = 0;

		if (list_level > 0)
			offset = list_level * -SPACES_PER_LIST_LEVEL;
		else
			offset = -SPACES_PER_LIST_LEVEL;
	}

	quote = citation_level ? citation_level + 1 : 0;
	quote *= 2;

	final_width = word_wrap_length - quote + offset;
	final_width -= SPACES_PER_INDENTATION * indentation_level;

	return e_html_editor_selection_wrap_paragraph_length (
		selection, WEBKIT_DOM_ELEMENT (paragraph), final_width);
}

/**
 * e_html_editor_selection_save:
 * @selection: an #EHTMLEditorSelection
 *
 * Saves current cursor position or current selection range. The selection can
 * be later restored by calling e_html_editor_selection_restore().
 *
 * Note that calling e_html_editor_selection_save() overwrites previously saved
 * position.
 *
 * Note that this method inserts special markings into the HTML code that are
 * used to later restore the selection. It can happen that by deleting some
 * segments of the document some of the markings are deleted too. In that case
 * restoring the selection by e_html_editor_selection_restore() can fail. Also by
 * moving text segments (Cut & Paste) can result in moving the markings
 * elsewhere, thus e_html_editor_selection_restore() will restore the selection
 * incorrectly.
 *
 * It is recommended to use this method only when you are not planning to make
 * bigger changes to content or structure of the document (formatting changes
 * are usually OK).
 */
void
e_html_editor_selection_save (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	WebKitDOMNode *container;
	WebKitDOMElement *marker;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	web_view = WEBKIT_WEB_VIEW (view);

	document = webkit_web_view_get_dom_document (web_view);

	/* First remove all markers (if present) */
	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (marker != NULL)
		remove_node (WEBKIT_DOM_NODE (marker));

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");
	if (marker != NULL)
		remove_node (WEBKIT_DOM_NODE (marker));

	range = html_editor_selection_get_current_range (selection);

	if (range != NULL) {
		WebKitDOMNode *marker_node;
		WebKitDOMNode *parent_node;
		WebKitDOMNode *split_node;
		glong offset;

		marker = webkit_dom_document_create_element (
			document, "SPAN", NULL);
		webkit_dom_element_set_id (
			marker, "-x-evo-selection-start-marker");

		container = webkit_dom_range_get_start_container (range, NULL);
		offset = webkit_dom_range_get_start_offset (range, NULL);

		if (WEBKIT_DOM_IS_TEXT (container)) {
			WebKitDOMText *split_text;

			split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (container), offset, NULL);
			split_node = WEBKIT_DOM_NODE (split_text);
		} else if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (container)) {
			webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (marker),
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (container)),
				NULL);

			goto end_marker;
		} else {
			if (!webkit_dom_node_get_previous_sibling (container)) {
				split_node = webkit_dom_node_get_parent_node (
					container);
			} else if (!webkit_dom_node_get_next_sibling (container)) {
				split_node = webkit_dom_node_get_parent_node (
					container);
				split_node = webkit_dom_node_get_next_sibling (
					split_node);
			} else
				split_node = container;
		}

		if (!split_node) {
			webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (marker),
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (container)),
				NULL);
		} else {
			marker_node = WEBKIT_DOM_NODE (marker);
			parent_node = webkit_dom_node_get_parent_node (split_node);

			webkit_dom_node_insert_before (
				parent_node, marker_node, split_node, NULL);
		}
 end_marker:
		marker = webkit_dom_document_create_element (
			document, "SPAN", NULL);
		webkit_dom_element_set_id (
			marker, "-x-evo-selection-end-marker");

		container = webkit_dom_range_get_end_container (range, NULL);
		offset = webkit_dom_range_get_end_offset (range, NULL);

		if (WEBKIT_DOM_IS_TEXT (container) && offset != 0) {
			WebKitDOMText *split_text;

			split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (container), offset, NULL);
			split_node = WEBKIT_DOM_NODE (split_text);
		} else if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (container)) {
			webkit_dom_node_append_child (
				container, WEBKIT_DOM_NODE (marker), NULL);
			g_object_unref (view);
			return;
		} else {
			if (!webkit_dom_node_get_previous_sibling (container)) {
				split_node = webkit_dom_node_get_parent_node (
					container);
			} else if (!webkit_dom_node_get_next_sibling (container)) {
				split_node = webkit_dom_node_get_parent_node (
					container);
				split_node = webkit_dom_node_get_next_sibling (
					split_node);
			} else
				split_node = container;
		}

		marker_node = WEBKIT_DOM_NODE (marker);

		if (split_node) {
			parent_node = webkit_dom_node_get_parent_node (split_node);

			webkit_dom_node_insert_before (
				parent_node, marker_node, split_node, NULL);
		} else {
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (container),
				marker_node,
				NULL);
		}
	}

	g_object_unref (view);
}

/**
 * e_html_editor_selection_restore:
 * @selection: an #EHTMLEditorSelection
 *
 * Restores cursor position or selection range that was saved by
 * e_html_editor_selection_save().
 *
 * Note that calling this function without calling e_html_editor_selection_save()
 * before is a programming error and the behavior is undefined.
 */
void
e_html_editor_selection_restore (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *marker;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	web_view = WEBKIT_WEB_VIEW (view);

	document = webkit_web_view_get_dom_document (web_view);
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);
	range = webkit_dom_document_create_range (document);

	if (range != NULL) {
		marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (!marker) {
			marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");
			if (marker)
				remove_node (WEBKIT_DOM_NODE (marker));
			return;
		}

		webkit_dom_range_set_start_after (
			range, WEBKIT_DOM_NODE (marker), NULL);
		remove_node (WEBKIT_DOM_NODE (marker));

		marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		if (!marker) {
			marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			if (marker)
				remove_node (WEBKIT_DOM_NODE (marker));
			return;
		}

		webkit_dom_range_set_end_before (
			range, WEBKIT_DOM_NODE (marker), NULL);
		remove_node (WEBKIT_DOM_NODE (marker));

		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
	}

	g_object_unref (view);
}

static void
html_editor_selection_modify (EHTMLEditorSelection *selection,
                              const gchar *alter,
                              gboolean forward,
                              EHTMLEditorSelectionGranularity granularity)
{
	EHTMLEditorView *view;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	const gchar *granularity_str;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	web_view = WEBKIT_WEB_VIEW (view);

	document = webkit_web_view_get_dom_document (web_view);
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	switch (granularity) {
		case E_HTML_EDITOR_SELECTION_GRANULARITY_CHARACTER:
			granularity_str = "character";
			break;
		case E_HTML_EDITOR_SELECTION_GRANULARITY_WORD:
			granularity_str = "word";
			break;
	}

	webkit_dom_dom_selection_modify (
		dom_selection, alter,
		forward ? "forward" : "backward",
		granularity_str);

	g_object_unref (view);
}

/**
 * e_html_editor_selection_extend:
 * @selection: an #EHTMLEditorSelection
 * @forward: whether to extend selection forward or backward
 * @granularity: granularity of the extension
 *
 * Extends current selection in given direction by given granularity.
 */
void
e_html_editor_selection_extend (EHTMLEditorSelection *selection,
                                gboolean forward,
                                EHTMLEditorSelectionGranularity granularity)
{
	html_editor_selection_modify (selection, "extend", forward, granularity);
}

/**
 * e_html_editor_selection_move:
 * @selection: an #EHTMLEditorSelection
 * @forward: whether to move the selection forward or backward
 * @granularity: granularity of the movement
 *
 * Moves current selection in given direction by given granularity
 */
void
e_html_editor_selection_move (EHTMLEditorSelection *selection,
                              gboolean forward,
                              EHTMLEditorSelectionGranularity granularity)
{
	html_editor_selection_modify (selection, "move", forward, granularity);
}

void
e_html_editor_selection_scroll_to_caret (EHTMLEditorSelection *selection)
{
	WebKitDOMElement *caret;

	caret = e_html_editor_selection_save_caret_position (selection);

	webkit_dom_element_scroll_into_view (caret, TRUE);

	e_html_editor_selection_clear_caret_position_marker (selection);
}
