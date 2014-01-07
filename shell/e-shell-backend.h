/*
 * e-shell-backend.h
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

#ifndef E_SHELL_BACKEND_H
#define E_SHELL_BACKEND_H

#include <libebackend/libebackend.h>

#include <shell/e-shell-common.h>
#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_SHELL_BACKEND \
	(e_shell_backend_get_type ())
#define E_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_BACKEND, EShellBackend))
#define E_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_BACKEND, EShellBackendClass))
#define E_IS_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_BACKEND))
#define E_IS_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL_BACKEND))
#define E_SHELL_BACKEND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_BACKEND, EShellBackendClass))

G_BEGIN_DECLS

/* Avoid including <e-shell.h>, because it includes us! */
struct _EShell;

typedef struct _EShellBackend EShellBackend;
typedef struct _EShellBackendClass EShellBackendClass;
typedef struct _EShellBackendPrivate EShellBackendPrivate;

/**
 * EShellBackend:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShellBackend {
	EExtension parent;
	EShellBackendPrivate *priv;
};

/**
 * EShellBackendClass:
 * @parent_class:	The parent class structure.
 * @name:		The name of the backend.  Also becomes the name of
 *			the corresponding #EShellView subclass that the
 *			backend will register.
 * @aliases:		Colon-separated list of aliases that can be used
 *			when referring to a backend by name.
 * @schemes:		Colon-separated list of URI schemes.  The #EShell
 *			will forward command-line URIs to the appropriate
 *			backend based on this list.
 * @sort_order:		Used to determine the order of backends listed in
 *			the main menu and in the switcher.  See
 *			e_shell_backend_compare().
 * @shell_view_type:	#GType for the corresponding #EShellView subclass.
 * @start:		Method for notifying the backend to begin loading
 *			data and running background tasks.  This is called
 *			just before the first instantiation of the
 *			corresponding #EShellView subclass.  It allows the
 *			backend to delay initialization steps that consume
 *			significant resources until they are actually needed.
 * @migrate:		Method for notifying the backend to migrate data and
 *			settings from the given version.  Returns %TRUE if the
 *			migration was successful or if no action was necessary.
 *			Returns %FALSE and sets a #GError if the migration
 *			failed.
 *
 * #EShellBackendClass contains a number of important settings for subclasses.
 **/
struct _EShellBackendClass {
	EExtensionClass parent_class;

	GType shell_view_type;

	const gchar *name;
	const gchar *aliases;
	const gchar *schemes;
	gint sort_order;
	const gchar *preferences_page;

	/* Methods */
	void		(*start)		(EShellBackend *shell_backend);
	gboolean	(*migrate)		(EShellBackend *shell_backend,
						 gint major,
						 gint minor,
						 gint micro,
						 GError **error);
	const gchar *	(*get_config_dir)	(EShellBackend *shell_backend);
	const gchar *	(*get_data_dir)		(EShellBackend *shell_backend);
};

GType		e_shell_backend_get_type	(void);
gint		e_shell_backend_compare		(EShellBackend *shell_backend_a,
						 EShellBackend *shell_backend_b);
const gchar *	e_shell_backend_get_config_dir	(EShellBackend *shell_backend);
const gchar *	e_shell_backend_get_data_dir	(EShellBackend *shell_backend);
struct _EShell *e_shell_backend_get_shell	(EShellBackend *shell_backend);
void		e_shell_backend_add_activity	(EShellBackend *shell_backend,
						 EActivity *activity);
gboolean	e_shell_backend_is_busy		(EShellBackend *shell_backend);
void		e_shell_backend_set_prefer_new_item
						(EShellBackend *shell_backend,
						 const gchar *prefer_new_item);
const gchar *	e_shell_backend_get_prefer_new_item
						(EShellBackend *shell_backend);
void		e_shell_backend_cancel_all	(EShellBackend *shell_backend);
void		e_shell_backend_start		(EShellBackend *shell_backend);
gboolean	e_shell_backend_is_started	(EShellBackend *shell_backend);
gboolean	e_shell_backend_migrate		(EShellBackend *shell_backend,
						 gint major,
						 gint minor,
						 gint micro,
						 GError **error);

G_END_DECLS

#endif /* E_SHELL_BACKEND_H */
