/*
 * e-shell.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_SHELL_H
#define E_SHELL_H

#include <libedataserver/libedataserver.h>
#include <libedataserverui/libedataserverui.h>

#include <e-util/e-util.h>

#include <shell/e-shell-common.h>
#include <shell/e-shell-backend.h>

/* Standard GObject macros */
#define E_TYPE_SHELL \
	(e_shell_get_type ())
#define E_SHELL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL, EShell))
#define E_SHELL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL, EShellClass))
#define E_IS_SHELL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL))
#define E_IS_SHELL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL))
#define E_SHELL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL, EShellClass))

G_BEGIN_DECLS

typedef struct _EShell EShell;
typedef struct _EShellClass EShellClass;
typedef struct _EShellPrivate EShellPrivate;

/**
 * EShellQuitReason:
 * @E_SHELL_QUIT_ACTION:
 *   @E_SHELL_WINDOW_ACTION_QUIT was activated.
 * @E_SHELL_QUIT_LAST_WINDOW:
 *   The last watched window has been destroyed.
 * @E_SHELL_QUIT_OPTION:
 *   The program was invoked with --quit.  Extensions will never
 *   see this value because they are not loaded when --quit is given.
 * @E_SHELL_QUIT_REMOTE_REQUEST:
 *   Another Evolution process requested we quit.
 * @E_SHELL_QUIT_SESSION_REQUEST:
 *   The desktop session requested we quit.
 *
 * These values are passed in the #EShell::quit-requested signal to
 * indicate why the shell is requesting to shut down.
 **/
typedef enum {
	E_SHELL_QUIT_ACTION,
	E_SHELL_QUIT_LAST_WINDOW,
	E_SHELL_QUIT_OPTION,
	E_SHELL_QUIT_REMOTE_REQUEST,
	E_SHELL_QUIT_SESSION_REQUEST
} EShellQuitReason;

/**
 * EShell:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShell {
	GtkApplication parent;
	EShellPrivate *priv;
};

struct _EShellClass {
	GtkApplicationClass parent_class;

	gboolean	(*handle_uri)		(EShell *shell,
						 const gchar *uri);
	void		(*prepare_for_offline)	(EShell *shell,
						 EActivity *activity);
	void		(*prepare_for_online)	(EShell *shell,
						 EActivity *activity);
	void		(*prepare_for_quit)	(EShell *shell,
						 EActivity *activity);
	void		(*quit_requested)	(EShell *shell,
						 EShellQuitReason reason);
	/*gboolean	(*view_uri)		(EShell *shell,
						 const gchar *uri);*/
};

GType		e_shell_get_type		(void);
EShell *	e_shell_get_default		(void);
void		e_shell_load_modules		(EShell *shell);
GList *		e_shell_get_shell_backends	(EShell *shell);
const gchar *	e_shell_get_canonical_name	(EShell *shell,
						 const gchar *name);
EShellBackend *	e_shell_get_backend_by_name	(EShell *shell,
						 const gchar *name);
EShellBackend *	e_shell_get_backend_by_scheme	(EShell *shell,
						 const gchar *scheme);
EClientCache *	e_shell_get_client_cache	(EShell *shell);
ESourceRegistry *
		e_shell_get_registry		(EShell *shell);
ECredentialsPrompter *
		e_shell_get_credentials_prompter(EShell *shell);
void		e_shell_allow_auth_prompt_for	(EShell *shell,
						 ESource *source);
GtkWidget *	e_shell_create_shell_window	(EShell *shell,
						 const gchar *view_name);
guint		e_shell_handle_uris		(EShell *shell,
						 const gchar * const *uris,
						 gboolean do_import,
						 gboolean do_view);
void		e_shell_submit_alert		(EShell *shell,
						 EAlert *alert);
GtkWindow *     e_shell_get_active_window	(EShell *shell);
gboolean	e_shell_get_express_mode	(EShell *shell);
const gchar *	e_shell_get_module_directory	(EShell *shell);
gboolean	e_shell_get_network_available	(EShell *shell);
void		e_shell_set_network_available	(EShell *shell,
						 gboolean network_available);
void		e_shell_lock_network_available	(EShell *shell);
gboolean	e_shell_get_online		(EShell *shell);
void		e_shell_set_online		(EShell *shell,
						 gboolean online);
GtkWidget *	e_shell_get_preferences_window	(EShell *shell);
void		e_shell_event			(EShell *shell,
						 const gchar *event_name,
						 gpointer event_data);
gboolean	e_shell_quit			(EShell *shell,
						 EShellQuitReason reason);
void		e_shell_cancel_quit		(EShell *shell);
void		e_shell_set_auth_prompt_parent	(EShell *shell,
						 ESource *source,
						 GtkWindow *parent);

G_END_DECLS

#endif /* E_SHELL_H */
