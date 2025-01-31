/*
 * e-mail-formatter.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include "e-mail-formatter.h"

#include <string.h>

#include <gdk/gdk.h>
#include <libebackend/libebackend.h>

#include <e-util/e-util.h>
#include <shell/e-shell.h>

#include "e-mail-formatter-enumtypes.h"
#include "e-mail-formatter-extension.h"
#include "e-mail-formatter-utils.h"
#include "e-mail-part.h"

#define d(x)

#define STYLESHEET_URI "evo-file://$EVOLUTION_WEBKITDATADIR/webview.css"

typedef struct _AsyncContext AsyncContext;

struct _EMailFormatterPrivate {
	EImageLoadingPolicy image_loading_policy;

	gboolean show_sender_photo;
	gboolean show_real_date;
	gboolean animate_images;

	GMutex property_lock;

	gchar *charset;
	gchar *default_charset;

	GdkRGBA colors[E_MAIL_FORMATTER_NUM_COLOR_TYPES];
};

struct _AsyncContext {
	GOutputStream *stream;
	EMailPartList *part_list;
	EMailFormatterHeaderFlags flags;
	EMailFormatterMode mode;
};

/* internal formatter extensions */
GType e_mail_formatter_attachment_get_type (void);
GType e_mail_formatter_audio_get_type (void);
GType e_mail_formatter_error_get_type (void);
GType e_mail_formatter_headers_get_type (void);
GType e_mail_formatter_image_get_type (void);
GType e_mail_formatter_message_rfc822_get_type (void);
GType e_mail_formatter_secure_button_get_type (void);
GType e_mail_formatter_source_get_type (void);
GType e_mail_formatter_text_enriched_get_type (void);
GType e_mail_formatter_text_html_get_type (void);
GType e_mail_formatter_text_plain_get_type (void);
#ifdef HAVE_MARKDOWN
GType e_mail_formatter_text_markdown_get_type (void);
#endif

static gpointer e_mail_formatter_parent_class = NULL;
static gint EMailFormatter_private_offset = 0;

static inline gpointer
e_mail_formatter_get_instance_private (EMailFormatter *self)
{
	return (G_STRUCT_MEMBER_P (self, EMailFormatter_private_offset));
}

enum {
	PROP_0,
	PROP_ANIMATE_IMAGES,
	PROP_BODY_COLOR,
	PROP_CHARSET,
	PROP_CITATION_COLOR,
	PROP_CONTENT_COLOR,
	PROP_DEFAULT_CHARSET,
	PROP_FRAME_COLOR,
	PROP_HEADER_COLOR,
	PROP_IMAGE_LOADING_POLICY,
	PROP_MARK_CITATIONS,
	PROP_SHOW_REAL_DATE,
	PROP_SHOW_SENDER_PHOTO,
	PROP_TEXT_COLOR
};

enum {
	NEED_REDRAW,
	CLAIM_ATTACHMENT,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL];

static void
async_context_free (AsyncContext *async_context)
{
	g_clear_object (&async_context->part_list);
	g_clear_object (&async_context->stream);

	g_slice_free (AsyncContext, async_context);
}

static EMailFormatterContext *
mail_formatter_create_context (EMailFormatter *formatter,
                               EMailPartList *part_list,
                               EMailFormatterMode mode,
                               EMailFormatterHeaderFlags flags)
{
	EMailFormatterClass *class;
	EMailFormatterContext *context;

	class = E_MAIL_FORMATTER_GET_CLASS (formatter);
	g_return_val_if_fail (class != NULL, NULL);

	g_warn_if_fail (class->context_size >= sizeof (EMailFormatterContext));

	context = g_malloc0 (class->context_size);
	context->part_list = g_object_ref (part_list);
	context->mode = mode;
	context->flags = flags;

	return context;
}

static void
mail_formatter_free_context (EMailFormatterContext *context)
{
	if (context->part_list != NULL)
		g_object_unref (context->part_list);

	g_free (context);
}

static void
shell_gone_cb (gpointer user_data,
	       GObject *gone_extension_registry)
{
	EMailFormatterClass *class = user_data;

	g_return_if_fail (class != NULL);

	g_clear_object (&class->extension_registry);
}

static void
e_mail_formatter_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ANIMATE_IMAGES:
			e_mail_formatter_set_animate_images (
				E_MAIL_FORMATTER (object),
				g_value_get_boolean (value));
			return;

		case PROP_BODY_COLOR:
			e_mail_formatter_set_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_BODY,
				g_value_get_boxed (value));
			return;

		case PROP_CHARSET:
			e_mail_formatter_set_charset (
				E_MAIL_FORMATTER (object),
				g_value_get_string (value));
			return;

		case PROP_CITATION_COLOR:
			e_mail_formatter_set_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_CITATION,
				g_value_get_boxed (value));
			return;

		case PROP_CONTENT_COLOR:
			e_mail_formatter_set_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_CONTENT,
				g_value_get_boxed (value));
			return;

		case PROP_DEFAULT_CHARSET:
			e_mail_formatter_set_default_charset (
				E_MAIL_FORMATTER (object),
				g_value_get_string (value));
			return;

		case PROP_FRAME_COLOR:
			e_mail_formatter_set_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_FRAME,
				g_value_get_boxed (value));
			return;

		case PROP_HEADER_COLOR:
			e_mail_formatter_set_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_HEADER,
				g_value_get_boxed (value));
			return;

		case PROP_IMAGE_LOADING_POLICY:
			e_mail_formatter_set_image_loading_policy (
				E_MAIL_FORMATTER (object),
				g_value_get_enum (value));
			return;

		case PROP_MARK_CITATIONS:
			e_mail_formatter_set_mark_citations (
				E_MAIL_FORMATTER (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_REAL_DATE:
			e_mail_formatter_set_show_real_date (
				E_MAIL_FORMATTER (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_SENDER_PHOTO:
			e_mail_formatter_set_show_sender_photo (
				E_MAIL_FORMATTER (object),
				g_value_get_boolean (value));
			return;

		case PROP_TEXT_COLOR:
			e_mail_formatter_set_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_TEXT,
				g_value_get_boxed (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_formatter_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ANIMATE_IMAGES:
			g_value_set_boolean (
				value,
				e_mail_formatter_get_animate_images (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_BODY_COLOR:
			g_value_set_boxed (
				value,
				e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_BODY));
			return;

		case PROP_CHARSET:
			g_value_take_string (
				value,
				e_mail_formatter_dup_charset (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_CITATION_COLOR:
			g_value_set_boxed (
				value,
				e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_CITATION));
			return;

		case PROP_CONTENT_COLOR:
			g_value_set_boxed (
				value,
				e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_CONTENT));
			return;

		case PROP_DEFAULT_CHARSET:
			g_value_take_string (
				value,
				e_mail_formatter_dup_default_charset (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_FRAME_COLOR:
			g_value_set_boxed (
				value,
				e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_FRAME));
			return;

		case PROP_HEADER_COLOR:
			g_value_set_boxed (
				value,
				e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_HEADER));
			return;

		case PROP_IMAGE_LOADING_POLICY:
			g_value_set_enum (
				value,
				e_mail_formatter_get_image_loading_policy (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_MARK_CITATIONS:
			g_value_set_boolean (
				value,
				e_mail_formatter_get_mark_citations (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_SHOW_REAL_DATE:
			g_value_set_boolean (
				value,
				e_mail_formatter_get_show_real_date (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_SHOW_SENDER_PHOTO:
			g_value_set_boolean (
				value,
				e_mail_formatter_get_show_sender_photo (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_TEXT_COLOR:
			g_value_set_boxed (
				value,
				e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_TEXT));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_formatter_finalize (GObject *object)
{
	EMailFormatter *self = E_MAIL_FORMATTER (object);

	g_free (self->priv->charset);
	g_free (self->priv->default_charset);

	g_mutex_clear (&self->priv->property_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_formatter_parent_class)->finalize (object);
}

static void
e_mail_formatter_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_formatter_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
mail_formatter_run (EMailFormatter *formatter,
                    EMailFormatterContext *context,
                    GOutputStream *stream,
                    GCancellable *cancellable)
{
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	gchar *hdr;
	const gchar *string;

	hdr = e_mail_formatter_get_html_header (formatter);
	g_output_stream_write_all (
		stream, hdr, strlen (hdr), NULL, cancellable, NULL);
	g_free (hdr);

	e_mail_part_list_queue_parts (context->part_list, NULL, &queue);

	head = g_queue_peek_head_link (&queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *part = link->data;
		const gchar *part_id;
		gboolean ok;

		part_id = e_mail_part_get_id (part);

		if (g_cancellable_is_cancelled (cancellable))
			break;

		if (part->is_hidden && !part->is_error) {
			if (e_mail_part_id_has_suffix (part, ".rfc822")) {
				link = e_mail_formatter_find_rfc822_end_iter (link);
			}

			if (link == NULL)
				break;

			continue;
		}

		if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING &&
		    !e_mail_part_get_is_printable (part))
			continue;

		/* Force formatting as source if needed */
		if (context->mode != E_MAIL_FORMATTER_MODE_SOURCE) {
			const gchar *mime_type;

			mime_type = e_mail_part_get_mime_type (part);
			if (mime_type == NULL)
				continue;

			ok = e_mail_formatter_format_as (
				formatter, context, part, stream,
				mime_type, cancellable);

			/* If the written part was message/rfc822 then
			 * jump to the end of the message, because content
			 * of the whole message has been formatted by
			 * message_rfc822 formatter */
			if (ok && e_mail_part_id_has_suffix (part, ".rfc822")) {
				link = e_mail_formatter_find_rfc822_end_iter (link);

				if (link == NULL)
					break;

				continue;
			}

		} else {
			ok = FALSE;
		}

		if (!ok) {
			/* We don't want to source these */
			if (e_mail_part_id_has_suffix (part, ".headers"))
				continue;

			e_mail_formatter_format_as (
				formatter, context, part, stream,
				"application/vnd.evolution.source", cancellable);

			/* .message is the entire message. There's nothing more
			 * to be written. */
			if (g_strcmp0 (part_id, ".message") == 0)
				break;

			/* If we just wrote source of a rfc822 message, then jump
			 * behind the message (otherwise source of all parts
			 * would be rendered twice) */
			if (e_mail_part_id_has_suffix (part, ".rfc822")) {

				do {
					part = link->data;
					if (e_mail_part_id_has_suffix (part, ".rfc822.end"))
						break;

					link = g_list_next (link);
				} while (link != NULL);

				if (link == NULL)
					break;
			}
		}
	}

	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));

	string = "</body></html>";
	g_output_stream_write_all (
		stream, string, strlen (string),
		NULL, cancellable, NULL);
}

static void
mail_formatter_update_style (EMailFormatter *formatter,
                             GtkStateFlags state)
{
	GtkStyleContext *style_context;
	GtkWidgetPath *widget_path;
	GdkRGBA rgba;

	g_object_freeze_notify (G_OBJECT (formatter));

	style_context = gtk_style_context_new ();
	widget_path = gtk_widget_path_new ();
	gtk_widget_path_append_type (widget_path, GTK_TYPE_WINDOW);
	gtk_style_context_set_path (style_context, widget_path);

	if (!gtk_style_context_lookup_color (style_context, "theme_bg_color", &rgba))
		gdk_rgba_parse (&rgba, E_UTILS_DEFAULT_THEME_BG_COLOR);

	e_mail_formatter_set_color (formatter, E_MAIL_FORMATTER_COLOR_BODY, &rgba);

	rgba.red *= 0.8;
	rgba.green *= 0.8;
	rgba.blue *= 0.8;
	e_mail_formatter_set_color (
		formatter, E_MAIL_FORMATTER_COLOR_FRAME, &rgba);

	if (!gtk_style_context_lookup_color (style_context, "theme_fg_color", &rgba))
		gdk_rgba_parse (&rgba, E_UTILS_DEFAULT_THEME_FG_COLOR);

	e_mail_formatter_set_color (formatter, E_MAIL_FORMATTER_COLOR_HEADER, &rgba);

	if (!gtk_style_context_lookup_color (style_context, "theme_base_color", &rgba))
		gdk_rgba_parse (&rgba, E_UTILS_DEFAULT_THEME_BASE_COLOR);

	e_mail_formatter_set_color  (formatter, E_MAIL_FORMATTER_COLOR_CONTENT, &rgba);

	if (!gtk_style_context_lookup_color (style_context, "theme_fg_color", &rgba))
		gdk_rgba_parse (&rgba, E_UTILS_DEFAULT_THEME_FG_COLOR);

	e_mail_formatter_set_color (formatter, E_MAIL_FORMATTER_COLOR_TEXT, &rgba);

	gtk_widget_path_free (widget_path);
	g_object_unref (style_context);

	g_object_thaw_notify (G_OBJECT (formatter));
}

static void
e_mail_formatter_base_init (EMailFormatterClass *class)
{
	EShell *shell;

	/* Register internal extensions. */
	g_type_ensure (e_mail_formatter_attachment_get_type ());
	/* This is currently disabled, because the WebKit player requires javascript,
	   which is disabled in Evolution. */
	/* g_type_ensure (e_mail_formatter_audio_get_type ()); */
	g_type_ensure (e_mail_formatter_error_get_type ());
	g_type_ensure (e_mail_formatter_headers_get_type ());
	g_type_ensure (e_mail_formatter_image_get_type ());
	g_type_ensure (e_mail_formatter_message_rfc822_get_type ());
	g_type_ensure (e_mail_formatter_secure_button_get_type ());
	g_type_ensure (e_mail_formatter_source_get_type ());
	g_type_ensure (e_mail_formatter_text_enriched_get_type ());
	g_type_ensure (e_mail_formatter_text_html_get_type ());
	g_type_ensure (e_mail_formatter_text_plain_get_type ());
#ifdef HAVE_MARKDOWN
	g_type_ensure (e_mail_formatter_text_markdown_get_type ());
#endif

	class->extension_registry = g_object_new (
		E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY, NULL);

	e_mail_formatter_extension_registry_load (
		class->extension_registry,
		E_TYPE_MAIL_FORMATTER_EXTENSION);

	e_extensible_load_extensions (
		E_EXTENSIBLE (class->extension_registry));

	class->text_html_flags =
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES |
		CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;

	shell = e_shell_get_default ();
	/* It can be NULL when creating developer documentation */
	if (shell)
		g_object_weak_ref (G_OBJECT (shell), shell_gone_cb, class);
}

static void
e_mail_formatter_class_init (EMailFormatterClass *class)
{
	GObjectClass *object_class;

	e_mail_formatter_parent_class = g_type_class_peek_parent (class);
	if (EMailFormatter_private_offset != 0)
		g_type_class_adjust_private_offset (class, &EMailFormatter_private_offset);

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = e_mail_formatter_set_property;
	object_class->get_property = e_mail_formatter_get_property;
	object_class->finalize = e_mail_formatter_finalize;
	object_class->constructed = e_mail_formatter_constructed;

	class->context_size = sizeof (EMailFormatterContext);
	class->run = mail_formatter_run;
	class->update_style = mail_formatter_update_style;

	g_object_class_install_property (
		object_class,
		PROP_ANIMATE_IMAGES,
		g_param_spec_boolean (
			"animate-images",
			"Animate images",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_BODY_COLOR,
		g_param_spec_boxed (
			"body-color",
			"Body Color",
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_CHARSET,
		g_param_spec_string (
			"charset",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_CITATION_COLOR,
		g_param_spec_boxed (
			"citation-color",
			"Citation Color",
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_CONTENT_COLOR,
		g_param_spec_boxed (
			"content-color",
			"Content Color",
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_CHARSET,
		g_param_spec_string (
			"default-charset",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_FRAME_COLOR,
		g_param_spec_boxed (
			"frame-color",
			"Frame Color",
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_HEADER_COLOR,
		g_param_spec_boxed (
			"header-color",
			"Header Color",
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_IMAGE_LOADING_POLICY,
		g_param_spec_enum (
			"image-loading-policy",
			"Image Loading Policy",
			NULL,
			E_TYPE_IMAGE_LOADING_POLICY,
			E_IMAGE_LOADING_POLICY_NEVER,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MARK_CITATIONS,
		g_param_spec_boolean (
			"mark-citations",
			"Mark Citations",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_REAL_DATE,
		g_param_spec_boolean (
			"show-real-date",
			"Show real Date header value",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_SENDER_PHOTO,
		g_param_spec_boolean (
			"show-sender-photo",
			"Show Sender Photo",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TEXT_COLOR,
		g_param_spec_boxed (
			"text-color",
			"Text Color",
			NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signals[CLAIM_ATTACHMENT] = g_signal_new (
		"claim-attachment",
		E_TYPE_MAIL_FORMATTER,
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailFormatterClass, claim_attachment),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1, E_TYPE_ATTACHMENT);

	signals[NEED_REDRAW] = g_signal_new (
		"need-redraw",
		E_TYPE_MAIL_FORMATTER,
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailFormatterClass, need_redraw),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);
}

static void
e_mail_formatter_init (EMailFormatter *formatter)
{
	GdkRGBA *rgba;

	formatter->priv = e_mail_formatter_get_instance_private (formatter);

	g_mutex_init (&formatter->priv->property_lock);

	rgba = &formatter->priv->colors[E_MAIL_FORMATTER_COLOR_BODY];
	gdk_rgba_parse (rgba, "#eeeeee");

	rgba = &formatter->priv->colors[E_MAIL_FORMATTER_COLOR_CONTENT];
	gdk_rgba_parse (rgba, "#ffffff");

	rgba = &formatter->priv->colors[E_MAIL_FORMATTER_COLOR_FRAME];
	gdk_rgba_parse (rgba, "#3f3f3f");

	rgba = &formatter->priv->colors[E_MAIL_FORMATTER_COLOR_HEADER];
	gdk_rgba_parse (rgba, "#000000");

	rgba = &formatter->priv->colors[E_MAIL_FORMATTER_COLOR_TEXT];
	gdk_rgba_parse (rgba, "#000000");
}

EMailFormatter *
e_mail_formatter_new (void)
{
	return g_object_new (E_TYPE_MAIL_FORMATTER, NULL);
}

GType
e_mail_formatter_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EMailFormatterClass),
			(GBaseInitFunc) e_mail_formatter_base_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) e_mail_formatter_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,	/* class_data */
			sizeof (EMailFormatter),
			0,	/* n_preallocs */
			(GInstanceInitFunc) e_mail_formatter_init,
			NULL	/* value_table */
		};

		const GInterfaceInfo e_extensible_interface_info = {
			(GInterfaceInitFunc) NULL
		};

		type = g_type_register_static (
			G_TYPE_OBJECT,
			"EMailFormatter", &type_info, 0);

		EMailFormatter_private_offset = g_type_add_instance_private (type, sizeof (EMailFormatterPrivate));

		g_type_add_interface_static (
			type, E_TYPE_EXTENSIBLE, &e_extensible_interface_info);
	}

	return type;
}

void
e_mail_formatter_claim_attachment (EMailFormatter *formatter,
				   EAttachment *attachment)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_signal_emit (formatter, signals[CLAIM_ATTACHMENT], 0, attachment);
}

void
e_mail_formatter_format_sync (EMailFormatter *formatter,
                              EMailPartList *part_list,
                              GOutputStream *stream,
                              EMailFormatterHeaderFlags flags,
                              EMailFormatterMode mode,
                              GCancellable *cancellable)
{
	EMailFormatterContext *context;
	EMailFormatterClass *class;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	/* EMailPartList can be NULL. */
	g_return_if_fail (G_IS_OUTPUT_STREAM (stream));

	class = E_MAIL_FORMATTER_GET_CLASS (formatter);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->run != NULL);

	context = mail_formatter_create_context (
		formatter, part_list, mode, flags);

	class->run (formatter, context, stream, cancellable);

	mail_formatter_free_context (context);
}

static void
mail_formatter_format_thread (GTask *task,
                              gpointer source_object,
                              gpointer task_data,
                              GCancellable *cancellable)
{
	AsyncContext *async_context = task_data;

	e_mail_formatter_format_sync (
		E_MAIL_FORMATTER (source_object),
		async_context->part_list,
		async_context->stream,
		async_context->flags,
		async_context->mode,
		cancellable);

	g_task_return_boolean (task, TRUE);
}

void
e_mail_formatter_format (EMailFormatter *formatter,
                         EMailPartList *part_list,
                         GOutputStream *stream,
                         EMailFormatterHeaderFlags flags,
                         EMailFormatterMode mode,
                         GAsyncReadyCallback callback,
                         GCancellable *cancellable,
                         gpointer user_data)
{
	GTask *task;
	AsyncContext *async_context;
	EMailFormatterClass *class;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	/* EMailPartList can be NULL. */
	g_return_if_fail (G_IS_OUTPUT_STREAM (stream));

	class = E_MAIL_FORMATTER_GET_CLASS (formatter);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->run != NULL);

	async_context = g_slice_new0 (AsyncContext);
	async_context->stream = g_object_ref (stream);
	async_context->flags = flags;
	async_context->mode = mode;

	task = g_task_new (formatter, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_formatter_format);
	g_task_set_task_data (task, async_context, (GDestroyNotify) async_context_free);

	if (part_list != NULL) {
		async_context->part_list = g_object_ref (part_list);
		g_task_run_in_thread (task, mail_formatter_format_thread);
	} else {
		g_task_return_boolean (task, TRUE);
	}

	g_object_unref (task);
}

gboolean
e_mail_formatter_format_finish (EMailFormatter *formatter,
                                GAsyncResult *result,
                                GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, formatter), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_formatter_format), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * e_mail_formatter_format_as:
 * @formatter: an #EMailFormatter
 * @context: an #EMailFormatterContext
 * @part: an #EMailPart
 * @stream: a #GOutputStream
 * @as_mime_type: (allow-none) mime-type to use for formatting, or %NULL
 * @cancellable: (allow-none) an optional #GCancellable
 *
 * Formats given @part using a @formatter extension for given mime type. When
 * the mime type is %NULL, the function will try to lookup the best formatter
 * for given @part by it's default mime type.
 *
 * Return Value: %TRUE on success, %FALSE when no suitable formatter is found or
 * when it fails to format the part. 
 */
gboolean
e_mail_formatter_format_as (EMailFormatter *formatter,
                            EMailFormatterContext *context,
                            EMailPart *part,
                            GOutputStream *stream,
                            const gchar *as_mime_type,
                            GCancellable *cancellable)
{
	EMailExtensionRegistry *extension_registry;
	GQueue *formatters;
	gboolean ok;
	d (
		gint _call_i;
		static gint _call = 0;
		G_LOCK_DEFINE_STATIC (_call);
		G_LOCK (_call);
		_call++;
		_call_i = _call;
		G_UNLOCK (_call)
	);

	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), FALSE);
	g_return_val_if_fail (part != NULL, FALSE);
	g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);

	if (as_mime_type == NULL || *as_mime_type == '\0')
		as_mime_type = e_mail_part_get_mime_type (part);

	if (as_mime_type == NULL || *as_mime_type == '\0')
		return FALSE;

	extension_registry =
		e_mail_formatter_get_extension_registry (formatter);
	formatters = e_mail_extension_registry_get_for_mime_type (
		extension_registry, as_mime_type);
	if (formatters == NULL)
		formatters = e_mail_extension_registry_get_fallback (
			extension_registry, as_mime_type);

	ok = FALSE;

	d (
		printf ("(%d) Formatting for part %s of type %s (found %d formatters)\n",
		_call_i, e_mail_part_get_id (part), as_mime_type,
		formatters ? g_queue_get_length (formatters) : 0));

	if (formatters != NULL) {
		GList *head, *link;

		head = g_queue_peek_head_link (formatters);

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailFormatterExtension *extension;

			extension = link->data;
			if (extension == NULL)
				continue;

			ok = e_mail_formatter_extension_format (
				extension, formatter, context,
				part, stream, cancellable);

			d (
				printf (
					"\t(%d) trying %s...%s\n", _call_i,
					G_OBJECT_TYPE_NAME (extension),
					ok ? "OK" : "failed"));

			if (ok)
				break;
		}
	}

	return ok;
}

static gboolean
emf_data_is_utf16 (CamelMimePart *part,
		   gboolean *out_be_variant)
{
	CamelStream *filtered_stream;
	CamelMimeFilter *filter;
	CamelStream *stream;
	const gchar *charset;
	gboolean is_utf16;

	g_return_val_if_fail (CAMEL_IS_MIME_PART (part), FALSE);

	stream = camel_stream_null_new ();
	filtered_stream = camel_stream_filter_new (stream);
	filter = camel_mime_filter_bestenc_new (CAMEL_BESTENC_GET_CHARSET);
	camel_stream_filter_add (CAMEL_STREAM_FILTER (filtered_stream), CAMEL_MIME_FILTER (filter));
	camel_data_wrapper_decode_to_stream_sync (camel_medium_get_content (CAMEL_MEDIUM (part)), filtered_stream, NULL, NULL);
	g_object_unref (filtered_stream);
	g_object_unref (stream);

	charset = camel_mime_filter_bestenc_get_best_charset (CAMEL_MIME_FILTER_BESTENC (filter));
	*out_be_variant = g_strcmp0 (charset, "UTF-16BE") == 0;
	is_utf16 = *out_be_variant || g_strcmp0 (charset, "UTF-16LE") == 0;

	g_object_unref (filter);

	return is_utf16;
}

/**
 * em_format_format_text:
 * @part: an #EMailPart to decode
 * @formatter: an #EMailFormatter
 * @stream: Where to write the converted text
 * @cancellable: optional #GCancellable object, or %NULL
 *
 * Decode/output a part's content to @stream.
 **/
void
e_mail_formatter_format_text (EMailFormatter *formatter,
                              EMailPart *part,
                              GOutputStream *stream,
                              GCancellable *cancellable)
{
	CamelMimeFilter *filter;
	const gchar *charset = NULL;
	CamelMimeFilter *windows = NULL;
	CamelMimePart *mime_part;
	CamelContentType *mime_type;
	gboolean utf16_be_variant = FALSE;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	mime_part = e_mail_part_ref_mime_part (part);
	mime_type = camel_data_wrapper_get_mime_type_field (CAMEL_DATA_WRAPPER (mime_part));

	if (emf_data_is_utf16 (mime_part, &utf16_be_variant)) {
		if (utf16_be_variant)
			charset = "UTF-16BE";
		else
			charset = "UTF-16LE";
	} else if (formatter->priv->charset != NULL) {
		charset = formatter->priv->charset;
	} else if (mime_type != NULL
		   && (charset = camel_content_type_param (mime_type, "charset"))
		   && g_ascii_strncasecmp (charset, "iso-8859-", 9) == 0) {
		GOutputStream *null_stream;
		GOutputStream *filter_stream;

		/* Since a few Windows mailers like to claim they sent
		 * out iso-8859-# encoded text when they really sent
		 * out windows-cp125#, do some simple sanity checking
		 * before we move on... */

		null_stream = camel_null_output_stream_new ();
		windows = camel_mime_filter_windows_new (charset);
		filter_stream = camel_filter_output_stream_new (
			null_stream, windows);
		g_filter_output_stream_set_close_base_stream (
			G_FILTER_OUTPUT_STREAM (filter_stream), FALSE);

		camel_data_wrapper_decode_to_output_stream_sync (
			CAMEL_DATA_WRAPPER (mime_part),
			filter_stream, cancellable, NULL);
		g_output_stream_flush (filter_stream, cancellable, NULL);

		g_object_unref (filter_stream);
		g_object_unref (null_stream);

		charset = camel_mime_filter_windows_real_charset (
			CAMEL_MIME_FILTER_WINDOWS (windows));
	} else if (charset == NULL) {
		charset = formatter->priv->default_charset;
	}

	filter = camel_mime_filter_charset_new (charset, "UTF-8");
	if (filter != NULL) {
		e_mail_part_set_converted_to_utf8 (part, TRUE);

		stream = camel_filter_output_stream_new (stream, filter);
		g_filter_output_stream_set_close_base_stream (
			G_FILTER_OUTPUT_STREAM (stream), FALSE);
		g_object_unref (filter);
	} else {
		g_object_ref (stream);
	}

	camel_data_wrapper_decode_to_output_stream_sync (
		camel_medium_get_content (CAMEL_MEDIUM (mime_part)),
		stream, cancellable, NULL);
	g_output_stream_flush (stream, cancellable, NULL);

	g_object_unref (stream);
	g_clear_object (&windows);
	g_clear_object (&mime_part);
}

const gchar *
e_mail_formatter_get_sub_html_header (EMailFormatter *formatter)
{
	return  "<!DOCTYPE HTML>\n"
		"<html>\n"
		"<head>\n"
		"<meta name=\"generator\" content=\"Evolution Mail\"/>\n"
		"<meta name=\"color-scheme\" content=\"light dark\">\n"
		"<title>Evolution Mail Display</title>\n"
		"<link type=\"text/css\" rel=\"stylesheet\" "
		" href=\"" STYLESHEET_URI "\"/>\n"
		"<style type=\"text/css\">\n"
		" table th { font-weight: bold; }\n"
		"</style>\n"
		"</head>"
		"<body class=\"-e-web-view-background-color -e-web-view-text-color\">";
}

gchar *
e_mail_formatter_get_html_header (EMailFormatter *formatter)
{
	return g_strdup (
		"<!DOCTYPE HTML>\n"
		"<html>\n"
		"<head>\n"
		"<meta name=\"generator\" content=\"Evolution Mail\"/>\n"
		"<meta name=\"color-scheme\" content=\"light dark\">\n"
		"<title>Evolution Mail Display</title>\n"
		"<link type=\"text/css\" rel=\"stylesheet\" "
		" href=\"" STYLESHEET_URI "\"/>\n"
		"<style type=\"text/css\">\n"
		" table th { font-weight: bold; }\n"
		"</style>\n"
		"</head>"
		"<body class=\"-e-mail-formatter-body-color "
		"-e-web-view-background-color -e-web-view-text-color\">");
}

EMailExtensionRegistry *
e_mail_formatter_get_extension_registry (EMailFormatter *formatter)
{
	EMailFormatterClass *klass;

	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);

	klass = E_MAIL_FORMATTER_GET_CLASS (formatter);
	g_return_val_if_fail (klass != NULL, NULL);

	return E_MAIL_EXTENSION_REGISTRY (klass->extension_registry);
}

CamelMimeFilterToHTMLFlags
e_mail_formatter_get_text_format_flags (EMailFormatter *formatter)
{
	EMailFormatterClass *klass;

	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), 0);

	klass = E_MAIL_FORMATTER_GET_CLASS (formatter);
	g_return_val_if_fail (klass != NULL, 0);

	return klass->text_html_flags;
}

const GdkRGBA *
e_mail_formatter_get_color (EMailFormatter *formatter,
                            EMailFormatterColor type)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);
	g_return_val_if_fail (type < E_MAIL_FORMATTER_NUM_COLOR_TYPES, NULL);

	return &(formatter->priv->colors[type]);
}

void
e_mail_formatter_set_color (EMailFormatter *formatter,
                            EMailFormatterColor type,
                            const GdkRGBA *color)
{
	GdkRGBA *format_color;
	const gchar *property_name;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (type < E_MAIL_FORMATTER_NUM_COLOR_TYPES);
	g_return_if_fail (color != NULL);

	format_color = &(formatter->priv->colors[type]);

	if (gdk_rgba_equal (color, format_color))
		return;

	format_color->red = color->red;
	format_color->green = color->green;
	format_color->blue = color->blue;

	switch (type) {
		case E_MAIL_FORMATTER_COLOR_BODY:
			property_name = "body-color";
			break;
		case E_MAIL_FORMATTER_COLOR_CITATION:
			property_name = "citation-color";
			break;
		case E_MAIL_FORMATTER_COLOR_CONTENT:
			property_name = "content-color";
			break;
		case E_MAIL_FORMATTER_COLOR_FRAME:
			property_name = "frame-color";
			break;
		case E_MAIL_FORMATTER_COLOR_HEADER:
			property_name = "header-color";
			break;
		case E_MAIL_FORMATTER_COLOR_TEXT:
			property_name = "text-color";
			break;
		default:
			g_return_if_reached ();
	}

	g_object_notify (G_OBJECT (formatter), property_name);
}

void
e_mail_formatter_update_style (EMailFormatter *formatter,
                               GtkStateFlags state)
{
	EMailFormatterClass *class;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	class = E_MAIL_FORMATTER_GET_CLASS (formatter);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->update_style != NULL);

	class->update_style (formatter, state);
}

EImageLoadingPolicy
e_mail_formatter_get_image_loading_policy (EMailFormatter *formatter)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), 0);

	return formatter->priv->image_loading_policy;
}

void
e_mail_formatter_set_image_loading_policy (EMailFormatter *formatter,
                                           EImageLoadingPolicy policy)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	if (policy == formatter->priv->image_loading_policy)
		return;

	formatter->priv->image_loading_policy = policy;

	g_object_notify (G_OBJECT (formatter), "image-loading-policy");
}

gboolean
e_mail_formatter_get_mark_citations (EMailFormatter *formatter)
{
	EMailFormatterClass *klass;
	guint32 flags;

	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), FALSE);

	klass = E_MAIL_FORMATTER_GET_CLASS (formatter);
	g_return_val_if_fail (klass != NULL, FALSE);

	flags = klass->text_html_flags;

	return ((flags & CAMEL_MIME_FILTER_TOHTML_MARK_CITATION) != 0);
}

void
e_mail_formatter_set_mark_citations (EMailFormatter *formatter,
                                     gboolean mark_citations)
{
	EMailFormatterClass *klass;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	klass = E_MAIL_FORMATTER_GET_CLASS (formatter);
	g_return_if_fail (klass != NULL);

	if (mark_citations)
		klass->text_html_flags |= CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;
	else
		klass->text_html_flags &= ~CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;

	g_object_notify (G_OBJECT (formatter), "mark-citations");
}

gboolean
e_mail_formatter_get_show_sender_photo (EMailFormatter *formatter)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), FALSE);

	return formatter->priv->show_sender_photo;
}

void
e_mail_formatter_set_show_sender_photo (EMailFormatter *formatter,
                                        gboolean show_sender_photo)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	if (formatter->priv->show_sender_photo == show_sender_photo)
		return;

	formatter->priv->show_sender_photo = show_sender_photo;

	g_object_notify (G_OBJECT (formatter), "show-sender-photo");
}

gboolean
e_mail_formatter_get_show_real_date (EMailFormatter *formatter)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), FALSE);

	return formatter->priv->show_real_date;
}

void
e_mail_formatter_set_show_real_date (EMailFormatter *formatter,
                                     gboolean show_real_date)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	if (formatter->priv->show_real_date == show_real_date)
		return;

	formatter->priv->show_real_date = show_real_date;

	g_object_notify (G_OBJECT (formatter), "show-real-date");
}

gboolean
e_mail_formatter_get_animate_images (EMailFormatter *formatter)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), FALSE);

	return formatter->priv->animate_images;
}

void
e_mail_formatter_set_animate_images (EMailFormatter *formatter,
                                     gboolean animate_images)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	if (formatter->priv->animate_images == animate_images)
		return;

	formatter->priv->animate_images = animate_images;

	g_object_notify (G_OBJECT (formatter), "animate-images");
}

const gchar *
e_mail_formatter_get_charset (EMailFormatter *formatter)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);

	return formatter->priv->charset;
}

gchar *
e_mail_formatter_dup_charset (EMailFormatter *formatter)
{
	const gchar *protected;
	gchar *duplicate;

	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);

	g_mutex_lock (&formatter->priv->property_lock);

	protected = e_mail_formatter_get_charset (formatter);
	duplicate = g_strdup (protected);

	g_mutex_unlock (&formatter->priv->property_lock);

	return duplicate;
}

void
e_mail_formatter_set_charset (EMailFormatter *formatter,
                              const gchar *charset)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	g_mutex_lock (&formatter->priv->property_lock);

	if (g_strcmp0 (formatter->priv->charset, charset) == 0) {
		g_mutex_unlock (&formatter->priv->property_lock);
		return;
	}

	g_free (formatter->priv->charset);
	formatter->priv->charset = g_strdup (charset);

	g_mutex_unlock (&formatter->priv->property_lock);

	g_object_notify (G_OBJECT (formatter), "charset");
}

const gchar *
e_mail_formatter_get_default_charset (EMailFormatter *formatter)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);

	return formatter->priv->default_charset;
}

gchar *
e_mail_formatter_dup_default_charset (EMailFormatter *formatter)
{
	const gchar *protected;
	gchar *duplicate;

	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);

	g_mutex_lock (&formatter->priv->property_lock);

	protected = e_mail_formatter_get_default_charset (formatter);
	duplicate = g_strdup (protected);

	g_mutex_unlock (&formatter->priv->property_lock);

	return duplicate;
}

void
e_mail_formatter_set_default_charset (EMailFormatter *formatter,
                                      const gchar *default_charset)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (default_charset && *default_charset);

	g_mutex_lock (&formatter->priv->property_lock);

	if (g_strcmp0 (formatter->priv->default_charset, default_charset) == 0) {
		g_mutex_unlock (&formatter->priv->property_lock);
		return;
	}

	g_free (formatter->priv->default_charset);
	formatter->priv->default_charset = g_strdup (default_charset);

	g_mutex_unlock (&formatter->priv->property_lock);

	g_object_notify (G_OBJECT (formatter), "default-charset");
}

