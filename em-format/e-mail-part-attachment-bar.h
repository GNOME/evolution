/*
 * e-mail-part-attachment-bar.h
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

#ifndef E_MAIL_PART_ATTACHMENT_BAR_H
#define E_MAIL_PART_ATTACHMENT_BAR_H

#include <em-format/e-mail-part.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PART_ATTACHMENT_BAR \
	(e_mail_part_attachment_bar_get_type ())
#define E_MAIL_PART_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PART_ATTACHMENT_BAR, EMailPartAttachmentBar))
#define E_MAIL_PART_ATTACHMENT_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PART_ATTACHMENT_BAR, EMailPartAttachmentBarClass))
#define E_IS_MAIL_PART_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PART_ATTACHMENT_BAR))
#define E_IS_MAIL_PART_ATTACHMENT_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PART_ATTACHMENT_BAR))
#define E_MAIL_PART_ATTACHMENT_BAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PART_ATTACHMENT_BAR, EMailPartAttachmentBarClass))

#define E_MAIL_PART_ATTACHMENT_BAR_MIME_TYPE \
	"application/vnd.evolution.widget.attachment-bar"

G_BEGIN_DECLS

typedef struct _EMailPartAttachmentBar EMailPartAttachmentBar;
typedef struct _EMailPartAttachmentBarClass EMailPartAttachmentBarClass;
typedef struct _EMailPartAttachmentBarPrivate EMailPartAttachmentBarPrivate;

struct _EMailPartAttachmentBar {
	EMailPart parent;
	EMailPartAttachmentBarPrivate *priv;
};

struct _EMailPartAttachmentBarClass {
	EMailPartClass parent_class;
};

GType		e_mail_part_attachment_bar_get_type
						(void) G_GNUC_CONST;
EMailPart *	e_mail_part_attachment_bar_new	(CamelMimePart *mime_part,
						 const gchar *id);
EAttachmentStore *
		e_mail_part_attachment_bar_get_store
						(EMailPartAttachmentBar *part);

G_END_DECLS

#endif /* E_MAIL_PART_ATTACHMENT_BAR_H */
