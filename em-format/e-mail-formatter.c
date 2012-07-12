/*
 * e-mail-formatter.c
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

#include "e-mail-formatter.h"

#include <camel/camel.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-formatter-utils.h"
#include "e-mail-part.h"

#include "e-mail-format-extensions.h"

#include <e-util/e-util.h>
#include <libebackend/libebackend.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

#define d(x)

struct _EMailFormatterPrivate {
	EMailImageLoadingPolicy image_loading_policy;

	guint only_local_photos	: 1;
	guint show_sender_photo	: 1;
	guint show_real_date	: 1;
        guint animate_images    : 1;

	gchar *charset;
	gchar *default_charset;

	GQueue *header_list;
};

#define E_MAIL_FORMATTER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_FORMATTER, EMailFormatterPrivate))\

static gpointer e_mail_formatter_parent_class = 0;

enum {
	PROP_0,
	PROP_BODY_COLOR,
	PROP_CITATION_COLOR,
	PROP_CONTENT_COLOR,
	PROP_FRAME_COLOR,
	PROP_HEADER_COLOR,
	PROP_TEXT_COLOR,
	PROP_IMAGE_LOADING_POLICY,
	PROP_FORCE_IMAGE_LOADING,
	PROP_MARK_CITATIONS,
	PROP_ONLY_LOCAL_PHOTOS,
	PROP_SHOW_SENDER_PHOTO,
	PROP_SHOW_REAL_DATE,
        PROP_ANIMATE_IMAGES,
	PROP_CHARSET,
	PROP_DEFAULT_CHARSET
};

enum {
	NEED_REDRAW,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL];

static void
mail_formatter_run (EMailFormatter *formatter,
                    EMailFormatterContext *context,
                    CamelStream *stream,
                    GCancellable *cancellable)
{
	GSList *iter;
	gchar *hdr;

	hdr = e_mail_formatter_get_html_header (formatter);
	camel_stream_write_string (stream, hdr, cancellable, NULL);
	g_free (hdr);

	for (iter = context->parts; iter; iter = iter->next) {

		EMailPart *part;
		gboolean ok;

		if (g_cancellable_is_cancelled (cancellable))
			break;

		part = iter->data;
		if (!part)
			continue;

		if (part->is_hidden && !part->is_error) {
			if (g_str_has_suffix (part->id, ".rfc822")) {
				iter = e_mail_formatter_find_rfc822_end_iter (iter);
			}

			if (!iter)
				break;

			continue;
		}

		/* Force formatting as source if needed */
		if (context->mode != E_MAIL_FORMATTER_MODE_SOURCE) {

			if (!part->mime_type)
				continue;

			ok = e_mail_formatter_format_as (
				formatter, context, part, stream,
				part->mime_type, cancellable);

			/* If the written part was message/rfc822 then
			 * jump to the end of the message, because content
			 * of the whole message has been formatted by
			 * message_rfc822 formatter */
			if (ok && g_str_has_suffix (part->id, ".rfc822")) {
				iter = e_mail_formatter_find_rfc822_end_iter (iter);

				if (!iter)
					break;

				continue;
			}

		} else {
			ok = FALSE;
		}

		if (!ok) {
			/* We don't want to source these */
			if (g_str_has_suffix (part->id, ".headers") ||
			    g_str_has_suffix (part->id, "attachment-bar"))
				continue;

			e_mail_formatter_format_as (
				formatter, context, part, stream,
				"application/vnd.evolution.source", cancellable);

			/* .message is the entire message. There's nothing more
			 * to be written. */
			if (g_strcmp0 (part->id, ".message") == 0)
				break;

			/* If we just wrote source of a rfc822 message, then jump
			 * behind the message (otherwise source of all parts
			 * would be rendered twice) */
			if (g_str_has_suffix (part->id, ".rfc822")) {

				do {
					part = iter->data;
					if (part && g_str_has_suffix (part->id, ".rfc822.end"))
						break;

					iter = iter->next;
				} while (iter);
			}
		}
	}

	camel_stream_write_string (stream, "</body></html>", cancellable, NULL);
}

static EMailFormatterContext *
mail_formatter_create_context (EMailFormatter *formatter)
{
	EMailFormatterClass *formatter_class;

	formatter_class = E_MAIL_FORMATTER_GET_CLASS (formatter);

	if (formatter_class->create_context) {
		if (!formatter_class->free_context) {
			g_warning ("%s implements create_context() but "
				   "does not implement free_context()!",
				   G_OBJECT_TYPE_NAME (formatter));
		}

		return formatter_class->create_context (formatter);
	}

	return g_new0 (EMailFormatterContext, 1);
}

static void
mail_formatter_free_context (EMailFormatter *formatter,
                             EMailFormatterContext *context)
{
	EMailFormatterClass *formatter_class;

	formatter_class = E_MAIL_FORMATTER_GET_CLASS (formatter);

	if (formatter_class->free_context) {
		formatter_class->free_context (formatter, context);
	} else {
		g_free (context);
	}
}

static void
mail_formatter_set_style (EMailFormatter *formatter,
                          GtkStyle *style,
                          GtkStateType state)
{
	GdkColor *color;
	EMailFormatterColorType type;

	g_object_freeze_notify (G_OBJECT (formatter));

	color = &style->bg[state];
	type = E_MAIL_FORMATTER_COLOR_BODY;
	e_mail_formatter_set_color (formatter, type, color);

	color = &style->base[GTK_STATE_NORMAL];
	type = E_MAIL_FORMATTER_COLOR_CONTENT;
	e_mail_formatter_set_color  (formatter, type, color);

	color = &style->dark[state];
	type = E_MAIL_FORMATTER_COLOR_FRAME;
	e_mail_formatter_set_color  (formatter, type, color);

	color = &style->fg[state];
	type = E_MAIL_FORMATTER_COLOR_HEADER;
	e_mail_formatter_set_color  (formatter, type, color);

	color = &style->text[state];
	type = E_MAIL_FORMATTER_COLOR_TEXT;
	e_mail_formatter_set_color  (formatter, type, color);

	g_object_thaw_notify (G_OBJECT (formatter));
}

static void
e_mail_formatter_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BODY_COLOR:
			e_mail_formatter_set_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_BODY,
				g_value_get_boxed (value));
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
				g_value_get_int (value));
			return;

		case PROP_MARK_CITATIONS:
			e_mail_formatter_set_mark_citations (
				E_MAIL_FORMATTER (object),
				g_value_get_boolean (value));
			return;

		case PROP_ONLY_LOCAL_PHOTOS:
			e_mail_formatter_set_only_local_photos (
				E_MAIL_FORMATTER (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_SENDER_PHOTO:
			e_mail_formatter_set_show_sender_photo (
				E_MAIL_FORMATTER (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_REAL_DATE:
			e_mail_formatter_set_show_real_date (
				E_MAIL_FORMATTER (object),
				g_value_get_boolean (value));
			return;

		case PROP_TEXT_COLOR:
			e_mail_formatter_set_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_TEXT,
				g_value_get_boxed (value));
			return;

		case PROP_ANIMATE_IMAGES:
			e_mail_formatter_set_animate_images (
				E_MAIL_FORMATTER (object),
				g_value_get_boolean (value));
			return;

		case PROP_CHARSET:
			e_mail_formatter_set_charset (
				E_MAIL_FORMATTER (object),
				g_value_get_string (value));
			return;

		case PROP_DEFAULT_CHARSET:
			e_mail_formatter_set_default_charset (
				E_MAIL_FORMATTER (object),
				g_value_get_string (value));
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
		case PROP_BODY_COLOR:
			g_value_set_boxed (value,
			e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_BODY));
			return;

		case PROP_CITATION_COLOR:
			g_value_set_boxed (value,
			e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_CITATION));
			return;

		case PROP_CONTENT_COLOR:
			g_value_set_boxed (value,
			e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_CONTENT));
			return;

		case PROP_FRAME_COLOR:
			g_value_set_boxed (value,
			e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_FRAME));
			return;

		case PROP_HEADER_COLOR:
			g_value_set_boxed (value,
			e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_HEADER));
			return;

		case PROP_IMAGE_LOADING_POLICY:
			g_value_set_int (
				value,
				e_mail_formatter_get_image_loading_policy (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_MARK_CITATIONS:
			g_value_set_boolean (
				value, e_mail_formatter_get_mark_citations (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_ONLY_LOCAL_PHOTOS:
			g_value_set_boolean (
				value, e_mail_formatter_get_only_local_photos (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_SHOW_SENDER_PHOTO:
			g_value_set_boolean (
				value, e_mail_formatter_get_show_sender_photo (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_SHOW_REAL_DATE:
			g_value_set_boolean (
				value, e_mail_formatter_get_show_real_date (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_TEXT_COLOR:
			g_value_set_boxed (value,
			e_mail_formatter_get_color (
				E_MAIL_FORMATTER (object),
				E_MAIL_FORMATTER_COLOR_TEXT));
			return;

		case PROP_ANIMATE_IMAGES:
			g_value_set_boolean (
				value, e_mail_formatter_get_animate_images (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_CHARSET:
			g_value_set_string (
				value, e_mail_formatter_get_charset (
				E_MAIL_FORMATTER (object)));
			return;

		case PROP_DEFAULT_CHARSET:
			g_value_set_string (
				value, e_mail_formatter_get_default_charset (
				E_MAIL_FORMATTER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_formatter_init (EMailFormatter *formatter)
{
	formatter->priv = E_MAIL_FORMATTER_GET_PRIVATE (formatter);

	formatter->priv->header_list = g_queue_new ();
	e_mail_formatter_set_default_headers (formatter);
}

static void
e_mail_formatter_finalize (GObject *object)
{
	EMailFormatterPrivate *priv;

	priv = E_MAIL_FORMATTER (object)->priv;

	if (priv->charset) {
		g_free (priv->charset);
		priv->charset = NULL;
	}

	if (priv->default_charset) {
		g_free (priv->default_charset);
		priv->default_charset = NULL;
	}

	if (priv->header_list) {
		e_mail_formatter_clear_headers (E_MAIL_FORMATTER (object));
		g_queue_free (priv->header_list);
		priv->header_list = NULL;
	}

	/* Chain up to parent's finalize() */
	G_OBJECT_CLASS (e_mail_formatter_parent_class)->finalize (object);
}

static void
e_mail_formatter_base_init (EMailFormatterClass *class)
{
	class->extension_registry = g_object_new (
		E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY, NULL);

	e_mail_formatter_internal_extensions_load (
			E_MAIL_EXTENSION_REGISTRY (class->extension_registry));

	e_extensible_load_extensions (
		E_EXTENSIBLE (class->extension_registry));

	class->text_html_flags =
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES |
		CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;
}

static void
e_mail_formatter_base_finalize (EMailFormatterClass *class)
{
	g_object_unref (class->extension_registry);
}

static void
e_mail_formatter_constructed (GObject *object)
{
	G_OBJECT_CLASS (e_mail_formatter_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
e_mail_formatter_class_init (EMailFormatterClass *class)
{
	GObjectClass *object_class;
	GdkColor *color;

	e_mail_formatter_parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailFormatterPrivate));

	class->run = mail_formatter_run;

	/* EMailFormatter calls these directly */
	class->create_context = NULL;
	class->free_context = NULL;
	class->set_style = mail_formatter_set_style;

	color = &class->colors[E_MAIL_FORMATTER_COLOR_BODY];
	gdk_color_parse ("#eeeeee", color);

	color = &class->colors[E_MAIL_FORMATTER_COLOR_CONTENT];
	gdk_color_parse ("#ffffff", color);

	color = &class->colors[E_MAIL_FORMATTER_COLOR_FRAME];
	gdk_color_parse ("#3f3f3f", color);

	color = &class->colors[E_MAIL_FORMATTER_COLOR_HEADER];
	gdk_color_parse ("#eeeeee", color);

	color = &class->colors[E_MAIL_FORMATTER_COLOR_TEXT];
	gdk_color_parse ("#000000", color);

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_mail_formatter_constructed;
	object_class->get_property = e_mail_formatter_get_property;
	object_class->set_property = e_mail_formatter_set_property;
	object_class->finalize = e_mail_formatter_finalize;

	g_object_class_install_property (
		object_class,
		PROP_BODY_COLOR,
		g_param_spec_boxed (
			"body-color",
			"Body Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CITATION_COLOR,
		g_param_spec_boxed (
			"citation-color",
			"Citation Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CONTENT_COLOR,
		g_param_spec_boxed (
			"content-color",
			"Content Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FRAME_COLOR,
		g_param_spec_boxed (
			"frame-color",
			"Frame Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HEADER_COLOR,
		g_param_spec_boxed (
			"header-color",
			"Header Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	/* FIXME Make this a proper enum property. */
	g_object_class_install_property (
		object_class,
		PROP_IMAGE_LOADING_POLICY,
		g_param_spec_int (
			"image-loading-policy",
			"Image Loading Policy",
			NULL,
			E_MAIL_IMAGE_LOADING_POLICY_NEVER,
			E_MAIL_IMAGE_LOADING_POLICY_ALWAYS,
			E_MAIL_IMAGE_LOADING_POLICY_NEVER,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MARK_CITATIONS,
		g_param_spec_boolean (
			"mark-citations",
			"Mark Citations",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ONLY_LOCAL_PHOTOS,
		g_param_spec_boolean (
			"only-local-photos",
			"Only Local Photos",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_SENDER_PHOTO,
		g_param_spec_boolean (
			"show-sender-photo",
			"Show Sender Photo",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_REAL_DATE,
		g_param_spec_boolean (
			"show-real-date",
			"Show real Date header value",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_TEXT_COLOR,
		g_param_spec_boxed (
			"text-color",
			"Text Color",
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ANIMATE_IMAGES,
		g_param_spec_boolean (
			"animate-images",
			"Animate images",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CHARSET,
		g_param_spec_string (
			"charset",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_CHARSET,
		g_param_spec_string (
			"default-charset",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	signals[NEED_REDRAW] = g_signal_new (
				"need-redraw",
				E_TYPE_MAIL_FORMATTER,
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EMailFormatterClass, need_redraw),
				NULL,
				NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE,  0, NULL);
}

static void
e_mail_formatter_extensible_interface_init (EExtensibleInterface *interface)
{

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
			(GBaseFinalizeFunc) e_mail_formatter_base_finalize,
			(GClassInitFunc) e_mail_formatter_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,	/* class_data */
			sizeof (EMailFormatter),
			0,	/* n_preallocs */
			(GInstanceInitFunc) e_mail_formatter_init,
			NULL	/* value_table */
		};

		const GInterfaceInfo e_extensible_interface_info = {
			(GInterfaceInitFunc) e_mail_formatter_extensible_interface_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
				"EMailFormatter", &type_info, 0);

		g_type_add_interface_static (type,
			E_TYPE_EXTENSIBLE, &e_extensible_interface_info);
	}

	return type;
}

void
e_mail_formatter_format_sync (EMailFormatter *formatter,
                              EMailPartList *parts,
                              CamelStream *stream,
                              guint32 flags,
                              EMailFormatterMode mode,
                              GCancellable *cancellable)
{
	EMailFormatterContext *context;
	EMailFormatterClass *formatter_class;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (CAMEL_IS_STREAM (stream));

	formatter_class = E_MAIL_FORMATTER_GET_CLASS (formatter);
	g_return_if_fail (formatter_class->run != NULL);

	context = mail_formatter_create_context (formatter);
	context->message = parts->message;
	context->folder = parts->folder;
	context->message_uid = parts->message_uid;
	context->parts = parts->list;
	context->flags = flags;
	context->mode = mode;

	formatter_class->run (
		formatter, context, stream, cancellable);

	mail_formatter_free_context (formatter, context);
}

static void
mail_format_async_prepare (GSimpleAsyncResult *result,
                           GObject *object,
                           GCancellable *cancellable)
{
	EMailFormatterContext *context;
	EMailFormatterClass *formatter_class;
	CamelStream *stream;

	context = g_object_get_data (G_OBJECT (result), "context");
	stream = g_object_get_data (G_OBJECT (result), "stream");

	formatter_class = E_MAIL_FORMATTER_GET_CLASS (object);
	formatter_class->run (
		E_MAIL_FORMATTER (object), context, stream, cancellable);
}

void
e_mail_formatter_format (EMailFormatter *formatter,
                         EMailPartList *parts,
                         CamelStream *stream,
                         guint32 flags,
                         EMailFormatterMode mode,
                         GAsyncReadyCallback callback,
                         GCancellable *cancellable,
                         gpointer user_data)
{
	GSimpleAsyncResult *simple;
	EMailFormatterContext *context;
	EMailFormatterClass *formatter_class;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (CAMEL_IS_STREAM (stream));

	formatter_class = E_MAIL_FORMATTER_GET_CLASS (formatter);
	g_return_if_fail (formatter_class->run != NULL);

	simple = g_simple_async_result_new (
			G_OBJECT (formatter), callback,
			user_data, e_mail_formatter_format);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	if (!parts && callback) {
		callback (G_OBJECT (formatter), G_ASYNC_RESULT (simple), user_data);
		g_object_unref (simple);
		return;
	}

	context = mail_formatter_create_context (formatter);
	context->message = g_object_ref (parts->message);
	context->folder = g_object_ref (parts->folder);
	context->message_uid = g_strdup (parts->message_uid);
	context->parts = g_slist_copy (parts->list);
	g_slist_foreach (context->parts, (GFunc) e_mail_part_ref, NULL);
	context->flags = flags;
	context->mode = mode;

	g_object_set_data (G_OBJECT (simple), "context", context);
	g_object_set_data (G_OBJECT (simple), "stream", stream);

	g_simple_async_result_run_in_thread (
		simple, mail_format_async_prepare,
		G_PRIORITY_DEFAULT, cancellable);

	g_object_unref (simple);
}

CamelStream *
e_mail_formatter_format_finished (EMailFormatter *formatter,
                                  GAsyncResult *result,
                                  GError *error)
{
	EMailFormatterContext *context;

	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

	context = g_object_get_data (G_OBJECT (result), "context");

	g_free (context->message_uid);
	g_object_unref (context->message);
	g_object_unref (context->folder);
	g_slist_foreach (context->parts, (GFunc) e_mail_part_unref, NULL);
	g_slist_free (context->parts);
	mail_formatter_free_context (formatter, context);

	return g_object_get_data (G_OBJECT (result), "stream");
}

/**
 * e_mail_formatter_format_as:
 * @formatter: an #EMailFormatter
 * @context: an #EMailFormatterContext
 * @part: an #EMailPart
 * @stream: a #CamelStream
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
                            CamelStream *stream,
                            const gchar *as_mime_type,
                            GCancellable *cancellable)
{
	EMailExtensionRegistry *reg;
	GQueue *formatters;
	GList *iter;
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
	g_return_val_if_fail (part, FALSE);
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), FALSE);

	if (!as_mime_type || !*as_mime_type)
		as_mime_type = part->mime_type;

	if (!as_mime_type || !*as_mime_type)
		return FALSE;

	reg = e_mail_formatter_get_extension_registry (formatter);
	formatters = e_mail_extension_registry_get_for_mime_type (
			reg, as_mime_type);
	if (!formatters) {
		formatters = e_mail_extension_registry_get_fallback (
			reg, as_mime_type);
	}

	ok = FALSE;

	d (printf ("(%d) Formatting for part %s of type %s (found %d formatters)\n",
		 _call_i, part->id, as_mime_type,
		 formatters ? g_queue_get_length (formatters) : 0));

	if (formatters) {
		for (iter = formatters->head; iter; iter = iter->next) {

			EMailFormatterExtension *extension;

			extension = iter->data;
			if (!extension)
				continue;

			ok = e_mail_formatter_extension_format (
					extension, formatter, context,
					part, stream, cancellable);

			d (printf ("\t(%d) trying %s...%s\n", _call_i,
					G_OBJECT_TYPE_NAME (extension),
					ok ? "OK" : "failed"));

			if (ok)
				break;
		}
	}

	return ok;
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
                              CamelStream *stream,
                              GCancellable *cancellable)
{
	CamelStream *filter_stream;
	CamelMimeFilter *filter;
	const gchar *charset = NULL;
	CamelMimeFilterWindows *windows = NULL;
	CamelStream *mem_stream = NULL;
	CamelDataWrapper *dw;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	dw = CAMEL_DATA_WRAPPER (part->part);

	if (formatter->priv->charset) {
		charset = formatter->priv->charset;
	} else if (dw->mime_type
		   && (charset = camel_content_type_param (dw->mime_type, "charset"))
		   && g_ascii_strncasecmp (charset, "iso-8859-", 9) == 0) {
		CamelStream *null;

		/* Since a few Windows mailers like to claim they sent
		 * out iso-8859-# encoded text when they really sent
		 * out windows-cp125#, do some simple sanity checking
		 * before we move on... */

		null = camel_stream_null_new ();
		filter_stream = camel_stream_filter_new (null);
		g_object_unref (null);

		windows = (CamelMimeFilterWindows *) camel_mime_filter_windows_new (charset);
		camel_stream_filter_add (
			CAMEL_STREAM_FILTER (filter_stream),
			CAMEL_MIME_FILTER (windows));

		camel_data_wrapper_decode_to_stream_sync (
			dw, (CamelStream *) filter_stream, cancellable, NULL);
		camel_stream_flush ((CamelStream *) filter_stream, cancellable, NULL);
		g_object_unref (filter_stream);

		charset = camel_mime_filter_windows_real_charset (windows);
	} else if (charset == NULL) {
		charset = formatter->priv->default_charset;
	}

	mem_stream = (CamelStream *) camel_stream_mem_new ();
	filter_stream = camel_stream_filter_new (mem_stream);

	if ((filter = camel_mime_filter_charset_new (charset, "UTF-8"))) {
		camel_stream_filter_add (
			CAMEL_STREAM_FILTER (filter_stream),
			CAMEL_MIME_FILTER (filter));
		g_object_unref (filter);
	}

	camel_data_wrapper_decode_to_stream_sync (
			camel_medium_get_content ((CamelMedium *) dw),
			(CamelStream *) filter_stream, cancellable, NULL);
	camel_stream_flush ((CamelStream *) filter_stream, cancellable, NULL);
	g_object_unref (filter_stream);

	g_seekable_seek (G_SEEKABLE (mem_stream), 0, G_SEEK_SET, NULL, NULL);

	camel_stream_write_to_stream (
		mem_stream, (CamelStream *) stream, cancellable, NULL);
	camel_stream_flush ((CamelStream *) mem_stream, cancellable, NULL);

	if (windows) {
		g_object_unref (windows);
	}

	g_object_unref (mem_stream);
}

gchar *
e_mail_formatter_get_html_header (EMailFormatter *formatter)
{
	return g_strdup_printf (
		"<!DOCTYPE HTML>\n<html>\n"
		"<head>\n<meta name=\"generator\" content=\"Evolution Mail Component\" />\n"
		"<title>Evolution Mail Display</title>\n"
		"<link type=\"text/css\" rel=\"stylesheet\" href=\"evo-file://" EVOLUTION_PRIVDATADIR "/theme/webview.css\" />\n"
		"<style type=\"text/css\">\n"
		"  table th { color: #%06x; font-weight: bold; }\n"
		"</style>\n"
		"</head><body bgcolor=\"#%06x\" text=\"#%06x\">",
		e_color_to_value ((GdkColor *)
			e_mail_formatter_get_color (
				formatter, E_MAIL_FORMATTER_COLOR_HEADER)),
		e_color_to_value ((GdkColor *)
			e_mail_formatter_get_color (
				formatter, E_MAIL_FORMATTER_COLOR_BODY)),
		e_color_to_value ((GdkColor *)
			e_mail_formatter_get_color (
				formatter, E_MAIL_FORMATTER_COLOR_TEXT)));
}

EMailExtensionRegistry *
e_mail_formatter_get_extension_registry (EMailFormatter *formatter)
{
	EMailFormatterClass * formatter_class;

	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);

	formatter_class = E_MAIL_FORMATTER_GET_CLASS (formatter);
	return E_MAIL_EXTENSION_REGISTRY (formatter_class->extension_registry);
}

guint32
e_mail_formatter_get_text_format_flags (EMailFormatter *formatter)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), 0);

	return E_MAIL_FORMATTER_GET_CLASS (formatter)->text_html_flags;
}

const GdkColor *
e_mail_formatter_get_color (EMailFormatter *formatter,
                            EMailFormatterColorType type)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);
	g_return_val_if_fail (type < E_MAIL_FORMATTER_NUM_COLOR_TYPES, NULL);

	return &E_MAIL_FORMATTER_GET_CLASS (formatter)->colors[type];
}

void
e_mail_formatter_set_color (EMailFormatter *formatter,
                            EMailFormatterColorType type,
                            const GdkColor *color)
{
	GdkColor *format_color;
	const gchar *property_name;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (type < E_MAIL_FORMATTER_NUM_COLOR_TYPES);
	g_return_if_fail (color != NULL);

	format_color = &E_MAIL_FORMATTER_GET_CLASS (formatter)->colors[type];

	if (gdk_color_equal (color, format_color))
		return;

	format_color->red   = color->red;
	format_color->green = color->green;
	format_color->blue  = color->blue;

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
e_mail_formatter_set_style (EMailFormatter *formatter,
                            GtkStyle *style,
                            GtkStateType state)
{
	EMailFormatterClass *formatter_class;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (GTK_IS_STYLE (style));

	formatter_class = E_MAIL_FORMATTER_GET_CLASS (formatter);
	g_return_if_fail (formatter_class->set_style != NULL);

	formatter_class->set_style (formatter, style, state);
}

EMailImageLoadingPolicy
e_mail_formatter_get_image_loading_policy (EMailFormatter *formatter)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), 0);

	return formatter->priv->image_loading_policy;
}

void
e_mail_formatter_set_image_loading_policy (EMailFormatter *formatter,
                                           EMailImageLoadingPolicy policy)
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
	guint32 flags;

	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), FALSE);

	flags = E_MAIL_FORMATTER_GET_CLASS (formatter)->text_html_flags;

	return ((flags & CAMEL_MIME_FILTER_TOHTML_MARK_CITATION) != 0);
}

void
e_mail_formatter_set_mark_citations (EMailFormatter *formatter,
                                     gboolean mark_citations)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	if (((E_MAIL_FORMATTER_GET_CLASS (formatter)->text_html_flags
	    & CAMEL_MIME_FILTER_TOHTML_MARK_CITATION) ? 1 : 0) == (mark_citations ? 1 : 0))
		return;

	if (mark_citations)
		E_MAIL_FORMATTER_GET_CLASS (formatter)->text_html_flags |=
			CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;
	else
		E_MAIL_FORMATTER_GET_CLASS (formatter)->text_html_flags &=
			~CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;

	g_object_notify (G_OBJECT (formatter), "mark-citations");
}

gboolean
e_mail_formatter_get_only_local_photos (EMailFormatter *formatter)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), FALSE);

	return formatter->priv->only_local_photos;
}

void
e_mail_formatter_set_only_local_photos (EMailFormatter *formatter,
                                        gboolean only_local_photos)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	if ((formatter->priv->only_local_photos ? 1 : 0) == (only_local_photos ? 1 : 0))
		return;

	formatter->priv->only_local_photos = only_local_photos;

	g_object_notify (G_OBJECT (formatter), "only-local-photos");
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

	if ((formatter->priv->show_sender_photo ? 1 : 0) == (show_sender_photo ? 1 : 0))
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

	if ((formatter->priv->show_real_date ? 1 : 0) == (show_real_date ? 1 : 0))
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

	if ((formatter->priv->animate_images ? 1 : 0) == (animate_images ? 1 : 0))
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

void
e_mail_formatter_set_charset (EMailFormatter *formatter,
                              const gchar *charset)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	if (g_strcmp0 (formatter->priv->charset, charset) == 0)
		return;

	g_free (formatter->priv->charset);

	if (!charset) {
		formatter->priv->charset = NULL;
	} else {
		formatter->priv->charset = g_strdup (charset);
	}

	g_object_notify (G_OBJECT (formatter), "charset");
}

const gchar *
e_mail_formatter_get_default_charset (EMailFormatter *formatter)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);

	return formatter->priv->default_charset;
}

void
e_mail_formatter_set_default_charset (EMailFormatter *formatter,
                                      const gchar *default_charset)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (default_charset && *default_charset);

	if (g_strcmp0 (formatter->priv->default_charset, default_charset) == 0)
		return;

	g_free (formatter->priv->default_charset);
	formatter->priv->default_charset = g_strdup (default_charset);

	g_object_notify (G_OBJECT (formatter), "default-charset");
}

/* note: also copied in em-mailer-prefs.c */
static const struct {
	const gchar *name;
	guint32 flags;
} default_headers[] = {
	{ N_("From"), E_MAIL_FORMATTER_HEADER_FLAG_BOLD },
	{ N_("Reply-To"), E_MAIL_FORMATTER_HEADER_FLAG_BOLD },
	{ N_("To"), E_MAIL_FORMATTER_HEADER_FLAG_BOLD },
	{ N_("Cc"), E_MAIL_FORMATTER_HEADER_FLAG_BOLD },
	{ N_("Bcc"), E_MAIL_FORMATTER_HEADER_FLAG_BOLD },
	{ N_("Subject"), E_MAIL_FORMATTER_HEADER_FLAG_BOLD },
	{ N_("Date"), E_MAIL_FORMATTER_HEADER_FLAG_BOLD },
	{ N_("Newsgroups"), E_MAIL_FORMATTER_HEADER_FLAG_BOLD },
	{ N_("Face"), 0 },
};

/**
 * e_mail_formatter_get_headers:
 * @formatter: an #EMailFormatter
 *
 * Returns list of currently set headers.
 *
 * Return Value: A #GQueue of headers which you should not modify or unref
 */
const GQueue *
e_mail_formatter_get_headers (EMailFormatter *formatter)
{
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);

	return formatter->priv->header_list;
}

/**
 * e_mail_formatter_clear_headers:
 * @formatter: an #EMailFormatter
 *
 * Clear the list of headers to be displayed.  This will force all headers to
 * be shown.
 **/
void
e_mail_formatter_clear_headers (EMailFormatter *formatter)
{
	EMailFormatterHeader *header;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	while ((header = g_queue_pop_head (formatter->priv->header_list)) != NULL) {
		e_mail_formatter_header_free (header);
	}
}

/**
 * e_mail_formatter_set_default_headers:
 * @formatter: an #EMailFormatter
 *
 * Clear the list of headers and sets the default ones, e.g. "To", "From", "Cc"
 * "Subject", etc...
 */
void
e_mail_formatter_set_default_headers (EMailFormatter *formatter)
{
	gint ii;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));

	/* Set the default headers */
	e_mail_formatter_clear_headers (formatter);
	for (ii = 0; ii < G_N_ELEMENTS (default_headers); ii++) {
		e_mail_formatter_add_header (
			formatter, default_headers[ii].name, NULL,
			default_headers[ii].flags);
	}
}

/**
 * e_mail_formatter_add_header:
 * @formatter:
 * @name: The name of the header, as it will appear during output.
 * @value: Value of the header. Can be %NULL.
 * @flags: EM_FORMAT_HEAD_* defines to control display attributes.
 *
 * Add a specific header to show.  If any headers are set, they will
 * be displayed in the order set by this function.  Certain known
 * headers included in this list will be shown using special
 * formatting routines.
 **/
void
e_mail_formatter_add_header (EMailFormatter *formatter,
                             const gchar *name,
                             const gchar *value,
                             guint32 flags)
{
	EMailFormatterHeader *h;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (name && *name);

	h = e_mail_formatter_header_new (name, value);
	h->flags = flags;
	g_queue_push_tail (formatter->priv->header_list, h);

	g_signal_emit (formatter, signals[NEED_REDRAW], 0, NULL);
}

void
e_mail_formatter_add_header_struct (EMailFormatter *formatter,
                                    const EMailFormatterHeader *header)
{
	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (header && header->name);

	e_mail_formatter_add_header (formatter, header->name, header->value, header->flags);
}

void e_mail_formatter_remove_header (EMailFormatter *formatter,
				     const gchar *name,
				     const gchar *value)
{
	GList *iter = NULL;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (name && *name);

	iter = g_queue_peek_head_link (formatter->priv->header_list);
	while (iter) {
		EMailFormatterHeader *header = iter->data;

		if (!header->value || !*header->value) {
			GList *next = iter->next;
			if (g_strcmp0 (name, header->name) == 0)
				g_queue_delete_link (formatter->priv->header_list, iter);

			iter = next;
			continue;
		}

		if (value && *value) {
			if ((g_strcmp0 (name, header->name) == 0) &&
			    (g_strcmp0 (value, header->value) == 0))
				break;
		} else {
			if (g_strcmp0 (name, header->name) == 0)
				break;
		}

		iter = iter->next;
	}

	if (iter) {
		e_mail_formatter_header_free (iter->data);
		g_queue_delete_link (formatter->priv->header_list, iter);
	}
}

void
e_mail_formatter_remove_header_struct (EMailFormatter *formatter,
                                       const EMailFormatterHeader *header)
{
	g_return_if_fail (header != NULL);

	e_mail_formatter_remove_header (formatter, header->name, header->value);
}

EMailFormatterHeader *
e_mail_formatter_header_new (const gchar *name,
                             const gchar *value)
{
	EMailFormatterHeader *header;

	g_return_val_if_fail (name && *name, NULL);

	header = g_new0 (EMailFormatterHeader, 1);
	header->name = g_strdup (name);
	if (value && *value)
		header->value = g_strdup (value);

	return header;
}

void
e_mail_formatter_header_free (EMailFormatterHeader *header)
{
	g_return_if_fail (header);

	if (header->name) {
		g_free (header->name);
		header->name = NULL;
	}

	if (header->value) {
		g_free (header->value);
		header->value = NULL;
	}

	g_free (header);
}
