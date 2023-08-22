/*
 * e-mail-formatter-utils.h
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

#ifndef E_MAIL_FORMATTER_UTILS_H_
#define E_MAIL_FORMATTER_UTILS_H_

#include <camel/camel.h>
#include <em-format/e-mail-formatter.h>

G_BEGIN_DECLS

void		e_mail_formatter_format_header (EMailFormatter *formatter,
						GString *buffer,
						const gchar *header_name,
						const gchar *header_value,
						guint32 flags,
						const gchar *charset);

void		e_mail_formatter_format_text_header
						(EMailFormatter *formatter,
						 GString *buffer,
						 const gchar *label,
						 const gchar *value,
						 guint32 flags);

gchar *		e_mail_formatter_format_address (EMailFormatter *formatter,
						 GString *out,
						 struct _camel_header_address *a,
						 const gchar *field,
						 gboolean no_links,
						 gboolean elipsize);

void		e_mail_formatter_canon_header_name
						(gchar *name);

GList *		e_mail_formatter_find_rfc822_end_iter
						(GList *rfc822_start_iter);

gchar *		e_mail_formatter_parse_html_mnemonics
						(const gchar *label,
						 gchar **out_access_key);

void		e_mail_formatter_format_security_header
						(EMailFormatter *formatter,
						 EMailFormatterContext *context,
						 GString *buffer,
						 EMailPart *part,
						 guint32 flags);
GHashTable * /* const gchar *message_part_id ~> NULL */
		e_mail_formatter_utils_extract_secured_message_ids
						(GList *parts); /* EMailPart * */
gboolean	e_mail_formatter_utils_consider_as_secured_part
						(EMailPart *part,
						 GHashTable *secured_message_ids);

G_END_DECLS

#endif /* E_MAIL_FORMATTER_UTILS_H_ */
