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
#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-util/e-plugin-ui.h"
#include "mail/em-composer-utils.h"
#include "mail/em-utils.h"

#define E_MAIL_DISPLAY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_DISPLAY, EMailDisplayPrivate))

struct _EMailDisplayPrivate {
	EMFormatHTML *formatter;
	GtkUIManager *ui_manager;
	gchar *selected_uri;
};

enum {
	PROP_0,
	PROP_ANIMATE,
	PROP_CARET_MODE,
	PROP_FORMATTER,
	PROP_SELECTED_URI
};

enum {
	POPUP_EVENT,
	STATUS_MESSAGE,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <menuitem action='http-open'/>"
"    <menuitem action='send-message'/>"
"    <menuitem action='uri-copy'/>"
"    <menuitem action='add-to-address-book'/>"
"    <menuitem action='mailto-copy'/>"
"    <menu action='search-folder-menu'>"
"      <menuitem action='search-folder-sender'/>"
"      <menuitem action='search-folder-recipient'/>"
"    </menu>"
"  </popup>"
"</ui>";

static void
action_add_to_address_book_cb (GtkAction *action,
                               EMailDisplay *display)
{
	CamelURL *curl;
	const gchar *uri;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (display));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	uri = e_mail_display_get_selected_uri (display);
	g_return_if_fail (uri != NULL);

	/* This should work because we checked it in update_actions(). */
	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	if (curl->path != NULL && *curl->path != '\0')
		em_utils_add_address (parent, curl->path);

	camel_url_free (curl);
}

static void
action_http_open_cb (GtkAction *action,
                     EMailDisplay *display)
{
	const gchar *uri;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (display));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	uri = e_mail_display_get_selected_uri (display);
	g_return_if_fail (uri != NULL);

	e_show_uri (parent, uri);
}

static void
action_mailto_copy_cb (GtkAction *action,
                       EMailDisplay *display)
{
	CamelURL *curl;
	CamelInternetAddress *inet_addr;
	GtkClipboard *clipboard;
	const gchar *uri;
	gchar *text;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	uri = e_mail_display_get_selected_uri (display);
	g_return_if_fail (uri != NULL);

	/* This should work because we checked it in update_actions(). */
	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	inet_addr = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);
	text = camel_address_encode (CAMEL_ADDRESS (inet_addr));
	if (text == NULL || *text == '\0')
		text = g_strdup (uri + strlen ("mailto:"));

	camel_object_unref (inet_addr);
	camel_url_free (curl);

	gtk_clipboard_set_text (clipboard, text, -1);
	gtk_clipboard_store (clipboard);

	g_free (text);
}

static void
action_send_message_cb (GtkAction *action,
                        EMailDisplay *display)
{
	const gchar *uri;

	uri = e_mail_display_get_selected_uri (display);
	g_return_if_fail (uri != NULL);

	em_utils_compose_new_message_with_mailto (uri, NULL);
}

static void
action_uri_copy_cb (GtkAction *action,
                    EMailDisplay *display)
{
	GtkClipboard *clipboard;
	const gchar *uri;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	uri = e_mail_display_get_selected_uri (display);
	g_return_if_fail (uri != NULL);

	gtk_clipboard_set_text (clipboard, uri, -1);
	gtk_clipboard_store (clipboard);
}

static GtkActionEntry uri_entries[] = {

	{ "uri-copy",
	  GTK_STOCK_COPY,
	  N_("_Copy Link Location"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_uri_copy_cb) },
};

static GtkActionEntry http_entries[] = {

	{ "http-open",
	  "emblem-web",
	  N_("_Open Link in Browser"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_http_open_cb) },
};

static GtkActionEntry mailto_entries[] = {

	{ "add-to-address-book",
	  "contact-new",
	  N_("_Add to Address Book..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_add_to_address_book_cb) },

	{ "mailto-copy",
	  GTK_STOCK_COPY,
	  N_("_Copy Email Address"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mailto_copy_cb) },

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

	{ "send-message",
	  "mail-message-new",
	  N_("_Send New Message To..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_send_message_cb) },

	/*** Menus ***/

	{ "search-folder-menu",
	  "folder-saved-search",
	  N_("Create Search _Folder"),
	  NULL,
	  NULL,
	  NULL }
};

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
                           GtkHTML *html,
                           gchar **uri,
                           EMFormatPURI **puri)
{
	EMFormat *formatter;
	gchar *text_uri;
	gchar *image_uri;
	gboolean is_cid;

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
			image_uri = temp;
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

static gboolean
mail_display_button_press_event_cb (EMailDisplay *display,
                                    GdkEventButton *event,
                                    GtkHTML *html)
{
	EMFormatPURI *puri = NULL;
	gboolean finished = TRUE;
	gchar *uri = NULL;

	/* The GtkHTML object may be the EMailDisplay itself
	 * or an inner iframe. */

	if (event->button != 3)
		return FALSE;

	mail_display_get_uri_puri (display, event, html, &uri, &puri);

	if (uri == NULL || g_str_has_prefix (uri, "##")) {
		g_free (uri);
		return FALSE;
	}

	finished = mail_display_emit_popup_event (display, event, uri, puri);

	g_free (uri);

	return finished;
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

		case PROP_SELECTED_URI:
			e_mail_display_set_selected_uri (
				E_MAIL_DISPLAY (object),
				g_value_get_string (value));
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

		case PROP_SELECTED_URI:
			g_value_set_string (
				value, e_mail_display_get_selected_uri (
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

	if (priv->ui_manager) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
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
	EMailDisplay *display = E_MAIL_DISPLAY (widget);
	GtkHTML *html = GTK_HTML (widget);

	if (mail_display_button_press_event_cb (display, event, html))
		return TRUE;

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
		G_CALLBACK (mail_display_button_press_event_cb), html);
}

static gboolean
mail_display_popup_event (EMailDisplay *display,
                          GdkEventButton *event,
                          const gchar *uri,
                          EMFormatPURI *puri)
{
	e_mail_display_set_selected_uri (display, uri);
	e_mail_display_show_popup_menu (display, event, NULL, NULL);

	return TRUE;
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

	class->popup_event = mail_display_popup_event;

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

	g_object_class_install_property (
		object_class,
		PROP_SELECTED_URI,
		g_param_spec_string (
			"selected-uri",
			"Selected URI",
			NULL,
			NULL,
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
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	const gchar *id;
	GError *error = NULL;

	display->priv = E_MAIL_DISPLAY_GET_PRIVATE (display);

	ui_manager = gtk_ui_manager_new ();
	display->priv->ui_manager = ui_manager;

	action_group = e_mail_display_add_action_group (display, "uri");

	gtk_action_group_add_actions (
		action_group, uri_entries,
		G_N_ELEMENTS (uri_entries), display);

	action_group = e_mail_display_add_action_group (display, "http");

	gtk_action_group_add_actions (
		action_group, http_entries,
		G_N_ELEMENTS (http_entries), display);

	action_group = e_mail_display_add_action_group (display, "mailto");

	gtk_action_group_add_actions (
		action_group, mailto_entries,
		G_N_ELEMENTS (mailto_entries), display);

	/* Because we are loading from a hard-coded string, there is
	 * no chance of I/O errors.  Failure here implies a malformed
	 * UI definition.  Full stop. */
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	if (error != NULL)
		g_error ("%s", error->message);

	id = "org.gnome.evolution.mail.display";
	e_plugin_ui_register_manager (ui_manager, id, display);
	e_plugin_ui_enable_manager (ui_manager, id);
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

const gchar *
e_mail_display_get_selected_uri (EMailDisplay *display)
{
	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	return display->priv->selected_uri;
}

void
e_mail_display_set_selected_uri (EMailDisplay *display,
                                 const gchar *selected_uri)
{
	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	g_free (display->priv->selected_uri);
	display->priv->selected_uri = g_strdup (selected_uri);

	g_object_notify (G_OBJECT (display), "selected-uri");
}

GtkAction *
e_mail_display_get_action (EMailDisplay *display,
                           const gchar *action_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_mail_display_get_ui_manager (display);

	return e_lookup_action (ui_manager, action_name);
}

GtkActionGroup *
e_mail_display_add_action_group (EMailDisplay *display,
                                 const gchar *group_name)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	const gchar *domain;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_mail_display_get_ui_manager (display);
	domain = GETTEXT_PACKAGE;

	action_group = gtk_action_group_new (group_name);
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	return action_group;
}

GtkActionGroup *
e_mail_display_get_action_group (EMailDisplay *display,
                                 const gchar *group_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_mail_display_get_ui_manager (display);

	return e_lookup_action_group (ui_manager, group_name);
}

GtkWidget *
e_mail_display_get_popup_menu (EMailDisplay *display)
{
	GtkUIManager *ui_manager;
	GtkWidget *menu;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	ui_manager = e_mail_display_get_ui_manager (display);
	menu = gtk_ui_manager_get_widget (ui_manager, "/context");
	g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

	return menu;
}

GtkUIManager *
e_mail_display_get_ui_manager (EMailDisplay *display)
{
	EMailDisplayPrivate *priv;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (display), NULL);

	priv = E_MAIL_DISPLAY_GET_PRIVATE (display);

	return priv->ui_manager;
}

void
e_mail_display_show_popup_menu (EMailDisplay *display,
                                GdkEventButton *event,
                                GtkMenuPositionFunc func,
                                gpointer user_data)
{
	GtkWidget *menu;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	e_mail_display_update_actions (display);

	menu = e_mail_display_get_popup_menu (display);

	if (event != NULL)
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, func,
			user_data, event->button, event->time);
	else
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, func,
			user_data, 0, gtk_get_current_event_time ());
}

void
e_mail_display_update_actions (EMailDisplay *display)
{
	CamelURL *curl;
	GtkActionGroup *action_group;
	gboolean scheme_is_http;
	gboolean scheme_is_mailto;
	gboolean uri_is_valid;
	gboolean visible;
	const gchar *uri;

	g_return_if_fail (E_IS_MAIL_DISPLAY (display));

	uri = e_mail_display_get_selected_uri (display);
	g_return_if_fail (uri != NULL);

	/* Parse the URI early so we know if the actions will work. */
	curl = camel_url_new (uri, NULL);
	uri_is_valid = (curl != NULL);
	camel_url_free (curl);

	scheme_is_http =
		(g_ascii_strncasecmp (uri, "http:", 5) == 0) ||
		(g_ascii_strncasecmp (uri, "https:", 6) == 0);

	scheme_is_mailto =
		(g_ascii_strncasecmp (uri, "mailto:", 7) == 0);

	/* Allow copying the URI even if it's malformed. */
	visible = !scheme_is_mailto;
	action_group = e_mail_display_get_action_group (display, "uri");
	gtk_action_group_set_visible (action_group, visible);

	visible = uri_is_valid && scheme_is_http;
	action_group = e_mail_display_get_action_group (display, "http");
	gtk_action_group_set_visible (action_group, visible);

	visible = uri_is_valid && scheme_is_mailto;
	action_group = e_mail_display_get_action_group (display, "mailto");
	gtk_action_group_set_visible (action_group, visible);
}
