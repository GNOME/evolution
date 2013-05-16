/*
 * e-mail-part-attachment-bar.h
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

#ifndef E_MAIL_PART_ATTACHMENT_BAR_H
#define E_MAIL_PART_ATTACHMENT_BAR_H

#include <em-format/e-mail-part.h>

#define E_MAIL_PART_ATTACHMENT_BAR(part) \
	((EMailPartAttachmentBar *) part)
#define E_IS_MAIL_PART_ATTACHMENT_BAR(part) \
	(E_MAIL_PART_IS (part, EMailPartAttachmentBar))

#define E_MAIL_PART_ATTACHMENT_BAR_MIME_TYPE \
	"application/vnd.evolution.widget.attachment-bar"

typedef struct _EMailPartAttachmentBar {
	EMailPart parent;

	EAttachmentStore *store;
} EMailPartAttachmentBar;

#endif /* E_MAIL_PART_ATTACHMENT_BAR_H */
