/*
 * e-book-shell-backend-migrate.c
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
 * Authors:
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <libebook/e-destination.h>
#include <libebook/e-book.h>
#include <glib/gi18n.h>

#include <libedataserver/e-xml-utils.h>

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "e-util/e-xml-utils.h"

#include "e-book-shell-migrate.h"

/*#define SLOW_MIGRATION*/

typedef struct {
	/* this hash table maps old folder uris to new uids.  It's
	   build in migrate_contact_folder and it's used in
	   migrate_completion_folders. */
	GHashTable *folder_uid_map;

	ESourceList *source_list;

	const gchar *data_dir;

	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *folder_label;
	GtkWidget *progress;
} MigrationContext;

#define LOCAL_BASE_URI "local:"
#define LDAP_BASE_URI "ldap://"
#define PERSONAL_RELATIVE_URI "system"

static void
create_groups (MigrationContext *context,
	       ESourceGroup **on_this_computer,
	       ESourceGroup **on_ldap_servers,
	       ESource      **personal_source)
{
	GSList *groups;
	ESourceGroup *group;

	*on_this_computer = NULL;
	*on_ldap_servers = NULL;
	*personal_source = NULL;

	groups = e_source_list_peek_groups (context->source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;
		gchar *base_dir, *base_uri;

		base_dir = g_build_filename (context->data_dir, "local", NULL);
		base_uri = g_filename_to_uri (base_dir, NULL, NULL);

		for (g = groups; g; g = g->next) {
			group = E_SOURCE_GROUP (g->data);

			if (strcmp (base_uri, e_source_group_peek_base_uri (group)) == 0)
				e_source_group_set_base_uri (group, LOCAL_BASE_URI);

			if (!*on_this_computer &&
					!strcmp (LOCAL_BASE_URI,
					e_source_group_peek_base_uri (group)))
				*on_this_computer = g_object_ref (group);
			else if (!*on_ldap_servers &&
					!strcmp (LDAP_BASE_URI,
					e_source_group_peek_base_uri (group)))
				*on_ldap_servers = g_object_ref (group);
		}

		g_free (base_dir);
		g_free (base_uri);
	}

	if (*on_this_computer) {
		/* make sure "Personal" shows up as a source under
		   this group */
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
	}
	else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), LOCAL_BASE_URI);
		e_source_list_add_group (context->source_list, group, -1);

		*on_this_computer = group;
	}

	if (!*personal_source) {
		/* Create the default Person addressbook */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (*on_this_computer, source, -1);

		e_source_set_property (source, "completion", "true");

		*personal_source = source;
	}

	if (!*on_ldap_servers) {
		/* Create the LDAP source group */
		group = e_source_group_new (_("On LDAP Servers"), LDAP_BASE_URI);
		e_source_list_add_group (context->source_list, group, -1);

		*on_ldap_servers = group;
	}
}

static MigrationContext *
migration_context_new (const gchar *data_dir)
{
	MigrationContext *context = g_new (MigrationContext, 1);

	/* set up the mapping from old uris to new uids */
	context->folder_uid_map = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	e_book_get_addressbooks (&context->source_list, NULL);

	context->data_dir = data_dir;

	return context;
}

static void
migration_context_free (MigrationContext *context)
{
	e_source_list_sync (context->source_list, NULL);

	g_hash_table_destroy (context->folder_uid_map);

	g_object_unref (context->source_list);

	g_free (context);
}

gboolean
e_book_shell_backend_migrate (EShellBackend *shell_backend,
                              gint major,
                              gint minor,
                              gint micro,
                              GError **error)
{
	ESourceGroup *on_this_computer;
	ESourceGroup *on_ldap_servers;
	ESource *personal_source;
	MigrationContext *context;
	const gchar *data_dir;

	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), FALSE);

	data_dir = e_shell_backend_get_data_dir (shell_backend);
	context = migration_context_new (data_dir);

	/* we call this unconditionally now - create_groups either
	   creates the groups/sources or it finds the necessary
	   groups/sources. */
	create_groups (context, &on_this_computer, &on_ldap_servers, &personal_source);

	if (on_this_computer)
		g_object_unref (on_this_computer);
	if (on_ldap_servers)
		g_object_unref (on_ldap_servers);
	if (personal_source)
		g_object_unref (personal_source);

	migration_context_free (context);

	return TRUE;
}
