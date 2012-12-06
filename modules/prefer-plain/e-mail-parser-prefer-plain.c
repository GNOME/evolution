/*
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
#include <glib/gi18n.h>

#include "e-mail-parser-prefer-plain.h"

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>
#include <em-format/e-mail-part-utils.h>

#include <libebackend/libebackend.h>

#define d(x)

typedef struct _EMailParserPreferPlain EMailParserPreferPlain;
typedef struct _EMailParserPreferPlainClass EMailParserPreferPlainClass;

struct _EMailParserPreferPlain {
	EExtension parent;

	GSettings *settings;
	gint mode;
	gboolean show_suppressed;
};

struct _EMailParserPreferPlainClass {
	EExtensionClass parent_class;
};

GType e_mail_parser_prefer_plain_get_type (void);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);
static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);

enum {
	PREFER_HTML,
	PREFER_PLAIN,
	ONLY_PLAIN
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailParserPreferPlain,
	e_mail_parser_prefer_plain,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar *parser_mime_types[] = {
	"multipart/alternative",
	"text/html",
	NULL
};

static struct {
	const gchar *key;
	const gchar *label;
	const gchar *description;
} epp_options[] = {
	{ "normal",
	  N_("Show HTML if present"),
	  N_("Let Evolution choose the best part to show.") },

	{ "prefer_plain",
	  N_("Show plain text if present"),
	  N_("Show plain text part, if present, otherwise "
	     "let Evolution choose the best part to show.") },

	{ "only_plain",
	  N_("Only ever show plain text"),
	  N_("Always show plain text part and make attachments "
	     "from other parts, if requested.") },
};

enum {
	PROP_0,
	PROP_MODE,
	PROP_SHOW_SUPPRESSED
};

static void
make_part_attachment (EMailParser *parser,
                      CamelMimePart *part,
                      GString *part_id,
                      gboolean force_html,
                      GCancellable *cancellable,
                      GQueue *out_mail_parts)
{
	if (camel_content_type_is (camel_mime_part_get_content_type (part), "text", "html")) {
		GQueue work_queue = G_QUEUE_INIT;
		EMailPart *mail_part;
		gint len;

		/* always show HTML as attachments and not inline */
		camel_mime_part_set_disposition (part, "attachment");

		if (!camel_mime_part_get_filename (part)) {
			gchar *str = g_strdup_printf ("%s.html", _("attachment"));
			camel_mime_part_set_filename (part, str);
			g_free (str);
		}

		len = part_id->len;
		g_string_append (part_id, ".text_html");
		mail_part = e_mail_part_new (part, part_id->str);
		mail_part->mime_type = g_strdup ("text/html");
		g_string_truncate (part_id, len);

		g_queue_push_tail (&work_queue, mail_part);

		e_mail_parser_wrap_as_attachment (
			parser, part, part_id, &work_queue);

		e_queue_transfer (&work_queue, out_mail_parts);

	} else if (force_html && CAMEL_IS_MIME_MESSAGE (part)) {
		/* message was asked to be formatted as text/html;
		 * might be for cases where message itself is a text/html part */
		CamelMimePart *new_part;
		CamelDataWrapper *content;

		content = camel_medium_get_content (CAMEL_MEDIUM (part));
		g_return_if_fail (content != NULL);

		new_part = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (new_part), content);

		e_mail_parser_parse_part (
			parser, new_part, part_id,
			cancellable, out_mail_parts);

		g_object_unref (new_part);
	} else {
		e_mail_parser_parse_part (
			parser, part, part_id, cancellable, out_mail_parts);
	}
}

static void
hide_parts (GQueue *work_queue)
{
	GList *head, *link;

	head = g_queue_peek_head_link (work_queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *mail_part = link->data;

		mail_part->is_hidden = TRUE;
	}
}

static gboolean
empe_prefer_plain_parse (EMailParserExtension *extension,
                         EMailParser *parser,
                         CamelMimePart *part,
                         GString *part_id,
                         GCancellable *cancellable,
                         GQueue *out_mail_parts)
{
	EMailParserPreferPlain *emp_pp;
	CamelMultipart *mp;
	gint i, nparts, partidlen;
	CamelContentType *ct;
	gboolean has_calendar = FALSE;
	GQueue plain_text_parts = G_QUEUE_INIT;
	GQueue work_queue = G_QUEUE_INIT;

	emp_pp = (EMailParserPreferPlain *) extension;

	ct = camel_mime_part_get_content_type (part);

	/* We can actually parse HTML, but just to discard it when
	 * "Only ever show plain text" mode is set */
	if (camel_content_type_is (ct, "text", "html")) {

		/* Prevent recursion, fall back to next (real text/html) parser */
		if (strstr (part_id->str, ".alternative-prefer-plain.") != NULL)
			return FALSE;

		/* Not enforcing text/plain, so use real parser */
		if (emp_pp->mode != ONLY_PLAIN)
			return FALSE;

		/* Enforcing text/plain, but got only HTML part, so add it
		 * as attachment, to not show empty message preview, which
		 * is confusing. */
		make_part_attachment (
			parser, part, part_id, FALSE,
			cancellable, out_mail_parts);

		return TRUE;
	}

	partidlen = part_id->len;

	mp = (CamelMultipart *) camel_medium_get_content (CAMEL_MEDIUM (part));

	if (!CAMEL_IS_MULTIPART (mp))
		return e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.source",
			cancellable, out_mail_parts);

	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		CamelMimePart *sp;

		sp = camel_multipart_get_part (mp, i);
		ct = camel_mime_part_get_content_type (sp);

		g_string_truncate (part_id, partidlen);
		g_string_append_printf (part_id, ".alternative-prefer-plain.%d", i);

		if (camel_content_type_is (ct, "text", "html")) {
			if (emp_pp->mode != PREFER_HTML) {
				if (emp_pp->show_suppressed) {
					make_part_attachment (
						parser, sp, part_id, FALSE,
						cancellable, &work_queue);
				}
			} else {
				e_mail_parser_parse_part (
					parser, sp, part_id,
					cancellable, &work_queue);
			}

		} else if (camel_content_type_is (ct, "text", "plain")) {
			e_mail_parser_parse_part (
				parser, sp, part_id,
				cancellable, &plain_text_parts);

		/* Always show calendar part! */
		} else if (camel_content_type_is (ct, "text", "calendar") ||
		    camel_content_type_is (ct, "text", "x-calendar")) {

			/* Hide everything else, displaying
			 * native calendar part only. */
			hide_parts (&work_queue);

			e_mail_parser_parse_part (
				parser, sp, part_id, cancellable, &work_queue);

			has_calendar = TRUE;

		/* Multiparts can represent a text/html with inline images or so */
		} else if (camel_content_type_is (ct, "multipart", "*")) {
			GQueue inner_queue = G_QUEUE_INIT;
			GList *head, *link;
			gboolean has_html = FALSE;

			e_mail_parser_parse_part (
				parser, sp, part_id, cancellable, &inner_queue);

			head = g_queue_peek_head_link (&inner_queue);

			/* Check whether the multipart contains a text/html part */
			for (link = head; link != NULL; link = g_list_next (link)) {
				EMailPart *mail_part = link->data;

				if (strstr (mail_part->id, ".text_html") != NULL) {
					has_html = TRUE;
					break;
				}
			}

			if (has_html && (emp_pp->mode != PREFER_HTML)) {
				if (emp_pp->show_suppressed) {
					e_mail_parser_wrap_as_attachment (
						parser, sp, part_id,
						&inner_queue);
				} else {
					hide_parts (&inner_queue);
				}
			}

			e_queue_transfer (&inner_queue, &work_queue);

		/* Parse everything else as an attachment */
		} else {
			GQueue inner_queue = G_QUEUE_INIT;

			e_mail_parser_parse_part (
				parser, sp, part_id,
				cancellable, &inner_queue);
			e_mail_parser_wrap_as_attachment (
				parser, sp, part_id, &inner_queue);

			e_queue_transfer (&inner_queue, &work_queue);
		}
	}

	/* Don't hide the plain text if there's nothing else to display */
	if (has_calendar || (nparts > 1 && emp_pp->mode == PREFER_HTML))
		hide_parts (&plain_text_parts);

	/* plain_text parts should be always first */
	e_queue_transfer (&plain_text_parts, out_mail_parts);
	e_queue_transfer (&work_queue, out_mail_parts);

	g_string_truncate (part_id, partidlen);

	return TRUE;
}

void
e_mail_parser_prefer_plain_type_register (GTypeModule *type_module)
{
	e_mail_parser_prefer_plain_register_type (type_module);
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = parser_mime_types;
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_prefer_plain_parse;
}

static void
e_mail_parser_prefer_plain_constructed (GObject *object)
{
	EExtensible *extensible;
	EMailExtensionRegistry *reg;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	reg = E_MAIL_EXTENSION_REGISTRY (extensible);

	e_mail_extension_registry_add_extension (reg, E_MAIL_EXTENSION (object));
}

static void
e_mail_parser_prefer_plain_get_property (GObject *object,
                                         guint property_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
	EMailParserPreferPlain *parser;

	parser = (EMailParserPreferPlain *) object;

	switch (property_id) {
		case PROP_MODE:
			g_value_set_int (value, parser->mode);
			return;
		case PROP_SHOW_SUPPRESSED:
			g_value_set_boolean (value, parser->show_suppressed);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_parser_prefer_plain_set_property (GObject *object,
                                         guint property_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
	EMailParserPreferPlain *parser;

	parser = (EMailParserPreferPlain *) object;

	switch (property_id) {
		case PROP_MODE:
			parser->mode = g_value_get_int (value);
			return;
		case PROP_SHOW_SUPPRESSED:
			parser->show_suppressed = g_value_get_boolean (value);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_parser_prefer_plain_dispose (GObject *object)
{
	EMailParserPreferPlain *parser;

	parser = (EMailParserPreferPlain *) object;

	g_clear_object (&parser->settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_parser_prefer_plain_parent_class)->
		dispose (object);
}

static void
e_mail_parser_prefer_plain_class_init (EMailParserPreferPlainClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_mail_parser_prefer_plain_constructed;
	object_class->get_property = e_mail_parser_prefer_plain_get_property;
	object_class->set_property = e_mail_parser_prefer_plain_set_property;
	object_class->dispose = e_mail_parser_prefer_plain_dispose;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY;

	g_object_class_install_property (
		object_class,
		PROP_MODE,
		g_param_spec_int (
			"mode",
			"Mode",
			NULL,
			PREFER_HTML,
			ONLY_PLAIN,
			PREFER_HTML,
			G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_SUPPRESSED,
		g_param_spec_boolean (
			"show-suppressed",
			"Show Suppressed",
			NULL,
			FALSE,
			G_PARAM_READABLE | G_PARAM_WRITABLE));
}

void
e_mail_parser_prefer_plain_class_finalize (EMailParserPreferPlainClass *class)
{
}

static gboolean
parser_mode_get_mapping (GValue *value,
                         GVariant *variant,
                         gpointer user_data)
{
	gint i;

	const gchar *key = g_variant_get_string (variant, NULL);
	if (key) {
		for (i = 0; i < G_N_ELEMENTS (epp_options); i++) {
			if (!strcmp (epp_options[i].key, key)) {
				g_value_set_int (value, i);
				return TRUE;
			}
		}
	} else {
		g_value_set_int (value, 0);
	}

	return TRUE;
}

static GVariant *
parser_mode_set_mapping (const GValue *value,
                         const GVariantType *expected_type,
                         gpointer user_data)
{
	return g_variant_new_string (epp_options[g_value_get_int (value)].key);
}

static void
e_mail_parser_prefer_plain_init (EMailParserPreferPlain *parser)
{
	gchar *key;
	gint i;

	parser->settings = g_settings_new ("org.gnome.evolution.plugin.prefer-plain");
	g_settings_bind_with_mapping (
		parser->settings, "mode",
		parser, "mode", G_SETTINGS_BIND_DEFAULT,
		parser_mode_get_mapping,
		parser_mode_set_mapping,
		NULL, NULL);
	g_settings_bind (
		parser->settings, "show-suppressed",
		parser, "show-suppressed", G_SETTINGS_BIND_DEFAULT);

	/* Initialize the settings */
	key = g_settings_get_string (parser->settings, "mode");
	if (key) {
		for (i = 0; i < G_N_ELEMENTS (epp_options); i++) {
			if (!strcmp (epp_options[i].key, key)) {
				parser->mode = i;
				break;
			}
		}
		g_free (key);
	} else {
		parser->mode = 0;
	}

	parser->show_suppressed = g_settings_get_boolean (parser->settings, "show-suppressed");
}
