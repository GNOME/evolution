/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-editor-add-im.h
 * Copyright (C) 2003  Ximian, Inc.
 * Author: Christian Hammond <chipx86@gnupdate.org>
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
#ifndef __E_CONTACT_EDITOR_IM_H__
#define __E_CONTACT_EDITOR_IM_H__

#include <gtk/gtkdialog.h>
#include <glade/glade.h>
#include <libebook/e-contact.h>

G_BEGIN_DECLS

/* EContactEditorIm - A dialog allowing the user to add an IM account to a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * service      EContactField   RW              The field of the IM service.
 * location     char *          RW              The location type.
 * username     char *          RW              The username of the account.
 */

#define E_TYPE_CONTACT_EDITOR_IM			(e_contact_editor_im_get_type ())
#define E_CONTACT_EDITOR_IM(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CONTACT_EDITOR_IM, EContactEditorIm))
#define E_CONTACT_EDITOR_IM_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CONTACT_EDITOR_IM, EContactEditorImClass))
#define E_IS_CONTACT_EDITOR_IM(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CONTACT_EDITOR_IM))
#define E_IS_CONTACT_EDITOR_IM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_CONTACT_EDITOR_IM))


typedef struct _EContactEditorIm       EContactEditorIm;
typedef struct _EContactEditorImClass  EContactEditorImClass;

struct _EContactEditorIm
{
	GtkDialog parent;
	
	/* item specific fields */
	EContactField service;
	char *location;
	char *username;
	GladeXML *gui;

	/* Whether the dialog will accept modifications */
	guint editable : 1;
};

struct _EContactEditorImClass
{
	GtkDialogClass parent_class;
};


GtkWidget *e_contact_editor_im_new(EContactField service, const char *location, const char *username);
GType      e_contact_editor_im_get_type (void);

G_END_DECLS

#endif /* __E_CONTACT_EDITOR_IM_H__ */

