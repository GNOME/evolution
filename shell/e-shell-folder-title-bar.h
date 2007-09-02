/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-folder-title-bar.h
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef __E_SHELL_FOLDER_TITLE_BAR_H__
#define __E_SHELL_FOLDER_TITLE_BAR_H__

#include <gtk/gtkhbox.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SHELL_FOLDER_TITLE_BAR			(e_shell_folder_title_bar_get_type ())
#define E_SHELL_FOLDER_TITLE_BAR(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHELL_FOLDER_TITLE_BAR, EShellFolderTitleBar))
#define E_SHELL_FOLDER_TITLE_BAR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_FOLDER_TITLE_BAR, EShellFolderTitleBarClass))
#define E_IS_SHELL_FOLDER_TITLE_BAR(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_SHELL_FOLDER_TITLE_BAR))
#define E_IS_SHELL_FOLDER_TITLE_BAR_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_FOLDER_TITLE_BAR))


typedef struct _EShellFolderTitleBar        EShellFolderTitleBar;
typedef struct _EShellFolderTitleBarPrivate EShellFolderTitleBarPrivate;
typedef struct _EShellFolderTitleBarClass   EShellFolderTitleBarClass;

struct _EShellFolderTitleBar {
	GtkHBox parent;

	EShellFolderTitleBarPrivate *priv;
};

struct _EShellFolderTitleBarClass {
	GtkHBoxClass parent_class;

	/* Signals.  */
	void  (* title_toggled)   (EShellFolderTitleBar *folder_title_bar, gboolean pressed);
	void  (* back_clicked)    (EShellFolderTitleBar *folder_title_bar);
	void  (* forward_clicked) (EShellFolderTitleBar *folder_title_bar);
};


GtkType    e_shell_folder_title_bar_get_type          (void);
void       e_shell_folder_title_bar_construct         (EShellFolderTitleBar *folder_title_bar);
GtkWidget *e_shell_folder_title_bar_new               (void);

void  e_shell_folder_title_bar_set_title             (EShellFolderTitleBar *folder_title_bar,
						      const char           *title);
void  e_shell_folder_title_bar_set_folder_bar_label  (EShellFolderTitleBar *folder_title_bar,
						      const char           *folder_bar_label);
void  e_shell_folder_title_bar_set_icon              (EShellFolderTitleBar *folder_title_bar,
						      GdkPixbuf            *icon);
void  e_shell_folder_title_bar_set_toggle_state      (EShellFolderTitleBar *folder_title_bar,
						      gboolean              state);
void  e_shell_folder_title_bar_set_title_clickable   (EShellFolderTitleBar *folder_title_bar,
						      gboolean              clickable);

void  e_shell_folder_title_bar_update_navigation_buttons  (EShellFolderTitleBar *folder_title_bar,
							   gboolean              can_go_back,
							   gboolean              can_go_forward);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_SHELL_FOLDER_TITLE_BAR_H__ */
