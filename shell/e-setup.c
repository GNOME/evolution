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

#include "e-util/e-gui-utils.h"

#include "e-setup.h"


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

	if (stat (evolution_directory, &statinfo) != 0)
		return copy_default_stuff (evolution_directory);

	if (! S_ISDIR (statinfo.st_mode)) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("The file `%s' is not a directory.\n"
			    "Please remove it in order to allow installation\n"
			    "of the Evolution user files."));
		return FALSE;
	}

	return TRUE;
}
