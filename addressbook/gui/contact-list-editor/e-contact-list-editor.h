/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-list-editor.h
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
#ifndef __E_CONTACT_LIST_EDITOR_H__
#define __E_CONTACT_LIST_EDITOR_H__

#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-app-helper.h>
#include <bonobo/bonobo-ui-component.h>
#include <glade/glade.h>
#include <gal/e-table/e-table-model.h>

#include "addressbook/backend/ebook/e-book.h"
#include "addressbook/backend/ebook/e-card.h"
#include "addressbook/backend/ebook/e-card-simple.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_CONTACT_LIST_EDITOR_TYPE			(e_contact_list_editor_get_type ())
#define E_CONTACT_LIST_EDITOR(obj)			(GTK_CHECK_CAST ((obj), E_CONTACT_LIST_EDITOR_TYPE, EContactListEditor))
#define E_CONTACT_LIST_EDITOR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_CONTACT_LIST_EDITOR_TYPE, EContactListEditorClass))
#define E_IS_CONTACT_LIST_EDITOR(obj)		(GTK_CHECK_TYPE ((obj), E_CONTACT_LIST_EDITOR_TYPE))
#define E_IS_CONTACT_LIST_EDITOR_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_CONTACT_LIST_EDITOR_TYPE))


typedef struct _EContactListEditor       EContactListEditor;
typedef struct _EContactListEditorClass  EContactListEditorClass;

struct _EContactListEditor
{
	GtkObject object;

	/* item specific fields */
	EBook *book;
	ECard *card;

	/* UI handler */
	BonoboUIComponent *uic;

	GladeXML *gui;
	GtkWidget *app;

	GtkWidget *table;
	ETableModel *model;
	GtkWidget *email_entry;
	GtkWidget *list_name_entry;
	GtkWidget *add_button;
	GtkWidget *remove_button;
	GtkWidget *visible_addrs_checkbutton;

	/* Whether we are editing a new card or an existing one */
	guint is_new_list : 1;

	/* Whether the card has been changed since bringing up the contact editor */
	guint changed : 1;

	/* Whether the contact editor will accept modifications */
	guint editable : 1;

	/* Whether an async wombat call is in progress */
	guint in_async_call : 1;
};

struct _EContactListEditorClass
{
	GtkObjectClass parent_class;

	/* Notification signals */

	void (* list_added)    (EContactListEditor *cle, EBookStatus status, ECard *card);
	void (* list_modified) (EContactListEditor *cle, EBookStatus status, ECard *card);
	void (* list_deleted)  (EContactListEditor *cle, EBookStatus status, ECard *card);
	void (* editor_closed) (EContactListEditor *cle);
};

EContactListEditor *e_contact_list_editor_new                (EBook *book,
							      ECard *list_card,
							      gboolean is_new_list,
							      gboolean editable);
GtkType             e_contact_list_editor_get_type           (void);
void                e_contact_list_editor_show               (EContactListEditor *editor);
void                e_contact_list_editor_raise              (EContactListEditor *editor);

gboolean            e_contact_list_editor_confirm_delete     (GtkWindow      *parent);

gboolean            e_contact_list_editor_request_close_all  (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_CONTACT_LIST_EDITOR_H__ */
