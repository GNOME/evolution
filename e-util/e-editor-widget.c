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

struct _EEditorWidgetPrivate {
	gint dummy;
};

G_DEFINE_TYPE (
	EEditorWidget,
	e_editor_widget,
	WEBKIT_TYPE_WEB_VIEW
);


static void
e_editor_widget_get_property (GObject *object,
			      guint property_id,
			      GValue *value,
			      GParamSpec *pspec)
{

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_editor_widget_set_property (GObject *object,
			      guint property_id,
			      const GValue *value,
			      GParamSpec *pspec)
{

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_editor_widget_finalize (GObject *object)
{
	EEditorWidget *editor = E_EDITOR_WIDGET (object);
}

static void
e_editor_widget_class_init (EEditorWidgetClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = e_editor_widget_get_property;
	object_class->set_property = e_editor_widget_set_property;
	object_class->finalize = e_editor_widget_finalize;
}

static void
e_editor_widget_init (EEditorWidget *editor)
{
	WebKitWebSettings *settings;
	GSettings *g_settings;
	gboolean enable_spellchecking;

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
}

EEditorWidget *
e_editor_widget_new (void)
{
	return g_object_new (
			E_TYPE_EDITOR_WIDGET,
			"editable", TRUE, NULL);
}

EEditorSelection *
e_editor_widget_get_selection (EEditorWidget *widget)
{
	return e_editor_selection_new (WEBKIT_WEB_VIEW (widget));
}

void
e_editor_widget_insert_html (EEditorWidget *widget,
			     const gchar *html)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));
	g_return_if_fail (html != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	webkit_dom_document_exec_command (
			document, "insertHTML", FALSE, html);
}

void
e_editor_widget_insert_text (EEditorWidget *widget,
			     const gchar *text)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitDOMElement *element;

	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));
	g_return_if_fail (text != NULL);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);

	if (webkit_dom_dom_selection_get_range_count (selection) < 1) {
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);

	element = webkit_dom_document_create_element (document, "DIV", NULL);
	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (element), text, NULL);

	webkit_dom_range_insert_node (
		range, webkit_dom_node_get_first_child (
			WEBKIT_DOM_NODE (element)), NULL);

	g_object_unref (element);
}

