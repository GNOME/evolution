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
 * card         ECard *         R               The card currently being edited
 */

#define E_CONTACT_EDITOR_TYPE			(e_contact_editor_get_type ())
#define E_CONTACT_EDITOR(obj)			(GTK_CHECK_CAST ((obj), E_CONTACT_EDITOR_TYPE, EContactEditor))
#define E_CONTACT_EDITOR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_CONTACT_EDITOR_TYPE, EContactEditorClass))
#define E_IS_MINICARD(obj)		(GTK_CHECK_TYPE ((obj), E_CONTACT_EDITOR_TYPE))
#define E_IS_MINICARD_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_CONTACT_EDITOR_TYPE))


typedef struct _EContactEditor       EContactEditor;
typedef struct _EContactEditorClass  EContactEditorClass;

struct _EContactEditor
{
	GtkVBox parent;
	
	/* item specific fields */
	/*  ECard *card; */
	
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
};

struct _EContactEditorClass
{
	GtkVBoxClass parent_class;
};


GtkWidget *e_contact_editor_new(void *card);
GtkType    e_contact_editor_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_CONTACT_EDITOR_H__ */
