/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-window-commands.c
 *
 * Copyright (C) 2003  Ettore Perazzoli
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

#include "e-shell-window-commands.h"

#include "e-shell-about-box.h"
#include "e-shell-importer.h"
#include "e-shell-window.h"

#include "evolution-shell-component-utils.h"

#include "e-util/e-dialog-utils.h"
#include "e-util/e-passwords.h"

#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-url.h>
#include <libgnome/gnome-i18n.h>

#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>

#include <bonobo/bonobo-ui-component.h>


/* Utility functions.  */

static void
launch_pilot_settings (const char *extra_arg)
{
        char *args[] = {
                "gpilotd-control-applet",
		(char *) extra_arg,
		NULL
        };
        int pid;

        args[0] = g_find_program_in_path ("gpilotd-control-applet");
        if (!args[0]) {
		e_notice (NULL, GTK_MESSAGE_ERROR,
			  _("The GNOME Pilot tools do not appear to be installed on this system."));
		return;
        }

        pid = gnome_execute_async (NULL, extra_arg ? 2 : 1, args);
        g_free (args[0]);

        if (pid == -1)
                e_notice (NULL, GTK_MESSAGE_ERROR, _("Error executing %s."), args[0]);
}


/* Command callbacks.  */

static void
command_import (BonoboUIComponent *uih,
		EShellWindow *window,
		const char *path)
{
	e_shell_importer_start_import (window);
}

static void
command_close (BonoboUIComponent *uih,
	       EShellWindow *window,
	       const char *path)
{
	if (e_shell_request_close_window (e_shell_window_peek_shell (window), window))
		gtk_widget_destroy (GTK_WIDGET (window));
}

static void
command_quit (BonoboUIComponent *uih,
	      EShellWindow *window,
	      const char *path)
{
	EShell *shell = e_shell_window_peek_shell (window);

	e_shell_quit(shell);
}

static void
command_submit_bug (BonoboUIComponent *uih,
		    EShellWindow *window,
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

        args[0] = g_find_program_in_path ("bug-buddy");
        if (!args[0]) {
                e_notice (NULL, GTK_MESSAGE_ERROR, _("Bug buddy is not installed."));
		return;
        }

        pid = gnome_execute_async (NULL, 4, args);
        g_free (args[0]);

        if (pid == -1)
                e_notice (NULL, GTK_MESSAGE_ERROR, _("Bug buddy could not be run."));
}

static int
about_box_event_callback (GtkWidget *widget,
			  GdkEvent *event,
			  GtkWidget **widget_pointer)
{
	gtk_widget_destroy (GTK_WIDGET (*widget_pointer));
	*widget_pointer = NULL;

	return TRUE;
}

static void
command_about_box (BonoboUIComponent *uih,
		   EShellWindow *window,
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

	about_box_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint (GTK_WINDOW (about_box_window), GDK_WINDOW_TYPE_HINT_DIALOG);
	
	gtk_window_set_resizable (GTK_WINDOW (about_box_window), FALSE);
	g_signal_connect (about_box_window, "key_press_event",
			  G_CALLBACK (about_box_event_callback), &about_box_window);
	g_signal_connect (about_box_window, "button_press_event",
			  G_CALLBACK (about_box_event_callback), &about_box_window);
	g_signal_connect (about_box_window, "delete_event",
			  G_CALLBACK (about_box_event_callback), &about_box_window);

	gtk_window_set_transient_for (GTK_WINDOW (about_box_window), GTK_WINDOW (window));
	gtk_window_set_title (GTK_WINDOW (about_box_window), _("About Ximian Evolution"));
	gtk_container_add (GTK_CONTAINER (about_box_window), about_box);
	gtk_widget_show (about_box_window);
}

static void
command_help_faq (BonoboUIComponent *uih,
		  EShellWindow *window,
		  const char *path)
{
	gnome_url_show ("http://www.ximian.com/apps/evolution-faq.html", NULL);	/* FIXME use the error */
}

static void
command_quick_reference (BonoboUIComponent *uih,
			 EShellWindow *window,
			 const char *path)
{
	char *quickref;
	char *uri;
	char *command;
	GString *str;
	GnomeVFSMimeApplication *app;
	const GList *lang_list = gnome_i18n_get_language_list ("LC_MESSAGES");

	for (; lang_list != NULL; lang_list = lang_list->next) {
		const char *lang = lang_list->data;

		/* This has to be a valid language AND a language with
		 * no encoding postfix.  The language will come up without
		 * encoding next */
		if (lang == NULL || strchr (lang, '.') != NULL)
			continue;

		quickref = g_build_filename (EVOLUTION_HELPDIR, "quickref", lang, "quickref.pdf", NULL);
		if (g_file_test (quickref, G_FILE_TEST_EXISTS)) {
			app = gnome_vfs_mime_get_default_application ("application/pdf");
			if (app) {
				str = g_string_new ("");
				str = g_string_append (str, app->command);

				switch (app->expects_uris) {
				case GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS:
					uri = g_strconcat ("file://", quickref, NULL);
					g_string_append_printf (str, " %s", uri);
					g_free (uri);
					break;
				case GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_PATHS:
				case GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS_FOR_NON_FILES:
					g_string_append_printf (str, " %s", quickref);
					break;
				}

				command = g_string_free (str, FALSE);
				if (command != NULL &&
				!g_spawn_command_line_async (command, NULL)) {
					g_warning ("Could not launch %s", command);
				}

				g_free (command);
				gnome_vfs_mime_application_free (app);
			}

			g_free (quickref);
			return;
		}

		g_free (quickref);
	}
}


static void
command_work_offline (BonoboUIComponent *uih,
		      EShellWindow *window,
		      const char *path)
{
	e_shell_go_offline (e_shell_window_peek_shell (window), window);
}

static void
command_work_online (BonoboUIComponent *uih,
		     EShellWindow *window,
		     const char *path)
{
	e_shell_go_online (e_shell_window_peek_shell (window), window);
}

static void
command_open_new_window (BonoboUIComponent *uih,
			 EShellWindow *window,
			 const char *path)
{
	e_shell_create_window (e_shell_window_peek_shell (window),
			       e_shell_window_peek_current_component_id (window),
			       window);
}


/* Actions menu.  */

static void
command_send_receive (BonoboUIComponent *uih,
		      EShellWindow *window,
		      const char *path)
{
	e_shell_send_receive (e_shell_window_peek_shell (window));
}

static void
command_forget_passwords (BonoboUIComponent *ui_component,
			  void *data,
			  const char *path)
{
	e_passwords_forget_passwords();
}

/* Tools menu.  */

static void
command_settings (BonoboUIComponent *uih,
		  EShellWindow *window,
		  const char *path)
{
	e_shell_window_show_settings (window);
}

static void
command_pilot_settings (BonoboUIComponent *uih,
			EShellWindow *window,
			const char *path)
{
	launch_pilot_settings (NULL);
}


static BonoboUIVerb file_verbs [] = {
	BONOBO_UI_VERB ("FileImporter", (BonoboUIVerbFn) command_import),
	BONOBO_UI_VERB ("FileClose", (BonoboUIVerbFn) command_close),
	BONOBO_UI_VERB ("FileExit", (BonoboUIVerbFn) command_quit),

	BONOBO_UI_VERB ("WorkOffline", (BonoboUIVerbFn) command_work_offline),
	BONOBO_UI_VERB ("WorkOnline", (BonoboUIVerbFn) command_work_online),

	BONOBO_UI_VERB_END
};

static BonoboUIVerb new_verbs [] = {
	BONOBO_UI_VERB ("OpenNewWindow", (BonoboUIVerbFn) command_open_new_window),

	BONOBO_UI_VERB_END
};

static BonoboUIVerb actions_verbs[] = {
	BONOBO_UI_VERB ("SendReceive", (BonoboUIVerbFn) command_send_receive),
	BONOBO_UI_VERB ("ForgetPasswords", command_forget_passwords),

	BONOBO_UI_VERB_END
};

static BonoboUIVerb tools_verbs[] = {
	BONOBO_UI_VERB ("Settings", (BonoboUIVerbFn) command_settings),
	BONOBO_UI_VERB ("PilotSettings", (BonoboUIVerbFn) command_pilot_settings),

	BONOBO_UI_VERB_END
};

static BonoboUIVerb help_verbs [] = {
	BONOBO_UI_VERB ("HelpFAQ", (BonoboUIVerbFn) command_help_faq),
	BONOBO_UI_VERB ("QuickReference", (BonoboUIVerbFn) command_quick_reference),
	BONOBO_UI_VERB ("HelpSubmitBug", (BonoboUIVerbFn) command_submit_bug),
	BONOBO_UI_VERB ("HelpAbout", (BonoboUIVerbFn) command_about_box),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/commands/SendReceive", "stock_mail-send-receive", 16),
	E_PIXMAP ("/Toolbar/SendReceive", "stock_mail-send-receive", 24),
	E_PIXMAP ("/menu/File/FileImporter", "stock_mail-import", 16),
	E_PIXMAP ("/menu/File/ToggleOffline", "stock_disconnect", 16),
	E_PIXMAP ("/menu/Tools/Settings", "gnome-settings", 16),

	E_PIXMAP_END
};

static EPixmap offline_pixmaps [] = {
	E_PIXMAP ("/menu/File/ToggleOffline", "stock_disconnect", 16),
	E_PIXMAP_END
};

static EPixmap online_pixmaps [] = {
	E_PIXMAP ("/menu/File/ToggleOffline", "stock_connect", 16),
	E_PIXMAP_END
};


/* The Work Online / Work Offline menu item.  */

static void
update_offline_menu_item (EShellWindow *shell_window,
			  EShellLineStatus line_status)
{
	BonoboUIComponent *ui_component;

	ui_component = e_shell_window_peek_bonobo_ui_component (shell_window);

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
			      EShellWindow *shell_window)
{
	update_offline_menu_item (shell_window, new_status);
}


/* Public API.  */

void
e_shell_window_commands_setup (EShellWindow *shell_window)
{
	BonoboUIComponent *uic;
	EShell *shell;

	g_return_if_fail (shell_window != NULL);
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	uic = e_shell_window_peek_bonobo_ui_component (shell_window);
	shell = e_shell_window_peek_shell (shell_window);

	bonobo_ui_component_add_verb_list_with_data (uic, file_verbs, shell_window);
	bonobo_ui_component_add_verb_list_with_data (uic, new_verbs, shell_window);
	bonobo_ui_component_add_verb_list_with_data (uic, actions_verbs, shell_window);
	bonobo_ui_component_add_verb_list_with_data (uic, tools_verbs, shell_window);
	bonobo_ui_component_add_verb_list_with_data (uic, help_verbs, shell_window);

	e_pixmaps_update (uic, pixmaps);

	/* Set up the work online / work offline menu item.  */
	g_signal_connect_object (shell, "line_status_changed",
				 G_CALLBACK (shell_line_status_changed_cb), shell_window, 0);
	update_offline_menu_item (shell_window, e_shell_get_line_status (shell));
}
