/*
 * e-editor-widget.c
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

#include <glib/gi18n-lib.h>

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

	EEditorWidgetMode mode;

	/* FIXME WEBKIT Is this in widget's competence? */
	GList *spelling_langs;
};

G_DEFINE_TYPE (
	EEditorWidget,
	e_editor_widget,
	WEBKIT_TYPE_WEB_VIEW
);

enum {
	PROP_0,
	PROP_CHANGED,
	PROP_MODE,
	PROP_INLINE_SPELLING,
	PROP_MAGIC_LINKS,
	PROP_MAGIC_SMILEYS,
	PROP_SPELL_LANGUAGES,
	PROP_CAN_COPY,
	PROP_CAN_CUT,
	PROP_CAN_PASTE,
	PROP_CAN_REDO,
	PROP_CAN_UNDO
};

static void
editor_widget_strip_formatting (EEditorWidget *widget)
{
	gchar *plain, *html;

	plain = e_editor_widget_get_text_plain (widget);

	/* Convert \n to <br> */
	regex = g_regex_new ("\n", 0, 0, NULL);
	html = g_regex_replace (regex, plain, strlen (plain), 0, "<br>", 0, NULL);

	e_editor_widget_set_text_html (widget, html);

	g_free (plain);
	g_free (html);
}

static void
editor_widget_user_changed_contents_cb (EEditorWidget *widget,
					gpointer user_data)
{
	gboolean can_redo, can_undo;

	e_editor_widget_set_changed (widget, TRUE);

	can_redo = webkit_web_view_can_redo (WEBKIT_WEB_VIEW (widget));
	if ((widget->priv->can_redo ? TRUE : FALSE) != (can_redo ? TRUE : FALSE)) {
		widget->priv->can_redo = can_redo;
		g_object_notify (G_OBJECT (widget), "can-redo");
	}

	can_undo = webkit_web_view_can_undo (WEBKIT_WEB_VIEW (widget));
	if ((widget->priv->can_undo ? TRUE : FALSE) != (can_undo ? TRUE : FALSE)) {
		widget->priv->can_undo = can_undo;
		g_object_notify (G_OBJECT (widget), "can-undo");
	}
}

static void
editor_widget_selection_changed_cb (EEditorWidget *widget,
				    gpointer user_data)
{
	gboolean can_copy, can_cut, can_paste;

	can_copy = webkit_web_view_can_copy_clipboard (WEBKIT_WEB_VIEW (widget));
	if ((widget->priv->can_copy ? TRUE : FALSE) != (can_copy ? TRUE : FALSE)) {
		widget->priv->can_copy = can_copy;
		g_object_notify (G_OBJECT (widget), "can-copy");
	}

	can_cut = webkit_web_view_can_cut_clipboard (WEBKIT_WEB_VIEW (widget));
	if ((widget->priv->can_cut ? TRUE : FALSE) != (can_cut ? TRUE : FALSE)) {
		widget->priv->can_cut = can_cut;
		g_object_notify (G_OBJECT (widget), "can-cut");
	}

	can_paste = webkit_web_view_can_paste_clipboard (WEBKIT_WEB_VIEW (widget));
	if ((widget->priv->can_paste ? TRUE : FALSE) != (can_paste ? TRUE : FALSE)) {
		widget->priv->can_paste = can_paste;
		g_object_notify (G_OBJECT (widget), "can-paste");
	}
}

static void
e_editor_widget_get_property (GObject *object,
			      guint property_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {

		case PROP_MODE:
			g_value_set_int (
				value, e_editor_widget_get_mode (
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
		case PROP_CHANGED:
			g_value_set_boolean (
				value, e_editor_widget_get_changed (
				E_EDITOR_WIDGET (object)));
			return;
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
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_editor_widget_set_property (GObject *object,
			      guint property_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {

		case PROP_MODE:
			e_editor_widget_set_mode (
				E_EDITOR_WIDGET (object),
				g_value_get_int (value));
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
		case PROP_CHANGED:
			e_editor_widget_set_changed (
				E_EDITOR_WIDGET (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_editor_widget_finalize (GObject *object)
{
}

static void
e_editor_widget_class_init (EEditorWidgetClass *klass)
{
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (EEditorWidgetPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = e_editor_widget_get_property;
	object_class->set_property = e_editor_widget_set_property;
	object_class->finalize = e_editor_widget_finalize;

	g_object_class_install_property (
		object_class,
		PROP_MODE,
		g_param_spec_int (
			"mode",
			"Mode",
			"Edit HTML or plain text",
			E_EDITOR_WIDGET_MODE_PLAIN_TEXT,
		    	E_EDITOR_WIDGET_MODE_HTML,
		    	E_EDITOR_WIDGET_MODE_PLAIN_TEXT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_INLINE_SPELLING,
		g_param_spec_boolean (
			"inline-spelling",
			"Inline Spelling",
			"Check your spelling as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_MAGIC_LINKS,
		g_param_spec_boolean (
			"magic-links",
			"Magic Links",
			"Make URIs clickable as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_MAGIC_SMILEYS,
		g_param_spec_boolean (
			"magic-smileys",
			"Magic Smileys",
			"Convert emoticons to images as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_CHANGED,
		g_param_spec_boolean (
			"changed",
			_("Changed property"),
			_("Whether editor changed"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CAN_COPY,
		g_param_spec_boolean (
			"can-copy",
			"Can Copy",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_CAN_CUT,
		g_param_spec_boolean (
			"can-cut",
			"Can Cut",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_CAN_PASTE,
		g_param_spec_boolean (
			"can-paste",
			"Can Paste",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_CAN_REDO,
		g_param_spec_boolean (
			"can-redo",
			"Can Redo",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_CAN_UNDO,
		g_param_spec_boolean (
			"can-undo",
			"Can Undo",
			NULL,
			FALSE,
			G_PARAM_READABLE));
}

static void
e_editor_widget_init (EEditorWidget *editor)
{
	WebKitWebSettings *settings;
	WebKitDOMDocument *document;
	GSettings *g_settings;
	gboolean enable_spellchecking;

	editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		editor, E_TYPE_EDITOR_WIDGET, EEditorWidgetPrivate);

	webkit_web_view_set_editable (WEBKIT_WEB_VIEW (editor), TRUE);
	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (editor));

	g_settings = g_settings_new ("org.gnome.evolution.mail");
	enable_spellchecking = g_settings_get_boolean (
			g_settings, "composer-inline-spelling");

	g_object_set (
		G_OBJECT (settings),
		"enable-developer-extras", TRUE,
		"enable-dom-paste", TRUE,
	        "enable-plugins", FALSE,
		"enable-spell-checking", enable_spellchecking,
	        "enable-scripts", FALSE,
		NULL);

	g_object_unref(g_settings);

	webkit_web_view_set_settings (WEBKIT_WEB_VIEW (editor), settings);

	/* Don't use CSS when possible to preserve compatibility with older
	 * versions of Evolution or other MUAs */
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (editor));
	webkit_dom_document_exec_command (
		document, "styleWithCSS", FALSE, "false");

	g_signal_connect (editor, "user-changed-contents",
		G_CALLBACK (editor_widget_user_changed_contents_cb), NULL);
	g_signal_connect (editor, "selection-changed",
		G_CALLBACK (editor_widget_selection_changed_cb), NULL);
}

EEditorWidget *
e_editor_widget_new (void)
{
	return g_object_new (E_TYPE_EDITOR_WIDGET, NULL);
}

EEditorSelection *
e_editor_widget_get_selection (EEditorWidget *widget)
{
	return e_editor_selection_new (WEBKIT_WEB_VIEW (widget));
}

gboolean
e_editor_widget_get_changed (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->changed;
}

void
e_editor_widget_set_changed (EEditorWidget *widget,
			     gboolean changed)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if ((widget->priv->changed ? TRUE : FALSE) == (changed ? TRUE : FALSE))
		return;

	widget->priv->changed = changed;

	g_object_notify (G_OBJECT (widget), "changed");
}

EEditorWidgetMode
e_editor_widget_get_mode (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->mode;
}

void
e_editor_widget_set_mode (EEditorWidget *widget,
			  EEditorWidgetMode mode)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if (widget->priv->mode == mode)
		return;

	widget->priv->mode = mode;

	if (widget->priv->mode == E_EDITOR_WIDGET_MODE_PLAIN_TEXT)
		editor_widget_strip_formatting (widget);

	g_object_notify (G_OBJECT (widget), "mode");
}

gboolean
e_editor_widget_get_inline_spelling (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->inline_spelling;
}

void
e_editor_widget_set_inline_spelling (EEditorWidget *widget,
				     gboolean inline_spelling)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if ((widget->priv->inline_spelling ? TRUE : FALSE) == (inline_spelling ? TRUE : FALSE))
		return;

	widget->priv->inline_spelling = inline_spelling;

	g_object_notify (G_OBJECT (widget), "inline-spelling");
}

gboolean
e_editor_widget_get_magic_links (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->magic_links;
}

void
e_editor_widget_set_magic_links (EEditorWidget *widget,
				 gboolean magic_links)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if ((widget->priv->magic_links ? TRUE : FALSE) == (magic_links ? TRUE : FALSE))
		return;

	widget->priv->magic_links = magic_links;

	g_object_notify (G_OBJECT (widget), "magic-links");
}

gboolean
e_editor_widget_get_magic_smileys (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->magic_smileys;
}

void
e_editor_widget_set_magic_smileys (EEditorWidget *widget,
				   gboolean magic_smileys)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if ((widget->priv->magic_smileys ? TRUE : FALSE) == (magic_smileys ? TRUE : FALSE))
		return;

	widget->priv->magic_smileys = magic_smileys;

	g_object_notify (G_OBJECT (widget), "magic-smileys");
}

GList *
e_editor_widget_get_spell_languages (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), NULL);

	return g_list_copy (widget->priv->spelling_langs);
}

void
e_editor_widget_set_spell_languages (EEditorWidget *widget,
				     GList *spell_languages)
{
	GList *iter;

	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));
	g_return_if_fail (spell_languages);

	g_list_free_full (widget->priv->spelling_langs, g_free);

	widget->priv->spelling_langs = NULL;
	for (iter = spell_languages; iter; iter = g_list_next (iter)) {
		widget->priv->spelling_langs =
			g_list_append (
				widget->priv->spelling_langs,
		  		g_strdup (iter->data));
	}

	g_object_notify (G_OBJECT (widget), "spell-languages");
}

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

gchar *
e_editor_widget_get_text_plain (EEditorWidget *widget)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *element;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	element = webkit_dom_document_get_body (document);
	return webkit_dom_html_element_get_inner_text (element);
}

void
e_editor_widget_set_text_html (EEditorWidget *widget,
			       const gchar *text)
{
	webkit_web_view_load_html_string (WEBKIT_WEB_VIEW (widget), text, NULL);
}

