/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shortcuts.h
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifndef _E_SHORTCUTS_H_
#define _E_SHORTCUTS_H_

#include <gtk/gtkwidget.h>

#include "e-folder-type-registry.h"
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

struct _EShortcutItem {
	/* URI of the shortcut.  */
	char *uri;

	/* Name of the shortcut.  */
	char *name;

	/* Folder type for the shortcut.  If the shortcut doesn't point to a
	   folder, this is NULL.  */
	char *type;

	/* Custom icon for the shortcut.  If this is NULL, then the shortcut
	   should just use the icon for the type.  */
	char *custom_icon_name;

	/* Number of unread items in the folder.  Zero if not a folder.  */
	int unread_count;
};
typedef struct _EShortcutItem EShortcutItem;

struct _EShortcuts {
	GtkObject parent;

	EShortcutsPrivate *priv;
};

struct _EShortcutsClass {
	GtkObjectClass parent_class;

	/* Signals.  */

	void  (* new_group)               (EShortcuts *shortcuts, int group_num);
	void  (* remove_group)     	  (EShortcuts *shortcuts, int group_num);
	void  (* rename_group)     	  (EShortcuts *shortcuts, int group_num, const char *new_title);

	void  (* group_change_icon_size)  (EShortcuts *shortcuts, int group_num, gboolean use_small_icons);

	void  (* new_shortcut)     	  (EShortcuts *shortcuts, int group_num, int item_num);
	void  (* remove_shortcut)  	  (EShortcuts *shortcuts, int group_num, int item_num);
	void  (* update_shortcut)  	  (EShortcuts *shortcuts, int group_num, int item_num);
};


#include "e-shell.h"


GtkType     e_shortcuts_get_type       (void);
void        e_shortcuts_construct      (EShortcuts *shortcuts,
					EShell     *shell);
EShortcuts *e_shortcuts_new_from_file  (EShell     *shell,
					const char *file_name);

int           e_shortcuts_get_num_groups          (EShortcuts *shortcuts);

GSList       *e_shortcuts_get_group_titles        (EShortcuts *shortcuts);
const char   *e_shortcuts_get_group_title         (EShortcuts *shortcuts,
						   int         group_num);
const GSList *e_shortcuts_get_shortcuts_in_group  (EShortcuts *shortcuts,
						   int         group_num);

const EShortcutItem *e_shortcuts_get_shortcut  (EShortcuts *shortcuts,
						int         group_num,
						int         num);

EShell *e_shortcuts_get_shell  (EShortcuts *shortcuts);

GtkWidget *e_shortcuts_new_view  (EShortcuts *shortcuts);

gboolean     e_shortcuts_load                    (EShortcuts          *shortcuts,
						  const char          *path);
gboolean     e_shortcuts_save                    (EShortcuts          *shortcuts,
						  const char          *path);

void  e_shortcuts_add_default_group  (EShortcuts *shortcuts);

void  e_shortcuts_remove_shortcut    (EShortcuts *shortcuts,
				      int         group_num,
				      int         num);
void  e_shortcuts_add_shortcut       (EShortcuts *shortcuts,
				      int         group_num,
				      int         num,
				      const char *uri,
				      const char *name,
				      int unread_count,
				      const char *type,
				      const char *custom_icon_name);
void  e_shortcuts_update_shortcut    (EShortcuts *shortcuts,
				      int         group_num,
				      int         num,
				      const char *uri,
				      const char *name,
				      int unread_count,
				      const char *type,
				      const char *custom_icon_name);

void  e_shortcuts_remove_group  (EShortcuts *shortcuts,
				 int         group_num);
void  e_shortcuts_add_group     (EShortcuts *shortcuts,
				 int         group_num,
				 const char *group_title);
void  e_shortcuts_rename_group  (EShortcuts *shortcuts,
				 int         group_num,
				 const char *new_title);

void      e_shortcuts_set_group_uses_small_icons  (EShortcuts *shortcuts,
						   int         group_num,
						   gboolean    use_small_icons);
gboolean  e_shortcuts_get_group_uses_small_icons  (EShortcuts *shortcuts,
						   int         group_num);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHORTCUTS_H_ */
