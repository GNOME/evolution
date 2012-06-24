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

typedef struct _EMailParserPreferPlain {
	EExtension parent;

	GSettings *settings;
	gint mode;
	gboolean show_suppressed;
} EMailParserPreferPlain;

typedef struct _EMailParserPreferPlainClass {
	EExtensionClass parent_class;
} EMailParserPreferPlainClass;

GType e_mail_parser_prefer_plain_get_type (void);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);
static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);

enum {
	EPP_NORMAL,
	EPP_PREFER,
	EPP_TEXT
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

static const gchar* parser_mime_types[] = { "multipart/alternative",
					    "text/html",
					    NULL };

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

static GSList *
make_part_attachment (EMailParser *parser,
                      CamelMimePart *part,
                      GString *part_id,
                      gboolean force_html,
                      GCancellable *cancellable)
{
	GSList *parts;

	if (camel_content_type_is (camel_mime_part_get_content_type (part), "text", "html")) {
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

		parts = e_mail_parser_wrap_as_attachment (
				parser, part, g_slist_append (NULL, mail_part),
				part_id, cancellable);

	} else if (force_html && CAMEL_IS_MIME_MESSAGE (part)) {
		/* message was asked to be formatted as text/html;
		 * might be for cases where message itself is a text/html part */
		CamelMimePart *new_part;
		CamelDataWrapper *content;

		content = camel_medium_get_content (CAMEL_MEDIUM (part));
		g_return_val_if_fail (content != NULL, NULL);

		new_part = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (new_part), content);

		parts = e_mail_parser_parse_part (
				parser, new_part, part_id, cancellable);

		g_object_unref (new_part);
	} else {
		parts = e_mail_parser_parse_part (
				parser, part, part_id, cancellable);
	}

	return parts;
}

static GSList *
export_as_attachments (CamelMultipart *mp,
                       EMailParser *parser,
                       CamelMimePart *except,
                       GString *part_id,
                       GCancellable *cancellable)
{
	gint i, nparts;
	CamelMimePart *part;
	gint len;
	GSList *parts;

	if (!mp || !CAMEL_IS_MULTIPART (mp))
		return NULL;

	len = part_id->len;
	nparts = camel_multipart_get_number (mp);
	parts = NULL;
	for (i = 0; i < nparts; i++) {
		part = camel_multipart_get_part (mp, i);

		if (part != except) {
			CamelMultipart *multipart = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);

			g_string_append_printf (part_id, ".aleternative-prefer-plain.%d", i);
			if (CAMEL_IS_MULTIPART (multipart)) {
				parts = g_slist_concat (parts,
						export_as_attachments (
							multipart, parser,
							except, part_id,
							cancellable));
			} else {
				parts = g_slist_concat (parts,
						make_part_attachment (
							parser, part, part_id,
							FALSE, cancellable));
			}
			g_string_truncate (part_id, len);
		}
	}

	return parts;
}

static GSList *
empe_prefer_plain_parse (EMailParserExtension *extension,
                         EMailParser *parser,
                         CamelMimePart *part,
                         GString *part_id,
                         GCancellable *cancellable)
{
	EMailParserPreferPlain *emp_pp;
	CamelMultipart *mp;
	CamelMimePart *display_part = NULL, *calendar_part = NULL;
	gint i, nparts, partidlen, displayid = 0, calendarid = 0;
	GSList *parts;

	emp_pp = (EMailParserPreferPlain *) extension;

	/* We 'can' parse HTML as well!
	 * The reason simply is to convert the HTML part to attachment in some
	 * cases, otherwise we will return NULL and fallback to "normal" parser. */
	if (camel_content_type_is (camel_mime_part_get_content_type (part), "text", "html")) {
		GQueue *extensions;
		EMailExtensionRegistry *reg;

		reg = e_mail_parser_get_extension_registry (parser);
		extensions = e_mail_extension_registry_get_for_mime_type (
					reg, "text/html");

		if (emp_pp->mode != EPP_TEXT
			|| strstr (part_id->str, ".alternative-prefer-plain.") != NULL
			|| e_mail_part_is_inline (part, extensions)) {

			return NULL;

		} else if (emp_pp->show_suppressed) {
			return make_part_attachment (
				parser, part, part_id,
				TRUE, cancellable);
		}

		/* Return an empty item. We MUST return something, otherwise
		 * the parser would think we have failed to parse the part
		 * and would let a fallback extension to parse it and we don't
		 * want that... */
		/* FIXME: In theory we could parse it anyway and just set
		 * is_hidden to TRUE....? */
		return g_slist_alloc ();
	}

	mp  = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);
	partidlen = part_id->len;

	parts = NULL;
	if (emp_pp->mode == EPP_NORMAL) {
		gboolean have_plain = FALSE;

		/* Try to find text/html part even when not as last and force
		 * to show it.  Old handler will show the last part of
		 * multipart/alternate, but if we can offer HTML, then
		 * offer it, regardless of position in multipart.  But do
		 * this when have only text/plain and text/html parts,
		 * not more. */
		nparts = camel_multipart_get_number (mp);
		for (i = 0; i < nparts; i++) {
			CamelContentType *content_type;

			part = camel_multipart_get_part (mp, i);

			if (!part)
				continue;

			content_type = camel_mime_part_get_content_type (part);

			if (camel_content_type_is (content_type, "text", "html")) {
				displayid = i;
				display_part = part;

				if (have_plain)
					break;
			} else if (camel_content_type_is (content_type, "text", "plain")) {
				have_plain = TRUE;

				if (display_part)
					break;
			}
		}

		if (display_part && have_plain && nparts == 2) {
			g_string_append_printf (part_id, ".alternative-prefer-plain.%d", displayid);
			/* FIXME Not passing a GCancellable here. */
			parts = e_mail_parser_parse_part_as (
				parser, display_part, part_id,
				"text/html", cancellable);

			g_string_truncate (part_id, partidlen);
		} else {
			/* Parser will automatically fallback to next extension */
			return NULL;

		}

		return parts;

	} else if (!CAMEL_IS_MULTIPART (mp)) {
		return e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.source", cancellable);
	}

	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		CamelContentType *ct;

		part = camel_multipart_get_part (mp, i);

		if (!part)
			continue;

		ct = camel_mime_part_get_content_type (part);
		if (!display_part && camel_content_type_is (ct, "text", "plain")) {
			displayid = i;
			display_part = part;
		} else if (!calendar_part && (camel_content_type_is (ct, "text", "calendar") || camel_content_type_is (ct, "text", "x-calendar"))) {
			calendarid = i;
			calendar_part = part;
		}
	}

	/* if we found a text part, show it */
	if (display_part) {
		g_string_append_printf(part_id, ".alternative-prefer-plain.%d", displayid);
		parts = g_slist_concat (parts,
				e_mail_parser_parse_part_as (
					parser, display_part, part_id,
				  	"text/plain", cancellable));

		g_string_truncate (part_id, partidlen);
	}

	/* all other parts are attachments */
	if (emp_pp->show_suppressed) {
		parts = g_slist_concat (parts,
				export_as_attachments (
					mp, parser, display_part, part_id,
					cancellable));
	} else if (calendar_part) {
		g_string_append_printf(part_id, ".alternative-prefer-plain.%d", calendarid);
		parts = g_slist_concat (parts,
				make_part_attachment (
					parser, calendar_part, part_id,
					FALSE, NULL));
	}

	g_string_truncate (part_id, partidlen);

	return parts;
}

static const gchar **
empe_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

void
e_mail_parser_prefer_plain_type_register (GTypeModule *type_module)
{
	e_mail_parser_prefer_plain_register_type (type_module);
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_mime_types;
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
e_mail_parser_prefer_plain_finalize (GObject *object)
{
	EMailParserPreferPlain *parser;

	parser = (EMailParserPreferPlain *) object;

	g_clear_object (&parser->settings);

	G_OBJECT_CLASS (e_mail_parser_prefer_plain_parent_class)->finalize (object);
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
	object_class->finalize = e_mail_parser_prefer_plain_finalize;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY;

	g_object_class_install_property (
		object_class,
		PROP_MODE,
		g_param_spec_int (
			"mode",
			"Mode",
			NULL,
			EPP_NORMAL,
			EPP_TEXT,
			EPP_NORMAL,
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
