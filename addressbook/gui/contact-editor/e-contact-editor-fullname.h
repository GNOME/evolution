/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-editor-fullname.h
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
#ifndef __E_CONTACT_EDITOR_FULLNAME_H__
#define __E_CONTACT_EDITOR_FULLNAME_H__

#include <gnome.h>
#include <glade/glade.h>
#include <ebook/e-card.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EContactEditorFullname - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * name         ECardName *     RW              The card currently being edited. Returns a copy.
 */

#define E_CONTACT_EDITOR_FULLNAME_TYPE			(e_contact_editor_fullname_get_type ())
#define E_CONTACT_EDITOR_FULLNAME(obj)			(GTK_CHECK_CAST ((obj), E_CONTACT_EDITOR_FULLNAME_TYPE, EContactEditorFullname))
#define E_CONTACT_EDITOR_FULLNAME_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_CONTACT_EDITOR_FULLNAME_TYPE, EContactEditorFullnameClass))
#define E_IS_CONTACT_EDITOR_FULLNAME(obj)		(GTK_CHECK_TYPE ((obj), E_CONTACT_EDITOR_FULLNAME_TYPE))
#define E_IS_CONTACT_EDITOR_FULLNAME_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_CONTACT_EDITOR_FULLNAME_TYPE))


typedef struct _EContactEditorFullname       EContactEditorFullname;
typedef struct _EContactEditorFullnameClass  EContactEditorFullnameClass;

struct _EContactEditorFullname
{
	GnomeDialog parent;
	
	/* item specific fields */
	ECardName *name;
	GladeXML *gui;
};

struct _EContactEditorFullnameClass
{
	GnomeDialogClass parent_class;
};


GtkWidget *e_contact_editor_fullname_new(const ECardName *name);
GtkType    e_contact_editor_fullname_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_CONTACT_EDITOR_FULLNAME_H__ */
