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

#include "e-shell-folder-creation-dialog.h"
#include "e-shell-folder-selection-dialog.h"

#include "e-shell-constants.h"

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
command_quit (BonoboUIHandler *uih,
	      void *data,
	      const char *path)
{
	EShellView *shell_view;
	EShell *shell;

	shell_view = E_SHELL_VIEW (data);

	shell = e_shell_view_get_shell (shell_view);
	e_shell_quit (shell);
}

static void
command_run_bugbuddy (BonoboUIHandler *uih,
		      void *data,
		      const char *path)
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
command_about_box (BonoboUIHandler *uih,
		   void *data,
		   const char *path)
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
command_help (BonoboUIHandler *uih,
	      void *data,
	      const char *path)
{
	char *url;

	url = g_strdup_printf ("ghelp:%s/gnome/help/evolution/C/%s",
			       EVOLUTION_DATADIR, (char *)data);
	gnome_url_show (url);
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


static void
command_new_folder (BonoboUIHandler *uih,
		    void *data,
		    const char *path)
{
	EShellView *shell_view;
	EShell *shell;
	const char *current_uri;
	const char *default_parent_folder;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);
	current_uri = e_shell_view_get_current_uri (shell_view);

	if (strncmp (current_uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) == 0)
		default_parent_folder = current_uri + E_SHELL_URI_PREFIX_LEN;
	else
		default_parent_folder = NULL;

	e_shell_show_folder_creation_dialog (shell, GTK_WINDOW (shell_view),
					     default_parent_folder);
}

static void
command_new_view (BonoboUIHandler *uih,
		  void *data,
		  const char *path)
{
	EShellView *shell_view;
	EShell *shell;
	const char *current_uri;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);
	current_uri = e_shell_view_get_current_uri (shell_view);

	e_shell_new_view (shell, current_uri);
}


/* Going to a folder.  */

static void
folder_selection_dialog_cancelled_cb (EShellFolderSelectionDialog *folder_selection_dialog,
				      void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	gtk_widget_destroy (GTK_WIDGET (folder_selection_dialog));
}

static void
folder_selection_dialog_folder_selected_cb (EShellFolderSelectionDialog *folder_selection_dialog,
					    const char *path,
					    void *data)
{
	if (path != NULL) {
		EShellView *shell_view;
		char *uri;

		shell_view = E_SHELL_VIEW (data);

		uri = g_strconcat (E_SHELL_URI_PREFIX, path, NULL);
		e_shell_view_display_uri (shell_view, uri);
		g_free (uri);
	}
}

static void
command_goto_folder (BonoboUIHandler *uih,
		     void *data,
		     const char *path)
{
	GtkWidget *folder_selection_dialog;
	EShellView *shell_view;
	EShell *shell;
	const char *current_uri;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);

	current_uri = e_shell_view_get_current_uri (shell_view);

	folder_selection_dialog = e_shell_folder_selection_dialog_new (shell,
								       _("Go to folder..."),
								       current_uri,
								       NULL);

	gtk_window_set_transient_for (GTK_WINDOW (folder_selection_dialog), GTK_WINDOW (shell_view));

	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "folder_selected",
			    GTK_SIGNAL_FUNC (folder_selection_dialog_folder_selected_cb), shell_view);
	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "cancelled",
			    GTK_SIGNAL_FUNC (folder_selection_dialog_cancelled_cb), shell_view);

	gtk_widget_show (folder_selection_dialog);
}

static void
command_create_folder (BonoboUIHandler *uih,
		       void *data,
		       const char *path)
{
	EShellView *shell_view;
	EShell *shell;
	const char *current_uri;
	const char *default_folder;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);

	current_uri = e_shell_view_get_current_uri (shell_view);

	if (strncmp (current_uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) == 0)
		default_folder = current_uri + E_SHELL_URI_PREFIX_LEN;
	else
		default_folder = NULL;

	e_shell_show_folder_creation_dialog (shell, GTK_WINDOW (shell_view), default_folder);
}


/* Unimplemented commands.  */

#define DEFINE_UNIMPLEMENTED(func)					\
static void								\
func (BonoboUIHandler *uih, void *data, const char *path)		\
{									\
	g_warning ("EShellView: %s: not implemented.", __FUNCTION__);	\
}									\

DEFINE_UNIMPLEMENTED (command_new_shortcut)
DEFINE_UNIMPLEMENTED (command_new_mail_message)
DEFINE_UNIMPLEMENTED (command_new_contact)
DEFINE_UNIMPLEMENTED (command_new_task_request)


static void
menu_create_file_new (BonoboUIHandler *uih,
		      void *data)
{
	bonobo_ui_handler_menu_new_subtree (uih, "/File/New",
					    _("_New"),
					    NULL, -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					    0, 0);

	bonobo_ui_handler_menu_new_item (uih, "/File/New/View",
					 _("_View"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 'v', GDK_CONTROL_MASK | GDK_SHIFT_MASK,
					 command_new_view, data);
	bonobo_ui_handler_menu_new_item (uih, "/File/New/View",
					 _("_Folder"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 'f', GDK_CONTROL_MASK | GDK_SHIFT_MASK,
					 command_new_folder, data);
	bonobo_ui_handler_menu_new_item (uih, "/File/New/Evolution bar shortcut",
					 _("Evolution bar _shortcut"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 's', GDK_CONTROL_MASK | GDK_SHIFT_MASK,
					 command_new_shortcut, data);

	bonobo_ui_handler_menu_new_separator (uih, "/File/New/Separator1", -1);
	
	bonobo_ui_handler_menu_new_item (uih, "/File/New/Mail message",
					 _("_Mail message (FIXME)"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 'm', GDK_CONTROL_MASK | GDK_SHIFT_MASK,
					 command_new_mail_message, data);
	bonobo_ui_handler_menu_new_item (uih, "/File/New/Appointment",
					 _("_Appointment (FIXME)"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 'a', GDK_CONTROL_MASK | GDK_SHIFT_MASK,
					 command_new_shortcut, data);
	bonobo_ui_handler_menu_new_item (uih, "/File/New/Contact",
					 _("_Contact (FIXME)"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 'c', GDK_CONTROL_MASK | GDK_SHIFT_MASK,
					 command_new_contact, data);
	bonobo_ui_handler_menu_new_item (uih, "/File/New/Contact",
					 _("_Task (FIXME)"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 't', GDK_CONTROL_MASK | GDK_SHIFT_MASK,
					 command_new_task_request, data);
}

static void
menu_create_file (BonoboUIHandler *uih,
		  void *data)
{
	bonobo_ui_handler_menu_new_subtree (uih, "/File",
					    _("_File"),
					    NULL, -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					    0, 0);

	menu_create_file_new (uih, data);

	bonobo_ui_handler_menu_new_separator (uih, "/File/Separator1", -1);

	bonobo_ui_handler_menu_new_item (uih, "/File/Go to folder",
					 _("_Go to folder..."),
					 _("Display a different folder"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0,
					 command_goto_folder, data);

	bonobo_ui_handler_menu_new_item (uih, "/File/Create new folder",
					 _("_Create new folder..."),
					 _("Create a new folder"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0,
					 command_create_folder, data);

	bonobo_ui_handler_menu_new_placeholder (uih, "/File/<Print Placeholder>");

	bonobo_ui_handler_menu_new_separator (uih, "/File/Separator2", -1);

	bonobo_ui_handler_menu_new_item (uih, "/File/Exit",
					 _("E_xit..."),
					 _("Create a new folder"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_EXIT,
					 0, 0,
					 command_quit, data);
}

static void
menu_create_edit (BonoboUIHandler *uih,
		  void *data)
{
	bonobo_ui_handler_menu_new_subtree (uih, "/Edit",
					    _("_Edit"),
					    NULL, -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					    0, 0);
}

static void
menu_create_view (BonoboUIHandler *uih,
		  void *data)
{
	bonobo_ui_handler_menu_new_subtree (uih, "/View",
					    _("_View"),
					    NULL, -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					    0, 0);

	bonobo_ui_handler_menu_new_toggleitem (uih, "/View/Show shortcut bar",
					       _("Show _shortcut bar"),
					       _("Show the shortcut bar"),
					       -1,
					       0, 0,
					       command_toggle_shortcut_bar, data);
	bonobo_ui_handler_menu_new_toggleitem (uih, "/View/Show folder bar",
					       _("Show _folder bar"),
					       _("Show the folder bar"),
					       -1,
					       0, 0,
					       command_toggle_folder_bar, data);
}

static void
menu_create_tools (BonoboUIHandler *uih,
		   void *data)
{
	bonobo_ui_handler_menu_new_subtree (uih, "/Tools",
					    _("_Tools"),
					    NULL, -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					    0, 0);
}

static void
menu_create_actions (BonoboUIHandler *uih,
		     void *data)
{
	bonobo_ui_handler_menu_new_subtree (uih, "/Actions",
					    _("_Actions"),
					    NULL, -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					    0, 0);
}

static void
menu_create_help (BonoboUIHandler *uih,
		  void *data)
{
	bonobo_ui_handler_menu_new_subtree (uih, "/Help",
					    _("_Help"),
					    NULL, -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					    0, 0);

	bonobo_ui_handler_menu_new_item (uih, "/Help/Help index",
					 _("Help _index"),
					 NULL,
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0,
					 command_help, "index.html");
	bonobo_ui_handler_menu_new_item (uih, "/Help/Getting started",
					 _("Getting _started"),
					 NULL,
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0,
					 command_help, "usage-mainwindow.html");
	bonobo_ui_handler_menu_new_item (uih, "/Help/Using the mailer",
					 _("Using the _mailer"),
					 NULL,
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0,
					 command_help, "usage-mail.html");
	bonobo_ui_handler_menu_new_item (uih, "/Help/Using the calendar",
					 _("Using the _calendar"),
					 NULL,
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0,
					 command_help, "usage-calendar.html");
	bonobo_ui_handler_menu_new_item (uih, "/Help/Using the contact manager",
					 _("Using the c_ontact manager"),
					 NULL,
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0,
					 command_help, "usage-contact.html");

	bonobo_ui_handler_menu_new_separator (uih, "/Help/Separator1", -1);

	bonobo_ui_handler_menu_new_item (uih, "/Help/Submit bug report",
					 _("_Submit bug report"),
					 _("Submit bug report using Bug Buddy"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0,
					 command_run_bugbuddy, data);

	bonobo_ui_handler_menu_new_separator (uih, "/Help/Separator2", -1);

	bonobo_ui_handler_menu_new_item (uih, "/Help/About Evolution",
					 _("_About Evolution..."),
					 _("Show information about Evolution"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0,
					 command_about_box, data);
}


/* FIXME these must match the corresponding setup in the GnomeUIInfo and this sucks sucks.  */
#define SHORTCUT_BAR_TOGGLE_PATH "/View/Show shortcut bar"
#define FOLDER_BAR_TOGGLE_PATH "/View/Show folder bar"

void
e_shell_view_menu_setup (EShellView *shell_view)
{
	BonoboUIHandler *uih;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	uih = e_shell_view_get_bonobo_ui_handler (shell_view);

	menu_create_file (uih, shell_view);
	menu_create_edit (uih, shell_view);
	menu_create_view (uih, shell_view);
	menu_create_tools (uih, shell_view);
	menu_create_actions (uih, shell_view);
	menu_create_help (uih, shell_view);

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
