/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-view-menu.c
 *
 * Copyright (C) 2000, 2001  Ximian, Inc.
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

/* FIXME: This file is a bit of a mess.  */

#include <config.h>

#include <glib.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-url.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>

#include <liboaf/liboaf.h>
#include <bonobo/bonobo-moniker-util.h>

#include "e-shell-folder-creation-dialog.h"
#include "e-shell-folder-selection-dialog.h"

#include "e-shell-constants.h"

#include "e-shell-view-menu.h"
#include "e-shell-importer.h"

#include "e-shell-folder-commands.h"

#include "evolution-shell-component-utils.h"


const char *authors[] = {
	"Seth Alves",
	"Anders Carlsson",
	"Damon Chaplin",
	"Clifford R. Conover",
	"Anna Dirks",
	"Miguel de Icaza",
	"Radek Doulik",
	"Arturo Espinoza",
	"Larry Ewing",
	"Nat Friedman",
	"Bertrand Guiheneuf",
	"Iain Holmes",
	"Tuomas Kuosmanen",
	"Christopher J. Lahey",
	"Matthew Loper",
	"Federico Mena",
	"Eskil Heyn Olsen",
	"Jesse Pavel",
	"Ettore Perazzoli",
	"JP Rosevear",
	"Jeffrey Stedfast",
        "Jakub Steiner",
	"Russell Steinthal",
	"Peter Teichman",
	"Chris Toshok",
	"Jon Trowbridge",
	"Peter Williams",
	"Dan Winship",
	"Michael Zucchi",
	NULL
};


/* EShellView callbacks.  */

static void
shortcut_bar_mode_changed_cb (EShellView *shell_view,
			      EShellViewSubwindowMode new_mode,
			      void *data)
{
	BonoboUIComponent *uic;
	const char *path;
	char *txt;

	if (new_mode == E_SHELL_VIEW_SUBWINDOW_HIDDEN)
		txt = "0";
	else
		txt = "1";

	path = (const char *) data;
	uic = e_shell_view_get_bonobo_ui_component (shell_view);

	bonobo_ui_component_set_prop (uic, path, "state", txt, NULL);
}

static void
folder_bar_mode_changed_cb (EShellView *shell_view,
			    EShellViewSubwindowMode new_mode,
			    void *data)
{
	BonoboUIComponent *uic;
	const char *path;
	char *txt;

	if (new_mode == E_SHELL_VIEW_SUBWINDOW_HIDDEN)
		txt = "0";
	else
		txt = "1";

	path = (const char *) data;
	uic = e_shell_view_get_bonobo_ui_component (shell_view);

	bonobo_ui_component_set_prop (uic, path, "state", txt, NULL);
}


/* Command callbacks.  */

static void
command_close (BonoboUIComponent *uih,
	       void *data,
	       const char *path)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	gtk_object_destroy (GTK_OBJECT (shell_view));
}

static void
command_quit (BonoboUIComponent *uih,
	      void *data,
	      const char *path)
{
	EShellView *shell_view;
	EShell *shell;

	shell_view = E_SHELL_VIEW (data);

	shell = e_shell_view_get_shell (shell_view);
	e_shell_quit (shell);
}

#if 0

static void
command_run_bugbuddy (BonoboUIComponent *uih,
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
                gnome_error_dialog (_("Bug buddy was not found in your $PATH."));
		return;
        }
        pid = gnome_execute_async (NULL, 4, args);
        g_free (args[0]);
        if (pid == -1) {
                gnome_error_dialog (_("Bug buddy could not be run."));
        }
}

#else

/* We have no mail interface in the Ximian bug tracker (yet), so Bug Buddy
   cannot talk to it.  For the time being, it's better to just fire up a
   browser window with bugzilla.ximian.com in it.  */

static void
command_submit_bug (BonoboUIComponent *uic,
		    void *data,
		    const char *path)
{
	gnome_url_show ("http://bugzilla.ximian.com");
}

#endif

static void
zero_pointer(GtkObject *object, void **pointer)
{
	*pointer = NULL;
}

static void
command_about_box (BonoboUIComponent *uih,
		   void *data,
		   const char *path)
{
	static GtkWidget *about_box = NULL;

	if (about_box) {
		gdk_window_raise(GTK_WIDGET(about_box)->window);
	} else {
		char *version;

		if (SUB_VERSION[0] == '\0')
			version = g_strdup (VERSION);
		else
			version = g_strdup_printf ("%s [%s]", VERSION, SUB_VERSION);

		about_box = gnome_about_new(_("Evolution"),
					    version,
					    _("Copyright 1999, 2000, 2001 Ximian, Inc."),
					    authors,
					    _("Evolution is a suite of groupware applications\n"
					      "for mail, calendaring, and contact management\n"
					      "within the GNOME desktop environment."),
					    NULL);
		gtk_signal_connect(GTK_OBJECT(about_box), "destroy",
				   GTK_SIGNAL_FUNC(zero_pointer), &about_box);
		gtk_widget_show(about_box);

		g_free (version);
	}
}

static void
command_help (BonoboUIComponent *uih,
	      void *data,
	      const char *path)
{
	char *url;

	url = g_strdup_printf ("ghelp:%s/gnome/help/evolution/C/%s",
			       EVOLUTION_DATADIR, (char *)data);
	gnome_url_show (url);
}

static void
command_toggle_folder_bar (BonoboUIComponent           *component,
			   const char                  *path,
			   Bonobo_UIComponent_EventType type,
			   const char                  *state,
			   gpointer                     user_data)
{
	EShellView *shell_view;
	EShellViewSubwindowMode mode;
	gboolean show;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	shell_view = E_SHELL_VIEW (user_data);

	show = atoi (state);
	if (show)
		mode = E_SHELL_VIEW_SUBWINDOW_STICKY;
	else
		mode = E_SHELL_VIEW_SUBWINDOW_HIDDEN;

	e_shell_view_set_folder_bar_mode (shell_view, mode);
}

static void
command_toggle_shortcut_bar (BonoboUIComponent           *component,
			     const char                  *path,
			     Bonobo_UIComponent_EventType type,
			     const char                  *state,
			     gpointer                     user_data)
{
	EShellView *shell_view;
	EShellViewSubwindowMode mode;
	gboolean show;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	shell_view = E_SHELL_VIEW (user_data);

	show = atoi (state);

	if (show)
		mode = E_SHELL_VIEW_SUBWINDOW_STICKY;
	else
		mode = E_SHELL_VIEW_SUBWINDOW_HIDDEN;

	e_shell_view_set_shortcut_bar_mode (shell_view, mode);
}


static void
command_new_folder (BonoboUIComponent *uih,
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
					     default_parent_folder,
					     NULL /* result_callback */,
					     NULL /* result_callback_data */);
}

static void
command_open_folder_in_new_window (BonoboUIComponent *uih,
				   gpointer          data,
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


/* Folder operations.  */

static void
command_move_folder (BonoboUIComponent *uih,
		     void *data,
		     const char *path)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);
	e_shell_command_move_folder (e_shell_view_get_shell (shell_view), shell_view);
}

static void
command_copy_folder (BonoboUIComponent *uih,
		     void *data,
		     const char *path)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);
	e_shell_command_copy_folder (e_shell_view_get_shell (shell_view), shell_view);
}

static void
command_add_folder_to_shortcut_bar (BonoboUIComponent *uih,
				    void *data,
				    const char *path)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);
	e_shell_command_add_to_shortcut_bar (e_shell_view_get_shell (shell_view), shell_view);
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
command_goto_folder (BonoboUIComponent *uih,
		     gpointer    data,
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
								       _("Select the folder that you want to open"),
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
command_create_folder (BonoboUIComponent *uih,
		       void *data,
		       const char *path)
{
	EShellView *shell_view;
	EShell *shell;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);

	e_shell_command_create_new_folder (shell, shell_view);
}

static void
command_xml_dump (gpointer    dummy,
		  EShellView *view)
{
	bonobo_window_dump (BONOBO_WINDOW (view), "On demand");
}


static void
command_work_offline (BonoboUIComponent *uih,
		      void *data,
		      const char *path)
{
	EShellView *shell_view;
	EShell *shell;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);

	g_warning ("Putting the shell offline");
	e_shell_go_offline (shell, shell_view);
}

static void
command_work_online (BonoboUIComponent *uih,
		     void *data,
		     const char *path)
{
	EShellView *shell_view;
	EShell *shell;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);

	g_warning ("Putting the shell online");
	e_shell_go_online (shell, shell_view);
}


/* Unimplemented commands.  */

#define DEFINE_UNIMPLEMENTED(func)					\
static void								\
func (BonoboUIComponent *uic, void *data, const char *path)		\
{									\
	g_warning ("e-shell-view-menu.c: %s: not implemented.", #func);	\
}									\

static void
command_new_mail_message (BonoboUIComponent *uih,
			  gpointer          data,
			  const char *path)
{
	CORBA_Environment ev;
	Bonobo_Unknown object;
	
	CORBA_exception_init (&ev);
	object = bonobo_get_object ("OAFIID:GNOME_Evolution_Mail_Composer!visible=1",
				    "Bonobo/Unknown", &ev);

	CORBA_exception_free (&ev);
}
	
DEFINE_UNIMPLEMENTED (command_new_shortcut)

DEFINE_UNIMPLEMENTED (command_new_contact)
DEFINE_UNIMPLEMENTED (command_new_task_request)

BonoboUIVerb new_verbs [] = {
	BONOBO_UI_VERB ("NewFolder", command_new_folder),
	BONOBO_UI_VERB ("NewShortcut", command_new_shortcut),
	BONOBO_UI_VERB ("NewMailMessage", command_new_mail_message),
		  
	BONOBO_UI_VERB ("NewAppointment", command_new_shortcut),
	BONOBO_UI_VERB ("NewContact", command_new_contact),
	BONOBO_UI_VERB ("NewTask", command_new_task_request),

	BONOBO_UI_VERB_END
};

BonoboUIVerb file_verbs [] = {
	BONOBO_UI_VERB ("FileImporter", (BonoboUIVerbFn) show_import_wizard),
	BONOBO_UI_VERB ("FileGoToFolder", command_goto_folder),
	BONOBO_UI_VERB ("FileCreateFolder", command_create_folder),
	BONOBO_UI_VERB ("FileClose", command_close),
	BONOBO_UI_VERB ("FileExit", command_quit),

	BONOBO_UI_VERB ("WorkOffline", command_work_offline),
	BONOBO_UI_VERB ("WorkOnline", command_work_online),

	BONOBO_UI_VERB_END
};

BonoboUIVerb folder_verbs [] = {
	BONOBO_UI_VERB ("OpenFolderInNewWindow", command_open_folder_in_new_window),
	BONOBO_UI_VERB ("MoveFolder", command_move_folder),
	BONOBO_UI_VERB ("CopyFolder", command_copy_folder),

	BONOBO_UI_VERB ("AddFolderToShortcutBar", command_add_folder_to_shortcut_bar),

	BONOBO_UI_VERB_END
};

BonoboUIVerb help_verbs [] = {
	BONOBO_UI_VERB_DATA ("HelpIndex", command_help, "evolution-guide/index.html"),
	BONOBO_UI_VERB_DATA ("HelpGetStarted", command_help, "evolution-guide/usage-mainwindow.html"),
	BONOBO_UI_VERB_DATA ("HelpUsingMail", command_help, "evolution-guide/usage-mail.html"),
	BONOBO_UI_VERB_DATA ("HelpUsingCalendar", command_help, "evolution-guide/usage-calendar.html"),
	BONOBO_UI_VERB_DATA ("HelpUsingContact", command_help, "evolution-guide/usage-contact.html"),
	BONOBO_UI_VERB_DATA ("HelpFAQ", command_help, "evolution-faq/index.html"),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/menu/File/New/Folder",	"folder.xpm"),
	E_PIXMAP ("/menu/File/Folder/Folder",	"folder.xpm"),
	E_PIXMAP ("/menu/File/FileImporter",	"import.xpm"),
	E_PIXMAP ("/menu/File/ToggleOffline",	"work_offline.xpm"),
	E_PIXMAP_END
};

static void
menu_do_misc (BonoboUIComponent *component,
	      EShellView        *shell_view)
{
	bonobo_ui_component_add_listener (component, "ViewShortcutBar",
					  command_toggle_shortcut_bar, shell_view);
	bonobo_ui_component_add_listener (component, "ViewFolderBar",
					  command_toggle_folder_bar, shell_view);

	bonobo_ui_component_add_verb (component, "HelpSubmitBug",
				      (BonoboUIVerbFn) command_submit_bug, shell_view);
	bonobo_ui_component_add_verb (component, "HelpAbout",
				      (BonoboUIVerbFn) command_about_box, shell_view);
	bonobo_ui_component_add_verb (component, "DebugDumpXml",
				      (BonoboUIVerbFn) command_xml_dump, shell_view);
}


/* The Work Online / Work Offline menu item.  */

static void
update_offline_menu_item (EShellView *shell_view,
			  EShellLineStatus line_status)
{
	BonoboUIComponent *ui_component;

	ui_component = e_shell_view_get_bonobo_ui_component (shell_view);

	switch (line_status) {
	case E_SHELL_LINE_STATUS_OFFLINE:
		bonobo_ui_component_set_prop (ui_component,
					      "/menu/File/ToggleOffline",
					      "label", _("Work online"), NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/menu/File/ToggleOffline",
					      "verb", "WorkOnline", NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/commands/WorkOnline",
					      "sensitive", "1", NULL);
		break;

	case E_SHELL_LINE_STATUS_ONLINE:
		bonobo_ui_component_set_prop (ui_component,
					      "/menu/File/ToggleOffline",
					      "label", _("Work offline"), NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/menu/File/ToggleOffline",
					      "verb", "WorkOffline", NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/commands/ToggleOffline",
					      "sensitive", "1", NULL);
		break;

	case E_SHELL_LINE_STATUS_GOING_OFFLINE:
		bonobo_ui_component_set_prop (ui_component,
					      "/menu/File/ToggleOffline",
					      "label", _("Work offline"), NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/menu/File/ToggleOffline",
					      "verb", "WorkOffline", NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/commands/ToggleOffline",
					      "sensitive", "0", NULL);
		break;

	default:
		g_assert_not_reached ();
	}
}

static void
shell_line_status_changed_cb (EShell *shell,
			      EShellLineStatus new_status,
			      void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	update_offline_menu_item (shell_view, new_status);
}


#define SHORTCUT_BAR_TOGGLE_PATH "/commands/ViewShortcutBar"
#define FOLDER_BAR_TOGGLE_PATH "/commands/ViewFolderBar"

void
e_shell_view_menu_setup (EShellView *shell_view)
{
	BonoboUIComponent *uic;
	EShell *shell;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	uic = e_shell_view_get_bonobo_ui_component (shell_view);
	shell = e_shell_view_get_shell (shell_view);

	bonobo_ui_component_add_verb_list_with_data (uic, file_verbs, shell_view);
	bonobo_ui_component_add_verb_list_with_data (uic, folder_verbs, shell_view);
	bonobo_ui_component_add_verb_list_with_data (uic, new_verbs, shell_view);

	bonobo_ui_component_add_verb_list (uic, help_verbs);

	menu_do_misc (uic, shell_view);

	e_pixmaps_update (uic, pixmaps);

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

	/* Set up the work online / work offline menu item.  */
	gtk_signal_connect (GTK_OBJECT (shell), "line_status_changed",
			    GTK_SIGNAL_FUNC (shell_line_status_changed_cb), shell_view);
	update_offline_menu_item (shell_view, e_shell_get_line_status (shell));
}
