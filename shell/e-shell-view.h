/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-view.h
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef _E_SHELL_VIEW_H_
#define _E_SHELL_VIEW_H_

#include "e-task-bar.h"

#include <bonobo/bonobo-win.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define E_TYPE_SHELL_VIEW			(e_shell_view_get_type ())
#define E_SHELL_VIEW(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHELL_VIEW, EShellView))
#define E_SHELL_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_VIEW, EShellViewClass))
#define E_IS_SHELL_VIEW(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHELL_VIEW))
#define E_IS_SHELL_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_VIEW))

typedef struct _EShellView        EShellView;
typedef struct _EShellViewPrivate EShellViewPrivate;
typedef struct _EShellViewClass   EShellViewClass;

#include "e-shell.h"

#define E_SHELL_VIEW_DEFAULT_URI "evolution:/summary"

struct _EShellView {
	BonoboWindow parent;

	EShellViewPrivate *priv;
};

struct _EShellViewClass {
	BonoboWindowClass parent_class;

	/* Signals.  */

	void (* shortcut_bar_visibility_changed) (EShellView *shell_view,
						  gboolean visible);
	void (* folder_bar_visibility_changed)   (EShellView *shell_view,
						  gboolean visible);

	void (* view_changed) (EShellView *shell_view,
			       const char *evolution_path,
			       const char *physical_uri,
			       const char *folder_type,
			       const char *component_id);
};


/* WARNING: Don't use `e_shell_view_new()' to create new views for the shell
   unless you know what you are doing; this is just the standard GTK+
   constructor thing and it won't allow the shell to do the required
   bookkeeping for the created views.  Instead, the right way to create a new
   view is calling `e_shell_new_view()'.  */

GtkType     e_shell_view_get_type   (void);
EShellView *e_shell_view_construct  (EShellView *shell_view,
				     EShell     *shell);
EShellView *e_shell_view_new        (EShell     *shell);

const GNOME_Evolution_ShellView  e_shell_view_get_corba_interface  (EShellView *view);

gboolean  e_shell_view_display_uri  (EShellView *shell_view,
				     const char *uri);

void      e_shell_view_show_shortcut_bar   (EShellView *shell_view,
					    gboolean    show);
gboolean  e_shell_view_shortcut_bar_shown  (EShellView *shell_view);
void      e_shell_view_show_folder_bar     (EShellView *shell_view,
					    gboolean    show);
gboolean  e_shell_view_folder_bar_shown    (EShellView *shell_view);

void e_shell_view_show_settings (EShellView *shell_view);

ETaskBar          *e_shell_view_get_task_bar               (EShellView *shell_view);
EShell            *e_shell_view_get_shell                  (EShellView *shell_view);
BonoboUIComponent *e_shell_view_get_bonobo_ui_component    (EShellView *shell_view);
BonoboUIContainer *e_shell_view_get_bonobo_ui_container    (EShellView *shell_view);
GtkWidget         *e_shell_view_get_appbar                 (EShellView *shell_view);
const char        *e_shell_view_get_current_uri            (EShellView *shell_view);
const char        *e_shell_view_get_current_physical_uri   (EShellView *shell_view);
const char        *e_shell_view_get_current_folder_type    (EShellView *shell_view);
const char        *e_shell_view_get_current_component_id   (EShellView *shell_view);
const char        *e_shell_view_get_current_path           (EShellView *shell_view);

gboolean  e_shell_view_save_settings  (EShellView *shell_view,
				       int view_num);
gboolean  e_shell_view_load_settings  (EShellView *shell_view,
				       int view_num);

int   e_shell_view_get_current_shortcuts_group_num  (EShellView *shell_view);
void  e_shell_view_set_current_shortcuts_group_num  (EShellView *shell_view,
						     int         group_num);

/* Private -- */
const char *e_shell_view_get_folder_bar_right_click_path (EShellView *shell_view);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHELL_VIEW_H_ */
