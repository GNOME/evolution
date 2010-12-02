/*
 * e-mail-display.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-mail-display.h"

#include <config.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-util/e-plugin-ui.h"
#include "mail/em-composer-utils.h"
#include "mail/em-utils.h"
#include "mail/mail-tools.h"

#define E_MAIL_DISPLAY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_DISPLAY, EMailDisplayPrivate))

struct _EMailDisplayPrivate {
	EMFormatHTML *formatter;
};

enum {
	PROP_0,
	PROP_FORMATTER
};

static gpointer parent_class;

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions-1'>"
"      <menuitem action='add-to-address-book'/>"
"    </placeholder>"
"    <placeholder name='custom-actions-3'>"
"      <menu action='search-folder-menu'>"
"        <menuitem action='search-folder-recipient'/>"
"        <menuitem action='search-folder-sender'/>"
"      </menu>"
"    </placeholder>"
"  </popup>"
"</ui>";

static GtkActionEntry mailto_entries[] = {

	{ "add-to-address-book",
	  "contact-new",
	  N_("_Add to Address Book..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  NULL   /* Handled by EMailReader */ },

	{ "search-folder-recipient",
	  NULL,
	  N_("_To This Address"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  NULL   /* Handled by EMailReader */ },

	{ "search-folder-sender",
	  NULL,
	  N_("_From This Address"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  NULL   /* Handled by EMailReader */ },

	/*** Menus ***/

	{ "search-folder-menu",
	  "folder-saved-search",
	  N_("Create Search _Folder"),
	  NULL,
	  NULL,
	  NULL }
};

static void
mail_display_update_formatter_colors (EMailDisplay *display)
{
	EMFormatHTMLColorType type;
	EMFormatHTML *formatter;
	GdkColor *color;
	GtkStateType state;
	GtkStyle *style;

	state = gtk_widget_get_state (GTK_WIDGET (display));
	formatter = display->priv->formatter;

	style = gtk_widget_get_style (GTK_WIDGET (display));
	if (style == NULL)
		return;

	g_object_freeze_notify (G_OBJECT (formatter));

	color = &style->bg[state];
	type = EM_FORMAT_HTML_COLOR_BODY;
	em_format_html_set_color (formatter, type, color);

	color = &style->base[GTK_STATE_NORMAL];
	type = EM_FORMAT_HTML_COLOR_CONTENT;
	em_format_html_set_color (formatter, type, color);

	color = &style->dark[state];
	type = EM_FORMAT_HTML_COLOR_FRAME;
	em_format_html_set_color (formatter, type, color);

	color = &style->fg[state];
	type = EM_FORMAT_HTML_COLOR_HEADER;
	em_format_html_set_color (formatter, type, color);

	color = &style->text[state];
	type = EM_FORMAT_HTML_COLOR_TEXT;
	em_format_html_set_color (formatter, type, color);

	g_object_thaw_notify (G_OBJECT (formatter));
}

static void
mail_display_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FORMATTER:
			e_mail_display_set_formatter (
				E_MAIL_DISPLAY (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_display_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FORMATTER:
			g_value_set_object (
				value, e_mail_display_get_formatter (
				E_MAIL_DISPLAY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_display_dispose (GObject *object)
{
	EMailDisplayPrivate *priv;

	priv = E_MAIL_DISPLAY_GET_PRIVATE (object);

	if (priv->formatter) {
		g_object_unref (priv->formatter);
		priv->formatter = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_display_realize (GtkWidget *widget)
{
	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	mail_display_update_formatter_colors (E_MAIL_DISPLAY (widget));
}

static void
mail_display_style_set (GtkWidget *widget,
                        GtkStyle *previous_style)
{
	EMailDisplayPrivate *priv;

	priv = E_MAIL_DISPLAY_GET_PRIVATE (widget);

	/* Chain up to parent's style_set() method. */
	GTK_WIDGET_CLASS (parent_class)->style_set (widget, previous_style);

	mail_display_update_formatter_colors (E_MAIL_DISPLAY (widget));
	em_format_queue_redraw (EM_FORMAT (priv->formatter));
}

static void
mail_display_load_string (EWebView *web_view,
                          const gchar *string)
{
	EMailDisplayPrivate *priv;

	priv = E_MAIL_DISPLAY_GET_PRIVATE (web_view);
	g_return_if_fail (priv->formatter != NULL);

	if (em_format_busy (EM_FORMAT (priv->formatter)))
		return;

	/* Chain up to parent's load_string() method. */
	E_WEB_VIEW_CLASS (parent_class)->load_string (web_view, string);
}

static void
mail_display_url_requested (GtkHTML *html,
                            const gchar *uri,
                            GtkHTMLStream *stream)
{
	/* XXX Sadly, we must block the default method
	 *     until EMFormatHTML is made asynchronous. */
}

static void
mail_display_link_clicked (GtkHTML *html,
                           const gchar *uri)
{
	EMailDisplayPrivate *priv;

	priv = E_MAIL_DISPLAY_GET_PRIVATE (html);
	g_return_if_fail (priv->formatter != NULL);

	if (g_str_has_prefix (uri, "##")) {
		guint32 flags;

		flags = priv->formatter->header_wrap_flags;

		if (strcmp (uri, "##TO##") == 0) {
			if (!(flags & EM_FORMAT_HTML_HEADER_TO))
				flags |= EM_FORMAT_HTML_HEADER_TO;
			else
				flags &= ~EM_FORMAT_HTML_HEADER_TO;
		} else if (strcmp (uri, "##CC##") == 0) {
			if (!(flags & EM_FORMAT_HTML_HEADER_CC))
				flags |= EM_FORMAT_HTML_HEADER_CC;
			else
				flags &= ~EM_FORMAT_HTML_HEADER_CC;
		} else if (strcmp (uri, "##BCC##") == 0) {
			if (!(flags & EM_FORMAT_HTML_HEADER_BCC))
				flags |= EM_FORMAT_HTML_HEADER_BCC;
			else
				flags &= ~EM_FORMAT_HTML_HEADER_BCC;
		}

		priv->formatter->header_wrap_flags = flags;
		em_format_queue_redraw (EM_FORMAT (priv->formatter));

	} else if (g_ascii_strncasecmp (uri, "mailto:", 7) == 0) {
		EMFormat *format = EM_FORMAT (priv->formatter);
		gchar *folder_uri = NULL;

		if (format && format->folder)
			folder_uri = mail_tools_folder_to_url (format->folder);

		em_utils_compose_new_message_with_mailto (e_shell_get_default (), uri, folder_uri);

		g_free (folder_uri);
	} else if (*uri == '#')
		gtk_html_jump_to_anchor (html, uri + 1);

	else if (g_ascii_strncasecmp (uri, "thismessage:", 12) == 0)
		/* ignore */ ;

	else if (g_ascii_strncasecmp (uri, "cid:", 4) == 0)
		/* ignore */ ;

	else {
		/* Chain up to parent's link_clicked() method. */
		GTK_HTML_CLASS (parent_class)->link_clicked (html, uri);
	}
}

static void
mail_display_class_init (EMailDisplayClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	EWebViewClass *web_view_class;
	GtkHTMLClass *html_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailDisplayPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_display_set_property;
	object_class->get_property = mail_display_get_property;
	object_class->dispose = mail_display_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = mail_display_realize;
	widget_class->style_set = mail_display_style_set;

	web_view_class = E_WEB_VIEW_CLASS (class);
	web_view_class->load_string = mail_display_load_string;

	html_class = GTK_HTML_CLASS (class);
	html_class->url_requested = mail_display_url_requested;
	html_class->link_clicked = mail_display_link_clicked;

	g_object_class_install_property (
		object_class,
		PROP_FORMATTER,
		g_param_spec_object (
			"formatter",
			"HTML Formatter",
			NULL,
			EM_TYPE_FORMAT_HTML,
			G_PARAM_READWRITE));
}

static void
mail_display_init (EMailDisplay *display)
{
	EWebView *web_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GError *error = NULL;

	web_view = E_WEB_VIEW (display);

	display->priv = E_MAIL_DISPLAY_GET_PRIVATE (display);

	/* EWebView's action groups are added during its instance
	 * initialization function (like what we're in now), so it
	 * is safe to fetch them this early in construction. */
	action_group = e_web_view_get_action_group (web_view, "mailto");

	/* We don't actually handle the actions we're adding.
	 * EMailReader handles them.  How devious is that? */
	gtk_action_group_add_actions (
		action_group, mailto_entries,
		G_N_ELEMENTS (mailto_entries), display);

	/* Because we are loading from a hard-coded string, there is
	 * no chance of I/O errors.  Failure here implies a malformed
	 * UI definition.  Full stop. */
	ui_manager = e_web_view_get_ui_manager (web_view);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	if (error != NULL)
		g_error ("%s", error->message);
}

GType
e_mail_display_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailDisplayClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_display_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailDisplay),
			0,     /* n_preallocs */
			(GInstanceInitFunc) mail_display_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_WEB_VIEW, "EMailDisplay", &type_info, 0);
	}

	return type;
}

EMFormatHTML *
e_mail_display_get_formatter (EMailDisplay *display)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	return display->priv->formatter;
}

void
e_mail_display_set_formatter (EMailDisplay *display,
                              EMFormatHTML *formatter)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));
	g_return_if_fail (EM_IS_FORMAT_HTML (formatter));

	if (display->priv->formatter != NULL)
		g_object_unref (display->priv->formatter);

	display->priv->formatter = g_object_ref (formatter);

	g_object_notify (G_OBJECT (display), "formatter");
}
