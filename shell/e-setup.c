/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-setup.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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
 */

/* This needs to be a lot better.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-setup.h"

#include "e-local-folder.h"
#include "e-shell-config.h"
#include "e-shell-constants.h"

#include "e-util/e-path.h"

#include <gconf/gconf-client.h>

#include <gtk/gtklabel.h>

#include <gal/widgets/e-gui-utils.h>

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_USER_PATH  EVOLUTION_DATADIR "/evolution-" BASE_VERSION "/default_user"


static GList *
check_dir_recur (const char *evolution_directory,
		 const char *current_directory)
{
	DIR *def;
	GList *newfiles = NULL;
	struct dirent *current;

	def = opendir (current_directory);
	if (def == NULL)
		return NULL;

	current = readdir (def);
	while (current != NULL) {
		struct stat buf;
		char *fullname, *fulldefaultname;

		if (current->d_name[0] == '.' &&
		    (current->d_name[1] == '\0' ||
		     (current->d_name[1] == '.' && current->d_name[2] == '\0'))) {
			current = readdir (def);
			continue;
		}

		/* Hack to not copy the old Executive-Summary dir */
		if (strcmp (current->d_name, "Executive-Summary") == 0) {
			current = readdir (def);
			continue;
		}

		fullname = g_build_filename (evolution_directory, current->d_name, NULL);
		fulldefaultname = g_build_filename (current_directory, current->d_name, NULL);

		if (stat (fullname, &buf) == -1) {
			char *name;

			name = g_strdup (fulldefaultname);
			newfiles = g_list_append (newfiles, name);
		} else {
			if (S_ISDIR (buf.st_mode)) {
				newfiles = g_list_concat (newfiles,
							  check_dir_recur (fullname,
									   fulldefaultname));
			}
		}

		g_free (fulldefaultname);
		g_free (fullname);
		current = readdir (def);
	}

	closedir (def);
	return newfiles;
}

static gboolean
check_evolution_directory (const char *evolution_directory)
{
	gboolean retval;
	GList *newfiles, *l;

	newfiles = g_list_concat (NULL, check_dir_recur (evolution_directory, DEFAULT_USER_PATH));

	if (newfiles == NULL) {
		retval = TRUE;
		goto out;
	}

	retval = TRUE;
	for (l = newfiles; l; l = l->next) {
		char *command;
		char *shortpath;

		shortpath = l->data + strlen (DEFAULT_USER_PATH);
		command = g_strconcat ("cp -r ",
				       l->data, " ",
				       evolution_directory, "/",
				       shortpath,
				       NULL);

		if (system (command) != 0) {
			retval = FALSE;
		} else {
			retval = (retval && TRUE);
		}

		g_free (command);
	}

	if (retval == FALSE)
		e_notice (NULL, GTK_MESSAGE_ERROR,
			  _("Could not update files correctly"));

 out:

	for (l = newfiles; l; l = l->next)
		g_free (l->data);

	g_list_free (newfiles);

	return retval;
}


static gboolean
copy_default_stuff (const char *evolution_directory)
{
	gboolean retval;
	char *command;

	if (mkdir (evolution_directory, 0700)) {
		e_notice (NULL, GTK_MESSAGE_ERROR,
			  _("Cannot create the directory\n%s\nError: %s"),
			  evolution_directory,
			  g_strerror (errno));
		return FALSE;
	}

	command = g_strconcat ("cp -r " DEFAULT_USER_PATH " ", evolution_directory, NULL);

	if (system (command) != 0) {
		/* FIXME: Give more help.  */
		e_notice (NULL, GTK_MESSAGE_ERROR,
			  _("An error occurred in copying files into\n`%s'."), evolution_directory);
		retval = FALSE;
	} else {
		retval = TRUE;
	}

	g_free (command);

	return retval;
}

static void
e_shell_rm_dir (const char *path)
{
	DIR *base;
	struct stat statbuf;
	struct dirent *contents;

	stat (path, &statbuf);
	if (!S_ISDIR (statbuf.st_mode)) {
		/* Not a directory */
		g_message ("Removing: %s", path);
		unlink (path);
		return;
	} else {
		g_message ("Opening: %s", path);
		base = opendir (path);

		if (base == NULL)
			return;

		contents = readdir (base);
		while (contents != NULL) {
			char *fullpath;

			if (strcmp (contents->d_name, ".") == 0|| 
			    strcmp (contents->d_name, "..") ==0) {
				contents = readdir (base);
				continue;
			}

			fullpath = g_build_filename (path, contents->d_name, NULL);
			e_shell_rm_dir (fullpath);
			g_free (fullpath);

			contents = readdir (base);
		}

		closedir (base);
		rmdir (path);
	}
}


/* FIXME: This is a workaround for bonobo-conf breakage.  */
static gboolean
setup_bonobo_conf_private_directory (const char *evolution_directory)
{
	char *name;
	struct stat buf;

	name = g_build_filename (evolution_directory, "private", NULL);
	if (stat (name, &buf) == -1) {
		if (mkdir (name, 0700) != 0) {
			e_notice (NULL, GTK_MESSAGE_ERROR,
				  _("Evolution could not create directory\n"
				    "%s:\n%s"),
				  name, strerror (errno));
			g_free (name);
			return FALSE;
		}

		g_free (name);
		return TRUE;
	}

	if (S_ISDIR (buf.st_mode) && access (name, R_OK | W_OK | X_OK) == 0) {
		g_free (name);
		return TRUE;
	}

	if (S_ISDIR (buf.st_mode)) {
		e_notice (NULL, GTK_MESSAGE_ERROR,
			  _("Directory %s\n"
			    "does not have the right permissions. Please make it\n"
			    "readable and executable and restart Evolution."),
			  name);
	} else {
		e_notice (NULL, GTK_MESSAGE_ERROR,
			  _("File %s\n"
			    "should be removed to allow Evolution to work correctly.\n"
			    "Please remove this file and restart Evolution."),
			  name, strerror (errno));
	}

	g_free (name);
	return FALSE;
}


gboolean
e_setup (const char *evolution_directory)
{
	struct stat statinfo;
	char *file;

	if (stat (evolution_directory, &statinfo) != 0) {
		return copy_default_stuff (evolution_directory);
	}

	if (! S_ISDIR (statinfo.st_mode)) {
		e_notice (NULL, GTK_MESSAGE_ERROR,
			  _("The file `%s' is not a directory.\n"
			    "Please move it in order to allow installation\n"
			    "of the Evolution user files."));
		return FALSE;
	}

	file = g_strdup_printf ("%s/searches.xml", evolution_directory);
	if (stat (file, &statinfo) != 0) {
		e_notice (NULL, GTK_MESSAGE_ERROR,
			  _("The directory `%s' exists but is not the\n"
			    "Evolution directory.  Please move it in order\n"
			    "to allow installation of the Evolution user "
			    "files."), evolution_directory);
		g_free (file);
		return FALSE;
	}
	g_free (file);

	/* If the user has an old-style config file, replace it with
	 * the new-style config directory. FIXME: This should be
	 * temporary.
	 */
	file = g_strdup_printf ("%s/config", evolution_directory);
	if (stat (file, &statinfo) == 0 && ! S_ISDIR (statinfo.st_mode)) {
		char *old = g_strdup_printf ("%s.old", file);

		rename (file, old);
		mkdir (file, 0700);
		g_free (old);
	}
	g_free (file);

	/* If the user has an old style trash folder, remove it so it gets
	 * replaced by the new vfolder-based trash folder.  FIXME: This should
	 * go away at some point.  */
	file = g_strdup_printf ("%s/local/Trash", evolution_directory);
	if (stat (file, &statinfo) == 0 && S_ISDIR (statinfo.st_mode)) {
		EFolder *local_folder;

		local_folder = e_local_folder_new_from_path (file);
		if (local_folder != NULL
		    && strcmp (e_folder_get_type_string (local_folder), "mail") == 0) {
			char *old = g_strdup_printf ("%s.old", file);

			rename (file, old);
			g_free (old);
		}

		if (local_folder != NULL)
			g_object_unref (local_folder);
	}
	g_free (file);

	if (! setup_bonobo_conf_private_directory (evolution_directory))
		return FALSE;

	/* User has evolution directory...
	   Check if it is up to date. */
	return check_evolution_directory (evolution_directory);
}


static void
set_default_folder_physical_uri_from_path (GConfClient *client,
					   const char *evolution_directory,
					   const char *path_key_name)
{
	char *gconf_path;
	char *path_value;

	gconf_path = g_strconcat ("/apps/evolution/shell/default_folders/", path_key_name, NULL);
	path_value = gconf_client_get_string (client, gconf_path, NULL);
	g_free (gconf_path);

	if (path_value != NULL
	    && strncmp (path_value, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) == 0
	    && strncmp (path_value + E_SHELL_URI_PREFIX_LEN, "/local/", 7) == 0) {
		char *key_prefix;
		char *local_physical_uri;
		char *file_uri_prefix;

		key_prefix = g_strndup (path_key_name, strchr (path_key_name, '_') - path_key_name);
		gconf_path = g_strconcat ("/apps/evolution/shell/default_folders/", key_prefix, "_uri", NULL);
		file_uri_prefix = g_strconcat ("file://", evolution_directory, "/local", NULL);
		local_physical_uri = e_path_to_physical (file_uri_prefix, path_value + E_SHELL_URI_PREFIX_LEN + 6);

		gconf_client_set_string (client, gconf_path, local_physical_uri, NULL);

		g_free (gconf_path);
		g_free (key_prefix);
		g_free (local_physical_uri);
		g_free (file_uri_prefix);
	}
}

void
e_setup_check_config (const char *evolution_directory)
{
	GConfClient *client;
	char *tmp;

	client = gconf_client_get_default ();

	tmp = gconf_client_get_string (client, "/apps/evolution/shell/default_folders/mail_uri", NULL);
	if (tmp != NULL && *tmp != 0) {
		g_object_unref (client);
		return;
	}

	/* The following ugliness is to initially set up the physical URIs
	   based on the default path values (which come from GConf).  Of
	   course, this way of configuring the default folders is a bit of a
	   mess and needs to be cleaned up...  */

	set_default_folder_physical_uri_from_path (client, evolution_directory, "mail_path");
	set_default_folder_physical_uri_from_path (client, evolution_directory, "contacts_path");
	set_default_folder_physical_uri_from_path (client, evolution_directory, "calendar_path");
	set_default_folder_physical_uri_from_path (client, evolution_directory, "tasks_path");

	g_object_unref (client);
}
