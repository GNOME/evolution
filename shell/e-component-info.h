/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-component-info.h - Load/save information about Evolution components.
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef E_COMPONENT_INFO_H
#define E_COMPONENT_INFO_H

#include <glib.h>

struct _EComponentInfoFolderType {
	char *name;
	char *icon_file_name;
	char *display_name;
	char *description;

	GSList *accepted_dnd_types; /* <char *> */
	GSList *exported_dnd_types; /* <char *> */

	unsigned int is_user_creatable : 1;
};
typedef struct _EComponentInfoFolderType EComponentInfoFolderType;

struct _EComponentInfoUserCreatableItemType {
	char *id;
	char *description;
	char *icon_file_name;

	char *menu_description;
	char *menu_shortcut;
};
typedef struct _EComponentInfoUserCreatableItemType EComponentInfoUserCreatableItemType;

struct _EComponentInfo {
	char *id;
	char *description;
	char *icon_file_name;

	GSList *folder_types;			/* <EComponentInfoFolderType> */
	GSList *uri_schemas;   			/* <char *> */
	GSList *user_creatable_item_types;	/* <EComponentInfoUserCreatableItemType> */
};
typedef struct _EComponentInfo EComponentInfo;


EComponentInfo *e_component_info_load  (const char     *file_name);
void            e_component_info_free  (EComponentInfo *info);

#endif /* E_COMPONENT_INFO_H */
