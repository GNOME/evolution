/*
 * e-shell-window-actions.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-shell-window-private.h"
#include "e-preferences-window.h"

#include <e-util/e-util-private.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-alert-dialog.h>
#include <e-util/e-print.h>
#include <gal-define-views-dialog.h>

#include <libedataserverui/e-passwords.h>

#define EVOLUTION_COPYRIGHT \
	"Copyright \xC2\xA9 1999 - 2008 Novell, Inc. and Others"

#define EVOLUTION_FAQ \
	"http://live.gnome.org/Evolution/FAQ"

#define EVOLUTION_WEBSITE \
	"http://www.gnome.org/projects/evolution/"

/* Authors and Documenters
 *
 * The names below must be in UTF-8.  The breaking of escaped strings
 * is so the hexadecimal sequences don't swallow too many characters.
 *
 * SO THAT MEANS, FOR 8-BIT CHARACTERS USE \xXX HEX ENCODING ONLY!
 *
 * Not all environments are UTF-8 and not all editors can handle it.
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
	"Jonathon Jongsma",
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

/**
 * E_SHELL_WINDOW_ACTION_ABOUT:
 * @window: an #EShellWindow
 *
 * Activation of this action displays the application's About dialog.
 *
 * Main menu item: Help -> About
 **/
static void
action_about_cb (GtkAction *action,
                 EShellWindow *shell_window)
{
	gchar *translator_credits;

	/* The translator-credits string is for translators to list
	 * per-language credits for translation, displayed in the
	 * about dialog. */
	translator_credits = _("translator-credits");
	if (strcmp (translator_credits, "translator-credits") == 0)
		translator_credits = NULL;

	gtk_show_about_dialog (
		GTK_WINDOW (shell_window),
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

/**
 * E_SHELL_WINDOW_ACTION_CLOSE:
 * @window: an #EShellWindow
 *
 * Activation of this action closes @window.  If this is the last window,
 * the application initiates shutdown.
 *
 * Main menu item: File -> Close
 **/
static void
action_close_cb (GtkAction *action,
                 EShellWindow *shell_window)
{
	GtkWidget *widget;
	GdkWindow *window;
	GdkEvent *event;

	widget = GTK_WIDGET (shell_window);
	window = gtk_widget_get_window (widget);

	/* Synthesize a delete_event on this window. */
	event = gdk_event_new (GDK_DELETE);
	event->any.window = g_object_ref (window);
	event->any.send_event = TRUE;
	gtk_main_do_event (event);
	gdk_event_free (event);
}

/**
 * E_SHELL_WINDOW_ACTION_CONTENTS:
 * @window: an #EShellWindow
 *
 * Activation of this action opens the application's user manual.
 *
 * Main menu item: Help -> Contents
 **/
static void
action_contents_cb (GtkAction *action,
                    EShellWindow *shell_window)
{
	e_display_help (GTK_WINDOW (shell_window), NULL);
}

static void
action_custom_rule_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
	EFilterRule *rule;
	EShellView *shell_view;
	const gchar *view_name;

	rule = g_object_get_data (G_OBJECT (action), "rule");
	g_return_if_fail (rule != NULL);

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	rule = g_object_get_data (G_OBJECT (action), "rule");
	g_return_if_fail (E_IS_FILTER_RULE (rule));

	e_shell_view_custom_search (shell_view, rule);
}

/**
 * E_SHELL_WINDOW_ACTION_FAQ:
 * @window: an #EShellWindow
 *
 * Activation of this action opens a web page with answers to frequently
 * asked questions about this application.
 *
 * Main menu item: Help -> Evolution FAQ
 **/
static void
action_faq_cb (GtkAction *action,
               EShellWindow *shell_window)
{
	e_show_uri (GTK_WINDOW (shell_window), EVOLUTION_FAQ);
}

/**
 * E_SHELL_WINDOW_ACTION_FORGET_PASSWORDS:
 * @window: an #EShellWindow
 *
 * Activation of this action deletes all stored passwords.
 *
 * Main menu item: File -> Forget Passwords
 **/
static void
action_forget_passwords_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	gint response;

	response = e_alert_run_dialog_for_args (
		GTK_WINDOW (shell_window), "shell:forget-passwords", NULL);

	if (response == GTK_RESPONSE_OK)
		e_passwords_forget_passwords ();
}

/**
 * E_SHELL_WINDOW_ACTION_GAL_DEFINE_VIEWS:
 * @window: an #EShellWindow
 *
 * Activation of this action opens a dialog for editing GAL views for
 * the current shell view.
 *
 * Main menu item: View -> Current View -> Define Views...
 **/
static void
action_gal_define_views_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	GalViewCollection *view_collection;
	GtkWidget *dialog;
	const gchar *view_name;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	view_collection = shell_view_class->view_collection;
	g_return_if_fail (view_collection != NULL);

	dialog = gal_define_views_dialog_new (view_collection);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gal_view_collection_save (view_collection);
	gtk_widget_destroy (dialog);

	e_shell_window_update_view_menu (shell_window);
}

/**
 * E_SHELL_WINDOW_ACTION_GAL_CUSTOM_VIEW:
 * @window: an #EShellWindow
 *
 * This radio action is selected when using a custom GAL view that has
 * not been saved.
 *
 * Main menu item: View -> Current View -> Custom View
 **/
static void
action_gal_view_cb (GtkRadioAction *action,
                    GtkRadioAction *current,
                    EShellWindow *shell_window)
{
	EShellView *shell_view;
	const gchar *view_name;
	const gchar *view_id;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	view_id = g_object_get_data (G_OBJECT (current), "view-id");
	e_shell_view_set_view_id (shell_view, view_id);
}

/**
 * E_SHELL_WINDOW_ACTION_GAL_SAVE_CUSTOM_VIEW:
 * @window: an #EShellWindow
 *
 * Activation of this action saves a custom GAL view.
 *
 * Main menu item: View -> Current View -> Save Custom View...
 **/

/**
 * E_SHELL_WINDOW_ACTION_IMPORT:
 * @window: an #EShellWindow
 *
 * Activation of this action opens the Evolution Import Assistant.
 *
 * Main menu item: File -> Import...
 **/
static void
action_import_cb (GtkAction *action,
                  EShellWindow *shell_window)
{
	GtkWidget *assistant;

	assistant = e_import_assistant_new (GTK_WINDOW (shell_window));

	/* These are "Run Last" signals, so use g_signal_connect_after()
	 * to give the default handlers a chance to run before we destroy
	 * the window. */

	g_signal_connect_after (
		assistant, "cancel",
		G_CALLBACK (gtk_widget_destroy), NULL);

	g_signal_connect_after (
		assistant, "finished",
		G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (assistant);
}

/**
 * E_SHELL_WINDOW_ACTION_NEW_WINDOW:
 * @window: an #EShellWindow
 *
 * Activation of this action opens a new shell window.
 *
 * Main menu item: File -> New Window
 **/
static void
action_new_window_cb (GtkAction *action,
                      EShellWindow *shell_window)
{
	EShell *shell;
	const gchar *view_name;

	shell = e_shell_window_get_shell (shell_window);
	view_name = e_shell_window_get_active_view (shell_window);

	e_shell_create_shell_window (shell, view_name);
}

/**
 * E_SHELL_WINDOW_ACTION_PAGE_SETUP:
 * @window: an #EShellWindow
 *
 * Activation of this action opens the application's Page Setup dialog.
 *
 * Main menu item: File -> Page Setup...
 **/
static void
action_page_setup_cb (GtkAction *action,
                      EShellWindow *shell_window)
{
	e_print_run_page_setup_dialog (GTK_WINDOW (shell_window));
}

/**
 * E_SHELL_WINDOW_ACTION_PREFERENCES:
 * @window: an #EShellWindow
 *
 * Activation of this action opens the application's Preferences window.
 *
 * Main menu item: Edit -> Preferences
 **/
static void
action_preferences_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
	EShell *shell;
	GtkWidget *preferences_window;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellBackendClass *shell_backend_class;
	const gchar *view_name;

	shell = e_shell_window_get_shell (shell_window);
	preferences_window = e_shell_get_preferences_window (shell);
	e_preferences_window_setup (E_PREFERENCES_WINDOW (preferences_window));

	gtk_window_set_transient_for (
		GTK_WINDOW (preferences_window),
		GTK_WINDOW (shell_window));
	gtk_window_set_position (
		GTK_WINDOW (preferences_window),
		GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_present (GTK_WINDOW (preferences_window));

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_backend_class = E_SHELL_BACKEND_GET_CLASS (shell_backend);

	if (shell_backend_class->preferences_page != NULL) {
		if (e_shell_get_express_mode (shell)) {
			e_preferences_window_filter_page (
				E_PREFERENCES_WINDOW (preferences_window),
				shell_backend_class->preferences_page);
		} else {
			e_preferences_window_show_page (
				E_PREFERENCES_WINDOW (preferences_window),
				shell_backend_class->preferences_page);
		}
	}
}

/**
 * E_SHELL_WINDOW_ACTION_QUICK_REFERENCE:
 * @window: an #EShellWindow
 *
 * Activation of this action opens a printable table of useful shortcut
 * keys for this application.
 *
 * Main menu item: Help -> Quick Reference
 **/
static void
action_quick_reference_cb (GtkAction *action,
                           EShellWindow *shell_window)
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

/**
 * E_SHELL_WINDOW_ACTION_QUIT:
 * @window: an #EShellWindow
 *
 * Activation of this action initiates application shutdown.
 *
 * Main menu item: File -> Quit
 **/
static void
action_quit_cb (GtkAction *action,
                EShellWindow *shell_window)
{
	EShell *shell;

	shell = e_shell_window_get_shell (shell_window);
	e_shell_quit (shell, E_SHELL_QUIT_ACTION);
}

/**
 * E_SHELL_WINDOW_ACTION_SEARCH_ADVANCED:
 * @window: an #EShellWindow
 *
 * Activation of this action opens an Advanced Search dialog.
 *
 * Main menu item: Search -> Advanced Search...
 **/
static void
action_search_advanced_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	const gchar *view_name;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_content = e_shell_view_get_shell_content (shell_view);

	e_shell_content_run_advanced_search_dialog (shell_content);
	e_shell_window_update_search_menu (shell_window);
}

/**
 * E_SHELL_WINDOW_ACTION_SEARCH_CLEAR:
 * @window: an #EShellWindow
 *
 * Activation of this action clears the most recent search results.
 *
 * Main menu item: Search -> Clear
 **/
static void
action_search_clear_cb (GtkAction *action,
                        EShellWindow *shell_window)
{
	EShellView *shell_view;
	const gchar *view_name;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	e_shell_view_clear_search (shell_view);
}

/**
 * E_SHELL_WINDOW_ACTION_SEARCH_EDIT:
 * @window: an #EShellWindow
 *
 * Activation of this action opens a dialog for editing saved searches.
 *
 * Main menu item: Search -> Edit Saved Searches...
 **/
static void
action_search_edit_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	const gchar *view_name;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_content = e_shell_view_get_shell_content (shell_view);

	e_shell_content_run_edit_searches_dialog (shell_content);
	e_shell_window_update_search_menu (shell_window);
}

/**
 * E_SHELL_WINDOW_ACTION_SEARCH_OPTIONS:
 * @window: an #EShellWindow
 *
 * Activation of this action displays a menu of search options.
 * This appears as a "find" icon in the window's search entry.
 **/
static void
action_search_options_cb (GtkAction *action,
                          EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	const gchar *view_name;
	const gchar *widget_path;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);

	widget_path = shell_view_class->search_options;
	e_shell_view_show_popup_menu (shell_view, widget_path, NULL);
}

/**
 * E_SHELL_WINDOW_ACTION_SEARCH_QUICK:
 * @window: an #EShellWindow
 *
 * Activation of this action executes the current search conditions.
 *
 * Main menu item: Search -> Find Now
 **/
static void
action_search_quick_cb (GtkAction *action,
                        EShellWindow *shell_window)
{
	EShellView *shell_view;
	const gchar *view_name;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	e_shell_view_execute_search (shell_view);
}

/**
 * E_SHELL_WINDOW_ACTION_SEARCH_SAVE:
 * @window: an #EShellWindow
 *
 * Activation of this action saves the current search conditions.
 *
 * Main menu item: Search -> Save Search...
 **/
static void
action_search_save_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	const gchar *view_name;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_content = e_shell_view_get_shell_content (shell_view);

	e_shell_content_run_save_search_dialog (shell_content);
	e_shell_window_update_search_menu (shell_window);
}

/**
 * E_SHELL_WINDOW_ACTION_SEND_RECEIVE:
 * @window: an #EShellWindow
 *
 * Activation of this action opens the Send &amp; Receive Mail dialog.
 *
 * Main menu item: File -> Send / Receive
 **/
static void
action_send_receive_cb (GtkAction *action,
                        EShellWindow *shell_window)
{
	EShell *shell;

	shell = e_shell_window_get_shell (shell_window);
	e_shell_send_receive (shell, GTK_WINDOW (shell_window));
}

/**
 * E_SHELL_WINDOW_ACTION_SHOW_SIDEBAR:
 * @window: an #EShellWindow
 *
 * This toggle action controls whether the side bar is visible.
 *
 * Main menu item: View -> Layout -> Show Side Bar
 **/

/**
 * E_SHELL_WINDOW_ACTION_SHOW_SWITCHER:
 * @window: an #EShellWindow
 *
 * This toggle action controls whether the switcher buttons are visible.
 *
 * Main menu item: View -> Switcher Appearance -> Show Buttons
 **/

/**
 * E_SHELL_WINDOW_ACTION_SHOW_TASKBAR:
 * @window: an #EShellWindow
 *
 * This toggle action controls whether the task bar is visible.
 *
 * Main menu item: View -> Layout -> Show Status Bar
 **/

/**
 * E_SHELL_WINDOW_ACTION_SHOW_TOOLBAR:
 * @window: an #EShellWindow
 *
 * This toggle action controls whether the tool bar is visible.
 *
 * Main menu item: View -> Layout -> Show Tool Bar
 **/

/**
 * E_SHELL_WINDOW_ACTION_SUBMIT_BUG:
 * @window: an #EShellWindow
 *
 * Activation of this action allows users to report a bug using
 * Bug Buddy.
 *
 * Main menu item: Help -> Submit Bug Report
 **/
static void
action_submit_bug_cb (GtkAction *action,
                      EShellWindow *shell_window)
{
	const gchar *command_line;
	GError *error = NULL;

	command_line = "bug-buddy --package=Evolution";

	g_debug ("Spawning: %s", command_line);
	g_spawn_command_line_async (command_line, &error);

	if (error != NULL) {
		const gchar *message;

		if (error->code == G_SPAWN_ERROR_NOENT)
			message = _("Bug Buddy is not installed.");
		else
			message = _("Bug Buddy could not be run.");
		e_notice (shell_window, GTK_MESSAGE_ERROR, message);
		g_error_free (error);
	}
}

static void
action_switcher_cb (GtkRadioAction *action,
                    GtkRadioAction *current,
                    EShellWindow *shell_window)
{
	const gchar *view_name;

	view_name = g_object_get_data (G_OBJECT (current), "view-name");
	e_shell_window_switch_to_view (shell_window, view_name);
}

static void
action_new_view_window_cb (GtkAction *action,
			   EShellWindow *shell_window)
{
	EShell *shell;
	const gchar *view_name;

	shell = e_shell_window_get_shell (shell_window);
	view_name = g_object_get_data (G_OBJECT (action), "view-name");

	fprintf (stderr, "create new window: %s\n", view_name);

	e_shell_create_shell_window (shell, view_name);
}

/**
 * E_SHELL_WINDOW_ACTION_SWITCHER_STYLE_BOTH:
 * @window: an #EShellWindow
 *
 * This radio action displays switcher buttons with icons and text.
 *
 * Main menu item: View -> Switcher Appearance -> Icons and Text
 **/

/**
 * E_SHELL_WINDOW_ACTION_SWITCHER_STYLE_ICONS:
 * @window: an #EShellWindow
 *
 * This radio action displays switcher buttons with icons only.
 *
 * Main menu item: View -> Switcher Appearance -> Icons Only
 **/

/**
 * E_SHELL_WINDOW_ACTION_SWITCHER_STYLE_TEXT:
 * @window: an #EShellWindow
 *
 * This radio action displays switcher buttons with text only.
 *
 * Main menu item: View -> Switcher Appearance -> Text Only
 **/

/**
 * E_SHELL_WINDOW_ACTION_SWITCHER_STYLE_USER:
 * @window: an #EShellWindow
 *
 * This radio action displays switcher buttons according to the desktop
 * toolbar setting.
 *
 * Main menu item: View -> Switcher Appearance -> Toolbar Style
 **/
static void
action_switcher_style_cb (GtkRadioAction *action,
                          GtkRadioAction *current,
                          EShellWindow *shell_window)
{
	EShellSwitcher *switcher;
	GtkToolbarStyle style;

	switcher = E_SHELL_SWITCHER (shell_window->priv->switcher);
	style = gtk_radio_action_get_current_value (action);

	switch (style) {
		case GTK_TOOLBAR_ICONS:
		case GTK_TOOLBAR_TEXT:
		case GTK_TOOLBAR_BOTH:
		case GTK_TOOLBAR_BOTH_HORIZ:
			e_shell_switcher_set_style (switcher, style);
			break;

		default:
			e_shell_switcher_unset_style (switcher);
			break;
	}
}

/**
 * E_SHELL_WINDOW_ACTION_WORK_OFFLINE:
 * @window: an #EShellWindow
 *
 * Activation of this action puts the application into offline mode.
 *
 * Main menu item: File -> Work Offline
 **/
static void
action_work_offline_cb (GtkAction *action,
                        EShellWindow *shell_window)
{
	EShell *shell;
	EShellSettings *shell_settings;

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	e_shell_set_online (shell, FALSE);

	/* XXX The sense of the setting is reversed.  Would be more
	 *     intuitive and less error-prone as "start-online". */
	e_shell_settings_set_boolean (
		shell_settings, "start-offline", TRUE);
}

/**
 * E_SHELL_WINDOW_ACTION_WORK_ONLINE:
 * @window: an #EShellWindow
 *
 * Activation of this action puts the application into online mode.
 *
 * Main menu item: File -> Work Online
 **/
static void
action_work_online_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
	EShell *shell;
	EShellSettings *shell_settings;

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	e_shell_set_online (shell, TRUE);

	/* XXX The sense of the setting is reversed.  Would be more
	 *     intuitive and less error-prone as "start-online". */
	e_shell_settings_set_boolean (
		shell_settings, "start-offline", FALSE);
}

/**
 * E_SHELL_WINDOW_ACTION_GROUP_CUSTOM_RULES:
 * @window: an #EShellWindow
 **/

/**
 * E_SHELL_WINDOW_ACTION_GROUP_GAL_VIEW:
 * @window: an #EShellWindow
 **/

/**
 * E_SHELL_WINDOW_ACTION_GROUP_NEW_ITEM:
 * @window: an #EShellWindow
 **/

/**
 * E_SHELL_WINDOW_ACTION_GROUP_NEW_SOURCE:
 * @window: an #EShellWindow
 **/

/**
 * E_SHELL_WINDOW_ACTION_GROUP_SHELL:
 * @window: an #EShellWindow
 **/

/**
 * E_SHELL_WINDOW_ACTION_GROUP_SWITCHER:
 * @window: an #EShellWindow
 **/

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

	{ "close-window-menu",
	  GTK_STOCK_CLOSE,
	  NULL,
	  "<Control>w",
	  N_("Close this window"),
	  G_CALLBACK (action_close_cb) },

	{ "close-window",
	  GTK_STOCK_CLOSE,
	  N_("_Close Window"),
	  "<Control>w",
	  N_("Close this window"),
	  G_CALLBACK (action_close_cb) },

	{ "contents",
	  GTK_STOCK_HELP,
	  N_("_Contents"),
	  "F1",
	  N_("Open the Evolution User Guide"),
	  G_CALLBACK (action_contents_cb) },

	{ "copy-clipboard",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "cut-clipboard",
	  GTK_STOCK_CUT,
	  NULL,
	  NULL,
	  N_("Cut the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "delete-selection",
	  GTK_STOCK_DELETE,
	  NULL,
	  NULL,
	  N_("Delete the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "faq",
	  "help-faq",
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

	{ "paste-clipboard",
	  GTK_STOCK_PASTE,
	  NULL,
	  NULL,
	  N_("Paste the clipboard"),
	  NULL },  /* Handled by EFocusTracker */

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

	{ "search-advanced",
	  NULL,
	  N_("_Advanced Search..."),
	  NULL,
	  N_("Construct a more advanced search"),
	  G_CALLBACK (action_search_advanced_cb) },

	{ "search-clear",
	  GTK_STOCK_CLEAR,
	  NULL,
	  "<Control><Shift>q",
	  N_("Clear the current search parameters"),
	  G_CALLBACK (action_search_clear_cb) },

	{ "search-edit",
	  NULL,
	  N_("_Edit Saved Searches..."),
	  NULL,
	  N_("Manage your saved searches"),
	  G_CALLBACK (action_search_edit_cb) },

	{ "search-options",
	  GTK_STOCK_FIND,
	  NULL,
	  NULL,
	  N_("Click here to change the search type"),
	  G_CALLBACK (action_search_options_cb) },

	{ "search-quick",
	  GTK_STOCK_FIND,
	  N_("_Find Now"),
	  "",      /* Block the default Ctrl+F. */
	  N_("Execute the current search parameters"),
	  G_CALLBACK (action_search_quick_cb) },

	{ "search-save",
	  NULL,
	  N_("_Save Search..."),
	  NULL,
	  N_("Save the current search parameters"),
	  G_CALLBACK (action_search_save_cb) },

	{ "select-all",
	  GTK_STOCK_SELECT_ALL,
	  NULL,
	  "<Control>a",
	  N_("Select all text"),
	  NULL },  /* Handled by EFocusTracker */

	{ "send-receive",
	  "mail-send-receive",
	  N_("Send / _Receive"),
	  "F9",
	  N_("Send queued items and retrieve new items"),
	  G_CALLBACK (action_send_receive_cb) },

	{ "submit-bug",
	  NULL,
	  N_("Submit _Bug Report..."),
	  NULL,
	  N_("Submit a bug report using Bug Buddy"),
	  G_CALLBACK (action_submit_bug_cb) },

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
	  "",
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

static EPopupActionEntry shell_popup_entries[] = {

	{ "popup-copy-clipboard",
	  NULL,
	  "copy-clipboard" },

	{ "popup-cut-clipboard",
	  NULL,
	  "cut-clipboard" },

	{ "popup-delete-selection",
	  NULL,
	  "delete-selection" },

	{ "popup-paste-clipboard",
	  NULL,
	  "paste-clipboard" }
};

static GtkToggleActionEntry shell_toggle_entries[] = {

	{ "show-sidebar",
	  NULL,
	  N_("Show Side _Bar"),
	  NULL,
	  N_("Show the side bar"),
	  NULL,
	  TRUE },

	{ "show-switcher",
	  NULL,
	  N_("Show _Buttons"),
	  NULL,
	  N_("Show the switcher buttons"),
	  NULL,
	  TRUE },

	{ "show-taskbar",
	  NULL,
	  N_("Show _Status Bar"),
	  NULL,
	  N_("Show the status bar"),
	  NULL,
	  TRUE },

	{ "show-toolbar",
	  NULL,
	  N_("Show _Tool Bar"),
	  NULL,
	  N_("Show the tool bar"),
	  NULL,
	  TRUE }
};

static GtkRadioActionEntry shell_switcher_entries[] = {

	/* This action represents the initial active shell view.
	 * It should not be visible in the UI, nor should it be
	 * possible to switch to it from another shell view. */
	{ "switcher-initial",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  -1 }
};

static GtkRadioActionEntry shell_switcher_style_entries[] = {

	{ "switcher-style-icons",
	  NULL,
	  N_("_Icons Only"),
	  NULL,
	  N_("Display window buttons with icons only"),
	  GTK_TOOLBAR_ICONS },

	{ "switcher-style-text",
	  NULL,
	  N_("_Text Only"),
	  NULL,
	  N_("Display window buttons with text only"),
	  GTK_TOOLBAR_TEXT },

	{ "switcher-style-both",
	  NULL,
	  N_("Icons _and Text"),
	  NULL,
	  N_("Display window buttons with icons and text"),
	  GTK_TOOLBAR_BOTH_HORIZ },

	{ "switcher-style-user",
	  NULL,
	  N_("Tool_bar Style"),
	  NULL,
	  N_("Display window buttons using the desktop toolbar setting"),
	  -1 }
};

static GtkActionEntry shell_gal_view_entries[] = {

	{ "gal-define-views",
	  NULL,
	  N_("Define Views..."),
	  NULL,
	  N_("Create or edit views"),
	  G_CALLBACK (action_gal_define_views_cb) },

	{ "gal-save-custom-view",
	  NULL,
	  N_("Save Custom View..."),
	  NULL,
	  N_("Save current custom view"),
	  NULL },  /* Handled by subclasses. */

	/*** Menus ***/

	{ "gal-view-menu",
	  NULL,
	  N_("C_urrent View"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkRadioActionEntry shell_gal_view_radio_entries[] = {

	{ "gal-custom-view",
	  NULL,
	  N_("Custom View"),
	  NULL,
	  N_("Current view is a customized view"),
	  -1 }
};

static GtkActionEntry shell_lockdown_print_setup_entries[] = {

	{ "page-setup",
	  GTK_STOCK_PAGE_SETUP,
	  NULL,
	  NULL,
	  N_("Change the page settings for your current printer"),
	  G_CALLBACK (action_page_setup_cb) }
};

static void
shell_window_extract_actions (EShellWindow *shell_window,
                              GList **source_list,
                              GList **destination_list)
{
	const gchar *current_view;
	GList *match_list = NULL;
	GList *iter;

	/* Pick out the actions from the source list that are tagged
	 * as belonging to the current EShellView and move them to the
	 * destination list. */

	current_view = e_shell_window_get_active_view (shell_window);

	/* Example: Suppose [A] and [C] are tagged for this EShellView.
	 *
	 *        source_list = [A] -> [B] -> [C]
	 *                       ^             ^
	 *                       |             |
	 *         match_list = [ ] --------> [ ]
	 *
	 *
	 *   destination_list = [1] -> [2]  (other actions)
	 */
	for (iter = *source_list; iter != NULL; iter = iter->next) {
		GtkAction *action = iter->data;
		const gchar *backend_name;

		backend_name = g_object_get_data (
			G_OBJECT (action), "backend-name");

		if (strcmp (backend_name, current_view) != 0)
			continue;

		if (g_object_get_data (G_OBJECT (action), "primary"))
			match_list = g_list_prepend (match_list, iter);
		else
			match_list = g_list_append (match_list, iter);
	}

	/* source_list = [B]   match_list = [A] -> [C] */
	for (iter = match_list; iter != NULL; iter = iter->next) {
		GList *link = iter->data;

		iter->data = link->data;
		*source_list = g_list_delete_link (*source_list, link);
	}

	/* destination_list = [1] -> [2] -> [A] -> [C] */
	*destination_list = g_list_concat (*destination_list, match_list);
}

void
e_shell_window_actions_init (EShellWindow *shell_window)
{
	GtkActionGroup *action_group;
	EFocusTracker *focus_tracker;
	GtkUIManager *ui_manager;
	gchar *path;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	e_ui_manager_add_ui_from_file (
		E_UI_MANAGER (ui_manager), "evolution-shell.ui");

	/* Shell Actions */
	action_group = ACTION_GROUP (SHELL);
	gtk_action_group_add_actions (
		action_group, shell_entries,
		G_N_ELEMENTS (shell_entries), shell_window);
	e_action_group_add_popup_actions (
		action_group, shell_popup_entries,
		G_N_ELEMENTS (shell_popup_entries));
	gtk_action_group_add_toggle_actions (
		action_group, shell_toggle_entries,
		G_N_ELEMENTS (shell_toggle_entries), shell_window);
	gtk_action_group_add_radio_actions (
		action_group, shell_switcher_style_entries,
		G_N_ELEMENTS (shell_switcher_style_entries),
		E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE,
		G_CALLBACK (action_switcher_style_cb), shell_window);
	gtk_action_group_add_actions (
		action_group, shell_gal_view_entries,
		G_N_ELEMENTS (shell_gal_view_entries), shell_window);
	gtk_action_group_add_radio_actions (
		action_group, shell_gal_view_radio_entries,
		G_N_ELEMENTS (shell_gal_view_radio_entries),
		0, G_CALLBACK (action_gal_view_cb), shell_window);

	/* Switcher Actions */
	action_group = ACTION_GROUP (SWITCHER);
	gtk_action_group_add_radio_actions (
		action_group, shell_switcher_entries,
		G_N_ELEMENTS (shell_switcher_entries),
		-1, G_CALLBACK (action_switcher_cb), shell_window);

	/* Lockdown Print Setup Actions */
	action_group = ACTION_GROUP (LOCKDOWN_PRINT_SETUP);
	gtk_action_group_add_actions (
		action_group, shell_lockdown_print_setup_entries,
		G_N_ELEMENTS (shell_lockdown_print_setup_entries),
		shell_window);

	/* Configure an EFocusTracker to manage selection actions. */

	focus_tracker = e_focus_tracker_new (GTK_WINDOW (shell_window));
	e_focus_tracker_set_cut_clipboard_action (
		focus_tracker, ACTION (CUT_CLIPBOARD));
	e_focus_tracker_set_copy_clipboard_action (
		focus_tracker, ACTION (COPY_CLIPBOARD));
	e_focus_tracker_set_paste_clipboard_action (
		focus_tracker, ACTION (PASTE_CLIPBOARD));
	e_focus_tracker_set_delete_selection_action (
		focus_tracker, ACTION (DELETE_SELECTION));
	e_focus_tracker_set_select_all_action (
		focus_tracker, ACTION (SELECT_ALL));
	shell_window->priv->focus_tracker = focus_tracker;

	/* Fine tuning. */

	gtk_action_set_sensitive (ACTION (SEARCH_QUICK), FALSE);

	g_object_set (ACTION (SEND_RECEIVE), "is-important", TRUE, NULL);

	e_mutual_binding_new (
		shell_window, "sidebar-visible",
		ACTION (SHOW_SIDEBAR), "active");

	e_mutual_binding_new (
		shell_window, "switcher-visible",
		ACTION (SHOW_SWITCHER), "active");

	e_mutual_binding_new (
		shell_window, "taskbar-visible",
		ACTION (SHOW_TASKBAR), "active");

	e_mutual_binding_new (
		shell_window, "toolbar-visible",
		ACTION (SHOW_TOOLBAR), "active");

	e_binding_new (
		ACTION (SHOW_SIDEBAR), "active",
		ACTION (SHOW_SWITCHER), "sensitive");

	e_binding_new (
		ACTION (SHOW_SIDEBAR), "active",
		ACTION (SWITCHER_STYLE_BOTH), "sensitive");

	e_binding_new (
		ACTION (SHOW_SIDEBAR), "active",
		ACTION (SWITCHER_STYLE_ICONS), "sensitive");

	e_binding_new (
		ACTION (SHOW_SIDEBAR), "active",
		ACTION (SWITCHER_STYLE_TEXT), "sensitive");

	e_binding_new (
		ACTION (SHOW_SIDEBAR), "active",
		ACTION (SWITCHER_STYLE_USER), "sensitive");

	e_binding_new (
		ACTION (SHOW_SIDEBAR), "active",
		ACTION (SWITCHER_MENU), "sensitive");

	/* Submitting bug reports requires bug-buddy. */
	path = g_find_program_in_path ("bug-buddy");
	if (path == NULL)
		gtk_action_set_visible (ACTION (SUBMIT_BUG), FALSE);
	g_free (path);
}

static GList *
express_filter_new_actions (GList *list)
{
	GList *l, *filtered = NULL;

	for (l = list; l; l = l->next) {
		const gchar *backend_name;

		backend_name = g_object_get_data (
			G_OBJECT (l->data), "backend-name");

		/* only the addressbook pieces in express mode */
		if (!strcmp (backend_name, "addressbook"))
			filtered = g_list_prepend (filtered, l->data);
	}
	g_list_free (list);

	return g_list_reverse (filtered);
}

GtkWidget *
e_shell_window_create_new_menu (EShellWindow *shell_window)
{
	EShell *shell;
	GtkActionGroup *action_group;
	GList *new_item_actions;
	GList *new_source_actions;
	GList *iter, *list = NULL;
	GtkWidget *menu;
	GtkWidget *separator;

	shell = e_shell_window_get_shell (shell_window);

	/* Get sorted lists of "new item" and "new source" actions. */

	action_group = ACTION_GROUP (NEW_ITEM);

	new_item_actions = g_list_sort (
		gtk_action_group_list_actions (action_group),
		(GCompareFunc) e_action_compare_by_label);

	action_group = ACTION_GROUP (NEW_SOURCE);

	new_source_actions = g_list_sort (
		gtk_action_group_list_actions (action_group),
		(GCompareFunc) e_action_compare_by_label);

	/* Give priority to actions that belong to this shell view. */

	shell_window_extract_actions (
		shell_window, &new_item_actions, &list);

	shell_window_extract_actions (
		shell_window, &new_source_actions, &list);

	/* Convert the actions to menu item proxy widgets. */

	for (iter = list; iter != NULL; iter = iter->next)
		iter->data = gtk_action_create_menu_item (iter->data);

	if (e_shell_get_express_mode (shell)) {
		new_item_actions = express_filter_new_actions (new_item_actions);
		g_list_free (new_source_actions);
		new_source_actions = NULL;
	}

	for (iter = new_item_actions; iter != NULL; iter = iter->next)
		iter->data = gtk_action_create_menu_item (iter->data);

	for (iter = new_source_actions; iter != NULL; iter = iter->next)
		iter->data = gtk_action_create_menu_item (iter->data);

	/* Add menu separators. */

	if (new_item_actions != NULL) {
		separator = gtk_separator_menu_item_new ();
		new_item_actions = g_list_prepend (new_item_actions, separator);
		gtk_widget_show (GTK_WIDGET (separator));
	}

	if (new_source_actions != NULL) {
		separator = gtk_separator_menu_item_new ();
		new_source_actions = g_list_prepend (new_source_actions, separator);
		gtk_widget_show (GTK_WIDGET (separator));
	}

	/* Merge everything into one list, reflecting the menu layout. */

	list = g_list_concat (list, new_item_actions);
	new_item_actions = NULL;    /* just for clarity */

	list = g_list_concat (list, new_source_actions);
	new_source_actions = NULL;  /* just for clarity */

	/* And finally, build the menu. */

	menu = gtk_menu_new ();

	for (iter = list; iter != NULL; iter = iter->next)
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), iter->data);

	g_list_free (list);

	return menu;
}

static GtkAction *
e_shell_window_create_switcher_action (GType type, EShellViewClass *class,
				       const gchar *name, const gchar *tooltip,
				       const gchar *view_name)
{
	GtkAction *action;

	action = g_object_new (type, "name", name,
			       "label", class->label,
			       "tooltip", tooltip,
			       "icon-name", class->icon_name,
			       NULL);

	g_object_set_data (G_OBJECT (action), "view-name",
			   (gpointer) view_name);

	return action;
}

/*
 * Create both the actions to switch the current window, and also
 * to create each view in a new window.
 */
void
e_shell_window_create_switcher_actions (EShellWindow *shell_window)
{
	GSList *group = NULL;
	GtkRadioAction *s_action;
	GtkActionGroup *s_action_group;
	GtkActionGroup *n_action_group;
	GtkUIManager *ui_manager;
	EShellSwitcher *switcher;
	EShell *shell;
	GList *list, *iter;
	guint merge_id;
	guint ii = 0;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	s_action_group = ACTION_GROUP (SWITCHER);
	n_action_group = ACTION_GROUP (NEW_WINDOW);
	switcher = E_SHELL_SWITCHER (shell_window->priv->switcher);
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	shell = e_shell_window_get_shell (shell_window);
	list = e_shell_get_shell_backends (shell);

	/* Construct a group of radio actions from the various EShellView
	 * subclasses and register them with the EShellSwitcher.  These
	 * actions are manifested as switcher buttons and View->Window
	 * menu items. */

	s_action = GTK_RADIO_ACTION (ACTION (SWITCHER_INITIAL));
	gtk_radio_action_set_group (s_action, group);
	group = gtk_radio_action_get_group (s_action);

	for (iter = list; iter != NULL; iter = iter->next) {
		EShellBackend *shell_backend = iter->data;
		EShellBackendClass *backend_class;
		EShellViewClass *class;
		GtkAction *n_action;
		GType view_type;
		const gchar *view_name;
		gchar *accelerator;
		gchar *s_action_name;
		gchar *n_action_name;
		gchar *tooltip;

		/* The backend name is also the view name. */
		backend_class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
		view_type = backend_class->shell_view_type;
		view_name = backend_class->name;

		if (!g_type_is_a (view_type, E_TYPE_SHELL_VIEW)) {
			g_critical (
				"%s is not a subclass of %s",
				g_type_name (view_type),
				g_type_name (E_TYPE_SHELL_VIEW));
			continue;
		}

		class = g_type_class_ref (view_type);

		if (class->label == NULL) {
			g_critical (
				"Label member not set on %s",
				G_OBJECT_CLASS_NAME (class));
			continue;
		}

		tooltip = g_strdup_printf (_("Switch to %s"), class->label);

		s_action_name = g_strdup_printf (E_SHELL_SWITCHER_FORMAT, view_name);

		/* Note, we have to set "icon-name" separately because
		 * gtk_radio_action_new() expects a "stock-id".  Sadly,
		 * GTK+ still distinguishes between the two. */

		s_action = GTK_RADIO_ACTION (
			e_shell_window_create_switcher_action (GTK_TYPE_RADIO_ACTION,
							       class, s_action_name,
							       tooltip, view_name));
		g_object_set (G_OBJECT (s_action), "value", ii++, NULL);
		gtk_radio_action_set_group (s_action, group);
		group = gtk_radio_action_get_group (s_action);

		/* The first nine views have accelerators Ctrl+(1-9). */
		if (ii < 10)
			accelerator = g_strdup_printf ("<Control>%d", ii);
		else
			accelerator = g_strdup ("");

		gtk_action_group_add_action_with_accel (
			s_action_group, GTK_ACTION (s_action), accelerator);

		g_free (accelerator);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id,
			"/main-menu/view-menu/window-menu",
			s_action_name, s_action_name,
			GTK_UI_MANAGER_AUTO, FALSE);
		g_free (s_action_name);

		/* Create in new window actions */
		n_action_name = g_strdup_printf (E_SHELL_NEW_WINDOW_FORMAT, view_name);
		n_action = e_shell_window_create_switcher_action (GTK_TYPE_ACTION,
								  class, n_action_name,
								  tooltip, view_name);
		g_signal_connect (G_OBJECT (n_action), "activate",
				  G_CALLBACK (action_new_view_window_cb),
				  shell_window);
		gtk_action_group_add_action (n_action_group, n_action);

		e_shell_switcher_add_action (switcher, GTK_ACTION (s_action), n_action);

		g_free (n_action_name);
		g_free (tooltip);

		g_type_class_unref (class);
	}
}

void
e_shell_window_update_view_menu (EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GalViewCollection *view_collection;
	GtkRadioAction *radio_action;
	GtkAction *action;
	GSList *radio_group;
	gboolean visible;
	const gchar *path;
	const gchar *view_id;
	const gchar *view_name;
	guint merge_id;
	gint count, ii;

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	g_return_if_fail (shell_view != NULL);

	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	view_collection = shell_view_class->view_collection;
	view_id = e_shell_view_get_view_id (shell_view);
	g_return_if_fail (view_collection != NULL);

	action_group = ACTION_GROUP (GAL_VIEW);
	merge_id = shell_window->priv->gal_view_merge_id;

	/* Unmerge the previous menu. */
	gtk_ui_manager_remove_ui (ui_manager, merge_id);
	e_action_group_remove_all_actions (action_group);

	/* We have a view ID, so forge ahead. */
	count = gal_view_collection_get_count (view_collection);
	path = "/main-menu/view-menu/gal-view-menu/gal-view-list";

	/* Prevent spurious activations. */
	action = ACTION (GAL_CUSTOM_VIEW);
	g_signal_handlers_block_matched (
		action, G_SIGNAL_MATCH_FUNC, 0, 0,
		NULL, action_gal_view_cb, NULL);

	/* Default to "Custom View", unless we find our view ID. */
	radio_action = GTK_RADIO_ACTION (ACTION (GAL_CUSTOM_VIEW));
	gtk_radio_action_set_group (radio_action, NULL);
	radio_group = gtk_radio_action_get_group (radio_action);
	gtk_radio_action_set_current_value (radio_action, -1);

	/* Add a menu item for each view collection item. */
	for (ii = 0; ii < count; ii++) {
		GalViewCollectionItem *item;
		gchar *action_name;
		gchar *tooltip;

		item = gal_view_collection_get_view_item (view_collection, ii);

		action_name = g_strdup_printf (
			"gal-view-%s-%d", view_name, ii);
		tooltip = g_strdup_printf ("Select view: %s", item->title);

		radio_action = gtk_radio_action_new (
			action_name, item->title, tooltip, NULL, ii);

		action = GTK_ACTION (radio_action);
		gtk_radio_action_set_group (radio_action, radio_group);
		radio_group = gtk_radio_action_get_group (radio_action);

		g_object_set_data_full (
			G_OBJECT (radio_action), "view-id",
			g_strdup (item->id), (GDestroyNotify) g_free);

		if (view_id != NULL && strcmp (item->id, view_id) == 0)
			gtk_radio_action_set_current_value (radio_action, ii);

		gtk_action_group_add_action (action_group, action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id,
			path, action_name, action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		g_free (action_name);
		g_free (tooltip);
	}

	/* Doesn't matter which radio action we check. */
	visible = (gtk_radio_action_get_current_value (radio_action) < 0);

	action = ACTION (GAL_CUSTOM_VIEW);
	gtk_action_set_visible (action, visible);
	g_signal_handlers_unblock_matched (
		action, G_SIGNAL_MATCH_FUNC, 0, 0,
		NULL, action_gal_view_cb, NULL);

	action = ACTION (GAL_SAVE_CUSTOM_VIEW);
	gtk_action_set_visible (action, visible);
}

void
e_shell_window_update_search_menu (EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	ERuleContext *context;
	EFilterRule *rule;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	const gchar *source;
	const gchar *view_name;
	gboolean sensitive;
	guint merge_id;
	gint ii = 0;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	/* Check for a NULL shell view before proceeding.  This can
	 * happen if the initial view name from GConf is unrecognized.
	 * Without this we would crash at E_SHELL_VIEW_GET_CLASS(). */
	g_return_if_fail (shell_view != NULL);

	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	context = shell_view_class->search_context;

	source = E_FILTER_SOURCE_INCOMING;

	/* Update sensitivity of search_options action. */
	sensitive = (shell_view_class->search_options != NULL);
	gtk_action_set_sensitive (ACTION (SEARCH_OPTIONS), sensitive);

	/* Add custom rules to the Search menu. */

	action_group = ACTION_GROUP (CUSTOM_RULES);
	merge_id = shell_window->priv->custom_rule_merge_id;

	/* Unmerge the previous menu. */
	gtk_ui_manager_remove_ui (ui_manager, merge_id);
	e_action_group_remove_all_actions (action_group);

	rule = e_rule_context_next_rule (context, NULL, source);
	while (rule != NULL) {
		GtkAction *action;
		gchar *action_name;
		gchar *action_label;

		action_name = g_strdup_printf ("custom-rule-%d", ii++);
		if (ii < 10)
			action_label = g_strdup_printf (
				"_%d. %s", ii, rule->name);
		else
			action_label = g_strdup (rule->name);

		action = gtk_action_new (
			action_name, action_label,
			_("Execute these search parameters"), NULL);

		g_object_set_data_full (
			G_OBJECT (action),
			"rule", g_object_ref (rule),
			(GDestroyNotify) g_object_unref);

		g_signal_connect (
			action, "activate",
			G_CALLBACK (action_custom_rule_cb), shell_window);

		gtk_action_group_add_action (action_group, action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id,
			"/main-menu/search-menu/custom-rules",
			action_name, action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		g_free (action_name);
		g_free (action_label);

		rule = e_rule_context_next_rule (context, rule, source);
	}
}
