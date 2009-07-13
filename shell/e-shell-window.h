/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_SHELL_WINDOW_H_
#define _E_SHELL_WINDOW_H_

#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-ui-component.h>
#include "e-sidebar.h"

#define E_TYPE_SHELL_WINDOW			(e_shell_window_get_type ())
#define E_SHELL_WINDOW(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SHELL_WINDOW, EShellWindow))
#define E_SHELL_WINDOW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_WINDOW, EShellWindowClass))
#define E_IS_SHELL_WINDOW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SHELL_WINDOW))
#define E_IS_SHELL_WINDOW_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_WINDOW))

typedef struct _EShellWindow        EShellWindow;
typedef struct _EShellWindowPrivate EShellWindowPrivate;
typedef struct _EShellWindowClass   EShellWindowClass;

struct _EShellWindow {
	BonoboWindow parent;

	EShellWindowPrivate *priv;
};

struct _EShellWindowClass {
	BonoboWindowClass parent_class;

	void (* component_changed) (EShellWindow *window);
};

#include "e-shell.h"

GType  e_shell_window_get_type  (void);

GtkWidget *e_shell_window_new  (EShell     *shell,
				const gchar *component_id);

void        e_shell_window_switch_to_component        (EShellWindow *shell,
						       const gchar   *component_id);
const gchar *e_shell_window_peek_current_component_id  (EShellWindow *shell);

EShell            *e_shell_window_peek_shell                (EShellWindow *window);
BonoboUIComponent *e_shell_window_peek_bonobo_ui_component  (EShellWindow *window);
ESidebar          *e_shell_window_peek_sidebar              (EShellWindow *window);
GtkWidget         *e_shell_window_peek_statusbar            (EShellWindow *window);

void e_shell_window_set_title(EShellWindow *window, const gchar *component_id, const gchar *title);

void  e_shell_window_save_defaults  (EShellWindow *window);
void  e_shell_window_show_settings  (EShellWindow *window);

void e_shell_window_change_component_button_icon (EShellWindow *window, const gchar *component_id, const gchar *icon_name);

#endif /* _E_SHELL_WINDOW_H_ */
