/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-view.c
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
 * Authors:
 *   Miguel de Icaza
 *   Ettore Perazzoli
 */

#include <config.h>
#include <gnome.h>

#include "e-shell-view.h"
#include "e-shell-view-menu.h"


static void
command_quit (GtkWidget *widget,
	      gpointer data)
{
	EShellView *shell_view;
	EShell *shell;

	shell_view = E_SHELL_VIEW (data);

	shell = e_shell_view_get_shell (shell_view);
	e_shell_quit (shell);
}

static void
command_run_bugbuddy (GtkWidget *menuitem, gpointer data)
{
        int pid;
        char *args[] = {
                "bug-buddy",
                "--sm-disable",
                "--package=evolution",
                "--package-ver="VERSION,
                NULL
        };
        args[0] = gnome_is_program_in_path ("bug-buddy");
        if (!args[0]) {
                /* you might have to call gnome_dialog_run() on the
                 * dialog returned here, I don't remember...
                 */
                gnome_error_dialog (_("Bug buddy was not found in your $PATH."));
        }
        pid = gnome_execute_async (NULL, 4, args);
        g_free (args[0]);
        if (pid == -1) {
                /* same as above */
                gnome_error_dialog (_("Bug buddy could not be run."));
        }
}

static void
zero_pointer(GtkObject *object, void **pointer)
{
	*pointer = NULL;
}

static void
command_about_box (GtkWidget *menuitem, gpointer data)
{
	static GtkWidget *about_box = NULL;

	if (about_box)
		gdk_window_raise(GTK_WIDGET(about_box)->window);
	else {
		const gchar *authors[] = {
			"Seth Alves",
			"Anders Carlsson",
			"Damon Chaplin",
			"Clifford R. Conover",
			"Miguel de Icaza",
			"Arturo Espinoza",
			"Larry Ewing",
			"Bertrand Guiheneuf",
			"Tuomas Kuosmanen",
			"Christopher J. Lahey",
			"Matthew Loper",
			"Federico Mena",
			"Eskil Heyn Olsen",
			"Nat Friedman",
			"Ettore Perazzoli",
			"Russell Steinthal",
			"Peter Teichman",
			"Chris Toshok",
			"Radek Doulik",
			"Dan Winship",
			"Michael Zucchi",
			NULL};

		about_box = gnome_about_new(_("Evolution"),
					    VERSION,
					    _("Copyright 1999, 2000 Helix Code, Inc."),
					    authors,
					    _("Evolution is a suite of groupware applications\n"
					      "for mail, calendaring, and contact management\n"
					      "within the GNOME desktop environment."),
					    NULL);
		gtk_signal_connect(GTK_OBJECT(about_box), "destroy",
				   GTK_SIGNAL_FUNC(zero_pointer), &about_box);
		gtk_widget_show(about_box);
	}
}



/* Unimplemented commands.  */



#define DEFINE_UNIMPLEMENTED(func)					\
static void								\
func (GtkWidget *widget, gpointer data)					\
{									\
	g_warning ("EShellView: %s: not implemented.", __FUNCTION__);	\
}									\

DEFINE_UNIMPLEMENTED (command_new_folder)
DEFINE_UNIMPLEMENTED (command_new_shortcut)
DEFINE_UNIMPLEMENTED (command_new_mail_message)
DEFINE_UNIMPLEMENTED (command_new_meeting_request)
DEFINE_UNIMPLEMENTED (command_new_contact)
DEFINE_UNIMPLEMENTED (command_new_task)
DEFINE_UNIMPLEMENTED (command_new_task_request)
DEFINE_UNIMPLEMENTED (command_new_journal_entry)
DEFINE_UNIMPLEMENTED (command_new_note)
DEFINE_UNIMPLEMENTED (command_open_selected_items)
DEFINE_UNIMPLEMENTED (command_save_as)
DEFINE_UNIMPLEMENTED (command_close_open_items)
DEFINE_UNIMPLEMENTED (command_toggle_shortcut_bar)
DEFINE_UNIMPLEMENTED (command_toggle_treeview)


/*
 * FIXME
 *
 * This menu is actually pretty dynamic, it changes de values of various entries
 * depending on the current data being displayed
 *
 * This is currently only a placeholder.  We need to figure what to do about this.
 */
static GnomeUIInfo menu_file_new [] = {

	{ GNOME_APP_UI_ITEM, N_("_Folder"),
	  NULL, command_new_folder, NULL,
	  NULL, 0, 0, 'e', GDK_CONTROL_MASK | GDK_SHIFT_MASK },

	{ GNOME_APP_UI_ITEM, N_("Evolution _Bar Shortcut"),
	  NULL, command_new_shortcut, NULL,
	  NULL, 0, 0, 'e', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	
	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, N_("_Mail message"),
	  N_("Composes a new mail message"), command_new_mail_message, NULL,
	  NULL, 0, 0, 'n', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Appointment"),
	  N_("Composes a new mail message"), command_new_mail_message, NULL,
	  NULL, 0, 0, 'a', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("Meeting Re_quest"), NULL,
	  command_new_meeting_request, NULL,
	  NULL, 0, 0, 'q', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Contact"), NULL,
	  command_new_contact, NULL,
	  NULL, 0, 0, 'c', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Task"), NULL,
	  command_new_task, NULL,
	  NULL, 0, 0, 'k', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("Task _Request"), NULL,
	  command_new_task_request, NULL,
	  NULL, 0, 0, 'u', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Journal Entry"), NULL,
	  command_new_journal_entry, NULL,
	  NULL, 0, 0, 'j', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Note"), NULL,
	  command_new_note, NULL,
	  NULL, 0, 0, 'o', GDK_CONTROL_MASK | GDK_SHIFT_MASK },

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_END
};

static GnomeUIInfo menu_file_open [] = {
	{ GNOME_APP_UI_ITEM, N_("_Selected Items"), NULL,
	  command_open_selected_items, NULL,
	  NULL, 0, 0, 'o', GDK_CONTROL_MASK },
	
	GNOMEUIINFO_END
};

static GnomeUIInfo menu_folder [] = {
	{ GNOME_APP_UI_ITEM, N_("_New Folder"), NULL,
	  command_new_folder, NULL,
	  NULL, 0, 0, 'e', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	
	GNOMEUIINFO_END
};

static GnomeUIInfo menu_file [] = {
	GNOMEUIINFO_SUBTREE_STOCK (N_("_New"), menu_file_new, GNOME_STOCK_MENU_NEW),
	GNOMEUIINFO_SUBTREE_STOCK (N_("_Open"), menu_file_open, GNOME_STOCK_MENU_NEW),
	GNOMEUIINFO_ITEM_NONE (N_("Clos_e All Items"), N_("Closes all the open items"), command_close_open_items),
	GNOMEUIINFO_SEPARATOR,
	
	GNOMEUIINFO_MENU_SAVE_AS_ITEM (command_save_as, NULL),
	
	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_SUBTREE (N_("_Folder"), menu_folder),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_MENU_EXIT_ITEM(command_quit, NULL),

	GNOMEUIINFO_END
};

static GnomeUIInfo menu_edit [] = {
	GNOMEUIINFO_END
};

static GnomeUIInfo menu_view [] = {
	{ GNOME_APP_UI_ITEM, N_("_Toggle Shortcut Bar"),
	  N_("Toggles the shortcut bar"), command_toggle_shortcut_bar, NULL,
	  NULL, 0, 0, 'n', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Toggle Treeview"),
	  N_("Toggles the tree view"), command_toggle_treeview, NULL,
	  NULL, 0, 0, 'n', GDK_CONTROL_MASK | GDK_SHIFT_MASK },		
	GNOMEUIINFO_END
};

static GnomeUIInfo menu_tools [] = {
	GNOMEUIINFO_END
};

static GnomeUIInfo menu_actions [] = {
	GNOMEUIINFO_END
};

static GnomeUIInfo menu_help [] = {
	GNOMEUIINFO_MENU_ABOUT_ITEM(command_about_box, NULL),
	{ GNOME_APP_UI_ITEM, N_("_Submit bug"),
	  N_("Submit bug-report via bug-buddy"), command_run_bugbuddy, NULL,
	  NULL, 0, 0, 'n', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	GNOMEUIINFO_END
};


/* Menu bar.  */

GnomeUIInfo e_shell_view_menu [] = {
	GNOMEUIINFO_MENU_FILE_TREE (menu_file),
	GNOMEUIINFO_MENU_EDIT_TREE (menu_edit),
	GNOMEUIINFO_MENU_VIEW_TREE (menu_view),

	/* FIXME: add Favorites here */

	{ GNOME_APP_UI_SUBTREE, N_("_Tools"), NULL, menu_tools },
	{ GNOME_APP_UI_SUBTREE, N_("_Actions"), NULL, menu_actions },
	GNOMEUIINFO_MENU_HELP_TREE (menu_help),

	/* FIXME: Should provide a help menu here; Bonobo needs it.  */

	GNOMEUIINFO_END
};
