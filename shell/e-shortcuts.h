/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shortcuts.h
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef _E_SHORTCUTS_H_
#define _E_SHORTCUTS_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkwidget.h>

#include "e-folder-type-repository.h"
#include "e-storage-set.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SHORTCUTS			(e_shortcuts_get_type ())
#define E_SHORTCUTS(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHORTCUTS, EShortcuts))
#define E_SHORTCUTS_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHORTCUTS, EShortcutsClass))
#define E_IS_SHORTCUTS(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHORTCUTS))
#define E_IS_SHORTCUTS_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHORTCUTS))


typedef struct _EShortcuts        EShortcuts;
typedef struct _EShortcutsPrivate EShortcutsPrivate;
typedef struct _EShortcutsClass   EShortcutsClass;

struct _EShortcuts {
	GtkObject parent;

	EShortcutsPrivate *priv;
};

struct _EShortcutsClass {
	GtkObjectClass parent_class;
};


GtkType     e_shortcuts_get_type   (void);
void        e_shortcuts_construct  (EShortcuts            *shortcuts,
				    EStorageSet           *storage_set,
				    EFolderTypeRepository *folder_type_repository);
EShortcuts *e_shortcuts_new        (EStorageSet           *storage_set,
				    EFolderTypeRepository *folder_type_repository);

GList       *e_shortcuts_get_group_titles        (EShortcuts *shortcuts);
GList       *e_shortcuts_get_shortcuts_in_group  (EShortcuts *shortcuts,
						  const char *group_title);

EStorageSet *e_shortcuts_get_storage_set         (EShortcuts *shortcuts);

GtkWidget *e_shortcuts_new_view  (EShortcuts *shortcuts);

gboolean  e_shortcuts_load  (EShortcuts *shortcuts,
			     const char *path);
gboolean  e_shortcuts_save  (EShortcuts *shortcuts,
			     const char *path);

const char *e_shortcuts_get_uri  (EShortcuts *shortcuts,
				  int         group_num,
				  int         num);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHORTCUTS_H_ */
