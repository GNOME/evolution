/*
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
 *
 * Authors:
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-mail-parser-itip.h"

#include <shell/e-shell.h>

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>

#include "e-mail-part-itip.h"
#include "itip-view.h"

#define d(x)

typedef EMailParserExtension EMailParserItip;
typedef EMailParserExtensionClass EMailParserItipClass;

typedef EExtension EMailParserItipLoader;
typedef EExtensionClass EMailParserItipLoaderClass;

GType e_mail_parser_itip_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailParserItip,
	e_mail_parser_itip,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"text/calendar",
	"application/ics",
	NULL
};

static void
empe_itip_wrap_attachment (EMailParser *parser,
			   GString *part_id,
			   ICalProperty *prop,
			   const gchar *content,
			   GQueue *queue)
{
	CamelMimePart *opart;
	CamelDataWrapper *dw;
	ICalParameter *param;
	const gchar *mime_type = NULL, *tmp;

	opart = camel_mime_part_new ();

	param = i_cal_property_get_first_parameter (prop, I_CAL_FILENAME_PARAMETER);

	if (param) {
		tmp = i_cal_parameter_get_filename (param);

		if (tmp && *tmp)
			camel_mime_part_set_filename (opart, tmp);

		g_object_unref (param);
	}

	param = i_cal_property_get_first_parameter (prop, I_CAL_FMTTYPE_PARAMETER);

	if (param)
		mime_type = i_cal_parameter_get_fmttype (param);

	if (!mime_type || !*mime_type)
		mime_type = "application/octet-stream";

	camel_mime_part_set_content (opart, content, strlen (content), mime_type);
	camel_mime_part_set_encoding (opart, CAMEL_TRANSFER_ENCODING_BASE64);

	dw = camel_medium_get_content (CAMEL_MEDIUM (opart));
	camel_data_wrapper_set_encoding (dw, CAMEL_TRANSFER_ENCODING_BASE64);

	e_mail_parser_wrap_as_attachment (parser, opart, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, queue);

	g_clear_object (&param);
	g_object_unref (opart);
}

static void
empe_itip_extract_attachments (EMailParser *parser,
			       const gchar *vcalendar_str,
			       GString *part_id,
			       GQueue *queue)
{
	ICalComponent *vcalendar, *ical_comp;
	ICalCompIter *iter;

	if (!vcalendar_str)
		return;

	vcalendar = i_cal_parser_parse_string (vcalendar_str);

	if (!vcalendar)
		return;

	iter = i_cal_component_begin_component (vcalendar, I_CAL_ANY_COMPONENT);
	ical_comp = i_cal_comp_iter_deref (iter);
	if (ical_comp) {
		ICalComponentKind kind;

		kind = i_cal_component_isa (ical_comp);
		if (kind != I_CAL_VEVENT_COMPONENT &&
		    kind != I_CAL_VTODO_COMPONENT &&
		    kind != I_CAL_VFREEBUSY_COMPONENT &&
		    kind != I_CAL_VJOURNAL_COMPONENT) {
			do {
				g_clear_object (&ical_comp);
				ical_comp = i_cal_comp_iter_next (iter);
				if (!ical_comp)
					break;
				kind = i_cal_component_isa (ical_comp);
			} while (ical_comp &&
				 kind != I_CAL_VEVENT_COMPONENT &&
				 kind != I_CAL_VTODO_COMPONENT &&
				 kind != I_CAL_VFREEBUSY_COMPONENT &&
				 kind != I_CAL_VJOURNAL_COMPONENT);
		}
	}

	g_clear_object (&iter);

	if (ical_comp) {
		ICalProperty *prop;
		gint len, index = 0;

		len = part_id->len;

		for (prop = i_cal_component_get_first_property (ical_comp, I_CAL_ATTACH_PROPERTY);
		     prop;
		     g_object_unref (prop), prop = i_cal_component_get_next_property (ical_comp, I_CAL_ATTACH_PROPERTY)) {
			ICalAttach *attach;

			attach = i_cal_property_get_attach (prop);

			if (attach && !i_cal_attach_get_is_url (attach)) {
				const gchar *content;

				content = (const gchar *) i_cal_attach_get_data (attach);

				if (content) {
					g_string_append_printf (part_id, ".attachment.%d", index);

					empe_itip_wrap_attachment (parser, part_id, prop, content, queue);

					g_string_truncate (part_id, len);
					index++;
				}
			}

			g_clear_object (&attach);
		}
	}

	g_clear_object (&ical_comp);
	g_clear_object (&vcalendar);
}

static gboolean
empe_itip_parse (EMailParserExtension *extension,
                 EMailParser *parser,
                 CamelMimePart *part,
                 GString *part_id,
                 GCancellable *cancellable,
                 GQueue *out_mail_parts)
{
	EMailPartItip *itip_part;
	const CamelContentDisposition *disposition;
	GQueue work_queue = G_QUEUE_INIT;
	gint len;

	len = part_id->len;
	g_string_append_printf (part_id, ".itip");

	itip_part = e_mail_part_itip_new (part, part_id->str);
	itip_part->itip_mime_part = g_object_ref (part);
	itip_part->vcalendar = itip_view_util_extract_part_content (part, FALSE);

	g_queue_push_tail (&work_queue, itip_part);

	disposition = camel_mime_part_get_content_disposition (part);
	if (disposition &&
	    (g_strcmp0 (disposition->disposition, "attachment") == 0)) {
		e_mail_parser_wrap_as_attachment (parser, part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);
	}

	e_queue_transfer (&work_queue, out_mail_parts);

	empe_itip_extract_attachments (parser, itip_part->vcalendar, part_id, &work_queue);
	e_queue_transfer (&work_queue, out_mail_parts);

	g_string_truncate (part_id, len);

	return TRUE;
}

static void
e_mail_parser_itip_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->flags = E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION;
	class->parse = empe_itip_parse;
}

static void
e_mail_parser_itip_class_finalize (EMailParserExtensionClass *class)
{
}

static void
e_mail_parser_itip_init (EMailParserExtension *class)
{
}

void
e_mail_parser_itip_type_register (GTypeModule *type_module)
{
	e_mail_parser_itip_register_type (type_module);
}
