/*
 * e-mail-part-attachment.h
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

#ifndef E_MAIL_PART_ATTACHMENT_H
#define E_MAIL_PART_ATTACHMENT_H

#include <em-format/e-mail-part.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PART_ATTACHMENT \
	(e_mail_part_attachment_get_type ())
#define E_MAIL_PART_ATTACHMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PART_ATTACHMENT, EMailPartAttachment))
#define E_MAIL_PART_ATTACHMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PART_ATTACHMENT, EMailPartAttachmentClass))
#define E_IS_MAIL_PART_ATTACHMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PART_ATTACHMENT))
#define E_IS_MAIL_PART_ATTACHMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PART_ATTACHMENT))
#define E_MAIL_PART_ATTACHMENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PART_ATTACHMENT, EMailPartAttachmentClass))

#define E_MAIL_PART_ATTACHMENT_MIME_TYPE \
	"application/vnd.evolution.attachment"

G_BEGIN_DECLS

typedef struct _EMailPartAttachment EMailPartAttachment;
typedef struct _EMailPartAttachmentClass EMailPartAttachmentClass;
typedef struct _EMailPartAttachmentPrivate EMailPartAttachmentPrivate;

struct _EMailPartAttachment {
	EMailPart parent;
	EMailPartAttachmentPrivate *priv;

	gchar *part_id_with_attachment;

	gboolean shown;
};

struct _EMailPartAttachmentClass {
	EMailPartClass parent_class;
};

GType		e_mail_part_attachment_get_type	(void) G_GNUC_CONST;
EMailPartAttachment *
		e_mail_part_attachment_new	(CamelMimePart *mime_part,
						 const gchar *id);
EAttachment *	e_mail_part_attachment_ref_attachment
						(EMailPartAttachment *part);
void		e_mail_part_attachment_set_expandable
						(EMailPartAttachment *part,
						 gboolean expandable);
gboolean	e_mail_part_attachment_get_expandable
						(EMailPartAttachment *part);
void		e_mail_part_attachment_take_guessed_mime_type
						(EMailPartAttachment *part,
						 gchar *guessed_mime_type);
const gchar *	e_mail_part_attachment_get_guessed_mime_type
						(EMailPartAttachment *part);

G_END_DECLS

#endif /* E_MAIL_PART_ATTACHMENT_H */
