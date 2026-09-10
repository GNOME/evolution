/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <e-util/e-util.h>

#include "calendar/gui/e-cal-table-events.h"
#include "e-cal-shell-migrate.h"

gboolean
e_cal_shell_backend_migrate (EShellBackend *shell_backend,
                             gint major,
                             gint minor,
                             gint micro,
                             GError **error)
{
	const gchar * const *legacy_column_ids;
	guint n_legacy_column_ids;
	gchar *views_dir;

	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), FALSE);

	if (major <= 2 || (major == 3 && minor < 64)) {
		legacy_column_ids = e_cal_table_events_get_legacy_etable_column_ids (&n_legacy_column_ids);

		views_dir = g_build_filename (e_shell_backend_get_config_dir (shell_backend), "views", NULL);
		gal_view_virtual_tree_util_migrate_views_dir (views_dir, legacy_column_ids, n_legacy_column_ids, NULL, NULL);
		g_free (views_dir);
	}

	return TRUE;
}
