/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component.h
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

#ifndef EVOLUTION_SHELL_COMPONENT_H
#define EVOLUTION_SHELL_COMPONENT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-control.h>

#include "Evolution.h"

#ifdef cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define EVOLUTION_TYPE_SHELL_COMPONENT            (evolution_shell_component_get_type ())
#define EVOLUTION_SHELL_COMPONENT(obj)            (GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_SHELL_COMPONENT, EvolutionShellComponent))
#define EVOLUTION_SHELL_COMPONENT_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_SHELL_COMPONENT, EvolutionShellComponentClass))
#define EVOLUTION_IS_SHELL_COMPONENT(obj)         (GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_SHELL_COMPONENT))
#define EVOLUTION_IS_SHELL_COMPONENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_SHELL_COMPONENT))


typedef struct _EvolutionShellComponent        EvolutionShellComponent;
typedef struct _EvolutionShellComponentPrivate EvolutionShellComponentPrivate;
typedef struct _EvolutionShellComponentClass   EvolutionShellComponentClass;

enum _EvolutionShellComponentResult {
	EVOLUTION_SHELL_COMPONENT_OK,
	EVOLUTION_SHELL_COMPONENT_CORBAERROR,
	EVOLUTION_SHELL_COMPONENT_INTERRUPTED,
	EVOLUTION_SHELL_COMPONENT_INVALIDARG,
	EVOLUTION_SHELL_COMPONENT_ALREADYOWNED,
	EVOLUTION_SHELL_COMPONENT_NOTOWNED,
	EVOLUTION_SHELL_COMPONENT_NOTFOUND,
	EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE,
	EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDOPERATION,
	EVOLUTION_SHELL_COMPONENT_INTERNALERROR,
	EVOLUTION_SHELL_COMPONENT_BUSY,
	EVOLUTION_SHELL_COMPONENT_EXISTS,
	EVOLUTION_SHELL_COMPONENT_INVALIDURI,
	EVOLUTION_SHELL_COMPONENT_PERMISSIONDENIED,
	EVOLUTION_SHELL_COMPONENT_HASSUBFOLDERS,
	EVOLUTION_SHELL_COMPONENT_NOSPACE,
	EVOLUTION_SHELL_COMPONENT_UNKNOWNERROR
};
typedef enum _EvolutionShellComponentResult EvolutionShellComponentResult;

typedef EvolutionShellComponentResult (* EvolutionShellComponentCreateViewFn)
	                                               (EvolutionShellComponent *shell_component,
							const char *physical_uri,
							const char *type,
							BonoboControl **control_return,
							void *closure);
typedef void (* EvolutionShellComponentCreateFolderFn) (EvolutionShellComponent *shell_component,
						        const char *physical_uri,
						        const char *type,
						        const Evolution_ShellComponentListener listener,
						        void *closure);
typedef void (* EvolutionShellComponentRemoveFolderFn) (EvolutionShellComponent *shell_component,
						        const char *physical_uri,
						        const Evolution_ShellComponentListener listener,
						        void *closure);

struct _EvolutionShellComponentFolderType {
	char *name;
	char *icon_name;
};
typedef struct _EvolutionShellComponentFolderType EvolutionShellComponentFolderType;

struct _EvolutionShellComponent {
	BonoboObject parent;

	EvolutionShellComponentPrivate *priv;
};

struct _EvolutionShellComponentClass {
	BonoboObjectClass parent_class;

	/* Signals.  */

	void (* owner_set)   (EvolutionShellComponent *shell_component,
			      Evolution_Shell shell_interface);
	void (* owner_unset) (EvolutionShellComponent *shell_component);
};


GtkType                  evolution_shell_component_get_type   (void);
void                     evolution_shell_component_construct  (EvolutionShellComponent                 *shell_component,
							       const EvolutionShellComponentFolderType  folder_types[],
							       Evolution_ShellComponent                 corba_object,
							       EvolutionShellComponentCreateViewFn      create_view_fn,
							       EvolutionShellComponentCreateFolderFn    create_folder_fn,
							       EvolutionShellComponentRemoveFolderFn    remove_folder_fn,
							       void                                    *closure);
EvolutionShellComponent *evolution_shell_component_new        (const EvolutionShellComponentFolderType  folder_types[],
							       EvolutionShellComponentCreateViewFn      create_view_fn,
							       EvolutionShellComponentCreateFolderFn    create_folder_fn,
							       EvolutionShellComponentRemoveFolderFn    remove_folder_fn,
							       void                                    *closure);
Evolution_Shell          evolution_shell_component_get_owner  (EvolutionShellComponent                 *shell_component);

#ifdef cplusplus
}
#endif /* cplusplus */

#endif /* EVOLUTION_SHELL_COMPONENT_H */
