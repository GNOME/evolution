/*
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
 *
 * Authors:
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_ADDRESSBOOK_UTIL_H__
#define __E_ADDRESSBOOK_UTIL_H__

#include <gtk/gtk.h>
#include <libebook/e-book.h>
#include "addressbook/gui/contact-editor/e-contact-editor.h"
#include "addressbook/gui/contact-list-editor/e-contact-list-editor.h"

G_BEGIN_DECLS

void                eab_error_dialog              (const gchar *msg,
						   EBookStatus  status);
void                eab_load_error_dialog         (GtkWidget *parent,
						   ESource *source,
						   EBookStatus status);
void                eab_search_result_dialog      (GtkWidget *parent,
						   EBookViewStatus status);
gint                eab_prompt_save_dialog        (GtkWindow   *parent);

EContactEditor     *eab_show_contact_editor       (EBook       *book,
						   EContact    *contact,
						   gboolean     is_new_contact,
						   gboolean     editable);
EContactListEditor *eab_show_contact_list_editor  (EBook       *book,
						   EContact    *contact,
						   gboolean     is_new_contact,
						   gboolean     editable);
void                eab_show_multiple_contacts    (EBook       *book,
						   GList       *list,
						   gboolean     editable);
void                eab_transfer_contacts         (EBook       *source,
						   GList       *contacts, /* adopted */
						   gboolean     delete_from_source,
						   GtkWindow   *parent_window);

void                eab_contact_save              (gchar *title,
						   EContact *contact,
						   GtkWindow *parent_window);

void                eab_contact_list_save         (gchar *title,
						   GList *list,
						   GtkWindow *parent_window);

typedef enum {
	EAB_DISPOSITION_AS_ATTACHMENT,
	EAB_DISPOSITION_AS_TO
} EABDisposition;

void                eab_send_contact              (EContact       *contact,
						   gint             email_num,
						   EABDisposition  disposition);
void                eab_send_contact_list         (GList          *contacts,
						   EABDisposition  disposition);

GtkWidget *eab_create_image_chooser_widget (gchar *name, gchar *string1, gchar *string2, gint int1, gint int2);

ESource            *eab_select_source             (const gchar *title, const gchar *message,
						   const gchar *select_uid, GtkWindow *parent);

/* To parse quoted printable address & return email & name fields */
gboolean eab_parse_qp_email (const gchar *string, gchar **name, gchar **email);
gchar *eab_parse_qp_email_to_html (const gchar *string);

G_END_DECLS

#endif /* __E_ADDRESSBOOK_UTIL_H__ */
