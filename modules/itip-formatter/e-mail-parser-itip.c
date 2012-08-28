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
 *
 *
 * Authors:
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-mail-parser-itip.h"

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>

#include <misc/e-attachment.h>

#include "e-mail-part-itip.h"
#include "itip-view.h"
#include <shell/e-shell.h>

#define CONF_KEY_DELETE "delete-processed"

#define d(x)

typedef struct _EMailParserItip {
	EExtension parent;
} EMailParserItip;

typedef struct _EMailParserItipClass {
	EExtensionClass parent_class;
} EMailParserItipClass;

GType e_mail_parser_itip_get_type (void);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);
static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailParserItip,
	e_mail_parser_itip,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar * parser_mime_types[] = { "text/calendar",
					     "application/ics",
					     NULL };

static void
mail_part_itip_free (EMailPart *mail_part)
{
	EMailPartItip *pitip = (EMailPartItip *) mail_part;
	gint i;

	g_cancellable_cancel (pitip->cancellable);
	g_clear_object (&pitip->cancellable);
	g_clear_object (&pitip->registry);

	for (i = 0; i < E_CAL_CLIENT_SOURCE_TYPE_LAST; i++) {
		if (pitip->clients[i]) {
			g_hash_table_destroy (pitip->clients[i]);
			pitip->clients[i] = NULL;
		}
	}

	g_free (pitip->vcalendar);
	pitip->vcalendar = NULL;

	if (pitip->comp) {
		g_object_unref (pitip->comp);
		pitip->comp = NULL;
	}

	if (pitip->top_level) {
		icalcomponent_free (pitip->top_level);
		pitip->top_level = NULL;
	}

	if (pitip->main_comp) {
		icalcomponent_free (pitip->main_comp);
		pitip->main_comp = NULL;
	}
	pitip->ical_comp = NULL;

	g_free (pitip->calendar_uid);
	pitip->calendar_uid = NULL;

	g_free (pitip->from_address);
	pitip->from_address = NULL;
	g_free (pitip->from_name);
	pitip->from_name = NULL;
	g_free (pitip->to_address);
	pitip->to_address = NULL;
	g_free (pitip->to_name);
	pitip->to_name = NULL;
	g_free (pitip->delegator_address);
	pitip->delegator_address = NULL;
	g_free (pitip->delegator_name);
	pitip->delegator_name = NULL;
	g_free (pitip->my_address);
	pitip->my_address = NULL;
	g_free (pitip->uid);
	g_hash_table_destroy (pitip->real_comps);

	g_clear_object (&pitip->view);
}

/******************************************************************************/

static void
bind_itip_view (EMailPart *part,
                WebKitDOMElement *element)
{
	GString *buffer;
	WebKitDOMDocument *document;
	ItipView *view;
	EMailPartItip *pitip;

	if (!WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element)) {

		WebKitDOMNodeList *nodes;
		guint length, i;

		nodes = webkit_dom_element_get_elements_by_tag_name (
				element, "iframe");
		length = webkit_dom_node_list_get_length (nodes);
		for (i = 0; i < length; i++) {

			element = WEBKIT_DOM_ELEMENT (
					webkit_dom_node_list_item (nodes, i));
			break;
		}

	}

	g_return_if_fail (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element));

	buffer = g_string_new ("");
	document = webkit_dom_html_iframe_element_get_content_document (
			WEBKIT_DOM_HTML_IFRAME_ELEMENT (element));
	pitip = E_MAIL_PART_ITIP (part);

	view = itip_view_new (pitip, pitip->registry);
	g_object_set_data_full (
		G_OBJECT (element), "view", view,
		(GDestroyNotify) g_object_unref);

	itip_view_create_dom_bindings (
		view, webkit_dom_document_get_document_element (document));

	itip_view_init_view (view);
	g_string_free (buffer, TRUE);
}

/*******************************************************************************/

static GSList *
empe_itip_parse (EMailParserExtension *extension,
                 EMailParser *parser,
                 CamelMimePart *part,
                 GString *part_id,
                 GCancellable *cancellable)
{
	EShell *shell;
	GSettings *settings;
	EMailPartItip *itip_part;
	CamelDataWrapper *content;
	CamelStream *stream;
	GByteArray *byte_array;
	gint len;
	const CamelContentDisposition *disposition;
	GSList *parts;

	len = part_id->len;
	g_string_append_printf (part_id, ".itip");

	settings = g_settings_new ("org.gnome.evolution.plugin.itip");
	shell = e_shell_get_default ();

	itip_part = (EMailPartItip *) e_mail_part_subclass_new (
					part, part_id->str, sizeof (EMailPartItip),
					(GFreeFunc)	mail_part_itip_free);
	itip_part->parent.mime_type = g_strdup ("text/calendar");
	itip_part->parent.bind_func = bind_itip_view;
	itip_part->parent.force_collapse = TRUE;
	itip_part->delete_message = g_settings_get_boolean (settings, CONF_KEY_DELETE);
	itip_part->has_organizer = FALSE;
	itip_part->no_reply_wanted = FALSE;
	itip_part->part = part;
	itip_part->cancellable = g_cancellable_new ();
	itip_part->real_comps = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	itip_part->registry = g_object_ref (e_shell_get_registry (shell));

	g_object_unref (settings);

	/* This is non-gui thread. Download the part for using in the main thread */
	content = camel_medium_get_content ((CamelMedium *) part);

	byte_array = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (byte_array);
	camel_data_wrapper_decode_to_stream_sync (content, stream, NULL, NULL);

	if (byte_array->len == 0)
		itip_part->vcalendar = NULL;
	else
		itip_part->vcalendar = g_strndup (
			(gchar *) byte_array->data, byte_array->len);

	g_object_unref (stream);

	parts = g_slist_append (NULL, itip_part);

	disposition = camel_mime_part_get_content_disposition (part);
	if (disposition &&
	    (g_strcmp0 (disposition->disposition, "attachment") == 0)) {
		parts = e_mail_parser_wrap_as_attachment (
			parser, part, parts, part_id, cancellable);
	}

	g_string_truncate (part_id, len);

	return parts;
}

static guint32
empe_itip_get_flags (EMailParserExtension *extension)
{
	return E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION;
}

static const gchar **
empe_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

void
e_mail_parser_itip_type_register (GTypeModule *type_module)
{
	e_mail_parser_itip_register_type (type_module);
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_mime_types;
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_itip_parse;
	iface->get_flags = empe_itip_get_flags;
}

static void
e_mail_parser_itip_constructed (GObject *object)
{
	EExtensible *extensible;
	EMailExtensionRegistry *reg;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	reg = E_MAIL_EXTENSION_REGISTRY (extensible);

	e_mail_extension_registry_add_extension (reg, E_MAIL_EXTENSION (object));
}

static void
e_mail_parser_itip_finalize (GObject *object)
{
	EExtensible *extensible;
	EMailExtensionRegistry *reg;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	reg = E_MAIL_EXTENSION_REGISTRY (extensible);

	e_mail_extension_registry_remove_extension (reg, E_MAIL_EXTENSION (object));
}

static void
e_mail_parser_itip_class_init (EMailParserItipClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_mail_parser_itip_constructed;
	object_class->finalize = e_mail_parser_itip_finalize;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY;
}

static void
e_mail_parser_itip_class_finalize (EMailParserItipClass *class)
{

}

static void
e_mail_parser_itip_init (EMailParserItip *self)
{
}
