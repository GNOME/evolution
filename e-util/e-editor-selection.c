/*
 * e-editor-selection.c
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


struct _EEditorSelectionPrivate {

	WebKitWebView *webview;

	gchar *text;
	gchar *background_color;
	gchar *font_color;
	gchar *font_family;
};

G_DEFINE_TYPE (
	EEditorSelection,
	e_editor_selection,
	G_TYPE_OBJECT
);

enum {
	PROP_0,
	PROP_WEBVIEW,
	PROP_ALIGNMENT,
	PROP_BACKGROUND_COLOR,
	PROP_BOLD,
	PROP_FONT_NAME,
	PROP_FONT_SIZE,
	PROP_FONT_COLOR,
	PROP_BLOCK_FORMAT,
	PROP_INDENTED,
	PROP_ITALIC,
	PROP_MONOSPACED,
	PROP_STRIKE_THROUGH,
	PROP_SUBSCRIPT,
	PROP_SUPERSCRIPT,
	PROP_TEXT,
	PROP_UNDERLINE,
};

static const GdkRGBA black = { 0 };

static WebKitDOMRange *
editor_selection_get_current_range (EEditorSelection *selection)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	window = webkit_dom_document_get_default_view (document);
	if (!window) {
		return NULL;
	}

	dom_selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		return NULL;
	}

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
	if (!range) {
		return FALSE;
	}

	node = webkit_dom_range_get_start_container (range, NULL);
	if (!WEBKIT_DOM_IS_ELEMENT (node)) {
		element = webkit_dom_node_get_parent_element (node);
	} else {
		element = WEBKIT_DOM_ELEMENT (node);
	}

	tag_len = strlen (style_tag);
	result = FALSE;
	while (!result && element) {
		gchar *element_tag;

		element_tag = webkit_dom_element_get_tag_name (element);

		result = ((tag_len == strlen (element_tag)) &&
				(g_ascii_strncasecmp (element_tag, style_tag, tag_len) == 0));

		/* Special case: <blockquote type=cite> marks quotation, while
		 * just <blockquote> is used for indentation. If the <blockquote>
		 * has type=cite, then ignore it */
		if (result && g_ascii_strncasecmp (element_tag, "blockquote", 10) == 0) {
			if (webkit_dom_element_has_attribute (element, "type")) {
				gchar *type;
				type = webkit_dom_element_get_attribute (
						element, "type");
				if (g_ascii_strncasecmp (type, "cite", 4) == 0) {
					result = FALSE;
				}
				g_free (type);
			}
		}

		g_free (element_tag);

		if (result) {
			break;
		}

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
	if (!range) {
		return NULL;
	}

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	element = e_editor_dom_node_find_parent_element (node, "FONT");
	if (!element) {
		return NULL;
	}

	g_object_get (G_OBJECT (element), font_property, &value, NULL);

	return value;
}

static void
webview_selection_changed (WebKitWebView *webview,
			   EEditorSelection *selection)
{
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
}

static void
editor_selection_set_webview (EEditorSelection *selection,
			      WebKitWebView *webview)
{
	selection->priv->webview = g_object_ref (webview);
	g_signal_connect (
		webview, "selection-changed",
		G_CALLBACK (webview_selection_changed), selection);
}


static void
e_editor_selection_get_property (GObject *object,
				 guint property_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	GdkRGBA rgba = { 0 };
	EEditorSelection *selection = E_EDITOR_SELECTION (object);

	switch (property_id) {
		case PROP_ALIGNMENT:
			g_value_set_int (value,
				e_editor_selection_get_alignment (selection));
			return;

		case PROP_BACKGROUND_COLOR:
			g_value_set_string (value,
				e_editor_selection_get_background_color (selection));
			return;

		case PROP_BOLD:
			g_value_set_boolean (value,
				e_editor_selection_get_bold (selection));
			return;

		case PROP_FONT_NAME:
			g_value_set_string (value,
				e_editor_selection_get_font_name (selection));
			return;

		case PROP_FONT_SIZE:
			g_value_set_int (value,
				e_editor_selection_get_font_size (selection));
			return;

		case PROP_FONT_COLOR:
			e_editor_selection_get_font_color (selection, &rgba);
			g_value_set_boxed (value, &rgba);
			return;

		case PROP_BLOCK_FORMAT:
			g_value_set_int (value,
				e_editor_selection_get_block_format (selection));
			return;

		case PROP_INDENTED:
			g_value_set_boolean (value,
				e_editor_selection_get_indented (selection));
			return;

		case PROP_ITALIC:
			g_value_set_boolean (value,
				e_editor_selection_get_italic (selection));
			return;

		case PROP_MONOSPACED:
			g_value_set_boolean (value,
				e_editor_selection_get_monospaced (selection));
			return;

		case PROP_STRIKE_THROUGH:
			g_value_set_boolean (value,
				e_editor_selection_get_strike_through (selection));
			return;

		case PROP_SUBSCRIPT:
			g_value_set_boolean (value,
				e_editor_selection_get_subscript (selection));
			return;

		case PROP_SUPERSCRIPT:
			g_value_set_boolean (value,
				e_editor_selection_get_superscript (selection));
			return;

		case PROP_TEXT:
			g_value_set_string (value,
				e_editor_selection_get_string (selection));
			break;

		case PROP_UNDERLINE:
			g_value_set_boolean (value,
				e_editor_selection_get_underline (selection));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_editor_selection_set_property (GObject *object,
				 guint property_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	EEditorSelection *selection = E_EDITOR_SELECTION (object);

	switch (property_id) {
		case PROP_WEBVIEW:
			editor_selection_set_webview (
				selection, g_value_get_object (value));
			return;

		case PROP_ALIGNMENT:
			e_editor_selection_set_alignment (
				selection, g_value_get_int (value));
			return;

		case PROP_BACKGROUND_COLOR:
			e_editor_selection_set_background_color (
				selection, g_value_get_string (value));
			return;

		case PROP_BOLD:
			e_editor_selection_set_bold (
				selection, g_value_get_boolean (value));
			return;

		case PROP_FONT_COLOR:
			e_editor_selection_set_font_color (
				selection, g_value_get_boxed (value));
			return;

		case PROP_BLOCK_FORMAT:
			e_editor_selection_set_block_format (
				selection, g_value_get_int (value));
			return;

		case PROP_FONT_NAME:
			e_editor_selection_set_font_name (
				selection, g_value_get_string (value));
			return;

		case PROP_FONT_SIZE:
			e_editor_selection_set_font_size (
				selection, g_value_get_int (value));
			return;

		case PROP_ITALIC:
			e_editor_selection_set_italic (
				selection, g_value_get_boolean (value));
			return;

		case PROP_MONOSPACED:
			e_editor_selection_set_monospaced (
				selection, g_value_get_boolean (value));
			return;

		case PROP_STRIKE_THROUGH:
			e_editor_selection_set_strike_through (
				selection, g_value_get_boolean (value));
			return;

		case PROP_SUBSCRIPT:
			e_editor_selection_set_subscript (
				selection, g_value_get_boolean (value));
			return;

		case PROP_SUPERSCRIPT:
			e_editor_selection_set_superscript (
				selection, g_value_get_boolean (value));
			return;

		case PROP_UNDERLINE:
			e_editor_selection_set_underline (
				selection, g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_editor_selection_finalize (GObject *object)
{
	EEditorSelection *selection = E_EDITOR_SELECTION (object);

	g_clear_object (&selection->priv->webview);

	g_free (selection->priv->text);
	selection->priv->text = NULL;
}

static void
e_editor_selection_class_init (EEditorSelectionClass *klass)
{
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (EEditorSelectionPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = e_editor_selection_get_property;
	object_class->set_property = e_editor_selection_set_property;
	object_class->finalize = e_editor_selection_finalize;

	g_object_class_install_property (
		object_class,
		PROP_WEBVIEW,
		g_param_spec_object (
			"webview",
			NULL,
			NULL,
		        WEBKIT_TYPE_WEB_VIEW,
		        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

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

	g_object_class_install_property (
		object_class,
		PROP_BACKGROUND_COLOR,
		g_param_spec_string (
			"background-color",
		        NULL,
		        NULL,
		        NULL,
		        G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_BOLD,
		g_param_spec_boolean (
			"bold",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FONT_NAME,
		g_param_spec_string (
			"font-name",
		        NULL,
		        NULL,
		        NULL,
		        G_PARAM_READWRITE));

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
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FONT_COLOR,
		g_param_spec_boxed (
			"font-color",
			NULL,
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE));

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
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_INDENTED,
		g_param_spec_boolean (
			"indented",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_ITALIC,
		g_param_spec_boolean (
			"italic",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MONOSPACED,
		g_param_spec_boolean (
			"monospaced",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_STRIKE_THROUGH,
		g_param_spec_boolean (
			"strike-through",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SUPERSCRIPT,
		g_param_spec_boolean (
			"superscript",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SUBSCRIPT,
		g_param_spec_boolean (
			"subscript",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TEXT,
		g_param_spec_string (
			"text",
		       NULL,
		       NULL,
		       NULL,
		       G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_UNDERLINE,
		g_param_spec_boolean (
			"underline",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));
}


static void
e_editor_selection_init (EEditorSelection *selection)
{
	selection->priv = E_EDITOR_SELECTION_GET_PRIVATE (selection);
}

EEditorSelection *
e_editor_selection_new (WebKitWebView *parent_view)
{
	g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (parent_view), NULL);

	return g_object_new (
			E_TYPE_EDITOR_SELECTION,
			"webview", parent_view, NULL);
}

gboolean
e_editor_selection_has_text (EEditorSelection *selection)
{
	WebKitDOMRange *range;
	WebKitDOMNode *node;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	range = editor_selection_get_current_range (selection);

	node = webkit_dom_range_get_start_container (range, NULL);
	if (webkit_dom_node_get_node_type (node) == 3) {
		return TRUE;
	}

	node = webkit_dom_range_get_end_container (range, NULL);
	if (webkit_dom_node_get_node_type (node) == 3) {
		return TRUE;
	}

	node = WEBKIT_DOM_NODE (webkit_dom_range_clone_contents (range, NULL));
	while (node) {
		if (webkit_dom_node_get_node_type (node) == 3) {
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

	return FALSE;
}

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

void
e_editor_selection_replace (EEditorSelection *selection,
			    const gchar *new_string)
{
	WebKitDOMDocumentFragment *frag;
	WebKitDOMRange *range;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	range = editor_selection_get_current_range (selection);

	frag = webkit_dom_range_create_contextual_fragment (
			range, new_string, NULL);

	webkit_dom_range_delete_contents (range, NULL);
	webkit_dom_range_insert_node (range, WEBKIT_DOM_NODE (frag), NULL);
}

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
	if (!range) {
		return E_EDITOR_SELECTION_ALIGNMENT_LEFT;
	}

	node = webkit_dom_range_get_start_container (range, NULL);
	if (!node) {
		return E_EDITOR_SELECTION_ALIGNMENT_LEFT;
	}

	if (!WEBKIT_DOM_IS_ELEMENT (node)) {
		element = webkit_dom_node_get_parent_element (node);
	} else {
		element = WEBKIT_DOM_ELEMENT (node);
	}

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

void
e_editor_selection_set_alignment (EEditorSelection *selection,
				  EEditorSelectionAlignment alignment)
{
	WebKitDOMDocument *document;
	const gchar *command;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (e_editor_selection_get_alignment (selection) == alignment) {
		return;
	}

	switch (alignment) {
		case E_EDITOR_SELECTION_ALIGNMENT_CENTER:
			command = "justifyCenter";
			break;

		case E_EDITOR_SELECTION_ALIGNMENT_LEFT:
			command = "justifyLeft";
			break;

		case E_EDITOR_SELECTION_ALIGNMENT_RIGHT:
			command = "justifyRight";
			break;
	}

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, command, FALSE, "");

	g_object_notify (G_OBJECT (selection), "alignment");
}


const gchar *
e_editor_selection_get_background_color	(EEditorSelection *selection)
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

void
e_editor_selection_set_background_color (EEditorSelection *selection,
					const gchar *color)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (color && *color);

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (
		document, "backColor", FALSE, color);

	g_object_notify (G_OBJECT (selection), "background-color");
}

EEditorSelectionBlockFormat
e_editor_selection_get_block_format (EEditorSelection *selection)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range;
	WebKitDOMElement *element;
	EEditorSelectionBlockFormat result;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection),
			      E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH);

	range = editor_selection_get_current_range (selection);
	if (!range) {
		return E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
	}

	node = webkit_dom_range_get_start_container (range, NULL);

	if (e_editor_dom_node_find_parent_element (node, "UL")) {
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST;
	} else if ((element = e_editor_dom_node_find_parent_element (node, "OL")) != NULL) {
		if (webkit_dom_element_has_attribute (element, "type")) {
			gchar *type;

			type = webkit_dom_element_get_attribute(element, "type");
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

void
e_editor_selection_set_block_format (EEditorSelection *selection,
				     EEditorSelectionBlockFormat format)
{
	EEditorSelectionBlockFormat current_format;
	WebKitDOMDocument *document;
	const gchar *command;
	const gchar *value;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	current_format = e_editor_selection_get_block_format (selection);
	if (current_format == format) {
		return;
	}

	document = webkit_web_view_get_dom_document (selection->priv->webview);

	switch (format) {
		case E_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE:
			command = "formatBlock";
			value = "BLOCKQUOTE";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H1:
			command = "formatBlock";
			value = "H1";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H2:
			command = "formatBlock";
			value = "H2";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H3:
			command = "formatBlock";
			value = "H3";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H4:
			command = "formatBlock";
			value = "H4";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H5:
			command = "formatBlock";
			value = "H5";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H6:
			command = "formatBlock";
			value = "H6";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH:
			command = "formatBlock";
			value = "P";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_PRE:
			command = "formatBlock";
			value = "PRE";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS:
			command = "formatBlock";
			value = "ADDRESS";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST:
		case E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA:
		case E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN:
			command = "insertOrderedList";
			value = "";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST:
			command = "insertUnorderedList";
			value = "";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_NONE:
		default:
			command = "removeFormat";
			value = "";
			break;
	}


	/* First remove (un)ordered list before changing formatting */
	if (current_format == E_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST) {
		webkit_dom_document_exec_command (
			document, "insertUnorderedList", FALSE, "");
		/*		    ^-- not a typo, "insert" toggles the formatting
		 * 			if already present */
	} else if (current_format >= E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST) {
		webkit_dom_document_exec_command (
			document, "insertOrderedList", FALSE ,"");
	}

	webkit_dom_document_exec_command (
		document, command, FALSE, value);

	/* Fine tuning - set the specific marker type for ordered lists */
	if ((format == E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA) ||
	    (format == E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN)) {

		WebKitDOMRange *range = editor_selection_get_current_range (selection);
		WebKitDOMNode *node;
		WebKitDOMElement *list;

		node = webkit_dom_range_get_start_container (range, NULL);

		list = e_editor_dom_node_find_child_element (node, "OL");
		if (!list) {
			list = e_editor_dom_node_find_parent_element (node, "OL");
		}

		if (list) {
			webkit_dom_element_set_attribute (
				list, "type",
				(format == E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA) ?
					"A" : "I", NULL);
		}
	}

	g_object_notify (G_OBJECT (selection), "block-format");
}

gboolean
e_editor_selection_get_bold (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "b");
}

void
e_editor_selection_set_bold (EEditorSelection *selection,
			     gboolean bold)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if ((e_editor_selection_get_bold (selection) ? TRUE : FALSE)
				== (bold ? TRUE : FALSE)) {
		return;
	}

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "bold", FALSE, "");

	g_object_notify (G_OBJECT (selection), "bold");
}

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

void
e_editor_selection_set_font_color (EEditorSelection *selection,
				   const GdkRGBA *rgba)
{
	WebKitDOMDocument *document;
	gchar *color;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if (!rgba) {
		rgba = &black;
	}

	color = g_strdup_printf ("#%06x", e_rgba_to_value ((GdkRGBA *) rgba));

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "foreColor", FALSE, color);

	g_free (color);

	g_object_notify (G_OBJECT (selection), "font-color");
}

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

void
e_editor_selection_set_font_name (EEditorSelection *selection,
				  const gchar *font_name)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "fontName", FALSE, "");

	g_object_notify (G_OBJECT (selection), "font-name");
}

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

void
e_editor_selection_set_font_size (EEditorSelection *selection,
				  guint font_size)
{
	WebKitDOMDocument *document;
	gchar *size_str;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	size_str = g_strdup_printf("%d", font_size);
	webkit_dom_document_exec_command (document, "fontSize", FALSE, size_str);
	g_free (size_str);

	g_object_notify (G_OBJECT (selection), "font-size");
}

gboolean
e_editor_selection_get_indented (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "blockquote");
}

void
e_editor_selection_indent (EEditorSelection *selection)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "indent", FALSE, "");

	g_object_notify (G_OBJECT (selection), "indented");
}

void
e_editor_selection_unindent (EEditorSelection *selection)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "outdent", FALSE, "");

	g_object_notify (G_OBJECT (selection), "indented");
}

gboolean
e_editor_selection_get_italic (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "i");
}

void
e_editor_selection_set_italic (EEditorSelection *selection,
			       gboolean italic)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if ((e_editor_selection_get_italic (selection) ? TRUE : FALSE)
				== (italic ? TRUE : FALSE)) {
		return;
	}

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "italic", FALSE, "");

	g_object_notify (G_OBJECT (selection), "italic");
}

gboolean
e_editor_selection_get_monospaced (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "tt");
}

void
e_editor_selection_set_monospaced (EEditorSelection *selection,
				   gboolean monospaced)
{
	WebKitDOMRange *range;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if ((e_editor_selection_get_monospaced (selection) ? TRUE : FALSE)
				== (monospaced ? TRUE : FALSE)) {
		return;
	}

	range = editor_selection_get_current_range (selection);
	if (!range) {
		return;
	}

	/* FIXME WEBKIT Although we can implement applying and
	 * removing style on our own by advanced DOM manipulation,
	 * this change will not be recorded in UNDO and REDO history
	 * TODO: Think of something..... */
	if (monospaced) {

		/* apply_format (selection, "TT"); */

	} else {
		/* remove_format (selection, "TT"); */
	}

	g_object_notify (G_OBJECT (selection), "monospaced");
}

gboolean
e_editor_selection_get_strike_through (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "strike");
}

void
e_editor_selection_set_strike_through (EEditorSelection *selection,
				       gboolean strike_through)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if ((e_editor_selection_get_strike_through (selection) ? TRUE : FALSE)
				== (strike_through? TRUE : FALSE)) {
		return;
	}

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "strikeThrough", FALSE, "");

	g_object_notify (G_OBJECT (selection), "strike-through");
}

gboolean
e_editor_selection_get_subscript (EEditorSelection *selection)
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

void
e_editor_selection_set_subscript (EEditorSelection *selection,
				  gboolean subscript)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if ((e_editor_selection_get_subscript (selection) ? TRUE : FALSE)
				== (subscript? TRUE : FALSE)) {
		return;
	}

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "subscript", FALSE, "");

	g_object_notify (G_OBJECT (selection), "subscript");
}

gboolean
e_editor_selection_get_superscript (EEditorSelection *selection)
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

void
e_editor_selection_set_superscript (EEditorSelection *selection,
				    gboolean superscript)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if ((e_editor_selection_get_superscript (selection) ? TRUE : FALSE)
				== (superscript? TRUE : FALSE)) {
		return;
	}

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "superscript", FALSE, "");

	g_object_notify (G_OBJECT (selection), "superscript");
}

gboolean
e_editor_selection_get_underline (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style (selection, "u");
}

void
e_editor_selection_set_underline (EEditorSelection *selection,
				  gboolean underline)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	if ((e_editor_selection_get_underline (selection) ? TRUE : FALSE)
				== (underline? TRUE : FALSE)) {
		return;
	}

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "underline", FALSE, "");

	g_object_notify (G_OBJECT (selection), "underline");
}

void
e_editor_selection_unlink (EEditorSelection *selection)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "unlink", FALSE, "");
}

void
e_editor_selection_create_link (EEditorSelection *selection,
				const gchar *uri)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (uri && *uri);

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "createLink", FALSE, uri);
}


void
e_editor_selection_insert_text (EEditorSelection *selection,
				const gchar *plain_text)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (plain_text != NULL);

	document = webkit_web_view_get_dom_document (selection->priv->webview);

	webkit_dom_document_exec_command (
		document, "insertText", FALSE, plain_text);
}

void
e_editor_selection_insert_html (EEditorSelection *selection,
				const gchar *html_text)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (html_text != NULL);

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (
			document, "insertHTML", FALSE, html_text);
}

void
e_editor_selection_insert_image (EEditorSelection *selection,
				 const gchar *image_uri)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (image_uri != NULL);

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (
			document, "insertImage", FALSE, image_uri);
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