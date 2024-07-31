/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#ifndef E_CAL_BASE_SHELL_BACKEND_H
#define E_CAL_BASE_SHELL_BACKEND_H

#include <shell/e-shell-backend.h>
#include <shell/e-shell-window.h>

/* Standard GObject macros */
#define E_TYPE_CAL_BASE_SHELL_BACKEND \
	(e_cal_base_shell_backend_get_type ())
#define E_CAL_BASE_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_BASE_SHELL_BACKEND, ECalBaseShellBackend))
#define E_CAL_BASE_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_BASE_SHELL_BACKEND, ECalBaseShellBackendClass))
#define E_IS_CAL_BASE_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_BASE_SHELL_BACKEND))
#define E_IS_CAL_BASE_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_BASE_SHELL_BACKEND))
#define E_CAL_BASE_SHELL_BACKEND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_BASE_SHELL_BACKEND, ECalBaseShellBackendClass))

G_BEGIN_DECLS

typedef struct _ECalBaseShellBackend ECalBaseShellBackend;
typedef struct _ECalBaseShellBackendClass ECalBaseShellBackendClass;
typedef struct _ECalBaseShellBackendPrivate ECalBaseShellBackendPrivate;

struct _ECalBaseShellBackend {
	EShellBackend parent;
	ECalBaseShellBackendPrivate *priv;
};

struct _ECalBaseShellBackendClass {
	EShellBackendClass parent_class;

	const EUIActionEntry *new_item_entries;
	guint new_item_n_entries;

	const EUIActionEntry *source_entries;
	guint source_n_entries;

	gboolean (* handle_uri) (EShellBackend *shell_backend,
				 const gchar *uri);
};

GType		e_cal_base_shell_backend_get_type	(void);

void		e_cal_base_shell_backend_util_new_source
							(EShellWindow *shell_window,
							 ECalClientSourceType source_type);

typedef void (* ECalBaseShellBackendHandleStartEndDatesFunc)
							(EShellBackend *shell_backend,
							 const GDate *start_date,
							 const GDate *end_date);
gboolean	e_cal_base_shell_backend_util_handle_uri
							(EShellBackend *shell_backend,
							 ECalClientSourceType source_type,
							 const gchar *uri,
							 ECalBaseShellBackendHandleStartEndDatesFunc handle_start_end_dates);

G_END_DECLS

#endif /* E_CAL_BASE_SHELL_BACKEND_H */
