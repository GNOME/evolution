/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-editor-address.h
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
#ifndef __E_CONTACT_EDITOR_ADDRESS_H__
#define __E_CONTACT_EDITOR_ADDRESS_H__

#include <gnome.h>
#include <glade/glade.h>
#include <ebook/e-card.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EContactEditorAddress - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * name         ECardName *     RW              The card currently being edited. Returns a copy.
 */

#define E_CONTACT_EDITOR_ADDRESS_TYPE			(e_contact_editor_address_get_type ())
#define E_CONTACT_EDITOR_ADDRESS(obj)			(GTK_CHECK_CAST ((obj), E_CONTACT_EDITOR_ADDRESS_TYPE, EContactEditorAddress))
#define E_CONTACT_EDITOR_ADDRESS_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_CONTACT_EDITOR_ADDRESS_TYPE, EContactEditorAddressClass))
#define E_IS_CONTACT_EDITOR_ADDRESS(obj)		(GTK_CHECK_TYPE ((obj), E_CONTACT_EDITOR_ADDRESS_TYPE))
#define E_IS_CONTACT_EDITOR_ADDRESS_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_CONTACT_EDITOR_ADDRESS_TYPE))


typedef struct _EContactEditorAddress       EContactEditorAddress;
typedef struct _EContactEditorAddressClass  EContactEditorAddressClass;

struct _EContactEditorAddress
{
	GnomeDialog parent;
	
	/* item specific fields */
	ECardDeliveryAddress *address;
	GladeXML *gui;
};

struct _EContactEditorAddressClass
{
	GnomeDialogClass parent_class;
};


GtkWidget *e_contact_editor_address_new(const ECardDeliveryAddress *name);
GtkType    e_contact_editor_address_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_CONTACT_EDITOR_ADDRESS_H__ */
