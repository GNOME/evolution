/*
 * e-shell-window.h
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

#ifndef E_SHELL_WINDOW_H
#define E_SHELL_WINDOW_H

#include <e-util/e-util.h>
#include <shell/e-shell.h>

/* Standard GObject macros */
#define E_TYPE_SHELL_WINDOW \
	(e_shell_window_get_type ())
#define E_SHELL_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_WINDOW, EShellWindow))
#define E_SHELL_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_WINDOW, EShellWindowClass))
#define E_IS_SHELL_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_WINDOW))
#define E_IS_SHELL_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_SHELL_WINDOW))
#define E_SHELL_WINDOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_WINDOW, EShellWindowClass))

G_BEGIN_DECLS

/* Avoid including <e-shell-view.h>, because it includes us! */
struct _EShellView;

typedef struct _EShellWindow EShellWindow;
typedef struct _EShellWindowClass EShellWindowClass;
typedef struct _EShellWindowPrivate EShellWindowPrivate;

/**
 * EShellWindow:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShellWindow {
	GtkWindow parent;
	EShellWindowPrivate *priv;
};

struct _EShellWindowClass {
	GtkWindowClass parent_class;

	/* Signals */
	void		(*close_alert)		(EShellWindow *shell_window);
	void		(*shell_view_created)	(EShellWindow *shell_window,
						 struct _EShellView *shell_view);
};

GType		e_shell_window_get_type		(void);
GtkWidget *	e_shell_window_new		(EShell *shell,
						 gboolean safe_mode,
						 const gchar *geometry);
EShell *	e_shell_window_get_shell	(EShellWindow *shell_window);
gboolean	e_shell_window_is_main_instance	(EShellWindow *shell_window);
struct _EShellView *
		e_shell_window_get_shell_view	(EShellWindow *shell_window,
						 const gchar *view_name);
struct _EShellView *
		e_shell_window_peek_shell_view	(EShellWindow *shell_window,
						 const gchar *view_name);
EUIAction *	e_shell_window_get_shell_view_action
						(EShellWindow *shell_window,
						 const gchar *view_name);
GtkWidget *	e_shell_window_get_alert_bar	(EShellWindow *shell_window);
EFocusTracker *	e_shell_window_get_focus_tracker
						(EShellWindow *shell_window);
EUIAction *	e_shell_window_get_ui_action	(EShellWindow *shell_window,
						 const gchar *action_name);
EUIActionGroup *e_shell_window_get_ui_action_group
						(EShellWindow *shell_window,
						 const gchar *group_name);
const gchar *	e_shell_window_get_active_view	(EShellWindow *shell_window);
void		e_shell_window_set_active_view	(EShellWindow *shell_window,
						 const gchar *view_name);
gboolean	e_shell_window_get_safe_mode	(EShellWindow *shell_window);
void		e_shell_window_set_safe_mode	(EShellWindow *shell_window,
						 gboolean safe_mode);
void		e_shell_window_add_action_group (EShellWindow *shell_window,
						 const gchar *group_name);

/* Helper function to open clients from shell's client cache in a dedicated
   thread with a callback being called in the main thread */

typedef void (* EShellWindowConnetClientFunc)	(EShellWindow *shell_window,
						 EClient *client,
						 gpointer user_data);

void		e_shell_window_connect_client	(EShellWindow *shell_window,
						 ESource *source,
						 const gchar *extension_name,
						 EShellWindowConnetClientFunc connected_cb,
						 gpointer user_data,
						 GDestroyNotify destroy_user_data);

/* These should be called from the shell backend's window_created() handler. */

void		e_shell_window_register_new_item_actions
						(EShellWindow *shell_window,
						 const gchar *backend_name,
						 const EUIActionEntry *entries,
						 guint n_entries);
void		e_shell_window_register_new_source_actions
						(EShellWindow *shell_window,
						 const gchar *backend_name,
						 const EUIActionEntry *entries,
						 guint n_entries);

G_END_DECLS

#endif /* E_SHELL_WINDOW_H */
