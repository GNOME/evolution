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

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"

#define E_MAIL_DISPLAY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_DISPLAY, EMailDisplayPrivate))

struct _EMailDisplayPrivate {
	EMFormatHTML *formatter;
};

enum {
	PROP_0,
	PROP_ANIMATE,
	PROP_CARET_MODE,
	PROP_FORMATTER
};

enum {
	POPUP_EVENT,
	STATUS_MESSAGE,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static gboolean
mail_display_emit_popup_event (EMailDisplay *display,
                               GdkEventButton *event,
                               const gchar *uri,
                               EMFormatPURI *puri)
{
	CamelMimePart *mime_part;
	gboolean stop_handlers = FALSE;

	mime_part = (puri != NULL) ? puri->part : NULL;

	g_signal_emit (
		display, signals[POPUP_EVENT], 0,
		event, uri, mime_part, &stop_handlers);

	return stop_handlers;
}

static void
mail_display_emit_status_message (EMailDisplay *display,
                                  const gchar *status_message)
{
	g_signal_emit (display, signals[STATUS_MESSAGE], 0, status_message);
}

static void
mail_display_get_uri_puri (EMailDisplay *display,
                           GdkEventButton *event,
                           gchar **uri,
                           EMFormatPURI **puri)
{
	EMFormat *formatter;
	GtkHTML *html;
	gchar *text_uri;
	gchar *image_uri;
	gboolean is_cid;

	html = GTK_HTML (display);
	formatter = EM_FORMAT (display->priv->formatter);

	if (event != NULL) {
		text_uri = gtk_html_get_url_at (html, event->x, event->y);
		image_uri = gtk_html_get_image_src_at (html, event->x, event->y);
	} else {
		text_uri = gtk_html_get_cursor_url (html);
		image_uri = gtk_html_get_cursor_image_src (html);
	}

	is_cid = (image_uri != NULL) &&
		(g_ascii_strncasecmp (image_uri, "cid:", 4) == 0);

	if (image_uri != NULL) {
		if (strstr (image_uri, "://") == NULL && !is_cid) {
			gchar *temp;

			temp = g_strconcat ("file://", image_uri, NULL);
			g_free (image_uri);
			temp = image_uri;
		}
	}

	if (puri != NULL) {
		if (text_uri != NULL)
			*puri = em_format_find_puri (formatter, text_uri);

		if (*puri == NULL && image_uri != NULL)
			*puri = em_format_find_puri (formatter, image_uri);
	}

	if (uri != NULL) {
		*uri = NULL;
		if (is_cid) {
			if (text_uri != NULL)
				*uri = g_strdup_printf (
					"%s\n%s", text_uri, image_uri);
			else {
				*uri = image_uri;
				image_uri = NULL;
			}
		} else {
			*uri = text_uri;
			text_uri = NULL;
		}
	}

	g_free (text_uri);
	g_free (image_uri);
}

static void
mail_display_update_formatter_colors (EMailDisplay *display)
{
	EMFormatHTMLColorType type;
	EMFormatHTML *formatter;
	GdkColor *color;
	GtkStyle *style;
	gint state;

	state = GTK_WIDGET_STATE (display);
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
		case PROP_ANIMATE:
			e_mail_display_set_animate (
				E_MAIL_DISPLAY (object),
				g_value_get_boolean (value));
			return;

		case PROP_CARET_MODE:
			e_mail_display_set_caret_mode (
				E_MAIL_DISPLAY (object),
				g_value_get_boolean (value));
			return;

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
		case PROP_ANIMATE:
			g_value_set_boolean (
				value, e_mail_display_get_animate (
				E_MAIL_DISPLAY (object)));
			return;

		case PROP_CARET_MODE:
			g_value_set_boolean (
				value, e_mail_display_get_caret_mode (
				E_MAIL_DISPLAY (object)));
			return;

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
	em_format_redraw (EM_FORMAT (priv->formatter));
}

static gboolean
mail_display_button_press_event (GtkWidget *widget,
                                 GdkEventButton *event)
{
	if (event->button == 3) {
		EMailDisplay *display;
		EMFormatPURI *puri = NULL;
		gboolean stop_handlers = TRUE;
		gchar *uri = NULL;

		display = E_MAIL_DISPLAY (widget);
		mail_display_get_uri_puri (display, event, &uri, &puri);

		if (uri == NULL || !g_str_has_prefix (uri, "##"))
			stop_handlers = mail_display_emit_popup_event (
				display, event, uri, puri);

		g_free (uri);

		if (stop_handlers)
			return TRUE;
	}

	/* Chain up to parent's button_press_event() method. */
	return GTK_WIDGET_CLASS (parent_class)->
		button_press_event (widget, event);
}

static gboolean
mail_display_scroll_event (GtkWidget *widget,
                           GdkEventScroll *event)
{
	if (event->state & GDK_CONTROL_MASK) {
		switch (event->direction) {
			case GDK_SCROLL_UP:
				gtk_html_zoom_in (GTK_HTML (widget));
				return TRUE;
			case GDK_SCROLL_DOWN:
				gtk_html_zoom_out (GTK_HTML (widget));
				return TRUE;
			default:
				break;
		}
	}

	return FALSE;
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
				flags |= EM_FORMAT_HTML_HEADER_CC;
		} else if (strcmp (uri, "##BCC##") == 0) {
			if (!(flags & EM_FORMAT_HTML_HEADER_BCC))
				flags |= EM_FORMAT_HTML_HEADER_BCC;
			else
				flags |= EM_FORMAT_HTML_HEADER_BCC;
		}

		priv->formatter->header_wrap_flags = flags;
		em_format_redraw (EM_FORMAT (priv->formatter));

	} else if (*uri == '#')
		gtk_html_jump_to_anchor (html, uri + 1);

	else if (g_ascii_strncasecmp (uri, "thismessage:", 12) == 0)
		/* ignore */ ;

	else if (g_ascii_strncasecmp (uri, "cid:", 4) == 0)
		/* ignore */ ;

	else {
		gpointer parent;

		parent = gtk_widget_get_toplevel (GTK_WIDGET (html));
		parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

		e_show_uri (parent, uri);
	}
}

static void
mail_display_on_url (GtkHTML *html,
                     const gchar *uri)
{
	EMailDisplay *display;
	CamelInternetAddress *address;
	CamelURL *curl;
	const gchar *format = NULL;
	gchar *message = NULL;
	gchar *who;

	display = E_MAIL_DISPLAY (html);

	if (uri == NULL || *uri == '\0')
		goto exit;

	if (g_str_has_prefix (uri, "mailto:"))
		format = _("Click to mail %s");
	else if (g_str_has_prefix (uri, "callto:"))
		format = _("Click to call %s");
	else if (g_str_has_prefix (uri, "h323:"))
		format = _("Click to call %s");
	else if (g_str_has_prefix (uri, "sip:"))
		format = _("Click to call %s");
	else if (g_str_has_prefix (uri, "##"))
		message = g_strdup (_("Click to hide/unhide addresses"));
	else
		message = g_strdup_printf (_("Click to open %s"), uri);

	if (format == NULL)
		goto exit;

	curl = camel_url_new (uri, NULL);
	address = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (address), curl->path);
	who = camel_address_format (CAMEL_ADDRESS (address));
	camel_object_unref (address);
	camel_url_free (curl);

	if (who == NULL)
		who = g_strdup (strchr (uri, ':') + 1);

	message = g_strdup_printf (format, who);

	g_free (who);

exit:
	mail_display_emit_status_message (display, message);

	g_free (message);
}

static void
mail_display_iframe_created (GtkHTML *html,
                             GtkHTML *iframe)
{
	g_signal_connect_swapped (
		iframe, "button-press-event",
		G_CALLBACK (mail_display_button_press_event), html);
}

static void
mail_display_class_init (EMailDisplayClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
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
	widget_class->button_press_event = mail_display_button_press_event;
	widget_class->scroll_event = mail_display_scroll_event;

	html_class = GTK_HTML_CLASS (class);
	html_class->link_clicked = mail_display_link_clicked;
	html_class->on_url = mail_display_on_url;
	html_class->iframe_created = mail_display_iframe_created;

	g_object_class_install_property (
		object_class,
		PROP_ANIMATE,
		g_param_spec_boolean (
			"animate",
			"Animate Images",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CARET_MODE,
		g_param_spec_boolean (
			"caret-mode",
			"Caret Mode",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FORMATTER,
		g_param_spec_object (
			"formatter",
			"HTML Formatter",
			NULL,
			EM_TYPE_FORMAT_HTML,
			G_PARAM_READWRITE));

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailDisplayClass, popup_event),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__BOXED_POINTER_POINTER,
		G_TYPE_BOOLEAN, 3,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_POINTER,
		G_TYPE_POINTER);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailDisplayClass, status_message),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);
}

static void
mail_display_init (EMailDisplay *display)
{
	display->priv = E_MAIL_DISPLAY_GET_PRIVATE (display);
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
			GTK_TYPE_HTML, "EMailDisplay", &type_info, 0);
	}

	return type;
}

gboolean
e_mail_display_get_animate (EMailDisplay *display)
{
	/* XXX This is just here to maintain symmetry
	 *     with e_mail_display_set_animate(). */

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), FALSE);

	return gtk_html_get_animate (GTK_HTML (display));
}

void
e_mail_display_set_animate (EMailDisplay *display,
                            gboolean animate)
{
	/* XXX GtkHTML does not utilize GObject properties as well
	 *     as it could.  This just wraps gtk_html_set_animate()
	 *     so we can get a "notify::animate" signal. */

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	gtk_html_set_animate (GTK_HTML (display), animate);

	g_object_notify (G_OBJECT (display), "animate");
}

gboolean
e_mail_display_get_caret_mode (EMailDisplay *display)
{
	/* XXX This is just here to maintain symmetry
	 *     with e_mail_display_set_caret_mode(). */

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), FALSE);

	return gtk_html_get_caret_mode (GTK_HTML (display));
}

void
e_mail_display_set_caret_mode (EMailDisplay *display,
                               gboolean caret_mode)
{
	/* XXX GtkHTML does not utilize GObject properties as well
	 *     as it could.  This just wraps gtk_html_set_caret_mode()
	 *     so we can get a "notify::caret-mode" signal. */

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	gtk_html_set_caret_mode (GTK_HTML (display), caret_mode);

	g_object_notify (G_OBJECT (display), "caret-mode");
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
