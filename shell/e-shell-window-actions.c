/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 * e-shell-window-actions.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 */

#include "e-shell-window-private.h"

#include "e-shell.h"
#include "e-shell-importer.h"

#include "e-util/e-dialog-utils.h"
#include "e-util/e-error.h"
#include "e-util/e-print.h"

#include <string.h>
#include <libedataserverui/e-passwords.h>

#define EVOLUTION_COPYRIGHT \
	"Copyright \xC2\xA9 1999 - 2008 Novell, Inc. and Others"

#define EVOLUTION_FAQ \
	"http://www.go-evolution.org/FAQ"

#define EVOLUTION_WEBSITE \
	"http://www.gnome.org/projects/evolution/"

/* Authors and Documenters
 *
 * The names below must be in UTF8.  The breaking of escaped strings
 * is so the hexadecimal sequences don't swallow too many characters.
 *
 * SO THAT MEANS, FOR 8-BIT CHARACTERS USE \xXX HEX ENCODING ONLY!
 *
 * Not all environments are UTF8 and not all editors can handle it.
 */
static const gchar *authors[] = {
	"Aaron Weber",
	"Abel Cheung",
	"Abhishek Parwal",
	"Adam Weinberger",
	"Adi Attar",
	"Ahmad Riza H Nst",
	"Aidan Delaney",
	"Aishwarya K",
	"Akagic Amila",
	"Akhil Laddha",
	"Akira Tagoh",
	"Alastair McKinstry",
	"Alastair Tse",
	"Alejandro Andres",
	"Alessandro Decina",
	"Alex Graveley",
	"Alex Jiang",
	"Alex Jones",
	"Alex Kloss",
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
	"Andreas Henriksson",
	"Andreas Hyden",
	"Andreas J. Guelzow",
	"Andreas K\xC3\xB6hler",
	"Andreas Köhler",
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
	"Armin Bauer",
	"Arturo Espinosa Aldama",
	"Arulanandan P",
	"Arun Prakash",
	"Arvind Sundararajan",
	"Arvind",
	"Ashish",
	"B S Srinidhi",
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
	"BjÃ¶rn Lindqvist",
	"Bob Doan",
	"Bob Mauchin",
	"Boby Wang",
	"Bolian Yin",
	"Brian Mury",
	"Brian Pepple",
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
	"Christian Kirbach",
	"Christian Krause",
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
	"Colin Leroy",
	"Craig Small",
	"Dafydd Harries",
	"Damian Ivereigh",
	"Damien Carbery",
	"Damon Chaplin",
	"Dan Berger",
	"Dan Damian",
	"Dan Nguyen",
	"Dan Winship",
	"Daniel Gryniewicz",
	"Daniel Nylander",
	"Daniel van Eeden",
	"Daniel Veillard",
	"Daniel Yacob",
	"Danilo \xC5\xA0" "egan",
	"Danilo Segan",
	"Darin Adler",
	"Dave Benson",
	"Dave Camp",
	"Dave Fallon",
	"Dave Malcolm",
	"Dave West",
	"David Farning",
	"David Kaelbling",
	"David Malcolm",
	"David Moore",
	"David Mosberger",
	"David Richards",
	"David Trowbridge",
	"David Turner",
	"David Woodhouse",
	"Denis Washington",
	"Devashish Sharma",
	"Diego Escalante Urrelo",
	"Diego Gonzalez",
	"Diego Sevilla Ruiz",
	"Dietmar Maurer",
	"Dinesh Layek",
	"Djihed Afifi",
	"Dmitry Mastrukov",
	"Dodji Seketeli",
	"Duarte Loreto",
	"Dulmandakh Sukhbaatar",
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
	"Gabor Kelemen",
	"Ganesh",
	"Gareth Owen",
	"Gary Coady",
	"Gary Ekker",
	"Gavin Scott",
	"Gediminas Paulauskas",
	"Gerg\xC5\x91 \xC3\x89rdi",
	"George Lebl",
	"Gerardo Marin",
	"Gert Kulyk",
	"Giancarlo Capella",
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
	"H P Nadig",
	"H\xC3\xA9" "ctor Garc\xC3\xAD" "a \xC3\x81" "lvarez",
	"Hans Petter Jansson",
	"Hao Sheng",
	"Hari Prasad Nadig",
	"Harish K",
	"Harish Krishnaswamy",
	"Harry Lu",
	"Hasbullah Bin Pit",
	"Havoc Pennington",
	"Heath Harrelson",
	"Hein-Pieter van Braam",
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
	"IvÃ¡n Frade",
	"J.H.M. Dassen (Ray)",
	"JP Rosevear",
	"J\xC3\xBC" "rg Billeter",
	"JÃÂ¼rg Billeter",
	"Jack Jia",
	"Jacob Ulysses Berkman",
	"Jacob Berkman",
	"Jaka Mocnik",
	"Jakub Steiner",
	"James Doc Livingston",
	"James Bowes",
	"James Henstridge",
	"James Willcox",
	"Jan Arne Petersen",
	"Jan Tichavsky",
	"Jan Van Buggenhout",
	"Jared Moore",
	"Jarkko Ranta",
	"Jason Leach",
	"Jason Tackaberry",
	"Jayaradha",
	"Jean-Noel Guiheneuf",
	"Jedy Wang",
	"Jeff Bailey",
	"Jeff Cai",
	"Jeff Garzik",
	"Jeffrey Stedfast",
	"Jens Granseuer",
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
	"Johnny",
	"Jon Ander Hernandez",
	"Jon K Hellan",
	"Jon Oberheide",
	"Jon Trowbridge",
	"Jonas Borgstr",
	"Jonathan Blandford",
	"Jonathan Dieter",
	"Jos Dehaes",
	"Josselin Mouette",
	"JP Rosvear",
	"Jukka Zitting",
	"Jules Colding",
	"Julian Missig",
	"Julio M. Merino Vidal",
	"Jürg Billeter",
	"Karl Eichwalder",
	"Karl Relton",
	"Karsten Br\xC3\xA4" "ckelmann",
	"Kaushal Kumar",
	"Kenneth Christiansen",
	"Kenny Graunke",
	"Keshav Upadhyaya",
	"Kevin Breit",
	"Kevin Piche",
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
	"LoÃ¯c Minier",
	"Loïc Minier",
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
	"Mark Tearle",
	"Martha Burke",
	"Martin Baulig",
	"Martin Hicks",
	"Martin Meyer",
	"Martin Norb\xC3\xA4" "ck",
	"Martyn Russell",
	"Masahiro Sakai",
	"Mathieu Lacage",
	"Matias Mutchinick",
	"Matt Bissiri",
	"Matt Brown",
	"Matt Loper",
	"Matt Martin",
	"Matt Wilson",
	"Matthew Barnes",
	"Matthew Daniel",
	"Matthew Hall",
	"Matthew Loper",
	"Matthew Wilson",
	"Matthias Clasen",
	"Max Horn",
	"Maxx Cao",
	"Mayank Jain",
	"Meilof Veeningen",
	"Mengjie Yu",
	"Michael Granger",
	"Michael M. Morrison",
	"Michael MacDonald",
	"Michael Meeks",
	"Michael Monreal",
	"Michael Terry",
	"Michael Zucchi",
	"Michel Daenzer",
	"Miguel Angel Lopez Hernandez",
	"Miguel de Icaza",
	"Mikael Hallendal",
	"Mikael Nilsson",
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
	"Nicholas Miell",
	"Nick Sukharev",
	"Nickolay V. Shmyrev",
	"Nike Gerdts",
	"Noel",
	"Nuno Ferreira",
	"Nyall Dawson",
	"Ondrej Jirman",
	"Oswald Rodrigues",
	"Owen Taylor",
	"Oystein Gisnas",
	"P Chenthill",
	"P S Chakravarthi",
	"Pablo Gonzalo del Campo",
	"Pablo Saratxaga",
	"Pamplona Hackers",
	"Paolo Molaro",
	"Parag Goel",
	"Parthasarathi Susarla",
	"Pascal Terjan",
	"Patrick Ohly",
	"Paul Bolle",
	"Paul Lindner",
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
	"Ricardo Markiewicz",
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
	"Rohini",
	"Roland Illig",
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
	"S. Caglar Onur",
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
	"Stéphane Raimbault",
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
	"Takao Fujiwara",
	"Takayuki Kusano",
	"Takeshi Aihana",
	"Tambet Ingo",
	"Taylor Hayward",
	"Ted Percival",
	"Theppitak Karoonboonyanan",
	"Thomas Cataldo",
	"Thomas Klausner",
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
	"Tõivo Leedjärv",
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
	"Xiurong Simon Zheng",
	"Yanko Kaneti",
	"Yi Jin",
	"Yong Sun",
	"Yu Mengjie",
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

static const gchar *documenters[] = {
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
action_about_cb (GtkAction *action,
                 EShellWindow *window)
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
		"documenters", documenters,
		"translator-credits", translator_credits,
		"logo-icon-name", "evolution",
		NULL);
}

static void
action_close_cb (GtkAction *action,
                 EShellWindow *window)
{
	GtkWidget *widget = GTK_WIDGET (window);
	GdkEvent *event;

	/* Synthesize a delete_event on this window. */
	event = gdk_event_new (GDK_DELETE);
	event->any.window = g_object_ref (widget->window);
	event->any.send_event = TRUE;
	gtk_main_do_event (event);
	gdk_event_free (event);
}

static void
action_contents_cb (GtkAction *action,
                    EShellWindow *window)
{
	/* FIXME  Unfinished. */
}

static void
action_faq_cb (GtkAction *action,
               EShellWindow *window)
{
	GError *error = NULL;

	gtk_show_uri (NULL, EVOLUTION_FAQ, GDK_CURRENT_TIME, &error);

	if (error != NULL) {
		/* FIXME Show an error dialog. */
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
action_forget_passwords_cb (GtkAction *action,
                            EShellWindow *window)
{
	gint response;

	response = e_error_run (
		GTK_WINDOW (window), "shell:forget-passwords", NULL);

	if (response == GTK_RESPONSE_OK)
		e_passwords_forget_passwords ();
}

static void
action_import_cb (GtkAction *action,
                  EShellWindow *window)
{
	e_shell_importer_start_import (window);
}

static void
action_new_window_cb (GtkAction *action,
                      EShellWindow *window)
{
	e_shell_create_window ();
}

static void
action_page_setup_cb (GtkAction *action,
                      EShellWindow *window)
{
	e_print_run_page_setup_dialog (GTK_WINDOW (window));
}

static void
action_preferences_cb (GtkAction *action,
                       EShellWindow *window)
{
	GtkWidget *preferences_window;

	preferences_window = e_shell_get_preferences_window ();
	gtk_window_present (GTK_WINDOW (preferences_window));

	/* FIXME Switch to a page appropriate for the current view. */
}

static void
action_quick_reference_cb (GtkAction *action,
                           EShellWindow *window)
{
	const gchar * const *language_names;

	language_names = g_get_language_names ();
	while (*language_names != NULL) {
		const gchar *language = *language_names++;
		gchar *filename;

		/* This must be a valid language AND a language with
		 * no encoding suffix.  The next language should have
		 * no encoding suffix. */
		if (language == NULL || strchr (language, '.') != NULL)
			continue;

		filename = g_build_filename (
			EVOLUTION_HELPDIR, "quickref",
			language, "quickref.pdf", NULL);

		if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
			GFile *file;
			gchar *uri;
			GError *error = NULL;

			file = g_file_new_for_path (filename);
			uri = g_file_get_uri (file);

			g_app_info_launch_default_for_uri (uri, NULL, &error);

			if (error != NULL) {
				/* FIXME Show an error dialog. */
				g_warning ("%s", error->message);
				g_error_free (error);
			}

			g_object_unref (file);
			g_free (uri);
		}

		g_free (filename);
	}
}

static void
action_quit_cb (GtkAction *action,
                EShellWindow *window)
{
	e_shell_quit ();
}

static void
action_send_receive_cb (GtkAction *action,
                        EShellWindow *window)
{
	e_shell_send_receive (GTK_WINDOW (window));
}

static void
action_shell_view_cb (GtkRadioAction *action,
                      GtkRadioAction *current,
                      EShellWindow *window)
{
	gint value;

	if (action != current)
		return;

	value = gtk_radio_action_get_current_value (action);
}

static void
action_show_sidebar_cb (GtkToggleAction *action,
                        EShellWindow *window)
{
	GtkWidget *widget;
	gboolean active;

	widget = window->priv->sidebar_notebook;
	active = gtk_toggle_action_get_active (action);
	g_object_set (widget, "visible", active, NULL);
}

static void
action_show_statusbar_cb (GtkToggleAction *action,
                          EShellWindow *window)
{
	GtkWidget *widget;
	gboolean active;

	widget = window->priv->status_area;
	active = gtk_toggle_action_get_active (action);
	g_object_set (widget, "visible", active, NULL);
}

static void
action_show_switcher_cb (GtkToggleAction *action,
                         EShellWindow *window)
{
	ESidebar *sidebar;
	gboolean active;

	sidebar = E_SIDEBAR (window->priv->sidebar);
	active = gtk_toggle_action_get_active (action);
	e_sidebar_set_actions_visible (sidebar, active);
}

static void
action_show_toolbar_cb (GtkToggleAction *action,
                        EShellWindow *window)
{
	GtkWidget *widget;
	gboolean active;

	widget = window->priv->main_toolbar;
	active = gtk_toggle_action_get_active (action);
	g_object_set (widget, "visible", active, NULL);
}

static void
action_submit_bug_cb (GtkAction *action,
                      EShellWindow *window)
{
	const gchar *command_line;
	GError *error = NULL;

	command_line = "bug-buddy --sm-disable --package=Evolution";

	g_debug ("Spawning: %s", command_line);
	g_spawn_command_line_async (command_line, &error);

	if (error != NULL) {
		const gchar *message;

		if (error->code == G_SPAWN_ERROR_NOENT)
			message = _("Bug Buddy is not installed.");
		else
			message = _("Bug Buddy could not be run.");
		e_notice (window, GTK_MESSAGE_ERROR, message);
		g_error_free (error);
	}
}

static void
action_switcher_style_cb (GtkRadioAction *action,
                          GtkRadioAction *current,
                          EShellWindow *window)
{
	/* FIXME  Unfinished. */
}

static void
action_sync_options_cb (GtkAction *action,
                        EShellWindow *window)
{
	const gchar *command_line;
	GError *error = NULL;

	command_line = "gpilotd-control-applet";

	g_debug ("Spawning: %s", command_line);
	g_spawn_command_line_async (command_line, &error);

	if (error != NULL) {
		const gchar *message;

		if (error->code == G_SPAWN_ERROR_NOENT)
			message = _("GNOME Pilot is not installed.");
		else
			message = _("GNOME Pilot could not be run.");
		e_notice (window, GTK_MESSAGE_ERROR, message);
		g_error_free (error);
	}
}

static void
action_work_offline_cb (GtkAction *action,
                        EShellWindow *window)
{
	e_shell_go_offline ();
}

static void
action_work_online_cb (GtkAction *action,
                       EShellWindow *window)
{
	e_shell_go_online ();
}

static GtkActionEntry shell_entries[] = {

	{ "about",
	  GTK_STOCK_ABOUT,
	  NULL,
	  NULL,
	  N_("Show information about Evolution"),
	  G_CALLBACK (action_about_cb) },

	{ "close",
	  GTK_STOCK_CLOSE,
	  N_("_Close Window"),
	  "<Control>w",
	  N_("Close this window"),
	  G_CALLBACK (action_close_cb) },

	{ "contents",
	  GTK_STOCK_HELP,
	  N_("_Contents"),
	  NULL,
	  N_("Open the Evolution User Guide"),
	  G_CALLBACK (action_contents_cb) },

	{ "faq",
	  GTK_STOCK_DIALOG_INFO,
	  N_("Evolution _FAQ"),
	  NULL,
	  N_("Open the Frequently Asked Questions webpage"),
	  G_CALLBACK (action_faq_cb) },

	{ "forget-passwords",
	  NULL,
	  N_("_Forget Passwords"),
	  NULL,
	  N_("Forget all remembered passwords"),
	  G_CALLBACK (action_forget_passwords_cb) },

	{ "import",
	  "stock_mail-import",
	  N_("I_mport..."),
	  NULL,
	  N_("Import data from other programs"),
	  G_CALLBACK (action_import_cb) },

	{ "new-window",
	  "window-new",
	  N_("New _Window"),
	  "<Control><Shift>w",
	  N_("Create a new window displaying this view"),
	  G_CALLBACK (action_new_window_cb) },

	{ "page-setup",
	  GTK_STOCK_PAGE_SETUP,
	  NULL,
	  NULL,
	  N_("Change the page settings for your current printer"),
	  G_CALLBACK (action_page_setup_cb) },

	{ "preferences",
	  GTK_STOCK_PREFERENCES,
	  NULL,
	  "<Control><Shift>s",
	  N_("Configure Evolution"),
	  G_CALLBACK (action_preferences_cb) },

	{ "quick-reference",
	  NULL,
	  N_("_Quick Reference"),
	  NULL,
	  N_("Show Evolution's shortcut keys"),
	  G_CALLBACK (action_quick_reference_cb) },

	{ "quit",
	  GTK_STOCK_QUIT,
	  NULL,
	  NULL,
	  N_("Exit the program"),
	  G_CALLBACK (action_quit_cb) },

	{ "send-receive",
	  "mail-send-receive",
	  N_("Send / _Receive"),
	  "F9",
	  N_("Send queued items and retrieve new items"),
	  G_CALLBACK (action_send_receive_cb) },

	{ "submit-bug",
	  NULL,
	  N_("Submit _Bug Report"),
	  NULL,
	  N_("Submit a bug report using Bug Buddy"),
	  G_CALLBACK (action_submit_bug_cb) },

	{ "sync-options",
	  NULL,
	  N_("_Synchronization Options..."),
	  NULL,
	  N_("Set up Pilot configuration"),
	  G_CALLBACK (action_sync_options_cb) },

	{ "work-offline",
	  "stock_disconnect",
	  N_("_Work Offline"),
	  NULL,
	  N_("Put Evolution into offline mode"),
	  G_CALLBACK (action_work_offline_cb) },

	{ "work-online",
	  "stock_connect",
	  N_("_Work Online"),
	  NULL,
	  N_("Put Evolution into online mode"),
	  G_CALLBACK (action_work_online_cb) },

	/*** Menus ***/

	{ "edit-menu",
	  NULL,
	  N_("_Edit"),
	  NULL,
	  NULL,
	  NULL },

	{ "file-menu",
	  NULL,
	  N_("_File"),
	  NULL,
	  NULL,
	  NULL },

	{ "help-menu",
	  NULL,
	  N_("_Help"),
	  NULL,
	  NULL,
	  NULL },

	{ "layout-menu",
	  NULL,
	  N_("Lay_out"),
	  NULL,
	  NULL,
	  NULL },

	{ "new-menu",
	  GTK_STOCK_NEW,
	  N_("_New"),
	  NULL,
	  NULL,
	  NULL },

	{ "search-menu",
	  NULL,
	  N_("_Search"),
	  NULL,
	  NULL,
	  NULL },

	{ "switcher-menu",
	  NULL,
	  N_("_Switcher Appearance"),
	  NULL,
	  NULL,
	  NULL },

	{ "view-menu",
	  NULL,
	  N_("_View"),
	  NULL,
	  NULL,
	  NULL },

	{ "window-menu",
	  NULL,
	  N_("_Window"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkToggleActionEntry shell_toggle_entries[] = {

	{ "show-sidebar",
	  NULL,
	  N_("Show Side _Bar"),
	  NULL,
	  N_("Show the side bar"),
	  G_CALLBACK (action_show_sidebar_cb),
	  TRUE },

	{ "show-statusbar",
	  NULL,
	  N_("Show _Status Bar"),
	  NULL,
	  N_("Show the status bar"),
	  G_CALLBACK (action_show_statusbar_cb),
	  TRUE },

	{ "show-switcher",
	  NULL,
	  N_("Show _Buttons"),
	  NULL,
	  N_("Show the switcher buttons"),
	  G_CALLBACK (action_show_switcher_cb),
	  TRUE },

	{ "show-toolbar",
	  NULL,
	  N_("Show _Toolbar"),
	  NULL,
	  N_("Show the toolbar"),
	  G_CALLBACK (action_show_toolbar_cb),
	  TRUE }
};

static GtkRadioActionEntry shell_switcher_style_entries[] = {

	{ "switcher-style-icons",
	  NULL,
	  N_("_Icons Only"),
	  NULL,
	  N_("Display window buttons with icons only"),
	  E_SWITCHER_ICONS },

	{ "switcher-style-text",
	  NULL,
	  N_("_Text Only"),
	  NULL,
	  N_("Display window buttons with text only"),
	  E_SWITCHER_TEXT },

	{ "switcher-style-both",
	  NULL,
	  N_("Icons _and Text"),
	  NULL,
	  N_("Display window buttons with icons and text"),
	  E_SWITCHER_BOTH },

	{ "switcher-style-user",
	  NULL,
	  N_("Tool_bar Style"),
	  NULL,
	  N_("Display window buttons using the desktop toolbar setting"),
	  E_SWITCHER_USER }
};

void
e_shell_window_actions_init (EShellWindow *window)
{
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	const gchar *domain;

	g_return_if_fail (E_IS_SHELL_WINDOW (window));

	manager = e_shell_window_get_ui_manager (window);
	domain = GETTEXT_PACKAGE;

	/* Shell Actions */
	action_group = window->priv->shell_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, shell_entries,
		G_N_ELEMENTS (shell_entries), window);
	gtk_action_group_add_toggle_actions (
		action_group, shell_toggle_entries,
		G_N_ELEMENTS (shell_toggle_entries), window);
	gtk_action_group_add_radio_actions (
		action_group, shell_switcher_style_entries,
		G_N_ELEMENTS (shell_switcher_style_entries),
		E_SWITCHER_USER,
		G_CALLBACK (action_switcher_style_cb),  window);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* New Item Actions (empty) */
	action_group = window->priv->new_item_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* New Group Actions (empty) */
	action_group = window->priv->new_group_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* New Source Actions (empty) */
	action_group = window->priv->new_source_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Shell View Actions (empty) */
	action_group = window->priv->shell_view_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
}

void
e_shell_window_create_shell_view_actions (EShellWindow *window)
{
	GType *types;
	GSList *group = NULL;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	guint n_types, ii;
	guint merge_id;

	g_return_if_fail (E_IS_SHELL_WINDOW (window));

	action_group = window->priv->shell_view_actions;
	types = e_shell_registry_get_view_types (&n_types);
	manager = e_shell_window_get_ui_manager (window);
	merge_id = gtk_ui_manager_new_merge_id (manager);

	for (ii = 0; ii < n_types; ii++) {
		EShellViewClass *class;
		GtkRadioAction *action;
		const gchar *type_name;
		gchar *action_name;
		gchar *tooltip;

		class = g_type_class_ref (types[ii]);
		type_name = g_type_name (types[ii]);

		if (class->label != NULL) {
			g_critical ("Label member not set on %s", type_name);
			continue;
		}

		action_name = g_strdup_printf ("shell-view-%s", type_name);
		tooltip = g_strdup_printf (_("Switch to %s"), class->label);

		action = gtk_radio_action_new (
			action_name, class->label,
			tooltip, class->icon_name, ii);

		g_signal_connect (
			action, "changed",
			G_CALLBACK (action_shell_view_cb), window);

		gtk_radio_action_set_group (action, group);
		group = gtk_radio_action_get_group (action);

		gtk_action_group_add_action (
			action_group, GTK_ACTION (action));

		e_sidebar_add_action (
			E_SIDEBAR (window->priv->sidebar),
			GTK_ACTION (action));

		gtk_ui_manager_add_ui (
			manager, merge_id,
			"/main-menu/view-menu/window-menu",
			action_name, action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		g_free (action_name);
		g_free (tooltip);

		g_type_class_unref (class);
	}

	g_free (types);
}
