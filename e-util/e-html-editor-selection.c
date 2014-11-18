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

#include <e-util/e-util.h>

#include <webkit2/webkit2.h>
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

	gchar *text;

	gboolean is_bold;
	gboolean is_italic;
	gboolean is_underline;
	gboolean is_monospaced;
	gboolean is_strikethrough;

	gchar *background_color;
	gchar *font_color;
	gchar *font_name;

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

	html_editor_selection_selection_changed_cb (WEBKIT_WEB_VIEW (view), selection);

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
	g_free (selection->priv->font_name);

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

	g_settings = e_util_ref_settings ("org.gnome.evolution.mail");
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
	EHTMLEditorView *view;
	gboolean ret_val = FALSE;
	GDBusProxy *web_extension;
	GVariant *result;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), ret_val);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, ret_val);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"DOMSelectionHasText",
		g_variant_new (
			"(t)", webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &ret_val);
		g_variant_unref (result);
	}
 out:
	g_object_unref (view);

	return ret_val;
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
	EHTMLEditorView *view;
	gchar *ret_val = NULL;
	GDBusProxy *web_extension;
	GVariant *result;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), ret_val);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, ret_val);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"DOMGetCaretWord",
		g_variant_new (
			"(t)", webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(s)", &ret_val);
		g_variant_unref (result);
	}
 out:
	g_object_unref (view);

	return ret_val;
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
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (replacement != NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	e_html_editor_view_set_changed (view, TRUE);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	g_dbus_proxy_call (
		web_extension,
		"DOMReplaceCaretWord",
		g_variant_new ("(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			replacement),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

 out:
	g_object_unref (view);
}

/**
 * e_html_editor_selection_replace:
 * @selection: an #EHTMLEditorSelection
 * @replacement: a string to replace current selection with
 *
 * Replaces currently selected text with @replacement.
 */
void
e_html_editor_selection_replace (EHTMLEditorSelection *selection,
                                 const gchar *replacement)
{
	EHTMLEditorView *view;
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	e_html_editor_view_set_changed (view, TRUE);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	g_dbus_proxy_call (
		web_extension,
		"DOMSelectionReplace",
		g_variant_new ("(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			replacement),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

 out:
	g_object_unref (view);
}

static const gchar *
html_editor_selection_get_format_string (EHTMLEditorSelection *selection,
                                         const gchar *format_name)
{
	EHTMLEditorView *view;
	const gchar *ret_val = NULL;
	GDBusProxy *web_extension;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	if (!e_html_editor_view_get_html_mode (view))
		goto out;
/* FIXME WK2 get cached property format_name from extension */
 out:
	g_object_unref (view);

	return ret_val;
}

static void
html_editor_selection_set_format_string (EHTMLEditorSelection *selection,
                                         const gchar *format_name,
                                         const gchar *format_dom_function,
                                         const gchar *format_value,
                                         gchar **format_value_priv)
{
	EHTMLEditorView *view;
	GDBusProxy *web_extension;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	if (!e_html_editor_view_get_html_mode (view))
		goto out;

	e_html_editor_view_set_changed (view, TRUE);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	g_dbus_proxy_call (
		web_extension,
		format_dom_function,
		g_variant_new ("(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			format_value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	if (format_value_priv) {
		g_free (&format_value_priv);
		*format_value_priv = g_strdup (format_value);
	}

	g_object_notify (G_OBJECT (selection), format_name);
 out:
	g_object_unref (view);
}

static gboolean
html_editor_selection_get_format_boolean (EHTMLEditorSelection *selection,
                                          const gchar *format_name)
{
	EHTMLEditorView *view;
	gboolean ret_val = FALSE;
	GDBusProxy *web_extension;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	if (!e_html_editor_view_get_html_mode (view))
		goto out;
/* FIXME WK2 get cached property format_name from extension */
 out:
	g_object_unref (view);

	return ret_val;
}

static void
html_editor_selection_set_format_boolean (EHTMLEditorSelection *selection,
                                          const gchar *format_name,
                                          const gchar *format_dom_function,
                                          gboolean format_value,
                                          gboolean *format_value_priv)
{
	EHTMLEditorView *view;
	GDBusProxy *web_extension;

	if (format_value_priv && *format_value_priv == format_value)
		return;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	if (!e_html_editor_view_get_html_mode (view))
		goto out;

	e_html_editor_view_set_changed (view, TRUE);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	g_dbus_proxy_call (
		web_extension,
		format_dom_function,
		g_variant_new (
			"(tb)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			format_value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	if (format_value_priv)
		*format_value_priv = format_value;
	g_object_notify (G_OBJECT (selection), format_name);

 out:
	g_object_unref (view);
}

static guint
html_editor_selection_get_format_uint (EHTMLEditorSelection *selection,
                                       const gchar *format_name)
{
	EHTMLEditorView *view;
	guint ret_val = 0;
	GDBusProxy *web_extension;

	if (!E_IS_HTML_EDITOR_SELECTION (selection))
		goto return_default;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	if (!view)
		goto return_default;

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	if (!e_html_editor_view_get_html_mode (view))
		goto out;
/* FIXME WK2 get cached property format_name from extension */
 out:
	g_object_unref (view);

	return ret_val;

 return_default:
	if (g_strcmp0 (format_name, "font-size") == 0)
		return E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;
	else if (g_strcmp0 (format_name, "alignment") == 0)
		return E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	else
		return E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
}

static void
html_editor_selection_set_format_uint (EHTMLEditorSelection *selection,
                                       const gchar *format_name,
                                       const gchar *format_dom_function,
                                       guint format_value,
                                       guint *format_value_priv)
{
	EHTMLEditorView *view;
	GDBusProxy *web_extension;

	if (format_value_priv && *format_value_priv == format_value)
		return;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	if (!e_html_editor_view_get_html_mode (view))
		goto out;

	e_html_editor_view_set_changed (view, TRUE);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	g_dbus_proxy_call (
		web_extension,
		format_dom_function,
		g_variant_new (
			"(tb)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			format_value),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	if (format_value_priv)
		*format_value_priv = format_value;
	g_object_notify (G_OBJECT (selection), format_name);

 out:
	g_object_unref (view);
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
	g_return_val_if_fail (
		E_IS_HTML_EDITOR_SELECTION (selection),
		E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT);

	return html_editor_selection_get_format_uint (selection, "alignment");
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	html_editor_selection_set_format_uint (
		selection, "alignment", "DOMSelectionSetAlignment", alignment, NULL);

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
	g_return_val_if_fail (
		E_IS_HTML_EDITOR_SELECTION (selection),
		E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH);

	return html_editor_selection_get_format_uint (selection, "block-format");
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	html_editor_selection_set_format_uint (
		selection, "block-format", "DOMSelectionSetBlockFormat", format, NULL);

	/* H1 - H6 have bold font by default */
	if (format >= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H1 &&
	    format <= E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H6)
		selection->priv->is_bold = TRUE;

	/* When changing the format we need to re-set the alignment */
	e_html_editor_selection_set_alignment (selection, selection->priv->alignment);
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
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);

	return html_editor_selection_get_format_string (selection, "text");
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

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, NULL);

	if (!e_html_editor_view_get_html_mode (view)) {
		g_object_unref (view);
		return "#ffffff";
	}
	g_object_unref (view);

	g_free (selection->priv->background_color);
	selection->priv->background_color = g_strdup (
		(html_editor_selection_get_format_string (selection, "font-family"));

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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (color != NULL && *color != '\0');

	html_editor_selection_set_format_string (
		selection,
		"background-color",
		"DOMSelectionSetBackgroundColor",
		color,
		NULL);
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
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), NULL);

	g_free (selection->priv->font_name);
	selection->priv->font_name = g_strdup (
		html_editor_selection_get_format_string (selection, "font-family")_;

	return selection->priv->font_name;
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	html_editor_selection_set_format_string (
		selection, "font-name", "DOMSelectionSetFontName", font_name, NULL);
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

/* FIXME WK2
	gchar *color;

	if (!html_mode) {
		*rgba = black;
		return;
	}

	if (e_html_editor_selection_is_collapsed (selection)) {
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
*/
	*rgba = black;
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
	guint32 rgba_value;
	gchar *color;

	if (!rgba)
		rgba = &black;

	rgba_value = e_rgba_to_value ((GdkRGBA *) rgba);
	color = g_strdup_printf ("#%06x", rgba_value);

	html_editor_selection_set_format_string (
		selection,
		"font-color",
		"DOMSelectionSetFontColor",
		color,
		&selection->priv->font_color);

	g_free (color);
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
	g_return_val_if_fail (
		E_IS_HTML_EDITOR_SELECTION (selection),
		E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL);

	return html_editor_selection_get_format_uint (selection, "font-size");
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	html_editor_selection_set_format_uint (
		selection,
		"font-size",
		"DOMSelectionSetFontSize",
		font_size,
		&selection->priv->font_size);
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
	EHTMLEditorView *view;
	gboolean ret_val = FALSE;
	GDBusProxy *web_extension;
	GVariant *result;

	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_val_if_fail (view != NULL, FALSE);

	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"DOMSelectionIsCitation",
		g_variant_new (
			"(t)", webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		g_variant_get (result, "(b)", &ret_val);
		g_variant_unref (result);
	}
 out:
	g_object_unref (view);

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
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	return html_editor_selection_get_format_boolean (selection, "indented");
}

/**
 * e_html_editor_selection_is_bold:
 * @selection: an #EHTMLEditorSelection
 *
 * Returns whether current selection or letter at current cursor position is bold.
 *
 * Returns @TRUE when selection is bold, @FALSE otherwise.
 */
gboolean
e_html_editor_selection_is_bold (EHTMLEditorSelection *selection)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_SELECTION (selection), FALSE);

	return html_editor_selection_get_format_boolean (selection, "bold");
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

	html_editor_selection_set_format_boolean (
		selection,
		"bold",
		"DOMSelectionSetBold",
		bold,
		&selection->priv->is_bold);
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

	return html_editor_selection_get_format_boolean (selection, "italic");
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

	html_editor_selection_set_format_boolean (
		selection,
		"italic",
		"DOMSelectionSetItalic",
		italic,
		&selection->priv->is_italic);
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

	return html_editor_selection_get_format_boolean (selection, "monospaced");
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	html_editor_selection_set_format_boolean (
		selection,
		"monospaced",
		"DOMSelectionSetMonospaced",
		monospaced,
		&selection->priv->is_monospaced);
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

	return html_editor_selection_get_format_boolean (selection, "strikethrough");
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

	html_editor_selection_set_format_boolean (
		selection,
		"strikethrough",
		"DOMSelectionSetStrikethrough",
		strikethrough,
		&selection->priv->is_strikethrough);
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

	return html_editor_selection_get_format_boolean (selection, "subscript");
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	html_editor_selection_set_format_boolean (
		selection, "subscript", "DOMSelectionSetSubscript", subscript, NULL);
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

	return html_editor_selection_get_format_boolean (selection, "superscript");
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
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	html_editor_selection_set_format_boolean (
		selection, "superscript", "DOMSSelectionetSuperscript", superscript, NULL);
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

	return html_editor_selection_get_format_boolean (selection, "underline");
}

void
e_html_editor_selection_set_underline (EHTMLEditorSelection *selection,
                                       gboolean underline)
{
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));

	html_editor_selection_set_format_boolean (
		selection,
		"underline",
		"DOMSelectionSetUnderline",
		underline,
		&selection->priv->is_underline);
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

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (plain_text != NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	e_html_editor_view_convert_and_insert_plain_text (view, plain_text);

	g_object_unref (view);
}

/************************* image_load_and_insert_async() *************************/

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	EHTMLEditorSelection *selection;
	GInputStream *input_stream;
	GOutputStream *output_stream;
	GFile *file;
	GFileInfo *file_info;
	goffset total_num_bytes;
	gssize bytes_read;
	const gchar *content_type;
	const gchar *filename;
	const gchar *selector;
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
                          const gchar *selector,
                          const gchar *base64_content,
                          const gchar *filename,
                          const gchar *uri)
{
	EHTMLEditorView *view;
	GDBusProxy *web_extension;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	e_html_editor_view_set_changed (view, TRUE);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	g_dbus_proxy_call (
		web_extension,
		"EHTMLEditorSelectionReplaceBase64ImageSrc",
		g_variant_new (
			"(tssss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			selector,
			base64_content,
			filename,
			uri),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

 out:
	g_object_unref (view);
}

static void
insert_base64_image (EHTMLEditorSelection *selection,
                     const gchar *base64_content,
                     const gchar *filename,
                     const gchar *uri)
{
	EHTMLEditorView *view;
	GDBusProxy *web_extension;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	e_html_editor_view_set_changed (view, TRUE);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	g_dbus_proxy_call (
		web_extension,
		"EHTMLEditorSelectionInsertBase64Image",
		g_variant_new (
			"(tssss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			base64_content,
			filename,
			uri),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

 out:
	g_object_unref (view);
}

static void
image_load_finish (LoadContext *load_context)
{
	EHTMLEditorSelection *selection;
	GMemoryOutputStream *output_stream;
	const gchar *selector;
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
	selector = load_context->selector;
	if (selector && *selector)
		replace_base64_image_src (
			selection, selector, output, load_context->filename, uri);
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
                             const gchar *selector,
                             const gchar *uri)
{
	LoadContext *load_context;
	GFile *file;

	g_return_if_fail (uri && *uri);

	file = g_file_new_for_uri (uri);
	g_return_if_fail (file != NULL);

	load_context = image_load_context_new (selection);
	load_context->file = file;
	if (selector && *selector)
		load_context->selector = g_strdup (selector);

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
	EHTMLEditorView *view;

	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (image_uri != NULL);

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	if (e_html_editor_view_get_html_mode (view)) {
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

	g_object_unref (view);
}

/**
 * e_html_editor_selection_replace_image_src:
 * @selection: an #EHTMLEditorSelection
 * @selector: CSS selector that describes the element that we want to change
 * @image_uri: an URI of the source image
 *
 * If element described by given selector is image, we will replace the src
 * attribute of it with base64 data from given @image_uri. Otherwise we will
 * set the base64 data to the background attribute of given element.
 */
void
e_html_editor_selection_replace_image_src (EHTMLEditorSelection *selection,
                                           const gchar *selector,
                                           const gchar *image_uri)
{
	g_return_if_fail (E_IS_HTML_EDITOR_SELECTION (selection));
	g_return_if_fail (image_uri != NULL);
	g_return_if_fail (selector && *selector);

	if (strstr (image_uri, ";base64,")) {
		if (g_str_has_prefix (image_uri, "data:"))
			replace_base64_image_src (
				selection, selector, image_uri, "", "");
		if (strstr (image_uri, ";data")) {
			const gchar *base64_data = strstr (image_uri, ";") + 1;
			gchar *filename;
			glong filename_length;

			filename_length =
				g_utf8_strlen (image_uri, -1) -
				g_utf8_strlen (base64_data, -1) - 1;
			filename = g_strndup (image_uri, filename_length);

			replace_base64_image_src (
				selection, selector, base64_data, filename, "");
			g_free (filename);
		}
	} else
		image_load_and_insert_async (selection, selector, image_uri);
}

static void
html_editor_selection_modify (EHTMLEditorSelection *selection,
                              const gchar *alter,
                              gboolean forward,
                              EHTMLEditorSelectionGranularity granularity)
{
	EHTMLEditorView *view;
	GDBusProxy *web_extension;

	view = e_html_editor_selection_ref_html_editor_view (selection);
	g_return_if_fail (view != NULL);

	e_html_editor_view_set_changed (view, TRUE);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		goto out;

	g_dbus_proxy_call (
		web_extension,
		"DOMSelectionModify",
		g_variant_new (
			"(tsbi)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			alter,
			forward,
			granularity),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

 out:
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

