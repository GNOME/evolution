/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell.h
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

#ifndef _E_SHELL_H_
#define _E_SHELL_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _EShell        EShell;
typedef struct _EShellPrivate EShellPrivate;
typedef struct _EShellClass   EShellClass;

#include "Evolution.h"
#include "e-shortcuts.h"
#include "e-shell-view.h"

#define E_TYPE_SHELL			(e_shell_get_type ())
#define E_SHELL(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHELL, EShell))
#define E_SHELL_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL, EShellClass))
#define E_IS_SHELL(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHELL))
#define E_IS_SHELL_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL))



struct _EShell {
	BonoboObject parent;

	EShellPrivate *priv;
};

struct _EShellClass {
	BonoboObjectClass parent_class;

	void (* no_views_left) (EShell *shell);
};


GtkType              e_shell_get_type                  (void);
void                 e_shell_construct                 (EShell          *shell,
							GNOME_Evolution_Shell  corba_object,
							const char      *local_directory,
							gboolean         show_splash);

EShell              *e_shell_new                       (const char      *local_directory,
							gboolean         show_splash);
EShellView          *e_shell_new_view                  (EShell          *shell,
							const char      *uri);

const char          *e_shell_get_local_directory       (EShell          *shell);
EShortcuts          *e_shell_get_shortcuts             (EShell          *shell);
EStorageSet         *e_shell_get_storage_set           (EShell          *shell);
EFolderTypeRegistry *e_shell_get_folder_type_registry  (EShell          *shell);

gboolean             e_shell_save_settings             (EShell          *shell);
gboolean             e_shell_restore_from_settings     (EShell          *shell);

void                 e_shell_quit                      (EShell          *shell);

void                 e_shell_component_maybe_crashed   (EShell          *shell,
							const char      *uri,
							const char      *type_name,
							EShellView      *shell_view);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHELL_H_ */
