/*
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

#ifndef _E_SHELL_H_
#define _E_SHELL_H_

#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-object.h>

G_BEGIN_DECLS

typedef struct _EShell        EShell;
typedef struct _EShellPrivate EShellPrivate;
typedef struct _EShellClass   EShellClass;

#include "Evolution.h"

#include "e-component-registry.h"
#include "e-shell-window.h"

#define E_TYPE_SHELL			(e_shell_get_type ())
#define E_SHELL(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SHELL, EShell))
#define E_SHELL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL, EShellClass))
#define E_IS_SHELL(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SHELL))
#define E_IS_SHELL_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL))

enum _EShellLineStatus {
	E_SHELL_LINE_STATUS_ONLINE,
	E_SHELL_LINE_STATUS_GOING_OFFLINE, /* NB: really means changing state in either direction */
	E_SHELL_LINE_STATUS_OFFLINE,
	E_SHELL_LINE_STATUS_FORCED_OFFLINE
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

GType                  e_shell_get_type   (void);
EShellConstructResult  e_shell_construct  (EShell                *shell,
					   const gchar            *iid,
					   EShellStartupLineMode  startup_line_mode);
EShell                *e_shell_new        (EShellStartupLineMode  startup_line_mode,
					   EShellConstructResult *construct_result_return);

gboolean  e_shell_attempt_upgrade  (EShell     *shell);

EShellWindow *e_shell_create_window         (EShell       *shell,
					     const gchar   *component_id,
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
void              e_shell_set_line_status  (EShell       *shell,
                                            GNOME_Evolution_ShellState shell_state);

gboolean	e_shell_get_crash_recovery	(EShell *shell);
void		e_shell_set_crash_recovery	(EShell *shell,
						 gboolean crash_recovery);

void  e_shell_send_receive  (EShell *shell);

void  e_shell_show_settings  (EShell       *shell,
			      const gchar   *type,
			      EShellWindow *shell_window);

gboolean e_shell_can_quit (EShell *shell);
gboolean e_shell_do_quit  (EShell *shell);
gboolean e_shell_quit     (EShell *shell);

const gchar *e_shell_construct_result_to_string (EShellConstructResult result);

typedef gboolean (*EMainShellFunc) (EShell *shell, EShellWindow *window, gpointer user_data);
void e_shell_foreach_shell_window (EShell *shell, EMainShellFunc func, gpointer user_data);

G_END_DECLS

#endif /* _E_SHELL_H_ */
