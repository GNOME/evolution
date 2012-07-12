/*
 * e-mail-parser-audio-inline.c
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "e-mail-parser-audio-inline.h"
#include "e-mail-part-audio-inline.h"

#include <camel/camel.h>

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>

#include <libebackend/libebackend.h>

#define d(x)

typedef struct _EMailParserInlineAudio {
	EExtension parent;
} EMailParserAudioInline;

typedef struct _EMailParserAudioInlineClass {
	EExtensionClass parent_class;
} EMailParserAudioInlineClass;

GType e_mail_parser_audio_inline_get_type (void);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);
static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailParserAudioInline,
	e_mail_parser_audio_inline,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar * parser_mime_types[] = { "audio/ac3", "audio/x-ac3",
					    "audio/basic", "audio/mpeg",
					    "audio/x-mpeg", "audio/mpeg3",
					    "audio/x-mpeg3", "audio/mp3",
					    "audio/x-mp3", "audio/mp4",
					    "audio/flac", "audio/x-flac",
					    "audio/mod", "audio/x-mod",
					    "audio/x-wav", "audio/microsoft-wav",
					    "audio/x-wma", "audio/x-ms-wma",
					    "application/ogg", "application/x-ogg",
					    NULL };

static void
mail_part_audio_inline_free (EMailPart *mail_part)
{
	EMailPartAudioInline *ai_part = (EMailPartAudioInline *) mail_part;

	g_clear_object (&ai_part->play_button);
	g_clear_object (&ai_part->pause_button);
	g_clear_object (&ai_part->stop_button);

	if (ai_part->filename) {
		g_unlink (ai_part->filename);
		g_free (ai_part->filename);
		ai_part->filename = NULL;
	}

	if (ai_part->bus_id) {
		g_source_remove (ai_part->bus_id);
		ai_part->bus_id = 0;
	}

	if (ai_part->playbin) {
		gst_element_set_state (ai_part->playbin, GST_STATE_NULL);
		gst_object_unref (ai_part->playbin);
		ai_part->playbin = NULL;
	}
}

static GSList *
empe_audio_inline_parse (EMailParserExtension *extension,
                         EMailParser *parser,
                         CamelMimePart *part,
                         GString *part_id,
                         GCancellable *cancellable)
{
	EMailPartAudioInline *mail_part;
	gint len;

	len = part_id->len;
	g_string_append (part_id, ".org-gnome-audio-inline-button-panel");

	d (printf ("audio inline formatter: format classid %s\n", part_id->str));

	mail_part = (EMailPartAudioInline *) e_mail_part_subclass_new (
			part, part_id->str, sizeof (EMailPartAudioInline),
			(GFreeFunc) mail_part_audio_inline_free);
	mail_part->parent.mime_type = camel_content_type_simple (
			camel_mime_part_get_content_type (part));
	mail_part->parent.is_attachment = TRUE;
	g_string_truncate (part_id, len);

	return e_mail_parser_wrap_as_attachment (
			parser, part, g_slist_append (NULL, mail_part),
			part_id, cancellable);
}

static guint32
empe_audio_inline_get_flags (EMailParserExtension *extension)
{
	return E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION;
}

static const gchar **
empe_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

void
e_mail_parser_audio_inline_type_register (GTypeModule *type_module)
{
	e_mail_parser_audio_inline_register_type (type_module);
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_mime_types;
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_audio_inline_parse;
	iface->get_flags = empe_audio_inline_get_flags;
}

static void
e_mail_parser_audio_inline_constructed (GObject *object)
{
	EExtensible *extensible;
	EMailExtensionRegistry *reg;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	reg = E_MAIL_EXTENSION_REGISTRY (extensible);

	e_mail_extension_registry_add_extension (reg, E_MAIL_EXTENSION (object));
}

static void
e_mail_parser_audio_inline_class_init (EMailParserAudioInlineClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_mail_parser_audio_inline_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY;
}

static void
e_mail_parser_audio_inline_class_finalize (EMailParserAudioInlineClass *class)
{

}

static void
e_mail_parser_audio_inline_init (EMailParserAudioInline *self)
{
}
