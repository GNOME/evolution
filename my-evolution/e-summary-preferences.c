/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-preferences.c
 *
 * Copyright (C) 2001 Ximian, Inc.
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
 * Author: Iain Holmes
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-config.h>

#include <libgnomeui/gnome-propertybox.h>
#include <libgnomeui/gnome-stock.h>

#include <glade/glade.h>
#include <stdio.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>

#include <bonobo-conf/bonobo-config-database.h>

#include <shell/evolution-storage-set-view-listener.h>

#include "e-summary.h"
#include "e-summary-preferences.h"
#include "e-summary-table.h"
#include "e-summary-shown.h"

#include "evolution-config-control.h"

#define FACTORY_ID "OAFIID:GNOME_Evolution_Summary_ConfigControlFactory"

static ESummaryPrefs *global_preferences = NULL;
static GNOME_Evolution_Shell global_shell = NULL;

static char *default_folders[2] = {
	"/local/Inbox", "/local/Outbox"
};

static void
make_initial_mail_list (ESummaryPrefs *prefs)
{
	char *evolution_dir;
	GList *folders = NULL;
	int i;
	
	evolution_dir = gnome_util_prepend_user_home ("evolution");
	for (i = 0; i < 2; i++) {
		ESummaryPrefsFolder *folder;

		folder = g_new (ESummaryPrefsFolder, 1);
		folder->evolution_uri = g_strconcat ("evolution:", default_folders[i], NULL);
		folder->physical_uri = g_strconcat ("file://", evolution_dir, default_folders[i], NULL);
		
		folders = g_list_append (folders, folder);
	}

	g_free (evolution_dir);
	prefs->display_folders = folders;
}

static void
make_initial_rdf_list (ESummaryPrefs *prefs)
{
	GList *rdfs;

	rdfs = g_list_prepend (NULL, g_strdup ("http://linuxtoday.com/backend/my-netscape.rdf"));
	rdfs = g_list_append (rdfs, g_strdup ("http://www.salon.com/feed/RDF/salon_use.rdf"));
	
	prefs->rdf_urls = rdfs;
}

static void
make_initial_weather_list (ESummaryPrefs *prefs)
{
	/* translators: Put a list of codes for locations you want to see in
	   My Evolution by default here. You can find the list of all
	   stations and their codes in Evolution sources.
	   (evolution/my-evolution/Locations)
	   Codes are seperated with : eg. "KBOS:EGAA"*/
	char *default_stations = _("KBOS"), **stations_v, **p;
	GList *stations = NULL;

	stations_v = g_strsplit (default_stations, ":", 0);
	g_assert (stations_v != NULL);
	for (p = stations_v; *p != NULL; p++) {
		stations = g_list_prepend (stations, *p);
	}
	g_free (stations_v);

	prefs->stations = g_list_reverse (stations);
}

/* Load the prefs off disk */

static char *
vector_from_str_list (GList *strlist)
{
	char *vector;
	GString *str;

	if (strlist == NULL) {
		return g_strdup ("");
	}

	str = g_string_new ("");
	for (; strlist; strlist = strlist->next) {
		g_string_append (str, strlist->data);

		/* No space at end */
		if (strlist->next) {
			g_string_append (str, " !<-->! ");
		}
	}

	vector = str->str;
	g_string_free (str, FALSE);

	return vector;
}

static GList *
str_list_from_vector (const char *vector)
{
	GList *strlist = NULL;
	char **tokens, **t;

	t = tokens = g_strsplit (vector, " !<-->! ", 8196);

	if (tokens == NULL) {
		return NULL;
	}

	for (; *tokens; tokens++) {
		strlist = g_list_prepend (strlist, g_strdup (*tokens));
	}

	g_strfreev (t);

	strlist = g_list_reverse (strlist);
	return strlist;
}

static GList *
folder_list_from_vector (const char *vector)
{
	GList *flist = NULL;
	char **tokens, **t;

	t = tokens = g_strsplit (vector, " !<-->! ", 8196);
	if (tokens == NULL) {
		return NULL;
	}

	for (tokens = t; *tokens; tokens += 2) {
		ESummaryPrefsFolder *folder;
		const char *evolution_uri;
		const char *physical_uri;

		evolution_uri = *tokens;
		if (evolution_uri == NULL || strncmp (evolution_uri, "evolution:", 10) != 0)
			break;

		physical_uri = *(tokens + 1);
		if (physical_uri == NULL)
			break;

		folder = g_new (ESummaryPrefsFolder, 1);
		folder->evolution_uri = g_strdup (evolution_uri);
		folder->physical_uri = g_strdup (physical_uri);

		flist = g_list_prepend (flist, folder);
	}

	g_strfreev (t);

	flist = g_list_reverse (flist);
	return flist;
}

static char *
vector_from_folder_list (GList *flist)
{
	char *vector;
	GString *string;

	if (flist == NULL) {
		return g_strdup ("");
	}

	string = g_string_new ("");
	for (; flist; flist = flist->next) {
		ESummaryPrefsFolder *folder;

		folder = flist->data;
		string = g_string_append (string, folder->evolution_uri);
		string = g_string_append (string, " !<-->! ");
		string = g_string_append (string, folder->physical_uri);

		if (flist->next != NULL) {
			string = g_string_append (string, " !<-->! ");
		}
	}

	vector = string->str;
	g_string_free (string, FALSE);

	return vector;
}

gboolean
e_summary_preferences_restore (ESummaryPrefs *prefs)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	char *vector;

	g_return_val_if_fail (prefs != NULL, FALSE);

	CORBA_exception_init (&ev);
	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		g_warning ("Error getting Wombat. Using defaults");
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);
	vector = bonobo_config_get_string (db, "My-Evolution/Mail/display_folders-1.2", &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Error getting Mail/display_folders. Using defaults");
		CORBA_exception_free (&ev);
		make_initial_mail_list (prefs);
	} else {
		prefs->display_folders = folder_list_from_vector (vector);
		g_free (vector);
	}

	prefs->show_full_path = bonobo_config_get_boolean (db, "My-Evolution/Mail/show_full_path", &ev);
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		g_warning ("Error getting Mail/show_full_path. Using defaults");
		bonobo_object_release_unref (db, NULL);
		CORBA_exception_free (&ev);
		return FALSE;
	}


	vector = bonobo_config_get_string (db, "My-Evolution/RDF/rdf_urls", &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Error getting RDF/rdf_urls");
		CORBA_exception_free (&ev);
		bonobo_object_release_unref (db, NULL);
		return FALSE;
	}
	prefs->rdf_urls = str_list_from_vector (vector);
  	g_free (vector);

	prefs->rdf_refresh_time = bonobo_config_get_long_with_default (db, "My-Evolution/RDF/rdf_refresh_time", 600, NULL);

	prefs->limit = bonobo_config_get_long_with_default (db, "My-Evolution/RDF/limit", 10, NULL);

	vector = bonobo_config_get_string (db, "My-Evolution/Weather/stations", &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Error getting Weather/stations");
		CORBA_exception_free (&ev);
		bonobo_object_release_unref (db, NULL);
		return FALSE;
	}
	prefs->stations = str_list_from_vector (vector);
  	g_free (vector);

	prefs->units = bonobo_config_get_long (db, "My-Evolution/Weather/units", &ev);
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		g_warning ("Error getting Weather/units. Using defaults");
		bonobo_object_release_unref (db, NULL);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	prefs->weather_refresh_time = bonobo_config_get_long (db, "My-Evolution/Weather/weather_refresh_time", &ev);
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		g_warning ("Error getting Weather/weather_refresh_time. Using defaults");
		bonobo_object_release_unref (db, NULL);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	
	prefs->days = bonobo_config_get_long (db, "My-Evolution/Schedule/days", &ev);
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		g_warning ("Error getting Schedule/days. Using defaults");
		bonobo_object_release_unref (db, NULL);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	prefs->show_tasks = bonobo_config_get_long (db, "My-Evolution/Schedule/show_tasks", &ev);
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		g_warning ("Error getting Schedule/show_tasks. Using defaults");
		bonobo_object_release_unref (db, NULL);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	bonobo_object_release_unref (db, NULL);
	return TRUE;
}

/* Write prefs to disk */
void
e_summary_preferences_save (ESummaryPrefs *prefs)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	char *vector;

	g_return_if_fail (prefs != NULL);

	CORBA_exception_init (&ev);
	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		g_warning ("Cannot save preferences");
		CORBA_exception_free (&ev);
		return;
	}
	CORBA_exception_free (&ev);

	vector = vector_from_folder_list (prefs->display_folders);
	bonobo_config_set_string (db, "My-Evolution/Mail/display_folders-1.2", vector, NULL);
  	g_free (vector); 

	bonobo_config_set_boolean (db, "My-Evolution/Mail/show_full_path", prefs->show_full_path, NULL);

	vector = vector_from_str_list (prefs->rdf_urls);
	bonobo_config_set_string (db, "My-Evolution/RDF/rdf_urls", vector, NULL);
  	g_free (vector); 

	bonobo_config_set_long (db, "My-Evolution/RDF/rdf_refresh_time", prefs->rdf_refresh_time, NULL);
	bonobo_config_set_long (db, "My-Evolution/RDF/limit", prefs->limit, NULL);

	vector = vector_from_str_list (prefs->stations);
	bonobo_config_set_string (db, "My-Evolution/Weather/stations", vector, NULL);
  	g_free (vector); 

	bonobo_config_set_long (db, "My-Evolution/Weather/units", prefs->units, NULL);
	bonobo_config_set_long (db, "My-Evolution/Weather/weather_refresh_time", prefs->weather_refresh_time, NULL);

	bonobo_config_set_long (db, "My-Evolution/Schedule/days", prefs->days, NULL);
	bonobo_config_set_long (db, "My-Evolution/Schedule/show_tasks", prefs->show_tasks, NULL);

	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (db, &ev);
	CORBA_exception_free (&ev);

	bonobo_object_release_unref (db, NULL);
}

static void
free_str_list (GList *list)
{
	for (; list; list = list->next) {
		g_free (list->data);
	}
}

static void
free_folder_list (GList *list)
{
	for (; list; list = list->next) {
		ESummaryPrefsFolder *f = list->data;
		
		g_free (f->evolution_uri);
		g_free (f->physical_uri);
		g_free (f);
	}
}

void
e_summary_preferences_free (ESummaryPrefs *prefs)
{
	if (prefs->display_folders) {
		free_folder_list (prefs->display_folders);
		g_list_free (prefs->display_folders);
	}

	if (prefs->rdf_urls) {
		free_str_list (prefs->rdf_urls);
		g_list_free (prefs->rdf_urls);
	}

	if (prefs->stations) {
		free_str_list (prefs->stations);
		g_list_free (prefs->stations);
	}

	g_free (prefs);
}

static GList *
copy_str_list (GList *list)
{
	GList *list_copy = NULL;

	for (; list; list = list->next) {
		list_copy = g_list_prepend (list_copy, g_strdup (list->data));
	}

	list_copy = g_list_reverse (list_copy);
	return list_copy;
}

static GList *
copy_folder_list (GList *list)
{
	GList *list_copy = NULL;

	for (; list; list = list->next) {
		ESummaryPrefsFolder *f1, *f2;

		f1 = list->data;
		f2 = g_new (ESummaryPrefsFolder, 1);
		f2->evolution_uri = g_strdup (f1->evolution_uri);
		f2->physical_uri = g_strdup (f1->physical_uri);

		list_copy = g_list_prepend (list_copy, f2);
	}

	list_copy = g_list_reverse (list_copy);
	return list_copy;
}

ESummaryPrefs *
e_summary_preferences_copy (ESummaryPrefs *prefs)
{
	ESummaryPrefs *prefs_copy;

	prefs_copy = g_new (ESummaryPrefs, 1);

	prefs_copy->display_folders = copy_folder_list (prefs->display_folders);
	prefs_copy->show_full_path = prefs->show_full_path;

	prefs_copy->rdf_urls = copy_str_list (prefs->rdf_urls);
	prefs_copy->rdf_refresh_time = prefs->rdf_refresh_time;
	prefs_copy->limit = prefs->limit;

	prefs_copy->stations = copy_str_list (prefs->stations);
	prefs_copy->units = prefs->units;
	prefs_copy->weather_refresh_time = prefs->weather_refresh_time;

	prefs_copy->days = prefs->days;
	prefs_copy->show_tasks = prefs->show_tasks;

	return prefs_copy;
}

ESummaryPrefs *
e_summary_preferences_init (void)
{
	ESummaryPrefs *prefs;

	if (global_preferences != NULL) {
		return global_preferences;
	}
	
	prefs = g_new0 (ESummaryPrefs, 1);
	global_preferences = prefs;
	
	if (e_summary_preferences_restore (prefs) == TRUE) {
		return prefs;
	}

	/* Defaults */
	
	/* Mail */
	make_initial_mail_list (prefs);

	/* RDF */
	make_initial_rdf_list (prefs);
	prefs->rdf_refresh_time = 600;
	prefs->limit = 10;

	/* Weather */
	make_initial_weather_list (prefs);
	prefs->units = UNITS_METRIC;
	prefs->weather_refresh_time = 600;

	prefs->days = E_SUMMARY_CALENDAR_ONE_DAY;
	prefs->show_tasks = E_SUMMARY_CALENDAR_ALL_TASKS;

	return prefs;
}

ESummaryPrefs *
e_summary_preferences_get_global (void)
{
	return global_preferences;
}

struct _MailPage {
	GtkWidget *storage_set_view;
	GtkWidget *all, *shown;
	GtkWidget *fullpath;
	GtkWidget *add, *remove;

	GHashTable *model;
	GList *tmp_list;
};

struct _RDFPage {
	GtkWidget *etable;
	GtkWidget *refresh, *limit;
	GtkWidget *new_button, *delete_url;

	GHashTable *default_hash, *model;
	GList *known, *tmp_list;
};

struct _WeatherPage {
	GtkWidget *etable;
	GtkWidget *refresh, *imperial, *metric;
	GtkWidget *add, *remove;

	GHashTable *model;
	GList *tmp_list;
};

struct _CalendarPage {
	GtkWidget *one, *five, *week, *month;
	GtkWidget *all, *today;
};

typedef struct _PropertyData {
	EvolutionConfigControl *config_control;

	GtkWidget *new_url_entry, *new_name_entry;
	GladeXML *xml;

	struct _MailPage *mail;
	struct _RDFPage *rdf;
	struct _WeatherPage *weather;
	struct _CalendarPage *calendar;
} PropertyData;

struct _RDFInfo {
	char *url;
	char *name;

	gboolean custom;
};

static struct _RDFInfo rdfs[] = {
	{"http://advogato.org/rss/articles.xml", "Advogato", FALSE},
	{"http://barrapunto.com/barrapunto.rdf", "Barrapunto", FALSE},
	{"http://barrapunto.com/gnome.rdf", "Barrapunto GNOME", FALSE,},
	{"http://www.bsdtoday.com/backend/bt.rdf", "BSD Today", FALSE},
	{"http://beyond2000.com/b2k.rdf", "Beyond 2000", FALSE},
	{"http://www.dictionary.com/wordoftheday/wotd.rss", N_("Dictionary.com Word of the Day"), FALSE},
	{"http://www.dvdreview.com/rss/newschannel.rss", "DVD Review", FALSE},
	{"http://freshmeat.net/backend/fm.rdf", "Freshmeat", FALSE},
	{"http://www.gnomedesktop.org/backend.php", "Footnotes - GNOME News", FALSE},
	{"http://headlines.internet.com/internetnews/prod-news/news.rss", "Internet.com", FALSE},
	{"http://www.hispalinux.es/backend.php", "HispaLinux", FALSE},
	{"http://dot.kde.org/rdf", "KDE Dot News", FALSE},
	{"http://www.kuro5hin.org/backend.rdf", "Kuro5hin", FALSE},
	{"http://linuxgames.com/bin/mynetscape.pl", "Linux Games", FALSE},
	{"http://linuxtoday.com/backend/my-netscape.rdf", "Linux Today", FALSE},
	{"http://lwn.net/headlines/rss", "Linux Weekly News", FALSE},
	{"http://memepool.com/memepool.rss", "Memepool", FALSE},
	{"http://www.mozilla.org/news.rdf", "Mozilla", FALSE},
	{"http://www.mozillazine.org/contents.rdf", "Mozillazine", FALSE},
	{"http://www.fool.com/about/headlines/rss_headlines.asp", "The Motley Fool", FALSE},
	{"http://www.newsforge.com/newsforge.rss", "Newsforge", FALSE},
	{"http://www.pigdog.org/pigdog.rdf", "Pigdog", FALSE},
	{"http://www.python.org/channews.rdf", "Python.org", FALSE},
	{"http://www.quotationspage.com/data/mqotd.rss", N_("Quotes of the Day"), FALSE},
	{"http://www.salon.com/feed/RDF/salon_use.rdf", "Salon", FALSE},
	{"http://slashdot.org/slashdot.rdf", "Slashdot", FALSE},
	{"http://www.theregister.co.uk/tonys/slashdot.rdf", "The Register", FALSE},
	{"http://www.webreference.com/webreference.rdf", "Web Reference", FALSE},
	{"http://redcarpet.ximian.com/red-carpet.rdf", "Ximian Red Carpet News", FALSE},
	{NULL, NULL, FALSE}
};

static void
save_known_rdfs (GList *rdfs)
{
	FILE *handle;
	char *rdf_file;

	rdf_file = gnome_util_prepend_user_home ("evolution/RDF-urls.txt");
	handle = fopen (rdf_file, "w");
	g_free (rdf_file);

	if (handle == NULL) {
		g_warning ("Error opening RDF-urls.txt");
		return;
	}

	for (; rdfs; rdfs = rdfs->next) {
		struct _RDFInfo *info;
		char *line;
		
		info = rdfs->data;
		if (info->custom == FALSE) {
			continue;
		}
		
		line = g_strconcat (info->url, ",", info->name, "\n", NULL);
		fputs (line, handle);
		g_free (line);
	}

	fclose (handle);
}

/* Yeah a silly loop, but p should be short enough that it doesn't matter much */
static gboolean
rdf_is_shown (PropertyData *pd,
	      const char *url)
{
	GList *p;

	for (p = global_preferences->rdf_urls; p; p = p->next) {
		if (strcmp (p->data, url) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
fill_rdf_etable (GtkWidget *widget,
		 PropertyData *pd)
{
	ESummaryShownModelEntry *entry;
	ESummaryShown *ess;
	FILE *handle;
	int i, total;
	char *rdf_file, line[4096];

	if (pd->rdf->default_hash == NULL) {
		pd->rdf->default_hash = g_hash_table_new (g_str_hash, g_str_equal);
	}

	ess = E_SUMMARY_SHOWN (widget);

	/* Fill the defaults first */
	for (i = 0; rdfs[i].url; i++) {
		entry = g_new (ESummaryShownModelEntry, 1);
		entry->location = g_strdup (rdfs[i].url);
		entry->name = g_strdup (rdfs[i].name);
		entry->showable = TRUE;
		entry->data = &rdfs[i];
		
		e_summary_shown_add_node (ess, TRUE, entry, NULL, TRUE, NULL);

		if (rdf_is_shown (pd, rdfs[i].url) == TRUE) {
			entry = g_new (ESummaryShownModelEntry, 1);
			entry->location = g_strdup (rdfs[i].url);
			entry->name = g_strdup (rdfs[i].name);
			entry->showable = TRUE;
			entry->data = &rdfs[i];
			
			e_summary_shown_add_node (ess, FALSE, entry, NULL, TRUE, NULL);
		}

		pd->rdf->known = g_list_append (pd->rdf->known, &rdfs[i]);

		g_hash_table_insert (pd->rdf->default_hash, rdfs[i].url, &rdfs[i]);
	}

	total = i;

	rdf_file = gnome_util_prepend_user_home ("evolution/RDF-urls.txt");
	handle = fopen (rdf_file, "r");
	g_free (rdf_file);
	if (handle == NULL) {
		/* Open the old location just so that users data isn't lost */
		rdf_file = gnome_util_prepend_user_home ("evolution/config/RDF-urls.txt");
		handle = fopen (rdf_file, "r");
		g_free (rdf_file);
	}

	if (handle == NULL) {
		return;
	}

	while (fgets (line, 4095, handle)) {
		char **tokens;
		struct _RDFInfo *info;
		int len;

		len = strlen (line);
		if (line[len - 1] == '\n') {
			line[len - 1] = 0;
		}

		tokens = g_strsplit (line, ",", 2);
		if (tokens == NULL) {
			continue;
		}

		if (g_hash_table_lookup (pd->rdf->default_hash, tokens[0]) != NULL) {
			g_strfreev (tokens);
			continue;
		}
		
		info = g_new (struct _RDFInfo, 1);
		info->url = g_strdup (tokens[0]);
		info->name = g_strdup (tokens[1]);
		info->custom = TRUE;
		
		pd->rdf->known = g_list_append (pd->rdf->known, info);

		entry = g_new (ESummaryShownModelEntry, 1);
		entry->location = g_strdup (info->url);
		entry->name = g_strdup (info->name);
		entry->showable = TRUE;
		entry->data = info;
		
		e_summary_shown_add_node (ess, TRUE, entry, NULL, TRUE, NULL);

		if (rdf_is_shown (pd, tokens[0]) == TRUE) {
			entry = g_new (ESummaryShownModelEntry, 1);
			entry->location = g_strdup (info->url);
			entry->name = g_strdup (info->name);
			entry->showable = TRUE;
			entry->data = info;
			
			e_summary_shown_add_node (ess, FALSE, entry, NULL, TRUE, NULL);
		}

		g_strfreev (tokens);
	}

	fclose (handle);
}

static void
fill_weather_etable (ESummaryShown *ess,
		     PropertyData *pd)
{
	e_summary_weather_fill_etable (ess);
}

static void
mail_show_full_path_toggled_cb (GtkToggleButton *tb,
				PropertyData *pd)
{
	global_preferences->show_full_path = gtk_toggle_button_get_active (tb);

	evolution_config_control_changed (pd->config_control);
}

static void
add_dialog_clicked_cb (GtkWidget *widget,
		       int button,
		       PropertyData *pd)
{
	if (button == 0) {
		char *url;
		char *name;

		name = gtk_entry_get_text (GTK_ENTRY (pd->new_name_entry));
		url = gtk_entry_get_text (GTK_ENTRY (pd->new_url_entry));

		if (name != NULL && *name != 0 &&
		    url != NULL && *url != 0) {
			ESummaryShownModelEntry *entry;
			struct _RDFInfo *info;

			info = g_new (struct _RDFInfo, 1);
			info->url = g_strdup (url);
			info->name = g_strdup (name);
			info->custom = TRUE;

			pd->rdf->known = g_list_append (pd->rdf->known, info);

			entry = g_new (ESummaryShownModelEntry, 1);
			entry->location = g_strdup (info->url);
			entry->name = g_strdup (info->name);
			entry->showable = TRUE;
			entry->data = info;
			
			e_summary_shown_add_node (E_SUMMARY_SHOWN (pd->rdf->etable), TRUE,
						  entry, NULL, TRUE, NULL);

			/* Should we add to shown? */

			save_known_rdfs (pd->rdf->known);

			evolution_config_control_changed (pd->config_control);
		}
	}

	gtk_widget_destroy (widget);
}

static void
rdf_new_url_clicked_cb (GtkButton *button,
			PropertyData *pd)
{
	static GtkWidget *add_dialog = NULL;
	GtkWidget *label, *hbox;

	if (add_dialog != NULL) {
		gdk_window_raise (add_dialog->window);
		gdk_window_show (add_dialog->window);
		return;
	} 

	add_dialog = gnome_dialog_new (_("Add a news feed"),
				       GNOME_STOCK_BUTTON_OK,
				       GNOME_STOCK_BUTTON_CANCEL, NULL);
	gtk_signal_connect (GTK_OBJECT (add_dialog), "clicked",
			    GTK_SIGNAL_FUNC (add_dialog_clicked_cb), pd);
	gtk_signal_connect (GTK_OBJECT (add_dialog), "destroy",
			    GTK_SIGNAL_FUNC (gtk_widget_destroyed), &add_dialog);

	label = gtk_label_new (_("Enter the URL of the news feed you wish to add"));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (add_dialog)->vbox), label,
			    TRUE, TRUE, 0);
	hbox = gtk_hbox_new (FALSE, 2);
	label = gtk_label_new (_("Name:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	pd->new_name_entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), pd->new_name_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (add_dialog)->vbox), hbox,
			    TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 2);
	label = gtk_label_new (_("URL:"));
	pd->new_url_entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), pd->new_url_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (add_dialog)->vbox), 
			    hbox, TRUE, TRUE, 0);
	gtk_widget_show_all (add_dialog);
}

static void
rdf_delete_url_cb (GtkButton *button,
		   PropertyData *pd)
{
	GList *selection;
	
	selection = e_summary_shown_get_selection (E_SUMMARY_SHOWN (pd->rdf->etable), TRUE);

	for (; selection; selection = selection->next) {
		ETreePath path = selection->data;
		ESummaryShownModelEntry *entry;

		entry = g_hash_table_lookup (E_SUMMARY_SHOWN (pd->rdf->etable)->all_model, path);
		
		if (entry == NULL) {
			continue;
		}
		
		e_summary_shown_remove_node (E_SUMMARY_SHOWN (pd->rdf->etable), TRUE, entry);
		pd->rdf->known = g_list_remove (pd->rdf->known, entry->data);
		
		/* FIXME: Remove from shown side as well */
	}

	save_known_rdfs (pd->rdf->known);
}

static void
rdf_refresh_value_changed_cb (GtkAdjustment *adj,
			      PropertyData *pd)
{
	global_preferences->rdf_refresh_time = (int) adj->value;
	evolution_config_control_changed (pd->config_control);
}

static void
rdf_limit_value_changed_cb (GtkAdjustment *adj,
			    PropertyData *pd)
{
	global_preferences->limit = (int) adj->value;
	evolution_config_control_changed (pd->config_control);
}

static void
rdf_etable_item_changed_cb (ESummaryShown *ess,
			    PropertyData *pd)
{
	if (pd->config_control != NULL) {
		evolution_config_control_changed (pd->config_control);
	}
}

static void
rdf_etable_selection_cb (ESummaryShown *ess,
			 GList *selection,
			 PropertyData *pd)
{
	if (pd->rdf->delete_url == NULL) {
		return;
	}

	if (selection != NULL) {
		GList *p;
		
		for (p = selection; p; p = p->next) {
			ESummaryShownModelEntry *entry;
			struct _RDFInfo *info;

			entry = g_hash_table_lookup (E_SUMMARY_SHOWN (pd->rdf->etable)->all_model, p->data);
			if (entry == NULL) {
				g_warning ("Hmmm\n");
				continue;
			}

			info = entry->data;
			if (info->custom == TRUE) {
				gtk_widget_set_sensitive (pd->rdf->delete_url, TRUE);
				return;
			}
		}
	}

	gtk_widget_set_sensitive (pd->rdf->delete_url, FALSE);
}

static void
weather_etable_item_changed_cb (ESummaryShown *ess,
				PropertyData *pd)
{
	if (pd->config_control != NULL) {
		evolution_config_control_changed (pd->config_control);
	}
}

static void
weather_refresh_value_changed_cb (GtkAdjustment *adj,
				  PropertyData *pd)
{
	global_preferences->weather_refresh_time = (int) adj->value;
	evolution_config_control_changed (pd->config_control);
}

static void
weather_metric_toggled_cb (GtkToggleButton *tb,
			   PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	global_preferences->units = UNITS_METRIC;
	evolution_config_control_changed (pd->config_control);
}

static void
weather_imperial_toggled_cb (GtkToggleButton *tb,
			     PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	global_preferences->units = UNITS_IMPERIAL;
	evolution_config_control_changed (pd->config_control);
}


static void
calendar_one_toggled_cb (GtkToggleButton *tb,
			 PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	global_preferences->days = E_SUMMARY_CALENDAR_ONE_DAY;
	evolution_config_control_changed (pd->config_control);
}

static void
calendar_five_toggled_cb (GtkToggleButton *tb,
			  PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	global_preferences->days = E_SUMMARY_CALENDAR_FIVE_DAYS;
	evolution_config_control_changed (pd->config_control);
}

static void
calendar_week_toggled_cb (GtkToggleButton *tb,
			  PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	global_preferences->days = E_SUMMARY_CALENDAR_ONE_WEEK;
	evolution_config_control_changed (pd->config_control);
}

static void
calendar_month_toggled_cb (GtkToggleButton *tb,
			   PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	global_preferences->days = E_SUMMARY_CALENDAR_ONE_MONTH;
	evolution_config_control_changed (pd->config_control);
}

static void
calendar_all_toggled_cb (GtkToggleButton *tb,
			 PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	global_preferences->show_tasks = E_SUMMARY_CALENDAR_ALL_TASKS;
	evolution_config_control_changed (pd->config_control);
}

static void
calendar_today_toggled_cb (GtkToggleButton *tb,
			   PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	global_preferences->show_tasks = E_SUMMARY_CALENDAR_TODAYS_TASKS;
	evolution_config_control_changed (pd->config_control);
}

static void
storage_set_changed (EvolutionStorageSetViewListener *listener,
		     PropertyData *pd)
{
	evolution_config_control_changed (pd->config_control);
}

static gboolean
make_property_dialog (PropertyData *pd)
{
	struct _MailPage *mail;
	struct _RDFPage *rdf;
	struct _WeatherPage *weather;
	struct _CalendarPage *calendar;
	GtkWidget *listener;
	
	/* Mail */
	mail = pd->mail = g_new (struct _MailPage, 1);
	mail->tmp_list = NULL;

	mail->storage_set_view = glade_xml_get_widget (pd->xml, "mail-custom");
	g_return_val_if_fail (mail->storage_set_view != NULL, FALSE);

	listener = gtk_object_get_data (GTK_OBJECT (mail->storage_set_view),
					"listener");
	gtk_signal_connect (GTK_OBJECT (listener), "folder-toggled",
			    GTK_SIGNAL_FUNC (storage_set_changed), pd);

	mail->fullpath = glade_xml_get_widget (pd->xml, "checkbutton1");
	g_return_val_if_fail (mail->fullpath != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mail->fullpath),
				      global_preferences->show_full_path);
	gtk_signal_connect (GTK_OBJECT (mail->fullpath), "toggled",
			    GTK_SIGNAL_FUNC (mail_show_full_path_toggled_cb), pd);
	
	/* RDF */
	rdf = pd->rdf = g_new0 (struct _RDFPage, 1);
	rdf->etable = glade_xml_get_widget (pd->xml, "rdf-custom");
	g_return_val_if_fail (rdf->etable != NULL, FALSE);

	gtk_signal_connect (GTK_OBJECT (rdf->etable), "item-changed",
			    GTK_SIGNAL_FUNC (rdf_etable_item_changed_cb), pd);
	gtk_signal_connect (GTK_OBJECT (rdf->etable), "selection-changed",
			    GTK_SIGNAL_FUNC (rdf_etable_selection_cb), pd);
	
	fill_rdf_etable (rdf->etable, pd);
	rdf->refresh = glade_xml_get_widget (pd->xml, "spinbutton1");
	g_return_val_if_fail (rdf->refresh != NULL, FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (rdf->refresh),
				   (float) global_preferences->rdf_refresh_time);
	gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (rdf->refresh)->adjustment), "value_changed",
			    GTK_SIGNAL_FUNC (rdf_refresh_value_changed_cb), pd);

	rdf->limit = glade_xml_get_widget (pd->xml, "spinbutton4");
	g_return_val_if_fail (rdf->limit != NULL, FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (rdf->limit), 
				   (float) global_preferences->limit);
	gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (rdf->limit)->adjustment), "value_changed",
			    GTK_SIGNAL_FUNC (rdf_limit_value_changed_cb), pd);

	rdf->new_button = glade_xml_get_widget (pd->xml, "button11");
	g_return_val_if_fail (rdf->limit != NULL, FALSE);
	gtk_signal_connect (GTK_OBJECT (rdf->new_button), "clicked",
			    GTK_SIGNAL_FUNC (rdf_new_url_clicked_cb), pd);

	rdf->delete_url = glade_xml_get_widget (pd->xml, "delete-button");
	g_return_val_if_fail (rdf->delete_url != NULL, FALSE);
	gtk_signal_connect (GTK_OBJECT (rdf->delete_url), "clicked",
			    GTK_SIGNAL_FUNC (rdf_delete_url_cb), pd);
	
	/* Weather */
	weather = pd->weather = g_new (struct _WeatherPage, 1);
	weather->tmp_list = NULL;
	
	weather->etable = glade_xml_get_widget (pd->xml, "weather-custom");
	g_return_val_if_fail (weather->etable != NULL, FALSE);

	gtk_signal_connect (GTK_OBJECT (weather->etable), "item-changed",
			    GTK_SIGNAL_FUNC (weather_etable_item_changed_cb),
			    pd);

	fill_weather_etable (E_SUMMARY_SHOWN (weather->etable), pd);

	weather->refresh = glade_xml_get_widget (pd->xml, "spinbutton5");
	g_return_val_if_fail (weather->refresh != NULL, FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (weather->refresh),
				   (float) global_preferences->weather_refresh_time);
	gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (weather->refresh)->adjustment),
					"value-changed",
					GTK_SIGNAL_FUNC (weather_refresh_value_changed_cb),
					pd);

	weather->metric = glade_xml_get_widget (pd->xml, "radiobutton7");
	g_return_val_if_fail (weather->metric != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (weather->metric),
				      (global_preferences->units == UNITS_METRIC));
	gtk_signal_connect (GTK_OBJECT (weather->metric), "toggled",
			    GTK_SIGNAL_FUNC (weather_metric_toggled_cb), pd);

	weather->imperial = glade_xml_get_widget (pd->xml, "radiobutton8");
	g_return_val_if_fail (weather->imperial != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (weather->imperial),
				      (global_preferences->units == UNITS_IMPERIAL));
	gtk_signal_connect (GTK_OBJECT (weather->imperial), "toggled",
			    GTK_SIGNAL_FUNC (weather_imperial_toggled_cb), pd);

	/* Calendar */
	calendar = pd->calendar = g_new (struct _CalendarPage, 1);
	calendar->one = glade_xml_get_widget (pd->xml, "radiobutton3");
	g_return_val_if_fail (calendar->one != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->one),
				      (global_preferences->days == E_SUMMARY_CALENDAR_ONE_DAY));
	gtk_signal_connect (GTK_OBJECT (calendar->one), "toggled",
			    GTK_SIGNAL_FUNC (calendar_one_toggled_cb), pd);

	calendar->five = glade_xml_get_widget (pd->xml, "radiobutton4");
	g_return_val_if_fail (calendar->five != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->five),
				      (global_preferences->days == E_SUMMARY_CALENDAR_FIVE_DAYS));
	gtk_signal_connect (GTK_OBJECT (calendar->five), "toggled",
			    GTK_SIGNAL_FUNC (calendar_five_toggled_cb), pd);

	calendar->week = glade_xml_get_widget (pd->xml, "radiobutton5");
	g_return_val_if_fail (calendar->week != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->week),
				      (global_preferences->days == E_SUMMARY_CALENDAR_ONE_WEEK));
	gtk_signal_connect (GTK_OBJECT (calendar->week), "toggled",
			    GTK_SIGNAL_FUNC (calendar_week_toggled_cb), pd);

	calendar->month = glade_xml_get_widget (pd->xml, "radiobutton6");
	g_return_val_if_fail (calendar->month != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->month),
				      (global_preferences->days == E_SUMMARY_CALENDAR_ONE_MONTH));
	gtk_signal_connect (GTK_OBJECT (calendar->month), "toggled",
			    GTK_SIGNAL_FUNC (calendar_month_toggled_cb), pd);

	calendar->all = glade_xml_get_widget (pd->xml, "radiobutton1");
	g_return_val_if_fail (calendar->all != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->all),
				      (global_preferences->show_tasks == E_SUMMARY_CALENDAR_ALL_TASKS));
	gtk_signal_connect (GTK_OBJECT (calendar->all), "toggled",
			    GTK_SIGNAL_FUNC (calendar_all_toggled_cb), pd);

	calendar->today = glade_xml_get_widget (pd->xml, "radiobutton2");
	g_return_val_if_fail (calendar->today != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->today),
				      (global_preferences->show_tasks == E_SUMMARY_CALENDAR_TODAYS_TASKS));
	gtk_signal_connect (GTK_OBJECT(calendar->today), "toggled",
			    GTK_SIGNAL_FUNC (calendar_today_toggled_cb), pd);

	return TRUE;
}

static void
free_property_dialog (PropertyData *pd)
{
	if (pd->rdf) {
		g_list_free (pd->rdf->known);

		free_str_list (pd->rdf->tmp_list);
		g_list_free (pd->rdf->tmp_list);
		g_free (pd->rdf);
	}
	if (pd->mail) {
		free_str_list (pd->mail->tmp_list);
		g_list_free (pd->mail->tmp_list);
		g_free (pd->mail);
	}
	if (pd->weather) {
		free_str_list (pd->weather->tmp_list);
		g_list_free (pd->weather->tmp_list);
		g_free (pd->weather);
	}
	if (pd->calendar) {
		g_free (pd->calendar);
	}

	if (pd->xml) {
		gtk_object_unref (GTK_OBJECT (pd->xml));
	}

	g_free (pd);
}


/* Prototypes to shut gcc up */
GtkWidget *e_summary_preferences_make_mail_table (PropertyData *pd);
GtkWidget *e_summary_preferences_make_rdf_table (PropertyData *pd);
GtkWidget *e_summary_preferences_make_weather_table (PropertyData *pd);

static void
set_selected_folders (GNOME_Evolution_StorageSetView view)
{
	GNOME_Evolution_FolderList *list;
	CORBA_Environment ev;
	GList *l;
	int i, count;
	
	for (count = 0, l = global_preferences->display_folders; l;
	     l = l->next, count++)
		;

	list = GNOME_Evolution_FolderList__alloc ();
	list->_length = count;
	list->_maximum = count;
	list->_buffer = CORBA_sequence_GNOME_Evolution_Folder_allocbuf (count);

	for (i = 0, l = global_preferences->display_folders; l; i++, l = l->next) {
		ESummaryPrefsFolder *folder = l->data;
		
		list->_buffer[i].type = CORBA_string_dup ("");
		list->_buffer[i].description = CORBA_string_dup ("");
		list->_buffer[i].displayName = CORBA_string_dup ("");
		list->_buffer[i].evolutionUri = CORBA_string_dup (folder->evolution_uri);
		list->_buffer[i].physicalUri = CORBA_string_dup (folder->physical_uri);
		list->_buffer[i].unreadCount = 0;
		list->_buffer[i].canSyncOffline = TRUE;
		list->_buffer[i].sortingPriority = 0;
		list->_buffer[i].customIconName = CORBA_string_dup ("");
	}

	CORBA_exception_init (&ev);
	GNOME_Evolution_StorageSetView__set_checkedFolders (view, list, &ev);
	CORBA_exception_free (&ev);
}

GtkWidget *
e_summary_preferences_make_mail_table (PropertyData *pd)
{
	CORBA_Environment ev;
	Bonobo_Control control;
	GNOME_Evolution_StorageSetView view;
	EvolutionStorageSetViewListener *listener;
	GNOME_Evolution_StorageSetViewListener corba_listener;
	GtkWidget *widget;
	
	g_assert (global_shell != NULL);
	
	CORBA_exception_init (&ev);
	control = GNOME_Evolution_Shell_createStorageSetView (global_shell, &ev);
	if (BONOBO_EX (&ev) || control == CORBA_OBJECT_NIL) {
		g_warning ("Error getting StorageSetView");
		CORBA_exception_free (&ev);
		return NULL;
	}

	view = Bonobo_Unknown_queryInterface (control,
					      "IDL:GNOME/Evolution/StorageSetView:1.0", &ev);
	if (BONOBO_EX (&ev) || view == CORBA_OBJECT_NIL) {
		g_warning ("Error querying %s",
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return NULL;
	}

	GNOME_Evolution_StorageSetView__set_showCheckboxes (view, TRUE, &ev);

	listener = evolution_storage_set_view_listener_new ();

	corba_listener = evolution_storage_set_view_listener_corba_objref (listener);

	GNOME_Evolution_StorageSetView_addListener (view, corba_listener, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Error adding listener %s",
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return NULL;
	}
	
	CORBA_exception_free (&ev);

	widget = bonobo_widget_new_control_from_objref (control, CORBA_OBJECT_NIL);
	gtk_object_set_data (GTK_OBJECT (widget), "listener", listener);
	gtk_object_set_data (GTK_OBJECT (widget), "corba_view", view);

	set_selected_folders (view);
	return widget;
}

GtkWidget *
e_summary_preferences_make_rdf_table (PropertyData *pd)
{
	return e_summary_shown_new ();
}

GtkWidget *
e_summary_preferences_make_weather_table (PropertyData *pd)
{
	return e_summary_shown_new ();
}


/* The factory for the ConfigControl.  */

static void
add_shown_to_list (gpointer key,
		   gpointer value,
		   gpointer data)
{
	ESummaryShownModelEntry *item;
	GList **list;

	item = (ESummaryShownModelEntry *) value;
	list = (GList **) data;

	*list = g_list_prepend (*list, g_strdup (item->location));
}

static GList *
get_folders_from_view (GtkWidget *view)
{
	GNOME_Evolution_StorageSetView set_view;
	GNOME_Evolution_FolderList *list;
	CORBA_Environment ev;
	GList *out_list = NULL;
	int i;
	
	set_view = gtk_object_get_data (GTK_OBJECT (view), "corba_view");
	CORBA_exception_init (&ev);

	list = GNOME_Evolution_StorageSetView__get_checkedFolders (set_view, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Error getting checkedFolders\n%s",
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	for (i = 0; i < list->_length; i++) {
		GNOME_Evolution_Folder folder = list->_buffer[i];
		ESummaryPrefsFolder *f;

		f = g_new (ESummaryPrefsFolder, 1);
		f->evolution_uri = g_strdup (folder.evolutionUri);
		f->physical_uri = g_strdup (folder.physicalUri);
		out_list = g_list_append (out_list, f);
	}
	
	return out_list;
}
	
static void
config_control_apply_cb (EvolutionConfigControl *control,
			 void *data)
{
	PropertyData *pd;

	pd = (PropertyData *) data;

	if (pd->rdf->tmp_list) {
		free_str_list (pd->rdf->tmp_list);
		g_list_free (pd->rdf->tmp_list);
		pd->rdf->tmp_list = NULL;
	}
	/* Take each news feed which is on and add it
	   to the shown list */
	g_hash_table_foreach (E_SUMMARY_SHOWN (pd->rdf->etable)->shown_model,
			      add_shown_to_list, &pd->rdf->tmp_list);
	
	if (global_preferences->rdf_urls) {
		free_str_list (global_preferences->rdf_urls);
		g_list_free (global_preferences->rdf_urls);
	}
	
	global_preferences->rdf_urls = copy_str_list (pd->rdf->tmp_list);
	
	/* Weather */
	if (pd->weather->tmp_list) {
		free_str_list (pd->weather->tmp_list);
		g_list_free (pd->weather->tmp_list);
		pd->weather->tmp_list = NULL;
	}
	
	g_hash_table_foreach (E_SUMMARY_SHOWN (pd->weather->etable)->shown_model,
			      add_shown_to_list, &pd->weather->tmp_list);
	if (global_preferences->stations) {
		free_str_list (global_preferences->stations);
		g_list_free (global_preferences->stations);
	}
	global_preferences->stations = copy_str_list (pd->weather->tmp_list);
	
	/* Folders */
	if (pd->mail->tmp_list) {
		free_str_list (pd->mail->tmp_list);
		g_list_free (pd->mail->tmp_list);
		pd->mail->tmp_list = NULL;
	}
#if 0
	g_hash_table_foreach (pd->mail->model, maybe_add_to_shown, &pd->mail->tmp_list);
#endif
	
	if (global_preferences->display_folders) {
		free_folder_list (global_preferences->display_folders);
		g_list_free (global_preferences->display_folders);
	}
	global_preferences->display_folders = get_folders_from_view (pd->mail->storage_set_view);

  	e_summary_reconfigure_all ();
}

static void
config_control_destroy_cb (EvolutionConfigControl *config_control,
			   void *data)
{
	PropertyData *pd;

	pd = (PropertyData *) data;

	e_summary_preferences_save (global_preferences);
	free_property_dialog (pd);
}

static BonoboObject *
factory_fn (BonoboGenericFactory *generic_factory,
	    void *data)
{
	PropertyData *pd;
	GtkWidget *widget;

	pd = g_new0 (PropertyData, 1);

	pd->xml = glade_xml_new (EVOLUTION_GLADEDIR "/my-evolution.glade", NULL);
	g_return_val_if_fail (pd->xml != NULL, NULL);

	widget = glade_xml_get_widget (pd->xml, "notebook");
	if (widget == NULL || ! make_property_dialog (pd)) {
		g_warning ("Missing some part of XML file");
		free_property_dialog (pd);
		return NULL;
	}

	gtk_widget_ref (widget);
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);

	gtk_widget_show (widget);
	pd->config_control = evolution_config_control_new (widget);

	gtk_widget_unref (widget);

	gtk_signal_connect (GTK_OBJECT (pd->config_control), "apply",
			    GTK_SIGNAL_FUNC (config_control_apply_cb), pd);
	gtk_signal_connect (GTK_OBJECT (pd->config_control), "destroy",
			    GTK_SIGNAL_FUNC (config_control_destroy_cb), pd);

	return BONOBO_OBJECT (pd->config_control);
}

gboolean
e_summary_preferences_register_config_control_factory (GNOME_Evolution_Shell corba_shell)
{
	if (bonobo_generic_factory_new (FACTORY_ID, factory_fn, NULL) == NULL)
		return FALSE;

	global_shell = corba_shell;
	
	return TRUE;
}
