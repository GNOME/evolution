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

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gtk/gtklabel.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-stock.h>
#include <gal/widgets/e-gui-utils.h>

#include "e-local-folder.h"

#include "e-setup.h"


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

		fullname = g_concat_dir_and_file (evolution_directory,
						  current->d_name);
		fulldefaultname = g_concat_dir_and_file (current_directory,
							 current->d_name);

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
	GtkWidget *dialog;
	GtkWidget *label1, *label2;
	gboolean retval;
	GList *newfiles, *l;
	char *defaultdir;
	int result;

	defaultdir = g_strdup (EVOLUTION_DATADIR "/evolution/default_user");
	newfiles = g_list_concat (NULL, check_dir_recur (evolution_directory,
							 defaultdir));

	if (newfiles == NULL) {
		retval = TRUE;
		goto out;
	}

	dialog = gnome_dialog_new (_("Evolution installation"),
				   GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL,
				   NULL);

	label1 = gtk_label_new (_("This new version of Evolution needs to install additional files\ninto your personal Evolution directory"));
	label2 = gtk_label_new (_("Please click \"OK\" to install the files, or \"Cancel\" to exit."));

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label1, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label2, TRUE, TRUE, 0);

	gtk_widget_show (label1);
	gtk_widget_show (label2);

	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, FALSE);

	result = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));

	if (result != 0) {
		retval = FALSE;
		goto out;
	}

	retval = TRUE;
	for (l = newfiles; l; l = l->next) {
		char *command;
		char *shortpath;

		shortpath = l->data + strlen (EVOLUTION_DATADIR "/evolution/default_user/");
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
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Could not update files correctly"));

 out:

	for (l = newfiles; l; l = l->next)
		g_free (l->data);

	g_list_free (newfiles);
	g_free (defaultdir);

	return retval;
}


static gboolean
copy_default_stuff (const char *evolution_directory)
{
	gboolean retval;
	char *command;
	char *old_default_shortcuts_file;

	if (mkdir (evolution_directory, 0700)) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot create the directory\n%s\nError: %s"),
			  evolution_directory,
			  g_strerror (errno));
		return FALSE;
	}

	command = g_strconcat ("cp -r ",
			       EVOLUTION_DATADIR,
			       "/evolution/default_user/* ",
			       evolution_directory,
			       NULL);

	if (system (command) != 0) {
		/* FIXME: Give more help.  */
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("An error occurred in copying files into\n`%s'."), evolution_directory);
		retval = FALSE;
	} else {
		retval = TRUE;
	}

	/* Temporary block of code to keep it from using the older
           shortcuts.xml that exists for current users in
           $GNOME/share/evolution/default_user.  Remove this by 1.0 */
	old_default_shortcuts_file = g_concat_dir_and_file (evolution_directory,
							    "shortcuts.xml");
	unlink (old_default_shortcuts_file);
	g_free (old_default_shortcuts_file);
	
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

			fullpath = g_concat_dir_and_file (path, contents->d_name);
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

	name = g_concat_dir_and_file (evolution_directory, "private");
	if (stat (name, &buf) == -1) {
		if (mkdir (name, 0700) != 0) {
			e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
				  _("Evolution could not create directory\n"
				    "%s:\n%s"),
				  name, strerror (errno));
			free (name);
			return FALSE;
		}

		free (name);
		return TRUE;
	}

	if (S_ISDIR (buf.st_mode) && access (name, R_OK | W_OK | X_OK) == 0) {
		free (name);
		return TRUE;
	}

	if (S_ISDIR (buf.st_mode)) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Directory %s\n"
			    "does not have the right permissions. Please make it\n"
			    "readable and executable and restart Evolution."),
			  name);
	} else {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("File %s\n"
			    "should be removed to allow Evolution to work correctly.\n"
			    "Please remove this file and restart Evolution."),
			  name, strerror (errno));
	}

	free (name);
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
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("The file `%s' is not a directory.\n"
			    "Please move it in order to allow installation\n"
			    "of the Evolution user files."));
		return FALSE;
	}

	/* Make sure this is really our directory, not an Evolution
	 * build tree or something like that.
	 */
	file = g_strdup_printf ("%s/local/Executive-Summary", evolution_directory);
	if (stat (file, &statinfo) == 0) {
		if (S_ISDIR (statinfo.st_mode)) {
			GtkWidget *dialog;

			dialog = gnome_message_box_new (_("Evolution has detected an old\n"
							  "Executive-Summary directory.\n"
							  "This needs to be removed before\n"
							  "Evolution will run.\n"
							  "Do you want me to remove this directory?"),
							GNOME_MESSAGE_BOX_INFO,
							GNOME_STOCK_BUTTON_YES,
							GNOME_STOCK_BUTTON_NO,
							NULL);
			switch (gnome_dialog_run_and_close (GNOME_DIALOG (dialog))) {
			case 0:
				e_shell_rm_dir (file);
				break;

			default:
				return FALSE;
			}
		}
	}

	g_free (file);

	file = g_strdup_printf ("%s/searches.xml", evolution_directory);
	if (stat (file, &statinfo) != 0) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
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

		gtk_object_unref (GTK_OBJECT (local_folder));
	}
	g_free (file);

	if (! setup_bonobo_conf_private_directory (evolution_directory))
		return FALSE;

	/* User has evolution directory...
	   Check if it is up to date. */
	return check_evolution_directory (evolution_directory);
}
