/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-folder-list.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __E_FOLDER_LIST_H__
#define __E_FOLDER_LIST_H__

#include <gal/e-table/e-table-scrolled.h>

#include <gtk/gtkvbox.h>

#include "evolution-shell-client.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EFolderList - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define E_FOLDER_LIST_TYPE			(e_folder_list_get_type ())
#define E_FOLDER_LIST(obj)			(GTK_CHECK_CAST ((obj), E_FOLDER_LIST_TYPE, EFolderList))
#define E_FOLDER_LIST_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_FOLDER_LIST_TYPE, EFolderListClass))
#define E_IS_FOLDER_LIST(obj)		(GTK_CHECK_TYPE ((obj), E_FOLDER_LIST_TYPE))
#define E_IS_FOLDER_LIST_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_FOLDER_LIST_TYPE))


typedef struct _EFolderListPrivate  EFolderListPrivate;
typedef struct _EFolderList         EFolderList;
typedef struct _EFolderListClass    EFolderListClass;

struct _EFolderList
{
	GtkVBox parent;
	
	/* item specific fields */
	EFolderListPrivate *priv;
};

struct _EFolderListClass
{
	GtkVBoxClass parent_class;

	void (*changed) (EFolderList *efl);
	void (*option_menu_changed) (EFolderList *efl, int value);
};

typedef struct {
	char *uri;
	char *physical_uri;
	char *display_name;
} EFolderListItem;


EFolderListItem *e_folder_list_parse_xml                           (const char            *xml);
char            *e_folder_list_create_xml                          (EFolderListItem       *items);
void             e_folder_list_free_items                          (EFolderListItem       *items);

/* Standard functions */
GtkType          e_folder_list_get_type                            (void);
GtkWidget       *e_folder_list_new                                 (EvolutionShellClient  *client,
								    const char            *xml);
GtkWidget       *e_folder_list_construct                           (EFolderList           *efl,
								    EvolutionShellClient  *client,
								    const char            *xml);

/* data access functions */
void             e_folder_list_set_items                           (EFolderList           *efl,
								    EFolderListItem       *items);
EFolderListItem *e_folder_list_get_items                           (EFolderList           *efl);
void             e_folder_list_set_xml                             (EFolderList           *efl,
								    const char            *xml);
char            *e_folder_list_get_xml                             (EFolderList           *efl);

/* Option Menu functions */
void             e_folder_list_set_option_menu_strings_from_array  (EFolderList           *efl,
								    const char           **strings);
void             e_folder_list_set_option_menu_strings             (EFolderList           *efl,
								    const char            *first_label,
								    ...);
int              e_folder_list_get_option_menu_value               (EFolderList           *efl);
void             e_folder_list_set_option_menu_value               (EFolderList           *efl,
								    int                    value);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_FOLDER_LIST_H__ */
