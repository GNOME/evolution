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

#include "addressbook/gui/contact-editor/eab-editor.h"

#include <libebook/e-book.h>
#include <libebook/e-contact.h>

#include <gtk/gtktreeview.h>
#include <gtk/gtkliststore.h>

G_BEGIN_DECLS

/* EContactEditor - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * card         ECard *         RW              The card currently being edited
 */

#define E_TYPE_CONTACT_EDITOR			(e_contact_editor_get_type ())
#define E_CONTACT_EDITOR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CONTACT_EDITOR, EContactEditor))
#define E_CONTACT_EDITOR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CONTACT_EDITOR, EContactEditorClass))
#define E_IS_CONTACT_EDITOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CONTACT_EDITOR))
#define E_IS_CONTACT_EDITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_CONTACT_EDITOR))


typedef struct _EContactEditor       EContactEditor;
typedef struct _EContactEditorClass  EContactEditorClass;

struct _EContactEditor
{
	EABEditor object;
	
	/* item specific fields */
	EBook *source_book;
	EBook *target_book;
	EContact *contact;

	/* UI handler */
	BonoboUIComponent *uic;
	
	GladeXML *gui;
	GtkWidget *app;

	GtkWidget *file_selector;

	EContactName *name;

	/* Whether we are editing a new contact or an existing one */
	guint is_new_contact : 1;

	/* Whether the image chooser widget has been changed. */
	guint image_set : 1;

	/* Whether the contact has been changed since bringing up the contact editor */
	guint changed : 1;

	/* Whether the contact editor will accept modifications, save */
	guint target_editable : 1;

	/* Whether an async wombat call is in progress */
	guint in_async_call : 1;

	EList *writable_fields;

	/* ID for async load_source call */
	guint  load_source_id;
	EBook *load_book;

	/* signal ids for "writable_status" */
	int target_editable_id;
};

struct _EContactEditorClass
{
	EABEditorClass parent_class;
};

EContactEditor *e_contact_editor_new                (EBook          *book,
						     EContact       *contact,
						     gboolean        is_new_contact,
						     gboolean        editable);
GType           e_contact_editor_get_type           (void);

G_END_DECLS

#endif /* __E_CONTACT_EDITOR_H__ */
