/*
 * e-mail-part-attachment.h
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

#ifndef E_MAIL_PART_ATTACHMENT_H
#define E_MAIL_PART_ATTACHMENT_H

#include <em-format/e-mail-part.h>

#define E_MAIL_PART_ATTACHMENT(part) \
	((EMailPartAttachment *) part)
#define E_IS_MAIL_PART_ATTACHMENT(part) \
	(E_MAIL_PART_IS (part, EMailPartAttachment))

#define E_MAIL_PART_ATTACHMENT_MIME_TYPE \
	"application/vnd.evolution.attachment"

G_BEGIN_DECLS

typedef struct _EMailPartAttachment {
	EMailPart parent;

	EAttachment *attachment;
	gchar *attachment_view_part_id;

	gboolean shown;
	const gchar *snoop_mime_type;

} EMailPartAttachment;

void		e_mail_part_attachment_free	(EMailPartAttachment *empa);
EAttachment *	e_mail_part_attachment_ref_attachment
						(EMailPartAttachment *part);

G_END_DECLS

#endif /* E_MAIL_PART_ATTACHMENT_H */
