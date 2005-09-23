/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-window.h
 *
 * Copyright (C) 2003  Ettore Perazzoli
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
				const char *component_id);

void        e_shell_window_switch_to_component        (EShellWindow *shell,
						       const char   *component_id);
const char *e_shell_window_peek_current_component_id  (EShellWindow *shell);

EShell            *e_shell_window_peek_shell                (EShellWindow *window);
BonoboUIComponent *e_shell_window_peek_bonobo_ui_component  (EShellWindow *window);
ESidebar          *e_shell_window_peek_sidebar              (EShellWindow *window);
GtkWidget         *e_shell_window_peek_statusbar            (EShellWindow *window);

void  e_shell_window_save_defaults  (EShellWindow *window);
void  e_shell_window_show_settings  (EShellWindow *window);

#endif /* _E_SHELL_WINDOW_H_ */
