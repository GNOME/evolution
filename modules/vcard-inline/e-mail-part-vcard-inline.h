/*
 * e-mail-part-vcard-inline.h
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

#ifndef E_MAIL_PART_VCARD_INLINE_H
#define E_MAIL_PART_VCARD_INLINE_H

#include <em-format/e-mail-part.h>

#include <addressbook/gui/widgets/eab-contact-formatter.h>
#include <webkit/webkitdom.h>

#define E_IS_MAIL_PART_VCARD(part) \
	(E_MAIL_PART_IS (part, EMailPartVCardInline))

G_BEGIN_DECLS

typedef struct _EMailPartVCardInline EMailPartVCardInline;

struct _EMailPartVCardInline {
	EMailPart parent;

	GSList *contact_list;
	GtkWidget *contact_display;
	GtkWidget *message_label;

	EABContactFormatter *formatter;
	WebKitDOMElement *iframe;
	WebKitDOMElement *toggle_button;
	WebKitDOMElement *save_button;

	CamelFolder *folder;
	gchar *message_uid;
};

G_END_DECLS

#endif /* E_MAIL_PART_VCARD_INLINE_H */

