/*
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
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include <glib/gi18n.h>

#include <gio/gio.h>

#include <bonobo/bonobo-ui-component.h>

#include <libedataserverui/e-passwords.h>

#include <gconf/gconf-client.h>

#include "e-util/e-dialog-utils.h"
#include "e-util/e-error.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-print.h"
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "e-shell-window-commands.h"
#include "e-shell-window.h"
#include "evolution-shell-component-utils.h"

#include "e-shell-importer.h"

#define EVOLUTION_COPYRIGHT \
	"Copyright \xC2\xA9 1999 - 2009 Novell, Inc. and Others"

#define EVOLUTION_WEBSITE \
	"http://www.gnome.org/projects/evolution/"

/* Utility functions.  */

static void
launch_pilot_settings (void)
{
	GError* error = NULL;

	gchar * args = g_find_program_in_path ("gpilotd-control-applet");
	if (args == NULL) {
		e_notice (NULL, GTK_MESSAGE_ERROR,
			_("The GNOME Pilot tools do not appear to be installed on this system."));
		return;
	}

	g_spawn_command_line_async (args, &error);
	g_free (args);

	if (error != NULL) {
		e_notice (NULL, GTK_MESSAGE_ERROR,
			_("Error executing %s. (%s)"), args, error->message);
		g_error_free (error);
	}
}

/* Command callbacks.  */

static void
command_import (BonoboUIComponent *uih,
		EShellWindow *window,
		const gchar *path)
{
	e_shell_importer_start_import (window);
}

static void
command_page_setup (BonoboUIComponent *uih,
		    EShellWindow *window,
		    const gchar *path)
{
	e_print_run_page_setup_dialog (GTK_WINDOW (window));
}

static void
command_close (BonoboUIComponent *uih,
	       EShellWindow *window,
	       const gchar *path)
{
	if (e_shell_request_close_window (e_shell_window_peek_shell (window), window))
		gtk_widget_destroy (GTK_WIDGET (window));
}

static void
command_quit (BonoboUIComponent *uih,
	      EShellWindow *window,
	      const gchar *path)
{
	EShell *shell = e_shell_window_peek_shell (window);

	e_shell_quit(shell);
}

static void
command_submit_bug (BonoboUIComponent *uih,
		    EShellWindow *window,
		    const gchar *path)
{
	const gchar *command_line;
	GError *error = NULL;

        command_line = "bug-buddy --package=Evolution";

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
	"Ales Nyakhaychyk",
	"Alessandro Decina",
	"Alessio Frusciante",
	"Alex Graveley",
	"Alex Jiang",
	"Alex Jones",
	"Alex Kloss",
	"Alex Rostovtsev",
	"Alexander Didebulidze",
	"Alexander Shopov",
	"Alexander Winston",
	"Alexandre Folle de Menezes",
	"Alfred Peng",
	"Ali Abdin",
	"Ali Akcaagac",
	"Alireza Kheirkhahan",
	"Almer S. Tigelaar",
	"Alp Toker",
	"Amanpreet Singh Alam",
	"Ambuj Chitranshi",
	"Amish",
	"Amitakhya Phukan",
	"Anand V M",
	"Anders Carlsson",
	"Andras Timar",
	"Andrea Campi",
	"Andreas Henriksson",
	"Andreas Hyden",
	"Andreas J. Guelzow",
	"Andreas Köhler",
	"Andrew Ruthven",
	"Andre Klapper",
	"Andrew T. Veliath",
	"Andrew V. Samoilov",
	"Andrew Wu",
	"Ani Peter",
	"Ankit Patel",
	"Anna Marie Dirks",
	"Antonio Xu",
	"Arafat Medini",
	"Arangel Angov",
	"Archit Baweja",
	"Ariel Rios",
	"Arik Devens",
	"Armin Bauer",
	"Arjan Scherpenisse",
	"Arkadiusz Lipiec",
	"Artis Trops",
	"Artur Flinta",
	"Arturo Espinosa Aldama",
	"Arulanandan P",
	"Arun Prakash",
	"Arvind Sundararajan",
	"Ashish Shrivastava",
	"Åsmund Skjæveland",
	"Audrey Simons",
	"Baptiste Mille-Mathias",
	"Baris Cicek",
	"Bastien Nocera",
	"Behnam Esfahbod",
	"Benedikt Roth",
	"Ben Gamari",
	"Benjamin Berg",
	"Benjamin Kahn",
	"Benoît Dejean",
	"Bernard Leach",
	"Bertrand Guiheneuf",
	"Bharath Acharya",
	"Bharat Kumar",
	"Bharathi Gauthaman",
	"Big Iain Holmes",
	"Bill Zhu",
	"Björn Torkelsson",
	"Björn Lindqvist",
	"Bob Doan",
	"Bob Mauchin",
	"Boby Wang",
	"Bolian Yin",
	"Borislav Aleksandrov",
	"Boulton",
	"Brian Mury",
	"Brian Pepple",
	"Brigitte Le Grand",
	"Bruce Tao",
	"B S Srinidhi",
	"Calvin Liu",
	"Cantona Su",
	"Carlos Garcia Campos",
	"Carlos Garnacho Parro",
	"Carlos Perelló Marín",
	"Carl Sun",
	"Carsten Guenther",
	"Carsten Schaar",
	"Changwoo Ryu",
	"Chao-Hsiung Liao",
	"Charles Zhang",
	"Chema Celorio",
	"Chenthill Palanisamy",
	"Christian Persch",
	"Chris Halls",
	"Chris Heath",
	"Chris Lahey",
	"Christian Hammond",
	"Christian Kellner",
	"Christian Kintner",
	"Christian Kirbach",
	"Christian Krause",
	"Christian Kreibich",
	"Christian Meyer",
	"Christian Neumair",
	"Christian Persch",
	"Christian Rose",
	"Christophe Fergeau",
	"Christophe Merlet",
	"Christopher Blizzard",
	"Christopher James Lahey",
	"Christopher R. Gabriel",
	"Chris Phelps",
	"Chris Toshok",
	"Clara Tattoni",
	"Claude Paroz",
	"Claudio Saavedra",
	"Clifford R. Conover",
	"Clytie Siddall",
	"Cody Russell",
	"Colin Leroy",
	"Craig Jeffares",
	"Craig Small",
	"Cyprien Le Pannérer",
	"Dafydd Harries",
	"Damian Ivereigh",
	"Damien Carbery",
	"Damon Chaplin",
	"Daniel Gryniewicz",
	"Daniel Nylander",
	"Daniel van Eeden",
	"Daniel Veillard",
	"Daniel Yacob",
	"Danilo Šegan",
	"Danishka Navin",
	"Dan Korostelev",
	"Danny Baumann",
	"Dan Berger",
	"Dan Damian",
	"Dan Nguyen",
	"Dan Williams",
	"Dan Winship",
	"Darin Adler",
	"Dave Benson",
	"Dave Camp",
	"Dave Fallon",
	"Dave Malcolm",
	"Dave West",
	"David Farning",
	"David Kaelbling",
	"David Lodge",
	"David Malcolm",
	"David Moore",
	"David Mosberger",
	"David O'Callaghan",
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
	"Dirk-Jan C. Binnema",
	"Djihed Afifi",
	"Dmitrijs Ledkovs",
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
	"Emil Hessman",
	"Eneko Lacunza",
	"Enver Altin",
	"Erdal Ronahi",
	"Erdi Gergo",
	"Eric Busboom",
	"Eric Zhao",
	"Eskild Hustvedt"
	"Eskil Heyn Olsen",
	"Espen Stefansen",
	"Ettore Perazzoli",
	"Evandro Fernandes Giovanini",
	"Evan Yan",
	"Fatih Demir",
	"Fazlu & Hannah",
	"Federico Mena Quintero",
	"Fernando Herrera",
	"Ferretti",
	"F. Priyadharshini",
	"Francisco Javier Fernandez Serrador",
	"Frank Arnold",
	"Frank Belew",
	"Frederic Crozat",
	"Frederic Peters",
	"Frederic Riss",
	"Fredrik Wendt",
	"Funda Wang",
	"Gabor Kelemen",
	"Ganesh",
	"GÃ¶ran Uddeborg",
	"Gareth Owen",
	"Gary Coady",
	"Gary Ekker",
	"Gavin Scott",
	"Gediminas Paulauskas",
	"George Lebl",
	"Gerardo Marin",
	"Gergő Érdi",
	"Gert Kulyk",
	"Giancarlo Capella",
	"Gilbert Fang",
	"Gildas Guillemot",
	"Gil Forcada",
	"Gilles Dartiguelongue",
	"Gil Osher",
	"Gintautas Miliauskas",
	"Goran Rakić",
	"Grahame Bowland",
	"Greg Hudson",
	"Gregory Leblanc",
	"Gregory McLean",
	"Grzegorz Goawski",
	"Guilherme de S. Pastore",
	"Guntupalli Karunakar",
	"Gustavo GirÎldez",
	"Gustavo Maciel Dias Vieira",
	"Gustavo Noronha Silva",
	"Hamed Malek",
	"Hannah & Fazlu",
	"Hans Petter Jansson",
	"Hao Sheng",
	"Hari Prasad Nadig",
	"Harish Krishnaswamy",
	"Harry Lu",
	"Hasbullah Bin Pit",
	"Havoc Pennington",
	"Heath Harrelson",
	"Hein-Pieter van Braam",
	"Héctor García Álvarez",
	"Helgi Þormar Þorbjörnsson",
	"Hendrik Brandt",
	"Hendrik Richter",
	"Herbert V. Riedel",
	"Hessam M. Armandehi",
	"Hiroyuki Ikezoe",
	"Iain Buchanan",
	"Iain Holmes",
	"Ian Campbell",
	"Iassen Pramatarov",
	"Iestyn Pryce",
	"I.Felix",
	"Ignacio Casal Quinteiro",
	"Igor Nestorović",
	"Ihar Hrachyshka",
	"Ilkka Tuohela",
	"Imam Musthaqim",
	"Iñaki Larrañaga",
	"Inaki Larranaga Murgoitio",
	"Indu",
	"Irene Huang",
	"Ismael Olea",
	"Israel Escalante",
	"Ivan Stojmirov",
	"Ivar Smolin",
	"Ivelina Karcheva",
	"Iván Frade",
	"J.H.M. Dassen (Ray)",
	"Jaap A. Haitsma",
	"Jack Jia",
	"Jacob Berkman",
	"Jacob Brown",
	"Jacobo Tarrio Barreiro",
	"Jacob Ulysses Berkman",
	"Jaka Mocnik",
	"Jakub Friedl",
	"Jakub Steiner",
	"James Bowes",
	"James Doc Livingston",
	"James Henstridge",
	"James Westby",
	"James Willcox",
	"Jamil Ahmed",
	"Jan Arne Petersen",
	"Jan Tichavsky",
	"Jan Van Buggenhout",
	"JÃ�rg Billeter",
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
	"Jeremy Messenger",
	"Jeremy Wise",
	"Jerome Lacoste",
	"Jerry Yu",
	"Jesse Pavel",
	"Jesus Bravo Alvarez",
	"Jesse Pavel",
	"Ji Lee",
	"Joan Sanfeliu",
	"João Vale",
	"Joaquim Fellmann",
	"Jody Goldberg",
	"Joe Man",
	"Joe Marcus Clarke",
	"Joe Shaw",
	"Johan Dahlin",
	"Johan Euphrosine",
	"John Gotts",
	"Johnny Jacob",
	"Jon Ander Hernandez",
	"Jonas Borgstr�m",
	"Jonathan Blandford",
	"Jonathan Dieter",
	"Jonathan Ernst",
	"Jonh Wendell",
	"Jon K Hellan",
	"Jon Oberheide",
	"Jon Trowbridge",
	"Jonas Borgstr",
	"Jonathan Blandford",
	"Jonathan Dieter",
	"Joop Stakenborg",
	"Jordi Mallach",
	"Jordi Mas",
	"Jorge Gonzalez",
	"Jörgen Scheibengruber",
	"Jos Dehaes",
	"Josep Puigdemont Casamajó",
	"Josselin Mouette",
	"Jovan Naumovski",
	"JP Rosevear",
	"Juan Manuel GarcÃ­a Molina",
	"Juan Pizarro",
	"Jukka Zitting",
	"Jules Colding",
	"Julian Missig",
	"Julien Puydt",
	"Julio M. Merino Vidal",
	"Juraj Kubelka",
	"Jürg Billeter",
	"Justina Klingaitė",
	"Kai Lahmann",
	"Kang Jeong-Hee",
	"Karl Eichwalder",
	"Karl Relton",
	"Karsten Bräckelmann",
	"Kaushal Kumar",
	"Keith Packard",
	"Keld Simonsen",
	"Kenneth Christiansen",
	"Kenneth Nielsen",
	"Kenneth Rohde Christiansen",
	"Kenny Graunke",
	"Keshav Upadhyaya",
	"Kevin Breit",
	"Kevin Piche",
	"Kevin Vandersloot",
	"Khasim Shaheed",
	"Kidd Wang",
	"Kjartan Maraas",
	"Krishnan R",
	"Kostas Papadimas",
	"Krishna Babu K",
	"Krisztian Pifko",
	"Kyle Ambroff",
	"Larry Ewing",
	"Laszlo Dvornik",
	"Laszlo (Laca) Peter",
	"Laurent Dhima",
	"Lauris Kaplinski",
	"Leonardo Ferreira Fontenelle",
	"Leonid Kanter",
	"Leon Zhang",
	"Li Yuan",
	"Loïc Minier",
	"Lorenzo Gil Sanchez",
	"Luca Ferretti",
	"Lucas Rocha",
	"Lucian Langa",
	"Lucky Wankhede",
	"Luis Villa",
	"Lukas Novotny",
	"Lutz M",
	"Maciej Piechotka",
	"Maciej Stachowiak",
	"Makuchaku",
	"Malcolm Tredinnick",
	"Manuel A. Fernández Montecelo",
	"Manuel Borchers",
	"Marcel Telka",
	"Marco Ciampa",
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
	"Martin Norbäck",
	"Martin Willemoes Hansen",
	"Martyn Russell",
	"Masahiro Sakai",
	"Matej Urbančič",
	"Mathieu Lacage",
	"Matias Mutchinick",
	"Matic Žgur",
	"Matt Bissiri",
	"Matt Brown",
	"Matthew Barnes",
	"Matthew Daniel",
	"Matthew Hall",
	"Matthew Loper",
	"Matthew Wilson",
	"Matthias Braun",
	"Matthias Clasen",
	"Matthias Warkus",
	"Matt Loper",
	"Matt Martin",
	"Matt McCutchen",
	"Matt Wilson",
	"Max Horn",
	"Maxim Dziumanenko",
	"Maxx Cao",
	"Mayank Jain",
	"Meelad Zakaria",
	"Meilof Veeningen",
	"Mendel Mobach",
	"Mengjie Yu",
	"Metin Amiroff",
	"Michael Granger",
	"Michael M. Morrison",
	"Michael MacDonald",
	"Michael Meeks",
	"Michael Monreal",
	"Michael Terry",
	"Michael Zucchi",
	"Michal Bukovjan",
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
	"Miloslav Trmač",
	"MIMOS Open Source Development Group",
	"Mohammad Damt",
	"Moritz Mertinkat",
	"Morten Welinder",
	"Mubeen Jukaku",
	"Mugurel Tudor",
	"Murray Cumming",
	"M Victor Aloysius J",
	"Naba Kumar",
	"Nagappan Alagappan",
	"Nancy Cai",
	"Naresh N",
	"Nat Friedman",
	"Nathan Owens",
	"Nguyễn Thái Ngọc Duy",
	"Nicel KM",
	"Nicholas J Kreucher",
	"Nicholas Miell",
	"Nickolay V. Shmyrev",
	"Nick Sukharev",
	"Nike Gerdts",
	"Nikos Charonitakis",
	"Noel",
	"Nuno Ferreira",
	"Nyall Dawson",
	"Og Maciel",
	"Ole Laursen",
	"Ondrej Jirman",
	"Oswald Rodrigues",
	"Owen Taylor",
	"Øystein Gisnås",
	"Pablo Gonzalo del Campo",
	"Pablo Saratxaga",
	"Pamplona Hackers",
	"Pauli Virtanen",
	"Paolo Borelli",
	"Paolo Molaro",
	"Parag Goel",
	"Parthasarathi Susarla",
	"Pascal Terjan",
	"Patrick Ohly",
	"Paul Bolle",
	"Paul Duffy",
	"Paul Iadonisi",
	"Paul Lindner",
	"Paul Smith",
	"Paulo Gomes Vanzuita",
	"Pavel Cholakov",
	"Pavel Cisler",
	"Pavel Roskin",
	"Pavithran",
	"Pawan Chitrakar",
	"Pedro Villavicencio",
	"Pema Geyleg",
	"Peteris Krisjanis",
	"Peter Bach",
	"Peter Pouliot",
	"Peter Teichman",
	"Peter Williams",
	"Petr Kovar",
	"Petta Pietikainen",
	"Phil Goembel",
	"Philipp Kerling",
	"Philip Van Hoof",
	"Philip Withnall",
	"Philip Zhao",
	"Poornima Nayak",
	"Pramod",
	"Prasad Kandepu",
	"Pratik V. Parikh",
	"Praveen Arimbrathodiyil",
	"Praveen Kumar",
	"Priit Laes",
	"Priyanshu Raj",
	"P S Chakravarthi",
	"Radek Doulik",
	"Raghavendran R",
	"Rahul Bhalerao",
	"Raivis Dejus",
	"Raja R Harinath",
	"Rajeev Ramanathan",
	"Rajesh Ranjan",
	"Rakesh k.g",
	"Ramiro Estrugo",
	"Ranjan Somani",
	"Raphael Higino",
	"Ray Strode",
	"Reinout van Schouwen",
	"Rhys Jones",
	"Ricardo Markiewicz",
	"Richard Boulton",
	"Richard Hult",
	"Richard Li",
	"Rob Bradford",
	"Robert-André Mauchin",
	"Robert Brady",
	"Robert Sedak",
	"Robin Slomkowski",
	"Rodney Dawes",
	"Rodrigo Moya",
	"Roger Zauner",
	"Rohini S",
	"Roland Illig",
	"Ronald Kuetemeier",
	"Roozbeh Pournader",
	"Roshan Kumar Singh",
	"Ross Burton",
	"Rostislav Raykov",
	"Rouslan Solomakhin",
	"Roy-Magne Mo",
	"Runa Bhattacharjee",
	"Russell Steinthal",
	"Russian team",
	"Rusty Conover",
	"Ryan P. Skadberg",
	"Sam Creasey",
	"Sami Pesonen",
	"Samúel Jón Gunnarsson",
	"Sam Yang",
	"Sankar P",
	"Sanlig Badral",
	"Sanshao Jiang",
	"S.Antony Vincent Pandian",
	"Sarfraaz Ahmed",
	"Satoru SATOH",
	"Sayamindu Dasgupta",
	"S. Caglar Onur",
	"Sean Atkinson",
	"Seán de Búrca",
	"Sean Gao",
	"Sebastian Rittau",
	"Sebastian Wilhelmi",
	"Sebastien Bacher",
	"Sergey Panov",
	"Sergio Villar Senín",
	"Seth Alves",
	"Seth Nickell",
	"Shakti Sen",
	"Shankar Prasad",
	"Shi Pu",
	"Shilpa C",
	"Shree Krishnan",
	"Shreyas Srinivasan",
	"Shuai Liu",
	"Sigurd Gartmann",
	"Simon Zheng",
	"Simos Xenitellis",
	"Sitic Vulnerability Advisory",
	"Sivaiah Nallagatla",
	"Slobodan D. Sredojevic",
	"S N Tejasvi",
	"Spiros Papadimitriou",
	"Srinivasa Ragavan",
	"Stanislav Brabec",
	"Stanislav Slusny",
	"Stanislav Visnovsky",
	"Stéphane Raimbault",
	"Stephen Cook",
	"Steve Murphy",
	"Steven Zhang",
	"Stuart Parmenter",
	"Subhransu Behera",
	"Subodh Soni",
	"Suman Manjunath",
	"Sunil Mohan Adapa",
	"Supranee Thirawatthanasuk",
	"Suresh Chandrasekharan",
	"Sushma Rai",
	"Sven Herzberg",
	"Sweta Kothari",
	"Szabolcs Ban",
	"Takao Fujiwara",
	"Takayuki Kusano",
	"Takeshi Aihana",
	"Takuo Kitame",
	"Tambet Ingo",
	"Taylor Hayward",
	"Ted Percival",
	"Telsa Gwynne",
	"Terance Sola",
	"Theppitak Karoonboonyanan",
	"Thierry Moisan",
	"Thierry Randrianiriana",
	"Thomas Cataldo",
	"Thomas Klausner",
	"Thomas Mirlacher",
	"Thouis R. Jones",
	"Tiago Antao",
	"Timo Jyrinki",
	"Timur Bakeyev",
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
	"Tomasz Kłoczko",
	"Tomislav Vujec",
	"Tommi Komulainen",
	"Tommi Vainikainen",
	"Tony Tsui",
	"Tor Lillqvist",
	"Trent Lloyd",
	"Tristan Tarrant",
	"Tuomas J. Lukka",
	"Tuomas Kuosmanen",
	"Udomsak Chundang",
	"Ulrich Neumann",
	"Umeshtej",
	"Umesh Tiwari",
	"Ushveen Kaur",
	"Vadim Strizhevsky",
	"Valek Filippov",
	"Vandana Shenoy .B",
	"Vardhman Jain",
	"Vasiliy Faronov",
	"Veerapuram Varadhan",
	"Vincent Carriere",
	"Vincent Noel",
	"Vincent Renardias",
	"Vincent Untz",
	"Vincent van Adrighem",
	"Viren.L",
	"Vivek Jain",
	"Vladimer Sichinava",
	"Vladimir Petkov",
	"Vladimir Vukicevic",
	"Vladimir Melo",
	"V Ravi Kumar Raju",
	"Wadim Dziedzic",
	"Wang Jian",
	"Wang Li",
	"Wang Xin",
	"Washington Lins",
	"Wayne Davis",
	"William Jon McCann",
	"Woodman Tuen",
	"Wouter Bolsterlee",
	"Xan Lopez",
	"Xavier Conde Rueda",
	"Xiurong Simon Zheng",
	"Yair Hershkovitz",
	"Yanko Kaneti",
	"Yannig Marchegay",
	"Yavor Doganov",
	"Yi Jin",
	"Yong Sun",
	"Yuedong Du",
	"Yukihiro Nakai",
	"Yu Mengjie",
	"Yuri Pankov",
	"Yuri Syrota",
	"Yuriy Penkin",
	"Zach Frey",
	"Zan Lynx",
	"Zbigniew Chyla",
	"Zhe Su",
	"Zipeco",
	"Žygimantas Beručka",
 	"Sandeep Shedmake",
	"Adam Petaccia",
	"Daniel NylanderAstur",
	"Sergio Villar Senin",
	"Srinivasa Ragavankrishnababu k",
	"Daniel S. Koda",
	"fujianwzh",
	"Og B. Maciel",
	"Yaron Shahrabani",
	"Ant\xc3\xb3n M\xc3\xa9ixome",
	"khaledh",
	"Ihar Hrachyshka",
	"David Planela",
	"Ask H. Larsen",
	"Mark Krapivner",
	"Marios Zindilis",
	"Michel D\xc3\xa4nzer",
	"Goran Rakic",
	"Maxim V. Dziumanenko",
	"Yavor Doganov",
	"Norman",
	"Igor Nestorovi\xc4\x87",
	"Daniel Macks",
	"Jorge Gonzalez Gonzalez",
	"Amanpreet Singh Alam",
	"Chen Congwu",
	"Marc-Andr\xc3\xa9 Lureau",
	"Ivar Smolin",
	"Ani",
	"Carles Ferrando",
	"dooteo",
	"Ritesh Khadgaray",
	"Djavan Fagundes",
	"Gil Forcada",
	"Ken VanDine",
	"Holger Macht",
	"Sergio Villar Sen\xc3\xadn",
	"Shankar Prasad",
	"Milo\xc5\xa1 Popovi\xc4\x87",
	"ifelix",
	"Sukhbir Singh",
	"Mario Bl\xc3\xa4ttermann",
	"Jorge Gonzalez",
	"Sweta Kothari",
	"Iestyn Pryce",
	"Marco Barisione",
	"paul",
	"Marcel Stimberg",
	"Jamil Ahmed",
	"Sven Anders",
	"Piotr Dr\xc4\x85g",
	"Robin Stocker",
	"Se\xc3\xa1n de B\xc3\xbarca",
	"Inaki Larranaga Murgoitio",
	"Kenneth Nielsen",
	"Philip Withnall",
	"Srinivasa RagavanAni",
	"Thomas Hindoe Paaboel Andersen",
	"Lucian Langa",
	"Adi Roiban",
	"Matej Urban\xc4\x8di\xc4\x8d",
	"Yan Li",
	"Alexander Klepikov",
	"Damien Lespiau",
	"David Ronis",
	"Denis Pauk",
	"Khaled Hosny",
	"krishnababu k",
	"drtvasudevan",
	"Romuald Brunet",
	"Amitakhya Phukan",
	"David Liang",
	"Tiziano M\xc3\xbcller",
	"Inaki Larranaga",
	"H.Habighorst",
	"Petr Kovar",
	"Leonardo Ferreira Fontenelle",
	"A S Alam",
	"Fridrich Strba",
	"Gil Forcada Codinachs",
	"Jorge Gonz\xc3\xa1lez",
	"Baris Cicek",
	"Manoj Kumar Giri",
	"G\xf6tz Waschk",
	"Yaron Sharabani",
	"Bruce Cowan",
	"Mattias P\xc3\xb5ldaru",
	NULL
};

static const gchar *documentors[] = {
	"Aaron Weber",
	"Binika Preet",
	"Dan Winship",
	"David Trowbridge",
	"Jessica Prabhakar",
	"JP Rosevear",
	"Radhika Nair",
	"Akhil Laddha",
	NULL
};

static void
command_about (BonoboUIComponent *uih,
               EShellWindow *window,
               const gchar *path)
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

static void
command_open_faq (BonoboUIComponent *uih,
		  EShellWindow *window,
		  const gchar *path)
{
	const gchar *uri;

	uri = "http://www.go-evolution.org/FAQ";
	e_show_uri (GTK_WINDOW (window), uri);
}

static void
command_quick_reference (BonoboUIComponent *uih,
			 EShellWindow *window,
			 const gchar *path)
{
	gchar *quickref;
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
			GFile *file = g_file_new_for_path (quickref);

			if (file) {
				GError *error = NULL;
				gchar *uri = g_file_get_uri (file);

				g_app_info_launch_default_for_uri (uri, NULL, &error);

				if (error) {
					g_warning ("%s", error->message);
					g_error_free (error);
				}

				g_object_unref (file);
				g_free (uri);
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
		      const gchar *path)
{
	EShell *shell;

	shell = e_shell_window_peek_shell (window);
	e_shell_set_line_status (shell, GNOME_Evolution_USER_OFFLINE);
}

static void
command_work_online (BonoboUIComponent *uih,
		     EShellWindow *window,
		     const gchar *path)
{
	EShell *shell;

	shell = e_shell_window_peek_shell (window);
	e_shell_set_line_status (shell, GNOME_Evolution_USER_ONLINE);
}

static void
command_open_new_window (BonoboUIComponent *uih,
			 EShellWindow *window,
			 const gchar *path)
{
	e_shell_create_window (e_shell_window_peek_shell (window),
			       e_shell_window_peek_current_component_id (window),
			       window);
}

/* Actions menu.  */

static void
command_send_receive (BonoboUIComponent *uih,
		      EShellWindow *window,
		      const gchar *path)
{
	e_shell_send_receive (e_shell_window_peek_shell (window));
}

static void
command_forget_passwords (BonoboUIComponent *ui_component,
			  gpointer data,
			  const gchar *path)
{
	if (e_error_run (NULL, "shell:forget-passwords", NULL) == GTK_RESPONSE_OK)
		e_passwords_forget_passwords();
}

/* Tools menu.  */

static void
command_settings (BonoboUIComponent *uih,
		  EShellWindow *window,
		  const gchar *path)
{
	e_shell_window_show_settings (window);
}

static void
command_pilot_settings (BonoboUIComponent *uih,
			EShellWindow *window,
			const gchar *path)
{
	launch_pilot_settings ();
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
	BONOBO_UI_VERB ("HelpOpenFAQ", (BonoboUIVerbFn) command_open_faq),
	BONOBO_UI_VERB ("HelpSubmitBug", (BonoboUIVerbFn) command_submit_bug),
	BONOBO_UI_VERB ("HelpAbout", (BonoboUIVerbFn) command_about),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/Toolbar/SendReceive", "mail-send-receive", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/menu/File/OpenNewWindow", "window-new", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/SendReceive", "mail-send-receive", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/FileImporter", "stock_mail-import", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/Print/FilePageSetup", "stock_print-setup", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/ToggleOffline", "stock_disconnect", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/FileClose", "window-close", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/File/FileExit", "application-exit", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/Edit/Settings", "preferences-desktop", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/menu/Help/HelpOpenFAQ", "help-faq", GTK_ICON_SIZE_MENU),

	E_PIXMAP_END
};

static EPixmap offline_pixmaps [] = {
	E_PIXMAP ("/menu/File/ToggleOffline", "stock_disconnect", GTK_ICON_SIZE_MENU),
	E_PIXMAP_END
};

static EPixmap online_pixmaps [] = {
	E_PIXMAP ("/menu/File/ToggleOffline", "stock_connect", GTK_ICON_SIZE_MENU),
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
	case E_SHELL_LINE_STATUS_FORCED_OFFLINE:
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
					    const gchar                  *path,
					    Bonobo_UIComponent_EventType type,
					    const gchar                  *state,
					    EShellWindow                *shell_window)
{
	ESidebar *sidebar;

	sidebar = e_shell_window_peek_sidebar (shell_window);
	e_sidebar_set_mode (sidebar, E_SIDEBAR_MODE_BOTH);
}

static void
view_buttons_icon_item_toggled_handler (BonoboUIComponent           *ui_component,
					const gchar                  *path,
					Bonobo_UIComponent_EventType type,
					const gchar                  *state,
					EShellWindow                *shell_window)
{
	ESidebar *sidebar;

	sidebar = e_shell_window_peek_sidebar (shell_window);
	e_sidebar_set_mode (sidebar, E_SIDEBAR_MODE_ICON);
}

static void
view_buttons_text_item_toggled_handler (BonoboUIComponent           *ui_component,
					const gchar                  *path,
					Bonobo_UIComponent_EventType type,
					const gchar                  *state,
					EShellWindow                *shell_window)
{
	ESidebar *sidebar;

	sidebar = e_shell_window_peek_sidebar (shell_window);
	e_sidebar_set_mode (sidebar, E_SIDEBAR_MODE_TEXT);
}

static void
view_buttons_toolbar_item_toggled_handler (BonoboUIComponent           *ui_component,
					   const gchar                  *path,
					   Bonobo_UIComponent_EventType type,
					   const gchar                  *state,
					   EShellWindow                *shell_window)
{
	ESidebar *sidebar;

	sidebar = e_shell_window_peek_sidebar (shell_window);
	e_sidebar_set_mode (sidebar, E_SIDEBAR_MODE_TOOLBAR);
}

static void
view_buttons_hide_item_toggled_handler (BonoboUIComponent           *ui_component,
					const gchar                  *path,
					Bonobo_UIComponent_EventType type,
					const gchar                  *state,
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
				   const gchar                  *path,
				   Bonobo_UIComponent_EventType type,
				   const gchar                  *state,
				   EShellWindow                *shell_window)
{
	gboolean is_visible;

	is_visible = state[0] == '1';

	bonobo_ui_component_set_prop (ui_component, "/Toolbar",
				      "hidden", is_visible ? "0" : "1", NULL);
}

static void
view_statusbar_item_toggled_handler (BonoboUIComponent           *ui_component,
				     const gchar                  *path,
				     Bonobo_UIComponent_EventType type,
				     const gchar                  *state,
				     EShellWindow                *shell_window)
{
	GtkWidget *status_bar = e_shell_window_peek_statusbar (shell_window);
	gboolean is_visible;
	GConfClient *gconf_client;

	is_visible = state[0] == '1';
	if (is_visible)
		gtk_widget_show (status_bar);
	else
		gtk_widget_hide (status_bar);
	gconf_client = gconf_client_get_default ();
	gconf_client_set_bool (gconf_client,"/apps/evolution/shell/view_defaults/statusbar_visible", is_visible, NULL);
	g_object_unref (gconf_client);
}

static void
view_sidebar_item_toggled_handler (BonoboUIComponent           *ui_component,
				     const gchar                  *path,
				     Bonobo_UIComponent_EventType type,
				     const gchar                  *state,
				     EShellWindow                *shell_window)
{
	GtkWidget *side_bar = GTK_WIDGET(e_shell_window_peek_sidebar (shell_window));
	gboolean is_visible;
	GConfClient *gconf_client;

	is_visible = state[0] == '1';
	if (is_visible)
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
