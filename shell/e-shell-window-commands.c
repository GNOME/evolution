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

#include "e-shell-importer.h"
#include "e-shell-window.h"

#include "evolution-shell-component-utils.h"

#include "e-util/e-icon-factory.h"
#include "e-util/e-dialog-utils.h"
#include "e-util/e-passwords.h"

#include <glib/gprintf.h>

#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-url.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-about.h>

#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>

#include <bonobo/bonobo-ui-component.h>

#include <string.h>

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

/* must be in utf8, the weird breaking of escaped strings
   is so the hex escape strings dont swallow too many chars */
static const char *authors[] = {
	"Aaron Weber",
	"Abel Cheung",
	"Adam Weinberger",
	"Akira TAGOH",
	"Alastair McKinstry",
	"Alex Graveley",
	"Alex Jiang",
	"Alfred Peng",
	"Almer S. Tigelaar",
	"Anders Carlsson",
	"Andre Klapper",
	"Andreas Hyden",
	"Andrew T. Veliath",
	"Andrew Wu",
	"Ankit Patel",
	"Anna Marie Dirks",
	"Antonio Xu",
	"Arafat Medini",
	"Ariel Rios",
	"Arik Devens",
	"Arturo Espinosa Aldama",
	"Bastien Nocera",
	"Benjamin Kahn",
	"Bertrand Guiheneuf",
	"Bill Zhu",
	"Bjorn Torkelsson"
	"Bob Doan",
	"Bolian Yin",
	"Bruce Tao",
	"Calvin Liu",
	"Cantona Su",
	"Carl Sun",
	"Carlos Garnacho Parro",
	"Carlos Perell\xC3\xB3" " Mar\xC3\xAD" "n",
	"Carsten Schaar",
	"Changwoo Ryu",
	"Charles Zhang",
	"Chema Celorio",
	"Chenthill Palanisamy",
	"Chris Lahey",
	"Chris Toshok",
	"Christian Hammond",
	"Christian Kellner",
	"Christian Kreibich",
	"Christian Neumair",
	"Christophe Fergeau",
	"Christophe Merlet",
	"Christopher Blizzard",
	"Christopher J. Lahey",
	"Clifford R. Conover",
	"Cody Russell",
	"Craig Small",
	"Damon Chaplin",
	"Dan Berger",
	"Dan Winship",
	"Danilo \xC5\xA0" "egan",
	"Darin Adler",
	"Dave Camp",
	"Dave Fallon",
	"Dave West",
	"David Malcolm",
	"David Moore",
	"David Trowbridge",
	"David Woodhouse",
	"Dietmar Maurer",
	"Duarte Loreto",
	"Duncan Mak",
	"ERDI Gergo",
	"Ed Catmur",
	"Edd Dumbill",
	"Edgar Luna Díaz",
	"Elliot Lee",
	"Elliot Turner",
	"Eneko Lacunza",
	"Enver ALTIN",
	"Eric Zhao",
	"Eskil Heyn Olsen",
	"Ettore Perazzoli",
	"Fatih Demir",
	"Federico Mena Quintero",
	"Fernando Herrera",
	"Francisco Javier F. Serrador",
	"Frank Belew",
	"Frederic Crozat",
	"Gary Ekker",
	"Gediminas Paulauskas",
	"Gerardo Marin",
	"Gil Osher",
	"Gilbert Fang",
	"Grahame Bowland",
	"Greg Hudson",
	"Gregory McLean",
	"Grzegorz Goawski",
	"Gustavo Maciel Dias Vieira",
	"H P Nadig",
	"H\xC3\xA9" "ctor Garc\xC3\xAD" "a \xC3\x81" "lvarez",
	"Hans Petter Jansson",
	"Hao Sheng",
	"Hari Prasad Nadig",
	"Harish Krishnaswamy",
	"Harry Lu",
	"Hasbullah Bin Pit",
	"Havoc Pennington",
	"Heath Harrelson",
	"Herbert V. Riedel",
	"Iain Holmes",
	"Ian Campbell",
	"Ismael Olea",
	"Israel Escalante",
	"J.H.M. Dassen (Ray)",
	"JP Rosevear",
	"Jack Jia",
	"Jacob Berkman",
	"Jaka Mocnik",
	"Jakub Steiner",
	"James Henstridge",
	"James Willcox",
	"Jan Arne Petersen",
	"Jason Leach",
	"Jason Tackaberry",
	"Jean-Noel Guiheneuf",
	"Jeff Garzik",
	"Jeffrey Stedfast",
	"Jeremy Katz",
	"Jeremy Wise",
	"Jerome Lacoste",
	"Jes\xC3\xBA" "s Bravo \xC3\x81" "lvarez",
	"Jesse Pavel",
	"Ji Lee",
	"Jody Goldberg",
	"Joe Shaw",
	"Jon K Hellan",
	"Jon Oberheide",
	"Jon Trowbridge",
	"Jonas Borgstr",
	"Jonathan Blandford",
	"Jos Dehaes",
	"Jukka Zitting",
	"J\xC3\xBC" "rg Billeter",
	"Karl Eichwalder",
	"Karsten Br\xC3\xA4" "ckelmann",
	"Kenneth Christiansen",
	"Kenny Graunke",
	"Kevin Breit",
	"Kidd Wang",
	"Kjartan Maraas",
	"Larry Ewing",
	"Laurent Dhima",
	"Lauris Kaplinski",
	"Leon Zhang",
	"Lorenzo Gil Sanchez",
	"Luis Villa",
	"Maciej Stachowiak",
	"Malcolm Tredinnick",
	"Marius Andreiana",
	"Marius Vollmer",
	"Mark Crichton",
	"Mark Gordon",
	"Martha Burke",
	"Martin Baulig",
	"Martin Hicks",
	"Martin Norb\xC3\xA4" "ck",
	"Martyn Russell",
	"Mathieu Lacage",
	"Matt Bissiri",
	"Matt Martin",
	"Matt Wilson",
	"Matthew Loper",
	"Matthew Wilson",
	"Max Horn",
	"Maxx Cao",
	"Meilof Veeningen",
	"Michael M. Morrison",
	"Michael MacDonald",
	"Michael Meeks",
	"Michael Terry",
	"Michael Zucchi",
	"Michel Daenzer",
	"Miguel de Icaza",
	"Mikael Hallendal",
	"Mike Castle",
	"Mike Kestner",
	"Mike McEwan",
	"Miles Lane",
	"Nat Friedman",
	"Nicel KM",
	"Nicholas J Kreucher",
	"Nike Gerdts",
	"Nuno Ferreira",
	"P Chenthill",
	"Pablo Gonzalo del Campo",
	"Pablo Saratxaga",
	"Paolo Molaro",
	"Parthasarathi S A",
	"Pavel Cisler",
	"Pavel Roskin",
	"Peter Pouliot",
	"Peter Teichman",
	"Peter Williams",
	"Petta Pietikainen",
	"Philip Zhao",
	"Pratik V. Parikh",
	"Priit Laes",
	"Priyanshu Raj",
	"Radek Doul\xC3\xADk",
	"Raja R Harinath",
	"Ray Strode",
	"Richard Boulton",
	"Richard Hult",
	"Richard Li",
	"Robert Brady",
	"Robert Sedak",
	"Rodney Dawes",
	"Rodrigo Moya",
	"Ronald Kuetemeier",
	"Roozbeh Pournader",
	"Ross Burton",
	"Russell Steinthal",
	"Ryan P. Skadberg",
	"S N Tejasvi",
	"Sam Creasey",
	"Sam\xC3\xBA" "el J\xC3\xB3" "n Gunnarsson",
	"Sanlig Badral",
	"Sanshao Jiang",
	"Sarfraaz Ahmed",
	"Sean Atkinson",
	"Sean Gao",
	"Sebastian Rittau",
	"Sebastian Wilhelmi",
	"Sergey Panov",
	"Seth Alves",
	"Sivaiah Nallagatla",
	"Stanislav Brabec",
	"Stanislav Visnovsky",
	"Steve Murphy",
	"Stuart Parmenter",
	"Suresh Chandrasekharan",
	"Sushma Rai",
	"Szabolcs BAN",
	"T\xC3\xB5" "ivo Leedj\xC3\xA4" "rv",
	"Taylor Hayward",
	"Tim Wo",
	"Timo Sirainen",
	"Timothy Lee",
	"Timur Bakeyev",
	"Tom Tromey",
	"Tomas Ogren",
	"Tomislav Vujec",
	"Trent Lloyd",
	"Tuomas J. Lukka",
	"Tuomas Kuosmanen",
	"Umesh Tiwari",
	"Umeshtej",
	"V Ravi Kumar Raju",
	"Vadim Strizhevsky",
	"Valek Filippov",
	"Vardhman Jain",
	"Vladimir Vukicevic",
	"Wang Jian",
	"Wayne Davis",
	"William Jon McCann",
	"Xan Lopez",
	"Yanko Kaneti",
	"Yong Sun",
	"Yuedong Du",
	"Yukihiro Nakai",
	"Yuri Syrota",
	"Zbigniew Chyla",
	NULL
};
static const char *documentors[] = { 
	"Aaron Weber",
	"David Trowbridge",
	NULL
};

static GtkWidget *
about_box_new (void)
{
	GtkWidget *about_box = NULL;
	GdkPixbuf *pixbuf = NULL;
	char copyright[1024];
	char *filename = NULL;

	/* The translator-credits string is for translators to list
	 * per language credits for translation, displayed in the
	 * about box*/
	char *translator_credits = _("translator-credits");
	
	g_sprintf (copyright, "Copyright \xC2\xA9 1999 - 2004 Novell, Inc. and Others");
                                                                                
	filename = g_build_filename (EVOLUTION_DATADIR, "pixmaps",
				     "evolution-1.5.png", NULL);
	if (filename != NULL) {
		pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
		g_free (filename);
	}
                                                                                
	about_box = gnome_about_new ("Evolution",
				     VERSION,
				     copyright,
				     _("Groupware Suite"),
				     authors, documentors,
				     strcmp (translator_credits, "translator-credits") ? translator_credits : NULL,
				     pixbuf);
	
        if (pixbuf != NULL)
                g_object_unref (pixbuf);

	return GTK_WIDGET (about_box);
}

static void
command_about_box (BonoboUIComponent *uih,
		   EShellWindow *window,
		   const char *path)
{
	static GtkWidget *about_box_window = NULL;

	if (about_box_window != NULL) {
		gdk_window_raise (about_box_window->window);
		return;
	}

	about_box_window = about_box_new ();
	
	g_signal_connect (G_OBJECT (about_box_window), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &about_box_window);

	gtk_window_set_transient_for (GTK_WINDOW (about_box_window), GTK_WINDOW (window));

	gtk_widget_show (about_box_window);
}

static void
command_help_faq (BonoboUIComponent *uih,
		  EShellWindow *window,
		  const char *path)
{
	/* FIXME Show when we have a faq */
	/* FIXME use the error */
	gnome_url_show ("http://gnome.org/projects/evolution/faq.shtml", NULL);	
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
	BONOBO_UI_VERB ("QuickReference", (BonoboUIVerbFn) command_quick_reference),
	BONOBO_UI_VERB ("HelpSubmitBug", (BonoboUIVerbFn) command_submit_bug),
	BONOBO_UI_VERB ("HelpAbout", (BonoboUIVerbFn) command_about_box),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/commands/SendReceive", "stock_mail-send-receive", E_ICON_SIZE_MENU),
	E_PIXMAP ("/Toolbar/SendReceive", "stock_mail-send-receive", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/menu/File/FileImporter", "stock_mail-import", E_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/ToggleOffline", "stock_disconnect", E_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/Tools/Settings", "gnome-settings", E_ICON_SIZE_MENU),
	
	E_PIXMAP_END
};

static EPixmap offline_pixmaps [] = {
	E_PIXMAP ("/menu/File/ToggleOffline", "stock_disconnect", E_ICON_SIZE_MENU),
	E_PIXMAP_END
};

static EPixmap online_pixmaps [] = {
	E_PIXMAP ("/menu/File/ToggleOffline", "stock_connect", E_ICON_SIZE_MENU),
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

static void
view_toolbar_item_toggled_handler (BonoboUIComponent           *ui_component,
				   const char                  *path,
				   Bonobo_UIComponent_EventType type,
				   const char                  *state,
				   EShellWindow                *shell_window)
{
	gboolean is_visible;

	is_visible = state[0] == '1';

	bonobo_ui_component_set_prop (ui_component, "/Toolbar",
				      "hidden", is_visible ? "0" : "1", NULL);
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
	bonobo_ui_component_add_listener (uic, "ViewToolbar",
					  (BonoboUIListenerFn)view_toolbar_item_toggled_handler,
					  (gpointer)shell_window);

	e_pixmaps_update (uic, pixmaps);

	/* Set up the work online / work offline menu item.  */
	g_signal_connect_object (shell, "line_status_changed",
				 G_CALLBACK (shell_line_status_changed_cb), shell_window, 0);
	update_offline_menu_item (shell_window, e_shell_get_line_status (shell));
}
