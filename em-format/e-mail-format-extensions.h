/*
 * e-mail-format-extensions.h
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

#ifndef E_MAIL_FORMAT_EXTENSIONS_H
#define E_MAIL_FORMAT_EXTENSIONS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-part.h>
#include <e-util/e-util.h>

G_BEGIN_DECLS

void	e_mail_formatter_internal_extensions_load	(EMailExtensionRegistry *ereg);
void	e_mail_parser_internal_extensions_load		(EMailExtensionRegistry *ereg);
void	e_mail_formatter_quote_internal_extensions_load	(EMailExtensionRegistry *ereg);
void	e_mail_formatter_print_internal_extensions_load	(EMailExtensionRegistry *ereg);

GType	e_mail_formatter_attachment_get_type		(void);
GType	e_mail_formatter_attachment_bar_get_type
							(void);
GType	e_mail_formatter_error_get_type			(void);
GType	e_mail_formatter_headers_get_type		(void);
GType	e_mail_formatter_secure_button_get_type
							(void);
GType	e_mail_formatter_source_get_type		(void);
GType	e_mail_formatter_image_get_type			(void);
GType	e_mail_formatter_message_rfc822_get_type	(void);
GType	e_mail_formatter_text_enriched_get_type		(void);
GType	e_mail_formatter_text_html_get_type		(void);
GType	e_mail_formatter_text_plain_get_type		(void);

GType	e_mail_parser_application_mbox_get_type		(void);
GType	e_mail_parser_attachment_bar_get_type		(void);
GType	e_mail_parser_headers_get_type			(void);
GType	e_mail_parser_message_get_type			(void);
GType	e_mail_parser_secure_button_get_type		(void);
GType	e_mail_parser_source_get_type			(void);
GType	e_mail_parser_image_get_type			(void);
GType	e_mail_parser_inline_pgp_encrypted_get_type	(void);
GType	e_mail_parser_inline_pgp_signed_get_type	(void);
GType	e_mail_parser_message_delivery_status_get_type	(void);
GType	e_mail_parser_message_external_get_type		(void);
GType	e_mail_parser_message_rfc822_get_type		(void);
GType	e_mail_parser_multipart_alternative_get_type	(void);
GType	e_mail_parser_multipart_apple_double_get_type	(void);
GType	e_mail_parser_multipart_digest_get_type		(void);
GType	e_mail_parser_multipart_encrypted_get_type	(void);
GType	e_mail_parser_multipart_mixed_get_type		(void);
GType	e_mail_parser_multipart_related_get_type	(void);
GType	e_mail_parser_multipart_signed_get_type		(void);
GType	e_mail_parser_text_enriched_get_type		(void);
GType	e_mail_parser_text_html_get_type		(void);
GType	e_mail_parser_text_plain_get_type		(void);
#ifdef ENABLE_SMIME
GType	e_mail_parser_application_smime_get_type	(void);
#endif

GType	e_mail_formatter_quote_attachment_get_type	(void);
GType	e_mail_formatter_quote_headers_get_type		(void);
GType	e_mail_formatter_quote_message_rfc822_get_type	(void);
GType	e_mail_formatter_quote_text_enriched_get_type	(void);
GType	e_mail_formatter_quote_text_html_get_type	(void);
GType	e_mail_formatter_quote_text_plain_get_type	(void);

GType	e_mail_formatter_print_headers_get_type		(void);

G_END_DECLS

#endif /* E_MAIL_FORMAT_EXTENSIONS_H */
