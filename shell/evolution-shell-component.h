/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component.h
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

#ifndef EVOLUTION_SHELL_COMPONENT_H
#define EVOLUTION_SHELL_COMPONENT_H

#include "Evolution.h"

#include "evolution-shell-client.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-control.h>

#ifdef cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define EVOLUTION_TYPE_SHELL_COMPONENT            (evolution_shell_component_get_type ())
#define EVOLUTION_SHELL_COMPONENT(obj)            (GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_SHELL_COMPONENT, EvolutionShellComponent))
#define EVOLUTION_SHELL_COMPONENT_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_SHELL_COMPONENT, EvolutionShellComponentClass))
#define EVOLUTION_IS_SHELL_COMPONENT(obj)         (GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_SHELL_COMPONENT))
#define EVOLUTION_IS_SHELL_COMPONENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_SHELL_COMPONENT))

#define EVOLUTION_SHELL_COMPONENT_POPUP_PLACEHOLDER "/popups/FolderPopup/ComponentPlaceholder/Items"


typedef struct _EvolutionShellComponent        EvolutionShellComponent;
typedef struct _EvolutionShellComponentPrivate EvolutionShellComponentPrivate;
typedef struct _EvolutionShellComponentClass   EvolutionShellComponentClass;

enum _EvolutionShellComponentResult {
	EVOLUTION_SHELL_COMPONENT_OK,
	EVOLUTION_SHELL_COMPONENT_CANCEL,
	EVOLUTION_SHELL_COMPONENT_CORBAERROR,
	EVOLUTION_SHELL_COMPONENT_INTERRUPTED,
	EVOLUTION_SHELL_COMPONENT_INVALIDARG,
	EVOLUTION_SHELL_COMPONENT_ALREADYOWNED,
	EVOLUTION_SHELL_COMPONENT_NOTOWNED,
	EVOLUTION_SHELL_COMPONENT_NOTFOUND,
	EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE,
	EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDSCHEMA,
	EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDOPERATION,
	EVOLUTION_SHELL_COMPONENT_INTERNALERROR,
	EVOLUTION_SHELL_COMPONENT_BUSY,
	EVOLUTION_SHELL_COMPONENT_EXISTS,
	EVOLUTION_SHELL_COMPONENT_INVALIDURI,
	EVOLUTION_SHELL_COMPONENT_PERMISSIONDENIED,
	EVOLUTION_SHELL_COMPONENT_HASSUBFOLDERS,
	EVOLUTION_SHELL_COMPONENT_NOSPACE,
	EVOLUTION_SHELL_COMPONENT_OLDOWNERHASDIED,
	EVOLUTION_SHELL_COMPONENT_UNKNOWNERROR
};
typedef enum _EvolutionShellComponentResult EvolutionShellComponentResult;

typedef EvolutionShellComponentResult (* EvolutionShellComponentCreateViewFn)
	                                               (EvolutionShellComponent *shell_component,
							const char *physical_uri,
							const char *type,
							const char *view_info,
							BonoboControl **control_return,
							void *closure);
typedef void (* EvolutionShellComponentCreateFolderFn) (EvolutionShellComponent *shell_component,
						        const char *physical_uri,
						        const char *type,
						        const GNOME_Evolution_ShellComponentListener listener,
						        void *closure);
typedef void (* EvolutionShellComponentRemoveFolderFn) (EvolutionShellComponent *shell_component,
						        const char *physical_uri,
							const char *type,
						        const GNOME_Evolution_ShellComponentListener listener,
						        void *closure);
typedef void (* EvolutionShellComponentXferFolderFn)   (EvolutionShellComponent *shell_component,
							const char *source_physical_uri,
							const char *destination_physical_uri,
							const char *type,
							gboolean remove_source,
						        const GNOME_Evolution_ShellComponentListener listener,
						        void *closure);
typedef void (* EvolutionShellComponentPopulateFolderContextMenuFn) (EvolutionShellComponent *shell_component,
								   BonoboUIComponent *uic,
								   const char *physical_uri,
								   const char *type,
								   void *closure);
typedef void (* EvolutionShellComponentUnpopulateFolderContextMenuFn) (EvolutionShellComponent *shell_component,
								       BonoboUIComponent *uic,
								       const char *physical_uri,
								       const char *type,
								       void *closure);
typedef char * (* EvolutionShellComponentGetDndSelectionFn) (EvolutionShellComponent *shell_component,
							     const char *physical_uri,
							     int type,
							     int *format_return,
							     const char **selection_return,
							     int *selection_length_return,
							     void *closure);

typedef gboolean (* EvolutionShellComponentRequestQuitFn) (EvolutionShellComponent *shell_component,
							   void *closure);

struct _EvolutionShellComponentFolderType {
	char *name;
	char *icon_name;
	char *display_name;
	char *description;

	gboolean user_creatable;

	/* The following are NULL-terminated arrays.  */
	char **accepted_dnd_types;
	char **exported_dnd_types;
};
typedef struct _EvolutionShellComponentFolderType EvolutionShellComponentFolderType;

struct _EvolutionShellComponent {
	BonoboObject parent;

	EvolutionShellComponentPrivate *priv;
};

struct _EvolutionShellComponentClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_ShellComponent__epv epv;

	/* Signals.  */

	void (* owner_set)   (EvolutionShellComponent *shell_component,
			      EvolutionShellClient *shell_client,
			      const char *evolution_homedir);
	void (* owner_unset) (EvolutionShellComponent *shell_component);
	void (* owner_died)  (EvolutionShellComponent *shell_component);

	void (* debug)       (EvolutionShellComponent *shell_component);

	void (* interactive)  (EvolutionShellComponent *shell_component,
			       gboolean is_interactive,
			       unsigned long new_view_xid);

	void (* handle_external_uri) (EvolutionShellComponent *shell_component,
				      const char *uri);

	void (* user_create_new_item) (EvolutionShellComponent *shell_component,
				       const char *id,
				       const char *parent_folder_physical_uri,
				       const char *parent_folder_type);

	void (* send_receive) (EvolutionShellComponent *shell_component,
			       gboolean show_dialog);
};


GtkType evolution_shell_component_get_type   (void);
void evolution_shell_component_construct  (EvolutionShellComponent                             *shell_component,
					   const EvolutionShellComponentFolderType              folder_types[],
					   const char                                          *external_uri_schemas[],
					   EvolutionShellComponentCreateViewFn                  create_view_fn,
					   EvolutionShellComponentCreateFolderFn                create_folder_fn,
					   EvolutionShellComponentRemoveFolderFn                remove_folder_fn,
					   EvolutionShellComponentXferFolderFn                  xfer_folder_fn,
					   EvolutionShellComponentPopulateFolderContextMenuFn   populate_folder_context_menu_fn,
					   EvolutionShellComponentUnpopulateFolderContextMenuFn unpopulate_folder_context_menu_fn,
					   EvolutionShellComponentGetDndSelectionFn             get_dnd_selection_fn,
					   EvolutionShellComponentRequestQuitFn                 request_quit_fn,
					   void                                                *closure);
EvolutionShellComponent *evolution_shell_component_new (const EvolutionShellComponentFolderType              folder_types[],
							const char                                          *external_uri_schemas[],
							EvolutionShellComponentCreateViewFn                  create_view_fn,
							EvolutionShellComponentCreateFolderFn                create_folder_fn,
							EvolutionShellComponentRemoveFolderFn                remove_folder_fn,
							EvolutionShellComponentXferFolderFn                  xfer_folder_fn,
							EvolutionShellComponentPopulateFolderContextMenuFn   populate_folder_context_menu_fn,
							EvolutionShellComponentUnpopulateFolderContextMenuFn unpopulate_folder_context_menu_fn,
							EvolutionShellComponentGetDndSelectionFn             get_dnd_selection_fn,
							EvolutionShellComponentRequestQuitFn                 request_quit_fn,
							void                                                *closure);
EvolutionShellClient    *evolution_shell_component_get_owner  (EvolutionShellComponent                            *shell_component);

gulong evolution_shell_component_get_parent_view_xid(EvolutionShellComponent                            *shell_component);

void  evolution_shell_component_add_user_creatable_item  (EvolutionShellComponent *shell_component,
							  const char              *id,
							  const char              *description,
							  const char              *menu_description,
							  const char              *tooltip,
							  const char     	  *folder_type,
							  char                     menu_shortcut,
							  GdkPixbuf               *icon);

const char *evolution_shell_component_result_to_string (EvolutionShellComponentResult result);

#ifdef cplusplus
}
#endif /* cplusplus */

#endif /* EVOLUTION_SHELL_COMPONENT_H */
