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

#include "addressbook/gui/contact-editor/eab-editor.h"

#include <libebook/e-book.h>
#include <libebook/e-contact.h>
#include "addressbook/util/e-destination.h"

G_BEGIN_DECLS

#define E_TYPE_CONTACT_LIST_EDITOR	   (e_contact_list_editor_get_type ())
#define E_CONTACT_LIST_EDITOR(obj)	   (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CONTACT_LIST_EDITOR, EContactListEditor))
#define E_CONTACT_LIST_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CONTACT_LIST_EDITOR, EContactListEditorClass))
#define E_IS_CONTACT_LIST_EDITOR(obj)	   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CONTACT_LIST_EDITOR))
#define E_IS_CONTACT_LIST_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_CONTACT_LIST_EDITOR))

#define SELECT_NAMES_OAFIID "OAFIID:GNOME_Evolution_Addressbook_SelectNames:" BASE_VERSION

typedef struct _EContactListEditor       EContactListEditor;
typedef struct _EContactListEditorClass  EContactListEditorClass;

struct _EContactListEditor
{
	EABEditor parent;

	/* item specific fields */
	EBook *book;

	EContact *contact;

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
	GtkWidget *select_button;
	GtkWidget *list_image_button;
	GtkWidget *visible_addrs_checkbutton;
	GtkWidget *list_image;
	GtkWidget *source_menu;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;

	/* FIXME: Unfortunately, we can't use the proper name here, as it'd
	 * create a circular dependency. The long-term solution would be to
	 * move the select-names component out of the component/ dir so it can
	 * be built before sources using this.
	 * 
	 * GNOME_Evolution_Addressbook_SelectNames corba_select_names; */
	gpointer corba_select_names;

	/* Whether we are editing a new contact or an existing one */
	guint is_new_list : 1;

	/* Whether the image chooser widget has been changed. */
	guint image_set : 1;

	/* Whether the contact has been changed since bringing up the contact editor */
	guint changed : 1;

	/* Whether the contact editor will accept modifications */
	guint editable : 1;

	/* Whether the target book accepts storing of contact lists */
	guint allows_contact_lists : 1;

	/* Whether an async wombat call is in progress */
	guint in_async_call : 1;

	/* ID for async load_source call */
	guint  load_source_id;
	EBook *load_book;
};

struct _EContactListEditorClass
{
	EABEditorClass parent_class;
};

EContactListEditor *e_contact_list_editor_new                (EBook *book,
							      EContact *list_contact,
							      gboolean is_new_list,
							      gboolean editable);
GType               e_contact_list_editor_get_type           (void);

gboolean            e_contact_list_editor_request_close_all  (void);

G_END_DECLS


#endif /* __E_CONTACT_LIST_EDITOR_H__ */
