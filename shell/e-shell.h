/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell.h
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

#ifndef _E_SHELL_H_
#define _E_SHELL_H_

#include <liboaf/liboaf.h>	/* For the registration stuff.  */
#include <bonobo/bonobo-xobject.h>
#include <bonobo-conf/bonobo-config-database.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _EShell        EShell;
typedef struct _EShellPrivate EShellPrivate;
typedef struct _EShellClass   EShellClass;

#include "Evolution.h"

#include "e-component-registry.h"
#include "e-shortcuts.h"
#include "e-shell-view.h"
#include "e-uri-schema-registry.h"
#include "e-shell-user-creatable-items-handler.h"
#include "e-local-storage.h"


#define E_TYPE_SHELL			(e_shell_get_type ())
#define E_SHELL(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHELL, EShell))
#define E_SHELL_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL, EShellClass))
#define E_IS_SHELL(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHELL))
#define E_IS_SHELL_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL))


enum _EShellLineStatus {
	E_SHELL_LINE_STATUS_ONLINE,
	E_SHELL_LINE_STATUS_GOING_OFFLINE,
	E_SHELL_LINE_STATUS_OFFLINE
};
typedef enum _EShellLineStatus EShellLineStatus;

enum _EShellStartupLineMode {
	E_SHELL_STARTUP_LINE_MODE_CONFIG,
	E_SHELL_STARTUP_LINE_MODE_ONLINE,
	E_SHELL_STARTUP_LINE_MODE_OFFLINE
};
typedef enum _EShellStartupLineMode EShellStartupLineMode;

struct _EShell {
	BonoboXObject parent;

	EShellPrivate *priv;
};

struct _EShellClass {
	BonoboXObjectClass parent_class;

	POA_GNOME_Evolution_Shell__epv epv;

	void (* no_views_left) (EShell *shell);
	void (* line_status_changed) (EShell *shell, EShellLineStatus status);
};


/* ID for registering the shell in the OAF name service.  */
#define E_SHELL_OAFIID  "OAFIID:GNOME_Evolution_Shell"

enum _EShellConstructResult {
	E_SHELL_CONSTRUCT_RESULT_OK,
	E_SHELL_CONSTRUCT_RESULT_INVALIDARG,
	E_SHELL_CONSTRUCT_RESULT_CANNOTREGISTER,
	E_SHELL_CONSTRUCT_RESULT_NOCONFIGDB,
	E_SHELL_CONSTRUCT_RESULT_GENERICERROR
};
typedef enum _EShellConstructResult EShellConstructResult;



GtkType                e_shell_get_type   (void);
EShellConstructResult  e_shell_construct  (EShell                *shell,
					   const char            *iid,
					   const char            *local_directory,
					   gboolean               show_splash,
					   EShellStartupLineMode  startup_line_mode);
EShell                *e_shell_new        (const char            *local_directory,
					   gboolean               show_splash,
					   EShellStartupLineMode  startup_line_mode,
					   EShellConstructResult *construct_result_return);

EShellView *e_shell_create_view                (EShell     *shell,
						const char *uri,
						EShellView *template_view);
EShellView *e_shell_create_view_from_settings  (EShell     *shell,
						const char *uri,
						EShellView  *template_view,
						int         view_num,
						gboolean   *settings_found);

const char          *e_shell_get_local_directory       (EShell          *shell);
EShortcuts          *e_shell_get_shortcuts             (EShell          *shell);
EStorageSet         *e_shell_get_storage_set           (EShell          *shell);
ELocalStorage       *e_shell_get_local_storage         (EShell          *shell);
EFolderTypeRegistry *e_shell_get_folder_type_registry  (EShell          *shell);
EUriSchemaRegistry  *e_shell_get_uri_schema_registry   (EShell          *shell);

gboolean             e_shell_save_settings             (EShell          *shell);
gboolean             e_shell_restore_from_settings     (EShell          *shell);

void                 e_shell_destroy_all_views         (EShell          *shell);

void                 e_shell_unregister_all            (EShell          *shell);
void                 e_shell_disconnect_db             (EShell          *shell);

void                 e_shell_component_maybe_crashed   (EShell          *shell,
							const char      *uri,
							const char      *type_name,
							EShellView      *shell_view);

EShellLineStatus  e_shell_get_line_status  (EShell     *shell);
void              e_shell_go_offline       (EShell     *shell,
					    EShellView *action_view);
void              e_shell_go_online        (EShell     *shell,
					    EShellView *action_view);

Bonobo_ConfigDatabase            e_shell_get_config_db                     (EShell *shell);
EComponentRegistry              *e_shell_get_component_registry            (EShell *shell);
EShellUserCreatableItemsHandler *e_shell_get_user_creatable_items_handler  (EShell *shell);


const char *e_shell_construct_result_to_string (EShellConstructResult result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHELL_H_ */
