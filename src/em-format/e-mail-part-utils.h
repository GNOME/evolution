/*
 * e-mail-part-utils.h
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

#ifndef E_MAIL_PART_UTILS_H_
#define E_MAIL_PART_UTILS_H_

#include <camel/camel.h>
#include "e-mail-part.h"

G_BEGIN_DECLS

/* Header/parameter name for guessed MIME types; it's set to "1" when it's guessed. */
#define E_MAIL_PART_X_EVOLUTION_GUESSED "X-Evolution-Guessed"

gboolean	e_mail_part_is_secured		(CamelMimePart *part);

const gchar *	e_mail_part_get_frame_security_style
						(EMailPart *part);

gchar *		e_mail_part_guess_mime_type	(CamelMimePart *part);

gboolean	e_mail_part_is_attachment	(CamelMimePart *part);

void		e_mail_part_preserve_charset_in_content_type
						(CamelMimePart *ipart,
						 CamelMimePart *opart);

CamelMimePart *	e_mail_part_get_related_display_part
						(CamelMimePart *part,
						 gint *out_displayid);

void		e_mail_part_animation_extract_frame (
						GBytes *bytes,
						gchar **out_frame,
						gsize *out_len);

gchar *		e_mail_part_build_uri		(CamelFolder *folder,
						 const gchar *message_uid,
						 const gchar *first_param_name,
						 ...);

gchar *		e_mail_part_describe		(CamelMimePart *part,
						 const gchar *mime_type);

gboolean	e_mail_part_is_inline		(CamelMimePart *part,
						 GQueue *extensions);

gboolean	e_mail_part_utils_body_refers	(const gchar *body,
						 const gchar *cid);

CamelMimePart *	e_mail_part_utils_find_parent_part
						(CamelMimeMessage *message,
						 CamelMimePart *child);
G_END_DECLS

#endif /* E_MAIL_PART_UTILS_H_ */
