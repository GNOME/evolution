/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-folder-title-bar.h
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

#ifndef __E_SHELL_FOLDER_TITLE_BAR_H__
#define __E_SHELL_FOLDER_TITLE_BAR_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkeventbox.h>
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
	GtkEventBox parent;

	EShellFolderTitleBarPrivate *priv;
};

struct _EShellFolderTitleBarClass {
	GtkEventBoxClass parent_class;

	/* Signals.  */
	void  (* title_clicked)  (EShellFolderTitleBar *folder_title_bar);
};


GtkType    e_shell_folder_title_bar_get_type   (void);
void       e_shell_folder_title_bar_construct  (EShellFolderTitleBar *folder_title_bar);
GtkWidget *e_shell_folder_title_bar_new        (void);

void       e_shell_folder_title_bar_set_title  (EShellFolderTitleBar *folder_title_bar,
						const char           *title);
void       e_shell_folder_title_bar_set_icon   (EShellFolderTitleBar *folder_title_bar,
						const GdkPixbuf      *icon);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_SHELL_FOLDER_TITLE_BAR_H__ */
