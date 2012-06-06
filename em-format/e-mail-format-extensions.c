/*
 * e-mail-format-extensions.c
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

#include "e-mail-format-extensions.h"

#include "em-format/e-mail-parser-extension.h"
#include "em-format/e-mail-formatter-extension.h"

typedef GType (*TypeFunc) (void);

TypeFunc parser_funcs[] = {
	e_mail_parser_application_mbox_get_type,
	e_mail_parser_attachment_bar_get_type,
	e_mail_parser_headers_get_type,
	e_mail_parser_message_get_type,
	e_mail_parser_secure_button_get_type,
	e_mail_parser_source_get_type,
	e_mail_parser_image_get_type,
	e_mail_parser_inline_pgp_encrypted_get_type,
	e_mail_parser_inline_pgp_signed_get_type,
	e_mail_parser_message_delivery_status_get_type,
	e_mail_parser_message_external_get_type,
	e_mail_parser_message_rfc822_get_type,
	e_mail_parser_multipart_alternative_get_type,
	e_mail_parser_multipart_apple_double_get_type,
	e_mail_parser_multipart_digest_get_type,
	e_mail_parser_multipart_encrypted_get_type,
	e_mail_parser_multipart_mixed_get_type,
	e_mail_parser_multipart_related_get_type,
	e_mail_parser_multipart_signed_get_type,
	e_mail_parser_text_enriched_get_type,
	e_mail_parser_text_html_get_type,
	e_mail_parser_text_plain_get_type,
#ifdef ENABLE_SMIME
	e_mail_parser_application_smime_get_type,
#endif
	NULL
};

TypeFunc formatter_funcs[] = {
	e_mail_formatter_attachment_get_type,
	e_mail_formatter_attachment_bar_get_type,
	e_mail_formatter_error_get_type,
	e_mail_formatter_headers_get_type,
	e_mail_formatter_secure_button_get_type,
	e_mail_formatter_source_get_type,
	e_mail_formatter_image_get_type,
	e_mail_formatter_message_rfc822_get_type,
	e_mail_formatter_text_enriched_get_type,
	e_mail_formatter_text_html_get_type,
	e_mail_formatter_text_plain_get_type,
	NULL
};

TypeFunc quote_formatter_funcs[] = {
	e_mail_formatter_quote_attachment_get_type,
	e_mail_formatter_quote_headers_get_type,
	e_mail_formatter_quote_message_rfc822_get_type,
	e_mail_formatter_quote_text_enriched_get_type,
	e_mail_formatter_quote_text_html_get_type,
	e_mail_formatter_quote_text_plain_get_type,
	NULL
};

TypeFunc print_formatter_funcs[] = {
	e_mail_formatter_print_headers_get_type,
	NULL
};

static void
load (EMailExtensionRegistry *ereg,
      TypeFunc *func_array)
{
	gint i = 0;

	for (i = 0; func_array[i] != NULL; i++) {
		GType type;
		EMailExtension *extension;

		type = func_array[i]();
		extension = g_object_new (type, NULL);

		e_mail_extension_registry_add_extension (ereg, extension);
	}
}

void
e_mail_parser_internal_extensions_load (EMailExtensionRegistry *ereg)
{
	load (ereg, parser_funcs);
}

void
e_mail_formatter_internal_extensions_load (EMailExtensionRegistry *ereg)
{
	load (ereg, formatter_funcs);
}

void
e_mail_formatter_quote_internal_extensions_load (EMailExtensionRegistry *ereg)
{
	load (ereg, quote_formatter_funcs);
}

void
e_mail_formatter_print_internal_extensions_load (EMailExtensionRegistry *ereg)
{
	load (ereg, print_formatter_funcs);
}
