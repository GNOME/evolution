/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-setup.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
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
 *
 */

/* This needs to be a lot better.  */

#include <glib.h>
#include <gnome.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "e-util/e-gui-utils.h"

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
		
		fullname = g_concat_dir_and_file (evolution_directory,
						  current->d_name);
		fulldefaultname = g_concat_dir_and_file (current_directory,
							 current->d_name);

		if (current->d_name[0] == '.' && 
		    (current->d_name[1] == '\0' || 
		     (current->d_name[1] == '.' && current->d_name[2] == '\0'))) {
			current = readdir (def);
			continue;
		}

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
	GtkWidget *scroller, *clist;
	GtkWidget *label;
	gboolean retval;
	GList *newfiles, *l;
	char *defaultdir;
	int result;

	defaultdir = g_strdup (EVOLUTION_DATADIR "/evolution/default_user");
	newfiles = g_list_concat (NULL, check_dir_recur (evolution_directory,
							 defaultdir));
	
	if (newfiles == NULL)
		return TRUE;

	dialog = gnome_dialog_new (_("Evolution Installation"),
				   GNOME_STOCK_BUTTON_OK, 
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);
	label = gtk_label_new (_("Since Evolution was installed,"
				 "\nthe following files need to be updated"));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label,
			    TRUE, TRUE, 0);
	scroller = gtk_scrolled_window_new (NULL, NULL);
	clist = gtk_clist_new (1);
	gtk_clist_column_titles_hide (GTK_CLIST (clist));

	for (l = newfiles; l; l = l->next) {
		char *row[1];

		row[0] = l->data;
		gtk_clist_append (GTK_CLIST (clist), row);
	}

	gtk_container_add (GTK_CONTAINER (scroller), clist);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), scroller,
			    TRUE, TRUE, 0);

	gtk_widget_show (label);
	gtk_widget_show_all (scroller);
	result = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));

	if (result != 0)
		return FALSE;

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
		
		g_free (l->data);
	}

	g_list_free (newfiles);
	g_free (defaultdir);

	if (retval == FALSE) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Could not update files correctly"));
		return FALSE;
	} else {
		e_notice (NULL, GNOME_MESSAGE_BOX_INFO,
			  _("Evolution files successfully installed."));
		return TRUE;
	}

	return TRUE;
}
		

static gboolean
copy_default_stuff (const char *evolution_directory)
{
	GtkWidget *dialog;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *label3;
	gboolean retval;
	char *command;
	int result;

	dialog = gnome_dialog_new (_("Evolution installation"),
				   GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL,
				   NULL);

	label1 = gtk_label_new (_("This seems to be the first time you run Evolution."));
	label2 = gtk_label_new (_("Please click \"OK\" to install the Evolution user files under"));
	label3 = gtk_label_new (evolution_directory);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label1, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label2, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label3, TRUE, TRUE, 0);

	gtk_widget_show (label1);
	gtk_widget_show (label2);
	gtk_widget_show (label3);

	result = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	if (result != 0)
		return FALSE;

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
			  _("Cannot copy files into\n`%s'."), evolution_directory);
		retval = FALSE;
	} else {
		e_notice (NULL, GNOME_MESSAGE_BOX_INFO,
			  _("Evolution files successfully installed."));
		retval = TRUE;
	}

	g_free (command);

	return retval;
}


gboolean
e_setup (const char *evolution_directory)
{
	struct stat statinfo;
	char *file;

	if (stat (evolution_directory, &statinfo) != 0)
		return copy_default_stuff (evolution_directory);

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
	file = g_strdup_printf ("%s/shortcuts.xml", evolution_directory);
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

	/* User has evolution directory...
	   Check if it is up to date. */
	return check_evolution_directory (evolution_directory);
}
