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

#include <webkit/webkit.h>
#include <webkit/webkitdom.h>
#include <string.h>
#include <stdlib.h>

#define E_EDITOR_SELECTION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_EDITOR_SELECTION, EEditorSelectionPrivate))


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
	PROP_BACKGROUND_COLOR,
	PROP_BOLD,
	PROP_FONT_NAME,
	PROP_FONT_SIZE,
	PROP_FONT_COLOR,
	PROP_BLOCK_FORMAT,
	PROP_ITALIC,
	PROP_STRIKE_THROUGH,
	PROP_SUBSCRIPT,
	PROP_SUPERSCRIPT,
	PROP_TEXT,
	PROP_UNDERLINE,
};

static WebKitDOMRange *
editor_selection_get_current_range (EEditorSelection *selection)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1)
		return NULL;

	return webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
}

static gboolean
get_has_style_property (EEditorSelection *selection,
			const gchar *style,
			const gchar *value)
{
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range;
	WebKitDOMCSSStyleDeclaration *css;
	gchar *style_value;
	gboolean result;

	range = editor_selection_get_current_range (selection);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	if (!WEBKIT_DOM_IS_ELEMENT (node)) {
		element = webkit_dom_node_get_parent_element (node);
	} else {
		element = WEBKIT_DOM_ELEMENT (node);
	}

	css = webkit_dom_element_get_style (element);
	style_value = webkit_dom_css_style_declaration_get_property_value (
			css, style);

	result = (g_ascii_strncasecmp (style_value, value, strlen (value)) == 0);
	g_free (style_value);

	return result;
}

static void
webview_selection_changed (WebKitWebView *webview,
			   EEditorSelection *selection)
{
	g_object_notify (G_OBJECT (selection), "background-color");
	g_object_notify (G_OBJECT (selection), "bold");
	g_object_notify (G_OBJECT (selection), "font-name");
	g_object_notify (G_OBJECT (selection), "font-size");
	g_object_notify (G_OBJECT (selection), "font-color");
	g_object_notify (G_OBJECT (selection), "block-format");
	g_object_notify (G_OBJECT (selection), "italic");
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
	g_signal_connect (webview, "selection-changed",
			  G_CALLBACK (webview_selection_changed), selection);
}


static void
e_editor_selection_get_property (GObject *object,
				 guint property_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	EEditorSelection *selection = E_EDITOR_SELECTION (object);

	switch (property_id) {
		case PROP_BACKGROUND_COLOR:
			g_value_set_string (value,
				e_editor_selection_get_background_color (
					selection));
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
			g_value_set_string (value,
				e_editor_selection_get_font_color (selection));
			return;

		case PROP_BLOCK_FORMAT:
			g_value_set_int (value,
				e_editor_selection_get_block_format (selection));
			return;

		case PROP_ITALIC:
			g_value_set_boolean (value,
				e_editor_selection_get_italic (selection));
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
				selection, g_value_get_string (value));
			return;

		case PROP_FONT_NAME:
			e_editor_selection_set_font_name (
				selection, g_value_get_string (value));
			return;

		case PROP_FONT_SIZE:
			e_editor_selection_set_font_size (
				selection, g_value_get_uint (value));
			return;

		case PROP_ITALIC:
			e_editor_selection_set_italic (
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
		g_param_spec_string (
			"font-color",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_BLOCK_FORMAT,
		g_param_spec_uint (
			"block-format",
			NULL,
			NULL,
			0,
			G_MAXUINT,
			0,
			G_PARAM_READWRITE));

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
	gchar *tmp, *node_name;
	EEditorSelectionBlockFormat result;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection),
			      E_EDITOR_SELECTION_BLOCK_FORMAT_NONE);

	range = editor_selection_get_current_range (selection);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);

	tmp = webkit_dom_node_get_node_name (node);
	node_name = g_ascii_strdown (tmp, -1);
	g_free (tmp);

	if (g_strcmp0 (node_name, "blockquote") == 0)
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE;
	else if (g_strcmp0 (node_name, "h1") == 0)
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H1;
	else if (g_strcmp0 (node_name, "h2") == 0)
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H2;
	else if (g_strcmp0 (node_name, "h3") == 0)
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H3;
	else if (g_strcmp0 (node_name, "h4") == 0)
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H4;
	else if (g_strcmp0 (node_name, "h5") == 0)
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H5;
	else if (g_strcmp0 (node_name, "h6") == 0)
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_H6;
	else if (g_strcmp0 (node_name, "p") == 0)
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
	else if (g_strcmp0 (node_name, "pre") == 0)
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_PRE;
	else if (g_strcmp0 (node_name, "address") == 0)
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS;
	else
		result = E_EDITOR_SELECTION_BLOCK_FORMAT_NONE;

	g_free (node_name);
	return result;
}

void
e_editor_selection_set_block_format (EEditorSelection *selection,
				     EEditorSelectionBlockFormat format)
{
	WebKitDOMDocument *document;
	const gchar *value;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	switch (format) {
		case E_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE:
			value = "BLOCKQUOTE";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H1:
			value = "H1";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H2:
			value = "H2";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H3:
			value = "H3";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H4:
			value = "H4";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H5:
			value = "H5";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_H6:
			value = "H6";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH:
			value = "P";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_PRE:
			value = "PRE";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS:
			value = "ADDRESS";
			break;
		case E_EDITOR_SELECTION_BLOCK_FORMAT_NONE:
		default:
			value = NULL;
			break;
	}

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	if (value) {
		webkit_dom_document_exec_command (
			document, "formatBlock", FALSE, value);
	} else {
		webkit_dom_document_exec_command (
			document, "removeFormat", FALSE, "");
	}

	g_object_notify (G_OBJECT (selection), "block-format");
}

gboolean
e_editor_selection_get_bold (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return (get_has_style_property (selection, "fontWeight", "bold") ||
	       get_has_style_property (selection, "fontWeight", "700"));
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

const gchar *
e_editor_selection_get_font_color (EEditorSelection *selection)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range;
	WebKitDOMCSSStyleDeclaration *css;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), NULL);

	range = editor_selection_get_current_range (selection);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);

	g_free (selection->priv->font_color);
	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (node));
	selection->priv->font_color =
		webkit_dom_css_style_declaration_get_property_value (css, "color");

	return selection->priv->font_color;
}

void
e_editor_selection_set_font_color (EEditorSelection *selection,
				   const gchar *color)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	webkit_dom_document_exec_command (document, "foreColor", FALSE, "");

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
	WebKitDOMNode *node;
	WebKitDOMRange *range;
	WebKitDOMCSSStyleDeclaration *css;
	gchar *size;
	gint size_int;

	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), 0);

	range = editor_selection_get_current_range (selection);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);

	g_free (selection->priv->font_family);
	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (node));
	size = webkit_dom_css_style_declaration_get_property_value (css, "fontSize");

	size_int = atoi (size);
	g_free (size);

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
e_editor_selection_get_italic (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style_property (selection, "fontStyle", "italic");
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
e_editor_selection_get_strike_through (EEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_EDITOR_SELECTION (selection), FALSE);

	return get_has_style_property (selection, "textDecoration", "overline");
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

	return get_has_style_property (selection, "textDecoration", "underline");
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
e_editor_selection_insert_text (EEditorSelection *selection,
				const gchar *plain_text)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	WebKitDOMElement *element;

	g_return_if_fail (E_IS_EDITOR_SELECTION (selection));
	g_return_if_fail (plain_text != NULL);

	range = editor_selection_get_current_range (selection);
	document = webkit_web_view_get_dom_document (selection->priv->webview);
	element = webkit_dom_document_create_element (document, "DIV", NULL);
	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (element), plain_text, NULL);

	webkit_dom_range_insert_node (
		range, webkit_dom_node_get_first_child (
			WEBKIT_DOM_NODE (element)), NULL);

	g_object_unref (element);
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
