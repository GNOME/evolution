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

#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _EShell        EShell;
typedef struct _EShellPrivate EShellPrivate;
typedef struct _EShellClass   EShellClass;

#include "Evolution.h"

#include "e-component-registry.h"
#include "e-shell-window.h"


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
	BonoboObject parent;

	EShellPrivate *priv;
};

struct _EShellClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Shell__epv epv;

	void (* no_windows_left) (EShell *shell);
	void (* line_status_changed) (EShell *shell, EShellLineStatus status);
	void (* new_window_created) (EShell *shell, EShellWindow *window);
};


/* ID for registering the shell in the OAF name service.  */
#define E_SHELL_OAFIID  "OAFIID:GNOME_Evolution_Shell:" BASE_VERSION

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
					   EShellStartupLineMode  startup_line_mode);
EShell                *e_shell_new        (EShellStartupLineMode  startup_line_mode,
					   EShellConstructResult *construct_result_return);

gboolean  e_shell_attempt_upgrade  (EShell     *shell);

EShellWindow *e_shell_create_window         (EShell       *shell,
					     const char   *component_id,
					     EShellWindow *template_window);
gboolean      e_shell_request_close_window  (EShell       *shell,
					     EShellWindow *window);


#if 0
EUriSchemaRegistry *e_shell_peek_uri_schema_registry  (EShell *shell);
#endif

EComponentRegistry *e_shell_peek_component_registry   (EShell *shell);

gboolean            e_shell_save_settings            (EShell *shell);
void                e_shell_close_all_windows        (EShell *shell);

EShellLineStatus  e_shell_get_line_status  (EShell       *shell);
void              e_shell_go_offline       (EShell       *shell,
					    EShellWindow *action_window);
void              e_shell_go_online        (EShell       *shell,
					    EShellWindow *action_window);

void  e_shell_send_receive  (EShell *shell);

void  e_shell_show_settings  (EShell       *shell,
			      const char   *type,
			      EShellWindow *shell_window);

gboolean e_shell_quit (EShell *shell);

const char *e_shell_construct_result_to_string (EShellConstructResult result);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHELL_H_ */
