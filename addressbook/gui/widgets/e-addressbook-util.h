/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-addressbook-util.h
 * Copyright (C) 2001  Ximian, Inc.
 * Author: Chris Toshok <toshok@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __E_ADDRESSBOOK_UTIL_H__
#define __E_ADDRESSBOOK_UTIL_H__

#include "addressbook/backend/ebook/e-book.h"
#include "addressbook/gui/contact-editor/e-contact-editor.h"
#include "addressbook/gui/contact-list-editor/e-contact-list-editor.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

void                e_addressbook_error_dialog              (const gchar *msg,
							     EBookStatus  status);
gint                e_addressbook_prompt_save_dialog        (GtkWindow   *parent);
EContactEditor     *e_addressbook_show_contact_editor       (EBook       *book,
							     ECard       *card,
							     gboolean     is_new_card,
							     gboolean     editable);
EContactListEditor *e_addressbook_show_contact_list_editor  (EBook       *book,
							     ECard       *card,
							     gboolean     is_new_card,
							     gboolean     editable);
void                e_addressbook_show_multiple_cards       (EBook       *book,
							     GList       *list,
							     gboolean     editable);
void                e_addressbook_transfer_cards            (EBook       *source,
							     GList       *cards, /* adopted */
							     gboolean     delete_from_source,
							     GtkWindow   *parent_window);

typedef enum {
	E_ADDRESSBOOK_DISPOSITION_AS_ATTACHMENT,
	E_ADDRESSBOOK_DISPOSITION_AS_TO,
} EAddressbookDisposition;

void                e_addressbook_send_card                 (ECard                   *card,
							     EAddressbookDisposition  disposition);
void                e_addressbook_send_card_list            (GList                   *cards,
							     EAddressbookDisposition  disposition);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_ADDRESSBOOK_UTIL_H__ */
