/*
 * e-shell.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/**
 * SECTION: e-shell
 * @short_description: the backbone of Evolution
 * @include: shell/e-shell.h
 **/

#ifndef E_SHELL_H
#define E_SHELL_H

#include <unique/unique.h>
#include <gconf/gconf-client.h>
#include <shell/e-shell-common.h>
#include <shell/e-shell-backend.h>
#include <shell/e-shell-settings.h>

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
 * EShell:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShell {
	UniqueApp parent;
	EShellPrivate *priv;
};

struct _EShellClass {
	UniqueAppClass parent_class;
};

GType		e_shell_get_type		(void);
EShell *	e_shell_get_default		(void);
GList *		e_shell_get_shell_backends	(EShell *shell);
const gchar *	e_shell_get_canonical_name	(EShell *shell,
						 const gchar *name);
EShellBackend *	e_shell_get_backend_by_name	(EShell *shell,
						 const gchar *name);
EShellBackend *	e_shell_get_backend_by_scheme	(EShell *shell,
						 const gchar *scheme);
EShellSettings *e_shell_get_shell_settings	(EShell *shell);
GConfClient *	e_shell_get_gconf_client	(EShell *shell);
GtkWidget *	e_shell_create_shell_window	(EShell *shell,
						 const gchar *view_name);
guint		e_shell_handle_uris		(EShell *shell,
						 gchar **uris);
void		e_shell_watch_window		(EShell *shell,
						 GtkWindow *window);
GList *		e_shell_get_watched_windows	(EShell *shell);
void		e_shell_send_receive		(EShell *shell,
						 GtkWindow *parent);
gboolean	e_shell_get_network_available	(EShell *shell);
void		e_shell_set_network_available	(EShell *shell,
						 gboolean network_available);
gboolean	e_shell_get_online		(EShell *shell);
void		e_shell_set_online		(EShell *shell,
						 gboolean online);
GtkWidget *	e_shell_get_preferences_window	(EShell *shell);
void		e_shell_event			(EShell *shell,
						 const gchar *event_name,
						 gpointer event_data);
gboolean	e_shell_is_busy			(EShell *shell);
gboolean	e_shell_do_quit			(EShell *shell);
gboolean	e_shell_quit			(EShell *shell);

G_END_DECLS

#endif /* E_SHELL_H */
