/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* killev.c
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>

typedef struct {
	char *location;
	GPtrArray *names;
} KillevComponent;

GSList *languages;
GHashTable *components;

static gboolean
kill_process (const char *proc_name, KillevComponent *comp)
{
	int status, i;
	char *command;
	GString *desc;

	command = g_strdup_printf (KILL_PROCESS_CMD " -0 %s 2> /dev/null",
				   proc_name);
	status = system (command);
	g_free (command);

	if (status == -1 || !WIFEXITED (status)) {
		/* This most likely means that KILL_PROCESS_CMD wasn't
		 * found, so just bail completely.
		 */
		fprintf (stderr, _("Could not execute '%s': %s\n"),
			 KILL_PROCESS_CMD, strerror (errno));
		exit (1);
	}

	if (WEXITSTATUS (status) != 0)
		return FALSE;

	desc = g_string_new (NULL);
	for (i = 0; i < comp->names->len; i++) {
		if (i > 0)
			g_string_append (desc, " / ");
		g_string_append (desc, comp->names->pdata[i]);
	}

	printf (_("Shutting down %s (%s)\n"), proc_name, desc->str);
	g_string_free (desc, TRUE);
	command = g_strdup_printf (KILL_PROCESS_CMD " -9 %s 2> /dev/null",
				   proc_name);
	system (command);
	g_free (command);
	return TRUE;
};

static const char *patterns[] = {
	"%s", "%.16s", "lt-%s", "lt-%.13s"
};
static const int n_patterns = G_N_ELEMENTS (patterns);

static gboolean
kill_component (gpointer key, gpointer value, gpointer data)
{
	KillevComponent *comp = value;
	char *base_name, *exe_name, *dash;
	int i;

	base_name = g_strdup (comp->location);
 try_again:
	for (i = 0; i < n_patterns; i++) {
		exe_name = g_strdup_printf (patterns[i], base_name);
		if (kill_process (exe_name, comp)) {
			g_free (exe_name);
			g_free (base_name);
			return TRUE;
		}
		g_free (exe_name);
	}

	dash = strrchr (base_name, '-');
	if (dash && !strcmp (dash + 1, BASE_VERSION)) {
		*dash = '\0';
		goto try_again;
	}

	g_free (base_name);
	return TRUE;
}

static void
add_matching_query (const char *query)
{
	Bonobo_ServerInfoList *info_list;
	Bonobo_ServerInfo *info;
	CORBA_Environment ev;
	const char *location, *name;
	KillevComponent *comp;
	int i;

	CORBA_exception_init (&ev);

	info_list = bonobo_activation_query (query, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		printf ("Bonobo activation failure: %s\n",
			bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	for (i = 0; i < info_list->_length; i++) {
		info = &info_list->_buffer[i];

		if (strcmp (info->server_type, "exe") != 0)
			continue;

		location = info->location_info;
		if (strchr (location, '/'))
			location = strrchr (location, '/') + 1;

		comp = g_hash_table_lookup (components, location);
		if (!comp) {
			comp = g_new (KillevComponent, 1);
			comp->location = g_strdup (location);
			comp->names = g_ptr_array_new ();
			g_hash_table_insert (components, comp->location, comp);
		}

		name = bonobo_server_info_prop_lookup (info, "name", languages);
		if (name)
			g_ptr_array_add (comp->names, g_strdup (name));
	}

	CORBA_free (info_list);
	CORBA_exception_free (&ev);
}

static void
add_matching_repo_id (const char *repo_id)
{
	char *query;

	query = g_strdup_printf ("repo_ids.has ('%s')", repo_id);
	add_matching_query (query);
	g_free (query);
}

static void
add_matching_iid (const char *iid)
{
	char *query;

	query = g_strdup_printf ("iid == '%s'", iid);
	add_matching_query (query);
	g_free (query);
}

int
main (int argc, char **argv)
{
	const GList *l;

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init (PACKAGE, VERSION, LIBGNOME_MODULE, argc, argv, 
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    NULL);

	l = gnome_i18n_get_language_list("LC_MESSAGES");
	for (languages=NULL;l;l=l->next)
		languages = g_slist_append(languages, l->data);

	components = g_hash_table_new (g_str_hash, g_str_equal);

	add_matching_repo_id ("IDL:GNOME/Evolution/Shell:" BASE_VERSION);
	g_hash_table_foreach_remove (components, kill_component, NULL);

	add_matching_repo_id ("IDL:GNOME/Evolution/Component:" BASE_VERSION);
	add_matching_repo_id ("IDL:GNOME/Evolution/DataServer/CalFactory:" DATASERVER_API_VERSION);
	add_matching_repo_id ("IDL:GNOME/Evolution/DataServer/BookFactory:" DATASERVER_API_VERSION);
	add_matching_repo_id ("IDL:GNOME/Evolution/Importer:" BASE_VERSION);
	add_matching_repo_id ("IDL:GNOME/Evolution/IntelligentImporter:" BASE_VERSION);
	add_matching_repo_id ("IDL:GNOME/Spell/Dictionary:0.3");

	add_matching_iid ("OAFIID:GNOME_Evolution_Calendar_AlarmNotify_Factory:" BASE_VERSION);
	add_matching_iid ("OAFIID:GNOME_GtkHTML_Editor_Factory:3.1");

	g_hash_table_foreach_remove (components, kill_component, NULL);

	return 0;
}
