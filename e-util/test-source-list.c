/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-source-list.c - Test for the ESourceList class.
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-source-list.h"

#include <libgnomeui/gnome-ui-init.h>
#include <glib/gmain.h>


/* Globals. */

static GMainLoop *main_loop = NULL;
static ESourceList *list = NULL;
static int idle_dump_id = 0;


/* Options.  */

static gboolean listen = FALSE;
static gboolean dump = FALSE;
static char *key_arg = "/apps/evolution/test/source_list";
static char *source_arg = NULL;
static char *group_arg = NULL;
static char *add_group_arg = NULL;
static char *add_source_arg = NULL;
static char *remove_group_arg = NULL;
static char *remove_source_arg = NULL;
static char *set_name_arg = NULL;
static char *set_base_uri_arg = NULL;
static char *set_relative_uri_arg = NULL;
static char *set_color_arg = NULL;
static gboolean unset_color = FALSE;

static struct poptOption options[] = {
	{ "key", '\0', POPT_ARG_STRING, &key_arg, 0,
	  "Name of the GConf key to use", "PATH" },
	{ "source", '\0', POPT_ARG_STRING, &source_arg, 0, 
	  "UID of source to apply operation to", "UID" },
	{ "group", '\0', POPT_ARG_STRING, &group_arg, 0, 
	  "UID of group to apply operation to", "UID" },
	{ "add-group", '\0', POPT_ARG_STRING, &add_group_arg, 0,
	  "Add group of specified name", "NAME" },
	{ "add-source", '\0', POPT_ARG_STRING, &add_source_arg, 0,
	  "Add source of specified name", "NAME" },
	{ "remove-group", '\0', POPT_ARG_STRING, &remove_group_arg, 0,
	  "Remove group of specified name", "NAME" },
	{ "remove-source", '\0', POPT_ARG_STRING, &remove_source_arg, 0,
	  "Remove source of specified name", "NAME" },
	{ "set-name", '\0', POPT_ARG_STRING, &set_name_arg, 0,
	  "Set name of source or group.  When used with --group, it sets the name of a group.  "
	  "When used with both --group and --source, it sets the name of a source.", "NAME" },
	{ "set-relative-uri", '\0', POPT_ARG_STRING, &set_relative_uri_arg, 0,
	  "Set relative URI of a source.  Use with --source or --add-source.", "NAME" },
	{ "set-base-uri", '\0', POPT_ARG_STRING, &set_base_uri_arg, 0,
	  "Set base URI of a group.  Use with --group or --add-group.", "NAME" },
	{ "set-color", '\0', POPT_ARG_STRING, &set_color_arg, 0,
	  "Set the color of a source.  Use with --source or --add-source.", "COLOR (rrggbb)" },
	{ "unset-color", '\0', POPT_ARG_NONE, &unset_color, 0,
	  "Unset the color of a source.  Use with --source or --add-source.", NULL },
	{ "listen", '\0', POPT_ARG_NONE, &listen, 0,
	  "Wait and listen for changes.", "" },
	{ "dump", '\0', POPT_ARG_NONE, &dump, 0,
	  "List the current configured sources.", "" },
	POPT_AUTOHELP
	{ NULL }
};


/* Forward decls.  */
static void group_added_callback (ESourceList *list, ESourceGroup *group);
static void group_removed_callback (ESourceList *list, ESourceGroup *group);
static void source_added_callback (ESourceGroup *group, ESource *source);
static void source_removed_callback (ESourceGroup *group, ESource *source);


static void
dump_source (ESource *source)
{
	char *uri = e_source_get_uri (source);
	gboolean has_color;
	guint32 color;

	g_print ("\tSource %s\n", e_source_peek_uid (source));
	g_print ("\t\tname: %s\n", e_source_peek_name (source));
	g_print ("\t\trelative_uri: %s\n", e_source_peek_relative_uri (source));
	g_print ("\t\tabsolute_uri: %s\n", uri);

	has_color = e_source_get_color (source, &color);
	if (has_color)
		g_print ("\t\tcolor: %06x\n", color);

	g_free (uri);
}

static void
dump_group (ESourceGroup *group)
{
	GSList *sources, *p;

	g_print ("Group %s\n", e_source_group_peek_uid (group));
	g_print ("\tname: %s\n", e_source_group_peek_name (group));
	g_print ("\tbase_uri: %s\n", e_source_group_peek_base_uri (group));

	sources = e_source_group_peek_sources (group);
	for (p = sources; p != NULL; p = p->next) {
		ESource *source = E_SOURCE (p->data);

		dump_source (source);

		if (e_source_peek_group (source) != group)
			g_warning ("\t\t** ERROR ** parent pointer is %p, should be %p",
				   e_source_peek_group (source), group);
	}
}

static void
dump_list (void)
{
	GSList *groups, *p;

	groups = e_source_list_peek_groups (list);
	if (groups == NULL) {
		g_print ("(No items)\n");
		return;
	}

	for (p = groups; p != NULL; p = p->next)
		dump_group (E_SOURCE_GROUP (p->data));
}


static int
idle_dump_callback (void *unused_data)
{
	dump_list ();
	idle_dump_id = 0;

	return FALSE;
}

static void
dump_on_idle (void)
{
	if (idle_dump_id == 0)
		idle_dump_id = g_idle_add (idle_dump_callback, NULL);
}


static void
source_changed_callback (ESource *source)
{
	static int count = 0;

	g_print ("** Event: source \"%s\" changed (%d)\n", e_source_peek_name (source), ++count);

	dump_on_idle ();
}

static void
group_changed_callback (ESourceGroup *group)
{
	static int count = 0;

	g_print ("** Event: group \"%s\" changed (%d)\n", e_source_group_peek_name (group), ++count);

	dump_on_idle ();
}

static void
list_changed_callback (ESourceGroup *group)
{
	static int count = 0;

	g_print ("** Event: list changed (%d)\n", ++count);

	dump_on_idle ();
}


static void
connect_source (ESource *source)
{
	g_object_ref (source);
	g_signal_connect (source, "changed", G_CALLBACK (source_changed_callback), NULL);
}

static void
connect_group (ESourceGroup *group)
{
	GSList *sources, *p;

	g_object_ref (group);
	g_signal_connect (group, "changed", G_CALLBACK (group_changed_callback), NULL);
	g_signal_connect (group, "source_added", G_CALLBACK (source_added_callback), NULL);
	g_signal_connect (group, "source_removed", G_CALLBACK (source_removed_callback), NULL);

	sources = e_source_group_peek_sources (group);
	for (p = sources; p != NULL; p = p->next)
		connect_source (E_SOURCE (p->data));
}

static void
connect_list (void)
{
	GSList *groups, *p;

	g_signal_connect (list, "changed", G_CALLBACK (list_changed_callback), NULL);
	g_signal_connect (list, "group_added", G_CALLBACK (group_added_callback), NULL);
	g_signal_connect (list, "group_removed", G_CALLBACK (group_removed_callback), NULL);

	groups = e_source_list_peek_groups (list);
	for (p = groups; p != NULL; p = p->next)
		connect_group (E_SOURCE_GROUP (p->data));
}

static void
disconnect_group (ESourceGroup *group)
{
	g_signal_handlers_disconnect_by_func (group, G_CALLBACK (group_changed_callback), NULL);
	g_signal_handlers_disconnect_by_func (group, G_CALLBACK (source_added_callback), NULL);

	g_object_unref (group);
}

static void
disconnect_source (ESource *source)
{
	g_signal_handlers_disconnect_by_func (source, G_CALLBACK (source_changed_callback), NULL);

	g_object_unref (source);
}


static void
source_added_callback (ESourceGroup *group,
		       ESource *source)
{
	static int count = 0;

	g_print ("** Event: source \"%s\" added (%d)\n", e_source_peek_name (source), ++count);

	connect_source (source);
	dump_on_idle ();
}

static void
source_removed_callback (ESourceGroup *group,
			 ESource *source)
{
	static int count = 0;

	g_print ("** Event: source \"%s\" removed (%d)\n", e_source_peek_name (source), ++count);

	disconnect_source (source);
	dump_on_idle ();
}

static void
group_added_callback (ESourceList *list,
		      ESourceGroup *group)
{
	static int count = 0;

	g_print ("** Event: group \"%s\" added (%d)\n", e_source_group_peek_name (group), ++count);

	connect_group (group);
	dump_on_idle ();
}

static void
group_removed_callback (ESourceList *list,
			ESourceGroup *group)
{
	static int count = 0;

	g_print ("** Event: group \"%s\" removed (%d)\n", e_source_group_peek_name (group), ++count);

	disconnect_group (group);
	dump_on_idle ();
}


static int
on_idle_do_stuff (void *unused_data)
{
	GConfClient *client = gconf_client_get_default ();
	ESourceGroup *new_group = NULL;
	ESource *new_source = NULL;

	list = e_source_list_new_for_gconf (client, key_arg);
	g_object_unref (client);

	if (add_group_arg != NULL) {
		if (group_arg != NULL) {
			fprintf (stderr, "--add-group and --group cannot be used at the same time.\n");
			exit (1);
		}
		if (set_base_uri_arg == NULL) {
			fprintf (stderr, "When using --add-group, you need to specify a base URI using --set-base-uri.\n");
			exit (1);
		}

		new_group = e_source_group_new (add_group_arg, set_base_uri_arg);
		e_source_list_add_group (list, new_group, -1);
		g_object_unref (new_group);

		e_source_list_sync (list, NULL);
	}

	if (remove_group_arg != NULL) {
		ESourceGroup *group;

		group = e_source_list_peek_group_by_uid (list, remove_group_arg);
		if (group == NULL) {
			fprintf (stderr, "No such group \"%s\".\n", remove_group_arg);
			exit (1);
		}

		e_source_list_remove_group (list, group);
		e_source_list_sync (list, NULL);
	}

	if (add_source_arg != NULL) {
		ESourceGroup *group;

		if (group_arg == NULL && new_group == NULL) {
			fprintf (stderr,
				 "When using --add-source, you need to specify a group using either --group\n"
				 "or --add-group.\n");
			exit (1);
		}
		if (set_relative_uri_arg == NULL) {
			fprintf (stderr,
				 "When using --add-source, you need to specify a relative URI using\n"
				 "--set-relative-uri.\n");
			exit (1);
		}

		if (group_arg == NULL) {
			group = new_group;
		} else {
			group = e_source_list_peek_group_by_uid (list, group_arg);
			if (group == NULL) {
				fprintf (stderr, "No such group \"%s\".\n", group_arg == NULL ? add_group_arg : group_arg);
				exit (1);
			}
		}

		new_source = e_source_new (add_source_arg, set_relative_uri_arg);
		e_source_group_add_source (group, new_source, -1);
		e_source_list_sync (list, NULL);
	}

	if (remove_source_arg != NULL) {
		ESource *source;

		if (group_arg == NULL) {
			fprintf (stderr, "When using --remove-source, you need to specify a group using --group.\n");
			exit (1);
		}

		source = e_source_list_peek_source_by_uid (list, group_arg, remove_source_arg);
		if (source == NULL) {
			fprintf (stderr, "No such source \"%s\" in group \"%s\".\n", remove_source_arg, group_arg);
			exit (1);
		}

		e_source_list_remove_source_by_uid (list, group_arg, remove_source_arg);
		e_source_list_sync (list, NULL);
	}

	if (set_name_arg != NULL) {
		if (group_arg == NULL) {
			fprintf (stderr,
				 "When using --set-name, you need to specify a source (using --group and\n"
				 "--source) or a group (using --group alone).\n");
			exit (1);
		}

		if (source_arg != NULL) {
			ESource *source = e_source_list_peek_source_by_uid (list, group_arg, source_arg);

			if (source != NULL) {
				e_source_set_name (source, set_name_arg);
			} else {
				fprintf (stderr, "No such source \"%s\" in group \"%s\".\n", source_arg, group_arg);
				exit (1);
			}
		} else {
			ESourceGroup *group = e_source_list_peek_group_by_uid (list, group_arg);

			if (group != NULL) {
				e_source_group_set_name (group, set_name_arg);
			} else {
				fprintf (stderr, "No such group \"%s\".\n", group_arg);
				exit (1);
			}
		}

		e_source_list_sync (list, NULL);
	}

	if (set_relative_uri_arg != NULL && add_source_arg == NULL) {
		ESource *source;

		if (source_arg == NULL || group_arg == NULL) {
			fprintf (stderr,
				 "When using --set-relative-uri, you need to specify a source using --group\n"
				 "and --source.\n");
			exit (1);
		}

		source = e_source_list_peek_source_by_uid (list, group_arg, source_arg);
		e_source_set_relative_uri (source, set_relative_uri_arg);
		e_source_list_sync (list, NULL);
	}

	if (set_color_arg != NULL) {
		ESource *source;
		guint32 color;

		if (add_source_arg == NULL && (source_arg == NULL || group_arg == NULL)) {
			fprintf (stderr,
				 "When using --set-color, you need to specify a source using --group\n"
				 "and --source.\n");
			exit (1);
		}

		if (add_source_arg != NULL)
			source = new_source;
		else
			source = e_source_list_peek_source_by_uid (list, group_arg, source_arg);

		sscanf (set_color_arg, "%06x", &color);
		e_source_set_color (source, color);
		e_source_list_sync (list, NULL);
	}

	if (unset_color) {
		ESource *source;

		if (add_source_arg == NULL && (source_arg == NULL || group_arg == NULL)) {
			fprintf (stderr,
				 "When using --unset-color, you need to specify a source using --group\n"
				 "and --source.\n");
			exit (1);
		}

		if (add_source_arg != NULL)
			source = new_source;
		else
			source = e_source_list_peek_source_by_uid (list, group_arg, source_arg);

		e_source_unset_color (source);
		e_source_list_sync (list, NULL);
	}

	if (set_base_uri_arg != NULL && add_group_arg == NULL) {
		ESourceGroup *group;

		if (group_arg == NULL) {
			fprintf (stderr,
				 "When using --set-base-uri, you need to specify a group using --group.\n");
			exit (1);
		}

		group = e_source_list_peek_group_by_uid (list, group_arg);
		e_source_group_set_base_uri (group, set_base_uri_arg);
		e_source_list_sync (list, NULL);
	}

	connect_list ();

	if (dump)
		dump_list ();

	if (! listen)
		g_main_loop_quit (main_loop);

	return FALSE;
}


int
main (int argc,
      char **argv)
{
	GnomeProgram *program;

	program = gnome_program_init ("test-source-list", "0.0",
				      LIBGNOMEUI_MODULE, argc, argv,
				      GNOME_PARAM_POPT_TABLE, options,
				      NULL);

	g_idle_add (on_idle_do_stuff, NULL);

	main_loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (main_loop);

	return 0;
}
