/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-editor.h
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
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
#ifndef __E_CONTACT_EDITOR_H__
#define __E_CONTACT_EDITOR_H__

#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-app-helper.h>
#include <bonobo/bonobo-ui-component.h>
#include <glade/glade.h>

#include "addressbook/gui/component/select-names/e-select-names-manager.h"
#include "addressbook/backend/ebook/e-book.h"
#include "addressbook/backend/ebook/e-card.h"
#include "addressbook/backend/ebook/e-card-simple.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EContactEditor - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * card         ECard *         RW              The card currently being edited
 */

#define E_CONTACT_EDITOR_TYPE			(e_contact_editor_get_type ())
#define E_CONTACT_EDITOR(obj)			(GTK_CHECK_CAST ((obj), E_CONTACT_EDITOR_TYPE, EContactEditor))
#define E_CONTACT_EDITOR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_CONTACT_EDITOR_TYPE, EContactEditorClass))
#define E_IS_CONTACT_EDITOR(obj)		(GTK_CHECK_TYPE ((obj), E_CONTACT_EDITOR_TYPE))
#define E_IS_CONTACT_EDITOR_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_CONTACT_EDITOR_TYPE))


typedef struct _EContactEditor       EContactEditor;
typedef struct _EContactEditorClass  EContactEditorClass;

struct _EContactEditor
{
	GtkObject object;
	
	/* item specific fields */
	EBook *book;
	ECard *card;
	ECardSimple *simple;

	/* UI handler */
	BonoboUIComponent *uic;
	
	GladeXML *gui;
	GtkWidget *app;
	GnomeUIInfo *email_info;
	GnomeUIInfo *phone_info;
	GnomeUIInfo *address_info;
	GtkWidget *email_popup;
	GtkWidget *phone_popup;
	GtkWidget *address_popup;
	GList *email_list;
	GList *phone_list;
	GList *address_list;

	ESelectNamesManager *select_names_contacts;

	ECardName *name;
	char *company;

	ECardSimpleEmailId email_choice;
	ECardSimplePhoneId phone_choice[4];
	ECardSimpleAddressId address_choice;
	ECardSimpleAddressId address_mailing;
	
	GList *arbitrary_fields;

	/* Whether we are editing a new card or an existing one */
	guint is_new_card : 1;

	/* Whether the card has been changed since bringing up the contact editor */
	guint changed : 1;

	/* Whether the contact editor will accept modifications */
	guint editable : 1;

	/* Whether the fullname will accept modifications */
	guint fullname_editable : 1;

	/* Whether each of the addresses are editable */
	gboolean address_editable[E_CARD_SIMPLE_ADDRESS_ID_LAST];

	/* Whether an async wombat call is in progress */
	guint in_async_call : 1;

	EList *writable_fields;
};

struct _EContactEditorClass
{
	GtkObjectClass parent_class;

	/* Notification signals */

	void (* card_added)    (EContactEditor *ce, EBookStatus status, ECard *card);
	void (* card_modified) (EContactEditor *ce, EBookStatus status, ECard *card);
	void (* card_deleted)  (EContactEditor *ce, EBookStatus status, ECard *card);
	void (* editor_closed) (EContactEditor *ce);
};

EContactEditor *e_contact_editor_new                (EBook          *book,
						     ECard          *card,
						     gboolean        is_new_card,
						     gboolean        editable);
GtkType         e_contact_editor_get_type           (void);

void            e_contact_editor_show               (EContactEditor *editor);
void            e_contact_editor_close              (EContactEditor *editor);
void            e_contact_editor_raise              (EContactEditor *editor);

gboolean        e_contact_editor_confirm_delete     (GtkWindow      *parent);

gboolean        e_contact_editor_request_close_all  (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_CONTACT_EDITOR_H__ */
