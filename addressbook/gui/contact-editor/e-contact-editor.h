/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-editor.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <gnome.h>
#include <glade/glade.h>
#include <ebook/e-card.h>

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

typedef enum _EContactEditorPhoneId EContactEditorPhoneId;
typedef enum _EContactEditorEmailId EContactEditorEmailId;
typedef enum _EContactEditorAddressId EContactEditorAddressId;

enum _EContactEditorPhoneId {
	E_CONTACT_EDITOR_PHONE_ID_ASSISTANT,
	E_CONTACT_EDITOR_PHONE_ID_BUSINESS,
	E_CONTACT_EDITOR_PHONE_ID_BUSINESS_2,
	E_CONTACT_EDITOR_PHONE_ID_BUSINESS_FAX,
	E_CONTACT_EDITOR_PHONE_ID_CALLBACK,
	E_CONTACT_EDITOR_PHONE_ID_CAR,
	E_CONTACT_EDITOR_PHONE_ID_COMPANY,
	E_CONTACT_EDITOR_PHONE_ID_HOME,
	E_CONTACT_EDITOR_PHONE_ID_HOME_2,
	E_CONTACT_EDITOR_PHONE_ID_HOME_FAX,
	E_CONTACT_EDITOR_PHONE_ID_ISDN,
	E_CONTACT_EDITOR_PHONE_ID_MOBILE,
	E_CONTACT_EDITOR_PHONE_ID_OTHER,
	E_CONTACT_EDITOR_PHONE_ID_OTHER_FAX,
	E_CONTACT_EDITOR_PHONE_ID_PAGER,
	E_CONTACT_EDITOR_PHONE_ID_PRIMARY,
	E_CONTACT_EDITOR_PHONE_ID_RADIO,
	E_CONTACT_EDITOR_PHONE_ID_TELEX,
	E_CONTACT_EDITOR_PHONE_ID_TTYTTD,
	E_CONTACT_EDITOR_PHONE_ID_LAST
};

/* We need HOME and WORK email addresses here. */
enum _EContactEditorEmailId {
	E_CONTACT_EDITOR_EMAIL_ID_EMAIL,
	E_CONTACT_EDITOR_EMAIL_ID_EMAIL_2,
	E_CONTACT_EDITOR_EMAIL_ID_EMAIL_3,
	E_CONTACT_EDITOR_EMAIL_ID_LAST
};

/* Should this include (BILLING/SHIPPING)? */
enum _EContactEditorAddressId {
	E_CONTACT_EDITOR_ADDRESS_ID_BUSINESS,
	E_CONTACT_EDITOR_ADDRESS_ID_HOME,
	E_CONTACT_EDITOR_ADDRESS_ID_OTHER,
	E_CONTACT_EDITOR_ADDRESS_ID_LAST
};

#define E_CONTACT_EDITOR_TYPE			(e_contact_editor_get_type ())
#define E_CONTACT_EDITOR(obj)			(GTK_CHECK_CAST ((obj), E_CONTACT_EDITOR_TYPE, EContactEditor))
#define E_CONTACT_EDITOR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_CONTACT_EDITOR_TYPE, EContactEditorClass))
#define E_IS_CONTACT_EDITOR(obj)		(GTK_CHECK_TYPE ((obj), E_CONTACT_EDITOR_TYPE))
#define E_IS_CONTACT_EDITOR_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_CONTACT_EDITOR_TYPE))


typedef struct _EContactEditor       EContactEditor;
typedef struct _EContactEditorClass  EContactEditorClass;

struct _EContactEditor
{
	GtkVBox parent;
	
	/* item specific fields */
	ECard *card;
	
	GladeXML *gui;
	GnomeUIInfo *email_info;
	GnomeUIInfo *phone_info;
	GnomeUIInfo *address_info;
	GtkWidget *email_popup;
	GtkWidget *phone_popup;
	GtkWidget *address_popup;
	GList *email_list;
	GList *phone_list;
	GList *address_list;

	ECardPhone *phone[E_CONTACT_EDITOR_PHONE_ID_LAST];
	char *email[E_CONTACT_EDITOR_EMAIL_ID_LAST];
	ECardAddrLabel *address[E_CONTACT_EDITOR_ADDRESS_ID_LAST];

	EContactEditorEmailId email_choice;
	EContactEditorPhoneId phone_choice[4];
	EContactEditorAddressId address_choice;
};

struct _EContactEditorClass
{
	GtkVBoxClass parent_class;
};


GtkWidget *e_contact_editor_new(ECard *card);
GtkType    e_contact_editor_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_CONTACT_EDITOR_H__ */
