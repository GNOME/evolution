/*
 * text-highlight.c
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

#include "text-highlight.h"

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include <glib/gi18n-lib.h>
#include <X11/Xlib.h>
#include <camel/camel.h>

typedef struct _EMailFormatterTextHighlight EMailFormatterTextHighlight;
typedef struct _EMailFormatterTextHighlightClass EMailFormatterTextHighlightClass;

struct _EMailFormatterTextHighlight {
	EExtension parent;
};

struct _EMailFormatterTextHighlightClass {
	EExtensionClass parent_class;
};

GType e_mail_formatter_text_highlight_get_type (void);
static void e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailFormatterTextHighlight,
	e_mail_formatter_text_highlight,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_formatter_extension_interface_init));

static const gchar *formatter_mime_types[] = { "text/x-diff",
					       "text/x-patch",
					       NULL };

static gchar * get_default_font (void)
{
	gchar *font;
	GSettings *settings;

	settings = g_settings_new ("org.gnome.desktop.interface");

	font = g_settings_get_string (settings, "monospace-font-name");

	return font ? font : g_strdup ("monospace 10");
}

static gboolean
emfe_text_highlight_format (EMailFormatterExtension *extension,
                            EMailFormatter *formatter,
                            EMailFormatterContext *context,
                            EMailPart *part,
                            CamelStream *stream,
                            GCancellable *cancellable)
{
	if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {

		CamelDataWrapper *dw;
		CamelStream *filter_stream;
		CamelMimeFilter *mime_filter;

		dw = camel_medium_get_content (CAMEL_MEDIUM (part->part));
		if (!dw) {
			return FALSE;
		}

		camel_stream_write_string (
			stream, "<pre><div class=\"pre\">", cancellable, NULL);

		filter_stream = camel_stream_filter_new (stream);
		mime_filter = camel_mime_filter_tohtml_new (
				CAMEL_MIME_FILTER_TOHTML_PRE |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES,
				0x7a7a7a);
		camel_stream_filter_add (
			CAMEL_STREAM_FILTER (filter_stream), mime_filter);
		g_object_unref (mime_filter);

		e_mail_formatter_format_text (
			formatter, part, filter_stream, cancellable);

		camel_stream_flush (filter_stream, cancellable, NULL);
		g_object_unref (filter_stream);

		camel_stream_write_string (
			stream, "</div></pre>", cancellable, NULL);

		return TRUE;

	} else if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
		gint stdin, stdout;
		GPid pid;
		CamelStream *read, *write;
		CamelDataWrapper *dw;
		gchar *font_family, *font_size;
		gboolean use_custom_font;
		GSettings *settings;
		PangoFontDescription *fd;
		const gchar *argv[] = { "highlight",
					NULL,	/* don't move these! */
					NULL,
					"--out-format=html",
					"--include-style",
					"--inline-css",
					"--style=bclear",
					"--syntax=diff",
					"--failsafe",
					NULL };

		dw = camel_medium_get_content (CAMEL_MEDIUM (part->part));
		if (!dw) {
			return FALSE;
		}

		fd = NULL;
		settings = g_settings_new ("org.gnome.evolution.mail");
		use_custom_font = g_settings_get_boolean (settings, "use-custom-font");
		if (!use_custom_font) {
			gchar *font;

			font = get_default_font ();
			fd = pango_font_description_from_string (font);
			g_free (font);

			g_object_unref (settings);

		} else {
			gchar *font;

			font = g_settings_get_string (settings, "monospace-font");
			if (!font)
				font = get_default_font ();

			fd = pango_font_description_from_string (font);

			g_free (font);
		}

		font_family = g_strdup_printf ("--font='%s'",
				pango_font_description_get_family (fd));
		font_size = g_strdup_printf ("--font-size=%d",
				pango_font_description_get_size (fd) / PANGO_SCALE);

		argv[1] = font_family;
		argv[2] = font_size;

		if (!g_spawn_async_with_pipes (
			NULL, (gchar **) argv, NULL,
			G_SPAWN_SEARCH_PATH |
			G_SPAWN_DO_NOT_REAP_CHILD,
			NULL, NULL, &pid, &stdin, &stdout, NULL, NULL)) {
			return FALSE;
		}

		write = camel_stream_fs_new_with_fd (stdin);
		read = camel_stream_fs_new_with_fd (stdout);

		camel_data_wrapper_decode_to_stream_sync (
			dw, write, cancellable, NULL);
		g_object_unref (write);

		g_spawn_close_pid (pid);

		g_seekable_seek (G_SEEKABLE (read), 0, G_SEEK_SET, cancellable, NULL);
		camel_stream_write_to_stream (read, stream, cancellable, NULL);
		camel_stream_flush (read, cancellable, NULL);
		g_object_unref (read);

		g_free (font_family);
		g_free (font_size);
		pango_font_description_free (fd);

	} else {
		gchar *uri, *str;

		uri = e_mail_part_build_uri (
			context->folder, context->message_uid,
			"part_id", G_TYPE_STRING, part->id,
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			NULL);

		str = g_strdup_printf (
			"<div class=\"part-container\" style=\"border-color: #%06x; "
			"background-color: #%06x;\">"
			"<div class=\"part-container-inner-margin\">\n"
			"<iframe width=\"100%%\" height=\"10\""
			" name=\"%s\" frameborder=\"0\" src=\"%s\"></iframe>"
			"</div></div>",
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_FRAME)),
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_CONTENT)),
			part->id, uri);

		camel_stream_write_string (stream, str, cancellable, NULL);

		g_free (str);
		g_free (uri);

	}

	return TRUE;
}

static const gchar *
emfe_text_highlight_get_display_name (EMailFormatterExtension *extension)
{
	return _("Patch");
}

static const gchar *
emfe_text_highlight_get_description (EMailFormatterExtension *extension)
{
	return _("Format part as a patch");
}

static const gchar **
emfe_text_highlight_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
emfe_text_highlight_constructed (GObject *object)
{
	EExtensible *extensible;
	EMailExtensionRegistry *reg;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	reg = E_MAIL_EXTENSION_REGISTRY (extensible);

	e_mail_extension_registry_add_extension (reg, E_MAIL_EXTENSION (object));
}

static void
e_mail_formatter_text_highlight_init (EMailFormatterTextHighlight *object)
{
}

static void
e_mail_formatter_text_highlight_class_init (EMailFormatterTextHighlightClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = emfe_text_highlight_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY;
}

static void
e_mail_formatter_text_highlight_class_finalize (EMailFormatterTextHighlightClass *class)
{
}

void
e_mail_formatter_text_highlight_type_register (GTypeModule *type_module)
{
	e_mail_formatter_text_highlight_register_type (type_module);
}

static void
e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfe_text_highlight_format;
	iface->get_display_name = emfe_text_highlight_get_display_name;
	iface->get_description = emfe_text_highlight_get_description;
}

static void
e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfe_text_highlight_mime_types;
}
