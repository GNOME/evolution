/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-view-menu.c
 *
 * Copyright (C) 2000, 2001  Ximian, Inc.
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
 * Authors:
 *   Miguel de Icaza
 *   Ettore Perazzoli
 */

/* FIXME: This file is a bit of a mess.  */

#include <config.h>

#include <glib.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-help.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-url.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>

#include <liboaf/liboaf.h>
#include <bonobo/bonobo-moniker-util.h>

#include <gal/widgets/e-gui-utils.h>

#include "e-shell-folder-creation-dialog.h"
#include "e-shell-folder-selection-dialog.h"

#include "e-shell-constants.h"

#include "e-shell-view-menu.h"
#include "e-shell-importer.h"
#include "e-shell-about-box.h"

#include "e-shell-folder-commands.h"

#include "evolution-shell-component-utils.h"


/* Utility functions.  */

static const char *
get_path_for_folder_op (EShellView *shell_view)
{
	const char *path;

	path = e_shell_view_get_folder_bar_right_click_path (shell_view);
	if (path != NULL)
		return path;

	return e_shell_view_get_current_path (shell_view);
}


/* EShellView callbacks.  */

static void
shortcut_bar_visibility_changed_cb (EShellView *shell_view,
				    gboolean visible,
				    void *data)
{
	BonoboUIComponent *uic;
	const char *path;
	const char *txt;

	if (visible)
		txt = "1";
	else
		txt = "0";

	path = (const char *) data;
	uic = e_shell_view_get_bonobo_ui_component (shell_view);

	bonobo_ui_component_set_prop (uic, path, "state", txt, NULL);
}

static void
folder_bar_visibility_changed_cb (EShellView *shell_view,
				  gboolean visible,
				  void *data)
{
	BonoboUIComponent *uic;
	const char *path;
	const char *txt;

	if (visible)
		txt = "1";
	else
		txt = "0";

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
	e_shell_destroy_all_views (shell);
}

static void
command_submit_bug (BonoboUIComponent *uih,
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

        if (pid == -1)
                gnome_error_dialog (_("Bug buddy could not be run."));
}

static int
about_box_event_callback (GtkWidget *widget,
			  GdkEvent *event,
			  void *data)
{
	GtkWidget **widget_pointer;

	widget_pointer = (GtkWidget **) data;

	gtk_widget_destroy (GTK_WIDGET (*widget_pointer));
	*widget_pointer = NULL;

	return TRUE;
}

static void
command_about_box (BonoboUIComponent *uih,
		   void *data,
		   const char *path)
{
	static GtkWidget *about_box_window = NULL;
	GtkWidget *about_box;

	if (about_box_window != NULL) {
		gdk_window_raise (about_box_window->window);
		return;
	}

	about_box = e_shell_about_box_new ();
	gtk_widget_show (about_box);

	about_box_window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_window_set_policy (GTK_WINDOW (about_box_window), FALSE, FALSE, FALSE);
	gtk_signal_connect (GTK_OBJECT (about_box_window), "button_press_event",
			    GTK_SIGNAL_FUNC (about_box_event_callback), &about_box_window);
	gtk_signal_connect (GTK_OBJECT (about_box_window), "delete_event",
			    GTK_SIGNAL_FUNC (about_box_event_callback), &about_box_window);

	gtk_window_set_transient_for (GTK_WINDOW (about_box_window), GTK_WINDOW (data));
	gtk_window_set_title (GTK_WINDOW (about_box_window), _("About Ximian Evolution"));
	gtk_container_add (GTK_CONTAINER (about_box_window), about_box);
	gtk_widget_show (about_box_window);
}

static void
command_help_faq (BonoboUIComponent *uih,
		  void *data,
		  const char *path)
{
	gnome_url_show ("http://www.ximian.com/apps/evolution-faq.html");
}

static void
command_toggle_folder_bar (BonoboUIComponent           *component,
			   const char                  *path,
			   Bonobo_UIComponent_EventType type,
			   const char                  *state,
			   gpointer                     user_data)
{
	EShellView *shell_view;
	gboolean show;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	shell_view = E_SHELL_VIEW (user_data);
	show = atoi (state);

	e_shell_view_show_folder_bar (shell_view, show);
}

static void
command_toggle_shortcut_bar (BonoboUIComponent           *component,
			     const char                  *path,
			     Bonobo_UIComponent_EventType type,
			     const char                  *state,
			     gpointer                     user_data)
{
	EShellView *shell_view;
	gboolean show;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	shell_view = E_SHELL_VIEW (user_data);

	show = atoi (state);

	e_shell_view_show_shortcut_bar (shell_view, show);
}


static void
command_new_folder (BonoboUIComponent *uih,
		    void *data,
		    const char *path)
{
	EShellView *shell_view;
	EShell *shell;
	
	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);

	e_shell_show_folder_creation_dialog (shell, GTK_WINDOW (shell_view),
					     get_path_for_folder_op (shell_view),
					     NULL,
					     NULL /* result_callback */,
					     NULL /* result_callback_data */);
}

static void
command_activate_view (BonoboUIComponent *uih,
		       void *data,
		       const char *path)
{
	EShellView *shell_view;
	char *uri;

	shell_view = E_SHELL_VIEW (data);

	uri = g_strconcat (E_SHELL_URI_PREFIX, get_path_for_folder_op (shell_view), NULL);
	e_shell_view_display_uri (shell_view, uri);
	g_free (uri);
}

static void
command_open_folder_in_new_window (BonoboUIComponent *uih,
				   gpointer          data,
				   const char *path)
{
	EShellView *shell_view, *new_view;
	EShell *shell;
	char *uri;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);

	uri = g_strconcat (E_SHELL_URI_PREFIX, get_path_for_folder_op (shell_view), NULL);
	new_view = e_shell_create_view (shell, uri, shell_view);
	g_free (uri);

	e_shell_view_show_shortcut_bar (new_view, FALSE);
	e_shell_view_show_folder_bar (new_view, FALSE);
}


/* Folder operations.  */

static void
command_move_folder (BonoboUIComponent *uih,
		     void *data,
		     const char *path)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);
	e_shell_command_move_folder (e_shell_view_get_shell (shell_view), shell_view,
				     get_path_for_folder_op (shell_view));
}

static void
command_copy_folder (BonoboUIComponent *uih,
		     void *data,
		     const char *path)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);
	e_shell_command_copy_folder (e_shell_view_get_shell (shell_view), shell_view,
				     get_path_for_folder_op (shell_view));
}

static void
command_delete_folder (BonoboUIComponent *uih,
		       void *data,
		       const char *path)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);
	e_shell_command_delete_folder (e_shell_view_get_shell (shell_view), shell_view,
				       get_path_for_folder_op (shell_view));
}

static void
command_rename_folder (BonoboUIComponent *uih,
		       void *data,
		       const char *path)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);
	e_shell_command_rename_folder (e_shell_view_get_shell (shell_view), shell_view,
				       get_path_for_folder_op (shell_view));
}

static void
command_add_folder_to_shortcut_bar (BonoboUIComponent *uih,
				    void *data,
				    const char *path)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);
	e_shell_command_add_to_shortcut_bar (e_shell_view_get_shell (shell_view), shell_view,
					     get_path_for_folder_op (shell_view));
}


/* Going to a folder.  */

static void
goto_folder_dialog_cancelled_cb (EShellFolderSelectionDialog *folder_selection_dialog,
				 void *data)
{
	gtk_widget_destroy (GTK_WIDGET (folder_selection_dialog));
}

static void
goto_folder_dialog_folder_selected_cb (EShellFolderSelectionDialog *folder_selection_dialog,
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
								       NULL, NULL);

	gtk_window_set_transient_for (GTK_WINDOW (folder_selection_dialog), GTK_WINDOW (shell_view));

	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "cancelled",
			    GTK_SIGNAL_FUNC (goto_folder_dialog_cancelled_cb), shell_view);
	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "folder_selected",
			    GTK_SIGNAL_FUNC (goto_folder_dialog_folder_selected_cb), shell_view);

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

	e_shell_command_create_new_folder (shell, shell_view, get_path_for_folder_op (shell_view));
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

	g_message ("Putting the shell offline");
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

	g_message ("Putting the shell online");
	e_shell_go_online (shell, shell_view);
}


static void
new_shortcut_dialog_cancelled_cb (EShellFolderSelectionDialog *folder_selection_dialog,
				  void *data)
{
	gtk_widget_destroy (GTK_WIDGET (folder_selection_dialog));
}

static void
new_shortcut_dialog_folder_selected_cb (EShellFolderSelectionDialog *folder_selection_dialog,
					const char *path,
					void *data)
{
	EShellView *shell_view;
	EShell *shell;
	EShortcuts *shortcuts;
	EFolder *folder;
	int group_num;
	char *evolution_uri;

	if (path == NULL)
		return;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);
	shortcuts = e_shell_get_shortcuts (shell);

	folder = e_storage_set_get_folder (e_shell_get_storage_set (shell), path);
	if (folder == NULL)
		return;

	group_num = e_shell_view_get_current_shortcuts_group_num (shell_view);

	evolution_uri = g_strconcat (E_SHELL_URI_PREFIX, path, NULL);

	/* FIXME: I shouldn't have to set the type here.  Maybe.  */
	e_shortcuts_add_shortcut (shortcuts, group_num, -1, evolution_uri, NULL, e_folder_get_unread_count (folder), e_folder_get_type_string (folder));

	g_free (evolution_uri);

	gtk_widget_destroy (GTK_WIDGET (folder_selection_dialog));
}

static void
command_new_shortcut (BonoboUIComponent *uih,
		      void *data,
		      const char *path)
{
	EShellView *shell_view;
	GtkWidget *folder_selection_dialog;

	shell_view = E_SHELL_VIEW (data);

	folder_selection_dialog = e_shell_folder_selection_dialog_new (e_shell_view_get_shell (shell_view),
								       _("Create a new shortcut"),
								       _("Select the folder you want the shortcut to point to:"),
								       e_shell_view_get_current_uri (shell_view),
								       NULL, NULL);
	e_shell_folder_selection_dialog_set_allow_creation (E_SHELL_FOLDER_SELECTION_DIALOG (folder_selection_dialog),
							    FALSE);

	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "cancelled",
			    GTK_SIGNAL_FUNC (new_shortcut_dialog_cancelled_cb), shell_view);
	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "folder_selected",
			    GTK_SIGNAL_FUNC (new_shortcut_dialog_folder_selected_cb), shell_view);

	gtk_widget_show (folder_selection_dialog);
}
	

/* Tools menu.  */

static void
command_pilot_settings (BonoboUIComponent *uih,
			void *data,
			const char *path)
{
        char *args[] = {
                "gpilotd-control-applet",
                NULL
        };
        int pid;

        args[0] = gnome_is_program_in_path ("gpilotd-control-applet");
        if (!args[0]) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("The GNOME Pilot tools do not appear to be installed on this system."));
		return;
        }

        pid = gnome_execute_async (NULL, 4, args);
        g_free (args[0]);

        if (pid == -1)
                e_notice (NULL, GNOME_MESSAGE_BOX_ERROR, _("Error executing %s."), args[0]);
}


BonoboUIVerb new_verbs [] = {
	BONOBO_UI_VERB ("NewFolder", command_new_folder),
	BONOBO_UI_VERB ("NewShortcut", command_new_shortcut),
		  
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
	BONOBO_UI_VERB ("ActivateView", command_activate_view),
	BONOBO_UI_VERB ("OpenFolderInNewWindow", command_open_folder_in_new_window),
	BONOBO_UI_VERB ("MoveFolder", command_move_folder),
	BONOBO_UI_VERB ("CopyFolder", command_copy_folder),

	BONOBO_UI_VERB ("DeleteFolder", command_delete_folder),
	BONOBO_UI_VERB ("RenameFolder", command_rename_folder),

	BONOBO_UI_VERB ("AddFolderToShortcutBar", command_add_folder_to_shortcut_bar),

	BONOBO_UI_VERB_END
};

BonoboUIVerb tools_verbs[] = {
	BONOBO_UI_VERB ("PilotSettings", command_pilot_settings),

	BONOBO_UI_VERB_END
};

BonoboUIVerb help_verbs [] = {
	BONOBO_UI_VERB_DATA ("HelpFAQ", command_help_faq, NULL),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/menu/File/New/Folder",	"folder.xpm"),
	E_PIXMAP ("/menu/File/Folder/Folder",	"folder.xpm"),
	E_PIXMAP ("/menu/File/FileImporter",	"import.xpm"),
	E_PIXMAP ("/menu/File/ToggleOffline",	"work_offline.xpm"),

	E_PIXMAP_END
};

static EPixmap offline_pixmaps [] = {
	E_PIXMAP ("/menu/File/ToggleOffline", "work_offline.xpm"),

	E_PIXMAP_END
};

static EPixmap online_pixmaps [] = {
	E_PIXMAP ("/menu/File/ToggleOffline", "work_online-16.png"),
	
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
					      "label", _("_Work Online"), NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/menu/File/ToggleOffline",
					      "verb", "WorkOnline", NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/commands/ToggleOffline",
					      "sensitive", "1", NULL);
		e_pixmaps_update (ui_component, online_pixmaps);
		break;

	case E_SHELL_LINE_STATUS_ONLINE:
		bonobo_ui_component_set_prop (ui_component,
					      "/menu/File/ToggleOffline",
					      "label", _("_Work Offline"), NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/menu/File/ToggleOffline",
					      "verb", "WorkOffline", NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/commands/ToggleOffline",
					      "sensitive", "1", NULL);
		e_pixmaps_update (ui_component, offline_pixmaps);
		break;

	case E_SHELL_LINE_STATUS_GOING_OFFLINE:
		bonobo_ui_component_set_prop (ui_component,
					      "/menu/File/ToggleOffline",
					      "label", _("Work Offline"), NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/menu/File/ToggleOffline",
					      "verb", "WorkOffline", NULL);
		bonobo_ui_component_set_prop (ui_component,
					      "/commands/ToggleOffline",
					      "sensitive", "0", NULL);
		e_pixmaps_update (ui_component, offline_pixmaps);
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

	bonobo_ui_component_add_verb_list_with_data (uic, tools_verbs, shell_view);

	bonobo_ui_component_add_verb_list (uic, help_verbs);

	menu_do_misc (uic, shell_view);

	e_pixmaps_update (uic, pixmaps);

	gtk_signal_connect (GTK_OBJECT (shell_view), "shortcut_bar_visibility_changed",
			    GTK_SIGNAL_FUNC (shortcut_bar_visibility_changed_cb),
			    SHORTCUT_BAR_TOGGLE_PATH);
	gtk_signal_connect (GTK_OBJECT (shell_view), "folder_bar_visibility_changed",
			    GTK_SIGNAL_FUNC (folder_bar_visibility_changed_cb),
			    FOLDER_BAR_TOGGLE_PATH);

	/* Initialize the toggles.  Yeah, this is, well, yuck.  */
	folder_bar_visibility_changed_cb   (shell_view, e_shell_view_folder_bar_shown (shell_view),
					    FOLDER_BAR_TOGGLE_PATH);
	shortcut_bar_visibility_changed_cb (shell_view, e_shell_view_shortcut_bar_shown (shell_view),
					    SHORTCUT_BAR_TOGGLE_PATH);

	/* Set up the work online / work offline menu item.  */
	gtk_signal_connect_while_alive (GTK_OBJECT (shell), "line_status_changed",
					GTK_SIGNAL_FUNC (shell_line_status_changed_cb), shell_view,
					GTK_OBJECT (shell_view));
	update_offline_menu_item (shell_view, e_shell_get_line_status (shell));
}
