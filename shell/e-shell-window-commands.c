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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include <libgnome/gnome-exec.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-url.h>

#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <bonobo/bonobo-ui-component.h>

#include <libedataserverui/e-passwords.h>

#include <gconf/gconf-client.h>

#include "e-util/e-icon-factory.h"
#include "e-util/e-dialog-utils.h"
#include "e-util/e-error.h"
#include "e-util/e-print.h"
#include "e-util/e-util-private.h"

#include "e-shell-window-commands.h"
#include "e-shell-window.h"
#include "evolution-shell-component-utils.h"

#include "e-shell-importer.h"

#define EVOLUTION_COPYRIGHT \
	"Copyright \xC2\xA9 1999 - 2007 Novell, Inc. and Others"

#define EVOLUTION_WEBSITE \
	"http://www.gnome.org/projects/evolution/"

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
command_page_setup (BonoboUIComponent *uih,
		    EShellWindow *window,
		    const char *path)
{
	e_print_run_page_setup_dialog (GTK_WINDOW (window));
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
	gchar *command_line;
	GError *error = NULL;

        command_line = "bug-buddy --sm-disable --package=Evolution";

	g_debug ("Spawning: %s", command_line);

	if (!g_spawn_command_line_async (command_line, &error)) {
		if (error->code == G_SPAWN_ERROR_NOENT)
			e_notice (NULL, GTK_MESSAGE_ERROR,
				_("Bug buddy is not installed."));
		else
			e_notice (NULL, GTK_MESSAGE_ERROR,
				_("Bug buddy could not be run."));
		g_error_free (error);
	}
}

/* must be in utf8, the weird breaking of escaped strings
   is so the hex escape strings dont swallow too many chars

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   So that means, 8 bit characters, use \xXX hex encoding ONLY
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  No all environments are utf8 and not all editors can handle it.
*/
static const char *authors[] = {
	"Aaron Weber",
	"Abel Cheung",
	"Abhishek Parwal",
	"Adam Weinberger",
	"Adi Attar",
	"Ahmad Riza H Nst",
	"Aishwarya K",
	"Akagic Amila",
	"Akira Tagoh",
	"Alastair McKinstry",
	"Alastair Tse",
	"Alejandro Andres",
	"Alessandro Decina",
	"Alex Graveley",
	"Alex Jiang",
	"Alexander Shopov",
	"Alfred Peng",
	"Ali Abdin",
	"Ali Akcaagac",
	"Almer S. Tigelaar",
	"Amish",
	"Anand V M",
	"Anders Carlsson",
	"Andre Klapper",
	"Andrea Campi",
	"Andreas Hyden",
	"Andreas J. Guelzow",
	"Andreas K\xC3\xB6hler",
	"Andrew Ruthven",
	"Andrew T. Veliath",
	"Andrew Wu",
	"Ankit Patel",
	"Anna Marie Dirks",
	"Antonio Xu",
	"Arafat Medini",
	"Arangel Angov",
	"Archit Baweja",
	"Ariel Rios",
	"Arik Devens",
	"Arturo Espinosa Aldama",
	"Arulanandan P",
	"Arunprakash",
	"Arvind Sundararajan",
	"Bastien Nocera",
	"Behnam Esfahbod",
	"Ben Gamari",
	"Benjamin Berg",
	"Benjamin Kahn",
	"Benoît Dejean",
	"Bernard Leach",
	"Bertrand Guiheneuf",
	"Bharath Acharya",
	"Bill Zhu",
	"Bj\xC3\xB6rn Torkelsson",
	"Bob Doan",
	"Boby Wang",
	"Bolian Yin",
	"Brian Mury",
	"Bruce Tao",
	"Calvin Liu",
	"Cantona Su",
	"Carl Sun",
	"Carlos Garcia Campos",
	"Carlos Garnacho Parro",
	"Carlos Perell\xC3\xB3" " Mar\xC3\xAD" "n",
	"Carsten Guenther",
	"Carsten Schaar",
	"Changwoo Ryu",
	"Chao-Hsiung Liao",
	"Charles Zhang",
	"Chema Celorio",
	"Chenthill Palanisamy",
	"Chpe",
	"Chris Halls",
	"Chris Heath",
	"Chris Phelps",
	"Chris Toshok",
	"Christian Hammond",
	"Christian Kellner",
	"Christian Kreibich",
	"Christian Neumair",
	"Christophe Fergeau",
	"Christophe Merlet",
	"Christopher Blizzard",
	"Christopher J. Lahey",
	"Christopher R. Gabriel",
	"Claude Paroz",
	"Claudio Saavedra",
	"Clifford R. Conover",
	"Cody Russell",
	"Craig Small",
	"Dafydd Harries",
	"Damian Ivereigh",
	"Damon Chaplin",
	"Dan Berger",
	"Dan Winship",
	"Daniel Gryniewicz",
	"Daniel Nylander",
	"Daniel Veillard",
	"Daniel Yacob",
	"Danilo \xC5\xA0" "egan",
	"Darin Adler",
	"Dave Camp",
	"Dave Fallon",
	"Dave West",
	"David Farning",
	"David Malcolm",
	"David Moore",
	"David Mosberger",
	"David Richards",
	"David Trowbridge",
	"David Woodhouse",
	"Devashish Sharma",
	"Diego Escalante Urrelo",
	"Diego Gonzalez",
	"Diego Sevilla Ruiz",
	"Dietmar Maurer",
	"Dinesh Layek",
	"Dmitry Mastrukov",
	"Duarte Loreto",
	"Duncan Mak",
	"Ebby Wiselyn",
	"Ed Catmur",
	"Edd Dumbill",
	"Edgar Luna Díaz",
	"Edward Rudd",
	"Elijah Newren",
	"Elizabeth Greene",
	"Elliot Lee",
	"Elliot Turner",
	"Eneko Lacunza",
	"Enver Altin",
	"Erdal Ronahi",
	"Erdi Gergo",
	"Eric Busboom",
	"Eric Zhao",
	"Eskil Heyn Olsen",
	"Ettore Perazzoli",
	"Evan Yan",
	"Fatih Demir",
	"Fazlu & Hannah",
	"Federico Mena Quintero",
	"Fernando Herrera",
	"Francisco Javier F. Serrador",
	"Frank Arnold",
	"Frank Belew",
	"Frederic Crozat",
	"Frederic Peters",
	"Funda Wang",
	"Gareth Owen",
	"Gary Coady",
	"Gary Ekker",
	"Gavin Scott",
	"Gediminas Paulauskas",
	"George Lebl",
	"Gerardo Marin",
	"Gil Osher",
	"Gilbert Fang",
	"Gilles Dartiguelongue",
	"Grahame Bowland",
	"Greg Hudson",
	"Gregory Leblanc",
	"Gregory McLean",
	"Grzegorz Goawski",
	"Gustavo Gir\xC3\x8E" "ldez",
	"Gustavo Maciel Dias Vieira",
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
	"Hiroyuki Ikezoe",
	"Iain Buchanan",
	"Iain Holmes",
	"Ian Campbell",
	"Ilkka Tuohela",
	"Irene Huang",
	"Ismael Olea",
	"Israel Escalante",
	"Iv\xC3\xA1" "n Frade",
	"J.H.M. Dassen (Ray)",
	"JP Rosevear",
	"J\xC3\xBC" "rg Billeter",
	"Jack Jia",
	"Jacob Berkman",
	"Jaka Mocnik",
	"Jakub Steiner",
	"James Bowes",
	"James Henstridge",
	"James Willcox",
	"Jan Arne Petersen",
	"Jan Van Buggenhout",
	"Jarkko Ranta",
	"Jason Leach",
	"Jason Tackaberry",
	"Jayaradha",
	"Jean-Noel Guiheneuf",
	"Jedy Wang",
	"Jeff Cai",
	"Jeff Garzik",
	"Jeffrey Stedfast",
	"Jens Seidel",
	"Jeremy Katz",
	"Jeremy Wise",
	"Jerome Lacoste",
	"Jerry Yu",
	"Jes\xC3\xBA" "s Bravo \xC3\x81" "lvarez",
	"Jesse Pavel",
	"Ji Lee",
	"Joan Sanfeliu",
	"Jody Goldberg",
	"Joe Marcus Clarke",
	"Joe Shaw",
	"John Gotts",
	"Johnny Jacob",
	"Jon Ander Hernandez",
	"Jon K Hellan",
	"Jon Oberheide",
	"Jon Trowbridge",
	"Jonas Borgstr",
	"Jonathan Blandford",
	"Jonathan Dieter",
	"Jos Dehaes",
	"Jukka Zitting",
	"Jules Colding",
	"Julian Missig",
	"Julio M. Merino Vidal",
	"Karl Eichwalder",
	"Karl Relton",
	"Karsten Br\xC3\xA4" "ckelmann",
	"Kaushal Kumar",
	"Kenneth Christiansen",
	"Kenny Graunke",
	"Keshav Upadhyaya",
	"Kevin Breit",
	"Kevin Vandersloot",
	"Khasim Shaheed",
	"Kidd Wang",
	"Kjartan Maraas",
	"Krishnan R",
	"Krisztian Pifko",
	"Kyle Ambroff",
	"Larry Ewing",
	"Laszlo (Laca) Peter",
	"Laurent Dhima",
	"Lauris Kaplinski",
	"Leon Zhang",
	"Li Yuan",
	"Lorenzo Gil Sanchez",
	"Luca Ferretti",
	"Lucky Wankhede",
	"Luis Villa",
	"Lutz M",
	"M Victor Aloysius J",
	"Maciej Stachowiak",
	"Makuchaku",
	"Malcolm Tredinnick",
	"Marco Pesenti Gritti",
	"Marius Andreiana",
	"Marius Vollmer",
	"Mark Crichton",
	"Mark G. Adams",
	"Mark Gordon",
	"Mark McLoughlin",
	"Mark Moulder",
	"Martha Burke",
	"Martin Baulig",
	"Martin Hicks",
	"Martin Norb\xC3\xA4" "ck",
	"Martyn Russell",
	"Masahiro Sakai",
	"Mathieu Lacage",
	"Matias Mutchinick",
	"Matt Bissiri",
	"Matt Brown",
	"Matt Martin",
	"Matt Wilson",
	"Matthew Barnes",
	"Matthew Hall",
	"Matthew Loper",
	"Matthew Wilson",
	"Max Horn",
	"Maxx Cao",
	"Meilof Veeningen",
	"Mengjie Yu",
	"Michael M. Morrison",
	"Michael MacDonald",
	"Michael Meeks",
	"Michael Terry",
	"Michael Zucchi",
	"Michel Daenzer",
	"Miguel Angel Lopez Hernandez",
	"Miguel de Icaza",
	"Mikael Hallendal",
	"Mike Castle",
	"Mike Kestner",
	"Mike McEwan",
	"Mikhail Zabaluev",
	"Milan Crha",
	"Miles Lane",
	"Mohammad Damt",
	"Morten Welinder",
	"Mubeen Jukaku",
	"Murray Cumming",
	"Naba Kumar",
	"Nagappan Alagappan",
	"Nancy Cai",
	"Nat Friedman",
	"Nathan Owens",
	"Nicel KM",
	"Nicholas J Kreucher",
	"Nick Sukharev",
	"Nickolay V. Shmyrev",
	"Nike Gerdts",
	"Nuno Ferreira",
	"Oswald Rodrigues",
	"Owen Taylor",
	"P S Chakravarthi",
	"Pablo Gonzalo del Campo",
	"Pablo Saratxaga",
	"Pamplona Hackers",
	"Paolo Molaro",
	"Parag Goel",
	"Parthasarathi Susarla",
	"Patrick Ohly",
	"Pavel Cisler",
	"Pavel Roskin",
	"Pavithran",
	"Pawan Chitrakar",
	"Pedro Villavicencio",
	"Peter Pouliot",
	"Peter Teichman",
	"Peter Williams",
	"Peteris Krisjanis",
	"Petta Pietikainen",
	"Phil Goembel",
	"Philip Van Hoof",
	"Philip Zhao",
	"Poornima Nayak",
	"Pratik V. Parikh",
	"Praveen Kumar",
	"Priit Laes",
	"Priyanshu Raj",
	"Radek Doul\xC3\xADk",
	"Raghavendran R",
	"Raja R Harinath",
	"Rajeev Ramanathan",
	"Rajesh Ranjan",
	"Rakesh k.g",
	"Ramiro Estrugo",
	"Ranjan Somani",
	"Ray Strode",
	"Rhys Jones",
	"Richard Boulton",
	"Richard Hult",
	"Richard Li",
	"Rob Bradford",
	"Robert Brady",
	"Robert Sedak",
	"Robin Slomkowski",
	"Rodney Dawes",
	"Rodrigo Moya",
	"Rohini S",
	"Ronald Kuetemeier",
	"Roozbeh Pournader",
	"Ross Burton",
	"Rouslan Solomakhin",
	"Runa Bhattacharjee",
	"Russell Steinthal",
	"Rusty Conover",
	"Ryan P. Skadberg",
	"S Antony Vincent Pandian",
	"S N Tejasvi",
	"S. \xC3\x87" "a\xC4\x9F" "lar Onur",
	"S.Antony Vincent Pandian",
	"Sam Creasey",
	"Sam Yang",
	"Sam\xC3\xBA" "el J\xC3\xB3" "n Gunnarsson",
	"Sankar P",
	"Sanlig Badral",
	"Sanshao Jiang",
	"Sarfraaz Ahmed",
	"Sayamindu Dasgupta",
	"Sean Atkinson",
	"Sean Gao",
	"Sebastian Rittau",
	"Sebastian Wilhelmi",
	"Sebastien Bacher",
	"Sergey Panov",
	"Seth Alves",
	"Seth Nickell",
	"Shakti Sen",
	"Shi Pu",
	"Shilpa C",
	"Shree Krishnan",
	"Shreyas Srinivasan",
	"Simon Zheng",
	"Simos Xenitellis",
	"Sivaiah Nallagatla",
	"Srinivasa Ragavan",
	"Stanislav Brabec",
	"Stanislav Visnovsky",
	"Stephen Cook",
	"Steve Murphy",
	"Steven Zhang",
	"Stuart Parmenter",
	"Subodh Soni",
	"Suman Manjunath",
	"Sunil Mohan Adapa",
	"Suresh Chandrasekharan",
	"Sushma Rai",
	"Sven Herzberg",
	"Szabolcs Ban",
	"T\xC3\xB5" "ivo Leedj\xC3\xA4" "rv",
	"Takayuki Kusano",
	"Takeshi Aihana",
	"Tambet Ingo",
	"Taylor Hayward",
	"Theppitak Karoonboonyanan",
	"Thomas Cataldo",
	"Thomas Mirlacher",
	"Thouis R. Jones",
	"Tim Wo",
	"Tim Yamin",
	"Timo Hoenig",
	"Timo Sirainen",
	"Timothy Lee",
	"Timur Bakeyev",
	"Tino Meinen",
	"Tobias Mueller",
	"Tom Tromey",
	"Tomas Ogren",
	"Tomasz K\xC5\x82" "oczko",
	"Tomislav Vujec",
	"Tommi Komulainen",
	"Tommi Vainikainen",
	"Tony Tsui",
	"Tor Lillqvist",
	"Trent Lloyd",
	"Tuomas J. Lukka",
	"Tuomas Kuosmanen",
	"Ulrich Neumann",
	"Umesh Tiwari",
	"Umeshtej",
	"Ushveen Kaur",
	"V Ravi Kumar Raju",
	"Vadim Strizhevsky",
	"Valek Filippov",
	"Vandana Shenoy .B",
	"Vardhman Jain",
	"Veerapuram Varadhan",
	"Vincent Noel",
	"Vincent van Adrighem",
	"Viren",
	"Vivek Jain",
	"Vladimer Sichinava",
	"Vladimir Vukicevic",
	"Wadim Dziedzic",
	"Wang Jian",
	"Wang Xin",
	"Wayne Davis",
	"William Jon McCann",
	"Wouter Bolsterlee",
	"Xan Lopez",
	"Yanko Kaneti",
	"Yong Sun",
	"Yuedong Du",
	"Yukihiro Nakai",
	"Yuri Pankov",
	"Yuri Syrota",
	"Zach Frey",
	"Zan Lynx",
	"Zbigniew Chyla",
	"\xC3\x98ystein Gisn\xC3\xA5s",
	"\xC5\xBDygimantas Beru\xC4\x8Dka",
	NULL
};

static const char *documentors[] = {
	"Aaron Weber",
	"Binika Preet",
	"Dan Winship",
	"David Trowbridge",
	"Jessica Prabhakar",
	"JP Rosevear",
	"Radhika Nair",
	NULL
};

static void
command_about (BonoboUIComponent *uih,
               EShellWindow *window,
               const char *path)
{
	gchar *translator_credits;

	/* The translator-credits string is for translators to list
	 * per-language credits for translation, displayed in the
	 * about dialog. */
	translator_credits = _("translator-credits");
	if (strcmp (translator_credits, "translator-credits") == 0)
		translator_credits = NULL;

	gtk_show_about_dialog (
		GTK_WINDOW (window),
		"program-name", "Evolution",
		"version", VERSION,
		"copyright", EVOLUTION_COPYRIGHT,
		"comments", _("Groupware Suite"),
		"website", EVOLUTION_WEBSITE,
		"website-label", _("Evolution Website"),
		"authors", authors,
		"documenters", documentors,
		"translator-credits", translator_credits,
		"logo-icon-name", "evolution",
		NULL);
}

#if 0
/* Unused */
static void
command_help_faq (BonoboUIComponent *uih,
		  EShellWindow *window,
		  const char *path)
{
	/* FIXME Show when we have a faq */
	/* FIXME use the error */
	gnome_url_show ("http://gnome.org/projects/evolution/faq.shtml", NULL);
}
#endif

static void
command_quick_reference (BonoboUIComponent *uih,
			 EShellWindow *window,
			 const char *path)
{
	char *quickref;
	GnomeVFSMimeApplication *app;
	const gchar * const *language_names;

	language_names = g_get_language_names ();
	while (*language_names != NULL) {
		const gchar *lang = *language_names++;

		/* This has to be a valid language AND a language with
		 * no encoding postfix.  The language will come up without
		 * encoding next */
		if (lang == NULL || strchr (lang, '.') != NULL)
			continue;

		quickref = g_build_filename (EVOLUTION_HELPDIR, "quickref", lang, "quickref.pdf", NULL);
		if (g_file_test (quickref, G_FILE_TEST_EXISTS)) {
			app = gnome_vfs_mime_get_default_application ("application/pdf");

			if (app) {
				GList *uris = NULL;
				char *uri;

				uri = gnome_vfs_get_uri_from_local_path (quickref);
				uris = g_list_append (uris, uri);

				gnome_vfs_mime_application_launch (app, uris);

				g_free (uri);
				g_list_free (uris);
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
	e_shell_go_offline (e_shell_window_peek_shell (window), window, GNOME_Evolution_USER_OFFLINE);
}

static void
command_work_online (BonoboUIComponent *uih,
		     EShellWindow *window,
		     const char *path)
{
	e_shell_go_online (e_shell_window_peek_shell (window), window, GNOME_Evolution_USER_ONLINE);
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
	if (e_error_run (NULL, "shell:forget-passwords", NULL) == GTK_RESPONSE_OK)
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
	BONOBO_UI_VERB ("FilePageSetup", (BonoboUIVerbFn) command_page_setup),
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
	BONOBO_UI_VERB ("HelpAbout", (BonoboUIVerbFn) command_about),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/Toolbar/SendReceive", "mail-send-receive", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/menu/File/OpenNewWindow", "window-new", E_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/SendReceive", "mail-send-receive", E_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/FileImporter", "stock_mail-import", E_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/Print/FilePageSetup", "stock_print-setup", E_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/ToggleOffline", "stock_disconnect", E_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/FileClose", "window-close", E_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/FileExit", "application-exit", E_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/Edit/Settings", "preferences-desktop", E_ICON_SIZE_MENU),

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
		g_return_if_reached();
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
view_buttons_icontext_item_toggled_handler (BonoboUIComponent           *ui_component,
					    const char                  *path,
					    Bonobo_UIComponent_EventType type,
					    const char                  *state,
					    EShellWindow                *shell_window)
{
	ESidebar *sidebar;

	sidebar = e_shell_window_peek_sidebar (shell_window);
	e_sidebar_set_mode (sidebar, E_SIDEBAR_MODE_BOTH);
}

static void
view_buttons_icon_item_toggled_handler (BonoboUIComponent           *ui_component,
					const char                  *path,
					Bonobo_UIComponent_EventType type,
					const char                  *state,
					EShellWindow                *shell_window)
{
	ESidebar *sidebar;

	sidebar = e_shell_window_peek_sidebar (shell_window);
	e_sidebar_set_mode (sidebar, E_SIDEBAR_MODE_ICON);
}

static void
view_buttons_text_item_toggled_handler (BonoboUIComponent           *ui_component,
					const char                  *path,
					Bonobo_UIComponent_EventType type,
					const char                  *state,
					EShellWindow                *shell_window)
{
	ESidebar *sidebar;

	sidebar = e_shell_window_peek_sidebar (shell_window);
	e_sidebar_set_mode (sidebar, E_SIDEBAR_MODE_TEXT);
}

static void
view_buttons_toolbar_item_toggled_handler (BonoboUIComponent           *ui_component,
					   const char                  *path,
					   Bonobo_UIComponent_EventType type,
					   const char                  *state,
					   EShellWindow                *shell_window)
{
	ESidebar *sidebar;

	sidebar = e_shell_window_peek_sidebar (shell_window);
	e_sidebar_set_mode (sidebar, E_SIDEBAR_MODE_TOOLBAR);
}

static void
view_buttons_hide_item_toggled_handler (BonoboUIComponent           *ui_component,
					const char                  *path,
					Bonobo_UIComponent_EventType type,
					const char                  *state,
					EShellWindow                *shell_window)
{
	ESidebar *sidebar;
	gboolean is_visible;

	sidebar = e_shell_window_peek_sidebar (shell_window);

	is_visible = state[0] == '0';

	e_sidebar_set_show_buttons (sidebar, is_visible);
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

static void
view_statusbar_item_toggled_handler (BonoboUIComponent           *ui_component,
				     const char                  *path,
				     Bonobo_UIComponent_EventType type,
				     const char                  *state,
				     EShellWindow                *shell_window)
{
	GtkWidget *status_bar = e_shell_window_peek_statusbar (shell_window);
	gboolean is_visible;
	GConfClient *gconf_client;

	is_visible = state[0] == '1';
	if(is_visible)
		gtk_widget_show (status_bar);
	else
		gtk_widget_hide (status_bar);
	gconf_client = gconf_client_get_default ();
	gconf_client_set_bool (gconf_client,"/apps/evolution/shell/view_defaults/statusbar_visible", is_visible, NULL);
	g_object_unref (gconf_client);
}

static void
view_sidebar_item_toggled_handler (BonoboUIComponent           *ui_component,
				     const char                  *path,
				     Bonobo_UIComponent_EventType type,
				     const char                  *state,
				     EShellWindow                *shell_window)
{
	GtkWidget *side_bar = GTK_WIDGET(e_shell_window_peek_sidebar (shell_window));
	gboolean is_visible;
	GConfClient *gconf_client;

	is_visible = state[0] == '1';
	if(is_visible)
		gtk_widget_show (side_bar);
	else
		gtk_widget_hide (side_bar);
	gconf_client = gconf_client_get_default ();
	gconf_client_set_bool (gconf_client_get_default (),"/apps/evolution/shell/view_defaults/sidebar_visible", is_visible, NULL);
	g_object_unref (gconf_client);
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
	bonobo_ui_component_add_listener (uic, "ViewButtonsIconText",
					  (BonoboUIListenerFn)view_buttons_icontext_item_toggled_handler,
					  (gpointer)shell_window);
	bonobo_ui_component_add_listener (uic, "ViewButtonsIcon",
					  (BonoboUIListenerFn)view_buttons_icon_item_toggled_handler,
					  (gpointer)shell_window);
	bonobo_ui_component_add_listener (uic, "ViewButtonsText",
					  (BonoboUIListenerFn)view_buttons_text_item_toggled_handler,
					  (gpointer)shell_window);
	bonobo_ui_component_add_listener (uic, "ViewButtonsToolbar",
					  (BonoboUIListenerFn)view_buttons_toolbar_item_toggled_handler,
					  (gpointer)shell_window);
	bonobo_ui_component_add_listener (uic, "ViewButtonsHide",
					  (BonoboUIListenerFn)view_buttons_hide_item_toggled_handler,
					  (gpointer)shell_window);
	bonobo_ui_component_add_listener (uic, "ViewToolbar",
					  (BonoboUIListenerFn)view_toolbar_item_toggled_handler,
					  (gpointer)shell_window);
	bonobo_ui_component_add_listener (uic, "ViewStatusBar",
					  (BonoboUIListenerFn)view_statusbar_item_toggled_handler,
					  (gpointer)shell_window);
	bonobo_ui_component_add_listener (uic, "ViewSideBar",
					  (BonoboUIListenerFn)view_sidebar_item_toggled_handler,
					  (gpointer)shell_window);

	e_pixmaps_update (uic, pixmaps);

	/* Set up the work online / work offline menu item.  */
	g_signal_connect_object (shell, "line_status_changed",
				 G_CALLBACK (shell_line_status_changed_cb), shell_window, 0);
	update_offline_menu_item (shell_window, e_shell_get_line_status (shell));
}
