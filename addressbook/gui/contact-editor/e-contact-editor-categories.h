/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-editor-categories.h
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
#ifndef __E_CONTACT_EDITOR_CATEGORIES_H__
#define __E_CONTACT_EDITOR_CATEGORIES_H__

#include <gnome.h>
#include <glade/glade.h>
#include <ebook/e-card.h>
#include <widgets/e-table/e-table-model.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EContactEditorCategories - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * name         ECardName *     RW              The card currently being edited. Returns a copy.
 */

#define E_CONTACT_EDITOR_CATEGORIES_TYPE			(e_contact_editor_categories_get_type ())
#define E_CONTACT_EDITOR_CATEGORIES(obj)			(GTK_CHECK_CAST ((obj), E_CONTACT_EDITOR_CATEGORIES_TYPE, EContactEditorCategories))
#define E_CONTACT_EDITOR_CATEGORIES_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_CONTACT_EDITOR_CATEGORIES_TYPE, EContactEditorCategoriesClass))
#define E_IS_CONTACT_EDITOR_CATEGORIES(obj)		(GTK_CHECK_TYPE ((obj), E_CONTACT_EDITOR_CATEGORIES_TYPE))
#define E_IS_CONTACT_EDITOR_CATEGORIES_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_CONTACT_EDITOR_CATEGORIES_TYPE))


typedef struct _EContactEditorCategories       EContactEditorCategories;
typedef struct _EContactEditorCategoriesClass  EContactEditorCategoriesClass;

struct _EContactEditorCategories
{
	GnomeDialog parent;
	
	/* item specific fields */
	char *categories;
	GtkWidget *entry;
	ETableModel *model;

	int list_length;
	char **category_list;
	gboolean *selected_list;

	GladeXML *gui;
};

struct _EContactEditorCategoriesClass
{
	GnomeDialogClass parent_class;
};


GtkWidget *e_contact_editor_categories_new(char *categories);
GtkType    e_contact_editor_categories_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_CONTACT_EDITOR_CATEGORIES_H__ */
