/*
 * e-memo-shell-migrate.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-memo-shell-migrate.h"

#include <string.h>
#include <glib/gi18n.h>
#include <camel/camel.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-group.h>
#include <libedataserver/e-source-list.h>

#include "e-util/e-account-utils.h"
#include "calendar/gui/calendar-config-keys.h"
#include "shell/e-shell.h"

#include "e-memo-shell-backend.h"

#define LOCAL_BASE_URI "local:"
#define WEBCAL_BASE_URI "webcal://"
#define PERSONAL_RELATIVE_URI "system"
#define GROUPWISE_BASE_URI "groupwise://"

static void
create_memo_sources (EShellBackend *shell_backend,
                     ESourceList *source_list,
                     ESourceGroup **on_this_computer,
                     ESourceGroup **on_the_web,
                     ESource **personal_source)
{
	EShell *shell;
	EShellSettings *shell_settings;
	GSList *groups;
	ESourceGroup *group;

	*on_this_computer = NULL;
	*on_the_web = NULL;
	*personal_source = NULL;

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	groups = e_source_list_peek_groups (source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;
		gchar *base_dir, *base_uri;

		base_dir = g_build_filename (e_shell_backend_get_data_dir (shell_backend), "local", NULL);
		base_uri = g_filename_to_uri (base_dir, NULL, NULL);

		for (g = groups; g; g = g->next) {
			group = E_SOURCE_GROUP (g->data);

			if (strcmp (base_uri, e_source_group_peek_base_uri (group)) == 0)
				e_source_group_set_base_uri (group, LOCAL_BASE_URI);

			if (!*on_this_computer && !strcmp (LOCAL_BASE_URI, e_source_group_peek_base_uri (group)))
				*on_this_computer = g_object_ref (group);
			else if (!*on_the_web && !strcmp (WEBCAL_BASE_URI, e_source_group_peek_base_uri (group)))
				*on_the_web = g_object_ref (group);
		}

		g_free (base_dir);
		g_free (base_uri);
	}

	if (*on_this_computer) {
		/* make sure "Personal" shows up as a source under
		 * this group */
		GSList *sources = e_source_group_peek_sources (*on_this_computer);
		GSList *s;
		for (s = sources; s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			const gchar *relative_uri;

			relative_uri = e_source_peek_relative_uri (source);
			if (relative_uri == NULL)
				continue;
			if (!strcmp (PERSONAL_RELATIVE_URI, relative_uri)) {
				*personal_source = g_object_ref (source);
				break;
			}
		}
	} else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), LOCAL_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		*on_this_computer = group;
	}

	if (!*personal_source) {
		GSList *selected;
		gchar *primary_memo_list;

		/* Create the default Person memo list */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (*on_this_computer, source, -1);

		primary_memo_list = e_shell_settings_get_string (
			shell_settings, "cal-primary-memo-list");

		selected = e_memo_shell_backend_get_selected_memo_lists (
			E_MEMO_SHELL_BACKEND (shell_backend));

		if (primary_memo_list == NULL && selected == NULL) {
			GSList link;

			e_shell_settings_set_string (
				shell_settings, "cal-primary-memo-list",
				e_source_peek_uid (source));

			link.data = (gpointer) e_source_peek_uid (source);
			link.next = NULL;

			e_memo_shell_backend_set_selected_memo_lists (
				E_MEMO_SHELL_BACKEND (shell_backend), &link);
		}

		g_slist_foreach (selected, (GFunc) g_free, NULL);
		g_slist_free (selected);

		e_source_set_color_spec (source, "#BECEDD");
		*personal_source = source;
	}

	if (!*on_the_web) {
		/* Create the Webcal source group */
		group = e_source_group_new (_("On The Web"), WEBCAL_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		*on_the_web = group;
	}
}

gboolean
e_memo_shell_backend_migrate (EShellBackend *shell_backend,
                              gint major,
                              gint minor,
                              gint revision,
                              GError **error)
{
	ESourceGroup *on_this_computer = NULL;
	ESourceGroup *on_the_web = NULL;
	ESource *personal_source = NULL;
	ESourceList *source_list = NULL;
	gboolean retval = FALSE;

	g_object_get (shell_backend, "source-list", &source_list, NULL);

	/* we call this unconditionally now - create_groups either
	 * creates the groups/sources or it finds the necessary
	 * groups/sources. */
	create_memo_sources (
		shell_backend, source_list, &on_this_computer,
		&on_the_web, &personal_source);

	e_source_list_sync (source_list, NULL);
	retval = TRUE;

	if (on_this_computer)
		g_object_unref (on_this_computer);
	if (on_the_web)
		g_object_unref (on_the_web);
	if (personal_source)
		g_object_unref (personal_source);

	return retval;
}
