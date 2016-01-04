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
	gboolean selection_changed_callbacks_blocked;

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

static const GdkRGBA black = { 0, 0, 0, 1 };

G_DEFINE_TYPE (
	EHTMLEditorSelection,
	e_html_editor_selection,
	G_TYPE_OBJECT
);

static WebKitDOMRange *
html_editor_selection_get_current_range (EHTMLEditorSelection *selection)
{
	EHTMLEditorView *view;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range = NULL;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);
	dom_window = webkit_dom_document_get_default_view (document);
	if (!dom_window)
		return NULL;

	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	if (!dom_selection) {
		g_object_unref (dom_window);
		return NULL;
	}

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1)
		goto exit;

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
 exit:
	g_object_unref (dom_selection);
	g_object_unref (dom_window);

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
	g_object_unref (range);

	tag_len = strlen (style_tag);
	result = FALSE;
	while (!result && element) {
		gchar *element_tag;
		gboolean accept_citation = FALSE;

		element_tag = webkit_dom_element_get_tag_name (element);

		if (g_ascii_strncasecmp (style_tag, "citation", 8) == 0) {
			accept_citation = TRUE;
			result = WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (element);
			if (element_has_class (element, "-x-evo-indented"))
				result = FALSE;
		} else {
			result = ((tag_len == strlen (element_tag)) &&
				(g_ascii_strncasecmp (element_tag, style_tag, tag_len) == 0));
		}

		/* Special case: <blockquote type=cite> marks quotation, while
		 * just <blockquote> is used for indentation. If the <blockquote>
		 * has type=cite, then ignore it unless style_tag is "citation" */
		if (result && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (element)) {
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
	g_object_unref (range);
	element = e_html_editor_dom_node_find_parent_element (node, "FONT");
	while (element && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (element) &&
	       !webkit_dom_element_has_attribute (element, font_property)) {
		element = e_html_editor_dom_node_find_parent_element (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)), "FONT");
	}

	if (!element)
		return NULL;

	g_object_get (G_OBJECT (element), font_property, &value, NULL);

	return value;
}

static void
html_editor_selection_selection_changed_cb (WebKitWebView *web_view,
                                            EHTMLEditorSelection *selection)
{
	WebKitDOMRange *range = NULL;

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return;
	g_object_unref (range);

	g_object_freeze_notify (G_OBJECT (selection));

	g_object_notify (G_OBJECT (selection), "alignment");
	g_object_notify (G_OBJECT (selection), "block-format");
	g_object_notify (G_OBJECT (selection), "indented");
	g_object_notify (G_OBJECT (selection), "text");

	if (!e_html_editor_view_get_html_mode (E_HTML_EDITOR_VIEW (web_view)))
		goto out;

	g_object_notify (G_OBJECT (selection), "background-color");
	g_object_notify (G_OBJECT (selection), "bold");
	g_object_notify (G_OBJECT (selection), "font-name");
	g_object_notify (G_OBJECT (selection), "font-size");
	g_object_notify (G_OBJECT (selection), "font-color");
	g_object_notify (G_OBJECT (selection), "italic");
	g_object_notify (G_OBJECT (selection), "monospaced");
	g_object_notify (G_OBJECT (selection), "strikethrough");
	g_object_notify (G_OBJECT (selection), "subscript");
	g_object_notify (G_OBJECT (selection), "superscript");
	g_object_notify (G_OBJECT (selection), "underline");

 out:
	g_object_thaw_notify (G_OBJECT (selection));
}

void
e_html_editor_selection_block_selection_changed (EHTMLEditorSelection *selection)
{
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (!selection->priv->selection_changed_callbacks_blocked) {
		EHTMLEditorView *view;

		view = e_html_editor_selection_ref_html_editor_view (selection);
		g_signal_handlers_block_by_func (
			view, html_editor_selection_selection_changed_cb, selection);
		g_object_unref (view);
		selection->priv->selection_changed_callbacks_blocked = TRUE;
	}
}

void
e_html_editor_selection_unblock_selection_changed (EHTMLEditorSelection *selection)
{
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (selection->priv->selection_changed_callbacks_blocked) {
		EHTMLEditorView *view;

		view = e_html_editor_selection_ref_html_editor_view (selection);
		g_signal_handlers_unblock_by_func (
			view, html_editor_selection_selection_changed_cb, selection);
		g_object_unref (view);
		selection->priv->selection_changed_callbacks_blocked = FALSE;

		html_editor_selection_selection_changed_cb (WEBKIT_WEB_VIEW (view), selection);
	}
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
	 * EHTMLEditorSelection:alignment
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
	 * EHTMLEditorSelection:background-color
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
	 * EHTMLEditorSelection:block-format
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
	 * EHTMLEditorSelection:bold
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
	 * EHTMLEditorSelection:font-color
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
	 * EHTMLEditorSelection:font-name
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
	 * EHTMLEditorSelection:font-size
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
	 * EHTMLEditorSelection:indented
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
	 * EHTMLEditorSelection:italic
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
	 * EHTMLEditorSelection:monospaced
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
	 * EHTMLEditorSelection:strikethrough
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
	 * EHTMLEditorSelection:superscript
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
	 * EHTMLEditorSelection:subscript
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
	 * EHTMLEditorSelection:text
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
	 * EHTMLEditorSelection:underline
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

	g_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	selection->priv->word_wrap_length =
		g_settings_get_int (g_settings, "composer-word-wrap-length");
	g_object_unref (g_settings);

	selection->priv->selection_changed_callbacks_blocked = FALSE;
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
	if (WEBKIT_DOM_IS_TEXT (node)) {
		g_object_unref (range);
		return TRUE;
	}

	node = webkit_dom_range_get_end_container (range, NULL);
	if (WEBKIT_DOM_IS_TEXT (node)) {
		g_object_unref (range);
		return TRUE;
	}

	node = WEBKIT_DOM_NODE (webkit_dom_range_clone_contents (range, NULL));
	while (node) {
		if (WEBKIT_DOM_IS_TEXT (node)) {
			g_object_unref (range);
			return TRUE;
		}

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

	if (node)
		g_object_unref (node);

	g_object_unref (range);

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
	gchar *word;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);

	range = html_editor_selection_get_current_range (selection);

	/* Don't operate on the visible selection */
	range = webkit_dom_range_clone_range (range, NULL);
	webkit_dom_range_expand (range, "word", NULL);
	word = webkit_dom_range_to_string (range, NULL);

	g_object_unref (range);

	return word;
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
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (replacement != NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	web_view = WEBKIT_WEB_VIEW (view);

	range = html_editor_selection_get_current_range (selection);
	document = webkit_web_view_get_dom_document (web_view);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	webkit_dom_range_expand (range, "word", NULL);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	fragment = webkit_dom_range_extract_contents (range, NULL);

	/* Get the text node to replace and leave other formatting nodes
	 * untouched (font color, boldness, ...). */
	webkit_dom_node_normalize (WEBKIT_DOM_NODE (fragment));
	node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
	if (!WEBKIT_DOM_IS_TEXT (node)) {
		while (node && WEBKIT_DOM_IS_ELEMENT (node))
			node = webkit_dom_node_get_first_child (node);
	}

	if (node && WEBKIT_DOM_IS_TEXT (node)) {
		WebKitDOMText *text;

		/* Replace the word */
		text = webkit_dom_document_create_text_node (document, replacement);
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (text),
			node,
			NULL);

		/* Insert the word on current location. */
		webkit_dom_range_insert_node (range, WEBKIT_DOM_NODE (fragment), NULL);

		webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);
	}

	e_html_editor_view_force_spell_check_for_current_paragraph (view);

	g_object_unref (range);
	g_object_unref (dom_selection);
	g_object_unref (dom_window);
	g_object_unref (view);
}

/**
 * e_html_editor_selection_is_collapsed:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns if selection is collapsed.
 *
 * Returns: Whether the selection is collapsed (just caret) or not (someting is selected).
 */
gboolean
e_html_editor_selection_is_collapsed (EHTMLEditorSelection *selection)
{
	gboolean collapsed;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), TRUE);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return TRUE;

	collapsed = webkit_dom_range_get_collapsed (range, NULL);
	g_object_unref (range);

	return collapsed;
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

	g_object_unref (range);

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
	EHTMLEditorViewHistoryEvent *ev = NULL;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		WebKitDOMDocument *document;
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;
		WebKitDOMRange *range;

		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_REPLACE;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);

		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		ev->data.string.from = webkit_dom_range_get_text (range);
		ev->data.string.to = g_strdup (new_string);
		g_object_unref (dom_selection);
		g_object_unref (dom_window);
		g_object_unref (range);
	}

	e_html_editor_view_exec_command (
		view, E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, new_string);

	if (ev) {
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}

	e_html_editor_view_force_spell_check_for_current_paragraph (view);

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
	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-align-center"))
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER;
	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-align-right"))
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT;
	else
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
}

static EHTMLEditorSelectionAlignment
e_html_editor_selection_get_alignment_from_node (WebKitDOMNode *node)
{
	EHTMLEditorSelectionAlignment alignment;
	gchar *value;
	WebKitDOMCSSStyleDeclaration *style;

	style = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (node));
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

	g_object_unref (style);

	g_free (value);

	return alignment;
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
	gchar *value;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_val_if_fail (
		E_IS_HTML_EDITOR_SELECTION (selection),
		E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT);

	range = html_editor_selection_get_current_range (selection);
	if (!range) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
		goto out;
	}

	node = webkit_dom_range_get_start_container (range, NULL);
	g_object_unref (range);
	if (!node) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
		goto out;
	}

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	if (element_has_class (element, "-x-evo-align-right")) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT;
		goto out;
	} else if (element_has_class (element, "-x-evo-align-center")) {
		alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER;
		goto out;
	}

	style = webkit_dom_element_get_style (element);
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

	g_object_unref (style);
	g_free (value);

 out:
	selection->priv->alignment = alignment;

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
	gboolean inserting_unordered_list;
	WebKitDOMElement *list;

	inserting_unordered_list = format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;

	list = webkit_dom_document_create_element (
		document, inserting_unordered_list  ? "UL" : "OL", NULL);

	if (!inserting_unordered_list)
		set_ordered_list_type_to_element (list, format);

	if (level >= 0 && !html_mode) {
		gint offset;

		offset = (level + 1) * SPACES_PER_LIST_LEVEL;

		offset += !inserting_unordered_list ?
			SPACES_ORDERED_LIST_FIRST_LEVEL - SPACES_PER_LIST_LEVEL: 0;

		e_html_editor_selection_set_paragraph_style (
			selection, list, -1, -offset, "");

		if (inserting_unordered_list)
			webkit_dom_element_set_attribute (list, "data-evo-plain-text", "", NULL);
	}

	return list;
}

static WebKitDOMNode *
get_list_item_node_from_child (WebKitDOMNode *child)
{
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (child);

	while (parent && !WEBKIT_DOM_IS_HTMLLI_ELEMENT (parent))
		parent = webkit_dom_node_get_parent_node (parent);

	return parent;
}

static WebKitDOMNode *
get_list_node_from_child (WebKitDOMNode *child)
{
	WebKitDOMNode *parent = get_list_item_node_from_child (child);

	return webkit_dom_node_get_parent_node (parent);
}

static WebKitDOMElement *
do_format_change_list_to_list (WebKitDOMElement *list_to_process,
                               WebKitDOMElement *new_list_template,
                               EHTMLEditorSelectionBlockFormat to)
{
	EHTMLEditorSelectionBlockFormat current_format;

	current_format = get_list_format_from_node (
		WEBKIT_DOM_NODE (list_to_process));
	if (to == current_format) {
		/* Same format, skip it. */
		return list_to_process;
	} else if (current_format >= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST &&
		   to >= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST) {
		/* Changing from ordered list type to another ordered list type. */
		set_ordered_list_type_to_element (list_to_process, to);
		return list_to_process;
	} else {
		WebKitDOMNode *clone, *child;

		/* Create new list from template. */
		clone = webkit_dom_node_clone_node (
			WEBKIT_DOM_NODE (new_list_template), FALSE);

		/* Insert it before the list that we are processing. */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (list_to_process)),
			clone,
			WEBKIT_DOM_NODE (list_to_process),
			NULL);

		/* Move all it children to the new one. */
		while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (list_to_process))))
			webkit_dom_node_append_child (clone, child, NULL);

		remove_node (WEBKIT_DOM_NODE (list_to_process));

		return WEBKIT_DOM_ELEMENT (clone);
	}

	return NULL;
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

	/* Copy elements from previous block to list */
	item = get_list_item_node_from_child (WEBKIT_DOM_NODE (selection_start_marker));
	source_list = webkit_dom_node_get_parent_node (item);
	current_list = source_list;
	source_list_clone = webkit_dom_node_clone_node (source_list, FALSE);

	new_list = create_list_element (selection, document, to, 0, html_mode);

	if (element_has_class (WEBKIT_DOM_ELEMENT (source_list), "-x-evo-indented"))
		element_add_class (WEBKIT_DOM_ELEMENT (new_list), "-x-evo-indented");

	while (item) {
		gboolean selection_end;
		WebKitDOMNode *next_item = webkit_dom_node_get_next_sibling (item);

		selection_end = webkit_dom_node_contains (
			item, WEBKIT_DOM_NODE (selection_end_marker));

		if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (item)) {
			/* Actual node is an item, just copy it. */
			webkit_dom_node_append_child (
				after_selection_end ?
					source_list_clone : WEBKIT_DOM_NODE (new_list),
				item,
				NULL);
		} else if (node_is_list (item) && !selection_end && !after_selection_end) {
			/* Node is a list and it doesn't contain the selection end
			 * marker, we can process the whole list. */
			gint ii;
			WebKitDOMNodeList *list;
			WebKitDOMElement *processed_list;

			list = webkit_dom_element_query_selector_all (
				WEBKIT_DOM_ELEMENT (item), "ol,ul", NULL);
			ii = webkit_dom_node_list_get_length (list);
			g_object_unref (list);

			/* Process every sublist separately. */
			while (ii) {
				WebKitDOMElement *list_to_process;

				list_to_process = webkit_dom_element_query_selector (
					WEBKIT_DOM_ELEMENT (item), "ol,ul", NULL);
				if (list_to_process)
					do_format_change_list_to_list (list_to_process, new_list, to);
				ii--;
			}

			/* Process the current list. */
			processed_list = do_format_change_list_to_list (
				WEBKIT_DOM_ELEMENT (item), new_list, to);

			webkit_dom_node_append_child (
				after_selection_end ?
					source_list_clone : WEBKIT_DOM_NODE (new_list),
				WEBKIT_DOM_NODE (processed_list),
				NULL);
		} else if (node_is_list (item) && !after_selection_end) {
			/* Node is a list and it contains the selection end marker,
			 * thus we have to process it until we find the marker. */
			gint ii;
			WebKitDOMNodeList *list;

			list = webkit_dom_element_query_selector_all (
				WEBKIT_DOM_ELEMENT (item), "ol,ul", NULL);
			ii = webkit_dom_node_list_get_length (list);
			g_object_unref (list);

			/* No nested lists - process the items. */
			if (ii == 0) {
				WebKitDOMNode *clone, *child;

				clone = webkit_dom_node_clone_node (
					WEBKIT_DOM_NODE (new_list), FALSE);

				webkit_dom_node_append_child (
					after_selection_end ?
						source_list_clone : WEBKIT_DOM_NODE (new_list),
					clone,
					NULL);

				while ((child = webkit_dom_node_get_first_child (item))) {
					webkit_dom_node_append_child (clone, child, NULL);
					if (webkit_dom_node_contains (child, WEBKIT_DOM_NODE (selection_end_marker)))
						break;
				}

				if (webkit_dom_node_get_first_child (item))
					webkit_dom_node_append_child (
						after_selection_end ?
							source_list_clone : WEBKIT_DOM_NODE (new_list),
						item,
						NULL);
				else
					remove_node (item);
			} else {
				gboolean done = FALSE;
				WebKitDOMNode *tmp_parent = WEBKIT_DOM_NODE (new_list);
				WebKitDOMNode *tmp_item = WEBKIT_DOM_NODE (item);

				while (!done) {
					WebKitDOMNode *clone, *child;

					clone = webkit_dom_node_clone_node (
						WEBKIT_DOM_NODE (new_list), FALSE);

					webkit_dom_node_append_child (
						tmp_parent, clone, NULL);

					while ((child = webkit_dom_node_get_first_child (tmp_item))) {
						if (!webkit_dom_node_contains (child, WEBKIT_DOM_NODE (selection_end_marker))) {
							webkit_dom_node_append_child (clone, child, NULL);
						} else if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (child)) {
							webkit_dom_node_append_child (clone, child, NULL);
							done = TRUE;
							break;
						} else {
							tmp_parent = clone;
							tmp_item = child;
							break;
						}
					}
				}
			}
		} else {
			webkit_dom_node_append_child (
				after_selection_end ?
					source_list_clone : WEBKIT_DOM_NODE (new_list),
				item,
				NULL);
		}

		if (selection_end) {
			source_list_clone = webkit_dom_node_clone_node (current_list, FALSE);
			after_selection_end = TRUE;
		}

		if (!next_item) {
			if (after_selection_end)
				break;

			current_list = webkit_dom_node_get_next_sibling (current_list);
			if (!node_is_list_or_item (current_list))
				break;
			if (node_is_list (current_list)) {
				next_item = webkit_dom_node_get_first_child (current_list);
				if (!node_is_list_or_item (next_item))
					break;
			} else if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (current_list)) {
				next_item = current_list;
				current_list = webkit_dom_node_get_parent_node (next_item);
			}
		}

		item = next_item;
	}

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (source_list),
		WEBKIT_DOM_NODE (source_list_clone),
		webkit_dom_node_get_next_sibling (source_list),
		NULL);

	if (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (new_list)))
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (source_list_clone),
			WEBKIT_DOM_NODE (new_list),
			source_list_clone,
			NULL);

	if (!webkit_dom_node_has_child_nodes (source_list))
		remove_node (source_list);

	if (!webkit_dom_node_has_child_nodes (source_list_clone))
		remove_node (source_list_clone);

	merge_lists_if_possible (WEBKIT_DOM_NODE (new_list));
}

static void
set_block_alignment (WebKitDOMElement *element,
                     const gchar *class)
{
	WebKitDOMElement *parent;

	element_remove_class (element, "-x-evo-align-center");
	element_remove_class (element, "-x-evo-align-right");
	element_add_class (element, class);
	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (element));
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		element_remove_class (parent, "-x-evo-align-center");
		element_remove_class (parent, "-x-evo-align-right");
		parent = webkit_dom_node_get_parent_element (
			WEBKIT_DOM_NODE (parent));
	}
}

void
e_html_editor_selection_get_selection_coordinates (EHTMLEditorSelection *selection,
                                                   guint *start_x,
                                                   guint *start_y,
                                                   guint *end_x,
                                                   guint *end_y)
{
	EHTMLEditorView *view;
	gboolean created_selection_markers = FALSE;
	guint local_x = 0, local_y = 0;
	WebKitDOMElement *element, *parent;
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (start_x != NULL);
	g_return_if_fail (start_y != NULL);
	g_return_if_fail (end_x != NULL);
	g_return_if_fail (end_y != NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (!element) {
		created_selection_markers = TRUE;
		e_html_editor_selection_save (selection);
		element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (!element)
			return;
	}

	parent = element;
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		local_x += (guint) webkit_dom_element_get_offset_left (parent);
		local_y += (guint) webkit_dom_element_get_offset_top (parent);
		parent = webkit_dom_element_get_offset_parent (parent);
	}

	if (start_x)
		*start_x = local_x;
	if (start_y)
		*start_y = local_y;

	if (e_html_editor_selection_is_collapsed (selection)) {
		*end_x = local_x;
		*end_y = local_y;

		if (created_selection_markers)
			e_html_editor_selection_restore (selection);

		goto workaroud;
	}

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	local_x = 0;
	local_y = 0;

	parent = element;
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		local_x += (guint) webkit_dom_element_get_offset_left (parent);
		local_y += (guint) webkit_dom_element_get_offset_top (parent);
		parent = webkit_dom_element_get_offset_parent (parent);
	}

	if (end_x)
		*end_x = local_x;
	if (end_y)
		*end_y = local_y;

	if (created_selection_markers)
		e_html_editor_selection_restore (selection);

 workaroud:
	/* Workaround for bug 749712 on the Evolution side. The cause of the bug
	 * is that WebKit is having problems determining the right line height
	 * for some fonts and font sizes (the right and wrong value differ by 1).
	 * To fix this we will add an extra one to the final top offset. This is
	 * safe to do even for fonts and font sizes that don't behave badly as we
	 * will still get the right element as we use fonts bigger than 1 pixel. */
	*start_y += 1;
	*end_y += 1;
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
	EHTMLEditorSelectionAlignment current_alignment;
	EHTMLEditorView *view;
	EHTMLEditorViewHistoryEvent *ev = NULL;
	gboolean after_selection_end = FALSE;
	const gchar *class = "";
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	current_alignment = e_html_editor_selection_get_alignment (selection);

	if (current_alignment == alignment)
		return;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	switch (alignment) {
		case E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER:
			class = "-x-evo-align-center";
			break;

		case E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT:
			break;

		case E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT:
			class = "-x-evo-align-right";
			break;
	}

	selection->priv->alignment = alignment;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	e_html_editor_selection_save (selection);

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker) {
		g_object_unref (view);
		return;
	}

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_ALIGNMENT;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
		ev->data.style.from = current_alignment;
		ev->data.style.to = alignment;
	}

	block = e_html_editor_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	while (block && !after_selection_end) {
		WebKitDOMNode *next_block;

		next_block = webkit_dom_node_get_next_sibling (block);

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		if (element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-indented")) {
			gint ii, length;
			WebKitDOMNodeList *list;

			list = webkit_dom_element_query_selector_all (
				WEBKIT_DOM_ELEMENT (block),
				".-x-evo-indented > *:not(.-x-evo-indented):not(li)",
				NULL);
			length = webkit_dom_node_list_get_length (list);

			for (ii = 0; ii < length; ii++) {
				WebKitDOMNode *item = webkit_dom_node_list_item (list, ii);

				set_block_alignment (WEBKIT_DOM_ELEMENT (item), class);

				after_selection_end = webkit_dom_node_contains (
					item, WEBKIT_DOM_NODE (selection_end_marker));
				g_object_unref (item);
				if (after_selection_end)
					break;
			}

			g_object_unref (list);
		} else {
			set_block_alignment (WEBKIT_DOM_ELEMENT (block), class);
		}

		block = next_block;
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
	e_html_editor_view_force_spell_check_for_current_paragraph (view);

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
	EHTMLEditorView *view;
	WebKitDOMNode *ancestor;
	WebKitDOMRange *range;
	WebKitDOMCSSStyleDeclaration *css;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	if (!e_html_editor_view_get_html_mode (view)) {
		g_object_unref (view);
		return "#ffffff";
	}

	g_object_unref (view);

	range = html_editor_selection_get_current_range (selection);

	ancestor = webkit_dom_range_get_common_ancestor_container (range, NULL);

	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (ancestor));
	g_free (selection->priv->background_color);
	selection->priv->background_color =
		webkit_dom_css_style_declaration_get_property_value (
			css, "background-color");

	g_object_unref (css);
	g_object_unref (range);

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
	node = e_html_editor_get_parent_block_node_from_child (node);

	return node;
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

	if ((element = e_html_editor_dom_node_find_parent_element (node, "UL"))) {
		WebKitDOMElement *tmp_element;

		tmp_element = e_html_editor_dom_node_find_parent_element (node, "OL");
		if (tmp_element) {
			if (webkit_dom_node_contains (WEBKIT_DOM_NODE (tmp_element), WEBKIT_DOM_NODE (element)))
				result = get_list_format_from_node (WEBKIT_DOM_NODE (element));
			else
				result = get_list_format_from_node (WEBKIT_DOM_NODE (tmp_element));
		} else
			result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;
	} else if ((element = e_html_editor_dom_node_find_parent_element (node, "OL")) != NULL) {
		WebKitDOMElement *tmp_element;

		tmp_element = e_html_editor_dom_node_find_parent_element (node, "UL");
		if (tmp_element) {
			if (webkit_dom_node_contains (WEBKIT_DOM_NODE (element), WEBKIT_DOM_NODE (tmp_element)))
				result = get_list_format_from_node (WEBKIT_DOM_NODE (element));
			else
				result = get_list_format_from_node (WEBKIT_DOM_NODE (tmp_element));
		} else
			result = get_list_format_from_node (WEBKIT_DOM_NODE (element));
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

			if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (block) ||
			    element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-paragraph"))
				result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
			else {
				/* Paragraphs inside quote */
				if ((element = e_html_editor_dom_node_find_parent_element (node, "DIV")) != NULL)
					if (element_has_class (element, "-x-evo-paragraph"))
						result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
					else
						result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE;
				else
					result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE;
			}
		}
	} else if (e_html_editor_dom_node_find_parent_element (node, "P")) {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
	} else {
		result = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
	}

	g_object_unref (range);

	return result;
}

void
remove_wrapping_from_element (WebKitDOMElement *element)
{
	WebKitDOMNodeList *list;
	gint ii, length;

	g_return_if_fail (element != NULL);

	list = webkit_dom_element_query_selector_all (
		element, "br.-x-evo-wrap-br", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		WebKitDOMNode *parent;

		parent = e_html_editor_get_parent_block_node_from_child (node);
		if (!webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "data-user-wrapped"))
			remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);

	list = webkit_dom_element_query_selector_all (
		element, "span[data-hidden-space]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *hidden_space_node;
		WebKitDOMNode *parent;

		hidden_space_node = webkit_dom_node_list_item (list, ii);
		parent = e_html_editor_get_parent_block_node_from_child (hidden_space_node);
		if (!webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "data-user-wrapped")) {
			webkit_dom_html_element_set_outer_text (
				WEBKIT_DOM_HTML_ELEMENT (hidden_space_node), " ", NULL);
		}
		g_object_unref (hidden_space_node);
	}
	g_object_unref (list);

	webkit_dom_node_normalize (WEBKIT_DOM_NODE (element));
}

void
remove_quoting_from_element (WebKitDOMElement *element)
{
	gint ii, length;
	WebKitDOMNodeList *list;

	g_return_if_fail (element != NULL);

	list = webkit_dom_element_query_selector_all (
		element, "span.-x-evo-quoted", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);

	list = webkit_dom_element_query_selector_all (
		element, "span.-x-evo-temp-text-wrapper", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);

		while (webkit_dom_node_get_first_child (node))
			webkit_dom_node_insert_before (
				parent,
				webkit_dom_node_get_first_child (node),
				node,
				NULL);

		remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);

	list = webkit_dom_element_query_selector_all (
		element, "br.-x-evo-temp-br", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		remove_node (node);
		g_object_unref (node);
	}
	g_object_unref (list);

	webkit_dom_node_normalize (WEBKIT_DOM_NODE (element));
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

static WebKitDOMNode *
indent_block (EHTMLEditorSelection *selection,
              WebKitDOMDocument *document,
              WebKitDOMNode *block,
              gint width)
{
	WebKitDOMElement *element;
	WebKitDOMNode *sibling, *tmp;

	sibling = webkit_dom_node_get_previous_sibling (block);
	if (WEBKIT_DOM_IS_ELEMENT (sibling) &&
	    element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-indented")) {
		element = WEBKIT_DOM_ELEMENT (sibling);
	} else {
		element = e_html_editor_selection_get_indented_element (
			selection, document, width);

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			WEBKIT_DOM_NODE (element),
			block,
			NULL);
	}

	/* Remove style and let the paragraph inherit it from parent */
	if (element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-paragraph"))
		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (block), "style");

	tmp = webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (element),
		block,
		NULL);

	sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));

	while (WEBKIT_DOM_IS_ELEMENT (sibling) &&
	       element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-indented")) {
		WebKitDOMNode *next_sibling;
		WebKitDOMNode *child;

		next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (sibling));

		while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (sibling)))) {
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element),
				child,
				NULL);
		}
		remove_node (sibling);
		sibling = next_sibling;
	}

	return tmp;
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

static gboolean
do_format_change_list_to_block (EHTMLEditorSelection *selection,
                                EHTMLEditorSelectionBlockFormat format,
                                WebKitDOMNode *item,
                                const gchar *value,
                                WebKitDOMDocument *document)
{
	gboolean after_end = FALSE;
	gint level;
	WebKitDOMElement *element, *selection_end;
	WebKitDOMNode *node, *source_list;

	selection_end = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	source_list = webkit_dom_node_get_parent_node (item);
	while (source_list) {
		WebKitDOMNode *parent;

		parent = webkit_dom_node_get_parent_node (source_list);
		if (node_is_list (parent))
			source_list = parent;
		else
			break;
	}

	if (webkit_dom_node_contains (source_list, WEBKIT_DOM_NODE (selection_end)))
		source_list = split_node_into_two (item, -1);
	else {
		source_list = webkit_dom_node_get_next_sibling (source_list);
	}

	/* Process all nodes that are in selection one by one */
	while (item && WEBKIT_DOM_IS_HTMLLI_ELEMENT (item)) {
		WebKitDOMNode *next_item;

		next_item = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (item));
		if (!next_item) {
			WebKitDOMNode *parent;
			WebKitDOMNode *tmp = item;

			while (tmp) {
				parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (tmp));
				if (!node_is_list (parent))
					break;

				next_item = webkit_dom_node_get_next_sibling (parent);
				if (node_is_list (next_item)) {
					next_item = webkit_dom_node_get_first_child (next_item);
					break;
				} else if (next_item && !WEBKIT_DOM_IS_HTMLLI_ELEMENT (next_item)) {
					next_item = webkit_dom_node_get_next_sibling (next_item);
					break;
				} else if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (next_item)) {
					break;
				}
				tmp = parent;
			}
		} else if (node_is_list (next_item)) {
			next_item = webkit_dom_node_get_first_child (next_item);
		} else if (!WEBKIT_DOM_IS_HTMLLI_ELEMENT (next_item)) {
			next_item = webkit_dom_node_get_next_sibling (item);
			continue;
		}

		if (!after_end) {
			after_end = webkit_dom_node_contains (item, WEBKIT_DOM_NODE (selection_end));

			level = get_indentation_level (WEBKIT_DOM_ELEMENT (item));

			if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH) {
				element = e_html_editor_selection_get_paragraph_element (
					selection, document, -1, 0);
			} else
				element = webkit_dom_document_create_element (
					document, value, NULL);

			while ((node = webkit_dom_node_get_first_child (item)))
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (element), node, NULL);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (source_list),
				WEBKIT_DOM_NODE (element),
				source_list,
				NULL);

			if (level > 0) {
				gint final_width = 0;

				node = WEBKIT_DOM_NODE (element);

				if (element_has_class (element, "-x-evo-paragraph"))
					final_width = selection->priv->word_wrap_length -
						SPACES_PER_INDENTATION * level;

				while (level--)
					node = indent_block (selection, document, node, final_width);
			}

			remove_node_and_parents_if_empty (item);
		} else
			break;

		item = next_item;
	}

	remove_node_if_empty (source_list);

	return after_end;
}

static void
format_change_list_to_block (EHTMLEditorSelection *selection,
                             EHTMLEditorSelectionBlockFormat format,
                             const gchar *value,
                             WebKitDOMDocument *document)
{
	WebKitDOMElement *selection_start;
	WebKitDOMNode *item;

	selection_start = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);

	item = get_list_item_node_from_child (WEBKIT_DOM_NODE (selection_start));

	do_format_change_list_to_block (selection, format, item, value, document);
}

static void
change_leading_space_to_nbsp (WebKitDOMNode *block)
{
	WebKitDOMNode *child;

	if (!WEBKIT_DOM_IS_HTML_PRE_ELEMENT (block))
		return;

	if ((child = webkit_dom_node_get_first_child (block)) &&
	     WEBKIT_DOM_IS_CHARACTER_DATA (child)) {
		gchar *data;

		data = webkit_dom_character_data_substring_data (
			WEBKIT_DOM_CHARACTER_DATA (child), 0, 1, NULL);

		if (data && *data == ' ')
			webkit_dom_character_data_replace_data (
				WEBKIT_DOM_CHARACTER_DATA (child), 0, 1, UNICODE_NBSP, NULL);
		g_free (data);
	}
}

static void
change_trailing_space_in_block_to_nbsp (WebKitDOMNode *block)
{
	WebKitDOMNode *child;

	if ((child = webkit_dom_node_get_last_child (block)) &&
	    WEBKIT_DOM_IS_CHARACTER_DATA (child)) {
		gchar *tmp;
		gulong length;

		length = webkit_dom_character_data_get_length (
			WEBKIT_DOM_CHARACTER_DATA (child));

		tmp = webkit_dom_character_data_substring_data (
			WEBKIT_DOM_CHARACTER_DATA (child), length - 1, 1, NULL);
		if (tmp && *tmp == ' ') {
			webkit_dom_character_data_replace_data (
				WEBKIT_DOM_CHARACTER_DATA (child),
				length - 1,
				1,
				UNICODE_NBSP,
				NULL);
		}
		g_free (tmp);
	}
}

static void
change_space_before_selection_to_nbsp (WebKitDOMNode *node)
{
	WebKitDOMNode *prev_sibling;

	if ((prev_sibling = webkit_dom_node_get_previous_sibling (node))) {
		if (WEBKIT_DOM_IS_CHARACTER_DATA (prev_sibling)) {
			gchar *tmp;
			gulong length;

			length = webkit_dom_character_data_get_length (
				WEBKIT_DOM_CHARACTER_DATA (prev_sibling));

			tmp = webkit_dom_character_data_substring_data (
				WEBKIT_DOM_CHARACTER_DATA (prev_sibling), length - 1, 1, NULL);
			if (tmp && *tmp == ' ') {
				webkit_dom_character_data_replace_data (
					WEBKIT_DOM_CHARACTER_DATA (prev_sibling),
					length - 1,
					1,
					UNICODE_NBSP,
					NULL);
			}
			g_free (tmp);
		}
	}
}

static gboolean
process_block_to_block (EHTMLEditorSelection *selection,
                        EHTMLEditorView *view,
                        WebKitDOMDocument *document,
                        EHTMLEditorSelectionBlockFormat format,
                        const gchar *value,
                        WebKitDOMNode *block,
                        WebKitDOMNode *end_block,
                        WebKitDOMNode *blockquote,
                        gboolean html_mode)
{
	gboolean after_selection_end = FALSE;
	WebKitDOMNode *next_block;

	while (!after_selection_end && block) {
		gboolean quoted = FALSE;
		gboolean empty = FALSE;
		gchar *content;
		WebKitDOMNode *child;
		WebKitDOMElement *element;

		if (is_citation_node (block)) {
			gboolean finished;

			next_block = webkit_dom_node_get_next_sibling (block);
			finished = process_block_to_block (
				selection,
				view,
				document,
				format,
				value,
				webkit_dom_node_get_first_child (block),
				end_block,
				blockquote,
				html_mode);

			if (finished)
				return TRUE;

			block = next_block;

			continue;
		}

		if (webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), "span.-x-evo-quoted", NULL)) {
			quoted = TRUE;
			remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));
		}

		if (!html_mode)
			remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));

		after_selection_end = webkit_dom_node_is_same_node (block, end_block);

		next_block = webkit_dom_node_get_next_sibling (block);

		if (node_is_list (block)) {
			WebKitDOMNode *item;

			item = webkit_dom_node_get_first_child (block);
			while (item && !WEBKIT_DOM_IS_HTMLLI_ELEMENT (item))
				item = webkit_dom_node_get_first_child (item);

			if (item && do_format_change_list_to_block (selection, format, item, value, document))
				return TRUE;

			block = next_block;

			continue;
		}

		if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH ||
		    format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE)
			element = e_html_editor_selection_get_paragraph_element (
				selection, document, -1, 0);
		else
			element = webkit_dom_document_create_element (
				document, value, NULL);

		content = webkit_dom_node_get_text_content (block);

		empty = !*content || (g_strcmp0 (content, UNICODE_ZERO_WIDTH_SPACE) == 0);
		g_free (content);

		change_leading_space_to_nbsp (block);
		change_trailing_space_in_block_to_nbsp (block);

		while ((child = webkit_dom_node_get_first_child (block))) {
			if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (child))
				empty = FALSE;

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element), child, NULL);
		}

		if (empty) {
			WebKitDOMElement *br;

			br = webkit_dom_document_create_element (
				document, "BR", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element), WEBKIT_DOM_NODE (br), NULL);
		}

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			WEBKIT_DOM_NODE (element),
			block,
			NULL);

		remove_node (block);

		if (!next_block && !after_selection_end) {
			gint citation_level;

			citation_level = get_citation_level (WEBKIT_DOM_NODE (element));

			if (citation_level > 0) {
				next_block = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
				next_block = webkit_dom_node_get_next_sibling (next_block);
			}
		}

		block = next_block;

		if (!html_mode &&
		    (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH ||
		     format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE)) {
			gint citation_level, quote;

			if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE)
				citation_level = 1;
			else
				citation_level = get_citation_level (WEBKIT_DOM_NODE (element));
			quote = citation_level ? citation_level * 2 : 0;

			if (citation_level > 0)
				element = e_html_editor_selection_wrap_paragraph_length (
					selection, element, selection->priv->word_wrap_length - quote);
		}

		if (blockquote && format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE) {
			webkit_dom_node_append_child (
				blockquote, WEBKIT_DOM_NODE (element), NULL);
			if (!html_mode)
				e_html_editor_view_quote_plain_text_element_after_wrapping (document, element, 1);
		} else
			if (!html_mode && quoted)
				e_html_editor_view_quote_plain_text_element (view, element);
	}

	return after_selection_end;
}

static void
format_change_block_to_block (EHTMLEditorSelection *selection,
                              EHTMLEditorSelectionBlockFormat format,
                              EHTMLEditorView *view,
                              const gchar *value,
                              WebKitDOMDocument *document)
{
	gboolean html_mode;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block, *end_block, *blockquote = NULL;

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

	block = e_html_editor_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	html_mode = e_html_editor_view_get_html_mode (view);

	if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE) {
		blockquote = WEBKIT_DOM_NODE (
			webkit_dom_document_create_element (document, "BLOCKQUOTE", NULL));

		webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (blockquote), "type", "cite", NULL);
		if (!html_mode)
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (blockquote), "class", "-x-evo-plaintext-quoted", NULL);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			blockquote,
			block,
			NULL);
	}

	end_block = e_html_editor_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_end_marker));

	/* Process all blocks that are in the selection one by one */
	process_block_to_block (
		selection, view, document, format, value, block, end_block, blockquote, html_mode);
}

static void
format_change_block_to_list (EHTMLEditorSelection *selection,
                             EHTMLEditorSelectionBlockFormat format,
                             EHTMLEditorView *view,
                             WebKitDOMDocument *document)
{
	gboolean after_selection_end = FALSE, in_quote = FALSE;
	gboolean html_mode = e_html_editor_view_get_html_mode (view);
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *item, *list;
	WebKitDOMNode *block, *next_block;

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

	block = e_html_editor_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	list = create_list_element (selection, document, format, 0, html_mode);

	if (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (block), "span.-x-evo-quoted", NULL)) {
		WebKitDOMElement *element;
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;
		WebKitDOMRange *range;

		in_quote = TRUE;

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		range = webkit_dom_document_create_range (document);

		webkit_dom_range_select_node (range, block, NULL);
		webkit_dom_range_collapse (range, TRUE, NULL);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);

		g_object_unref (range);
		g_object_unref (dom_selection);
		g_object_unref (dom_window);

		e_html_editor_view_exec_command (
			view, E_HTML_EDITOR_VIEW_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, NULL);

		element = webkit_dom_document_query_selector (
			document, "body>br", NULL);

		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			WEBKIT_DOM_NODE (list),
			WEBKIT_DOM_NODE (element),
			NULL);

		block = e_html_editor_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
	} else
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			WEBKIT_DOM_NODE (list),
			block,
			NULL);

	/* Process all blocks that are in the selection one by one */
	while (block && !after_selection_end) {
		gboolean empty = FALSE;
		gchar *content;
		WebKitDOMNode *child, *parent;

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		next_block = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (block));

		remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));
		remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));

		item = webkit_dom_document_create_element (document, "LI", NULL);
		content = webkit_dom_node_get_text_content (block);

		empty = !*content || (g_strcmp0 (content, UNICODE_ZERO_WIDTH_SPACE) == 0);
		g_free (content);

		change_leading_space_to_nbsp (block);
		change_trailing_space_in_block_to_nbsp (block);

		while ((child = webkit_dom_node_get_first_child (block))) {
			if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (child))
				empty = FALSE;

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (item), child, NULL);
		}

		/* We have to use again the hidden space to move caret into newly inserted list */
		if (empty) {
			WebKitDOMElement *br;

			br = webkit_dom_document_create_element (
				document, "BR", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (item), WEBKIT_DOM_NODE (br), NULL);
		}

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (list), WEBKIT_DOM_NODE (item), NULL);

		parent = webkit_dom_node_get_parent_node (block);
		remove_node (block);

		if (in_quote) {
			/* Remove all parents if previously removed node was the
			 * only one with text content */
			content = webkit_dom_node_get_text_content (parent);
			while (parent && content && !*content) {
				WebKitDOMNode *tmp = webkit_dom_node_get_parent_node (parent);

				remove_node (parent);
				parent = tmp;

				g_free (content);
				content = webkit_dom_node_get_text_content (parent);
			}
			g_free (content);
		}

		block = next_block;
	}

	merge_lists_if_possible (WEBKIT_DOM_NODE (list));
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

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	current_list = get_list_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	prev_list = get_list_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	next_list = get_list_node_from_child (
		WEBKIT_DOM_NODE (selection_end_marker));

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
		return;
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
			return;
		}
	}

	prev = get_list_format_from_node (prev_list);
	next = get_list_format_from_node (next_list);

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
		return;

	format_change_list_from_list (selection, document, format, html_mode);
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
	EHTMLEditorSelectionAlignment current_alignment;
	EHTMLEditorViewHistoryEvent *ev = NULL;
	const gchar *value;
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

	current_alignment = selection->priv->alignment;

	e_html_editor_selection_save (selection);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		if (format != E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE)
			ev->type = HISTORY_BLOCK_FORMAT;
		else
			ev->type = HISTORY_BLOCKQUOTE;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		if (format != E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE) {
			ev->data.style.from = current_format;
			ev->data.style.to = format;
		} else {
			WebKitDOMDocumentFragment *fragment;
			WebKitDOMElement *selection_start_marker, *selection_end_marker;
			WebKitDOMNode *block, *end_block;

			selection_start_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			selection_end_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");
			block = e_html_editor_get_parent_block_node_from_child (
				WEBKIT_DOM_NODE (selection_start_marker));
			end_block = e_html_editor_get_parent_block_node_from_child (
				WEBKIT_DOM_NODE (selection_end_marker));
			if (webkit_dom_range_get_collapsed (range, NULL) ||
			    webkit_dom_node_is_same_node (block, end_block)) {
				fragment = webkit_dom_document_create_document_fragment (document);

				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (fragment),
					webkit_dom_node_clone_node (block, TRUE),
					NULL);
			} else {
				fragment = webkit_dom_range_clone_contents (range, NULL);
				webkit_dom_node_replace_child (
					WEBKIT_DOM_NODE (fragment),
					webkit_dom_node_clone_node (block, TRUE),
					webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
					NULL);

				webkit_dom_node_replace_child (
					WEBKIT_DOM_NODE (fragment),
					webkit_dom_node_clone_node (end_block, TRUE),
					webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (fragment)),
					NULL);
			}
			ev->data.fragment = fragment;
		}
	}

	g_object_unref (range);

	if (current_format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PRE) {
		WebKitDOMElement *selection_marker;

		selection_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (selection_marker)
			change_space_before_selection_to_nbsp (WEBKIT_DOM_NODE (selection_marker));
		selection_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		if (selection_marker)
			change_space_before_selection_to_nbsp (WEBKIT_DOM_NODE (selection_marker));
	}

	if (from_list && to_list)
		format_change_list_to_list (selection, format, document, html_mode);

	if (!from_list && !to_list)
		format_change_block_to_block (selection, format, view, value, document);

	if (from_list && !to_list) {
		format_change_list_to_block (selection, format, value, document);
		if (format == E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE) {
			e_html_editor_selection_restore (selection);
			format_change_block_to_block (selection, format, view, value, document);
		}
	}

	if (!from_list && to_list)
		format_change_block_to_list (selection, format, view, document);

	e_html_editor_selection_restore (selection);

	e_html_editor_view_force_spell_check_for_current_paragraph (view);

	/* When changing the format we need to re-set the alignment */
	e_html_editor_selection_set_alignment (selection, current_alignment);

	e_html_editor_view_set_changed (view, TRUE);

	if (ev) {
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}

	g_object_unref (view);

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
	EHTMLEditorView *view;
	gchar *color;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	if (!e_html_editor_view_get_html_mode (view)) {
		g_object_unref (view);
		*rgba = black;
		return;
	}

	color = get_font_property (selection, "color");
	if (!(color && *color)) {
		WebKitDOMDocument *document;
		WebKitDOMHTMLElement *body;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		body = webkit_dom_document_get_body (document);

		g_free (color);
		color = webkit_dom_html_body_element_get_text (WEBKIT_DOM_HTML_BODY_ELEMENT (body));
		if (!(color && *color)) {
			*rgba = black;
			g_object_unref (view);
			g_free (color);
			return;
		}
	}

	gdk_rgba_parse (rgba, color);
	g_free (color);
	g_object_unref (view);
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
	EHTMLEditorViewHistoryEvent *ev = NULL;
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

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_FONT_COLOR;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
		ev->data.string.from = g_strdup (selection->priv->font_color);
		ev->data.string.to = g_strdup (color);
	}

	g_free (selection->priv->font_color);
	selection->priv->font_color = g_strdup (color);
	e_html_editor_view_exec_command (view, command, color);
	g_free (color);

	if (ev) {
		ev->after.start.x = ev->before.start.x;
		ev->after.start.y = ev->before.start.y;
		ev->after.end.x = ev->before.end.x;
		ev->after.end.y = ev->before.end.y;

		e_html_editor_view_insert_new_history_event (view, ev);
	}

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
	g_object_unref (range);

	g_free (selection->priv->font_family);
	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (node));
	selection->priv->font_family =
		webkit_dom_css_style_declaration_get_property_value (css, "fontFamily");

	g_object_unref (css);

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
	gboolean increment;

	g_return_val_if_fail (
		E_IS_HTML_EDITOR_SELECTION (selection),
		E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL);

	size = get_font_property (selection, "size");
	if (!(size && *size)) {
		g_free (size);
		return E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;
	}

	/* We don't support increments, but when going through a content that
	 * was not written in Evolution we can find it. In this case just report
	 * the normal size. */
	/* FIXME: go through all parent and get the right value. */
	increment = size[0] == '+' || size[0] == '-';
	size_int = atoi (size);
	g_free (size);

	if (increment || size_int == 0)
		return E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;

	return size_int;
}

static WebKitDOMElement *
set_font_style (WebKitDOMDocument *document,
                const gchar *element_name,
                gboolean value)
{
	WebKitDOMElement *element;
	WebKitDOMNode *parent;

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-end-marker");
	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
	if (value) {
		WebKitDOMNode *node;
		WebKitDOMElement *el;
		gchar *name;

		el = webkit_dom_document_create_element (document, element_name, NULL);
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (el), UNICODE_ZERO_WIDTH_SPACE, NULL);

		node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (el), node, NULL);
		name = webkit_dom_node_get_local_name (parent);
		if (g_strcmp0 (name, element_name) == 0 && g_strcmp0 (name, "font") != 0)
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (el),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
		else
			webkit_dom_node_insert_before (
				parent,
				WEBKIT_DOM_NODE (el),
				WEBKIT_DOM_NODE (element),
				NULL);
		g_free (name);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (el), WEBKIT_DOM_NODE (element), NULL);

		return el;
	} else {
		WebKitDOMNode *node;

		node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));

		/* Turning the formatting in the middle of element. */
		if (webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element))) {
			WebKitDOMNode *clone;
			WebKitDOMNode *sibling;

			clone = webkit_dom_node_clone_node (
				WEBKIT_DOM_NODE (parent), FALSE);

			while ((sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element))))
				webkit_dom_node_insert_before (
					clone,
					sibling,
					webkit_dom_node_get_first_child (clone),
					NULL);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				clone,
				webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent)),
				NULL);
		}

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			WEBKIT_DOM_NODE (element),
			webkit_dom_node_get_next_sibling (parent),
			NULL);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			node,
			webkit_dom_node_get_next_sibling (parent),
			NULL);

		webkit_dom_html_element_insert_adjacent_text (
			WEBKIT_DOM_HTML_ELEMENT (parent),
			"afterend",
			UNICODE_ZERO_WIDTH_SPACE,
			NULL);
	}

	return NULL;
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
	EHTMLEditorViewHistoryEvent *ev = NULL;
	gchar *size_str;
	guint current_font_size;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	current_font_size = e_html_editor_selection_get_font_size (selection);
	if (current_font_size == font_size) {
		g_object_unref (view);
		return;
	}

	e_html_editor_selection_save (selection);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_FONT_SIZE;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
		ev->data.style.from = current_font_size;
		ev->data.style.to = font_size;
	}

	selection->priv->font_size = font_size;
	size_str = g_strdup_printf ("%d", font_size);

	if (e_html_editor_selection_is_collapsed (selection)) {
		WebKitDOMElement *font;
		WebKitDOMDocument *document;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		font = set_font_style (document, "font", font_size != 3);
		if (font)
			webkit_dom_element_set_attribute (font, "size", size_str, NULL);
		e_html_editor_selection_restore (selection);
		goto exit;
	}

	e_html_editor_selection_restore (selection);

	command = E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE;
	e_html_editor_view_exec_command (view, command, size_str);

	/* Text in <font size="3"></font> (size 3 is our default size) is a little
	 * bit smaller than font outsize it. So move it outside of it. */
	if (font_size == E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL) {
		WebKitDOMDocument *document;
		WebKitDOMElement *element;

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		element = webkit_dom_document_query_selector (document, "font[size=\"3\"]", NULL);
		if (element) {
			WebKitDOMNode *child;

			while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element))))
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
					child,
					WEBKIT_DOM_NODE (element),
					NULL);

			remove_node (WEBKIT_DOM_NODE (element));
		}
	}
 exit:
	g_free (size_str);

	if (ev) {
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}

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
	g_object_unref (range);

	if (WEBKIT_DOM_IS_TEXT (node))
		return get_has_style (selection, "citation");

	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return FALSE;
	}
	g_free (text_content);

	value = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "type");

	/* citation == <blockquote type='cite'> */
	if (strstr (value, "cite"))
		ret_val = TRUE;
	else
		ret_val = get_has_style (selection, "citation");

	g_free (value);
	return ret_val;
}

static WebKitDOMNode *
get_parent_indented_block (WebKitDOMNode *node)
{
	WebKitDOMNode *parent, *block = NULL;

	parent = webkit_dom_node_get_parent_node (node);
	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-indented"))
		block = parent;

	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent) &&
	       !WEBKIT_DOM_IS_HTML_HTML_ELEMENT (parent)) {
		if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-indented"))
			block = parent;
		parent = webkit_dom_node_get_parent_node (parent);
	}

	return block;
}

static WebKitDOMElement*
get_element_for_inspection (WebKitDOMRange *range)
{
	WebKitDOMNode *node;

	node = webkit_dom_range_get_end_container (range, NULL);
	/* No selection or whole body selected */
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node))
		return NULL;

	return WEBKIT_DOM_ELEMENT (get_parent_indented_block (node));
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
	WebKitDOMElement *element;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	if (webkit_dom_range_get_collapsed (range, NULL)) {
		element = get_element_for_inspection (range);
		g_object_unref (range);
		return element_has_class (element, "-x-evo-indented");
	} else {
		WebKitDOMNode *node;
		gboolean ret_val;

		node = webkit_dom_range_get_end_container (range, NULL);
		/* No selection or whole body selected */
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node))
			goto out;

		element = WEBKIT_DOM_ELEMENT (get_parent_indented_block (node));
		ret_val = element_has_class (element, "-x-evo-indented");
		if (!ret_val)
			goto out;

		node = webkit_dom_range_get_start_container (range, NULL);
		/* No selection or whole body selected */
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node))
			goto out;

		element = WEBKIT_DOM_ELEMENT (get_parent_indented_block (node));
		ret_val = element_has_class (element, "-x-evo-indented");

		g_object_unref (range);
		return ret_val;
	}

 out:
	g_object_unref (range);

	return FALSE;
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

static gboolean
indent_list (EHTMLEditorSelection *selection,
             WebKitDOMDocument *document)
{
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *item, *next_item;
	gboolean after_selection_end = FALSE;

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);

	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	item = e_html_editor_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (item)) {
		gboolean html_mode = is_in_html_mode (selection);
		WebKitDOMElement *list;
		WebKitDOMNode *source_list = webkit_dom_node_get_parent_node (item);
		EHTMLEditorSelectionBlockFormat format;

		format = get_list_format_from_node (source_list);

		list = create_list_element (
			selection, document, format, get_list_level (item), html_mode);

		element_add_class (list, "-x-evo-indented");

		webkit_dom_node_insert_before (
			source_list, WEBKIT_DOM_NODE (list), item, NULL);

		while (item && !after_selection_end) {
			after_selection_end = webkit_dom_node_contains (
				item, WEBKIT_DOM_NODE (selection_end_marker));

			next_item = webkit_dom_node_get_next_sibling (item);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (list), item, NULL);

			item = next_item;
		}

		merge_lists_if_possible (WEBKIT_DOM_NODE (list));
	}

	return after_selection_end;
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
	EHTMLEditorView *view;
	EHTMLEditorViewHistoryEvent *ev = NULL;
	gboolean after_selection_start = FALSE, after_selection_end = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

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

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_INDENT;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
		ev->data.style.from = 1;
		ev->data.style.to = 1;
	}

	block = get_parent_indented_block (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (!block)
		block = e_html_editor_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

	while (block && !after_selection_end) {
		gint ii, length, level, final_width = 0;
		gint word_wrap_length = selection->priv->word_wrap_length;
		WebKitDOMNode *next_block;
		WebKitDOMNodeList *list;

		next_block = webkit_dom_node_get_next_sibling (block);

		list = webkit_dom_element_query_selector_all (
			WEBKIT_DOM_ELEMENT (block),
			".-x-evo-indented > *:not(.-x-evo-indented):not(li)",
			NULL);

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		length = webkit_dom_node_list_get_length (list);
		if (length == 0 && node_is_list_or_item (block)) {
			after_selection_end = indent_list (selection, document);
			goto next;
		}

		if (length == 0) {
			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block, WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start)
					goto next;
			}

			if (element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-paragraph")) {
				level = get_indentation_level (WEBKIT_DOM_ELEMENT (block));

				final_width = word_wrap_length - SPACES_PER_INDENTATION * (level + 1);
				if (final_width < MINIMAL_PARAGRAPH_WIDTH &&
				    !is_in_html_mode (selection))
					goto next;
			}

			indent_block (selection, document, block, final_width);

			if (after_selection_end)
				goto next;
		}

		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *block_to_process;

			block_to_process = webkit_dom_node_list_item (list, ii);

			after_selection_end = webkit_dom_node_contains (
				block_to_process, WEBKIT_DOM_NODE (selection_end_marker));

			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block_to_process,
					WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start) {
					g_object_unref (block_to_process);
					continue;
				}
			}

			if (element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-paragraph")) {
				level = get_indentation_level (
					WEBKIT_DOM_ELEMENT (block_to_process));

				final_width = word_wrap_length - SPACES_PER_INDENTATION * (level + 1);
				if (final_width < MINIMAL_PARAGRAPH_WIDTH &&
				    !is_in_html_mode (selection)) {
					g_object_unref (block_to_process);
					continue;
				}
			}

			indent_block (selection, document, block_to_process, final_width);

			if (after_selection_end) {
				g_object_unref (block_to_process);
				break;
			}
			g_object_unref (block_to_process);
		}

 next:
		g_object_unref (list);

		if (!after_selection_end)
			block = next_block;
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
	e_html_editor_view_force_spell_check_for_current_paragraph (view);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "indented");
}

static const gchar *
get_css_alignment_value_class (EHTMLEditorSelectionAlignment alignment)
{
	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT)
		return ""; /* Left is by default on ltr */

	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER)
		return "-x-evo-align-center";

	if (alignment == E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT)
		return "-x-evo-align-right";

	return "";
}

static void
unindent_list (EHTMLEditorSelection *selection,
               WebKitDOMDocument *document)
{
	gboolean after = FALSE;
	WebKitDOMElement *new_list;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *source_list, *source_list_clone, *current_list, *item;
	WebKitDOMNode *prev_item;

	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);
	selection_end_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-end-marker", NULL);

	if (!selection_start_marker || !selection_end_marker)
		return;

	/* Copy elements from previous block to list */
	item = e_html_editor_get_parent_block_node_from_child (
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

	prev_item = source_list;

	while (item) {
		WebKitDOMNode *next_item = webkit_dom_node_get_next_sibling (item);

		if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (item)) {
			if (after)
				prev_item = webkit_dom_node_append_child (
					source_list_clone, WEBKIT_DOM_NODE (item), NULL);
			else
				prev_item = webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (prev_item),
					item,
					webkit_dom_node_get_next_sibling (prev_item),
					NULL);
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

static void
unindent_block (EHTMLEditorSelection *selection,
                WebKitDOMDocument *document,
                WebKitDOMNode *block)
{
	gboolean before_node = TRUE;
	gint word_wrap_length = selection->priv->word_wrap_length;
	gint level, width;
	EHTMLEditorSelectionAlignment alignment;
	WebKitDOMElement *element;
	WebKitDOMElement *prev_blockquote = NULL, *next_blockquote = NULL;
	WebKitDOMNode *block_to_process, *node_clone = NULL, *child;

	block_to_process = block;

	alignment = e_html_editor_selection_get_alignment_from_node (block_to_process);

	element = webkit_dom_node_get_parent_element (block_to_process);

	if (!WEBKIT_DOM_IS_HTML_DIV_ELEMENT (element) &&
	    !element_has_class (element, "-x-evo-indented"))
		return;

	element_add_class (WEBKIT_DOM_ELEMENT (block_to_process), "-x-evo-to-unindent");

	level = get_indentation_level (element);
	width = word_wrap_length - SPACES_PER_INDENTATION * level;

	/* Look if we have previous siblings, if so, we have to
	 * create new blockquote that will include them */
	if (webkit_dom_node_get_previous_sibling (block_to_process))
		prev_blockquote = e_html_editor_selection_get_indented_element (
			selection, document, width);

	/* Look if we have next siblings, if so, we have to
	 * create new blockquote that will include them */
	if (webkit_dom_node_get_next_sibling (block_to_process))
		next_blockquote = e_html_editor_selection_get_indented_element (
			selection, document, width);

	/* Copy nodes that are before / after the element that we want to unindent */
	while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)))) {
		if (webkit_dom_node_is_equal_node (child, block_to_process)) {
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
	}

	if (node_clone) {
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

		if (level == 1 && element_has_class (WEBKIT_DOM_ELEMENT (node_clone), "-x-evo-paragraph")) {
			e_html_editor_selection_set_paragraph_style (
				selection, WEBKIT_DOM_ELEMENT (node_clone), word_wrap_length, 0, "");
			element_add_class (
				WEBKIT_DOM_ELEMENT (node_clone),
				get_css_alignment_value_class (alignment));
		}

		/* Insert the unindented element */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			node_clone,
			WEBKIT_DOM_NODE (element),
			NULL);
	} else {
		g_warn_if_reached ();
	}

	/* Insert blockqoute with nodes that were after the element that we want to unindent */
	if (next_blockquote) {
		if (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (next_blockquote))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
				WEBKIT_DOM_NODE (next_blockquote),
				WEBKIT_DOM_NODE (element),
				NULL);
		}
	}

	/* Remove old blockquote */
	remove_node (WEBKIT_DOM_NODE (element));
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
	EHTMLEditorView *view;
	EHTMLEditorViewHistoryEvent *ev = NULL;
	gboolean after_selection_start = FALSE, after_selection_end = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

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

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_INDENT;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
	}

	block = get_parent_indented_block (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (!block)
		block = e_html_editor_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

	while (block && !after_selection_end) {
		gint ii, length;
		WebKitDOMNode *next_block;
		WebKitDOMNodeList *list;

		next_block = webkit_dom_node_get_next_sibling (block);

		list = webkit_dom_element_query_selector_all (
			WEBKIT_DOM_ELEMENT (block),
			".-x-evo-indented > *:not(.-x-evo-indented):not(li)",
			NULL);

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		length = webkit_dom_node_list_get_length (list);
		if (length == 0 && node_is_list_or_item (block)) {
			unindent_list (selection, document);
			goto next;
		}

		if (length == 0) {
			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block, WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start)
					goto next;
			}

			unindent_block (selection, document, block);

			if (after_selection_end)
				goto next;
		}

		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *block_to_process;

			block_to_process = webkit_dom_node_list_item (list, ii);

			after_selection_end = webkit_dom_node_contains (
				block_to_process,
				WEBKIT_DOM_NODE (selection_end_marker));

			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block_to_process,
					WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start) {
					g_object_unref (block_to_process);
					continue;
				}
			}

			unindent_block (selection, document, block_to_process);

			if (after_selection_end) {
				g_object_unref (block_to_process);
				break;
			}

			g_object_unref (block_to_process);
		}
 next:
		g_object_unref (list);
		block = next_block;
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
	e_html_editor_view_force_spell_check_for_current_paragraph (view);

	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "indented");
}

typedef gboolean (*IsRightFormatNodeFunc) (WebKitDOMElement *element);

static gboolean
is_bold_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	if (element_has_tag (element, "b"))
		return TRUE;

	/* Headings are bold by default */
	return WEBKIT_DOM_IS_HTML_HEADING_ELEMENT (element);
}

static gboolean
html_editor_selection_is_font_format (EHTMLEditorSelection *selection,
                                      IsRightFormatNodeFunc func,
                                      gboolean *previous_value)
{
	EHTMLEditorView *view;
	gboolean ret_val = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMNode *start, *end, *sibling;
	WebKitDOMRange *range = NULL;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	if (!e_html_editor_view_get_html_mode (view))
		goto out;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection))
		goto out;

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	if (!range)
		goto out;

	if (webkit_dom_range_get_collapsed (range, NULL) && previous_value) {
		WebKitDOMNode *node;
		gchar* text_content;

		node = webkit_dom_range_get_common_ancestor_container (range, NULL);
		/* If we are changing the format of block we have to re-set the
		 * format property, otherwise it will be turned off because of no
		 * text in block. */
		text_content = webkit_dom_node_get_text_content (node);
		if (g_strcmp0 (text_content, "") == 0) {
			g_free (text_content);
			ret_val = *previous_value;
			goto out;
		}
		g_free (text_content);
	}

	/* Range without start or end point is a wrong range. */
	start = webkit_dom_range_get_start_container (range, NULL);
	end = webkit_dom_range_get_end_container (range, NULL);
	if (!start || !end)
		goto out;

	if (WEBKIT_DOM_IS_TEXT (start))
		start = webkit_dom_node_get_parent_node (start);
	while (start && WEBKIT_DOM_IS_ELEMENT (start) && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (start)) {
		/* Find the start point's parent node with given formatting. */
		if (func (WEBKIT_DOM_ELEMENT (start))) {
			ret_val = TRUE;
			break;
		}
		start = webkit_dom_node_get_parent_node (start);
	}

	/* Start point doesn't have the given formatting. */
	if (!ret_val)
		goto out;

	/* If the selection is collapsed, we can return early. */
	if (webkit_dom_range_get_collapsed (range, NULL))
		goto out;

	/* The selection is in the same node and that node is supposed to have
	 * the same formatting (otherwise it is split up with formatting element. */
	if (webkit_dom_node_is_same_node (
		webkit_dom_range_get_start_container (range, NULL),
		webkit_dom_range_get_end_container (range, NULL)))
		goto out;

	ret_val = FALSE;

	if (WEBKIT_DOM_IS_TEXT (end))
		end = webkit_dom_node_get_parent_node (end);
	while (end && WEBKIT_DOM_IS_ELEMENT (end) && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (end)) {
		/* Find the end point's parent node with given formatting. */
		if (func (WEBKIT_DOM_ELEMENT (end))) {
			ret_val = TRUE;
			break;
		}
		end = webkit_dom_node_get_parent_node (end);
	}

	if (!ret_val)
		goto out;

	ret_val = FALSE;

	/* Now go between the end points and check the inner nodes for format validity. */
	sibling = start;
	while ((sibling = webkit_dom_node_get_next_sibling (sibling))) {
		if (webkit_dom_node_is_same_node (sibling, end)) {
			ret_val = TRUE;
			goto out;
		}

		if (WEBKIT_DOM_IS_TEXT (sibling))
			goto out;
		else if (func (WEBKIT_DOM_ELEMENT (sibling)))
			continue;
		else if (webkit_dom_node_get_first_child (sibling)) {
			WebKitDOMNode *first_child;

			first_child = webkit_dom_node_get_first_child (sibling);
			if (!webkit_dom_node_get_next_sibling (first_child))
				if (WEBKIT_DOM_IS_ELEMENT (first_child) && func (WEBKIT_DOM_ELEMENT (first_child)))
					continue;
				else
					goto out;
			else
				goto out;
		} else
			goto out;
	}

	sibling = end;
	while ((sibling = webkit_dom_node_get_previous_sibling (sibling))) {
		if (webkit_dom_node_is_same_node (sibling, start))
			break;

		if (WEBKIT_DOM_IS_TEXT (sibling))
			goto out;
		else if (func (WEBKIT_DOM_ELEMENT (sibling)))
			continue;
		else if (webkit_dom_node_get_first_child (sibling)) {
			WebKitDOMNode *first_child;

			first_child = webkit_dom_node_get_first_child (sibling);
			if (!webkit_dom_node_get_next_sibling (first_child))
				if (WEBKIT_DOM_IS_ELEMENT (first_child) && func (WEBKIT_DOM_ELEMENT (first_child)))
					continue;
				else
					goto out;
			else
				goto out;
		} else
			goto out;
	}

	ret_val = TRUE;
 out:
	g_object_unref (view);
	g_clear_object (&range);
	g_clear_object (&dom_window);
	g_clear_object (&dom_selection);

	return ret_val;
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
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	selection->priv->is_bold = html_editor_selection_is_font_format (
		selection, (IsRightFormatNodeFunc) is_bold_element, &selection->priv->is_bold);

	return selection->priv->is_bold;
}

static void
html_editor_selection_set_font_style (EHTMLEditorSelection *selection,
                                      EHTMLEditorViewCommand command,
                                      gboolean value)
{
	EHTMLEditorView *view;
	EHTMLEditorViewHistoryEvent *ev = NULL;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	e_html_editor_selection_save (selection);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		if (command == E_HTML_EDITOR_VIEW_COMMAND_BOLD)
			ev->type = HISTORY_BOLD;
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_ITALIC)
			ev->type = HISTORY_ITALIC;
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE)
			ev->type = HISTORY_UNDERLINE;
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH)
			ev->type = HISTORY_STRIKETHROUGH;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
		ev->data.style.from = !value;
		ev->data.style.to = value;
	}

	if (e_html_editor_selection_is_collapsed (selection)) {
		WebKitDOMDocument *document;
		const gchar *element_name = NULL;

		if (command == E_HTML_EDITOR_VIEW_COMMAND_BOLD)
			element_name = "b";
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_ITALIC)
			element_name = "i";
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE)
			element_name = "u";
		else if (command == E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH)
			element_name = "strike";

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		if (element_name)
			set_font_style (document, element_name, value);
		e_html_editor_selection_restore (selection);

		goto exit;
	}
	e_html_editor_selection_restore (selection);

	e_html_editor_view_exec_command (view, command, NULL);
 exit:
	if (ev) {
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}

	e_html_editor_view_force_spell_check_for_current_paragraph (view);

	g_object_unref (view);
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_bold (selection) == bold)
		return;

	selection->priv->is_bold = bold;

	html_editor_selection_set_font_style (
		selection, E_HTML_EDITOR_VIEW_COMMAND_BOLD, bold);

	g_object_notify (G_OBJECT (selection), "bold");
}

static gboolean
is_italic_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "i") || element_has_tag (element, "address");
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
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	selection->priv->is_italic = html_editor_selection_is_font_format (
		selection, (IsRightFormatNodeFunc) is_italic_element, &selection->priv->is_italic);

	return selection->priv->is_italic;
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_italic (selection) == italic)
		return;

	selection->priv->is_italic = italic;

	html_editor_selection_set_font_style (
		selection, E_HTML_EDITOR_VIEW_COMMAND_ITALIC, italic);

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
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	selection->priv->is_monospaced = html_editor_selection_is_font_format (
		selection, (IsRightFormatNodeFunc) is_monospaced_element, &selection->priv->is_monospaced);

	return selection->priv->is_monospaced;
}

static void
monospace_selection (EHTMLEditorSelection *selection,
                     WebKitDOMDocument *document,
                     WebKitDOMElement *monospaced_element)
{
	gboolean selection_end = FALSE;
	gboolean first = TRUE;
	gint length, ii;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *sibling, *node, *monospace, *block;
	WebKitDOMNodeList *list;

	e_html_editor_selection_save (selection);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	block = WEBKIT_DOM_NODE (get_parent_block_element (WEBKIT_DOM_NODE (selection_start_marker)));

	monospace = WEBKIT_DOM_NODE (monospaced_element);
	node = WEBKIT_DOM_NODE (selection_start_marker);
	/* Go through first block in selection. */
	while (block && node && !webkit_dom_node_is_same_node (block, node)) {
		if (webkit_dom_node_get_next_sibling (node)) {
			/* Prepare the monospaced element. */
			monospace = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (node),
				first ? monospace : webkit_dom_node_clone_node (monospace, FALSE),
				first ? node : webkit_dom_node_get_next_sibling (node),
				NULL);
		} else
			break;

		/* Move the nodes into monospaced element. */
		while (((sibling = webkit_dom_node_get_next_sibling (monospace)))) {
			webkit_dom_node_append_child (monospace, sibling, NULL);
			if (webkit_dom_node_is_same_node (WEBKIT_DOM_NODE (selection_end_marker), sibling)) {
				selection_end = TRUE;
				break;
			}
		}

		node = webkit_dom_node_get_parent_node (monospace);
		first = FALSE;
	}

	/* Just one block was selected. */
	if (selection_end)
		goto out;

	/* Middle blocks (blocks not containing the end of the selection. */
	block = webkit_dom_node_get_next_sibling (block);
	while (block && !selection_end) {
		WebKitDOMNode *next_block;

		selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		if (selection_end)
			break;

		next_block = webkit_dom_node_get_next_sibling (block);

		monospace = webkit_dom_node_insert_before (
			block,
			webkit_dom_node_clone_node (monospace, FALSE),
			webkit_dom_node_get_first_child (block),
			NULL);

		while (((sibling = webkit_dom_node_get_next_sibling (monospace))))
			webkit_dom_node_append_child (monospace, sibling, NULL);

		block = next_block;
	}

	/* Block containing the end of selection. */
	node = WEBKIT_DOM_NODE (selection_end_marker);
	while (block && node && !webkit_dom_node_is_same_node (block, node)) {
		monospace = webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (node),
			webkit_dom_node_clone_node (monospace, FALSE),
			webkit_dom_node_get_next_sibling (node),
			NULL);

		while (((sibling = webkit_dom_node_get_previous_sibling (monospace)))) {
			webkit_dom_node_insert_before (
				monospace,
				sibling,
				webkit_dom_node_get_first_child (monospace),
				NULL);
		}

		node = webkit_dom_node_get_parent_node (monospace);
	}
 out:
	/* Merge all the monospace elements inside other monospace elements. */
	list = webkit_dom_document_query_selector_all (
		document, "font[face=monospace] > font[face=monospace]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *item;
		WebKitDOMNode *child;

		item = webkit_dom_node_list_item (list, ii);
		while ((child = webkit_dom_node_get_first_child (item))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (item),
				child,
				item,
				NULL);
		}
		remove_node (item);
		g_object_unref (item);
	}
	g_object_unref (list);

	/* Merge all the adjacent monospace elements. */
	list = webkit_dom_document_query_selector_all (
		document, "font[face=monospace] + font[face=monospace]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *item;
		WebKitDOMNode *child;

		item = webkit_dom_node_list_item (list, ii);
		/* The + CSS selector will return some false positives as it doesn't
		 * take text between elements into account so it will return this:
		 * <font face="monospace">xx</font>yy<font face="monospace">zz</font>
		 * as valid, but it isn't so we have to check if previous node
		 * is indeed element or not. */
		if (WEBKIT_DOM_IS_ELEMENT (webkit_dom_node_get_previous_sibling (item))) {
			while ((child = webkit_dom_node_get_first_child (item))) {
				webkit_dom_node_append_child (
					webkit_dom_node_get_previous_sibling (item), child, NULL);
			}
			remove_node (item);
		}
		g_object_unref (item);
	}
	g_object_unref (list);

	e_html_editor_selection_restore (selection);
}

static void
unmonospace_selection (EHTMLEditorSelection *selection,
                       WebKitDOMDocument *document)
{
	WebKitDOMElement *selection_start_marker;
	WebKitDOMElement *selection_end_marker;
	WebKitDOMElement *selection_start_clone;
	WebKitDOMElement *selection_end_clone;
	WebKitDOMNode *sibling, *node;
	gboolean selection_end = FALSE;
	WebKitDOMNode *block, *clone, *monospace;

	e_html_editor_selection_save (selection);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	block = WEBKIT_DOM_NODE (get_parent_block_element (WEBKIT_DOM_NODE (selection_start_marker)));

	node = WEBKIT_DOM_NODE (selection_start_marker);
	monospace = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	while (monospace && !is_monospaced_element (WEBKIT_DOM_ELEMENT (monospace)))
		monospace = webkit_dom_node_get_parent_node (monospace);

	/* No monospaced element was found as a parent of selection start node. */
	if (!monospace)
		goto out;

	/* Make a clone of current monospaced element. */
	clone = webkit_dom_node_clone_node (monospace, TRUE);

	/* First block */
	/* Remove all the nodes that are after the selection start point as they
	 * will be in the cloned node. */
	while (monospace && node && !webkit_dom_node_is_same_node (monospace, node)) {
		WebKitDOMNode *tmp;
		while (((sibling = webkit_dom_node_get_next_sibling (node))))
			remove_node (sibling);

		tmp = webkit_dom_node_get_parent_node (node);
		if (webkit_dom_node_get_next_sibling (node))
			remove_node (node);
		node = tmp;
	}

	selection_start_clone = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (clone), "#-x-evo-selection-start-marker", NULL);
	selection_end_clone = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (clone), "#-x-evo-selection-end-marker", NULL);

	/* No selection start node in the block where it is supposed to be, return. */
	if (!selection_start_clone)
		goto out;

	/* Remove all the nodes until we hit the selection start point as these
	 * nodes will stay monospaced and they are already in original element. */
	node = webkit_dom_node_get_first_child (clone);
	while (node) {
		WebKitDOMNode *next_sibling;

		next_sibling = webkit_dom_node_get_next_sibling (node);
		if (webkit_dom_node_get_first_child (node)) {
			if (webkit_dom_node_contains (node, WEBKIT_DOM_NODE (selection_start_clone))) {
				node = webkit_dom_node_get_first_child (node);
				continue;
			} else
				remove_node (node);
		} else if (webkit_dom_node_is_same_node (node, WEBKIT_DOM_NODE (selection_start_clone)))
			break;
		else
			remove_node (node);

		node = next_sibling;
	}

	/* Insert the clone into the tree. Do it after the previous clean up. If
	 * we would do it the other way the line would contain duplicated text nodes
	 * and the block would be expading and shrinking while we would modify it. */
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (monospace),
		clone,
		webkit_dom_node_get_next_sibling (monospace),
		NULL);

	/* Move selection start point the right place. */
	remove_node (WEBKIT_DOM_NODE (selection_start_marker));
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (clone),
		WEBKIT_DOM_NODE (selection_start_clone),
		clone,
		NULL);

	/* Move all the nodes the are supposed to lose the monospace formatting
	 * out of monospaced element. */
	node = webkit_dom_node_get_first_child (clone);
	while (node) {
		WebKitDOMNode *next_sibling;

		next_sibling = webkit_dom_node_get_next_sibling (node);
		if (webkit_dom_node_get_first_child (node)) {
			if (selection_end_clone &&
			    webkit_dom_node_contains (node, WEBKIT_DOM_NODE (selection_end_clone))) {
				node = webkit_dom_node_get_first_child (node);
				continue;
			} else
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (clone),
					node,
					clone,
					NULL);
		} else if (selection_end_clone &&
			   webkit_dom_node_is_same_node (node, WEBKIT_DOM_NODE (selection_end_clone))) {
			selection_end = TRUE;
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);
			break;
		} else
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);

		node = next_sibling;
	}

	if (!webkit_dom_node_get_first_child (clone))
		remove_node (clone);

	/* Just one block was selected and we hit the selection end point. */
	if (selection_end)
		goto out;

	/* Middle blocks */
	block = webkit_dom_node_get_next_sibling (block);
	while (block && !selection_end) {
		WebKitDOMNode *next_block, *child, *parent;
		WebKitDOMElement *monospaced_element;

		selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		if (selection_end)
			break;

		next_block = webkit_dom_node_get_next_sibling (block);

		/* Find the monospaced element and move all the nodes from it and
		 * finally remove it. */
		monospaced_element = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), "font[face=monospace]", NULL);
		if (!monospaced_element)
			break;

		parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (monospaced_element));
		while  ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (monospaced_element)))) {
			webkit_dom_node_insert_before (
				parent, child, WEBKIT_DOM_NODE (monospaced_element), NULL);
		}

		remove_node (WEBKIT_DOM_NODE (monospaced_element));

		block = next_block;
	}

	/* End block */
	node = WEBKIT_DOM_NODE (selection_end_marker);
	monospace = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_end_marker));
	while (monospace && !is_monospaced_element (WEBKIT_DOM_ELEMENT (monospace)))
		monospace = webkit_dom_node_get_parent_node (monospace);

	/* No monospaced element was found as a parent of selection end node. */
	if (!monospace)
		return;

	clone = WEBKIT_DOM_NODE (monospace);
	node = webkit_dom_node_get_first_child (clone);
	/* Move all the nodes that are supposed to lose the monospaced formatting
	 * out of the monospaced element. */
	while (node) {
		WebKitDOMNode *next_sibling;

		next_sibling = webkit_dom_node_get_next_sibling (node);
		if (webkit_dom_node_get_first_child (node)) {
			if (webkit_dom_node_contains (node, WEBKIT_DOM_NODE (selection_end_marker))) {
				node = webkit_dom_node_get_first_child (node);
				continue;
			} else
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (clone),
					node,
					clone,
					NULL);
		} else if (webkit_dom_node_is_same_node (node, WEBKIT_DOM_NODE (selection_end_marker))) {
			selection_end = TRUE;
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);
			break;
		} else {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);
		}

		node = next_sibling;
	}

	if (!webkit_dom_node_get_first_child (clone))
		remove_node (clone);
 out:
	e_html_editor_selection_restore (selection);
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
	EHTMLEditorViewHistoryEvent *ev = NULL;
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_monospaced (selection) == monospaced)
		return;

	selection->priv->is_monospaced = monospaced;

	range = html_editor_selection_get_current_range (selection);
	if (!range)
		return;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_MONOSPACE;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
		ev->data.style.from = !monospaced;
		ev->data.style.to = monospaced;
	}

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	if (monospaced) {
		guint font_size;
		WebKitDOMElement *monospace;

		monospace = webkit_dom_document_create_element (
			document, "font", NULL);
		webkit_dom_element_set_attribute (
			monospace, "face", "monospace", NULL);

		font_size = selection->priv->font_size;
		if (font_size != 0) {
			gchar *font_size_str;

			font_size_str = g_strdup_printf ("%d", font_size);
			webkit_dom_element_set_attribute (
				monospace, "size", font_size_str, NULL);
			g_free (font_size_str);
		}

		if (!webkit_dom_range_get_collapsed (range, NULL))
			monospace_selection (selection, document, monospace);
		else {
			/* https://bugs.webkit.org/show_bug.cgi?id=15256 */
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (monospace),
				UNICODE_ZERO_WIDTH_SPACE,
				NULL);
			webkit_dom_range_insert_node (
				range, WEBKIT_DOM_NODE (monospace), NULL);

			e_html_editor_selection_move_caret_into_element (
				document, monospace, FALSE);
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
				g_object_unref (range);
				g_object_unref (dom_selection);
				g_object_unref (dom_window);
				g_free (ev);
				return;
			}
		}

		/* Save current formatting */
		is_bold = selection->priv->is_bold;
		is_italic = selection->priv->is_italic;
		is_underline = selection->priv->is_underline;
		is_strikethrough = selection->priv->is_strikethrough;
		font_size = selection->priv->font_size;

		if (!webkit_dom_range_get_collapsed (range, NULL))
			unmonospace_selection (selection, document);
		else {
			e_html_editor_selection_save (selection);
			set_font_style (document, "", FALSE);
			e_html_editor_selection_restore (selection);
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

		if (font_size)
			e_html_editor_selection_set_font_size (selection, font_size);
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

	e_html_editor_view_force_spell_check_for_current_paragraph (view);

	g_object_unref (range);
	g_object_unref (dom_selection);
	g_object_unref (dom_window);
	g_object_unref (view);

	g_object_notify (G_OBJECT (selection), "monospaced");
}

static gboolean
is_strikethrough_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "strike");
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
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	selection->priv->is_strikethrough = html_editor_selection_is_font_format (
		selection, (IsRightFormatNodeFunc) is_strikethrough_element, &selection->priv->is_strikethrough);

	return selection->priv->is_strikethrough;
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_strikethrough (selection) == strikethrough)
		return;

	selection->priv->is_strikethrough = strikethrough;

	html_editor_selection_set_font_style (
		selection, E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH, strikethrough);

	g_object_notify (G_OBJECT (selection), "strikethrough");
}

static gboolean
is_subscript_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "sub");
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
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	return html_editor_selection_is_font_format (
		selection, (IsRightFormatNodeFunc) is_subscript_element, NULL);
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

static gboolean
is_superscript_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "sup");
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
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	return html_editor_selection_is_font_format (
		selection, (IsRightFormatNodeFunc) is_superscript_element, NULL);
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

static gboolean
is_underline_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "u");
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
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	selection->priv->is_underline = html_editor_selection_is_font_format (
		selection, (IsRightFormatNodeFunc) is_underline_element, &selection->priv->is_underline);

	return selection->priv->is_underline;
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	if (e_html_editor_selection_is_underline (selection) == underline)
		return;

	selection->priv->is_underline = underline;

	html_editor_selection_set_font_style (
		selection, E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE, underline);

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
	gchar *text;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *link;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	link = e_html_editor_dom_node_find_parent_element (
			webkit_dom_range_get_start_container (range, NULL), "A");

	g_object_unref (dom_selection);
	g_object_unref (dom_window);

	if (!link) {
		WebKitDOMNode *node;

		/* get element that was clicked on */
		node = webkit_dom_range_get_common_ancestor_container (range, NULL);
		if (node && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
			link = e_html_editor_dom_node_find_parent_element (node, "A");
			if (link && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (link)) {
				g_object_unref (range);
				return;
			}
		} else
			link = WEBKIT_DOM_ELEMENT (node);
	}

	g_object_unref (range);

	if (!link)
		return;

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		EHTMLEditorViewHistoryEvent *ev;
		WebKitDOMDocumentFragment *fragment;

		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_REMOVE_LINK;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		fragment = webkit_dom_document_create_document_fragment (document);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (link), TRUE),
			NULL);
		ev->data.fragment = fragment;

		e_html_editor_view_insert_new_history_event (view, ev);
	}

	text = webkit_dom_html_element_get_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (link));
	webkit_dom_html_element_set_outer_html (
		WEBKIT_DOM_HTML_ELEMENT (link), text, NULL);

	g_free (text);
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
	EHTMLEditorViewHistoryEvent *ev = NULL;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (plain_text != NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		gboolean collapsed;

		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_PASTE;

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
		ev->data.string.to = g_strdup (plain_text);
	}

	e_html_editor_view_convert_and_insert_plain_text (view, plain_text);

	if (ev) {
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}

	e_html_editor_view_set_changed (view, TRUE);

	g_object_unref (view);
}

static gboolean
pasting_quoted_content (const gchar *content)
{
	/* Check if the content we are pasting is a quoted content from composer.
	 * If it is, we can't use WebKit to paste it as it would leave the formatting
	 * on the content. */
	return g_str_has_prefix (
		content,
		"<meta http-equiv=\"content-type\" content=\"text/html; "
		"charset=utf-8\"><blockquote type=\"cite\"") &&
		strstr (content, "\"-x-evo-");
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
	EHTMLEditorViewHistoryEvent *ev = NULL;
	gboolean html_mode;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (html_text != NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		gboolean collapsed;

		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_INSERT_HTML;

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
		ev->data.string.to = g_strdup (html_text);
	}

	html_mode = e_html_editor_view_get_html_mode (view);
	command = E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML;
	if (html_mode ||
	    (e_html_editor_view_is_pasting_content_from_itself (view) &&
	     !pasting_quoted_content (html_text))) {
		if (!e_html_editor_selection_is_collapsed (selection)) {
			EHTMLEditorViewHistoryEvent *event;
			WebKitDOMDocumentFragment *fragment;
			WebKitDOMRange *range;

			event = g_new0 (EHTMLEditorViewHistoryEvent, 1);
			event->type = HISTORY_DELETE;

			range = html_editor_selection_get_current_range (selection);
			fragment = webkit_dom_range_clone_contents (range, NULL);
			g_object_unref (range);
			event->data.fragment = fragment;

			e_html_editor_selection_get_selection_coordinates (
				selection,
				&event->before.start.x,
				&event->before.start.y,
				&event->before.end.x,
				&event->before.end.y);

			event->after.start.x = event->before.start.x;
			event->after.start.y = event->before.start.y;
			event->after.end.x = event->before.start.x;
			event->after.end.y = event->before.start.y;

			e_html_editor_view_insert_new_history_event (view, event);

			event = g_new0 (EHTMLEditorViewHistoryEvent, 1);
			event->type = HISTORY_AND;

			e_html_editor_view_insert_new_history_event (view, event);
		}

		e_html_editor_view_exec_command (view, command, html_text);
		e_html_editor_view_fix_file_uri_images (view);
		if (strstr (html_text, "id=\"-x-evo-selection-start-marker\""))
			e_html_editor_selection_restore (selection);

		if (!html_mode) {
			WebKitDOMDocument *document;
			WebKitDOMNodeList *list;
			gint ii, length;

			document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
			list = webkit_dom_document_query_selector_all (
				document, "span[style^=font-family]", NULL);
			length = webkit_dom_node_list_get_length (list);
			if (length > 0)
				e_html_editor_selection_save (selection);

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
				e_html_editor_selection_restore (selection);
		}

		e_html_editor_view_check_magic_links (view, FALSE);
		e_html_editor_view_force_spell_check_in_viewport (view);

		e_html_editor_selection_scroll_to_caret (selection);
	} else
		e_html_editor_view_convert_and_insert_html_to_plain_text (
			view, html_text);

	if (ev) {
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}

	e_html_editor_view_set_changed (view, TRUE);

	g_object_unref (view);
}

void
e_html_editor_selection_insert_as_text (EHTMLEditorSelection *selection,
                                        const gchar *html_text)
{
	EHTMLEditorView *view;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (html_text != NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	e_html_editor_view_convert_and_insert_html_to_plain_text (view, html_text);

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

	if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (element))
		webkit_dom_html_image_element_set_src (
			WEBKIT_DOM_HTML_IMAGE_ELEMENT (element),
			base64_content);
	else
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (element),
			"background",
			base64_content,
			NULL);
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
	EHTMLEditorViewHistoryEvent *ev = NULL;
	WebKitDOMDocument *document;
	WebKitDOMElement *element, *selection_start_marker, *resizable_wrapper;
	WebKitDOMText *text;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

	e_html_editor_view_set_changed (view, TRUE);

	if (!e_html_editor_selection_is_collapsed (selection)) {
		EHTMLEditorViewHistoryEvent *ev;
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMRange *range;

		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_DELETE;

		range = html_editor_selection_get_current_range (selection);
		fragment = webkit_dom_range_clone_contents (range, NULL);
		g_object_unref (range);
		ev->data.fragment = fragment;

		e_html_editor_selection_get_selection_coordinates (
			selection,
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
		e_html_editor_view_exec_command (
			view, E_HTML_EDITOR_VIEW_COMMAND_DELETE, NULL);
	}

	e_html_editor_selection_save (selection);
	selection_start_marker = webkit_dom_document_query_selector (
		document, "span#-x-evo-selection-start-marker", NULL);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_IMAGE;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
	}

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
			WEBKIT_DOM_NODE (selection_start_marker)),
		WEBKIT_DOM_NODE (resizable_wrapper),
		WEBKIT_DOM_NODE (selection_start_marker),
		NULL);

	/* We have to again use UNICODE_ZERO_WIDTH_SPACE character to restore
	 * caret on right position */
	text = webkit_dom_document_create_text_node (
		document, UNICODE_ZERO_WIDTH_SPACE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (selection_start_marker)),
		WEBKIT_DOM_NODE (text),
		WEBKIT_DOM_NODE (selection_start_marker),
		NULL);

	if (ev) {
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMNode *node;

		fragment = webkit_dom_document_create_document_fragment (document);
		node = webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node (WEBKIT_DOM_NODE (resizable_wrapper), TRUE),
			NULL);
		webkit_dom_html_element_insert_adjacent_html (
			WEBKIT_DOM_HTML_ELEMENT (node), "afterend", "&#8203;", NULL);
		ev->data.fragment = fragment;
		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}

	e_html_editor_selection_restore (selection);
	e_html_editor_view_force_spell_check_for_current_paragraph (view);

	g_object_unref (view);
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
 * @element: #WebKitDOMElement element
 * @image_uri: an URI of the source image
 *
 * If given @element is image we will replace the src attribute of it with base64
 * data from given @image_uri. Otherwise we will set the base64 data to
 * the background attribute of given @element.
 */
void
e_html_editor_selection_replace_image_src (EHTMLEditorSelection *selection,
                                           WebKitDOMElement *element,
                                           const gchar *image_uri)
{
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (image_uri != NULL);
	g_return_if_fail (element && WEBKIT_DOM_IS_ELEMENT (element));

	if (strstr (image_uri, ";base64,")) {
		if (g_str_has_prefix (image_uri, "data:"))
			replace_base64_image_src (
				selection, element, image_uri, "", "");
		if (strstr (image_uri, ";data")) {
			const gchar *base64_data = strstr (image_uri, ";") + 1;
			gchar *filename;
			glong filename_length;

			filename_length =
				g_utf8_strlen (image_uri, -1) -
				g_utf8_strlen (base64_data, -1) - 1;
			filename = g_strndup (image_uri, filename_length);

			replace_base64_image_src (
				selection, element, base64_data, filename, "");
			g_free (filename);
		}
	} else
		image_load_and_insert_async (selection, element, image_uri);
}

void
e_html_editor_selection_move_caret_into_element (WebKitDOMDocument *document,
                                                 WebKitDOMElement *element,
                                                 gboolean to_start)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *new_range;

	if (!element)
		return;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	new_range = webkit_dom_document_create_range (document);

	webkit_dom_range_select_node_contents (
		new_range, WEBKIT_DOM_NODE (element), NULL);
	webkit_dom_range_collapse (new_range, to_start, NULL);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, new_range);
	g_object_unref (new_range);
	g_object_unref (dom_selection);
	g_object_unref (dom_window);
}

static gint
find_where_to_break_line (WebKitDOMCharacterData *node,
                          gint max_length)
{
	gboolean last_break_position_is_dash = FALSE;
	gchar *str, *text_start;
	gunichar uc;
	gint pos = 1, last_break_position = 0, ret_val = 0;

	text_start = webkit_dom_character_data_get_data (node);

	str = text_start;
	do {
		uc = g_utf8_get_char (str);
		if (!uc) {
			ret_val = pos <= max_length ? pos : last_break_position > 0 ? last_break_position - 1 : 0;
			goto out;
		}

		if ((g_unichar_isspace (uc) && !(g_unichar_break_type (uc) == G_UNICODE_BREAK_NON_BREAKING_GLUE)) ||
		     *str == '-') {
			if ((last_break_position_is_dash = *str == '-')) {
				/* There was no space before the dash */
				if (pos - 1 != last_break_position) {
					gchar *rest;

					rest = g_utf8_next_char (str);
					if (rest && *rest) {
						gunichar next_char;

						/* There is no space after the dash */
						next_char = g_utf8_get_char (rest);
						if (g_unichar_isspace (next_char))
							last_break_position_is_dash = FALSE;
						else
							last_break_position = pos;
					} else
						last_break_position_is_dash = FALSE;
				} else
					last_break_position_is_dash = FALSE;
			} else
				last_break_position = pos;
		}

		if ((pos == max_length)) {
			/* Look one character after the limit to check if there
			 * is a space (skip dash) that we are allowed to break at, if so
			 * break it there. */
			if (*str) {
				str = g_utf8_next_char (str);
				uc = g_utf8_get_char (str);

				if ((g_unichar_isspace (uc) &&
				    !(g_unichar_break_type (uc) == G_UNICODE_BREAK_NON_BREAKING_GLUE)))
					last_break_position = ++pos;
			}
			break;
		}

		pos++;
		str = g_utf8_next_char (str);
	} while (*str);

	if (last_break_position != 0)
		ret_val = last_break_position - 1;
 out:
	g_free (text_start);

	/* Always break after the dash character. */
	if (last_break_position_is_dash)
		ret_val++;

	/* No character to break at is found. We should split at max_length, but
	 * we will leave the decision on caller as it depends on context. */
	if (ret_val == 0 && last_break_position == 0)
		ret_val = -1;

	return ret_val;
}

static void
mark_and_remove_trailing_space (WebKitDOMDocument *document,
                                WebKitDOMNode *node)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_attribute (element, "data-hidden-space", "", NULL);
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (node),
		WEBKIT_DOM_NODE (element),
		webkit_dom_node_get_next_sibling (node),
		NULL);
	webkit_dom_character_data_replace_data (
		WEBKIT_DOM_CHARACTER_DATA (node),
		webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node)),
		1,
		"",
		NULL);
}

static void
mark_and_remove_leading_space (WebKitDOMDocument *document,
                               WebKitDOMNode *node)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_attribute (element, "data-hidden-space", "", NULL);
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (node),
		WEBKIT_DOM_NODE (element),
		node,
		NULL);
	webkit_dom_character_data_replace_data (
		WEBKIT_DOM_CHARACTER_DATA (node), 0, 1, "", NULL);
}

static WebKitDOMElement *
wrap_lines (EHTMLEditorSelection *selection,
            WebKitDOMNode *block,
            WebKitDOMDocument *document,
            gboolean remove_all_br,
            gint length_to_wrap,
            gint word_wrap_length)
{
	WebKitDOMNode *node, *start_node, *block_clone;
	guint line_length;
	gulong length_left;
	gchar *text_content;
	gboolean compensated = FALSE;
	gboolean check_next_node = FALSE;

	if (selection) {
		gint ii, length;
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMNodeList *list;
		WebKitDOMRange *range;

		range = html_editor_selection_get_current_range (selection);
		fragment = webkit_dom_range_clone_contents (range, NULL);
		g_object_unref (range);

		/* Select all BR elements or just ours that are used for wrapping.
		 * We are not removing user BR elements when this function is activated
		 * from Format->Wrap Lines action */
		list = webkit_dom_document_fragment_query_selector_all (
			fragment,
			remove_all_br ? "br" : "br.-x-evo-wrap-br",
			NULL);
		length = webkit_dom_node_list_get_length (list);
		/* And remove them */
		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *node = webkit_dom_node_list_item (list, length);
			remove_node (node);
			g_object_unref (node);
		}
		g_object_unref (list);

		list = webkit_dom_document_fragment_query_selector_all (
			fragment, "span[data-hidden-space]", NULL);
		length = webkit_dom_node_list_get_length (list);
		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *hidden_space_node;

			hidden_space_node = webkit_dom_node_list_item (list, ii);
			webkit_dom_html_element_set_outer_text (
				WEBKIT_DOM_HTML_ELEMENT (hidden_space_node), " ", NULL);
			g_object_unref (hidden_space_node);
		}
		g_object_unref (list);

		node = WEBKIT_DOM_NODE (fragment);
		start_node = node;
	} else {
		WebKitDOMElement *selection_start_marker, *selection_end_marker;
		WebKitDOMNode *start_point = NULL, *first_child;

		if (!webkit_dom_node_has_child_nodes (block))
			return WEBKIT_DOM_ELEMENT (block);

		/* Avoid wrapping when the block contains just the BR element alone
		 * or with selection markers. */
		if ((first_child = webkit_dom_node_get_first_child (block)) &&
		     WEBKIT_DOM_IS_HTMLBR_ELEMENT (first_child)) {
			WebKitDOMNode *next_sibling;

			if ((next_sibling = webkit_dom_node_get_next_sibling (first_child))) {
			       if (e_html_editor_node_is_selection_position_node (next_sibling) &&
				   (next_sibling = webkit_dom_node_get_next_sibling (next_sibling)) &&
				   e_html_editor_node_is_selection_position_node (next_sibling) &&
				   !webkit_dom_node_get_next_sibling (next_sibling))
					return WEBKIT_DOM_ELEMENT (block);
			} else
				return WEBKIT_DOM_ELEMENT (block);
		}

		block_clone = webkit_dom_node_clone_node (block, TRUE);
		/* When we wrap, we are wrapping just the text after caret, text
		 * before the caret is already wrapped, so unwrap the text after
		 * the caret position */
		selection_end_marker = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block_clone),
			"span#-x-evo-selection-end-marker",
			NULL);

		if (selection_end_marker) {
			WebKitDOMNode *nd = WEBKIT_DOM_NODE (selection_end_marker);

			while (nd) {
				WebKitDOMNode *parent_node;
				WebKitDOMNode *next_nd = webkit_dom_node_get_next_sibling (nd);

				parent_node = webkit_dom_node_get_parent_node (nd);
				if (!next_nd && parent_node && !webkit_dom_node_is_same_node (parent_node, block_clone))
					next_nd = webkit_dom_node_get_next_sibling (parent_node);

				if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (nd)) {
					if (remove_all_br)
						remove_node (nd);
					else if (element_has_class (WEBKIT_DOM_ELEMENT (nd), "-x-evo-wrap-br"))
						remove_node (nd);
				} else if (WEBKIT_DOM_IS_ELEMENT (nd) &&
				           webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (nd), "data-hidden-space"))
					webkit_dom_html_element_set_outer_text (
						WEBKIT_DOM_HTML_ELEMENT (nd), " ", NULL);

				nd = next_nd;
			}
		} else {
			gint ii, length;
			WebKitDOMNodeList *list;

			list = webkit_dom_element_query_selector_all (
				WEBKIT_DOM_ELEMENT (block_clone), "span[data-hidden-space]", NULL);
			length = webkit_dom_node_list_get_length (list);
			for (ii = 0; ii < length; ii++) {
				WebKitDOMNode *hidden_space_node;

				hidden_space_node = webkit_dom_node_list_item (list, ii);
				webkit_dom_html_element_set_outer_text (
					WEBKIT_DOM_HTML_ELEMENT (hidden_space_node), " ", NULL);
				g_object_unref (hidden_space_node);
			}
			g_object_unref (list);
		}

		/* We have to start from the end of the last wrapped line */
		selection_start_marker = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block_clone),
			"span#-x-evo-selection-start-marker",
			NULL);

		if (selection_start_marker) {
			gboolean first_removed = FALSE;
			WebKitDOMNode *nd;

			nd = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (selection_start_marker));
			while (nd) {
				WebKitDOMNode *prev_nd = webkit_dom_node_get_previous_sibling (nd);

				if (!prev_nd && !webkit_dom_node_is_same_node (webkit_dom_node_get_parent_node (nd), block_clone))
					prev_nd = webkit_dom_node_get_previous_sibling (webkit_dom_node_get_parent_node (nd));

				if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (nd)) {
					if (first_removed) {
						start_point = nd;
						break;
					} else {
						remove_node (nd);
						first_removed = TRUE;
					}
				} else if (WEBKIT_DOM_IS_ELEMENT (nd) &&
				           webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (nd), "data-hidden-space")) {
					webkit_dom_html_element_set_outer_text (
						WEBKIT_DOM_HTML_ELEMENT (nd), " ", NULL);
				} else if (!prev_nd) {
					start_point = nd;
				}

				nd = prev_nd;
			}
		}

		webkit_dom_node_normalize (block_clone);
		node = webkit_dom_node_get_first_child (block_clone);
		if (node) {
			text_content = webkit_dom_node_get_text_content (node);
			if (g_strcmp0 ("\n", text_content) == 0)
				node = webkit_dom_node_get_next_sibling (node);
			g_free (text_content);
		}

		if (start_point) {
			if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (start_point))
				node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (start_point));
			else
				node = start_point;
			start_node = block_clone;
		} else
			start_node = node;
	}

	line_length = 0;
	while (node) {
		gint offset = 0;
		WebKitDOMElement *element;

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
			newline = strstr (text_content, "\n");

			next_sibling = node;
			while (newline) {
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
				newline = strstr (text_content, "\n");
			}
			g_free (text_content);
		} else {
			if (e_html_editor_node_is_selection_position_node (node)) {
				if (line_length == 0) {
					WebKitDOMNode *tmp_node;

					tmp_node = webkit_dom_node_get_previous_sibling (node);
					/* Only check if there is some node before the selection marker. */
					if (tmp_node && !e_html_editor_node_is_selection_position_node (tmp_node))
						check_next_node = TRUE;
				}
				node = webkit_dom_node_get_next_sibling (node);
				continue;
			}

			check_next_node = FALSE;
			/* If element is ANCHOR we wrap it separately */
			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
				glong anchor_length;
				WebKitDOMNode *next_sibling;

				text_content = webkit_dom_node_get_text_content (node);
				anchor_length = g_utf8_strlen (text_content, -1);
				g_free (text_content);

				next_sibling = webkit_dom_node_get_next_sibling (node);
				/* If the anchor doesn't fit on the line move the inner
				 * nodes out of it and start to wrap them. */
				if ((line_length + anchor_length) > length_to_wrap) {
					WebKitDOMNode *inner_node;

					while ((inner_node = webkit_dom_node_get_first_child (node))) {
						g_object_set_data (
							G_OBJECT (inner_node),
							"-x-evo-anchor-text",
							GINT_TO_POINTER (1));
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (node),
							inner_node,
							next_sibling,
							NULL);
					}
					next_sibling = webkit_dom_node_get_next_sibling (node);
					remove_node (node);
					node = next_sibling;
					continue;
				}

				line_length += anchor_length;
				node = next_sibling;
				continue;
			}

			if (element_has_class (WEBKIT_DOM_ELEMENT (node), "Apple-tab-span")) {
				WebKitDOMNode *sibling;
				gint tab_length;

				sibling = webkit_dom_node_get_previous_sibling (node);
				if (sibling && WEBKIT_DOM_IS_ELEMENT (sibling) &&
				    element_has_class (WEBKIT_DOM_ELEMENT (sibling), "Apple-tab-span"))
					tab_length = TAB_LENGTH;
				else {
					tab_length = TAB_LENGTH - (line_length + compensated ? 0 : (word_wrap_length - length_to_wrap)) % TAB_LENGTH;
					compensated = TRUE;
				}

				if (line_length + tab_length > length_to_wrap) {
					if (webkit_dom_node_get_next_sibling (node)) {
						element = webkit_dom_document_create_element (
							document, "BR", NULL);
						element_add_class (element, "-x-evo-wrap-br");
						node = webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (node),
							WEBKIT_DOM_NODE (element),
							webkit_dom_node_get_next_sibling (node),
							NULL);
					}
					line_length = 0;
					compensated = FALSE;
				} else
					line_length += tab_length;

				sibling = webkit_dom_node_get_next_sibling (node);
				node = sibling;
				continue;
			}
			/* When we are not removing user-entered BR elements (lines wrapped by user),
			 * we need to skip those elements */
			if (!remove_all_br && WEBKIT_DOM_IS_HTMLBR_ELEMENT (node)) {
				if (!element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br")) {
					line_length = 0;
					compensated = FALSE;
					node = webkit_dom_node_get_next_sibling (node);
					continue;
				}
			}
			goto next_node;
		}

		/* If length of this node + what we already have is still less
		 * then length_to_wrap characters, then just concatenate it and
		 * continue to next node */
		length_left = webkit_dom_character_data_get_length (
			WEBKIT_DOM_CHARACTER_DATA (node));

		if ((length_left + line_length) <= length_to_wrap) {
			if (check_next_node)
				goto check_node;
			line_length += length_left;
			if (line_length == length_to_wrap) {
				line_length = 0;

				element = webkit_dom_document_create_element (document, "BR", NULL);
				element_add_class (element, "-x-evo-wrap-br");

				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (node),
					WEBKIT_DOM_NODE (element),
					webkit_dom_node_get_next_sibling (node),
					NULL);
			}
			goto next_node;
		}

		/* wrap until we have something */
		while (node && (length_left + line_length) > length_to_wrap) {
			gboolean insert_and_continue;
			gint max_length;

 check_node:
			insert_and_continue = FALSE;

			if (!WEBKIT_DOM_IS_CHARACTER_DATA (node))
				goto next_node;

			element = webkit_dom_document_create_element (document, "BR", NULL);
			element_add_class (element, "-x-evo-wrap-br");

			max_length = length_to_wrap - line_length;
			if (max_length < 0)
				max_length = length_to_wrap;
			else if (max_length == 0) {
				if (check_next_node) {
					insert_and_continue = TRUE;
					goto check;
				}

				/* Break before the current node and continue. */
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (node),
					WEBKIT_DOM_NODE (element),
					node,
					NULL);
				line_length = 0;
				continue;
			}

			/* Allow anchors to break on any character. */
			if (g_object_steal_data (G_OBJECT (node), "-x-evo-anchor-text"))
				offset = max_length;
			else {
				/* Find where we can line-break the node so that it
				 * effectively fills the rest of current row. */
				offset = find_where_to_break_line (
					WEBKIT_DOM_CHARACTER_DATA (node), max_length);

				/* When pressing delete on the end of line to concatenate
				 * the last word from the line and first word from the
				 * next line we will end with the second word split
				 * somewhere in the middle (to be precise it will be
				 * split after the last character that will fit on the
				 * previous line. To avoid that we need to put the
				 * concatenated word on the next line. */
				if (offset == -1 || check_next_node) {
					WebKitDOMNode *prev_sibling;

 check:
					check_next_node = FALSE;
					prev_sibling = webkit_dom_node_get_previous_sibling (node);
					if (prev_sibling && e_html_editor_node_is_selection_position_node (prev_sibling)) {
						WebKitDOMNode *prev_br = NULL;

						prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);

						/* Collapsed selection */
						if (prev_sibling && e_html_editor_node_is_selection_position_node (prev_sibling))
							prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);

						if (prev_sibling && WEBKIT_DOM_IS_HTMLBR_ELEMENT (prev_sibling) &&
						    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-wrap-br")) {
							prev_br = prev_sibling;
							prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);
						}

						if (prev_sibling && WEBKIT_DOM_IS_CHARACTER_DATA (prev_sibling)) {
							gchar *data;
							glong text_length, length = 0;

							data = webkit_dom_character_data_get_data (
								WEBKIT_DOM_CHARACTER_DATA (prev_sibling));
							text_length = webkit_dom_character_data_get_length (
								WEBKIT_DOM_CHARACTER_DATA (prev_sibling));

							/* Find the last character where we can break. */
							while (text_length - length > 0) {
								if (strchr (" ", data[text_length - length - 1])) {
									length++;
									break;
								} else if (data[text_length - length - 1] == '-' &&
								           text_length - length > 1 &&
								           !strchr (" ", data[text_length - length - 2]))
									break;
								length++;
							}

							if (text_length != length) {
								WebKitDOMNode *nd;

								webkit_dom_text_split_text (
									WEBKIT_DOM_TEXT (prev_sibling),
									text_length - length,
									NULL);

								if ((nd = webkit_dom_node_get_next_sibling (prev_sibling))) {
									gchar *nd_content;

									nd_content = webkit_dom_node_get_text_content (nd);
									if (nd_content && *nd_content) {
										if (*nd_content == ' ')
											mark_and_remove_leading_space (document, nd);

										if (!webkit_dom_node_get_next_sibling (nd) &&
										    g_str_has_suffix (nd_content, " "))
											mark_and_remove_trailing_space (document, nd);

										g_free (nd_content);
									}

									if (nd) {
										if (prev_br)
											remove_node (prev_br);
										 webkit_dom_node_insert_before (
											webkit_dom_node_get_parent_node (nd),
											WEBKIT_DOM_NODE (element),
											nd,
											NULL);

										offset = 0;
										line_length = length;
										continue;
									}
								}
							}
						}
					}
					if (insert_and_continue) {
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (node),
							WEBKIT_DOM_NODE (element),
							node,
							NULL);

						offset = 0;
						line_length = 0;
						insert_and_continue = FALSE;
						continue;
					}

					offset = offset != -1 ? offset : max_length;
				}
			}

			if (offset >= 0) {
				WebKitDOMNode *nd;

				if (offset != length_left && offset != 0) {
					webkit_dom_text_split_text (
						WEBKIT_DOM_TEXT (node), offset, NULL);

					nd = webkit_dom_node_get_next_sibling (node);
				} else
					nd = node;

				if (nd) {
					gboolean no_sibling = FALSE;
					gchar *nd_content;

					nd_content = webkit_dom_node_get_text_content (nd);
					if (nd_content && *nd_content) {
						if (*nd_content == ' ')
							mark_and_remove_leading_space (document, nd);

						if (!webkit_dom_node_get_next_sibling (nd) &&
						    length_left <= length_to_wrap &&
						    g_str_has_suffix (nd_content, " ")) {
							mark_and_remove_trailing_space (document, nd);
							no_sibling = TRUE;
						}

						g_free (nd_content);
					}

					if (!no_sibling)
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (node),
							WEBKIT_DOM_NODE (element),
							nd,
							NULL);
					offset = 0;

					nd_content = webkit_dom_node_get_text_content (nd);
					if (!*nd_content)
						remove_node (nd);
					g_free (nd_content);

					if (no_sibling)
						node = NULL;
					else
						node = webkit_dom_node_get_next_sibling (
							WEBKIT_DOM_NODE (element));
				} else {
					webkit_dom_node_append_child (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						NULL);
				}
			}
			if (node && WEBKIT_DOM_IS_CHARACTER_DATA (node))
				length_left = webkit_dom_character_data_get_length (
					WEBKIT_DOM_CHARACTER_DATA (node));

			line_length = 0;
			compensated = FALSE;
		}
		line_length += length_left - offset;
 next_node:
		if (!node)
			break;

		if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (node)) {
			line_length = 0;
			compensated = FALSE;
		}

		/* Move to next node */
		if (webkit_dom_node_has_child_nodes (node)) {
			node = webkit_dom_node_get_first_child (node);
		} else if (webkit_dom_node_get_next_sibling (node)) {
			node = webkit_dom_node_get_next_sibling (node);
		} else {
			WebKitDOMNode *tmp_parent;

			if (webkit_dom_node_is_equal_node (node, start_node))
				break;

			/* Find a next node that we can process. */
			tmp_parent = webkit_dom_node_get_parent_node (node);
			if (tmp_parent && webkit_dom_node_get_next_sibling (tmp_parent))
				node = webkit_dom_node_get_next_sibling (tmp_parent);
			else {
				WebKitDOMNode *tmp;

				tmp = tmp_parent;
				/* Find a node that is not a start node (that would mean
				 * that we already processed the whole block) and it has
				 * a sibling that we can process. */
				while (tmp && !webkit_dom_node_is_equal_node (tmp, start_node) &&
				       !webkit_dom_node_get_next_sibling (tmp)) {
					tmp = webkit_dom_node_get_parent_node (tmp);
				}

				/* If we found a node to process, let's process its
				 * sibling, otherwise give up. */
				if (tmp)
					node = webkit_dom_node_get_next_sibling (tmp);
				else
					break;
			}
		}
	}

	if (selection) {
		gchar *html;
		WebKitDOMElement *element;

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

		/* Overwrite the current selection by the processed content */
		e_html_editor_selection_insert_html (selection, html);

		g_free (html);

		return NULL;
	} else {
		WebKitDOMNode *last_child;

		last_child = webkit_dom_node_get_last_child (block_clone);
		if (last_child && WEBKIT_DOM_IS_HTMLBR_ELEMENT (last_child) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (last_child), "-x-evo-wrap-br"))
			remove_node (last_child);

		webkit_dom_node_normalize (block_clone);

		node = webkit_dom_node_get_parent_node (block);
		if (node) {
			/* Replace block with wrapped one */
			webkit_dom_node_replace_child (
				node, block_clone, block, NULL);
		}

		return WEBKIT_DOM_ELEMENT (block_clone);
	}
}

void
e_html_editor_selection_set_indented_style (EHTMLEditorSelection *selection,
                                            WebKitDOMElement *element,
                                            gint width)
{
	gchar *style;
	gint word_wrap_length;

	/* width < 0, set block width to word_wrap_length
	 * width ==  0, no width limit set,
	 * width > 0, set width limit to given value */
	word_wrap_length = (width < 0) ? selection->priv->word_wrap_length : width;

	webkit_dom_element_set_class_name (element, "-x-evo-indented");

	if (is_in_html_mode (selection) || word_wrap_length == 0)
		style = g_strdup_printf ("margin-left: %dch;", SPACES_PER_INDENTATION);
	else
		style = g_strdup_printf (
			"margin-left: %dch; word-wrap: normal; width: %dch;",
			SPACES_PER_INDENTATION, word_wrap_length);

	webkit_dom_element_set_attribute (element, "style", style, NULL);
	g_free (style);
}

WebKitDOMElement *
e_html_editor_selection_get_indented_element (EHTMLEditorSelection *selection,
                                              WebKitDOMDocument *document,
                                              gint width)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "DIV", NULL);
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
	char *style = NULL;
	gint word_wrap_length = (width == -1) ? selection->priv->word_wrap_length : width;
	WebKitDOMNode *parent;

	element_add_class (element, "-x-evo-paragraph");

	/* Don't set the alignment for nodes as they are handled separately. */
	if (!node_is_list (WEBKIT_DOM_NODE (element))) {
		EHTMLEditorSelectionAlignment alignment;

		alignment = e_html_editor_selection_get_alignment (selection);
		element_add_class (element, get_css_alignment_value_class (alignment));
	}

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
	/* Don't set the width limit to sub-blocks as the width limit is inhered
	 * from its parents. */
	if (!is_in_html_mode (selection) &&
	    (!parent || WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent))) {
		style = g_strdup_printf (
			"width: %dch; "
			"word-wrap: break-word; "
			"word-break: break-word; %s",
			(word_wrap_length + offset), style_to_add);
	} else {
		if (*style_to_add)
			style = g_strdup_printf ("%s", style_to_add);
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
                                                 gboolean with_input)
{
	WebKitDOMRange *range;
	WebKitDOMElement *container;

	range = webkit_dom_document_create_range (document);
	container = e_html_editor_selection_get_paragraph_element (selection, document, -1, 0);
	webkit_dom_range_select_node (range, node, NULL);
	webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (container), NULL);
	/* We have to move caret position inside this container */
	if (with_input)
		add_selection_markers_into_element_end (document, container, NULL, NULL);

	g_object_unref (range);
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
	EHTMLEditorViewHistoryEvent *ev = NULL;
	gboolean after_selection_end = FALSE, html_mode;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block, *next_block;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

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

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_WRAP;

		e_html_editor_selection_get_selection_coordinates (
			selection,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
		ev->data.style.from = 1;
		ev->data.style.to = 1;
	}

	block = e_html_editor_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	html_mode = e_html_editor_view_get_html_mode (view);

	/* Process all blocks that are in the selection one by one */
	while (block && !after_selection_end) {
		gboolean quoted = FALSE;
		gint citation_level, quote;
		WebKitDOMElement *wrapped_paragraph;

		next_block = webkit_dom_node_get_next_sibling (block);

		/* Don't try to wrap the 'Normal' blocks as they are already wrapped and*/
		/* also skip blocks that we already wrapped with this function. */
		if ((!html_mode && element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-paragraph")) ||
		    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (block), "data-user-wrapped")) {
			block = next_block;
			continue;
		}

		if (webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), "span.-x-evo-quoted", NULL)) {
			quoted = TRUE;
			remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));
		}

		if (!html_mode)
			remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		citation_level = get_citation_level (block);
		quote = citation_level ? citation_level * 2 : 0;

		wrapped_paragraph = e_html_editor_selection_wrap_paragraph_length (
			selection, WEBKIT_DOM_ELEMENT (block), selection->priv->word_wrap_length - quote);

		webkit_dom_element_set_attribute (
			wrapped_paragraph, "data-user-wrapped", "", NULL);

		if (quoted && !html_mode)
			e_html_editor_view_quote_plain_text_element (view, wrapped_paragraph);

		block = next_block;
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

	e_html_editor_view_force_spell_check_in_viewport (view);

	g_object_unref (view);
}

WebKitDOMElement *
e_html_editor_selection_wrap_paragraph_length (EHTMLEditorSelection *selection,
                                               WebKitDOMElement *paragraph,
                                               gint length)
{
	WebKitDOMDocument *document;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);
	g_return_val_if_fail (WEBKIT_DOM_IS_ELEMENT (paragraph), NULL);
	g_return_val_if_fail (length >= MINIMAL_PARAGRAPH_WIDTH, NULL);

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (paragraph));

	return wrap_lines (
		NULL, WEBKIT_DOM_NODE (paragraph), document, FALSE, length, selection->priv->word_wrap_length);
}

void
e_html_editor_selection_wrap_paragraphs_in_document (EHTMLEditorSelection *selection,
                                                     WebKitDOMDocument *document)
{
	WebKitDOMNodeList *list;
	gint ii, length;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	/* Only wrap paragraphs that are inside the quoted content, others are
	 * wrapped by CSS. */
	list = webkit_dom_document_query_selector_all (
		document,
		"blockquote[type=cite] > div.-x-evo-paragraph:not(#-x-evo-input-start)",
		NULL);

	length = webkit_dom_node_list_get_length (list);

	for (ii = 0; ii < length; ii++) {
		gint quote, citation_level;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		citation_level = get_citation_level (node);
		quote = citation_level ? citation_level * 2 : 0;

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
		g_object_unref (node);
	}
	g_object_unref (list);
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

	if (node_is_list_or_item (WEBKIT_DOM_NODE (paragraph))) {
		gint list_level = get_list_level (WEBKIT_DOM_NODE (paragraph));
		indentation_level = 0;

		if (list_level > 0)
			offset = list_level * -SPACES_PER_LIST_LEVEL;
		else
			offset = -SPACES_PER_LIST_LEVEL;
	}

	quote = citation_level ? citation_level * 2 : 0;

	final_width = word_wrap_length - quote + offset;
	final_width -= SPACES_PER_INDENTATION * indentation_level;

	return e_html_editor_selection_wrap_paragraph_length (
		selection, WEBKIT_DOM_ELEMENT (paragraph), final_width);
}

static WebKitDOMNode *
in_empty_block_in_quoted_content (WebKitDOMNode *element)
{
	WebKitDOMNode *first_child, *next_sibling;

	first_child = webkit_dom_node_get_first_child (element);
	if (!WEBKIT_DOM_IS_ELEMENT (first_child))
		return NULL;

	if (!element_has_class (WEBKIT_DOM_ELEMENT (first_child), "-x-evo-quoted"))
		return NULL;

	next_sibling = webkit_dom_node_get_next_sibling (first_child);
	if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling))
		return next_sibling;

	if (!WEBKIT_DOM_IS_ELEMENT (next_sibling))
		return NULL;

	if (!element_has_id (WEBKIT_DOM_ELEMENT (next_sibling), "-x-evo-selection-start-marker"))
		return NULL;

	next_sibling = webkit_dom_node_get_next_sibling (next_sibling);
	if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling))
		return next_sibling;

	return NULL;
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
	gboolean collapsed = FALSE;
	glong offset, anchor_offset;
	EHTMLEditorView *view;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMNode *container, *next_sibling, *marker_node;
	WebKitDOMNode *split_node, *parent_node, *anchor;
	WebKitDOMElement *start_marker = NULL, *end_marker = NULL;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	web_view = WEBKIT_WEB_VIEW (view);

	document = webkit_web_view_get_dom_document (web_view);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	g_object_unref (view);

	/* First remove all markers (if present) */
	remove_selection_markers (document);

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_object_unref (dom_selection);
		g_object_unref (dom_window);
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	if (!range) {
		g_object_unref (dom_selection);
		g_object_unref (dom_window);
		return;
	}

	anchor = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);

	collapsed = webkit_dom_range_get_collapsed (range, NULL);
	start_marker = create_selection_marker (document, TRUE);

	container = webkit_dom_range_get_start_container (range, NULL);
	offset = webkit_dom_range_get_start_offset (range, NULL);
	parent_node = webkit_dom_node_get_parent_node (container);

	if (webkit_dom_node_is_same_node (anchor, container) && offset == anchor_offset)
		webkit_dom_element_set_attribute (start_marker, "data-anchor", "", NULL);

	if (element_has_class (WEBKIT_DOM_ELEMENT (parent_node), "-x-evo-quote-character")) {
		WebKitDOMNode *node;

		node = webkit_dom_node_get_parent_node (
			webkit_dom_node_get_parent_node (parent_node));

		if ((next_sibling = in_empty_block_in_quoted_content (node))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (next_sibling),
				WEBKIT_DOM_NODE (start_marker),
				next_sibling,
				NULL);
		} else {
			webkit_dom_node_insert_before (
				node,
				WEBKIT_DOM_NODE (start_marker),
				webkit_dom_node_get_next_sibling (
					webkit_dom_node_get_parent_node (parent_node)),
				NULL);
		}
		goto insert_end_marker;
	} else if (element_has_class (WEBKIT_DOM_ELEMENT (parent_node), "-x-evo-smiley-text")) {
		WebKitDOMNode *node;

		node = webkit_dom_node_get_parent_node (parent_node);
		if (offset == 0) {
			marker_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (node),
				WEBKIT_DOM_NODE (start_marker),
				node,
				NULL);
			goto insert_end_marker;
		}
	} else if (element_has_class (WEBKIT_DOM_ELEMENT (parent_node), "Apple-tab-span") && offset == 1) {
			marker_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent_node),
				WEBKIT_DOM_NODE (start_marker),
				webkit_dom_node_get_next_sibling (parent_node),
				NULL);
			goto insert_end_marker;
	}

	if (WEBKIT_DOM_IS_TEXT (container)) {
		if (offset != 0) {
			WebKitDOMText *split_text;

			split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (container), offset, NULL);
			split_node = WEBKIT_DOM_NODE (split_text);
		} else {
			marker_node = webkit_dom_node_insert_before (
				parent_node,
				WEBKIT_DOM_NODE (start_marker),
				container,
				NULL);
			goto insert_end_marker;
		}
	} else if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (container)) {
		marker_node = webkit_dom_node_insert_before (
			container,
			WEBKIT_DOM_NODE (start_marker),
			webkit_dom_node_get_first_child (container),
			NULL);
		goto insert_end_marker;
	} else if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (container)) {
		marker_node = webkit_dom_node_insert_before (
			container,
			WEBKIT_DOM_NODE (start_marker),
			webkit_dom_node_get_first_child (container),
			NULL);
		goto insert_end_marker;
	} else if (element_has_class (WEBKIT_DOM_ELEMENT (container), "-x-evo-resizable-wrapper")) {
		marker_node = webkit_dom_node_insert_before (
			parent_node,
			WEBKIT_DOM_NODE (start_marker),
			webkit_dom_node_get_next_sibling (container),
			NULL);
		goto insert_end_marker;
	} else {
		/* Insert the selection marker on the right position in
		 * an empty paragraph in the quoted content */
		if ((next_sibling = in_empty_block_in_quoted_content (container))) {
			marker_node = webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (start_marker),
				next_sibling,
				NULL);
			goto insert_end_marker;
		}
		if (!webkit_dom_node_get_previous_sibling (container)) {
			marker_node = webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (start_marker),
				webkit_dom_node_get_first_child (container),
				NULL);
			goto insert_end_marker;
		} else if (!webkit_dom_node_get_next_sibling (container)) {
			WebKitDOMNode *tmp;

			tmp = webkit_dom_node_get_last_child (container);
			if (tmp && WEBKIT_DOM_IS_HTMLBR_ELEMENT (tmp))
				marker_node = webkit_dom_node_insert_before (
					container,
					WEBKIT_DOM_NODE (start_marker),
					tmp,
					NULL);
			else
				marker_node = webkit_dom_node_append_child (
					container,
					WEBKIT_DOM_NODE (start_marker),
					NULL);
			goto insert_end_marker;
		} else {
			if (webkit_dom_node_get_first_child (container)) {
				marker_node = webkit_dom_node_insert_before (
					container,
					WEBKIT_DOM_NODE (start_marker),
					webkit_dom_node_get_first_child (container),
					NULL);
				goto insert_end_marker;
			}
			split_node = container;
		}
	}

	/* Don't save selection straight into body */
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (split_node))
		goto out;

	if (!split_node) {
		marker_node = webkit_dom_node_insert_before (
			container,
			WEBKIT_DOM_NODE (start_marker),
			webkit_dom_node_get_first_child (
				WEBKIT_DOM_NODE (container)),
			NULL);
	} else {
		marker_node = WEBKIT_DOM_NODE (start_marker);
		parent_node = webkit_dom_node_get_parent_node (split_node);

		webkit_dom_node_insert_before (
			parent_node, marker_node, split_node, NULL);
	}

	webkit_dom_node_normalize (parent_node);

 insert_end_marker:
	end_marker = create_selection_marker (document, FALSE);

	if (collapsed) {
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (start_marker)),
			WEBKIT_DOM_NODE (end_marker),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (start_marker)),
			NULL);
		goto out;
	}

	container = webkit_dom_range_get_end_container (range, NULL);
	offset = webkit_dom_range_get_end_offset (range, NULL);
	parent_node = webkit_dom_node_get_parent_node (container);

	if (webkit_dom_node_is_same_node (anchor, container) && offset == anchor_offset)
		webkit_dom_element_set_attribute (end_marker, "data-anchor", "", NULL);

	if (element_has_class (WEBKIT_DOM_ELEMENT (parent_node), "-x-evo-quote-character")) {
		WebKitDOMNode *node;

		node = webkit_dom_node_get_parent_node (
			webkit_dom_node_get_parent_node (parent_node));

		if ((next_sibling = in_empty_block_in_quoted_content (node))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (next_sibling),
				WEBKIT_DOM_NODE (end_marker),
				next_sibling,
				NULL);
		} else {
			webkit_dom_node_insert_before (
				node,
				WEBKIT_DOM_NODE (end_marker),
				webkit_dom_node_get_next_sibling (
					webkit_dom_node_get_parent_node (parent_node)),
				NULL);
		}
		goto out;
	}

	if (WEBKIT_DOM_IS_TEXT (container)) {
		if (offset != 0) {
			WebKitDOMText *split_text;

			split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (container), offset, NULL);
			split_node = WEBKIT_DOM_NODE (split_text);
		} else {
			marker_node = webkit_dom_node_insert_before (
				parent_node, WEBKIT_DOM_NODE (end_marker), container, NULL);
			goto check;

		}
	} else if (WEBKIT_DOM_IS_HTMLLI_ELEMENT (container)) {
		webkit_dom_node_append_child (
			container, WEBKIT_DOM_NODE (end_marker), NULL);
		goto out;
	} else {
		/* Insert the selection marker on the right position in
		 * an empty paragraph in the quoted content */
		if ((next_sibling = in_empty_block_in_quoted_content (container))) {
			webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (end_marker),
				next_sibling,
				NULL);
			goto out;
		}
		if (!webkit_dom_node_get_previous_sibling (container)) {
			split_node = parent_node;
		} else if (!webkit_dom_node_get_next_sibling (container)) {
			split_node = parent_node;
			split_node = webkit_dom_node_get_next_sibling (split_node);
		} else
			split_node = container;
	}

	/* Don't save selection straight into body */
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (split_node)) {
		remove_node (WEBKIT_DOM_NODE (start_marker));
		g_object_unref (range);
		g_object_unref (dom_selection);
		g_object_unref (dom_window);
		return;
	}

	marker_node = WEBKIT_DOM_NODE (end_marker);

	if (split_node) {
		parent_node = webkit_dom_node_get_parent_node (split_node);

		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent_node)) {
			if (offset == 0)
				webkit_dom_node_insert_before (
					split_node,
					marker_node,
					webkit_dom_node_get_first_child (split_node),
					NULL);
			else
				webkit_dom_node_append_child (
					webkit_dom_node_get_previous_sibling (split_node),
					marker_node,
					NULL);
		} else
			webkit_dom_node_insert_before (
				parent_node, marker_node, split_node, NULL);
	} else {
		WebKitDOMNode *first_child;

		first_child = webkit_dom_node_get_first_child (container);
		if (offset == 0 && WEBKIT_DOM_IS_TEXT (first_child))
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (container), marker_node, webkit_dom_node_get_first_child (container), NULL);
		else
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (container), marker_node, NULL);
	}

	webkit_dom_node_normalize (parent_node);

 check:
	if ((next_sibling = webkit_dom_node_get_next_sibling (marker_node))) {
		if (!WEBKIT_DOM_IS_ELEMENT (next_sibling))
			next_sibling = webkit_dom_node_get_next_sibling (next_sibling);
		/* If the selection is collapsed ensure that the selection start marker
		 * is before the end marker */
		if (next_sibling && webkit_dom_node_is_same_node (next_sibling, WEBKIT_DOM_NODE (start_marker))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (marker_node),
				next_sibling,
				marker_node,
				NULL);
		}
	}
 out:
	if (!collapsed) {
		if (start_marker && end_marker) {
			webkit_dom_range_set_start_after (range, WEBKIT_DOM_NODE (start_marker), NULL);
			webkit_dom_range_set_end_before (range, WEBKIT_DOM_NODE (end_marker), NULL);
		} else {
			g_warn_if_reached ();
		}

		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
	}

	g_object_unref (range);
	g_object_unref (dom_selection);
	g_object_unref (dom_window);
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
	gboolean start_is_anchor = FALSE;
	glong offset;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMElement *marker;
	WebKitDOMNode *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *parent_start, *parent_end, *anchor;
	WebKitDOMRange *range;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	web_view = WEBKIT_WEB_VIEW (view);

	document = webkit_web_view_get_dom_document (web_view);
	g_object_unref (view);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	g_object_unref (dom_window);
	if (!range) {
		WebKitDOMHTMLElement *body;

		range = webkit_dom_document_create_range (document);
		body = webkit_dom_document_get_body (document);

		webkit_dom_range_select_node_contents (range, WEBKIT_DOM_NODE (body), NULL);
		webkit_dom_range_collapse (range, TRUE, NULL);
		webkit_dom_dom_selection_add_range (dom_selection, range);
	}

	selection_start_marker = webkit_dom_range_get_start_container (range, NULL);
	if (selection_start_marker) {
		gboolean ok = FALSE;
		selection_start_marker =
			webkit_dom_node_get_next_sibling (selection_start_marker);

		ok = e_html_editor_node_is_selection_position_node (selection_start_marker);

		if (ok) {
			ok = FALSE;
			if (webkit_dom_range_get_collapsed (range, NULL)) {
				selection_end_marker = webkit_dom_node_get_next_sibling (
					selection_start_marker);

				ok = e_html_editor_node_is_selection_position_node (selection_end_marker);
				if (ok) {
					WebKitDOMNode *next_sibling;

					next_sibling = webkit_dom_node_get_next_sibling (selection_end_marker);

					if (next_sibling && !WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling)) {
						parent_start = webkit_dom_node_get_parent_node (selection_end_marker);

						remove_node (selection_start_marker);
						remove_node (selection_end_marker);

						webkit_dom_node_normalize (parent_start);
						g_object_unref (range);
						g_object_unref (dom_selection);
						return;
					}
				}
			}
		}
	}

	g_object_unref (range);
	range = webkit_dom_document_create_range (document);
	if (!range) {
		g_object_unref (dom_selection);
		return;
	}

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (!marker) {
		marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		if (marker)
			remove_node (WEBKIT_DOM_NODE (marker));
		g_object_unref (dom_selection);
		g_object_unref (range);
		return;
	}

	start_is_anchor = webkit_dom_element_has_attribute (marker, "data-anchor");
	parent_start = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (marker));

	webkit_dom_range_set_start_after (range, WEBKIT_DOM_NODE (marker), NULL);
	remove_node (WEBKIT_DOM_NODE (marker));

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");
	if (!marker) {
		marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (marker)
			remove_node (WEBKIT_DOM_NODE (marker));
		g_object_unref (dom_selection);
		g_object_unref (range);
		return;
	}

	parent_end = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (marker));
	webkit_dom_range_set_end_before (range, WEBKIT_DOM_NODE (marker), NULL);
	remove_node (WEBKIT_DOM_NODE (marker));

	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	if (webkit_dom_node_is_same_node (parent_start, parent_end))
		webkit_dom_node_normalize (parent_start);
	else {
		webkit_dom_node_normalize (parent_start);
		webkit_dom_node_normalize (parent_end);
	}

	if (start_is_anchor) {
		anchor = webkit_dom_range_get_end_container (range, NULL);
		offset = webkit_dom_range_get_end_offset (range, NULL);

		webkit_dom_range_collapse (range, TRUE, NULL);
	} else {
		anchor = webkit_dom_range_get_start_container (range, NULL);
		offset = webkit_dom_range_get_start_offset (range, NULL);

		webkit_dom_range_collapse (range, FALSE, NULL);
	}
	webkit_dom_dom_selection_add_range (dom_selection, range);
	webkit_dom_dom_selection_extend (dom_selection, anchor, offset, NULL);
	g_object_unref (range);
	g_object_unref (dom_selection);
}

void
e_html_editor_selection_set_on_point (EHTMLEditorSelection *selection,
                                      guint x,
                                      guint y)
{
	EHTMLEditorView *view;
	WebKitDOMRange *range;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	range = webkit_dom_document_caret_range_from_point (document, x, y);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_object_unref (range);
	g_object_unref (dom_selection);
	g_object_unref (dom_window);
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
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	const gchar *granularity_str = NULL;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	web_view = WEBKIT_WEB_VIEW (view);

	document = webkit_web_view_get_dom_document (web_view);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	switch (granularity) {
		case E_HTML_EDITOR_SELECTION_GRANULARITY_CHARACTER:
			granularity_str = "character";
			break;
		case E_HTML_EDITOR_SELECTION_GRANULARITY_WORD:
			granularity_str = "word";
			break;
	}

	if (granularity_str) {
		webkit_dom_dom_selection_modify (
			dom_selection, alter,
			forward ? "forward" : "backward",
			granularity_str);
	}

	g_object_unref (dom_selection);
	g_object_unref (dom_window);
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
	glong element_top, element_left;
	glong window_top, window_left, window_right, window_bottom;
	EHTMLEditorView *view;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMElement *selection_start_marker;

	e_html_editor_selection_save (selection);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	g_object_unref (view);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (!selection_start_marker)
		return;

	dom_window = webkit_dom_document_get_default_view (document);

	window_top = webkit_dom_dom_window_get_scroll_y (dom_window);
	window_left = webkit_dom_dom_window_get_scroll_x (dom_window);
	window_bottom = window_top + webkit_dom_dom_window_get_inner_height (dom_window);
	window_right = window_left + webkit_dom_dom_window_get_inner_width (dom_window);

	element_left = webkit_dom_element_get_offset_left (selection_start_marker);
	element_top = webkit_dom_element_get_offset_top (selection_start_marker);

	/* Check if caret is inside viewport, if not move to it */
	if (!(element_top >= window_top && element_top <= window_bottom &&
	     element_left >= window_left && element_left <= window_right)) {
		webkit_dom_element_scroll_into_view (selection_start_marker, TRUE);
	}

	e_html_editor_selection_restore (selection);

	g_object_unref (dom_window);
}
