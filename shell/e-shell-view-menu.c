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


/* EShellView callbacks.  */

static void
shortcut_bar_mode_changed_cb (EShellView *shell_view,
			      EShellViewSubwindowMode new_mode,
			      void *data)
{
	BonoboUIHandler *uih;
	const char *path;
	gboolean toggle_state;

	if (new_mode == E_SHELL_VIEW_SUBWINDOW_HIDDEN)
		toggle_state = FALSE;
	else
		toggle_state = TRUE;

	path = (const char *) data;
	uih = e_shell_view_get_bonobo_ui_handler (shell_view);

	bonobo_ui_handler_menu_set_toggle_state (uih, path, toggle_state);
}

static void
folder_bar_mode_changed_cb (EShellView *shell_view,
			    EShellViewSubwindowMode new_mode,
			    void *data)
{
	BonoboUIHandler *uih;
	const char *path;
	gboolean toggle_state;

	if (new_mode == E_SHELL_VIEW_SUBWINDOW_HIDDEN)
		toggle_state = FALSE;
	else
		toggle_state = TRUE;

	path = (const char *) data;
	uih = e_shell_view_get_bonobo_ui_handler (shell_view);

	bonobo_ui_handler_menu_set_toggle_state (uih, path, toggle_state);
}


/* Command callbacks.  */
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
			"Radek Doulik",
			"Arturo Espinoza",
			"Larry Ewing",
			"Nat Friedman",
			"Bertrand Guiheneuf",
			"Tuomas Kuosmanen",
			"Christopher J. Lahey",
			"Matthew Loper",
			"Federico Mena",
			"Eskil Heyn Olsen",
			"Ettore Perazzoli",
			"Russell Steinthal",
			"Peter Teichman",
			"Chris Toshok",
			"Dan Winship",
			"Michael Zucchi",
			"Jeffrey Stedfast",
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

static void
command_toggle_folder_bar (BonoboUIHandler *uih,
			   void *data,
			   const char *path)
{
	EShellView *shell_view;
	EShellViewSubwindowMode mode;
	gboolean show;

	shell_view = E_SHELL_VIEW (data);

	show = bonobo_ui_handler_menu_get_toggle_state (uih, path);
	if (show)
		mode = E_SHELL_VIEW_SUBWINDOW_STICKY;
	else
		mode = E_SHELL_VIEW_SUBWINDOW_HIDDEN;

	e_shell_view_set_folder_bar_mode (shell_view, mode);
}

static void
command_toggle_shortcut_bar (BonoboUIHandler *uih,
			     void *data,
			     const char *path)
{
	EShellView *shell_view;
	EShellViewSubwindowMode mode;
	gboolean show;

	shell_view = E_SHELL_VIEW (data);

	show = bonobo_ui_handler_menu_get_toggle_state (uih, path);

	if (show)
		mode = E_SHELL_VIEW_SUBWINDOW_STICKY;
	else
		mode = E_SHELL_VIEW_SUBWINDOW_HIDDEN;

	e_shell_view_set_shortcut_bar_mode (shell_view, mode);
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
	{ GNOME_APP_UI_TOGGLEITEM, N_("Show _shortcut bar"),
	  N_("Show the shortcut bar"), command_toggle_shortcut_bar, NULL,
	  NULL, 0, 0, 'n', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_TOGGLEITEM, N_("Show _folder bar"),
	  N_("Show the folder bar"), command_toggle_folder_bar, NULL,
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

static GnomeUIInfo menu [] = {
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


/* FIXME these must match the corresponding setup in the GnomeUIInfo and this sucks sucks.  */
#define SHORTCUT_BAR_TOGGLE_PATH "/View/Show shortcut bar"
#define FOLDER_BAR_TOGGLE_PATH "/View/Show folder bar"

void
e_shell_view_menu_setup (EShellView *shell_view)
{
	BonoboUIHandlerMenuItem *list;
	BonoboUIHandler *uih;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	uih = e_shell_view_get_bonobo_ui_handler (shell_view);

	list = bonobo_ui_handler_menu_parse_uiinfo_list_with_data (menu, shell_view);
	bonobo_ui_handler_menu_add_list (uih, "/", list);
	bonobo_ui_handler_menu_free_list (list);

	gtk_signal_connect (GTK_OBJECT (shell_view), "shortcut_bar_mode_changed",
			    GTK_SIGNAL_FUNC (shortcut_bar_mode_changed_cb),
			    SHORTCUT_BAR_TOGGLE_PATH);
	gtk_signal_connect (GTK_OBJECT (shell_view), "folder_bar_mode_changed",
			    GTK_SIGNAL_FUNC (folder_bar_mode_changed_cb),
			    FOLDER_BAR_TOGGLE_PATH);

	/* Initialize the toggles.  Yeah, this is, well, yuck.  */

	folder_bar_mode_changed_cb   (shell_view, e_shell_view_get_folder_bar_mode (shell_view),
				      FOLDER_BAR_TOGGLE_PATH);
	shortcut_bar_mode_changed_cb (shell_view, e_shell_view_get_shortcut_bar_mode (shell_view),
				      SHORTCUT_BAR_TOGGLE_PATH);
}
