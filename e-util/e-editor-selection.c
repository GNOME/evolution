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

#define UNICODE_HIDDEN_SPACE "\xe2\x80\x8b"

/**
 * EEditorSelection:
 *
 * The #EEditorSelection object represents current position of the cursor
 * with the editor or current text selection within the editor. To obtain
 * valid #EEditorSelection, call e_editor_widget_get_selection().
 */

struct _EEditorSelectionPrivate {

	GWeakRef editor_widget;
	gulong selection_changed_handler_id;

	gchar *text;

	gboolean is_bold;
	gboolean is_italic;
	gboolean is_underline;
	gboolean is_monospaced;
	gboolean is_strike_through;

	gchar *background_color;
	gchar *font_color;
	gchar *font_family;

	gulong selection_offset;

	gint word_wrap_length;
	guint font_size;

	EEditorSelectionAlignment alignment;
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
	EEditorWidget *editor_widget;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range = NULL;

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_val_if_fail (editor_widget != NULL, NULL);

	web_view = WEBKIT_WEB_VIEW (editor_widget);

	document = webkit_web_view_get_dom_document (web_view);
	window = webkit_dom_document_get_default_view (document);
	if (!window)
		goto exit;

	dom_selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1)
		goto exit;

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

exit:
	g_object_unref (editor_widget);

	return range;
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
	if (!range)
		return NULL;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	element = e_editor_dom_node_find_parent_element (node, "FONT");
	if (!element)
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
	gulong handler_id;

	g_return_if_fail (E_IS_EDITOR_WIDGET (editor_widget));

	g_weak_ref_set (&selection->priv->editor_widget, editor_widget);

	handler_id = g_signal_connect (
		editor_widget, "selection-changed",
		G_CALLBACK (editor_selection_selection_changed_cb),
		selection);

	selection->priv->selection_changed_handler_id = handler_id;
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

		case PROP_EDITOR_WIDGET:
			g_value_take_object (
				value,
				e_editor_selection_ref_editor_widget (
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
	EEditorWidget *editor_widget;

	priv = E_EDITOR_SELECTION_GET_PRIVATE (object);

	editor_widget = g_weak_ref_get (&priv->editor_widget);
	if (editor_widget != NULL) {
		g_signal_handler_disconnect (
			editor_widget, priv->selection_changed_handler_id);
		priv->selection_changed_handler_id = 0;
		g_object_unref (editor_widget);
	}

	g_weak_ref_set (&priv->editor_widget, NULL);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_editor_selection_parent_class)->dispose (object);
}

static void
editor_selection_finalize (GObject *object)
{
	EEditorSelection *selection = E_EDITOR_SELECTION (object);

	g_free (selection->priv->text);
	g_free (selection->priv->background_color);
	g_free (selection->priv->font_color);
	g_free (selection->priv->font_family);

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
			G_PARAM_READWRITE |
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
	GSettings *g_settings;

	selection->priv = E_EDITOR_SELECTION_GET_PRIVATE (selection);

	g_settings = g_settings_new ("org.gnome.evolution.mail");
	selection->priv->word_wrap_length = g_settings_get_int (g_settings, "composer-word-wrap-length");
	g_object_unref (g_settings);
}

/**
 * e_editor_selection_ref_editor_widget:
 * @selection: an #EEditorSelection
 *
 * Returns a new reference to @selection's #EEditorWidget.  Unreference
 * the #EEditorWidget with g_object_unref() when finished with it.
 *
 * Returns: an #EEditorWidget
 **/
EEditorWidget *
e_editor_selection_ref_editor_widget (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), NULL);

	return g_weak_ref_get (&selection->priv->editor_widget);
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
	EEditorWidget *editor_widget;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (replacement != NULL);

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	web_view = WEBKIT_WEB_VIEW (editor_widget);

	range = editor_selection_get_current_range (selection);
	document = webkit_web_view_get_dom_document (web_view);
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	webkit_dom_range_expand (range, "word", NULL);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	e_editor_selection_insert_html (selection, replacement);

	g_object_unref (editor_widget);
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
e_editor_selection_get_string(EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), NULL);

	range = editor_selection_get_current_range (selection);
	if (!range)
		return NULL;

	g_free (selection->priv->text);
	selection->priv->text = webkit_dom_range_get_text (selection->priv->range);

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
	EEditorWidget *editor_widget;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	e_editor_widget_exec_command (
		editor_widget,
		E_EDITOR_WIDGET_COMMAND_INSERT_TEXT,
		new_string);

	g_object_unref (editor_widget);
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
	if (!range)
		return E_EDITOR_SELECTION_ALIGNMENT_LEFT;

	node = webkit_dom_range_get_start_container (range, NULL);
	if (!node)
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
	EEditorWidget *editor_widget;
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

	selection->priv->alignment = alignment;

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	e_editor_widget_exec_command (editor_widget, command, NULL);

	g_object_unref (editor_widget);

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
	WebKitDOMCSSStyleDeclaration *css;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), NULL);

	ancestor = webkit_dom_range_get_common_ancestor_container (
			selection->priv->range, NULL);

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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (color != NULL && *color != '\0');

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_BACKGROUND_COLOR;
	e_editor_widget_exec_command (editor_widget, command, color);

	g_object_unref (editor_widget);

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
	if (!range)
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
	EEditorWidget *editor_widget;
	EEditorSelectionBlockFormat current_format;
	EEditorWidgetCommand command;
	const gchar *value;
	gboolean inserting_ordered_list = FALSE;

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
			inserting_ordered_list = TRUE;
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

	/* H1 - H6 have bold font by default */
	if (format >= E_EDITOR_SELECTION_BLOCK_FORMAT_H1 && format <= E_EDITOR_SELECTION_BLOCK_FORMAT_H6)
		selection->priv->is_bold = TRUE;

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	/* First remove (un)ordered list before changing formatting */
	if (current_format == E_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST) {
		e_editor_widget_exec_command (
			editor_widget,
			E_EDITOR_WIDGET_COMMAND_INSERT_UNORDERED_LIST, NULL);
		/*		    ^-- not a typo, "insert" toggles the
		 *			formatting if already present */
	} else if (current_format >= E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST) {
		e_editor_widget_exec_command (
			editor_widget,
			E_EDITOR_WIDGET_COMMAND_INSERT_ORDERED_LIST, NULL);
	}

	if (!inserting_ordered_list)
		e_editor_widget_exec_command (editor_widget, command, value);
	else {
		/* Ordered list have to be handled separately (not directly in WebKit),
		 * because all variant or ordered list are treated as one in WebKit.
		 * When list is inserted after existing one, it is not inserted as new,
		 * but it will start to continuing in previous one */
		WebKitDOMRange *range, *new_range;
		WebKitDOMElement *element;
		WebKitDOMNode *node;
		WebKitWebView *web_view;
		WebKitDOMDocument *document;
		WebKitDOMDOMSelection *window_selection;
		WebKitDOMDOMWindow *window;
		WebKitDOMNodeList *list;
		gint paragraph_count, ii;

		web_view = WEBKIT_WEB_VIEW (editor_widget);
		document = webkit_web_view_get_dom_document (web_view);

		range = editor_selection_get_current_range (selection);

		list = webkit_dom_document_get_elements_by_class_name (document, "-x-evo-paragraph");

		paragraph_count = webkit_dom_node_list_get_length (list);
		for (ii = paragraph_count - 1; ii >= 0; ii--) {
		WebKitDOMNode *br = webkit_dom_node_list_item (list, ii);
			if (g_strcmp0 (webkit_dom_node_get_text_content (br), UNICODE_HIDDEN_SPACE) == 0) {
				webkit_dom_node_set_text_content (br, "", NULL);
			}
		}

		/* Create list elements */
		element = webkit_dom_document_create_element (document, "OL", NULL);

		/* We have to use again the hidden space to move caret into newly
		 * inserted list */
		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (element),
			g_strconcat ("<li>", UNICODE_HIDDEN_SPACE, "</li>", NULL),
			NULL);

		if (format != E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST) {
			webkit_dom_element_set_attribute (
				element, "type",
				(format == E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA) ?
					"A" : "I", NULL);
		}
		webkit_dom_range_insert_node (range, WEBKIT_DOM_NODE (element), NULL);

		/* Move caret into newly inserted list */
		window = webkit_dom_document_get_default_view (document);
		window_selection = webkit_dom_dom_window_get_selection (window);
		new_range = webkit_dom_document_create_range (document);

		webkit_dom_range_select_node_contents (
			new_range, WEBKIT_DOM_NODE (element), NULL);
		webkit_dom_range_collapse (new_range, FALSE, NULL);
		webkit_dom_dom_selection_remove_all_ranges (window_selection);
		webkit_dom_dom_selection_add_range (window_selection, new_range);

		/* Remove hidden space character */
		node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element));
		webkit_dom_html_element_set_inner_text (WEBKIT_DOM_HTML_ELEMENT (node), "", NULL);
	}

	/* Fine tuning - set the specific marker type for ordered lists */
	if ((format == E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA) ||
	    (format == E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN)) {

		WebKitDOMRange *range;
		WebKitDOMNode *node;
		WebKitDOMElement *list;

		range = editor_selection_get_current_range (selection);
		node = webkit_dom_range_get_start_container (range, NULL);

		list = e_editor_dom_node_find_child_element (node, "OL");
		if (!list)
			list = e_editor_dom_node_find_parent_element (node, "OL");

		if (list) {
			webkit_dom_element_set_attribute (
				list, "type",
				(format == E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA) ?
					"A" : "I", NULL);
		}
	}

	/* If we were inserting new paragraph, mark it for wrapping */
	if (format == E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH) {
		WebKitDOMRange *range;
		WebKitDOMNode *node;
		WebKitDOMElement *paragraph;

		range = editor_selection_get_current_range (selection);
		node = webkit_dom_range_get_start_container (range, NULL);

		paragraph = e_editor_dom_node_find_child_element (node, "P");
		if (!paragraph)
			paragraph = e_editor_dom_node_find_parent_element (node, "P");

		if (paragraph) {
			webkit_dom_element_set_class_name (WEBKIT_DOM_ELEMENT (paragraph),
							   "-x-evo-paragraph");
		}
	}

	g_object_unref (editor_widget);

	/* When changing the format we need to re-set the alignment */
	e_editor_selection_set_alignment (selection, selection->priv->alignment);

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

	if (g_strcmp0 (e_editor_selection_get_string (selection), "") == 0) {
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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;
	guint32 rgba_value;
	gchar *color;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (!rgba)
		rgba = &black;

	rgba_value = e_rgba_to_value ((GdkRGBA *) rgba);

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_FORE_COLOR;
	color = g_strdup_printf ("#%06x", rgba_value);
	selection->priv->font_color = g_strdup (color);
	e_editor_widget_exec_command (editor_widget, command, color);
	g_free (color);

	g_object_unref (editor_widget);

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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_FONT_NAME;
	e_editor_widget_exec_command (editor_widget, command, font_name);

	g_object_unref (editor_widget);

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
	guint size_int;

	g_return_val_if_fail (
		E_IS_EDITOR_SELECTION (selection),
		E_EDITOR_SELECTION_FONT_SIZE_NORMAL);

	if (g_strcmp0 (e_editor_selection_get_string (selection), "") == 0) {
		size_int = selection->priv->font_size;
	} else {
		gchar *size = get_font_property (selection, "size");
		if (!size) {
			return E_EDITOR_SELECTION_FONT_SIZE_NORMAL;
		}

		size_int = atoi (size);
		g_free (size);
	}

	if (size_int == 0)
		return E_EDITOR_SELECTION_FONT_SIZE_NORMAL;

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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;
	gchar *size_str;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	selection->priv->font_size = font_size;
	command = E_EDITOR_WIDGET_COMMAND_FONT_SIZE;
	size_str = g_strdup_printf ("%d", font_size);
	e_editor_widget_exec_command (editor_widget, command, size_str);
	g_free (size_str);

	g_object_unref (editor_widget);

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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_INDENT;
	e_editor_widget_exec_command (editor_widget, command, NULL);

	g_object_unref (editor_widget);

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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_OUTDENT;
	e_editor_widget_exec_command (editor_widget, command, NULL);

	g_object_unref (editor_widget);

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
	gboolean ret_val;
	gchar *value;
	EEditorWidget *editor_widget;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_val_if_fail (editor_widget != NULL, FALSE);

	range = editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (editor_widget));
	window = webkit_dom_document_get_default_view (document);

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set bold property,
	 * otherwise it will be turned off because of no text in composer */
	if (g_strcmp0 (webkit_dom_node_get_text_content (node), "") == 0)
		return selection->priv->is_bold;

	style = webkit_dom_dom_window_get_computed_style (
			window, webkit_dom_node_get_parent_element (node), NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "font-weight");

	if (g_strstr_len (value, -1, "normal"))
		ret_val = FALSE;
	else
		ret_val = TRUE;

	g_free (value);
	return ret_val;
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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_bold (selection) == bold)
		return;

	selection->priv->is_bold = bold;

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_BOLD;
	e_editor_widget_exec_command (editor_widget, command, NULL);

	g_object_unref (editor_widget);

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
	gboolean ret_val;
	gchar *value;
	EEditorWidget *editor_widget;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_val_if_fail (editor_widget != NULL, FALSE);

	range = editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (editor_widget));
	window = webkit_dom_document_get_default_view (document);

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set italic property,
	 * otherwise it will be turned off because of no text in composer */
	if (g_strcmp0 (webkit_dom_node_get_text_content (node), "") == 0)
		return selection->priv->is_italic;

	style = webkit_dom_dom_window_get_computed_style (
			window, webkit_dom_node_get_parent_element (node), NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "font-style");

	if (g_strstr_len (value, -1, "italic"))
		ret_val = TRUE;
	else
		ret_val = FALSE;

	g_free (value);
	return ret_val;
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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_italic (selection) == italic)
		return;

	selection->priv->is_italic = italic;

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_ITALIC;
	e_editor_widget_exec_command (editor_widget, command, NULL);

	g_object_unref (editor_widget);

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
	WebKitDOMRange *range;
	WebKitDOMNode *node;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	range = editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set monospaced property,
	 * otherwise it will be turned off because of no text in composer */
	if (g_strcmp0 (webkit_dom_node_get_text_content (node), "") == 0)
		return selection->priv->is_monospaced;

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
	EEditorWidget *editor_widget;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *window_selection;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_monospaced (selection) == monospaced)
		return;

	selection->priv->is_monospaced = monospaced;

	range = editor_selection_get_current_range (selection);
	if (!range)
		return;

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	web_view = WEBKIT_WEB_VIEW (editor_widget);

	document = webkit_web_view_get_dom_document (web_view);
	window = webkit_dom_document_get_default_view (document);
	window_selection = webkit_dom_dom_window_get_selection (window);

	if (monospaced) {
		if (g_strcmp0 (e_editor_selection_get_string (selection), "") != 0) {
			gchar *html;
			WebKitDOMElement *wrapper;

			wrapper = webkit_dom_document_create_element (document, "TT", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (wrapper),
				WEBKIT_DOM_NODE (webkit_dom_range_clone_contents (range, NULL)),
				NULL);

			html = webkit_dom_html_element_get_outer_html (
				WEBKIT_DOM_HTML_ELEMENT (wrapper));
			e_editor_selection_insert_html (selection, html);

			g_free (html);
		} else {
			WebKitDOMElement *tt_element;

			tt_element = webkit_dom_document_create_element (document, "TT", NULL);
			/* https://bugs.webkit.org/show_bug.cgi?id=15256 */
			webkit_dom_html_element_set_inner_html (WEBKIT_DOM_HTML_ELEMENT (tt_element), UNICODE_HIDDEN_SPACE, NULL);
			webkit_dom_range_insert_node (range, WEBKIT_DOM_NODE (tt_element), NULL);
			webkit_dom_range_collapse (range, FALSE, NULL);

			webkit_dom_dom_selection_remove_all_ranges (window_selection);
			webkit_dom_dom_selection_add_range (window_selection, range);
		}
	} else {
		if (g_strcmp0 (e_editor_selection_get_string (selection), "") != 0) {
			EEditorWidgetCommand command;

			/* XXX This removes _all_ formatting that the selection has.
			 *     In theory it's possible to write a code that would
			 *     remove the <TT> from selection using advanced DOM
			 *     manipulation, but right now I don't really feel like
			 *     writing it all. */
			command = E_EDITOR_WIDGET_COMMAND_REMOVE_FORMAT;
			e_editor_widget_exec_command (editor_widget, command, NULL);
		} else {
			WebKitDOMElement *tt_element;
			WebKitDOMRange *new_range;
			WebKitDOMNode *node;
			gchar *outer_html, *inner_html, *new_inner_html;
			GRegex *regex;

			node = webkit_dom_range_get_end_container (range, NULL);
			tt_element = webkit_dom_node_get_parent_element (node);

			if (g_strcmp0 (webkit_dom_element_get_tag_name (tt_element), "TT") != 0)
				return;

			regex = g_regex_new (UNICODE_HIDDEN_SPACE, 0, 0, NULL);
			if (!regex)
				return;

			webkit_dom_html_element_set_id (WEBKIT_DOM_HTML_ELEMENT (tt_element), "ev-tt");

			inner_html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (tt_element));
			new_inner_html = g_regex_replace_literal (regex, inner_html, -1, 0, "", 0, NULL);
			webkit_dom_html_element_set_inner_html (WEBKIT_DOM_HTML_ELEMENT (tt_element), new_inner_html, NULL);

		        outer_html = webkit_dom_html_element_get_outer_html (WEBKIT_DOM_HTML_ELEMENT (tt_element));
			webkit_dom_html_element_set_outer_html (WEBKIT_DOM_HTML_ELEMENT (tt_element), g_strconcat (outer_html, UNICODE_HIDDEN_SPACE, NULL), NULL);
			/* We need to get that element again */
			tt_element = webkit_dom_document_get_element_by_id (document, "ev-tt");
			webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (tt_element), "id");

			new_range = webkit_dom_document_create_range (document);
			webkit_dom_range_set_start_after (new_range, WEBKIT_DOM_NODE (tt_element), NULL);
			webkit_dom_range_set_end_after (new_range, WEBKIT_DOM_NODE (tt_element), NULL);

			webkit_dom_dom_selection_remove_all_ranges (window_selection);
			webkit_dom_dom_selection_add_range (window_selection, new_range);

			webkit_dom_dom_selection_modify (window_selection, "move", "right", "character");

			g_regex_unref (regex);
			g_free (inner_html);
			g_free (new_inner_html);
			g_free (outer_html);
		}
	}

	g_object_unref (editor_widget);

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
	gboolean ret_val;
	gchar *value;
	EEditorWidget *editor_widget;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_val_if_fail (editor_widget != NULL, FALSE);

	range = editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (editor_widget));
	window = webkit_dom_document_get_default_view (document);

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set strike-through property,
	 * otherwise it will be turned off because of no text in composer */
	if (g_strcmp0 (webkit_dom_node_get_text_content (node), "") == 0)
		return selection->priv->is_strike_through;

	style = webkit_dom_dom_window_get_computed_style (
			window, webkit_dom_node_get_parent_element (node), NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-decoration");

	if (g_strstr_len (value, -1, "line-through"))
		ret_val = TRUE;
	else
		ret_val = get_has_style (selection, "strike");

	g_free (value);
	return ret_val;
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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_strike_through (selection) == strike_through)
		return;

	selection->priv->is_strike_through = strike_through;

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_STRIKETHROUGH;
	e_editor_widget_exec_command (editor_widget, command, NULL);

	g_object_unref (editor_widget);

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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_subscript (selection) == subscript)
		return;

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_SUBSCRIPT;
	e_editor_widget_exec_command (editor_widget, command, NULL);

	g_object_unref (editor_widget);

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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_superscript (selection) == superscript)
		return;

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_SUPERSCRIPT;
	e_editor_widget_exec_command (editor_widget, command, NULL);

	g_object_unref (editor_widget);

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
	gboolean ret_val;
	gchar *value;
	EEditorWidget *editor_widget;
	WebKitDOMCSSStyleDeclaration *style;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *node;
	WebKitDOMRange *range;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_val_if_fail (editor_widget != NULL, FALSE);

	range = editor_selection_get_current_range (selection);
	if (!range)
		return FALSE;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (editor_widget));
	window = webkit_dom_document_get_default_view (document);

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	/* If we are changing the format of block we have to re-set underline property,
	 * otherwise it will be turned off because of no text in composer */
	if (g_strcmp0 (webkit_dom_node_get_text_content (node), "") == 0)
		return selection->priv->is_underline;

	style = webkit_dom_dom_window_get_computed_style (
			window, webkit_dom_node_get_parent_element (node), NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-decoration");

	if (g_strstr_len (value, -1, "underline"))
		ret_val = TRUE;
	else
		ret_val = get_has_style (selection, "u");

	g_free (value);
	return ret_val;
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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_is_underline (selection) == underline)
		return;

	selection->priv->is_underline = underline;

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_UNDERLINE;
	e_editor_widget_exec_command (editor_widget, command, NULL);

	g_object_unref (editor_widget);

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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *link;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (editor_widget));
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	link = e_editor_dom_node_find_parent_element (
			webkit_dom_range_get_start_container (range, NULL), "A");

	if (!link) {
		gchar *text;
		/* get element that was clicked on */
		link = e_editor_widget_get_element_under_mouse_click (editor_widget);
		if (!WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (link))
			link = NULL;

		text = webkit_dom_html_element_get_inner_text (
				WEBKIT_DOM_HTML_ELEMENT (link));
		webkit_dom_html_element_set_outer_html (WEBKIT_DOM_HTML_ELEMENT (link), text, NULL);
		g_free (text);
	} else {
		command = E_EDITOR_WIDGET_COMMAND_UNLINK;
		e_editor_widget_exec_command (editor_widget, command, NULL);
	}
	g_object_unref (editor_widget);
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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (uri != NULL && *uri != '\0');

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_CREATE_LINK;
	e_editor_widget_exec_command (editor_widget, command, uri);

	g_object_unref (editor_widget);
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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (plain_text != NULL);

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_INSERT_TEXT;
	e_editor_widget_exec_command (editor_widget, command, plain_text);

	g_object_unref (editor_widget);
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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (html_text != NULL);

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_INSERT_HTML;
	e_editor_widget_exec_command (editor_widget, command, html_text);

	g_object_unref (editor_widget);
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
	EEditorWidget *editor_widget;
	EEditorWidgetCommand command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (image_uri != NULL);

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	command = E_EDITOR_WIDGET_COMMAND_INSERT_IMAGE;
	e_editor_widget_exec_command (editor_widget, command, image_uri);

	g_object_unref (editor_widget);
}

/**
 * e_editor_selection_clear_caret_position_marker:
 * @selection: an #EEditorSelection
 *
 * Removes previously set caret position marker from composer.
 */
void
e_editor_selection_clear_caret_position_marker (EEditorSelection *selection)
{
	EEditorWidget *widget;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (widget != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-caret-position");

	if (element) {
		webkit_dom_node_remove_child (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			WEBKIT_DOM_NODE (element),
			NULL);
	}

	g_object_unref (widget);
}

/**
 * e_editor_selection_save_caret_position:
 * @selection: an #EEditorSelection
 *
 * Saves current caret position in composer.
 */
void
e_editor_selection_save_caret_position (EEditorSelection *selection)
{
	EEditorWidget *widget;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMNode *split_node;
	WebKitDOMNode *start_offset_node;
	WebKitDOMRange *range;
	gulong start_offset;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (widget != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	e_editor_selection_clear_caret_position_marker (selection);

	range = editor_selection_get_current_range (selection);
	if (!range)
		return;

	start_offset = webkit_dom_range_get_start_offset (range, NULL);
	start_offset_node = webkit_dom_range_get_end_container (range, NULL);

	element	= webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_html_element_set_id (WEBKIT_DOM_HTML_ELEMENT (element), "-x-evo-caret-position");
	webkit_dom_html_element_set_inner_html (WEBKIT_DOM_HTML_ELEMENT (element), "*", NULL);

	if (WEBKIT_DOM_IS_TEXT (start_offset_node)) {
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
		WEBKIT_DOM_NODE (element),
		split_node,
		NULL);

	g_object_unref (widget);
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
 * e_editor_selection_restore_caret_position:
 * @selection: an #EEditorSelection
 *
 * Restores previously saved caret position in composer.
 */
void
e_editor_selection_restore_caret_position (EEditorSelection *selection)
{
	EEditorWidget *widget;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (widget != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	element = webkit_dom_document_get_element_by_id (document, "-x-evo-caret-position");

	if (element) {
		move_caret_into_element (document, element);
		webkit_dom_node_remove_child (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			WEBKIT_DOM_NODE (element),
			NULL);
	}

	g_object_unref (widget);
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
		 * word_wrap_length characters, so continue untill we find
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
			g_free (text_start);
			return last_space;
		}

		if (g_unichar_isspace (uc)) {
			last_space = pos;
		}

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

static gboolean
is_caret_position_node (WebKitDOMNode *node)
{
	return (WEBKIT_DOM_IS_HTML_ELEMENT (node) &&
			(g_strcmp0 (webkit_dom_html_element_get_id (WEBKIT_DOM_HTML_ELEMENT (node)),
				    "-x-evo-caret-position")) == 0);
}

static void
wrap_lines (EEditorSelection *selection,
	    WebKitDOMNode *paragraph,
	    WebKitWebView *web_view,
	    gboolean jump_to_previous_line,
	    gboolean remove_all_br,
	    gint word_wrap_length,
	    gboolean delete_pressed)
{
	WebKitDOMNode *node, *start_node;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMNodeList *wrap_br;
	gint len, ii, br_count;
	gulong length_left;
	glong paragraph_char_count;
	WebKitDOMNode *paragraph_clone;

	document = webkit_web_view_get_dom_document (web_view);

	if (selection) {
		paragraph_char_count = g_utf8_strlen (e_editor_selection_get_string (selection), -1);

		fragment = webkit_dom_range_clone_contents (
				editor_selection_get_current_range (selection),
				NULL);

		/* Select all BR elements or just ours that are used for wrapping.
		 * We are not removing user BR elements when this function is activated
		 * from Format->Wrap Lines action */
		wrap_br = webkit_dom_document_fragment_query_selector_all (
				fragment,
				remove_all_br ? "br" : "br.-x-evo-wrap-br",
				NULL);
	} else {
		paragraph_clone = webkit_dom_node_clone_node (paragraph, TRUE);
		paragraph_char_count = g_utf8_strlen (webkit_dom_node_get_text_content (paragraph_clone), -1);

		wrap_br = webkit_dom_element_query_selector_all (
				WEBKIT_DOM_ELEMENT (paragraph_clone),
				remove_all_br ? "br" : "br.-x-evo-wrap-br",
				NULL);
	}

	/* And remove them */
	br_count = webkit_dom_node_list_get_length (wrap_br);
	for (ii = 0; ii < br_count; ii++) {
		WebKitDOMNode *br = webkit_dom_node_list_item (wrap_br, ii);
		webkit_dom_node_remove_child (
				webkit_dom_node_get_parent_node (br), br, NULL);
	}

	if (selection)
		node = WEBKIT_DOM_NODE (fragment);
	else
		node = webkit_dom_node_get_first_child (paragraph_clone);

	start_node = node;
	len = 0;
	while (node) {
		if (WEBKIT_DOM_IS_TEXT (node)) {
			if (jump_to_previous_line) {
				/* If we need to jump to the previous line (e.g. Backspace pressed
				 * on the beginning of line, we need to remove last character on
				 * the line that is above that line */
				WebKitDOMNode *next_sibling = webkit_dom_node_get_next_sibling (node);

				if (is_caret_position_node (next_sibling)) {
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (node),
						webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node)) - 1,
						1, NULL);
				}
			}

			/* If there is temporary hidden space we remove it */
			if (strstr (webkit_dom_node_get_text_content (node), UNICODE_HIDDEN_SPACE))
				webkit_dom_character_data_delete_data (
					WEBKIT_DOM_CHARACTER_DATA (node), 0, 1, NULL);
		} else {
			/* If element is ANCHOR we wrap it separately */
			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
				glong anchor_length;

				anchor_length = g_utf8_strlen (webkit_dom_node_get_text_content (node), -1);
				if (len + anchor_length > word_wrap_length) {
					element = webkit_dom_document_create_element (
							document, "BR", NULL);
					webkit_dom_element_set_class_name (element, "-x-evo-wrap-br");
					webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (node),
							WEBKIT_DOM_NODE (element),
							node,
							NULL);
					len = anchor_length;
				} else
					len += anchor_length;

				node = webkit_dom_node_get_next_sibling (node);
				continue;
			}

			if (is_caret_position_node (node)) {
				node = webkit_dom_node_get_next_sibling (node);

				/* If Delete key is pressed on the end of line, we need to manually
				 * delete the first character of the line that is below us */
				if (delete_pressed && (paragraph_char_count / (br_count + 1)) > word_wrap_length) {
					WebKitDOMNode *next_text = webkit_dom_node_get_next_sibling (node);
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (next_text), 0, 1, NULL);
				}
				continue;
			}

			/* When we are not removing user-entered BR elements (lines wrapped by user),
			 * we need to skip those elements */
			if (!remove_all_br && g_strcmp0 (webkit_dom_node_get_local_name (node), "br") == 0) {
				if (!g_strcmp0 (webkit_dom_element_get_class_name (WEBKIT_DOM_ELEMENT (node)), "-x-evo-wrap-br") == 0) {
					len = 0;
					node = webkit_dom_node_get_next_sibling (node);
					continue;
				}
			}

			/* Find nearest text node */
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
			continue;
		}

		/* If length of this node + what we already have is still less
		 * then word_wrap_length characters, then just join it and continue to next
		 * node */
		length_left = webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node));

		if ((length_left + len) < word_wrap_length) {
			len += length_left;
		} else {
			gint offset = 0;

			/* wrap until we have something */
			while ((length_left + len) > word_wrap_length) {
				/* Find where we can line-break the node so that it
				 * effectively fills the rest of current row */
				offset = find_where_to_break_line (node, word_wrap_length - len, word_wrap_length);

				element = webkit_dom_document_create_element (document, "BR", NULL);
				webkit_dom_element_set_class_name (element, "-x-evo-wrap-br");

				if (offset > 0 && offset <= word_wrap_length) {
					if (offset != length_left) {
						webkit_dom_text_split_text (
								WEBKIT_DOM_TEXT (node), offset, NULL);
					}
					if (webkit_dom_node_get_next_sibling (node)) {
						WebKitDOMNode *nd = webkit_dom_node_get_next_sibling (node);
						nd = webkit_dom_node_get_next_sibling (node);
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
					if (offset != length_left) {
						webkit_dom_text_split_text (
								WEBKIT_DOM_TEXT (node), offset + 1, NULL);
					}
					if (webkit_dom_node_get_next_sibling (node)) {
						WebKitDOMNode *nd = webkit_dom_node_get_next_sibling (node);
						nd = webkit_dom_node_get_next_sibling (node);
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
				length_left = webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node));
				len = 0;
			}
			len += length_left - offset;
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

	if (selection) {
		gchar *html;

		/* Create a wrapper DIV and put the processed content into it */
		element = webkit_dom_document_create_element (document, "DIV", NULL);
		webkit_dom_element_set_class_name (WEBKIT_DOM_ELEMENT (element), "-x-evo-paragraph");
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (element),
			WEBKIT_DOM_NODE (start_node),
			NULL);

		/* Get HTML code of the processed content */
		html = webkit_dom_html_element_get_inner_html (WEBKIT_DOM_HTML_ELEMENT (element));

		/* Overwrite the current selection be the processed content */
		e_editor_selection_insert_html (selection, html);

		g_free (html);

	} else {
		/* Replace paragraph with wrapped one */
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (paragraph),
			paragraph_clone,
			paragraph,
			NULL);
	}
}

static gboolean
check_if_previously_wrapped (WebKitDOMDocument *document)
{
	WebKitDOMNode *sibling;
	sibling = WEBKIT_DOM_NODE (webkit_dom_document_get_element_by_id (document, "-x-evo-caret-position"));

	while (sibling) {
		if (sibling && WEBKIT_DOM_IS_ELEMENT (sibling)) {
			if (g_strcmp0 (webkit_dom_element_get_class_name (WEBKIT_DOM_ELEMENT (sibling)), "-x-evo-wrap-br") == 0) {
				return TRUE;
			}
		}
		sibling = webkit_dom_node_get_next_sibling (sibling);
	}
	return FALSE;
}

/**
 * e_editor_selection_wrap_lines:
 * @selection: an #EEditorSelection
 * @while_typing: If true this function is capable to wrap while typing
 * @event: GdkEventKey of pressed key - can be NULL
 *
 * Wraps all lines in current selection to be 71 characters long.
 */
void
e_editor_selection_wrap_lines (EEditorSelection *selection,
			       gboolean while_typing,
			       GdkEventKey *event)
{
	EEditorWidget *editor_widget;
	WebKitWebView *web_view;
	WebKitDOMRange *range;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *window_selection;
	WebKitDOMElement *active_paragraph;
	gboolean adding = FALSE;
	gboolean backspace_pressed = FALSE;
	gboolean return_pressed = FALSE;
	gboolean delete_pressed = FALSE;
	gboolean jump_to_previous_line = FALSE;
	gboolean previously_wrapped = FALSE;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	if (event != NULL) {
		if ((event->keyval == GDK_KEY_Return) ||
		    (event->keyval == GDK_KEY_Linefeed) ||
		    (event->keyval == GDK_KEY_KP_Enter)) {

			return_pressed = TRUE;
		}

		if (event->keyval == GDK_KEY_Delete)
			delete_pressed = TRUE;

		if (return_pressed || (event->keyval == GDK_KEY_space))
			adding = TRUE;

		if (event->keyval == GDK_KEY_BackSpace)
			backspace_pressed = TRUE;
	}

	web_view = WEBKIT_WEB_VIEW (editor_widget);
	document = webkit_web_view_get_dom_document (web_view);
	window = webkit_dom_document_get_default_view (document);

	if (while_typing) {
		WebKitDOMNode *end_container;
		WebKitDOMNode *parent;
		WebKitDOMNode *paragraph;
		gulong start_offset;

		/* We need to save caret position and restore it after
		 * wrapping the selection, but we need to save it before we
		 * start to modify selection */
		range = editor_selection_get_current_range (selection);
		if (!range)
			return;

		e_editor_selection_save_caret_position (selection);

		start_offset = webkit_dom_range_get_start_offset (range, NULL);
		/* Extend the range to include entire nodes */
		webkit_dom_range_select_node_contents (
				range,
				webkit_dom_range_get_common_ancestor_container (range, NULL),
				NULL);

		window_selection = webkit_dom_dom_window_get_selection (window);

		end_container = webkit_dom_range_get_end_container (range, NULL);

		previously_wrapped = check_if_previously_wrapped (document);

		/* Wrap only text surrounded in DIV and P tags */
		parent = webkit_dom_node_get_parent_node(end_container);
		if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (parent) || WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT (parent)) {
			webkit_dom_element_set_class_name (WEBKIT_DOM_ELEMENT (parent), "-x-evo-paragraph");
			paragraph = parent;
		} else {
			WebKitDOMElement *parent_div = e_editor_dom_node_find_parent_element (parent, "DIV");

			if (parent_div && g_strcmp0 (webkit_dom_element_get_class_name (parent_div), "-x-evo-paragraph") == 0) {
				paragraph = WEBKIT_DOM_NODE (parent_div);
			} else {
				WebKitDOMNode *position = WEBKIT_DOM_NODE (webkit_dom_document_get_element_by_id (document, "-x-evo-caret-position"));
				if (!position)
					return;

				/* We try to select previous sibling */
				paragraph = webkit_dom_node_get_previous_sibling (position);
				if (paragraph) {
					/* When there is just text without container we have to surround it with paragraph div */
					if (WEBKIT_DOM_IS_TEXT (paragraph)) {
						WebKitDOMRange *new_range = webkit_dom_document_create_range (document);
						WebKitDOMNode *container = WEBKIT_DOM_NODE (webkit_dom_document_create_element (document, "DIV", NULL));
						webkit_dom_element_set_class_name (WEBKIT_DOM_ELEMENT (container), "-x-evo-paragraph");
						webkit_dom_range_select_node (new_range, paragraph, NULL);
						webkit_dom_range_surround_contents (new_range, container, NULL);
						/* We have to move caret position inside this container */
						webkit_dom_node_append_child (container, position, NULL);
						paragraph = container;
					}
				} else {
					/* When some weird element is selected, return */
					e_editor_selection_clear_caret_position_marker (selection);
					return;
				}
			}
		}

		if (!paragraph)
			return;

		webkit_dom_html_element_set_id (WEBKIT_DOM_HTML_ELEMENT (paragraph), "-x-evo-active-paragraph");

		/* If there is hidden space character in the beginning we remove it */
		if (strstr (webkit_dom_node_get_text_content (paragraph), UNICODE_HIDDEN_SPACE)) {
			WebKitDOMNode *child = webkit_dom_node_get_first_child (paragraph);
			GRegex *regex;
			gchar *node_text;

			regex = g_regex_new (UNICODE_HIDDEN_SPACE, 0, 0, NULL);
			if (!regex)
				return;

			node_text = webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (child));
			webkit_dom_character_data_set_data (
				WEBKIT_DOM_CHARACTER_DATA (child),
				g_regex_replace_literal (regex, node_text, -1, 0, "", 0, NULL),
				NULL);

			g_free (node_text);
			g_regex_unref (regex);
		}

		if (previously_wrapped) {
			/* If we are on the beginning of line we need to remember it */
			if (!adding && start_offset > selection->priv->word_wrap_length)
				jump_to_previous_line = TRUE;
		} else {
			WebKitDOMElement *caret_position;
			gboolean parent_is_body = FALSE;

			caret_position = webkit_dom_document_get_element_by_id (document, "-x-evo-caret-position");

			webkit_dom_dom_selection_select_all_children (
				window_selection,
				paragraph,
				NULL);

			if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (caret_position))))
				parent_is_body = TRUE;

			if (backspace_pressed && parent_is_body) {
				WebKitDOMElement *prev_sibling = webkit_dom_element_get_previous_element_sibling (caret_position);
				move_caret_into_element (document, prev_sibling);
				e_editor_selection_clear_caret_position_marker (selection);
				webkit_dom_dom_selection_modify (window_selection, "move", "forward", "character");
				webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (paragraph), "id");

				return;
			}

			/* If there is less than word_wrap_length characters do nothing */
			if (g_utf8_strlen (e_editor_selection_get_string (selection), -1) < selection->priv->word_wrap_length) {
				if (return_pressed) {
					WebKitDOMNode *next_sibling =
						webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (caret_position));

					if (WEBKIT_DOM_IS_ELEMENT (next_sibling))
						move_caret_into_element (document, WEBKIT_DOM_ELEMENT (next_sibling));

					e_editor_selection_clear_caret_position_marker (selection);
				} else {
					e_editor_selection_restore_caret_position (selection);
				}

				webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (paragraph), "id");

				return;
			}

			if (!adding && start_offset > selection->priv->word_wrap_length)
				jump_to_previous_line = TRUE;
		}

		webkit_dom_dom_selection_select_all_children (
			window_selection,
			paragraph,
			NULL);

		wrap_lines (NULL, paragraph, web_view, jump_to_previous_line,
			    FALSE, selection->priv->word_wrap_length, delete_pressed);

	} else {
		/* When there is nothing selected, we select and wrap everything
		 * that is not containing signature */
		e_editor_selection_save_caret_position (selection);
		if (g_strcmp0 (e_editor_selection_get_string (selection), "") == 0) {
			WebKitDOMNodeList *list;
			WebKitDOMNode *signature = NULL;
			gint ii;

			window_selection = webkit_dom_dom_window_get_selection (window);

			/* Check if signature is presented in editor */
			list = webkit_dom_document_get_elements_by_class_name (document, "-x-evolution-signature");
			if (webkit_dom_node_list_get_length (list) > 0)
				signature = webkit_dom_node_list_item (list, 0);

			list = webkit_dom_document_query_selector_all (document, "div.-x-evo-paragraph, p.-x-evo-paragraph", NULL);
			for (ii = 0; ii < webkit_dom_node_list_get_length (list); ii++) {
				WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

				/* Select elements that actualy have some text content */
				if (g_utf8_strlen (webkit_dom_node_get_text_content (node), -1) == 0)
					continue;

				if (signature) {
					if (!webkit_dom_node_contains (node, signature) &&
					    !webkit_dom_node_contains (signature, node)) {

						wrap_lines (NULL, node, web_view, jump_to_previous_line,
							    FALSE, selection->priv->word_wrap_length, delete_pressed);
					}
				} else {
					wrap_lines (NULL, node, web_view, jump_to_previous_line,
						    FALSE, selection->priv->word_wrap_length, delete_pressed);
				}
			}
		} else {
			/* If we have selection -> wrap it */
			wrap_lines (selection, NULL, web_view, jump_to_previous_line,
				    FALSE, selection->priv->word_wrap_length, delete_pressed);
		}
	}

	active_paragraph = webkit_dom_document_get_element_by_id (document, "-x-evo-active-paragraph");
	/* We have to move caret on position where it was before modifying the text */
	if (return_pressed) {
		e_editor_selection_clear_caret_position_marker (selection);
		move_caret_into_element (document, active_paragraph);
		webkit_dom_dom_selection_modify (window_selection, "move", "forward", "character");
	} else {
		if (while_typing)
			e_editor_selection_restore_caret_position (selection);
		else
			e_editor_selection_clear_caret_position_marker (selection);
	}

	/* Set paragraph as non-active */
	if (active_paragraph)
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (active_paragraph), "id");

	g_object_unref (editor_widget);
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
	EEditorWidget *editor_widget;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	WebKitDOMNode *container;
	WebKitDOMElement *marker;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	web_view = WEBKIT_WEB_VIEW (editor_widget);

	document = webkit_web_view_get_dom_document (web_view);

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

	if (range != NULL) {
		WebKitDOMNode *marker_node;
		WebKitDOMNode *parent_node;
		WebKitDOMNode *split_node;
		glong start_offset;

		start_offset = webkit_dom_range_get_start_offset (range, NULL);

		marker = webkit_dom_document_create_element (
			document, "SPAN", NULL);
		webkit_dom_html_element_set_id (
			WEBKIT_DOM_HTML_ELEMENT (marker),
			"-x-evolution-selection-start-marker");

		container = webkit_dom_range_get_start_container (range, NULL);
		if (WEBKIT_DOM_IS_TEXT (container)) {
			WebKitDOMText *split_text;

			split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (container),
				start_offset, NULL);
			split_node = WEBKIT_DOM_NODE (split_text);
		} else {
			split_node = container;
		}

		marker_node = WEBKIT_DOM_NODE (marker);
		parent_node = webkit_dom_node_get_parent_node (container);

		webkit_dom_node_insert_before (
			parent_node, marker_node, split_node, NULL);

		marker = webkit_dom_document_create_element (
			document, "SPAN", NULL);
		webkit_dom_html_element_set_id (
			WEBKIT_DOM_HTML_ELEMENT (marker),
			"-x-evolution-selection-end-marker");

		container = webkit_dom_range_get_end_container (range, NULL);
		if (WEBKIT_DOM_IS_TEXT (container)) {
			WebKitDOMText *split_text;

			split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (container),
				start_offset, NULL);
			split_node = WEBKIT_DOM_NODE (split_text);
		} else {
			split_node = container;
		}

		marker_node = WEBKIT_DOM_NODE (marker);
		parent_node = webkit_dom_node_get_parent_node (container);

		webkit_dom_node_insert_before (
			parent_node, marker_node, split_node, NULL);
	}

	g_object_unref (editor_widget);
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
	EEditorWidget *editor_widget;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	WebKitDOMElement *marker;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	web_view = WEBKIT_WEB_VIEW (editor_widget);

	document = webkit_web_view_get_dom_document (web_view);
	range = editor_selection_get_current_range (selection);

	if (range != NULL) {
		WebKitDOMNode *marker_node;
		WebKitDOMNode *parent_node;

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

	g_object_unref (editor_widget);
}

static void
editor_selection_modify (EEditorSelection *selection,
                         const gchar *alter,
                         gboolean forward,
                         EEditorSelectionGranularity granularity)
{
	EEditorWidget *editor_widget;
	WebKitWebView *web_view;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	const gchar *granularity_str;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	editor_widget = e_editor_selection_ref_editor_widget (selection);
	g_return_if_fail (editor_widget != NULL);

	web_view = WEBKIT_WEB_VIEW (editor_widget);

	document = webkit_web_view_get_dom_document (web_view);
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

	g_object_unref (editor_widget);
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
