/*
 * e-editor-selection.c
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

#include "e-editor-selection.h"
#include "e-editor-widget.h"
#include "e-editor.h"
#include "e-editor-utils.h"

#include <e-util/e-util.h>

#include <webkit/webkit.h>
#include <webkit/webkitdom.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define E_EDITOR_SELECTION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_EDITOR_SELECTION, EEditorSelectionPrivate))

#define WORD_WRAP_LENGTH 71

/**
 * EEditorSelection:
 *
 * The #EEditorSelection object represents current position of the cursor
 * with the editor or current text selection within the editor. To obtain
 * valid #EEditorSelection, call e_editor_widget_get_selection().
 */

struct _EEditorSelectionPrivate {

	WebKitWebView *webview;

	gchar *text;
	gchar *background_color;
	gchar *font_color;
	gchar *font_family;

	gulong selection_offset;
};

enum {
	PROP_0,
	PROP_ALIGNMENT,
	PROP_BACKGROUND_COLOR,
	PROP_BLOCK_FORMAT,
	PROP_BOLD,
	PROP_EDITOR_WIDGET,
	PROP_FONT_COLOR,
	PROP_FONT_NAME,
	PROP_FONT_SIZE,
	PROP_INDENTED,
	PROP_ITALIC,
	PROP_MONOSPACED,
	PROP_STRIKE_THROUGH,
	PROP_SUBSCRIPT,
	PROP_SUPERSCRIPT,
	PROP_TEXT,
	PROP_UNDERLINE
};

static const GdkRGBA black = { 0 };

G_DEFINE_TYPE (
	EEditorSelection,
	e_editor_selection,
	G_TYPE_OBJECT
);

static WebKitDOMRange *
editor_selection_get_current_range (EEditorSelection *selection)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	window = webkit_dom_document_get_default_view (document);
	if (window == NULL)
		return NULL;

	dom_selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1)
		return NULL;

	return webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
}

static gboolean
get_has_style (EEditorSelection *selection,
               const gchar *style_tag)
{
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;
	gboolean result;
	gint tag_len;

	range = editor_selection_get_current_range (selection);
	if (range == NULL)
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
get_font_property (EEditorSelection *selection,
                   const gchar *font_property)
{
	WebKitDOMRange *range;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	gchar *value;

	range = editor_selection_get_current_range (selection);
	if (range == NULL)
		return NULL;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	element = e_editor_dom_node_find_parent_element (node, "FONT");
	if (element == NULL)
		return NULL;

	g_object_get (G_OBJECT (element), font_property, &value, NULL);

	return value;
}

static void
editor_selection_selection_changed_cb (WebKitWebView *webview,
                                       EEditorSelection *selection)
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
	g_object_notify (G_OBJECT (selection), "strike-through");
	g_object_notify (G_OBJECT (selection), "subscript");
	g_object_notify (G_OBJECT (selection), "superscript");
	g_object_notify (G_OBJECT (selection), "text");
	g_object_notify (G_OBJECT (selection), "underline");

	g_object_thaw_notify (G_OBJECT (selection));
}

static void
editor_selection_set_editor_widget (EEditorSelection *selection,
                                    EEditorWidget *editor_widget)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (editor_widget));
	g_return_if_fail (selection->priv->webview == NULL);

	selection->priv->webview = g_object_ref (editor_widget);

	g_signal_connect (
		editor_widget, "selection-changed",
		G_CALLBACK (editor_selection_selection_changed_cb), selection);
}

static void
editor_selection_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	GdkRGBA rgba = { 0 };

	switch (property_id) {
		case PROP_ALIGNMENT:
			g_value_set_int (
				value,
				e_editor_selection_get_alignment (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_BACKGROUND_COLOR:
			g_value_set_string (
				value,
				e_editor_selection_get_background_color (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_BLOCK_FORMAT:
			g_value_set_int (
				value,
				e_editor_selection_get_block_format (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_BOLD:
			g_value_set_boolean (
				value,
				e_editor_selection_is_bold (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_FONT_COLOR:
			e_editor_selection_get_font_color (
				E_EDITOR_SELECTION (object), &rgba);
			g_value_set_boxed (value, &rgba);
			return;

		case PROP_FONT_NAME:
			g_value_set_string (
				value,
				e_editor_selection_get_font_name (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_FONT_SIZE:
			g_value_set_int (
				value,
				e_editor_selection_get_font_size (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_INDENTED:
			g_value_set_boolean (
				value,
				e_editor_selection_is_indented (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_ITALIC:
			g_value_set_boolean (
				value,
				e_editor_selection_is_italic (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_MONOSPACED:
			g_value_set_boolean (
				value,
				e_editor_selection_is_monospaced (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_STRIKE_THROUGH:
			g_value_set_boolean (
				value,
				e_editor_selection_is_strike_through (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_SUBSCRIPT:
			g_value_set_boolean (
				value,
				e_editor_selection_is_subscript (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_SUPERSCRIPT:
			g_value_set_boolean (
				value,
				e_editor_selection_is_superscript (
				E_EDITOR_SELECTION (object)));
			return;

		case PROP_TEXT:
			g_value_set_string (
				value,
				e_editor_selection_get_string (
				E_EDITOR_SELECTION (object)));
			break;

		case PROP_UNDERLINE:
			g_value_set_boolean (
				value,
				e_editor_selection_is_underline (
				E_EDITOR_SELECTION (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_selection_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALIGNMENT:
			e_editor_selection_set_alignment (
				E_EDITOR_SELECTION (object),
				g_value_get_int (value));
			return;

		case PROP_BACKGROUND_COLOR:
			e_editor_selection_set_background_color (
				E_EDITOR_SELECTION (object),
				g_value_get_string (value));
			return;

		case PROP_BOLD:
			e_editor_selection_set_bold (
				E_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_EDITOR_WIDGET:
			editor_selection_set_editor_widget (
				E_EDITOR_SELECTION (object),
				g_value_get_object (value));
			return;

		case PROP_FONT_COLOR:
			e_editor_selection_set_font_color (
				E_EDITOR_SELECTION (object),
				g_value_get_boxed (value));
			return;

		case PROP_BLOCK_FORMAT:
			e_editor_selection_set_block_format (
				E_EDITOR_SELECTION (object),
				g_value_get_int (value));
			return;

		case PROP_FONT_NAME:
			e_editor_selection_set_font_name (
				E_EDITOR_SELECTION (object),
				g_value_get_string (value));
			return;

		case PROP_FONT_SIZE:
			e_editor_selection_set_font_size (
				E_EDITOR_SELECTION (object),
				g_value_get_int (value));
			return;

		case PROP_ITALIC:
			e_editor_selection_set_italic (
				E_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_MONOSPACED:
			e_editor_selection_set_monospaced (
				E_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_STRIKE_THROUGH:
			e_editor_selection_set_strike_through (
				E_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_SUBSCRIPT:
			e_editor_selection_set_subscript (
				E_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_SUPERSCRIPT:
			e_editor_selection_set_superscript (
				E_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;

		case PROP_UNDERLINE:
			e_editor_selection_set_underline (
				E_EDITOR_SELECTION (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_selection_dispose (GObject *object)
{
	EEditorSelectionPrivate *priv;

	priv = E_EDITOR_SELECTION_GET_PRIVATE (object);

	g_clear_object (&priv->webview);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_editor_selection_parent_class)->dispose (object);
}

static void
editor_selection_finalize (GObject *object)
{
	EEditorSelection *selection = E_EDITOR_SELECTION (object);

	g_free (selection->priv->text);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_editor_selection_parent_class)->finalize (object);
}

static void
e_editor_selection_class_init (EEditorSelectionClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EEditorSelectionPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = editor_selection_get_property;
	object_class->set_property = editor_selection_set_property;
	object_class->dispose = editor_selection_dispose;
	object_class->finalize = editor_selection_finalize;

	/**
	 * EEditorSelection:alignment
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
			E_EDITOR_SELECTION_ALIGNMENT_LEFT,
			E_EDITOR_SELECTION_ALIGNMENT_RIGHT,
			E_EDITOR_SELECTION_ALIGNMENT_LEFT,
			G_PARAM_READWRITE));

	/**
	 * EEditorSelection:background-color
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
	 * EEditorSelection:block-format
	 *
	 * Holds block format of current paragraph. See
	 * #EEditorSelectionBlockFormat for valid values.
	 */
	/* FIXME Convert the EEditorSelectionBlockFormat
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
	 * EEditorSelection:bold
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
		PROP_EDITOR_WIDGET,
		g_param_spec_object (
			"editor-widget",
			NULL,
			NULL,
			E_TYPE_EDITOR_WIDGET,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EEditorSelection:font-color
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
	 * EEditorSelection:font-name
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
	 * EEditorSelection:font-size
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
	 * EEditorSelection:indented
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
	 * EEditorSelection:italic
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
	 * EEditorSelection:monospaced
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
	 * EEditorSelection:strike-through
	 *
	 * Holds whether current selection or letter at current cursor position
	 * is strike-through.
	 */
	g_object_class_install_property (
		object_class,
		PROP_STRIKE_THROUGH,
		g_param_spec_boolean (
			"strike-through",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EEditorSelection:superscript
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
	 * EEditorSelection:subscript
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
	 * EEditorSelection:text
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
	 * EEditorSelection:underline
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
e_editor_selection_init (EEditorSelection *selection)
{
	selection->priv = E_EDITOR_SELECTION_GET_PRIVATE (selection);
}

/**
 * e_editor_selection_has_text:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection contains any text.
 *
 * Returns: @TRUE when current selection contains text, @FALSE otherwise.
 */
gboolean
e_editor_selection_has_text (EEditorSelection *selection)
{
	WebKitDOMRange *range;
	WebKitDOMNode *node;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	range = editor_selection_get_current_range (selection);

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
 * e_editor_selection_get_caret_word:
 * @selection: an #EEditorSelection
 *
 * Returns word under cursor.
 *
 * Returns: A newly allocated string with current caret word or @NULL when there
 * is no text under cursor or when selection is active. [transfer-full].
 */
gchar *
e_editor_selection_get_caret_word (EEditorSelection *selection)
{
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), NULL);

	range = editor_selection_get_current_range (selection);

	/* Don't operate on the visible selection */
	range = webkit_dom_range_clone_range (range, NULL);
	webkit_dom_range_expand (range, "word", NULL);

	return webkit_dom_range_to_string (range, NULL);
}

/**
 * e_editor_selection_replace_caret_word:
 * @selection: an #EEditorSelection
 * @replacement: a string to replace current caret word with
 *
 * Replaces current word under cursor with @replacement.
 */
void
e_editor_selection_replace_caret_word (EEditorSelection *selection,
                                       const gchar *replacement)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (replacement);

	range = editor_selection_get_current_range (selection);
	document = webkit_web_view_get_dom_document (selection->priv->webview);
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	webkit_dom_range_expand (range, "word", NULL);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	e_editor_selection_insert_html (selection, replacement);
}

/**
 * e_editor_selection_get_string:
 * @selection: an #EEditorSelection
 *
 * Returns currently selected string.
 *
 * Returns: A pointer to content of current selection. The string is owned by
 * #EEditorSelection and should not be free'd.
 */
const gchar *
e_editor_selection_get_string (EEditorSelection *selection)
{
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), NULL);

	range = editor_selection_get_current_range (selection);

	g_free (selection->priv->text);
	selection->priv->text = webkit_dom_range_get_text (range);

	return selection->priv->text;
}

/**
 * e_editor_selection_replace:
 * @selection: an #EEditorSelection
 * @new_string: a string to replace current selection with
 *
 * Replaces currently selected text with @new_string.
 */
void
e_editor_selection_replace (EEditorSelection *selection,
                            const gchar *new_string)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_INSERT_TEXT,
		new_string);
}

/**
 * e_editor_selection_get_alignment:
 * @selection: #an EEditorSelection
 *
 * Returns alignment of current paragraph
 *
 * Returns: #EEditorSelectionAlignment
 */
EEditorSelectionAlignment
e_editor_selection_get_alignment (EEditorSelection *selection)
{
	WebKitDOMRange *range;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMCSSStyleDeclaration *style;
	gchar *value;
	EEditorSelectionAlignment alignment;

	g_return_val_if_fail (
		E_IS_EDITOR_SELECTION (selection),
		E_EDITOR_SELECTION_ALIGNMENT_LEFT);

	range = editor_selection_get_current_range (selection);
	if (range == NULL)
		return E_EDITOR_SELECTION_ALIGNMENT_LEFT;

	node = webkit_dom_range_get_start_container (range, NULL);
	if (node == NULL)
		return E_EDITOR_SELECTION_ALIGNMENT_LEFT;

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	style = webkit_dom_element_get_style (element);
	value = webkit_dom_css_style_declaration_get_property_value (
		style, "text-align");

	if (!value || !*value ||
	    (g_ascii_strncasecmp (value, "left", 4) == 0)) {
		alignment = E_EDITOR_SELECTION_ALIGNMENT_LEFT;
	} else if (g_ascii_strncasecmp (value, "center", 6) == 0) {
		alignment = E_EDITOR_SELECTION_ALIGNMENT_CENTER;
	} else if (g_ascii_strncasecmp (value, "right", 5) == 0) {
		alignment = E_EDITOR_SELECTION_ALIGNMENT_RIGHT;
	} else {
		alignment = E_EDITOR_SELECTION_ALIGNMENT_LEFT;
	}

	g_free (value);

	return alignment;
}

/**
 * e_editor_selection_set_alignment:
 * @selection: an #EEditorSelection
 * @alignment: an #EEditorSelectionAlignment value to apply
 *
 * Sets alignment of current paragraph to give @alignment.
 */
void
e_editor_selection_set_alignment (EEditorSelection *selection,
                                  EEditorSelectionAlignment alignment)
{
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_get_alignment (selection) == alignment)
		return;

	switch (alignment) {
		case E_EDITOR_SELECTION_ALIGNMENT_CENTER:
			command = E_EDITOR_WIDGET_COMMAND_JUSTIFY_CENTER;
			break;

		case E_EDITOR_SELECTION_ALIGNMENT_LEFT:
			command = E_EDITOR_WIDGET_COMMAND_JUSTIFY_LEFT;
			break;

		case E_EDITOR_SELECTION_ALIGNMENT_RIGHT:
			command = E_EDITOR_WIDGET_COMMAND_JUSTIFY_RIGHT;
			break;
	}

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview), command, NULL);

	g_object_notify (G_OBJECT (selection), "alignment");
}

/**
 * e_editor_selection_get_background_color:
 * @selection: an #EEditorSelection
 *
 * Returns background color of currently selected text or letter at current
 * cursor position.
 *
 * Returns: A string with code of current background color.
 */
const gchar *
e_editor_selection_get_background_color (EEditorSelection *selection)
{
	WebKitDOMNode *ancestor;
	WebKitDOMRange *range;
	WebKitDOMCSSStyleDeclaration *css;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), NULL);

	range = editor_selection_get_current_range (selection);

	ancestor = webkit_dom_range_get_common_ancestor_container (range, NULL);

	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (ancestor));
	selection->priv->background_color =
		webkit_dom_css_style_declaration_get_property_value (
			css, "background-color");

	return selection->priv->background_color;
}

/**
 * e_editor_selection_set_background_color:
 * @selection: an #EEditorSelection
 * @color: code of new background color to set
 *
 * Changes background color of current selection or letter at current cursor
 * position to @color.
 */
void
e_editor_selection_set_background_color (EEditorSelection *selection,
                                        const gchar *color)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (color != NULL && *color != '\0');

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_BACKGROUND_COLOR, color);

	g_object_notify (G_OBJECT (selection), "background-color");
}

/**
 * e_editor_selection_get_block_format:
 * @selection: an #EEditorSelection
 *
 * Returns block format of current paragraph.
 *
 * Returns: #EEditorSelectionBlockFormat
 */
EEditorSelectionBlockFormat
e_editor_selection_get_block_format (EEditorSelection *selection)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range;
	WebKitDOMElement *element;
	EEditorSelectionBlockFormat result;

	g_return_val_if_fail (
		E_IS_EDITOR_SELECTION (selection),
		E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH);

	range = editor_selection_get_current_range (selection);
	if (range == NULL)
		return E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;

	node = webkit_dom_range_get_start_container (range, NULL);

	if (e_editor_dom_node_find_parent_element (node, "UL")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;
	} else if ((element = e_editor_dom_node_find_parent_element (node, "OL")) != NULL) {
		if (webkit_dom_element_has_attribute (element, "type")) {
			gchar *type;

			type = webkit_dom_element_get_attribute (element, "type");
			if (type && ((*type == 'a') || (*type == 'A'))) {
				result = E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA;
			} else if (type && ((*type == 'i') || (*type == 'I'))) {
				result = E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN;
			} else {
				result = E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST;
			}

			g_free (type);
		} else {
			result = E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST;
		}
	} else if (e_editor_dom_node_find_parent_element (node, "BLOCKQUOTE")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE;
	} else if (e_editor_dom_node_find_parent_element (node, "PRE")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_PRE;
	} else if (e_editor_dom_node_find_parent_element (node, "ADDRESS")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS;
	} else if (e_editor_dom_node_find_parent_element (node, "H1")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H1;
	} else if (e_editor_dom_node_find_parent_element (node, "H2")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H2;
	} else if (e_editor_dom_node_find_parent_element (node, "H3")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H3;
	} else if (e_editor_dom_node_find_parent_element (node, "H4")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H4;
	} else if (e_editor_dom_node_find_parent_element (node, "H5")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H5;
	} else if (e_editor_dom_node_find_parent_element (node, "H6")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H6;
	} else if (e_editor_dom_node_find_parent_element (node, "P")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
	} else {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
	}

	return result;
}

/**
 * e_editor_selection_set_block_format:
 * @selection: an #EEditorSelection
 * @format: an #EEditorSelectionBlockFormat value
 *
 * Changes block format of current paragraph to @format.
 */
void
e_editor_selection_set_block_format (EEditorSelection *selection,
                                     EEditorSelectionBlockFormat format)
{
	EEditorSelectionBlockFormat current_format;
	EEditorWidgetCommand command;
	const gchar *value;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	current_format = e_editor_selection_get_block_format (selection);
	if (current_format == format) {
		return;
	}

	switch (format) {
		case E_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE:
			command = E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK;
			value = "BLOCKQUOTE";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H1:
			command = E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK;
			value = "H1";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H2:
			command = E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK;
			value = "H2";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H3:
			command = E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK;
			value = "H3";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H4:
			command = E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK;
			value = "H4";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H5:
			command = E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK;
			value = "H5";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H6:
			command = E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK;
			value = "H6";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH:
			command = E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK;
			value = "P";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_PRE:
			command = E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK;
			value = "PRE";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS:
			command = E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK;
			value = "ADDRESS";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST:
		case E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA:
		case E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN:
			command = E_EDITOR_WIDGET_COMMAND_INSERT_ORDERED_LIST;
			value = NULL;
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST:
			command = E_EDITOR_WIDGET_COMMAND_INSERT_UNORDERED_LIST;
			value = NULL;
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_NONE:
		default:
			command = E_EDITOR_WIDGET_COMMAND_REMOVE_FORMAT;
			value = NULL;
			break;
	}

	/* First remove (un)ordered list before changing formatting */
	if (current_format == E_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST) {
		e_editor_widget_exec_command (
			E_EDITOR_WIDGET (selection->priv->webview),
			E_EDITOR_WIDGET_COMMAND_INSERT_UNORDERED_LIST, NULL);
		/*		    ^-- not a typo, "insert" toggles the
		 *			formatting if already present */
	} else if (current_format >= E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST) {
		e_editor_widget_exec_command (
			E_EDITOR_WIDGET (selection->priv->webview),
			E_EDITOR_WIDGET_COMMAND_INSERT_ORDERED_LIST, NULL);
	}

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview), command, value);

	/* Fine tuning - set the specific marker type for ordered lists */
	if ((format == E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA) ||
	    (format == E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN)) {

		WebKitDOMRange *range;
		WebKitDOMNode *node;
		WebKitDOMElement *list;

		range = editor_selection_get_current_range (selection);
		node = webkit_dom_range_get_start_container (range, NULL);

		list = e_editor_dom_node_find_child_element (node, "OL");
		if (list == NULL)
			list = e_editor_dom_node_find_parent_element (node, "OL");

		if (list != NULL) {
			webkit_dom_element_set_attribute (
				list, "type",
				(format == E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA) ?
					"A" : "I", NULL);
		}
	}

	g_object_notify (G_OBJECT (selection), "block-format");
}

/**
 * e_editor_selection_get_font_color:
 * @selection: an #EEditorSelection
 * @rgba: a #GdkRGBA object to be set to current font color
 *
 * Sets @rgba to contain color of current text selection or letter at current
 * cursor position.
 */
void
e_editor_selection_get_font_color (EEditorSelection *selection,
                                   GdkRGBA *rgba)
{
	gchar *color;
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	color = get_font_property (selection, "color");
	if (!color) {
		*rgba = black;
		return;
	}

	gdk_rgba_parse (rgba, color);
	g_free (color);
}

/**
 * e_editor_selection_set_font_color:
 * @selection: an #EEditorSelection
 * @rgba: a #GdkRGBA
 *
 * Sets font color of current selection or letter at current cursor position to
 * color defined in @rgba.
 */
void
e_editor_selection_set_font_color (EEditorSelection *selection,
                                   const GdkRGBA *rgba)
{
	guint32 rgba_value;
	gchar *color;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (rgba == NULL)
		rgba = &black;

	rgba_value = e_rgba_to_value ((GdkRGBA *) rgba);

	color = g_strdup_printf ("#%06x", rgba_value);

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_FORE_COLOR, color);

	g_free (color);

	g_object_notify (G_OBJECT (selection), "font-color");
}

/**
 * e_editor_selection_get_font_name:
 * @selection: an #EEditorSelection
 *
 * Returns name of font used in current selection or at letter at current cursor
 * position.
 *
 * Returns: A string with font name. [transfer-none]
 */
const gchar *
e_editor_selection_get_font_name (EEditorSelection *selection)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range;
	WebKitDOMCSSStyleDeclaration *css;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), NULL);

	range = editor_selection_get_current_range (selection);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);

	g_free (selection->priv->font_family);
	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (node));
	selection->priv->font_family =
		webkit_dom_css_style_declaration_get_property_value (css, "fontFamily");

	return selection->priv->font_family;
}

/**
 * e_editor_selection_set_font_name:
 * @selection: an #EEditorSelection
 * @font_name: a font name to apply
 *
 * Sets font name of current selection or of letter at current cursor position
 * to @font_name.
 */
void
e_editor_selection_set_font_name (EEditorSelection *selection,
                                  const gchar *font_name)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_FONT_NAME, font_name);

	g_object_notify (G_OBJECT (selection), "font-name");
}

/**
 * e_editor_Selection_get_font_size:
 * @selection: an #EEditorSelection
 *
 * Returns point size of current selection or of letter at current cursor position.
 */
 guint
e_editor_selection_get_font_size (EEditorSelection *selection)
{
	gchar *size;
	gint size_int;

	g_return_val_if_fail (
		E_IS_EDITOR_SELECTION (selection),
		E_EDITOR_SELECTION_FONT_SIZE_NORMAL);

	size = get_font_property (selection, "size");
	if (!size) {
		return E_EDITOR_SELECTION_FONT_SIZE_NORMAL;
	}

	size_int = atoi (size);
	g_free (size);

	if (size_int == 0) {
		return E_EDITOR_SELECTION_FONT_SIZE_NORMAL;
	}

	return size_int;
}

/**
 * e_editor_selection_set_font_size:
 * @selection: an #EEditorSelection
 * @font_size: point size to apply
 *
 * Sets font size of current selection or of letter at current cursor position
 * to @font_size.
 */
void
e_editor_selection_set_font_size (EEditorSelection *selection,
                                  guint font_size)
{
	gchar *size_str;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	size_str = g_strdup_printf ("%d", font_size);

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_FONT_SIZE, size_str);
	g_free (size_str);

	g_object_notify (G_OBJECT (selection), "font-size");
}

/**
 * e_editor_selection_is_citation:
 * @selection: an #EEditorSelection
 *
 * Returns whether current paragraph is a citation.
 *
 * Returns: @TRUE when current paragraph is a citation, @FALSE otherwise.
 */
gboolean
e_editor_selection_is_citation (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	/* citation == <blockquote type='cite'>
	 * special case handled in get_has_style() */
	return get_has_style (selection, "citation");
}

/**
 * e_editor_selection_is_indented:
 * @selection: an #EEditorSelection
 *
 * Returns whether current paragraph is indented. This does not include
 * citations.  To check, whether paragraph is a citation, use
 * e_editor_selection_is_citation().
 *
 * Returns: @TRUE when current paragraph is indented, @FALSE otherwise.
 */
gboolean
e_editor_selection_is_indented (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "blockquote");
}

/**
 * e_editor_selection_indent:
 * @selection: an #EEditorSelection
 *
 * Indents current paragraph by one level.
 */
void
e_editor_selection_indent (EEditorSelection *selection)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_INDENT, NULL);

	g_object_notify (G_OBJECT (selection), "indented");
}

/**
 * e_editor_selection_unindent:
 * @selection: an #EEditorSelection
 *
 * Unindents current paragraph by one level.
 */
void
e_editor_selection_unindent (EEditorSelection *selection)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_OUTDENT, NULL);

	g_object_notify (G_OBJECT (selection), "indented");
}

/**
 * e_editor_selection_is_bold:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is bold.
 *
 * Returns @TRUE when selection is bold, @FALSE otherwise.
 */
gboolean
e_editor_selection_is_bold (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "b");
}

/**
 * e_editor_selection_set_bold:
 * @selection: an #EEditorSelection
 * @bold: @TRUE to enable bold, @FALSE to disable
 *
 * Toggles bold formatting of current selection or letter at current cursor
 * position, depending on whether @bold is @TRUE or @FALSE.
 */
void
e_editor_selection_set_bold (EEditorSelection *selection,
                             gboolean bold)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_bold (selection) == bold)
		return;

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_BOLD, NULL);

	g_object_notify (G_OBJECT (selection), "bold");
}

/**
 * e_editor_selection_is_italic:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is italic.
 *
 * Returns @TRUE when selection is italic, @FALSE otherwise.
 */
gboolean
e_editor_selection_is_italic (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "i");
}

/**
 * e_editor_selection_set_italic:
 * @selection: an #EEditorSelection
 * @italic: @TRUE to enable italic, @FALSE to disable
 *
 * Toggles italic formatting of current selection or letter at current cursor
 * position, depending on whether @italic is @TRUE or @FALSE.
 */
void
e_editor_selection_set_italic (EEditorSelection *selection,
                               gboolean italic)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_italic (selection) == italic)
		return;

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_ITALIC, NULL);

	g_object_notify (G_OBJECT (selection), "italic");
}

/**
 * e_editor_selection_is_monospaced:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is monospaced.
 *
 * Returns @TRUE when selection is monospaced, @FALSE otherwise.
 */
gboolean
e_editor_selection_is_monospaced (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "tt");
}

/**
 * e_editor_selection_set_monospaced:
 * @selection: an #EEditorSelection
 * @monospaced: @TRUE to enable monospaced, @FALSE to disable
 *
 * Toggles monospaced formatting of current selection or letter at current cursor
 * position, depending on whether @monospaced is @TRUE or @FALSE.
 */
void
e_editor_selection_set_monospaced (EEditorSelection *selection,
                                   gboolean monospaced)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_monospaced (selection) == monospaced)
		return;

	range = editor_selection_get_current_range (selection);
	if (range == NULL)
		return;

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	if (monospaced) {
		WebKitDOMElement *wrapper;
		gchar *html;

		wrapper = webkit_dom_document_create_element (
			document, "TT", NULL);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (wrapper),
			WEBKIT_DOM_NODE (webkit_dom_range_clone_contents (range, NULL)),
			NULL);

		html = webkit_dom_html_element_get_outer_html (
			WEBKIT_DOM_HTML_ELEMENT (wrapper));
		e_editor_selection_insert_html (selection, html);
	} else {
		/* XXX This removes _all_ formatting that the selection has.
		 *     In theory it's possible to write a code that would
		 *     remove the <TT> from selection using advanced DOM
		 *     manipulation, but right now I don't really feel like
		 *     writing it all. */
		e_editor_widget_exec_command (
			E_EDITOR_WIDGET (selection->priv->webview),
			E_EDITOR_WIDGET_COMMAND_REMOVE_FORMAT, NULL);
	}

	g_object_notify (G_OBJECT (selection), "monospaced");
}

/**
 * e_editor_selection_is_strike_through:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is striked through.
 *
 * Returns @TRUE when selection is striked through, @FALSE otherwise.
 */
gboolean
e_editor_selection_is_strike_through (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "strike");
}

/**
 * e_editor_selection_set_strike_through:
 * @selection: an #EEditorSelection
 * @strike_through: @TRUE to enable strike through, @FALSE to disable
 *
 * Toggles strike through formatting of current selection or letter at current
 * cursor position, depending on whether @strike_through is @TRUE or @FALSE.
 */
void
e_editor_selection_set_strike_through (EEditorSelection *selection,
                                       gboolean strike_through)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_strike_through (selection) == strike_through)
		return;

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_STRIKETHROUGH, NULL);

	g_object_notify (G_OBJECT (selection), "strike-through");
}

/**
 * e_editor_selection_is_subscript:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is in subscript.
 *
 * Returns @TRUE when selection is in subscript, @FALSE otherwise.
 */
gboolean
e_editor_selection_is_subscript (EEditorSelection *selection)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	range = editor_selection_get_current_range (selection);
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
 * e_editor_selection_set_subscript:
 * @selection: an #EEditorSelection
 * @subscript: @TRUE to enable subscript, @FALSE to disable
 *
 * Toggles subscript of current selection or letter at current cursor position,
 * depending on whether @subscript is @TRUE or @FALSE.
 */
void
e_editor_selection_set_subscript (EEditorSelection *selection,
                                  gboolean subscript)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_subscript (selection) == subscript)
		return;

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_SUBSCRIPT, NULL);

	g_object_notify (G_OBJECT (selection), "subscript");
}

/**
 * e_editor_selection_is_superscript:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is in superscript.
 *
 * Returns @TRUE when selection is in superscript, @FALSE otherwise.
 */
gboolean
e_editor_selection_is_superscript (EEditorSelection *selection)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	range = editor_selection_get_current_range (selection);
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
 * e_editor_selection_set_superscript:
 * @selection: an #EEditorSelection
 * @superscript: @TRUE to enable superscript, @FALSE to disable
 *
 * Toggles superscript of current selection or letter at current cursor position,
 * depending on whether @superscript is @TRUE or @FALSE.
 */
void
e_editor_selection_set_superscript (EEditorSelection *selection,
                                    gboolean superscript)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_superscript (selection) == superscript)
		return;

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_SUPERSCRIPT, NULL);

	g_object_notify (G_OBJECT (selection), "superscript");
}

/**
 * e_editor_selection_is_underline:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is underlined.
 *
 * Returns @TRUE when selection is underlined, @FALSE otherwise.
 */
gboolean
e_editor_selection_is_underline (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "u");
}

/**
 * e_editor_selection_set_underline:
 * @selection: an #EEditorSelection
 * @underline: @TRUE to enable underline, @FALSE to disable
 *
 * Toggles underline formatting of current selection or letter at current
 * cursor position, depending on whether @underline is @TRUE or @FALSE.
 */
void
e_editor_selection_set_underline (EEditorSelection *selection,
                                  gboolean underline)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_underline (selection) == underline)
		return;

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_UNDERLINE, NULL);

	g_object_notify (G_OBJECT (selection), "underline");
}

/**
 * e_editor_selection_unlink:
 * @selection: an #EEditorSelection
 *
 * Removes any links (&lt;A&gt; elements) from current selection or at current
 * cursor position.
 */
void
e_editor_selection_unlink (EEditorSelection *selection)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_UNLINK, NULL);
}

/**
 * e_editor_selection_create_link:
 * @selection: an #EEditorSelection
 * @uri: destination of the new link
 *
 * Converts current selection into a link pointing to @url.
 */
void
e_editor_selection_create_link (EEditorSelection *selection,
                                const gchar *uri)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (uri && *uri);

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_CREATE_LINK, uri);
}

/**
 * e_editor_selection_insert_text:
 * @selection: an #EEditorSelection
 * @plain_text: text to insert
 *
 * Inserts @plain_text at current cursor position. When a text range is selected,
 * it will be replaced by @plain_text.
 */
void
e_editor_selection_insert_text (EEditorSelection *selection,
                                const gchar *plain_text)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (plain_text != NULL);

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_INSERT_TEXT, plain_text);
}

/**
 * e_editor_selection_insert_html:
 * @selection: an #EEditorSelection
 * @html_text: an HTML code to insert
 *
 * Insert @html_text into document at current cursor position. When a text range
 * is selected, it will be replaced by @html_text.
 */
void
e_editor_selection_insert_html (EEditorSelection *selection,
                                const gchar *html_text)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (html_text != NULL);

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_INSERT_HTML, html_text);
}

/**
 * e_editor_selection_insert_image:
 * @selection: an #EEditorSelection
 * @image_uri: an URI of the source image
 *
 * Inserts image at current cursor position using @image_uri as source. When a
 * text range is selected, it will be replaced by the image.
 */
void
e_editor_selection_insert_image (EEditorSelection *selection,
                                 const gchar *image_uri)
{
	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (image_uri != NULL);

	e_editor_widget_exec_command (
		E_EDITOR_WIDGET (selection->priv->webview),
		E_EDITOR_WIDGET_COMMAND_INSERT_IMAGE, image_uri);
}

static gint
find_where_to_break_line (WebKitDOMNode *node,
                          gint max_len)
{
	gchar *str, *text_start;
	gunichar uc;
	gint pos, last_space;

	text_start = webkit_dom_text_get_whole_text ((WebKitDOMText *) node);

	pos = 0;
	last_space = 0;
	str = text_start;
	do {
		uc = g_utf8_get_char (str);
		if (!uc) {
			g_free (text_start);
			if (pos <= max_len) {
				return pos;
			} else {
				return last_space;
			}
		}

		/* If last_space is zero then the word is longer than
		 * WORD_WRAP_LENGTH characters, so continue untill we find
		 * a space */
		if ((pos > max_len) && (last_space > 0)) {
			g_free (text_start);
			return last_space;
		}

		if (g_unichar_isspace (uc)) {
			last_space = pos;
		}

		pos += 1;
		str = g_utf8_next_char (str);
	} while (*str);

	g_free (text_start);
	return max_len;
}

/**
 * e_editor_selection_wrap_lines:
 * @selection: an #EEditorSelection
 *
 * Wraps all lines in current selection to be 71 characters long.
 */
void
e_editor_selection_wrap_lines (EEditorSelection *selection)
{
	WebKitDOMRange *range;
	WebKitDOMNode *node, *start_node;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMDocumentFragment *fragment;
	gint len;
	gchar *html;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	range = editor_selection_get_current_range (selection);

	/* Extend the range to include entire nodes */
	webkit_dom_range_select_node_contents (
		range,
		webkit_dom_range_get_common_ancestor_container (range, NULL),
		NULL);

	/* Copy the selection from DOM, wrap the lines and then paste it back
	 * using the DOM command which will overwrite the selection, and
	 * record it as an undoable action */
	fragment = webkit_dom_range_clone_contents (range, NULL);
	node = WEBKIT_DOM_NODE (fragment);

	start_node = node;
	len = 0;
	while (node) {
		/* Find nearest text node */
		if (webkit_dom_node_get_node_type (node) != 3) {
			if (webkit_dom_node_has_child_nodes (node)) {
				node = webkit_dom_node_get_first_child (node);
			} else if (webkit_dom_node_get_next_sibling (node)) {
				node = webkit_dom_node_get_next_sibling (node);
			} else {
				if (webkit_dom_node_is_equal_node (node, start_node)) {
					break;
				}

				node = webkit_dom_node_get_parent_node (node);
				if (node) {
					node = webkit_dom_node_get_next_sibling (node);
				}
			}
			continue;
		}

		/* If length of this node + what we already have is still less
		 * then 71 characters, then just join it and continue to next
		 * node */
		if ((webkit_dom_character_data_get_length (
			(WebKitDOMCharacterData *) node) + len) < WORD_WRAP_LENGTH) {

			len += webkit_dom_character_data_get_length (
				(WebKitDOMCharacterData *) node);

		} else {
			gint offset;

			/* Find where we can line-break the node so that it
			 * effectively fills the rest of current row */
			offset = find_where_to_break_line (node, WORD_WRAP_LENGTH - len);

			if (offset > 0) {
				/* Split the node and append <BR> tag to it */
				webkit_dom_text_split_text (
					(WebKitDOMText *) node, len + offset, NULL);

				element = webkit_dom_document_create_element (
					document, "BR", NULL);

				/* WebKit throws warning when ref_child is NULL */
				if (webkit_dom_node_get_next_sibling (node)) {
					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						webkit_dom_node_get_next_sibling (node),
						NULL);
				} else {
					webkit_dom_node_append_child (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						NULL);
				}

				len = 0;
			}
		}

		/* Skip to next node */
		if (webkit_dom_node_get_next_sibling (node)) {
			node = webkit_dom_node_get_next_sibling (node);
		} else {
			if (webkit_dom_node_is_equal_node (node, start_node)) {
				break;
			}

			node = webkit_dom_node_get_parent_node (node);
			if (node) {
				node = webkit_dom_node_get_next_sibling (node);
			}
		}
	}

	/* Create a wrapper DIV and put the processed content into it */
	element = webkit_dom_document_create_element (document, "DIV", NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (element), WEBKIT_DOM_NODE (start_node), NULL);

	/* Get HTML code of the processed content */
	html = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element));

	/* Overwrite the current selection be the processed content, so that
	 * "UNDO" and "REDO" buttons work as expected */
	e_editor_selection_insert_html (selection, html);

	g_free (html);
}

/**
 * e_editor_selection_save:
 * @selection: an #EEditorSelection
 *
 * Saves current cursor position or current selection range. The selection can
 * be later restored by calling e_editor_selection_restore().
 *
 * Note that calling e_editor_selection_save() overwrites previously saved
 * position.
 *
 * Note that this method inserts special markings into the HTML code that are
 * used to later restore the selection. It can happen that by deleting some
 * segments of the document some of the markings are deleted too. In that case
 * restoring the selection by e_editor_selection_restore() can fail. Also by
 * moving text segments (Cut & Paste) can result in moving the markings
 * elsewhere, thus e_editor_selection_restore() will restore the selection
 * incorrectly.
 *
 * It is recommended to use this method only when you are not planning to make
 * bigger changes to content or structure of the document (formatting changes
 * are usually OK).
 */
void
e_editor_selection_save (EEditorSelection *selection)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	WebKitDOMNode *container;
	WebKitDOMElement *marker;
	WebKitDOMNode *marker_node;
	WebKitDOMNode *parent_node;
	WebKitDOMNode *split_node;
	glong start_offset;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	document = webkit_web_view_get_dom_document (selection->priv->webview);

	/* First remove all markers (if present) */
	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evolution-selection-start-marker");
	if (marker != NULL) {
		WebKitDOMNode *marker_node;
		WebKitDOMNode *parent_node;

		marker_node = WEBKIT_DOM_NODE (marker);
		parent_node = webkit_dom_node_get_parent_node (marker_node);
		webkit_dom_node_remove_child (parent_node, marker_node, NULL);
	}

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evolution-selection-end-marker");
	if (marker != NULL) {
		WebKitDOMNode *marker_node;
		WebKitDOMNode *parent_node;

		marker_node = WEBKIT_DOM_NODE (marker);
		parent_node = webkit_dom_node_get_parent_node (marker_node);
		webkit_dom_node_remove_child (parent_node, marker_node, NULL);
	}

	range = editor_selection_get_current_range (selection);

	start_offset = webkit_dom_range_get_start_offset (range, NULL);

	marker = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_html_element_set_id (
		WEBKIT_DOM_HTML_ELEMENT (marker),
		"-x-evolution-selection-start-marker");

	container = webkit_dom_range_get_start_container (range, NULL);
	if (WEBKIT_DOM_IS_TEXT (container)) {
		WebKitDOMText *split_text;

		split_text = webkit_dom_text_split_text (
			WEBKIT_DOM_TEXT (container), start_offset, NULL);
		split_node = WEBKIT_DOM_NODE (split_text);
	} else {
		split_node = container;
	}

	marker_node = WEBKIT_DOM_NODE (marker);
	parent_node = webkit_dom_node_get_parent_node (container);

	webkit_dom_node_insert_before (
		parent_node, marker_node, split_node, NULL);

	marker = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_html_element_set_id (
		WEBKIT_DOM_HTML_ELEMENT (marker),
		"-x-evolution-selection-end-marker");

	container = webkit_dom_range_get_end_container (range, NULL);
	if (WEBKIT_DOM_IS_TEXT (container)) {
		WebKitDOMText *split_text;

		split_text = webkit_dom_text_split_text (
			WEBKIT_DOM_TEXT (container), start_offset, NULL);
		split_node = WEBKIT_DOM_NODE (split_text);
	} else {
		split_node = container;
	}

	marker_node = WEBKIT_DOM_NODE (marker);
	parent_node = webkit_dom_node_get_parent_node (container);

	webkit_dom_node_insert_before (
		parent_node, marker_node, split_node, NULL);
}

/**
 * e_editor_selection_restore:
 * @selection: an #EEditorSelection
 *
 * Restores cursor position or selection range that was saved by
 * e_editor_selection_save().
 *
 * Note that calling this function without calling e_editor_selection_save()
 * before is a programming error and the behavior is undefined.
 */
void
e_editor_selection_restore (EEditorSelection *selection)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	WebKitDOMElement *marker;
	WebKitDOMNode *marker_node;
	WebKitDOMNode *parent_node;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	range = editor_selection_get_current_range (selection);

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evolution-selection-start-marker");
	g_return_if_fail (marker != NULL);

	marker_node = WEBKIT_DOM_NODE (marker);
	parent_node = webkit_dom_node_get_parent_node (marker_node);

	webkit_dom_range_set_start_after (range, marker_node, NULL);
	webkit_dom_node_remove_child (parent_node, marker_node, NULL);

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evolution-selection-end-marker");
	g_return_if_fail (marker != NULL);

	marker_node = WEBKIT_DOM_NODE (marker);
	parent_node = webkit_dom_node_get_parent_node (marker_node);

	webkit_dom_range_set_end_before (range, marker_node, NULL);
	webkit_dom_node_remove_child (parent_node, marker_node, NULL);
}

static void
editor_selection_modify (EEditorSelection *selection,
                         const gchar *alter,
                         gboolean forward,
                         EEditorSelectionGranularity granularity)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	const gchar *granularity_str;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	switch (granularity) {
		case E_EDITOR_SELECTION_GRANULARITY_CHARACTER:
			granularity_str = "character";
			break;
		case E_EDITOR_SELECTION_GRANULARITY_WORD:
			granularity_str = "word";
			break;
	}

	webkit_dom_dom_selection_modify (
		dom_selection, alter,
		forward ? "forward" : "backward",
		granularity_str);
}

/**
 * e_editor_selection_extend:
 * @selection: an #EEditorSelection
 * @forward: whether to extend selection forward or backward
 * @granularity: granularity of the extension
 *
 * Extends current selection in given direction by given granularity.
 */
void
e_editor_selection_extend (EEditorSelection *selection,
                           gboolean forward,
                           EEditorSelectionGranularity granularity)
{
	editor_selection_modify (selection, "extend", forward, granularity);
}

/**
 * e_editor_selection_move:
 * @selection: an #EEditorSelection
 * @forward: whether to move the selection forward or backward
 * @granularity: granularity of the movement
 *
 * Moves current selection in given direction by given granularity
 */
void
e_editor_selection_move (EEditorSelection *selection,
                         gboolean forward,
                         EEditorSelectionGranularity granularity)
{
	editor_selection_modify (selection, "move", forward, granularity);
}
